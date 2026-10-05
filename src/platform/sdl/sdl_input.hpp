#pragma once

#include "coverplayer/platform/platform.hpp"

#include <SDL.h>

namespace coverplayer::platform {

// Owns the game controller lifecycle and maps raw SDL keyboard/controller
// events to InputActions. Knows the current Screen only as a parameter, so
// it has no dependency on rendering or Linux system integration.
class SdlInput {
public:
    SdlInput();
    ~SdlInput();

    SdlInput(const SdlInput&) = delete;
    SdlInput& operator=(const SdlInput&) = delete;

    [[nodiscard]] InputActions poll(Screen currentScreen, bool bluetoothEnabled);
    [[nodiscard]] bool helpVisible() const noexcept { return helpVisible_; }

private:
    void openController(int deviceIndex);
    void closeController();

    SDL_GameController* controller_ = nullptr;
    SDL_JoystickID controllerInstanceId_ = -1;
    bool leftTriggerPressed_ = false;
    bool rightTriggerPressed_ = false;
    bool swapFaceButtons_ = false;
    bool helpVisible_ = false;
    bool startHeld_ = false;
    bool startLongPressTriggered_ = false;
    Uint32 startPressedAt_ = 0;
};

} // namespace coverplayer::platform
