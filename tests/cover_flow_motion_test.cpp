#include "platform/sdl/cover_flow_motion.hpp"
#include <cmath>
#include <initializer_list>
#include <stdexcept>

using coverplayer::platform::CoverFlowMotion;
void require(bool value) { if (!value) throw std::runtime_error("CoverFlow motion regression"); }
int main() {
    CoverFlowMotion low, high;
    low.shift(1); high.shift(1);
    for (int i = 0; i < 6; ++i) low.advance(1.0 / 30);
    for (int i = 0; i < 24; ++i) high.advance(1.0 / 120);
    require(std::abs(low.offset() - high.offset()) < 1e-10);
    require(std::abs(low.velocity() - high.velocity()) < 1e-10);
    // Same-direction taps and reversals preserve world position and velocity.
    for (double shift : {1.0, 1.0, -1.0, -1.0, 1.0}) {
        const double before = low.offset(), speed = low.velocity();
        low.shift(shift);
        require(std::abs(low.offset() - shift - before) < 1e-10);
        require(low.velocity() == speed);
        low.advance(0.045);
    }
    low.advance(2);
    require(low.offset() == 0 && low.velocity() == 0);
    high.reset(); high.shift(-1);
    double previous = -1;
    for (int i = 0; i < 120; ++i) {
        high.advance(1.0 / 120);
        require(high.offset() >= previous && high.offset() <= 0);
        previous = high.offset();
    }
    high.shift(3); high.reset();
    require(high.offset() == 0 && high.velocity() == 0);
}
