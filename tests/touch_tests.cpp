#include "TouchRegions.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>

#define CHECK(x) do {if (!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}} while (false)
namespace {
struct Host {
    ImGuiContext *context = ImGui::CreateContext();
    Touch::CaptureRegions regions;
    bool captured = false;
    float keyboard = 0;
    Host() {
        auto &io = ImGui::GetIO();
        io.IniFilename = io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1000,700);
        io.DeltaTime = 1.f/60;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetPlatformIO().Renderer_TextureMaxWidth = 4096;
        ImGui::GetPlatformIO().Renderer_TextureMaxHeight = 4096;
        io.Fonts->AddFontDefault();
    }
    ~Host() {
        for (auto *texture : ImGui::GetIO().Fonts->TexList) {
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
        }
        ImGui::DestroyContext(context);
    }
    void Frame(const std::function<void()> &draw) {
        ImGui::NewFrame();draw();
        regions = Touch::CollectCaptureRegions(ImGui::GetIO().DisplaySize,keyboard);
        ImGui::Render();
        for (auto *texture : ImGui::GetIO().Fonts->TexList) {
            if (texture->Status == ImTextureStatus_WantCreate) texture->SetTexID(texture->UniqueID+1);
            if (texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates)
                texture->SetStatus(ImTextureStatus_OK);
            if (texture->Status == ImTextureStatus_WantDestroy) {
                texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
    }
    void Click(ImVec2 position, const std::function<void()> &draw) {
        // Same gesture boundary as the evdev reader: decide ownership on down,
        // retain it until release, and feed only captured gestures to ImGui.
        captured = regions.Contains(position.x,position.y);
        CHECK(captured);
        auto &io = ImGui::GetIO();
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(position.x,position.y);
        io.AddMouseButtonEvent(0,true);Frame(draw);
        io.AddMouseButtonEvent(0,false);Frame(draw);Frame(draw);
        captured = false;
    }
};
ImVec2 Center() {
    return ImLerp(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),.5f);
}
void IndependentWindows() {
    Host host;
    int main_clicks=0,monitor_clicks=0;
    bool monitor=true;
    ImVec2 main_button,monitor_button;
    auto draw = [&] {
        ImGui::SetNextWindowPos(ImVec2(50,60));ImGui::SetNextWindowSize(ImVec2(300,360));
        ImGui::Begin("main",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        if (ImGui::Button("Main action")) ++main_clicks;
        main_button=Center();ImGui::End();
        if (monitor) {
            ImGui::SetNextWindowPos(ImVec2(500,100),ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(250,260),ImGuiCond_Once);
            ImGui::Begin("monitor");
            if (ImGui::Button("Monitor action")) ++monitor_clicks;
            monitor_button=Center();ImGui::End();
        }
        ImGui::SetNextWindowPos(ImVec2(820,30));ImGui::SetNextWindowSize(ImVec2(160,100));
        ImGui::Begin("visual only",nullptr,ImGuiWindowFlags_NoMouseInputs);
        ImGui::TextUnformatted("No mouse capture");ImGui::End();
    };
    host.Frame(draw);host.Frame(draw);
    CHECK(!host.regions.Contains(400,150)); // gap, not a bounding-box union
    CHECK(!host.regions.Contains(850,60)); // decorative window
    for (int i=0;i<5;++i) {
        host.Click(main_button,draw);
        CHECK(GImGui->NavWindow && GImGui->NavWindow->RootWindow==ImGui::FindWindowByName("main"));
        CHECK(host.regions.Contains(monitor_button.x,monitor_button.y));
        host.Click(monitor_button,draw);
        CHECK(GImGui->NavWindow && GImGui->NavWindow->RootWindow==ImGui::FindWindowByName("monitor"));
    }
    CHECK(main_clicks==5 && monitor_clicks==5);
    // An unfocused auxiliary window can also be dragged from outside the shell.
    host.Click(main_button,draw);
    auto *window=ImGui::FindWindowByName("monitor");
    ImVec2 title(window->Pos.x+100,window->Pos.y+8),before=window->Pos;
    CHECK(host.regions.Contains(title.x,title.y));
    auto &io=ImGui::GetIO();io.AddMousePosEvent(title.x,title.y);io.AddMouseButtonEvent(0,true);host.Frame(draw);
    io.AddMousePosEvent(title.x+60,title.y+40);host.Frame(draw);host.Frame(draw);
    io.AddMouseButtonEvent(0,false);host.Frame(draw);host.Frame(draw);
    CHECK(window->Pos.x>before.x+40 && window->Pos.y>before.y+20);
    host.Click(main_button,draw);host.Click(monitor_button,draw);
    CHECK(main_clicks==7 && monitor_clicks==6);
    auto old=monitor_button;monitor=false;host.Frame(draw);
    CHECK(!host.regions.Contains(old.x,old.y));
    std::puts("PASS native touch regions: main/monitor refocus, independent drag, close, gaps and NoMouseInputs");
}
void PopupAndKeyboard() {
    Host host;bool open_popup=false;
    auto draw=[&] {
        ImGui::SetNextWindowPos(ImVec2(50,60));ImGui::SetNextWindowSize(ImVec2(300,500));
        ImGui::Begin("popup owner");
        ImGui::TextUnformatted("Content");
        if (open_popup) {ImGui::OpenPopup("choices");open_popup=false;}
        if (ImGui::BeginPopup("choices")) {ImGui::Selectable("One");ImGui::Selectable("Two");ImGui::EndPopup();}
        ImGui::End();
    };
    host.Frame(draw);host.Frame(draw);
    CHECK(!host.regions.Contains(900,500));
    open_popup=true;host.Frame(draw);host.Frame(draw);
    CHECK(host.regions.Contains(900,500));
    host.Click(ImVec2(900,500),draw);
    CHECK(GImGui->OpenPopupStack.empty() && !host.regions.Contains(900,500));
    host.keyboard=400;host.Frame(draw);
    CHECK(host.regions.Contains(100,200) && !host.regions.Contains(100,350));
    open_popup=true;host.Frame(draw);host.Frame(draw);
    CHECK(host.regions.Contains(900,200) && !host.regions.Contains(900,350));
    host.keyboard=700;host.Frame(draw);CHECK(host.regions.count==0);
    std::puts("PASS native touch regions: popup outside dismissal and IME exclusion for windows/popups");
}
void Geometry() {
    Host host;
    auto draw=[] {
        ImGui::SetNextWindowPos(ImVec2(-100,80));ImGui::SetNextWindowSize(ImVec2(300,300));
        ImGui::Begin("partly offscreen");ImGui::TextUnformatted("visible");ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(500,70));ImGui::SetNextWindowSize(ImVec2(200,300));
        ImGui::SetNextWindowCollapsed(true);
        ImGui::Begin("collapsed");ImGui::TextUnformatted("hidden");ImGui::End();
    };
    host.Frame(draw);host.Frame(draw);
    CHECK(host.regions.Contains(10,100) && !host.regions.Contains(-1,100));
    CHECK(host.regions.Contains(550,75) && !host.regions.Contains(550,200));
    CHECK(Touch::CollectCaptureRegions(ImVec2(0,700)).count==0);
    CHECK(Touch::CollectCaptureRegions(ImVec2(NAN,700)).count==0);
    Touch::CaptureRegions bounded;
    for (int i=0;i<200;++i) bounded.Add(ImVec2(i*2.f,0),ImVec2(i*2.f+1,10));
    CHECK(bounded.count==Touch::CaptureRegions::Capacity);
    CHECK(!bounded.Contains(300,5));
    Touch::CaptureRegions invalid;
    invalid.Add(ImVec2(0,0),ImVec2(INFINITY,10));
    invalid.Add(ImVec2(0,0),ImVec2(-1,10));
    CHECK(invalid.count==0);
    std::puts("PASS native touch regions: clipped/collapsed bounds, invalid geometry and fixed capacity");
}
}
int main() {IndependentWindows();PopupAndKeyboard();Geometry();}
