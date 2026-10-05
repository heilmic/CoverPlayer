#pragma once

#include "coverplayer/persistence/progress_store.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace coverplayer::persistence {

class FileProgressStore final : public ProgressStore {
public:
    FileProgressStore();

    [[nodiscard]] std::optional<TrackProgress> load(const std::string& mediaId) override;
    bool save(const std::string& mediaId, const TrackProgress& progress) override;
    [[nodiscard]] std::string lastMediaId() override;
    [[nodiscard]] std::string mediaRoot() override;
    bool saveMediaRoot(const std::string& path) override;
    [[nodiscard]] std::vector<MediaCollection> collections() override;
    bool saveCollections(const std::vector<MediaCollection>& collections) override;
    [[nodiscard]] std::vector<double> bookmarks(const std::string& mediaId) override;
    bool addBookmark(const std::string& mediaId, double positionSeconds) override;

    [[nodiscard]] const std::string& error() const noexcept;

private:
    bool loadFile();
    bool writeFile();

    std::filesystem::path stateDirectory_;
    std::filesystem::path progressFile_;
    std::filesystem::path collectionsFile_;
    std::filesystem::path namedCollectionsFile_;
    std::filesystem::path bookmarksFile_;
    std::map<std::string, TrackProgress> entries_;
    std::string lastMediaId_;
    std::string mediaRoot_;
    bool loaded_ = false;
    std::string error_;
};

} // namespace coverplayer::persistence
