# CMake 安装与独立接入

源码接入仍使用 `add_subdirectory`，不需要先安装。可设置 `SKUI_INSTALL=OFF` 禁用安装规则。
公开 targets 是 `skui::ui` 和仅依赖 C++17 标准库的 `skui::navigation`。

## 随库 ImGui

```bash
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DSKUI_BUILD_ANDROID_DEMO=OFF
cmake --build build/package --parallel 4
cmake --install build/package --prefix /absolute/path/skui-sdk
# 可以将整个 skui-sdk 目录移动到其他位置。
cmake -S examples/package-consumer -B build/installed-consumer \
  -DCMAKE_PREFIX_PATH=/new/path/skui-sdk
cmake --build build/installed-consumer --parallel 4
build/installed-consumer/skui-package-consumer
```

应用自己的 CMake：

```cmake
find_package(skui 0.1 CONFIG REQUIRED)
target_link_libraries(your-app PRIVATE skui::ui skui::navigation)
```

安装内容包含静态库、公开头文件、匹配的 ImGui 核心头文件、CMake 导出和许可清单。
不会安装 Android 悬浮宿主、IME APK、测试、Gallery、构建缓存或图片。
ImGui 平台 / 渲染后端仍由宿主选择并编译，版本须匹配 [固定依赖](dependencies.md)。
不要给同一应用再链接另一份 ImGui；否则上下文和内部类型无法保持一致。

仅使用路由核心时可以跳过 ImGui 导入：

```cmake
find_package(skui 0.1 CONFIG REQUIRED COMPONENTS navigation)
target_link_libraries(your-app PRIVATE skui::navigation)
```

## 宿主提供 ImGui

构建时先定义宿主 ImGui target，再设置 `SKUI_IMGUI_TARGET`，最后加入本库。
安装这种库包时，导出的 skui 不包含第二份 ImGui 或它的头文件。
消费端必须先创建或导入一个兼容的 target，名称可以和生产端不同：

```cmake
# 先由应用已有的依赖系统定义 my-app-imgui。
set(SKUI_IMGUI_TARGET my-app-imgui)
find_package(skui 0.1 CONFIG REQUIRED COMPONENTS ui navigation)
target_link_libraries(your-app PRIVATE skui::ui)
```

宿主 target 负责公开匹配的 `imgui.h`、`imgui_internal.h`、配置宏、编译选项和依赖。
缺少 target 会明确配置失败，不会悄悄编译内嵌 ImGui。
仅请求 `navigation` 时，即使安装包来自宿主 ImGui 构建，也不需要该 target。
所有库须使用兼容的架构、编译器 / C++ ABI 和 ImGui 配置；CMake 包版本不代表任意 ImGui 版本兼容。

## 验证与范围

```bash
python3 scripts/check-package.py
CC=clang CXX=clang++ python3 scripts/check-package.py
# 已完成 Android 构建后，验证交叉安装与消费（只编译，不启动设备）：
python3 scripts/check-package.py --android-build build/arm64-v8a
```

脚本在自己创建的临时目录构建 / 安装，将安装目录移动到含空格的新路径，
用复制到独立目录的 consumer 验证 UI 与仅路由两种入口。
分别覆盖随库 / 宿主 ImGui，检查导出元数据不含原源码 / 构建 / 安装路径，
缺失宿主、未知组件、过高版本的失败行为，以及许可文件。
测试退出清理自己的临时文件；本地与 CI 使用相同脚本。

默认使用 GNUInstallDirs 的相对 include / lib / share 目录以支持移动前缀。
自行把安装目录改成绝对路径、改写导出文件或只移动 SDK 的一部分，不属于可搬移保证。
Android 默认 ImGui 构建还依赖 FreeType，安装包含相应库 / 头文件，配置通过 `find_dependency(Freetype)`
查找；交叉消费端须继续使用匹配的 NDK / ABI / STL，并把 SDK 加入 CMake 查找根目录。
当前安装包验证以 Linux GCC / Clang 为基线，不宣称已完成 Windows / macOS 运行验证。
2026-11-25：GCC / Clang 的两种 ImGui 包均完成移动前缀后的编译与执行；
Android arm64 / NDK 28.2.13676358 完成交叉安装消费，小米 Android 13 的 UI consumer 原生运行通过。
