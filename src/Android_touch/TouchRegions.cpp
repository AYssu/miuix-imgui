#include "TouchRegions.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace Touch {
bool CaptureRegions::Contains(float x, float y) const {
    for (std::size_t i = 0; i < count; ++i) {
        const auto &r = rectangles[i];
        if (x >= r.x && y >= r.y && x < r.z && y < r.w) return true;
    }
    return false;
}
void CaptureRegions::Add(ImVec2 min, ImVec2 max) {
    if (count == Capacity || !std::isfinite(min.x) || !std::isfinite(min.y) ||
        !std::isfinite(max.x) || !std::isfinite(max.y) || min.x >= max.x || min.y >= max.y) return;
    rectangles[count++] = ImVec4(min.x, min.y, max.x, max.y);
}
CaptureRegions CollectCaptureRegions(ImVec2 display, float keyboard_height) {
    CaptureRegions result;
    if (!ImGui::GetCurrentContext() || !std::isfinite(display.x) || !std::isfinite(display.y) ||
        display.x <= 0 || display.y <= 0) return result;
    if (!std::isfinite(keyboard_height)) keyboard_height = 0;
    ImVec2 max(display.x, display.y - std::clamp(keyboard_height, 0.f, display.y));
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        result.Add(ImVec2(0, 0), max);
        return result;
    }
    // Prefer frontmost windows if the bounded snapshot fills up. Focus does
    // not determine capture: unfocused windows must be clickable to regain it.
    auto &windows = GImGui->Windows;
    for (int i = windows.Size - 1; i >= 0; --i) {
        auto *window = windows[i];
        if (!window->Active || window->Hidden || (window->IsFallbackWindow && !window->WriteAccessed) ||
            (window->Flags & (ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_ChildWindow))) continue;
        const auto &rect = window->OuterRectClipped;
        result.Add(ImVec2(std::max(0.f, rect.Min.x), std::max(0.f, rect.Min.y)),
                   ImVec2(std::min(max.x, rect.Max.x), std::min(max.y, rect.Max.y)));
    }
    return result;
}
} // namespace Touch
