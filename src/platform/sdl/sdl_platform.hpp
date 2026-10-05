#pragma once

#include "coverplayer/platform/platform.hpp"
#include "platform/sdl/linux_audio_system.hpp"
#include "platform/sdl/sdl_input.hpp"
#include "platform/sdl/sdl_renderer.hpp"

#include <SDL.h>

namespace coverplayer::platform {

// Thin Platform implementation: owns the SDL subsystem lifetime and
// composes a renderer, an input mapper, and the Linux audio/Bluetooth
// integration, forwarding each Platform call to the collaborator that
// owns it.
class SdlPlatform final : public Platform {
public:
    SdlPlatform();
    ~SdlPlatform() override;

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] Capabilities capabilities() const noexcept override;
    [[nodiscard]] InputActions pollEvents() override;
    void setPlaybackStatus(bool active, bool paused, double positionSeconds, double durationSeconds) override;
    void setView(ViewModel view) override;
    void setSleepTimer(int minutes) override;
    void setPlayerDetails(int volumePercent, std::size_t bookmarkCount, std::string notice, std::optional<int> batteryPercent) override;
    [[nodiscard]] std::optional<int> systemVolumePercent() const override;
    std::optional<int> adjustSystemVolume(int deltaPercent) override;
    [[nodiscard]] std::optional<int> batteryPercent() const override;
    [[nodiscard]] BluetoothState bluetoothState() override;
    bool setBluetoothEnabled(bool enabled) override;
    bool setBluetoothDeviceConnected(const std::string& address, bool connected) override;
    bool enterBackgroundPlayback(const std::vector<std::string>& trackPaths, std::size_t startIndex) override;

private:
    // Must be the first member: guarantees SDL_Init()/SDL_Quit() bracket the
    // lifetime of every other member below, even if one of their
    // constructors throws during SdlPlatform construction.
    struct SdlSubsystemGuard {
        SdlSubsystemGuard();
        ~SdlSubsystemGuard();
        SdlSubsystemGuard(const SdlSubsystemGuard&) = delete;
        SdlSubsystemGuard& operator=(const SdlSubsystemGuard&) = delete;
    } sdlGuard_;

    SdlRenderer renderer_;
    SdlInput input_;
    LinuxAudioSystem audio_;

    // Battery charge changes slowly - reading the sysfs power-supply tree on
    // every frame (batteryPercent() is called from Application's ~8ms main
    // loop) would be needless syscall churn, so this is cached and
    // refreshed on the same kind of interval as LinuxAudioSystem's own
    // volume/Bluetooth polling.
    mutable std::optional<int> batteryPercent_;
    mutable Uint32 batteryRefreshAt_ = 0;
};

} // namespace coverplayer::platform
