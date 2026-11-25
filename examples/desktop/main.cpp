#include "gallery.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {
struct Wake {
    Uint32 type=static_cast<Uint32>(-1);
    std::atomic<bool> queued{false};
    static void Notify(void *data) {
        auto &self=*static_cast<Wake *>(data);
        if(self.type==static_cast<Uint32>(-1) || self.queued.exchange(true)) return;
        SDL_Event event{};event.type=self.type;
        if(SDL_PushEvent(&event)!=1) self.queued=false;
    }
};
struct Application {
    SDL_Window *window=nullptr;
    SDL_GLContext gl=nullptr;
    bool sdl=false,imgui=false,platform=false,renderer=false;
    Wake wake;
    ~Application() {
        // Join producers before destroying their event queue, then release GPU
        // objects while the host GL context is still current.
        gallery::Shutdown();gallery::SetWakeHandler(nullptr);
        if(renderer) ImGui_ImplOpenGL3_Shutdown();
        if(platform) ImGui_ImplSDL2_Shutdown();
        if(imgui) ImGui::DestroyContext();
        if(gl) SDL_GL_DeleteContext(gl);
        if(window) SDL_DestroyWindow(window);
        if(sdl) SDL_Quit();
    }
};
bool LoadFont(const char *requested) {
    const char *candidates[]={requested,"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc","/System/Library/Fonts/PingFang.ttc"};
    for(auto *path:candidates) {
        if(!path || !*path) continue;
        std::error_code error;
        if(std::filesystem::is_regular_file(path,error) &&
           ImGui::GetIO().Fonts->AddFontFromFileTTF(path,18)) return true;
        if(path==requested) return false;
    }
    ImGui::GetIO().Fonts->AddFontDefault();
    return true;
}
}
int main(int argc,char **argv) {
    const char *settings=nullptr,*font=nullptr;
    int frame_limit=0;bool continuous=false;
    for(int i=1;i<argc;++i) {
        if(std::strcmp(argv[i],"--continuous")==0) continuous=true;
        else if(std::strcmp(argv[i],"--help")==0) {
            std::puts("skui-gallery [--settings PATH] [--font TTF_OR_TTC] [--continuous] [--frames N]");return 0;
        } else if((std::strcmp(argv[i],"--settings")==0 || std::strcmp(argv[i],"--font")==0) && i+1<argc) {
            bool is_font=std::strcmp(argv[i],"--font")==0;
            if(is_font) font=argv[++i];else settings=argv[++i];
        } else if(std::strcmp(argv[i],"--frames")==0 && i+1<argc) {
            const char *text=argv[++i];auto parsed=std::from_chars(text,text+std::strlen(text),frame_limit);
            if(parsed.ec!=std::errc{} || *parsed.ptr || frame_limit<=0 || frame_limit>1000000) {
                std::fputs("Invalid --frames value\n",stderr);return 64;
            }
        } else {std::fprintf(stderr,"Unknown or incomplete argument: %s\n",argv[i]);return 64;}
    }
    Application app;
    app.sdl=true;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_GAMECONTROLLER)!=0) {
        std::fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;
    }
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI,"1");
#endif
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,2);
    const char *glsl="#version 150";
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    const char *glsl="#version 130";
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    app.window=SDL_CreateWindow("miuix-imgui Gallery",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,800,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!app.window) {std::fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());return 2;}
    app.gl=SDL_GL_CreateContext(app.window);
    if(!app.gl || SDL_GL_MakeCurrent(app.window,app.gl)!=0) {
        std::fprintf(stderr,"SDL_GL_CreateContext: %s\n",SDL_GetError());return 3;
    }
    bool vsync=SDL_GL_SetSwapInterval(1)==0;
    IMGUI_CHECKVERSION();ImGui::CreateContext();app.imgui=true;
    auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    if(!LoadFont(font)) {std::fprintf(stderr,"Cannot load font: %s\n",font);return 4;}
    app.platform=ImGui_ImplSDL2_InitForOpenGL(app.window,app.gl);
    if(!app.platform) return 5;
    app.renderer=ImGui_ImplOpenGL3_Init(glsl);
    if(!app.renderer) return 6;
    app.wake.type=SDL_RegisterEvents(1);
    gallery::SetWakeHandler(Wake::Notify,&app.wake);gallery::ConfigurePreferences(settings);
    using Clock=std::chrono::steady_clock;
    auto active_until=Clock::now()+std::chrono::milliseconds(250);
    auto &profiler=gallery::Profiler();
    bool running=true;int frames=0;int wait_ms=0;
    while(running) {
        auto process=[&](SDL_Event &event) {
            if(event.type==app.wake.type) app.wake.queued=false;
            else ImGui_ImplSDL2_ProcessEvent(&event);
            if(event.type==SDL_QUIT || (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_CLOSE &&
                                      event.window.windowID==SDL_GetWindowID(app.window))) running=false;
            active_until=Clock::now()+std::chrono::milliseconds(250);
        };
        SDL_Event event;
        bool have_event=wait_ms>0 && SDL_WaitEventTimeout(&event,wait_ms);
        auto frame_start=Clock::now();profiler.BeginFrame();
        if(have_event) process(event);
        while(SDL_PollEvent(&event)) process(event);
        if(!running) break;
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();profiler.EndInput();
        if(io.DisplaySize.x>0 && io.DisplaySize.y>0) gallery::Draw(&running,io.DisplaySize);
        ImGui::Render();profiler.EndBuild();
        int width=0,height=0;SDL_GL_GetDrawableSize(app.window,&width,&height);
        if(width>0 && height>0) {
            glViewport(0,0,width,height);glClearColor(.13f,.14f,.16f,1);glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());SDL_GL_SwapWindow(app.window);
        }
        profiler.EndFrame();gallery::FlushPreferences();
        if(frame_limit && ++frames>=frame_limit) break;
        auto request=skui::CurrentFrameRequest();
        bool active=continuous || request.continuous || io.WantTextInput || ImGui::IsAnyMouseDown() || Clock::now()<active_until;
        double elapsed=std::chrono::duration<double>(Clock::now()-frame_start).count();
        wait_ms=active ? 0 : static_cast<int>(std::ceil(1000*std::max(0.0,std::min(.1,request.wake_after_seconds)-elapsed)));
        // Offscreen or unsupported swap control must not produce a busy loop.
        if((!vsync || width<=0 || height<=0) && elapsed<1.0/60)
            SDL_Delay(static_cast<Uint32>(std::ceil(1000*(1.0/60-elapsed))));
    }
    gallery::FlushPreferences(true);
    return 0;
}
