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

    [[nodiscard]] InputActions poll(Screen currentScreen, bool bluetoothEnabled, bool wakeOnly = false,
        Uint32 now = SDL_GetTicks());
    [[nodiscard]] bool helpVisible() const noexcept { return helpVisible_; }

    bool hadActivity() const { return hadActivity_; }
private:
    bool hadActivity_ = false;
    void openController(int deviceIndex);
    void closeController();
    enum class Direction { None, Left, Right, Up, Down };
    Direction direction() const;
    void updateDirection(InputActions& actions, Screen screen, Uint32 now);
    static void applyDirection(InputActions& actions, Screen screen, Direction direction);
    void clearDirections();
    unsigned keyboardDirections_ = 0;
    unsigned padDirections_ = 0;
    int stickX_ = 0;
    int stickY_ = 0;
    Direction stickDirection_ = Direction::None;
    Direction heldDirection_ = Direction::None;
    Uint32 directionHeldAt_ = 0;
    Uint32 directionRepeatedAt_ = 0;
    bool directionBlocked_ = false;
    bool hasScreen_ = false;
    Screen previousScreen_{};

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
