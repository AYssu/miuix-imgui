# Android 原生悬浮 Demo

所有主机命令从仓库根目录执行。构建步骤见 [构建规范](building.md)。

当前原生悬浮后端需要 root 访问触摸设备和系统合成接口。
历史实测范围：arm64、Android 13、1080×2400、Vulkan；本轮构建是否上机须单独记录。
系统私有 Surface 接口存在版本差异，其他设备先运行 `--diagnose` 检查。

```bash
adb -s <设备序列号> push build/arm64-v8a/sk-imgui-ui /data/local/tmp/sk-imgui-ui
adb -s <设备序列号> shell "su -c 'chmod 755 /data/local/tmp/sk-imgui-ui'"
adb -s <设备序列号> shell "su -c '/data/local/tmp/sk-imgui-ui --diagnose'"
# 启动前确认没有旧实例
adb -s <设备序列号> shell "su -c 'pidof sk-imgui-ui'"
adb -s <设备序列号> shell "su -c '/data/local/tmp/sk-imgui-ui'"
```

同一个可执行文件路径使用文件锁防止重复启动，重复实例退出码为 5。
测试结束点击 ×，或向本次启动的 PID 发送 `kill -TERM <PID>`，再检查进程已退出。
触摸线程会在退出时停止，触摸设备解除独占，悬浮 Surface 随后销毁。

点击表单或搜索输入框会调起手机当前的 Android 系统输入法，支持中文候选提交、英文、
退格和完成收起。窗口会避让键盘，键盘区域的触摸交给系统，关闭 Demo 时同步结束输入连接。

组件库仍是 C++17。Android Demo 内嵌 Java `View/InputConnection` 桥接及一个签名辅助 APK，
继续只分发原生可执行文件，运行仍需要 root。

常规输入的探测、回退、安装与卸载静默处理；`--diagnose-ime`、
`--diagnose-ime-apk` 或环境变量 `SKUI_IME_DIAGNOSTICS` 用于主动诊断。
最终输入失败或卸载失败仍会报告给宿主。

输入时先探测独立 `app_process` 的窗口能力；支持就直接使用，不安装 APK。
若窗口创建被拒绝、桥接退出或系统键盘一直没有显示，则自动切换为临时辅助 APK，
由系统登记的应用进程承载透明输入窗口。辅助包名为 `skui.android.imehelper`，
没有桌面启动图标、最近任务入口或后台服务；使用期间仍是正常的已安装应用。
它只通过本机回环连接同步输入，连接使用一次性令牌，入口仅允许 root/shell 等具有 DUMP 权限的调用方启动。

每次输入完成、返回收起或取消焦点时卸载临时辅助包；再次输入时按需重装。
安装与卸载由独立监护进程执行，不阻塞渲染，主程序意外退出时监护进程也会清理。
若设备已经装有同名包，则拒绝覆盖和卸载；卸载失败会明确报错。
断电或监护进程也被强制杀死时无法保证立即清理，可检查并手动执行
`su -c 'pm uninstall --user current skui.android.imehelper'`。

原独立桥接已在 Android 13 / MIUI / 讯飞输入法实测；遇到 Android 16 / 一加 PLQ110
此前记录的 `Unknown pid` 拒绝时，现在会改走临时 APK 路线。
回退路线已在 Android 13 测试，尚未完成 Android 16 真机验证。

可单独检查输入窗口接口兼容性，不启动 Gallery 或打开键盘：

```bash
su -c "/path/to/sk-imgui-ui --diagnose-ime"
```

返回 0 表示输入窗口会话可建立，返回 1 表示不可用，并输出具体原因。
这个检测验证窗口会话；中文输入的完整支持仍需要实际输入法交互验证。

可强制验证临时 APK 路线，检测期间会安装辅助包，结束时卸载，不打开键盘：

```bash
su -c "/path/to/sk-imgui-ui --diagnose-ime-apk"
```

设备回归测试会打开真实键盘并注入测试文字，测试前应确认设备可操作且没有旧实例。
它使用真实输入适配器，只在测试可执行文件里模拟独立进程被拒绝：

```bash
cmake --build build/arm64-v8a --target skui-ime-tests --parallel 6
python3 scripts/test-ime-device.py --serial <设备序列号>
```

覆盖自动回退、中文/emoji 初始文字、英文提交、退格、返回、取消焦点、渲染进程断开、
安装中终止、监护进程收到终止信号、已有同名包保护及原直接桥接。
测试结束确认辅助包和本轮进程都已移除。

