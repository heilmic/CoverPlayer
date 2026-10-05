#include "coverplayer/library/library_scanner.hpp"
#include "coverplayer/platform/file_system.hpp"
#include "audio/mp3_duration.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <unordered_set>

namespace coverplayer::library {
namespace {
constexpr std::size_t maximumDepth = 64;

bool isMp3(const std::string& name) {
    if (name.size() < 4 || name.rfind("._", 0) == 0) return false;
    auto extension = name.substr(name.size() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension == ".mp3";
}

bool isCover(const std::string& name) {
    auto value = name;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value == "cover.jpg" || value == "cover.jpeg" || value == "cover.png" ||
        value == "folder.jpg" || value == "folder.png";
}

bool naturalLess(const std::string& left, const std::string& right) {
    std::size_t leftIndex = 0, rightIndex = 0;
    while (leftIndex < left.size() && rightIndex < right.size()) {
        const auto leftCharacter = static_cast<unsigned char>(left[leftIndex]);
        const auto rightCharacter = static_cast<unsigned char>(right[rightIndex]);
        if (std::isdigit(leftCharacter) && std::isdigit(rightCharacter)) {
            std::size_t leftEnd = leftIndex, rightEnd = rightIndex;
            while (leftEnd < left.size() && std::isdigit(static_cast<unsigned char>(left[leftEnd]))) ++leftEnd;
            while (rightEnd < right.size() && std::isdigit(static_cast<unsigned char>(right[rightEnd]))) ++rightEnd;
            const auto leftNumber = std::stoull(left.substr(leftIndex, leftEnd - leftIndex));
            const auto rightNumber = std::stoull(right.substr(rightIndex, rightEnd - rightIndex));
            if (leftNumber != rightNumber) return leftNumber < rightNumber;
            leftIndex = leftEnd; rightIndex = rightEnd; continue;
        }
        const auto foldedLeft = static_cast<unsigned char>(std::tolower(leftCharacter));
        const auto foldedRight = static_cast<unsigned char>(std::tolower(rightCharacter));
        if (foldedLeft != foldedRight) return foldedLeft < foldedRight;
        ++leftIndex; ++rightIndex;
    }
    return left.size() < right.size();
}

std::string folderName(const std::string& path) {
    const auto end = path.find_last_not_of("/\\");
    if (end == std::string::npos) return path;
    const auto slash = path.find_last_of("/\\", end);
    const auto start = slash == std::string::npos ? 0 : slash + 1;
    return path.substr(start, end - start + 1);
}

struct ScannedTrack { Track track; std::string coverPath; };
struct ScanContext {
    const platform::FileSystem& fileSystem;
    const ScanProgress& progress;
    std::unordered_set<std::string> visitedPaths;
    std::size_t visitedCount = 0;
};

std::optional<Collection> scanDirectory(ScanContext& context, const std::string& path, std::size_t depth) {
    if (depth > maximumDepth) {
        std::cerr << "CoverPlayer skipped directory below depth limit: " << path << '\n';
        return std::nullopt;
    }
    if (!context.visitedPaths.insert(context.fileSystem.normalizedPath(path)).second) return std::nullopt;
    ++context.visitedCount;
    if (context.progress) context.progress(context.visitedCount, path);

    const auto entries = context.fileSystem.list(path);
    std::vector<Collection> children;
    std::vector<ScannedTrack> directTracks;
    std::string folderCover;
    for (const auto& entry : entries) {
        if (entry.directory) {
            auto child = scanDirectory(context, entry.path, depth + 1);
            if (child) children.push_back(std::move(*child));
        } else if (isMp3(entry.name)) {
            const auto metadata = context.fileSystem.metadata(entry.path);
            if (!metadata.valid) {
                std::cerr << "CoverPlayer skipped invalid MP3: " << entry.path << '\n';
                continue;
            }
            const auto filename = entry.name.substr(0, entry.name.size() - 4);
            const auto durationSeconds = coverplayer::audio::estimateMp3DurationSeconds(entry.path);
            directTracks.push_back({{metadata.title.empty() ? filename : metadata.title, entry.path,
                metadata.artist, metadata.album, metadata.trackNumber, durationSeconds}, metadata.embeddedCoverPath});
        } else if (folderCover.empty() && isCover(entry.name)) {
            folderCover = entry.path;
        }
    }

    std::sort(directTracks.begin(), directTracks.end(), [](const auto& left, const auto& right) {
        if (left.track.trackNumber > 0 && right.track.trackNumber > 0 &&
            left.track.trackNumber != right.track.trackNumber) return left.track.trackNumber < right.track.trackNumber;
        return naturalLess(left.track.name, right.track.name);
    });

    if (children.empty()) {
        if (directTracks.empty()) return std::nullopt;
        Collection leaf;
        // The folder name is the stable, user-controlled identity of an
        // album; the ID3 album tag is metadata about the tracks, not a
        // naming authority, and can disagree with how the user organized
        // their files (e.g. multiple ID3 album variants inside one folder).
        leaf.name = folderName(path);
        leaf.path = path;
        leaf.coverPath = folderCover;
        leaf.artist = directTracks.front().track.artist;
        for (auto& scanned : directTracks) {
            if (leaf.coverPath.empty() && !scanned.coverPath.empty()) leaf.coverPath = scanned.coverPath;
            leaf.tracks.push_back(std::move(scanned.track));
        }
        return leaf;
    }

    for (auto& scanned : directTracks) {
        Collection single;
        single.name = scanned.track.name;
        single.path = scanned.track.path;
        single.coverPath = scanned.coverPath.empty() ? folderCover : scanned.coverPath;
        single.artist = scanned.track.artist;
        single.tracks.push_back(std::move(scanned.track));
        children.push_back(std::move(single));
    }
    std::sort(children.begin(), children.end(), [](const auto& left, const auto& right) {
        return naturalLess(left.name, right.name);
    });

    Collection group;
    group.name = folderName(path);
    group.path = path;
    group.coverPath = folderCover;
    group.children = std::move(children);
    if (group.coverPath.empty()) {
        for (const auto& child : group.children) if (!child.coverPath.empty()) {
            group.coverPath = child.coverPath;
            break;
        }
    }
    return group;
}
}

LibraryScanner::LibraryScanner(const platform::FileSystem& fileSystem) : fileSystem_(fileSystem) {}

std::vector<Collection> LibraryScanner::scan(const std::string& root, ScanProgress progress) const {
    ScanContext context{fileSystem_, progress, {}, 0};
    auto rootNode = scanDirectory(context, root, 0);
    if (!rootNode) return {};
    if (!rootNode->children.empty()) return std::move(rootNode->children);
    return {std::move(*rootNode)};
}
}
