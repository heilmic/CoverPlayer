#pragma once

#include "coverplayer/platform/platform.hpp"

#include <SDL.h>

#include <optional>
#include <string>

namespace coverplayer::platform {

// Owns every Linux system-volume and Bluetooth shell integration
// (pactl/bluetoothctl/knulli-bluetooth). No SDL rendering or input code
// depends on this class, and this class never touches a window or renderer.
class LinuxAudioSystem {
public:
    LinuxAudioSystem();

    [[nodiscard]] bool systemVolumeEnabled() const noexcept { return systemVolumeEnabled_; }
    [[nodiscard]] bool bluetoothEnabled() const noexcept { return bluetoothEnabled_; }
    [[nodiscard]] bool bluetoothAudioActive() const noexcept { return bluetoothAudioActive_; }

    [[nodiscard]] std::optional<int> systemVolumePercent() const { return systemVolumePercent_; }
    std::optional<int> adjustSystemVolume(int deltaPercent);

    [[nodiscard]] BluetoothState bluetoothState();
    bool setBluetoothEnabled(bool enabled);
    bool setBluetoothDeviceConnected(const std::string& address, bool connected);

    // Call once per poll iteration; refreshes volume/Bluetooth status on their
    // own cadence, matching the throttling the SDL platform used to do inline.
    void tick();

private:
    void refreshSystemVolume();
    void refreshBluetoothAudioStatus();

    bool systemVolumeEnabled_ = false;
    bool bluetoothEnabled_ = false;
    std::optional<int> systemVolumePercent_;
    bool systemVolumeMuted_ = false;
    Uint32 volumeRefreshAt_ = 0;
    bool bluetoothAudioActive_ = false;
    Uint32 bluetoothRefreshAt_ = 0;
};

} // namespace coverplayer::platform
