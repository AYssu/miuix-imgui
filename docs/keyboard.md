# 键盘、焦点与返回

宿主启用 `ImGuiConfigFlags_NavEnableKeyboard`，并通过 ImGui 后端或 `io.AddKeyEvent`、
`io.AddInputCharactersUTF8` 投递输入。库沿用 ImGui 的焦点与快捷键路由，不创建第二套输入队列。
键盘事件和鼠标事件一样，需要唤醒宿主绘制；线程与闲置约定见 [运行调度](runtime.md)。

## 操作约定

| 控件 | 操作 |
| --- | --- |
| 按钮、图标、设置项、开关、单选 / 多选、色块 | Tab / Shift+Tab 移动，Enter / Space 操作 |
| NavigationBar / NavigationRail / TabBar / BreadcrumbBar | Tab 或对应方向键移动，Enter / Space 选择 |
| Slider / ColorPicker | 沿用 ImGui 滑块操作；Space 调节，方向键改变值，Enter 可进入数值输入 |
| RangeSlider | 下限、上限各一个焦点；Enter / Space 开始调节，左右键改变当前值，Enter / Escape 结束；两端不交叉 |
| NumberPicker | 一个焦点；Enter / Space 调节，上 / 下增加或减少一个 step，Shift 加速为 10 步；Enter / Escape 结束 |
| ColorPalette 网格 | 一个焦点；Enter / Space 开始，方向键选择颜色，保留透明度；Enter / Escape 结束 |
| Dropdown / Spinner / 列表弹窗 | 打开时定位当前有效选项，方向键移动，Enter / Space 选择；禁用项跳过 |
| 独立的浮动工具窗口 | 使用 ImGui 的 Ctrl+Tab 切换窗口，再操作其中按钮 |

NumberPicker 使用 64 位中间索引计算键盘步进，公开的整数值不会经过绝对 float 索引。
Card、自适应高度 Surface 和数字滚轮内部容器共享上层焦点顺序，不额外要求用户确认进入容器。
业务 ID 应保持稳定；动态列表删除当前焦点项时，宿主应使用 `SetKeyboardFocusHere` 指定新的合理目标。

## 返回优先级

Escape 先结束当前编辑，再关闭最上层弹窗，最后由 `RouterView` 返回上一页。
输入框使用 ImGui 的取消语义；数值和调色盘结束调节时保留当前值。
弹窗通过有归属的快捷键关闭，退出期间继续阻挡背景交互，结束后恢复打开它的控件焦点。
不会把同一次 Escape 同时用于关闭弹窗和返回页面。

`RouterView` 在启用键盘导航、页面处于焦点范围且没有编辑 / 弹窗 / 转场时响应 Escape 或 Alt+Left。
根页面不退出应用。转场结束后进入目标内容的焦点，返回时复用该页面的 ImGui 焦点和滚动记录。
直接使用 `Router::Draw` 的宿主自行绑定返回；宿主的全局快捷键可用 `ImGui::Shortcut`，
避免用无归属的 `IsKeyPressed` 抢走输入框或弹窗正在处理的按键。

平台 Back 可以由宿主映射为 Escape，或在平台 IME 消费后调用自己的返回分派。
当前 Android 原生悬浮 Demo 的底层输入仍以触摸和 IME 为主；这里不宣称已经接通并实测所有 Android
硬件键盘 / 系统返回事件。组件键盘协议由 GCC、Clang、Sanitizer 和 Android headless 合成事件验证。
键盘焦点不等于屏幕阅读器语义，相关平台能力单独评估。

## 回归

`skui-keyboard-tests` 发送实际 ImGui IO 按键与字符事件，检查 Tab / 反向 Tab、方向导航、
禁用跳过、选择提交、数值范围与整数精度、调色盘、短 / 长选项列表、输入取消、弹窗与路由返回优先级、
退出隔离和焦点恢复。测试用 headless 纹理后端，不依赖截图或 Android SDK。
原有触摸、拖动、滚轮和取消手势回归继续保留。
