#include "skui/skui.h"
#include "imgui_internal.h"
#include "detail.h"
#include <algorithm>
#include <cmath>

namespace skui {
ViewportBounds ResolveViewport(ImVec2 display,SafeInsets safe,float keyboard_height) {
    auto positive=[](float value){return std::isfinite(value) ? std::max(0.f,value) : 0.f;};
    display=ImVec2(positive(display.x),positive(display.y));
    safe.left=std::min(positive(safe.left),display.x);safe.right=std::min(positive(safe.right),display.x-safe.left);
    safe.top=std::min(positive(safe.top),display.y);
    safe.bottom=std::min(std::max(positive(safe.bottom),positive(keyboard_height)),display.y-safe.top);
    return {ImVec2(safe.left,safe.top),ImVec2(display.x-safe.left-safe.right,display.y-safe.top-safe.bottom)};
}
WindowPlacement FitWindow(WindowPlacement window,ViewportBounds viewport,ImVec2 minimum) {
    for(int axis=0;axis<2;++axis) {
        float extent=std::isfinite(viewport.size[axis]) ? std::max(0.f,viewport.size[axis]) : 0.f;
        float origin=std::isfinite(viewport.position[axis]) ? viewport.position[axis] : 0.f;
        float lower=std::isfinite(minimum[axis]) ? std::clamp(minimum[axis],0.f,extent) : 0.f;
        float size=std::isfinite(window.size[axis]) ? window.size[axis] : extent;
        window.size[axis]=std::clamp(size,lower,extent);
        float position=std::isfinite(window.position[axis]) ? window.position[axis] : origin;
        window.position[axis]=std::clamp(position,origin,origin+extent-window.size[axis]);
    }
    return window;
}
WidthClass ClassifyWidth(float width) {
    return !std::isfinite(width) || width<Dp(600) ? WidthClass::Compact : width<Dp(840) ? WidthClass::Medium : WidthClass::Expanded;
}
int AdaptiveColumns(float available,float minimum,int maximum) {
    if(!std::isfinite(available) || available<=0 || !std::isfinite(minimum) || minimum<=0) return 1;
    maximum=std::clamp(maximum,1,16);
    float gap=Dp(GetTheme().tokens.item_spacing.x);
    float cell=Dp(minimum*std::max(1.f,GetTheme().tokens.text_scale));
    return static_cast<int>(std::clamp(std::floor((available+gap)/(cell+gap)),1.f,static_cast<float>(maximum)));
}
bool BeginAdaptiveGrid(const char *id,float minimum,int maximum) {
    int columns=AdaptiveColumns(ImGui::GetContentRegionAvail().x,minimum,maximum);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(Dp(GetTheme().tokens.item_spacing.x)*.5f,Dp(GetTheme().tokens.item_spacing.y)*.5f));
    bool visible=ImGui::BeginTable(id,columns,ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings);
    ImGui::PopStyleVar();return visible;
}
void NextAdaptiveCell() {ImGui::TableNextColumn();}
void EndAdaptiveGrid() {ImGui::EndTable();}
float detail::RowTextHeight(const char *title,const char *description,float width,float minimum,float title_size,float description_size) {
    width=std::max(1.f,width);
    float height=title ? ImGui::GetFont()->CalcTextSizeA(TextSize(title_size),FLT_MAX,width,title).y : 0;
    if(description) height+=Dp(8)+ImGui::GetFont()->CalcTextSizeA(TextSize(description_size),FLT_MAX,width,description).y;
    return std::max(Dp(minimum),height+Dp(24));
}
void detail::RowText(ImVec2 min,ImVec2 max,const char *title,const char *description,float left,float right,float title_size,float description_size) {
    float width=std::max(1.f,max.x-min.x-left-right);
    float height=title ? ImGui::GetFont()->CalcTextSizeA(TextSize(title_size),FLT_MAX,width,title).y : 0;
    ImVec2 position(min.x+left,min.y+(description ? Dp(12) : (max.y-min.y-height)*.5f));
    auto *draw=ImGui::GetWindowDrawList();
    draw->PushClipRect(min,ImVec2(std::max(min.x,max.x-right),max.y),true);
    if(title) draw->AddText(ImGui::GetFont(),TextSize(title_size),position,ImGui::GetColorU32(TextColor()),title,nullptr,width);
    if(description) {
        position.y+=height+Dp(8);
        draw->AddText(ImGui::GetFont(),TextSize(description_size),position,ImGui::GetColorU32(MutedColor()),description,nullptr,width);
    }
    draw->PopClipRect();
}
namespace {
ImVec2 At(ImVec2 p, float x, float y) { return ImVec2(p.x+x,p.y+y); }
ImU32 U(ImVec4 color) { return ImGui::GetColorU32(color); }
void Label(ImVec2 p, const char *text, float size, ImVec4 color) {
    if(text) ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),TextSize(size),p,U(color),text);
}
struct PreferenceRow {
    ImVec2 min,max;
    bool pressed;
    PreferenceRow(const char *id,const char *title,const char *description,Icon icon,
                  const char *value,bool reserve_end) {
        min=ImGui::GetCursorScreenPos();
        float available=ImGui::GetContentRegionAvail().x;
        float left=Dp(icon==Icon::None ? 2 : 38), right=reserve_end ? Dp(46) : Dp(4);
        float value_width=value ? std::min(available*.38f,ImGui::GetFont()->CalcTextSizeA(TextSize(13),FLT_MAX,0,value).x) : 0;
        float text_right=right+(value ? value_width+Dp(10) : 0);
        float height=detail::RowTextHeight(title,description,available-left-text_right,description ? 76 : 52);
        pressed=ImGui::InvisibleButton(id,ImVec2(available,height),ImGuiButtonFlags_EnableNav);
        detail::AllowScroll(); max=ImGui::GetItemRectMax();
        auto *draw=ImGui::GetWindowDrawList();
        float feedback=detail::InteractionFeedback(.05f,.12f);
        if(feedback>0) {
            auto color=GetTheme().accent; color.w=feedback;
            draw->AddRectFilled(min,max,U(color),Dp(GetTheme().tokens.row_radius));
        }
        if(icon!=Icon::None) DrawIcon(icon,At(min,Dp(17),height/2),Dp(22),U(TextColor()));
        if(value) {
            float width=value_width;
            draw->PushClipRect(ImVec2(max.x-right-width,min.y),ImVec2(max.x-right,max.y),true);
            Label(ImVec2(max.x-right-width,min.y+(height-TextSize(13))/2),value,13,MutedColor());
            draw->PopClipRect();
        }
        detail::RowText(min,max,title,description,left,text_right);
    }
};
void PaintChoice(const PreferenceRow &row,bool selected,bool radio) {
    float a=detail::Animate("selected",selected ? 1.f : 0.f);
    auto center=ImVec2(row.max.x-Dp(17),(row.min.y+row.max.y)/2);
    auto *draw=ImGui::GetWindowDrawList();
    if(radio) {
        draw->AddCircle(center,Dp(11),U(ImLerp(MutedColor(),GetTheme().accent,a)),32,Dp(2));
        if(a>0) draw->AddCircleFilled(center,Dp(6)*a,U(GetTheme().accent),32);
    } else {
        auto color=ImLerp(MutedColor(),GetTheme().accent,a);
        draw->AddRect(At(center,-Dp(11),-Dp(11)),At(center,Dp(11),Dp(11)),U(color),Dp(7),Dp(2));
        if(a>0) {
            color.w*=a;
            draw->AddRectFilled(At(center,-Dp(11),-Dp(11)),At(center,Dp(11),Dp(11)),U(color),Dp(7));
            DrawIcon(Icon::Check,center,Dp(12+8*a),U(ImVec4(1,1,1,a)));
        }
    }
}
bool Navigation(const char *id,const NavItem *items,int count,int *selected,bool rail) {
    if(!items || !selected || count<=0) return false;
    ImGui::PushID(id); ImGui::BeginGroup();
    float width=rail ? std::min(Dp(172),ImGui::GetContentRegionAvail().x)
                     : std::max(Dp(20),(ImGui::GetContentRegionAvail().x-Dp(4)*(count-1))/count);
    bool changed=false;
    for(int i=0;i<count;++i) {
        if(i && !rail) ImGui::SameLine(0,Dp(4));
        ImGui::PushID(i); auto p=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("item",ImVec2(width,Dp(58)),ImGuiButtonFlags_EnableNav)) { changed=*selected!=i; *selected=i; }
        float a=detail::Animate("selected",*selected==i ? 1.f : 0.f);
        auto color=ImLerp(MutedColor(),GetTheme().accent,a);
        float feedback=detail::InteractionFeedback(.06f,.18f);
        if(a>0 || feedback>0) {
            auto bg=GetTheme().accent; bg.w=std::max(.12f*a,feedback);
            ImGui::GetWindowDrawList()->AddRectFilled(p,At(p,width,Dp(56)),U(bg),Dp(18));
        }
        DrawIcon(items[i].icon,At(p,rail ? Dp(22) : width/2,Dp(rail ? 28 : 18)),Dp(22),U(color));
        if(rail) Label(At(p,Dp(44),Dp(20)),items[i].label,14,color);
        else {
            auto text=ImGui::GetFont()->CalcTextSizeA(TextSize(11),FLT_MAX,0,items[i].label);
            Label(At(p,(width-text.x)/2,Dp(36)),items[i].label,11,color);
        }
        ImGui::PopID();
    }
    ImGui::EndGroup(); ImGui::PopID(); return changed;
}
} // namespace
bool BeginSurface(const char *id,ImVec2 size,float rounding,ImVec4 color) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,Dp(rounding < 0 ? GetTheme().tokens.card_radius : std::max(0.f,rounding)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(Dp(GetTheme().tokens.surface_padding.x),Dp(GetTheme().tokens.surface_padding.y)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,color.w<0 ? Surface() : color);
    auto active_input=size.y==0 && detail::ContainsActiveInput(id) ? ImGuiChildFlags_AlwaysAutoResize : 0;
    return ImGui::BeginChild(id,size,(size.y==0 ? ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_NavFlattened : 0) | active_input |
        ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}
void EndSurface() { ImGui::EndChild(); ImGui::PopStyleColor(); ImGui::PopStyleVar(2); }
bool BasicComponent(const char *id,const char *title,const char *description,Icon icon,
                    const char *value,bool arrow) {
    PreferenceRow row(id,title,description,icon,value,arrow);
    if(arrow) DrawIcon(Icon::Chevron,ImVec2(row.max.x-Dp(12),(row.min.y+row.max.y)/2),Dp(20),U(MutedColor()));
    return row.pressed;
}
bool ArrowPreference(const char *id,const char *title,const char *description,const char *value) {
    return BasicComponent(id,title,description,Icon::None,value,true);
}
bool SwitchPreference(const char *title,bool *value,const char *description) {
    return value && Toggle(title,value,description);
}
bool CheckboxPreference(const char *title,bool *value,const char *description) {
    if(!value) return false;
    ImGui::PushID(title);
    PreferenceRow row("row",title,description,Icon::None,nullptr,true);
    if(row.pressed) *value=!*value;
    PaintChoice(row,*value,false);
    ImGui::PopID(); return row.pressed;
}
bool RadioButtonPreference(const char *title,int *value,int option,const char *description) {
    if(!value) return false;
    ImGui::PushID(title);
    PreferenceRow row("row",title,description,Icon::None,nullptr,true);
    bool changed=row.pressed && *value!=option;
    if(row.pressed) *value=option;
    PaintChoice(row,*value==option,true);
    ImGui::PopID(); return changed;
}
bool SliderPreference(const char *title,float *value,float minimum,float maximum,
                      const char *description,const char *format) {
    if(!value) return false;
    ImGui::PushID(title); Text(title); if(description) Caption(description);
    bool changed=Slider("##value",value,minimum,maximum,format);
    ImGui::PopID(); return changed;
}
bool RangeSliderPreference(const char *title,float *lower,float *upper,float minimum,float maximum,
                           const char *description) {
    ImGui::PushID(title); Text(title); if(description) Caption(description);
    bool changed=RangeSlider("##value",lower,upper,minimum,maximum);
    ImGui::PopID(); return changed;
}
void AnchoredBadge(const char *label) {
    auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
    auto *draw=ImGui::GetWindowDrawList(); auto red=ImVec4(.9f,.18f,.25f,1);
    auto anchor=ImVec2(max.x-Dp(2),min.y+Dp(4));
    if(!label) draw->AddCircleFilled(anchor,Dp(3),U(red),20);
    else {
        float text=ImGui::GetFont()->CalcTextSizeA(TextSize(10),FLT_MAX,0,label).x;
        float width=std::max(Dp(16),text+Dp(8)),height=Dp(16);
        auto p=At(anchor,-width*.5f,-height*.5f);
        draw->AddRectFilled(p,At(p,width,height),U(red),height/2);
        Label(At(p,(width-text)/2,Dp(2)),label,10,ImVec4(1,1,1,1));
    }
}
bool TopAppBar(const char *id,const char *title,const char *subtitle,bool can_go_back) {
    if(!title) title="";
    ImGui::PushID(id); ImGui::BeginGroup(); auto p=ImGui::GetCursorScreenPos();
    float width=ImGui::GetContentRegionAvail().x,left=0;
    bool back=false;
    if(can_go_back) { back=IconButton("back",Icon::Back); left=Dp(46); }
    float available=std::max(1.f,width-left);
    float title_height=ImGui::GetFont()->CalcTextSizeA(TextSize(24),FLT_MAX,available,title).y;
    float subtitle_height=subtitle ? ImGui::GetFont()->CalcTextSizeA(TextSize(12),FLT_MAX,available,subtitle).y : 0;
    auto *draw=ImGui::GetWindowDrawList();
    draw->AddText(ImGui::GetFont(),TextSize(24),At(p,left,Dp(4)),U(TextColor()),title,nullptr,available);
    if(subtitle) draw->AddText(ImGui::GetFont(),TextSize(12),At(p,left,Dp(14)+title_height),U(MutedColor()),subtitle,nullptr,available);
    float height=std::max(Dp(subtitle ? 64 : 44),title_height+(subtitle ? subtitle_height+Dp(28) : Dp(20)));
    ImGui::SetCursorScreenPos(p); ImGui::Dummy(ImVec2(width,height));
    ImGui::EndGroup(); ImGui::PopID(); return back;
}
bool NavigationBar(const char *id,const NavItem *items,int count,int *selected) {
    return Navigation(id,items,count,selected,false);
}
bool CollapsingTopAppBar(const char *id,const char *title,TopAppBarState *state,const char *subtitle,bool back) {
    if(!state) return TopAppBar(id,title,subtitle,back);
    ImGui::PushID(id);ImGui::BeginGroup();
    float target=std::clamp(state->scroll_offset/std::max(1.f,Dp(state->collapse_range)),0.f,1.f);
    // Scrolling is continuous input. Interpolate geometry directly from its
    // offset instead of restarting a timed animation on every scroll frame.
    state->collapsed_fraction=target;
    float t=state->collapsed_fraction,height=Dp(104-48*t);
    auto p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
    bool clicked=false;
    if(back) {clicked=IconButton("back",Icon::Back);}
    auto color=TextColor();float font=28-8*t,left=back ? 46*t : 0;
    auto *draw=ImGui::GetWindowDrawList();
    draw->PushClipRect(p,At(p,width,height),true);
    Label(At(p,Dp(left),Dp(42-30*t)),title,font,color);
    if(subtitle) {color=MutedColor();color.w*=1-t;Label(At(p,0,Dp(82)),subtitle,12,color);}
    draw->PopClipRect();
    ImGui::SetCursorScreenPos(p);ImGui::Dummy(ImVec2(width,height));
    ImGui::EndGroup();ImGui::PopID();return clicked;
}
bool NavigationRail(const char *id,const NavItem *items,int count,int *selected,bool *expanded) {
    if (!expanded) return Navigation(id,items,count,selected,true);
    if (!items || !selected || count<=0) return false;
    ImGui::PushID(id); ImGui::BeginGroup();
    const auto origin=ImGui::GetCursorScreenPos();
    auto *draw=ImGui::GetWindowDrawList();
    // A fixed toggle target remains reachable while the rail is moving.
    ImGui::SetCursorScreenPos(At(origin,Dp(16),0));
    if (IconButton("toggle",Icon::Sidebar)) *expanded=!*expanded;
    Tooltip(*expanded ? "收起侧栏" : "展开侧栏");
    ImGui::SetCursorScreenPos(At(origin,0,Dp(44)+ImGui::GetStyle().ItemSpacing.y));
    float progress=detail::Animate("expand-progress",*expanded ? 1.f : 0.f,.32f);
    float width=std::min(Dp(72+100*progress),ImGui::GetContentRegionAvail().x);
    auto hint=MutedColor(); hint.w*=progress;
    draw->PushClipRect(origin,At(origin,width,Dp(44)),true);
    Label(At(origin,Dp(60),Dp(14)),*expanded ? "收起侧栏" : "展开侧栏",12,hint);
    draw->PopClipRect();
    bool changed=false;
    for(int i=0;i<count;++i) {
        ImGui::PushID(i);
        auto p=ImVec2(origin.x,ImGui::GetCursorScreenPos().y);
        float height=Dp(64-6*progress);
        ImGui::SetCursorScreenPos(p);
        if(ImGui::InvisibleButton("item",ImVec2(width,height),ImGuiButtonFlags_EnableNav)) {
            changed=*selected!=i; *selected=i;
        }
        float a=detail::Animate("selected",*selected==i ? 1.f : 0.f);
        auto color=ImLerp(MutedColor(),GetTheme().accent,a);
        float feedback=detail::InteractionFeedback(.06f,.18f);
        if(a>0 || feedback>0) {
            auto bg=GetTheme().accent; bg.w=std::max(.12f*a,feedback);
            float inset=Dp(8*(1-progress));
            draw->AddRectFilled(At(p,inset,0),At(p,width-inset,Dp(32+24*progress)),U(bg),Dp(16));
        }
        DrawIcon(items[i].icon,At(p,width*.5f*(1-progress)+Dp(22)*progress,Dp(18+10*progress)),Dp(22),U(color));
        float font=11+3*progress;
        auto text=ImGui::GetFont()->CalcTextSizeA(TextSize(font),FLT_MAX,0,items[i].label);
        float x=(width-text.x)*.5f*(1-progress)+Dp(44)*progress;
        draw->PushClipRect(p,At(p,width,height),true);
        Label(At(p,x,Dp(38-18*progress)),items[i].label,font,color);
        draw->PopClipRect();
        ImGui::PopID();
    }
    ImGui::EndGroup(); ImGui::PopID(); return changed;
}
} // namespace skui
