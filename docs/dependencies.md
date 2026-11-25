# 依赖版本与升级

## Dear ImGui

- 上游：<https://github.com/ocornut/imgui/releases/tag/v1.93.0>，稳定版本，2026-10-14 发布。
- 标签：`v1.93.0`，提交：`c1aaac65e896a45f81f47a4d68f9959e0f86c859`。
- 归档：<https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.93.0>。
- SHA-256：`f7bf6bc76e5d79d40d751096663e6c5d18daf08129dd52e7f7b09e4251c8eeee`。
- 许可：[Dear ImGui MIT](../LICENSES/Dear-ImGui-MIT.txt)；内嵌默认字体分别保留
  [ProggyClean](../LICENSES/ProggyClean-MIT.txt) 和 [ProggyForever](../LICENSES/ProggyForever-MIT.txt) 的 MIT 许可。
  后者许可原文取自 `ocornut/proggyforever` 提交 `f8868ba43bb42c4d88454d03ad81cbd139789f72` 的 `LICENSE.txt`。

`include/ImGui/` 和 `src/ImGui/` 中的核心、stb、标准 Android/SDL2/OpenGL/Vulkan 后端及
FreeType 适配均取自同一个标签，保持上游文件内容。这里只附带构建需要的源码和头文件，
不附带上游示例图片、字体文件或工具配置。Android 系统字体在设备端读取。

可选桌面 Gallery 使用宿主 SDL2 2.0.18+ 与 OpenGL，不让纯库 / headless 测试下载依赖。
SDL2 后端从上述已校验归档中提取，保持原文：`imgui_impl_sdl2.h` SHA-256 为
`d1d31f18c3375f6208a039c57a590b885e71f6af3565acbc990095fc5a08fb4e`，
`imgui_impl_sdl2.cpp` 为 `5f2319b0deebad511975517ac6e0b92a0c7dc0878f0213b50b368375310f2736`。

从早期 `1.92.0 WIP` 更新时，项目适配包括：

- 动态字体源数据由图集持有，直到字体清理或上下文销毁；不能在注册后立即释放。
- Gallery 使用 `PushFont(nullptr, size)` 指定字号；主题保留宿主的基础字号与缩放设置。
- Vulkan 明确使用应用已有的 API 1.1，并适配 `PipelineInfoMain`、交换链用途和纹理注册接口。
- 路由快照保留 `ImTextureRef`，并在动态字体后端下复制所需图集像素。
  新页面加载字形、图集重排或动画中途反向时，旧页面保持原来的 UV 与像素。
  快照结束后交由图集和渲染后端按协议回收纹理。
- 颜色预览区分颜色透明度与界面淡入淡出透明度；描边调用使用新的参数顺序。

路由快照仍不回放自定义绘制回调；宿主提供的应用纹理需要自行保持有效，直到过渡结束。
字体图集的清空、宿主上下文切换和路由绘制应在同一渲染线程上管理，
关闭时先停止绘制，再关闭渲染后端和 ImGui 上下文。

## 升级与回退流程

核心、`imgui_internal.h`、渲染后端与 FreeType 适配必须一起更新，不能混用不同版本。
外部 `SKUI_IMGUI_TARGET` 以本文件记录的版本为兼容基线，内部 API 不承诺跨版本兼容。
更新来源、提交、归档哈希及许可证后，在干净构建目录运行 [测试规范](testing.md) 中的
主机 GCC/Clang、Sanitizer、consumer 和 Android 构建检查。

主机测试同时保留旧式静态图集宿主和新版动态纹理协议路径；动态路径覆盖按需字号、
切页中重排、过渡中断、纹理回收和上下文销毁。真机另检查中文字体、路由、浮层及输入。
编译通过不能代替真实渲染和输入验证。

升级前的可回退基线为 `7e468b7`。需要回退时，应整体撤销依赖升级提交并重新构建，
同时回退其项目适配；不要只恢复 `imgui.h` 或继续使用旧构建产物。

FreeType 仍固定为 `VER-2-13-3`，源码归档与哈希记录于 CMake 和
[第三方说明](../THIRD_PARTY_NOTICES.md)，本次未改变其版本。

可机器读取的固定依赖见 [dependencies.lock.json](../dependencies.lock.json)，CI 用 `scripts/check-versions.py`
核对公开版本、构建声明和锁。桌面系统依赖只约束最低版本，实际二进制交付需记录宿主解析版本。
