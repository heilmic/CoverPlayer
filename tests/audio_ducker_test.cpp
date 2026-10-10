#include "platform/sdl/audio_ducker.hpp"
#include <SDL.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <fstream>
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--recover") {
        coverplayer::platform::recoverAudioDucking();
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--audio-ducking")
        return coverplayer::platform::runAudioDucker(std::stol(argv[2]));
    if (argc == 2 && std::string(argv[1]) == "--background-audio") {
        if (const char* path = std::getenv("FAKE_OWNER_FILE")) {
            // Some containers mount the host procfs in a nested PID namespace.
            const std::string temporary = std::string(path) + ".tmp";
            std::ofstream(temporary) << std::filesystem::read_symlink("/proc/self").string();
            std::filesystem::rename(temporary, path);
        }
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    using namespace coverplayer::platform;
    const auto streams = parseAudioStreams(R"(Sink Input #12
    Volume: front-left: 32768 / 50% / -18.06 dB, front-right: 16384 / 25% / -36.12 dB
    Base Volume: 65536 / 100% / 0 dB
    Properties:
        application.name = "RetroArch"
        application.process.id = "412"
        object.serial = "55"
        module-stream-restore.id = "sink-input-by-application-name:RetroArch"
Sink Input #13
    Volume: mono: 65536 / 100% / 0 dB
        application.name = "PipeWire ALSA [coverplayer]"
        node.name = "alsa_playback.coverplayer"
Sink Input #14
    Volume: mono: 40000 / 61% / -12 dB
        application.process.binary = "coverplayer"
Sink Input #15
    Volume: mono: 65536 / 100% / 0 dB
        application.name = "not-coverplayer"
Sink Input #16
    Volume: nonsense
        application.name = "invalid"
)");
    if (streams.size() != 4 || streams[0].index != 12 || streams[0].own ||
        streams[0].volume != std::vector<unsigned>{32768, 16384} ||
        !streams[1].own || !streams[2].own || streams[3].own ||
        duckedVolume(streams[0].volume, 25) != std::vector<unsigned>{20643, 10321} ||
        streams[0].restoreKey != "sink-input-by-application-name:RetroArch" ||
        duckedVolume(std::vector<unsigned>{65536, 32768, 16384}, 50) != std::vector<unsigned>{52016, 26008, 13004} ||
        duckedVolume(streams[0].volume, 0) != std::vector<unsigned>{0, 0} ||
        duckedVolume(streams[0].volume, 100) != streams[0].volume) {
        std::cerr << "stream parsing, ownership or relative stereo attenuation failed\n";
        return 1;
    }
    return 0;
}
