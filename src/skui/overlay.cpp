#include "skui/skui.h"
#include "detail.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace skui::detail {
namespace {
const ImGuiID elapsed_key = ImHashStr("skui/overlay/elapsed");
const ImGuiID closing = ImHashStr("skui/overlay/closing");
const ImGuiID opacity = ImHashStr("skui/overlay/opacity");
const ImGuiID exit_from = ImHashStr("skui/overlay/exit-from");
const ImGuiID kind_key = ImHashStr("skui/overlay/kind");

// Transform the entire popup, including scrolling children and their clips.
// Window layout stays stable, so auto-sizing cannot feed back into animation.
void Transform(ImGuiWindow *window, ImVec2 origin, float scale, float dy, float alpha) {
    auto *draw = window->DrawList;
    for (auto &vertex : draw->VtxBuffer) {
        vertex.pos.x = origin.x + (vertex.pos.x - origin.x) * scale;
        vertex.pos.y = origin.y + (vertex.pos.y - origin.y) * scale + dy;
        auto a = static_cast<ImU32>(((vertex.col >> IM_COL32_A_SHIFT) & 255) * alpha);
        vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
    }
    for (auto &cmd : draw->CmdBuffer) {
        cmd.ClipRect.x = origin.x + (cmd.ClipRect.x - origin.x) * scale;
        cmd.ClipRect.z = origin.x + (cmd.ClipRect.z - origin.x) * scale;
        cmd.ClipRect.y = origin.y + (cmd.ClipRect.y - origin.y) * scale + dy;
        cmd.ClipRect.w = origin.y + (cmd.ClipRect.w - origin.y) * scale + dy;
    }
    for (auto *child : window->DC.ChildWindows)
        if (child->Active) Transform(child, origin, scale, dy, alpha);
}
}

void CloseOverlay() {
    if (GImGui->BeginPopupStack.empty()) return;
    if (!GetTheme().animations) { ImGui::CloseCurrentPopup(); return; }
    auto &state = GImGui->BeginPopupStack.back().Window->StateStorage;
    if (!state.GetBool(closing)) {
        state.SetBool(closing, true);
        state.SetFloat(elapsed_key, 0);
        state.SetFloat(exit_from, state.GetFloat(opacity, 1));
        RequestAnimationFrame();
    }
}

bool BeginOverlay(const char *id, OverlayKind kind, ImGuiWindowFlags flags) {
    // A modal popup supplies both the dim layer and background input exclusion.
    // BeginPopupEx keeps scoped popup IDs distinct (e.g. multiple dropdowns).
    if (!ImGui::BeginPopupEx(ImGui::GetID(id), flags | ImGuiWindowFlags_Modal |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) return false;
    auto *window = ImGui::GetCurrentWindow();
    window->StateStorage.SetBool(ImHashStr("skui/native-window"),WindowScope() && HasWindowBackend());
    auto &state = window->StateStorage;
    if (ImGui::IsWindowAppearing()) {
        state.SetFloat(elapsed_key, 0);
        state.SetBool(closing, false);
    }
    bool exit = state.GetBool(closing);
    float duration = exit ? (kind == OverlayKind::Popup ? .22f : .26f) : .3f;
    float elapsed = state.GetFloat(elapsed_key)+(window->Appearing ? 0.f : ImGui::GetIO().DeltaTime);
    state.SetFloat(elapsed_key,elapsed);
    float t = GetTheme().animations ? std::clamp(elapsed/(duration*GetTheme().motion_duration_scale), 0.f, 1.f) : 1.f;
    if (t<1) RequestAnimationFrame();
    if (exit && t >= 1) {
        // Begin already emitted an opaque window background. Exclude this
        // terminal frame from rendering instead of flashing that background.
        window->Hidden = true;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return false;
    }
    float eased = t*t*(3-2*t);
    float alpha = exit ? state.GetFloat(exit_from, 1) * (1-eased) : eased;
    state.SetFloat(opacity, alpha);
    state.SetInt(kind_key, static_cast<int>(kind));
    auto scrim = Colors().scrim; scrim.w *= alpha;
    window->DC.ModalDimBgColor = ImGui::ColorConvertFloat4ToU32(scrim);
    if (!GetTheme().animations) GImGui->DimBgRatio = 1;
    // A down event on the scrim is consumed by the modal. It never activates
    // the page underneath, including during the exit transition.
    if (!window->Appearing && ((ImGui::IsMouseClicked(0) && !window->Rect().Contains(ImGui::GetIO().MousePos)) ||
                              ImGui::IsKeyPressed(ImGuiKey_Escape, false))) CloseOverlay();
    BeginInert(state.GetBool(closing));
    return true;
}

void EndOverlay() {
    auto *window = ImGui::GetCurrentWindow();
    EndInert();
    ImGui::EndPopup();
    float alpha = window->StateStorage.GetFloat(opacity, 1);
    auto kind = static_cast<OverlayKind>(window->StateStorage.GetInt(kind_key));
    float scale = kind == OverlayKind::Sheet ? 1 : 1 - .06f * (1-alpha);
    ImVec2 origin(window->Pos.x+window->Size.x*.5f, window->Pos.y);
    if (kind == OverlayKind::Dialog) origin.y += window->Size.y*.5f;
    float dy = (1-alpha) * (kind == OverlayKind::Sheet ? window->Size.y :
                            kind == OverlayKind::Dialog ? Dp(10) : -Dp(6));
    Transform(window, origin, scale, dy, alpha);
}
}
