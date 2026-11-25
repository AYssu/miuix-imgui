#include "skui/skui.h"
#include "skui/diagnostics.h"
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
    skui::FrameProfiler profiler;
    for(bool enabled:{false,true}) {
        profiler.SetEnabled(enabled);auto start=Clock::now();
        for(int i=0;i<100000;++i) {profiler.BeginFrame();profiler.EndInput();profiler.EndBuild();profiler.EndFrame();}
        std::printf("Profiler enabled=%d average=%.3f us/frame\n",enabled,
            std::chrono::duration<double,std::micro>(Clock::now()-start).count()/100000);
    }
    ImGui::DestroyContext();
}
