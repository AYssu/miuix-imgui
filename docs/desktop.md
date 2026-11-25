# 桌面 Gallery

可选的 SDL2 / OpenGL3 宿主复用 [Gallery](../examples/gallery/gallery.cpp)，用于贡献者本地交互验证。
默认关闭，不影响纯组件库、headless 回归和 Android 构建。

## 构建与启动

POSIX 桌面需 SDL2 2.0.18+ 开发包、OpenGL 开发包、CMake 3.21+ 和 C++17 编译器。
Linux 示例依赖：`libsdl2-dev libgl1-mesa-dev fonts-noto-cjk`。
这些由系统管理，CMake 不自动下载；主机测试不要求安装。

```bash
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Debug \
  -DSKUI_BUILD_ANDROID_DEMO=OFF -DSKUI_BUILD_DESKTOP_DEMO=ON
cmake --build build/desktop --parallel 4
build/desktop/skui-gallery
# 可选：启用设置文件和自行提供的中文字体。
build/desktop/skui-gallery --settings /your/path/gallery.cfg --font /your/path/font.ttc
```

支持鼠标、滚轮、窗口缩放、Tab / 方向键 / Enter / Escape，以及 SDL2 输入法和剪贴板通道。
具体键盘规则见 [键盘与返回](keyboard.md)；平台输入法候选、中文组合和实际硬件按键仍需各桌面环境实测。
默认尝试系统 Noto CJK / PingFang 字体，找不到时退回 ImGui 默认字体；后者不包含完整中文。
显式 `--font` 无法加载会报错退出，系统字体只在运行时读取，不复制到仓库或安装包。

没有 Android 的 root、临时 IME APK 或原生 Surface；Window 系列使用已有 Overlay 回退。
触觉没有安装平台适配时不产生设备反馈。GL 渲染器支持 ImGui 1.93 的动态纹理协议，
字体、路由快照和 Glass 沿用同一套生命周期。

## 运行与退出

宿主通常启用 vsync。动画 / 输入活跃时持续绘制，静止时通过 SDL 事件队列等待，最长 100ms；
后台任务用合并的自定义 SDL 事件唤醒。不支持 vsync 的环境有约 60 Hz 的活动帧节流，避免空转。
`--continuous` 用于对比持续绘制，`--frames N` 用于限制验证帧数后正常退出。
性能采样从事件等待之后开始，不把 idle wait 算入 CPU 绘制耗时。

关闭 Gallery 内容的关闭按钮或桌面窗口后，先保存设置并 join 后台任务，
再释放 ImGui 渲染 / 平台资源、GL context、SDL 窗口与事件队列。
没有指定 `--settings` 时不创建设置文件；处理约定见 [基础能力](foundations.md)。

## 自动检查与范围

```bash
# 在桌面显示环境中运行；先确认没有已有 Gallery 实例。
python3 scripts/check-desktop.py build/desktop/skui-gallery
# 支持 offscreen 驱动的本地环境也可使用：
SDL_VIDEODRIVER=offscreen python3 scripts/check-desktop.py build/desktop/skui-gallery
# CI 使用 Xvfb + Mesa；需要 xvfb / xauth。
xvfb-run -a env SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 \
  python3 scripts/check-desktop.py build/desktop/skui-gallery
```

脚本启动真实 GL Gallery，检查正常有限帧退出、设置重启、损坏文件保留、字体失败和参数校验；
只清理自己创建的临时文件，超时只停止自己启动的进程。
组件操作的合成输入回归继续由六组 CTest 承担。
2026-11-25 本地 Linux / SDL2 / Mesa offscreen 已完成绘制和退出检查。
没有在此宣称 macOS、Windows、所有高 DPI 显示器或桌面输入法已经运行验证；
当前示例的设置文件实现使用 POSIX，CMake 对非 POSIX 平台明确拒绝启用此示例。
