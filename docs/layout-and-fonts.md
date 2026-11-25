# 自适应布局与字体

首页“自适应与字体”和布局页的同名入口演示动态字号、较长中英文文案、自动换列和键盘避让。
库不读取系统窗口或输入法信息；宿主提供实际像素尺寸与安全区域。

## 宿主边界

`SafeInsets` 是相对 main viewport 四边的物理像素留白。`ResolveViewport(display, insets, keyboard_height)`
返回可用矩形，底部安全留白与键盘取最大值，避免重复扣除。无效负数 / 非有限值按零处理，
超大留白截断到显示范围；`FitWindow` 将位置、大小及最小尺寸约束在该矩形内。

```cpp
skui::ShellOptions shell;
shell.safe_insets = {left_px, top_px, right_px, bottom_px};
shell.visible_bottom = keyboard_height_px > 0 ? display_height_px - keyboard_height_px : 0;
if (skui::BeginShell(shell)) DrawPage();
skui::EndShell();
```

Shell 默认在首次出现、移动、缩放和显示尺寸变化时约束几何，并在 `Begin` 前处理键盘限制，
使背景、裁剪与点击区域从首帧一致。键盘关闭恢复原窗口几何；若屏幕已旋转，恢复值仍受新边界约束。
空间小于正常最小尺寸时以可用空间为准。`constrain_to_viewport=false` 适合允许窗口越界的宿主。
`visible_bottom` 使用绝对屏幕 Y；传零取消键盘限制。Gallery 的 `Draw` 也接受可选 `SafeInsets`。
包含活动输入框的自动高度 Card / Surface 在暂时被裁剪时继续测量，直到父级滚动把输入框带回视野，
避免键盘缩小窗口后容器被跳过、编辑会话随即结束。其他容器保留正常裁剪优化。

原生 Android 悬浮 Demo 尚不读取系统 WindowInsets，默认四边为零；系统状态栏 / 手势区域由宿主
桥接或调用方传入，不能把默认零值当成已经识别设备刘海。现有默认悬浮位置留有屏幕边距。

## 换列与字号

`ClassifyWidth(width_px)` 按当前 `Theme::scale` 的 600 / 840 逻辑单位区分 Compact / Medium / Expanded。
这是应用选择布局的入口，现有 Shell 的侧栏切换仍保留 700 逻辑单位阈值。
`AdaptiveColumns` 依据可用宽度、逻辑最小列宽、间距和大于 1 的文字倍率计算 1～16 列。
`BeginAdaptiveGrid` 使用 ImGui table；返回 true 后逐个调用 `NextAdaptiveCell`，最后 `EndAdaptiveGrid`。

```cpp
if (skui::BeginAdaptiveGrid("settings", 260, 3)) {
    for (int i = 0; i < count; ++i) {
        skui::NextAdaptiveCell();
        ImGui::PushID(records[i].stable_id);
        DrawCard(records[i]);
        ImGui::PopID();
    }
    skui::EndAdaptiveGrid();
}
```

固定宽度按钮和单行标签仍需要调用方检查最长文案。常用 Toggle / Checkbox / Radio / ListItem /
BasicComponent 及 Preference 行按实际换行高度增高，为右侧状态和操作保留空间。
顶栏标题 / 副标题也按换行与字号增高；标题、分节和默认按钮高度容纳较大字号。
表格行高取本行最高单元格，卡片本身按内容增高；固定行高的虚拟列表必须确保实际行高与裁剪器一致。

`Theme::tokens.text_scale` 影响自绘文字；宿主的普通 ImGui 字体也要相应调整：

```cpp
skui::ThemeScope local(theme);
ImGui::PushFont(nullptr, skui::TextSize(16));
DrawContent();
ImGui::PopFont();
```

`Theme::scale` 是逻辑尺寸到像素的比例；文字倍率单独控制字号。不要再同时用多个全局字体倍率
重复放大同一因素。主题保留宿主 `FontSizeBase`、`FontScaleMain` 和 `FontScaleDpi`。

## 字体、资源与语言所有权

- 宿主持有 ImGui context、字体来源、翻译表及渲染纹理；本库不在组件内读取字体文件或每帧重建 atlas。
- 字体覆盖应包含应用语言、符号及所需字重。可以使用 ImGui 的字体合并机制追加回退字体；
  选择授权允许分发的字体并携带许可。Android 示例加载系统可用字体，不随库分发系统字体。
- `AddFontFromMemoryTTF` 默认将数据所有权交给 atlas；静态 / 外部所有权数据需显式配置
  `FontDataOwnedByAtlas=false`，并保持源数据有效至对应字体销毁。
- ImGui 1.93 按字号和缺失字符动态烘焙。渲染后端须处理纹理创建、更新与销毁，遵守
  `Renderer_TextureMaxWidth/Height`，不能只上传启动时的一次 atlas。不要为连续滑块的每个浮点字号创建新字体对象。
- 新语言或字号首次使用可能增加烘焙与上传开销。选择有限的常用字号 / 字体集合，观察诊断面板的
  atlas 纹理数及 CPU 耗时；大文件、整套 CJK 字体和缓存预算由宿主决定。
- 图片 / GPU 纹理由宿主缓存并释放，组件只借用传入的纹理标识。路由转场快照仍可能引用旧页纹理，
  应在相关转场结束后释放；销毁 context 前让后端完成退休纹理处理。
- 翻译文案和 UI 数据应在渲染线程切换，业务 ID 保持稳定。字体覆盖、字符串翻译和换行各有职责；
  此处没有实现屏幕阅读器语义桥或双向 / 复杂文字排版。

## 回归

`skui-runtime-tests` 检查安全留白与键盘重叠、异常参数、首次已有键盘时的恢复、横竖屏边界，
以及宽度断点、大字号换列、270 / 360 / 600 像素窗口中的长文案行高和真实顶点边界、顶栏换行。
动态字体 / atlas 退休与路由中字号变化另见现有高级绘制回归。
回归先复现了“缩小窗口后卡片被跳过、活动编辑丢失”，修复后检查每帧仍收到活动输入回调，
并且字段滚动到父窗口可见边界内。

2026-11-25，小米 Android 13 / Vulkan 真机验证约 1.49 倍文字、长英文换行、卡片操作和滚动；
页面底部卡片的系统输入框保持键盘打开，输入回填后关闭键盘并恢复窗口。
临时忽略桌面固定方向请求后，实际从 1080×2400 旋转到 2400×1080，窗口限制在屏幕内、导航和内容仍可见；
随后恢复竖屏、原方向策略并退出。正常日志 0 字节，本轮实例与上传文件已清理。

Release CPU 探针新增 100000 条固定高度记录场景（100 帧预热、3000 帧采样、仅提交 17 行）。
在同一 Linux 主机、同一 CPU 核和 GCC Release 下，将相同探针分别链接 `2154d3f` 的旧库与当前库：
三组旧库 p50 为 6.74～6.88 微秒，当前为 8.77～8.89 微秒，约增加 2 微秒/帧用于行文本测量等改动。
这是 headless 绘制 CPU 耗时，纹理更新仅模拟确认，不计 GPU / 上传；不是 Android 帧时间或功耗。
