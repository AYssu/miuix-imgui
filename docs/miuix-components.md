# Miuix 组件对照

来源：官方仓库 `compose-miuix-ui/miuix` 的 `docs/zh_CN/components`，核对日期 2026-11-24。
这是 Dear ImGui / C++ 的对应交互实现，不是 Compose 库的逐参数移植。

Window 系列复用浮层交互，并由 Android Vulkan / OpenGL 后端为弹层创建独立透明 Surface。
它们不是 Android Activity 或系统 Dialog；窗口未接入或提交失败时保留 Overlay 绘制。
Demo 首页提供浮动选择、Glass 与 Window 分类的直接入口，目录支持中文 / 英文搜索。

| 官方名称 | C++ 对应接口 / 当前范围 |
| --- | --- |
| Scaffold | BeginShell / EndShell：内容、顶栏、紧凑导航、宽屏侧栏与返回 |
| Surface | BeginSurface / EndSurface：独立容器、背景色、尺寸、圆角 |
| TopAppBar | TopAppBar；CollapsingTopAppBar 通过滚动偏移折叠高度、标题字号和摘要 |
| NavigationBar | NavigationBar：独立横向导航与选中状态 |
| NavigationRail | NavigationRail：独立纵向导航、可选折叠按钮、宽度与图标/标签过渡；示例包含联动内容区，宽屏壳共用该组件 |
| TabRow | TabBar |
| BreadcrumbBar | 32dp 胶囊、浅色高亮、箭头分隔、名称省略、横向触摸滚动与高亮项定位 |
| Card | BeginCard / EndCard |
| BasicComponent | BasicComponent：标题、摘要、图标、尾部值与箭头 |
| Button | Button：Primary / Neutral / Danger、禁用与取消点击 |
| IconButton | IconButton |
| Text | Text / Caption / Heading |
| SmallTitle | Section |
| TextField | InputText：宿主可接系统 IME |
| Switch | Toggle |
| Checkbox | Checkbox |
| RadioButton | Radio |
| Slider | Slider：圆形滑块、圆角轨道，保留 ImGui 输入行为 |
| NumberPicker | NumberPicker：五行居中滚轮、字号 / 透明度渐变、惯性减速、松手弹簧吸附、点击邻项、步长 / 边界 / 外部值同步 |
| ProgressIndicator | Progress / CircularProgress：确定进度与不确定圆形进度 |
| Snackbar | Snackbar |
| Tooltip | Tooltip：悬停或长按 |
| Badge | AnchoredBadge：叠加到前一控件的圆点 / 数字；Badge 保留旧标签接口 |
| Icon | DrawIcon |
| FloatingActionButton | FloatingActionButton |
| FloatingToolbar | FloatingToolbar |
| Divider | Divider |
| PullToRefresh | BeginRefreshScroll / EndScroll：请求与刷新状态由调用方管理 |
| SearchBar | SearchField |
| ColorPalette | ColorPalette(id, color)：7×13 HSV / 灰度网格、同帧棋盘预览、连续透明度；旧重载保留预设色板 |
| ColorPicker | ColorPicker：连续 HSV 与 alpha 滑块、同帧更新预览、黑色状态保留色相 / 饱和度、红色色相端点不跳回；新增 OkHSV / OkLab / OkLCH，裁出 sRGB 色域时保留编辑坐标 |
| ArrowPreference | ArrowPreference：整行点击与摘要，调用方接路由 |
| SwitchPreference | SwitchPreference：开关与摘要 |
| CheckboxPreference | CheckboxPreference：复选与摘要 |
| RadioButtonPreference | RadioButtonPreference：单选与摘要 |
| SliderPreference | SliderPreference：标题、摘要与滑块 |
| RangeSliderPreference | RangeSliderPreference：标题、摘要与双端点滑块 |
| OverlayListPopup | ListPopup + OpenDialog：自定义触发器、图标、分组、禁用与选中项 |
| OverlayCascadingListPopup | ListPopup + PopupItem::children：原位二级切换、返回上级、叶子 action |
| OverlayDropdownPreference | Dropdown：整行触发、浮动单选、选中标记；独立示例比较 3 项短列表和 18 项长列表 |
| OverlaySpinnerPreference | Spinner：图标、摘要、禁用项；独立示例比较浮层、对话框和额外的 MultiSelect |
| OverlayDropdownMenu | DropdownMenu：动作菜单；额外多选接口为 MultiSelect |
| OverlayIconDropdownMenu | DropdownMenu(icon_only=true) |
| OverlayIconCascadingDropdownMenu | DropdownMenu(icon_only=true) + children |
| OverlayBottomSheet | BeginBottomSheet / EndBottomSheet：应用窗口底部面板 |
| OverlayDialog | OpenDialog / BeginDialog / EndDialog / CloseDialog |
| WindowListPopup | WindowListPopup：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowCascadingListPopup | WindowCascadingListPopup：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowDropdownPreference | WindowDropdownPreference：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowSpinnerPreference | WindowSpinnerPreference：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowDropdownMenu | WindowDropdownMenu：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowIconDropdownMenu | WindowIconDropdownMenu：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowIconCascadingDropdownMenu | WindowIconCascadingDropdownMenu：Android 独立 Surface；无窗口后端时回退 Overlay |
| WindowBottomSheet | BeginWindowBottomSheet / EndWindowBottomSheet |
| WindowDialog | BeginWindowDialog / EndWindowDialog |

上表 Overlay 项提供同一 Surface 内的对应交互，统一使用带遮罩的 ImGui modal popup、焦点与关闭机制，
不依赖 Compose 的 Scaffold / MiuixPopupHost；因此与官方承载机制也存在差异。
Android 宿主在浮层打开时扩大触摸捕获，让窗口外选项和点击外部收起正常工作。
短列表按内容自适应；长列表保留必要的滚动。缩放后的窗口 padding 对齐像素，
避免亚像素舍入导致只有三项也出现滚动条。选中与按压反馈使用圆角背景。
触屏按压底色在松手后用约 120ms 淡出，手指移出控件时取消；不会把最后的触摸位置当成持续悬停。
鼠标/笔悬停仍有反馈，业务选中项保留选中底色。

Overlay 全系列共用 32% 黑色遮罩，遮罩与内容随进退场渐变；背景在关闭动画完成前保持不可点击。
列表 / 下拉为轻微缩放与淡入淡出，对话框为居中缩放与淡入淡出，底部面板从底部滑入 / 滑出。
打开约 300ms；列表退出 220ms，对话框 / 面板退出 260ms，可由 motion_duration_scale 缩放。动画按帧时间推进；
`Theme::animations=false` 时立即显示 / 关闭。触发 action 和选中值仍在操作帧返回，
关闭统一调用 `CloseDialog()`，不要直接调用 ImGui::CloseCurrentPopup() 跳过退场。

新增 GlassBackdrop / GlassStyle（采样、模糊、边缘折射、饱和度、色调、圆角），
CollapsingTopAppBar / TopAppBarState，触觉回调及 Android 适配，以及高级颜色转换。
Glass 先按实际帧缓冲密度采样，面积过滤后再按模糊强度降采样，减少细纹理的混叠；
三次可分离盒式滤波使用连续的边缘权重，避免整数半径跳变。零模糊保留原始清晰度，
采样与输出各限制为 1M 像素 / 2048 像素长边。
无模糊、无折射且饱和度为 1 时，未改变的同画布背景直接透出；色调蒙层仍可独立调整。
仅处理当前 ImDrawList 中已有 CPU 像素的纹理；
不是 Compose RuntimeShader 的完整移植，也不采样系统桌面或外部 GPU 纹理。

导航使用本次核对的官方 NavProgrammaticEasing（参考提交 07d4859021d7aed1a1f78afaee12c5c8a6917466），
默认归一化 response=.8、damping=.95、500ms；Theme 提供响应、阻尼和时长倍率。
短反馈和布局折叠保留平缓的三次曲线，不复用导航曲线；组件仍有各自的惯性与吸附参数。
尚未逐项复制 Compose 的所有可配置参数、着色器及系统导航手势。

新增与修改组件的动画要求见 [组件开发规范](component-development.md)，宿主调用约定见 [接入规范](integration.md)。

## 面包屑

BreadcrumbBar 对照 Miuix 提交 `07d4859021d7aed1a1f78afaee12c5c8a6917466` 的同名组件：
胶囊高 32dp、文字 14dp、水平内边距 10dp、单项最大宽度 160dp；条带外留白水平 12dp / 垂直 8dp。
普通胶囊取正文色 10% 透明度，高亮取主题色 20% 透明度，文字分别为正文色 55% 与主题色。
箭头占宽 18dp，超长名称单行省略，长路径横向滚动且隐藏滚动条；当前字体由宿主管理，不自动加载 Medium 字重。
首次显示定位高亮项，改变高亮时平滑滚动，拖动会取消自动定位；关闭动画则立即定位。
不实现 Compose 的 LazyRow、边缘弹性及惯性滚动；短反馈与定位沿用本项目动画曲线。

## 浮动选择

```cpp
static int selected = 0;
const skui::ChoiceItem items[] = {
    {"跟随系统", "使用系统外观", skui::Icon::Settings},
    {"浅色", nullptr, skui::Icon::Sun},
    {"深色", nullptr, skui::Icon::Moon},
    {"不可用", nullptr, skui::Icon::Info, false}
};
if (skui::Dropdown("外观", &selected, items, 4)) {
    // selected 已更新，在这里保存业务设置。
}
```

## 自定义触发器与列表

```cpp
const skui::PopupItem items[] = {{1, "复制"}, {2, "分享"}, {3, "保存"}};
if (skui::Button("打开列表")) skui::OpenDialog("actions");
int action = skui::ListPopup("actions", items, 3);
if (action >= 0) { /* 执行所选操作 */ }
```

触发与 ListPopup 必须使用同一 ID 作用域，并在每帧调用 ListPopup。
ChoiceItem / PopupItem 及字符串、子数组只需在本次调用期间有效。
PopupItem::action 为调用方定义的非负 ID，-1 表示本帧没有操作。重复标签使用 PushID 区分。
普通控件遵守 BeginDisabled；NumberPicker 的外部值会被归一到合法步长。

BeginCard、BeginSurface、BeginScroll、BeginRefreshScroll 即使返回 false 也要调用相应 End；
Dialog / BottomSheet 只在 Begin 返回 true 时调用 End。AnchoredBadge 不改变前一控件的
ID、布局或点击区域，调用方应在图标等锚点之后立即调用。
