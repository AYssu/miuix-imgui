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
当前基于附带的 `1.92.0 WIP` 内部 API，其他版本不保证直接兼容。
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
`Theme::scale` 缩放逻辑尺寸，宿主同时配置字体大小或 `FontGlobalScale`；
使用 `ImVec2` 屏幕坐标的接口按头文件注释传入实际像素。
`ShellOptions::initial_position/initial_size` 和悬浮操作位置均为实际屏幕像素。

标签/ID 应在组件存活期间稳定，同名组件通过 `PushID/PopID` 或 `##suffix` 区分。
不要每帧改变 ID，也不要用临时局部变量重置选中项、侧栏状态或 Router。
传入缓冲区、选项和回调的数据在调用时必须有效；由宿主负责生命周期。

## 容器与弹层配对

| Begin 接口 | 配对规则 |
| --- | --- |
| BeginCard / BeginSurface / BeginScroll / BeginShell | 无论返回 true/false，均调用对应 End |
| BeginRefreshScroll | 无论返回 true/false，均调用 EndScroll；`refreshing` 和请求处理由宿主负责 |
| BeginDialog / BeginBottomSheet | 仅返回 true 时调用 EndDialog / EndBottomSheet |

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

## 输入接入

已有输入后端继续通过 ImGui IO 投递按键、字符、鼠标/触摸事件。
需要同步系统完整编辑状态时，使用 `SetTextInputHandler`，回调运行在渲染线程：

- 宿主持有 NUL 结尾的 UTF-8 缓冲区，容量包含结尾 NUL。
- 选区使用 UTF-8 字节偏移，回填不得拆开多字节字符或超出容量。
- 完整快照改写后设置 `changed=true`，完成/取消编辑设置 `finished=true`。
- 用户数据和回调必须保持有效；宿主销毁时解除回调，避免悬空引用。

Android 实现在 `src/Android_my_imgui/AndroidIme*.cpp` 和 `android/ime/`，不参与主机组件库构建。
临时 APK 的静默回退、输入期间保留和结束后卸载约定见 [Android Demo](android-demo.md)。
