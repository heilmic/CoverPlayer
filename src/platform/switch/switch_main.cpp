#include "coverplayer/app/application.hpp"
#include "audio/mpg123_sdl_player.hpp"
#include "persistence/file_library_cache.hpp"
#include "persistence/file_progress_store.hpp"
#include "platform/native_file_system.hpp"
#include "platform/switch/switch_platform.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>

int main(int, char**) {
    // libnx mounts sdmc for NRO applications. The bundled SDL has no working
    // SDL_GetPrefPath implementation, so all persistent paths are explicit.
    try {
        std::filesystem::create_directories("sdmc:/config/coverplayer");
        std::filesystem::create_directories("sdmc:/media");
        if (chdir("sdmc:/config/coverplayer") != 0)
            throw std::runtime_error("Cannot open sdmc:/config/coverplayer");
        std::freopen("coverplayer.log", "w", stderr);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        std::fprintf(stderr, "CoverPlayer Switch alpha 4: state=sdmc:/config/coverplayer\n");
        const Result romfsResult = romfsInit();
        if (R_FAILED(romfsResult)) throw std::runtime_error("Cannot mount bundled assets");
        struct RomfsGuard { ~RomfsGuard() { romfsExit(); } } romfs;
        // Stay responsive to HOME/exit and save/pause when focus is lost.
        appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
        coverplayer::platform::SwitchPlatform platform;
        coverplayer::audio::Mpg123SdlPlayer player;
        coverplayer::persistence::FileProgressStore progress("sdmc:/config/coverplayer");
        coverplayer::persistence::FileLibraryCache cache("sdmc:/config/coverplayer/library-cache");
        coverplayer::platform::NativeFileSystem files;
        coverplayer::app::Application application(platform, &player, &progress,
            cache, files, "sdmc:/media", "", "sdmc:/");
        return application.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "CoverPlayer Switch startup failed: %s\n", error.what());
        return 1;
    }
}
