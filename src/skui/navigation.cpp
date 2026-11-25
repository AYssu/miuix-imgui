#include "skui/skui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
#include <unordered_map>

namespace skui {
namespace {
const ImGuiID focus_window_key=ImHashStr("skui/router/focus-window");
using Texture = std::shared_ptr<ImTextureData>;
struct DeleteMesh {
    std::vector<Texture> textures;
    void operator()(ImDrawList *p) const { IM_DELETE(p); }
};
using Mesh = std::unique_ptr<ImDrawList,DeleteMesh>;
using Scene = std::vector<Mesh>;

// Miuix NavProgrammaticEasing, normalized response=.8 / damping=.95 (07d48590).
float PageProgress(float seconds) {
    return std::clamp(SpringProgress(seconds/(.5f*std::max(.05f,GetTheme().motion_duration_scale)),
        GetTheme().motion_response,GetTheme().motion_damping),0.f,1.f);
}

struct CornerClip {
    ImVector<ImDrawIdx> indices;
    ImVector<ImDrawCmd> commands;
    std::vector<ImVec2> outline;
    ImRect bounds;
    float radius=0;

    void Set(const ImRect &rect,float r) {
        bounds=rect; radius=std::min({r,rect.GetWidth()/1.2f,rect.GetHeight()/2.4f});
        outline.clear();
        // Cubic squircle profile used by Miuix SquirclePath (leading corners).
        // Constants describe a single corner; use physical pixels throughout.
        auto curve=[&](ImVec2 a,ImVec2 b,ImVec2 c,ImVec2 d) {
            for(int i=1;i<=8;++i) {
                float t=i/8.f,u=1-t;
                outline.emplace_back(u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,
                                     u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y);
            }
        };
        float l=rect.Min.x,t=rect.Min.y,b=rect.Max.y;
        float reach=1.2f*radius;
        outline={{l+reach,t},{rect.Max.x,t},{rect.Max.x,b},{l+reach,b}};
        curve({l+reach,b},{l+.45f*radius,b},{l+.12f*radius,b-.15f*radius},{l+.05f*radius,b-.55f*radius});
        curve(outline.back(),{l,b-.75f*radius},{l,b-.9f*radius},{l,b-reach});
        outline.emplace_back(l,t+reach);
        curve(outline.back(),{l,t+.9f*radius},{l,t+.75f*radius},{l+.05f*radius,t+.55f*radius});
        curve(outline.back(),{l+.12f*radius,t+.15f*radius},{l+.45f*radius,t},{l+reach,t});
    }
    static float Side(ImVec2 a,ImVec2 b,ImVec2 p) {
        return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
    }
    static ImDrawVert Mix(const ImDrawVert &a,const ImDrawVert &b,float t) {
        ImDrawVert v;
        v.pos=ImVec2(a.pos.x+(b.pos.x-a.pos.x)*t,a.pos.y+(b.pos.y-a.pos.y)*t);
        v.uv=ImVec2(a.uv.x+(b.uv.x-a.uv.x)*t,a.uv.y+(b.uv.y-a.uv.y)*t);
        v.col=0;
        for(int shift=0;shift<32;shift+=8) {
            float x=static_cast<float>((a.col>>shift)&255),y=static_cast<float>((b.col>>shift)&255);
            v.col|=static_cast<ImU32>(std::clamp(std::lround(x+(y-x)*t),0l,255l))<<shift;
        }
        return v;
    }
    void Apply(ImDrawList *draw) {
        indices.clear();commands.clear();
        indices.reserve(draw->IdxBuffer.Size+256);commands.reserve(draw->CmdBuffer.Size+32);
        for(const auto &cmd:draw->CmdBuffer) {
            ImDrawCmd run=cmd;run.IdxOffset=indices.Size;run.ElemCount=0;
            auto flush=[&] {
                if(run.ElemCount || run.UserCallback) commands.push_back(run);
                run=cmd;run.IdxOffset=indices.Size;run.ElemCount=0;
            };
            if(cmd.UserCallback) {flush();continue;}
            for(unsigned i=0;i+2<cmd.ElemCount;i+=3) {
                const auto *idx=draw->IdxBuffer.Data+cmd.IdxOffset+i;
                ImDrawVert polygon[48],next[48];int count=3;
                for(int n=0;n<3;++n) polygon[n]=draw->VtxBuffer[cmd.VtxOffset+idx[n]];
                float minx=std::min({polygon[0].pos.x,polygon[1].pos.x,polygon[2].pos.x});
                float miny=std::min({polygon[0].pos.y,polygon[1].pos.y,polygon[2].pos.y});
                float maxy=std::max({polygon[0].pos.y,polygon[1].pos.y,polygon[2].pos.y});
                float reach=1.2f*radius;
                if(minx>=bounds.Min.x+reach || (miny>=bounds.Min.y+reach && maxy<=bounds.Max.y-reach)) {
                    for(int n=0;n<3;++n) indices.push_back(idx[n]);
                    run.ElemCount+=3;continue;
                }
                // Clip only corner triangles. Interior indices and vertices stay intact.
                for(std::size_t edge=0;edge<outline.size() && count; ++edge) {
                    ImVec2 a=outline[edge],b=outline[(edge+1)%outline.size()];
                    int output=0;
                    auto previous=polygon[count-1];float before=Side(a,b,previous.pos);
                    for(int n=0;n<count;++n) {
                        auto current=polygon[n];float after=Side(a,b,current.pos);
                        if((before>=0)!=(after>=0)) next[output++]=Mix(previous,current,before/(before-after));
                        if(after>=0) next[output++]=current;
                        previous=current;before=after;
                    }
                    count=output;std::copy(next,next+count,polygon);
                }
                if(count<3) continue;
                flush();
                auto clipped=cmd;clipped.IdxOffset=indices.Size;
                unsigned base=draw->VtxBuffer.Size;
                if(draw->Flags&ImDrawListFlags_AllowVtxOffset) clipped.VtxOffset=base;
                base-=clipped.VtxOffset;
                IM_ASSERT(static_cast<std::uint64_t>(base)+count<=static_cast<std::uint64_t>(std::numeric_limits<ImDrawIdx>::max())+1u);
                clipped.ElemCount=(count-2)*3;
                for(int n=0;n<count;++n) draw->VtxBuffer.push_back(polygon[n]);
                for(int n=1;n<count-1;++n) {
                    indices.push_back(static_cast<ImDrawIdx>(base));
                    indices.push_back(static_cast<ImDrawIdx>(base+n));indices.push_back(static_cast<ImDrawIdx>(base+n+1));
                }
                commands.push_back(clipped);
                run=cmd;run.IdxOffset=indices.Size;run.ElemCount=0;
            }
            flush();
        }
        draw->IdxBuffer.swap(indices);draw->CmdBuffer.swap(commands);
        draw->_IdxWritePtr=draw->IdxBuffer.Data+draw->IdxBuffer.Size;
        draw->_VtxWritePtr=draw->VtxBuffer.Data+draw->VtxBuffer.Size;
        draw->_VtxCurrentIdx=draw->VtxBuffer.Size-draw->_CmdHeader.VtxOffset;
    }
    void ApplyPage(ImGuiWindow *window) {
        Apply(window->DrawList);
        for(auto *child:window->DC.ChildWindows) if(child->Active) ApplyPage(child);
    }
};

void Transform(ImDrawList *draw, float dx, float dy, float alpha, const ImRect &bounds) {
    for (auto &v:draw->VtxBuffer) {
        v.pos.x+=dx; v.pos.y+=dy;
        auto a=static_cast<ImU32>(((v.col>>IM_COL32_A_SHIFT)&255)*alpha);
        v.col=(v.col&~IM_COL32_A_MASK)|(a<<IM_COL32_A_SHIFT);
    }
    for (auto &cmd:draw->CmdBuffer) {
        cmd.ClipRect.x=std::max(bounds.Min.x,cmd.ClipRect.x+dx);
        cmd.ClipRect.z=std::min(bounds.Max.x,cmd.ClipRect.z+dx);
        cmd.ClipRect.y=std::max(bounds.Min.y,cmd.ClipRect.y+dy);
        cmd.ClipRect.w=std::min(bounds.Max.y,cmd.ClipRect.w+dy);
    }
}
void TransformPage(ImGuiWindow *window, float dx, float alpha, const ImRect &bounds) {
    Transform(window->DrawList,dx,0,alpha,bounds);
    for(auto *child:window->DC.ChildWindows)
        if(child->Active) TransformPage(child,dx,alpha,bounds);
}
// Freeze atlas pixels as well as vertices: loading another page can repack or
// retire the dynamic atlas while the outgoing page is still being animated.
// The atlas owns these copies and lets the renderer retire them asynchronously.
Texture FreezeTexture(ImTextureData *source) {
    auto *context=GImGui;
    for(auto *atlas:context->FontAtlases) {
        if(!atlas->TexList.contains(source)) continue;
        auto *copy=IM_NEW(ImTextureData)();
        copy->UniqueID=atlas->TexNextUniqueID++;
        copy->Create(source->Format,source->Width,source->Height);
        copy->UseColors=source->UseColors;
        std::memcpy(copy->Pixels,source->Pixels,source->GetSizeInBytes());
        atlas->TexList.push_back(copy);
        return Texture(copy,[context,atlas](ImTextureData *texture) {
            // RouterView may outlive its context. Atlas destruction already
            // frees those textures; never access that context after shutdown.
            if(GImGui==context && context->FontAtlases.contains(atlas) && atlas->TexList.contains(texture))
                texture->WantDestroyNextFrame=true;
        });
    }
    return {};
}
void FreezeMesh(Mesh &mesh, std::unordered_map<ImTextureData*,Texture> &textures) {
    if(!(ImGui::GetIO().BackendFlags&ImGuiBackendFlags_RendererHasTextures)) return;
    for(auto &cmd:mesh->CmdBuffer) if(auto *source=cmd.TexRef._TexData) {
        auto it=textures.find(source);
        if(it==textures.end()) it=textures.emplace(source,FreezeTexture(source)).first;
        if(it->second) {
            cmd.TexRef=it->second->GetTexRef();
            mesh.get_deleter().textures.push_back(it->second);
        }
    }
}
void CapturePage(ImGuiWindow *window, int frame, Scene &scene,
                 std::unordered_map<ImTextureData*,Texture> &textures) {
    if(!window || window->LastFrameActive!=frame || window->Hidden) return;
    if(!window->DrawList->VtxBuffer.empty()) {
        Mesh mesh(window->DrawList->CloneOutput());
        FreezeMesh(mesh,textures);
        scene.push_back(std::move(mesh));
    }
    for(auto *child:window->DC.ChildWindows) CapturePage(child,frame,scene,textures);
}
void Capture(ImGuiWindow *window, int frame, Scene &scene) {
    std::unordered_map<ImTextureData*,Texture> textures;
    CapturePage(window,frame,scene,textures);
}
// Re-index command-sized batches through PrimReserve, so snapshots work with
// both 16-bit indices and renderers supporting vertex offsets. Never replay a
// page callback or a custom renderer callback with stale application state.
void Paint(const Scene &scene, ImDrawList *out, float dx, float dy, float alpha, const ImRect &bounds) {
    for(const auto &mesh:scene) for(const auto &cmd:mesh->CmdBuffer) {
        if(cmd.UserCallback || !cmd.ElemCount) continue;
        ImRect clip(ImVec2(cmd.ClipRect.x+dx,cmd.ClipRect.y+dy),
                    ImVec2(cmd.ClipRect.z+dx,cmd.ClipRect.w+dy));
        clip.ClipWith(bounds);
        if(clip.IsInverted()) continue;
        out->PushClipRect(clip.Min,clip.Max,true);
        out->PushTexture(cmd.TexRef);
        for(unsigned offset=0;offset<cmd.ElemCount;) {
            unsigned count=std::min(3000u,cmd.ElemCount-offset);
            const auto *indices=mesh->IdxBuffer.Data+cmd.IdxOffset+offset;
            auto limits=std::minmax_element(indices,indices+count);
            unsigned first=*limits.first, range=*limits.second-first+1;
            bool compact=range<32768;
            unsigned vertices=compact ? range : count;
            out->PrimReserve(count,vertices);
            unsigned base=out->_VtxCurrentIdx;
            for(unsigned i=0;i<vertices;++i) {
                auto v=mesh->VtxBuffer[cmd.VtxOffset+(compact ? first+i : indices[i])];
                v.pos.x+=dx; v.pos.y+=dy;
                auto a=static_cast<ImU32>(((v.col>>IM_COL32_A_SHIFT)&255)*alpha);
                v.col=(v.col&~IM_COL32_A_MASK)|(a<<IM_COL32_A_SHIFT);
                *out->_VtxWritePtr++=v;
            }
            for(unsigned i=0;i<count;++i)
                *out->_IdxWritePtr++=static_cast<ImDrawIdx>(base+(compact ? indices[i]-first : i));
            out->_VtxCurrentIdx+=vertices;
            offset+=count;
        }
        out->PopTexture();
        out->PopClipRect();
    }
}
}

struct RouterView::Impl {
    ImGuiContext *context=nullptr;
    Router *router=nullptr;
    ImGuiID page=0;
    int frame=-1;
    std::uint64_t revision=0;
    Router::Navigation direction=Router::Navigation::Reset;
    Scene outgoing;
    CornerClip corners;
    ImRect bounds;
    float elapsed=0, old_dx=0, old_alpha=1, old_dim=0;
    bool moving=false,focus_pending=false;
    void Stop() { moving=false; outgoing.clear(); }
};

RouterView::RouterView():impl_(std::make_unique<Impl>()) {}
RouterView::~RouterView()=default;
bool RouterView::IsTransitioning() const { return impl_->moving; }

void RouterView::Draw(Router &router, ImVec2 size, const Router::Page &header) {
    auto &s=*impl_;
    if(s.context!=GImGui || s.router!=&router || s.frame>=GImGui->FrameCount) {
        s=Impl{}; s.context=GImGui; s.router=&router;
    }
    if(!router.Current()) { s.Stop(); return; }
    auto min=ImGui::GetCursorScreenPos(), available=ImGui::GetContentRegionAvail();
    if(size.x<=0) size.x=std::max(1.f,available.x+size.x);
    if(size.y<=0) size.y=std::max(1.f,available.y+size.y);
    ImRect bounds(min,ImVec2(min.x+size.x,min.y+size.y));
    bool changed=s.page && s.revision!=router.Revision();
    auto *previous=ImGui::FindWindowByID(s.page);
    bool transfer_focus=changed && previous && GImGui->NavWindow &&
        ImGui::IsWindowChildOf(GImGui->NavWindow,previous,true);
    if(transfer_focus) s.focus_pending=true;
    bool continuous=s.frame==GImGui->FrameCount-1 &&
                    std::abs(s.bounds.GetWidth()-size.x)<1 && std::abs(s.bounds.GetHeight()-size.y)<1;
    if(changed) {
        Scene current;
        if(GetTheme().animations && continuous) {
            Capture(ImGui::FindWindowByID(s.page),s.frame,current);
            if(s.moving) {
                for(auto &mesh:s.outgoing) Transform(mesh.get(),s.old_dx,0,s.old_alpha,s.bounds);
                Mesh shade;
                if(s.old_dim>0) {
                    shade.reset(IM_NEW(ImDrawList)(ImGui::GetDrawListSharedData()));
                    shade->_ResetForNewFrame();
                    shade->PushTexture(ImGui::GetIO().Fonts->TexRef);
                    shade->PushClipRect(s.bounds.Min,s.bounds.Max);
                    shade->AddRectFilled(s.bounds.Min,s.bounds.Max,IM_COL32(0,0,0,static_cast<int>(255*s.old_dim)));
                    shade->PopClipRect();shade->PopTexture();
                    shade->_SetDrawListSharedData(nullptr);
                    std::unordered_map<ImTextureData*,Texture> textures;
                    FreezeMesh(shade,textures);
                }
                // Preserve the visible composition when another navigation
                // interrupts a transition, without re-running either page.
                if(s.direction==Router::Navigation::Back) {
                    if(shade) current.push_back(std::move(shade));
                    for(auto &mesh:s.outgoing) current.push_back(std::move(mesh));
                } else {
                    if(shade) s.outgoing.push_back(std::move(shade));
                    for(auto &mesh:current) s.outgoing.push_back(std::move(mesh));
                    current=std::move(s.outgoing);
                }
            }
        }
        s.outgoing=std::move(current);
        s.moving=!s.outgoing.empty();
        s.elapsed=0;
        s.direction=router.LastNavigation();
        if(auto *active=GImGui->ActiveIdWindow)
            if(auto *previous=ImGui::FindWindowByID(s.page))
                if(ImGui::IsWindowChildOf(active,previous,true)) ImGui::ClearActiveID();
        ImGui::ClosePopupsOverWindow(ImGui::GetCurrentWindow(),true);
    }
    if(!GetTheme().animations || !continuous) s.Stop();
    float duration=(s.direction==Router::Navigation::Reset ? .2f : .5f)*std::max(.05f,GetTheme().motion_duration_scale);
    if(s.moving) {
        if(!changed) s.elapsed+=ImGui::GetIO().DeltaTime;
        if(s.elapsed+1e-6f>=duration) s.Stop();
    }
    float t=s.moving ? std::clamp(s.elapsed/duration,0.f,1.f) : 1.f;
    if(s.moving) RequestAnimationFrame();
    float progress=s.moving ? (s.direction==Router::Navigation::Reset ? 1-std::pow(1-t,3) : PageProgress(s.elapsed)) : 1;
    float width=size.x;
    float incoming_dx=s.direction==Router::Navigation::Back ? -width*.25f*(1-progress) :
                      s.direction==Router::Navigation::Forward ? std::round(width*(1-progress)) : 0;
    float outgoing_dx=s.direction==Router::Navigation::Back ? std::round(width*progress) :
                      s.direction==Router::Navigation::Forward ? -width*.25f*progress : 0;
    float incoming_alpha=s.direction==Router::Navigation::Reset ? progress :
                         s.direction==Router::Navigation::Back ? .9f+.1f*progress : 1;
    float outgoing_alpha=s.direction==Router::Navigation::Forward ? 1-.1f*progress : 1;
    float origin_dx=bounds.Min.x-s.bounds.Min.x, origin_dy=bounds.Min.y-s.bounds.Min.y;
    if(s.moving && s.direction!=Router::Navigation::Back) {
        Paint(s.outgoing,ImGui::GetWindowDrawList(),outgoing_dx+origin_dx,origin_dy,outgoing_alpha,bounds);
        if(s.direction==Router::Navigation::Forward)
            ImGui::GetWindowDrawList()->AddRectFilled(bounds.Min,bounds.Max,IM_COL32(0,0,0,static_cast<int>(255*.2f*progress)));
    }

    ImGui::PushID(this);
    const auto rendered_revision=router.Revision();
    const Route route=*router.Current();
    ImGui::PushID(route.path.c_str());
    ImGui::PushID(static_cast<int>(router.Depth()));
    ImGui::PushID(route.parameter.c_str());
    bool visible=ImGui::BeginChild("page",size,0,ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto *page=ImGui::GetCurrentWindow();
    page->DrawList->AddRectFilled(page->InnerRect.Min,page->InnerRect.Max,ImGui::GetColorU32(Background()));
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,1.f);
    ImGui::BeginDisabled(s.moving);
    ImGui::PopStyleVar();
    bool content_open=false;
    try {
        if(visible) router.DrawBefore([&](const Route &entry) {
            if(header) header(entry);
            BeginScroll("content");content_open=true;
            if(!s.moving && s.focus_pending && GImGui->OpenPopupStack.empty()) {
                // Stable page windows already retain ImGui's last focus and
                // scroll. Transfer before submission so a new page can init.
                auto *target=ImGui::FindWindowByID(static_cast<ImGuiID>(page->StateStorage.GetInt(focus_window_key)));
                if(!target || !ImGui::IsWindowChildOf(target,page,true)) target=ImGui::GetCurrentWindow();
                ImGui::FocusWindow(target);
                ImGui::NavInitWindow(GImGui->NavWindow,false);s.focus_pending=false;
            }
        });
        else {BeginScroll("content");content_open=true;}
    } catch(...) {
        if(content_open) EndScroll();
        ImGui::EndDisabled(); ImGui::EndChild(); ImGui::PopID(); ImGui::PopID(); ImGui::PopID(); ImGui::PopID();
        s.Stop(); s.page=0;
        throw;
    }
    if(!s.moving && rendered_revision==router.Revision() && router.CanGoBack() &&
       (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) &&
       GImGui->OpenPopupStack.empty() && GImGui->ActiveId==0 && GImGui->ActiveIdPreviousFrame==0 &&
       (ImGui::Shortcut(ImGuiKey_Escape) || ImGui::Shortcut(ImGuiMod_Alt|ImGuiKey_LeftArrow)))
        router.Back();
    if(GImGui->NavWindow && !(GImGui->NavWindow->RootWindow->Flags & ImGuiWindowFlags_Popup) &&
       ImGui::IsWindowChildOf(GImGui->NavWindow,page,true))
        page->StateStorage.SetInt(focus_window_key,static_cast<int>(GImGui->NavWindow->ID));
    if(content_open) EndScroll();
    ImGui::EndDisabled(); ImGui::EndChild();
    if(s.moving) TransformPage(page,incoming_dx,incoming_alpha,bounds);
    if(s.moving && s.direction==Router::Navigation::Forward) {
        s.corners.Set(ImRect(ImVec2(bounds.Min.x+incoming_dx,bounds.Min.y),
                            ImVec2(bounds.Max.x+incoming_dx,bounds.Max.y)),Dp(28));
        s.corners.ApplyPage(page);
    }
    ImGui::PopID(); ImGui::PopID(); ImGui::PopID();
    if(s.moving && s.direction==Router::Navigation::Back) {
        auto cursor=ImGui::GetCursorScreenPos();
        auto item=GImGui->LastItemData;
        bool was_set_pos=ImGui::GetCurrentWindow()->DC.IsSetPos;
        ImGui::SetCursorScreenPos(bounds.Min);
        ImGui::BeginChild("back-shade",size,0,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs|
                          ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::GetWindowDrawList()->AddRectFilled(bounds.Min,bounds.Max,IM_COL32(0,0,0,static_cast<int>(255*.2f*(1-progress))));
        ImGui::EndChild();
        ImGui::SetCursorScreenPos(bounds.Min);
        ImGui::BeginChild("outgoing",size,0,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs|
                          ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        auto *underlay=ImGui::GetWindowDrawList();
        Paint(s.outgoing,ImGui::GetWindowDrawList(),outgoing_dx+origin_dx,origin_dy,outgoing_alpha,bounds);
        s.corners.Set(ImRect(ImVec2(bounds.Min.x+outgoing_dx,bounds.Min.y),
                            ImVec2(bounds.Max.x+outgoing_dx,bounds.Max.y)),Dp(28));
        s.corners.Apply(underlay);
        ImGui::EndChild();
        ImGui::SetCursorScreenPos(cursor);
        ImGui::GetCurrentWindow()->DC.IsSetPos=was_set_pos;
        GImGui->LastItemData=item;
    }
    ImGui::PopID();
    // Geometry now lives in the current view's coordinate space.
    if(s.moving && (origin_dx!=0 || origin_dy!=0))
        for(auto &mesh:s.outgoing) Transform(mesh.get(),origin_dx,origin_dy,1,bounds);
    s.old_dx=outgoing_dx; s.old_alpha=outgoing_alpha;
    s.old_dim=s.direction==Router::Navigation::Forward ? .2f*progress :
              s.direction==Router::Navigation::Back ? .2f*(1-progress) : 0;
    s.page=page->ID; s.frame=GImGui->FrameCount;
    // A callback may have queued navigation after drawing this page.
    s.revision=rendered_revision;
    s.bounds=bounds;
}
}
