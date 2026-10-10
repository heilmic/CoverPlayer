#pragma once
#include <string>
#include <vector>

namespace coverplayer::platform {
struct AudioStream {
    unsigned index = 0;
    std::string identity;
    std::vector<unsigned> volume;
    bool own = false;
    std::string restoreKey;
};
// pactl's C-locale text format; raw channel values preserve stereo balance.
std::vector<AudioStream> parseAudioStreams(const std::string& listing);
// Percent refers to linear audio amplitude, not PulseAudio UI volume.
std::vector<unsigned> duckedVolume(const std::vector<unsigned>& volume, unsigned percent);
void startAudioDucker();
int runAudioDucker(long owner);
void recoverAudioDucking();
}
