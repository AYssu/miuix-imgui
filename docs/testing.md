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
CTest 注册 `skui-tests`、`skui-advanced-tests`、`skui-foundation-tests` 和 `skui-runtime-tests`。
高级测试检查 9 个 Window 接口的绘制分层、提交失败回退及关闭通知，2916 个颜色往返和参考值，
Glass 的背景混色 / 纹理更新 / 缩放回收、零模糊细线清晰度、局部坐标、快照隔离及帧缓冲缩放，
以及模糊强度跨整数边界的连续性、细条纹随背景移动时的过滤稳定性，
以及动态顶栏、官方弹簧参考点和触觉开关。
面包屑回归覆盖胶囊尺寸 / 高亮透明度、短路径无滚动条、长名称省略、首次定位、
30/60/120 Hz 自动滚动与反向、同帧拖动取消点击、禁用时仍可滚动、负高亮索引及浅深色缩放。
独立 consumer 编译、链接并产生绘制数据，验证公开 CMake 目标可被另一个项目使用。
基础能力测试覆盖浅深色 / 缩放设计参数、宿主字体保留、真实容器顶点、局部主题嵌套与异常恢复，
以及无效参数回退；验证采样关闭 / 清空、固定历史上限、精确分位数和采样 / 统计零动态分配。
POSIX 主机还测试设置编解码、版本 / 默认值、损坏 / 超长输入、未知字段、原子替换、
失败时旧文件保留和临时文件清理。功能与性能的边界见 [基础能力说明](foundations.md)。
运行时测试检查重绘提示、零分配和 context 生命周期、任务取消 / 重试与旧结果排除，
以及 10 万条数据的稳定 ID / 锚点和可见行裁剪；Linux 还检查 eventfd 和通道唤醒。
真机闲置 CPU 对比、键盘回填和长列表记录见 [运行效率说明](runtime.md)。
运行时测试还覆盖 viewport / 键盘约束、旋转边界、换列、长文案 / 大字号的行高与顶点，
以及窗口缩小时活动输入卡片持续绘制和父级滚动；真机与 CPU 成本记录见 [布局与字体](layout-and-fonts.md)。

2026-11-25 基础能力真机检查：小米 Android 13 / Vulkan Gallery 验证深色与强调色、圆角修改、
诊断面板、手动保存、移动窗口、退出及重启恢复；损坏设置文件在启动 / 退出后保持原内容。
正常运行日志为 0 字节，本轮进程及上传文件均已清理。原生 headless 基础回归通过。
同机 Release CPU 探针中，100 帧预热后 3000 帧、每帧 12 个按钮的 ApplyTheme 中位数约
0.63 微秒，整帧 CPU 绘制中位数约 52.2 微秒；10 万次采样器循环开启时平均约 0.27 微秒。
探针没有原生窗口或 GPU 提交，不能用这些数字代表实际 Gallery 帧时间或功耗。

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
新增 Window 后端时另检查真实 Surface 创建、遮罩、选中 / 外部关闭、重复打开、旋转及退出回收；
Glass 检查动态背景和模糊参数变化，触觉检查开关与系统勿扰约束。
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

## 高级能力验证 · 2026-11-24

本轮本地 GCC / Clang 的两组 CTest、Clang ASan + UBSan、独立 consumer 均通过。
`skui-advanced-tests` 检查 9 个 Window 接口的绘制分层、索引/计数、后端失败回退、
关闭后的帧终结通知及退场期间保留窗口；另检查 2916 组颜色往返、OkLab 红色参考值、
Glass 的实际模糊与纹理更新/退休、顶栏滚动插值和触觉事件开关。

Android Gallery 和 IME 测试目标使用 NDK 28.2.13676358 编译，AArch64 ELF 的 LOAD 段
满足 16 KiB 对齐。小米 23049RAD8C / Android 13 / 1080×2400 真机分别运行 Vulkan 和
OpenGL，验证独立列表、对话框、底部面板、外部点击关闭、重新打开及 SurfaceFlinger 层回收，
并检查 Glass 实时显示和参数调整。系统振动记录接收到 TOUCH 事件，未测量物理振动强度。
正常 Gallery 日志为 0 字节；测试后关闭本轮实例并移除临时上传文件。
这不构成 Android 16 / 17 或其他厂商设备已完成实测的声明。图片和视频不进入仓库。

Glass 零参数回归补充：GCC / Clang、ASan + UBSan、独立 consumer 与 Android 编译通过，
同一小米 Android 13 设备上的原生回归验证 1 像素细线、局部面板坐标、2 倍帧缓冲密度、
捕获后追加绘制的隔离、半透明三角形共享边及 1M 像素预算；Vulkan Gallery 检查双零参数、
色调强度及重新增加模糊 / 折射。纯色三角形采用扫描线填充，避免高分辨率逐像素纹理采样。
856×484 背景、9 个圆形的 Release CPU 探针中，Capture + Draw 的中位耗时约为
零模糊 7.82 ms、模糊 1 时 6.45 ms、模糊 12 时 0.87 ms（逻辑缩放 2.2、折射 0、饱和度 1.2）。
零模糊 / 零折射 / 饱和度 1 的背景直通约 0.01 ms；这些数值不包含 GPU 上传、提交或整帧耗时。
