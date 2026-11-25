#include "AndroidIme.h"
#include "skui/skui.h"
#include "skui_ime_dex.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sstream>
#include <string>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;
namespace AndroidIme {
namespace {
pid_t child = -1;
int channel = -1;
unsigned long long session = 0;
ImGuiID target = 0;
int active_frame = -1, selection_start = 0, selection_end = 0;
float keyboard_height = 0;
bool pending = false, finished = false, ready = false;
bool probe_done = false, probe_supported = false;
bool using_apk = false, direct_unavailable = false, fallback_requested = false;
bool diagnosing = false, terminal_received = false;
bool cleanup_failed = false;
std::string received, outgoing, text, error, open_command;
size_t open_capacity = 0;
struct Child { pid_t pid; bool apk; };
std::vector<Child> retired;
std::chrono::steady_clock::time_point launched;

std::string Hex(const char *value) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
        result += digits[*p >> 4];
        result += digits[*p & 15];
    }
    return result;
}
bool Unhex(const std::string &value, std::string &result) {
    if (value.size() % 2)
        return false;
    result.clear();
    auto digit = [](char c) {
        return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    };
    for (size_t i = 0; i < value.size(); i += 2) {
        int a = digit(value[i]), b = digit(value[i + 1]);
        if (a < 0 || b < 0 || (a == 0 && b == 0))
            return false;
        result += static_cast<char>((a << 4) | b);
    }
    return true;
}
// ImGui offsets are UTF-8 bytes; Android selection offsets are UTF-16 units.
int ConvertOffset(const char *value, int offset, bool to_utf16) {
    int bytes = 0, units = 0;
    while (value[bytes] && (to_utf16 ? bytes : units) < offset) {
        unsigned char c = value[bytes];
        int count = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        int next_units = units + (count == 4 ? 2 : 1);
        if (!to_utf16 && next_units > offset)
            break;
        for (int n = 1; n < count; ++n)
            if (!value[bytes + n])
                return to_utf16 ? units : bytes;
        bytes += count;
        units = next_units;
    }
    return to_utf16 ? units : bytes;
}
void Flush() {
    while (channel >= 0 && !outgoing.empty()) {
        ssize_t count = send(channel, outgoing.data(), outgoing.size(), MSG_NOSIGNAL);
        if (count > 0)
            outgoing.erase(0, static_cast<size_t>(count));
        else if (count < 0 && errno == EINTR)
            continue;
        else
            break;
    }
}
void Send(std::string command) {
    if (channel < 0)
        return;
    if (outgoing.size() + command.size() > 65536) {
        error = "系统输入连接繁忙，请关闭输入框后重试";
        return;
    }
    outgoing += command + "\n";
    Flush();
}
void Reap() {
    retired.erase(std::remove_if(retired.begin(), retired.end(), [](const Child &process) {
        int status = 0;
        auto result = waitpid(process.pid, &status, WNOHANG);
        if (result == process.pid && process.apk && (!WIFEXITED(status) || WEXITSTATUS(status) != 0))
            cleanup_failed = true;
        return result == process.pid || (result < 0 && errno == ECHILD);
    }), retired.end());
}
void CloseTransport() {
    if (channel >= 0) {
        Send("Q");
        shutdown(channel, SHUT_WR);
        close(channel);
        channel = -1;
    }
    if (child > 0) retired.push_back({child, using_apk});
    child = -1;
    ready = false;
    received.clear();
    outgoing.clear();
    keyboard_height = 0;
}
bool Start(bool temporary_apk) {
    if (channel >= 0)
        return true;
    if (geteuid() != 0) {
        error = "原生悬浮输入需要以 root 运行可执行文件";
        return false;
    }
    int dex = -1;
    if (!temporary_apk) {
        dex = static_cast<int>(syscall(__NR_memfd_create, "skui-ime.dex", 2 /* MFD_ALLOW_SEALING */));
        if (dex < 0) {
            error = "无法创建系统输入桥接文件";
            return false;
        }
        size_t written = 0;
        while (written < sizeof(skui_ime_data::dex)) {
            auto count = write(dex, skui_ime_data::dex + written, sizeof(skui_ime_data::dex) - written);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                break;
            written += static_cast<size_t>(count);
        }
        fchmod(dex, 0444);
        fcntl(dex, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);
        if (written != sizeof(skui_ime_data::dex)) {
            close(dex);
            error = "无法写入系统输入桥接文件";
            return false;
        }
    }
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets)) {
        if (dex >= 0) close(dex);
        error = "无法创建系统输入通信通道";
        return false;
    }
    char executable[4096]{};
    auto length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, sockets[0]);
    posix_spawn_file_actions_addclose(&actions, sockets[1]);
    // Expected framework rejections are handled over the protocol, not printed
    // into the user's terminal. Explicit diagnostics retain the Java stack.
    const bool diagnostics = diagnosing || std::getenv("SKUI_IME_DIAGNOSTICS");
    if (!diagnostics)
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    std::vector<std::string> environment;
    for (char **p = environ; *p; ++p)
        if (std::strncmp(*p, "CLASSPATH=", 10) != 0 &&
            std::strncmp(*p, "SKUI_IME_DIAGNOSTICS=", 21) != 0)
            environment.emplace_back(*p);
    if (diagnostics) environment.emplace_back("SKUI_IME_DIAGNOSTICS=1");
    if (dex >= 0) environment.emplace_back("CLASSPATH=/proc/self/fd/" + std::to_string(dex));
    std::vector<char *> env;
    for (auto &value : environment)
        env.push_back(value.data());
    env.push_back(nullptr);
    char direct_mode[] = "--ime-host", apk_mode[] = "--ime-apk-host";
    char *args[] = {executable, temporary_apk ? apk_mode : direct_mode, nullptr};
    int result = length > 0 ? posix_spawn(&child, executable, &actions, nullptr, args, env.data()) : EINVAL;
    posix_spawn_file_actions_destroy(&actions);
    close(sockets[1]);
    if (dex >= 0) close(dex);
    if (result) {
        close(sockets[0]);
        child = -1;
        error = "启动系统输入失败: " + std::string(strerror(result));
        return false;
    }
    channel = sockets[0];
    fcntl(channel, F_SETFL, fcntl(channel, F_GETFL) | O_NONBLOCK);
    received.clear();
    outgoing.clear();
    ready = false;
    using_apk = temporary_apk;
    probe_done = probe_supported = terminal_received = false;
    launched = std::chrono::steady_clock::now();
    Send("P");
    return true;
}
void Message(const std::string &line) {
    if (line.compare(0, 5, "SKUI ") != 0)
        return;
    std::istringstream input(line.substr(5));
    char kind = 0;
    input >> kind;
    if (kind == 'R') {
        ready = true;
        return;
    }
    if (kind == 'A' || kind == 'F') {
        probe_done = true;
        probe_supported = kind == 'A';
        if (probe_supported && target && !open_command.empty()) Send(open_command);
        if (!probe_supported && !using_apk && !diagnosing) fallback_requested = true;
        return;
    }
    if (kind == 'E') {
        std::string payload, detail;
        input >> payload;
        if (Unhex(payload, detail)) {
            error = "系统输入错误: " + detail;
            if (!using_apk && !diagnosing) fallback_requested = true;
        }
        return;
    }
    unsigned long long incoming_session;
    if (!(input >> incoming_session) || incoming_session != session)
        return;
    if (kind == 'D' && using_apk) terminal_received = true;
    if (!target) return;
    if (kind == 'T') {
        int start, end;
        std::string payload, value;
        if (!(input >> start >> end))
            return;
        input >> payload;
        if (!Unhex(payload, value))
            return;
        text = std::move(value);
        selection_start = ConvertOffset(text.c_str(), start, false);
        selection_end = ConvertOffset(text.c_str(), end, false);
        pending = true;
        open_command = "O " + std::to_string(session) + " " + std::to_string(open_capacity) + " " +
                std::to_string(start) + " " + std::to_string(end) + " " + Hex(text.c_str());
    } else if (kind == 'V') {
        int height;
        if (input >> height)
            keyboard_height = static_cast<float>(std::clamp(height, 0, 10000));
    } else if (kind == 'D' && !fallback_requested)
        finished = true;
}
void Handle(skui::TextInputEvent &event, void *) {
    if (event.active) {
        active_frame = ImGui::GetFrameCount();
        if (event.id != target) {
            // An APK owns one editing session. Its monitor finishes uninstalling
            // under a cross-process lock while the next renderer frame continues.
            if (using_apk && channel >= 0) CloseTransport();
            if (!Start(direct_unavailable)) {
                if (!direct_unavailable) {
                    direct_unavailable = true;
                    if (Start(true)) error.clear();
                }
            }
            if (channel < 0) {
                event.finished = true;
                return;
            }
            target = event.id;
            ++session;
            pending = finished = false;
            selection_start = event.selection_start;
            selection_end = event.selection_end;
            open_capacity = event.capacity;
            open_command = "O " + std::to_string(session) + " " + std::to_string(event.capacity) + " " +
                 std::to_string(ConvertOffset(event.buffer, selection_start, true)) + " " +
                 std::to_string(ConvertOffset(event.buffer, selection_end, true)) + " " + Hex(event.buffer);
            if (probe_done && probe_supported) Send(open_command);
        }
    }
    if (event.id != target)
        return;
    if (pending) {
        // The Java InputFilter enforces the same byte capacity. Never copy a
        // partial snapshot.
        if (text.size() < event.capacity) {
            std::memcpy(event.buffer, text.c_str(), text.size() + 1);
            event.selection_start = selection_start;
            event.selection_end = selection_end;
            event.changed = true;
        }
        pending = false;
    } else if (event.active && !finished &&
               (selection_start != event.selection_start || selection_end != event.selection_end)) {
        selection_start = event.selection_start;
        selection_end = event.selection_end;
        Send("S " + std::to_string(session) + " " +
             std::to_string(ConvertOffset(event.buffer, selection_start, true)) + " " +
             std::to_string(ConvertOffset(event.buffer, selection_end, true)));
    }
    if (finished) {
        event.finished = true;
        target = 0;
        finished = false;
        open_command.clear();
    }
}
} // namespace

int RunHost() {
    pid_t parent = getppid();
    if (geteuid() != 0 || setgid(1000) != 0 || setuid(1000) != 0)
        return 6;
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || getppid() != parent)
        return 6;
    execl("/system/bin/app_process", "app_process", "/system/bin", "--nice-name=skui-ime",
          "skui.android.NativeIme", static_cast<char *>(nullptr));
    return 6;
}
void Init() { skui::SetTextInputHandler(Handle); }
void Poll() {
    Reap();
    if (channel < 0)
        return;
    Flush();
    char buffer[4096];
    bool closed = false;
    for (;;) {
        auto count = read(channel, buffer, sizeof(buffer));
        if (count > 0)
            received.append(buffer, static_cast<size_t>(count));
        else if (count < 0 && errno == EINTR)
            continue;
        else {
            closed = count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK);
            break;
        }
        if (received.size() > 65536) {
            closed = true;
            break;
        }
    }
    size_t newline;
    while ((newline = received.find('\n')) != std::string::npos) {
        Message(received.substr(0, newline));
        received.erase(0, newline + 1);
    }
    bool timed_out = (!ready || !probe_done) && std::chrono::steady_clock::now() - launched
            > std::chrono::seconds(using_apk ? 60 : 8);
    if (!using_apk && !diagnosing && (fallback_requested || ((closed || timed_out) && target && !finished))) {
        direct_unavailable = true;
        fallback_requested = false;
        CloseTransport();
        finished = false;
        if (target && Start(true)) error.clear();
        else if (target) finished = true;
        return;
    }
    if (closed || timed_out || terminal_received) {
        if (error.empty() && target && !finished && !terminal_received)
            error = "系统输入连接已退出，请重新点击输入框";
        CloseTransport();
        if (target) finished = true;
    }
}
void EndFrame() {
    if (target && active_frame != ImGui::GetFrameCount()) {
        Send("H " + std::to_string(session));
        target = 0;
        pending = finished = false;
        keyboard_height = 0;
        open_command.clear();
    }
    Flush();
}
float KeyboardHeight() { return keyboard_height; }
int ActivityFd() {return channel;}
std::string ConsumeError() {
    std::string result;
    result.swap(error);
    return result;
}
int Diagnose(bool temporary_apk) {
    diagnosing = true;
    cleanup_failed = false;
    probe_done = probe_supported = false;
    error.clear();
    bool started = Start(temporary_apk);
    if (started) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(temporary_apk ? 60 : 8);
        while (!probe_done && channel >= 0 && std::chrono::steady_clock::now() < deadline) {
            Poll();
            usleep(10000);
        }
    }
    bool supported = started && probe_done && probe_supported;
    if (!error.empty())
        std::fprintf(stderr, "[IME] %s\n", error.c_str());
    Shutdown();
    supported = supported && !cleanup_failed && retired.empty();
    std::printf("[IME] %s window session: %s\n", temporary_apk ? "temporary APK" : "framework",
            supported ? "supported" : "unavailable");
    diagnosing = false;
    return supported ? 0 : 1;
}
void Shutdown() {
    skui::SetTextInputHandler(nullptr);
    CloseTransport();
    // Do not kill the APK monitor: it must observe EOF and finish uninstalling,
    // including when the renderer was terminated during installation.
    for (const auto &process : retired) {
        if (!process.apk) kill(process.pid, SIGTERM);
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(40);
    while (!retired.empty() && std::chrono::steady_clock::now() < deadline) {
        Reap();
        if (!retired.empty()) usleep(10000);
    }
    if (!retired.empty()) std::fprintf(stderr, "[IME] cleanup monitor is still completing package-manager work\n");
    target = 0;
    pending = finished = false;
    open_command.clear();
}
} // namespace AndroidIme
