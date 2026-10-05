#pragma once

#include "coverplayer/audio/audio_player.hpp"

#include <SDL.h>
#include <mpg123.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coverplayer::audio {

class Mpg123SdlPlayer final : public AudioPlayer {
public:
    Mpg123SdlPlayer();
    ~Mpg123SdlPlayer() override;

    Mpg123SdlPlayer(const Mpg123SdlPlayer&) = delete;
    Mpg123SdlPlayer& operator=(const Mpg123SdlPlayer&) = delete;

    bool open(const std::string& path) override;
    void update() override;
    void togglePause() override;
    void seekSeconds(double delta) override;
    void setVolumePercent(int percent) override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool isPaused() const noexcept override;
    [[nodiscard]] bool isFinished() const noexcept override;
    [[nodiscard]] double positionSeconds() const noexcept override;
    [[nodiscard]] double durationSeconds() const noexcept override;
    [[nodiscard]] int volumePercent() const noexcept override;
    [[nodiscard]] std::string error() const override;

private:
    void close();
    bool configureOutput();
    void setError(const std::string& message);

    mpg123_handle* decoder_ = nullptr;
    SDL_AudioDeviceID device_ = 0;
    long sampleRate_ = 0;
    int channels_ = 0;
    std::size_t bytesPerSecond_ = 0;
    std::size_t targetQueueBytes_ = 0;
    std::vector<unsigned char> decodeBuffer_;
    bool open_ = false;
    bool paused_ = false;
    bool eofDecoded_ = false;
    bool finished_ = false;
    int volumePercent_ = 100;
    double durationSeconds_ = 0.0;
    std::string error_;
};

} // namespace coverplayer::audio
