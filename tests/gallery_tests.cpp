#include "gallery.h"
#include "floating_orb.h"
#include "TouchRegions.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>

#define CHECK(x) do {if(!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}} while(false)
namespace {
struct Host {
    ImGuiContext *context=ImGui::CreateContext();
    Touch::CaptureRegions regions;
    int vertices=0;
    Host() {
        auto &io=ImGui::GetIO();io.IniFilename=io.LogFilename=nullptr;
        io.DisplaySize={800,1000};io.DeltaTime=1.f/60;
        io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures|ImGuiBackendFlags_RendererHasVtxOffset;
        ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;
        ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
        io.Fonts->AddFontDefault();
    }
    ~Host() {
        for(auto *t:ImGui::GetIO().Fonts->TexList) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        ImGui::DestroyContext(context);
    }
    void Frame(const std::function<void()> &draw,int count=1) {
        for(int n=0;n<count;++n) {
            ImGui::NewFrame();draw();regions=Touch::CollectCaptureRegions(ImGui::GetIO().DisplaySize);ImGui::Render();
            vertices=ImGui::GetDrawData()->TotalVtxCount;
            for(auto *list:ImGui::GetDrawData()->CmdLists) for(const auto &v:list->VtxBuffer)
                CHECK(std::isfinite(v.pos.x) && std::isfinite(v.pos.y));
            for(auto *t:ImGui::GetIO().Fonts->TexList) {
                if(t->Status==ImTextureStatus_WantCreate) {t->SetTexID(t->UniqueID+1);t->SetStatus(ImTextureStatus_OK);}
                if(t->Status==ImTextureStatus_WantUpdates) t->SetStatus(ImTextureStatus_OK);
                if(t->Status==ImTextureStatus_WantDestroy) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
            }
        }
    }
    void Down(ImVec2 p,const std::function<void()> &draw) {
        CHECK(regions.Contains(p.x,p.y));
        auto &io=ImGui::GetIO();io.AddMousePosEvent(p.x,p.y);io.AddMouseButtonEvent(0,true);Frame(draw);
    }
    void Up(const std::function<void()> &draw) {ImGui::GetIO().AddMouseButtonEvent(0,false);Frame(draw);}
    void Click(ImVec2 p,const std::function<void()> &draw) {Down(p,draw);Up(draw);Frame(draw,2);}
};
void OrbGestures() {
    Host host;skui::Theme theme;gallery::FloatingOrb orb;
    auto draw=[&] {skui::ApplyTheme(theme);orb.BeginFrame(skui::ResolveViewport(ImGui::GetIO().DisplaySize,{20,40,30,50}));orb.Draw();};
    host.Frame(draw);
    ImGui::SetActiveID(123,ImGui::FindWindowByName("Debug##Default"));
    orb.Minimize({100,100},{500,700});CHECK(GImGui->ActiveId==0);
    host.Frame(draw);CHECK(orb.ShellVisible() && orb.Openness()>0 && orb.Openness()<1);
    host.Frame(draw,40);CHECK(orb.Minimized() && !orb.ShellVisible());
    auto position=orb.Position();CHECK(host.regions.count==1 && host.regions.Contains(position.x,position.y));
    CHECK(!host.regions.Contains(100,600));
    CHECK(!skui::CurrentFrameRequest().continuous && skui::CurrentFrameRequest().wake_after_seconds<=1./30);
    host.Down(position,draw);
    // A real drag must never restore on release.
    ImGui::GetIO().AddMousePosEvent(position.x-120,position.y+160);host.Frame(draw);
    host.Up(draw);host.Frame(draw,2);
    CHECK(orb.Minimized() && !orb.ShellVisible());
    CHECK(orb.Position().x<position.x-100 && orb.Position().y>position.y+140);
    int trailing=host.vertices;
    host.Frame(draw,70);CHECK(host.vertices<trailing && host.vertices<6000);
    ImGui::GetIO().DisplaySize={1000,800};host.Frame(draw,2);
    CHECK(orb.Position().x>=20+orb.Diameter()/2 && orb.Position().x<=970-orb.Diameter()/2);
    CHECK(orb.Position().y>=40+orb.Diameter()/2 && orb.Position().y<=750-orb.Diameter()/2);
    // Reduced motion has no idle wakeups, drag trails or animated state changes.
    theme.animations=false;host.Frame(draw);
    CHECK(!skui::CurrentFrameRequest().continuous && std::isinf(skui::CurrentFrameRequest().wake_after_seconds));
    position=orb.Position();host.Down(position,draw);
    ImGui::GetIO().AddMousePosEvent(position.x+1,position.y+2);host.Frame(draw);host.Up(draw);host.Frame(draw,2);
    CHECK(!orb.Minimized() && orb.Openness()==1); // Sub-threshold touch jitter still restores.
    orb.Minimize({100,100},{500,700});host.Frame(draw,2);CHECK(orb.Openness()==0);
    host.Down(orb.Position(),draw);ImGui::GetIO().AddMousePosEvent(-100,-100);host.Frame(draw);host.Up(draw);
    CHECK(orb.Minimized() && orb.Position().x>=20+orb.Diameter()/2 && orb.Position().y>=40+orb.Diameter()/2);
    host.Frame(draw,100);CHECK(host.vertices<6000);
    std::puts("PASS particle orb: minimize transition, isolated capture, drag vs tap, decaying trail, rotation bounds and reduced motion");
}
void GalleryLifecycle() {
    Host host;bool running=true;
    auto draw=[&] {gallery::Draw(&running,ImGui::GetIO().DisplaySize);};
    host.Frame(draw,45);
    auto *shell=ImGui::FindWindowByName("skui-app");CHECK(shell && shell->Active);
    auto original_position=shell->Pos,original_size=shell->Size;
    // Exercise real ImGui GC: inactive windows lose transient buffers/ID stacks.
    // A long-minimized window must restore without reading an empty ID stack.
    ImGui::GetIO().ConfigMemoryCompactTimer=.1f;
    auto minimize=ImVec2(shell->Pos.x+shell->Size.x-skui::Dp(74),shell->Pos.y+skui::Dp(24));
    host.Click(minimize,draw);host.Frame(draw,40);
    CHECK(running && !shell->Active);
    CHECK(shell->MemoryCompacted && shell->IDStack.empty());
    auto *orb=ImGui::FindWindowByName("##gallery-particle-orb");CHECK(orb && orb->Active);
    CHECK(host.regions.count==1 && !host.regions.Contains(original_position.x+50,original_position.y+original_size.y*.6f));
    CHECK(GImGui->ActiveId==0 && GImGui->OpenPopupStack.empty());
    auto center=ImVec2(orb->Pos.x+orb->Size.x/2,orb->Pos.y+orb->Size.y/2);
    host.Click(center,draw);host.Frame(draw,40);
    CHECK(shell->Active && !orb->Active && running);
    CHECK(shell->Pos.x==original_position.x && shell->Pos.y==original_position.y);
    CHECK(shell->Size.x==original_size.x && shell->Size.y==original_size.y);
    // Header actions must remain independent after restoring the transformed window.
    host.Click(minimize,draw);host.Frame(draw,40);CHECK(!shell->Active && orb->Active);
    ImGui::GetIO().DisplaySize={1000,800};host.Frame(draw,5);
    center={orb->Pos.x+orb->Size.x/2,orb->Pos.y+orb->Size.y/2};
    host.Click(center,draw);host.Frame(draw,40);
    CHECK(shell->Active && shell->Pos.x>=0 && shell->Pos.y>=0);
    CHECK(shell->Pos.x+shell->Size.x<=1000 && shell->Pos.y+shell->Size.y<=800);
    gallery::Shutdown();
    std::puts("PASS Gallery: actual header minimize/restore, full shell unmounted, focus/popups cleared, geometry preserved and rotated restoration");
}
}
int main() {OrbGestures();GalleryLifecycle();}
