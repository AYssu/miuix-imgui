#pragma once
#include "imgui.h"
#include <cstddef>

namespace skui {
// Rendering-thread, current-context controls. Changes take effect next frame.
// Only presentation motion is affected, never IO clocks or background work.
struct AnimationPlayback { float speed=1; bool paused=false; };
void SetAnimationPlayback(AnimationPlayback playback);
AnimationPlayback GetAnimationPlayback();
void StepAnimations(float seconds=1.f/60); // One bounded step while paused.

struct InspectedItem {
    static constexpr std::size_t PathCapacity=96;
    char path[PathCapacity]{};
    ImGuiID id=0, window=0;
    ImVec2 min{},max{},hit_min{},hit_max{};
    int frame=-1;
    bool visible=false, disabled=false, active=false, focused=false, hovered=false, ambiguous=false;
};
struct InspectionStatistics { std::size_t count=0, rejected=0, input_events=0; int frame=-1; };
// Explicit opt-in. At most 128 marked items and 64 recent input summaries.
// Paths are stable application names, not translated labels; no text values
// are captured. Disabling releases the registry and its context hooks.
void SetItemInspectionEnabled(bool enabled);
bool ItemInspectionEnabled();
bool InspectLastItem(const char *path);
bool FindInspectedItem(const char *path,InspectedItem &result);
InspectionStatistics GetInspectionStatistics();
// Draw inside the caller's existing window. Built-in ImGui Metrics and ID
// Stack windows can be opened separately by the host.
void DrawInteractionInspector();
} // namespace skui
