#include "skui/skui.h"
#include "skui/diagnostics.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#if defined(__unix__)
#include "preferences.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#endif

namespace {
bool count_allocations = false;
std::size_t allocations = 0;
}
void *operator new(std::size_t n) {
    if (count_allocations) ++allocations;
    if (void *p=std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p,std::size_t) noexcept { std::free(p); }
void operator delete[](void *p,std::size_t) noexcept { std::free(p); }
#define CHECK(c) do { if (!(c)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#c);std::exit(1);} } while(false)

namespace {
bool Near(float a,float b) {return std::abs(a-b)<.0001f;}
void Themes() {
    auto *context=ImGui::CreateContext();
    auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(800,900);
    unsigned char *pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    for (bool dark : {false,true}) for (float scale : {1.f,2.2f}) {
        skui::Theme base;base.dark=dark;base.scale=scale;
        ImGui::GetStyle().FontSizeBase=18;ImGui::GetStyle().FontScaleMain=1.25f;ImGui::GetStyle().FontScaleDpi=2;
        skui::ApplyTheme(base);
        CHECK(Near(ImGui::GetStyle().ChildRounding,18*scale));
        CHECK(Near(ImGui::GetStyle().FramePadding.x,14*scale));
        CHECK(ImGui::GetStyle().FontSizeBase==18 && ImGui::GetStyle().FontScaleMain==1.25f && ImGui::GetStyle().FontScaleDpi==2);
        CHECK((dark && skui::Background().x<.1f) || (!dark && skui::Background().x>.9f));
        auto customized=base;
        customized.animations=false;
        auto &palette=dark ? customized.dark_palette : customized.light;
        palette.surface=ImVec4(.33f,.44f,.55f,1);palette.scrim=ImVec4(.2f,.1f,.3f,.6f);
        customized.tokens.card_radius=5;customized.tokens.control_radius=7;
        customized.tokens.item_spacing=ImVec2(17,19);customized.tokens.text_scale=1.25f;
        ImGui::GetStyle().Alpha=.81f;
        try {
            skui::ThemeScope outer(customized);
            CHECK(Near(skui::Surface().x,.33f));CHECK(Near(ImGui::GetStyle().ChildRounding,5*scale));
            CHECK(Near(ImGui::GetStyle().FrameRounding,7*scale));
            CHECK(Near(ImGui::GetStyle().ItemSpacing.y,19*scale));
            CHECK(Near(skui::TextSize(16),20*scale));
            auto nested=customized;nested.tokens.control_radius=2;
            {skui::ThemeScope inner(nested);CHECK(Near(ImGui::GetStyle().FrameRounding,2*scale));}
            CHECK(Near(ImGui::GetStyle().FrameRounding,7*scale));
            throw std::runtime_error("scope unwind");
        } catch (const std::runtime_error &) {}
        CHECK(Near(ImGui::GetStyle().Alpha,.81f));CHECK(Near(ImGui::GetStyle().ChildRounding,18*scale));
        CHECK(Near(skui::GetTheme().tokens.text_scale,1));
        // A changed semantic surface reaches actual container draw data,
        // including disabled contents, rather than just the style structure.
        bool found=false;
        for(int frame=0;frame<3;++frame) {
            skui::ApplyTheme(customized);ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2(600,700));ImGui::Begin("tokens");
            skui::BeginCard("card");ImGui::BeginDisabled();skui::Button("disabled");ImGui::EndDisabled();skui::EndCard();
            auto *card=GImGui->CurrentWindow->DC.ChildWindows.back();
            CHECK(Near(card->WindowRounding,5*scale));
            auto expected=ImGui::GetColorU32(palette.surface);
            if(frame==1) {
                skui::OpenDialog("token-dialog");
                CHECK(skui::BeginDialog("token-dialog","Theme overlay"));
                CHECK(GImGui->CurrentWindow->DC.ModalDimBgColor==ImGui::ColorConvertFloat4ToU32(palette.scrim));
                CHECK(Near(ImGui::GetStyle().PopupRounding,customized.tokens.popup_radius*scale));
                skui::CloseDialog();skui::EndDialog();
            }
            found=false;
            ImGui::End();ImGui::Render();
            for(auto *list:ImGui::GetDrawData()->CmdLists)
                for(const auto &v:list->VtxBuffer) if(v.col==expected) found=true;
        }
        CHECK(found);
    }
    skui::Theme invalid;invalid.scale=std::numeric_limits<float>::quiet_NaN();
    invalid.tokens.control_radius=-3;invalid.tokens.card_radius=INFINITY;invalid.tokens.item_spacing.x=NAN;
    invalid.tokens.text_scale=INFINITY;invalid.light.surface.x=NAN;invalid.accent.x=2;
    skui::ApplyTheme(invalid);
    CHECK(Near(skui::Dp(1),1) && Near(ImGui::GetStyle().FrameRounding,0));
    CHECK(Near(ImGui::GetStyle().ChildRounding,18) && Near(skui::TextSize(16),16));
    CHECK(std::isfinite(skui::Surface().x) && skui::GetTheme().accent.x==1);
    ImGui::DestroyContext(context);
    std::puts("PASS theme tokens: light/dark, scale, host fonts, container vertices, invalid inputs and nested exception-safe restoration");
}
void Profiling() {
    skui::FrameProfiler profiler;
    CHECK(!profiler.Enabled());
    skui::FrameSample sample;sample.cpu_ms=1;
    CHECK(!profiler.Record(sample) && profiler.Statistics().samples==0);
    profiler.BeginFrame();profiler.EndInput();profiler.EndBuild();profiler.EndFrame();
    CHECK(profiler.SampleCount()==0);
    profiler.SetEnabled(true);
    for(int i=1;i<=300;++i) {
        sample.cpu_ms=i;sample.interval_ms=i*2;sample.vertices=i;
        CHECK(profiler.Record(sample));
    }
    auto stats=profiler.Statistics();
    CHECK(stats.samples==240 && stats.latest.vertices==300);
    CHECK(stats.cpu_p50_ms==180 && stats.cpu_p95_ms==288);
    CHECK(stats.interval_p50_ms==360 && stats.interval_p95_ms==576);
    sample.cpu_ms=NAN;CHECK(!profiler.Record(sample));sample.cpu_ms=-1;CHECK(!profiler.Record(sample));
    CHECK(profiler.SampleCount()==240 && profiler.Latest().vertices==300);
    profiler.Clear();CHECK(profiler.Enabled() && profiler.SampleCount()==0);
    sample.cpu_ms=3;sample.interval_ms=0;CHECK(profiler.Record(sample));
    CHECK(profiler.Statistics().cpu_p95_ms==3 && profiler.Statistics().interval_p95_ms==0);
    allocations=0;count_allocations=true;
    for(int i=0;i<10000;++i) {profiler.Record(sample);auto s=profiler.Statistics();CHECK(s.samples>0);}
    count_allocations=false;CHECK(allocations==0);
    profiler.SetEnabled(false);CHECK(profiler.SampleCount()==0);
    auto *context=ImGui::CreateContext();
    profiler.SetEnabled(true);profiler.BeginFrame();profiler.EndInput();profiler.EndInput();
    profiler.AddEffectTime(.5);profiler.AddEffectTime(.25);profiler.AddEffectTime(NAN);profiler.EndBuild();profiler.EndFrame();
    CHECK(profiler.SampleCount()==1 && profiler.Latest().interval_ms==0);
    CHECK(profiler.Latest().effects_ms==.75);
    CHECK(profiler.Latest().cpu_ms>=profiler.Latest().input_ms+profiler.Latest().build_ms+profiler.Latest().render_ms-.001);
    profiler.EndFrame();CHECK(profiler.SampleCount()==1);
    profiler.SetEnabled(false);profiler.SetEnabled(true);profiler.BeginFrame();profiler.EndBuild();profiler.EndFrame();
    CHECK(profiler.Latest().interval_ms==0);
    ImGui::DestroyContext(context);
    std::puts("PASS profiler: bounded history, exact percentiles, invalid samples, disabled/reset lifecycle and zero allocation during sampling/statistics");
}
#if defined(__unix__)
void Preferences() {
    gallery::Preferences source,decoded;
    source.theme.dark=true;source.theme.animations=false;source.theme.haptics=false;
    source.theme.accent=ImVec4(.07f,.2f,.7f,.9f);source.theme.motion_damping=1.2f;
    source.theme.tokens.text_scale=1.3f;source.theme.tokens.card_radius=7;source.theme.tokens.control_radius=4;
    source.wide=true;source.sidebar_expanded=false;source.position=ImVec2(.1f,.2f);source.size=ImVec2(.8f,.7f);
    auto encoded=gallery::EncodePreferences(source);CHECK(!encoded.empty());
    CHECK(gallery::DecodePreferences(encoded,decoded));
    CHECK(gallery::EncodePreferences(decoded)==encoded);
    CHECK(gallery::DecodePreferences("version=0\ndark=1\nfuture_setting=ignored\n",decoded));
    CHECK(decoded.theme.dark && decoded.theme.animations && Near(decoded.theme.tokens.text_scale,1));
    for (auto text : {"", "dark=1\n", "version=2\n", "version=1\ndark=2\n", "version=1\ndark=1\ndark=0\n",
            "version=1\ntext_scale=nan\n", "version=1\nmotion=0 1 1\n", "version=1\nposition=-1 0\n",
            "version=1\nsize=0 0\n", "version=1\ntext_scale=1 rubbish\n", "version=1\nradii=-1 3\n", "version=1\nposition=1 1\n"}) {
        auto before=gallery::EncodePreferences(decoded);
        CHECK(!gallery::DecodePreferences(text,decoded));CHECK(gallery::EncodePreferences(decoded)==before);
    }
    CHECK(!gallery::DecodePreferences(std::string(4097,'x'),decoded));
    CHECK(!gallery::DecodePreferences(std::string("version=1\n\0",11),decoded));
    char dir[]="foundation-preferences-XXXXXX";CHECK(::mkdtemp(dir));
    std::filesystem::path directory(dir);auto path=(directory/"preferences.cfg").string();
    CHECK(gallery::LoadPreferences(path,decoded)==gallery::LoadResult::Missing);
    CHECK(gallery::SavePreferences(path,source));
    CHECK(gallery::LoadPreferences(path,decoded)==gallery::LoadResult::Loaded && gallery::EncodePreferences(decoded)==encoded);
    source.theme.dark=false;CHECK(gallery::SavePreferences(path,source));
    CHECK(gallery::LoadPreferences(path,decoded)==gallery::LoadResult::Loaded && !decoded.theme.dark);
    source.theme.tokens.text_scale=NAN;CHECK(!gallery::SavePreferences(path,source));
    CHECK(gallery::LoadPreferences(path,decoded)==gallery::LoadResult::Loaded && !decoded.theme.dark);
    auto blocked=(directory/"blocked").string();std::filesystem::create_directory(blocked);
    {std::ofstream s(std::filesystem::path(blocked)/"keep");s<<"original";}
    CHECK(!gallery::SavePreferences(blocked,decoded));CHECK(std::filesystem::exists(std::filesystem::path(blocked)/"keep"));
    auto long_path=(directory/std::string(250,'a')).string();
    {std::ofstream s(long_path);s<<"original";}
    CHECK(!gallery::SavePreferences(long_path,decoded));
    {std::ifstream s(long_path);std::string text;s>>text;CHECK(text=="original");}
    for(const auto &item:std::filesystem::directory_iterator(directory)) CHECK(item.path().filename().string().find(".tmp.")==std::string::npos);
    {std::ofstream s(path);s<<"version=1\ndark=bad\n";}
    auto previous=gallery::EncodePreferences(decoded);
    CHECK(gallery::LoadPreferences(path,decoded)==gallery::LoadResult::Invalid && gallery::EncodePreferences(decoded)==previous);
    std::filesystem::remove_all(directory);
    std::puts("PASS preferences: roundtrip, defaults/migration, corrupt/oversized/future input, transactional decode, atomic replace and failure preservation/cleanup");
}
#endif
}
int main() {
    CHECK(skui::GetVersionNumber()==SKUI_VERSION_NUMBER && std::string(skui::GetVersion())==SKUI_VERSION_STRING);
    Themes();Profiling();
#if defined(__unix__)
    Preferences();
#endif
}
