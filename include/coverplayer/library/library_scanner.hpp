#pragma once
#include <string>
#include <vector>
#include <functional>
namespace coverplayer::platform { class FileSystem; }
namespace coverplayer::library {
struct Track { std::string name; std::string path; std::string artist; std::string album; int trackNumber = 0; double durationSeconds = 0.0; std::string coverPath; };
struct Collection {
    std::string name;
    std::string path;
    std::string coverPath;
    std::string artist;
    std::vector<Track> tracks;
    std::vector<Collection> children;
};
using ScanProgress = std::function<void(std::size_t, const std::string&)>;
class LibraryScanner { public: explicit LibraryScanner(const platform::FileSystem& fileSystem); [[nodiscard]] std::vector<Collection> scan(const std::string& root, ScanProgress progress = {}) const; private: const platform::FileSystem& fileSystem_; };
}
