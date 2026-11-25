#include "skui/skui.h"
#include "imgui_internal.h"
#include "data_model.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#ifdef SKUI_TEST_NATIVE_WAKE
#include "FrameWake.h"
#include <unistd.h>
#endif
#define CHECK(x) do {if(!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}} while(false)
namespace {
std::size_t live_allocations=0,allocation_calls=0;
void *TrackAllocate(std::size_t size,void *) {++live_allocations;++allocation_calls;return std::malloc(size);}
void TrackFree(void *p,void *) {if(p) {--live_allocations;std::free(p);}}
struct Frame {
    ImGuiContext *context=ImGui::CreateContext();
    skui::Theme theme;
    Frame() {
        ImGui::SetCurrentContext(context);
        auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(800,900);
        io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures|ImGuiBackendFlags_RendererHasVtxOffset;
        ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
        io.Fonts->AddFontDefault();
    }
    ~Frame() {
        ImGui::SetCurrentContext(context);
        for(auto *t:ImGui::GetIO().Fonts->TexList) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        ImGui::DestroyContext(context);
    }
    skui::FrameRequest Draw(const std::function<void()> &draw,float dt=1.f/60) {
        ImGui::SetCurrentContext(context);ImGui::GetIO().DeltaTime=dt;
        ImGui::NewFrame();skui::ApplyTheme(theme);
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(600,800));
        ImGui::Begin("runtime",nullptr,ImGuiWindowFlags_NoTitleBar);
        draw();ImGui::End();ImGui::Render();
        for(auto *t:ImGui::GetIO().Fonts->TexList) {
            if(t->Status==ImTextureStatus_WantCreate) {t->SetTexID(t->UniqueID+1);t->SetStatus(ImTextureStatus_OK);}
            if(t->Status==ImTextureStatus_WantUpdates) t->SetStatus(ImTextureStatus_OK);
            if(t->Status==ImTextureStatus_WantDestroy) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        }
        return skui::CurrentFrameRequest();
    }
};
void RequestCosts() {
    ImGuiMemAllocFunc original_allocate;ImGuiMemFreeFunc original_free;void *original_user;
    ImGui::GetAllocatorFunctions(&original_allocate,&original_free,&original_user);
    ImGui::SetAllocatorFunctions(TrackAllocate,TrackFree);
    for(int iteration=0;iteration<4;++iteration) {
        {
            Frame frame;frame.Draw([]{});
            auto before=allocation_calls;
            for(int i=0;i<10000;++i) {skui::RequestAnimationFrame();skui::RequestFrameAfter(.01);CHECK(skui::CurrentFrameRequest().continuous);}
            CHECK(before==allocation_calls);
        }
        CHECK(live_allocations==0);
    }
    ImGui::SetAllocatorFunctions(original_allocate,original_free,original_user);
    std::puts("PASS frame hint costs: zero steady-state allocations, context destruction/recreation releases owned state");
}
void Requests() {
    CHECK(!skui::CurrentFrameRequest().continuous);
    Frame first;Frame second;
    first.Draw([] {skui::RequestFrameAfter(.05);skui::RequestFrameAfter(.2);skui::RequestFrameAfter(NAN);skui::RequestAnimationFrame();});
    auto request=skui::CurrentFrameRequest();CHECK(request.continuous && request.wake_after_seconds==.05);
    second.Draw([]{});CHECK(!skui::CurrentFrameRequest().continuous);
    ImGui::SetCurrentContext(first.context);CHECK(skui::CurrentFrameRequest().continuous);
    request=first.Draw([]{});CHECK(!request.continuous && std::isinf(request.wake_after_seconds));
    bool expanded=false;float height=0;
    auto reveal=[&]{skui::Collapsible("reveal",expanded,[]{skui::Space(120);});height=ImGui::GetItemRectSize().y;};
    for(int i=0;i<40;++i) first.Draw(reveal);
    expanded=true;
    request=first.Draw(reveal,.1f);CHECK(request.continuous && height>0 && height<10); // Idle time predates the new target.
    request=first.Draw(reveal);CHECK(request.continuous && height>0 && height<10);
    for(int i=0;i<50;++i) request=first.Draw(reveal);
    CHECK(!request.continuous && height>=120);
    expanded=false;request=first.Draw(reveal,.1f);CHECK(request.continuous && height>110 && height<120);
    for(int i=0;i<50;++i) request=first.Draw(reveal);
    CHECK(!request.continuous && height==0);
    request=first.Draw([] {skui::CircularProgress();});CHECK(request.continuous);
    first.theme.animations=false;request=first.Draw([] {skui::CircularProgress();});CHECK(!request.continuous);
    skui::Snackbar snackbar;
    first.Draw([&]{snackbar.Show("saved",1);snackbar.Draw(ImVec2(30,700),300);});
    request=first.Draw([&]{snackbar.Draw(ImVec2(30,700),300);});
    CHECK(!request.continuous && request.wake_after_seconds>0 && request.wake_after_seconds<1);
    std::puts("PASS frame requests: context isolation, next-frame reset, delayed hints, idle-wake reveal continuity, settling and reduced motion");
}
template<class F> void Until(F ready) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready()) {CHECK(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
void Tasks() {
    std::atomic<int> wakes{0};gallery::DataLoader loader;
    loader.SetWakeHandler([](void *p){++*static_cast<std::atomic<int> *>(p);},&wakes);
    CHECK(loader.Status().state==gallery::LoadState::Idle && loader.Start(0)==0 && loader.Start(100001)==0);
    auto old=loader.Start(100000);CHECK(old>0);
    Until([&]{return loader.Status().progress>0;});loader.Cancel();
    CHECK(loader.Status().state==gallery::LoadState::Cancelled);
    gallery::DataResult result;CHECK(!loader.TakeResult(result));
    auto current=loader.Start(1000);CHECK(current>old);
    Until([&]{return loader.TakeResult(result);});CHECK(result.generation==current && result.rows.size()==1000 && result.error.empty());
    CHECK(result.rows.front().id==1 && result.rows.back().id==1000 && !loader.TakeResult(result));
    current=loader.Start(10,true);Until([&]{return loader.TakeResult(result);});
    CHECK(result.generation==current && !result.error.empty() && result.rows.empty() && loader.Status().state==gallery::LoadState::Failed);
    loader.Start(300,true);current=loader.Start(30,false);
    Until([&]{return loader.TakeResult(result);});CHECK(result.generation==current && result.rows.size()==30 && result.error.empty());
    current=loader.Start(10);Until([&]{return loader.Status().state==gallery::LoadState::Ready;});
    loader.Cancel();CHECK(!loader.TakeResult(result)); // Leave after publication but before UI delivery.
    loader.Start(100000);loader.Shutdown();loader.Shutdown();
    auto count=wakes.load();std::this_thread::sleep_for(std::chrono::milliseconds(20));CHECK(wakes==count);
    CHECK(loader.Start(10)==0);loader.SetWakeHandler(nullptr);
    std::puts("PASS async sample: progress, cancel/retry/error, request replacement, stale-result exclusion, bounded input and joined shutdown");
}
void LongData() {
    std::vector<gallery::DataRow> rows;rows.reserve(100000);
    for(int i=1;i<=100000;++i) {char title[80];std::snprintf(title,sizeof(title),"Record %05d · 示例记录",i);rows.push_back({i,title});}
    gallery::DataView view;view.Rebuild(rows,"",false);CHECK(view.Indices().size()==100000);
    auto anchor=view.Anchor(rows,500*58+20,58);CHECK(anchor.id==501 && anchor.offset==20);
    view.Rebuild(rows,"",true);CHECK(rows[view.Indices().front()].id==100000);
    auto restored=view.Restore(rows,anchor,58);auto same=view.Anchor(rows,restored,58);CHECK(same.id==501 && same.offset==20);
    view.Rebuild(rows,"ReCORD 00",false);CHECK(view.Indices().size()==999);
    CHECK(view.Anchor(rows,view.Restore(rows,anchor,58),58).id==501);
    view.Rebuild(rows,"示例",false);CHECK(view.Indices().size()==100000);
    Frame frame;frame.theme.animations=false;int drawn=0;ImGuiID before=0,after=0;
    auto draw=[&] {
        drawn=0;skui::BeginScroll("rows",ImVec2(0,300));
        float height=skui::Dp(50)+ImGui::GetStyle().ItemSpacing.y;
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(view.Indices().size()),height);
        while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            const auto &row=rows[view.Indices()[i]];ImGui::PushID(row.id);
            skui::ListItem("record",row.title.c_str(),nullptr,nullptr,false);++drawn;ImGui::PopID();
        }
        ImGui::PushID(501);before=ImGui::GetID("record");ImGui::PopID();skui::EndScroll();
    };
    frame.Draw(draw);frame.Draw(draw);CHECK(drawn>0 && drawn<16);auto id=before;
    view.Rebuild(rows,"",true);frame.Draw(draw);after=before;CHECK(drawn<16 && after==id);
    view.Rebuild(rows,"missing",false);CHECK(view.Indices().empty() && view.Restore(rows,anchor,58)==0);
    CHECK(view.Anchor(rows,INFINITY,58).id==0 && view.Restore(rows,anchor,NAN)==0);
    std::puts("PASS long data: 100000 rows, UTF-8/filter/sort, stable record IDs, scroll anchor preservation and visible-row-only drawing");
}
#ifdef SKUI_TEST_NATIVE_WAKE
void Waking() {
    FrameWake wake;CHECK(wake.Available() && !wake.Drain() && !wake.Wait(0));
    for(int i=0;i<1000;++i) wake.Notify();
    CHECK(wake.Wait(0) && wake.Drain() && !wake.Drain()); // Signal before wait cannot be lost.
    std::thread producer([&]{std::this_thread::sleep_for(std::chrono::milliseconds(10));wake.Notify();});
    bool ready=false;Until([&]{ready=wake.Wait(.1);return ready;});producer.join();CHECK(ready && wake.Drain());
    int descriptors[2];CHECK(::pipe(descriptors)==0);CHECK(::write(descriptors[1],"x",1)==1);
    CHECK(wake.Wait(.1,descriptors[0]));::close(descriptors[0]);::close(descriptors[1]);
    std::puts("PASS native wake: coalescing, pre-wait signal, producer wake and IME-style descriptor polling");
}
#endif
}
int main() {
    RequestCosts();Requests();Tasks();LongData();
#ifdef SKUI_TEST_NATIVE_WAKE
    Waking();
#endif
}
