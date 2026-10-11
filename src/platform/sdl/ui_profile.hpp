#pragma once

#include <cstddef>

namespace coverplayer::platform::ui {

// Selected per translation unit for the device or the explicit desktop preview.
// Font paths and SDL geometry loading still depend on the actual build platform.
struct Profile {
    bool switchUi;
    int width;
    int height;
    int bodyFontSize;
    int headingFontSize;
    std::size_t coverCacheLimit;
    int maximumCoverSide;
    float halfCoverSize;
    float singleHalfCoverSize;
    float focalLength;
    float coverCenterY;
    int artworkY;
    int artworkHeight;
    int listDetailOffset;
};

#ifdef COVERPLAYER_SWITCH_UI
inline constexpr Profile profile{true,   1280,   720,    28,     40, 16,  512,
                                 200.0F, 208.0F, 780.0F, 322.0F, 84, 450, 34};
#else
inline constexpr Profile profile{false,  640,    480,    18,     26, 24,  384,
                                 128.0F, 132.0F, 520.0F, 214.0F, 70, 288, 20};
#endif

} // namespace coverplayer::platform::ui
