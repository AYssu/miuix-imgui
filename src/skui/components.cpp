#include "skui/skui.h"
#include "imgui_internal.h"
#include "detail.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstring>

namespace skui {
namespace {
ImU32 U(ImVec4 color) { return ImGui::GetColorU32(color); }
ImVec2 Offset(ImVec2 p, float x, float y) { return ImVec2(p.x + x, p.y + y); }
void ColorPreview(ImDrawList *draw, ImVec2 min, ImVec2 max, ImVec4 color) {
    if(color.w>=1.f)
        draw->AddRectFilled(min,max,U(color),Dp(12));
    else
        ImGui::RenderColorRectWithAlphaCheckerboard(draw,min,max,
            ImGui::ColorConvertFloat4ToU32(color),ImGui::GetStyle().Alpha,Dp(8),ImVec2(0,0),Dp(12));
}
const char *SelectedLabel(int selected, const ChoiceItem *items, int count) {
    return items && selected >= 0 && selected < count ? items[selected].label : "请选择";
}
bool Choice(const ChoiceItem &item, bool selected, int index) {
    ImGui::PushID(index);
    ImGui::BeginDisabled(!item.enabled);
    auto p = ImGui::GetCursorScreenPos();
    float height = Dp(item.description ? 62 : 46);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0,0,0,0));
    bool pressed = ImGui::Selectable("##choice", selected, ImGuiSelectableFlags_NoAutoClosePopups,
                                     ImVec2(0, height));
    ImGui::PopStyleColor(3);
    detail::AllowScroll();
    auto *draw = ImGui::GetWindowDrawList();
    float selection=detail::Animate("selection",selected ? 1.f : 0.f);
    float feedback=detail::InteractionFeedback(.06f,.18f);
    if (selection>0 || feedback>0) {
        auto color = GetTheme().accent;
        color.w = std::max(.12f*selection,feedback);
        draw->AddRectFilled(Offset(p,Dp(2),0), Offset(ImGui::GetItemRectMax(),-Dp(2),0),
                            U(color), Dp(14));
    }
    float left = item.icon == Icon::None ? Dp(12) : Dp(44);
    if (item.icon != Icon::None)
        DrawIcon(item.icon, Offset(p, Dp(22), height / 2), Dp(21), U(TextColor()));
    draw->PushClipRect(p, Offset(p, ImGui::GetItemRectSize().x - Dp(30), height), true);
    draw->AddText(ImGui::GetFont(), Dp(15), Offset(p, left, Dp(item.description ? 9 : 14)),
                  U(TextColor()), item.label);
    if (item.description)
        draw->AddText(ImGui::GetFont(), Dp(11), Offset(p, left, Dp(34)), U(MutedColor()), item.description);
    draw->PopClipRect();
    if (selection>0) {
        auto color=GetTheme().accent; color.w*=selection;
        DrawIcon(Icon::Check, Offset(ImGui::GetItemRectMax(), -Dp(16), -height / 2), Dp(20),
                 U(color));
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    return pressed;
}
void PopupSize(float width, int count) {
    auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(Dp(240), available.x), 0),
                                       ImVec2(std::min(std::max(width, Dp(240)), available.x - Dp(12)),
                                              std::min(Dp(380), available.y - Dp(20))));
}
float ChoicesHeight(const ChoiceItem *items, int count) {
    float height = 0;
    for (int i = 0; i < count; ++i)
        height += Dp(items[i].description ? 62 : 46) + ImGui::GetStyle().ItemSpacing.y;
    return std::min(height, std::min(Dp(310), ImGui::GetMainViewport()->WorkSize.y * .6f));
}
bool BeginListPopup(const char *id) {
    // A popup opened inside a card inherits that card's fractional padding,
    // bypassing the global theme. Own and pixel-align popup padding here too.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(std::round(Dp(16)),std::round(Dp(12))));
    bool visible=detail::BeginOverlay(id, detail::OverlayKind::Popup, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar();
    return visible;
}
void PlaceChoices(ImVec2 min, ImVec2 max, float height) {
    auto *viewport = ImGui::GetMainViewport();
    auto origin = viewport->WorkPos, available = viewport->WorkSize;
    float width = std::min(std::clamp(max.x - min.x, Dp(240), Dp(340)), available.x - Dp(16));
    float x = std::clamp(min.x, origin.x + Dp(8),
                         std::max(origin.x + Dp(8), origin.x + available.x - width - Dp(8)));
    height += 2 * ImGui::GetStyle().WindowPadding.y;
    float y = max.y + Dp(4);
    if (y + height > origin.y + available.y)
        y = std::max(origin.y + Dp(8), min.y - height - Dp(4));
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(width, 0));
}
bool Select(const char *label, int *selected, const ChoiceItem *items, int count,
            const char *description, bool dialog) {
    if (!selected || !items || count <= 0) return false;
    ImGui::PushID(label);
    if (ListItem("trigger", label, description, SelectedLabel(*selected, items, count)))
        ImGui::OpenPopup("##choices");
    auto anchor = ImGui::GetItemRectMin();
    float width = ImGui::GetItemRectSize().x;
    bool changed = false;
    if (dialog) {
        if (BeginDialog("##choices", label)) {
            BeginScroll("options", ImVec2(0, ChoicesHeight(items, count)));
            for (int i = 0; i < count; ++i)
                if (Choice(items[i], *selected == i, i)) {
                    changed = *selected != i;
                    *selected = i;
                    detail::CloseOverlay();
                }
            EndScroll();
            if (Button("取消", Tone::Neutral, ImVec2(-1, 0))) CloseDialog();
            EndDialog();
        }
    } else {
        PopupSize(width, count);
        PlaceChoices(anchor, ImGui::GetItemRectMax(), ChoicesHeight(items, count));
        if (BeginListPopup("##choices")) {
            BeginScroll("options", ImVec2(0, ChoicesHeight(items, count)));
            for (int i = 0; i < count; ++i)
                if (Choice(items[i], *selected == i, i)) {
                    changed = *selected != i;
                    *selected = i;
                    detail::CloseOverlay();
                }
            EndScroll();
            detail::EndOverlay();
        }
    }
    ImGui::PopID();
    return changed;
}
int MenuItems(const PopupItem *items, int count, int *submenu) {
    int result = -1;
    for (int i = 0; items && i < count; ++i) {
        const auto &item = items[i];
        ImGui::PushID(i);
        if (item.separator_before && i) ImGui::Separator();
        bool has_children = submenu && item.children && item.child_count > 0;
        if (Choice({item.label, nullptr, item.icon, item.enabled}, item.selected, i)) {
            if (has_children) *submenu = i;
            else {
                result = item.action;
                detail::CloseOverlay();
            }
        }
        if (has_children)
            DrawIcon(Icon::Chevron, Offset(ImGui::GetItemRectMax(), -Dp(16), -Dp(23)),
                     Dp(18), U(MutedColor()));
        ImGui::PopID();
    }
    return result;
}
void BeginFloating(const char *id, ImVec2 position, float opacity, bool inert) {
    position.y+=Dp(14)*(1-opacity);
    ImGui::SetNextWindowPos(position);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*opacity);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Dp(8), Dp(8)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Dp(26));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Surface());
    ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav);
    // Focusing the underlying page brings its root forward. Keep floating
    // controls above that page while preserving popup/modal ordering.
    auto *window = ImGui::GetCurrentWindow();
    if (GImGui->OpenPopupStack.empty()) ImGui::BringWindowToDisplayFront(window);
    else if (auto *popup = GImGui->OpenPopupStack[0].Window)
        ImGui::BringWindowToDisplayBehind(window, popup->RootWindow);
    detail::BeginInert(inert);
}
void EndFloating() {
    detail::EndInert();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
} // namespace

bool Dropdown(const char *label, int *selected, const ChoiceItem *items, int count, const char *description) {
    return Select(label, selected, items, count, description, false);
}
bool Spinner(const char *label, int *selected, const ChoiceItem *items, int count,
             const char *description, bool dialog) {
    return Select(label, selected, items, count, description, dialog);
}
bool MultiSelect(const char *label, bool *selected, const ChoiceItem *items, int count) {
    if (!selected || !items || count <= 0) return false;
    ImGui::PushID(label);
    int checked = 0;
    for (int i = 0; i < count; ++i) checked += selected[i];
    char summary[48];
    std::snprintf(summary, sizeof(summary), "已选择 %d 项", checked);
    if (ListItem("trigger", label, nullptr, summary)) ImGui::OpenPopup("##choices");
    PopupSize(ImGui::GetItemRectSize().x, count);
    PlaceChoices(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ChoicesHeight(items, count) + Dp(60));
    bool changed = false;
    if (BeginListPopup("##choices")) {
        BeginScroll("options", ImVec2(0, ChoicesHeight(items, count)));
        for (int i = 0; i < count; ++i)
            if (Choice(items[i], selected[i], i)) {
                selected[i] = !selected[i];
                changed = true;
            }
        EndScroll();
        if (Button("完成", Tone::Primary, ImVec2(-1, 0))) detail::CloseOverlay();
        detail::EndOverlay();
    }
    ImGui::PopID();
    return changed;
}
int DropdownMenu(const char *label, const PopupItem *items, int count, bool icon_only) {
    ImGui::PushID(label);
    bool pressed = icon_only ? IconButton("trigger", Icon::Settings) : Button(label, Tone::Neutral);
    if (pressed && items && count > 0) ImGui::OpenPopup("##menu");
    int result = ListPopup("##menu", items, count);
    ImGui::PopID();
    return result;
}
int ListPopup(const char *id, const PopupItem *items, int count) {
    auto *state = ImGui::GetStateStorage();
    auto key = ImHashStr("submenu", 0, ImGui::GetID(id));
    PopupSize(Dp(270), count);
    auto anchor=ImGui::GetItemRectMin(),end=ImGui::GetItemRectMax();
    int visible_count=count;
    int current=state->GetInt(key,-1);
    if(items && current>=0 && current<count && items[current].children)
        visible_count=items[current].child_count+1;
    PlaceChoices(anchor,end,std::min(Dp(310),visible_count*(Dp(46)+ImGui::GetStyle().ItemSpacing.y)));
    int result = -1;
    if (BeginListPopup(id)) {
        auto *window=ImGui::GetCurrentWindow();
        auto display_key=ImHashStr("displayed-submenu",0,key);
        auto previous_key=ImHashStr("previous-submenu",0,key);
        auto elapsed_key=ImHashStr("submenu-elapsed",0,key);
        if (ImGui::IsWindowAppearing()) {
            state->SetInt(key,-1); state->SetInt(display_key,-1); state->SetInt(previous_key,-1);
            state->SetFloat(elapsed_key,.24f);
        }
        auto valid=[&](int page) { return items && page>=0 && page<count &&
            items[page].children && items[page].child_count>0; };
        int requested=state->GetInt(key,-1),page=state->GetInt(display_key,-1);
        int previous=state->GetInt(previous_key,-1);
        if(!valid(requested)) requested=-1;
        if(!valid(previous)) previous=-1;
        float elapsed=state->GetFloat(elapsed_key,.24f);
        if(requested!=page) { previous=page;page=requested;elapsed=0; }
        elapsed+=ImGui::GetIO().DeltaTime;
        float t=GetTheme().animations ? std::clamp(elapsed/.24f,0.f,1.f) : 1.f;
        float progress=t*t*(3-2*t);
        state->SetInt(display_key,page);state->SetInt(previous_key,previous);
        state->SetFloat(elapsed_key,elapsed);
        auto begin=ImGui::GetCursorScreenPos(),old_max=window->DC.CursorMaxPos;
        int next=page;
        // Crossfade the two lists in the same bounds. Neither a blank panel nor
        // two side-by-side phone popups are needed; retiring rows cannot act.
        auto render=[&](int index,float alpha,float dx,bool inert) {
            ImGui::SetCursorScreenPos(begin);
            ImGui::BeginGroup();ImGui::PushID(index);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*alpha);
            detail::BeginInert(inert);
            int first=window->DrawList->VtxBuffer.Size;
            int selected=index,action=-1;
            if(valid(index)) {
                ImGui::PushID("back");
                bool back=Choice({"返回",nullptr,Icon::Back},false,0);
                ImGui::PopID();ImGui::Separator();
                action=MenuItems(items[index].children,items[index].child_count,nullptr);
                if(back) selected=-1;
            } else selected=-1,action=MenuItems(items,count,&selected);
            float height=std::max(0.f,window->DC.CursorMaxPos.y-begin.y);
            for(int i=first;i<window->DrawList->VtxBuffer.Size;++i)
                window->DrawList->VtxBuffer[i].pos.x+=dx;
            detail::EndInert();ImGui::PopStyleVar();ImGui::PopID();
            GImGui->GroupStack.back().EmitItem=false;ImGui::EndGroup();
            window->DC.CursorMaxPos=old_max;
            if(!inert) { next=selected;result=action; }
            return height;
        };
        float direction=page>=0 ? 1.f : -1.f;
        float outgoing=0;
        if(progress<1) outgoing=render(previous,1-progress,-Dp(12)*direction*progress,true);
        float incoming=render(page,progress,Dp(12)*direction*(1-progress),progress<1);
        ImGui::Dummy(ImVec2(0,ImLerp(outgoing,incoming,progress)));
        if(progress==1) state->SetInt(key,next);
        detail::EndOverlay();
    }
    return result;
}

bool RangeSlider(const char *label, float *lower, float *upper, float minimum, float maximum) {
    if (!lower || !upper || !(maximum > minimum)) return false;
    ImGui::PushID(label);
    if(std::strncmp(label,"##",2)!=0) Text(label);
    auto p = ImGui::GetCursorScreenPos();
    float width = std::max(Dp(40), ImGui::GetContentRegionAvail().x), r = Dp(12);
    ImGui::InvisibleButton("##range", ImVec2(width, Dp(42)));
    auto *state = ImGui::GetStateStorage();
    auto key = ImGui::GetID("thumb");
    float low = std::clamp(*lower, minimum, maximum);
    float high = std::clamp(*upper, low, maximum);
    auto x = [&](float v) { return p.x + r + (width - 2 * r) * (v - minimum) / (maximum - minimum); };
    float mouse = ImGui::GetIO().MousePos.x;
    if (ImGui::IsItemActivated())
        state->SetInt(key, std::abs(mouse - x(low)) <= std::abs(mouse - x(high)) ? 0 : 1);
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
        float value = minimum + std::clamp((mouse - p.x - r) / (width - 2 * r), 0.f, 1.f) * (maximum - minimum);
        if (state->GetInt(key) == 0) low = std::min(value, high);
        else high = std::max(value, low);
    }
    bool changed = low != *lower || high != *upper;
    *lower = low; *upper = high;
    if (changed) ImGui::MarkItemEdited(ImGui::GetItemID());
    auto *draw = ImGui::GetWindowDrawList();
    float cy = p.y + Dp(21);
    draw->AddLine(ImVec2(p.x + r, cy), ImVec2(p.x + width - r, cy), U(MutedColor()), Dp(5));
    draw->AddLine(ImVec2(x(low), cy), ImVec2(x(high), cy), U(GetTheme().accent), Dp(5));
    draw->AddCircleFilled(ImVec2(x(low), cy), r, U(GetTheme().accent), 24);
    draw->AddCircleFilled(ImVec2(x(high), cy), r, U(GetTheme().accent), 24);
    char values[80];
    std::snprintf(values, sizeof(values), "%.0f — %.0f", low, high);
    Caption(values);
    ImGui::PopID();
    return changed;
}
bool NumberPicker(const char *label, int *value, int minimum, int maximum, int step) {
    if (!value || maximum < minimum || step <= 0) return false;
    ImGui::PushID(label);
    if (std::strncmp(label, "##", 2) != 0) Text(label);
    int original = *value;
    std::int64_t count = (static_cast<std::int64_t>(maximum)-minimum)/step+1;
    double index = (std::clamp<std::int64_t>(*value,minimum,maximum)-minimum)/step;
    float row = Dp(40);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::BeginChild("wheel",ImVec2(0,row*5),0,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto *window = ImGui::GetCurrentWindow();
    auto &state = window->StateStorage;
    window->StateStorage.SetInt(ImHashStr("skui/scroll-axis"),1);
    auto key = ImGui::GetID("last-value"), pos_key = ImGui::GetID("offset");
    auto vel_key = ImGui::GetID("velocity"), snap_key = ImGui::GetID("snapping");
    auto target_key = ImGui::GetID("target"), init_key = ImGui::GetID("initialized");
    bool disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    // Keep only the fractional offset in storage: absolute float indices lose
    // single-step precision for large integer ranges. The public value is exact.
    double position = index+state.GetFloat(pos_key);
    float velocity = state.GetFloat(vel_key);
    bool snapping = state.GetBool(snap_key);
    if (!state.GetBool(init_key) || state.GetInt(key) != *value || disabled || window->Appearing) {
        position = index; velocity = 0; snapping = false;
    }
    state.SetBool(init_key,true);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##wheel-drag",ImVec2(width,row*5));
    bool active = ImGui::IsItemActive(), released = ImGui::IsItemDeactivated();
    bool hovered = ImGui::IsItemHovered();
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    auto &io = ImGui::GetIO();
    float dt = std::clamp(io.DeltaTime,.0001f,.1f);
    double last = static_cast<double>(count-1);
    if (!disabled) {
        if (ImGui::IsItemActivated()) { velocity = 0; snapping = false; }
        if (active && ImGui::IsMouseDragging(0,Dp(3))) {
            float delta = -io.MouseDelta.y/row;
            position = std::clamp(position+delta,0.0,last);
            float blend = 1-std::exp(-30*dt);
            velocity += (std::clamp(delta/dt,-45.f,45.f)-velocity)*blend;
            snapping = false;
        }
        if (released && io.MouseDragMaxDistanceSqr[0] < Dp(3)*Dp(3)) {
            double target = std::clamp(std::round(position+(io.MousePos.y-(p.y+row*2.5f))/row),0.0,last);
            state.SetInt(target_key,static_cast<int>(static_cast<std::int64_t>(minimum)+static_cast<std::int64_t>(target)*step));
            snapping = true; velocity = 0;
        }
        if (hovered && io.MouseWheel != 0) {
            position = std::clamp(position-io.MouseWheel,0.0,last);
            velocity = 0; snapping = false;
        }
        if (!active) {
            if (!GetTheme().animations) { position = snapping ? (static_cast<std::int64_t>(state.GetInt(target_key))-minimum)/step : std::round(position); velocity = 0; snapping = false; }
            else {
                if (!snapping && std::abs(velocity) > .8f) {
                    float decay = std::exp(-6*dt);
                    position += velocity*(1-decay)/6;
                    velocity *= decay;
                    if (position < 0 || position > last) { position = std::clamp(position,0.0,last); velocity = 0; }
                } else {
                    if (!snapping) {
                        auto target_index = static_cast<std::int64_t>(std::round(position));
                        state.SetInt(target_key,static_cast<int>(static_cast<std::int64_t>(minimum)+target_index*step));
                        snapping = true;
                    }
                    double target = (static_cast<std::int64_t>(state.GetInt(target_key))-minimum)/step;
                    // Exact critically damped spring: stable at 30/60/120 Hz.
                    double offset = position-target, impulse = velocity+20*offset;
                    float decay = std::exp(-20*dt);
                    position = target+(offset+impulse*dt)*decay;
                    velocity = (velocity-20*impulse*dt)*decay;
                    if (std::abs(position-target)<.001f && std::abs(velocity)<.01f) {
                        position = target; velocity = 0;
                    }
                }
            }
        }
        index = std::clamp<std::int64_t>(std::llround(position),0,count-1);
    }
    auto *draw = window->DrawList;
    float cx = p.x+width*.5f, cy = p.y+row*2.5f;
    float band = std::min(width,Dp(156));
    auto fill = GetTheme().accent; fill.w = .075f;
    draw->AddRectFilled(ImVec2(cx-band*.5f,cy-row*.5f),
                        ImVec2(cx+band*.5f,cy+row*.5f),U(fill),Dp(14));
    draw->PushClipRect(p,Offset(p,width,row*5),true);
    auto center = static_cast<std::int64_t>(std::llround(position));
    for (std::int64_t i=std::max<std::int64_t>(0,center-3); i<=std::min(count-1,center+3); ++i) {
        float distance = static_cast<float>(i-position);
        float fade = std::clamp(std::abs(distance)/2.5f,0.f,1.f);
        float size = Dp(26)*(1-.2f*fade);
        auto color = TextColor(), muted = MutedColor();
        color.x += (muted.x-color.x)*fade; color.y += (muted.y-color.y)*fade;
        color.z += (muted.z-color.z)*fade; color.w *= (1-fade)*(1-.5f*fade);
        char text[48];
        std::snprintf(text,sizeof(text),"%lld",static_cast<long long>(minimum)+i*step);
        auto extent = ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,0,text);
        draw->AddText(ImGui::GetFont(),size,ImVec2(cx-extent.x*.5f,cy+distance*row-extent.y*.5f),U(color),text);
    }
    draw->PopClipRect();
    *value = static_cast<int>(static_cast<std::int64_t>(minimum)+static_cast<std::int64_t>(index)*step);
    state.SetInt(key,*value); state.SetFloat(pos_key,static_cast<float>(position-index)); state.SetFloat(vel_key,velocity);
    state.SetBool(snap_key,snapping);
    ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::PopID();
    return *value != original;
}
bool ColorPicker(const char *label, ImVec4 *color, bool alpha) {
    if (!color) return false;
    ImGui::PushID(label);
    Text(label);
    auto preview=ImGui::GetCursorScreenPos();
    float preview_width=ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(preview_width,Dp(32)));
    float hue, saturation, brightness;
    ImGui::ColorConvertRGBtoHSV(color->x, color->y, color->z, hue, saturation, brightness);
    auto *state = ImGui::GetStateStorage();
    auto key = ImGui::GetID("hue");
    const float saved_hue=state->GetFloat(key,hue);
    // Hue 1 and 0 are the same red. Preserve the dragged endpoint across RGB
    // conversion so the thumb doesn't jump from right to left after release.
    if (saturation == 0 || brightness == 0 || (hue == 0 && saved_hue == 1)) hue = saved_hue;
    if (brightness == 0) saturation = state->GetFloat(ImGui::GetID("saturation"),saturation);
    bool changed = Slider("色相", &hue, 0, 1, "%.2f", ImGuiSliderFlags_NoRoundToFormat);
    changed = Slider("饱和度", &saturation, 0, 1, "%.2f", ImGuiSliderFlags_NoRoundToFormat) || changed;
    changed = Slider("明度", &brightness, 0, 1, "%.2f", ImGuiSliderFlags_NoRoundToFormat) || changed;
    if (alpha) changed = Slider("透明度", &color->w, 0, 1, "%.2f", ImGuiSliderFlags_NoRoundToFormat) || changed;
    state->SetFloat(key, hue);
    state->SetFloat(ImGui::GetID("saturation"),saturation);
    if (changed) ImGui::ColorConvertHSVtoRGB(hue, saturation, brightness, color->x, color->y, color->z);
    ColorPreview(ImGui::GetWindowDrawList(),preview,Offset(preview,preview_width,Dp(32)),*color);
    ImGui::PopID();
    return changed;
}
bool ColorPicker(const char *label,ImVec4 *color,ColorSpace space,bool alpha) {
    if(space==ColorSpace::HSV) return ColorPicker(label,color,alpha);
    if(!color) return false;
    ImGui::PushID(label);ImGui::PushID(static_cast<int>(space));Text(label);
    auto preview=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(width,Dp(32)));
    auto channels=ToColorSpace(*color,space);auto *state=ImGui::GetStateStorage();
    const char *rgba_keys[]={"out-r","out-g","out-b","out-a"};
    const char *channel_keys[]={"channel-0","channel-1","channel-2","channel-3"};
    bool own_value=state->GetBool(ImGui::GetID("color-initialized"));
    for(int i=0;i<4;++i) own_value=own_value && state->GetFloat(ImGui::GetID(rgba_keys[i]))==(&color->x)[i];
    // Keep edited coordinates when gamut clipping loses information. Rebuilding
    // a/b/C from clipped sRGB every frame would move the slider under the finger.
    if(own_value) for(int i=0;i<4;++i) (&channels.x)[i]=state->GetFloat(ImGui::GetID(channel_keys[i]));
    if(space==ColorSpace::OkHSV) {
        if(channels.y<1e-5f || channels.z<1e-5f) channels.x=state->GetFloat(ImGui::GetID("hue"),channels.x);
        if(channels.z<1e-5f) channels.y=state->GetFloat(ImGui::GetID("saturation"),channels.y);
    } else if(space==ColorSpace::OkLCH && channels.y<1e-5f) channels.z=state->GetFloat(ImGui::GetID("hue"),channels.z);
    bool changed=false;
    if(space==ColorSpace::OkHSV) {
        changed=Slider("色相",&channels.x,0,1,"%.3f",ImGuiSliderFlags_NoRoundToFormat);
        changed=Slider("饱和度",&channels.y,0,1,"%.3f",ImGuiSliderFlags_NoRoundToFormat)||changed;
        changed=Slider("明度",&channels.z,0,1,"%.3f",ImGuiSliderFlags_NoRoundToFormat)||changed;
        state->SetFloat(ImGui::GetID("hue"),channels.x);state->SetFloat(ImGui::GetID("saturation"),channels.y);
    } else {
        changed=Slider("感知亮度 L",&channels.x,0,1,"%.3f",ImGuiSliderFlags_NoRoundToFormat);
        changed=Slider(space==ColorSpace::OkLab ? "绿—红 a" : "色度 C",&channels.y,
            space==ColorSpace::OkLab ? -.4f : 0,.4f,"%.3f",ImGuiSliderFlags_NoRoundToFormat)||changed;
        changed=Slider(space==ColorSpace::OkLab ? "蓝—黄 b" : "色相 h",&channels.z,
            space==ColorSpace::OkLab ? -.4f : 0,space==ColorSpace::OkLab ? .4f : 1,"%.3f",ImGuiSliderFlags_NoRoundToFormat)||changed;
        if(space==ColorSpace::OkLCH) state->SetFloat(ImGui::GetID("hue"),channels.z);
    }
    if(alpha) changed=Slider("透明度",&channels.w,0,1,"%.3f",ImGuiSliderFlags_NoRoundToFormat)||changed;
    if(changed) *color=FromColorSpace(channels,space);
    state->SetBool(ImGui::GetID("color-initialized"),true);
    for(int i=0;i<4;++i) {
        state->SetFloat(ImGui::GetID(rgba_keys[i]),(&color->x)[i]);
        state->SetFloat(ImGui::GetID(channel_keys[i]),(&channels.x)[i]);
    }
    ColorPreview(ImGui::GetWindowDrawList(),preview,Offset(preview,width,Dp(32)),*color);
    ImGui::PopID();ImGui::PopID();return changed;
}
bool ColorPalette(const char *id, ImVec4 *color, const ImVec4 *colors, int count) {
    if (!color || !colors || count <= 0) return false;
    ImGui::PushID(id);
    float width = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>((width + ImGui::GetStyle().ItemSpacing.x) /
                                             (Dp(42) + ImGui::GetStyle().ItemSpacing.x)));
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        if (i % columns) ImGui::SameLine();
        ImGui::PushID(i);
        bool pressed;
        bool selected = std::abs(color->x - colors[i].x) + std::abs(color->y - colors[i].y) +
                        std::abs(color->z - colors[i].z) + std::abs(color->w - colors[i].w) < .01f;
        Swatch("color", colors[i], selected, &pressed);
        if (pressed) { *color = colors[i]; changed = !selected; }
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}
bool ColorPalette(const char *id, ImVec4 *color) {
    if(!color) return false;
    ImGui::PushID(id);
    auto *draw=ImGui::GetWindowDrawList();
    float width=std::max(Dp(26),ImGui::GetContentRegionAvail().x), cell=width/13;
    auto preview=ImGui::GetCursorScreenPos(); float preview_height=Dp(36);
    ImGui::Dummy(ImVec2(width,preview_height));
    auto p=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("grid",ImVec2(width,cell*7));
    bool changed=false;
    auto sample=[](int x,int y) {
        float saturation=y<3 ? (y+1)/4.f : 1.f;
        float value=y<3 ? 1.f : 1.f-(y-2)*.18f;
        ImVec4 result(0,0,0,1);
        if(x==12) result.x=result.y=result.z=1.f-y/6.f;
        else ImGui::ColorConvertHSVtoRGB(x/12.f,saturation,value,result.x,result.y,result.z);
        return result;
    };
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
        auto mouse=ImGui::GetIO().MousePos;
        int x=std::clamp(static_cast<int>((mouse.x-p.x)/cell),0,12);
        int y=std::clamp(static_cast<int>((mouse.y-p.y)/cell),0,6);
        auto next=sample(x,y); next.w=color->w;
        changed=next.x!=color->x || next.y!=color->y || next.z!=color->z;
        *color=next;
    }
    float closest=FLT_MAX; ImVec2 indicator{};
    for(int y=0;y<7;++y) for(int x=0;x<13;++x) {
        auto value=sample(x,y);
        auto min=Offset(p,x*cell,y*cell);
        ImDrawFlags corners=ImDrawFlags_RoundCornersNone;
        if(x==0 && y==0) corners=ImDrawFlags_RoundCornersTopLeft;
        if(x==12 && y==0) corners=ImDrawFlags_RoundCornersTopRight;
        if(x==0 && y==6) corners=ImDrawFlags_RoundCornersBottomLeft;
        if(x==12 && y==6) corners=ImDrawFlags_RoundCornersBottomRight;
        draw->AddRectFilled(min,Offset(min,cell+.5f,cell+.5f),U(value),Dp(12),corners);
        float distance=std::abs(value.x-color->x)+std::abs(value.y-color->y)+std::abs(value.z-color->z);
        if(distance<closest) { closest=distance; indicator=Offset(min,cell/2,cell/2); }
    }
    draw->AddCircle(indicator,cell*.32f,U(ImVec4(0,0,0,.6f)),24,Dp(3));
    draw->AddCircle(indicator,cell*.32f,U(ImVec4(1,1,1,1)),24,Dp(1.5f));
    changed=Slider("透明度",&color->w,0,1,"%.2f",ImGuiSliderFlags_NoRoundToFormat) || changed;
    ColorPreview(draw,preview,Offset(preview,width,preview_height),*color);
    ImGui::PopID(); return changed;
}
void CircularProgress(float fraction, float size) {
    size = Dp(std::max(8.f, size));
    auto p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    auto *draw = ImGui::GetWindowDrawList();
    auto center = Offset(p, size / 2, size / 2);
    float radius = size * .4f;
    draw->AddCircle(center, radius, U(MutedColor()), 32, Dp(3));
    float start = fraction < 0 ? static_cast<float>(ImGui::GetTime() * 4) : -IM_PI / 2;
    float end = start + 2 * IM_PI * (fraction < 0 ? .28f : std::clamp(fraction, 0.f, 1.f));
    if (end > start) {
        draw->PathArcTo(center, radius, start, end, 40);
        draw->PathStroke(U(GetTheme().accent), Dp(3));
    }
}
void Tooltip(const char *text) {
    if(!text) return;
    auto &io=ImGui::GetIO();
    bool hovered=ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled);
    if(io.MouseSource==ImGuiMouseSource_TouchScreen) hovered=hovered && ImGui::IsMouseDown(0);
    bool show=hovered || (ImGui::IsItemActive() && ImGui::IsMouseDown(0) && io.MouseDownDuration[0]>.35f);
    ImGui::PushID(static_cast<int>(ImHashStr("tooltip",0,ImGui::GetItemID())));
    float a=detail::Animate("visibility",show ? 1.f : 0.f,.16f,0.f);
    if(a>0) {
        auto *state=ImGui::GetStateStorage();
        auto x=ImGui::GetID("x"),y=ImGui::GetID("y");
        if(!show) ImGui::SetNextWindowPos(ImVec2(state->GetFloat(x),state->GetFloat(y)));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*a);
        if(ImGui::BeginTooltip()) {
        state->SetFloat(x,ImGui::GetWindowPos().x); state->SetFloat(y,ImGui::GetWindowPos().y);
        ImGui::PushTextWrapPos(Dp(250));
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }
    ImGui::PopID();
}
bool BreadcrumbBar(const char *id, const char *const *labels, int count, int *selected) {
    if (!selected || !labels || count <= 0) return false;
    bool changed = false;
    BeginScroll(id, ImVec2(0, Dp(58)), true);
    for (int i = 0; i < count; ++i) {
        if (i) { ImGui::SameLine(); Text(">"); ImGui::SameLine(); }
        ImGui::PushID(i);
        if (Button(labels[i], *selected == i ? Tone::Primary : Tone::Neutral, ImVec2(0, Dp(36)))) {
            changed = *selected != i; *selected = i;
        }
        ImGui::PopID();
    }
    EndScroll();
    return changed;
}
bool FloatingActionButton(const char *id, Icon icon, ImVec2 position, bool visible) {
    ImGui::PushID(id);
    float a=detail::Animate("visibility",visible ? 1.f : 0.f,.26f,0.f);
    ImGui::PopID();
    if(a<=0) return false;
    BeginFloating(id, position, a, !visible);
    auto p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("action", ImVec2(Dp(48), Dp(48)));
    auto color = GetTheme().accent;
    float dim=detail::InteractionFeedback(0.f,.15f);
    color.x*=1-dim; color.y*=1-dim; color.z*=1-dim;
    ImGui::GetWindowDrawList()->AddRectFilled(p, Offset(p, Dp(48), Dp(48)), U(color), Dp(22));
    DrawIcon(icon, Offset(p, Dp(24), Dp(24)), Dp(24), U(ImVec4(1, 1, 1, 1)));
    EndFloating();
    return pressed;
}
int FloatingToolbar(const char *id, const Icon *icons, const char *const *labels, int count, ImVec2 position, bool visible) {
    if (!icons || count <= 0) return -1;
    ImGui::PushID(id);
    float a=detail::Animate("visibility",visible ? 1.f : 0.f,.26f,0.f);
    ImGui::PopID();
    if(a<=0) return -1;
    BeginFloating(id, position, a, !visible);
    int selected = -1;
    for (int i = 0; i < count; ++i) {
        if (i) ImGui::SameLine();
        ImGui::PushID(i);
        if (IconButton("action", icons[i])) selected = i;
        if (labels) Tooltip(labels[i]);
        ImGui::PopID();
    }
    EndFloating();
    return selected;
}
bool BeginRefreshScroll(const char *id, bool refreshing, bool *requested, ImVec2 size) {
    if (requested) *requested = false;
    bool visible = BeginScroll(id, size);
    auto &io = ImGui::GetIO();
    auto *state = ImGui::GetStateStorage();
    auto start_key = ImGui::GetID("pull-start"), pulling_key = ImGui::GetID("pulling");
    bool pulling = state->GetBool(pulling_key);
    if (ImGui::IsMouseClicked(0)) {
        pulling = !(GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) && !refreshing && ImGui::GetScrollY() <= 0 &&
                  ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        state->SetFloat(start_key, io.MousePos.y);
    }
    if (pulling && !detail::CanScroll()) pulling = false;
    float distance = pulling ? std::max(0.f, io.MousePos.y - state->GetFloat(start_key)) : 0;
    if (ImGui::IsMouseReleased(0)) {
        if (requested && !refreshing && distance >= Dp(60)) *requested = true;
        pulling = false;
        distance = 0;
    }
    if (!io.MouseDown[0]) pulling = false;
    state->SetBool(pulling_key, pulling);
    bool released_request=ImGui::IsMouseReleased(0) && requested && *requested;
    float target=refreshing || released_request ? 1.f : pulling ? std::clamp(distance/Dp(80),0.f,1.f) : 0.f;
    float header=detail::Animate("refresh/header",target,pulling ? 0.f : .26f);
    detail::Reveal("refresh/content",header,!pulling && !refreshing,[&] {
        if (pulling) ImGui::ClearActiveID();
        CircularProgress(refreshing ? -1 : std::min(1.f, distance / Dp(60)), 24);
        ImGui::SameLine(); Caption(refreshing ? "正在刷新" : distance >= Dp(60) ? "松开刷新" : "下拉刷新");
        Space(16);
    });
    return visible;
}
bool BeginBottomSheet(const char *id, const char *title, float height) {
    auto *root = ImGui::GetCurrentWindow()->RootWindow;
    auto viewport = ImGui::GetMainViewport();
    ImVec2 min(std::max(root->Pos.x, viewport->WorkPos.x), std::max(root->Pos.y, viewport->WorkPos.y));
    ImVec2 max(std::min(root->Pos.x + root->Size.x, viewport->WorkPos.x + viewport->WorkSize.x),
               std::min(root->Pos.y + root->Size.y, viewport->WorkPos.y + viewport->WorkSize.y));
    ImGui::SetNextWindowPos(ImVec2((min.x + max.x) / 2, max.y), ImGuiCond_Always, ImVec2(.5f, 1));
    ImGui::SetNextWindowSize(ImVec2(std::max(Dp(100), max.x - min.x),
                                  std::clamp(Dp(height), Dp(100), std::max(Dp(100), max.y - min.y))));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Dp(20), Dp(16)));
    bool visible = detail::BeginOverlay(id, detail::OverlayKind::Sheet, ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    if (visible) {
        Heading(title);
        ImGui::SameLine(ImGui::GetWindowWidth() - Dp(58));
        if (IconButton("close", Icon::Close)) detail::CloseOverlay();
        Divider();
    } else ImGui::PopStyleVar();
    return visible;
}
void EndBottomSheet() { detail::EndOverlay(); ImGui::PopStyleVar(); }
} // namespace skui
