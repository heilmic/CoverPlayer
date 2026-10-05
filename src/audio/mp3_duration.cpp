#include "audio/mp3_duration.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace coverplayer::audio {
namespace {

std::uint32_t readBigEndian(const std::vector<unsigned char>& data, std::size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return (static_cast<std::uint32_t>(data[offset]) << 24U) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 16U) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 8U) |
        static_cast<std::uint32_t>(data[offset + 3]);
}

bool markerAt(const std::vector<unsigned char>& data, std::size_t offset, const char* marker) {
    return offset + 4 <= data.size() && std::equal(marker, marker + 4, data.begin() + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

double estimateMp3DurationSeconds(const std::string& path) {
    // path is UTF-8 (matching every other file path in this codebase, e.g.
    // NativeFileSystem). A plain std::ifstream(const std::string&) on
    // Windows hands that straight to a narrow-char CreateFileA, which
    // decodes it with the current ANSI code page rather than UTF-8 - a
    // filename with a non-ASCII character (e.g. a German umlaut) then
    // resolves to the wrong bytes and the file is not found.
    // std::filesystem::u8path + the path-based ifstream constructor uses
    // CreateFileW internally and gets this right on every platform.
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!input) return 0.0;
    const auto fileSizeValue = input.tellg();
    if (fileSizeValue <= 0) return 0.0;
    const auto fileSize = static_cast<std::uint64_t>(fileSizeValue);
    input.seekg(0);

    std::array<unsigned char, 10> id3{};
    input.read(reinterpret_cast<char*>(id3.data()), static_cast<std::streamsize>(id3.size()));
    std::uint64_t audioOffset = 0;
    if (input.gcount() == static_cast<std::streamsize>(id3.size()) && id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
        const auto tagSize = (static_cast<std::uint64_t>(id3[6] & 0x7fU) << 21U) |
            (static_cast<std::uint64_t>(id3[7] & 0x7fU) << 14U) |
            (static_cast<std::uint64_t>(id3[8] & 0x7fU) << 7U) |
            static_cast<std::uint64_t>(id3[9] & 0x7fU);
        audioOffset = 10 + tagSize + ((id3[5] & 0x10U) != 0 ? 10 : 0);
    }
    if (audioOffset >= fileSize) return 0.0;
    input.clear();
    input.seekg(static_cast<std::streamoff>(audioOffset));
    const auto readSize = static_cast<std::size_t>(std::min<std::uint64_t>(fileSize - audioOffset, 256U * 1024U));
    std::vector<unsigned char> data(readSize);
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    data.resize(static_cast<std::size_t>(input.gcount()));

    constexpr std::array<int, 14> bitrateMpeg1{32,40,48,56,64,80,96,112,128,160,192,224,256,320};
    constexpr std::array<int, 14> bitrateMpeg2{8,16,24,32,40,48,56,64,80,96,112,128,144,160};
    constexpr std::array<int, 3> sampleRates{44100,48000,32000};
    for (std::size_t offset = 0; offset + 4 <= data.size(); ++offset) {
        const auto b1 = data[offset], b2 = data[offset + 1], b3 = data[offset + 2], b4 = data[offset + 3];
        const int version = (b2 >> 3U) & 3U;
        const int layer = (b2 >> 1U) & 3U;
        const int bitrateIndex = b3 >> 4U;
        const int rateIndex = (b3 >> 2U) & 3U;
        if (b1 != 0xffU || (b2 & 0xe0U) != 0xe0U || version == 1 || layer != 1 ||
            bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3) continue;

        int sampleRate = sampleRates[static_cast<std::size_t>(rateIndex)];
        if (version == 2) sampleRate /= 2;
        else if (version == 0) sampleRate /= 4;
        const bool mono = (b4 >> 6U) == 3U;
        const std::size_t crcBytes = (b2 & 1U) == 0 ? 2 : 0;
        const std::size_t sideInfo = version == 3 ? (mono ? 17 : 32) : (mono ? 9 : 17);
        const std::size_t xing = offset + 4 + crcBytes + sideInfo;
        if (markerAt(data, xing, "Xing") || markerAt(data, xing, "Info")) {
            const auto flags = readBigEndian(data, xing + 4);
            const auto frames = (flags & 1U) != 0 ? readBigEndian(data, xing + 8) : 0;
            if (frames > 0) return static_cast<double>(frames) * (version == 3 ? 1152.0 : 576.0) / sampleRate;
        }
        const std::size_t vbri = offset + 4 + 32;
        if (markerAt(data, vbri, "VBRI")) {
            const auto frames = readBigEndian(data, vbri + 14);
            if (frames > 0) return static_cast<double>(frames) * (version == 3 ? 1152.0 : 576.0) / sampleRate;
        }
        const int bitrate = (version == 3 ? bitrateMpeg1 : bitrateMpeg2)[static_cast<std::size_t>(bitrateIndex - 1)];
        return static_cast<double>((fileSize - audioOffset - offset) * 8U) / (bitrate * 1000.0);
    }
    return 0.0;
}

} // namespace coverplayer::audio
