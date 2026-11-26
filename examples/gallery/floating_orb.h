#pragma once
#include "skui/skui.h"
#include <array>

namespace gallery {
// Gallery-owned presentation: no native window, worker, texture or persisted state.
// Call BeginFrame even while the shell is visible, then Draw after the shell.
class FloatingOrb {
public:
    void BeginFrame(skui::ViewportBounds bounds);
    void Minimize(ImVec2 shell_position, ImVec2 shell_size);
    void Draw();
    void TransformShell(const char *window_id) const;
    bool ShellVisible() const { return openness_ > 0; }
    bool Minimized() const { return minimized_; }
    bool Transitioning() const { return openness_ != (minimized_ ? 0.f : 1.f); }
    ImVec2 Position() const { return center_; }
    float Diameter() const { return diameter_; }
    float Openness() const { return openness_; }
private:
    struct Particle { ImVec2 offset{}, velocity{}; };
    struct Trail { ImVec2 position{}; float age=1; };
    void Target(bool minimized);
    void Place();
    void RememberPosition();
    skui::ViewportBounds bounds_{};
    ImVec2 center_{}, normalized_{1,.28f};
    ImVec2 impulse_{};
    std::array<Particle,28> particles_{};
    std::array<Trail,20> trail_{};
    unsigned trail_next_=0;
    ImGuiID active_id_=0;
    float diameter_=64, openness_=1, from_=1, elapsed_=1, time_=0;
    bool minimized_=false, placed_=false, dragged_=false;
};
} // namespace gallery
