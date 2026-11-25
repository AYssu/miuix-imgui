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
void Add(ImVec4 &sum,const ImVec4 &p,float weight) {
    sum.x+=p.x*weight;sum.y+=p.y*weight;sum.z+=p.z*weight;sum.w+=p.w*weight;
}
// Area-filter before reducing resolution. Point-sampling thin shapes directly
// into the small blur buffer aliases them into moving bands and checkerboards.
void Reduce(std::vector<ImVec4> &data,int w,int h,int width,int height) {
    for(int axis=0;axis<2;++axis) {
        int length=axis ? h : w,target=axis ? height : width;
        if(target==length) continue;
        int lines=axis ? w : h;
        std::vector<ImVec4> out(lines*target);
        float step=static_cast<float>(length)/target;
        for(int line=0;line<lines;++line) for(int i=0;i<target;++i) {
            float lo=i*step,hi=std::min(static_cast<float>(length),(i+1)*step);ImVec4 sum{};
            for(int j=static_cast<int>(lo);j<static_cast<int>(std::ceil(hi));++j)
                Add(sum,data[axis ? j*w+line : line*w+j],(std::min(hi,j+1.f)-std::max(lo,static_cast<float>(j)))/step);
            out[axis ? i*w+line : line*target+i]=sum;
        }
        data.swap(out);if(axis) h=height;else w=width;
    }
}
void Blur(std::vector<ImVec4> &data,int w,int h,float sigma) {
    if(sigma<=0) return;
    // Three separable boxes approximate a Gaussian. Fractionally weighted
    // outer taps match its variance continuously, including below one pixel.
    float variance=std::min(sigma*sigma,1024.f)/3;
    int radius=static_cast<int>((std::sqrt(1+12*variance)-1)/2);
    float edge=std::clamp((2*radius+1)*(variance-radius*(radius+1)/3.f)/
        (2*((radius+1)*(radius+1)-variance)),0.f,1.f);
    float norm=1.f/(radius*2+1+2*edge);
    std::vector<ImVec4> temp(data.size());
    for(int pass=0;pass<3;++pass) for(int axis=0;axis<2;++axis) {
        if(axis) {
            // Walk rows in memory order instead of striding an entire column
            // per pixel. Keep one running sum per column for the vertical pass.
            std::vector<ImVec4> sums(w);
            for(int i=-radius;i<=radius;++i) for(int x=0;x<w;++x)
                Add(sums[x],data[std::clamp(i,0,h-1)*w+x],1);
            for(int y=0;y<h;++y) {
                int lo=std::clamp(y-radius-1,0,h-1)*w,hi=std::clamp(y+radius+1,0,h-1)*w;
                int leaving=std::clamp(y-radius,0,h-1)*w;
                for(int x=0;x<w;++x) {
                    auto filtered=sums[x];Add(filtered,data[lo+x],edge);Add(filtered,data[hi+x],edge);
                    temp[y*w+x]=ImVec4(filtered.x*norm,filtered.y*norm,filtered.z*norm,filtered.w*norm);
                    Add(sums[x],data[leaving+x],-1);Add(sums[x],data[hi+x],1);
                }
            }
            data.swap(temp);continue;
        }
        for(int line=0;line<h;++line) {
            auto index=[&](int x) {return line*w+std::clamp(x,0,w-1);};
            ImVec4 sum{};
            for(int i=-radius;i<=radius;++i) Add(sum,data[index(i)],1);
            for(int i=0;i<w;++i) {
                auto filtered=sum;
                Add(filtered,data[index(i-radius-1)],edge);Add(filtered,data[index(i+radius+1)],edge);
                temp[line*w+i]=ImVec4(filtered.x*norm,filtered.y*norm,filtered.z*norm,filtered.w*norm);
                Add(sum,data[index(i-radius)],-1);Add(sum,data[index(i+radius+1)],1);
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
        // Rasterize at bounded native density, then filter down to a blur grid.
        // Four samples per blur radius keep broad gradients smooth on upscale.
        auto fb=ImGui::GetIO().DisplayFramebufferScale;
        float density=std::max(1.f,std::max(fb.x,fb.y));
        float longest=std::max(s.max.x-s.min.x,s.max.y-s.min.y);
        // Bound both capture and output to 1M pixels and a 2048px side.
        float area=std::max((s.max.x-s.min.x)*(s.max.y-s.min.y),(max.x-min.x)*(max.y-min.y));
        float side=std::max(longest,std::max(max.x-min.x,max.y-min.y));
        density=std::min({density,2048.f/side,std::sqrt(1048576.f/area)});
        float ratio=blur>0 ? std::min(density,std::max(192.f/longest,4.f/blur)) : density;
        s.width=std::max(1,static_cast<int>((s.max.x-s.min.x)*density));
        s.height=std::max(1,static_cast<int>((s.max.y-s.min.y)*density));
        int width=std::max(1,static_cast<int>((max.x-min.x)*ratio));
        int height=std::max(1,static_cast<int>((max.y-min.y)*ratio));
        s.Rasterize();
        int sample_width=std::max(1,static_cast<int>((s.max.x-s.min.x)*ratio));
        int sample_height=std::max(1,static_cast<int>((s.max.y-s.min.y)*ratio));
        Reduce(s.pixels,s.width,s.height,sample_width,sample_height);
        s.width=sample_width;s.height=sample_height;
        Blur(s.pixels,s.width,s.height,blur*s.width/(s.max.x-s.min.x)/2);
        if(!s.texture) {
            s.texture=IM_NEW(ImTextureData)();s.texture->UseColors=true;
            s.texture->Create(ImTextureFormat_RGBA32,width,height);s.atlas->TexList.push_back(s.texture);
        }
        if(s.texture->Width!=width || s.texture->Height!=height) {
            s.texture->WantDestroyNextFrame=true;s.texture=IM_NEW(ImTextureData)();s.texture->UseColors=true;
            s.texture->Create(ImTextureFormat_RGBA32,width,height);s.atlas->TexList.push_back(s.texture);
        }
        auto *out=s.texture->Pixels;
        struct Sample {int lo,hi;float fraction;};
        auto coordinate=[&](int i,int length,float lo,float hi,float origin,float extent,int samples) {
            float u=(i+.5f)/length,q=2*u-1;
            float p=lo+u*(hi-lo)+q*bend*std::abs(q*q*q);
            float v=(p-origin)/extent*samples-.5f;int base=static_cast<int>(std::floor(v));
            return Sample{std::clamp(base,0,samples-1),std::clamp(base+1,0,samples-1),v-base};
        };
        // Refraction is separable. Resolve coordinates once per row/column,
        // rather than repeating divisions and edge clamps for every channel.
        std::vector<Sample> columns(width);
        for(int x=0;x<width;++x) columns[x]=coordinate(x,width,min.x,max.x,s.min.x,s.max.x-s.min.x,s.width);
        for(int y=0;y<height;++y) {
            auto row=coordinate(y,height,min.y,max.y,s.min.y,s.max.y-s.min.y,s.height);
            auto *upper=s.pixels.data()+row.lo*s.width,*lower=s.pixels.data()+row.hi*s.width;
            float fy=row.fraction;
            for(int x=0;x<width;++x) {
                auto column=columns[x];float fx=column.fraction;
                auto a=upper[column.lo],b=upper[column.hi],c0=lower[column.lo],d=lower[column.hi];
                ImVec4 c;
                for(int k=0;k<4;++k) (&c.x)[k]=((&a.x)[k]*(1-fx)+(&b.x)[k]*fx)*(1-fy)+((&c0.x)[k]*(1-fx)+(&d.x)[k]*fx)*fy;
                if(c.w>1e-6f) {c.x/=c.w;c.y/=c.w;c.z/=c.w;}
                float gray=.2126f*c.x+.7152f*c.y+.0722f*c.z;
                float values[]={gray+(c.x-gray)*saturation,gray+(c.y-gray)*saturation,gray+(c.z-gray)*saturation,c.w};
                for(int k=0;k<4;++k) out[(y*width+x)*4+k]=static_cast<unsigned char>(std::clamp(values[k],0.f,1.f)*255+.5f);
            }
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
