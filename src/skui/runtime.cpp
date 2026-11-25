#include "skui/runtime.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace skui {
namespace {
struct State {int frame=-1; FrameRequest request;};
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
    if(cached->frame!=context->FrameCount) {cached->frame=context->FrameCount;cached->request={};}
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
} // namespace skui
