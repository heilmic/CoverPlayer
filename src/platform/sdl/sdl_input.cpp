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
    startHeld_ = false;
    startLongPressTriggered_ = false;
}

InputActions SdlInput::poll(Screen currentScreen, bool bluetoothEnabled, bool wakeOnly) {
    InputActions actions{};
    hadActivity_ = false;
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        const bool activity = event.type == SDL_KEYDOWN || event.type == SDL_CONTROLLERBUTTONDOWN ||
            event.type == SDL_JOYBUTTONDOWN || event.type == SDL_MOUSEBUTTONDOWN ||
            (event.type == SDL_CONTROLLERAXISMOTION && std::abs(int(event.caxis.value)) > 16000) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED);
        hadActivity_ = hadActivity_ || activity;
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
                else if (event.key.keysym.sym == SDLK_LEFT) actions.navigate = -1;
                else if (event.key.keysym.sym == SDLK_RIGHT) actions.navigate = 1;
                else if (event.key.keysym.sym == SDLK_UP) actions.navigate = -10;
                else if (event.key.keysym.sym == SDLK_DOWN) actions.navigate = 10;
                continue;
            }
            if (currentScreen == Screen::CollectionManager) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) actions.back = true;
                else if (event.key.keysym.sym == SDLK_RETURN) actions.accept = true;
                else if (event.key.keysym.sym == SDLK_y) actions.openFolders = true;
                else if (event.key.keysym.sym == SDLK_DELETE || event.key.keysym.sym == SDLK_x) actions.deleteItem = true;
                else if (event.key.keysym.sym == SDLK_PAGEUP) actions.reorder = -1;
                else if (event.key.keysym.sym == SDLK_PAGEDOWN) actions.reorder = 1;
                else if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_LEFT) actions.navigate = -1;
                else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_RIGHT) actions.navigate = 1;
                continue;
            }
            if (currentScreen == Screen::Bluetooth) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) actions.back = true;
                else if (event.key.keysym.sym == SDLK_RETURN) actions.accept = true;
                else if (event.key.keysym.sym == SDLK_x) actions.toggleBluetooth = true;
                else if (event.key.keysym.sym == SDLK_y || event.key.keysym.sym == SDLK_r) actions.refreshBluetooth = true;
                else if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_LEFT) actions.navigate = -1;
                else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_RIGHT) actions.navigate = 1;
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
            if (currentScreen == Screen::Player) { if (event.key.keysym.sym == SDLK_SPACE) actions.togglePause=true;if(event.key.keysym.sym==SDLK_LEFT)actions.seekSeconds=-10;if(event.key.keysym.sym==SDLK_RIGHT)actions.seekSeconds=10;if(event.key.keysym.sym==SDLK_UP)actions.changeTrack=-1;if(event.key.keysym.sym==SDLK_DOWN)actions.changeTrack=1; }
            else { if(event.key.keysym.sym==SDLK_UP||event.key.keysym.sym==SDLK_LEFT)actions.navigate=-1;if(event.key.keysym.sym==SDLK_DOWN||event.key.keysym.sym==SDLK_RIGHT)actions.navigate=1; }
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
                startPressedAt_ = SDL_GetTicks();
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
                else if (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) actions.navigate = -1;
                else if (button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) actions.navigate = 1;
                else if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) actions.navigate = -10;
                else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) actions.navigate = 10;
                continue;
            }
            if(currentScreen==Screen::CollectionManager){if(button==SDL_CONTROLLER_BUTTON_B)actions.back=true;else if(button==SDL_CONTROLLER_BUTTON_A)actions.accept=true;else if(button==SDL_CONTROLLER_BUTTON_Y)actions.openFolders=true;else if(button==SDL_CONTROLLER_BUTTON_X)actions.deleteItem=true;else if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)actions.reorder=-1;else if(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)actions.reorder=1;else if(button==SDL_CONTROLLER_BUTTON_DPAD_UP||button==SDL_CONTROLLER_BUTTON_DPAD_LEFT)actions.navigate=-1;else if(button==SDL_CONTROLLER_BUTTON_DPAD_DOWN||button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)actions.navigate=1;continue;}
            if(currentScreen==Screen::Bluetooth){if(button==SDL_CONTROLLER_BUTTON_B)actions.back=true;else if(button==SDL_CONTROLLER_BUTTON_A)actions.accept=true;else if(button==SDL_CONTROLLER_BUTTON_X)actions.toggleBluetooth=true;else if(button==SDL_CONTROLLER_BUTTON_Y)actions.refreshBluetooth=true;else if(button==SDL_CONTROLLER_BUTTON_DPAD_UP||button==SDL_CONTROLLER_BUTTON_DPAD_LEFT)actions.navigate=-1;else if(button==SDL_CONTROLLER_BUTTON_DPAD_DOWN||button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)actions.navigate=1;continue;}
            if(button==SDL_CONTROLLER_BUTTON_B)actions.back=true;
            if(currentScreen==Screen::Collections&&button==SDL_CONTROLLER_BUTTON_Y)actions.openFolders=true;
            if((currentScreen==Screen::Collections||currentScreen==Screen::CoverFlow||currentScreen==Screen::AlbumList)&&button==SDL_CONTROLLER_BUTTON_X)actions.rescan=true;
            if((currentScreen==Screen::CoverFlow||currentScreen==Screen::AlbumList)&&button==SDL_CONTROLLER_BUTTON_Y)actions.toggleLibraryView=true;
            if(currentScreen==Screen::Folders&&button==SDL_CONTROLLER_BUTTON_Y)actions.rescan=true;
            if(currentScreen==Screen::Player&&button==SDL_CONTROLLER_BUTTON_Y)actions.addBookmark=true;
            if(currentScreen==Screen::Player&&button==SDL_CONTROLLER_BUTTON_X)actions.jumpBookmark=true;
            if(currentScreen==Screen::Player){if(button==SDL_CONTROLLER_BUTTON_A)actions.togglePause=true;if(button==SDL_CONTROLLER_BUTTON_DPAD_LEFT)actions.seekSeconds=-10;if(button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)actions.seekSeconds=10;if(button==SDL_CONTROLLER_BUTTON_DPAD_UP)actions.changeTrack=-1;if(button==SDL_CONTROLLER_BUTTON_DPAD_DOWN)actions.changeTrack=1;if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)actions.seekSeconds=-30;if(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)actions.seekSeconds=30;}
            else {if(button==SDL_CONTROLLER_BUTTON_A)actions.accept=true;if(button==SDL_CONTROLLER_BUTTON_DPAD_UP||button==SDL_CONTROLLER_BUTTON_DPAD_LEFT)actions.navigate=-1;if(button==SDL_CONTROLLER_BUTTON_DPAD_DOWN||button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)actions.navigate=1;if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)actions.navigate=-1;if(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)actions.navigate=1;}
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
    // A solo two-second hold requests backgrounding (or a plain quit if
    // nothing is playing - Application decides which). Start+Select
    // together stays an unconditional immediate quit, handled above.
    if (startHeld_ && !startLongPressTriggered_ && SDL_GetTicks() - startPressedAt_ >= 2000U) {
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
