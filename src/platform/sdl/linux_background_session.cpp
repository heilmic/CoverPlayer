#include "platform/sdl/linux_background_session.hpp"

#include <SDL.h>

#include <fstream>
#include <optional>

#ifndef _WIN32
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <sstream>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
#endif

namespace coverplayer::platform {
namespace {

#ifndef _WIN32
std::string resolveLockFilePath() {
    char* preferencePath = SDL_GetPrefPath("CoverPlayer", "CoverPlayer");
    if (preferencePath == nullptr) return {};
    std::string path = preferencePath;
    SDL_free(preferencePath);
    return path + "background-audio-v1.txt";
}

struct LockState {
    long pid = 0;
    std::string mediaPath;
};

std::optional<LockState> readLockState(const std::string& path) {
    std::ifstream input(path);
    std::string marker;
    LockState state;
    if (!(input >> marker >> state.pid) || marker != "PID") return std::nullopt;
    input.ignore(1);
    if (!(input >> marker) || marker != "PATH") return std::nullopt;
    input.ignore(1);
    // Read to end-of-file rather than std::getline's up-to-the-first-newline:
    // PATH is always the last field, and a POSIX filename may legally
    // contain an embedded newline, which getline would otherwise silently
    // truncate the path at.
    std::ostringstream rest;
    rest << input.rdbuf();
    state.mediaPath = rest.str();
    while (!state.mediaPath.empty() && state.mediaPath.back() == '\n') state.mediaPath.pop_back();
    return state;
}

void writeLockFile(long pid, const std::string& mediaPath) {
    const auto lockFilePath = resolveLockFilePath();
    if (lockFilePath.empty()) return;
    std::ofstream output(lockFilePath, std::ios::trunc);
    output << "PID " << pid << '\n' << "PATH " << mediaPath << '\n';
}

bool processAlive(long pid) {
    return pid > 0 && (::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM);
}

std::string readCommandLine(long pid) {
    std::ifstream input("/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

bool isRunningHelper(long pid) {
    return processAlive(pid) && LinuxBackgroundSession::isBackgroundHelperCommandLine(readCommandLine(pid));
}

std::string runCommand(const std::string& command) {
    FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r");
    if (pipe == nullptr) return {};
    std::string output;
    char buffer[1024]{};
    while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) output += buffer;
    pclose(pipe);
    return output;
}

int duckTargetPercent() {
    if (const char* value = std::getenv("COVERPLAYER_DUCK_PERCENT")) {
        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        if (end != value) return static_cast<int>(std::clamp(parsed, 0L, 100L));
    }
    return 50;
}

// Every PulseAudio sink input except this binary's own - a game, sound
// effects, EmulationStation itself. Knulli actually runs PipeWire with its
// `pipewire-pulse` PulseAudio-compatibility shim, not real PulseAudio, and
// the two disagree on which property carries the owning process's name:
// real PulseAudio's client library fills in `application.process.binary`,
// but PipeWire's ALSA-compat layer never sets that property at all -
// instead it exposes `application.name = "PipeWire ALSA [coverplayer]"`
// and `node.name = "alsa_playback.coverplayer"`. Checking only the former
// (as an earlier version of this function did) matches nothing at all on
// Knulli's real audio stack, silently misidentifying this process's own
// stream as "other" and ducking/never-restoring it right along with
// everything else - confirmed via a real `pactl list sink-inputs` capture
// on-device. Checking all three covers both stacks.
std::vector<int> otherSinkInputIndices() {
    const auto listing = runCommand("/usr/bin/pactl list sink-inputs");
    std::vector<int> indices;
    std::istringstream lines(listing);
    std::string line;
    int currentIndex = -1;
    bool currentIsOwn = false;
    const auto flush = [&]() {
        if (currentIndex >= 0 && !currentIsOwn) indices.push_back(currentIndex);
    };
    while (std::getline(lines, line)) {
        const auto marker = line.find("Sink Input #");
        if (marker != std::string::npos) {
            flush();
            currentIndex = std::atoi(line.c_str() + marker + 12);
            currentIsOwn = false;
            continue;
        }
        if ((line.find("application.process.binary") != std::string::npos ||
             line.find("application.name") != std::string::npos ||
             line.find("node.name") != std::string::npos) &&
            line.find("coverplayer") != std::string::npos) {
            currentIsOwn = true;
        }
    }
    flush();
    return indices;
}

void rampOtherAudio(int fromPercent, int toPercent, const std::function<void()>& onTick) {
    const auto indices = otherSinkInputIndices();
    if (indices.empty()) return;
    constexpr int steps = 6;
    constexpr auto stepDelay = std::chrono::milliseconds(120);
    for (int step = 1; step <= steps; ++step) {
        const int percent = fromPercent + (toPercent - fromPercent) * step / steps;
        for (const int index : indices) {
            runCommand("/usr/bin/pactl set-sink-input-volume " + std::to_string(index) + " " +
                std::to_string(percent) + "%");
        }
        if (step < steps) {
            std::this_thread::sleep_for(stepDelay);
            if (onTick) onTick();
        }
    }
}

// If a background helper is currently running, kills it and removes the
// lock file, waiting briefly to confirm it is actually gone before
// returning - so a caller that reopens the same file right after this can
// never end up racing the old helper for the same track. Returns the path
// it was playing.
std::optional<std::string> stopIfRunning(const std::string& lockFilePath) {
    if (lockFilePath.empty()) return std::nullopt;
    const auto existing = readLockState(lockFilePath);
    std::error_code removeError;
    std::filesystem::remove(std::filesystem::u8path(lockFilePath), removeError);
    if (!existing || !isRunningHelper(existing->pid)) return std::nullopt;
    // SIGKILL, not SIGTERM: it cannot be caught, blocked, or ignored by the
    // target for any reason, so this is guaranteed to actually end the
    // helper rather than merely asking it to - the helper has nothing to
    // flush that saveProgress() (every 5s) has not already persisted.
    ::kill(static_cast<pid_t>(existing->pid), SIGKILL);
    for (int attempt = 0; attempt < 40 && processAlive(existing->pid); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    // The killed helper never got a chance to restore ducked volumes
    // itself; do it here so a SIGKILL can never leave other audio stuck
    // quiet. Harmless if a replacement helper re-ducks moments later.
    rampOtherAudio(duckTargetPercent(), 100, {});
    return existing->mediaPath;
}
#endif

} // namespace

bool LinuxBackgroundSession::spawnBackgroundAudio(const std::vector<std::string>& trackPaths, std::size_t startIndex) {
#ifdef _WIN32
    static_cast<void>(trackPaths);
    static_cast<void>(startIndex);
    return false;
#else
    if (startIndex >= trackPaths.size()) return false;

    // Never let two helpers exist at once - a fresh background request
    // always replaces whatever was previously playing in the background.
    stopIfRunning(resolveLockFilePath());

    // Built before fork(): the child, between fork() and exec(), must do as
    // close to nothing but raw syscalls as possible. It is a single-threaded
    // copy of a process that may have been multi-threaded at the instant of
    // fork() (SDL's audio callback thread runs continuously during
    // playback); if that thread held the C library's malloc arena lock at
    // that instant, the child inherits it already held and would deadlock
    // on its first allocation. Building argv here means the child performs
    // none of its own.
    std::vector<std::string> argStorage;
    argStorage.reserve(trackPaths.size() + 3);
    argStorage.emplace_back("coverplayer");
    argStorage.emplace_back("--background-audio");
    argStorage.push_back(std::to_string(startIndex));
    for (const auto& path : trackPaths) argStorage.push_back(path);
    std::vector<char*> argv;
    argv.reserve(argStorage.size() + 1);
    for (auto& arg : argStorage) argv.push_back(arg.data());
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // Child: detach from the controlling terminal, then immediately
        // exec a fresh process image. Nothing here touches SDL or any
        // state duplicated from the parent - the fork-broken copy of that
        // state is discarded, not used, the moment exec() replaces it.
        // setsid()/open()/dup2()/close()/execv()/_exit() are all raw
        // syscall wrappers - no stdio, no allocation, unlike the
        // freopen()-based version this replaced.
        ::setsid();
        const int nullRead = ::open("/dev/null", O_RDONLY);
        if (nullRead >= 0) { ::dup2(nullRead, STDIN_FILENO); ::close(nullRead); }
        const int nullWrite = ::open("/dev/null", O_WRONLY);
        if (nullWrite >= 0) { ::dup2(nullWrite, STDOUT_FILENO); ::dup2(nullWrite, STDERR_FILENO); ::close(nullWrite); }
        ::execv("/proc/self/exe", argv.data());
        ::_exit(127); // exec failed
    }
    // Recorded here, using the pid fork() just returned, rather than
    // leaving the child to record itself once it gets around to it: a
    // caller that reopens CoverPlayer immediately after this must always
    // find this lock file, even if the child has not yet reached
    // mpg123_open() (slower now that the resync search can scan up to
    // 8 MiB) - otherwise takeOverIfRunning() would see no lock file, assume
    // nothing is running, and a second, uncoordinated helper could end up
    // playing alongside this one.
    writeLockFile(pid, trackPaths[startIndex]);
    return true;
#endif
}

std::optional<std::string> LinuxBackgroundSession::takeOverIfRunning() {
#ifdef _WIN32
    return std::nullopt;
#else
    return stopIfRunning(resolveLockFilePath());
#endif
}

void LinuxBackgroundSession::recordRunning(const std::string& mediaPath) {
#ifndef _WIN32
    writeLockFile(static_cast<long>(::getpid()), mediaPath);
#else
    static_cast<void>(mediaPath);
#endif
}

bool LinuxBackgroundSession::isBackgroundHelperCommandLine(const std::string& commandLine) {
    const auto firstEnd = commandLine.find('\0');
    if (firstEnd == std::string::npos) return false;
    const auto secondEnd = commandLine.find('\0', firstEnd + 1);
    if (secondEnd == std::string::npos) return false;
    return commandLine.compare(0, firstEnd, "coverplayer") == 0 &&
        commandLine.compare(firstEnd + 1, secondEnd - firstEnd - 1, "--background-audio") == 0;
}

void LinuxBackgroundSession::duckOtherAudio(const std::function<void()>& onTick) {
#ifndef _WIN32
    rampOtherAudio(100, duckTargetPercent(), onTick);
#else
    static_cast<void>(onTick);
#endif
}

void LinuxBackgroundSession::restoreOtherAudio(const std::function<void()>& onTick) {
#ifndef _WIN32
    rampOtherAudio(duckTargetPercent(), 100, onTick);
#else
    static_cast<void>(onTick);
#endif
}

} // namespace coverplayer::platform
