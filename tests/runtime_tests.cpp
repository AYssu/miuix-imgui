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
    ImVec2 window_size{600,800};
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
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(window_size);
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
void Viewports() {
    auto bounds=skui::ResolveViewport(ImVec2(1080,2400),{20,80,30,100},600);
    CHECK(bounds.position.x==20 && bounds.position.y==80 && bounds.size.x==1030 && bounds.size.y==1720);
    auto fitted=skui::FitWindow({ImVec2(-400,2000),ImVec2(2000,3000)},bounds,ImVec2(330,420));
    CHECK(fitted.position.x==20 && fitted.position.y==80 && fitted.size.x==1030 && fitted.size.y==1720);
    bounds=skui::ResolveViewport(ImVec2(100,60),{INFINITY,-5,500,500},NAN);
    CHECK(bounds.position.x==0 && bounds.position.y==0 && bounds.size.x==0 && bounds.size.y==0);
    fitted=skui::FitWindow({ImVec2(NAN,INFINITY),ImVec2(NAN,-5)},bounds,ImVec2(330,420));
    CHECK(fitted.position.x==0 && fitted.position.y==0 && fitted.size.x==0 && fitted.size.y==0);
    Frame frame;frame.theme.animations=false;
    skui::ShellOptions shell;shell.id="bounded-shell";shell.safe_insets={20,40,30,50};
    shell.initial_position=ImVec2(100,100);shell.initial_size=ImVec2(600,700);
    ImVec2 position,size;
    auto draw=[&]{skui::BeginShell(shell);position=skui::ShellPosition();size=skui::ShellSize();skui::EndShell();};
    frame.Draw(draw);CHECK(position.x==100 && position.y==100 && size.x==600 && size.y==700);
    shell.visible_bottom=500;frame.Draw(draw);
    CHECK(position.y>=40 && position.y+size.y<=492 && size.y>=420);
    // Background and geometry agree in the very first constrained frame.
    auto *window=ImGui::FindWindowByName(shell.id);CHECK(window->Pos.y==position.y && window->Size.y==size.y);
    shell.visible_bottom=0;frame.Draw(draw);CHECK(position.y==100 && size.y==700 && size.x==600);
    ImGui::GetIO().DisplaySize=ImVec2(450,800);frame.Draw(draw);
    CHECK(position.x>=20 && position.x+size.x<=420 && position.y>=40 && position.y+size.y<=750);
    ImGui::GetIO().DisplaySize=ImVec2(900,450);frame.Draw(draw);
    CHECK(position.x>=20 && position.x+size.x<=870 && position.y>=40 && position.y+size.y<=400);
    shell.id="initial-keyboard-shell";shell.visible_bottom=500;ImGui::GetIO().DisplaySize=ImVec2(800,900);
    frame.Draw(draw);CHECK(position.y+size.y<=492);
    shell.visible_bottom=0;frame.Draw(draw);CHECK(position.x==100 && position.y==100 && size.x==600 && size.y==700);
    CHECK(skui::ClassifyWidth(599)==skui::WidthClass::Compact && skui::ClassifyWidth(600)==skui::WidthClass::Medium && skui::ClassifyWidth(840)==skui::WidthClass::Expanded);
    CHECK(skui::AdaptiveColumns(800,260,3)==3 && skui::AdaptiveColumns(500,260,3)==1);
    frame.theme.tokens.text_scale=1.5f;frame.Draw([]{});CHECK(skui::AdaptiveColumns(800,260,3)==2);
    CHECK(skui::AdaptiveColumns(INFINITY)==1 && skui::AdaptiveColumns(800,NAN)==1);
    std::puts("PASS viewport layout: insets/keyboard overlap, finite bounds, first-frame keyboard restore, portrait/landscape, width classes and text-aware columns");
}
void AdaptiveRows() {
    Frame frame;frame.theme.animations=false;frame.theme.tokens.text_scale=1.5f;
    const char *title="Synchronize automatically when a trusted connection becomes available";
    const char *description="Background changes are saved after confirmation. A longer translated description must wrap and grow the row without overlapping the next action.";
    bool enabled=true;int selected=0;
    for(float width:{270.f,360.f,600.f}) {
        frame.window_size=ImVec2(width,800);
        auto draw=[&] {
            ImGui::PushFont(nullptr,skui::TextSize(16));
            auto check=[&](auto item) {
                float top=ImGui::GetCursorScreenPos().y;
                auto *draw=ImGui::GetWindowDrawList();int first=draw->VtxBuffer.Size;
                item();auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
                CHECK(min.y>=top && max.y>min.y && max.x<=width+1);
                CHECK(ImGui::GetCursorScreenPos().y>=max.y);
                for(int i=first;i<draw->VtxBuffer.Size;++i) {
                    CHECK(draw->VtxBuffer[i].pos.y>=min.y-1.5f && draw->VtxBuffer[i].pos.y<=max.y+1.5f);
                }
                return max.y-min.y;
            };
            float toggle=check([&]{skui::Toggle(title,&enabled,description);});
            CHECK(toggle>skui::Dp(66));
            float choice=check([&]{skui::Checkbox(title,&enabled);});CHECK(choice>=skui::Dp(48));
            check([&]{skui::Radio(title,&selected,0);});
            check([&]{skui::ListItem("long-list",title,description,"connected",true);});
            check([&]{skui::BasicComponent("long-preference",title,description,skui::Icon::Settings,"connected",true);});
            ImGui::PopFont();
        };
        frame.Draw(draw);frame.Draw(draw);
    }
    frame.Draw([&] {
        auto start=ImGui::GetCursorScreenPos();
        skui::TopAppBar("long-header",title,description,true);
        CHECK(ImGui::GetItemRectSize().y>skui::Dp(64));
        CHECK(ImGui::GetCursorScreenPos().y>start.y+ImGui::GetItemRectSize().y);
    });
    frame.window_size=ImVec2(1100,800);ImGui::GetIO().DisplaySize=ImVec2(1400,1000);
    int cells=0;std::vector<ImVec2> positions;
    auto grid=[&] {
        cells=0;positions.clear();
        if(skui::BeginAdaptiveGrid("grid",240,3)) {
            for(int i=0;i<5;++i) {skui::NextAdaptiveCell();positions.push_back(ImGui::GetCursorScreenPos());ImGui::Text("Cell %d",i);++cells;}
            skui::EndAdaptiveGrid();
        }
    };
    frame.Draw(grid);frame.Draw(grid);
    CHECK(cells==5 && positions[1].x>positions[0].x && positions[2].y>positions[0].y);
    std::puts("PASS adaptive content: narrow/large text, wrapped preference/choice rows, following controls and automatic table row breaks");
}
void NestedInputVisibility() {
    Frame frame;frame.theme.animations=false;
    int active_callbacks=0,mounted=0;char text[64]="";ImVec2 min,max;
    skui::SetTextInputHandler([](skui::TextInputEvent &event,void *data) {if(event.active) ++*static_cast<int *>(data);},&active_callbacks);
    auto draw=[&] {
        skui::Space(450);
        if(skui::BeginCard("input-card")) {
            ++mounted;skui::InputText("Name",text,sizeof(text));min=ImGui::GetItemRectMin();max=ImGui::GetItemRectMax();
        }
        skui::EndCard();skui::Space(80);
    };
    frame.Draw(draw);frame.Draw(draw);
    auto &io=ImGui::GetIO();io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMousePosEvent((min.x+max.x)*.5f,(min.y+max.y)*.5f);frame.Draw(draw);
    io.AddMouseButtonEvent(0,true);frame.Draw(draw);io.AddMouseButtonEvent(0,false);frame.Draw(draw);
    CHECK(active_callbacks>0);auto active=GImGui->ActiveId;CHECK(active!=0);
    frame.window_size.y=380;
    for(int i=0;i<10;++i) {
        auto old_mounted=mounted,old_active=active_callbacks;frame.Draw(draw);
        CHECK(mounted==old_mounted+1 && active_callbacks==old_active+1 && GImGui->ActiveId==active);
    }
    auto *window=ImGui::FindWindowByName("runtime");
    CHECK(max.y<=window->InnerRect.Max.y+1 && window->Scroll.y>0);
    skui::SetTextInputHandler(nullptr);
    std::puts("PASS nested input: active card survives viewport shrink and scrolls its field into view without ending the edit session");
}
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
    RequestCosts();Requests();Viewports();AdaptiveRows();NestedInputVisibility();Tasks();LongData();
#ifdef SKUI_TEST_NATIVE_WAKE
    Waking();
#endif
}
