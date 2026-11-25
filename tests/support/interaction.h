#pragma once
#include "skui/devtools.h"
#include <cstdio>
#include <cstdlib>
#include <functional>

// Small application-owned driver, not a dependency on ImGui Test Engine.
// The supplied frame callback performs a complete NewFrame/draw/Render cycle.
namespace skui_test {
class Interaction {
    std::function<void()> frame_;
    [[noreturn]] void Fail(const char *path,const char *reason) {
        auto stats=skui::GetInspectionStatistics();
        std::fprintf(stderr,"Interaction '%s': %s (frame %d, marked %zu, rejected %zu, recent input %zu)\n",
                     path,reason,stats.frame,stats.count,stats.rejected,stats.input_events);std::abort();
    }
  public:
    explicit Interaction(std::function<void()> frame):frame_(std::move(frame)) {}
    skui::InspectedItem Find(const char *path) {
        skui::InspectedItem item;
        if(!skui::FindInspectedItem(path,item)) Fail(path,"missing or ambiguous path");
        if(item.frame!=ImGui::GetFrameCount()) Fail(path,"stale frame");
        if(!item.visible || item.disabled) Fail(path,"not interactable");
        return item;
    }
    void Click(const char *path) {
        auto item=Find(path);
        auto &io=ImGui::GetIO();
        io.AddMousePosEvent((item.hit_min.x+item.hit_max.x)*.5f,(item.hit_min.y+item.hit_max.y)*.5f);
        frame_();frame_(); // Allow the host's hovered window to settle.
        item=Find(path);
        io.AddMousePosEvent((item.hit_min.x+item.hit_max.x)*.5f,(item.hit_min.y+item.hit_max.y)*.5f);
        io.AddMouseButtonEvent(0,true);frame_();
        auto held=Find(path);
        if(held.id!=item.id) {io.AddMouseButtonEvent(0,false);frame_();Fail(path,"ID changed during press");}
        io.AddMouseButtonEvent(0,false);frame_();frame_();
    }
    void Key(ImGuiKey key) {auto &io=ImGui::GetIO();io.AddKeyEvent(key,true);frame_();io.AddKeyEvent(key,false);frame_();}
    void WaitUntil(const char *description,const std::function<bool()> &condition,int frames=120) {
        for(int i=0;i<frames;++i) {if(condition()) return;frame_();}
        Fail(description,"frame deadline exceeded");
    }
};
}
