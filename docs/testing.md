# 测试规范

所有命令从仓库根目录执行。测试结果应记录提交、工具链、命令、结果与未验证范围。
不要把主机绘制测试或 Android 编译成功表述为所有真机兼容。

## 本地与 CI 共用入口

```bash
python3 scripts/check-repository.py
bash -n scripts/check.sh scripts/build-android.sh
bash scripts/check.sh host
bash scripts/check.sh consumer
CC=clang CXX=clang++ bash scripts/check.sh host build/check-clang
CC=clang CXX=clang++ \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  bash scripts/check.sh sanitizers
```

`check-repository.py` 检查 Git 已跟踪及未忽略的待加入文件，包括相对文档链接、Python 语法、
越界符号链接、构建产物、图片/录屏、本地工具文件和私钥标记；它不能替代完整的来源与许可确认。
`check.sh` 接受 `host|sanitizers|consumer` 和可选构建目录，`SKUI_JOBS` 控制并行数。
不同编译器/检测模式使用不同目录。CI 使用同样入口，不要求 root 或真实显示设备。

主机测试使用真实 ImGui 帧及合成输入，覆盖路由、控件事件、禁用/取消点击、缩放、
拖动/滚动、选择与级联菜单、弹层层级、UTF-8 编辑回填、折叠/侧栏/提示等动画。
折叠与侧栏已有 30/60/120 Hz 时间步长测试；不是所有动画都已逐一覆盖三种频率。
当前 CTest 注册一个 `skui-tests` 可执行文件，内部运行多个测试函数。
独立 consumer 编译、链接并产生绘制数据，验证公开 CMake 目标可被另一个项目使用。

## 按改动选择门槛

| 改动 | 必须验证 |
| --- | --- |
| 文档、CI、脚本 | 仓库检查、脚本语法、涉及的命令/工作流 |
| 公开 API、组件或路由 | 主机测试、独立接入、ASan/UBSan；视觉行为另需 Gallery 检查 |
| 动画、滚动、浮层输入 | 相关交互回归、进退场/反向/动画关闭；必要的帧时间和真机检查 |
| Android 窗口、触摸、IME | Android 两个目标编译、相关真机操作与退出清理 |
| 依赖或工具链 | 干净目录完整构建、许可/哈希检查、相关主机与 Android 路径 |

## Android 编译验证

```bash
bash scripts/build-android.sh arm64-v8a
cmake --build build/arm64-v8a --target skui-ime-tests --parallel 4
"$ANDROID_SDK_ROOT/build-tools/36.0.0/apksigner" verify \
  build/arm64-v8a/generated/ime/skui-ime-helper.apk
python3 scripts/check-android-elf.py \
  build/arm64-v8a/sk-imgui-ui build/arm64-v8a/skui-ime-tests
```

[CI](../.github/workflows/ci.yml) 在 push、PR 和手动触发时运行 GCC/Clang、Sanitizer、
consumer、固定 SDK/NDK 的 arm64 编译、辅助 APK 签名验证和 ELF 对齐检查。
Actions 固定到上游提交；升级时核对其运行时要求并重新验证工作流。
GitHub 托管 runner 不运行 root 悬浮窗口或真实键盘测试，也不自动发布二进制和签名材料。

## 真机回归与清理

明确选择允许操作的测试设备；检查旧实例后才启动，不使用无目标的批量 kill。
部署与诊断命令见 [Android Demo](android-demo.md)。至少记录系统/API、ABI、显示尺寸、
GPU/渲染后端和输入法，不公开设备唯一序列号。

Gallery 检查包含：点击/禁用/取消、拖动与嵌套滚动、短/长列表、菜单级联和关闭，
弹层遮罩与背景隔离，折叠双向/反向、路由前进返回、主题/缩放、颜色连续输入和数字滚轮。
修改系统输入法时另检查中文、emoji、退格、清空、完成/取消、键盘避让与退出。
动画评估同时检查掉帧、布局跳变、退出闪现，不只截取一个静态画面。

IME 自动回归会真实安装/卸载辅助包、打开输入法并注入文字，需使用专门的 root 测试设备：

```bash
python3 scripts/test-ime-device.py --serial <设备序列号> --build build/arm64-v8a
```

脚本覆盖直接桥接、回退、取消/断连、安装中退出、监护进程终止和同名包保护。
不支持直接窗口会话的系统可针对适用模式测试；该系统上 `--mode direct` 失败不等于回退失败。
不要为了让测试通过而覆盖/卸载设备上预先存在的同名包。

测试后关闭本轮 Gallery 与测试进程，确认 PID 已退出、触摸独占解除、辅助输入进程及
本轮安装的 `skui.android.imehelper` 已移除，并清理上传的测试文件。
不要删除其他会话的进程、文件或预先安装的包。
设备测试未执行时在 PR 中说明原因，保留明确的未验证项。
