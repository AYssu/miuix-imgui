#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void RequestStop(int) { stop_requested = 1; }
} // namespace

#include <android/log.h>

#include "AndroidImgui.h"    //创建绘制套
#include "AndroidIme.h"
#include "GraphicsManager.h" //获取 当前渲染模式
#include "draw.h"            //绘制套

int main(int argc, char *argv[]) {
    if (argc > 1 && std::strcmp(argv[1], "--ime-host") == 0)
        return AndroidIme::RunHost();
    if (argc > 1 && std::strcmp(argv[1], "--ime-apk-host") == 0)
        return AndroidIme::RunApkHost();
    if (argc > 1 && std::strcmp(argv[1], "--diagnose-ime") == 0)
        return AndroidIme::Diagnose();
    if (argc > 1 && std::strcmp(argv[1], "--diagnose-ime-apk") == 0)
        return AndroidIme::Diagnose(true);
    if (argc > 1 && (0 == std::strcmp(argv[1], "--diagnose") || 0 == std::strcmp(argv[1], "-d"))) {
        return android::ANativeWindowCreator::SelfCheck() ? 0 : 1;
    }

    // The lock is scoped to this executable path and released by the kernel on
    // exit, including crashes. It prevents accidentally launching duplicates.
    char executable[4096]{};
    auto length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length <= 0)
        return 5;
    std::string lock_path = std::string(executable) + ".lock";
    int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "sk-imgui-ui: another instance is running or the lock is unavailable\n");
        if (lock_fd >= 0)
            close(lock_fd);
        return 5;
    }
    std::signal(SIGTERM, RequestStop);
    std::signal(SIGINT, RequestStop);

    ::graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    // 获取屏幕信息
    ::screen_config();

    ::native_window_screen_x =
        (::displayInfo.height > ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);
    ::native_window_screen_y =
        (::displayInfo.height > ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);
    ::abs_ScreenX = (::displayInfo.height > ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);
    ::abs_ScreenY = (::displayInfo.height < ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);

    if (::native_window_screen_x <= 0 || ::native_window_screen_y <= 0) {
        __android_log_print(ANDROID_LOG_ERROR, "Surface",
                            "[-] Cannot read display info (width=%d height=%d); "
                            "run --diagnose for details",
                            ::native_window_screen_x, ::native_window_screen_y);
        return 2;
    }

    ::window = android::ANativeWindowCreator::Create("sk-imgui-ui", native_window_screen_x,
                                                     native_window_screen_y, false);
    if (nullptr == ::window) {
        __android_log_print(ANDROID_LOG_ERROR, "Surface",
                            "[-] createSurface failed; run --diagnose on this device");
        return 3;
    }

    if (!::graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y)) {
        __android_log_print(ANDROID_LOG_ERROR, "Surface", "[-] Failed to initialize Vulkan renderer");
        android::ANativeWindowCreator::Destroy(::window);
        return 4;
    }

    if (!Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false)) { // 最后一个参数改成true 只监听
        __android_log_print(ANDROID_LOG_WARN, "Surface",
                            "[!] Touch input is unavailable; the window is read-only");
    }
    Touch::setOrientation(displayInfo.orientation);

    ::init_My_drawdata(); // 初始化绘制数据
    AndroidIme::Init();
    static bool flag = true;
    while (flag && !stop_requested) {
        drawBegin();
        Touch::Poll();
        AndroidIme::Poll();
        graphics->NewFrame();

        Layout_tick_UI(&flag);
        AndroidIme::EndFrame();

        graphics->EndFrame();
    }

    AndroidIme::Shutdown();
    Touch::Close();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);
    close(lock_fd);
    return 0;
}
