#include "platform/native_file_system.hpp"
#include <SDL.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
void appendSize(std::vector<unsigned char>& bytes, std::size_t size) {
    bytes.push_back(static_cast<unsigned char>((size >> 24U) & 0xffU));
    bytes.push_back(static_cast<unsigned char>((size >> 16U) & 0xffU));
    bytes.push_back(static_cast<unsigned char>((size >> 8U) & 0xffU));
    bytes.push_back(static_cast<unsigned char>(size & 0xffU));
}

void appendTextFrame(std::vector<unsigned char>& tag, const char* id, const std::string& value) {
    tag.insert(tag.end(), id, id + 4);
    appendSize(tag, value.size() + 1);
    tag.push_back(0); tag.push_back(0);
    tag.push_back(3);
    tag.insert(tag.end(), value.begin(), value.end());
}

void writeTaggedMp3(const std::filesystem::path& path) {
    std::vector<unsigned char> tag;
    appendTextFrame(tag, "TIT2", "A Test Track");
    appendTextFrame(tag, "TPE1", "An Artist");
    appendTextFrame(tag, "TALB", "An Album");
    appendTextFrame(tag, "TRCK", "7/12");
    std::vector<unsigned char> bytes{'I','D','3',3,0,0,
        static_cast<unsigned char>((tag.size() >> 21U) & 0x7fU),
        static_cast<unsigned char>((tag.size() >> 14U) & 0x7fU),
        static_cast<unsigned char>((tag.size() >> 7U) & 0x7fU),
        static_cast<unsigned char>(tag.size() & 0x7fU)};
    bytes.insert(bytes.end(), tag.begin(), tag.end());
    bytes.insert(bytes.end(), {0xff, 0xfb, 0x90, 0x64});
    bytes.resize(bytes.size() + 512, 0);
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
}

int main(int, char**) {
    const auto root = std::filesystem::temp_directory_path() / "coverplayer-native-fs-test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error) return 1;
    const auto media = root / "track.mp3";
    writeTaggedMp3(media);

    coverplayer::platform::NativeFileSystem fileSystem;
    const auto metadata = fileSystem.metadata(media.u8string());
    if (!metadata.valid || metadata.title != "A Test Track" || metadata.artist != "An Artist" ||
        metadata.album != "An Album" || metadata.trackNumber != 7) {
        std::cerr << "ID3v2 metadata parsing or MP3 validation failed\n";
        return 1;
    }
    const auto before = fileSystem.fingerprint(root.u8string());
    std::filesystem::create_directories(root / "inside", error);
    std::ofstream(root / "cover.jpg", std::ios::binary).put('x');
    const auto after = fileSystem.fingerprint(root.u8string());
    if (!fileSystem.directoryExists(root.u8string()) || before == after ||
        fileSystem.normalizedPath((root / ".").u8string()) != fileSystem.normalizedPath(root.u8string()) ||
        !fileSystem.isPathWithin(root.u8string(), (root / "inside").u8string()) ||
        fileSystem.isPathWithin((root / "inside").u8string(), root.u8string())) {
        std::cerr << "Directory availability or cache fingerprint failed\n";
        return 1;
    }
    std::filesystem::remove_all(root, error);
    return 0;
}
