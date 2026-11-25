#include "skui/diagnostics.h"
#include "skui/skui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace skui {
void FrameProfiler::SetEnabled(bool value) {
    if (enabled_ != value) { Clear(); enabled_ = value; }
}
void FrameProfiler::Clear() {
    next_ = size_ = 0; in_frame_ = have_previous_ = false; stage_ = 0;
}
void FrameProfiler::BeginFrame() {
    if (!enabled_) return;
    auto now = Clock::now();
    pending_ = {};
    if (have_previous_)
        pending_.interval_ms = std::chrono::duration<double,std::milli>(now-previous_).count();
    begin_ = mark_ = previous_ = now;
    have_previous_ = in_frame_ = true; stage_ = 0;
}
void FrameProfiler::EndInput() {
    if (!enabled_ || !in_frame_ || stage_ != 0) return;
    auto now = Clock::now();
    pending_.input_ms = std::chrono::duration<double,std::milli>(now-mark_).count();
    mark_ = now; stage_ = 1;
}
void FrameProfiler::EndBuild() {
    if (!enabled_ || !in_frame_ || stage_ > 1) return;
    if (stage_ == 0) EndInput();
    auto now = Clock::now();
    pending_.build_ms = std::chrono::duration<double,std::milli>(now-mark_).count();
    mark_ = now; stage_ = 2;
}
void FrameProfiler::AddEffectTime(double milliseconds) {
    if (enabled_ && in_frame_ && std::isfinite(milliseconds) && milliseconds >= 0)
        pending_.effects_ms += milliseconds;
}
void FrameProfiler::EndFrame() {
    if (!enabled_ || !in_frame_) return;
    if (stage_ < 2) EndBuild();
    auto now = Clock::now();
    pending_.render_ms = std::chrono::duration<double,std::milli>(now-mark_).count();
    pending_.cpu_ms = std::chrono::duration<double,std::milli>(now-begin_).count();
    if (auto *g = ImGui::GetCurrentContext()) {
        pending_.vertices = g->IO.MetricsRenderVertices;
        pending_.indices = g->IO.MetricsRenderIndices;
        pending_.textures = g->IO.Fonts->TexList.Size;
        pending_.popups = g->OpenPopupStack.Size;
        pending_.active = g->ActiveId;
        pending_.focus = g->NavId;
    }
    Record(pending_); in_frame_ = false;
}
bool FrameProfiler::Record(const FrameSample &sample) {
    if (!enabled_) return false;
    for (auto v : {sample.interval_ms,sample.input_ms,sample.build_ms,sample.render_ms,sample.cpu_ms,sample.effects_ms})
        if (!std::isfinite(v) || v < 0) return false;
    if (sample.vertices < 0 || sample.indices < 0 || sample.textures < 0 || sample.popups < 0) return false;
    samples_[next_] = sample;
    next_ = (next_+1)%Capacity; size_ = std::min(size_+1,Capacity);
    return true;
}
FrameStatistics FrameProfiler::Statistics() const {
    FrameStatistics result;
    result.samples = size_;
    if (!size_) return result;
    result.latest = samples_[(next_+Capacity-1)%Capacity];
    std::array<double,Capacity> cpu{}, intervals{};
    std::size_t interval_count = 0;
    for (std::size_t i=0; i<size_; ++i) {
        cpu[i] = samples_[i].cpu_ms;
        // The first sample has no preceding frame. A zero interval from an
        // externally clocked host is similarly excluded from frame cadence.
        if (samples_[i].interval_ms > 0) intervals[interval_count++] = samples_[i].interval_ms;
    }
    auto percentiles = [](auto &values, std::size_t count, double &p50, double &p95) {
        if (!count) return;
        std::sort(values.begin(),values.begin()+count);
        p50=values[static_cast<std::size_t>(std::ceil(.50*count))-1];
        p95=values[static_cast<std::size_t>(std::ceil(.95*count))-1];
    };
    percentiles(cpu,size_,result.cpu_p50_ms,result.cpu_p95_ms);
    percentiles(intervals,interval_count,result.interval_p50_ms,result.interval_p95_ms);
    return result;
}
void DrawDiagnostics(const FrameProfiler &profiler) {
    // Cache on the caller's window storage, not global state or a pointer-keyed
    // map. Recompute at 4 Hz so opening the panel doesn't add per-frame sorting.
    ImGui::PushID(&profiler);
    auto *state=ImGui::GetStateStorage();
    auto stamp=ImGui::GetID("stamp"), cpu50=ImGui::GetID("cpu50"),cpu95=ImGui::GetID("cpu95");
    auto int50=ImGui::GetID("int50"),int95=ImGui::GetID("int95");
    if (ImGui::GetTime()-state->GetFloat(stamp,-1) >= .25 || profiler.SampleCount()<2 || !profiler.Enabled()) {
        auto s=profiler.Statistics();
        state->SetFloat(cpu50,static_cast<float>(s.cpu_p50_ms)); state->SetFloat(cpu95,static_cast<float>(s.cpu_p95_ms));
        state->SetFloat(int50,static_cast<float>(s.interval_p50_ms)); state->SetFloat(int95,static_cast<float>(s.interval_p95_ms));
        state->SetFloat(stamp,static_cast<float>(ImGui::GetTime()));
    }
    auto latest=profiler.Latest();
    ImGui::Text("Samples: %zu / %zu",profiler.SampleCount(),FrameProfiler::Capacity);
    ImGui::Text("CPU p50 / p95: %.3f / %.3f ms",state->GetFloat(cpu50),state->GetFloat(cpu95));
    ImGui::Text("Frame interval p50 / p95: %.3f / %.3f ms",state->GetFloat(int50),state->GetFloat(int95));
    ImGui::Text("Input / UI / render: %.3f / %.3f / %.3f ms",latest.input_ms,latest.build_ms,latest.render_ms);
    ImGui::Text("Effects (within UI): %.3f ms",latest.effects_ms);
    ImGui::Text("Vertices / indices: %d / %d",latest.vertices,latest.indices);
    ImGui::Text("Atlas/material textures: %d  Popups: %d",latest.textures,latest.popups);
    ImGui::Text("Active: %08X  Focus: %08X",latest.active,latest.focus);
    Caption("Render time is CPU submission/present time. GPU timestamps are not available here.");
    ImGui::PopID();
}
} // namespace skui
