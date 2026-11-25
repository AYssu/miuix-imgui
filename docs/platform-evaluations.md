# 平台与开发工具专项评估

核对日期：2026-11-25。本页记录本轮路线图中三个“单独评估”项目的决定、接入边界和验收条件。
它们没有在本轮变成 GPU 特效后端、屏幕阅读器桥或可视化编辑器；已有能力与提案分别说明。

## GPU Glass 与回退

**决定：保留 CPU 路径为现有实现；GPU 作为渲染宿主的可选扩展推进，尚不默认启用或冻结公开 ABI。**
依据 [Glass 实现](../src/skui/glass.cpp) 和 [像素 / 生命周期回归](../tests/advanced_tests.cpp)：
Capture 冻结当前 draw list 的命令 / 顶点 / 索引，在同帧 Draw 中进行 CPU 栅格化、面积过滤与模糊，
结果通过 ImGui 动态纹理协议上传。零模糊 / 零折射 / 中性饱和度且背景未改变时直接沿用已绘背景，
避免先缩小再放大损伤细线。采样像素限制为 1M、单边 2048，不代表总内存只有 4MiB；
浮点像素、滤波临时区、冻结网格和输出纹理都需要预算。

CPU 栅格器只采样具有 CPU Pixels 的纹理，跳过自定义 draw callback 与 GPU-only 图片，
不能把当前 CPU 路径视为任意宿主画面的完整帧缓冲模糊。
没有同步 GPU readback、GPU 时间戳或通用离屏渲染接口。

新的 [Glass CPU 探针](../benchmarks/glass.cpp) 用 80 条细线背景和 3 种面积 × 4 种模糊强度，
分别报告 Capture + Draw 的 p50 / p95；不含 GPU 上传 / 提交与电池测量。

```bash
cmake -S . -B build/performance -DCMAKE_BUILD_TYPE=Release -DSKUI_BUILD_BENCHMARKS=ON
cmake --build build/performance --parallel 4
taskset -c 0 build/performance/skui-glass-benchmark # taskset 为 Linux 可选项。
```

本地主机 GCC 16.2.1 / Release / CPU 0 的一次样本（单位 ms）：

| 逻辑面积，密度 1 | blur 4 p50 / p95 | blur 12 p50 / p95 |
| --- | --- | --- |
| 160 × 100 | 0.377 / 0.394 | 0.381 / 0.398 |
| 360 × 220 | 4.086 / 4.212 | 2.902 / 2.941 |
| 680 × 420 | 12.646 / 12.820 | 5.804 / 5.904 |

该合成场景用于发现成本随面积 / 分辨率的变化，数值不是 Android 真机或完整帧预算。
面积较大、模糊较轻时仍需处理较高分辨率，不能认为“模糊数值小所以一定便宜”。
每个材质由宿主结合目标设备预算选择面积、更新频率和是否显示；本轮没有根据主机 CPU 时间虚构 GPU 收益。

GPU 接入在实现前需要明确以下契约：

| 边界 | 拟采用的规则 |
| --- | --- |
| 捕获时点 | 宿主在背景绘制完成、Glass / 前景开始前生成 backdrop；不得把 Glass 自身递归采样进去 |
| 输入描述 | 物理区域、FramebufferScale、裁剪、有效 UV、色彩空间、预乘 alpha、背景 revision 和 context generation 都显式传入 |
| 调用线程 | UI 线程只提交请求；GPU 操作在宿主渲染线程 / 当前有效 context 中执行，不在组件函数中偷偷调用特定 GL API |
| 成功 / 失败 | 不支持 / 尺寸超限 / 创建失败 / context 丢失均有明确失败值；保持可继续绘制的当前帧 |
| 所有权 | 输出是宿主拥有的纹理 lease；组件仅借用；引用必须跨上传、主帧提交、Window 层和路由快照保持有效 |
| 退休 | 按 GPU fence / 完成帧回收；销毁或切换 context 时排空自己的请求并使旧 generation 失效，不能仅等 C++ 指针析构 |
| 回退 | 有可用 CPU 源时使用现有 CPU 路径；只有 GPU 源而 GPU 失败时退到主题 surface / tint，不同步读回整帧阻塞输入 |
| 缓存 | key 含尺寸 / 密度 / 背景 revision / 样式；静止背景可复用，内容变化明确失效；缓存和在途资源必须有总预算 |

现有路由快照不重放自定义 draw callback，Window 提交也有独立生命周期；
因此仅追加一个 shader callback 不足以正确支持全部场景，先解决纹理 lease 和快照边界再做后端。

进入默认路径前的门槛：0 / 轻 / 重模糊、透明边缘 / 细条纹 / 移动背景、截图坐标 / DPI、
路由中断、Window 关闭、context 丢失 / 重建、内存失败与回退都通过；
至少在当前 Android Vulkan 和一个桌面 GL 宿主比较 CPU / GPU 时间及内存峰值。
候选路径必须以 60Hz 的总帧预算评估，不能用异步提交很快代替 GPU 完成时间。
**本轮结论为接入协议与测量入口就绪，GPU 实现未交付。**

## 屏幕阅读器与复杂文字

**决定：键盘 / 翻译 / 字体与系统可访问性分别验收；现有库不声明 TalkBack、双向文本或复杂字形塑形支持。**

现有 [键盘协议](keyboard.md) 已提供可导航控件与返回优先级，
[字体规范](layout-and-fonts.md) 明确覆盖、动态字号和所有权。
`InspectLastItem` 是有界、可关闭的开发记录，缺少完整 role / value / actions / announcement 语义，
不能直接当成无障碍树。当前 FreeType 适配按码点取 glyph，增加字体文件也不会自动建立段落双向分析与塑形管线。

原生悬浮 Demo 不是普通 Activity 中的 View 控件树；临时 IME Activity 的输入连接仅服务编辑，
用完卸载也不会让主画布自动获得完整语义。需要屏幕阅读器的宿主应提供真实平台窗口与语义桥，
例如 Android 自绘 View 对应虚拟节点的宿主，而非把 root 进程的悬浮 Surface 当成已有可访问性节点。

可独立实施的宿主契约为：稳定节点 ID、role / label / hint / 当前值 / enabled / checked / range、
裁剪后屏幕边界、父子顺序与焦点；平台动作排队投递到 UI 线程，由下一帧确认结果。
滚动虚拟列表需保留逻辑节点与可见节点的区别，关闭弹层恢复原焦点；不在工作线程直接修改 ImGui 状态。
状态变更合并后通知，避免逐帧重建并广播全部节点。输入框的敏感值由应用决定是否暴露。

复杂文字管线应独立处理段落方向、脚本 / 字体分段、塑形、字形缓存、光标 / 选择的逻辑与视觉映射，
最后接入排版 / 命中测试；不能只倒序字符串或替换一张字体图集。
HarfBuzz / 双向分析工具属于候选依赖，本轮未增加依赖，也未把单纯 RTL 对齐当成完成排版。

验收样例至少覆盖 TalkBack 遍历 / 操作 / 弹层焦点恢复、字号与虚拟长列表，
以及阿拉伯语连写、印地语重排、混合中英文 / RTL / 数字、组合字符与 emoji 的编辑 / 选择 / 删除。
分别记录语言、字体和实际平台；通过键盘 headless 测试不能代替这些验收。

## 可视化编辑、代码生成与主题预览

**决定：现有主题实时预览与交互检查作为第一阶段；不将任意 C++ 业务热替换纳入库的默认能力。**

当前 Gallery 可即时修改主题颜色 / 圆角 / 字号，并显式保存和重启恢复；
`ThemeScope` 可用于可取消的参数预览。开发者可用语义标记和原生 ImGui ID Stack 定位真实控件。
这些已经支持“修改参数 → 看效果 → 撤销 / 保存”的工作流，无需每帧监视磁盘或增加编译服务。

同类项目的应用启动、布局、测试和编辑器思路见 [路线图参考](roadmap.md)。
本轮只参考功能边界，没有把 ImRAD / Slint 等编辑器源码或运行时加入本库。
后续若需要可视化编辑，建议先选择有限的静态页面与 token schema，再决定采用外部编辑器的自定义控件接入
还是单独生成器；不要维护无法往返还原的整份业务 C++ AST。

可交付的最小生成边界应是：

- 版本化页面 schema，包含稳定 ID、布局约束、主题 token 和已注册控件属性；禁用任意脚本执行。
- 仅生成独立的 UI 绘制片段 / 绑定声明；业务数据与回调留在应用拥有的文件，不覆盖手写实现。
- 校验未知组件、重复 ID、越界属性和 schema 升级；相同输入确定性生成、diff 可读。
- 预览使用实际 `skui::ui` 与可选桌面 Gallery 宿主；复用真实字体、布局和状态协议。
- 文件热预览按宿主明确请求或事件通知加载，先解析 / 校验临时对象，成功才原子切换，失败保留当前有效主题。

验收为导出后可直接编译 / 独立消费、交互与代码页面一致、再次导入不丢稳定 ID，
以及大字号 / 窄屏 / 禁用 / 弹层 / 错误输入的预览。未定义 schema 前不承诺通用编辑器或 C++ 热重载。

## 资料入口

- [Android 自定义 View 的可访问性](https://developer.android.com/guide/topics/ui/accessibility/custom-views)
- [HarfBuzz 官方文档](https://harfbuzz.github.io/)
- [ImGui 字体说明](https://github.com/ocornut/imgui/blob/v1.93.0/docs/FONTS.md)
- [ImRAD](https://github.com/tpecholt/imrad)、[Hello ImGui](https://github.com/pthom/hello_imgui)

评估的完成表示范围、所有权与验收已明确，不表示上述平台扩展已经实现或测试通过。
