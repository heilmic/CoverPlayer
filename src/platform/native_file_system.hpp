#pragma once
#include "coverplayer/platform/file_system.hpp"
namespace coverplayer::platform { class NativeFileSystem final:public FileSystem { public:
    [[nodiscard]] std::vector<DirectoryEntry> list(const std::string& path) const override;
    [[nodiscard]] std::string parent(const std::string& path) const override;
    [[nodiscard]] bool directoryExists(const std::string& path) const override;
    [[nodiscard]] std::uint64_t fingerprint(const std::string& path) const override;
    [[nodiscard]] MediaMetadata metadata(const std::string& path) const override;
    [[nodiscard]] std::string normalizedPath(const std::string& path) const override;
    [[nodiscard]] bool isPathWithin(const std::string& root, const std::string& path) const override;
}; }
