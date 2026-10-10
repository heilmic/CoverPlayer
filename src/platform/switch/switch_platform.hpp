#pragma once

#include "coverplayer/platform/platform.hpp"
#include "platform/sdl/sdl_input.hpp"
#include "platform/sdl/sdl_renderer.hpp"

#include <switch.h>
#include <utility>

namespace coverplayer::platform {

class SwitchPlatform final : public Platform {
public:
    SwitchPlatform();
    ~SwitchPlatform() override;
    std::string name() const override { return "switch-sdl2"; }
    Capabilities capabilities() const noexcept override { return {false, false, true}; }
    InputActions pollEvents() override;
    void setPlaybackStatus(bool active, bool paused, double position, double duration) override;
    void setView(ViewModel view) override { renderer_.setView(std::move(view)); }
    void setSleepTimer(int minutes) override { renderer_.setSleepTimer(minutes); }
    void setPlayerDetails(int volume, std::size_t bookmarks, std::string notice,
                          std::optional<int> battery) override {
        renderer_.setPlayerDetails(volume, bookmarks, std::move(notice), battery);
    }
    void setLanguage(Language language) override { renderer_.setLanguage(language); }
    std::optional<int> systemVolumePercent() const override { return std::nullopt; }
    std::optional<int> adjustSystemVolume(int) override { return std::nullopt; }
    std::optional<int> batteryPercent() const override;

private:
    struct SdlGuard {
        SdlGuard();
        ~SdlGuard();
    } guard_;
    SdlRenderer renderer_;
    SdlInput input_;
    AppletHookCookie hook_{};
    bool suspendPending_ = false;
    bool resumePending_ = false;
    bool sleepDisabled_ = false;
    bool focused_ = true;
    mutable int battery_ = -1;
    mutable Uint32 batteryReadAt_ = 0;
    static void onAppletEvent(AppletHookType type, void* context);
};

} // namespace coverplayer::platform
