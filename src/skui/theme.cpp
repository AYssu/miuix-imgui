#include "skui/skui.h"
#include <algorithm>
#include <cmath>

namespace skui {
Palette DefaultPalette(bool dark) {
    auto rgb = [dark](unsigned light, unsigned night) {
        auto c = dark ? night : light;
        return ImVec4(((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, 1);
    };
    return {rgb(0xF5F6F8,0x121316), rgb(0xFFFFFF,0x202126), rgb(0x181A20,0xF3F4F7),
        rgb(0x7B808D,0x9CA1AE), rgb(0xE0E3E9,0x383B43), rgb(0xEFF1F5,0x2B2E36),
        rgb(0xE5E9F1,0x353945), rgb(0xDCE4F3,0x3C4353), rgb(0xE9EDF5,0x343A48),
        rgb(0xE1E8F5,0x3B4354), rgb(0xCCD0DA,0x525866), rgb(0xECEEF2,0x33363E),
        rgb(0xECEEF3,0x333640), rgb(0xFCEBEB,0x46282D), rgb(0xDB3D4C,0xFF8A95),
        rgb(0xF0F3FA,0x30343E), rgb(0xE6E9EF,0x353943), rgb(0xE4E6EA,0x494D57),
        rgb(0xE5E7EC,0x444955), rgb(0xE4E6EC,0x444955), rgb(0xE8F0FF,0x243650),
        rgb(0xE5EDFC,0x26344C)};
}
const Palette &Colors() { return GetTheme().dark ? GetTheme().dark_palette : GetTheme().light; }
float TextSize(float size) { return Dp(size * GetTheme().tokens.text_scale); }
ThemeScope::ThemeScope(const Theme &value)
    : previous_(GetTheme()), style_(ImGui::GetStyle()), context_(ImGui::GetCurrentContext()) {
    ApplyTheme(value);
}
ThemeScope::~ThemeScope() {
    IM_ASSERT(ImGui::GetCurrentContext() == context_ && "ThemeScope must end in its original context");
    if (ImGui::GetCurrentContext() == context_) {
        ApplyTheme(previous_);
        ImGui::GetStyle() = style_;
    }
}
} // namespace skui
