#include "skui/runtime.h"
#include "skui/devtools.h"
#include "detail.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace skui {
namespace {
struct State {
    int frame=-1;
    FrameRequest request;
    AnimationPlayback playback;
    float delta=0,step=0;
    double time=0;
    bool clock_started=false;
};
thread_local ImGuiContext *cached_context = nullptr;
thread_local State *cached = nullptr;
void DestroyState(ImGuiContext *context,ImGuiContextHook *hook) {
    if(cached_context==context) {cached_context=nullptr;cached=nullptr;}
    IM_DELETE(static_cast<State *>(hook->UserData));hook->UserData=nullptr;
}
State *SelectFrame() {
    auto *context=ImGui::GetCurrentContext();
    if(!context) return nullptr;
    if(cached_context!=context) {
        cached=nullptr;
        for(auto &hook:context->Hooks)
            if(hook.Type==ImGuiContextHookType_Shutdown && hook.Callback==DestroyState)
                cached=static_cast<State *>(hook.UserData);
        if(!cached) {
            cached=IM_NEW(State)();
            ImGuiContextHook hook;hook.Type=ImGuiContextHookType_Shutdown;
            hook.Callback=DestroyState;hook.UserData=cached;
            ImGui::AddContextHook(context,&hook);
        }
        cached_context=context;
    }
    if(cached->frame!=context->FrameCount) {
        cached->frame=context->FrameCount;cached->request={};
        float raw=context->IO.DeltaTime;
        cached->delta=cached->playback.paused ? cached->step : raw*cached->playback.speed;
        if(!cached->clock_started) {cached->time=context->Time-raw;cached->clock_started=true;}
        cached->time+=cached->delta;cached->step=0;
    }
    return cached;
}
}
void RequestAnimationFrame() {
    if(auto *state=SelectFrame()) state->request.continuous=true;
}
void RequestFrameAfter(double seconds) {
    if(auto *state=SelectFrame(); state && std::isfinite(seconds))
        state->request.wake_after_seconds=std::min(state->request.wake_after_seconds,std::max(0.0,seconds));
}
FrameRequest CurrentFrameRequest() {auto *state=SelectFrame(); return state ? state->request : FrameRequest{};}
void SetAnimationPlayback(AnimationPlayback playback) {
    playback.speed=std::isfinite(playback.speed) ? std::clamp(playback.speed,.1f,4.f) : 1.f;
    if(auto *state=SelectFrame()) state->playback=playback;
}
AnimationPlayback GetAnimationPlayback() {auto *state=SelectFrame();return state ? state->playback : AnimationPlayback{};}
void StepAnimations(float seconds) {
    if(auto *state=SelectFrame();state && state->playback.paused && std::isfinite(seconds) && seconds>0)
        state->step=std::min(.5f,state->step+seconds);
}
namespace detail {
float MotionDeltaTime() {auto *state=SelectFrame();return state ? state->delta : 0;}
double MotionTime() {auto *state=SelectFrame();return state ? state->time : 0;}
void RequestMotionFrame() {
    if(auto *state=SelectFrame();state && !state->playback.paused) state->request.continuous=true;
}
}
} // namespace skui
