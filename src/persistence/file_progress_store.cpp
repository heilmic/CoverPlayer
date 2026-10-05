#include "persistence/file_progress_store.hpp"

#include <SDL.h>

#include <fstream>
#include <iomanip>
#include <system_error>
#include <sstream>

namespace coverplayer::persistence {

FileProgressStore::FileProgressStore() {
    char* preferencePath = SDL_GetPrefPath("CoverPlayer", "CoverPlayer");
    if (preferencePath == nullptr) {
        error_ = std::string("Cannot determine state directory: ") + SDL_GetError();
        return;
    }
    stateDirectory_ = std::filesystem::u8path(preferencePath);
    SDL_free(preferencePath);
    progressFile_ = stateDirectory_ / "progress-v1.txt";
    collectionsFile_ = stateDirectory_ / "collections-v1.txt";
    namedCollectionsFile_ = stateDirectory_ / "collections-v2.txt";
    bookmarksFile_ = stateDirectory_ / "bookmarks-v1.txt";
    languageFile_ = stateDirectory_ / "language-v1.txt";
}

std::optional<TrackProgress> FileProgressStore::load(const std::string& mediaId) {
    if (!loaded_ && !loadFile()) {
        return std::nullopt;
    }
    const auto entry = entries_.find(mediaId);
    if (entry == entries_.end()) {
        return std::nullopt;
    }
    return entry->second;
}

bool FileProgressStore::save(const std::string& mediaId, const TrackProgress& progress) {
    if (!loaded_ && !loadFile()) {
        return false;
    }
    entries_[mediaId] = progress;
    lastMediaId_ = mediaId;
    return writeFile();
}

std::string FileProgressStore::lastMediaId() {
    if (!loaded_) loadFile();
    return lastMediaId_;
}

std::string FileProgressStore::mediaRoot() { if(!loaded_)loadFile();return mediaRoot_; }
bool FileProgressStore::saveMediaRoot(const std::string& path) { if(!loaded_&&!loadFile())return false;mediaRoot_=path;return writeFile(); }

std::vector<MediaCollection> FileProgressStore::collections() {
    std::vector<MediaCollection> result;
    std::ifstream namedInput(namedCollectionsFile_);
    std::string line;
    while (std::getline(namedInput, line)) {
        std::istringstream record(line);
        MediaCollection collection;
        if (record >> std::quoted(collection.name) >> std::quoted(collection.path)) {
            if (!(record >> std::quoted(collection.type))) collection.type = "audiobook";
            result.push_back(std::move(collection));
        }
    }
    if (!result.empty() || std::filesystem::exists(namedCollectionsFile_)) return result;

    std::ifstream legacyInput(collectionsFile_);
    std::string path;
    while (legacyInput >> std::quoted(path)) {
        const auto filesystemPath = std::filesystem::u8path(path);
        auto name = filesystemPath.filename().u8string();
        if (name.empty()) name = filesystemPath.parent_path().filename().u8string();
        result.push_back({name.empty() ? path : name, path, "audiobook"});
    }
    return result;
}
bool FileProgressStore::saveCollections(const std::vector<MediaCollection>& collections) {
    std::error_code error;
    std::filesystem::create_directories(stateDirectory_, error);
    if (error) return false;
    const std::filesystem::path temporaryFile = namedCollectionsFile_.string() + ".tmp";
    {
        std::ofstream output(temporaryFile, std::ios::trunc);
        for (const auto& collection : collections) {
            output << std::quoted(collection.name) << ' ' << std::quoted(collection.path) << ' '
                << std::quoted(collection.type) << '\n';
        }
        output.flush();
        if (!output) return false;
    }
    std::filesystem::rename(temporaryFile, namedCollectionsFile_, error);
    if (error) {
        std::filesystem::remove(namedCollectionsFile_, error);
        error.clear();
        std::filesystem::rename(temporaryFile, namedCollectionsFile_, error);
    }
    return !error;
}
std::vector<double> FileProgressStore::bookmarks(const std::string& mediaId) {
    std::vector<double> result;std::ifstream input(bookmarksFile_);std::string id;double seconds=0;while(input>>std::quoted(id)>>seconds)if(id==mediaId)result.push_back(seconds);return result;
}
bool FileProgressStore::addBookmark(const std::string& mediaId,double positionSeconds) {
    std::error_code error;std::filesystem::create_directories(stateDirectory_,error);std::ofstream output(bookmarksFile_,std::ios::app);output<<std::quoted(mediaId)<<' '<<std::setprecision(17)<<positionSeconds<<'\n';return static_cast<bool>(output);
}

std::string FileProgressStore::language() {
    std::ifstream input(languageFile_);
    std::string code;
    return (input >> code && code == "en") ? "en" : "de";
}

bool FileProgressStore::saveLanguage(const std::string& code) {
    if (code != "de" && code != "en") return false;
    std::error_code error;
    std::filesystem::create_directories(stateDirectory_, error);
    if (error) return false;
    const auto temporaryFile = languageFile_.string() + ".tmp";
    {
        std::ofstream output(temporaryFile, std::ios::trunc);
        output << code << '\n';
        if (!output) return false;
    }
    std::filesystem::rename(temporaryFile, languageFile_, error);
    if (error) {
        std::filesystem::remove(languageFile_, error);
        error.clear();
        std::filesystem::rename(temporaryFile, languageFile_, error);
    }
    return !error;
}

bool FileProgressStore::loadFile() {
    loaded_ = true;
    if (progressFile_.empty() || !std::filesystem::exists(progressFile_)) {
        return error_.empty();
    }
    std::ifstream input(progressFile_);
    std::string header;
    if (!std::getline(input, header) || (header != "COVERPLAYER_PROGRESS 1" && header != "COVERPLAYER_PROGRESS 2" && header != "COVERPLAYER_PROGRESS 3")) {
        error_ = "Unsupported or invalid progress file";
        return false;
    }
    std::string mediaId;
    TrackProgress progress{};
    if (header == "COVERPLAYER_PROGRESS 2" || header == "COVERPLAYER_PROGRESS 3") {
        std::string marker;
        if (!(input >> marker >> std::quoted(lastMediaId_)) || marker != "LAST") {
            error_ = "Invalid last-media record";
            return false;
        }
        if(header=="COVERPLAYER_PROGRESS 3"&&(!(input>>marker>>std::quoted(mediaRoot_))||marker!="ROOT")){error_="Invalid media-root record";return false;}
    }
    while (input >> std::quoted(mediaId) >> progress.positionSeconds >> progress.completed) {
        entries_[mediaId] = progress;
    }
    if (!input.eof()) {
        error_ = "Invalid progress record";
        return false;
    }
    return true;
}

bool FileProgressStore::writeFile() {
    if (progressFile_.empty()) {
        return false;
    }
    std::error_code filesystemError;
    std::filesystem::create_directories(stateDirectory_, filesystemError);
    if (filesystemError) {
        error_ = "Cannot create state directory: " + filesystemError.message();
        return false;
    }
    const std::filesystem::path temporaryFile = progressFile_.string() + ".tmp";
    {
        std::ofstream output(temporaryFile, std::ios::trunc);
        if (!output) {
            error_ = "Cannot create temporary progress file";
            return false;
        }
        output << "COVERPLAYER_PROGRESS 3\nLAST " << std::quoted(lastMediaId_) << "\nROOT " << std::quoted(mediaRoot_) << '\n' << std::setprecision(17);
        for (const auto& [mediaId, progress] : entries_) {
            output << std::quoted(mediaId) << ' ' << progress.positionSeconds << ' '
                   << progress.completed << '\n';
        }
        output.flush();
        if (!output) {
            error_ = "Cannot write progress file";
            return false;
        }
    }
    std::filesystem::rename(temporaryFile, progressFile_, filesystemError);
    if (filesystemError) {
        std::filesystem::remove(progressFile_, filesystemError);
        filesystemError.clear();
        std::filesystem::rename(temporaryFile, progressFile_, filesystemError);
    }
    if (filesystemError) {
        error_ = "Cannot replace progress file: " + filesystemError.message();
        return false;
    }
    error_.clear();
    return true;
}

const std::string& FileProgressStore::error() const noexcept { return error_; }

} // namespace coverplayer::persistence
