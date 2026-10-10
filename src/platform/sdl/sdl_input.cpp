#include "platform/sdl/sdl_input.hpp"

#include <cstdlib>

namespace coverplayer::platform {

SdlInput::SdlInput() {
    swapFaceButtons_ = std::getenv("COVERPLAYER_SWAP_FACE_BUTTONS") != nullptr;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_IsGameController(index) == SDL_TRUE) {
            openController(index);
            break;
        }
    }
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_StartTextInput();
}

SdlInput::~SdlInput() {
    closeController();
    SDL_StopTextInput();
}

void SdlInput::openController(int deviceIndex) {
    SDL_GameController* candidate = SDL_GameControllerOpen(deviceIndex);
    if (candidate == nullptr) {
        return;
    }
    closeController();
    controller_ = candidate;
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller_);
    controllerInstanceId_ = SDL_JoystickInstanceID(joystick);
}

void SdlInput::closeController() {
    if (controller_ != nullptr) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
    controllerInstanceId_ = -1;
    clearDirections();
    startHeld_ = false;
    startLongPressTriggered_ = false;
}

void SdlInput::clearDirections() {
    keyboardDirections_ = padDirections_ = 0;
    stickX_ = stickY_ = 0;
    stickDirection_ = heldDirection_ = Direction::None;
}

SdlInput::Direction SdlInput::direction() const {
    const unsigned buttons = keyboardDirections_ | padDirections_;
    // Opposite directions cancel, and physical buttons take precedence over the stick.
    if (buttons) {
        const int x = int(bool(buttons & 2)) - int(bool(buttons & 1));
        const int y = int(bool(buttons & 8)) - int(bool(buttons & 4));
        if (x) return x < 0 ? Direction::Left : Direction::Right;
        if (y) return y < 0 ? Direction::Up : Direction::Down;
        return Direction::None;
    }
    return stickDirection_;
}

void SdlInput::applyDirection(InputActions& actions, Screen screen, Direction value) {
    if (value == Direction::None) return;
    const int sign = value == Direction::Left || value == Direction::Up ? -1 : 1;
    if (screen == Screen::Player) {
        if (value == Direction::Left || value == Direction::Right) actions.seekSeconds = sign * 10;
        else actions.changeTrack = sign;
    } else {
        actions.navigate = sign * (screen == Screen::CollectionName &&
            (value == Direction::Up || value == Direction::Down) ? 10 : 1);
    }
}

void SdlInput::updateDirection(InputActions& actions, Screen screen, Uint32 now) {
    const auto value = direction();
    if (value == Direction::None) directionBlocked_ = false;
    if (value != heldDirection_) {
        heldDirection_ = value;
        directionHeldAt_ = directionRepeatedAt_ = now;
        if (!directionBlocked_ && !helpVisible_) applyDirection(actions, screen, value);
    }
}

InputActions SdlInput::poll(Screen currentScreen, bool bluetoothEnabled, bool wakeOnly, Uint32 now) {
    InputActions actions{};
    hadActivity_ = false;
    if (wakeOnly || helpVisible_ || (hasScreen_ && previousScreen_ != currentScreen))
        directionBlocked_ = direction() != Direction::None;
    previousScreen_ = currentScreen;
    hasScreen_ = true;
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        const bool activity = event.type == SDL_KEYDOWN || event.type == SDL_CONTROLLERBUTTONDOWN ||
            event.type == SDL_JOYBUTTONDOWN || event.type == SDL_MOUSEBUTTONDOWN ||
            (event.type == SDL_CONTROLLERAXISMOTION && std::abs(int(event.caxis.value)) > 16000) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED);
        hadActivity_ = hadActivity_ || activity;
        if (event.type == SDL_APP_WILLENTERBACKGROUND ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
            clearDirections();
        }
        bool directionalEvent = false;
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            unsigned bit = 0;
            switch (event.key.keysym.sym) {
            case SDLK_LEFT: bit = 1; break; case SDLK_RIGHT: bit = 2; break;
            case SDLK_UP: bit = 4; break; case SDLK_DOWN: bit = 8; break;
            default: break;
            }
            if (bit) {
                directionalEvent = true;
                if (event.type == SDL_KEYUP) keyboardDirections_ &= ~bit;
                else if (!event.key.repeat) keyboardDirections_ |= bit;
            }
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
            unsigned bit = 0;
            switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: bit = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: bit = 2; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: bit = 4; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: bit = 8; break;
            default: break;
            }
            if (bit) {
                directionalEvent = true;
                if (controller_ && event.cbutton.which != controllerInstanceId_) continue;
                if (event.type == SDL_CONTROLLERBUTTONUP) padDirections_ &= ~bit;
                else padDirections_ |= bit;
            }
        }
        if (event.type == SDL_CONTROLLERAXISMOTION &&
            (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
            if (controller_ && event.caxis.which != controllerInstanceId_) continue;
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) stickX_ = event.caxis.value;
            else stickY_ = event.caxis.value;
            continue; // Evaluate both axes together after draining the event queue.
        }
        if (directionalEvent) {
            if (wakeOnly || helpVisible_) directionBlocked_ = direction() != Direction::None;
            updateDirection(actions, currentScreen, now);
            continue;
        }
        if (wakeOnly && (activity || event.type == SDL_KEYUP || event.type == SDL_CONTROLLERBUTTONUP ||
            event.type == SDL_JOYBUTTONUP || event.type == SDL_TEXTINPUT)) {
            startHeld_ = false;
            startLongPressTriggered_ = false;
            continue;
        }
        if (event.type == SDL_QUIT) actions.quit = true;
        if (event.type == SDL_APP_WILLENTERBACKGROUND) actions.suspend = true;
        if (event.type == SDL_APP_DIDENTERFOREGROUND) actions.resume = true;
        if (event.type == SDL_TEXTINPUT && currentScreen == Screen::CollectionName && !helpVisible_) {
            actions.textInput += event.text.text;
        }
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_TAB) {
                helpVisible_ = !helpVisible_;
                continue;
            }
            if (helpVisible_) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) helpVisible_ = false;
                else if (event.key.keysym.sym == SDLK_x && bluetoothEnabled && currentScreen != Screen::Bluetooth) { helpVisible_ = false; actions.openBluetooth = true; }
                else if (event.key.keysym.sym == SDLK_y) actions.toggleLanguage = true;
                continue;
            }
            if (currentScreen == Screen::CollectionName) {
                if (event.key.keysym.sym == SDLK_ESCAPE) actions.back = true;
                else if (event.key.keysym.sym == SDLK_BACKSPACE) actions.eraseText = true;
                else if (event.key.keysym.sym == SDLK_DELETE) actions.clearText = true;
                else if (event.key.keysym.sym == SDLK_RETURN) actions.saveText = true;
                continue;
            }
            if (currentScreen == Screen::CollectionManager) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) actions.back = true;
                else if (event.key.keysym.sym == SDLK_RETURN) actions.accept = true;
                else if (event.key.keysym.sym == SDLK_y) actions.openFolders = true;
                else if (event.key.keysym.sym == SDLK_DELETE || event.key.keysym.sym == SDLK_x) actions.deleteItem = true;
                else if (event.key.keysym.sym == SDLK_PAGEUP) actions.reorder = -1;
                else if (event.key.keysym.sym == SDLK_PAGEDOWN) actions.reorder = 1;
                continue;
            }
            if (currentScreen == Screen::Bluetooth) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) actions.back = true;
                else if (event.key.keysym.sym == SDLK_RETURN) actions.accept = true;
                else if (event.key.keysym.sym == SDLK_x) actions.toggleBluetooth = true;
                else if (event.key.keysym.sym == SDLK_y || event.key.keysym.sym == SDLK_r) actions.refreshBluetooth = true;
                continue;
            }
            if (event.key.keysym.sym == SDLK_q) actions.quit = true;
            if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) actions.back = true;
            if (event.key.keysym.sym == SDLK_RETURN) actions.accept = true;
            if (event.key.keysym.sym == SDLK_r) actions.rescan = true;
            if (event.key.keysym.sym == SDLK_s) actions.cycleSleepTimer = true;
            if (event.key.keysym.sym == SDLK_b) actions.addBookmark = true;
            if (event.key.keysym.sym == SDLK_j) actions.jumpBookmark = true;
            if (event.key.keysym.sym == SDLK_m && currentScreen == Screen::Collections) actions.openFolders = true;
            if ((event.key.keysym.sym == SDLK_y || event.key.keysym.sym == SDLK_v) &&
                (currentScreen == Screen::CoverFlow || currentScreen == Screen::AlbumList)) actions.toggleLibraryView = true;
            if (event.key.keysym.sym == SDLK_MINUS) actions.volumeDelta = -5;
            if (event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_EQUALS) actions.volumeDelta = 5;
            if (currentScreen == Screen::Player && event.key.keysym.sym == SDLK_SPACE) actions.togglePause = true;
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            auto button=static_cast<SDL_GameControllerButton>(event.cbutton.button);
            if(swapFaceButtons_){if(button==SDL_CONTROLLER_BUTTON_A)button=SDL_CONTROLLER_BUTTON_B;else if(button==SDL_CONTROLLER_BUTTON_B)button=SDL_CONTROLLER_BUTTON_A;else if(button==SDL_CONTROLLER_BUTTON_X)button=SDL_CONTROLLER_BUTTON_Y;else if(button==SDL_CONTROLLER_BUTTON_Y)button=SDL_CONTROLLER_BUTTON_X;}
            if (helpVisible_) {
                if (button == SDL_CONTROLLER_BUTTON_BACK || button == SDL_CONTROLLER_BUTTON_B) helpVisible_ = false;
                else if (button == SDL_CONTROLLER_BUTTON_X && bluetoothEnabled && currentScreen != Screen::Bluetooth) { helpVisible_ = false; actions.openBluetooth = true; }
                else if (button == SDL_CONTROLLER_BUTTON_Y) actions.toggleLanguage = true;
                continue;
            }
            if (button == SDL_CONTROLLER_BUTTON_START) {
                startHeld_ = true;
                startLongPressTriggered_ = false;
                startPressedAt_ = now;
                if (controller_ != nullptr && SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_BACK)) actions.quit = true;
            }
            if (button == SDL_CONTROLLER_BUTTON_BACK) {
                if (startHeld_ || (controller_ != nullptr && SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_START))) actions.quit = true;
                else helpVisible_ = !helpVisible_;
                continue;
            }
            if (currentScreen == Screen::CollectionName) {
                if (button == SDL_CONTROLLER_BUTTON_B) actions.back = true;
                else if (button == SDL_CONTROLLER_BUTTON_A) actions.accept = true;
                else if (button == SDL_CONTROLLER_BUTTON_X) actions.eraseText = true;
                else if (button == SDL_CONTROLLER_BUTTON_Y) actions.saveText = true;
                else if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) actions.clearText = true;
                else if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) actions.navigate = 10;
                continue;
            }
            if(currentScreen==Screen::CollectionManager){if(button==SDL_CONTROLLER_BUTTON_B)actions.back=true;else if(button==SDL_CONTROLLER_BUTTON_A)actions.accept=true;else if(button==SDL_CONTROLLER_BUTTON_Y)actions.openFolders=true;else if(button==SDL_CONTROLLER_BUTTON_X)actions.deleteItem=true;else if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)actions.reorder=-1;else if(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)actions.reorder=1;continue;}
            if(currentScreen==Screen::Bluetooth){if(button==SDL_CONTROLLER_BUTTON_B)actions.back=true;else if(button==SDL_CONTROLLER_BUTTON_A)actions.accept=true;else if(button==SDL_CONTROLLER_BUTTON_X)actions.toggleBluetooth=true;else if(button==SDL_CONTROLLER_BUTTON_Y)actions.refreshBluetooth=true;continue;}
            if(button==SDL_CONTROLLER_BUTTON_B)actions.back=true;
            if(currentScreen==Screen::Collections&&button==SDL_CONTROLLER_BUTTON_Y)actions.openFolders=true;
            if((currentScreen==Screen::Collections||currentScreen==Screen::CoverFlow||currentScreen==Screen::AlbumList)&&button==SDL_CONTROLLER_BUTTON_X)actions.rescan=true;
            if((currentScreen==Screen::CoverFlow||currentScreen==Screen::AlbumList)&&button==SDL_CONTROLLER_BUTTON_Y)actions.toggleLibraryView=true;
            if(currentScreen==Screen::Folders&&button==SDL_CONTROLLER_BUTTON_Y)actions.rescan=true;
            if(currentScreen==Screen::Player&&button==SDL_CONTROLLER_BUTTON_Y)actions.addBookmark=true;
            if(currentScreen==Screen::Player&&button==SDL_CONTROLLER_BUTTON_X)actions.jumpBookmark=true;
            if(currentScreen==Screen::Player){if(button==SDL_CONTROLLER_BUTTON_A)actions.togglePause=true;if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)actions.seekSeconds=-30;if(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)actions.seekSeconds=30;}
            else {if(button==SDL_CONTROLLER_BUTTON_A)actions.accept=true;if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)actions.navigate=-1;if(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)actions.navigate=1;}
        }
        if (event.type == SDL_CONTROLLERBUTTONUP && event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            // START has two deliberately separate gestures in the player:
            // a short press cycles the sleep timer, while a two-second hold
            // hands playback to the background helper. Resolve the short
            // press on release so a long press can never trigger both.
            if (startHeld_ && !startLongPressTriggered_ && currentScreen == Screen::Player) {
                actions.cycleSleepTimer = true;
            }
            startHeld_ = false;
            startLongPressTriggered_ = false;
        }
        if (event.type == SDL_DROPFILE && event.drop.file != nullptr) {
            actions.openMediaPath = event.drop.file;
            SDL_free(event.drop.file);
        }
        if (event.type == SDL_CONTROLLERDEVICEADDED && controller_ == nullptr) {
            openController(event.cdevice.which);
        }
        if (event.type == SDL_CONTROLLERDEVICEREMOVED && event.cdevice.which == controllerInstanceId_) {
            closeController();
            for (int index = 0; index < SDL_NumJoysticks(); ++index) {
                if (SDL_IsGameController(index) == SDL_TRUE) {
                    openController(index);
                    break;
                }
            }
        }
    }
    const int x = std::abs(stickX_), y = std::abs(stickY_);
    const int threshold = stickDirection_ == Direction::None ? 16000 : 10000;
    if (x < threshold && y < threshold) stickDirection_ = Direction::None;
    else {
        // Dominant axis, with hysteresis around diagonals to prevent jitter.
        const bool wasHorizontal = stickDirection_ == Direction::Left || stickDirection_ == Direction::Right;
        bool horizontal = x >= y;
        if (stickDirection_ != Direction::None && std::abs(x - y) < 4000)
            horizontal = wasHorizontal;
        stickDirection_ = horizontal ? (stickX_ < 0 ? Direction::Left : Direction::Right)
            : (stickY_ < 0 ? Direction::Up : Direction::Down);
    }
    if (wakeOnly || helpVisible_) directionBlocked_ = direction() != Direction::None;
    updateDirection(actions, currentScreen, now);
    if (heldDirection_ != Direction::None && !directionBlocked_ && !helpVisible_ && !wakeOnly) {
        hadActivity_ = true;
        const Uint32 heldFor = now - directionHeldAt_;
        const Uint32 interval = heldFor >= 1200U ? 65U : 110U;
        // Repeat browsing only: holding Up/Down in the player must not skip many tracks.
        // Emit at most one step per poll, never a burst after a slow frame.
        if (currentScreen != Screen::Player && heldFor >= 330U && now - directionRepeatedAt_ >= interval) {
            applyDirection(actions, currentScreen, heldDirection_);
            directionRepeatedAt_ = now;
        }
    }
    // A solo two-second hold requests backgrounding (or a plain quit if
    // nothing is playing - Application decides which). Start+Select
    // together stays an unconditional immediate quit, handled above.
    if (startHeld_ && !startLongPressTriggered_ && now - startPressedAt_ >= 2000U) {
        actions.background = true;
        startLongPressTriggered_ = true;
    }
    if (controller_ != nullptr && !helpVisible_) {
        const bool leftPressed = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000;
        const bool rightPressed = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
        if (!wakeOnly && leftPressed && !leftTriggerPressed_) actions.seekSeconds = -30.0;
        if (!wakeOnly && rightPressed && !rightTriggerPressed_) actions.seekSeconds = 30.0;
        leftTriggerPressed_ = leftPressed;
        rightTriggerPressed_ = rightPressed;
    }
    if (helpVisible_ && !actions.quit) {
        const bool suspend = actions.suspend;
        const bool resume = actions.resume;
        const bool toggleLanguage = actions.toggleLanguage;
        actions = {};
        actions.suspend = suspend;
        actions.resume = resume;
        actions.toggleLanguage = toggleLanguage;
    }
    return actions;
}

} // namespace coverplayer::platform
