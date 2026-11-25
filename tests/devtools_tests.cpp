#include "skui/skui.h"
#include "skui/devtools.h"
#include "imgui_internal.h"
#include "../src/skui/detail.h"
#include "support/interaction.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <functional>

#define CHECK(x) do {if(!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}} while(false)
namespace {
struct Host {
    ImGuiContext *context=ImGui::CreateContext();
    float width=700;
    Host() {
        auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1000,1000);
        io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures|ImGuiBackendFlags_RendererHasVtxOffset;
        ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
        io.Fonts->AddFontDefault();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    }
    ~Host() {
        for(auto *t:ImGui::GetIO().Fonts->TexList) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        ImGui::DestroyContext(context);
    }
    void Frame(const std::function<void()> &draw) {
        ImGui::SetCurrentContext(context);auto &io=ImGui::GetIO();io.DeltaTime=1.f/60;
        ImGui::NewFrame();skui::ApplyTheme({});
        ImGui::SetNextWindowPos(ImVec2(20,20));ImGui::SetNextWindowSize(ImVec2(width,900));
        ImGui::Begin("devtools",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        draw();ImGui::End();ImGui::Render();
        for(auto *t:io.Fonts->TexList) {
            if(t->Status==ImTextureStatus_WantCreate) t->SetTexID(t->UniqueID+1);
            if(t->Status==ImTextureStatus_WantCreate || t->Status==ImTextureStatus_WantUpdates) t->SetStatus(ImTextureStatus_OK);
            if(t->Status==ImTextureStatus_WantDestroy) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        }
    }
};
bool Near(double a,double b) {return std::abs(a-b)<.00001;}
void Clock() {
    Host host;float value=0,dt=0;double time=0;
    bool target=false;
    auto draw=[&] {dt=skui::detail::MotionDeltaTime();time=skui::detail::MotionTime();
        value=skui::detail::Animate("probe",target ? 1.f : 0.f,1.f);};
    host.Frame(draw);CHECK(Near(dt,1./60));CHECK(value==0);
    target=true;host.Frame(draw);CHECK(value>0 && value<1);float before=value;
    skui::SetAnimationPlayback({.25f,true});host.Frame(draw);double frozen=time;
    for(int i=0;i<12;++i) {host.Frame(draw);CHECK(Near(value,before));CHECK(Near(time,frozen));CHECK(dt==0);CHECK(!skui::CurrentFrameRequest().continuous);}
    skui::StepAnimations();host.Frame(draw);CHECK(value>before && Near(time-frozen,1./60));before=value;
    host.Frame(draw);CHECK(Near(value,before) && dt==0);
    skui::SetAnimationPlayback({.25f,false});host.Frame(draw);CHECK(Near(dt,1./240));CHECK(skui::CurrentFrameRequest().continuous);
    skui::SetAnimationPlayback({std::numeric_limits<float>::quiet_NaN(),false});CHECK(skui::GetAnimationPlayback().speed==1);
    skui::SetAnimationPlayback({100,true});CHECK(skui::GetAnimationPlayback().speed==4);
    skui::StepAnimations(-1);host.Frame(draw);CHECK(dt==0);
    skui::StepAnimations(10);host.Frame(draw);CHECK(dt==.5f);
    // IO timing stays real and control state belongs to the current context.
    CHECK(Near(ImGui::GetIO().DeltaTime,1./60));
    auto *other=ImGui::CreateContext();ImGui::SetCurrentContext(other);CHECK(!skui::GetAnimationPlayback().paused);CHECK(skui::GetAnimationPlayback().speed==1);
    ImGui::DestroyContext(other);ImGui::SetCurrentContext(host.context);CHECK(skui::GetAnimationPlayback().paused);
    std::puts("PASS animation: default/slow/pause/step, bounded inputs, context isolation, IO and idle hints");
}
void Registry() {
    Host host;skui::InspectedItem item;
    CHECK(!skui::ItemInspectionEnabled());
    host.Frame([&] {skui::Button("ignored");CHECK(!skui::InspectLastItem("ignored"));});
    skui::SetItemInspectionEnabled(true);
    host.Frame([&] {
        ImGui::BeginDisabled();skui::Button("disabled");CHECK(skui::InspectLastItem("controls/disabled"));ImGui::EndDisabled();
        skui::Button("duplicate1");CHECK(skui::InspectLastItem("duplicate"));
        skui::Button("duplicate2");CHECK(!skui::InspectLastItem("duplicate"));
        CHECK(!skui::InspectLastItem(nullptr));
        char long_path[100];for(auto &c:long_path) c='x';long_path[99]=0;CHECK(!skui::InspectLastItem(long_path));
    });
    CHECK(skui::FindInspectedItem("controls/disabled",item) && item.disabled && item.visible);
    CHECK(!skui::FindInspectedItem("duplicate",item));CHECK(skui::GetInspectionStatistics().rejected==3);
    host.Frame([&] {for(int i=0;i<150;++i) {char path[32];std::snprintf(path,sizeof(path),"item/%d",i);ImGui::PushID(i);skui::Button("record");skui::InspectLastItem(path);ImGui::PopID();}});
    CHECK(skui::GetInspectionStatistics().count==128 && skui::GetInspectionStatistics().rejected==22);
    CHECK(skui::FindInspectedItem("item/127",item) && !item.visible);
    for(int i=0;i<100;++i) {ImGui::GetIO().AddMousePosEvent(30.f+i,45);host.Frame([]{});}
    CHECK(skui::GetInspectionStatistics().input_events==64);CHECK(!skui::FindInspectedItem("item/0",item));
    for(int i=0;i<3;++i) host.Frame([&] {
        skui::Button("inspector target");CHECK(skui::InspectLastItem("inspector/target"));
        skui::DrawInteractionInspector();
    });
    CHECK(skui::GetInspectionStatistics().count==1);
    auto *other=ImGui::CreateContext();ImGui::SetCurrentContext(other);
    CHECK(!skui::ItemInspectionEnabled());skui::SetItemInspectionEnabled(true);
    ImGui::DestroyContext(other);ImGui::SetCurrentContext(host.context);CHECK(skui::ItemInspectionEnabled());
    skui::SetItemInspectionEnabled(false);CHECK(!skui::ItemInspectionEnabled());CHECK(skui::GetInspectionStatistics().count==0);
    skui::SetItemInspectionEnabled(true);CHECK(skui::GetInspectionStatistics().input_events<=1);
    std::puts("PASS inspection: disabled/default, duplicates, bounds, clipped state, input ring, release/re-enable");
}
void Allocations() {
    Host host;ImGuiMemAllocFunc original_alloc;ImGuiMemFreeFunc original_free;void *user;
    ImGui::GetAllocatorFunctions(&original_alloc,&original_free,&user);
    struct Counter {ImGuiMemAllocFunc alloc;ImGuiMemFreeFunc free;void *user;bool measuring=false;int allocations=0;} counter{original_alloc,original_free,user};
    ImGui::SetAllocatorFunctions([](size_t size,void *data)->void * {auto &c=*static_cast<Counter *>(data);if(c.measuring) ++c.allocations;return c.alloc(size,c.user);},
        [](void *pointer,void *data) {auto &c=*static_cast<Counter *>(data);c.free(pointer,c.user);},&counter);
    for(bool enabled:{false,true}) {
        skui::SetItemInspectionEnabled(enabled);
        host.Frame([&] {for(int i=0;i<100;++i) {
            char path[32];std::snprintf(path,sizeof(path),"allocation/%d",i);ImGui::PushID(i);skui::Button("value");
            counter.measuring=true;skui::InspectLastItem(path);counter.measuring=false;ImGui::PopID();
        }});
        CHECK(counter.allocations==0);
    }
    skui::SetItemInspectionEnabled(false);ImGui::SetAllocatorFunctions(original_alloc,original_free,user);
    std::puts("PASS inspection allocation: disabled and 100 marked items allocate zero per-item storage");
}
void Semantic() {
    Host host;skui::SetItemInspectionEnabled(true);bool checked=false,translated=false,confirmed=false;
    skui::Router router;skui::RouterView view;
    router.Register("/",[&](const skui::Route &) {
        skui::Toggle(translated ? "Translated notification preference" : "Notifications",&checked);
        CHECK(skui::InspectLastItem("home/notifications"));
        if(skui::Button("Open confirmation")) skui::OpenDialog("confirmation");
        CHECK(skui::InspectLastItem("home/open"));
        if(skui::Button("Details")) router.Push("/detail");
        CHECK(skui::InspectLastItem("home/details"));
        if(skui::BeginDialog("confirmation","Confirm")) {
            if(skui::Button("Confirm")) {confirmed=true;skui::CloseDialog();}
            CHECK(skui::InspectLastItem("confirmation/confirm"));skui::EndDialog();
        }
    });
    router.Register("/detail",[&](const skui::Route &) {if(skui::Button("Back")) router.Back();skui::InspectLastItem("detail/back");});
    router.Reset("/");
    auto frame=[&] {host.Frame([&] {view.Draw(router,ImVec2(0,0));});};
    skui_test::Interaction ui(frame);for(int i=0;i<4;++i) frame();
    ui.Click("home/notifications");CHECK(checked);
    host.width=490;translated=true;frame();frame();ui.Click("home/notifications");CHECK(!checked);
    ui.Click("home/open");ui.WaitUntil("dialog enter",[&] {skui::InspectedItem i;return skui::FindInspectedItem("confirmation/confirm",i) && i.visible;});
    skui::InspectedItem confirm;CHECK(skui::FindInspectedItem("confirmation/confirm",confirm));
    auto *dialog=ImGui::FindWindowByID(confirm.window);CHECK(dialog);
    auto opacity=ImHashStr("skui/overlay/opacity");
    float alpha=dialog->StateStorage.GetFloat(opacity);
    skui::SetAnimationPlayback({1,true});
    for(int i=0;i<8;++i) {frame();CHECK(Near(dialog->StateStorage.GetFloat(opacity),alpha));}
    skui::StepAnimations(.15f);frame();CHECK(dialog->StateStorage.GetFloat(opacity)>alpha);
    skui::SetAnimationPlayback({});
    // Modal entry remains interactive by design; closure retires its input.
    ui.Click("confirmation/confirm");CHECK(confirmed);
    ui.WaitUntil("dialog exit",[] {return GImGui->OpenPopupStack.empty();});
    ui.Click("home/details");CHECK(view.IsTransitioning());skui::SetAnimationPlayback({1,true});
    for(int i=0;i<12;++i) {frame();CHECK(view.IsTransitioning());CHECK(!skui::CurrentFrameRequest().continuous);}
    skui::StepAnimations(.5f);frame();CHECK(!view.IsTransitioning());skui::SetAnimationPlayback({});
    ui.WaitUntil("route enter",[&] {return !view.IsTransitioning();});
    CHECK(router.Current()->path=="/detail");ui.Click("detail/back");
    ui.WaitUntil("route back",[&] {return !view.IsTransitioning();});CHECK(router.Current()->path=="/");
    std::puts("PASS semantic interaction: stable paths across labels/resize, toggle/dialog/animated route and return");
}
}
int main() {Clock();Registry();Allocations();Semantic();}
