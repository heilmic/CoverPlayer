#include "platform/sdl/playback_power.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif
namespace coverplayer::platform {
namespace {
namespace fs = std::filesystem;
std::string root() {
    const char* value = std::getenv("COVERPLAYER_POWER_TEST_ROOT");
    return value ? value : "";
}
std::string path(const char* p) { return root() + p; }
long readNumber(const std::string& p) { long n = -1; std::ifstream(p) >> n; return n; }
bool writeNumber(const std::string& p, long n) { std::ofstream f(p); f << n << '\n'; return bool(f); }
void removeFile(const std::string& p) { std::error_code ec; fs::remove(p, ec); }
[[maybe_unused]] std::string state(long owner) { return path("/tmp/coverplayer-power-") + std::to_string(owner); }
bool alive(long owner) {
#ifndef _WIN32
    if (kill(static_cast<pid_t>(owner), 0) != 0) return false;
    std::ifstream f("/proc/" + std::to_string(owner) + "/stat");
    std::string s; std::getline(f, s); const auto end = s.rfind(')');
    return end != std::string::npos && end + 2 < s.size() && s[end + 2] != 'Z';
#else
    static_cast<void>(owner);
    return false;
#endif
}
[[maybe_unused]] bool contains(const std::string& p, const std::string& text) {
    std::ifstream f(p, std::ios::binary);
    const std::string s{std::istreambuf_iterator<char>(f), {}};
    return s.find(text) != std::string::npos;
}
void inhibit(const std::string& backend, long owner, bool enable) {
    if (backend == "knulli") {
        const auto p = path("/var/run/battery-saver/coverplayer-") + std::to_string(owner) + ".pause";
        if (enable) writeNumber(p, owner); else removeFile(p);
    } else if (backend == "muos") {
        for (const auto* name : {"/run/muos/idle_game_inhibit", "/run/muos/idle_sleep_inhibit"}) {
            const auto p = path(name); const auto previous = readNumber(p);
            if (enable) {
                // These slots are shared with muOS/game launchers. Never steal a live owner's slot.
                if (previous == owner || previous < 1 || !alive(previous)) writeNumber(p, owner);
            } else if (previous == owner) removeFile(p);
        }
    }
}
long getBrightness(const std::string& backlight) {
    if (backlight != "knulli-command") return readNumber(backlight);
    FILE* pipe = popen("/usr/bin/timeout 3 /usr/bin/knulli-brightness 2>/dev/null", "r");
    if (!pipe) return -1;
    long value = -1; const int count = fscanf(pipe, "%ld", &value);
    const int status = pclose(pipe);
    return count == 1 && status == 0 ? value : -1;
}
bool brightness(const std::string& backend, const std::string& backlight, int value) {
    if (backend == "knulli" && backlight != "knulli-command") return writeNumber(backlight, value);
    if (backend == "knulli") return std::system(("/usr/bin/timeout 3 /usr/bin/knulli-brightness " +
        std::to_string(value) + " >/dev/null 2>&1").c_str()) == 0;
    // Only enabled after verifying transient support; user brightness is never saved/changed.
    if (backend == "muos") return std::system((path("/opt/muos/script/device/bright.sh") +
        " " + std::to_string(value) + " transient >/dev/null 2>&1").c_str()) == 0;
    return false;
}
}
PlaybackPower::PlaybackPower(bool foreground) : foreground_(foreground) {
#ifndef _WIN32
    owner_ = getpid(); runtime_ = state(owner_);
    if (fs::exists(path("/etc/idlewatcher/idlewatcher.conf")) && fs::is_directory(path("/var/run/battery-saver"))) {
        backend_ = "knulli";
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(path("/sys/class/backlight"), ec)) {
            if (fs::exists(entry.path() / "brightness")) { backlight_ = (entry.path() / "brightness").string(); break; }
        }
        if (backlight_.empty() && fs::exists(path("/usr/bin/knulli-brightness"))) backlight_ = "knulli-command";
    } else if (fs::is_directory(path("/run/muos")) &&
        contains(path("/opt/muos/script/device/bright.sh"), "transient") &&
        contains(path("/opt/muos/frontend/muhotkey"), "idle_game_inhibit") &&
        contains(path("/opt/muos/frontend/muhotkey"), "idle_sleep_inhibit")) {
        backend_ = "muos";
        backlight_ = path("/opt/muos/config/settings/general/brightness");
    }
    if (const char* value = std::getenv("COVERPLAYER_DIM_SECONDS")) {
        char* end = nullptr; const long n = std::strtol(value, &end, 10);
        if (end != value && *end == '\0') timeout_ = static_cast<std::uint32_t>(std::clamp(n, 1L, 3600L)) * 1000;
    }
#endif
    lastActivity_ = SDL_GetTicks();
}
PlaybackPower::~PlaybackPower() {
    wake();
    if (!backend_.empty()) inhibit(backend_, owner_, false);
    if (started_) removeFile(runtime_ + ".live");
}
void PlaybackPower::playback(bool active) {
    if (backend_.empty() || active == active_) return;
    active_ = active;
    if (active) {
        lastActivity_ = SDL_GetTicks();
#ifndef _WIN32
        if (!started_) {
            if (!writeNumber(runtime_ + ".live", owner_)) { active_ = false; return; }
            const std::string owner = std::to_string(owner_);
            const pid_t child = fork();
            if (child == 0) {
                execl("/proc/self/exe", "coverplayer", "--playback-power", owner.c_str(), static_cast<char*>(nullptr));
                _exit(127);
            }
            if (child < 0) { removeFile(runtime_ + ".live"); active_ = false; return; }
            started_ = true;
        }
#endif
        inhibit(backend_, owner_, true);
    } else {
        wake(); inhibit(backend_, owner_, false);
    }
}
bool PlaybackPower::activity() {
    lastActivity_ = SDL_GetTicks();
    const bool wasDark = dark_; wake(); return wasDark;
}
void PlaybackPower::wake() {
    if (!dark_) return;
    if ((backend_ == "knulli" && getBrightness(backlight_) > 0) || brightness(backend_, backlight_, saved_)) {
        dark_ = false; saved_ = -1; removeFile(runtime_ + ".brightness");
    }
}
void PlaybackPower::tick(std::uint32_t now) {
    if (!active_) return;
    if (now - lastCheck_ >= 2000) { inhibit(backend_, owner_, true); lastCheck_ = now; }
    if (!foreground_ || dark_ || now - lastActivity_ < timeout_ || backlight_.empty()) return;
    const auto level = getBrightness(backlight_);
    if (level <= 0 || level > 65535) return;
    // Journal before dimming, so the watcher can restore after SIGKILL as well.
    if (!writeNumber(runtime_ + ".brightness", level)) return;
    saved_ = static_cast<int>(level);
    if (brightness(backend_, backlight_, 0)) dark_ = true;
    else removeFile(runtime_ + ".brightness");
}
int PlaybackPower::watch(long owner) {
#ifndef _WIN32
    if (owner <= 1) return 1;
    PlaybackPower device(false);
    const auto runtime = state(owner);
    while (alive(owner) && fs::exists(runtime + ".live")) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto saved = readNumber(runtime + ".brightness");
    if (!device.backend_.empty()) {
        // Do not overwrite a brightness adjustment made outside CoverPlayer.
        if (saved > 0 && saved <= 65535 &&
            (device.backend_ == "muos" || getBrightness(device.backlight_) == 0))
            brightness(device.backend_, device.backlight_, static_cast<int>(saved));
        inhibit(device.backend_, owner, false);
    }
    removeFile(runtime + ".brightness"); removeFile(runtime + ".live");
#else
    static_cast<void>(owner);
#endif
    return 0;
}
}
