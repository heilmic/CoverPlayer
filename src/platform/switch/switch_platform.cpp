#include "platform/switch/switch_platform.hpp"

#include <stdexcept>

namespace coverplayer::platform {

SwitchPlatform::SdlGuard::SdlGuard() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        throw std::runtime_error(std::string("SDL initialization failed: ") + SDL_GetError());
}

SwitchPlatform::SdlGuard::~SdlGuard() { SDL_Quit(); }

SwitchPlatform::SwitchPlatform() {
    appletHook(&hook_, onAppletEvent, this);
    renderer_.setBluetoothStatus(false, false);
}

SwitchPlatform::~SwitchPlatform() {
    appletUnhook(&hook_);
    if (sleepDisabled_) appletSetAutoSleepDisabled(false);
}

void SwitchPlatform::onAppletEvent(AppletHookType type, void* context) {
    auto& self = *static_cast<SwitchPlatform*>(context);
    if (type == AppletHookType_OnFocusState) {
        self.focused_ = appletGetFocusState() == AppletFocusState_InFocus;
        if (self.focused_) self.resumePending_ = true;
        else self.suspendPending_ = true;
    } else if (type == AppletHookType_OnResume && appletGetFocusState() == AppletFocusState_InFocus) {
        self.resumePending_ = true;
    }
}

InputActions SwitchPlatform::pollEvents() {
    // The Switch SDL event pump calls appletMainLoop and handles dock changes.
    auto actions = input_.poll(renderer_.currentScreen(), false);
    actions.suspend = actions.suspend || suspendPending_;
    actions.resume = actions.resume || resumePending_;
    suspendPending_ = resumePending_ = false;
    // A normal NRO cannot hand audio to an independent game-time service.
    if (actions.background) { actions.background = false; actions.quit = true; }
    renderer_.setHelpVisible(input_.helpVisible());
    renderer_.setDrawingEnabled(focused_);
    if (!focused_) SDL_Delay(40);
    renderer_.present();
    return actions;
}

void SwitchPlatform::setPlaybackStatus(bool active, bool paused, double position, double duration) {
    renderer_.setPlaybackStatus(active, paused, position, duration);
    const bool disableSleep = active && !paused;
    if (disableSleep != sleepDisabled_) {
        if (R_SUCCEEDED(appletSetAutoSleepDisabled(disableSleep))) sleepDisabled_ = disableSleep;
    }
}

std::optional<int> SwitchPlatform::batteryPercent() const {
    const Uint32 now = SDL_GetTicks();
    if (batteryReadAt_ == 0 || now - batteryReadAt_ >= 30000U) {
        SDL_GetPowerInfo(nullptr, &battery_);
        batteryReadAt_ = now;
    }
    return battery_ >= 0 && battery_ <= 100 ? std::optional<int>{battery_} : std::nullopt;
}

} // namespace coverplayer::platform
