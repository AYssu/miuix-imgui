#include "skui/skui.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

#define CHECK(x) do {if(!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s (nav=%08X active=%08X)\n",__FILE__,__LINE__,#x,GImGui->NavId,GImGui->ActiveId);std::exit(1);}} while(false)
namespace {
struct Host {
    ImGuiContext *context=ImGui::CreateContext();
    skui::Theme theme;
    bool focus=false;
    Host() {
        auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(900,1200);
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigNavCursorVisibleAlways=true;
        io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures|ImGuiBackendFlags_RendererHasVtxOffset;
        ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
        io.Fonts->AddFontDefault();theme.animations=false;
    }
    ~Host() {
        for(auto *t:ImGui::GetIO().Fonts->TexList) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        ImGui::DestroyContext(context);
    }
    void Draw(const std::function<void()> &draw) {
        ImGui::GetIO().DeltaTime=1.f/60;ImGui::NewFrame();skui::ApplyTheme(theme);
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(700,1100));
        ImGui::Begin("keyboard",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        draw();ImGui::End();ImGui::Render();focus=false;
        for(auto *t:ImGui::GetIO().Fonts->TexList) {
            if(t->Status==ImTextureStatus_WantCreate) {t->SetTexID(t->UniqueID+1);t->SetStatus(ImTextureStatus_OK);}
            if(t->Status==ImTextureStatus_WantUpdates) t->SetStatus(ImTextureStatus_OK);
            if(t->Status==ImTextureStatus_WantDestroy) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        }
    }
    void Tap(const std::function<void()> &draw,ImGuiKey key,bool shift=false,bool alt=false) {
        auto &io=ImGui::GetIO();
        if(shift) io.AddKeyEvent(ImGuiMod_Shift,true);
        if(alt) io.AddKeyEvent(ImGuiMod_Alt,true);
        io.AddKeyEvent(key,true);Draw(draw);
        io.AddKeyEvent(key,false);Draw(draw);
        if(shift) io.AddKeyEvent(ImGuiMod_Shift,false);
        if(alt) io.AddKeyEvent(ImGuiMod_Alt,false);
        Draw(draw);
    }
    void Start(const std::function<void()> &draw) {
        Draw(draw);Draw(draw);focus=true;Draw(draw);Draw(draw);
    }
};
void ButtonsAndFocus() {
    Host host;bool toggle=false,check=false,pressed=false;int radio=0,icons=0,preferences=0,buttons=0,rows=0,swatches=0;
    std::vector<ImGuiID> ids;
    auto draw=[&] {
        ids.clear();if(host.focus) ImGui::SetKeyboardFocusHere();
        if(skui::IconButton("icon",skui::Icon::Heart)) ++icons;ids.push_back(ImGui::GetItemID());
        skui::Toggle("Toggle",&toggle);ids.push_back(ImGui::GetItemID());
        skui::Checkbox("Checkbox",&check);ids.push_back(ImGui::GetItemID());
        skui::Radio("Radio",&radio,1);ids.push_back(ImGui::GetItemID());
        if(skui::ArrowPreference("preference","Preference")) ++preferences;ids.push_back(ImGui::GetItemID());
        ImGui::BeginDisabled();skui::IconButton("disabled",skui::Icon::Close);ImGui::EndDisabled();
        skui::Swatch("swatch",ImVec4(1,0,0,1),false,&pressed);if(pressed) ++swatches;ids.push_back(ImGui::GetItemID());
        if(skui::Button("Button")) ++buttons;ids.push_back(ImGui::GetItemID());
        if(skui::ListItem("row","Row")) ++rows;ids.push_back(ImGui::GetItemID());
    };
    host.Start(draw);CHECK(GImGui->NavId==ids[0]);
    host.Tap(draw,ImGuiKey_Enter);CHECK(icons==1);
    for(int i=1;i<8;++i) {
        host.Tap(draw,ImGuiKey_Tab);CHECK(GImGui->NavId==ids[i]);
        host.Tap(draw,i%2 ? ImGuiKey_Space : ImGuiKey_Enter);
    }
    CHECK(toggle && check && radio==1 && preferences==1 && buttons==1 && rows==1 && swatches==1);
    host.Tap(draw,ImGuiKey_Tab,true);CHECK(GImGui->NavId==ids[6]);
    std::puts("PASS keyboard buttons: Tab/Shift+Tab order, disabled skip, Enter/Space activation for custom controls");
}
void ContainersAndNavigation() {
    Host host;bool toggle=false,check=false;ImGuiID first=0,second=0,third=0;
    auto cards=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        skui::IconButton("before",skui::Icon::Heart);first=ImGui::GetItemID();
        if(skui::BeginCard("one")) {skui::Toggle("Toggle",&toggle);second=ImGui::GetItemID();}skui::EndCard();
        if(skui::BeginSurface("two")) {skui::Checkbox("Check",&check);third=ImGui::GetItemID();}skui::EndSurface();
    };
    host.Start(cards);CHECK(GImGui->NavId==first);
    host.Tap(cards,ImGuiKey_Tab);CHECK(GImGui->NavId==second);host.Tap(cards,ImGuiKey_Enter);CHECK(toggle);
    host.Tap(cards,ImGuiKey_Tab);CHECK(GImGui->NavId==third);host.Tap(cards,ImGuiKey_Space);CHECK(check);
    host.Tap(cards,ImGuiKey_Tab,true);CHECK(GImGui->NavId==second);
    for(int kind=0;kind<3;++kind) {
        int selected=0;const skui::NavItem items[]={{"First",skui::Icon::Heart},{"Second",skui::Icon::Settings}};
        const char *labels[]={"First","Second"};
        auto draw=[&] {
            if(host.focus) ImGui::SetKeyboardFocusHere();
            if(kind==0) skui::NavigationBar("nav",items,2,&selected);
            if(kind==1) skui::NavigationRail("nav",items,2,&selected);
            if(kind==2) skui::TabBar("nav",labels,2,&selected);
        };
        host.Start(draw);host.Tap(draw,ImGuiKey_Tab);host.Tap(draw,ImGuiKey_Enter);CHECK(selected==1);
        host.Tap(draw,kind==1 ? ImGuiKey_UpArrow : ImGuiKey_LeftArrow);host.Tap(draw,ImGuiKey_Enter);CHECK(selected==0);
    }
    std::puts("PASS keyboard containers: Tab across cards/surfaces, navigation bar/rail/tabs and directional focus");
}
void NumericControls() {
    Host host;float low=20,high=80;ImGuiID lower=0,upper=0,after=0;
    auto range=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        ImGui::PushID("##range");lower=ImGui::GetID("lower");upper=ImGui::GetID("upper");ImGui::PopID();
        skui::RangeSlider("##range",&low,&high,0,100);
        skui::Button("After");after=ImGui::GetItemID();
    };
    host.Start(range);CHECK(GImGui->NavId==lower);
    host.Tap(range,ImGuiKey_Space);CHECK(GImGui->ActiveId==lower);
    host.Tap(range,ImGuiKey_RightArrow);CHECK(low>20 && high==80);
    host.Tap(range,ImGuiKey_Escape);CHECK(GImGui->ActiveId==0 && GImGui->NavId==lower);
    host.Tap(range,ImGuiKey_Tab);CHECK(GImGui->NavId==upper);
    host.Tap(range,ImGuiKey_Enter);host.Tap(range,ImGuiKey_LeftArrow);CHECK(high<80 && high>=low);
    host.Tap(range,ImGuiKey_Enter);CHECK(GImGui->ActiveId==0);
    host.Tap(range,ImGuiKey_Tab);CHECK(GImGui->NavId==after);
    low=high=50;host.focus=true;host.Draw(range);host.Draw(range);
    host.Tap(range,ImGuiKey_Enter);host.Tap(range,ImGuiKey_RightArrow);CHECK(low<=high);

    int number=2147483640;ImGuiID wheel=0;
    auto picker=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        skui::Button("BeforePicker");
        skui::NumberPicker("##number",&number,-2147483647-1,2147483647);
        auto *parent=ImGui::GetCurrentWindow();
        ImGui::PushID("##number");auto child_id=ImGui::GetID("wheel");ImGui::PopID();
        for(auto *child:parent->DC.ChildWindows) if(child->ChildId==child_id) wheel=child->GetID("##wheel-drag");
        skui::Button("Next");
    };
    host.Start(picker);host.Tap(picker,ImGuiKey_Tab);
    CHECK(wheel && GImGui->NavId==wheel);
    host.Tap(picker,ImGuiKey_Enter);host.Tap(picker,ImGuiKey_UpArrow);CHECK(number==2147483641);
    host.Tap(picker,ImGuiKey_DownArrow);CHECK(number==2147483640);
    host.Tap(picker,ImGuiKey_UpArrow,true);CHECK(number==2147483647);
    host.Tap(picker,ImGuiKey_Escape);CHECK(GImGui->ActiveId==0);
    number=-10;
    auto stepped=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        skui::Button("BeforePicker");skui::NumberPicker("##number",&number,-10,11,3);
    };
    host.Draw(stepped);host.Draw(stepped);CHECK(GImGui->NavId==wheel);
    host.Tap(stepped,ImGuiKey_Enter);CHECK(GImGui->ActiveId==wheel);
    host.Tap(stepped,ImGuiKey_UpArrow);CHECK(number==-7);
    host.Tap(stepped,ImGuiKey_DownArrow);host.Tap(stepped,ImGuiKey_DownArrow);CHECK(number==-10);
    std::puts("PASS keyboard numbers: separate range handles, activation/cancel, no crossing and exact large integer picker steps");
}
void PaletteGrid() {
    Host host;ImVec4 color(1,1,1,.37f);ImGuiID grid=0;
    auto draw=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        ImGui::PushID("palette");grid=ImGui::GetID("grid");ImGui::PopID();
        skui::ColorPalette("palette",&color);
    };
    host.Start(draw);CHECK(GImGui->NavId==grid);
    host.Tap(draw,ImGuiKey_Enter);auto old=color;
    host.Tap(draw,ImGuiKey_DownArrow);CHECK(color.x!=old.x || color.y!=old.y || color.z!=old.z);
    CHECK(std::abs(color.w-.37f)<.00001f);
    host.Tap(draw,ImGuiKey_LeftArrow);CHECK(color.w==.37f);
    host.Tap(draw,ImGuiKey_Escape);CHECK(GImGui->ActiveId==0);
    host.Tap(draw,ImGuiKey_Tab);CHECK(GImGui->NavId!=grid);
    std::puts("PASS keyboard palette: one grid focus stop, arrows adjust color, alpha preserved, Escape and Tab leave edit");
}
void PickerPointerOwnership() {
    Host host;host.theme.animations=true;int value=100;ImVec2 wheel,button;ImGuiID button_id=0;
    auto draw=[&] {
        skui::NumberPicker("##picker",&value,0,1000);
        auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();wheel=ImVec2((min.x+max.x)/2,(min.y+max.y)/2);
        skui::Button("Hold another control");min=ImGui::GetItemRectMin();max=ImGui::GetItemRectMax();
        button=ImVec2((min.x+max.x)/2,(min.y+max.y)/2);button_id=ImGui::GetItemID();
    };
    host.Draw(draw);host.Draw(draw);auto &io=ImGui::GetIO();io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMousePosEvent(wheel.x,wheel.y);host.Draw(draw);io.AddMouseButtonEvent(0,true);host.Draw(draw);
    io.AddMousePosEvent(wheel.x,wheel.y-70);host.Draw(draw);io.AddMouseButtonEvent(0,false);host.Draw(draw);
    auto released=value;io.AddMousePosEvent(button.x,button.y);host.Draw(draw);
    io.AddMouseButtonEvent(0,true);host.Draw(draw);
    for(int i=0;i<8;++i) {host.Draw(draw);CHECK(GImGui->ActiveId==button_id);}
    CHECK(value>released);io.AddMouseButtonEvent(0,false);host.Draw(draw);
    std::puts("PASS pointer picker: momentum continues without stealing another control's edit ownership");
}
void PopupPriority() {
    for(bool animations:{false,true}) {
        Host host;host.theme.animations=animations;char text[32]="original";ImGuiID trigger=0;
        bool focus_input=false;int actions=0;
        auto draw=[&] {
            if(host.focus) ImGui::SetKeyboardFocusHere();
            if(skui::Button("Open")) skui::OpenDialog("dialog");trigger=ImGui::GetItemID();
            if(skui::BeginDialog("dialog","Dialog")) {
                if(focus_input) {ImGui::SetKeyboardFocusHere();focus_input=false;}
                skui::InputText("Text",text,sizeof(text));
                if(skui::Button("Confirm")) {++actions;skui::CloseDialog();}
                skui::EndDialog();
            }
        };
        host.Start(draw);host.Tap(draw,ImGuiKey_Enter);
        for(int i=0;i<25;++i) host.Draw(draw);
        CHECK(!GImGui->OpenPopupStack.empty());
        focus_input=true;host.Draw(draw);host.Draw(draw);CHECK(GImGui->ActiveId!=0);
        ImGui::GetIO().AddInputCharactersUTF8("changed");host.Draw(draw);
        host.Tap(draw,ImGuiKey_Escape);CHECK(!GImGui->OpenPopupStack.empty() && GImGui->ActiveId==0);
        CHECK(std::string(text)=="original");
        host.Tap(draw,ImGuiKey_Escape);
        if(animations) {
            CHECK(!GImGui->OpenPopupStack.empty());
            // Exit keeps the page blocked and cannot activate its confirm action.
            host.Tap(draw,ImGuiKey_Enter);CHECK(actions==0);
        }
        for(int i=0;i<25;++i) host.Draw(draw);
        CHECK(GImGui->OpenPopupStack.empty() && GImGui->NavId==trigger);
    }
    std::puts("PASS keyboard popup: edit cancellation precedes dismissal, exit is inert and opener focus is restored");
}
void DropdownSelection() {
    Host host;int selected=1;ImGuiID opener=0;
    const skui::ChoiceItem choices[]={{"Disabled",nullptr,skui::Icon::None,false},{"Second"},{"Third"}};
    auto draw=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        skui::Dropdown("Choice",&selected,choices,3);
        ImGui::PushID("Choice");ImGui::PushID("trigger");opener=ImGui::GetID("##item");ImGui::PopID();ImGui::PopID();
    };
    host.Start(draw);CHECK(GImGui->NavId==opener);
    host.Tap(draw,ImGuiKey_Enter);CHECK(!GImGui->OpenPopupStack.empty());
    host.Tap(draw,ImGuiKey_DownArrow);host.Tap(draw,ImGuiKey_Enter);
    CHECK(selected==2 && GImGui->OpenPopupStack.empty() && GImGui->NavId==opener);
    std::vector<skui::ChoiceItem> many(30,{"Choice"});selected=23;
    auto long_list=[&] {
        if(host.focus) ImGui::SetKeyboardFocusHere();skui::Dropdown("Long",&selected,many.data(),30);
    };
    host.Start(long_list);host.Tap(long_list,ImGuiKey_Enter);
    host.Tap(long_list,ImGuiKey_DownArrow);host.Tap(long_list,ImGuiKey_Enter);
    CHECK(selected==24 && GImGui->OpenPopupStack.empty());
    std::puts("PASS keyboard dropdown: selected choice gets initial focus, arrow navigation skips disabled items and restores opener");
}
void RouterPriority() {
    Host host;skui::Router router;skui::RouterView view;char text[32]="original";
    bool focus_input=false;ImGuiID home=0;int clicked=0;
    router.Register("home",[&](const skui::Route &) {
        if(host.focus) ImGui::SetKeyboardFocusHere();
        if(skui::Button("Detail")) {++clicked;router.Push("detail");}home=ImGui::GetItemID();
    });
    router.Register("detail",[&](const skui::Route &) {
        if(skui::Button("Dialog")) skui::OpenDialog("nested");
        if(skui::BeginDialog("nested","Nested")) {skui::Text("Modal");skui::EndDialog();}
        if(focus_input) {ImGui::SetKeyboardFocusHere();focus_input=false;}
        skui::InputText("Text",text,sizeof(text));
        skui::Button("Other");
    });
    router.Reset("home");auto draw=[&] {view.Draw(router,ImVec2(600,900));};
    host.Start(draw);CHECK(GImGui->NavId==home);
    host.Tap(draw,ImGuiKey_Enter);CHECK(router.Current()->path=="detail" && clicked==1);
    for(int i=0;i<3;++i) host.Draw(draw);
    focus_input=true;host.Draw(draw);host.Draw(draw);CHECK(GImGui->ActiveId!=0);
    ImGui::GetIO().AddInputCharactersUTF8("changed");host.Draw(draw);
    host.Tap(draw,ImGuiKey_Escape);CHECK(router.Current()->path=="detail" && GImGui->ActiveId==0);
    // Focus the detail page's first action through Shift+Tab and open its popup.
    host.Tap(draw,ImGuiKey_Tab,true);host.Tap(draw,ImGuiKey_Enter);
    CHECK(!GImGui->OpenPopupStack.empty());
    host.Tap(draw,ImGuiKey_Escape);CHECK(GImGui->OpenPopupStack.empty() && router.Current()->path=="detail");
    host.Tap(draw,ImGuiKey_Escape);CHECK(router.Current()->path=="home");
    host.Draw(draw);host.Draw(draw);CHECK(GImGui->NavId==home);
    host.Tap(draw,ImGuiKey_Enter);CHECK(router.Current()->path=="detail");
    host.Draw(draw);host.Draw(draw);host.Tap(draw,ImGuiKey_LeftArrow,false,true);
    CHECK(router.Current()->path=="home");
    host.Tap(draw,ImGuiKey_Escape);CHECK(router.Depth()==1);
    std::puts("PASS keyboard router: text/popup/page priority, Alt+Left, root boundary and back focus restoration");
}
void NestedRouteFocus() {
    for(bool animations:{false,true}) {
        Host host;host.theme.animations=animations;skui::Router router;skui::RouterView view;int number=5;
        router.Register("home",[&](const skui::Route &) {
            if(host.focus) ImGui::SetKeyboardFocusHere();skui::Button("Before");
            skui::NumberPicker("Picker",&number,0,20);
        });
        router.Register("other",[](const skui::Route &) {skui::Button("Other");});router.Reset("home");
        auto draw=[&] {view.Draw(router,ImVec2(600,900));};
        host.Start(draw);host.Tap(draw,ImGuiKey_Tab);auto wheel=GImGui->NavId;
        host.Tap(draw,ImGuiKey_Enter);CHECK(GImGui->ActiveId==wheel);
        host.Tap(draw,ImGuiKey_Escape);CHECK(GImGui->ActiveId==0);
        router.Push("other");for(int i=0;i<40;++i) host.Draw(draw);
        host.Tap(draw,ImGuiKey_Escape);CHECK(router.Current()->path=="home");
        for(int i=0;i<40;++i) host.Draw(draw);
        CHECK(GImGui->NavId==wheel);
        host.Tap(draw,ImGuiKey_Enter);host.Tap(draw,ImGuiKey_UpArrow);CHECK(number==6);
    }
    std::puts("PASS keyboard nested routes: focus returns to a child picker after immediate and animated navigation");
}
}
int main() {ButtonsAndFocus();ContainersAndNavigation();NumericControls();PaletteGrid();PickerPointerOwnership();PopupPriority();DropdownSelection();RouterPriority();NestedRouteFocus();}
