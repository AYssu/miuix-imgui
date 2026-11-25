#pragma once
#include "imgui.h"
#include <array>
#include <chrono>
#include <cstddef>

namespace skui {
struct FrameSample {
    double interval_ms = 0, input_ms = 0, build_ms = 0, render_ms = 0, cpu_ms = 0, effects_ms = 0;
    int vertices = 0, indices = 0, textures = 0, popups = 0;
    ImGuiID active = 0, focus = 0;
};
struct FrameStatistics {
    std::size_t samples = 0;
    double cpu_p50_ms = 0, cpu_p95_ms = 0, interval_p50_ms = 0, interval_p95_ms = 0;
    FrameSample latest;
};
// One per rendering thread/host. Disabled by default; no clocks or allocation
// on the disabled path. Storage is fixed; percentile sorting is on demand.
class FrameProfiler {
  public:
    static constexpr std::size_t Capacity = 240;
    void SetEnabled(bool enabled);
    bool Enabled() const { return enabled_; }
    void Clear();
    void BeginFrame();
    void EndInput();
    void EndBuild();
    void AddEffectTime(double milliseconds); // Nested within build_ms, not added to cpu_ms.
    void EndFrame(); // After rendering; captures ImGui's full-frame metrics.
    bool Record(const FrameSample &sample); // For hosts with their own clocks.
    FrameStatistics Statistics() const;
    std::size_t SampleCount() const { return size_; }
    FrameSample Latest() const { return size_ ? samples_[(next_+Capacity-1)%Capacity] : FrameSample{}; }
  private:
    using Clock = std::chrono::steady_clock;
    std::array<FrameSample, Capacity> samples_{};
    std::size_t next_ = 0, size_ = 0;
    bool enabled_ = false, in_frame_ = false, have_previous_ = false;
    unsigned stage_ = 0;
    Clock::time_point begin_{}, previous_{}, mark_{};
    FrameSample pending_;
};
// Draw inside an existing host window. GPU time is deliberately not inferred
// from submission time; render_ms is CPU backend/submission/present work.
void DrawDiagnostics(const FrameProfiler &profiler);
} // namespace skui
