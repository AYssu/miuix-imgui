#include "skui/skui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace skui {
struct GlassBackdrop::Impl {
    ImGuiContext *context=nullptr;
    ImFontAtlas *atlas=nullptr;
    ImTextureData *texture=nullptr;
    ImVec2 min{},max{};
    int width=0,height=0;
    std::vector<ImVec4> pixels;
    ~Impl() {
        if(context==GImGui && context && context->FontAtlases.contains(atlas) && atlas->TexList.contains(texture))
            texture->WantDestroyNextFrame=true;
    }
};
GlassBackdrop::GlassBackdrop():impl_(new Impl) {}
GlassBackdrop::~GlassBackdrop()=default;
namespace {
float Cross(ImVec2 a,ImVec2 b,ImVec2 c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
void Blur(std::vector<ImVec4> &data,int w,int h,int radius) {
    if(radius<=0) return;
    std::vector<ImVec4> temp(data.size());
    for(int pass=0;pass<3;++pass) for(int axis=0;axis<2;++axis) {
        int lines=axis ? w : h,length=axis ? h : w;
        auto index=[&](int line,int v) {return axis ? v*w+line : line*w+v;};
        for(int line=0;line<lines;++line) {
            ImVec4 sum{};
            auto add=[&](const ImVec4 &p,float sign) {sum.x+=p.x*sign;sum.y+=p.y*sign;sum.z+=p.z*sign;sum.w+=p.w*sign;};
            for(int i=-radius;i<=radius;++i) add(data[index(line,std::clamp(i,0,length-1))],1);
            for(int i=0;i<length;++i) {
                float n=1.f/(radius*2+1);temp[index(line,i)]=ImVec4(sum.x*n,sum.y*n,sum.z*n,sum.w*n);
                add(data[index(line,std::clamp(i-radius,0,length-1))],-1);
                add(data[index(line,std::clamp(i+radius+1,0,length-1))],1);
            }
        }
        data.swap(temp);
    }
}
}
bool GlassBackdrop::Capture(ImDrawList *source,ImVec2 min,ImVec2 max) {
    if(!source || !GImGui || max.x<=min.x || max.y<=min.y ||
       !(ImGui::GetIO().BackendFlags&ImGuiBackendFlags_RendererHasTextures)) return false;
    auto &s=*impl_;
    if(s.context && s.context!=GImGui) return false;
    s.context=GImGui;s.atlas=ImGui::GetIO().Fonts;s.min=min;s.max=max;
    float ratio=std::min(1.f,192.f/std::max(max.x-min.x,max.y-min.y));
    s.width=std::max(1,static_cast<int>((max.x-min.x)*ratio));s.height=std::max(1,static_cast<int>((max.y-min.y)*ratio));
    s.pixels.assign(s.width*s.height,ImVec4(0,0,0,0));
    auto sx=[&](float x){return (x-min.x)/(max.x-min.x)*s.width;};
    auto sy=[&](float y){return (y-min.y)/(max.y-min.y)*s.height;};
    for(const auto &cmd:source->CmdBuffer) {
        if(cmd.UserCallback) continue;
        auto *tex=cmd.TexRef._TexData;
        // External GPU-only textures cannot be read synchronously here.
        if(!tex || !tex->Pixels || tex==s.texture) continue;
        for(unsigned i=0;i+2<cmd.ElemCount;i+=3) {
            const auto &a=source->VtxBuffer[cmd.VtxOffset+source->IdxBuffer[cmd.IdxOffset+i]];
            const auto &b=source->VtxBuffer[cmd.VtxOffset+source->IdxBuffer[cmd.IdxOffset+i+1]];
            const auto &c=source->VtxBuffer[cmd.VtxOffset+source->IdxBuffer[cmd.IdxOffset+i+2]];
            float area=Cross(a.pos,b.pos,c.pos);if(std::abs(area)<1e-6f) continue;
            int x0=std::clamp(static_cast<int>(std::floor(sx(std::max(std::min({a.pos.x,b.pos.x,c.pos.x}),cmd.ClipRect.x)))),0,s.width);
            int x1=std::clamp(static_cast<int>(std::ceil(sx(std::min(std::max({a.pos.x,b.pos.x,c.pos.x}),cmd.ClipRect.z)))),0,s.width);
            int y0=std::clamp(static_cast<int>(std::floor(sy(std::max(std::min({a.pos.y,b.pos.y,c.pos.y}),cmd.ClipRect.y)))),0,s.height);
            int y1=std::clamp(static_cast<int>(std::ceil(sy(std::min(std::max({a.pos.y,b.pos.y,c.pos.y}),cmd.ClipRect.w)))),0,s.height);
            auto ca=ImGui::ColorConvertU32ToFloat4(a.col),cb=ImGui::ColorConvertU32ToFloat4(b.col),cc=ImGui::ColorConvertU32ToFloat4(c.col);
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                ImVec2 p(min.x+(x+.5f)*(max.x-min.x)/s.width,min.y+(y+.5f)*(max.y-min.y)/s.height);
                if(p.x<cmd.ClipRect.x || p.x>=cmd.ClipRect.z || p.y<cmd.ClipRect.y || p.y>=cmd.ClipRect.w) continue;
                float u=Cross(b.pos,c.pos,p)/area,v=Cross(c.pos,a.pos,p)/area,w=1-u-v;
                if(u<0 || v<0 || w<0) continue;
                int tx=std::clamp(static_cast<int>((u*a.uv.x+v*b.uv.x+w*c.uv.x)*tex->Width),0,tex->Width-1);
                int ty=std::clamp(static_cast<int>((u*a.uv.y+v*b.uv.y+w*c.uv.y)*tex->Height),0,tex->Height-1);
                auto *pixel=static_cast<unsigned char *>(tex->GetPixelsAt(tx,ty));
                ImVec4 color(u*ca.x+v*cb.x+w*cc.x,u*ca.y+v*cb.y+w*cc.y,u*ca.z+v*cb.z+w*cc.z,u*ca.w+v*cb.w+w*cc.w);
                if(tex->Format==ImTextureFormat_RGBA32) {color.x*=pixel[0]/255.f;color.y*=pixel[1]/255.f;color.z*=pixel[2]/255.f;color.w*=pixel[3]/255.f;}
                else color.w*=pixel[0]/255.f;
                auto &dst=s.pixels[y*s.width+x];float a=color.w;
                dst=ImVec4(color.x*a+dst.x*(1-a),color.y*a+dst.y*(1-a),color.z*a+dst.z*(1-a),a+dst.w*(1-a));
            }
        }
    }
    return true;
}
bool GlassBackdrop::Draw(ImVec2 min,ImVec2 max,const GlassStyle &style) {
    auto &s=*impl_;if(s.pixels.empty() || s.context!=GImGui || max.x<=min.x || max.y<=min.y) return false;
    auto pixels=s.pixels;
    int radius=std::clamp(static_cast<int>(Dp(std::max(0.f,style.blur))*s.width/(s.max.x-s.min.x)/2),0,32);
    Blur(pixels,s.width,s.height,radius);
    if(!s.texture) {
        s.texture=IM_NEW(ImTextureData)();s.texture->UseColors=true;
        s.texture->Create(ImTextureFormat_RGBA32,s.width,s.height);s.atlas->TexList.push_back(s.texture);
    }
    if(s.texture->Width!=s.width || s.texture->Height!=s.height) {
        s.texture->WantDestroyNextFrame=true;s.texture=IM_NEW(ImTextureData)();s.texture->UseColors=true;
        s.texture->Create(ImTextureFormat_RGBA32,s.width,s.height);s.atlas->TexList.push_back(s.texture);
    }
    auto *out=s.texture->Pixels;
    for(int y=0;y<s.height;++y) for(int x=0;x<s.width;++x) {
        float u=(x+.5f)/s.width,v=(y+.5f)/s.height;
        float bend=std::clamp(style.refraction,0.f,24.f)*Dp(1);
        float px=min.x+u*(max.x-min.x)+(2*u-1)*bend*std::pow(std::abs(2*u-1),3.f);
        float py=min.y+v*(max.y-min.y)+(2*v-1)*bend*std::pow(std::abs(2*v-1),3.f);
        int ix=std::clamp(static_cast<int>((px-s.min.x)/(s.max.x-s.min.x)*s.width),0,s.width-1);
        int iy=std::clamp(static_cast<int>((py-s.min.y)/(s.max.y-s.min.y)*s.height),0,s.height-1);
        auto c=pixels[iy*s.width+ix];
        if(c.w>1e-6f) {c.x/=c.w;c.y/=c.w;c.z/=c.w;}
        float gray=.2126f*c.x+.7152f*c.y+.0722f*c.z,saturation=std::clamp(style.saturation,0.f,3.f);
        float values[]={gray+(c.x-gray)*saturation,gray+(c.y-gray)*saturation,gray+(c.z-gray)*saturation,c.w};
        for(int k=0;k<4;++k) out[(y*s.width+x)*4+k]=static_cast<unsigned char>(std::clamp(values[k],0.f,1.f)*255+.5f);
    }
    if(s.texture->Status==ImTextureStatus_OK) {
        s.texture->Updates.resize(1);s.texture->Updates[0]={0,0,static_cast<unsigned short>(s.width),static_cast<unsigned short>(s.height)};
        s.texture->UpdateRect=s.texture->Updates[0];s.texture->SetStatus(ImTextureStatus_WantUpdates);
    }
    auto *draw=ImGui::GetWindowDrawList();float rounding=Dp(std::max(0.f,style.rounding));
    // Soft elevation, then the sampled/blurred material, tint and rim.
    for(int i=6;i>0;--i) {
        float spread=Dp(i);draw->AddRectFilled(ImVec2(min.x-spread,min.y-spread+Dp(3)),ImVec2(max.x+spread,max.y+spread+Dp(3)),
            ImGui::GetColorU32(ImVec4(0,0,0,.012f)),rounding+spread);
    }
    draw->AddImageRounded(s.texture->GetTexRef(),min,max,ImVec2(0,0),ImVec2(1,1),ImGui::GetColorU32(ImVec4(1,1,1,1)),rounding);
    draw->AddRectFilled(min,max,ImGui::GetColorU32(style.tint),rounding);
    draw->AddRect(min,max,ImGui::GetColorU32(ImVec4(1,1,1,.35f)),rounding,Dp(1));
    return true;
}
} // namespace skui
