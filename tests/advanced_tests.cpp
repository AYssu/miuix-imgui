#include "skui/skui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if(!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);} } while(false)
namespace {
struct Frame {
    skui::Theme theme;
    Frame() {
        ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;
        io.DisplaySize=ImVec2(800,900);io.DeltaTime=1.f/60;
        io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures|ImGuiBackendFlags_RendererHasVtxOffset;
        ImGui::GetPlatformIO().Renderer_TextureMaxWidth=4096;
        ImGui::GetPlatformIO().Renderer_TextureMaxHeight=4096;
        io.Fonts->AddFontDefault();theme.animations=false;
    }
    ~Frame() {
        skui::SetWindowRenderHandler(nullptr);skui::SetHapticHandler(nullptr);
        for(auto *t:ImGui::GetIO().Fonts->TexList) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        ImGui::DestroyContext();
    }
    void Draw(const std::function<void()> &fn,float x=-100,float y=-100,bool down=false,bool escape=false) {
        auto &io=ImGui::GetIO();io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,down);io.AddKeyEvent(ImGuiKey_Escape,escape);
        ImGui::NewFrame();skui::ApplyTheme(theme);
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(600,800));
        ImGui::Begin("advanced-tests",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove);
        fn();ImGui::End();ImGui::Render();
        // Simulate the renderer's dynamic-texture protocol without a GPU.
        for(auto *t:io.Fonts->TexList) {
            if(t->Status==ImTextureStatus_WantCreate) {t->SetTexID(static_cast<ImTextureID>(t->UniqueID+1));t->SetStatus(ImTextureStatus_OK);}
            if(t->Status==ImTextureStatus_WantUpdates) t->SetStatus(ImTextureStatus_OK);
            if(t->Status==ImTextureStatus_WantDestroy) {t->SetTexID(ImTextureID_Invalid);t->SetStatus(ImTextureStatus_Destroyed);}
        }
    }
};
void Colors() {
    for(auto space:{skui::ColorSpace::HSV,skui::ColorSpace::OkHSV,skui::ColorSpace::OkLab,skui::ColorSpace::OkLCH})
        for(int r=0;r<=8;++r) for(int g=0;g<=8;++g) for(int b=0;b<=8;++b) {
            ImVec4 input(r/8.f,g/8.f,b/8.f,.37f);
            auto channels=skui::ToColorSpace(input,space),out=skui::FromColorSpace(channels,space);
            for(int c=0;c<3;++c) CHECK(std::isfinite((&channels.x)[c]) && std::abs((&out.x)[c]-(&input.x)[c])<.0001f);
            CHECK(channels.w==input.w && out.w==input.w);
        }
    auto red=skui::ToColorSpace(ImVec4(1,0,0,1),skui::ColorSpace::OkLab);
    CHECK(std::abs(red.x-.62795536)<1e-6 && std::abs(red.y-.22486306)<1e-6 && std::abs(red.z-.12584630)<1e-6);
    auto clipped=skui::FromColorSpace(ImVec4(.7f,.4f,.4f,.2f),skui::ColorSpace::OkLab);
    CHECK(clipped.x<=1 && clipped.y>=0 && clipped.z>=0 && clipped.w==.2f);
    std::puts("PASS perceptual colors: 2916 grid round trips, reference red, neutral boundaries, gamut and alpha");
}
void SpringsAndHeader() {
    CHECK(std::abs(skui::SpringProgress(.2f)-.47938295f)<1e-6);
    CHECK(std::abs(skui::SpringProgress(.5f)-.92323875f)<1e-6);
    CHECK(skui::SpringProgress(0)==0 && skui::SpringProgress(1)==1);
    for(float damping:{.2f,.95f,1.f,1.6f}) for(int i=0;i<100;++i)
        CHECK(std::isfinite(skui::SpringProgress(i/100.f,.8f,damping)));
    CHECK(std::isfinite(skui::SpringProgress(.5f,NAN,NAN)));
    Frame f;f.theme.animations=true;skui::TopAppBarState bar;float height=0;
    auto draw=[&]{skui::CollapsingTopAppBar("top","Title",&bar,"Subtitle",true);height=ImGui::GetItemRectSize().y;};
    f.Draw(draw);f.Draw(draw);CHECK(height==104 && bar.collapsed_fraction==0);
    bar.scroll_offset=32;f.Draw(draw);CHECK(height==80 && bar.collapsed_fraction==.5f);
    bar.scroll_offset=64;f.Draw(draw);
    CHECK(height==56 && bar.collapsed_fraction==1);
    bar.scroll_offset=16;f.Draw(draw);CHECK(height==92);
    bar.scroll_offset=0;
    f.theme.animations=false;f.Draw(draw);CHECK(height==104);
    std::puts("PASS motion: official spring reference, damping branches, collapsing header reversal and reduced motion");
}
struct Layers {int frames=0,ends=0,vertices=0;bool accept=true;};
bool Submit(ImGuiID id,ImDrawData *data,void *user) {
    auto &s=*static_cast<Layers *>(user);
    if(!data) {CHECK(id==0);++s.ends;return true;}
    CHECK(id!=0 && data->Valid && data->TotalVtxCount>0 && data->DisplaySize.x==800);
    ++s.frames;s.vertices+=data->TotalVtxCount;
    for(auto *list:data->CmdLists) for(const auto &cmd:list->CmdBuffer)
        for(unsigned i=0;i<cmd.ElemCount;++i) CHECK(cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+i]<static_cast<unsigned>(list->VtxBuffer.Size));
    return s.accept;
}
void Windows() {
    for(int kind=0;kind<9;++kind) {
        Frame f;f.theme.animations=kind==7;Layers layer;skui::SetWindowRenderHandler(Submit,&layer);CHECK(skui::HasWindowBackend());
        int selected=0;bool first=true;ImVec2 trigger{};
        const skui::ChoiceItem choices[]={{"one"},{"two"},{"three"}};
        const skui::PopupItem children[]={{4,"nested"},{5,"second"}};
        const skui::PopupItem items[]={{1,"first"},{-1,"more",skui::Icon::None,true,false,false,children,2},{2,"third"}};
        auto draw=[&] {
            if(kind<2) {
                if(kind==0) skui::WindowDropdownPreference("select",&selected,choices,3);
                else skui::WindowSpinnerPreference("select",&selected,choices,3);
            } else if(kind<5) {
                if(kind==2) skui::WindowDropdownMenu("menu",items,3);
                if(kind==3) skui::WindowIconDropdownMenu("menu",items,3);
                if(kind==4) skui::WindowIconCascadingDropdownMenu("menu",items,3);
            } else {
                skui::Button("trigger");
                if(first) skui::OpenDialog("panel");
                if(kind==5) skui::WindowListPopup("panel",items,3);
                if(kind==6) skui::WindowCascadingListPopup("panel",items,3);
                if(kind==7 && skui::BeginWindowDialog("panel","Dialog")) {skui::Text("content");skui::EndWindowDialog();}
                if(kind==8 && skui::BeginWindowBottomSheet("panel","Sheet",240)) {skui::Text("content");skui::EndWindowBottomSheet();}
            }
            first=false;auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();trigger=ImVec2((a.x+b.x)/2,(a.y+b.y)/2);
        };
        f.Draw(draw);f.Draw(draw);
        if(kind<5) {auto p=trigger;f.Draw(draw,p.x,p.y);f.Draw(draw,p.x,p.y,true);f.Draw(draw,p.x,p.y,false);f.Draw(draw);}
        CHECK(!GImGui->OpenPopupStack.empty());
        // A rejected native submission preserves every main draw list for fallback.
        layer.accept=false;auto *main=ImGui::GetDrawData();int original=main->TotalVtxCount;
        skui::RenderWindowLayers(main);CHECK(layer.frames>0 && main->TotalVtxCount==original && layer.ends==1);
        layer.accept=true;layer.vertices=0;skui::RenderWindowLayers(main);
        CHECK(layer.vertices>0 && main->TotalVtxCount==original-layer.vertices && main->CmdListsCount==main->CmdLists.Size);
        int sum=0;for(auto *list:main->CmdLists) sum+=list->VtxBuffer.Size;CHECK(sum==main->TotalVtxCount);
        f.Draw(draw,-100,-100,false,true);f.Draw(draw);f.Draw(draw);
        if(kind==7) {
            int retained=layer.frames;skui::RenderWindowLayers(ImGui::GetDrawData());
            CHECK(layer.frames>retained); // Native surface survives the exit animation.
            for(int i=0;i<30;++i) f.Draw(draw);
        }
        int submitted=layer.frames;skui::RenderWindowLayers(ImGui::GetDrawData());
        CHECK(layer.frames==submitted && GImGui->OpenPopupStack.empty());
    }
    Frame f;CHECK(!skui::HasWindowBackend());bool first=true;
    auto fallback=[&]{if(first) skui::OpenDialog("fallback");first=false;if(skui::BeginWindowDialog("fallback","Fallback")) {skui::Text("visible");skui::EndWindowDialog();}};
    f.Draw(fallback);f.Draw(fallback);int count=ImGui::GetDrawData()->TotalVtxCount;
    skui::RenderWindowLayers(ImGui::GetDrawData());CHECK(count>0 && ImGui::GetDrawData()->TotalVtxCount==count);
    std::puts("PASS nine Window components: native-layer split, counts/indices, fallback, dismissal and retirement notification");
}
void Glass() {
    Frame f;auto *atlas=ImGui::GetIO().Fonts;int baseline=0;ImTextureData *material=nullptr;
    auto dummy=[] {skui::Text("warm font atlas");};f.Draw(dummy);baseline=atlas->TexList.Size;
    {
        skui::GlassBackdrop glass;skui::GlassStyle style;style.blur=10;style.refraction=0;style.saturation=1;
        ImVec2 size(160,100);bool reverse=false;
        auto draw=[&] {
            auto *d=ImGui::GetWindowDrawList();auto p=ImGui::GetCursorScreenPos();
            d->AddRectFilled(p,ImVec2(p.x+size.x/2,p.y+size.y),reverse ? IM_COL32(0,0,255,255) : IM_COL32(255,0,0,255));
            d->AddRectFilled(ImVec2(p.x+size.x/2,p.y),ImVec2(p.x+size.x,p.y+size.y),reverse ? IM_COL32(255,0,0,255) : IM_COL32(0,0,255,255));
            CHECK(glass.Capture(d,p,ImVec2(p.x+size.x,p.y+size.y)));
            CHECK(glass.Draw(p,ImVec2(p.x+size.x,p.y+size.y),style));ImGui::Dummy(size);
        };
        f.Draw(draw);CHECK(atlas->TexList.Size==baseline+1);material=atlas->TexList.back();CHECK(material->Width==160 && material->Height==100);
        auto sample=[&](int x,int channel){return static_cast<unsigned char *>(material->GetPixelsAt(x,50))[channel];};
        CHECK(sample(20,0)>240 && sample(140,2)>240);
        CHECK(sample(80,0)>20 && sample(80,2)>20); // Actual blur mixes the sharp boundary.
        reverse=true;f.Draw(draw);CHECK(atlas->TexList.Size==baseline+1 && sample(20,2)>240);
        size=ImVec2(250,100);f.Draw(draw);CHECK(material->WantDestroyNextFrame);
        material=atlas->TexList.back();CHECK(material->Width==192);
        f.Draw(draw);f.Draw(draw);
    }
    CHECK(material->WantDestroyNextFrame);f.Draw(dummy);f.Draw(dummy);
    CHECK(atlas->TexList.Size==baseline);
    std::puts("PASS Glass: sampled background, blurred boundary, same-frame update, bounded resolution, resize and resource retirement");
}
void Haptics() {
    Frame f;int events=0;skui::SetHapticHandler([](skui::Haptic,void *p){++*static_cast<int *>(p);},&events);
    skui::ApplyTheme(f.theme);skui::PerformHaptic(skui::Haptic::Selection);CHECK(events==1);
    ImGui::GetIO().AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    auto draw=[] {skui::Button("touch");};f.Draw(draw);f.Draw(draw,70,35);f.Draw(draw,70,35,true);CHECK(events==2);
    f.Draw(draw,70,35,true);f.Draw(draw,70,35,false);CHECK(events==2);
    f.theme.haptics=false;f.Draw(draw,70,35,true);skui::PerformHaptic(skui::Haptic::Confirm);CHECK(events==2);
    skui::SetHapticHandler(nullptr);skui::PerformHaptic(skui::Haptic::Press);CHECK(events==2);
    std::puts("PASS haptic hook: touch activation exactly once, selection event, opt-out and callback teardown");
}
void PerceptualPicker() {
    for(auto space:{skui::ColorSpace::OkHSV,skui::ColorSpace::OkLab,skui::ColorSpace::OkLCH}) {
        Frame f;ImVec4 color(1,0,0,.37f);ImVec2 min{},max{},preview{};float knob=0;bool changed=false,preview_matches=false;
        auto draw=[&] {
            preview=ImGui::GetCursorScreenPos();preview.y+=ImGui::GetTextLineHeight()+ImGui::GetStyle().ItemSpacing.y;
            changed=skui::ColorPicker("perceptual",&color,space,false);
            min=ImGui::GetItemRectMin();max=ImGui::GetItemRectMax();knob=0;preview_matches=false;
            auto accent=ImGui::GetColorU32(f.theme.accent);
            auto expected=ImAlphaBlendColors(IM_COL32(204,204,204,255),ImGui::ColorConvertFloat4ToU32(color));
            for(const auto &v:ImGui::GetWindowDrawList()->VtxBuffer) {
                if(v.col==accent && v.pos.y>=min.y && v.pos.y<=max.y) knob=std::max(knob,v.pos.x);
                if(v.col==expected && v.pos.y>=preview.y && v.pos.y<=preview.y+32) preview_matches=true;
            }
        };
        f.Draw(draw);f.Draw(draw);float x=max.x-18,y=(min.y+max.y)/2;
        f.Draw(draw,x,y);f.Draw(draw,x,y,true);x-=70;f.Draw(draw,x,y,true);
        CHECK(changed && preview_matches && color.w==.37f);
        float edited=knob;CHECK(edited>min.x+100);
        f.Draw(draw,x,y,false);f.Draw(draw);CHECK(!changed && std::abs(knob-edited)<.01f);
        // External values still resynchronise the picker after a retained edit.
        color=ImVec4(.2f,.2f,.2f,.37f);f.Draw(draw);CHECK(!changed && color.x==.2f);
    }
    std::puts("PASS perceptual picker: same-frame preview, alpha preservation, gamut/hue slider stability and external synchronization");
}
}
int main() {Colors();SpringsAndHeader();Windows();Glass();Haptics();PerceptualPicker();}
