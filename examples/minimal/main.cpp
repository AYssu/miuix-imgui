#include <cstdio>
#include <skui/skui.h>

// In a real host, call this between ImGui::NewFrame() and ImGui::Render().
void DrawSettings() {
    static bool enabled = true;
    static float volume = 50;
    skui::Section("设置");
    if (skui::BeginCard("settings")) {
        skui::Toggle("消息通知", &enabled);
        skui::Slider("音量", &volume, 0, 100, "%.0f %%");
        skui::Button("保存", skui::Tone::Primary, ImVec2(-1, 0));
    }
    skui::EndCard();
}

int main() {
    // Headless integration check. A host normally supplies its own window,
    // renderer, fonts and event loop instead of this one-frame setup.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(600, 800);
    io.DeltaTime = 1.f / 60;
    io.Fonts->AddFontDefault();
    // No renderer here: build a static atlas for this headless check only.
    // Real hosts use the texture protocol supplied by their renderer backend.
    unsigned char *pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    skui::Router router;
    skui::RouterView router_view;
    router.Register("/settings", [](const skui::Route &) { DrawSettings(); });
    router.Reset("/settings");
    skui::ApplyTheme({});
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(500, 650));
    ImGui::Begin("My app");
    router_view.Draw(router);
    ImGui::End();
    ImGui::Render();
    bool ok = ImGui::GetDrawData()->TotalVtxCount > 0;
    ImGui::DestroyContext();
    std::puts(ok ? "skui consumer: draw data generated" : "skui consumer: no draw data");
    return ok ? 0 : 1;
}
