#include "floating_orb.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace gallery {
namespace {
constexpr float pi=3.141592654f;
ImVec2 Add(ImVec2 a,ImVec2 b) {return {a.x+b.x,a.y+b.y};}
ImVec2 Scale(ImVec2 a,float s) {return {a.x*s,a.y*s};}
ImU32 Color(ImVec4 c,float alpha) {c.w*=std::clamp(alpha,0.f,1.f);return ImGui::ColorConvertFloat4ToU32(c);}
float Smooth(float t) {t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
}
void FloatingOrb::Target(bool minimized) {
    minimized_=minimized;from_=openness_;elapsed_=0;
    skui::RequestAnimationFrame();
}
void FloatingOrb::RememberPosition() {
    float r=diameter_*.5f;
    float w=bounds_.size.x-diameter_,h=bounds_.size.y-diameter_;
    if(w>0) normalized_.x=std::clamp((center_.x-bounds_.position.x-r)/w,0.f,1.f);
    if(h>0) normalized_.y=std::clamp((center_.y-bounds_.position.y-r)/h,0.f,1.f);
}
void FloatingOrb::Place() {
    float r=diameter_*.5f;
    center_={bounds_.position.x+r+normalized_.x*std::max(0.f,bounds_.size.x-diameter_),
             bounds_.position.y+r+normalized_.y*std::max(0.f,bounds_.size.y-diameter_)};
}
void FloatingOrb::Minimize(ImVec2 position,ImVec2 size) {
    if(!placed_) {
        center_={position.x+size.x-diameter_*.5f,position.y+diameter_*.5f};
        RememberPosition();Place();placed_=true;
    }
    ImGui::ClearActiveID();
    if(!GImGui->OpenPopupStack.empty()) ImGui::ClosePopupToLevel(0,true);
    Target(true);
}
void FloatingOrb::BeginFrame(skui::ViewportBounds bounds) {
    bool resized=bounds.position.x!=bounds_.position.x || bounds.position.y!=bounds_.position.y ||
        bounds.size.x!=bounds_.size.x || bounds.size.y!=bounds_.size.y;
    bounds_=bounds;
    diameter_=std::max(0.f,std::min({skui::Dp(64),bounds.size.x,bounds.size.y}));
    Place();
    // Old screen-space tails must not appear across the screen after rotation.
    if(resized) {for(auto &t:trail_) t.age=1;impulse_={};}
    float dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.05f);
    elapsed_+=dt;
    float target=minimized_ ? 0.f : 1.f;
    float duration=.36f*std::max(.05f,skui::GetTheme().motion_duration_scale);
    float t=skui::GetTheme().animations ? std::min(elapsed_/duration,1.f) : 1.f;
    float eased=std::clamp(skui::SpringProgress(t,skui::GetTheme().motion_response,
                                            skui::GetTheme().motion_damping),0.f,1.f);
    openness_=t>=1 ? target : from_+(target-from_)*eased;
    if(Transitioning()) skui::RequestAnimationFrame();
}
void FloatingOrb::TransformShell(const char *id) const {
    if(openness_>=1) return;
    auto *root=ImGui::FindWindowByName(id);if(!root) return;
    ImVec2 origin=Add(root->Pos,Scale(root->Size,.5f));
    float p=openness_;
    ImVec2 destination={center_.x+(origin.x-center_.x)*p,center_.y+(origin.y-center_.y)*p};
    float sx=p+(1-p)*diameter_/std::max(1.f,root->Size.x);
    float sy=p+(1-p)*diameter_/std::max(1.f,root->Size.y);
    float alpha=Smooth(p);
    auto transform=[&](ImVec2 v) {return ImVec2(destination.x+(v.x-origin.x)*sx,destination.y+(v.y-origin.y)*sy);};
    for(auto *window:GImGui->Windows) if(window->Active && window->RootWindow==root) {
        // Visual geometry is transformed; layout/scroll and the saved placement stay intact.
        // No hit boxes may remain at the untransformed full-window position.
        window->Flags|=ImGuiWindowFlags_NoMouseInputs;
        for(auto &v:window->DrawList->VtxBuffer) {
            v.pos=transform(v.pos);
            auto a=static_cast<ImU32>(((v.col>>IM_COL32_A_SHIFT)&255)*alpha);
            v.col=(v.col&~IM_COL32_A_MASK)|(a<<IM_COL32_A_SHIFT);
        }
        for(auto &cmd:window->DrawList->CmdBuffer) {
            auto min=transform({cmd.ClipRect.x,cmd.ClipRect.y});
            auto max=transform({cmd.ClipRect.z,cmd.ClipRect.w});
            cmd.ClipRect={min.x,min.y,max.x,max.y};
        }
    }
}
void FloatingOrb::Draw() {
    if((!minimized_ && !Transitioning()) || diameter_<=0) return;
    auto &io=ImGui::GetIO();
    float dt=std::clamp(io.DeltaTime,0.f,.05f);
    bool motion=skui::GetTheme().animations;
    ImVec2 movement{};
    if(minimized_ && !Transitioning() && active_id_ && GImGui->ActiveId==active_id_ &&
       ImGui::IsMouseDown(0) && ImGui::IsMouseDragging(0,skui::Dp(5))) {
        dragged_=true;
        auto before=center_;
        center_=Add(center_,io.MouseDelta);RememberPosition();Place();
        movement={center_.x-before.x,center_.y-before.y};
        if(motion && (movement.x!=0 || movement.y!=0)) {
            trail_[trail_next_]={before,0};trail_next_=(trail_next_+1)%trail_.size();
            impulse_.x=std::clamp(impulse_.x-movement.x/diameter_*2.8f,-1.5f,1.5f);
            impulse_.y=std::clamp(impulse_.y-movement.y/diameter_*2.8f,-1.5f,1.5f);
            for(unsigned i=0;i<particles_.size();++i) {
                auto &p=particles_[i];
                float swirl=(i%2 ? 1.f : -1.f)*.45f;
                p.velocity.x=std::clamp(p.velocity.x+(-movement.x+movement.y*swirl)/diameter_*7,-3.f,3.f);
                p.velocity.y=std::clamp(p.velocity.y+(-movement.y-movement.x*swirl)/diameter_*7,-3.f,3.f);
            }
        }
    }
    float visibility=1-Smooth(openness_);
    float r=diameter_*.5f;
    ImGui::SetNextWindowPos({center_.x-r,center_.y-r});
    ImGui::SetNextWindowSize({diameter_,diameter_});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
               ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollWithMouse;
    if(Transitioning()) flags|=ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##gallery-particle-orb",nullptr,flags);
    ImGui::SetCursorPos({0,0});
    bool pressed=ImGui::InvisibleButton("恢复窗口",{diameter_,diameter_},ImGuiButtonFlags_EnableNav);
    active_id_=ImGui::GetItemID();
    if(ImGui::IsItemActivated()) dragged_=false;
    bool held=ImGui::IsItemActive();
    if(pressed && !dragged_) {Target(false);skui::PerformHaptic(skui::Haptic::Press);}
    ImGui::End();ImGui::PopStyleVar();

    // Foreground geometry adds no input region: only the 64 dp orb owns touch.
    auto *draw=ImGui::GetForegroundDrawList();
    auto accent=skui::GetTheme().accent;
    if(motion) time_+=dt;
    else {impulse_={};for(auto &p:particles_) p={};}
    for(auto &tail:trail_) {
        tail.age=motion ? tail.age+dt : 1;
        if(tail.age>=.55f) continue;
        float fade=1-tail.age/.55f;
        draw->AddCircle(tail.position,r*(.70f+.12f*fade),Color(accent,.13f*fade*fade*visibility),40,skui::Dp(1.4f));
    }
    // Open center: only the rim, travelling arcs and particles cover the background.
    draw->AddCircle(center_,r-skui::Dp(3),Color(accent,.22f*visibility),64,skui::Dp(1));
    float angle=time_*.42f;
    for(int i=0;i<2;++i) {
        float a=angle+i*pi;
        draw->PathArcTo(center_,r-skui::Dp(3),a,a+pi*.65f,28);
        draw->PathStroke(Color(accent,(held ?.9f : .65f)*visibility),0,skui::Dp(1.8f));
    }
    impulse_=Scale(impulse_,std::exp(-dt*4.2f));
    // Analytic wandering targets plus damped inertial offsets. All state is bounded.
    for(unsigned i=0;i<particles_.size();++i) {
        auto &p=particles_[i];
        float seed=i*2.39996323f;
        float orbit=std::sqrt((i+.5f)/particles_.size())*.68f;
        float phase=seed+time_*(.22f+.035f*(i%5));
        ImVec2 home{std::cos(phase)*orbit,std::sin(phase)*orbit};
        home.x+=.055f*std::sin(time_*.8f+seed*2);home.y+=.055f*std::cos(time_*.65f+seed*3);
        if(motion) {
            p.velocity.x+=(impulse_.x*(.7f+.3f*std::sin(seed))-p.offset.x*12-p.velocity.x*5)*dt;
            p.velocity.y+=(impulse_.y*(.7f+.3f*std::cos(seed))-p.offset.y*12-p.velocity.y*5)*dt;
            p.offset=Add(p.offset,Scale(p.velocity,dt));
            float distance=std::sqrt(p.offset.x*p.offset.x+p.offset.y*p.offset.y);
            if(distance>.32f) {p.offset=Scale(p.offset,.32f/distance);p.velocity=Scale(p.velocity,.6f);}
        }
        auto point=Add(home,p.offset);
        float length=std::sqrt(point.x*point.x+point.y*point.y);
        if(length>.78f) point=Scale(point,.78f/length);
        auto at=Add(center_,Scale(point,r));
        float size=skui::Dp(.85f+.3f*(i%3));
        float opacity=(.32f+.16f*(i%4))*visibility;
        draw->AddCircleFilled(at,size*2.5f,Color(accent,opacity*.09f),12);
        draw->AddCircleFilled(at,size,Color(accent,opacity),12);
    }
    if(motion) {
        if(held || Transitioning()) skui::RequestAnimationFrame();
        else skui::RequestFrameAfter(1./30);
    }
}
} // namespace gallery
