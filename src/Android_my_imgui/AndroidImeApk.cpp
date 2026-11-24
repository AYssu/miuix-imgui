#include "AndroidIme.h"
#include "skui_ime_dex.h"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;
namespace AndroidIme {
namespace {
using Clock = std::chrono::steady_clock;
constexpr const char *package = "skui.android.imehelper";
volatile std::sig_atomic_t cancelled = 0;
void Cancel(int) { cancelled = 1; }
bool Diagnostics() { return std::getenv("SKUI_IME_DIAGNOSTICS") != nullptr; }

std::string Hex(const std::string &value) {
    std::string result;
    for (unsigned char c : value) {
        result += "0123456789abcdef"[c >> 4];
        result += "0123456789abcdef"[c & 15];
    }
    return result;
}
void Error(const std::string &value) {
    std::fprintf(stderr, "[IME APK] %s\n", value.c_str());
    std::printf("SKUI E %s\n", Hex(value).c_str());
    std::fflush(stdout);
}
bool ParentClosed() {
    if (cancelled) return true;
    pollfd descriptor{STDIN_FILENO, POLLIN, 0};
    if (poll(&descriptor, 1, 0) <= 0) return false;
    if (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL)) return true;
    char c;
    return (descriptor.revents & POLLIN) && recv(STDIN_FILENO, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0;
}

// Execute fixed argument vectors, never a shell command. Installation/uninstallation
// run in this monitor, so package-manager work cannot block the renderer.
bool Run(std::vector<std::string> arguments, std::string &output) {
    int pipe[2];
    output.clear();
    if (pipe2(pipe, O_CLOEXEC) != 0) return false;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipe[0]);
    posix_spawn_file_actions_addclose(&actions, pipe[1]);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    std::vector<char *> argv;
    for (auto &value : arguments) argv.push_back(value.data());
    argv.push_back(nullptr);
    pid_t pid = -1;
    int result = posix_spawn(&pid, argv[0], &actions, &attributes, argv.data(), environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe[1]);
    if (result != 0) { close(pipe[0]); output = strerror(result); return false; }
    fcntl(pipe[0], F_SETFL, O_NONBLOCK);
    auto deadline = Clock::now() + std::chrono::seconds(30);
    int status = 0;
    bool done = false;
    for (;;) {
        char bytes[4096];
        ssize_t count;
        while ((count = read(pipe[0], bytes, sizeof(bytes))) > 0)
            if (output.size() < 65536) output.append(bytes, static_cast<size_t>(count));
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid || (waited < 0 && errno == ECHILD)) { done = true; break; }
        if (Clock::now() >= deadline) break;
        pollfd descriptor{pipe[0], POLLIN, 0};
        poll(&descriptor, 1, 50);
    }
    if (!done) {
        kill(-pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
        output += " (package-manager command timed out)";
    } else {
        char bytes[4096];
        ssize_t count;
        while ((count = read(pipe[0], bytes, sizeof(bytes))) > 0)
            if (output.size() < 65536) output.append(bytes, static_cast<size_t>(count));
    }
    close(pipe[0]);
    return done && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

struct TemporaryPackage {
    std::string user;
    bool owned = false;

    bool Installed(bool &installed) {
        std::string output;
        if (!Run({"/system/bin/pm", "list", "packages", "--user", user, package}, output)) return false;
        installed = false;
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line == std::string("package:") + package) installed = true;
        }
        return true;
    }
    bool Remove() {
        if (!owned) return true;
        owned = false;
        std::string output;
        Run({"/system/bin/am", "force-stop", "--user", user, package}, output);
        Run({"/system/bin/pm", "uninstall", "--user", user, package}, output);
        bool installed = true;
        if (!Installed(installed) || installed) {
            Error("临时输入辅助包卸载失败：" + std::string(package) + "；" + output);
            return false;
        }
        // An interrupted install can leave nothing to uninstall. Absence is the proof.
        if (Diagnostics()) std::fprintf(stderr, "[IME APK] temporary package removed for user %s\n", user.c_str());
        return true;
    }
    ~TemporaryPackage() { Remove(); }
};

bool Write(int fd, std::string &buffer) {
    if (buffer.empty()) return true;
    ssize_t count = send(fd, buffer.data(), buffer.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
    if (count > 0) buffer.erase(0, static_cast<size_t>(count));
    return count > 0 || (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR));
}
void FlushParent(std::string &buffer) {
    auto deadline = Clock::now() + std::chrono::seconds(2);
    while (!buffer.empty() && Clock::now() < deadline) {
        if (!Write(STDOUT_FILENO, buffer)) break;
        if (!buffer.empty()) {
            pollfd descriptor{STDOUT_FILENO, POLLOUT, 0};
            poll(&descriptor, 1, 20);
        }
    }
}
int Accept(int server, const std::string &token) {
    auto deadline = Clock::now() + std::chrono::seconds(20);
    while (Clock::now() < deadline && !ParentClosed()) {
        pollfd descriptor{server, POLLIN, 0};
        if (poll(&descriptor, 1, 100) <= 0) continue;
        int peer = accept4(server, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (peer < 0) continue;
        std::string hello;
        auto authentication_deadline = Clock::now() + std::chrono::seconds(1);
        while (hello.size() < 128 && Clock::now() < authentication_deadline && !ParentClosed()) {
            char c;
            auto count = recv(peer, &c, 1, MSG_DONTWAIT);
            if (count == 1) {
                if (c == '\n') break;
                hello += c;
            } else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                break;
            } else {
                pollfd pending{peer, POLLIN, 0};
                poll(&pending, 1, 20);
            }
        }
        if (hello == "SKUI K " + token) return peer;
        close(peer);
    }
    return -1;
}
} // namespace

int RunApkHost() {
    if (geteuid() != 0) { Error("临时输入辅助包需要 root 安装权限"); return 6; }
    // Ignore SIGPIPE so EOF still reaches the uninstall path when the renderer dies.
    std::signal(SIGPIPE, SIG_IGN);
    // Terminal signals may reach both renderer and monitor. The monitor must
    // finish package-manager work and uninstall instead of dying immediately.
    std::signal(SIGINT, Cancel);
    std::signal(SIGTERM, Cancel);
    int lock = open("/data/local/tmp/skui-ime-helper.lock", O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock < 0) { Error("无法创建临时输入安装锁"); return 6; }
    auto lock_deadline = Clock::now() + std::chrono::seconds(35);
    while (flock(lock, LOCK_EX | LOCK_NB) != 0) {
        if ((errno != EAGAIN && errno != EWOULDBLOCK) || Clock::now() >= lock_deadline || ParentClosed()) {
            close(lock); Error("临时输入辅助包仍在清理，请稍后重试"); return 6;
        }
        poll(nullptr, 0, 50);
    }
    // RAII order: the package is removed before releasing the cross-instance lock.
    struct Lock { int fd; ~Lock() { close(fd); } } locked{lock};
    TemporaryPackage temporary;
    std::string output;
    if (!Run({"/system/bin/am", "get-current-user"}, output)) { Error("无法确定 Android 当前用户"); return 6; }
    std::istringstream user_output(output);
    int user = -1;
    if (!(user_output >> user) || user < 0) { Error("无法确定 Android 当前用户：" + output); return 6; }
    temporary.user = std::to_string(user);
    bool installed = false;
    std::string existing;
    bool has_code = Run({"/system/bin/pm", "path", package}, existing) && existing.find("package:") != std::string::npos;
    if (!temporary.Installed(installed) || installed || has_code) {
        Error("已有同名输入辅助包，拒绝覆盖或卸载：" + std::string(package)); return 6;
    }
    if (ParentClosed()) return 0;
    char apk_path[] = "/data/local/tmp/skui-ime-XXXXXX";
    int apk = mkstemp(apk_path);
    if (apk < 0) { Error("无法释放临时输入 APK"); return 6; }
    size_t written = 0;
    while (written < sizeof(skui_ime_data::apk)) {
        ssize_t count = write(apk, skui_ime_data::apk + written, sizeof(skui_ime_data::apk) - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        written += static_cast<size_t>(count);
    }
    fchmod(apk, 0644);
    close(apk);
    if (written != sizeof(skui_ime_data::apk)) {
        unlink(apk_path); Error("写入临时输入 APK 失败"); return 6;
    }
    temporary.owned = true;
    if (Diagnostics()) std::fprintf(stderr, "[IME APK] installing temporary input package\n");
    bool success = Run({"/system/bin/pm", "install", "--user", temporary.user, apk_path}, output);
    unlink(apk_path);
    if (!success || !temporary.Installed(installed) || !installed) {
        Error("安装临时输入辅助包失败：" + output); return 6;
    }
    if (ParentClosed()) return 0;
    int server = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = sizeof(address);
    if (server < 0 || bind(server, reinterpret_cast<sockaddr *>(&address), size) != 0
            || listen(server, 4) != 0 || getsockname(server, reinterpret_cast<sockaddr *>(&address), &size) != 0) {
        if (server >= 0) close(server);
        Error("无法建立本机输入连接"); return 6;
    }
    unsigned char random[16];
    if (getrandom(random, sizeof(random), 0) != sizeof(random)) {
        close(server); Error("无法生成输入连接令牌"); return 6;
    }
    std::string token = Hex(std::string(reinterpret_cast<char *>(random), sizeof(random)));
    success = Run({"/system/bin/am", "start", "--user", temporary.user, "-n",
            std::string(package) + "/skui.android.ImeActivity", "--activity-no-animation",
            "--ei", "port", std::to_string(ntohs(address.sin_port)), "--es", "token", token}, output);
    int peer = success && !ParentClosed() ? Accept(server, token) : -1;
    close(server);
    if (peer < 0) { if (!ParentClosed()) Error("临时输入窗口连接失败：" + output); return 6; }
    if (Diagnostics()) std::fprintf(stderr, "[IME APK] registered activity connected\n");

    std::string to_app, to_parent, incoming, completed;
    bool parent_closed = false;
    bool healthy = true;
    while (completed.empty()) {
        if (cancelled) { parent_closed = true; break; }
        pollfd descriptors[2] = {{STDIN_FILENO, static_cast<short>(POLLIN | (to_parent.empty() ? 0 : POLLOUT)), 0},
                {peer, static_cast<short>(POLLIN | (to_app.empty() ? 0 : POLLOUT)), 0}};
        if (poll(descriptors, 2, 100) < 0) { if (errno == EINTR) continue; healthy = false; break; }
        if (descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char bytes[4096];
            auto count = recv(STDIN_FILENO, bytes, sizeof(bytes), MSG_DONTWAIT);
            if (count > 0) to_app.append(bytes, static_cast<size_t>(count));
            else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                if (Diagnostics()) std::fprintf(stderr, "[IME APK] renderer disconnected\n");
                parent_closed = true; break;
            }
        }
        if (descriptors[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            char bytes[4096];
            auto count = recv(peer, bytes, sizeof(bytes), MSG_DONTWAIT);
            if (count > 0) {
                incoming.append(bytes, static_cast<size_t>(count));
                size_t newline;
                while ((newline = incoming.find('\n')) != std::string::npos) {
                    auto line = incoming.substr(0, newline + 1);
                    incoming.erase(0, newline + 1);
                    if (line.compare(0, 7, "SKUI D ") == 0) completed = line;
                    else to_parent += line;
                }
            } else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                if (Diagnostics()) std::fprintf(stderr, "[IME APK] activity disconnected\n");
                break;
            }
        }
        if (!Write(peer, to_app) || !Write(STDOUT_FILENO, to_parent)) { healthy = false; break; }
        if (to_app.size() + to_parent.size() + incoming.size() > 262144) { healthy = false; break; }
    }
    close(peer);
    FlushParent(to_parent);
    if (!healthy && !parent_closed) Error("临时输入连接中断");
    bool removed = temporary.Remove();
    if (!completed.empty()) {
        to_parent = completed;
        FlushParent(to_parent);
    }
    return removed && healthy ? 0 : 6;
}
} // namespace AndroidIme
