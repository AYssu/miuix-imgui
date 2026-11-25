#include <skui/router.h>
#include <cstdio>
int main() {
    skui::Router router;int drawn=0;
    router.Register("home",[&](const skui::Route &) {++drawn;});
    router.Register("other",[&](const skui::Route &) {++drawn;});
    router.Reset("home");router.Draw();router.Push("other");router.Draw();router.Back();
    bool ok=drawn==2 && router.Current()->path=="home";
    std::puts(ok ? "skui navigation package: no ImGui dependency" : "router failed");
    return ok ? 0 : 1;
}
