# 兼容性与验证矩阵

核对日期：2026-11-26。以下按实际证据区分“编译 / 运行 / 协议回归”，不以单台真机代表整个系统版本。
公开 C++17 库与 Android 原生悬浮 Demo 的要求不同，后者依赖设备 root 与非公开平台接口。

| 环境 | 构建 / 配置 | 当前证据 | 边界 |
| --- | --- | --- | --- |
| Linux 组件库 | 本地 GCC 16.2.1 / Clang 23.1.5；CI Ubuntu 24.04 自带 GCC / Clang | 七组 headless CTest、consumer、安装包移动前缀后运行 | 合成输入，不代表平台硬件 / 屏幕阅读器 |
| Linux 检测构建 | Clang ASan / UBSan、leak 检测 | 七组测试通过；context / 纹理 / 线程回收覆盖 | 没有 GPU 驱动检测器 |
| Linux 桌面 Gallery | SDL2 2.0.18+ / OpenGL3；本地 offscreen、CI Xvfb / Mesa | 实际 GL 绘制、设置重启、失败与退出检查 | 其他桌面输入法、所有 DPI 环境仍需测试 |
| Android 库 / Gallery | NDK 28.2.13676358、API 28、arm64-v8a、静态 libc++、JDK17 / SDK36 | CI 编译，签名辅助包、16KiB ELF 对齐、安装包交叉消费验证 | 交叉编译不能证明所有设备可运行 |
| 小米 Android 13 / MIUI | 原生 Vulkan Gallery、OpenGL 历史回归、系统字体 / 讯飞输入法 | 触摸 / 中文回填 / 临时 APK 回退；主题 / 设置 / 布局 / 长列表；原生组件、运行与键盘 / 调试协议测试 | 硬件键盘 / 系统 Back 桥接未实测；原生 headless 测试不计作真机画质测试 |
| Android 16 / 一加 PLQ110 | direct `app_process` 会话被拒绝（Unknown pid），使用临时 APK / 搜狗 OEM 输入法 | Vulkan Gallery 验证竖屏转横屏后的搜索、卡片文本回填、编辑中左右横屏切换、返回卸载 | 未覆盖该机全部组件、其他输入法或完整 IME 故障矩阵；见 [测试记录](testing.md) |
| Android 17 / 其他 ROM | 有历史平台适配代码 | 未在本轮建立运行证据 | 不能承诺兼容 |
| macOS | 可选桌面示例的 SDL2 / GL3.2 分支 | 源码分支存在，尚未构建 / 运行 | 不计入已支持的运行矩阵 |
| Windows | 核心库没有 SDL / POSIX 依赖；桌面示例明确限制 POSIX | 尚未建立编译 / 运行证据 | 示例设置实现需要平台适配 |
| 其他 ABI / 图形后端 | CMake 可选择 Android ABI；宿主拥有渲染器 | 本轮仅 arm64-v8a 建立完整构建记录 | 按 ABI / 后端分别验证 |

ImGui 固定为 [1.93.0](dependencies.md)。随库 ImGui 与宿主 ImGui 安装包分别验证；
宿主提供目标时不能再次链接第二份 ImGui，内部 API 不承诺任意版本兼容。
仅路由组件不要求 ImGui，见 [安装包边界](packaging.md)。

动态图集宿主必须支持 1.93 纹理生命周期；应用自有纹理需保持到路由快照 / Window 提交结束。
CPU Glass 的 GPU 路径、复杂排版、屏幕阅读器和编辑器能力见 [评估边界](platform-evaluations.md)。
