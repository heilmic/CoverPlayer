#include "platform/sdl/playback_power.hpp"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
namespace fs = std::filesystem;
using coverplayer::platform::PlaybackPower;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void put(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream(p) << s; }
long number(const fs::path& p) { long n = -1; std::ifstream(p) >> n; return n; }
void settle() { std::this_thread::sleep_for(std::chrono::milliseconds(600)); }
int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--playback-power") return PlaybackPower::watch(std::stol(argv[2]));
    if (argc == 2 && std::string(argv[1]) == "--crash-child") {
        PlaybackPower power; power.playback(true); power.tick(SDL_GetTicks() + 61000);
        while (true) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (argc == 2 && std::string(argv[1]) == "--knulli-hardware-check") {
        PlaybackPower power;
        power.playback(true);
        power.tick(SDL_GetTicks() + 61000);
        require(power.dark(), "real backlight did not enter dim state");
        require(fs::exists("/var/run/battery-saver/coverplayer-" + std::to_string(getpid()) + ".pause"), "real inhibitor missing");
        std::cout << "Dimmed backlight: " << std::flush;
        require(std::system("/usr/bin/knulli-brightness") == 0, "brightness read failed");
        require(std::system("/etc/idlewatcher/idle.d/01idle-hooks idle") == 0, "idle hook failed");
        require(std::system("/etc/idlewatcher/extended.d/01extended-hooks extended") == 0, "extended hook failed");
        std::cout << "Mute after both idle hooks: " << std::flush;
        std::system("pactl get-sink-mute @DEFAULT_SINK@");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        power.activity(); power.playback(false);
        std::cout << "Restored backlight: " << std::flush;
        std::system("/usr/bin/knulli-brightness");
        require(!fs::exists("/var/run/battery-saver/coverplayer-" + std::to_string(getpid()) + ".pause"), "real inhibitor left behind");
        return 0;
    }
    const auto base = fs::temp_directory_path() / ("coverplayer-power-test-" + std::to_string(getpid()));
    try {
        fs::create_directories(base / "tmp");
        setenv("COVERPLAYER_POWER_TEST_ROOT", base.c_str(), 1);
        const auto backlight = base / "sys/class/backlight/test/brightness";
        put(base / "etc/idlewatcher/idlewatcher.conf", "idle=300\n");
        fs::create_directories(base / "var/run/battery-saver");
        put(backlight, "123\n");
        const auto marker = base / ("var/run/battery-saver/coverplayer-" + std::to_string(getpid()) + ".pause");
        {
            PlaybackPower power;
            power.tick(SDL_GetTicks() + 61000);
            require(!fs::exists(marker) && !power.dark(), "idle app blocked standby");
            power.playback(true);
            require(fs::exists(marker), "no playback inhibitor");
            power.tick(SDL_GetTicks() + 59000); require(!power.dark(), "dimmed too early");
            power.tick(SDL_GetTicks() + 61000); require(power.dark() && number(backlight) == 0, "did not dim fully");
            require(power.activity() && number(backlight) == 123, "wake did not restore brightness");
            power.tick(SDL_GetTicks() + 61000); power.playback(false);
            require(!power.dark() && number(backlight) == 123 && !fs::exists(marker), "pause did not release/restore");
            power.tick(SDL_GetTicks() + 120000);
            require(!fs::exists(marker) && !power.dark(), "inactive tick renewed standby inhibitor");
            power.playback(true); power.tick(SDL_GetTicks() + 61000);
            put(backlight, "77\n"); power.activity();
            require(number(backlight) == 77, "overwrote external brightness");
        }
        settle();
        {
            PlaybackPower background(false); background.playback(true); background.tick(SDL_GetTicks() + 61000);
            require(!background.dark() && number(backlight) == 77, "background playback dimmed another app");
        }
        settle();
        put(backlight, "123\n");
        const auto child = fork();
        if (child == 0) { execl("/proc/self/exe", "power-test", "--crash-child", static_cast<char*>(nullptr)); _exit(127); }
        require(child > 0, "fork failed");
        for (int i = 0; i < 30 && number(backlight) != 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        require(number(backlight) == 0, "child did not dim");
        kill(child, SIGKILL); waitpid(child, nullptr, 0); settle();
        require(number(backlight) == 123, "crash cleanup did not restore");
        require(!fs::exists(base / ("var/run/battery-saver/coverplayer-" + std::to_string(child) + ".pause")), "crash left inhibitor");
        fs::remove(base / "etc/idlewatcher/idlewatcher.conf");
        put(base / "opt/muos/frontend/muhotkey", "idle_game_inhibit idle_sleep_inhibit");
        put(base / "opt/muos/config/settings/general/brightness", "100\n");
        const auto script = base / "opt/muos/script/device/bright.sh";
        put(script, "#!/bin/sh\n# transient\nprintf '%s\\n' \"$1\" > \"$COVERPLAYER_POWER_TEST_ROOT/muos-hardware\"\n");
        fs::permissions(script, fs::perms::owner_all);
        fs::create_directories(base / "run/muos");
        {
            PlaybackPower power; power.playback(true);
            require(number(base / "run/muos/idle_sleep_inhibit") == getpid(), "muOS sleep not inhibited");
            power.tick(SDL_GetTicks() + 61000); require(power.dark() && number(base / "muos-hardware") == 0, "muOS dim failed");
            power.activity(); require(number(base / "muos-hardware") == 100, "muOS restore failed");
            require(number(base / "opt/muos/config/settings/general/brightness") == 100, "muOS config modified");
            // Another live process owns the slot: do not overwrite or delete it.
            put(base / "run/muos/idle_game_inhibit", std::to_string(getppid()));
            power.tick(SDL_GetTicks() + 2000); power.playback(false);
            require(number(base / "run/muos/idle_game_inhibit") == getppid(), "stole muOS slot");
            require(!fs::exists(base / "run/muos/idle_sleep_inhibit"), "muOS inhibitor not released");
        }
        settle(); fs::remove_all(base);
        std::cout << "Playback power: dim/wake, pause, background, external brightness, SIGKILL cleanup, muOS transient/ownership passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
