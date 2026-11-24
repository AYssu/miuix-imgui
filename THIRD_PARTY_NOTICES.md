# 第三方许可与来源

根目录 [MIT 许可](LICENSE) 适用于项目作者有权授权的新增代码，包括 `include/skui/`、
`src/skui/`、Gallery、最小示例、测试及本项目新增的 IME 桥接、文档和构建脚本。
其中 SpringProgress 和颜色数学的第三方改编部分分别按下表许可处理。
它不覆盖第三方原始代码，也不为来源未确认的历史代码补授许可。
已有原始版权头和许可注释必须保留。

## 已确认的第三方组件

| 组件 | 位置/版本 | 许可与说明 |
| --- | --- | --- |
| Dear ImGui | include/ImGui 与 src/ImGui；上游 v1.93.0（c1aaac65e896a45f81f47a4d68f9959e0f86c859） | [MIT](LICENSES/Dear-ImGui-MIT.txt)，Omar Cornut；包含平台/渲染后端及 FreeType 适配。my_imgui_impl_android 也包含其 Android 后端派生实现 |
| stb | include/ImGui/imstb_*，include/My_Utils/stb_image，src/My_Utils/stb_image | 采用其双许可中的 [MIT](LICENSES/stb-MIT.txt)，Sean Barrett；原文件末尾保留许可 |
| ProggyClean | src/ImGui/imgui_draw.cpp 内嵌默认字体 | [MIT](LICENSES/ProggyClean-MIT.txt)，Tristan Grimmer |
| ProggyForever | src/ImGui/imgui_draw.cpp 内嵌可缩放默认字体 | [MIT](LICENSES/ProggyForever-MIT.txt)，Disco Hello、Tristan Grimmer |
| gl3w / Khronos GL 声明 | include/ImGui/backends/imgui_impl_opengl3_loader.h | gl3w 为 Unlicense，Khronos 部分为 MIT；许可原文保留在该文件头部 |
| Miuix 导航弹簧 | src/skui/skui.cpp 的 SpringProgress；参考 compose-miuix-ui/miuix 提交 07d4859021d7aed1a1f78afaee12c5c8a6917466 的 NavSettleEasing.kt | [Apache-2.0](LICENSES/Apache-2.0.txt)，Copyright 2026 compose-miuix-ui contributors；C++ 改编增加临界/过阻尼、参数边界与末帧归一 |
| Ok-color | src/skui/color.cpp；bottosson/bottosson.github.io 提交 d69831edb90ffdcd08b7e64da3c5405acd48ad2c 的 misc/colorpicker/colorconversion.js | [MIT](LICENSES/Ok-color-MIT.txt)，Copyright 2021 Björn Ottosson；C++ 改编采用数值求 cusp，增加 alpha 与 sRGB 裁切 |
| Google Vulkan wrapper | include/Android_Graphics/vulkan_wrapper.h、src/Android_Graphics/vulkan_wrapper.cpp | [Apache-2.0](LICENSES/Apache-2.0.txt)，Copyright 2016 Google Inc. |
| FreeType | 构建时获取 VER-2-13-3，不提交旧预编译归档 | 选择 [FreeType License](LICENSES/FreeType-FTL.txt)，另见 [许可汇总](LICENSES/FreeType-LICENSE.txt)、[BDF](LICENSES/FreeType-BDF.txt)、[PCF](LICENSES/FreeType-PCF.txt) 说明 |

This software uses the FreeType library. Portions of this software are copyright
© 1996–2024 The FreeType Project. All rights reserved.

FreeType 上游源码同时保留各子模块的原始许可；本项目关闭可选外部依赖，不修改其源码。
Dear ImGui 的日本字形范围数据在 `imgui_draw.cpp` 中另外保留了政府数据来源和 CC BY 4.0 说明。
附带的 ImGui 核心、标准后端与 FreeType 适配文件保持 v1.93.0 上游原文；
`Android_my_imgui` 等项目平台适配是独立的本地派生代码。

上游参考：

- Dear ImGui：<https://github.com/ocornut/imgui>（本次源码和许可均来自 v1.93.0，归档校验见 [依赖版本](docs/dependencies.md)）
- Miuix：<https://github.com/compose-miuix-ui/miuix/tree/07d4859021d7aed1a1f78afaee12c5c8a6917466>
- Ok-color：<https://github.com/bottosson/bottosson.github.io/tree/d69831edb90ffdcd08b7e64da3c5405acd48ad2c/misc/colorpicker>
- stb：<https://github.com/nothings/stb>
- FreeType：<https://github.com/freetype/freetype/tree/VER-2-13-3>
- FreeType 归档：<https://codeload.github.com/freetype/freetype/tar.gz/refs/tags/VER-2-13-3>
- Apache-2.0 原文：<https://www.apache.org/licenses/LICENSE-2.0.txt>

FreeType 下载归档 SHA-256：`bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103`。

## 历史 Android 适配代码

根据项目作者说明，旧悬浮窗口、触摸及部分平台适配代码**来源于网络**；
`include/native_surface/ANativeWindowCreator.h` 的 Android 17 适配由项目作者自行提供。
Android 17 适配的来源说明不是该系统上已完成测试的声明。

以下历史部分尚未获得可核验的原始上游、提交和许可：

- `include/native_surface/ANativeWindowCreator.h` 的历史基础实现。
- `include/Android_touch/`、`src/Android_touch/` 的历史实现。
- `include/Android_Graphics/`、`src/Android_Graphics/` 中除已标识 Google wrapper 之外的历史平台适配。
- `include/Android_my_imgui/`、`src/Android_my_imgui/`、`include/Android_draw/`、
  `src/Android_draw/` 的历史模板部分；本项目新增 IME 代码及已识别的 ImGui 派生部分分别按上文处理。

“来源于网络”仅记录目前已知来源，不等于开源许可证。
公开或再分发包含这些部分的完整源码/二进制前，应补充原始链接、作者、版本与许可授权，
或替换为来源和许可清楚的实现。当前清单不会将这些历史部分统一认定为 MIT。

## 本地保留但不进入仓库

原模板的 `include/ImGui/My_font/`（OPPO/Font Awesome 等字体与 SVG 归档）、
`include/My_Utils/My_icon/`、TouchHelper ZIP 备份、旧 FreeType 头文件和 `.a` 库
由 `.gitignore` 排除，当前构建不引用它们。它们没有被删除，也不属于源码发布内容。
Demo 在设备上读取系统字体，不把系统字体复制到发布包。

Miuix / HyperOS 是设计参考，名称与标识不因本项目 MIT 许可而取得授权。

## 更新要求

新增或升级依赖时记录来源、版本/提交、校验值、许可原文、本地修改和用途；
源码和二进制分发均携带本文件及相关许可。不要用忽略规则隐藏仍被构建使用的必要源码。
