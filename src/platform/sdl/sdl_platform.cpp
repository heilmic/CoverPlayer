#include "platform/sdl/sdl_platform.hpp"
#include "platform/sdl/linux_background_session.hpp"
#include "platform/sdl/linux_battery.hpp"

#include <stdexcept>
#include <utility>

namespace coverplayer::platform {

SdlPlatform::SdlSubsystemGuard::SdlSubsystemGuard() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        throw std::runtime_error(std::string("SDL initialization failed: ") + SDL_GetError());
    }
}

SdlPlatform::SdlSubsystemGuard::~SdlSubsystemGuard() { SDL_Quit(); }

SdlPlatform::SdlPlatform() {
    renderer_.setBluetoothStatus(audio_.bluetoothEnabled(), audio_.bluetoothAudioActive());
    renderer_.present();
}

SdlPlatform::~SdlPlatform() = default;

std::string SdlPlatform::name() const {
    return "desktop-sdl2";
}

Capabilities SdlPlatform::capabilities() const noexcept {
    return {audio_.systemVolumeEnabled(), audio_.bluetoothEnabled(), true};
}

InputActions SdlPlatform::pollEvents() {
    auto actions = input_.poll(renderer_.currentScreen(), audio_.bluetoothEnabled());
    audio_.tick();
    renderer_.setHelpVisible(input_.helpVisible());
    renderer_.setBluetoothStatus(audio_.bluetoothEnabled(), audio_.bluetoothAudioActive());
    renderer_.present();
    return actions;
}

void SdlPlatform::setPlaybackStatus(bool active, bool paused, double positionSeconds, double durationSeconds) {
    renderer_.setPlaybackStatus(active, paused, positionSeconds, durationSeconds);
}

void SdlPlatform::setView(ViewModel view) {
    renderer_.setView(std::move(view));
}

void SdlPlatform::setSleepTimer(int minutes) {
    renderer_.setSleepTimer(minutes);
}

void SdlPlatform::setPlayerDetails(int volumePercent, std::size_t bookmarkCount, std::string notice, std::optional<int> batteryPercent) {
    renderer_.setPlayerDetails(volumePercent, bookmarkCount, std::move(notice), batteryPercent);
}

void SdlPlatform::setLanguage(Language language) { renderer_.setLanguage(language); }

std::optional<int> SdlPlatform::systemVolumePercent() const {
    return audio_.systemVolumePercent();
}

std::optional<int> SdlPlatform::adjustSystemVolume(int deltaPercent) {
    return audio_.adjustSystemVolume(deltaPercent);
}

std::optional<int> SdlPlatform::batteryPercent() const {
    const Uint32 now = SDL_GetTicks();
    if (batteryRefreshAt_ == 0 || now - batteryRefreshAt_ >= 30000U) {
        batteryPercent_ = readBatteryPercent();
        batteryRefreshAt_ = now;
    }
    return batteryPercent_;
}

BluetoothState SdlPlatform::bluetoothState() {
    return audio_.bluetoothState();
}

bool SdlPlatform::setBluetoothEnabled(bool enabled) {
    return audio_.setBluetoothEnabled(enabled);
}

bool SdlPlatform::setBluetoothDeviceConnected(const std::string& address, bool connected) {
    return audio_.setBluetoothDeviceConnected(address, connected);
}

bool SdlPlatform::enterBackgroundPlayback(const std::vector<std::string>& trackPaths, std::size_t startIndex) {
    // This process's own window/audio state is never reused after this -
    // the caller performs a completely normal shutdown right afterward
    // regardless of the return value, so there is nothing to tear down or
    // recreate here.
    return LinuxBackgroundSession::spawnBackgroundAudio(trackPaths, startIndex);
}

} // namespace coverplayer::platform
