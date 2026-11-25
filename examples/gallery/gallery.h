#pragma once
#include "skui/skui.h"
#include "skui/diagnostics.h"

namespace gallery {
// Caller owns ImGui context, fonts, renderer and input. These same components
// can be used without this gallery or its Android executable.
void Draw(bool *running, ImVec2 display_size, float keyboard_height = 0);
void Notify(const char *message);
float FontScale();
skui::FrameProfiler &Profiler();
// Host explicitly opts into storage; call before the first Draw. Flushing is
// outside the UI/render frame, on explicit save or shutdown only.
void ConfigurePreferences(const char *path);
void FlushPreferences(bool exiting = false);
} // namespace gallery
