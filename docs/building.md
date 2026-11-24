# 构建规范

所有路径相对仓库根目录；统一使用独立的 `build/` 子目录，禁止在源码目录中生成 CMake 文件。
切换编译器、ABI 或工具链时使用新的构建目录。

## 主机组件库

基线：Linux、CMake 3.21+、C/C++17 编译器、Make、Python 3.9+。
CI 使用 Ubuntu 24.04 的 GCC 和 Clang。组件库本身只依赖随仓库附带的 ImGui，主机构建不下载依赖。

```bash
cmake -S . -B build/host \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSKUI_BUILD_ANDROID_DEMO=OFF -DSKUI_BUILD_TESTS=ON
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
```

库目标为 `skui::ui` 和 `skui::navigation`；当前接入方式为 `add_subdirectory`，
尚未提供安装包或 `find_package(skui)`。

## Android Demo

本地和 CI 参考配置：JDK 17、NDK `28.2.13676358`、SDK platform `36`、
build-tools `36.0.0`、`arm64-v8a`、原生 API 28、静态 libc++、Release。
SDK 编译平台不等于最低设备 API；输入辅助 APK 的 minSdk 为 28、targetSdk 为 36。

已安装 Android command-line tools 后执行：

```bash
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
sdkmanager 'platforms;android-36' 'build-tools;36.0.0' 'ndk;28.2.13676358'
export SKUI_NDK="$ANDROID_SDK_ROOT/ndk/28.2.13676358"
bash scripts/build-android.sh arm64-v8a
cmake --build build/arm64-v8a --target skui-ime-tests --parallel 4
python3 scripts/check-android-elf.py \
  build/arm64-v8a/sk-imgui-ui build/arm64-v8a/skui-ime-tests
```

脚本读取 `ANDROID_SDK_ROOT`，其次 `ANDROID_HOME`，最后 `$HOME/Android/Sdk`。
NDK 读取 `SKUI_NDK`、其次 `ANDROID_NDK_HOME`；未指定时选择 SDK 内版本号最高的已安装 NDK。
贡献与回归记录应显式指定 NDK，避免本地工具更新造成差异。
`SKUI_JOBS` 控制并行数；第二个位置参数指定构建目录。

需要直接调用 CMake 时，等价配置为：

```bash
cmake -S . -B build/arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE="$SKUI_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
  -DSKUI_ANDROID_SDK="$ANDROID_SDK_ROOT" \
  -DSKUI_ANDROID_PLATFORM=36 -DSKUI_ANDROID_BUILD_TOOLS=36.0.0 \
  -DSKUI_BUILD_ANDROID_DEMO=ON -DSKUI_BUILD_TESTS=OFF
cmake --build build/arm64-v8a --parallel 4
```

当前 CI 只验证 arm64；其他 ABI 未完成完整设备验证。
`javac`、`keytool`、`d8`、`aapt`、`zipalign`、`apksigner` 均参与内嵌 IME 构建。
每个构建目录自动生成临时签名密钥，不需要发布密钥、GitHub Secrets 或手工签名。
`scripts/build-ime.py` 中的固定口令只用于本地生成的辅助签名容器，不是用户或发布凭据。
密钥及 APK/DEX 中间文件全部位于 `build/.../generated/ime/`，禁止提交或上传整个构建目录。

## 依赖与离线构建

Android 默认 ImGui 目标使用 FreeType `2.13.3`，通过 CMake FetchContent 下载源码并按 SHA-256 校验：

```text
版本标签：VER-2-13-3
SHA-256：bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103
```

关闭 FreeType 的可选 ZLIB、BZIP2、PNG、HarfBuzz、Brotli 依赖，以保持构建依赖明确。
字体从设备系统目录读取；不分发原模板的 OPPO/Font Awesome 字体及图片。
FreeType 源码来源与许可见 [第三方清单](../THIRD_PARTY_NOTICES.md)。
首次下载需要网络；离线环境先将同一版本源码解压，再在 CMake 配置中传入：

```bash
-DFETCHCONTENT_SOURCE_DIR_SKUI_FREETYPE=/absolute/path/to/freetype-VER-2-13-3
```

这是源码覆盖入口，CMake 不会为覆盖目录校验下载哈希；使用者须自行核对归档哈希与版本。
已有宿主 ImGui 目标通过 `SKUI_IMGUI_TARGET` 接入时由宿主管理字体依赖；
该目标必须公开头文件、兼容配置，以及其使用的 FreeType 等传递链接依赖。

## 产物与分发

Android 产物是 `build/<ABI>/sk-imgui-ui`，不是可点击安装的 Gallery APK。
IME 辅助 APK 已嵌入其中，无需单独复制；进程启动方式见 [设备运行](android-demo.md)。
最终 ELF 使用 16 KiB LOAD 段对齐；该检查不能替代实际设备兼容性测试。

分发时提供提交号、工具链版本、ABI、SHA-256、验证范围，以及 `LICENSE`、
`THIRD_PARTY_NOTICES.md`、`LICENSES/`。源码和二进制都需要先完成清单中的历史来源确认。
不分发签名密钥、设备日志、CMake 缓存及本机路径。辅助 APK 的随机签名和时间戳意味着
当前构建流程可复现步骤，但不保证跨构建目录的二进制逐字节一致。
