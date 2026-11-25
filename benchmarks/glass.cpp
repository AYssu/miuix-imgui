#include "skui/skui.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

// CPU capture+raster/filter/material only. No native surface, GPU timing,
// texture upload or power measurement; compare identical builds/devices.
int main() {
    ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.DisplaySize=ImVec2(1000,800);io.DeltaTime=1.f/60;
    io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures|ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
    io.Fonts->AddFontDefault();skui::ApplyTheme({});
    auto acknowledge=[&] {
        for(auto *texture:io.Fonts->TexList) {
            if(texture->Status==ImTextureStatus_WantCreate) texture->SetTexID(texture->UniqueID+1);
            if(texture->Status==ImTextureStatus_WantCreate || texture->Status==ImTextureStatus_WantUpdates) texture->SetStatus(ImTextureStatus_OK);
            if(texture->Status==ImTextureStatus_WantDestroy) {texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);}
        }
    };
    std::puts("Glass CPU only; each case 10 warmup / 60 samples; density=1; no GPU upload or submission");
    {
        skui::GlassBackdrop glass;
        for(auto size:{ImVec2(160,100),ImVec2(360,220),ImVec2(680,420)}) for(float blur:{0.f,4.f,12.f,24.f}) {
            std::array<double,60> samples{};int texture_width=0,texture_height=0;
            for(int frame=0;frame<70;++frame) {
                ImGui::NewFrame();ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(950,750));
                ImGui::Begin("glass-benchmark",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoBackground);
                auto *draw=ImGui::GetWindowDrawList();ImVec2 min(20,20),max(20+size.x,20+size.y);
                draw->AddRectFilled(min,max,IM_COL32(24,65,120,255));
                for(int i=0;i<80;++i) {
                    float x=min.x+i*(size.x/80.f);
                    draw->AddRectFilled(ImVec2(x,min.y),ImVec2(x+1,max.y),i%2 ? IM_COL32(250,120,40,255) : IM_COL32(240,240,240,160));
                }
                skui::GlassStyle style;style.blur=blur;style.refraction=0;style.saturation=1;style.tint=ImVec4(0,0,0,0);
                auto begin=std::chrono::steady_clock::now();
                bool ok=glass.Capture(draw,min,max) && glass.Draw(min,max,style);
                auto end=std::chrono::steady_clock::now();
                if(!ok) return 1;
                if(frame>=10) samples[frame-10]=std::chrono::duration<double,std::milli>(end-begin).count();
                texture_width=texture_height=0;
                for(const auto &command:draw->CmdBuffer) {
                    auto *texture=command.TexRef._TexData;
                    if(command.ElemCount && texture && texture!=io.Fonts->TexData) {texture_width=texture->Width;texture_height=texture->Height;}
                }
                ImGui::End();ImGui::Render();acknowledge();
            }
            std::sort(samples.begin(),samples.end());
            std::printf("%.0fx%.0f blur=%.0f CPU p50=%.3f p95=%.3f ms material=%dx%d\n",size.x,size.y,blur,samples[29],samples[56],texture_width,texture_height);
        }
    }
    // Consume deferred texture retirement before destroying the simulated host.
    ImGui::NewFrame();ImGui::Render();acknowledge();
    for(auto *texture:io.Fonts->TexList) {texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);}
    ImGui::DestroyContext();
}
