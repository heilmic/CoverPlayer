#pragma once

#include <string>

namespace coverplayer::audio {

class AudioPlayer {
public:
    virtual ~AudioPlayer() = default;

    virtual bool open(const std::string& path) = 0;
    virtual void update() = 0;
    virtual void togglePause() = 0;
    virtual void seekSeconds(double delta) = 0;
    virtual void setVolumePercent(int percent) = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual bool isPaused() const noexcept = 0;
    [[nodiscard]] virtual bool isFinished() const noexcept = 0;
    [[nodiscard]] virtual double positionSeconds() const noexcept = 0;
    [[nodiscard]] virtual double durationSeconds() const noexcept = 0;
    [[nodiscard]] virtual int volumePercent() const noexcept = 0;
    [[nodiscard]] virtual std::string error() const = 0;
};

} // namespace coverplayer::audio
