#include "coverplayer/app/application.hpp"
#include "coverplayer/audio/audio_player.hpp"
#include "coverplayer/persistence/progress_store.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <thread>
#include <utility>
#include <cctype>

namespace coverplayer::app {
namespace {
std::string folderName(const std::string& path) {
    const auto end = path.find_last_not_of("/\\");
    if (end == std::string::npos) return path;
    const auto slash = path.find_last_of("/\\", end);
    const auto start = slash == std::string::npos ? 0 : slash + 1;
    return path.substr(start, end - start + 1);
}
std::string trackTitle(std::size_t index, std::size_t count, const std::string& name) {
    return std::to_string(index + 1) + "/" + std::to_string(count) + "  " + name;
}
const std::string& keyboardCharacters() {
    static const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_&.";
    return characters;
}
const std::vector<std::pair<std::string,std::string>>& collectionTypes(){static const std::vector<std::pair<std::string,std::string>> values={{"audiobook","HOERBUCH"},{"radioplay","HOERSPIEL"},{"music","MUSIK"},{"podcast","PODCAST"},{"general","ALLGEMEIN"}};return values;}
std::string typeLabel(const std::string& type, Language language){for(const auto& value:collectionTypes())if(value.first==type)return tr(language,value.second.c_str());return tr(language,"ALLGEMEIN");}
bool isCoverName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return name == "cover.jpg" || name == "cover.jpeg" || name == "cover.png" ||
        name == "folder.jpg" || name == "folder.png";
}
std::string trimmed(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}
void eraseLastCharacter(std::string& value) {
    if (value.empty()) return;
    value.pop_back();
    while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0U) == 0x80U) value.pop_back();
}
// Audiobooks/radio plays are meant to be listened to start-to-finish once,
// so a real elapsed-time percentage is meaningful for them - including the
// common "one 70-minute file" layout, where a per-file completed/not
// counter would sit at "0/1" for the entire 70 minutes. Music/podcasts/
// general collections are browsed non-linearly, so that percentage (and
// the plain completed-file counter it replaces) is not shown for them.
bool isTimeTrackedType(const std::string& type) { return type == "audiobook" || type == "radioplay"; }

struct MediaProgress {
    std::size_t totalTracks = 0;
    std::size_t completedTracks = 0;
    double totalSeconds = 0.0;
    double playedSeconds = 0.0;
};

MediaProgress computeProgress(const library::Collection& node, persistence::ProgressStore* store) {
    MediaProgress result;
    for (const auto& track : node.tracks) {
        ++result.totalTracks;
        result.totalSeconds += track.durationSeconds;
        const auto progress = store != nullptr ? store->load(track.path) : std::nullopt;
        if (!progress) continue;
        if (progress->completed) {
            ++result.completedTracks;
            result.playedSeconds += track.durationSeconds;
        } else {
            result.playedSeconds += std::min(progress->positionSeconds, track.durationSeconds);
        }
    }
    for (const auto& child : node.children) {
        const auto childProgress = computeProgress(child, store);
        result.totalTracks += childProgress.totalTracks;
        result.completedTracks += childProgress.completedTracks;
        result.totalSeconds += childProgress.totalSeconds;
        result.playedSeconds += childProgress.playedSeconds;
    }
    return result;
}

MediaProgress computeProgress(const std::vector<library::Collection>& nodes, persistence::ProgressStore* store) {
    MediaProgress result;
    for (const auto& node : nodes) {
        const auto nodeProgress = computeProgress(node, store);
        result.totalTracks += nodeProgress.totalTracks;
        result.completedTracks += nodeProgress.completedTracks;
        result.totalSeconds += nodeProgress.totalSeconds;
        result.playedSeconds += nodeProgress.playedSeconds;
    }
    return result;
}

// Empty for types where progress isn't shown at all; a percentage of
// elapsed time for audiobook/radioplay; falls back to the old completed-
// file counter only if durations are not (yet) known, e.g. a library
// cached before this feature existed and not yet rescanned.
std::string progressLabel(const std::string& collectionType, const MediaProgress& progress, Language language) {
    if (!isTimeTrackedType(collectionType)) return {};
    if (progress.totalSeconds <= 0.0) {
        return std::to_string(progress.completedTracks) + "/" + std::to_string(progress.totalTracks) + " " + tr(language,"FERTIG");
    }
    const auto percent = static_cast<int>(std::lround(100.0 * std::min(1.0, progress.playedSeconds / progress.totalSeconds)));
    return std::to_string(percent) + "% " + tr(language,"GEHOERT");
}
}

Application::Application(platform::Platform& platform, audio::AudioPlayer* audioPlayer,
    persistence::ProgressStore* progressStore, library::LibraryCache& libraryCache,
    platform::FileSystem& fileSystem, std::string mediaRoot, std::string initialMediaPath,
    std::string browseRoot)
    : platform_(platform), audioPlayer_(audioPlayer), progressStore_(progressStore),
      libraryCache_(libraryCache), fileSystem_(fileSystem), mediaRoot_(std::move(mediaRoot)),
      initialMediaPath_(std::move(initialMediaPath)), browseRoot_(std::move(browseRoot)),
      lastProgressSave_(std::chrono::steady_clock::now()) {
    if(!browseRoot_.empty())browseRoot_=fileSystem_.normalizedPath(browseRoot_);
}

int Application::run() {
    if (progressStore_ != nullptr) {
        language_ = languageFromCode(progressStore_->language());
        const auto savedRoot = progressStore_->mediaRoot();
        if (!savedRoot.empty()) mediaRoot_ = savedRoot;
        collections_ = progressStore_->collections();
    }
    platform_.setLanguage(language_);
    if(browseRoot_.empty())browseRoot_=fileSystem_.normalizedPath(fileSystem_.parent(mediaRoot_));
    if (collections_.empty()) collections_.push_back({folderName(mediaRoot_), mediaRoot_, "audiobook"});
    scanLibrary();
    if (!initialMediaPath_.empty()) { openMedia(initialMediaPath_); screen_ = platform::Screen::Player; }
    else if (sources_.size() == 1 && sources_.front().available && !sources_.front().albums.empty()) screen_ = albumBrowserScreen_;
    publishCurrentView();

    bool quitRequested = false;
    while (!quitRequested) {
        const auto actions = platform_.pollEvents();
        if (actions.toggleLanguage) {
            language_ = language_ == Language::German ? Language::English : Language::German;
            if (progressStore_ != nullptr) progressStore_->saveLanguage(languageCode(language_));
            platform_.setLanguage(language_);
            playerNotice_.clear();
            editor_.message.clear();
            bluetooth_.message.clear();
        }
        quitRequested = actions.quit;
        if (!quitRequested && actions.background) { handleBackgroundRequest(); quitRequested = true; }

        // Each call below is a straight, order-preserving extraction of what
        // used to be one 170-line if/else-if ladder. The order matters:
        // several of these mutate screen_ mid-frame, and later calls must
        // see that updated screen_, exactly like the original sequential
        // statements did.
        handleBluetoothOpenRequest(actions);
        handleCollectionManagementInput(actions);
        handleBrowserViewToggle(actions);
        handleBluetoothScreenShortcuts(actions);
        handleTransportShortcuts(actions);
        handlePlayerAndResumeShortcuts(actions);
        handleRescanRequest(actions);
        handleCollectionNameEditing(actions);
        handleNavigation(actions);
        handleAcceptAndBack(actions);
        updatePlayback(actions);
        updateSleepTimerDisplay();
        publishCurrentView();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    saveProgress();
    return 0;
}

void Application::handleBackgroundRequest() {
    // Nothing to keep playing in the background - this becomes a plain quit,
    // same as the gesture always did before backgrounding existed. The
    // background helper always starts playing immediately (it has no way to
    // represent "open but paused"), so backgrounding a paused track would
    // otherwise resume it as an unreachable side effect of a gesture that
    // wasn't a play command.
    if (audioPlayer_ == nullptr || !audioPlayer_->isOpen() || audioPlayer_->isPaused()) return;
    saveProgress();
    // Hand off the whole album in playback order, not just the one track
    // that happens to be open, so the helper can advance to the next track
    // itself once this one finishes instead of exiting - a backgrounded
    // audiobook or radio play must not stop dead at the end of one chapter.
    // Falls back to a single-track handoff if the current track cannot be
    // matched back into an album's track list.
    const auto* album = selectedAlbum();
    if (album != nullptr && trackIndex_ < album->tracks.size() && album->tracks[trackIndex_].path == currentMediaId_) {
        std::vector<std::string> trackPaths;
        trackPaths.reserve(album->tracks.size());
        for (const auto& track : album->tracks) trackPaths.push_back(track.path);
        platform_.enterBackgroundPlayback(trackPaths, trackIndex_);
    } else {
        platform_.enterBackgroundPlayback({currentMediaId_}, 0);
    }
    // This process always quits normally right after this call regardless
    // of whether spawning the background helper succeeded; on success, an
    // independent process picks up playback from the position
    // saveProgress() just persisted.
}

void Application::handleBluetoothOpenRequest(const platform::InputActions& actions) {
    if (actions.openBluetooth && platform_.capabilities().bluetoothManagement &&
        screen_ != platform::Screen::Bluetooth) openBluetooth();
}

void Application::handleCollectionManagementInput(const platform::InputActions& actions) {
    if (actions.openFolders && screen_ == platform::Screen::Collections) {
        collectionManagerIndex_ = 0;
        editor_.message.clear();
        screen_ = platform::Screen::CollectionManager;
    } else if(actions.openFolders && screen_ == platform::Screen::CollectionManager) {
        if(collectionManagerIndex_==0)beginCollectionFolderSelection();else{editor_.editedIndex=collectionManagerIndex_-1;editor_.editingPath=true;folderBrowser_.path=collections_[editor_.editedIndex].path;if(!fileSystem_.isPathWithin(browseRoot_,folderBrowser_.path))folderBrowser_.path=browseRoot_;scanFolders();screen_=platform::Screen::Folders;}
    }
    if(actions.reorder!=0&&screen_==platform::Screen::CollectionManager)reorderSelectedCollection(actions.reorder);
    if(actions.deleteItem&&screen_==platform::Screen::CollectionManager&&collectionManagerIndex_>0)screen_=platform::Screen::CollectionDelete;
}

void Application::handleBrowserViewToggle(const platform::InputActions& actions) {
    if(actions.toggleLibraryView&&(screen_==platform::Screen::CoverFlow||screen_==platform::Screen::AlbumList)){
        screen_=screen_==platform::Screen::CoverFlow?platform::Screen::AlbumList:platform::Screen::CoverFlow;
        albumBrowserScreen_=screen_;
    }
}

void Application::handleBluetoothScreenShortcuts(const platform::InputActions& actions) {
    if(actions.refreshBluetooth&&screen_==platform::Screen::Bluetooth)refreshBluetooth(t("Bluetooth-Status aktualisiert"));
    if(actions.toggleBluetooth&&screen_==platform::Screen::Bluetooth){const bool enable=!bluetooth_.state.powered;const bool success=platform_.setBluetoothEnabled(enable);refreshBluetooth(t(success?(enable?"Bluetooth eingeschaltet":"Bluetooth ausgeschaltet"):"Bluetooth konnte nicht geschaltet werden"));}
}

void Application::handleTransportShortcuts(const platform::InputActions& actions) {
    if (actions.cycleSleepTimer) cycleSleepTimer();
    if (actions.volumeDelta != 0 && audioPlayer_ != nullptr) {
        const auto systemVolume = platform_.adjustSystemVolume(actions.volumeDelta);
        if (!systemVolume) {
            audioPlayer_->setVolumePercent(audioPlayer_->volumePercent() + actions.volumeDelta);
        }
    }
    if (actions.addBookmark && audioPlayer_ != nullptr && audioPlayer_->isOpen() && progressStore_ != nullptr) {
        progressStore_->addBookmark(currentMediaId_, audioPlayer_->positionSeconds());
        playerNotice_ = t("LESEZEICHEN GESPEICHERT");
    }
    if (actions.jumpBookmark && audioPlayer_ != nullptr && audioPlayer_->isOpen() && progressStore_ != nullptr) {
        const auto marks = progressStore_->bookmarks(currentMediaId_);
        if (!marks.empty()) {
            double target = marks.front();
            for (double mark : marks) if (mark > audioPlayer_->positionSeconds() + 1.0) { target = mark; break; }
            audioPlayer_->seekSeconds(target - audioPlayer_->positionSeconds());
            const int seconds = static_cast<int>(target);
            playerNotice_ = std::string(t("LESEZEICHEN")) + " " + std::to_string(seconds / 60) + ":" +
                (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
        }
    }
    if (actions.suspend && audioPlayer_ != nullptr) {
        saveProgress(); resumeAfterSuspend_ = audioPlayer_->isOpen() && !audioPlayer_->isPaused();
        if (resumeAfterSuspend_) audioPlayer_->togglePause();
    }
    if (actions.resume && resumeAfterSuspend_ && audioPlayer_ != nullptr && audioPlayer_->isPaused()) {
        audioPlayer_->togglePause(); resumeAfterSuspend_ = false;
    }
}

void Application::handlePlayerAndResumeShortcuts(const platform::InputActions& actions) {
    if (screen_ == platform::Screen::Player && actions.changeTrack != 0) changeTrack(actions.changeTrack);
    if (actions.resumeLast && screen_ != platform::Screen::Player && selectedAlbum() != nullptr && !selectedAlbum()->tracks.empty()) {
        openSelectedTrack(); screen_ = platform::Screen::Player;
    }
}

void Application::handleRescanRequest(const platform::InputActions& actions) {
    if (actions.rescan && screen_ == platform::Screen::Folders) {
        if(!fileSystem_.directoryExists(folderBrowser_.path)||!fileSystem_.isPathWithin(browseRoot_,folderBrowser_.path))editor_.message=t("Ordner ist nicht erreichbar");
        else {editor_.pendingPath=folderBrowser_.path;editor_.message.clear();
            if(editor_.editingPath&&editor_.editedIndex<collections_.size()){editor_.pendingName=collections_[editor_.editedIndex].name;editor_.pendingType=collections_[editor_.editedIndex].type;keyboardIndex_=0;screen_=platform::Screen::CollectionName;}
            else{collectionTypeIndex_=0;screen_=platform::Screen::CollectionType;}}
    } else if (actions.rescan) {
        for (const auto& collection : collections_) libraryCache_.invalidate(collection.path);
        scanLibrary();
    }
}

void Application::handleCollectionNameEditing(const platform::InputActions& actions) {
    if (screen_ == platform::Screen::CollectionName) {
        if (!actions.textInput.empty() && editor_.pendingName.size() < 40) {
            editor_.pendingName += actions.textInput.substr(0, 40 - editor_.pendingName.size());
        }
        if (actions.eraseText) eraseLastCharacter(editor_.pendingName);
        if (actions.clearText) editor_.pendingName.clear();
        if (actions.accept && editor_.pendingName.size() < 40) {
            editor_.pendingName += keyboardCharacters()[keyboardIndex_];
        }
        if (actions.saveText) savePendingCollection();
    }
}

void Application::handleNavigation(const platform::InputActions& actions) {
    if (screen_ != platform::Screen::Player && actions.navigate != 0) moveSelection(actions.navigate);
}

void Application::handleAcceptAndBack(const platform::InputActions& actions) {
    // Only one screen_ value is active per frame, so dispatching here cannot
    // reorder anything relative to the original single if/else-if chain -
    // exactly one of these ever ran per frame there too.
    switch (screen_) {
        case platform::Screen::Collections: handleCollectionsAcceptOrBack(actions); break;
        case platform::Screen::CollectionManager: handleCollectionManagerAcceptOrBack(actions); break;
        case platform::Screen::CollectionType: handleCollectionTypeAcceptOrBack(actions); break;
        case platform::Screen::CollectionDelete: handleCollectionDeleteAcceptOrBack(actions); break;
        case platform::Screen::Bluetooth: handleBluetoothAcceptOrBack(actions); break;
        case platform::Screen::CoverFlow:
        case platform::Screen::AlbumList: handleBrowserAcceptOrBack(actions); break;
        case platform::Screen::Tracks: handleTracksAcceptOrBack(actions); break;
        case platform::Screen::Player: handlePlayerBack(actions); break;
        case platform::Screen::Folders: handleFoldersAcceptOrBack(actions); break;
        case platform::Screen::CollectionName: handleCollectionNameBack(actions); break;
    }
}

void Application::handleCollectionsAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept && !sources_.empty()) {
        screen_ = albumBrowserScreen_;
    } else if (actions.back) {
        // The root screen deliberately absorbs B. Exiting requires the
        // explicit controller gesture handled by the platform adapter.
    }
}

void Application::handleCollectionManagerAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept) {
        if (collectionManagerIndex_ == 0) beginCollectionFolderSelection();
        else {editor_.editedIndex=collectionManagerIndex_-1;editor_.pendingName=collections_[editor_.editedIndex].name;editor_.pendingPath=collections_[editor_.editedIndex].path;editor_.pendingType=collections_[editor_.editedIndex].type;collectionTypeIndex_=0;for(std::size_t index=0;index<collectionTypes().size();++index)if(collectionTypes()[index].first==editor_.pendingType)collectionTypeIndex_=index;screen_=platform::Screen::CollectionType;}
    } else if (actions.back) screen_ = platform::Screen::Collections;
}

void Application::handleCollectionTypeAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept) { editor_.pendingType=collectionTypes()[collectionTypeIndex_].first; beginCollectionName(editor_.editedIndex); }
    else if (actions.back) screen_ = editor_.editedIndex<collections_.size()?platform::Screen::CollectionManager:platform::Screen::Folders;
}

void Application::handleCollectionDeleteAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept) deleteSelectedCollection();
    else if (actions.back) screen_ = platform::Screen::CollectionManager;
}

void Application::handleBluetoothAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept) {
        if(bluetooth_.index<bluetooth_.state.devices.size()){
            const auto device=bluetooth_.state.devices[bluetooth_.index];const bool connect=!device.connected;
            const bool resumePlayback=audioPlayer_!=nullptr&&audioPlayer_->isOpen()&&!audioPlayer_->isPaused();
            if(resumePlayback)audioPlayer_->togglePause();
            const bool success=platform_.setBluetoothDeviceConnected(device.address,connect);
            if(success&&resumePlayback)audioPlayer_->togglePause();
            refreshBluetooth(t(success?(connect?"Verbunden - Lautstaerke synchronisiert":"Getrennt - Lautstaerke synchronisiert"):"Audio bleibt pausiert: Lautstaerke nicht sicher synchronisiert"));
        }
    } else if (actions.back) screen_ = bluetooth_.returnScreen;
}

void Application::handleBrowserAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept && selectedAlbum() != nullptr) {
        openSelectedNode();
    } else if (actions.back) {
        if(navigationPath_.empty())screen_=platform::Screen::Collections;
        else {albumIndex_=navigationPath_.back();navigationPath_.pop_back();}
    }
}

void Application::handleTracksAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept && selectedAlbum() != nullptr) { openSelectedTrack(); screen_ = platform::Screen::Player; }
    else if (actions.back) {
        saveProgress();
        if (audioPlayer_ != nullptr && audioPlayer_->isOpen() && !audioPlayer_->isPaused()) audioPlayer_->togglePause();
        screen_ = albumBrowserScreen_;
    }
}

void Application::handlePlayerBack(const platform::InputActions& actions) {
    if (actions.back) {
        saveProgress();
        const auto* album = selectedAlbum();
        if (album != nullptr && album->tracks.size() == 1 && audioPlayer_ != nullptr &&
            audioPlayer_->isOpen() && !audioPlayer_->isPaused()) audioPlayer_->togglePause();
        screen_ = album != nullptr && album->tracks.size() == 1 ? albumBrowserScreen_ : platform::Screen::Tracks;
    }
}

void Application::handleFoldersAcceptOrBack(const platform::InputActions& actions) {
    if (actions.accept && !folderBrowser_.entries.empty()) { folderBrowser_.path = folderBrowser_.entries[folderBrowser_.index].path; scanFolders(); }
    else if (actions.back) screen_ = platform::Screen::CollectionManager;
}

void Application::handleCollectionNameBack(const platform::InputActions& actions) {
    if (actions.back) screen_ = editor_.editedIndex<collections_.size()?platform::Screen::CollectionManager:platform::Screen::CollectionType;
}

void Application::updatePlayback(const platform::InputActions& actions) {
    if (audioPlayer_ == nullptr) return;
    if (!actions.openMediaPath.empty()) { saveProgress(); openMedia(actions.openMediaPath); screen_ = platform::Screen::Player; }
    if (actions.togglePause && screen_ == platform::Screen::Player) { audioPlayer_->togglePause(); saveProgress(); }
    if (actions.seekSeconds != 0.0 && screen_ == platform::Screen::Player) { audioPlayer_->seekSeconds(actions.seekSeconds); saveProgress(); }
    audioPlayer_->update();
    if (!audioPlayer_->isFinished()) trackEndHandled_ = false;
    if (sleepMinutes_ > 0 && std::chrono::steady_clock::now() >= sleepDeadline_) {
        if (audioPlayer_->isOpen() && !audioPlayer_->isPaused()) audioPlayer_->togglePause();
        saveProgress(); sleepMinutes_ = 0; platform_.setSleepTimer(0);
    }
    const auto* album = selectedAlbum();
    if (audioPlayer_->isFinished() && !trackEndHandled_ && (screen_ == platform::Screen::Player || screen_ == platform::Screen::Tracks) && album != nullptr) {
        const auto playing = std::find_if(album->tracks.begin(), album->tracks.end(),
            [&](const auto& track) { return track.path == currentMediaId_; });
        if (playing != album->tracks.end()) {
            trackEndHandled_ = true;
            saveProgress();
            const auto playingIndex = static_cast<std::size_t>(playing - album->tracks.begin());
            if (playingIndex + 1 < album->tracks.size()) {
                const auto selection = trackIndex_;
                trackIndex_ = playingIndex + 1;
                openSelectedTrack();
                if (screen_ == platform::Screen::Tracks && selection != playingIndex) trackIndex_ = selection;
            } else if (screen_ == platform::Screen::Player) {
                screen_ = album->tracks.size() == 1 ? albumBrowserScreen_ : platform::Screen::Tracks;
            }
        }
    }
    platform_.setPlaybackStatus(audioPlayer_->isOpen() && !audioPlayer_->isFinished(),
        audioPlayer_->isPaused(), audioPlayer_->positionSeconds(), audioPlayer_->durationSeconds());
    const auto systemVolume = platform_.systemVolumePercent();
    platform_.setPlayerDetails(systemVolume.value_or(audioPlayer_->volumePercent()),
        progressStore_ != nullptr ? progressStore_->bookmarks(currentMediaId_).size() : 0, playerNotice_,
        platform_.batteryPercent());
    if (std::chrono::steady_clock::now() - lastProgressSave_ >= std::chrono::seconds(5)) saveProgress();
}

void Application::updateSleepTimerDisplay() {
    if (sleepMinutes_ > 0) {
        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(sleepDeadline_ - std::chrono::steady_clock::now()).count();
        platform_.setSleepTimer(static_cast<int>((remaining + 59) / 60));
    }
}

void Application::publishCurrentView() {
    switch (screen_) {
        case platform::Screen::Folders: publishFolders(); break;
        case platform::Screen::CollectionManager: publishCollectionManager(); break;
        case platform::Screen::CollectionName: publishCollectionName(); break;
        case platform::Screen::CollectionType: publishCollectionType(); break;
        case platform::Screen::CollectionDelete: publishCollectionDelete(); break;
        case platform::Screen::Bluetooth: publishBluetooth(); break;
        case platform::Screen::Collections: publishCollections(); break;
        case platform::Screen::CoverFlow:
        case platform::Screen::AlbumList: publishBrowser(); break;
        case platform::Screen::Tracks: publishTracks(); break;
        case platform::Screen::Player: publishPlayer(); break;
    }
}

void Application::scanLibrary() {
    sources_.clear();
    for (const auto& collection : collections_) {
        const bool available=fileSystem_.directoryExists(collection.path);
        if(!available){sources_.push_back({collection.name,collection.path,{},collection.type,false,{}});continue;}
        platform::ViewModel progressView;progressView.screen=platform::Screen::Collections;progressView.title=t("BIBLIOTHEK WIRD EINGELESEN");progressView.message=collection.name;progressView.immediate=true;platform_.setView(progressView);
        const auto fingerprint=fileSystem_.fingerprint(collection.path);
        auto cached = libraryCache_.load(collection.path,fingerprint);
        auto albums = cached ? *cached : library::LibraryScanner(fileSystem_).scan(collection.path,[&](std::size_t visited,const std::string& current){if(visited==1||visited%16==0){progressView.message=collection.name+"  "+std::to_string(visited)+" "+t("ORDNER")+"  "+folderName(current);platform_.setView(progressView);}});
        if (!cached) libraryCache_.save(collection.path,fingerprint,albums);
        const auto coverPath = findCollectionCover(collection.path, albums);
        sources_.push_back({collection.name, collection.path, coverPath,collection.type,true,std::move(albums)});
    }
    if (sourceIndex_ >= sources_.size()) sourceIndex_ = 0;
    navigationPath_.clear();albumIndex_ = 0; trackIndex_ = 0; restoreLastSelection();
}

void Application::restoreLastSelection() {
    if (progressStore_ == nullptr) return;
    const auto last = progressStore_->lastMediaId();
    std::function<bool(const std::vector<library::Collection>&,std::vector<std::size_t>)> find;
    find=[&](const auto& nodes,std::vector<std::size_t> parents){for(std::size_t nodeIndex=0;nodeIndex<nodes.size();++nodeIndex){const auto& node=nodes[nodeIndex];for(std::size_t track=0;track<node.tracks.size();++track)if(node.tracks[track].path==last){navigationPath_=std::move(parents);albumIndex_=nodeIndex;trackIndex_=track;return true;}auto childParents=parents;childParents.push_back(nodeIndex);if(find(node.children,std::move(childParents)))return true;}return false;};
    for (std::size_t source = 0; source < sources_.size(); ++source) if(find(sources_[source].albums,{})){sourceIndex_=source;return;}
}

void Application::selectResumeTrack() {
    auto* album = selectedAlbum();
    if (album == nullptr || album->tracks.empty()) { trackIndex_ = 0; return; }
    std::size_t firstIncomplete = 0;
    bool foundIncomplete = false;
    for (std::size_t index = 0; index < album->tracks.size(); ++index) {
        const auto progress = progressStore_ != nullptr ? progressStore_->load(album->tracks[index].path) : std::nullopt;
        if (progress && !progress->completed && progress->positionSeconds > 0.0) {
            trackIndex_ = index;
            return;
        }
        if (!foundIncomplete && (!progress || !progress->completed)) {
            firstIncomplete = index;
            foundIncomplete = true;
        }
    }
    trackIndex_ = foundIncomplete ? firstIncomplete : 0;
}

library::Collection* Application::selectedAlbum() {
    auto* nodes=currentNodes();
    if(nodes==nullptr||nodes->empty())return nullptr;
    albumIndex_=std::min(albumIndex_,nodes->size()-1);
    return &(*nodes)[albumIndex_];
}
const library::Collection* Application::selectedAlbum() const {
    const auto* nodes=currentNodes();
    if(nodes==nullptr||albumIndex_>=nodes->size())return nullptr;
    return &(*nodes)[albumIndex_];
}
std::vector<library::Collection>* Application::currentNodes(){if(sources_.empty()||sourceIndex_>=sources_.size())return nullptr;auto* nodes=&sources_[sourceIndex_].albums;for(const auto index:navigationPath_){if(index>=nodes->size())return nullptr;nodes=&(*nodes)[index].children;}return nodes;}
const std::vector<library::Collection>* Application::currentNodes()const{if(sources_.empty()||sourceIndex_>=sources_.size())return nullptr;const auto* nodes=&sources_[sourceIndex_].albums;for(const auto index:navigationPath_){if(index>=nodes->size())return nullptr;nodes=&(*nodes)[index].children;}return nodes;}
std::string Application::currentContainerName()const{if(sources_.empty()||sourceIndex_>=sources_.size())return t("SAMMLUNG");const auto* nodes=&sources_[sourceIndex_].albums;const library::Collection* container=nullptr;for(const auto index:navigationPath_){if(index>=nodes->size())break;container=&(*nodes)[index];nodes=&container->children;}return container==nullptr?sources_[sourceIndex_].name:container->name;}
void Application::openSelectedNode(){auto* node=selectedAlbum();if(node==nullptr)return;if(!node->children.empty()){navigationPath_.push_back(albumIndex_);albumIndex_=0;trackIndex_=0;return;}if(node->tracks.empty())return;selectResumeTrack();if(node->tracks.size()==1){openSelectedTrack();screen_=platform::Screen::Player;}else screen_=platform::Screen::Tracks;}
void Application::openSelectedTrack() {
    auto* album = selectedAlbum();
    if (album == nullptr || album->tracks.empty()) return;
    trackIndex_ = std::min(trackIndex_, album->tracks.size() - 1);
    saveProgress();
    currentTrackName_ = trackTitle(trackIndex_, album->tracks.size(), album->tracks[trackIndex_].name);
    if (audioPlayer_ != nullptr && audioPlayer_->isOpen() && !audioPlayer_->isFinished() &&
        currentMediaId_ == album->tracks[trackIndex_].path) {
        if (audioPlayer_->isPaused()) audioPlayer_->togglePause();
        return;
    }
    openMedia(album->tracks[trackIndex_].path);
}

void Application::moveSelection(int delta) {
    std::size_t count = 0; std::size_t* index = nullptr;
    if (screen_ == platform::Screen::Folders) { count = folderBrowser_.entries.size(); index = &folderBrowser_.index; }
    else if (screen_ == platform::Screen::CollectionManager) { count = collections_.size() + 1; index = &collectionManagerIndex_; }
    else if (screen_ == platform::Screen::CollectionName) { count = keyboardCharacters().size(); index = &keyboardIndex_; }
    else if (screen_ == platform::Screen::CollectionType) { count = collectionTypes().size(); index = &collectionTypeIndex_; }
    else if (screen_ == platform::Screen::Collections) { count = sources_.size(); index = &sourceIndex_; }
    else if (screen_ == platform::Screen::CoverFlow||screen_==platform::Screen::AlbumList) { const auto* nodes=currentNodes();count=nodes==nullptr?0:nodes->size();index=&albumIndex_; }
    else if (screen_ == platform::Screen::Bluetooth) { count = bluetooth_.state.devices.size(); index = &bluetooth_.index; }
    else if (screen_ == platform::Screen::Tracks) { const auto* album = selectedAlbum(); count = album == nullptr ? 0 : album->tracks.size(); index = &trackIndex_; }
    if (count == 0 || index == nullptr) return;
    if (screen_ == platform::Screen::CoverFlow) {
        const auto next = static_cast<long long>(*index) + delta;
        *index = static_cast<std::size_t>(std::clamp(next, 0LL, static_cast<long long>(count - 1)));
    } else {
        const auto n = static_cast<long long>(count);
        *index = static_cast<std::size_t>(((static_cast<long long>(*index) + delta) % n + n) % n);
    }
    if (screen_ == platform::Screen::Collections) { navigationPath_.clear();albumIndex_ = 0; trackIndex_ = 0; }
    else if (screen_ == platform::Screen::CoverFlow||screen_==platform::Screen::AlbumList) trackIndex_ = 0;
}

void Application::publishCollections() {
    platform::ViewModel view; view.screen = screen_;
    view.title = t("SAMMLUNGEN"); view.selected = sourceIndex_;
    for (const auto& source : sources_) {
        const auto progress = computeProgress(source.albums, progressStore_);
        const auto label = progressLabel(source.type, progress, language_);
        view.items.push_back((source.available?std::string{}:std::string(t("[FEHLT]"))+"  ")+source.name+"|"+typeLabel(source.type,language_)+"  "+std::to_string(source.albums.size())+" "+t("MEDIEN")+(label.empty()?"":"  "+label));
        view.itemImages.push_back(source.coverPath);
    }
    if (!sources_.empty()) view.coverPath = sources_[sourceIndex_].coverPath;
    if (view.items.empty()) view.message = t("Noch keine Sammlung konfiguriert");
    platform_.setView(std::move(view));
}

void Application::publishBrowser() {
    platform::ViewModel view; view.screen = screen_;
    view.title = currentContainerName(); view.selected = albumIndex_;
    const auto* nodes=currentNodes();
    const auto collectionType = sourceIndex_ < sources_.size() ? sources_[sourceIndex_].type : std::string{};
    if (nodes!=nullptr) for (const auto& node : *nodes) {
        const auto progress=computeProgress(node,progressStore_);
        const auto trackWord=language_==Language::English&&progress.totalTracks==1?"TRACK":t("TITEL");
        const auto kind=node.children.empty()?std::to_string(progress.totalTracks)+" "+trackWord:std::to_string(node.children.size())+" "+t("EINTRAEGE")+" / "+std::to_string(progress.totalTracks)+" "+trackWord;
        const auto label=progressLabel(collectionType,progress,language_);
        view.items.push_back(node.name + "|" + kind + (label.empty()?"":"  "+label));
        view.itemImages.push_back(node.coverPath);
    }
    if(screen_==platform::Screen::AlbumList&&!view.itemImages.empty()&&view.selected<view.itemImages.size())view.coverPath=view.itemImages[view.selected];
    if (view.items.empty()) view.message = t("Keine Medien in dieser Sammlung");
    platform_.setView(std::move(view));
}

void Application::publishTracks() {
    platform::ViewModel view; view.screen = screen_;
    const auto* album = selectedAlbum();
    if (album != nullptr) {
        view.title = album->name; view.coverPath = album->coverPath; view.selected = trackIndex_;
        for (const auto& track : album->tracks) {
            std::string label = track.name;
            if(!track.artist.empty())label+="  -  "+track.artist;
            const auto progress = progressStore_ != nullptr ? progressStore_->load(track.path) : std::nullopt;
            if (progress) {
                if (progress->completed) label += std::string("  ") + t("[FERTIG]");
                else if (progress->positionSeconds > 0) { const int seconds = static_cast<int>(progress->positionSeconds); label += "  [" + std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60) + "]"; }
            }
            view.items.push_back(std::move(label));
        }
    }
    platform_.setView(std::move(view));
}

void Application::publishPlayer() {
    platform::ViewModel view; view.screen = screen_;
    const auto* album = selectedAlbum();
    if (album != nullptr && !album->tracks.empty()) {
        view.title = album->tracks[trackIndex_].name;
        view.subtitle = album->name + (album->artist.empty()?"":"  -  "+album->artist) + "  |  " + (language_==Language::English?"TRACK":t("TITEL")) + " " + std::to_string(trackIndex_ + 1) + "/" + std::to_string(album->tracks.size());
        view.coverPath = album->tracks[trackIndex_].coverPath.empty() ? album->coverPath : album->tracks[trackIndex_].coverPath;
    } else view.title = currentTrackName_.empty() ? t("WIEDERGABE") : currentTrackName_;
    view.message = lastError_;
    platform_.setView(std::move(view));
}

void Application::changeTrack(int delta) {
    auto* album = selectedAlbum(); if (album == nullptr || album->tracks.empty()) return;
    const auto target = static_cast<long long>(trackIndex_) + delta;
    if (target < 0 || target >= static_cast<long long>(album->tracks.size())) return;
    saveProgress(); trackIndex_ = static_cast<std::size_t>(target); openSelectedTrack();
}
void Application::cycleSleepTimer() {
    sleepMinutes_ = sleepMinutes_ == 0 ? 15 : sleepMinutes_ == 15 ? 30 : sleepMinutes_ == 30 ? 60 : 0;
    if (sleepMinutes_ > 0) sleepDeadline_ = std::chrono::steady_clock::now() + std::chrono::minutes(sleepMinutes_);
    platform_.setSleepTimer(sleepMinutes_);
}
void Application::scanFolders() {
    folderBrowser_.entries.clear(); folderBrowser_.index = 0;folderBrowser_.path=fileSystem_.normalizedPath(folderBrowser_.path); const auto parent = fileSystem_.normalizedPath(fileSystem_.parent(folderBrowser_.path));
    if (parent != folderBrowser_.path&&fileSystem_.isPathWithin(browseRoot_,parent)) folderBrowser_.entries.push_back({"..", parent, true});
    for (const auto& entry : fileSystem_.list(folderBrowser_.path)) if (entry.directory&&fileSystem_.isPathWithin(browseRoot_,entry.path)) folderBrowser_.entries.push_back(entry);
    const auto first = folderBrowser_.entries.begin() + (folderBrowser_.entries.empty() || folderBrowser_.entries.front().name != ".." ? 0 : 1);
    std::sort(first, folderBrowser_.entries.end(), [](const auto& left, const auto& right) { return left.name < right.name; });
}
void Application::publishFolders() {
    platform::ViewModel view; view.screen = platform::Screen::Folders; view.title = t("SAMMLUNGSORDNER");
    view.message = editor_.message.empty()?folderBrowser_.path:editor_.message; view.selected = folderBrowser_.index;
    for (const auto& folder : folderBrowser_.entries) view.items.push_back(folder.name == ".." ? "[..]" : std::string(t("[ORDNER]")) + "  " + folder.name);
    platform_.setView(std::move(view));
}

void Application::beginCollectionFolderSelection() {
    folderBrowser_.path = browseRoot_;
    editor_.editedIndex = static_cast<std::size_t>(-1);
    editor_.editingPath = false;
    editor_.message.clear();
    scanFolders();
    screen_ = platform::Screen::Folders;
}

void Application::beginCollectionName(std::size_t editIndex) {
    editor_.editedIndex = editIndex;
    editor_.message.clear();
    if (editIndex < collections_.size()) {
        editor_.pendingName = collections_[editIndex].name;
        editor_.pendingPath = collections_[editIndex].path;
    } else {
        editor_.pendingPath = folderBrowser_.path;
        editor_.pendingName = folderName(folderBrowser_.path);
    }
    keyboardIndex_ = 0;
    screen_ = platform::Screen::CollectionName;
}

void Application::savePendingCollection() {
    editor_.pendingName = trimmed(editor_.pendingName);
    editor_.pendingPath = fileSystem_.normalizedPath(editor_.pendingPath);
    if (editor_.pendingName.empty()) {
        editor_.message = t("Bitte einen Namen eingeben");
        return;
    }
    std::size_t savedIndex = editor_.editedIndex;
    auto updatedCollections = collections_;
    std::string previousPath;
    if (editor_.editedIndex < collections_.size()) {
        const auto duplicate=std::find_if(collections_.begin(),collections_.end(),[&](const auto& collection){return fileSystem_.normalizedPath(collection.path)==editor_.pendingPath;});
        if(duplicate!=collections_.end()&&static_cast<std::size_t>(std::distance(collections_.begin(),duplicate))!=editor_.editedIndex){editor_.message=t("Ordner wird bereits verwendet");return;}
        previousPath = collections_[editor_.editedIndex].path;
        updatedCollections[editor_.editedIndex] = {editor_.pendingName, editor_.pendingPath,editor_.pendingType};
    } else {
        const auto duplicate = std::find_if(collections_.begin(), collections_.end(), [&](const auto& collection) {
            return fileSystem_.normalizedPath(collection.path) == editor_.pendingPath;
        });
        if (duplicate != collections_.end()) {editor_.message=t("Ordner wird bereits verwendet");return;} else {
            updatedCollections.push_back({editor_.pendingName, editor_.pendingPath,editor_.pendingType});
            savedIndex = updatedCollections.size() - 1;
        }
    }
    if (progressStore_ != nullptr && !progressStore_->saveCollections(updatedCollections)) {
        editor_.message = t("Sammlung konnte nicht gespeichert werden");
        return;
    }
    collections_ = std::move(updatedCollections);
    if (!previousPath.empty() && previousPath != editor_.pendingPath) libraryCache_.invalidate(previousPath);
    libraryCache_.invalidate(editor_.pendingPath);
    scanLibrary();
    sourceIndex_ = std::min(savedIndex, sources_.empty() ? std::size_t{0} : sources_.size() - 1);
    collectionManagerIndex_ = sourceIndex_ + 1;
    screen_ = platform::Screen::Collections;
}

void Application::publishCollectionManager() {
    platform::ViewModel view;
    view.screen = platform::Screen::CollectionManager;
    view.title = t("SAMMLUNGEN VERWALTEN");
    view.selected = collectionManagerIndex_;
    view.items.push_back(t("[+]  NEUE SAMMLUNG"));
    view.itemImages.push_back({});
    for (const auto& source : sources_) {
        view.items.push_back((source.available?std::string{}:std::string(t("[FEHLT]"))+"  ")+source.name+"|"+typeLabel(source.type,language_));
        view.itemImages.push_back(source.coverPath);
    }
    if (!editor_.message.empty()) {
        view.message = editor_.message;
    } else if (collectionManagerIndex_ > 0 && collectionManagerIndex_ - 1 < sources_.size()) {
        view.coverPath = sources_[collectionManagerIndex_ - 1].coverPath;
        view.message = sources_[collectionManagerIndex_ - 1].path;
    } else {
        view.message = t("Ordner waehlen und Sammlung benennen");
    }
    platform_.setView(std::move(view));
}

void Application::publishCollectionName() {
    platform::ViewModel view;
    view.screen = platform::Screen::CollectionName;
    view.title = t("NAME DER SAMMLUNG");
    view.subtitle = editor_.pendingName.empty() ? "_" : editor_.pendingName;
    view.message = editor_.message.empty() ? editor_.pendingPath : editor_.message;
    view.selected = keyboardIndex_;
    for (const char character : keyboardCharacters()) {
        view.items.push_back(character == ' ' ? t("LEER") : std::string(1, character));
    }
    platform_.setView(std::move(view));
}

void Application::publishCollectionType(){platform::ViewModel view;view.screen=platform::Screen::CollectionType;view.title=t("SAMMLUNGSTYP");view.message=editor_.pendingPath;view.selected=collectionTypeIndex_;for(const auto&type:collectionTypes())view.items.push_back(t(type.second.c_str()));platform_.setView(std::move(view));}
void Application::publishCollectionDelete(){platform::ViewModel view;view.screen=platform::Screen::CollectionDelete;view.title=t("SAMMLUNG LOESCHEN?");if(collectionManagerIndex_>0&&collectionManagerIndex_-1<collections_.size()){const auto& collection=collections_[collectionManagerIndex_-1];view.subtitle=collection.name;view.message=collection.path;}view.items={t("A  ENDGUELTIG LOESCHEN"),t("B  ABBRECHEN")};platform_.setView(std::move(view));}
void Application::deleteSelectedCollection(){if(collectionManagerIndex_==0||collections_.size()<=1){editor_.message=t("Mindestens eine Sammlung bleibt erhalten");screen_=platform::Screen::CollectionManager;return;}const auto index=collectionManagerIndex_-1;if(index>=collections_.size())return;auto updatedCollections=collections_;const auto removedPath=updatedCollections[index].path;updatedCollections.erase(updatedCollections.begin()+static_cast<std::ptrdiff_t>(index));if(progressStore_&&!progressStore_->saveCollections(updatedCollections)){editor_.message=t("Sammlung konnte nicht geloescht werden");screen_=platform::Screen::CollectionManager;return;}editor_.message.clear();collections_=std::move(updatedCollections);libraryCache_.invalidate(removedPath);scanLibrary();collectionManagerIndex_=std::min(index+1,collections_.size());screen_=platform::Screen::CollectionManager;}
void Application::reorderSelectedCollection(int direction){if(collectionManagerIndex_==0||collections_.size()<2)return;const auto index=collectionManagerIndex_-1;const auto target=static_cast<long long>(index)+direction;if(target<0||target>=static_cast<long long>(collections_.size()))return;auto updatedCollections=collections_;std::swap(updatedCollections[index],updatedCollections[static_cast<std::size_t>(target)]);if(progressStore_&&!progressStore_->saveCollections(updatedCollections)){editor_.message=t("Reihenfolge konnte nicht gespeichert werden");return;}editor_.message.clear();collections_=std::move(updatedCollections);scanLibrary();collectionManagerIndex_=static_cast<std::size_t>(target)+1;}

void Application::openBluetooth(){bluetooth_.returnScreen=screen_;screen_=platform::Screen::Bluetooth;bluetooth_.index=0;refreshBluetooth();}
void Application::refreshBluetooth(const std::string& message){platform::ViewModel loading;loading.screen=platform::Screen::Bluetooth;loading.title="BLUETOOTH";loading.message=t("Bluetooth wird gelesen ...");loading.immediate=true;platform_.setView(std::move(loading));bluetooth_.state=platform_.bluetoothState();if(bluetooth_.index>=bluetooth_.state.devices.size())bluetooth_.index=bluetooth_.state.devices.empty()?0:bluetooth_.state.devices.size()-1;bluetooth_.message=message;if(bluetooth_.message.empty()&&bluetooth_.state.devices.empty())bluetooth_.message=t("Keine gekoppelten Kopfhoerer gefunden");}
void Application::publishBluetooth(){platform::ViewModel view;view.screen=platform::Screen::Bluetooth;view.title=t("BLUETOOTH-KOPFHOERER");view.subtitle=std::string("BLUETOOTH ")+t(bluetooth_.state.powered?"AN":"AUS")+"  |  AUDIO "+t(bluetooth_.state.audioActive?"AKTIV":"NICHT AKTIV");view.selected=bluetooth_.index;for(const auto& device:bluetooth_.state.devices)view.items.push_back(device.name+(device.active?std::string("  ")+t("[AKTIV]"):device.connected?std::string("  ")+t("[VERBUNDEN]"):""));view.message=bluetooth_.message;platform_.setView(std::move(view));}

std::string Application::findCollectionCover(const std::string& path,
    const std::vector<library::Collection>& albums) const {
    for (const auto& entry : fileSystem_.list(path)) {
        if (!entry.directory && isCoverName(entry.name)) return entry.path;
    }
    for (const auto& album : albums) if (!album.coverPath.empty()) return album.coverPath;
    return {};
}
bool Application::openMedia(const std::string& path) {
    if (audioPlayer_ == nullptr || !audioPlayer_->open(path)) {
        lastError_ = audioPlayer_ == nullptr ? t("Audioausgabe nicht verfuegbar") : audioPlayer_->error();
        if (language_ == Language::English && lastError_.rfind("Keine MP3-Datei (", 0) == 0)
            lastError_ = "Not an MP3 file";
        std::cerr << "CoverPlayer audio error for '" << path << "': " << lastError_ << '\n'; return false;
    }
    lastError_.clear(); currentMediaId_ = path; trackEndHandled_ = false;
    if (progressStore_ != nullptr) { const auto progress = progressStore_->load(currentMediaId_); if (progress && !progress->completed && progress->positionSeconds > 0.0) audioPlayer_->seekSeconds(progress->positionSeconds); }
    lastProgressSave_ = std::chrono::steady_clock::now(); return true;
}
void Application::saveProgress() {
    if (progressStore_ == nullptr || audioPlayer_ == nullptr || currentMediaId_.empty() || !audioPlayer_->isOpen()) return;
    progressStore_->save(currentMediaId_, {audioPlayer_->positionSeconds(), audioPlayer_->isFinished()});
    lastProgressSave_ = std::chrono::steady_clock::now();
}
}
