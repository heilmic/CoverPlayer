#include "audio/mpg123_sdl_player.hpp"

#include <SDL.h>

#include <cmath>
#include <iostream>

#ifdef _WIN32
int SDL_main(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 2) {
        std::cerr << "usage: coverplayer_player_seek_test <mp3>\n";
        return 2;
    }
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    coverplayer::audio::Mpg123SdlPlayer player;
    if (!player.open(argv[1])) {
        std::cerr << player.error() << '\n';
        return 1;
    }
    const double duration = player.durationSeconds();
    if (duration <= 0.0) {
        std::cerr << "duration unavailable\n";
        return 1;
    }

    const double target = duration * 0.88;
    player.seekSeconds(target);
    const double actual = player.positionSeconds();
    if (!player.error().empty() || std::abs(actual - target) > 2.0) {
        std::cerr << "seek failed: target=" << target << " actual=" << actual
                  << " error=" << player.error() << '\n';
        return 1;
    }
    std::cout << "duration=" << duration << " target=" << target
              << " actual=" << actual << '\n';
    SDL_Quit();
    return 0;
}
