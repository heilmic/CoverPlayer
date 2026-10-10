#include "coverplayer/app/application.hpp"
#include "audio/mpg123_sdl_player.hpp"
#include "persistence/file_progress_store.hpp"
#include "persistence/file_library_cache.hpp"
#include "platform/sdl/linux_background_session.hpp"
#include "platform/sdl/audio_ducker.hpp"
#include "platform/sdl/playback_power.hpp"
#include "platform/sdl/sdl_platform.hpp"
#include "platform/native_file_system.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

namespace {

// Minimal headless mode spawned by LinuxBackgroundSession::spawnBackgroundAudio():
// a fresh process image (via fork()+exec(), never a continuation of the GUI
// process's own SDL state - see docs/architecture.md) that decodes and plays
// trackPaths in order starting at startIndex, with no window, no controller,
// and no library scanning of its own - the GUI already resolved the album's
// track order before backgrounding, so this just walks the list it was
// handed. It is fully independent once running: whatever launches next (a
// game, an emulator, EmulationStation's own menu) has no effect on it.
int runBackgroundAudio(const std::vector<std::string>& trackPaths, std::size_t startIndex) {
    if (startIndex >= trackPaths.size()) return 1;
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::cerr << "CoverPlayer background audio: SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    coverplayer::audio::Mpg123SdlPlayer audioPlayer;
    coverplayer::persistence::FileProgressStore progressStore;
    coverplayer::platform::PlaybackPower power(false);
    bool guardianStarted = false;
    for (std::size_t index = startIndex; index < trackPaths.size(); ++index) {
        const auto& mediaPath = trackPaths[index];
        if (!audioPlayer.open(mediaPath)) {
            std::cerr << "CoverPlayer background audio: open failed for '" << mediaPath << "': "
                << audioPlayer.error() << '\n';
            break;
        }
        power.playback(true);
        coverplayer::platform::LinuxBackgroundSession::recordRunning(mediaPath);
        if (!guardianStarted) {
            coverplayer::platform::startAudioDucker();
            guardianStarted = true;
        }
        if (const auto progress = progressStore.load(mediaPath)) {
            if (!progress->completed && progress->positionSeconds > 0.0) {
                audioPlayer.seekSeconds(progress->positionSeconds);
            }
        }
        auto lastSave = std::chrono::steady_clock::now();
        while (!audioPlayer.isFinished()) {
            audioPlayer.update();
            power.tick(SDL_GetTicks());
            if (std::chrono::steady_clock::now() - lastSave >= std::chrono::seconds(5)) {
                progressStore.save(mediaPath, {audioPlayer.positionSeconds(), false});
                lastSave = std::chrono::steady_clock::now();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        progressStore.save(mediaPath, {audioPlayer.positionSeconds(), true});
    }
    power.playback(false);
    SDL_Quit();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--playback-power") {
        char* end = nullptr; const long owner = std::strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0') return 1;
        return coverplayer::platform::PlaybackPower::watch(owner);
    }
    if (argc == 3 && std::string(argv[1]) == "--audio-ducking") {
        char* end = nullptr;
        const long owner = std::strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0') return 1;
        return coverplayer::platform::runAudioDucker(owner);
    }
    if (argc >= 4 && std::string(argv[1]) == "--background-audio") {
        std::size_t startIndex = 0;
        try {
            startIndex = static_cast<std::size_t>(std::stoull(argv[2]));
        } catch (const std::exception&) {
            std::cerr << "CoverPlayer background audio: invalid start index '" << argv[2] << "'\n";
            return 1;
        }
        std::vector<std::string> trackPaths;
        trackPaths.reserve(static_cast<std::size_t>(argc) - 3);
        for (int index = 3; index < argc; ++index) trackPaths.emplace_back(argv[index]);
        return runBackgroundAudio(trackPaths, startIndex);
    }
    try {
        coverplayer::platform::SdlPlatform platform;
        coverplayer::audio::Mpg123SdlPlayer audioPlayer;
        coverplayer::persistence::FileProgressStore progressStore;
        coverplayer::persistence::FileLibraryCache libraryCache;
        coverplayer::platform::NativeFileSystem fileSystem;
        std::string mediaRoot=std::getenv("COVERPLAYER_MEDIA_ROOT")?std::getenv("COVERPLAYER_MEDIA_ROOT"):".";
        std::string browseRoot=std::getenv("COVERPLAYER_BROWSE_ROOT")?std::getenv("COVERPLAYER_BROWSE_ROOT"):"";
        std::string initialMediaPath;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--library"&&i+1<argc)mediaRoot=argv[++i];else initialMediaPath=arg;}
        // If a background helper (see enterBackgroundPlayback) is currently
        // playing something, take control back now rather than leaving it
        // running detached and unreachable - and, crucially, before this
        // session could ever background a *different* track and end up
        // with two helpers playing at once.
        {
            if (const auto backgroundedPath = coverplayer::platform::LinuxBackgroundSession::takeOverIfRunning()) {
                if (initialMediaPath.empty()) initialMediaPath = *backgroundedPath;
                std::cerr << "CoverPlayer: took over background playback of '" << initialMediaPath << "'\n";
            }
        }
        coverplayer::app::Application application(
            platform, &audioPlayer, &progressStore, libraryCache, fileSystem, mediaRoot, initialMediaPath, browseRoot);
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "CoverPlayer startup failed: " << error.what() << '\n';
        return 1;
    }
}
