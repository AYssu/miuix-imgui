# 交互检查与动画调试

这些工具面向应用开发，默认关闭，不修改 ImGui 的输入队列或系统时间。
Gallery 的“设置 → 开发工具 → 交互检查”可打开独立面板；也可以直接使用 `skui/devtools.h`。

## 标记、检查与定位

在目标控件绘制后立即给最后一项标记稳定路径：

```cpp
skui::SetItemInspectionEnabled(true); // 在宿主初始化或用户开启调试时调用。
skui::Toggle(translated_title, &notifications);
skui::InspectLastItem("settings/notifications");
// 在单独的开发窗口中：
skui::DrawInteractionInspector();
```

记录控件 / 窗口 ID、原始矩形、裁剪后的命中区域、帧号、禁用 / 可见 / 活动 / 焦点 / 悬停状态。
悬停面板条目时高亮实际矩形。面板还显示当前活动与焦点 ID、弹层深度、输入捕获及最近输入摘要。
Gallery 的开关可另外打开 ImGui 原生 Metrics 与 ID Stack 窗口，用于检查未标记控件和完整 ID 栈；
没有启用会主动中断调试器的 Item Picker。

路径由应用指定，不随翻译或控件屏幕位置改变；动态列表使用业务稳定 ID，避免用当前行号。
同一帧重复路径标记为歧义并拒绝查找，空路径、超过 95 字节的路径、无 ID 的最后项也拒绝。
只表示显式标记的项目，不能把数量当作全页面控件总数。Gallery 当前标记外观的三个开关，
其他宿主 / 测试页面按需要添加标记，组件库不会自动保存所有业务标签。

每个 ImGui context 最多 128 项，每帧重置；输入环最多保留 64 条，单帧也最多处理最后 64 个事件。
输入摘要只保留指针 / 按钮 / 滚轮、导航键及文本事件类型，省略文本内容和字符键名称。
这些数据仅驻留内存，没有自动日志或文件导出。`SetItemInspectionEnabled(false)` 释放记录和 hooks，
context 销毁同样回收；启用后的标记和环记录没有每项动态分配。

## 展示动画的时间控制

```cpp
skui::SetAnimationPlayback({.25f, false}); // 四分之一速度。
skui::SetAnimationPlayback({1.f, true});  // 暂停。
skui::StepAnimations();                 // 下一帧推进 1/60 秒，随后继续暂停。
skui::SetAnimationPlayback({});          // 恢复真实速度。
```

配置按 context 保存，下一帧采用；同一帧多次绘制读取同一个动画步长。
速度限 0.1–4，非有限值恢复 1；单步累加最多 0.5 秒，负数 / 非有限步长忽略。
暂停的展示动画停止请求连续重绘，真实输入与宿主其他重绘理由仍有效。
禁用 Theme 动画时仍立即采用最终状态。

覆盖短反馈 / 展开折叠、弹层进退、子菜单、面包屑定位、路由过渡、Snackbar 淡入淡出和不确定进度旋转。
NumberPicker 手势 / 惯性、滚动与按键重复、Tooltip 的触发延迟、Snackbar 的显示期限、任务进度、
平台输入法和真实 IO `DeltaTime` / `GetTime` 保持真实时间。逐帧功能不承诺冻结整个应用或 GPU。
Gallery 动画控件和 ImGui 原生调试窗口使用不同机制，后者不受此时间控制。

## 语义交互回归

[测试驱动](../tests/support/interaction.h) 是应用拥有的轻量测试辅助；不增加 Dear ImGui Test Engine 依赖。
调用者提供完整 NewFrame / 绘制 / Render 回调，以路径查找当前帧项目并注入真实 ImGui IO 事件：

```cpp
skui_test::Interaction ui(draw_one_frame);
ui.Click("settings/notifications");
ui.Key(ImGuiKey_Escape);
ui.WaitUntil("transition complete", [&] { return !view.IsTransitioning(); });
```

驱动拒绝歧义、陈旧、裁剪不可见或禁用项目，点击裁剪后的命中区域；按下后核对 ID 是否变化。
所有等待都有帧数上限，失败报告路径、帧号和容量统计。不可见的滚动项目由应用先滚入视野，
没有为测试强制调用业务回调或绕过弹层 / 路由输入屏蔽。

`skui-devtools-tests` 检查暂停 / 慢放 / 单步与 context 隔离、上限与重复路径、记录释放，以及
改名 / 缩放后的开关、弹窗、路由和返回完整交互。
原有键盘回归继续验证 Tab / 编辑 / Escape 优先级；这些 headless 事件不替代平台硬件与画质检查。

## 验证与成本

2026-11-25：GCC / Clang 六组主机回归、Clang ASan / UBSan、Linux offscreen Gallery 和
小米 Android 13 原生 devtools 测试通过；原生测试退出后确认进程消失并删除测试文件。
测试包含真实弹层不透明度冻结 / 单步，以及路由暂停期间不请求连续展示帧。
100 个显式标记的单项记录在开 / 关两条路径均无 ImGui 堆分配；总量和输入环有固定上限。

Release 探针已增加相同 12 按钮的标记开 / 关场景，CI 输出 p50 / p95；本地主机样本约
0.038–0.042ms / 帧，受时钟和负载影响，不作为设备性能门槛，也不包含 GPU / 字体上传 / 文件访问。
统计默认关闭，发布应用可完全不调用这些接口。
