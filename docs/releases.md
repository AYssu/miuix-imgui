# API 版本与发布

## 版本规则

当前开发版本为 `0.1.0`，公开宏和编译后查询位于 [version.h](../include/skui/version.h)。
CMake 项目 / 包版本、头文件宏和 Gallery 显示必须一致；`scripts/check-versions.py` 在 CI 检查声明，
最小消费端核对头文件与实际链接库的 `GetVersion()` / `GetVersionNumber()`。
版本整数为 `major * 1000000 + minor * 1000 + patch`。

0.x 阶段不兼容的公开接口变化提升 minor，同一 minor 的 patch 用于兼容修复；同 minor 的安装包版本
由 CMake `SameMinorVersion` 匹配。默认值 / 交互语义有变化也必须进入 [变更记录](../CHANGELOG.md)。
1.0 之前不承诺稳定 C++ ABI；升级静态库时同步更新公开头文件并重新编译宿主。
后端、`imgui_internal.h` 和 ImGui 核心按 [固定依赖](dependencies.md) 同步升级。
业务文件格式独立版本化，不跟随组件版本自动迁移或覆盖。

## 可复现的源码快照

源码打包只读取指定 Git commit，不读未提交、未跟踪或忽略文件；调用者先提交需要交付的修改。

```bash
python3 scripts/check-repository.py
python3 scripts/check-versions.py
python3 scripts/check-release.py
python3 scripts/package-release.py --ref HEAD --output build/release
cd build/release
sha256sum -c SHA256SUMS
```

输出目录必须为空，已有文件不会覆盖。产物有：

- `miuix-imgui-<version>-g<12位commit>.tar.gz`：带独立顶层目录的源码及许可。
- `manifest.json`：完整提交、源码时间、API 版本和 [依赖锁](../dependencies.lock.json)。
- `SHA256SUMS`：上述文件的 SHA-256；下载后可核对文件完整性。

归档使用固定提交 / 时间和无文件名的 gzip 头；同一工具环境重复执行得到相同字节。
路径、链接类型和发布排除规则都验证；即使图片 / 构建产物 / 工具私有文件被误提交，也会拒绝打包。
不打包 APK、密钥、系统字体、截图、录屏、设备日志或工作区其他项目。
源码包包含构建脚本和许可证，不包含预编译依赖；Android 的 FreeType 按锁中的归档哈希获取。

测试验证重复构建、commit 内容边界、manifest 与校验和，以及错误标签 / 非空目标 / 误提交图片的失败契约。
[Source package 工作流](../.github/workflows/source-package.yml) 可手动选择 ref，或由 `v*` 标签触发，
保存 CI artifact 14 天；只具有读取仓库权限，不自动创建 GitHub Release 或改写版本标签。
标签必须形如 `v0.1.0` 且与对应提交的公开版本匹配，实际发布前应等该提交全部 CI 成功。

## 二进制交付记录

源码快照可复现不等于原生可执行文件逐字节可复现。
Android IME 辅助 APK 的构建密钥按本地构建目录生成，签名与打包时间可能变化；
不为了复现而将私钥加入仓库。需要二进制交付时另存它的 SHA-256、完整源码提交、
NDK / JDK / SDK / CMake / 编译器与构建参数、实际系统依赖版本、ELF 检查和真机测试结果。
本轮工作流只交付源码快照，未发布预编译二进制。

发布前运行 [测试规范](testing.md)，核对 [兼容矩阵](compatibility.md) 中“编译”与“运行”的区别，
并保留当前第三方来源 / 许可说明的边界。回退按固定 commit 重新构建，不混用不同版本的头文件、库和后端。
