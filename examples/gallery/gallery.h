#pragma once
#include "skui/skui.h"

namespace gallery {
// Caller owns ImGui context, fonts, renderer and input. These same components
// can be used without this gallery or its Android executable.
void Draw(bool *running, ImVec2 display_size, float keyboard_height = 0);
void Notify(const char *message);
float FontScale();
} // namespace gallery
