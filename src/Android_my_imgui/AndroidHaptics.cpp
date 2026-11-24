#include "AndroidHaptics.h"
#include "skui/skui.h"
#include <chrono>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace AndroidHaptics {
namespace {
pid_t child=-1;
std::chrono::steady_clock::time_point last{};
void Emit(skui::Haptic event,void *) {
    Poll();auto now=std::chrono::steady_clock::now();
    if(child>0 || now-last<std::chrono::milliseconds(45)) return;
    const char *effect=event==skui::Haptic::Selection ? "2" : event==skui::Haptic::Confirm ? "5" : "0";
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/dev/null",O_WRONLY,0);
    posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
    char *args[]={const_cast<char *>("cmd"),const_cast<char *>("vibrator_manager"),const_cast<char *>("synced"),
        const_cast<char *>("-B"),const_cast<char *>("-d"),const_cast<char *>("skui-touch"),
        const_cast<char *>("prebaked"),const_cast<char *>("-b"),const_cast<char *>(effect),nullptr};
    pid_t pid=-1;
    if(posix_spawn(&pid,"/system/bin/cmd",&actions,nullptr,args,environ)==0) {child=pid;last=now;}
    posix_spawn_file_actions_destroy(&actions);
}
}
void Init() {skui::SetHapticHandler(Emit);}
void Poll() {
    if(child<=0) return;
    auto result=waitpid(child,nullptr,WNOHANG);
    if(result==child || (result<0 && errno==ECHILD)) child=-1;
}
void Shutdown() {
    skui::SetHapticHandler(nullptr);Poll();
    if(child>0) {kill(child,SIGTERM);while(waitpid(child,nullptr,0)<0 && errno==EINTR) {} child=-1;}
}
}
