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
// Configure before starting a task. Shutdown joins producers before the host
// destroys wake/input/render resources; it never owns the host's wake source.
void SetWakeHandler(void (*handler)(void *),void *user=nullptr);
void Shutdown();
} // namespace gallery
