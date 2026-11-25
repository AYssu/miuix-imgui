#include "skui/skui.h"
#include "skui/diagnostics.h"
#include "skui/devtools.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

// Headless CPU probe. No native window, GPU submission, texture upload or
// file I/O is timed. Compare the same compiler/build mode on the same machine.
int main() {
    using Clock=std::chrono::steady_clock;
    ImGui::CreateContext();
    auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.DisplaySize=ImVec2(800,1200);io.DeltaTime=1.f/60;
    unsigned char *pixels;int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    skui::Theme theme;
    std::vector<double> themes,frames;themes.reserve(3000);frames.reserve(3000);
    for(int frame=0;frame<3100;++frame) {
        auto start=Clock::now();skui::ApplyTheme(theme);auto themed=Clock::now();
        ImGui::NewFrame();ImGui::SetNextWindowSize(ImVec2(600,1000));ImGui::Begin("benchmark");
        for(int i=0;i<12;++i) {ImGui::PushID(i);skui::Button("Theme / performance");ImGui::PopID();}
        ImGui::End();ImGui::Render();auto end=Clock::now();
        if(frame>=100) {
            themes.push_back(std::chrono::duration<double,std::micro>(themed-start).count());
            frames.push_back(std::chrono::duration<double,std::micro>(end-start).count());
        }
    }
    auto print=[](const char *label,std::vector<double> &values) {
        std::sort(values.begin(),values.end());
        std::printf("%s p50=%.3f p95=%.3f us\n",label,values[1499],values[2849]);
    };
    std::puts("Headless CPU elapsed; 100 warmup / 3000 measured frames, 12 buttons");
    print("ApplyTheme",themes);print("UI frame",frames);
    for(bool inspect:{false,true}) {
        skui::SetItemInspectionEnabled(inspect);frames.clear();
        for(int frame=0;frame<3100;++frame) {
            auto start=Clock::now();ImGui::NewFrame();ImGui::SetNextWindowSize(ImVec2(600,1000));ImGui::Begin("inspection-benchmark");
            for(int i=0;i<12;++i) {
                char path[32];std::snprintf(path,sizeof(path),"benchmark/button/%d",i);ImGui::PushID(i);
                skui::Button("Theme / performance");skui::InspectLastItem(path);ImGui::PopID();
            }
            ImGui::End();ImGui::Render();auto end=Clock::now();
            if(frame>=100) frames.push_back(std::chrono::duration<double,std::micro>(end-start).count());
        }
        std::printf("12 explicit inspection markers enabled=%d ",inspect);print("UI frame",frames);
    }
    skui::SetItemInspectionEnabled(false);
    skui::FrameProfiler profiler;
    for(bool enabled:{false,true}) {
        profiler.SetEnabled(enabled);auto start=Clock::now();
        for(int i=0;i<100000;++i) {profiler.BeginFrame();profiler.EndInput();profiler.EndBuild();profiler.EndFrame();}
        std::printf("Profiler enabled=%d average=%.3f us/frame\n",enabled,
            std::chrono::duration<double,std::micro>(Clock::now()-start).count()/100000);
    }
    ImGui::DestroyContext();

    ImGui::CreateContext();
    auto &list_io=ImGui::GetIO();list_io.IniFilename=nullptr;list_io.DisplaySize=ImVec2(800,1200);list_io.DeltaTime=1.f/60;
    list_io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
    list_io.Fonts->AddFontDefault();theme.animations=false;frames.clear();int drawn=0;
    for(int frame=0;frame<3100;++frame) {
        auto start=Clock::now();skui::ApplyTheme(theme);ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(600,1000));ImGui::Begin("list-benchmark");
        drawn=0;skui::BeginScroll("records");ImGuiListClipper clipper;
        clipper.Begin(100000,skui::Dp(50)+ImGui::GetStyle().ItemSpacing.y);
        while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            char title[64];std::snprintf(title,sizeof(title),"Record %05d",i+1);ImGui::PushID(i+1);
            skui::ListItem("record",title,nullptr,i==0 ? "Selected" : nullptr,false);ImGui::PopID();++drawn;
        }
        skui::EndScroll();ImGui::End();ImGui::Render();auto end=Clock::now();
        if(frame>=100) frames.push_back(std::chrono::duration<double,std::micro>(end-start).count());
        // Simulated renderer acknowledgements; no GPU upload is included.
        for(auto *texture:list_io.Fonts->TexList) {
            if(texture->Status==ImTextureStatus_WantCreate) texture->SetTexID(texture->UniqueID+1);
            if(texture->Status==ImTextureStatus_WantCreate || texture->Status==ImTextureStatus_WantUpdates) texture->SetStatus(ImTextureStatus_OK);
            if(texture->Status==ImTextureStatus_WantDestroy) {texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);}
        }
    }
    std::printf("100000 fixed-height records; visible submitted=%d\n",drawn);print("List UI frame",frames);
    for(auto *texture:list_io.Fonts->TexList) {texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);}
    ImGui::DestroyContext();
}
