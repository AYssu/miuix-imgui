#pragma once
#include <limits>

namespace skui {
struct FrameRequest {
    bool continuous = false;
    double wake_after_seconds = std::numeric_limits<double>::infinity();
};
// Rendering-thread hints, scoped to the current ImGui context/frame. A host
// still owns scheduling and event delivery. One state is owned by each context
// and retired on shutdown; no per-frame allocation, timer or thread.
void RequestAnimationFrame();
void RequestFrameAfter(double seconds);
FrameRequest CurrentFrameRequest();
} // namespace skui
