#include "skui/skui.h"
#include "imgui_internal.h"
#include "detail.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace skui {
const char *GetVersion() {return SKUI_VERSION_STRING;}
unsigned GetVersionNumber() {return SKUI_VERSION_NUMBER;}

namespace {
// Call ApplyTheme after selecting the host context, on the rendering thread.
thread_local Theme theme;
thread_local TextInputHandler text_input_handler = nullptr;
thread_local void *text_input_user_data = nullptr;
thread_local HapticHandler haptic_handler=nullptr;
thread_local void *haptic_data=nullptr;
thread_local ImGuiContext *haptic_context=nullptr;
bool UpdateTextInput(char *buffer, std::size_t capacity) {
    if (!text_input_handler)
        return false;
    auto id = ImGui::GetItemID();
    auto *state = ImGui::GetInputTextState(id);
    bool active = ImGui::IsItemActive();
    TextInputEvent event{id, buffer, capacity, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                         active, ImGui::IsItemActivated(),
                         state ? state->GetSelectionStart() : 0,
                         state ? state->GetSelectionEnd() : 0};
    text_input_handler(event, text_input_user_data);
    if (event.changed && state) {
        state->ReloadUserBufAndKeepSelection();
        int length = static_cast<int>(std::strlen(buffer));
        state->ReloadSelectionStart = std::clamp(event.selection_start, 0, length);
        state->ReloadSelectionEnd = std::clamp(event.selection_end, 0, length);
        ImGui::MarkItemEdited(id);
    }
    if (active)
        ImGui::ScrollToRect(ImGui::GetCurrentWindow(), ImGui::GetCurrentContext()->LastItemData.Rect,
                            ImGuiScrollFlags_KeepVisibleEdgeY);
    if (event.finished && active)
        ImGui::ClearActiveID();
    return event.changed;
}
ImVec4 Color(unsigned light, unsigned dark) {
    unsigned rgb = theme.dark ? dark : light;
    return ImVec4(((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, 1);
}
ImU32 U(ImVec4 c) { return ImGui::GetColorU32(c); }
ImVec2 Add(ImVec2 a, float x, float y) { return ImVec2(a.x + x, a.y + y); }
float Width() { return ImGui::GetContentRegionAvail().x; }
void Label(ImDrawList *draw, ImVec2 p, const char *text, ImVec4 color, float size = 16) {
    if (text)
        draw->AddText(ImGui::GetFont(), TextSize(size), p, U(color), text);
}
thread_local ImGuiID scrollable_item = 0;
void AllowScroll() {
    if (ImGui::IsItemActivated())
        scrollable_item = ImGui::GetItemID();
}
struct Row {
    ImVec2 min, max;
    bool pressed, hovered;
    Row(const char *id, float height) {
        min = ImGui::GetCursorScreenPos();
        pressed = ImGui::InvisibleButton(id, ImVec2(Width(), height),ImGuiButtonFlags_EnableNav);
        AllowScroll();
        max = ImGui::GetItemRectMax();
        hovered = ImGui::IsItemHovered();
        float a = detail::InteractionFeedback(0.f,1.f);
        auto color = Colors().interaction; color.w *= a;
        if (a > 0) ImGui::GetWindowDrawList()->AddRectFilled(min, max, U(color), Dp(GetTheme().tokens.press_radius));
    }
};
void CheckMark(ImDrawList *d, ImVec2 center, ImU32 color, float r) {
    d->AddLine(Add(center, -r * .48f, 0), Add(center, -r * .1f, r * .35f), color, Dp(2));
    d->AddLine(Add(center, -r * .1f, r * .35f), Add(center, r * .5f, -r * .38f), color, Dp(2));
}
struct ShellFrame {
    ImVec2 content, position, size;
    bool wide;
};
thread_local ShellFrame shell;
} // namespace

namespace detail {
namespace {
struct InertState {
    ImGuiWindow *window, *hovered;
    ImGuiWindowFlags flags;
    ImGuiID activate, down, pressed;
    bool inert;
};
thread_local std::vector<InertState> inert_stack;
}
float Animate(ImGuiID id, float target, float seconds, float initial) {
    auto *s = ImGui::GetStateStorage();
    auto key = [id](const char *field) { return ImHashStr(field, 0, id); };
    int frame = ImGui::GetFrameCount(), last = s->GetInt(key("motion/frame"), -2);
    float value = s->GetFloat(key("motion/value"), target);
    float from = s->GetFloat(key("motion/from"), value);
    float previous = s->GetFloat(key("motion/target"), target);
    float elapsed = s->GetFloat(key("motion/elapsed"));
    bool restarted = last < frame - 1;
    if (restarted) {
        value = from = std::isnan(initial) ? target : initial;
        previous = target;
        elapsed = 0;
    }
    if (previous != target) { from = value; elapsed = 0; restarted = true; }
    // Give a new target immediate feedback, but don't fast-forward it by the
    // long interval spent idle before this input arrived.
    if (last != frame) elapsed += restarted ? std::min(MotionDeltaTime(),1.f/60) : MotionDeltaTime();
    float duration=seconds*GetTheme().motion_duration_scale;
    float t = GetTheme().animations && duration > 0 ? std::clamp(elapsed / duration, 0.f, 1.f) : 1.f;
    if (t<1 && from!=target) RequestMotionFrame();
    // Layout reveals keep a gentle start. The navigation spring belongs to
    // page transitions, not short press feedback or collapsing geometry.
    value = t == 1 ? target : ImLerp(from, target, t*t*(3-2*t));
    if (t == 1) from = target;
    s->SetFloat(key("motion/value"), value);
    s->SetFloat(key("motion/from"), from);
    s->SetFloat(key("motion/target"), target);
    s->SetFloat(key("motion/elapsed"), elapsed);
    s->SetInt(key("motion/frame"), frame);
    return value;
}
float InteractionFeedback(float hover, float press) {
    if(ImGui::IsItemActivated() && ImGui::GetIO().MouseSource==ImGuiMouseSource_TouchScreen)
        PerformHaptic(Haptic::Press);
    const bool hovered=ImGui::IsItemHovered();
    const bool held=ImGui::IsItemActive() &&
        (hovered || GImGui->ActiveIdSource==ImGuiInputSource_Keyboard ||
                    GImGui->ActiveIdSource==ImGuiInputSource_Gamepad);
    const bool mouse_hover=hovered && ImGui::GetIO().MouseSource!=ImGuiMouseSource_TouchScreen;
    return Animate(ImHashStr("feedback",0,ImGui::GetItemID()),held ? press : mouse_hover ? hover : 0.f,.12f);
}
void BeginInert(bool inert, bool clear_window_active) {
    auto &g=*GImGui;
    auto *window=ImGui::GetCurrentWindow();
    inert_stack.push_back({window,g.HoveredWindow,window->Flags,
                          g.NavActivateId,g.NavActivateDownId,g.NavActivatePressedId,inert});
    if(!inert) return;
    window->Flags|=ImGuiWindowFlags_NoInputs;
    if(g.HoveredWindow && ImGui::IsWindowChildOf(g.HoveredWindow,window,true)) g.HoveredWindow=nullptr;
    if(clear_window_active && g.ActiveIdWindow && ImGui::IsWindowChildOf(g.ActiveIdWindow,window,true)) ImGui::ClearActiveID();
    if(g.NavWindow && ImGui::IsWindowChildOf(g.NavWindow,window,true))
        g.NavActivateId=g.NavActivateDownId=g.NavActivatePressedId=0;
}
void EndInert() {
    IM_ASSERT(!inert_stack.empty());
    auto saved=inert_stack.back();inert_stack.pop_back();
    if(!saved.inert) return;
    auto &g=*GImGui;
    saved.window->Flags=saved.flags;
    g.HoveredWindow=saved.hovered;
    g.NavActivateId=saved.activate;g.NavActivateDownId=saved.down;g.NavActivatePressedId=saved.pressed;
}
void Reveal(const char *id, float progress, bool inert, const std::function<void()> &content) {
    auto *window = ImGui::GetCurrentWindow();
    if (!content || window->SkipItems) return;
    ImGui::PushID(id);
    auto *storage = ImGui::GetStateStorage();
    auto active_id = ImGui::GetID("active-item");
    // A reveal shares its parent's window. Only cancel an active widget owned
    // by this group; another control in the same window must keep its press.
    if (inert && GImGui->ActiveId && GImGui->ActiveId == static_cast<ImGuiID>(storage->GetInt(active_id)))
        ImGui::ClearActiveID();
    if (progress <= 0) { ImGui::PopID(); return; }
    auto height_id = ImGui::GetID("height");
    auto start = window->DC.CursorPos, old_max = window->DC.CursorMaxPos;
    auto old_ideal = window->DC.IdealMaxPos, old_prev = window->DC.PrevLineSize;
    float old_base = window->DC.PrevLineTextBaseOffset;
    float height = Dp(storage->GetFloat(height_id, (window->ClipRect.Max.y-start.y)/GetTheme().scale));
    auto clip = window->ClipRect;
    // Layout still measures the full contents, while both drawing and hit
    // testing are clipped. Children inherit this clip as well.
    ImGui::PushClipRect(start, ImVec2(clip.Max.x, start.y + height*progress), true);
    BeginInert(inert, false);
    ImGui::BeginGroup();
    auto finish = [&] {
        auto size = ImVec2(std::max(0.f, window->DC.CursorMaxPos.x-start.x),
                           std::max(0.f, window->DC.CursorMaxPos.y-start.y));
        bool contains_active = GImGui->ActiveId && GImGui->ActiveIdIsAlive == GImGui->ActiveId &&
            GImGui->GroupStack.back().BackupActiveIdIsAlive != GImGui->ActiveId;
        storage->SetInt(active_id, contains_active ? static_cast<int>(GImGui->ActiveId) : 0);
        // EndGroup restores the original line/cursor; reserve only the revealed
        // size rather than letting hidden content expand its parent card.
        GImGui->GroupStack.back().EmitItem = false;
        ImGui::EndGroup();
        window->DC.CursorMaxPos = old_max;
        window->DC.IdealMaxPos = old_ideal;
        window->DC.PrevLineSize = old_prev;
        window->DC.PrevLineTextBaseOffset = old_base;
        EndInert();
        ImGui::PopClipRect();
        storage->SetFloat(height_id, size.y/GetTheme().scale);
        ImGui::Dummy(ImVec2(size.x, size.y*progress));
        ImGui::PopID();
    };
    try { content(); } catch (...) { finish(); throw; }
    finish();
}
void AllowScroll() { skui::AllowScroll(); }
bool ContainsActiveInput(const char *child_id) {
    if(!GImGui->ActiveId || GImGui->InputTextState.ID!=GImGui->ActiveId) return false;
    auto *parent=ImGui::GetCurrentWindow();auto id=parent->GetID(child_id);
    for(auto *window=GImGui->ActiveIdWindow;window && window!=parent;window=window->ParentWindow)
        if(window->ParentWindow==parent && window->ChildId==id) return true;
    return false;
}
bool CanScroll() {
    auto *active = GImGui->ActiveIdWindow;
    bool text = active && GImGui->InputTextState.ID == GImGui->ActiveId;
    bool background = active && GImGui->ActiveId == active->MoveId;
    return !text && (GImGui->ActiveId == 0 || background || GImGui->ActiveId == scrollable_item);
}
} // namespace detail

// Analytic oscillator solution; Miuix's default is response=.8, damping=.95.
// Adapted from Miuix NavProgrammaticEasing, commit 07d48590.
// Copyright 2026, compose-miuix-ui contributors. Apache-2.0.
// C++ adaptation adds critical/overdamping, validation and final-frame snap.
float SpringProgress(float fraction,float response,float damping) {
    if(!std::isfinite(fraction)) return 1;
    if(fraction<=0) return 0;if(fraction>=1) return 1;
    response=std::isfinite(response) ? std::clamp(response,.05f,4.f) : .8f;
    damping=std::isfinite(damping) ? std::clamp(damping,0.f,4.f) : .95f;
    double omega=2*IM_PI/response,t=fraction,z=damping;
    if(std::abs(z-1)<1e-4) return static_cast<float>(1-(1+omega*t)*std::exp(-omega*t));
    if(z<1) {
        double w=omega*std::sqrt(1-z*z);
        return static_cast<float>(1-std::exp(-z*omega*t)*(std::cos(w*t)+z*omega/w*std::sin(w*t)));
    }
    double a=-omega*(z-std::sqrt(z*z-1)),b=-omega*(z+std::sqrt(z*z-1));
    return static_cast<float>(1-(b*std::exp(a*t)-a*std::exp(b*t))/(b-a));
}
void SetHapticHandler(HapticHandler handler,void *data) {
    haptic_handler=handler;haptic_data=data;haptic_context=ImGui::GetCurrentContext();
}
void PerformHaptic(Haptic event) {
    if(theme.haptics && haptic_handler && haptic_context==ImGui::GetCurrentContext()) haptic_handler(event,haptic_data);
}
float Dp(float v) { return v * theme.scale; }
const Theme &GetTheme() { return theme; }
ImVec4 Background() { return Colors().background; }
ImVec4 Surface() { return Colors().surface; }
ImVec4 TextColor() { return Colors().text; }
ImVec4 MutedColor() { return Colors().muted; }
void ApplyTheme(const Theme &value) {
    theme = value;
    auto bounded = [](float v, float fallback, float lo, float hi) {
        return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
    };
    theme.scale = bounded(theme.scale,1,.5f,4);
    theme.motion_response = bounded(theme.motion_response,.8f,.05f,4);
    theme.motion_damping = bounded(theme.motion_damping,.95f,0,4);
    theme.motion_duration_scale=std::isfinite(theme.motion_duration_scale) ? std::clamp(theme.motion_duration_scale,.05f,10.f) : 1.f;
    auto &tokens = theme.tokens;
    DesignTokens defaults;
    for (auto field : {&DesignTokens::window_radius, &DesignTokens::card_radius,
            &DesignTokens::control_radius, &DesignTokens::popup_radius, &DesignTokens::row_radius,
            &DesignTokens::press_radius, &DesignTokens::menu_radius, &DesignTokens::scrollbar_size,
            &DesignTokens::border_width, &DesignTokens::shadow_size})
        tokens.*field = bounded(tokens.*field, defaults.*field, 0, 100);
    tokens.disabled_alpha = bounded(tokens.disabled_alpha,.38f,0,1);
    tokens.shadow_alpha = bounded(tokens.shadow_alpha,.012f,0,1);
    tokens.text_scale = bounded(tokens.text_scale,1,.5f,2);
    for (auto field : {&DesignTokens::window_padding, &DesignTokens::frame_padding,
            &DesignTokens::item_spacing, &DesignTokens::card_padding,
            &DesignTokens::surface_padding, &DesignTokens::popup_padding}) {
        (tokens.*field).x = bounded((tokens.*field).x,(defaults.*field).x,0,100);
        (tokens.*field).y = bounded((tokens.*field).y,(defaults.*field).y,0,100);
    }
    auto color = [&](ImVec4 &c, ImVec4 fallback) {
        c.x=bounded(c.x,fallback.x,0,1); c.y=bounded(c.y,fallback.y,0,1);
        c.z=bounded(c.z,fallback.z,0,1); c.w=bounded(c.w,fallback.w,0,1);
    };
    color(theme.accent,ImVec4(.2f,.48f,1,1));
    for (bool dark : {false,true}) {
        auto &palette = dark ? theme.dark_palette : theme.light;
        auto fallback = DefaultPalette(dark);
        for (auto field : {&Palette::background,&Palette::surface,&Palette::text,&Palette::muted,
                &Palette::border,&Palette::field,&Palette::field_hovered,&Palette::field_active,
                &Palette::selection,&Palette::selection_hovered,&Palette::scrollbar,&Palette::separator,
                &Palette::neutral,&Palette::danger_surface,&Palette::danger_text,&Palette::interaction,
                &Palette::icon_feedback,&Palette::toggle_off,&Palette::checkbox_off,&Palette::track,
                &Palette::badge,&Palette::navigation,&Palette::scrim})
            color(palette.*field,fallback.*field);
    }
    ImGuiStyle style;
    const auto &host_style=ImGui::GetStyle();
    style.FontSizeBase=host_style.FontSizeBase;
    style.FontScaleMain=host_style.FontScaleMain;
    style.FontScaleDpi=host_style.FontScaleDpi;
    const auto &t = theme.tokens;
    // Pixel-aligned padding prevents a subpixel scroll extent in short menus.
    style.WindowPadding = ImVec2(std::round(Dp(t.window_padding.x)), std::round(Dp(t.window_padding.y)));
    style.FramePadding = ImVec2(Dp(t.frame_padding.x), Dp(t.frame_padding.y));
    style.ItemSpacing = ImVec2(Dp(t.item_spacing.x), Dp(t.item_spacing.y));
    style.ItemInnerSpacing = ImVec2(Dp(8), Dp(6));
    style.WindowRounding = Dp(t.window_radius);
    style.ChildRounding = Dp(t.card_radius);
    style.FrameRounding = Dp(t.control_radius);
    style.PopupRounding = Dp(t.popup_radius);
    style.GrabRounding = Dp(8);
    style.ScrollbarRounding = Dp(t.scrollbar_size);
    style.ScrollbarSize = Dp(t.scrollbar_size);
    style.WindowBorderSize = Dp(t.border_width);
    style.ChildBorderSize = 0;
    style.PopupBorderSize = 0;
    style.FrameBorderSize = 0;
    style.GrabMinSize = Dp(22);
    style.DisabledAlpha = t.disabled_alpha;
    style.Colors[ImGuiCol_Text] = TextColor();
    style.Colors[ImGuiCol_TextDisabled] = MutedColor();
    style.Colors[ImGuiCol_WindowBg] = Background();
    style.Colors[ImGuiCol_ChildBg] = Surface();
    style.Colors[ImGuiCol_PopupBg] = Surface();
    style.Colors[ImGuiCol_Border] = Colors().border;
    style.Colors[ImGuiCol_FrameBg] = Colors().field;
    style.Colors[ImGuiCol_FrameBgHovered] = Colors().field_hovered;
    style.Colors[ImGuiCol_FrameBgActive] = Colors().field_active;
    style.Colors[ImGuiCol_Button] = theme.accent;
    style.Colors[ImGuiCol_ButtonHovered] =
        ImVec4(theme.accent.x * .9f, theme.accent.y * .9f, theme.accent.z * .9f, 1);
    style.Colors[ImGuiCol_ButtonActive] =
        ImVec4(theme.accent.x * .8f, theme.accent.y * .8f, theme.accent.z * .8f, 1);
    style.Colors[ImGuiCol_CheckMark] = theme.accent;
    style.Colors[ImGuiCol_SliderGrab] = theme.accent;
    style.Colors[ImGuiCol_SliderGrabActive] = theme.accent;
    style.Colors[ImGuiCol_Header] = Colors().selection;
    style.Colors[ImGuiCol_HeaderHovered] = Colors().selection_hovered;
    style.Colors[ImGuiCol_HeaderActive] = theme.accent;
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarGrab] = Colors().scrollbar;
    style.Colors[ImGuiCol_Separator] = Colors().separator;
    style.Colors[ImGuiCol_ModalWindowDimBg] = Colors().scrim;
    style.Colors[ImGuiCol_NavHighlight] = theme.accent;
    ImGui::GetStyle() = style;
}
void DrawIcon(Icon icon, ImVec2 c, float size, ImU32 color) {
    auto *d = ImGui::GetWindowDrawList();
    float r = size * .5f, t = Dp(1.7f);
    auto line = [&](float x, float y, float xx, float yy) {
        d->AddLine(Add(c, x * r, y * r), Add(c, xx * r, yy * r), color, t);
    };
    switch (icon) {
    case Icon::None:
        break;
    case Icon::Close:
        line(-.5f, -.5f, .5f, .5f);
        line(.5f, -.5f, -.5f, .5f);
        break;
    case Icon::Back:
        line(.25f, -.55f, -.3f, 0);
        line(-.3f, 0, .25f, .55f);
        break;
    case Icon::Chevron:
        line(-.2f, -.4f, .2f, 0);
        line(.2f, 0, -.2f, .4f);
        break;
    case Icon::Check:
        CheckMark(d, c, color, r);
        break;
    case Icon::Plus:
        line(-.55f, 0, .55f, 0);
        line(0, -.55f, 0, .55f);
        break;
    case Icon::Search:
        d->AddCircle(Add(c, -r * .15f, -r * .15f), r * .48f, color, 24, t);
        line(.22f, .22f, .65f, .65f);
        break;
    case Icon::Components:
        for (int y = -1; y <= 1; y += 2)
            for (int x = -1; x <= 1; x += 2)
                d->AddRect(Add(c, x * r * .47f - r * .28f, y * r * .47f - r * .28f),
                           Add(c, x * r * .47f + r * .28f, y * r * .47f + r * .28f), color, Dp(2), t);
        break;
    case Icon::Layout:
        d->AddRect(Add(c, -r * .7f, -r * .65f), Add(c, r * .7f, r * .65f), color, Dp(3), t);
        line(-.2f, -.65f, -.2f, .65f);
        line(-.2f, -.1f, .7f, -.1f);
        break;
    case Icon::Sidebar:
        d->AddRect(Add(c, -r * .8f, -r * .65f), Add(c, r * .8f, r * .65f), color, Dp(3), t);
        line(-.2f, -.65f, -.2f, .65f);
        break;
    case Icon::Navigation:
        line(-.65f, .55f, 0, -.65f);
        line(0, -.65f, .65f, .55f);
        line(.65f, .55f, 0, .3f);
        line(0, .3f, -.65f, .55f);
        break;
    case Icon::Settings:
        for (int i = 0; i < 3; ++i) {
            float y = (i - 1) * .55f, x = i == 1 ? .3f : -.25f;
            line(-.75f, y, .75f, y);
            d->AddCircleFilled(Add(c, x * r, y * r), r * .2f, color, 16);
        }
        break;
    case Icon::Sun:
    case Icon::Moon:
        d->AddCircle(c, r * .36f, color, 24, t);
        for (int i = 0; i < 8; ++i) {
            float a = i * 3.14159265f / 4;
            d->AddLine(Add(c, std::cos(a) * r * .6f, std::sin(a) * r * .6f),
                       Add(c, std::cos(a) * r * .82f, std::sin(a) * r * .82f), color, t);
        }
        break;
    case Icon::Heart:
        d->AddBezierCubic(Add(c, 0, r * .6f), Add(c, -r * 1.4f, -r * .3f), Add(c, -r * .5f, -r * 1.1f),
                          Add(c, 0, -r * .35f), color, t);
        d->AddBezierCubic(Add(c, 0, -r * .35f), Add(c, r * .5f, -r * 1.1f), Add(c, r * 1.4f, -r * .3f),
                          Add(c, 0, r * .6f), color, t);
        break;
    case Icon::Info:
        d->AddCircle(c, r * .7f, color, 24, t);
        line(0, -.05f, 0, .4f);
        d->AddCircleFilled(Add(c, 0, -r * .35f), Dp(1.3f), color);
        break;
    }
}
void Text(const char *text) { ImGui::TextUnformatted(text); }
void Caption(const char *text) {
    ImGui::PushStyleColor(ImGuiCol_Text, MutedColor());
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
void Heading(const char *text) {
    auto p = ImGui::GetCursorScreenPos();
    Label(ImGui::GetWindowDrawList(), p, text, TextColor(), 28);
    ImGui::Dummy(ImVec2(0, std::max(Dp(38),TextSize(28)+Dp(10))));
}
void Section(const char *title) {
    Space(9);
    auto p = ImGui::GetCursorScreenPos();
    Label(ImGui::GetWindowDrawList(), Add(p, Dp(4), 0), title, MutedColor(), 13);
    ImGui::Dummy(ImVec2(0, std::max(Dp(22),TextSize(13)+Dp(9))));
}
void Divider() { ImGui::Separator(); }
void Space(float h) { ImGui::Dummy(ImVec2(0, Dp(h))); }
void Collapsible(const char *id, bool expanded, const std::function<void()> &content) {
    ImGui::PushID(id);
    float progress = detail::Animate("reveal", expanded ? 1.f : 0.f, .32f);
    detail::Reveal("contents", progress, !expanded, content);
    ImGui::PopID();
}
bool BeginCard(const char *id) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Dp(theme.tokens.card_padding.x), Dp(theme.tokens.card_padding.y)));
    // A keyboard can shrink the parent before it scrolls the focused field
    // into view. Keep this card measuring for that frame: dropping its contents
    // would end the host IME session and immediately dismiss the keyboard.
    auto active_input=detail::ContainsActiveInput(id) ? ImGuiChildFlags_AlwaysAutoResize : 0;
    return ImGui::BeginChild(id, ImVec2(0, 0),
                             ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding |
                             ImGuiChildFlags_NavFlattened | active_input,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}
void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
bool detail::BeginScroll(const char *id, ImVec2 size, bool horizontal, ImGuiWindowFlags flags,ImGuiChildFlags child_flags) {
    // Set the target before BeginChild lays out this frame. SetScrollY after
    // drawing defers the movement until the next frame and visibly trails input.
    auto *storage = ImGui::GetStateStorage();
    ImGui::PushID(id);
    auto key = ImGui::GetID("##scroll-window");
    ImGui::PopID();
    auto *window = ImGui::FindWindowByID(static_cast<ImGuiID>(storage->GetInt(key)));
    auto *hovered = GImGui->HoveredWindow;
    auto &io = ImGui::GetIO();
    // A nested picker/refresh list owns its drag. The outer page must not also
    // move underneath it. Cards are not marked and still forward page gestures.
    auto axis_key = ImHashStr("skui/scroll-axis");
    bool nested = false;
    for (auto *child = hovered; child && child != window; child = child->ParentWindow) {
        if (child->StateStorage.GetInt(axis_key) == (horizontal ? 2 : 1)) {
            nested = true;
            break;
        }
    }
    bool popup = window && (window->RootWindow->Flags & ImGuiWindowFlags_Popup);
    if (window && hovered && !nested && ImGui::IsWindowChildOf(hovered, window, true) &&
        (popup || !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) &&
        ImGui::IsMouseDragging(0, Dp(8)) &&
        (horizontal ? std::abs(io.MouseDelta.x) > std::abs(io.MouseDelta.y)
                    : std::abs(io.MouseDelta.y) >= std::abs(io.MouseDelta.x))) {
        // ImGui claims a window MoveId on blank or disabled child contents,
        // even when NoMove is set. Treat that capture as a page gesture too.
        if (detail::CanScroll()) {
            ImGui::ClearActiveID();
            ImGui::SetNextWindowScroll(horizontal
                ? ImVec2(std::clamp(window->Scroll.x - io.MouseDelta.x, 0.f, window->ScrollMax.x), -1)
                : ImVec2(-1, std::clamp(window->Scroll.y - io.MouseDelta.y, 0.f, window->ScrollMax.y)));
        }
    }
    bool visible = ImGui::BeginChild(id, size, child_flags, flags | ImGuiWindowFlags_NoBackground |
                                    (horizontal ? ImGuiWindowFlags_HorizontalScrollbar : 0));
    ImGui::GetCurrentWindow()->StateStorage.SetInt(axis_key, horizontal ? 2 : 1);
    storage->SetInt(key, static_cast<int>(ImGui::GetCurrentWindow()->ID));
    return visible;
}
bool BeginScroll(const char *id, ImVec2 size, bool horizontal) {
    return detail::BeginScroll(id,size,horizontal,0);
}
void EndScroll() { ImGui::EndChild(); }
bool Button(const char *label, Tone tone, ImVec2 size) {
    if (ImGui::GetCurrentWindow()->SkipItems) return false;
    if (size.y <= 0)
        size.y = std::max(Dp(44),ImGui::GetFontSize()+ImGui::GetStyle().FramePadding.y*2);
    ImVec4 bg = tone == Tone::Primary  ? theme.accent
                : tone == Tone::Danger ? Colors().danger_surface
                                       : Colors().neutral;
    ImVec4 fg = tone == Tone::Primary  ? ImVec4(1, 1, 1, 1)
                : tone == Tone::Danger ? Colors().danger_text
                                       : TextColor();
    ImGui::PushID(label);
    bg.x=detail::Animate("bg/r",bg.x); bg.y=detail::Animate("bg/g",bg.y); bg.z=detail::Animate("bg/b",bg.z);
    fg.x=detail::Animate("fg/r",fg.x); fg.y=detail::Animate("fg/g",fg.y); fg.z=detail::Animate("fg/b",fg.z);
    ImGui::PopID();
    // Reserve the background before ImGui draws its text/nav cursor, then tint
    // those vertices after input is known. No per-button channel allocations.
    auto *draw = ImGui::GetWindowDrawList();
    auto p=ImGui::GetCursorScreenPos();
    auto text=ImGui::CalcTextSize(label,nullptr,true);
    auto padding=ImGui::GetStyle().FramePadding;
    auto bounds=ImGui::CalcItemSize(size,text.x+padding.x*2,text.y+padding.y*2);
    int first=draw->VtxBuffer.Size;
    draw->AddRectFilled(p,Add(p,bounds.x,bounds.y),IM_COL32_WHITE,ImGui::GetStyle().FrameRounding);
    int last=draw->VtxBuffer.Size;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, fg);
    bool clicked = ImGui::Button(label, size);
    AllowScroll();
    float dim=detail::InteractionFeedback(.06f,.15f);
    bg.x*=1-dim; bg.y*=1-dim; bg.z*=1-dim;
    ImU32 color=U(bg);
    for(int i=first;i<last;++i) {
        auto &vertex=draw->VtxBuffer[i];
        auto alpha=((vertex.col>>IM_COL32_A_SHIFT)&255u)*((color>>IM_COL32_A_SHIFT)&255u)/255u;
        vertex.col=(color&~IM_COL32_A_MASK)|(alpha<<IM_COL32_A_SHIFT);
    }
    ImGui::PopStyleColor(4);
    return clicked;
}
bool IconButton(const char *id, Icon icon) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(Dp(36), Dp(36)),ImGuiButtonFlags_EnableNav);
    float a=detail::InteractionFeedback(.5f,1.f);
    auto bg=Colors().icon_feedback; bg.w*=a;
    if (a>0)
        ImGui::GetWindowDrawList()->AddRectFilled(p, Add(p, Dp(36), Dp(36)), U(bg),
                                                  Dp(12));
    DrawIcon(icon, Add(p, Dp(18), Dp(18)), Dp(20), U(TextColor()));
    return clicked;
}
bool Toggle(const char *label, bool *value, const char *description) {
    ImGui::PushID(label);
    Row row("##toggle",detail::RowTextHeight(label,description,Width()-Dp(62),description ? 66 : 52));
    if (row.pressed)
        *value = !*value;
    float a = detail::Animate("##position", *value ? 1.f : 0.f);
    auto *d = ImGui::GetWindowDrawList();
    float cy = (row.min.y + row.max.y) * .5f;
    detail::RowText(row.min,row.max,label,description,0,Dp(62));
    ImVec2 p(row.max.x - Dp(46), cy - Dp(13));
    ImVec4 off = Colors().toggle_off;
    ImVec4 bg = ImLerp(off, theme.accent, a);
    d->AddRectFilled(p, Add(p, Dp(46), Dp(26)), U(bg), Dp(13));
    d->AddCircleFilled(Add(p, Dp(13 + 20 * a), Dp(13)), Dp(9.5f), U(ImVec4(1, 1, 1, 1)), 24);
    ImGui::PopID();
    return row.pressed;
}
bool Checkbox(const char *label, bool *value) {
    ImGui::PushID(label);
    Row row("##check",detail::RowTextHeight(label,nullptr,Width()-Dp(36),48));
    if (row.pressed)
        *value = !*value;
    auto *d = ImGui::GetWindowDrawList();
    ImVec2 c = Add(row.min, Dp(12), (row.max.y-row.min.y)*.5f);
    float a = detail::Animate("selected", *value ? 1.f : 0.f);
    d->AddCircleFilled(c, Dp(11), U(ImLerp(Colors().checkbox_off, theme.accent, a)), 24);
    if (a > 0)
        CheckMark(d, c, U(ImVec4(1, 1, 1, a)), Dp(10) * (.6f + .4f*a));
    detail::RowText(row.min,row.max,label,nullptr,Dp(36));
    ImGui::PopID();
    return row.pressed;
}
bool Radio(const char *label, int *value, int option) {
    ImGui::PushID(label);
    Row row("##radio",detail::RowTextHeight(label,nullptr,Width()-Dp(36),48));
    if (row.pressed)
        *value = option;
    ImVec2 c = Add(row.min, Dp(12), (row.max.y-row.min.y)*.5f);
    auto *d = ImGui::GetWindowDrawList();
    float a = detail::Animate("selected", *value == option ? 1.f : 0.f);
    d->AddCircle(c, Dp(10), U(ImLerp(MutedColor(), theme.accent, a)), 24, Dp(2));
    if (a > 0)
        d->AddCircleFilled(c, Dp(5.5f)*a, U(theme.accent), 24);
    detail::RowText(row.min,row.max,label,nullptr,Dp(36));
    ImGui::PopID();
    return row.pressed;
}
bool Slider(const char *label, float *value, float minimum, float maximum, const char *format, ImGuiSliderFlags flags) {
    if(!value || maximum<=minimum) return false;
    ImGui::PushID(label);
    if(std::strncmp(label,"##",2)!=0) Text(label);
    ImGui::SetNextItemWidth(Width());
    // Preserve ImGui's keyboard/drag behaviour while drawing a rounded track
    // and circular thumb instead of its desktop frame and vertical grab.
    for(auto color:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,
                    ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,ImGuiCol_Text})
        ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
    bool result = ImGui::SliderFloat("##slider", value, minimum, maximum, format, flags);
    ImGui::PopStyleColor(6);
    auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
    auto *draw=ImGui::GetWindowDrawList(); float radius=Dp(10),cy=(min.y+max.y)/2;
    float left=min.x+radius,right=max.x-radius;
    float x=left+(right-left)*std::clamp((*value-minimum)/(maximum-minimum),0.f,1.f);
    draw->AddRectFilled(ImVec2(left,cy-Dp(3)),ImVec2(right,cy+Dp(3)),U(Colors().track),Dp(3));
    if(x>left) draw->AddRectFilled(ImVec2(left,cy-Dp(3)),ImVec2(x,cy+Dp(3)),U(theme.accent),Dp(3));
    draw->AddCircleFilled(ImVec2(x,cy),radius,U(theme.accent),32);
    char number[64];ImFormatString(number,sizeof(number),format ? format : "%.0f",*value);
    auto text=ImGui::CalcTextSize(number);
    draw->AddText(ImVec2(max.x-text.x,min.y-text.y-Dp(4)),U(MutedColor()),number);
    ImGui::PopID();
    return result;
}
bool InputText(const char *label, char *buffer, std::size_t capacity, const char *hint) {
    ImGui::PushID(label);
    Text(label);
    ImGui::SetNextItemWidth(Width());
    bool result = ImGui::InputTextWithHint("##input", hint, buffer, capacity);
    result = UpdateTextInput(buffer, capacity) || result;
    ImGui::PopID();
    return result;
}
bool SearchField(const char *id, char *buffer, std::size_t capacity) {
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(Width());
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Dp(24));
    bool changed = ImGui::InputTextWithHint("##search", "搜索组件 / Search", buffer, capacity);
    changed = UpdateTextInput(buffer, capacity) || changed;
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}
void SetTextInputHandler(TextInputHandler handler, void *user_data) {
    text_input_handler = handler;
    text_input_user_data = user_data;
}
bool Combo(const char *label, int *selected, const char *const *items, int count) {
    if(!items || !selected || count<=0) return false;
    std::vector<ChoiceItem> choices;
    choices.reserve(count);
    for(int i=0;i<count;++i) choices.push_back({items[i]});
    return Dropdown(label,selected,choices.data(),count);
}
bool ListItem(const char *id, const char *title, const char *description, const char *value, bool arrow) {
    ImGui::PushID(id);
    float value_width=value ? std::min(Width()*.38f,ImGui::GetFont()->CalcTextSizeA(TextSize(13),FLT_MAX,0,value).x) : 0;
    float right=Dp(arrow ? 30 : 0)+(value ? value_width+Dp(10) : 0);
    Row row("##item",detail::RowTextHeight(title,description,Width()-right,description ? 68 : 50));
    auto *d = ImGui::GetWindowDrawList();
    detail::RowText(row.min,row.max,title,description,0,right);
    if (value) {
        d->PushClipRect(ImVec2(row.max.x-value_width-Dp(arrow ? 26 : 0),row.min.y),ImVec2(row.max.x-Dp(arrow ? 26 : 0),row.max.y),true);
        Label(d, ImVec2(row.max.x - value_width - Dp(arrow ? 26 : 0), (row.min.y + row.max.y - TextSize(13)) * .5f),
              value, MutedColor(), 13);
        d->PopClipRect();
    }
    if (arrow)
        DrawIcon(Icon::Chevron, ImVec2(row.max.x - Dp(8), (row.min.y + row.max.y) * .5f), Dp(20),
                 U(MutedColor()));
    ImGui::PopID();
    return row.pressed;
}
bool TabBar(const char *id, const char *const *labels, int count, int *selected) {
    if (count <= 0 || !labels || !selected || ImGui::GetCurrentWindow()->SkipItems)
        return false;
    ImGui::PushID(id);
    float w = std::max(1.f,(Width() - Dp(4) * (count - 1)) / count);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        if (i)
            ImGui::SameLine(0, Dp(4));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("tab", ImVec2(w, Dp(38)), ImGuiButtonFlags_EnableNav)) {
            changed = *selected != i;
            *selected = i;
        }
        detail::AllowScroll();
        ImGui::PopID();
    }
    auto *draw = ImGui::GetWindowDrawList();
    for (int i=0;i<count;++i) {
        auto p=Add(origin,i*(w+Dp(4)),0);
        draw->AddRectFilled(p,Add(p,w,Dp(38)),U(Colors().neutral),Dp(14));
    }
    float position=detail::Animate("indicator",static_cast<float>(std::clamp(*selected,0,count-1)),.26f);
    auto indicator=Add(origin,position*(w+Dp(4)),0);
    draw->AddRectFilled(indicator,Add(indicator,w,Dp(38)),U(theme.accent),Dp(14));
    for(int i=0;i<count;++i) {
        float a=std::max(0.f,1.f-std::abs(position-i));
        auto size=ImGui::GetFont()->CalcTextSizeA(TextSize(16),FLT_MAX,0,labels[i]);
        auto p=Add(origin,i*(w+Dp(4)),0);
        draw->PushClipRect(p,Add(p,w,Dp(38)),true);
        Label(draw,Add(p,(w-size.x)/2,(Dp(38)-size.y)/2),labels[i],ImLerp(TextColor(),ImVec4(1,1,1,1),a));
        draw->PopClipRect();
        ImGui::PushID(i);
        ImGui::RenderNavCursor(ImRect(p,Add(p,w,Dp(38))),ImGui::GetID("tab"));
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}
void Progress(float fraction, const char *label) {
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, theme.accent);
    ImGui::ProgressBar(std::clamp(fraction, 0.f, 1.f), ImVec2(-1, Dp(10)), "");
    ImGui::PopStyleColor();
    if (label)
        Caption(label);
}
void Badge(const char *label) {
    auto p = ImGui::GetCursorScreenPos();
    auto size = ImGui::CalcTextSize(label);
    ImVec2 max = Add(p, size.x + Dp(20), size.y + Dp(8));
    ImGui::GetWindowDrawList()->AddRectFilled(p, max, U(Colors().badge), Dp(7));
    Label(ImGui::GetWindowDrawList(), Add(p, Dp(10), Dp(4)), label, theme.accent, 13);
    ImGui::Dummy(ImVec2(max.x - p.x, max.y - p.y));
}
void Swatch(const char *id, ImVec4 color, bool selected, bool *pressed) {
    auto p = ImGui::GetCursorScreenPos();
    *pressed = ImGui::InvisibleButton(id, ImVec2(Dp(42), Dp(42)),ImGuiButtonFlags_EnableNav);
    auto c = Add(p, Dp(21), Dp(21));
    auto *d = ImGui::GetWindowDrawList();
    d->AddCircleFilled(c, Dp(17), U(color), 32);
    float a = detail::Animate(ImHashStr("selected",0,ImGui::GetItemID()), selected ? 1.f : 0.f);
    if (a > 0)
        CheckMark(d, c, U(ImVec4(1, 1, 1, a)), Dp(13)*(.6f+.4f*a));
}
void OpenDialog(const char *id) { ImGui::OpenPopup(id); }
bool BeginDialog(const char *id, const char *title, float width) {
    ImGui::SetNextWindowSize(ImVec2(Dp(width), 0), ImGuiCond_Always);
    auto *parent = ImGui::GetCurrentWindow()->RootWindow;
    ImGui::SetNextWindowPos(Add(parent->Pos, parent->Size.x * .5f, parent->Size.y * .5f), ImGuiCond_Appearing,
                            ImVec2(.5f, .5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(std::round(Dp(16)), std::round(Dp(18))));
    bool visible = detail::BeginOverlay(id, detail::OverlayKind::Dialog,
                                          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                                              ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar();
    if (!visible)
        return false;
    Space(5);
    Heading(title);
    return true;
}
void EndDialog() { detail::EndOverlay(); }
void CloseDialog() { detail::CloseOverlay(); }

bool BeginShell(const ShellOptions &o) {
    if (o.back_requested)
        *o.back_requested = false;
    if (o.close_requested)
        *o.close_requested = false;
    auto *previous=ImGui::FindWindowByName(o.id);
    WindowPlacement placement{previous ? previous->Pos : o.initial_position,previous ? previous->Size : o.initial_size};
    // Resolve all geometry before Begin, so background, children and hit boxes
    // use the same bounds on the first keyboard/rotation frame.
    if(previous && GImGui->ActiveId==previous->GetID("##drag") && ImGui::IsMouseDragging(0))
        placement.position=Add(placement.position,ImGui::GetIO().MouseDelta.x,ImGui::GetIO().MouseDelta.y);
    auto *viewport=ImGui::GetMainViewport();
    auto bounds=ResolveViewport(viewport->Size,o.safe_insets);
    bounds.position=Add(bounds.position,viewport->Pos.x,viewport->Pos.y);
    ImVec2 minimum(Dp(330),Dp(420));
    auto initial_unlimited=placement;
    if(previous) {
        auto &saved=previous->StateStorage;
        auto limited=previous->GetID("##ime-limited");
        auto x=previous->GetID("##ime-x"),y=previous->GetID("##ime-y");
        auto width=previous->GetID("##ime-width"),height=previous->GetID("##ime-height");
        bool was_limited=saved.GetBool(limited);
        if(o.visible_bottom>0 && std::isfinite(o.visible_bottom)) {
            if(!was_limited) {
                saved.SetFloat(x,placement.position.x);saved.SetFloat(y,placement.position.y);
                saved.SetFloat(width,placement.size.x);saved.SetFloat(height,placement.size.y);
            }
        } else if(was_limited) {
            placement={ImVec2(saved.GetFloat(x),saved.GetFloat(y)),ImVec2(saved.GetFloat(width),saved.GetFloat(height))};
        }
        saved.SetBool(limited,o.visible_bottom>0 && std::isfinite(o.visible_bottom));
    }
    bool keyboard=o.visible_bottom>0 && std::isfinite(o.visible_bottom);
    if(keyboard) {
        bounds.size.y=std::max(0.f,std::min(bounds.position.y+bounds.size.y,o.visible_bottom-Dp(8))-bounds.position.y);
        placement.position.y=std::clamp(placement.position.y,bounds.position.y,
            bounds.position.y+std::max(0.f,bounds.size.y-minimum.y));
        placement.size.y=std::min(placement.size.y,bounds.position.y+bounds.size.y-placement.position.y);
    }
    if(o.constrain_to_viewport || keyboard) {
        placement=FitWindow(placement,bounds,minimum);
        minimum=ImVec2(std::min(minimum.x,bounds.size.x),std::min(minimum.y,bounds.size.y));
    }
    if(!previous || placement.position.x!=previous->Pos.x || placement.position.y!=previous->Pos.y)
        ImGui::SetNextWindowPos(placement.position);
    if(!previous || placement.size.x!=previous->Size.x || placement.size.y!=previous->Size.y)
        ImGui::SetNextWindowSize(placement.size);
    auto maximum=o.constrain_to_viewport || keyboard ? bounds.size : ImVec2(FLT_MAX,FLT_MAX);
    ImGui::SetNextWindowSizeConstraints(minimum,maximum);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool visible = ImGui::Begin(o.id, nullptr,
                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto *storage = ImGui::GetStateStorage();
    if(!previous && keyboard) {
        storage->SetBool(ImGui::GetID("##ime-limited"),true);
        storage->SetFloat(ImGui::GetID("##ime-x"),initial_unlimited.position.x);
        storage->SetFloat(ImGui::GetID("##ime-y"),initial_unlimited.position.y);
        storage->SetFloat(ImGui::GetID("##ime-width"),initial_unlimited.size.x);
        storage->SetFloat(ImGui::GetID("##ime-height"),initial_unlimited.size.y);
    }
    auto pos = ImGui::GetWindowPos();
    auto size = ImGui::GetWindowSize();
    auto *d = ImGui::GetWindowDrawList();
    shell.position = pos;
    shell.size = size;
    shell.wide = size.x >= Dp(700);
    float rail = 0;
    float top = Dp(o.content_includes_header ? 44 : 116), bottom = shell.wide ? Dp(12) : Dp(76);
    // Drag only this quiet strip; regular content never drags the application.
    ImGui::SetCursorPos(ImVec2(Dp(16), Dp(5)));
    ImGui::InvisibleButton("##drag", ImVec2(std::max(Dp(20), size.x - Dp(70)), Dp(28)));
    Label(d, Add(pos, Dp(20), Dp(14)), "sk-imgui-ui", MutedColor(), 11);
    ImGui::SetCursorPos(ImVec2(size.x - Dp(50), Dp(6)));
    if (IconButton("##close", Icon::Close) && o.close_requested)
        *o.close_requested = true;
    if (shell.wide && o.items && o.selected && o.item_count > 0) {
        auto expanded_id = ImGui::GetID("##sidebar-expanded");
        bool stored_expanded = storage->GetBool(expanded_id, true);
        bool *expanded = o.sidebar_expanded ? o.sidebar_expanded : &stored_expanded;
        ImGui::SetCursorPos(ImVec2(Dp(8), Dp(52)));
        NavigationRail("##shell-rail", o.items, o.item_count, o.selected, expanded);
        rail = ImGui::GetItemRectSize().x + Dp(16);
        storage->SetBool(expanded_id, *expanded);
    }
    float titleX = rail + Dp(20);
    if (o.can_go_back && !o.content_includes_header) {
        ImGui::SetCursorPos(ImVec2(titleX, Dp(48)));
        if (IconButton("##back", Icon::Back) && o.back_requested)
            *o.back_requested = true;
        titleX += Dp(44);
    }
    if (!o.content_includes_header) {
        Label(d, Add(pos, titleX, Dp(48)), o.title, TextColor(), 28);
        if (o.subtitle)
            Label(d, Add(pos, titleX, Dp(85)), o.subtitle, MutedColor(), 12);
    }
    if (shell.wide)
        d->AddLine(Add(pos, rail, Dp(44)), Add(pos, rail, size.y - Dp(16)), U(Color(0xE4E7ED, 0x31343D)),
                   Dp(1));
    if (!shell.wide && o.items && o.selected && o.item_count > 0) {
        float cell = shell.wide ? rail - Dp(20) : size.x / o.item_count;
        if (!shell.wide)
            d->AddRectFilled(Add(pos, Dp(1), size.y - bottom), Add(pos, size.x - Dp(1), size.y - Dp(1)),
                             U(Surface()), Dp(24), ImDrawFlags_RoundCornersBottom);
        for (int i = 0; i < o.item_count; ++i) {
            ImVec2 p =
                shell.wide ? Add(pos, Dp(10), Dp(114 + i * 54)) : Add(pos, i * cell, size.y - bottom + Dp(7));
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##nav", ImVec2(cell, Dp(50)),ImGuiButtonFlags_EnableNav))
                *o.selected = i;
            float a = detail::Animate("selected", *o.selected == i ? 1.f : 0.f);
            ImGui::PopID();
            ImVec4 color = ImLerp(MutedColor(), theme.accent, a);
            if (a > 0 && shell.wide)
                d->AddRectFilled(p, Add(p, cell, Dp(48)), U(Colors().navigation), Dp(14));
            ImVec2 c = shell.wide ? Add(p, Dp(22), Dp(24)) : Add(p, cell * .5f, Dp(16));
            DrawIcon(o.items[i].icon, c, Dp(21), U(color));
            if (shell.wide)
                Label(d, Add(p, Dp(44), Dp(16)), o.items[i].label, color, 14);
            else {
                auto ts = ImGui::GetFont()->CalcTextSizeA(TextSize(11), FLT_MAX, 0, o.items[i].label);
                Label(d, Add(p, (cell - ts.x) * .5f, Dp(34)), o.items[i].label, color, 11);
            }
        }
    }
    ImGui::SetCursorPos(ImVec2(rail + Dp(16), top));
    shell.content = ImVec2(size.x - rail - Dp(32), size.y - top - bottom);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Dp(2), 0));
    ImGui::BeginChild("##page-host", shell.content, ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    return visible;
}
ImVec2 ShellPosition() { return shell.position; }
ImVec2 ShellSize() { return shell.size; }
void EndShell() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
}
void Snackbar::Show(std::string message, float seconds) {
    message_ = std::move(message);
    until_ = ImGui::GetTime() + seconds;
}
void Snackbar::Draw(ImVec2 p, float width) {
    if (message_.empty())
        return;
    double now = ImGui::GetTime();
    bool showing = now < until_;
    if (showing != showing_) { from_ = opacity_; elapsed_ = 0; showing_ = showing; }
    if (last_draw_ != now) elapsed_ += detail::MotionDeltaTime();
    last_draw_ = now;
    float t = GetTheme().animations ? std::clamp(elapsed_/.24f,0.f,1.f) : 1.f;
    opacity_ = ImLerp(from_, showing ? 1.f : 0.f, t*t*(3-2*t));
    if(t<1 && from_!=(showing ? 1.f : 0.f)) detail::RequestMotionFrame();
    if(showing) RequestFrameAfter(until_-now);
    if (opacity_ <= 0) return;
    p.y += Dp(14)*(1-opacity_);
    auto *d = ImGui::GetForegroundDrawList();
    d->AddRectFilled(p, Add(p, width, Dp(46)), IM_COL32(36, 39, 47, static_cast<int>(245*opacity_)), Dp(14));
    d->AddText(ImGui::GetFont(), TextSize(14), Add(p, Dp(16), Dp(14)), IM_COL32(255, 255, 255, static_cast<int>(255*opacity_)),
               message_.c_str());
}
} // namespace skui
