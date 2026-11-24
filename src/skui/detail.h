#pragma once
#include "imgui.h"
#include <limits>
#include <functional>
namespace skui::detail {
// Per-window, per-ID motion. Reversals start at the currently presented value.
float Animate(ImGuiID id, float target, float seconds = .22f,
              float initial = std::numeric_limits<float>::quiet_NaN());
inline float Animate(const char *key, float target, float seconds = .22f,
                     float initial = std::numeric_limits<float>::quiet_NaN()) {
    return Animate(ImGui::GetID(key), target, seconds, initial);
}
// Paint the last item's owned press; a released touch position is not a hover.
float InteractionFeedback(float hover, float press);
bool &WindowScope();
// Exclude input during exit without introducing the disabled-state gray tint.
void BeginInert(bool inert, bool clear_window_active = true);
void EndInert();
void Reveal(const char *id, float progress, bool inert, const std::function<void()> &content);
void AllowScroll();
bool CanScroll();
enum class OverlayKind { Popup, Dialog, Sheet };
bool BeginOverlay(const char *id, OverlayKind kind, ImGuiWindowFlags flags);
void EndOverlay();
void CloseOverlay();
}
