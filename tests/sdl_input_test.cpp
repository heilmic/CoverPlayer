#include "platform/sdl/sdl_input.hpp"

#include <SDL.h>

#include <iostream>

namespace {

bool pushStartEvent(Uint32 type) {
    SDL_Event event{};
    event.type = type;
    event.cbutton.type = type;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_START;
    return SDL_PushEvent(&event) == 1;
}

bool pushButton(Uint8 button) {
    SDL_Event event{};
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = button;
    return SDL_PushEvent(&event) == 1;
}

} // namespace

#ifdef _WIN32
int SDL_main(int, char**) {
#else
int main() {
#endif
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    {
        coverplayer::platform::SdlInput input;
        if (!pushStartEvent(SDL_CONTROLLERBUTTONDOWN)) return 1;
        const auto pressed = input.poll(coverplayer::platform::Screen::Player, false);
        if (pressed.cycleSleepTimer || pressed.background) {
            std::cerr << "pressing START immediately triggered an action\n";
            return 1;
        }

        if (!pushStartEvent(SDL_CONTROLLERBUTTONUP)) return 1;
        const auto released = input.poll(coverplayer::platform::Screen::Player, false);
        if (!released.cycleSleepTimer || released.background) {
            std::cerr << "short START did not exclusively trigger the sleep timer\n";
            return 1;
        }
    }

    {
        coverplayer::platform::SdlInput input;
        if (!pushButton(SDL_CONTROLLER_BUTTON_BACK)) return 1;
        static_cast<void>(input.poll(coverplayer::platform::Screen::Player, false));
        if (!input.helpVisible() || !pushButton(SDL_CONTROLLER_BUTTON_Y)) return 1;
        const auto switched = input.poll(coverplayer::platform::Screen::Player, false);
        if (!switched.toggleLanguage || switched.cycleSleepTimer || switched.addBookmark || !input.helpVisible()) {
            std::cerr << "language switch in help triggered a player action\n";
            return 1;
        }
        if (!pushButton(SDL_CONTROLLER_BUTTON_B)) return 1;
        static_cast<void>(input.poll(coverplayer::platform::Screen::Player, false));
        if (input.helpVisible()) {
            std::cerr << "B did not close help\n";
            return 1;
        }
    }

    {
        coverplayer::platform::SdlInput input;
        if (!pushStartEvent(SDL_CONTROLLERBUTTONDOWN)) return 1;
        static_cast<void>(input.poll(coverplayer::platform::Screen::Player, false));
        SDL_Delay(2050);
        const auto held = input.poll(coverplayer::platform::Screen::Player, false);
        if (!held.background || held.cycleSleepTimer) {
            std::cerr << "long START did not exclusively trigger background playback\n";
            return 1;
        }

        if (!pushStartEvent(SDL_CONTROLLERBUTTONUP)) return 1;
        const auto released = input.poll(coverplayer::platform::Screen::Player, false);
        if (released.background || released.cycleSleepTimer) {
            std::cerr << "releasing long START triggered a second action\n";
            return 1;
        }
    }

    {
        coverplayer::platform::SdlInput input;
        if (!pushButton(SDL_CONTROLLER_BUTTON_A)) return 1;
        const auto wake = input.poll(coverplayer::platform::Screen::Player, false, true);
        if (!input.hadActivity() || wake.accept || wake.togglePause) return 1;
        if (!pushStartEvent(SDL_CONTROLLERBUTTONDOWN)) return 1;
        const auto startWake = input.poll(coverplayer::platform::Screen::Player, false, true);
        if (startWake.cycleSleepTimer || startWake.background) return 1;
        if (!pushStartEvent(SDL_CONTROLLERBUTTONUP)) return 1;
        const auto release = input.poll(coverplayer::platform::Screen::Player, false);
        if (release.cycleSleepTimer || release.background) return 1;
        if (!pushButton(SDL_CONTROLLER_BUTTON_A)) return 1;
        const auto next = input.poll(coverplayer::platform::Screen::Player, false);
        if (!next.togglePause) return 1;
    }

    SDL_Quit();
    return 0;
}
