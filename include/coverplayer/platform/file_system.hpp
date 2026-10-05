#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace coverplayer::platform {
struct DirectoryEntry { std::string name; std::string path; bool directory = false; };
struct MediaMetadata { bool valid = true; std::string title; std::string artist; std::string album; int trackNumber = 0; std::string embeddedCoverPath; };
class FileSystem { public:
    virtual ~FileSystem() = default;
    [[nodiscard]] virtual std::vector<DirectoryEntry> list(const std::string& path) const = 0;
    [[nodiscard]] virtual std::string parent(const std::string& path) const = 0;
    [[nodiscard]] virtual bool directoryExists(const std::string&) const { return true; }
    [[nodiscard]] virtual std::uint64_t fingerprint(const std::string&) const { return 0; }
    [[nodiscard]] virtual MediaMetadata metadata(const std::string&) const { return {}; }
    [[nodiscard]] virtual std::string normalizedPath(const std::string& path) const { return path; }
    [[nodiscard]] virtual bool isPathWithin(const std::string&, const std::string&) const { return true; }
};
}
