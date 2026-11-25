# 主题、设置和性能诊断

## 主题设计参数

`Theme` 保留原有浅深色、强调色、缩放、动画和触觉设置，增加 `light` / `dark_palette`
两套语义色与 `DesignTokens`。默认参数保持原有外观；库不创建字体或上下文。

```cpp
skui::Theme theme;
theme.dark = true;
theme.dark_palette.surface = ImVec4(.12f, .14f, .18f, 1);
theme.tokens.card_radius = 16;
theme.tokens.control_radius = 10;
theme.tokens.item_spacing = ImVec2(10, 12);
skui::ApplyTheme(theme); // 选择宿主上下文后、Begin 之前
```

颜色按未预乘的 sRGB RGBA 提供；透明度和分量限制到 `[0,1]`，非有限值恢复默认值。
设计尺寸使用逻辑单位，经 `Theme::scale` 缩放；圆角、间距等参数也会验证范围。
窗口 / 卡片 / 控件 / 弹窗、行反馈 / 菜单圆角、容器内边距、滚动条、禁用透明度和 Glass 阴影
各有参数。几何图标、滚轮选中带等有特定形状的元素继续保留自身尺寸。

`TextSize(design_size)` 为自绘文本应用 `tokens.text_scale` 和主题缩放；
Gallery 同时给宿主字体使用这个尺寸。外部宿主也应在绘制边界选择相应字体大小，
已有 `FontSizeBase` / `FontScaleMain` / `FontScaleDpi` 不会被 `ApplyTheme` 清除。
字号放大需要配合布局检查；它不等于完整的复杂文字排版支持。

局部覆盖使用 `ThemeScope`，作用域退出时恢复完整 ImGui 样式和先前主题，支持嵌套及异常恢复：

```cpp
auto local = skui::GetTheme();
local.accent = ImVec4(.8f, .25f, .35f, 1);
{
    skui::ThemeScope scope(local);
    skui::Button("局部操作");
}
```

在同一渲染线程和上下文中结束作用域；内部 Begin/End、Push/Pop 必须配对。
局部作用域会应用一套样式，应在容器或成组内容边界使用，避免为每个列表单元反复复制样式。

## Gallery 设置保存

文件访问属于 Gallery 宿主，组件库没有默认存储位置。Android 启动参数显式启用：

```bash
su -c '/path/to/sk-imgui-ui --settings /data/local/tmp/gallery-settings.cfg'
```

未传入 `--settings` 时不读写设置。传入后，启动时加载，设置页可以手动保存，正常退出时保存一次。
文件 I/O 在绘制与提交之后执行，平常帧只有待保存标志检查。
保存内容为深浅色、动画、触觉、强调色、动效参数、文字缩放、卡片与控件圆角、
宽屏预览、侧栏折叠和窗口几何；业务内容、输入文本和诊断历史不保存。
窗口位置 / 大小按可见屏幕归一化，键盘临时避让尺寸不写入设置。
配置中的几何不得把窗口放到屏幕之外；实际尺寸仍受 Shell 的最小尺寸约束。

`examples/gallery/preferences.cpp` 提供示例编解码和 POSIX 文件适配，文件上限 4096 字节、64 个字段。
缺少可选字段使用默认值；版本 0 接受已有字段并补默认值，保存时写版本 1；未知可选字段忽略，
未知版本、重复字段、非有限 / 越界值及错误格式拒绝，解析失败不修改传入对象。
读取失败会提示并使用默认设置，退出时保留原文件；用户明确点击保存后才尝试替换。

保存先在同目录创建独占临时文件，写入并 fsync，关闭后原子 rename。
创建、写入、同步、关闭或替换失败会清理临时文件，并保留先前的有效配置。
未承诺断电持久性：当前没有同步父目录；异常强制终止也可能遗留临时文件。
这份适配用于 Android / POSIX Gallery，其他宿主自行选择存储和文件接口。

## 性能采样

```cpp
#include <skui/diagnostics.h>
skui::FrameProfiler profiler;
profiler.SetEnabled(true); // 默认关闭
// 每帧：
profiler.BeginFrame();
// 输入、平台 NewFrame
profiler.EndInput();
// UI 构建
profiler.EndBuild();
// ImGui::Render、后端提交
profiler.EndFrame();
```

采样固定保留最近 240 帧。关闭时不读时钟，关闭或清空后丢弃旧历史；重新开启不会把关闭期间
算入第一帧间隔。采样、读取最近值和计算分位数均无动态内存分配；没有后台统计线程。
`Statistics()` 使用固定数组按最近秩计算 p50 / p95，调用成本受 240 帧上限约束；
诊断面板按 4 Hz 更新分位数，其他帧只读取最近值与计数。

记录输入 / UI / 渲染阶段、整帧工作耗时、帧间隔、顶点 / 索引、活动项 / 导航焦点、弹层数，
以及 ImGui 字体 / 材质纹理数量。纹理计数不包含宿主私有纹理，也不表示 GPU 显存字节数。
Glass 示例在诊断开启时额外记录 Capture + Draw，归入 UI 子项，避免重复计入总耗时。

这些时间由 steady_clock 测量，包含调用期间的阻塞 / 等待；渲染阶段是 CPU 侧后端、提交与 present
的经过时间，不能据此推断 GPU 执行耗时。GPU 时间需要后端时间戳支持，当前面板明确标注未提供。
帧间隔反映调度、同步和保存等帧外工作的影响，与各阶段工作耗时分别展示。

性能验证需固定设备、构建模式、工作负载、预热与采样数量。将新增功能的关 / 开成本分别测量，
同时检查画质、交互、资源回收；主机的 headless CPU 数字不能替代真机 GPU 帧时间和耗电验证。

仓库提供可复现的 CPU 探针，预热 100 帧，采样 3000 帧，每帧绘制 12 个按钮，另对比采样器关 / 开：

```bash
cmake -S . -B build/performance -DCMAKE_BUILD_TYPE=Release \
  -DSKUI_BUILD_ANDROID_DEMO=OFF -DSKUI_BUILD_BENCHMARKS=ON
cmake --build build/performance --parallel 4
build/performance/skui-foundation-benchmark
```

CI 的 GCC 任务输出同一探针的结果；共享 runner 的时间仅作为观察数据，不设置跨设备的绝对速度断言。
固定容量和采样 / 统计零分配由主机测试做确定性检查。基准源码位于 `benchmarks/foundations.cpp`。
