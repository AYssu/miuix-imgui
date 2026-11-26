# miuix-imgui

基于 Dear ImGui 的 C++17 组件库，附带 Android 原生悬浮 Gallery。
设计参考 Miuix / HyperOS，以 `skui::` 接口提供设置项、选择菜单、弹层、颜色与数字选择、导航及页面转场。
这是独立的 C++ 实现，与 Xiaomi、Miuix 和 Dear ImGui 官方没有隶属关系。

组件库可以接入已有 ImGui 渲染循环；Android Demo 是需要 root 的原生可执行文件。
仓库根目录就是本项目，不依赖外层工作区或旁边的其他工程。
公开接口继续使用 `skui::`，Android Demo 可执行文件保留名称 `sk-imgui-ui`。

## 能做什么

| 分类 | 内容 |
| --- | --- |
| 基础与表单 | Button、Switch、Checkbox、Radio、Slider、TextField、SearchField 与带摘要的 Preference |
| 浮动选择 | Dropdown、Spinner、MultiSelect、列表/图标/级联菜单、Dialog、BottomSheet |
| 布局与导航 | Surface、Card、TopAppBar、CollapsingTopAppBar、可折叠 NavigationRail、NavigationBar、TabBar、BreadcrumbBar、RouterView |
| 反馈与连续输入 | Snackbar、Tooltip、Badge、Progress、下拉刷新、NumberPicker、ColorPalette、ColorPicker |
| 高级能力 | 9 个 Window 组件、Glass 模糊/折射、OkHSV / OkLab / OkLCH、触觉回调与可调导航弹簧 |
| Android Demo | 拖动/缩放窗口、粒子悬浮球收起与恢复、触摸适配、系统输入法、主题切换、分类展示与中英文搜索 |

Miuix 目录的 55 个名称均有对应交互。9 个 Window 组件在 Android Demo 中提交到
独立的透明 Surface，支持 Vulkan / OpenGL；其他宿主可安装窗口回调，未接入时回退到 Overlay。
Glass 使用有界 CPU 采样与模糊，不读取外部 GPU 纹理或系统壁纸。
具体接口、差异与过渡行为见 [组件对照](docs/miuix-components.md)。
使用 ImGui 内部 API，默认附带的版本为 `v1.93.0`；接入其他版本须自行验证兼容性。
升级来源与适配细节见 [依赖版本](docs/dependencies.md)。
原生悬浮后端依赖 Android 私有接口，当前设备验证以 Android 13 / arm64 为主，
不能仅凭编译成功推定其他系统版本可用。

## 快速验证

在 Linux 上安装 C++17 编译器、CMake 3.21+、Make 和 Python 3.9+，从仓库根目录执行：

```bash
git clone https://github.com/AYssu/miuix-imgui.git
cd miuix-imgui
bash scripts/check.sh host
bash scripts/check.sh consumer
```

主机验证不需要 Android SDK、NDK、显示服务器或 root，也不会启动悬浮窗口。
`consumer` 编译独立 CMake 接入示例并生成一帧绘制数据。

Android Demo 另需 JDK 17、SDK platform 36、build-tools 36.0.0 和 NDK：

```bash
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export SKUI_NDK="$ANDROID_SDK_ROOT/ndk/28.2.13676358"
bash scripts/build-android.sh arm64-v8a
```

输出为 `build/arm64-v8a/sk-imgui-ui`。首次构建 Android Demo 会下载并校验固定版本的 FreeType 源码。
完整工具链安装、离线构建和参数见 [构建规范](docs/building.md)，
部署、root 要求与 IME 生命周期见 [Android Demo](docs/android-demo.md)。

## 接入现有项目

```cmake
set(SKUI_BUILD_ANDROID_DEMO OFF CACHE BOOL "" FORCE)
# 宿主已有 ImGui target 时，在 add_subdirectory 之前指定：
# set(SKUI_IMGUI_TARGET my-imgui CACHE STRING "" FORCE)
add_subdirectory(third_party/miuix-imgui skui)
target_link_libraries(your-app PRIVATE skui::ui skui::navigation)
```

窗口、上下文、字体、输入和渲染器由宿主管理。完整每帧示例及 ID、状态、动画、
弹层和输入协议见 [组件接入规范](docs/integration.md)。
可运行示例位于 [examples/minimal](examples/minimal/main.cpp)，完整 Gallery 位于 [examples/gallery](examples/gallery/gallery.cpp)。

## 开发与贡献

| 文档 | 用途 |
| --- | --- |
| [贡献规范](CONTRIBUTING.md) | Issue、PR、代码风格、兼容性和验收要求 |
| [新增组件规范](docs/component-development.md) | API、实现归属、交互、过渡、示例与测试要求 |
| [构建规范](docs/building.md) | 工具链、CMake 参数、产物与依赖 |
| [测试规范](docs/testing.md) | 主机、Sanitizer、接入测试、真机验证与清理 |
| [基础能力路线图](docs/roadmap.md) | 同类项目参考、当前差距、实施顺序与验收标准 |
| [主题、设置与性能](docs/foundations.md) | 设计参数、局部主题、可选设置保存、固定容量性能采样 |
| [调度、任务与长列表](docs/runtime.md) | 闲置唤醒、可取消加载、稳定 ID / 滚动锚点、可见行裁剪 |
| [自适应布局与字体](docs/layout-and-fonts.md) | 安全区域、旋转 / 键盘约束、换列、大字号与翻译、资源所有权 |
| [键盘与返回](docs/keyboard.md) | 控件导航、编辑 / 弹层 / 页面返回优先级及焦点恢复 |
| [CMake 包](docs/packaging.md) | 安装 / 导出、移动前缀、宿主 ImGui 与仅路由消费 |
| [桌面 Gallery](docs/desktop.md) | 可选 SDL2 / OpenGL3 宿主、字体、闲置等待与退出检查 |
| [CI 工作流](.github/workflows/ci.yml) | GCC/Clang、ASan/UBSan、独立接入及 Android 构建 |

```text
include/skui/            公开组件与独立 Router 接口
src/skui/                组件、布局、弹层、共享动画与路由绘制
examples/                完整 Gallery 和最小宿主
android/ime/             系统输入法桥接与临时辅助 Activity
src/Android_*/           Android 悬浮、渲染和触摸适配
include/ImGui/, src/ImGui/  随仓库附带的 Dear ImGui
tests/, scripts/         测试与构建入口
docs/, LICENSES/         规范、验证证据和第三方许可原文
```

## 许可与来源

项目自有代码使用 [MIT](LICENSE)。第三方组件保留各自许可，分发时同时携带
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与 `LICENSES/`。
旧 Android 悬浮/触摸适配部分来源于网络，尚未确认的上游与许可在该清单单独标注；
“来源于网络”不表示已取得这些部分的 MIT 授权。Android 17 适配由项目作者提供。
公开完整 Android 源码或二进制前，需要补齐清单中的来源确认项。

开发时可使用 [交互检查与动画调试](docs/devtools.md)，默认关闭，按需启用。

版本与交付：[变更记录](CHANGELOG.md)、[发布规范](docs/releases.md)、[兼容矩阵](docs/compatibility.md)。
