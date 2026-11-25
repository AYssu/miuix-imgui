#pragma once
#include "imgui.h"

namespace skui {
// Semantic colors are unpremultiplied sRGB. The default palettes preserve the
// existing gallery; applications can edit either palette before ApplyTheme.
struct Palette {
    ImVec4 background, surface, text, muted, border;
    ImVec4 field, field_hovered, field_active;
    ImVec4 selection, selection_hovered, scrollbar, separator;
    ImVec4 neutral, danger_surface, danger_text, interaction, icon_feedback;
    ImVec4 toggle_off, checkbox_off, track, badge, navigation;
    ImVec4 scrim{0, 0, 0, .32f};
};
Palette DefaultPalette(bool dark = false);
struct DesignTokens {
    float window_radius = 24, card_radius = 18, control_radius = 12, popup_radius = 22;
    float row_radius = 14, press_radius = 10, menu_radius = 14;
    ImVec2 window_padding{20, 14}, frame_padding{14, 12}, item_spacing{10, 8};
    ImVec2 card_padding{16, 10}, surface_padding{16, 12}, popup_padding{16, 12};
    float scrollbar_size = 5, border_width = 1, disabled_alpha = .38f;
    float shadow_size = 6, shadow_alpha = .012f;
    // Multiplies custom text sizes as well as the gallery's host font size.
    // Layout must still be checked when an application chooses larger text.
    float text_scale = 1;
};
} // namespace skui
