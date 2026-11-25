#include "skui/devtools.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace skui {
namespace {
struct Inspection {
    std::array<InspectedItem,128> items{};
    std::array<std::array<char,96>,64> events{};
    std::size_t count=0,rejected=0,event_count=0,event_next=0;
    int frame=-1;
    ImGuiID shutdown=0,new_frame=0;
};
thread_local ImGuiContext *cached_context=nullptr;
thread_local Inspection *cached=nullptr;
void Destroy(ImGuiContext *context,ImGuiContextHook *hook) {
    if(cached_context==context) {cached_context=nullptr;cached=nullptr;}
    IM_DELETE(static_cast<Inspection *>(hook->UserData));
}
void StartFrame(Inspection &state,ImGuiContext &context) {
    if(state.frame==context.FrameCount) return;
    state.frame=context.FrameCount;state.count=state.rejected=0;
    // Bound work even when a backend delivers a large batch. Text content and
    // printable key names are deliberately omitted from the event summaries.
    for(int i=std::max(0,context.InputEventsTrail.Size-64);i<context.InputEventsTrail.Size;++i) {
        const auto &event=context.InputEventsTrail[i];
        auto &text=state.events[state.event_next];
        switch(event.Type) {
        case ImGuiInputEventType_MousePos:
            std::snprintf(text.data(),text.size(),"%d pointer %.0f %.0f",state.frame,event.MousePos.PosX,event.MousePos.PosY);break;
        case ImGuiInputEventType_MouseButton:
            std::snprintf(text.data(),text.size(),"%d button %d %s",state.frame,event.MouseButton.Button,event.MouseButton.Down ? "down" : "up");break;
        case ImGuiInputEventType_MouseWheel:
            std::snprintf(text.data(),text.size(),"%d wheel %.1f %.1f",state.frame,event.MouseWheel.WheelX,event.MouseWheel.WheelY);break;
        case ImGuiInputEventType_Key: {
            auto key=event.Key.Key;
            bool printable=(key>=ImGuiKey_0 && key<=ImGuiKey_Z) || (key>=ImGuiKey_Apostrophe && key<=ImGuiKey_Equal) ||
                           (key>=ImGuiKey_Keypad0 && key<=ImGuiKey_KeypadEqual);
            std::snprintf(text.data(),text.size(),"%d key %s %s",state.frame,printable ? "[text key]" : ImGui::GetKeyName(key),event.Key.Down ? "down" : "up");break;
        }
        case ImGuiInputEventType_Text:
            std::snprintf(text.data(),text.size(),"%d text [content omitted]",state.frame);break;
        case ImGuiInputEventType_Focus:
            std::snprintf(text.data(),text.size(),"%d app focus %s",state.frame,event.AppFocused.Focused ? "in" : "out");break;
        default:continue;
        }
        state.event_next=(state.event_next+1)%state.events.size();state.event_count=std::min(state.event_count+1,state.events.size());
    }
}
void NewFrame(ImGuiContext *context,ImGuiContextHook *hook) {StartFrame(*static_cast<Inspection *>(hook->UserData),*context);}
Inspection *Current() {
    auto *context=ImGui::GetCurrentContext();
    if(!context) return nullptr;
    if(cached_context!=context) {
        cached=nullptr;cached_context=context;
        for(auto &hook:context->Hooks)
            if(hook.Type==ImGuiContextHookType_Shutdown && hook.Callback==Destroy) cached=static_cast<Inspection *>(hook.UserData);
    }
    return cached;
}
}
void SetItemInspectionEnabled(bool enabled) {
    auto *context=ImGui::GetCurrentContext();if(!context) return;
    auto *state=Current();
    if(enabled && !state) {
        state=IM_NEW(Inspection)();cached=state;
        ImGuiContextHook hook;hook.UserData=state;hook.Type=ImGuiContextHookType_Shutdown;hook.Callback=Destroy;
        state->shutdown=ImGui::AddContextHook(context,&hook);
        hook.Type=ImGuiContextHookType_NewFramePost;hook.Callback=NewFrame;
        state->new_frame=ImGui::AddContextHook(context,&hook);
        StartFrame(*state,*context);
    } else if(!enabled && state) {
        ImGui::RemoveContextHook(context,state->new_frame);ImGui::RemoveContextHook(context,state->shutdown);
        cached=nullptr;IM_DELETE(state);
    }
}
bool ItemInspectionEnabled() {return Current()!=nullptr;}
bool InspectLastItem(const char *path) {
    auto *state=Current();if(!state) return false;
    auto &g=*GImGui;
    if(!path || !*path || std::strlen(path)>=InspectedItem::PathCapacity || !g.CurrentWindow || !g.LastItemData.ID) {
        ++state->rejected;return false;
    }
    for(std::size_t i=0;i<state->count;++i) if(std::strcmp(path,state->items[i].path)==0) {
        state->items[i].ambiguous=true;++state->rejected;return false;
    }
    if(state->count==state->items.size()) {++state->rejected;return false;}
    auto &item=state->items[state->count++];item={};
    std::strcpy(item.path,path);item.id=g.LastItemData.ID;item.window=g.CurrentWindow->ID;item.frame=g.FrameCount;
    item.min=ImGui::GetItemRectMin();item.max=ImGui::GetItemRectMax();
    auto hit=g.LastItemData.Rect;hit.ClipWith(g.CurrentWindow->ClipRect);item.hit_min=hit.Min;item.hit_max=hit.Max;
    item.visible=ImGui::IsItemVisible() && hit.GetWidth()>0 && hit.GetHeight()>0;item.disabled=(g.LastItemData.ItemFlags&ImGuiItemFlags_Disabled)!=0;
    item.active=ImGui::IsItemActive();item.focused=ImGui::IsItemFocused();item.hovered=ImGui::IsItemHovered();
    return true;
}
bool FindInspectedItem(const char *path,InspectedItem &result) {
    auto *state=Current();if(!state || !path) return false;
    for(std::size_t i=0;i<state->count;++i) if(std::strcmp(path,state->items[i].path)==0) {
        if(state->items[i].ambiguous) return false;
        result=state->items[i];return true;
    }
    return false;
}
InspectionStatistics GetInspectionStatistics() {
    auto *state=Current();return state ? InspectionStatistics{state->count,state->rejected,state->event_count,state->frame} : InspectionStatistics{};
}
void DrawInteractionInspector() {
    auto *state=Current();if(!state) {ImGui::TextUnformatted("Item inspection is disabled.");return;}
    auto &g=*GImGui;
    ImGui::Text("Frame %d: %zu marked, %zu rejected",state->frame,state->count,state->rejected);
    ImGui::Text("Active %08X  Focus %08X  Hover %08X",g.ActiveId,g.NavId,g.HoveredId);
    ImGui::Text("Popups %d  Keyboard %s  Text input %s",g.OpenPopupStack.Size,g.IO.WantCaptureKeyboard ? "yes" : "no",g.IO.WantTextInput ? "yes" : "no");
    ImGui::PushID(state);
    if(ImGui::BeginChild("items",ImVec2(0,180),ImGuiChildFlags_Borders))
        for(std::size_t i=0;i<state->count;++i) {
            const auto &item=state->items[i];ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s  #%08X%s%s",item.path,item.id,item.disabled ? " disabled" : "",item.ambiguous ? " DUPLICATE" : "");
            if(ImGui::IsItemHovered()) {
                ImGui::GetForegroundDrawList()->AddRect(item.min,item.max,IM_COL32(255,120,30,255),0.f,2.f);
                ImGui::SetTooltip("Window %08X  Rect %.0f %.0f / %.0f %.0f\nvisible %d active %d focus %d",item.window,item.min.x,item.min.y,item.max.x,item.max.y,item.visible,item.active,item.focused);
            }
            ImGui::PopID();
        }
    ImGui::EndChild();
    if(ImGui::TreeNode("Recent input (text omitted)")) {
        for(std::size_t i=0;i<state->event_count;++i)
            ImGui::TextUnformatted(state->events[(state->event_next+state->events.size()-state->event_count+i)%state->events.size()].data());
        ImGui::TreePop();
    }
    ImGui::PopID();
}
} // namespace skui
