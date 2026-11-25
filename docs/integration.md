# 组件接入规范

宿主管理 ImGui 上下文、窗口、事件循环、字体、输入与渲染器。
`skui::ui` 只提供绘制与交互，不创建 Android Surface、不启动键盘或工作线程。
最小可执行接入见 [examples/minimal/main.cpp](../examples/minimal/main.cpp)。

## CMake 与版本

```cmake
set(SKUI_BUILD_ANDROID_DEMO OFF CACHE BOOL "" FORCE)
# 已有 ImGui target 时启用，且该 target 必须先定义：
# set(SKUI_IMGUI_TARGET my-imgui CACHE STRING "" FORCE)
add_subdirectory(third_party/miuix-imgui skui)
target_link_libraries(your-app PRIVATE skui::ui skui::navigation)
```

默认编译随仓库附带的 ImGui。外部 target 必须公开 `imgui.h`、`imgui_internal.h`、
配置宏及链接依赖，且与宿主使用同一份上下文和实现；不要再编译一份 ImGui。
当前基于附带的 `v1.93.0` 内部 API，其他版本不保证直接兼容。
动态字体及后端升级要求见 [依赖版本](dependencies.md)。
`ApplyTheme()` 保留宿主的 `FontSizeBase`、`FontScaleMain` 和 `FontScaleDpi`；
需要临时字号时使用 `ImGui::PushFont(nullptr, size)` / `PopFont()`。
仅使用无动画路由时可以只链接 `skui::navigation`，该头文件目标不依赖 ImGui 或 Android。

## 每帧调用与状态

先由后端更新 IO、字体与帧时间，再开始帧。示例中的业务状态跨帧保存：

```cpp
#include <skui/skui.h>

// 宿主拥有的持久状态。
static bool enabled = true;
static float volume = 50;
static skui::Theme theme;

// 已选定 ImGui 上下文，后端已准备本帧 IO。
skui::ApplyTheme(theme);
ImGui::NewFrame();
ImGui::Begin("Settings");
if (skui::BeginCard("preferences")) {
    skui::Toggle("Notifications", &enabled);
    skui::Slider("Volume", &volume, 0, 100, "%.0f %%");
    if (skui::Button("Save")) { /* 保存业务数据 */ }
}
skui::EndCard();
ImGui::End();
ImGui::Render();
// 宿主渲染 ImGui::GetDrawData()。
```

组件及主题在渲染线程调用。多上下文切换后重新应用主题。
`Theme::scale` 缩放逻辑尺寸，宿主同时通过 `PushFont` / 字体倍率配置所需字号；
使用 `ImVec2` 屏幕坐标的接口按头文件注释传入实际像素。
`ShellOptions::initial_position/initial_size` 和悬浮操作位置均为实际屏幕像素。
安全区域、自动换列、长翻译与字体资源约定见 [自适应布局与字体](layout-and-fonts.md)。

标签/ID 应在组件存活期间稳定，同名组件通过 `PushID/PopID` 或 `##suffix` 区分。
不要每帧改变 ID，也不要用临时局部变量重置选中项、侧栏状态或 Router。
传入缓冲区、选项和回调的数据在调用时必须有效；由宿主负责生命周期。

## 容器与弹层配对

| Begin 接口 | 配对规则 |
| --- | --- |
| BeginCard / BeginSurface / BeginScroll / BeginShell | 无论返回 true/false，均调用对应 End |
| BeginRefreshScroll | 无论返回 true/false，均调用 EndScroll；`refreshing` 和请求处理由宿主负责 |
| BeginDialog / BeginBottomSheet | 仅返回 true 时调用 EndDialog / EndBottomSheet |
| BeginAdaptiveGrid | 仅返回 true 时逐个调用 NextAdaptiveCell，最后 EndAdaptiveGrid |

这些规则以 [公开头文件](../include/skui/skui.h) 的注释为准。
Overlay 是同一 ImGui Surface 内的弹层，不是独立 Android Window。
可以放入已有 ImGui 宿主窗口；使用 Shell 是可选的。

```cpp
if (skui::Button("Show")) skui::OpenDialog("confirm");
// 必须在相同窗口与 ID scope 每帧调用，包括关闭动画期间。
if (skui::BeginDialog("confirm", "Confirm")) {
    skui::Text("Apply these settings?");
    if (skui::Button("Apply")) skui::CloseDialog();
    skui::EndDialog();
}
```

不要再用外层 `if (open)` 在关闭瞬间移除弹层、折叠或悬浮组件。
`Collapsible(id, expanded, content)` 始终调用；内容退场期间保留绘制并隔离输入。
`FloatingActionButton/FloatingToolbar(..., visible)` 每帧调用，隐藏时传 `false`。
短选项列表应按内容定高，仅溢出时滚动；不额外套固定高度的滚动窗口。
`Theme::animations=false` 时离散过渡立即完成，拖动和颜色预览始终同帧响应。

## 路由与转场

`Router`、`RouterView` 及页面业务状态跨帧持有，页面注册一次：

```cpp
skui::Router router;
skui::RouterView view;
router.Register("/home", [](const skui::Route &) {
    skui::Text("Home");
});
router.Reset("/home");
// 后续每帧在宿主窗口中：
view.Draw(router);
```

上述初始化不要放在每帧函数里。页面回调内的 Push/Back/Replace/Reset 在当次绘制结束后提交。
`Router::Draw()` 是无动画核心；`RouterView::Draw()` 管理滚动区域和页面转场，
不要在其外重复包一个同用途的 BeginScroll。
第三个参数可以传顶栏回调，使标题和正文一起转场；与 Shell 配合时设置
`content_includes_header=true`，避免重复标题。

返回后业务值由宿主持有，页面滚动位置由 RouterView 保留。
旧页是绘制快照，不会重跑业务回调；快照引用的纹理须保持有效至转场完成。
快照不重放自定义 ImDrawCmd 回调。尺寸变化或中断绘制后会直接显示目标页。

## 面包屑路径

```cpp
static int current = 2;
const char *path[] = {"设备", "文档", "项目", "资料"};
if (skui::BreadcrumbBar("path", path, 4, &current)) {
    // 根据 current 导航；完整 path 可以保留，供用户再次进入后续节点。
}
// 禁用点击但保留横向滚动：最后一个参数传 false。
```

`current` 为高亮索引，与完整路径独立。负数或越界时无高亮、不自动定位；
点击有效节点会写入索引，仅索引发生变化时返回 true。首次显示立即定位高亮项，
之后切换时平滑滚动；用户拖动会取消定位，关闭 `Theme::animations` 后立即定位。
每项最长 160dp，超出单行省略；整体高 48dp，包含上下留白。字体与字重由宿主管理。

## 输入接入

已有输入后端继续通过 ImGui IO 投递按键、字符、鼠标/触摸事件。
键盘启用、数值 / 选项操作、焦点恢复和 Escape / Alt+Left 优先级见 [键盘与返回](keyboard.md)。
需要同步系统完整编辑状态时，使用 `SetTextInputHandler`，回调运行在渲染线程：

- 宿主持有 NUL 结尾的 UTF-8 缓冲区，容量包含结尾 NUL。
- 选区使用 UTF-8 字节偏移，回填不得拆开多字节字符或超出容量。
- 完整快照改写后设置 `changed=true`，完成/取消编辑设置 `finished=true`。
- 用户数据和回调必须保持有效；宿主销毁时解除回调，避免悬空引用。

Android 实现在 `src/Android_my_imgui/AndroidIme*.cpp` 和 `android/ime/`，不参与主机组件库构建。
临时 APK 的静默回退、输入期间保留和结束后卸载约定见 [Android Demo](android-demo.md)。

## 独立窗口、材质与设备反馈

`Window*` 的 Begin/End 与 Overlay 遵循相同规则，触发仍用 `OpenDialog`，关闭仍用
`CloseDialog`。独立宿主安装 `SetWindowRenderHandler`，在 `ImGui::Render()` 后、主绘制前调用
`RenderWindowLayers(ImGui::GetDrawData())`。每个非零 ID 接收一组保持屏幕坐标的绘制数据；
回调成功返回 true 才从主绘制移除该组。提交失败返回 false，主绘制保留内容。
每帧最后的 `(0, nullptr)` 通知宿主回收本帧未使用的窗口，关闭动画完成前不得提前销毁。
回调不能持有借用的 ImDrawData；宿主负责透明背景、正确层级、坐标和上下文、纹理共享。

Android Demo 已接入 Vulkan / OpenGL：每个活动弹层拥有独立 SurfaceControl / ANativeWindow
及交换链或 EGLSurface，触摸仍由现有输入桥统一处理。这不是 Activity 或 Android Dialog。
Surface 使用主画布尺寸，窗口内部透明；仍受私有 Surface 接口和 root 限制。
Vulkan 后端为安全复用上游顶点上传缓存，会在独立窗口提交之间等待队列，可能增加耗时。
`HasWindowBackend()` 可查询回调是否安装；没有适配时 Window 接口退回同画布 Overlay。
销毁前解除回调，再释放所有平台窗口；正常关闭、绘制失败和程序退出均须清理。

`GlassBackdrop` 由宿主持有，先在同帧已画好的背景上 Capture，再 Draw，最后画玻璃上方内容：

```cpp
// 持久对象；与 ImGui 上下文同线程，最好在上下文销毁之前释放。
skui::GlassBackdrop glass;
// 每帧在已有窗口内，先绘制背景，再使用实际屏幕坐标：
// glass.Capture(ImGui::GetWindowDrawList(), background_min, background_max);
// glass.Draw(panel_min, panel_max, skui::GlassStyle{});
```

每个对象每帧绘制一块玻璃；多块用多个对象。后端必须支持 ImGui 1.93 动态纹理协议
`RendererHasTextures`。Capture 冻结背景绘制数据，Draw 先按实际帧缓冲像素密度采样，
再做面积过滤和自适应降采样；模糊权重连续变化，零模糊保留细节。
采样和输出各最多 1,048,576 像素、长边 2048 像素。
模糊、折射均为 0 且饱和度为 1 时，同一画布中未改变的背景直接透出，只叠加色调与边框；
默认色调仍含 22% 白色，完全清透可将 `tint.w` 设为 0。读取 ImDrawList 的 CPU 纹理像素；
不支持 GPU 专有外部纹理、自定义绘制回调或跨画布桌面采样。模糊与边缘折射是有界 CPU 实现。
尺寸变化和销毁通过动态纹理协议退休资源。宿主必须继续驱动协议直至资源回收，不能提前卸载渲染器。

`TopAppBarState` 跨帧保存，把滚动区域的 `GetScrollY()` 写入 `scroll_offset`，由
`CollapsingTopAppBar` 在展开/折叠高度之间连续插值，同帧跟随滚动，不额外添加追赶动画；`collapse_range` 是逻辑单位，offset 为实际像素。
`Theme::motion_response/motion_damping` 调整路由弹簧，`motion_duration_scale` 调整共享状态、
路由和弹层时长；关闭 animations 则立即完成离散过渡。数字滚轮的惯性/吸附独立管理。

`ColorPicker(label, color, ColorSpace::OkHSV/OkLab/OkLCH)` 保留旧 HSV 接口；
`ToColorSpace/FromColorSpace` 的输入/输出在头文件注明，alpha 原样保留。
OkLab 的 a/b 和 OkLCH 的 C 可超出 sRGB 色域，显示时裁切到 [0,1]；编辑坐标跨帧保留，
外部改变 RGBA 后重新同步，不提供 HDR / 广色域输出。

`SetHapticHandler` 接收 Press / Selection / Confirm，库只发事件；宿主负责系统服务与节流。
触屏按钮激活自动发 Press，其他语义可调用 `PerformHaptic`；`Theme::haptics=false` 停用。
Android Demo 的适配使用 Android 12+ 的 vibrator_manager 命令、异步子进程和 45ms 节流，
不使用强制绕过勿扰选项，命令不可用时静默跳过。其他宿主可接自己的反馈 API。
回调在渲染线程执行，用户数据须保持有效，退出前解除回调。
