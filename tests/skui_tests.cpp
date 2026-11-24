#include "skui/skui.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);                      \
            std::exit(1);                                                                                    \
        }                                                                                                    \
    } while (false)

void NavigationTests() {
    skui::Router router;
    int visits = 0;
    CHECK(!router.Push("/missing"));
    CHECK(!router.Back());
    CHECK(!router.Register("", [](const skui::Route &) {}));
    CHECK(router.Register("/home", [&](const skui::Route &route) {
        ++visits;
        CHECK(route.path == "/home");
        CHECK(router.Push("/detail", "from-home"));
        CHECK(router.Current()->path == "/home"); // committed at frame boundary
    }));
    CHECK(!router.Register("/home", [](const skui::Route &) {}));
    CHECK(router.Register("/detail", [&](const skui::Route &route) {
        ++visits;
        CHECK(route.parameter == "from-home");
        CHECK(router.Back());
        CHECK(router.Current()->path == "/detail");
    }));
    CHECK(router.Reset("/home"));
    router.Draw();
    CHECK(visits == 1 && router.Depth() == 2);
    router.Draw();
    CHECK(visits == 2 && router.Depth() == 1);
    CHECK(!router.Replace("/missing"));
    CHECK(router.Current()->path == "/home");
    CHECK(router.Replace("/detail", "from-home"));
    CHECK(router.Depth() == 1);
    CHECK(router.Reset("/home"));
    CHECK(router.Depth() == 1);
    CHECK(router.Register("/throw", [](const skui::Route &) { throw std::runtime_error("test"); }));
    CHECK(router.Push("/throw"));
    try {
        router.Draw();
        CHECK(false);
    } catch (const std::runtime_error &) {
    }
    CHECK(router.Back());
    CHECK(router.Current()->path == "/home");
    std::puts("PASS navigation: registration, parameters, deferred transitions, back, replace, reset, "
              "exception recovery");
}

struct UiFixture {
    ImVec2 control_min{}, control_max{};
    float scroll = 0, slider = 42;
    bool value = false;
    UiFixture() {
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 900);
        io.DeltaTime = 1.f / 60;
        io.Fonts->AddFontDefault();
        unsigned char *pixels;
        int w, h;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    }
    ~UiFixture() { ImGui::DestroyContext(); }
    void Frame(float x = -100, float y = -100, bool down = false, bool disabled = false) {
        auto &io = ImGui::GetIO();
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        skui::Theme theme;
        theme.scale = 1;
        skui::ApplyTheme(theme);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(600, 800));
        ImGui::Begin("test", nullptr, ImGuiWindowFlags_NoTitleBar);
        ImGui::BeginDisabled(disabled);
        skui::Toggle("test-toggle", &value);
        control_min = ImGui::GetItemRectMin();
        control_max = ImGui::GetItemRectMax();
        ImGui::EndDisabled();
        ImGui::End();
        ImGui::Render();
        CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
    }
    void ScrollFrame(float x = -100, float y = -100, bool down = false) {
        auto &io = ImGui::GetIO();
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        skui::ApplyTheme(skui::Theme{});
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(600, 800));
        ImGui::Begin("scroll-test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        skui::BeginScroll("page", ImVec2(0, 300));
        scroll = ImGui::GetScrollY();
        skui::BeginCard("card");
        skui::Toggle("toggle", &value);
        skui::Slider("slider", &slider, 0, 100);
        skui::Space(900);
        skui::EndCard();
        skui::EndScroll();
        ImGui::End();
        ImGui::Render();
    }
};
void ComponentTests() {
    UiFixture ui;
    ui.Frame();
    float x = (ui.control_min.x + ui.control_max.x) * .5f, y = (ui.control_min.y + ui.control_max.y) * .5f;
    ui.Frame(x, y);
    ui.Frame(x, y, true);
    CHECK(!ui.value);
    ui.Frame(x, y, false);
    CHECK(ui.value);
    ui.Frame(x, y, false, true);
    ui.Frame(x, y, true, true);
    ui.Frame(x, y, false, true);
    CHECK(ui.value);
    // A press released outside must not trigger a control.
    ui.Frame(x, y);
    ui.Frame(x, y, true);
    ui.Frame(750, 850, false);
    CHECK(ui.value);
    skui::Theme theme;
    theme.dark = true;
    theme.scale = 2;
    skui::ApplyTheme(theme);
    CHECK(std::abs(skui::Dp(20) - 40) < .01f);
    CHECK(skui::Background().x < .1f);
    ImGui::GetStyle().FontSizeBase=18;
    ImGui::GetStyle().FontScaleMain=1.25f;
    ImGui::GetStyle().FontScaleDpi=2;
    skui::ApplyTheme(theme);
    CHECK(ImGui::GetStyle().FontSizeBase==18);
    CHECK(ImGui::GetStyle().FontScaleMain==1.25f && ImGui::GetStyle().FontScaleDpi==2);
    CHECK(std::abs(ImGui::GetStyle().FramePadding.x - 28) < .01f); // no repeated scaling
    std::puts("PASS components: draw data, toggle release, disabled interaction, cancelled click, dark "
              "theme, repeatable scale");
}
void ScrollTests() {
    UiFixture ui;
    ui.ScrollFrame();
    ui.ScrollFrame();
    ui.ScrollFrame(200, 240);
    ui.ScrollFrame(200, 240, true);
    ui.ScrollFrame(200, 140, true);
    CHECK(ui.scroll >= 99); // the current frame must already follow the finger
    ui.ScrollFrame(200, 80, true);
    CHECK(ui.scroll >= 159);
    ui.ScrollFrame(200, 80, false);
    ui.ScrollFrame();
    CHECK(ui.scroll > 90);
    CHECK(!ui.value);
    CHECK(ui.slider == 42);
    std::puts("PASS scroll: drag on nested card whitespace, controls preserved");
}
void ShellDragTests() {
    UiFixture ui;
    ImVec2 position, content;
    auto frame = [&](float x, float y, bool down) {
        auto &io = ImGui::GetIO();
        io.AddMousePosEvent(x, y); io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame(); skui::ApplyTheme(skui::Theme{});
        skui::ShellOptions options;
        options.initial_position = ImVec2(20, 20);
        options.initial_size = ImVec2(450, 600);
        skui::BeginShell(options);
        position = skui::ShellPosition();
        content = ImGui::GetCursorScreenPos();
        skui::Text("content");
        skui::EndShell(); ImGui::Render();
    };
    frame(-100,-100,false); frame(70,35,false); frame(70,35,true);
    auto before = position, previous_content = content;
    frame(160,105,true);
    CHECK(std::abs(position.x - before.x - 90) < 1);
    CHECK(std::abs(position.y - before.y - 70) < 1);
    CHECK(std::abs(content.x - previous_content.x - 90) < 1);
    CHECK(std::abs(content.y - previous_content.y - 70) < 1);
    frame(210,135,true);
    CHECK(std::abs(position.x - before.x - 140) < 1);
    frame(210,135,false);
    std::puts("PASS shell drag: frame, content and reported position move in the input frame");
}
struct Interaction {
    UiFixture ui;
    bool animations = false;
    ImVec2 min{}, max{};
    void Frame(const std::function<void()> &draw, float x = -100, float y = -100, bool down = false) {
        auto &io = ImGui::GetIO();
        io.AddMousePosEvent(x,y); io.AddMouseButtonEvent(0,down);
        ImGui::NewFrame(); skui::Theme theme; theme.animations = animations; skui::ApplyTheme(theme);
        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(600,800));
        ImGui::Begin("interactions",nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        draw();
        min = ImGui::GetItemRectMin(); max = ImGui::GetItemRectMax();
        ImGui::End(); ImGui::Render();
    }
    void Click(const std::function<void()> &draw, float x, float y) {
        Frame(draw,x,y); Frame(draw,x,y,true); Frame(draw,x,y,false); Frame(draw,x,y);
    }
    ImGuiWindow *Popup() {
        if (GImGui->OpenPopupStack.empty()) return nullptr;
        return GImGui->OpenPopupStack.back().Window;
    }
};
float MarkerX(ImU32 color) {
    float x=FLT_MAX;
    for(auto *list:ImGui::GetDrawData()->CmdLists)
        for(auto &v:list->VtxBuffer) if((v.col&~IM_COL32_A_MASK)==(color&~IM_COL32_A_MASK)) x=std::min(x,v.pos.x);
    return x;
}
bool HasRouteDim(int alpha) {
    for(auto *list:ImGui::GetDrawData()->CmdLists) for(auto &v:list->VtxBuffer)
        if(v.col==IM_COL32(0,0,0,alpha)) return true;
    return false;
}
void RouteViewTests() {
    for(int fps:{30,60,120}) {
        Interaction h; h.animations=true; ImGui::GetIO().DeltaTime=1.f/fps;
        skui::Router router;skui::RouterView view;
        int home_calls=0,detail_calls=0,clicks=0;
        ImVec2 button{};float home_scroll=0;bool scroll_home=false,push_in_page=false;
        router.Register("/home",[&](const skui::Route &) {
            ++home_calls;home_scroll=ImGui::GetScrollY();
            CHECK(ImGui::GetStyle().DisabledAlpha<1.f);
            if(!view.IsTransitioning()) {
                float alpha=ImGui::GetStyle().Alpha;
                ImGui::BeginDisabled();
                CHECK(std::abs(ImGui::GetStyle().Alpha-alpha*ImGui::GetStyle().DisabledAlpha)<.001f);
                ImGui::EndDisabled();
            }
            if(scroll_home) {ImGui::SetScrollY(160);scroll_home=false;}
            auto p=ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p,ImVec2(p.x+30,p.y+30),IM_COL32(255,0,0,255));
            skui::Text("home");skui::Space(900);
            if(push_in_page) {CHECK(router.Push("/detail","inside"));push_in_page=false;}
        });
        router.Register("/detail",[&](const skui::Route &) {
            ++detail_calls;
            auto p=ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p,ImVec2(p.x+30,p.y+30),IM_COL32(0,255,0,255));
            if(skui::Button("action")) ++clicks;
            button=ImGui::GetItemRectMin();
            skui::BeginCard("nested");skui::Text("nested content");skui::EndCard();
        });
        router.Reset("/home");
        auto draw=[&] {view.Draw(router,ImVec2(560,600));};
        h.Frame(draw);h.Frame(draw);CHECK(!view.IsTransitioning());
        scroll_home=true;h.Frame(draw);h.Frame(draw);CHECK(home_scroll==160);
        int home_before=home_calls;
        CHECK(router.Push("/detail","test"));h.Frame(draw);
        CHECK(view.IsTransitioning() && router.Current()->parameter=="test");
        float start=MarkerX(IM_COL32(0,255,0,255));CHECK(start>500 && start<700);
        h.Click(draw,button.x+20,button.y+20);CHECK(clicks==0);
        float moving=MarkerX(IM_COL32(0,255,0,255));CHECK(moving<start);
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        CHECK(!view.IsTransitioning() && home_calls==home_before);
        CHECK(MarkerX(IM_COL32(0,255,0,255))<50);
        h.Click(draw,button.x+20,button.y+20);CHECK(clicks==1);
        int detail_before=detail_calls;
        CHECK(router.Back());h.Frame(draw);CHECK(view.IsTransitioning());
        CHECK(MarkerX(IM_COL32(255,0,0,255))<0);
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        CHECK(!view.IsTransitioning() && home_scroll==160 && detail_calls==detail_before);
        // Navigation committed inside Draw is noticed on the next frame.
        push_in_page=true;h.Frame(draw);CHECK(router.Current()->path=="/detail");
        h.Frame(draw);CHECK(view.IsTransitioning());
        for(int i=0;i<fps/10;++i) h.Frame(draw);
        float u=2*IM_PI/.24f*.1f;
        int dim=static_cast<int>(255*.2f*(1-(1+u)*std::exp(-u)));
        CHECK(HasRouteDim(dim));
        // Interruptions preserve a frozen picture, with one live page callback.
        int before=home_calls+detail_calls;
        CHECK(router.Back());h.Frame(draw);CHECK(view.IsTransitioning());
        CHECK(HasRouteDim(dim)); // the prior composite retains its scrim during reversal
        CHECK(home_calls+detail_calls==before+1);
        CHECK(router.Replace("/detail","replace"));h.Frame(draw);CHECK(view.IsTransitioning());
        CHECK(router.Reset("/home"));h.Frame(draw);CHECK(view.IsTransitioning() && router.Depth()==1);
        h.animations=false;h.Frame(draw);CHECK(!view.IsTransitioning());
        CHECK(router.Push("/detail"));h.Frame(draw);CHECK(!view.IsTransitioning());
        for(auto *list:ImGui::GetDrawData()->CmdLists)
            for(auto &cmd:list->CmdBuffer) for(unsigned i=0;i<cmd.ElemCount;++i)
                CHECK(cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+i] < static_cast<unsigned>(list->VtxBuffer.Size));
    }
    std::puts("PASS route view: forward/back/reset, deferred navigation, scroll retention, inert outgoing pages, interruption, reduced motion at 30/60/120Hz");
}
void RouteMotionReferenceTests() {
    Interaction h;h.animations=true;
    skui::Router router;skui::RouterView view;
    int home_headers=0;bool header_back=false;ImGuiWindow *top=nullptr;ImVec2 origin{};
    router.Register("a",[](const skui::Route &) {skui::Text("home");});
    router.Register("b",[](const skui::Route &) {
        auto p=ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x+80,p.y+100),ImVec2(p.x+100,p.y+120),IM_COL32(0,255,0,255));
    });
    auto draw=[&] {
        origin=ImGui::GetCursorScreenPos();
        view.Draw(router,ImVec2(560,600),[&](const skui::Route &r) {
            top=ImGui::GetCurrentWindow();
            if(r.path=="a") ++home_headers;
            auto p=ImGui::GetCursorScreenPos();
            if(r.path=="b") ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x+80,p.y+10),ImVec2(p.x+100,p.y+30),IM_COL32(0,255,255,255));
            ImGui::Dummy(ImVec2(100,64));
            if(header_back) {CHECK(router.Back());CHECK(router.Current()->path==r.path);header_back=false;}
        });
    };
    router.Reset("a");h.Frame(draw);h.Frame(draw);int before=home_headers;
    router.Push("b");h.Frame(draw);
    for(int i=0;i<6;++i) h.Frame(draw);
    float u=2*IM_PI/.24f*.1f,progress=1-(1+u)*std::exp(-u);
    float dx=std::round(560*(1-progress));
    CHECK(std::abs(MarkerX(IM_COL32(0,255,0,255))-(origin.x+80+dx))<1);
    CHECK(std::abs(MarkerX(IM_COL32(0,255,255,255))-MarkerX(IM_COL32(0,255,0,255)))<1);
    CHECK(home_headers==before && view.IsTransitioning());
    bool dim=false;
    int expected_alpha=static_cast<int>(255*.2f*progress);
    for(auto *list:ImGui::GetDrawData()->CmdLists) for(auto &v:list->VtxBuffer)
        if((v.col&~IM_COL32_A_MASK)==0 && std::abs(static_cast<int>((v.col>>IM_COL32_A_SHIFT)&255)-expected_alpha)<=1) dim=true;
    CHECK(dim);
    // Only referenced geometry counts: the discarded square corner must be absent.
    bool top_edge=false;
    for(auto &cmd:top->DrawList->CmdBuffer) for(unsigned i=0;i<cmd.ElemCount;++i) {
        auto &v=top->DrawList->VtxBuffer[cmd.VtxOffset+top->DrawList->IdxBuffer[cmd.IdxOffset+i]];
        if(v.pos.y<=origin.y+.1f) {CHECK(v.pos.x>=origin.x+dx+28*1.2f-.1f);top_edge=true;}
    }
    CHECK(top_edge);
    for(int i=0;i<14;++i) h.Frame(draw);CHECK(view.IsTransitioning()); // 333ms is still settling
    for(int i=0;i<10;++i) h.Frame(draw);CHECK(!view.IsTransitioning()); // 500ms
    router.Register("throw",[](const skui::Route &) {throw std::runtime_error("page failed");});
    router.Push("throw");bool caught=false;
    h.Frame([&] {
        int styles=GImGui->StyleVarStack.Size;
        try {draw();} catch(const std::runtime_error &) {caught=true;}
        CHECK(GImGui->StyleVarStack.Size==styles && std::strcmp(ImGui::GetCurrentWindow()->Name,"interactions")==0);
        CHECK(!(GImGui->CurrentItemFlags&ImGuiItemFlags_Disabled));
    });
    CHECK(caught);router.Back();h.Frame(draw);CHECK(!view.IsTransitioning());
    header_back=true;h.Frame(draw);CHECK(router.Current()->path=="a");
    h.Frame(draw);CHECK(view.IsTransitioning());
    std::puts("PASS Miuix motion: 500ms critical damping, shared header/body movement, 20% covered scrim, leading squircle clip");
}
void RouteClipIndexTests() {
    for(bool offsets:{false,true}) {
        Interaction h;h.animations=true;
        if(offsets) ImGui::GetIO().BackendFlags|=ImGuiBackendFlags_RendererHasVtxOffset;
        skui::Router router;skui::RouterView view;
        router.Register("a",[](const skui::Route &) {skui::Text("home");});
        router.Register("b",[&](const skui::Route &) {
            auto *draw=ImGui::GetWindowDrawList();auto p=ImGui::GetCursorScreenPos();
            draw->AddImage((ImTextureID)42,p,ImVec2(p.x+560,p.y+600));
            for(int i=0;i<(offsets ? 17000 : 1000);++i)
                draw->AddRectFilled(ImVec2(p.x+80,p.y+120),ImVec2(p.x+84,p.y+124),IM_COL32_WHITE);
        });
        auto draw=[&] {view.Draw(router,ImVec2(560,600));};
        auto check=[] {
            for(auto *list:ImGui::GetDrawData()->CmdLists) for(auto &cmd:list->CmdBuffer)
                for(unsigned i=0;i<cmd.ElemCount;++i) {
                    unsigned index=cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+i];
                    CHECK(index<static_cast<unsigned>(list->VtxBuffer.Size));
                    auto &v=list->VtxBuffer[index];CHECK(std::isfinite(v.uv.x) && std::isfinite(v.uv.y));
                    CHECK(v.uv.x>=0 && v.uv.x<=1 && v.uv.y>=0 && v.uv.y<=1);
                }
        };
        router.Reset("a");h.Frame(draw);h.Frame(draw);
        router.Push("b");h.Frame(draw);h.Frame(draw);check();
        for(int i=0;i<31;++i) h.Frame(draw);
        router.Back();h.Frame(draw);h.Frame(draw);check();
    }
    std::puts("PASS route clipping: image UVs, 16-bit indices, vertex offsets and >64K vertices on forward/back");
}
void SelectionTests() {
    Interaction h;
    const skui::ChoiceItem items[] = {{"first"}, {"disabled",nullptr,skui::Icon::None,false},
                                     {"third", "description", skui::Icon::Sun}};
    int selected = 0;
    auto draw = [&] { skui::Dropdown("choose", &selected, items, 3); };
    h.Frame(draw); h.Frame(draw);
    float x = (h.min.x+h.max.x)/2, y=(h.min.y+h.max.y)/2;
    h.Click(draw,x,y);
    CHECK(h.Popup());
    auto origin = h.Popup()->DC.CursorStartPos;
    float option_y = origin.y + 46 + ImGui::GetStyle().ItemSpacing.y + 23;
    h.Click(draw,origin.x+80,option_y);
    CHECK(selected == 0 && h.Popup());
    option_y = origin.y + 2*(46+ImGui::GetStyle().ItemSpacing.y)+31;
    h.Click(draw,origin.x+80,option_y);
    CHECK(selected == 2 && !h.Popup());
    h.Click(draw,x,y); CHECK(h.Popup());
    h.Click(draw,700,850); CHECK(!h.Popup());
    bool values[3] = {false,false,false};
    auto multi = [&] { skui::MultiSelect("multiple",values,items,3); };
    h.Frame(multi); h.Frame(multi);
    x=(h.min.x+h.max.x)/2; y=(h.min.y+h.max.y)/2;
    h.Click(multi,x,y); CHECK(h.Popup());
    origin=h.Popup()->DC.CursorStartPos;
    h.Click(multi,origin.x+80,origin.y+23);
    CHECK(values[0] && h.Popup());
    h.Click(multi,origin.x+80,origin.y+23);
    CHECK(!values[0] && h.Popup());
    h.Click(multi,700,850); CHECK(!h.Popup());
    std::puts("PASS floating selection: choose, disabled option, outside dismissal and persistent multiselect");
}
void RangeAndPickerTests() {
    Interaction h;
    float low=20, high=80;
    auto range = [&] { skui::RangeSlider("range",&low,&high,0,100); };
    h.Frame(range); h.Frame(range);
    float x=20+12+(560-24)*.2f, y=14+ImGui::GetFontSize()+8+21;
    h.Frame(range,x,y); h.Frame(range,x,y,true); h.Frame(range,300,y,true);
    CHECK(low > 45 && low < 55 && high == 80);
    h.Frame(range,580,y,true); CHECK(low == high && high == 80);
    h.Frame(range,580,y,false);
    auto disabled = [&] { ImGui::BeginDisabled(); range(); ImGui::EndDisabled(); };
    h.Click(disabled,120,y); CHECK(low == 80 && high == 80);
    int number=5;
    float outer_scroll=0;
    auto picker = [&] {
        skui::BeginScroll("outer",ImVec2(0,350)); outer_scroll=ImGui::GetScrollY();
        skui::NumberPicker("number",&number,0,30); skui::Space(600); skui::EndScroll();
    };
    h.Frame(picker); h.Frame(picker);
    auto *wheel = ImGui::FindWindowByName("interactions/wheel_00000000");
    for (auto *window : GImGui->Windows)
        if (std::strstr(window->Name,"/wheel_")) wheel=window;
    CHECK(wheel);
    auto p=wheel->InnerRect.Min;
    h.Frame(picker,p.x+100,p.y+120);
    h.Frame(picker,p.x+100,p.y+120,true);
    h.Frame(picker,p.x+100,p.y+48,true);
    CHECK(number >= 7);
    CHECK(outer_scroll==0); // the inner wheel must own this gesture
    h.Frame(picker,p.x+100,p.y+48,false);
    number=30; h.Frame(picker); h.Frame(picker); CHECK(number==30);
    auto disabled_picker=[&] { ImGui::BeginDisabled(); picker(); ImGui::EndDisabled(); };
    h.Frame(disabled_picker,p.x+100,p.y+48); h.Frame(disabled_picker,p.x+100,p.y+48,true);
    h.Frame(disabled_picker,p.x+100,p.y+120,true); h.Frame(disabled_picker,p.x+100,p.y+120,false);
    CHECK(number==30);
    number=5; h.Frame(picker,p.x+100,p.y+100); h.Frame(picker,p.x+100,p.y+100);
    ImGui::GetIO().AddMouseWheelEvent(0,-1);
    h.Frame(picker,p.x+100,p.y+100);
    CHECK(number==6 && outer_scroll==0);
    std::puts("PASS range and picker: live drag, no crossing, disabled control, wheel drag and external state");
}
unsigned MaxAlpha(ImGuiWindow *window) {
    unsigned alpha=0;
    for(const auto &vertex:window->DrawList->VtxBuffer)
        alpha=std::max(alpha,(vertex.col>>IM_COL32_A_SHIFT)&255u);
    return alpha;
}
void OverlayMotionTests() {
    for(int fps:{30,60,120}) {
        Interaction h; h.animations=true; ImGui::GetIO().DeltaTime=1.f/fps;
        bool open=true,close=false; int background_clicks=0;
        ImVec2 background{};
        auto draw=[&] {
            if(skui::Button("background")) ++background_clicks;
            background=ImGui::GetItemRectMin();
            if(open) { skui::OpenDialog("motion"); open=false; }
            if(skui::BeginDialog("motion","title")) {
                skui::Text("content");
                if(close) { skui::CloseDialog(); close=false; }
                skui::EndDialog();
            }
        };
        h.Frame(draw); h.Frame(draw);
        CHECK(h.Popup()); unsigned initial=MaxAlpha(h.Popup());
        CHECK(initial<255);
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        CHECK(h.Popup() && MaxAlpha(h.Popup())==255);
        CHECK(((h.Popup()->DC.ModalDimBgColor>>IM_COL32_A_SHIFT)&255)>70);
        // Scrim dismissal keeps the background button inert through exit.
        h.Click(draw,background.x+20,background.y+15);
        CHECK(background_clicks==0 && h.Popup());
        unsigned exiting=MaxAlpha(h.Popup()); CHECK(exiting<255);
        for(int i=0;i<fps/3;++i) h.Frame(draw);
        CHECK(!h.Popup());
        open=true; h.Frame(draw);
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        CHECK(h.Popup()); close=true; h.Frame(draw);
        CHECK(h.Popup());
        for(int i=0;i<fps/10;++i) h.Frame(draw);
        CHECK(h.Popup() && MaxAlpha(h.Popup())<255);
        for(int i=0;i<fps/3;++i) h.Frame(draw);
        CHECK(!h.Popup());
        // Disabling motion skips both entry and deferred exit.
        h.animations=false; open=true; h.Frame(draw); h.Frame(draw);
        CHECK(h.Popup() && MaxAlpha(h.Popup())==255);
        close=true; h.Frame(draw); CHECK(!h.Popup());
    }
    Interaction h; h.animations=true;
    int selected=0,changes=0;
    const skui::ChoiceItem items[]={{"one"},{"two"},{"three"}};
    auto draw=[&] { if(skui::Dropdown("choices",&selected,items,3)) ++changes; };
    h.Frame(draw); h.Frame(draw);
    h.Click(draw,(h.min.x+h.max.x)/2,(h.min.y+h.max.y)/2);
    for(int i=0;i<24;++i) h.Frame(draw);
    CHECK(h.Popup() && (h.Popup()->Flags&ImGuiWindowFlags_Modal));
    auto *popup=h.Popup(); CHECK(!popup->ScrollbarY);
    ImGuiWindow *options=nullptr;
    for(auto *child:popup->DC.ChildWindows) if(child->Active) options=child;
    CHECK(options && !options->ScrollbarY);
    auto p=options->DC.CursorStartPos;
    h.Click(draw,p.x+60,p.y+46+ImGui::GetStyle().ItemSpacing.y+20);
    CHECK(selected==1 && changes==1 && h.Popup());
    h.Click(draw,p.x+60,p.y+20); CHECK(selected==1 && changes==1);
    for(int i=0;i<20;++i) h.Frame(draw);
    CHECK(!h.Popup());
    std::puts("PASS overlay motion: entry/exit at 30/60/120Hz, scrim/input exclusion, reopen, reduced motion, dropdown exit");
}
void MenuExitFrameTests() {
    Interaction h; h.animations=true;
    const skui::PopupItem items[]={{1,"first"},{2,"second"},{3,"third"}};
    auto draw=[&] { skui::DropdownMenu("menu",items,3); };
    h.Frame(draw);h.Frame(draw);h.Click(draw,60,36);
    for(int i=0;i<24;++i) h.Frame(draw);
    auto *popup=h.Popup();CHECK(popup);
    h.Click(draw,700,850);
    bool closed=false;
    for(int i=0;i<20;++i) {
        h.Frame(draw);
        if(!h.Popup()) {
            closed=true;
            for(auto *list:ImGui::GetDrawData()->CmdLists) CHECK(list!=popup->DrawList);
            break;
        }
    }
    CHECK(closed);
    std::puts("PASS menu exit: no opaque background in final dismissal frame");
}
void ColorPreviewFrameTests() {
    Interaction h;
    ImVec4 color(.2f,.5f,1,1); ImVec2 preview{},grid{};float width=0;
    auto draw=[&] {
        preview=ImGui::GetCursorScreenPos();width=ImGui::GetContentRegionAvail().x;
        grid=ImVec2(preview.x,preview.y+36+ImGui::GetStyle().ItemSpacing.y);
        skui::ColorPalette("same-frame",&color);
    };
    h.Frame(draw);h.Frame(draw);
    float x=grid.x+width/26,y=grid.y+width/13*3.5f;
    h.Frame(draw,x,y);h.Frame(draw,x,y,true);
    CHECK(color.x>.8f && color.y<.01f);
    bool matches=false;
    auto expected=ImGui::GetColorU32(color);
    for(auto &v:ImGui::FindWindowByName("interactions")->DrawList->VtxBuffer)
        if(v.pos.y>=preview.y && v.pos.y<=preview.y+36 && v.col==expected) matches=true;
    CHECK(matches);
    std::puts("PASS color preview: grid input changes preview in the same frame");
}
void ContinuousColorTests() {
    Interaction h;
    ImVec4 color(.1f,.8f,.2f,1);ImVec2 preview{};float saved_hue=0;
    auto draw=[&] {
        preview=ImGui::GetCursorScreenPos();preview.y+=ImGui::GetTextLineHeight()+ImGui::GetStyle().ItemSpacing.y;
        skui::ColorPicker("picker",&color,false);
        ImGui::PushID("picker");saved_hue=ImGui::GetStateStorage()->GetFloat(ImGui::GetID("hue"));ImGui::PopID();
    };
    h.Frame(draw);h.Frame(draw);
    float x=h.min.x+(h.max.x-h.min.x)*.6f,y=(h.min.y+h.max.y)*.5f;
    h.Frame(draw,x,y);h.Frame(draw,x,y,true);
    float before=std::max(color.x,std::max(color.y,color.z));
    h.Frame(draw,x+1,y,true);
    float after=std::max(color.x,std::max(color.y,color.z));
    CHECK(after>before && after-before<.005f);
    bool matches=false;auto expected=ImGui::GetColorU32(color);
    for(auto &v:ImGui::FindWindowByName("interactions")->DrawList->VtxBuffer)
        if(v.pos.y>=preview.y && v.pos.y<=preview.y+32 && v.col==expected) matches=true;
    CHECK(matches);
    h.Frame(draw,x+1,y,false);
    auto min=h.min,max=h.max;
    h.Click(draw,min.x+1,y);CHECK(color.x==0 && color.y==0 && color.z==0);
    h.Click(draw,min.x+(max.x-min.x)*.7f,y);
    CHECK(color.y>color.x*2 && color.y>color.z*2); // black retains chroma
    float hue_y=y-2*(ImGui::GetTextLineHeight()+ImGui::GetFrameHeight()+2*ImGui::GetStyle().ItemSpacing.y);
    h.Click(draw,max.x-1,hue_y);h.Frame(draw);
    CHECK(saved_hue>.99f); // the red endpoint stays on the right after RGB round-trip
    auto alpha=[&] {skui::ColorPalette("alpha",&color);};
    h.Frame(alpha);h.Frame(alpha);min=h.min;max=h.max;
    x=min.x+(max.x-min.x)*.6f;y=(min.y+max.y)*.5f;
    h.Frame(alpha,x,y);h.Frame(alpha,x,y,true);before=color.w;
    h.Frame(alpha,x+1,y,true);CHECK(color.w>before && color.w-before<.005f);
    h.Frame(alpha,x+1,y,false);
    std::puts("PASS continuous color: one-pixel brightness/alpha, same-frame preview, black retains saturation, stable hue endpoint");
}
void PickerMotionTests() {
    for(int fps:{30,60,120}) {
        Interaction h; h.animations=true; ImGui::GetIO().DeltaTime=1.f/fps;
        int value=8; ImGuiWindow *wheel=nullptr;
        auto draw=[&] {
            skui::NumberPicker("motion picker",&value,0,60);
            for(auto *w:GImGui->Windows) if(std::strstr(w->Name,"/wheel_")) wheel=w;
        };
        h.Frame(draw); h.Frame(draw); CHECK(wheel && !wheel->ScrollbarY && wheel->ScrollMax.y==0);
        auto p=wheel->InnerRect.Min;
        h.Frame(draw,p.x+100,p.y+140);
        h.Frame(draw,p.x+100,p.y+140,true);
        for(int i=1;i<=4;++i) h.Frame(draw,p.x+100,p.y+140-i*20,true);
        CHECK(value==10); // update in the drag frame
        h.Frame(draw,p.x+100,p.y+60,false);
        int release=value;
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        CHECK(value>release); // inertia survives release
        for(int i=0;i<fps*2;++i) h.Frame(draw);
        int settled=value;
        float offset=wheel->StateStorage.GetFloat(wheel->GetID("offset"));
        CHECK(std::abs(offset)<.001f);
        h.Click(draw,p.x+100,p.y+140);
        for(int i=0;i<fps;++i) h.Frame(draw);
        CHECK(value==settled+1);
        value=60; h.Frame(draw); CHECK(value==60);
        h.Frame(draw,p.x+100,p.y+140); h.Frame(draw,p.x+100,p.y+140,true);
        h.Frame(draw,p.x+100,p.y+40,true); h.Frame(draw,p.x+100,p.y+40,false);
        for(int i=0;i<fps;++i) h.Frame(draw);
        CHECK(value==60);
        value=0; h.Frame(draw); CHECK(value==0);
        auto disabled=[&] { ImGui::BeginDisabled(); draw(); ImGui::EndDisabled(); };
        h.Click(disabled,p.x+100,p.y+140);
        for(int i=0;i<fps;++i) h.Frame(disabled);
        CHECK(value==0);
    }
    Interaction edge;
    int large=2147483646;
    auto large_range=[&] { skui::NumberPicker("large",&large,-2147483647-1,2147483647); };
    edge.Frame(large_range); edge.Frame(large_range); CHECK(large==2147483646);
    large=-2147483647; edge.Frame(large_range); CHECK(large==-2147483647);
    int stepped=-1;
    auto stepped_range=[&] { skui::NumberPicker("step",&stepped,-10,11,3); };
    edge.Frame(stepped_range); CHECK(stepped==-1);
    stepped=11; edge.Frame(stepped_range); CHECK(stepped==11);
    std::puts("PASS picker motion: same-frame drag, inertia, centered snap, tap, bounds/external updates/disabled at 30/60/120Hz");
}
void LongSelectionTests() {
    Interaction h;
    skui::ChoiceItem choices[18];
    for(auto &choice:choices) choice={"option"};
    int selected=0; float outer=0;
    auto draw=[&] {
        skui::BeginScroll("page",ImVec2(0,500)); outer=ImGui::GetScrollY();
        skui::Dropdown("long list",&selected,choices,18);
        skui::Space(900); skui::EndScroll();
    };
    h.Frame(draw); h.Frame(draw);
    h.Click(draw,200,40); CHECK(h.Popup());
    ImGuiWindow *options=nullptr;
    for(auto *window:GImGui->Windows)
        if(std::strstr(window->Name,"/options_") && window->WasActive) options=window;
    CHECK(options && options->ScrollMax.y>100);
    auto p=options->InnerRect.Min;
    h.Frame(draw,p.x+100,p.y+200); h.Frame(draw,p.x+100,p.y+200,true);
    h.Frame(draw,p.x+100,p.y+80,true);
    CHECK(options->Scroll.y>=119 && outer==0 && selected==0);
    h.Frame(draw,p.x+100,p.y+80,false);
    CHECK(selected==0 && h.Popup());
    std::puts("PASS long choices: touch scroll stays in popup and cancels option activation");
}
void MenuSheetAndRefreshTests() {
    Interaction h;
    const skui::PopupItem children[] = {{42,"child action"},{43,"disabled child",skui::Icon::None,false}};
    const skui::PopupItem items[] = {{1,"first action"},
        {-1,"submenu",skui::Icon::None,true,false,false,children,2}};
    int action=-1;
    auto menu = [&] { int next=skui::DropdownMenu("actions",items,2); if(next>=0) action=next; };
    h.Frame(menu); h.Frame(menu);
    float x=(h.min.x+h.max.x)/2,y=(h.min.y+h.max.y)/2;
    h.Click(menu,x,y); CHECK(h.Popup());
    auto origin=h.Popup()->DC.CursorStartPos;
    h.Click(menu,origin.x+80,origin.y+46+ImGui::GetStyle().ItemSpacing.y+7);
    CHECK(GImGui->OpenPopupStack.Size==1);
    origin=h.Popup()->DC.CursorStartPos;
    h.Click(menu,origin.x+80,origin.y+23);
    CHECK(action==-1 && h.Popup()); // Back returns to the root list.
    origin=h.Popup()->DC.CursorStartPos;
    h.Click(menu,origin.x+80,origin.y+46+ImGui::GetStyle().ItemSpacing.y+7);
    origin=h.Popup()->DC.CursorStartPos;
    h.Click(menu,origin.x+80,origin.y+46+ImGui::GetStyle().ItemSpacing.y+23);
    CHECK(action==42 && !h.Popup());
    bool open=true; ImVec2 sheet_pos{},sheet_size{},button_min{},button_max{};
    auto sheet = [&] {
        if(open) { skui::OpenDialog("sheet"); open=false; }
        if(skui::BeginBottomSheet("sheet","title",300)) {
            sheet_pos=ImGui::GetWindowPos(); sheet_size=ImGui::GetWindowSize();
            if(skui::Button("done")) skui::CloseDialog();
            button_min=ImGui::GetItemRectMin(); button_max=ImGui::GetItemRectMax();
            skui::EndBottomSheet();
        }
    };
    h.Frame(sheet); h.Frame(sheet); CHECK(h.Popup());
    CHECK(std::abs(sheet_pos.y+sheet_size.y-800)<1);
    h.Click(sheet,(button_min.x+button_max.x)/2,(button_min.y+button_max.y)/2);
    CHECK(!h.Popup());
    int requests=0; bool refreshing=false;
    auto refresh = [&] {
        bool requested=false;
        skui::BeginRefreshScroll("refresh",refreshing,&requested,ImVec2(0,300));
        skui::Text("list content"); skui::Space(600); skui::EndScroll();
        if(requested) { ++requests; refreshing=true; }
    };
    h.Frame(refresh); h.Frame(refresh);
    h.Frame(refresh,200,100); h.Frame(refresh,200,100,true);
    h.Frame(refresh,200,190,true); h.Frame(refresh,200,190,false);
    CHECK(requests==1);
    h.Frame(refresh,200,100); h.Frame(refresh,200,100,true);
    h.Frame(refresh,200,230,true); h.Frame(refresh,200,230,false);
    CHECK(requests==1);
    std::puts("PASS menus, sheet and refresh: submenu action closes tree, sheet anchoring/dismissal, single refresh request");
}
void NarrowMenuTests() {
    Interaction h;
    ImGui::GetIO().DisplaySize=ImVec2(1080,2400);
    const skui::PopupItem children[]={{42,"child action"}};
    const skui::PopupItem items[]={{-1,"submenu",skui::Icon::None,true,false,false,children,1}};
    int action=-1;
    auto draw=[&] {
        skui::Theme theme; theme.scale=2.2f; theme.animations=false; skui::ApplyTheme(theme);
        ImGui::SetCursorPos(ImVec2(450,400));
        int next=skui::DropdownMenu("actions",items,1); if(next>=0) action=next;
    };
    h.Frame(draw); h.Frame(draw);
    h.Click(draw,(h.min.x+h.max.x)/2,(h.min.y+h.max.y)/2);
    CHECK(h.Popup());
    auto origin=h.Popup()->DC.CursorStartPos;
    h.Click(draw,origin.x+100,origin.y+skui::Dp(23));
    auto *popup=h.Popup(); CHECK(popup && GImGui->OpenPopupStack.Size==1);
    CHECK(popup->Pos.x>=0 && popup->Pos.x+popup->Size.x<=1080);
    origin=popup->DC.CursorStartPos;
    h.Click(draw,origin.x+100,origin.y+skui::Dp(46)+ImGui::GetStyle().ItemSpacing.y+skui::Dp(23));
    CHECK(action==42 && !h.Popup());
    std::puts("PASS narrow menu: scaled submenu fits physical display and leaf action works");
}
void ShortMenuTests() {
    Interaction h;
    const skui::PopupItem children[]={{10,"one"},{11,"two"},{12,"three"}};
    const skui::PopupItem items[]={{0,"first"},
        {-1,"submenu",skui::Icon::None,true,false,false,children,3},{1,"other"},{2,"last"}};
    auto draw=[&] { skui::Theme t; t.scale=2.2f; t.animations=false; skui::ApplyTheme(t);
        skui::DropdownMenu("short menu",items,4); };
    h.Frame(draw);h.Frame(draw);
    h.Click(draw,(h.min.x+h.max.x)/2,(h.min.y+h.max.y)/2);
    auto p=h.Popup()->DC.CursorStartPos;
    h.Click(draw,p.x+80,p.y+skui::Dp(54+23));
    for(int i=0;i<5;++i) h.Frame(draw);
    CHECK(h.Popup() && !h.Popup()->ScrollbarY && h.Popup()->ScrollMax.y==0);
    std::puts("PASS short submenu: back and three actions fit without scrollbar");
}
void ShortPopupTests() {
    Interaction h;
    const skui::ChoiceItem choices[]={{"first"},{"second"},{"third"}};
    int selected=0;
    auto draw=[&] { skui::Dropdown("three choices",&selected,choices,3); };
    h.Frame(draw); h.Frame(draw);
    h.Click(draw,200,40);
    for(int i=0;i<5;++i) h.Frame(draw);
    CHECK(h.Popup() && !h.Popup()->ScrollbarY && h.Popup()->ScrollMax.y==0);
    for(auto *window:GImGui->Windows)
        if(window->WasActive && std::strstr(window->Name,"/options_"))
            CHECK(!window->ScrollbarY && window->ScrollMax.y==0);
    std::puts("PASS short popup: three choices fit without scrollbar or scroll extent");
}
void ScaledDescriptionPopupTests() {
    Interaction h;ImGui::GetIO().DisplaySize=ImVec2(1080,2400);
    const skui::ChoiceItem choices[]={{"first","summary"},{"second","summary"},
        {"third","summary"},{"disabled","summary",skui::Icon::None,false}};
    int selected=0;
    auto draw=[&] {skui::Theme t;t.scale=1080.f/490;t.animations=false;skui::ApplyTheme(t);
        skui::BeginCard("card");skui::Dropdown("descriptions",&selected,choices,4);skui::EndCard();};
    h.Frame(draw);h.Frame(draw);h.Click(draw,250,80);
    for(int i=0;i<5;++i) h.Frame(draw);
    CHECK(h.Popup() && !h.Popup()->ScrollbarY && h.Popup()->ScrollMax.y==0);
    for(auto *window:GImGui->Windows)
        if(window->WasActive && std::strstr(window->Name,"/options_")) {
            CHECK(!window->ScrollbarY && window->ScrollMax.y==0);
        }
}
void FloatingTests() {
    Interaction h;
    int clicks=0;
    auto draw=[&] {
        skui::Text("background page");
        if(skui::FloatingActionButton("floating-test",skui::Icon::Plus,ImVec2(300,300))) ++clicks;
    };
    h.Frame(draw); h.Frame(draw);
    h.Click(draw,100,100); // Focus the page, which normally raises it above other windows.
    auto *floating=ImGui::FindWindowByName("floating-test");
    auto *page=ImGui::FindWindowByName("interactions");
    CHECK(floating && page && ImGui::IsWindowAbove(floating,page));
    h.Click(draw,332,332); CHECK(clicks==1);
    std::puts("PASS floating action: remains above focused page and accepts touch");
}
void IndependentComponentTests() {
    Interaction h;
    bool checked=false,disabled=false; int radio=0;
    auto check=[&] {
        ImGui::BeginDisabled(disabled);
        skui::CheckboxPreference("sync",&checked,"summary text");
        ImGui::EndDisabled();
    };
    h.Frame(check);h.Frame(check);h.Click(check,200,45);CHECK(checked);
    disabled=true;h.Click(check,200,45);CHECK(checked);disabled=false;
    auto radio_draw=[&] { skui::RadioButtonPreference("mode",&radio,1,"summary"); };
    h.Frame(radio_draw);h.Click(radio_draw,200,45);CHECK(radio==1);
    bool clicked=false;
    auto badge=[&] {
        clicked=skui::IconButton("badge-anchor",skui::Icon::Heart) || clicked;
        auto id=ImGui::GetItemID();auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
        skui::AnchoredBadge("99+");
        CHECK(ImGui::GetItemID()==id && ImGui::GetItemRectMin().x==min.x && ImGui::GetItemRectMax().y==max.y);
    };
    h.Frame(badge);h.Click(badge,38,32);CHECK(clicked);
    int selected=0;
    const skui::NavItem items[]={{"one",skui::Icon::Heart},{"two",skui::Icon::Settings}};
    auto bar=[&] {skui::NavigationBar("bar",items,2,&selected);};
    h.Frame(bar);h.Frame(bar);h.Click(bar,430,45);CHECK(selected==1);
    auto rail=[&] {skui::NavigationRail("rail",items,2,&selected);};
    h.Frame(rail);h.Frame(rail);h.Click(rail,100,45);CHECK(selected==0);
    auto surface=[&] {
        bool visible=skui::BeginSurface("surface",ImVec2(0,160),24,ImVec4(.2f,.3f,.4f,1));
        if(visible) {skui::Text("content");skui::Button("action");}
        CHECK(std::abs(ImGui::GetStyle().ChildRounding-24)<.1f);
        skui::EndSurface();
    };
    h.Frame(surface);h.Frame(surface);
    std::puts("PASS independent components: summary preferences, disabled state, badge anchor, navigation and surface");
}
void SidebarTests() {
    Interaction h;
    bool expanded=true,disabled=false;
    int selected=0;
    float width=0;
    const skui::NavItem items[]={{"home",skui::Icon::Components},{"settings",skui::Icon::Settings}};
    auto rail=[&] {
        ImGui::BeginDisabled(disabled);
        skui::NavigationRail("expandable",items,2,&selected,&expanded);
        width=ImGui::GetItemRectSize().x;
        ImGui::EndDisabled();
    };
    h.Frame(rail);h.Frame(rail);CHECK(std::abs(width-172)<1);
    h.Click(rail,38,32);CHECK(!expanded && std::abs(width-72)<1);
    h.Click(rail,50,160);CHECK(selected==1);
    disabled=true;h.Click(rail,38,32);CHECK(!expanded);disabled=false;
    h.animations=true;
    h.Click(rail,38,32);CHECK(expanded && width>72 && width<172);
    for(int i=0;i<60;++i) h.Frame(rail);
    CHECK(std::abs(width-172)<1 && selected==1);
    // Two controls own independent state even in the same window.
    bool second=false;
    auto pair=[&] {
        skui::NavigationRail("first",items,2,&selected,&expanded);
        ImGui::SameLine();skui::NavigationRail("second",items,2,&selected,&second);
    };
    h.Frame(pair);h.Click(pair,38,32);CHECK(!expanded && !second);
    std::puts("PASS sidebar: toggle, compact selection, disabled state, animated width and independent instances");
}
void ShellSidebarTests() {
    UiFixture ui;
    bool expanded=true;
    int selected=0;
    ImVec2 content{};
    const skui::NavItem items[]={{"home",skui::Icon::Components},{"settings",skui::Icon::Settings}};
    auto frame=[&](float x=-100,float y=-100,bool down=false) {
        auto &io=ImGui::GetIO();io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,down);
        ImGui::NewFrame();skui::Theme theme;theme.animations=false;skui::ApplyTheme(theme);
        skui::ShellOptions options;options.initial_position=ImVec2(20,20);options.initial_size=ImVec2(780,600);
        options.items=items;options.item_count=2;options.selected=&selected;
        options.sidebar_expanded=&expanded;options.content_includes_header=true;
        skui::BeginShell(options);content=ImGui::GetCursorScreenPos();skui::Text("page");skui::EndShell();ImGui::Render();
    };
    frame();frame();auto before=content;
    frame(46,92);frame(46,92,true);frame(46,92,false);
    CHECK(!expanded && std::abs(before.x-content.x-100)<1 && std::abs(before.y-content.y)<1);
    frame(46,215);frame(46,215,true);frame(46,215,false);CHECK(selected==1);
    frame(46,92);frame(46,92,true);frame(46,92,false);
    CHECK(expanded && selected==1 && std::abs(content.x-before.x)<1);
    std::puts("PASS shell sidebar: shares rail toggle and gives released width to page without moving its top");
}
void PaletteTests() {
    Interaction h;
    ImVec4 color(.2f,.5f,1,.4f); ImVec2 grid{};float width=0;bool disabled=false;
    auto draw=[&] {
        grid=ImGui::GetCursorScreenPos();grid.y+=36+ImGui::GetStyle().ItemSpacing.y;
        width=ImGui::GetContentRegionAvail().x;
        ImGui::BeginDisabled(disabled);skui::ColorPalette("grid",&color);ImGui::EndDisabled();
    };
    h.Frame(draw);h.Frame(draw);
    h.Click(draw,grid.x+width/26,grid.y+width/13*3.5f);
    CHECK(color.x>.8f && color.y<.01f && color.z<.01f && std::abs(color.w-.4f)<.001f);
    h.Click(draw,grid.x+width*12.5f/13,grid.y+width/13*2.5f);
    CHECK(std::abs(color.x-color.y)<.001f && std::abs(color.y-color.z)<.001f);
    auto min=h.min,max=h.max;
    h.Frame(draw,min.x+10,(min.y+max.y)/2);h.Frame(draw,min.x+10,(min.y+max.y)/2,true);
    h.Frame(draw,min.x+(max.x-min.x)*.8f,(min.y+max.y)/2,true);
    h.Frame(draw,min.x+(max.x-min.x)*.8f,(min.y+max.y)/2,false);
    CHECK(color.w>.7f);
    auto saved=color;disabled=true;
    h.Click(draw,grid.x+width/26,grid.y+width/13*3.5f);
    CHECK(color.x==saved.x && color.y==saved.y && color.z==saved.z && color.w==saved.w);
    std::puts("PASS HSV grid: hue, gray column, retained alpha, alpha drag and disabled input");
}
void SystemInputTests() {
    UiFixture ui;
    struct Input {
        const char *replacement = nullptr;
        bool finish = false, active = false;
    } input;
    skui::SetTextInputHandler([](skui::TextInputEvent &event, void *data) {
        auto &input = *static_cast<Input *>(data);
        input.active = event.active;
        if (event.active && input.replacement) {
            CHECK(std::strlen(input.replacement) < event.capacity);
            std::strcpy(event.buffer, input.replacement);
            event.selection_start = event.selection_end = static_cast<int>(std::strlen(event.buffer));
            event.changed = true;
            input.replacement = nullptr;
        }
        event.finished = input.finish;
    }, &input);
    char buffer[64] = "initial";
    ImVec2 min, max;
    auto frame = [&]() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(600, 800));
        ImGui::Begin("ime-test", nullptr, ImGuiWindowFlags_NoTitleBar);
        bool changed = skui::InputText("name", buffer, sizeof(buffer));
        min = ImGui::GetItemRectMin(); max = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return changed;
    };
    frame();
    auto &io = ImGui::GetIO();
    io.AddMousePosEvent((min.x + max.x) / 2, (min.y + max.y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    CHECK(input.active);
    input.replacement = "你好😀";
    CHECK(frame());
    CHECK(std::strcmp(buffer, "你好😀") == 0);
    frame();
    CHECK(std::strcmp(buffer, "你好😀") == 0);
    input.replacement = "你好"; CHECK(frame()); frame();
    CHECK(std::strcmp(buffer, "你好") == 0);
    input.replacement = ""; CHECK(frame()); frame();
    CHECK(buffer[0] == '\0');
    input.replacement = "完成"; input.finish = true; CHECK(frame());
    input.finish = false; frame();
    CHECK(std::strcmp(buffer, "完成") == 0);
    CHECK(!input.active);
    skui::SetTextInputHandler(nullptr);
    std::puts("PASS system input: Chinese, supplementary characters, deletion, clear, final commit and focus release");
}
// Check the presented geometry and vertex opacity, not just boolean state.
void CollapseMotionTests() {
    for(int fps:{30,60,120}) {
        Interaction h; h.animations=true; ImGui::GetIO().DeltaTime=1.f/fps;
        bool expanded=true; float height=0,following=0; int mounted=0,clicks=0;
        auto draw=[&] {
            skui::Collapsible("details",expanded,[&] {
                ++mounted;
                if(skui::Button("inside",skui::Tone::Primary,ImVec2(180,44))) ++clicks;
                skui::Space(140);
            });
            height=ImGui::GetItemRectSize().y;
            following=ImGui::GetCursorScreenPos().y;
            // Hidden full-size contents must not retain the expanded scroll range.
            CHECK(ImGui::GetCurrentWindow()->DC.CursorMaxPos.y<=following+1);
        };
        h.Frame(draw);h.Frame(draw);float full=height,top=following-full;
        CHECK(full>180);
        expanded=false; h.Frame(draw);
        CHECK(height>full*.95f && height<full);
        float previous=height;
        for(int i=0;i<fps/8;++i) {
            h.Frame(draw);CHECK(height<=previous+.01f && height>0);previous=height;
        }
        expanded=true;h.Frame(draw);
        CHECK(height>previous && height-previous<full*.06f);
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        CHECK(std::abs(height-full)<1);
        // Input is excluded from the first closing frame, while content remains mounted.
        h.Frame(draw,70,30);h.Frame(draw,70,30,true);
        expanded=false;h.Frame(draw,70,30,false);CHECK(clicks==0);
        CHECK(height>full*.95f);
        for(int i=0;i<fps/2;++i) h.Frame(draw);
        int closed_count=mounted;h.Frame(draw);CHECK(mounted==closed_count);
        CHECK(std::abs(following-(top-ImGui::GetStyle().ItemSpacing.y))<1);
        // A freshly measured reveal from the closed state also grows progressively.
        expanded=true;h.Frame(draw);CHECK(height>0 && height<full*.06f);
        h.animations=false;expanded=false;h.Frame(draw);CHECK(mounted==closed_count+1);
        expanded=true;h.Frame(draw);CHECK(std::abs(height-full)<1);
    }
    {
        Interaction h;h.animations=true;bool expanded=true;ImVec2 outside{};int clicks=0;
        auto draw=[&] {
            outside=ImGui::GetCursorScreenPos();
            if(skui::Button("outside",skui::Tone::Neutral,ImVec2(180,44))) ++clicks;
            skui::Collapsible("contents",expanded,[&] {skui::Button("inside");skui::Space(120);});
        };
        h.Frame(draw);h.Frame(draw);float x=outside.x+60,y=outside.y+20;
        h.Frame(draw,x,y);h.Frame(draw,x,y,true);CHECK(GImGui->ActiveId!=0);
        expanded=false;h.Frame(draw,x,y,true);CHECK(GImGui->ActiveId!=0);
        h.Frame(draw,x,y,false);CHECK(clicks==1);
    }
    std::puts("PASS collapse motion: retained exit, clipped height/scroll extent, reversal, input exclusion, 30/60/120Hz and reduced motion");
}
void SidebarMotionTests() {
    for(int fps:{30,60,120}) {
        Interaction h;h.animations=true;ImGui::GetIO().DeltaTime=1.f/fps;
        bool expanded=true;int selected=0;float width=0;
        const skui::NavItem items[]={{"home",skui::Icon::Components},{"settings",skui::Icon::Settings}};
        auto draw=[&] {skui::NavigationRail("rail-motion",items,2,&selected,&expanded);width=ImGui::GetItemRectSize().x;};
        h.Frame(draw);h.Frame(draw);CHECK(std::abs(width-172)<1);
        expanded=false;h.Frame(draw);CHECK(width>167 && width<172);
        float previous=width;
        for(int i=0;i<fps/6;++i) {h.Frame(draw);CHECK(width<previous);previous=width;}
        expanded=true;h.Frame(draw);CHECK(width>previous && width-previous<5);
        for(int i=0;i<fps/2;++i) h.Frame(draw);CHECK(std::abs(width-172)<1);
        expanded=false;
        for(int i=0;i<fps/2;++i) h.Frame(draw);CHECK(std::abs(width-72)<1);
    }
    std::puts("PASS sidebar timing: gradual collapse and expansion, reversible geometry at 30/60/120Hz");
}
unsigned MarkerAlpha(ImGuiWindow *window,ImU32 rgb) {
    unsigned alpha=0;
    for(auto &v:window->DrawList->VtxBuffer)
        if((v.col&~IM_COL32_A_MASK)==(rgb&~IM_COL32_A_MASK))
            alpha=std::max(alpha,(v.col>>IM_COL32_A_SHIFT)&255u);
    return alpha;
}
void OverlayContentExitTests() {
    for(int kind=1;kind<=2;++kind) {
        Interaction h;h.animations=true;bool open=true,close=false;int clicks=0;
        constexpr ImU32 marker=IM_COL32(19,173,92,255),disabled_marker=IM_COL32(163,47,81,255);
        auto draw=[&] {
            if(open) {skui::OpenDialog("content-exit");open=false;}
            bool visible=kind==2 ? skui::BeginBottomSheet("content-exit","sheet",260)
                : kind==1 ? skui::BeginDialog("content-exit","dialog") : false;
            if(visible) {
                auto p=ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(p,ImVec2(p.x+40,p.y+30),ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(marker)));
                ImGui::BeginDisabled();
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x+45,p.y),ImVec2(p.x+75,p.y+30),ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(disabled_marker)));
                ImGui::EndDisabled();skui::Space(35);
                if(skui::Button("action")) ++clicks;
                if(close) {skui::CloseDialog();close=false;}
                if(kind==2) skui::EndBottomSheet();else skui::EndDialog();
            }
        };
        h.Frame(draw);for(int i=0;i<30;++i) h.Frame(draw);
        CHECK(h.Popup());close=true;h.Frame(draw);h.Frame(draw);
        CHECK(MarkerAlpha(h.Popup(),marker)>240);
        CHECK(MarkerAlpha(h.Popup(),disabled_marker)>80 && MarkerAlpha(h.Popup(),disabled_marker)<110);
        for(int i=0;i<25;++i) h.Frame(draw);
        CHECK(!h.Popup() && clicks==0);
    }
    std::puts("PASS overlay content: closing does not gray content, genuine disabled controls retain their tint, dialog/sheet settle");
}
void FloatingAndSnackbarMotionTests() {
    Interaction h;h.animations=true;bool visible=false;int clicks=0;
    auto draw=[&] {if(skui::FloatingActionButton("motion-fab",skui::Icon::Plus,ImVec2(630,100),visible)) ++clicks;};
    h.Frame(draw);visible=true;h.Frame(draw);
    auto *window=ImGui::FindWindowByName("motion-fab");CHECK(window && MaxAlpha(window)<30);
    for(int i=0;i<25;++i) h.Frame(draw);CHECK(MaxAlpha(window)==255);
    visible=false;h.Frame(draw);CHECK(MaxAlpha(window)>240);
    h.Click(draw,662,132);CHECK(clicks==0);
    for(int i=0;i<25;++i) h.Frame(draw);
    for(auto *list:ImGui::GetDrawData()->CmdLists) CHECK(list!=window->DrawList);
    skui::Snackbar snackbar;
    auto toast=[&] {snackbar.Draw(ImVec2(50,680),300);};
    snackbar.Show("motion message",.5f);h.Frame(toast);
    auto foreground_alpha=[] {
        unsigned a=0;
        for(auto &v:ImGui::GetForegroundDrawList()->VtxBuffer) a=std::max(a,(v.col>>IM_COL32_A_SHIFT)&255u);
        return a;
    };
    CHECK(foreground_alpha()>0 && foreground_alpha()<30);
    for(int i=0;i<16;++i) h.Frame(toast);CHECK(foreground_alpha()==255);
    for(int i=0;i<14;++i) h.Frame(toast);CHECK(foreground_alpha()>0 && foreground_alpha()<255);
    snackbar.Show("replace during exit",.5f);h.Frame(toast);CHECK(foreground_alpha()>0);
    for(int i=0;i<50;++i) h.Frame(toast);CHECK(foreground_alpha()==0);
    std::puts("PASS floating/toast: entry, retained noninteractive exit, final draw removal, toast reversal");
}
void SelectionMotionTests() {
    for(int preference=0;preference<2;++preference) {
        Interaction h;h.animations=true;bool value=false;unsigned check_alpha=0;
        auto draw=[&] {
            auto *list=ImGui::GetWindowDrawList();int first=list->VtxBuffer.Size;
            if(preference) skui::CheckboxPreference("checkbox preference",&value);
            else skui::Checkbox("checkbox",&value);
            check_alpha=0;
            for(int i=first;i<list->VtxBuffer.Size;++i) {
                auto color=list->VtxBuffer[i].col;
                if((color&~IM_COL32_A_MASK)==(IM_COL32_WHITE&~IM_COL32_A_MASK))
                    check_alpha=std::max(check_alpha,(color>>IM_COL32_A_SHIFT)&255u);
            }
        };
        h.Frame(draw);h.Frame(draw);CHECK(check_alpha==0);
        value=true;h.Frame(draw);CHECK(check_alpha>0 && check_alpha<30);
        for(int i=0;i<20;++i) h.Frame(draw);CHECK(check_alpha==255);
        value=false;h.Frame(draw);CHECK(check_alpha>240);
        for(int i=0;i<20;++i) h.Frame(draw);CHECK(check_alpha==0);
        // With motion disabled the click and painted tick agree on the release frame.
        h.animations=false;h.Frame(draw,60,32);h.Frame(draw,60,32,true);h.Frame(draw,60,32,false);
        CHECK(value && check_alpha==255);
    }
    Interaction h;h.animations=true;int selected=0;
    const char *tabs[]={"first","second","third"};
    auto draw=[&] {skui::TabBar("moving-tabs",tabs,3,&selected);};
    auto indicator=[] {return MarkerX(ImGui::ColorConvertFloat4ToU32(skui::GetTheme().accent));};
    h.Frame(draw);h.Frame(draw);float start=indicator();
    selected=2;h.Frame(draw);float first=indicator();CHECK(first>start && first<start+10);
    for(int i=0;i<25;++i) h.Frame(draw);float end=indicator();CHECK(end>start+300);
    selected=0;h.Frame(draw);CHECK(indicator()<end && indicator()>end-10);
    std::puts("PASS selection motion: checkbox/preference check fades in both directions, same-frame reduced-motion state, sliding tab indicator");
}
void RefreshMotionTests() {
    Interaction h;h.animations=true;bool refreshing=false;int requests=0;float content_y=0;
    auto draw=[&] {
        bool requested=false;
        skui::BeginRefreshScroll("refresh-motion",refreshing,&requested,ImVec2(0,300));
        content_y=ImGui::GetCursorScreenPos().y;
        skui::Text("contents");skui::Space(600);skui::EndScroll();
        if(requested) {++requests;refreshing=true;}
    };
    h.Frame(draw);h.Frame(draw);float rest=content_y;
    h.Frame(draw,200,100);h.Frame(draw,200,100,true);h.Frame(draw,200,190,true);
    CHECK(content_y>rest+20);float pulled=content_y;
    h.Frame(draw,200,190,false);
    CHECK(requests==1 && content_y>=pulled-2);
    for(int i=0;i<25;++i) h.Frame(draw);float full=content_y;
    refreshing=false;h.Frame(draw);CHECK(content_y<full && content_y>full-3);
    for(int i=0;i<25;++i) h.Frame(draw);CHECK(std::abs(content_y-rest)<1 && requests==1);
    std::puts("PASS refresh motion: same-frame pull height, retained release header, animated return without a second request");
}
void SubmenuMotionTests() {
    Interaction h;h.animations=true;
    const skui::PopupItem children[]={{42,"child first"},{43,"child second"},{44,"child third"}};
    const skui::PopupItem items[]={{-1,"submenu",skui::Icon::None,true,false,false,children,3}};
    int action=-1;
    auto draw=[&] {int next=skui::DropdownMenu("cascading",items,1);if(next>=0) action=next;};
    h.Frame(draw);h.Frame(draw);h.Click(draw,60,36);
    for(int i=0;i<25;++i) h.Frame(draw);CHECK(h.Popup());
    auto *popup=h.Popup();auto p=popup->DC.CursorStartPos;float before=popup->Size.y;
    h.Click(draw,p.x+80,p.y+23);
    CHECK(action==-1 && popup->Size.y<=before+1);
    bool intermediate=false;
    for(int i=0;i<35;++i) {
        h.Frame(draw);
        CHECK(MarkerAlpha(popup,ImGui::ColorConvertFloat4ToU32(skui::TextColor()))>110);
        if(popup->Size.y>before+2 && popup->Size.y<before+130) intermediate=true;
    }
    CHECK(intermediate && popup->Size.y>before+140 && !popup->ScrollbarY);
    p=popup->DC.CursorStartPos;
    h.Click(draw,p.x+80,p.y+46+ImGui::GetStyle().ItemSpacing.y+23);CHECK(action==42);
    for(int i=0;i<25;++i) h.Frame(draw);CHECK(!h.Popup());
    std::puts("PASS submenu motion: retained old list, fade and gradual height change, no spurious action, leaf closes normally");
}
void TooltipMotionTests() {
    Interaction h;h.animations=true;ImGuiWindow *tooltip=nullptr;
    auto draw=[&] {
        skui::IconButton("hint",skui::Icon::Info);skui::Tooltip("help text");
        for(auto *window:GImGui->Windows) if(window->Active && (window->Flags&ImGuiWindowFlags_Tooltip)) tooltip=window;
    };
    h.Frame(draw);h.Frame(draw);bool intermediate=false;
    for(int i=0;i<50;++i) {
        h.Frame(draw,30,30);
        if(tooltip && MaxAlpha(tooltip)>0 && MaxAlpha(tooltip)<240) intermediate=true;
    }
    CHECK(tooltip && intermediate && MaxAlpha(tooltip)==255);
    auto position=tooltip->Pos;
    h.Frame(draw,500,500);CHECK(MaxAlpha(tooltip)>240);
    CHECK(std::abs(position.x-tooltip->Pos.x)<1 && std::abs(position.y-tooltip->Pos.y)<1);
    for(int i=0;i<20;++i) h.Frame(draw);
    for(auto *list:ImGui::GetDrawData()->CmdLists) CHECK(list!=tooltip->DrawList);
    std::puts("PASS tooltip motion: delayed hover, gradual entry, retained exit at the original position, final draw removal");
}
void DynamicAtlasRouteTests() {
    ImGui::CreateContext();
    auto &io=ImGui::GetIO();
    io.IniFilename=nullptr;
    io.DisplaySize=ImVec2(800,900);
    io.DeltaTime=1.f/60;
    io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
    io.Fonts->AddFontDefaultVector();
    skui::Router router;
    skui::RouterView view;
    router.Register("a",[](const skui::Route &) {skui::Text("Outgoing page");});
    router.Register("b",[](const skui::Route &) {
        ImGui::PushFont(nullptr,37);
        skui::Text("New size loads glyphs on demand");
        ImGui::PopFont();
    });
    router.Reset("a");
    ImTextureID next_id=1;
    auto frame=[&](bool repack=false) {
        ImGui::NewFrame();
        skui::ApplyTheme({});
        ImGui::SetNextWindowSize(ImVec2(600,800));
        ImGui::Begin("dynamic-atlas");
        view.Draw(router,ImVec2(560,600));
        if(repack) {
            auto *atlas=io.Fonts;
            ImFontAtlasTextureRepack(atlas,atlas->TexData->Width*2,atlas->TexData->Height*2);
        }
        ImGui::End();
        ImGui::Render();
        // Simulate the backend texture protocol, including immediate retirement.
        for(auto *texture:*ImGui::GetDrawData()->Textures) {
            if(texture->Status==ImTextureStatus_WantDestroy) {
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            } else if(texture->Status!=ImTextureStatus_OK) {
                if(texture->GetTexID()==ImTextureID_Invalid) texture->SetTexID(next_id++);
                texture->SetStatus(ImTextureStatus_OK);
            }
        }
        for(auto *list:ImGui::GetDrawData()->CmdLists) for(auto &cmd:list->CmdBuffer) {
            if(!cmd.ElemCount || cmd.UserCallback) continue;
            CHECK(cmd.GetTexID()!=ImTextureID_Invalid);
            if(cmd.TexRef._TexData) CHECK(cmd.TexRef._TexData->Status==ImTextureStatus_OK);
        }
    };
    frame();frame();
    CHECK(router.Push("b"));frame();CHECK(view.IsTransitioning());
    ImTextureData *frozen=nullptr;
    for(auto *texture:io.Fonts->TexList)
        if(texture!=io.Fonts->TexData && !texture->WantDestroyNextFrame) frozen=texture;
    CHECK(frozen);
    std::vector<unsigned char> pixels(frozen->Pixels,frozen->Pixels+frozen->GetSizeInBytes());
    frame(true);frame();frame();
    CHECK(view.IsTransitioning() && io.Fonts->TexList.contains(frozen));
    CHECK(!frozen->WantDestroyNextFrame);
    CHECK(std::memcmp(frozen->Pixels,pixels.data(),pixels.size())==0);
    // Interrupt an active transition, exercising the retained dimming layer.
    CHECK(router.Back());frame();
    CHECK(router.Push("b"));frame();frame(true);frame();
    for(int i=0;i<40;++i) frame();
    CHECK(!view.IsTransitioning());
    CHECK(io.Fonts->TexList.Size==1);
    // A RouterView can still contain a snapshot when its host shuts down.
    CHECK(router.Back());frame();CHECK(view.IsTransitioning());
    ImGui::DestroyContext();
    std::puts("PASS dynamic atlas: new font sizes, repack during routes, interruption, texture retirement, context shutdown");
}
int main() {
    NavigationTests();
    ComponentTests();
    ScrollTests();
    ShellDragTests();
    RouteViewTests();
    RouteMotionReferenceTests();
    RouteClipIndexTests();
    DynamicAtlasRouteTests();
    SelectionTests();
    OverlayMotionTests();
    MenuExitFrameTests();
    ColorPreviewFrameTests();
    ContinuousColorTests();
    PickerMotionTests();
    LongSelectionTests();
    RangeAndPickerTests();
    MenuSheetAndRefreshTests();
    NarrowMenuTests();
    ShortPopupTests();
    ScaledDescriptionPopupTests();
    ShortMenuTests();
    FloatingTests();
    IndependentComponentTests();
    SidebarTests();
    ShellSidebarTests();
    CollapseMotionTests();
    SidebarMotionTests();
    OverlayContentExitTests();
    FloatingAndSnackbarMotionTests();
    SelectionMotionTests();
    RefreshMotionTests();
    SubmenuMotionTests();
    TooltipMotionTests();
    PaletteTests();
    SystemInputTests();
}
