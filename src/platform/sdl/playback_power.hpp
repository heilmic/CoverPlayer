#pragma once
#include <cstdint>
#include <string>
namespace coverplayer::platform {
// Temporary runtime inhibitors and backlight control; never edits firmware settings.
class PlaybackPower {
public:
    explicit PlaybackPower(bool foreground = true);
    ~PlaybackPower();
    PlaybackPower(const PlaybackPower&) = delete;
    PlaybackPower& operator=(const PlaybackPower&) = delete;
    void playback(bool active);
    bool activity(); // true when the wake input must be consumed
    void tick(std::uint32_t now);
    bool dark() const { return dark_; }
    static int watch(long owner);
private:
    void wake();
    bool foreground_, active_ = false, dark_ = false, started_ = false;
    long owner_ = 0;
    std::uint32_t lastActivity_ = 0, lastCheck_ = 0, timeout_ = 60000;
    std::string backend_, backlight_, runtime_;
    int saved_ = -1;
};
}
