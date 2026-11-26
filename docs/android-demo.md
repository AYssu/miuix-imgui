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
触摸捕获覆盖所有可交互的 ImGui 顶层窗口，包括主窗口之外的监控 / 检查窗口；
窗口间的空白仍交给 Android，弹窗打开期间捕获外部点击以便关闭，键盘区域始终排除。
横竖屏切换使用同一方形 Surface；交换链失效时即使宽高未变也会重建，不额外暂停半秒。
可传入 `--settings <文件路径>` 启用 Gallery 设置加载、手动与退出保存，默认不访问设置文件。
设置页提供主题参数和可关闭的性能诊断，详见 [基础能力](foundations.md)。
右上角 `−` 将 Gallery 收拢为主题色透明粒子悬浮球，内部无实心底盘和底盘阴影；点击球恢复当前页面、窗口位置和尺寸，
拖动超过 5 dp 后只移动球，松手不会展开。拖动产生短暂轨迹和内部粒子扰动，停下后逐渐衰减。
84 个粒子中，72 个默认沿外圈带旋转，12 个在内部缓慢游动；粒子、圆环和拖尾均跟随主题强调色。
球的位置按可用屏幕范围归一化，旋转或键盘区域变化时保持在屏内；状态只保留在本次进程中。
完全收起后不再绘制页面、工具条或检查窗口，触摸只捕获球的 64 dp 方形范围，轨迹不捕获触摸。
收起会清除文本焦点，沿用输入桥接的结束与卸载流程。普通展开恢复页面时不会自动重新调起键盘。
粒子空闲时请求 30 Hz 更新，拖动 / 收拢过渡请求连续绘制；关闭主题动画后不请求粒子空闲重绘。
实现位于 [Gallery FloatingOrb](../examples/gallery/floating_orb.cpp)，组件宿主可通过
`ShellOptions::minimize_requested` 接收缩小操作，再管理自己的收起状态。
首页“任务与长列表”演示取消 / 重试和可见行裁剪。默认启用闲置降帧，`--continuous` 可用于对比；
唤醒约定和性能记录见 [运行效率](runtime.md)。
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
回退路线已在 Android 13 测试；2026-11-26 亦在该一加 Android 16 / 搜狗 OEM 输入法上验证
搜索与卡片文本回填、竖屏转横屏和编辑中左右横屏切换，以及返回后卸载。
该机尚未执行完整 IME 故障矩阵；范围见 [测试记录](testing.md)。

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

