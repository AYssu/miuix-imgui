#include "draw.h"
#include "gallery.h"
#include "AndroidIme.h"
#include "imgui_internal.h"
#include <chrono>

std::unique_ptr<AndroidImgui> graphics;
ANativeWindow *window = nullptr;
android::ANativeWindowCreator::DisplayInfo displayInfo{};
int abs_ScreenX = 0, abs_ScreenY = 0;
int native_window_screen_x = 0, native_window_screen_y = 0;

void init_My_drawdata() {
    ImGuiIO &io = ImGui::GetIO();
    if (!ImGui::My_Android_LoadSystemFont(gallery::FontScale())) {
        ImFontConfig config;
        config.SizePixels = gallery::FontScale();
        io.Fonts->AddFontDefault(&config);
    }
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
}

void screen_config() {
    static std::chrono::steady_clock::time_point last{};
    auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::milliseconds(250))
        return;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    last = now;
}

void drawBegin() {
    screen_config();
    Touch::setOrientation(displayInfo.orientation);
    // The native surface is square to survive rotation. Layout and popup
    // placement must use the visible display, not that larger backing buffer.
    ImGui::GetIO().DisplaySize = ImVec2(displayInfo.width, displayInfo.height);
}

void Layout_tick_UI(bool *running) {
    auto error = AndroidIme::ConsumeError();
    if (!error.empty()) gallery::Notify(error.c_str());
    gallery::Draw(running,
                  ImVec2(static_cast<float>(displayInfo.width), static_cast<float>(displayInfo.height)),
                  AndroidIme::KeyboardHeight());
    auto pos = skui::ShellPosition(), size = skui::ShellSize();
    // Popups can extend beyond the shell. Capture the screen while one is open,
    // so both selecting an entry and tapping outside to dismiss reach ImGui.
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        Touch::SetCaptureRegion(0, 0, displayInfo.width,
                                displayInfo.height - AndroidIme::KeyboardHeight());
    else
        Touch::SetCaptureRegion(pos.x, pos.y, size.x, size.y);
}
