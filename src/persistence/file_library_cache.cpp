#include "persistence/file_library_cache.hpp"

#include <SDL.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace coverplayer::persistence {
namespace {
constexpr std::size_t maximumDepth = 64;
constexpr std::size_t maximumNodes = 1000000;
constexpr std::size_t maximumTracks = 1000000;

std::uint64_t hashRoot(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char byte : value) { hash ^= byte; hash *= 1099511628211ULL; }
    return hash;
}

bool writeNode(std::ostream& output, const library::Collection& node, std::size_t depth) {
    if (depth > maximumDepth || node.tracks.size() > maximumTracks || node.children.size() > maximumNodes) return false;
    output << std::quoted(node.name) << ' ' << std::quoted(node.path) << ' '
        << std::quoted(node.coverPath) << ' ' << std::quoted(node.artist) << ' '
        << node.tracks.size() << ' ' << node.children.size() << '\n';
    for (const auto& track : node.tracks) {
        output << std::quoted(track.name) << ' ' << std::quoted(track.path) << ' '
            << std::quoted(track.artist) << ' ' << std::quoted(track.album) << ' '
            << track.trackNumber << ' ' << track.durationSeconds << '\n';
    }
    for (const auto& child : node.children) if (!writeNode(output, child, depth + 1)) return false;
    return static_cast<bool>(output);
}

bool readNode(std::istream& input, library::Collection& node, std::size_t depth, std::size_t& nodesRead) {
    if (depth > maximumDepth || ++nodesRead > maximumNodes) return false;
    std::size_t trackTotal = 0, childTotal = 0;
    if (!(input >> std::quoted(node.name) >> std::quoted(node.path) >> std::quoted(node.coverPath)
        >> std::quoted(node.artist) >> trackTotal >> childTotal) ||
        trackTotal > maximumTracks || childTotal > maximumNodes) return false;
    node.tracks.reserve(trackTotal);
    for (std::size_t index = 0; index < trackTotal; ++index) {
        library::Track track;
        if (!(input >> std::quoted(track.name) >> std::quoted(track.path) >> std::quoted(track.artist)
            >> std::quoted(track.album) >> track.trackNumber >> track.durationSeconds)) return false;
        node.tracks.push_back(std::move(track));
    }
    node.children.resize(childTotal);
    for (auto& child : node.children) if (!readNode(input, child, depth + 1, nodesRead)) return false;
    return true;
}
}

FileLibraryCache::FileLibraryCache() {
    char* path = SDL_GetPrefPath("CoverPlayer", "CoverPlayer");
    if (path) { directory_ = std::filesystem::u8path(path) / "library-cache"; SDL_free(path); }
}

FileLibraryCache::FileLibraryCache(std::filesystem::path directory) : directory_(std::move(directory)) {}

std::filesystem::path FileLibraryCache::pathFor(const std::string& root) const {
    std::ostringstream name; name << std::hex << hashRoot(root) << ".idx"; return directory_ / name.str();
}

std::optional<std::vector<library::Collection>> FileLibraryCache::load(const std::string& root, std::uint64_t fingerprint) {
    std::ifstream input(pathFor(root));
    std::string header, cachedRoot; std::uint64_t cachedFingerprint = 0;
    if (!std::getline(input, header) || header != "COVERPLAYER_LIBRARY 4" ||
        !(input >> std::quoted(cachedRoot) >> cachedFingerprint) || cachedRoot != root ||
        cachedFingerprint != fingerprint) return std::nullopt;
    std::size_t nodeTotal = 0;
    if (!(input >> nodeTotal) || nodeTotal > maximumNodes) return std::nullopt;
    std::vector<library::Collection> result(nodeTotal); std::size_t nodesRead = 0;
    for (auto& node : result) if (!readNode(input, node, 0, nodesRead)) return std::nullopt;
    return result;
}

bool FileLibraryCache::save(const std::string& root, std::uint64_t fingerprint,
    const std::vector<library::Collection>& collections) {
    if (collections.size() > maximumNodes) return false;
    std::error_code error; std::filesystem::create_directories(directory_, error); if (error) return false;
    const auto target = pathFor(root); const auto temporary = std::filesystem::path(target.string() + ".tmp");
    {
        std::ofstream output(temporary, std::ios::trunc);
        output << "COVERPLAYER_LIBRARY 4\n" << std::quoted(root) << ' ' << fingerprint << '\n'
            << collections.size() << '\n';
        for (const auto& node : collections) if (!writeNode(output, node, 0)) return false;
        output.flush(); if (!output) return false;
    }
    std::filesystem::rename(temporary, target, error);
    if (error) { std::filesystem::remove(target, error); error.clear(); std::filesystem::rename(temporary, target, error); }
    return !error;
}

void FileLibraryCache::invalidate(const std::string& root) {
    std::error_code error; std::filesystem::remove(pathFor(root), error);
}
}
