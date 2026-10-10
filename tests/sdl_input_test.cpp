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

void releaseButton(Uint8 button) {
    SDL_Event event{}; event.type = SDL_CONTROLLERBUTTONUP;
    event.cbutton.button = button; SDL_PushEvent(&event);
}
void key(SDL_Keycode value, bool down, bool repeat = false) {
    SDL_Event event{}; event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.sym = value; event.key.repeat = repeat; SDL_PushEvent(&event);
}
void axis(Uint8 value, Sint16 position) {
    SDL_Event event{}; event.type = SDL_CONTROLLERAXISMOTION;
    event.caxis.axis = value; event.caxis.value = position; SDL_PushEvent(&event);
}
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
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

    using coverplayer::platform::Screen;
    using coverplayer::platform::SdlInput;
    {
        SdlInput input;
        key(SDLK_RIGHT, true);
        require(input.poll(Screen::CoverFlow, false, false, 100).navigate == 1, "initial arrow");
        key(SDLK_RIGHT, true, true);
        require(input.poll(Screen::CoverFlow, false, false, 200).navigate == 0, "OS repeat bypassed delay");
        require(input.poll(Screen::CoverFlow, false, false, 429).navigate == 0, "repeat too early");
        require(input.poll(Screen::CoverFlow, false, false, 430).navigate == 1, "held arrow did not repeat");
        require(input.poll(Screen::CoverFlow, false, false, 539).navigate == 0, "repeat interval");
        require(input.poll(Screen::CoverFlow, false, false, 540).navigate == 1, "second repeat");
        require(input.poll(Screen::CoverFlow, false, false, 1300).navigate == 1, "long hold");
        require(input.poll(Screen::CoverFlow, false, false, 1365).navigate == 1, "accelerated repeat");
        key(SDLK_RIGHT, false);
        require(input.poll(Screen::CoverFlow, false, false, 1500).navigate == 0, "release did not stop");
        require(input.poll(Screen::CoverFlow, false, false, 2000).navigate == 0, "repeat stuck");
        pushButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        require(input.poll(Screen::AlbumList, false, false, 2100).navigate == -1, "D-pad press");
        require(input.poll(Screen::AlbumList, false, false, 2430).navigate == -1, "D-pad repeat");
        require(input.poll(Screen::Tracks, false, false, 2800).navigate == 0, "held input crossed screen change");
        releaseButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        static_cast<void>(input.poll(Screen::Tracks, false, false, 2900));
        pushButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        require(input.poll(Screen::Tracks, false, false, 3000).navigate == -1, "repress after screen change");
        SDL_Event lost{}; lost.type = SDL_WINDOWEVENT; lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
        SDL_PushEvent(&lost);
        require(input.poll(Screen::Tracks, false, false, 4000).navigate == 0, "focus loss left repeat active");
    }
    {
        SdlInput input;
        axis(SDL_CONTROLLER_AXIS_LEFTX, 9000);
        require(input.poll(Screen::CoverFlow, false, false, 100).navigate == 0, "stick drift");
        axis(SDL_CONTROLLER_AXIS_LEFTX, 22000);
        require(input.poll(Screen::CoverFlow, false, false, 200).navigate == 1, "stick right");
        require(input.poll(Screen::CoverFlow, false, false, 530).navigate == 1, "stick repeat");
        axis(SDL_CONTROLLER_AXIS_LEFTY, 23000);
        require(input.poll(Screen::CoverFlow, false, false, 540).navigate == 0, "diagonal jitter");
        axis(SDL_CONTROLLER_AXIS_LEFTX, 0); axis(SDL_CONTROLLER_AXIS_LEFTY, 0);
        require(input.poll(Screen::CoverFlow, false, false, 1000).navigate == 0, "center did not stop");
        axis(SDL_CONTROLLER_AXIS_LEFTX, -24000);
        require(input.poll(Screen::CoverFlow, false, true, 1100).navigate == 0, "stick wake navigated");
        require(input.poll(Screen::CoverFlow, false, false, 1800).navigate == 0, "wake hold repeated");
        axis(SDL_CONTROLLER_AXIS_LEFTX, 0); static_cast<void>(input.poll(Screen::CoverFlow, false, false, 1900));
        axis(SDL_CONTROLLER_AXIS_LEFTY, -24000);
        require(input.poll(Screen::CollectionName, false, false, 2000).navigate == -10, "stick keyboard row");
        axis(SDL_CONTROLLER_AXIS_LEFTY, 0); static_cast<void>(input.poll(Screen::CollectionName, false, false, 2100));
        axis(SDL_CONTROLLER_AXIS_LEFTX, 24000);
        require(input.poll(Screen::Player, false, false, 2200).seekSeconds == 10, "stick player seek");
        const auto held = input.poll(Screen::Player, false, false, 3000);
        require(held.seekSeconds == 0 && held.changeTrack == 0, "player action auto-repeated");
        axis(SDL_CONTROLLER_AXIS_LEFTX, 0); static_cast<void>(input.poll(Screen::Player, false, false, 3100));
        axis(SDL_CONTROLLER_AXIS_LEFTY, -24000);
        require(input.poll(Screen::Player, false, false, 3200).changeTrack == -1, "stick previous track");
    }

    SDL_Quit();
    return 0;
}
