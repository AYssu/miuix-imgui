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
    ImDrawList *source=nullptr;
    ImVec2 min{},max{};
    int width=0,height=0;
    int frame=-1;
    ImVector<ImDrawCmd> commands;
    ImVector<ImDrawVert> vertices;
    ImVector<ImDrawIdx> indices;
    std::vector<ImVec4> pixels;
    void Rasterize();
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
    s.context=GImGui;s.atlas=ImGui::GetIO().Fonts;s.source=source;s.min=min;s.max=max;
    // Freeze the capture before Draw adds the material and foreground. Delay
    // rasterization until its blur is known, so clear glass is not downsampled.
    s.commands=source->CmdBuffer;s.vertices=source->VtxBuffer;s.indices=source->IdxBuffer;
    s.frame=GImGui->FrameCount;
    return true;
}
void GlassBackdrop::Impl::Rasterize() {
    auto &s=*this;auto min=s.min,max=s.max;
    s.pixels.assign(s.width*s.height,ImVec4(0,0,0,0));
    auto sx=[&](float x){return (x-min.x)/(max.x-min.x)*s.width;};
    auto sy=[&](float y){return (y-min.y)/(max.y-min.y)*s.height;};
    for(const auto &cmd:s.commands) {
        if(cmd.UserCallback) continue;
        auto *tex=cmd.TexRef._TexData;
        // External GPU-only textures cannot be read synchronously here.
        if(!tex || !tex->Pixels || tex==s.texture) continue;
        for(unsigned i=0;i+2<cmd.ElemCount;i+=3) {
            const auto &a=s.vertices[cmd.VtxOffset+s.indices[cmd.IdxOffset+i]];
            const auto &b=s.vertices[cmd.VtxOffset+s.indices[cmd.IdxOffset+i+1]];
            const auto &c=s.vertices[cmd.VtxOffset+s.indices[cmd.IdxOffset+i+2]];
            float area=Cross(a.pos,b.pos,c.pos);if(std::abs(area)<1e-6f) continue;
            int x0=std::clamp(static_cast<int>(std::floor(sx(std::max(std::min({a.pos.x,b.pos.x,c.pos.x}),cmd.ClipRect.x)))),0,s.width);
            int x1=std::clamp(static_cast<int>(std::ceil(sx(std::min(std::max({a.pos.x,b.pos.x,c.pos.x}),cmd.ClipRect.z)))),0,s.width);
            int y0=std::clamp(static_cast<int>(std::floor(sy(std::max(std::min({a.pos.y,b.pos.y,c.pos.y}),cmd.ClipRect.y)))),0,s.height);
            int y1=std::clamp(static_cast<int>(std::ceil(sy(std::min(std::max({a.pos.y,b.pos.y,c.pos.y}),cmd.ClipRect.w)))),0,s.height);
            auto ca=ImGui::ColorConvertU32ToFloat4(a.col),cb=ImGui::ColorConvertU32ToFloat4(b.col),cc=ImGui::ColorConvertU32ToFloat4(c.col);
            auto modulate=[&](ImVec4 color,float u,float v) {
                int tx=std::clamp(static_cast<int>(u*tex->Width),0,tex->Width-1);
                int ty=std::clamp(static_cast<int>(v*tex->Height),0,tex->Height-1);
                auto *pixel=static_cast<unsigned char *>(tex->GetPixelsAt(tx,ty));
                if(tex->Format==ImTextureFormat_RGBA32) {color.x*=pixel[0]/255.f;color.y*=pixel[1]/255.f;color.z*=pixel[2]/255.f;color.w*=pixel[3]/255.f;}
                else color.w*=pixel[0]/255.f;
                return color;
            };
            bool solid=a.col==b.col && a.col==c.col && a.uv.x==b.uv.x && a.uv.x==c.uv.x && a.uv.y==b.uv.y && a.uv.y==c.uv.y;
            ImVec4 flat=solid ? modulate(ca,a.uv.x,a.uv.y) : ImVec4{};
            // Scan triangle spans instead of shading their bounding boxes.
            // Shared edges are half-open, so translucent triangles blend once.
            for(int y=y0;y<y1;++y) {
                float py=min.y+(y+.5f)*(max.y-min.y)/s.height;
                if(py<cmd.ClipRect.y || py>=cmd.ClipRect.w) continue;
                float left=INFINITY,right=-INFINITY;
                ImVec2 points[]={a.pos,b.pos,c.pos};
                for(int e=0;e<3;++e) {
                    auto lo=points[e],hi=points[(e+1)%3];if(lo.y>hi.y) std::swap(lo,hi);
                    if(py<lo.y || py>=hi.y) continue;
                    float x=lo.x+(py-lo.y)*(hi.x-lo.x)/(hi.y-lo.y);
                    left=std::min(left,x);right=std::max(right,x);
                }
                if(left>=right) continue;
                int begin=std::max(x0,static_cast<int>(std::ceil(sx(std::max(left,cmd.ClipRect.x))-.5f)));
                int end=std::min(x1,static_cast<int>(std::ceil(sx(std::min(right,cmd.ClipRect.z))-.5f)));
                if(begin>=end) continue;
                if(solid && flat.w==1) {
                    std::fill(s.pixels.begin()+y*s.width+begin,s.pixels.begin()+y*s.width+end,flat);
                    continue;
                }
                for(int x=begin;x<end;++x) {
                    ImVec4 color=flat;
                    if(!solid) {
                        ImVec2 p(min.x+(x+.5f)*(max.x-min.x)/s.width,py);
                        float u=Cross(b.pos,c.pos,p)/area,v=Cross(c.pos,a.pos,p)/area,w=1-u-v;
                        color=modulate(ImVec4(u*ca.x+v*cb.x+w*cc.x,u*ca.y+v*cb.y+w*cc.y,u*ca.z+v*cb.z+w*cc.z,u*ca.w+v*cb.w+w*cc.w),
                            u*a.uv.x+v*b.uv.x+w*c.uv.x,u*a.uv.y+v*b.uv.y+w*c.uv.y);
                    }
                    auto &dst=s.pixels[y*s.width+x];float alpha=color.w;
                    dst=ImVec4(color.x*alpha+dst.x*(1-alpha),color.y*alpha+dst.y*(1-alpha),color.z*alpha+dst.z*(1-alpha),alpha+dst.w*(1-alpha));
                }
            }
        }
    }
}
bool GlassBackdrop::Draw(ImVec2 min,ImVec2 max,const GlassStyle &style) {
    auto &s=*impl_;if(!GImGui || s.context!=GImGui || s.frame!=GImGui->FrameCount || max.x<=min.x || max.y<=min.y) return false;
    float blur=Dp(std::max(0.f,style.blur)),bend=std::clamp(style.refraction,0.f,24.f)*Dp(1);
    float saturation=std::clamp(style.saturation,0.f,3.f);
    auto *draw=ImGui::GetWindowDrawList();float rounding=Dp(std::max(0.f,style.rounding));
    bool clear=blur==0 && bend==0 && saturation==1 && s.source==draw &&
        s.vertices.Size==draw->VtxBuffer.Size && s.indices.Size==draw->IdxBuffer.Size &&
        min.x>=s.min.x && min.y>=s.min.y && max.x<=s.max.x && max.y<=s.max.y;
    if(!clear) {
        // Small/zero blur needs native pixels. Only lower the resolution when the
        // requested blur can hide that reduction; retain the old 192px floor.
        auto fb=ImGui::GetIO().DisplayFramebufferScale;
        float density=std::max(1.f,std::max(fb.x,fb.y));
        float longest=std::max(s.max.x-s.min.x,s.max.y-s.min.y);
        float ratio=blur>0 ? std::min(density,std::max(192.f/longest,2.f/blur)) : density;
        // Bound both capture and output to 1M pixels and a 2048px side.
        float area=std::max((s.max.x-s.min.x)*(s.max.y-s.min.y),(max.x-min.x)*(max.y-min.y));
        float side=std::max(longest,std::max(max.x-min.x,max.y-min.y));
        ratio=std::min({ratio,2048.f/side,std::sqrt(1048576.f/area)});
        s.width=std::max(1,static_cast<int>((s.max.x-s.min.x)*ratio));
        s.height=std::max(1,static_cast<int>((s.max.y-s.min.y)*ratio));
        int width=std::max(1,static_cast<int>((max.x-min.x)*ratio));
        int height=std::max(1,static_cast<int>((max.y-min.y)*ratio));
        s.Rasterize();
        int radius=std::clamp(static_cast<int>(blur*s.width/(s.max.x-s.min.x)/2),0,32);
        Blur(s.pixels,s.width,s.height,radius);
        if(!s.texture) {
            s.texture=IM_NEW(ImTextureData)();s.texture->UseColors=true;
            s.texture->Create(ImTextureFormat_RGBA32,width,height);s.atlas->TexList.push_back(s.texture);
        }
        if(s.texture->Width!=width || s.texture->Height!=height) {
            s.texture->WantDestroyNextFrame=true;s.texture=IM_NEW(ImTextureData)();s.texture->UseColors=true;
            s.texture->Create(ImTextureFormat_RGBA32,width,height);s.atlas->TexList.push_back(s.texture);
        }
        auto *out=s.texture->Pixels;
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
            float u=(x+.5f)/width,v=(y+.5f)/height;
            float qx=2*u-1,qy=2*v-1;
            float px=min.x+u*(max.x-min.x)+qx*bend*std::abs(qx*qx*qx);
            float py=min.y+v*(max.y-min.y)+qy*bend*std::abs(qy*qy*qy);
            float sx=(px-s.min.x)/(s.max.x-s.min.x)*s.width-.5f;
            float sy=(py-s.min.y)/(s.max.y-s.min.y)*s.height-.5f;
            int ix=static_cast<int>(std::floor(sx)),iy=static_cast<int>(std::floor(sy));
            float fx=sx-ix,fy=sy-iy;
            auto at=[&](int x,int y){return s.pixels[std::clamp(y,0,s.height-1)*s.width+std::clamp(x,0,s.width-1)];};
            auto a=at(ix,iy),b=at(ix+1,iy),c0=at(ix,iy+1),d=at(ix+1,iy+1);
            ImVec4 c;
            for(int k=0;k<4;++k) (&c.x)[k]=((&a.x)[k]*(1-fx)+(&b.x)[k]*fx)*(1-fy)+((&c0.x)[k]*(1-fx)+(&d.x)[k]*fx)*fy;
            if(c.w>1e-6f) {c.x/=c.w;c.y/=c.w;c.z/=c.w;}
            float gray=.2126f*c.x+.7152f*c.y+.0722f*c.z;
            float values[]={gray+(c.x-gray)*saturation,gray+(c.y-gray)*saturation,gray+(c.z-gray)*saturation,c.w};
            for(int k=0;k<4;++k) out[(y*width+x)*4+k]=static_cast<unsigned char>(std::clamp(values[k],0.f,1.f)*255+.5f);
        }
        if(s.texture->Status==ImTextureStatus_OK) {
            s.texture->Updates.resize(1);s.texture->Updates[0]={0,0,static_cast<unsigned short>(width),static_cast<unsigned short>(height)};
            s.texture->UpdateRect=s.texture->Updates[0];s.texture->SetStatus(ImTextureStatus_WantUpdates);
        }
    }
    // Soft elevation, then the sampled/blurred material, tint and rim.
    for(int i=6;i>0;--i) {
        float spread=Dp(i);draw->AddRectFilled(ImVec2(min.x-spread,min.y-spread+Dp(3)),ImVec2(max.x+spread,max.y+spread+Dp(3)),
            ImGui::GetColorU32(ImVec4(0,0,0,.012f)),rounding+spread);
    }
    if(!clear) draw->AddImageRounded(s.texture->GetTexRef(),min,max,ImVec2(0,0),ImVec2(1,1),ImGui::GetColorU32(ImVec4(1,1,1,1)),rounding);
    draw->AddRectFilled(min,max,ImGui::GetColorU32(style.tint),rounding);
    draw->AddRect(min,max,ImGui::GetColorU32(ImVec4(1,1,1,.35f)),rounding,Dp(1));
    return true;
}
} // namespace skui
