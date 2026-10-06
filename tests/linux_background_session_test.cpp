#include "platform/sdl/linux_background_session.hpp"

#include <SDL.h>

#include <iostream>
#include <string>

using namespace std::string_literals;

#ifdef _WIN32
int SDL_main(int, char**) {
#else
int main() {
#endif
    using coverplayer::platform::LinuxBackgroundSession;

    if (!LinuxBackgroundSession::isBackgroundHelperCommandLine(
            "coverplayer\0--background-audio\0" "1\0/a.mp3\0/b.mp3\0"s)) {
        std::cerr << "the real background helper command line was not recognized\n";
        return 1;
    }

    const std::string unrelated[] = {
        ""s,
        "coverplayer\0"s,
        "coverplayer\0--library\0/userdata/audiobooks\0"s,
        "retroarch\0-L\0core.so\0game.zip\0"s,
        "emulationstation\0--background-audio\0"s,
        "coverplayer-tool\0--background-audio\0"s,
        "coverplayer\0--background-audio-v2\0"s,
        "coverplayer --background-audio 1 /a.mp3"s,
    };
    for (const auto& commandLine : unrelated) {
        if (LinuxBackgroundSession::isBackgroundHelperCommandLine(commandLine)) {
            std::cerr << "an unrelated process command line was accepted as the background helper\n";
            return 1;
        }
    }
    return 0;
}
