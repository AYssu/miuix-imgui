// Device integration harness. Uses the real adapter and APK lifecycle; only the
// direct-host rejection and renderer's frame clock are simulated by default.
#include "AndroidIme.h"
#include "skui/skui.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {
int frame = 0;
skui::TextInputHandler input_handler = nullptr;
void *input_data = nullptr;
}
namespace ImGui { int GetFrameCount() { return frame; } }
namespace skui {
void SetTextInputHandler(TextInputHandler handler, void *data) {
    input_handler = handler;
    input_data = data;
}
}

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1 && std::strcmp(argv[1], "--ime-apk-host") == 0) return AndroidIme::RunApkHost();
    if (argc > 1 && std::strcmp(argv[1], "--ime-host") == 0) {
        if (std::getenv("SKUI_TEST_REAL_DIRECT")) return AndroidIme::RunHost();
        std::puts("SKUI R\nSKUI E 556e6b6e6f776e207069643d74657374207569643d31303030\nSKUI F");
        char command[64];
        while (std::fgets(command, sizeof(command), stdin) && command[0] != 'Q') { }
        return 0;
    }
    std::string mode = argc > 1 ? argv[1] : "input";
    if (mode == "direct") setenv("SKUI_TEST_REAL_DIRECT", "1", 1);
    char buffer[256] = "你好😀";
    int selection = static_cast<int>(std::strlen(buffer));
    bool shown = false, done = false, cancelled = false, failed = false;
    auto began = std::chrono::steady_clock::now();
    auto shown_at = began;
    AndroidIme::Init();
    std::printf("[TEST] pid=%d mode=%s\n", getpid(), mode.c_str());
    while (std::chrono::steady_clock::now() - began < std::chrono::seconds(65)) {
        ++frame;
        AndroidIme::Poll();
        auto error = AndroidIme::ConsumeError();
        if (!error.empty()) { std::fprintf(stderr, "[TEST] error=%s\n", error.c_str()); failed = true; break; }
        if (!cancelled) {
            skui::TextInputEvent event{42, buffer, sizeof(buffer), {}, {}, true, frame == 1, selection, selection};
            input_handler(event, input_data);
            selection = event.selection_end;
            if (event.changed) std::printf("[TEST] text=%s selection=%d\n", buffer, selection);
            if (event.finished) { done = true; break; }
        }
        if (!shown && AndroidIme::KeyboardHeight() > 0) {
            shown = true;
            shown_at = std::chrono::steady_clock::now();
            std::printf("[TEST] keyboard=%g\n", AndroidIme::KeyboardHeight());
        }
        if (shown && mode == "disconnect") {
            std::puts("[TEST] disconnecting renderer without Shutdown");
            _exit(0);
        }
        if (shown && mode == "cancel" && std::chrono::steady_clock::now() - shown_at > std::chrono::milliseconds(300))
            cancelled = true;
        AndroidIme::EndFrame();
        if (cancelled && std::chrono::steady_clock::now() - shown_at > std::chrono::seconds(2)) { done = true; break; }
        usleep(10000);
    }
    AndroidIme::Shutdown();
    std::printf("[TEST] shown=%d done=%d final=%s\n", shown, done, buffer);
    return shown && done && !failed ? 0 : 1;
}
