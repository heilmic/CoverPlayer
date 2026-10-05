#include "audio/mp3_duration.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

void writeFile(const char* path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

int main(int argc, char** argv) {
    if (argc > 1) {
        const double duration = coverplayer::audio::estimateMp3DurationSeconds(argv[1]);
        std::cout << duration << '\n';
        return duration > 0.0 ? 0 : 1;
    }
    const char* path = "coverplayer-duration-test.mp3";
    std::vector<unsigned char> cbr(16000, 0);
    cbr[0] = 0xff; cbr[1] = 0xfb; cbr[2] = 0x90; cbr[3] = 0x64;
    writeFile(path, cbr);
    const double cbrDuration = coverplayer::audio::estimateMp3DurationSeconds(path);

    std::vector<unsigned char> xing(128, 0);
    xing[0] = 0xff; xing[1] = 0xfb; xing[2] = 0x90; xing[3] = 0x64;
    xing[36] = 'X'; xing[37] = 'i'; xing[38] = 'n'; xing[39] = 'g';
    xing[43] = 1;
    xing[46] = 3; xing[47] = 0xe8; // 1000 frames
    writeFile(path, xing);
    const double xingDuration = coverplayer::audio::estimateMp3DurationSeconds(path);
    std::remove(path);

    const double expectedXing = 1000.0 * 1152.0 / 44100.0;
    if (std::abs(cbrDuration - 1.0) > 0.01 || std::abs(xingDuration - expectedXing) > 0.001) {
        std::cerr << "MP3 duration parser failed: CBR=" << cbrDuration << " Xing=" << xingDuration << '\n';
        return 1;
    }
    return 0;
}
