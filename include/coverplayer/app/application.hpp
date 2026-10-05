#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "coverplayer/library/library_scanner.hpp"
#include "coverplayer/library/library_cache.hpp"
#include "coverplayer/persistence/progress_store.hpp"
#include "coverplayer/platform/platform.hpp"
#include "coverplayer/platform/file_system.hpp"

namespace coverplayer::platform {
class Platform;
}

namespace coverplayer::audio {
class AudioPlayer;
}

namespace coverplayer::persistence {
class ProgressStore;
}
namespace coverplayer::platform { class FileSystem; }

namespace coverplayer::app {

class Application {
public:
    Application(
        platform::Platform& platform,
        audio::AudioPlayer* audioPlayer,
        persistence::ProgressStore* progressStore,
        library::LibraryCache& libraryCache,
        platform::FileSystem& fileSystem,
        std::string mediaRoot,
        std::string initialMediaPath,
        std::string browseRoot = {});
    int run();

private:
    struct LibrarySource {
        std::string name;
        std::string path;
        std::string coverPath;
        std::string type;
        bool available = true;
        std::vector<library::Collection> albums;
    };

    bool openMedia(const std::string& path);
    void saveProgress();
    void scanLibrary();
    void moveSelection(int delta);
    void changeTrack(int delta);
    void cycleSleepTimer();
    void scanFolders();
    void publishFolders();
    void beginCollectionFolderSelection();
    void beginCollectionName(std::size_t editIndex);
    void savePendingCollection();
    void publishCollectionManager();
    void publishCollectionName();
    void publishCollectionType();
    void publishCollectionDelete();
    void deleteSelectedCollection();
    void reorderSelectedCollection(int direction);
    void openBluetooth();
    void refreshBluetooth(const std::string& message = {});
    void publishBluetooth();
    [[nodiscard]] std::string findCollectionCover(const std::string& path,
        const std::vector<library::Collection>& albums) const;

    // run() dispatch: each method below is a named, order-preserving slice
    // of the single loop body run() used to be. Call order in run() matches
    // the original statement order exactly, since several of these mutate
    // screen_ mid-frame and later slices must see that updated screen_.
    void handleBackgroundRequest();
    void handleBluetoothOpenRequest(const platform::InputActions& actions);
    void handleCollectionManagementInput(const platform::InputActions& actions);
    void handleBrowserViewToggle(const platform::InputActions& actions);
    void handleBluetoothScreenShortcuts(const platform::InputActions& actions);
    void handleTransportShortcuts(const platform::InputActions& actions);
    void handlePlayerAndResumeShortcuts(const platform::InputActions& actions);
    void handleRescanRequest(const platform::InputActions& actions);
    void handleCollectionNameEditing(const platform::InputActions& actions);
    void handleNavigation(const platform::InputActions& actions);
    void handleAcceptAndBack(const platform::InputActions& actions);
    void updatePlayback(const platform::InputActions& actions);
    void updateSleepTimerDisplay();
    void publishCurrentView();

    // handleAcceptAndBack sub-handlers: only one fires per frame (screen_ is
    // singular), so splitting per screen cannot reorder anything relative to
    // the original if/else-if chain.
    void handleCollectionsAcceptOrBack(const platform::InputActions& actions);
    void handleCollectionManagerAcceptOrBack(const platform::InputActions& actions);
    void handleCollectionTypeAcceptOrBack(const platform::InputActions& actions);
    void handleCollectionDeleteAcceptOrBack(const platform::InputActions& actions);
    void handleBluetoothAcceptOrBack(const platform::InputActions& actions);
    void handleBrowserAcceptOrBack(const platform::InputActions& actions);
    void handleTracksAcceptOrBack(const platform::InputActions& actions);
    void handlePlayerBack(const platform::InputActions& actions);
    void handleFoldersAcceptOrBack(const platform::InputActions& actions);
    void handleCollectionNameBack(const platform::InputActions& actions);

    // publishCurrentView sub-views: replaces the former single publishView().
    void publishCollections();
    void publishBrowser();
    void publishTracks();
    void publishPlayer();
    [[nodiscard]] const char* t(const char* text) const { return tr(language_, text); }

    platform::Platform& platform_;
    audio::AudioPlayer* audioPlayer_;
    persistence::ProgressStore* progressStore_;
    library::LibraryCache& libraryCache_;
    platform::FileSystem& fileSystem_;
    std::string mediaRoot_;
    std::string currentMediaId_;
    std::string currentTrackName_;
    std::string lastError_;
    std::string initialMediaPath_;
    std::string browseRoot_;
    std::chrono::steady_clock::time_point lastProgressSave_;
    [[nodiscard]] library::Collection* selectedAlbum();
    [[nodiscard]] const library::Collection* selectedAlbum() const;
    [[nodiscard]] std::vector<library::Collection>* currentNodes();
    [[nodiscard]] const std::vector<library::Collection>* currentNodes() const;
    [[nodiscard]] std::string currentContainerName() const;
    void openSelectedNode();
    void restoreLastSelection();
    void selectResumeTrack();
    void openSelectedTrack();

    std::vector<LibrarySource> sources_;
    std::vector<persistence::MediaCollection> collections_;
    std::size_t sourceIndex_ = 0;
    std::size_t albumIndex_ = 0;
    std::size_t trackIndex_ = 0;
    std::vector<std::size_t> navigationPath_;
    platform::Screen screen_ = platform::Screen::Collections;
    std::chrono::steady_clock::time_point sleepDeadline_{};
    int sleepMinutes_ = 0;
    bool resumeAfterSuspend_ = false;
    std::string playerNotice_;
    std::size_t collectionManagerIndex_ = 0;
    std::size_t keyboardIndex_ = 0;
    std::size_t collectionTypeIndex_ = 0;
    platform::Screen albumBrowserScreen_ = platform::Screen::CoverFlow;
    Language language_ = Language::German;

    // Transient state for the collection-editor flow (folder pick -> type ->
    // name), valid only while one of the Collection* screens is active.
    struct CollectionEditorState {
        std::string pendingName;
        std::string pendingPath;
        std::string pendingType = "audiobook";
        std::size_t editedIndex = static_cast<std::size_t>(-1);
        std::string message;
        bool editingPath = false;
    } editor_;

    // Transient state for the Folders screen, valid only while browsing.
    struct FolderBrowserState {
        std::string path;
        std::vector<platform::DirectoryEntry> entries;
        std::size_t index = 0;
    } folderBrowser_;

    // Transient state for the Bluetooth screen, valid only while it is open.
    struct BluetoothUiState {
        platform::BluetoothState state;
        std::size_t index = 0;
        std::string message;
        platform::Screen returnScreen = platform::Screen::Collections;
    } bluetooth_;
};

} // namespace coverplayer::app
