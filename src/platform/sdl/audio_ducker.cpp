#include "platform/sdl/audio_ducker.hpp"
#include "platform/sdl/linux_background_session.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifndef _WIN32
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#endif

namespace coverplayer::platform {
std::vector<AudioStream> parseAudioStreams(const std::string& listing) {
    std::vector<AudioStream> streams;
    std::istringstream lines(listing);
    std::string line, binary, app, node, pid, serial, restoreKey;
    AudioStream current;
    bool valid = false;
    const auto flush = [&] {
        // Require an owning identity. Unknown streams are never changed.
        if (valid && !current.volume.empty() && (!binary.empty() || !app.empty() || !node.empty())) {
            current.identity = binary + "|" + app + "|" + node + "|" + pid + "|" + serial;
            const auto binaryName = binary.substr(binary.find_last_of('/') == std::string::npos ? 0 : binary.find_last_of('/') + 1);
            current.own = binaryName == "coverplayer" || app == "CoverPlayer" || app == "coverplayer" ||
                app == "PipeWire ALSA [coverplayer]" || node == "alsa_playback.coverplayer";
            current.restoreKey = restoreKey;
            streams.push_back(current);
        }
    };
    while (std::getline(lines, line)) {
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line.erase(0, first);
        if (line.rfind("Sink Input #", 0) == 0) {
            flush(); current = {}; binary.clear(); app.clear(); node.clear(); pid.clear(); serial.clear(); restoreKey.clear();
            std::istringstream index(line.substr(12));
            valid = static_cast<bool>(index >> current.index);
        } else if (line.rfind("Volume:", 0) == 0) {
            // Each channel is "name: RAW / percent / dB". Do not read Base Volume.
            std::istringstream channels(line.substr(7));
            std::string channel;
            while (std::getline(channels, channel, ',')) {
                const auto colon = channel.find(':');
                if (colon == std::string::npos) continue;
                std::istringstream value(channel.substr(colon + 1));
                unsigned raw; char slash;
                if (value >> raw >> slash && slash == '/' && raw <= 0x7fffffffU) current.volume.push_back(raw);
            }
        } else {
            const auto eq = line.find(" = ");
            if (eq == std::string::npos) continue;
            std::istringstream value(line.substr(eq + 3));
            std::string property; value >> std::quoted(property);
            const auto key = line.substr(0, eq);
            if (key == "application.process.binary") binary = property;
            if (key == "application.name") app = property;
            if (key == "node.name") node = property;
            if (key == "application.process.id") pid = property;
            if (key == "object.serial") serial = property;
            if (key == "module-stream-restore.id") restoreKey = property;
        }
    }
    flush();
    return streams;
}
std::vector<unsigned> duckedVolume(const std::vector<unsigned>& volume, unsigned percent) {
    auto result = volume;
    // PulseAudio raw software volumes have a cubic amplitude mapping.
    const double factor = std::cbrt(std::min(percent, 100U) / 100.0);
    for (auto& channel : result) channel = static_cast<unsigned>(std::llround(channel * factor));
    return result;
}
#ifndef _WIN32
namespace {
struct SavedStream { AudioStream stream; std::vector<unsigned> lowered; };
bool enabled() {
    const char* value = std::getenv("COVERPLAYER_AUDIO_DUCKING");
    return value && std::string(value) == "1";
}
std::string statePath() {
    char* pref = SDL_GetPrefPath("CoverPlayer", "CoverPlayer");
    if (!pref) return {};
    std::string result = std::string(pref) + "audio-ducking-v1";
    SDL_free(pref); return result;
}
// Bound a missing/unresponsive audio server. Runs only in the independent
// watchdog, never on the audio decoding thread. Arguments are numeric only.
std::string command(const std::string& args, bool* success = nullptr) {
    if (success) *success = false;
    FILE* pipe = popen(("LC_ALL=C timeout 2 pactl " + args + " 2>/dev/null").c_str(), "r");
    if (!pipe) return {};
    std::string result; char buffer[2048];
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    const int status = pclose(pipe);
    if (success) *success = status == 0;
    return result;
}
bool setVolume(unsigned index, const std::vector<unsigned>& volume) {
    std::string args = "set-sink-input-volume " + std::to_string(index);
    for (const auto channel : volume) args += " " + std::to_string(channel);
    bool success = false;
    command(args, &success);
    return success;
}
std::vector<SavedStream> readState(const std::string& path) {
    std::ifstream input(path); std::vector<SavedStream> result;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        SavedStream saved; unsigned count;
        if (!(row >> saved.stream.index >> std::quoted(saved.stream.identity) >> count) || count == 0 || count > 32) continue;
        saved.stream.volume.resize(count); saved.lowered.resize(count);
        for (auto& v : saved.stream.volume) row >> v;
        for (auto& v : saved.lowered) row >> v;
        if (!row) continue;
        // Older journals end after channel values; preserve their exact-ID recovery.
        row >> std::quoted(saved.stream.restoreKey);
        result.push_back(std::move(saved));
    }
    return result;
}
bool writeState(const std::string& path, const std::vector<SavedStream>& state) {
    std::ofstream output(path + ".tmp", std::ios::trunc);
    for (const auto& saved : state) {
        output << saved.stream.index << ' ' << std::quoted(saved.stream.identity) << ' ' << saved.stream.volume.size();
        for (const auto v : saved.stream.volume) output << ' ' << v;
        for (const auto v : saved.lowered) output << ' ' << v;
        output << ' ' << std::quoted(saved.stream.restoreKey) << '\n';
    }
    output.close();
    return output && std::rename((path + ".tmp").c_str(), path.c_str()) == 0;
}
bool same(const AudioStream& a, const AudioStream& b) {
    return a.index == b.index && a.identity == b.identity;
}
void restore(const std::string& path) {
    bool available = false;
    const auto live = parseAudioStreams(command("list sink-inputs", &available));
    if (!available) return; // retain the journal for next-launch recovery
    const auto saved = readState(path);
    std::vector<SavedStream> pending;
    for (const auto& item : saved) {
        auto found = std::find_if(live.begin(), live.end(), [&](const auto& s) { return !s.own && same(s, item.stream); });
        if (found == live.end() && !item.stream.restoreKey.empty()) {
            found = std::find_if(live.begin(), live.end(), [&](const auto& s) {
                return !s.own && s.restoreKey == item.stream.restoreKey;
            });
        }
        if (found == live.end()) {
            // Keep an application-level recovery record for streams that may return.
            if (!item.stream.restoreKey.empty() && item.stream.volume != item.lowered) pending.push_back(item);
        } else if (found->volume == item.lowered && !setVolume(found->index, item.stream.volume)) {
            auto retry = item;
            retry.stream.index = found->index; retry.stream.identity = found->identity;
            pending.push_back(std::move(retry));
        }
        // A different current level is a manual change and must be respected.
    }
    if (pending.empty()) std::remove(path.c_str());
    else writeState(path, pending);
}
bool helperAlive(long owner) {
    std::ifstream input("/proc/" + std::to_string(owner) + "/cmdline", std::ios::binary);
    std::ostringstream contents; contents << input.rdbuf();
    return LinuxBackgroundSession::isBackgroundHelperCommandLine(contents.str());
}
}
#endif
void startAudioDucker() {
#ifndef _WIN32
    if (!enabled()) return;
    // Fork before invoking any audio-server commands; exec immediately. A
    // separate guardian can restore volumes even if the decoder is SIGKILLed.
    const std::string owner = std::to_string(getpid());
    const pid_t pid = fork();
    if (pid == 0) {
        execl("/proc/self/exe", "coverplayer", "--audio-ducking", owner.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
#endif
}
int runAudioDucker(long owner) {
#ifndef _WIN32
    if (!enabled() || owner <= 1) return 1;
    const auto path = statePath(); if (path.empty()) return 1;
    const int lock = open((path + ".lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0) return 1;
    bool acquired = false;
    for (int attempt = 0; attempt < 60 && helperAlive(owner); ++attempt) {
        if (flock(lock, LOCK_EX | LOCK_NB) == 0) { acquired = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!acquired) { close(lock); return 1; }
    restore(path);
    unsigned percent = 50;
    if (const char* value = std::getenv("COVERPLAYER_DUCK_PERCENT")) {
        char* end = nullptr; const long number = std::strtol(value, &end, 10);
        if (end != value && *end == '\0') percent = static_cast<unsigned>(std::clamp(number, 0L, 100L));
    }
    std::vector<SavedStream> saved = readState(path);
    while (helperAlive(owner)) {
        bool available = false;
        const auto live = parseAudioStreams(command("list sink-inputs", &available));
        if (!available) { std::this_thread::sleep_for(std::chrono::milliseconds(350)); continue; }
        for (const auto& stream : live) {
            if (stream.own || !helperAlive(owner)) continue;
            auto found = std::find_if(saved.begin(), saved.end(), [&](const auto& s) { return same(s.stream, stream); });
            if (found != saved.end()) continue; // no cumulative attenuation
            auto original = stream;
            if (!stream.restoreKey.empty()) {
                // PipeWire may create a new stream at the preceding game's ducked
                // volume. Transfer the original only for the same restore group
                // and an exact match to a level we previously applied.
                const auto previous = std::find_if(saved.rbegin(), saved.rend(), [&](const auto& item) {
                    return item.stream.restoreKey == stream.restoreKey && item.lowered == stream.volume;
                });
                if (previous != saved.rend()) original.volume = previous->stream.volume;
            }
            auto next = saved;
            next.erase(std::remove_if(next.begin(), next.end(), [&](const auto& item) {
                return !stream.restoreKey.empty() && item.stream.restoreKey == stream.restoreKey &&
                    std::none_of(live.begin(), live.end(), [&](const auto& s) { return same(s, item.stream); });
            }), next.end());
            next.push_back({original, duckedVolume(original.volume, percent)});
            // Persist BEFORE changing anything, so GUI recovery can undo an interrupted write.
            if (writeState(path, next)) {
                saved = std::move(next);
                setVolume(stream.index, saved.back().lowered);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
    }
    restore(path);
    flock(lock, LOCK_UN);
    // If a game ended before playback stopped, its server-side saved volume
    // still needs recovery when it next appears. Yield to any new guardian.
    while (!readState(path).empty()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (flock(lock, LOCK_EX | LOCK_NB) != 0) break;
        restore(path);
        flock(lock, LOCK_UN);
    }
    close(lock);
#else
    static_cast<void>(owner);
#endif
    return 0;
}
void recoverAudioDucking() {
#ifndef _WIN32
    const auto path = statePath(); if (path.empty()) return;
    const int lock = open((path + ".lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0) return;
    // An active guardian owns cleanup. Never compete with it.
    if (flock(lock, LOCK_EX | LOCK_NB) == 0) {
        if (std::ifstream(path).good()) restore(path);
    }
    close(lock);
#endif
}
}
