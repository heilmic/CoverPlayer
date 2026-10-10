#pragma once
#include <cmath>

namespace coverplayer::platform {
// Critically damped motion: retargeting changes the destination, never the
// visible position or velocity. Exact integration is independent of frame rate.
class CoverFlowMotion {
public:
    void reset() { offset_ = velocity_ = 0.0; }
    void shift(double slots) { offset_ += slots; }
    void advance(double seconds) {
        if (seconds <= 0.0) return;
        constexpr double omega = 18.0;
        const double impulse = velocity_ + omega * offset_;
        const double decay = std::exp(-omega * seconds);
        offset_ = (offset_ + impulse * seconds) * decay;
        velocity_ = (velocity_ - omega * impulse * seconds) * decay;
        if (std::abs(offset_) < 0.0001 && std::abs(velocity_) < 0.001) reset();
    }
    double offset() const { return offset_; }
    double velocity() const { return velocity_; }
private:
    double offset_ = 0.0;
    double velocity_ = 0.0;
};
}
