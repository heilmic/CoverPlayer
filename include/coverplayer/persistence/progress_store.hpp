#pragma once

#include <optional>
#include <string>
#include <vector>

namespace coverplayer::persistence {

struct TrackProgress {
    double positionSeconds = 0.0;
    bool completed = false;
};

struct MediaCollection {
    std::string name;
    std::string path;
    std::string type = "audiobook";
};

class ProgressStore {
public:
    virtual ~ProgressStore() = default;
    [[nodiscard]] virtual std::optional<TrackProgress> load(const std::string& mediaId) = 0;
    virtual bool save(const std::string& mediaId, const TrackProgress& progress) = 0;
    [[nodiscard]] virtual std::string lastMediaId() = 0;
    [[nodiscard]] virtual std::string mediaRoot() = 0;
    virtual bool saveMediaRoot(const std::string& path) = 0;
    [[nodiscard]] virtual std::vector<MediaCollection> collections() = 0;
    virtual bool saveCollections(const std::vector<MediaCollection>& collections) = 0;
    [[nodiscard]] virtual std::vector<double> bookmarks(const std::string& mediaId) = 0;
    virtual bool addBookmark(const std::string& mediaId, double positionSeconds) = 0;
};

} // namespace coverplayer::persistence
