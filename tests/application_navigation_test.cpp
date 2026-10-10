#include "coverplayer/app/application.hpp"
#include "coverplayer/audio/audio_player.hpp"
#include "coverplayer/library/library_cache.hpp"
#include "coverplayer/persistence/progress_store.hpp"
#include "coverplayer/platform/file_system.hpp"
#include "coverplayer/platform/platform.hpp"

#include <iostream>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

class FakePlatform final : public coverplayer::platform::Platform {
public:
    std::vector<coverplayer::platform::InputActions> script;
    std::vector<coverplayer::platform::ViewModel> views;
    std::size_t cursor = 0;
    std::function<void(std::size_t)> beforePoll;
    std::vector<bool> pausedStates;
    std::string name() const override { return "test"; }
    bool bluetoothAvailable = false;
    coverplayer::platform::BluetoothState bluetooth{};
    std::string changedBluetoothAddress;
    bool bluetoothConnectionSucceeds = true;
    bool backgroundSupported = false;
    coverplayer::Language activeLanguage = coverplayer::Language::German;
    int enterBackgroundCalls = 0;
    std::vector<std::string> lastBackgroundTrackPaths;
    std::size_t lastBackgroundStartIndex = 0;
    coverplayer::platform::Capabilities capabilities() const noexcept override { return {false, bluetoothAvailable, true}; }
    bool enterBackgroundPlayback(const std::vector<std::string>& trackPaths, std::size_t startIndex) override {
        ++enterBackgroundCalls; lastBackgroundTrackPaths = trackPaths; lastBackgroundStartIndex = startIndex; return backgroundSupported;
    }
    coverplayer::platform::InputActions pollEvents() override { if (beforePoll) beforePoll(cursor); return cursor < script.size() ? script[cursor++] : coverplayer::platform::InputActions{true}; }
    void setPlaybackStatus(bool, bool paused, double, double) override { pausedStates.push_back(paused); }
    void setView(coverplayer::platform::ViewModel view) override { views.push_back(std::move(view)); }
    void setSleepTimer(int) override {}
    void setPlayerDetails(int, std::size_t, std::string, std::optional<int>) override {}
    void setLanguage(coverplayer::Language language) override { activeLanguage = language; }
    std::optional<int> systemVolumePercent() const override { return std::nullopt; }
    std::optional<int> adjustSystemVolume(int) override { return std::nullopt; }
    coverplayer::platform::BluetoothState bluetoothState() override { return bluetooth; }
    bool setBluetoothEnabled(bool enabled) override { bluetooth.powered = enabled; return true; }
    bool setBluetoothDeviceConnected(const std::string& address, bool connected) override {
        changedBluetoothAddress = address;
        if (bluetoothConnectionSucceeds) for (auto& device : bluetooth.devices) if (device.address == address) device.connected = connected;
        return bluetoothConnectionSucceeds;
    }
};

class FakeAudio final : public coverplayer::audio::AudioPlayer {
public:
    std::string opened;
    std::vector<std::string> opens;
    bool finished = false;
    bool open(const std::string& path) override { opened = path; opens.push_back(path); paused = false; finished = false; position = 0; return true; }
    void update() override {}
    void togglePause() override { paused = !paused; }
    void seekSeconds(double delta) override { position += delta; }
    void setVolumePercent(int percent) override { volume = percent; }
    bool isOpen() const noexcept override { return !opened.empty(); }
    bool isPaused() const noexcept override { return paused; }
    bool isFinished() const noexcept override { return finished; }
    double positionSeconds() const noexcept override { return position; }
    double durationSeconds() const noexcept override { return 3600; }
    int volumePercent() const noexcept override { return volume; }
    std::string error() const override { return {}; }
private:
    bool paused = false;
    double position = 0;
    int volume = 100;
};

class FakeProgress : public coverplayer::persistence::ProgressStore {
public:
    std::string savedLanguage = "de";
    std::vector<std::pair<std::string, coverplayer::persistence::TrackProgress>> saved;
    std::vector<coverplayer::persistence::MediaCollection> configured{{"Crime", "library/Crime"}};
    std::optional<coverplayer::persistence::TrackProgress> load(const std::string& path) override {
        if (path == "two-2.mp3") return coverplayer::persistence::TrackProgress{42.0, false};
        return std::nullopt;
    }
    bool save(const std::string& path, const coverplayer::persistence::TrackProgress& progress) override { saved.emplace_back(path, progress); return true; }
    std::string lastMediaId() override { return {}; }
    std::string mediaRoot() override { return {}; }
    bool saveMediaRoot(const std::string&) override { return true; }
    std::vector<coverplayer::persistence::MediaCollection> collections() override { return configured; }
    bool saveCollections(const std::vector<coverplayer::persistence::MediaCollection>& collections) override { configured = collections; return true; }
    std::vector<double> bookmarks(const std::string&) override { return {}; }
    bool addBookmark(const std::string&, double) override { return true; }
    std::string language() override { return savedLanguage; }
    bool saveLanguage(const std::string& code) override { savedLanguage = code; return true; }
};

class FakeCache final : public coverplayer::library::LibraryCache {
public:
    std::optional<std::vector<coverplayer::library::Collection>> load(const std::string&,std::uint64_t) override {
        return std::vector<coverplayer::library::Collection>{
            {"Book One", "library/Crime/Book One", "one.jpg", "", {{"Chapter 1", "one-1.mp3", "", "", 1}}},
            {"Book Two", "library/Crime/Book Two", "two.jpg", "", {{"Chapter 1", "two-1.mp3", "", "", 1, 0, "track-one.jpg"}, {"Chapter 2", "two-2.mp3", "", "", 2}}}
        };
    }
    bool save(const std::string&,std::uint64_t,const std::vector<coverplayer::library::Collection>&) override { return true; }
    void invalidate(const std::string&) override {}
};

class HierarchicalCache final : public coverplayer::library::LibraryCache {
public:
    std::optional<std::vector<coverplayer::library::Collection>> load(const std::string&,std::uint64_t) override {
        coverplayer::library::Collection albumOne{"Album One", "library/Crime/Artist/Album One", "one.jpg", "Artist",
            {{"Chapter 1", "one-1.mp3", "Artist", "Album One", 1}}};
        coverplayer::library::Collection albumTwo{"Album Two", "library/Crime/Artist/Album Two", "two.jpg", "Artist",
            {{"Chapter 1", "two-1.mp3", "Artist", "Album Two", 1}, {"Chapter 2", "two-2.mp3", "Artist", "Album Two", 2}}};
        coverplayer::library::Collection artist{"Artist", "library/Crime/Artist", "one.jpg", "", {}, {albumOne, albumTwo}};
        return std::vector<coverplayer::library::Collection>{artist};
    }
    bool save(const std::string&,std::uint64_t,const std::vector<coverplayer::library::Collection>&) override { return true; }
    void invalidate(const std::string&) override {}
};

class SingleTrackCache final : public coverplayer::library::LibraryCache {
public:
    std::optional<std::vector<coverplayer::library::Collection>> load(const std::string&,std::uint64_t) override {
        return std::vector<coverplayer::library::Collection>{
            {"Single Book", "library/Single", "single.jpg", "", {{"Whole Book", "single.mp3", "", "", 100}}}
        };
    }
    bool save(const std::string&,std::uint64_t,const std::vector<coverplayer::library::Collection>&) override { return true; }
    void invalidate(const std::string&) override {}
};

class SingleTrackProgress final : public FakeProgress {
public:
    SingleTrackProgress() { configured = {{"Single", "library/Single"}}; }
    std::optional<coverplayer::persistence::TrackProgress> load(const std::string& path) override {
        if (path == "single.mp3") return coverplayer::persistence::TrackProgress{88.0, false};
        return std::nullopt;
    }
};

class MultipleCollectionProgress final : public FakeProgress {
public:
    MultipleCollectionProgress() {
        configured = {{"First", "library/First"}, {"Second", "library/Second"}};
    }
};

class CountingCache final : public coverplayer::library::LibraryCache {
public:
    std::vector<std::string> loaded;
    std::vector<std::string> invalidated;
    std::optional<std::vector<coverplayer::library::Collection>> load(const std::string& root,std::uint64_t) override {
        loaded.push_back(root);
        return std::vector<coverplayer::library::Collection>{
            {root, root, "", "", {{"Track", root + "/track.mp3", "", "", 1}}}
        };
    }
    bool save(const std::string&,std::uint64_t,const std::vector<coverplayer::library::Collection>&) override { return true; }
    void invalidate(const std::string& root) override { invalidated.push_back(root); }
};

class FakeFileSystem final : public coverplayer::platform::FileSystem {
public:
    bool provideRootCover = false;
    bool rootAvailable = true;
    std::vector<coverplayer::platform::DirectoryEntry> list(const std::string& path) const override {
        if (provideRootCover && path == "library/Crime") return {{"folder.png", "library/Crime/folder.png", false}};
        return {};
    }
    std::string parent(const std::string& path) const override { return path; }
    bool directoryExists(const std::string&) const override { return rootAvailable; }
};

int main() {
    FakePlatform platform;
    coverplayer::platform::InputActions rootBack; rootBack.back = true;
    coverplayer::platform::InputActions openCollection; openCollection.accept = true;
    coverplayer::platform::InputActions nextCover; nextCover.navigate = 1;
    coverplayer::platform::InputActions openBook; openBook.accept = true;
    coverplayer::platform::InputActions play; play.accept = true;
    coverplayer::platform::InputActions quit; quit.quit = true;
    platform.script = {rootBack, openCollection, nextCover, openBook, play, quit};
    FakeAudio audio; FakeProgress progress; FakeCache cache; FakeFileSystem fileSystem;
    coverplayer::app::Application app(platform, &audio, &progress, cache, fileSystem, "unused", "");
    app.run();

    bool sawCoverFlow = false;
    for (const auto& view : platform.views) {
        if (view.screen == coverplayer::platform::Screen::CoverFlow && view.items.size() == 2 &&
            view.itemImages.size() == 2 && view.itemImages[1] == "two.jpg") sawCoverFlow = true;
    }
    if (!sawCoverFlow || audio.opened != "two-2.mp3" || audio.positionSeconds() != 42.0) {
        std::cerr << "CoverFlow navigation or default resume selection failed\n";
        return 1;
    }

    // A one-file audiobook must use exactly the same resume path as a
    // multi-track book. This guards the common long-single-MP3 layout.
    FakePlatform singleResumePlatform;
    singleResumePlatform.script = {openBook, rootBack, quit};
    FakeAudio singleResumeAudio; SingleTrackProgress singleResumeProgress; SingleTrackCache singleResumeCache; FakeFileSystem singleResumeFileSystem;
    coverplayer::app::Application singleResumeApp(singleResumePlatform, &singleResumeAudio, &singleResumeProgress,
        singleResumeCache, singleResumeFileSystem, "unused", "");
    singleResumeApp.run();
    if (singleResumeAudio.opened != "single.mp3" || singleResumeAudio.positionSeconds() != 88.0) {
        std::cerr << "single-track audiobook did not resume at its saved position\n";
        return 1;
    }
    bool sawSinglePlayer = false;
    bool sawSingleTracks = false;
    for (const auto& view : singleResumePlatform.views) {
        sawSinglePlayer |= view.screen == coverplayer::platform::Screen::Player;
        sawSingleTracks |= view.screen == coverplayer::platform::Screen::Tracks;
    }
    if (!sawSinglePlayer || sawSingleTracks || singleResumePlatform.views.back().screen != coverplayer::platform::Screen::CoverFlow) {
        std::cerr << "single-track audiobook showed the track list instead of returning to the album browser\n";
        return 1;
    }

    // Explicit refresh currently means a complete library refresh: every
    // configured collection cache is invalidated and then loaded again.
    FakePlatform rescanPlatform;
    coverplayer::platform::InputActions rescan; rescan.rescan = true;
    rescanPlatform.script = {rescan, quit};
    FakeAudio rescanAudio; MultipleCollectionProgress rescanProgress; CountingCache rescanCache; FakeFileSystem rescanFileSystem;
    coverplayer::app::Application rescanApp(rescanPlatform, &rescanAudio, &rescanProgress,
        rescanCache, rescanFileSystem, "unused", "");
    rescanApp.run();
    if (rescanCache.invalidated != std::vector<std::string>{"library/First", "library/Second"} ||
        rescanCache.loaded != std::vector<std::string>{"library/First", "library/Second", "library/First", "library/Second"}) {
        std::cerr << "explicit scan did not refresh every configured collection\n";
        return 1;
    }

    FakePlatform singlePlatform;singlePlatform.script={quit};
    FakeAudio singleAudio;FakeProgress singleProgress;FakeCache singleCache;FakeFileSystem singleFileSystem;
    coverplayer::app::Application singleApp(singlePlatform,&singleAudio,&singleProgress,singleCache,singleFileSystem,"unused","");
    singleApp.run();
    if(singlePlatform.views.empty()||singlePlatform.views.back().screen!=coverplayer::platform::Screen::CoverFlow){
        std::cerr<<"single available collection did not skip the collection picker\n";return 1;
    }

    FakePlatform boundaryPlatform;
    coverplayer::platform::InputActions previousAtStart; previousAtStart.navigate = -1;
    boundaryPlatform.script = {rootBack, openCollection, previousAtStart, openBook, play, quit};
    FakeAudio boundaryAudio; FakeProgress boundaryProgress; FakeCache boundaryCache; FakeFileSystem boundaryFileSystem;
    coverplayer::app::Application boundaryApp(boundaryPlatform, &boundaryAudio, &boundaryProgress,
        boundaryCache, boundaryFileSystem, "unused", "");
    boundaryApp.run();
    if (boundaryAudio.opened != "one-1.mp3") {
        std::cerr << "CoverFlow wrapped past its first item instead of stopping\n";
        return 1;
    }

    FakePlatform listPlatform;
    coverplayer::platform::InputActions toggleView; toggleView.toggleLibraryView = true;
    listPlatform.script = {rootBack, openCollection, toggleView, nextCover, openBook, rootBack, quit};
    FakeAudio listAudio; FakeProgress listProgress; FakeCache listCache; FakeFileSystem listFileSystem;
    coverplayer::app::Application listApp(listPlatform, &listAudio, &listProgress, listCache, listFileSystem, "unused", "");
    listApp.run();
    bool returnedToList = false;
    for (const auto& view : listPlatform.views) {
        if (view.screen == coverplayer::platform::Screen::AlbumList && view.selected == 1 && view.coverPath == "two.jpg") returnedToList = true;
    }
    if (!returnedToList) {
        std::cerr << "CoverFlow/list toggle lost the album selection or return view\n";
        return 1;
    }

    FakePlatform hierarchyPlatform;
    hierarchyPlatform.script = {rootBack, openCollection, openBook, nextCover, openBook, play, quit};
    FakeAudio hierarchyAudio; FakeProgress hierarchyProgress; HierarchicalCache hierarchyCache; FakeFileSystem hierarchyFileSystem;
    coverplayer::app::Application hierarchyApp(hierarchyPlatform, &hierarchyAudio, &hierarchyProgress,
        hierarchyCache, hierarchyFileSystem, "unused", "");
    hierarchyApp.run();
    bool sawArtistLevel=false,sawAlbumLevel=false,sawLeafTracks=false;
    for(const auto& view:hierarchyPlatform.views){
        if(view.screen==coverplayer::platform::Screen::CoverFlow&&view.title=="Crime"&&view.items.size()==1)sawArtistLevel=true;
        if(view.screen==coverplayer::platform::Screen::CoverFlow&&view.title=="Artist"&&view.items.size()==2)sawAlbumLevel=true;
        if(view.screen==coverplayer::platform::Screen::Tracks&&view.title=="Album Two"&&view.items.size()==2)sawLeafTracks=true;
    }
    if(!sawArtistLevel||!sawAlbumLevel||!sawLeafTracks||hierarchyAudio.opened!="two-2.mp3"||hierarchyAudio.positionSeconds()!=42.0){
        std::cerr<<"recursive browser did not keep CoverFlow above the leaf track list\n";return 1;
    }

    bool sawCollectionFallback = false;
    for (const auto& view : platform.views) {
        if (view.screen == coverplayer::platform::Screen::Collections &&
            !view.itemImages.empty() && view.itemImages.front() == "one.jpg") sawCollectionFallback = true;
    }
    if (!sawCollectionFallback) {
        std::cerr << "Collection did not fall back to its first album cover\n";
        return 1;
    }

    FakePlatform directCoverPlatform; directCoverPlatform.script = {rootBack,quit};
    FakeAudio directCoverAudio; FakeProgress directCoverProgress; FakeCache directCoverCache; FakeFileSystem directCoverFileSystem;
    directCoverFileSystem.provideRootCover = true;
    coverplayer::app::Application directCoverApp(directCoverPlatform, &directCoverAudio, &directCoverProgress,
        directCoverCache, directCoverFileSystem, "unused", "");
    directCoverApp.run();
    bool sawDirectCover=false;for(const auto& view:directCoverPlatform.views)if(view.screen==coverplayer::platform::Screen::Collections&&view.coverPath=="library/Crime/folder.png")sawDirectCover=true;
    if (!sawDirectCover) {
        std::cerr << "Collection root cover did not override the album fallback\n";
        return 1;
    }

    FakePlatform missingPlatform; missingPlatform.script = {quit};
    FakeAudio missingAudio; FakeProgress missingProgress; FakeCache missingCache; FakeFileSystem missingFileSystem;
    missingFileSystem.rootAvailable = false;
    coverplayer::app::Application missingApp(missingPlatform, &missingAudio, &missingProgress,
        missingCache, missingFileSystem, "unused", "");
    missingApp.run();
    bool sawMissing = false;
    for (const auto& view : missingPlatform.views) {
        if (view.screen == coverplayer::platform::Screen::Collections && !view.items.empty() &&
            view.items.front().find("[FEHLT]") != std::string::npos) sawMissing = true;
    }
    if (!sawMissing) {
        std::cerr << "Missing collection path was not surfaced in the library\n";
        return 1;
    }

    FakePlatform addPlatform;
    coverplayer::platform::InputActions manage; manage.openFolders = true;
    coverplayer::platform::InputActions chooseAdd; chooseAdd.accept = true;
    coverplayer::platform::InputActions chooseFolder; chooseFolder.rescan = true;
    coverplayer::platform::InputActions selectPodcast; selectPodcast.navigate = 3;
    coverplayer::platform::InputActions chooseType; chooseType.accept = true;
    coverplayer::platform::InputActions clearName; clearName.clearText = true;
    coverplayer::platform::InputActions enterName; enterName.textInput = "Podcasts";
    coverplayer::platform::InputActions saveName; saveName.saveText = true;
    addPlatform.script = {rootBack,manage, chooseAdd, chooseFolder, selectPodcast, chooseType, clearName, enterName, saveName, quit};
    FakeAudio addAudio; FakeProgress addProgress; FakeCache addCache; FakeFileSystem addFileSystem;
    coverplayer::app::Application addApp(addPlatform, &addAudio, &addProgress, addCache, addFileSystem, "unused", "");
    addApp.run();
    if (addProgress.configured.size() != 2 || addProgress.configured[1].name != "Podcasts" ||
        addProgress.configured[1].path != "unused" || addProgress.configured[1].type != "podcast") {
        std::cerr << "Named collection creation workflow failed\n";
        return 1;
    }

    FakePlatform editPlatform;
    coverplayer::platform::InputActions selectExisting; selectExisting.navigate = 1;
    coverplayer::platform::InputActions editExisting; editExisting.accept = true;
    coverplayer::platform::InputActions selectMusic; selectMusic.navigate = 2;
    editPlatform.script = {rootBack,manage, selectExisting, editExisting, selectMusic, chooseType, saveName, quit};
    FakeAudio editAudio; FakeProgress editProgress; FakeCache editCache; FakeFileSystem editFileSystem;
    coverplayer::app::Application editApp(editPlatform, &editAudio, &editProgress, editCache, editFileSystem, "unused", "");
    editApp.run();
    if (editProgress.configured.size() != 1 || editProgress.configured[0].name != "Crime" ||
        editProgress.configured[0].type != "music") {
        std::cerr << "Existing collection type edit created a duplicate or lost the edit\n";
        return 1;
    }

    FakePlatform organizePlatform;
    coverplayer::platform::InputActions moveDown; moveDown.reorder = 1;
    coverplayer::platform::InputActions requestDelete; requestDelete.deleteItem = true;
    coverplayer::platform::InputActions confirmDelete; confirmDelete.accept = true;
    organizePlatform.script = {manage, selectExisting, moveDown, requestDelete, confirmDelete, quit};
    FakeAudio organizeAudio; FakeProgress organizeProgress; FakeCache organizeCache; FakeFileSystem organizeFileSystem;
    organizeProgress.configured = {{"Crime", "library/Crime", "audiobook"},
        {"Music", "library/Music", "music"}, {"Podcasts", "library/Podcasts", "podcast"}};
    coverplayer::app::Application organizeApp(organizePlatform, &organizeAudio, &organizeProgress,
        organizeCache, organizeFileSystem, "unused", "");
    organizeApp.run();
    if (organizeProgress.configured.size() != 2 || organizeProgress.configured[0].name != "Music" ||
        organizeProgress.configured[1].name != "Podcasts") {
        std::cerr << "Collection reorder/delete workflow failed\n";
        return 1;
    }

    FakePlatform bluetoothPlatform;
    bluetoothPlatform.bluetoothAvailable = true;
    bluetoothPlatform.bluetooth = {true, true, false, {
        {"00:11:22:33:44:55", "Headphones", true, false},
        {"AA:BB:CC:DD:EE:FF", "Speaker", false, false}}};
    coverplayer::platform::InputActions openBluetooth; openBluetooth.openBluetooth = true;
    coverplayer::platform::InputActions nextDevice; nextDevice.navigate = 1;
    coverplayer::platform::InputActions connectDevice; connectDevice.accept = true;
    coverplayer::platform::InputActions powerOff; powerOff.toggleBluetooth = true;
    bluetoothPlatform.script = {openBluetooth, nextDevice, connectDevice, powerOff, rootBack, quit};
    FakeAudio bluetoothAudio; FakeProgress bluetoothProgress; FakeCache bluetoothCache; FakeFileSystem bluetoothFileSystem;
    coverplayer::app::Application bluetoothApp(bluetoothPlatform, &bluetoothAudio, &bluetoothProgress,
        bluetoothCache, bluetoothFileSystem, "unused", "");
    bluetoothApp.run();
    bool sawBluetooth = false;
    for (const auto& view : bluetoothPlatform.views) if (view.screen == coverplayer::platform::Screen::Bluetooth) sawBluetooth = true;
    if (!sawBluetooth || bluetoothPlatform.changedBluetoothAddress != "AA:BB:CC:DD:EE:FF" || bluetoothPlatform.bluetooth.powered) {
        std::cerr << "Bluetooth menu connect or power workflow failed\n";
        return 1;
    }

    FakePlatform unsafeBluetoothPlatform;
    unsafeBluetoothPlatform.bluetoothAvailable=true;
    unsafeBluetoothPlatform.bluetoothConnectionSucceeds=false;
    unsafeBluetoothPlatform.bluetooth={true,true,false,{{"AA:BB:CC:DD:EE:FF","Headphones",false,false}}};
    unsafeBluetoothPlatform.script={openBluetooth,connectDevice,quit};
    FakeAudio unsafeBluetoothAudio;FakeProgress unsafeBluetoothProgress;FakeCache unsafeBluetoothCache;FakeFileSystem unsafeBluetoothFileSystem;
    coverplayer::app::Application unsafeBluetoothApp(unsafeBluetoothPlatform,&unsafeBluetoothAudio,&unsafeBluetoothProgress,
        unsafeBluetoothCache,unsafeBluetoothFileSystem,"unused","playing.mp3");
    unsafeBluetoothApp.run();
    if(!unsafeBluetoothAudio.isPaused()){
        std::cerr<<"playback resumed after unsafe Bluetooth volume synchronization failure\n";return 1;
    }

    coverplayer::platform::InputActions requestBackground; requestBackground.background = true;

    // Backgrounding always ends this process (a detached helper takes over
    // playback independently - see docs/architecture.md) - so the loop
    // should stop right at the background action regardless of whether the
    // platform reports success, and it must hand off the whole album's
    // track order plus which one is actually playing, so the helper can
    // advance to the next track itself instead of stopping dead at the end
    // of one chapter.
    FakePlatform backgroundPlatform; backgroundPlatform.backgroundSupported = true;
    backgroundPlatform.script = {rootBack, openCollection, nextCover, openBook, play, requestBackground};
    FakeAudio backgroundAudio; FakeProgress backgroundProgress; FakeCache backgroundCache; FakeFileSystem backgroundFileSystem;
    coverplayer::app::Application backgroundApp(backgroundPlatform, &backgroundAudio, &backgroundProgress,
        backgroundCache, backgroundFileSystem, "unused", "");
    backgroundApp.run();
    if (backgroundPlatform.enterBackgroundCalls != 1 || backgroundPlatform.cursor != backgroundPlatform.script.size() ||
        backgroundPlatform.lastBackgroundTrackPaths != std::vector<std::string>{"two-1.mp3", "two-2.mp3"} ||
        backgroundPlatform.lastBackgroundStartIndex != 1) {
        std::cerr << "backgrounding while playing did not hand off the whole album and quit\n";
        return 1;
    }

    FakePlatform noAudioBackgroundPlatform; noAudioBackgroundPlatform.backgroundSupported = true;
    noAudioBackgroundPlatform.script = {requestBackground, quit};
    FakeAudio noAudioBackgroundAudio; FakeProgress noAudioBackgroundProgress; FakeCache noAudioBackgroundCache; FakeFileSystem noAudioBackgroundFileSystem;
    coverplayer::app::Application noAudioBackgroundApp(noAudioBackgroundPlatform, &noAudioBackgroundAudio, &noAudioBackgroundProgress,
        noAudioBackgroundCache, noAudioBackgroundFileSystem, "unused", "");
    noAudioBackgroundApp.run();
    if (noAudioBackgroundPlatform.enterBackgroundCalls != 0 || noAudioBackgroundPlatform.cursor != 1) {
        std::cerr << "backgrounding with nothing open did not fall back to an immediate quit\n";
        return 1;
    }

    FakePlatform unsupportedBackgroundPlatform; unsupportedBackgroundPlatform.backgroundSupported = false;
    unsupportedBackgroundPlatform.script = {rootBack, openCollection, nextCover, openBook, play, requestBackground, quit};
    FakeAudio unsupportedBackgroundAudio; FakeProgress unsupportedBackgroundProgress; FakeCache unsupportedBackgroundCache; FakeFileSystem unsupportedBackgroundFileSystem;
    coverplayer::app::Application unsupportedBackgroundApp(unsupportedBackgroundPlatform, &unsupportedBackgroundAudio, &unsupportedBackgroundProgress,
        unsupportedBackgroundCache, unsupportedBackgroundFileSystem, "unused", "");
    unsupportedBackgroundApp.run();
    if (unsupportedBackgroundPlatform.enterBackgroundCalls != 1 || unsupportedBackgroundPlatform.cursor != 6) {
        std::cerr << "backgrounding still quit when the spawn was reported unsupported\n";
        return 1;
    }

    // A paused track has nothing to keep playing in the background, and the
    // background helper always starts playing immediately (it cannot
    // represent "open but paused") - so backgrounding it would resume
    // playback as a side effect of a gesture that never pressed play. This
    // should fall back to a plain quit, exactly like the "nothing open" case
    // above, without ever calling the platform.
    coverplayer::platform::InputActions pauseTrack; pauseTrack.togglePause = true;
    FakePlatform pausedBackgroundPlatform; pausedBackgroundPlatform.backgroundSupported = true;
    pausedBackgroundPlatform.script = {rootBack, openCollection, nextCover, openBook, play, pauseTrack, requestBackground, quit};
    FakeAudio pausedBackgroundAudio; FakeProgress pausedBackgroundProgress; FakeCache pausedBackgroundCache; FakeFileSystem pausedBackgroundFileSystem;
    coverplayer::app::Application pausedBackgroundApp(pausedBackgroundPlatform, &pausedBackgroundAudio, &pausedBackgroundProgress,
        pausedBackgroundCache, pausedBackgroundFileSystem, "unused", "");
    pausedBackgroundApp.run();
    if (pausedBackgroundPlatform.enterBackgroundCalls != 0 || pausedBackgroundPlatform.cursor != 7) {
        std::cerr << "backgrounding a paused track resumed playback via the background helper\n";
        return 1;
    }

    coverplayer::platform::InputActions switchLanguage; switchLanguage.toggleLanguage = true;
    FakePlatform languagePlatform; languagePlatform.script = {switchLanguage, rootBack, quit};
    FakeAudio languageAudio; FakeProgress languageProgress; FakeCache languageCache; FakeFileSystem languageFileSystem;
    coverplayer::app::Application languageApp(languagePlatform, &languageAudio, &languageProgress,
        languageCache, languageFileSystem, "unused", "");
    languageApp.run();
    bool sawEnglishCollections = false;
    for (const auto& view : languagePlatform.views)
        if (view.screen == coverplayer::platform::Screen::Collections && view.title == "COLLECTIONS") sawEnglishCollections = true;
    if (languageProgress.savedLanguage != "en" || languagePlatform.activeLanguage != coverplayer::Language::English || !sawEnglishCollections) {
        std::cerr << "switching to English did not update and save the UI language\n";
        return 1;
    }
    FakePlatform restartedPlatform; restartedPlatform.script = {rootBack, quit};
    coverplayer::app::Application restartedApp(restartedPlatform, &languageAudio, &languageProgress,
        languageCache, languageFileSystem, "unused", "");
    restartedApp.run();
    if (restartedPlatform.activeLanguage != coverplayer::Language::English) {
        std::cerr << "saved English language was not restored on restart\n";
        return 1;
    }

    // Browsing must not change the running track or reopen it on returning.
    coverplayer::platform::InputActions previousRow; previousRow.navigate = -1;
    FakePlatform browsePlatform; FakeAudio browseAudio; FakeProgress browseProgress;
    browsePlatform.script = {nextCover, openBook, previousRow, play, rootBack, nextCover, previousRow, play, rootBack, nextCover, play, rootBack, rootBack, quit};
    browsePlatform.beforePoll = [&](std::size_t frame) { if (frame == 4) browseAudio.seekSeconds(17); };
    coverplayer::app::Application browseApp(browsePlatform, &browseAudio, &browseProgress, cache, fileSystem, "unused", "");
    browseApp.run();
    if (browseAudio.opens != std::vector<std::string>{"two-1.mp3", "two-2.mp3"} ||
        browsePlatform.pausedStates[4] || browsePlatform.pausedStates[7] ||
        !browsePlatform.pausedStates[12] || browseAudio.opened != "two-2.mp3") {
        std::cerr << "album navigation interrupted or restarted playback\n"; return 1;
    }
    bool sawTrackCover = false, sawAlbumFallback = false, savedFirstPosition = false;
    for (const auto& view : browsePlatform.views) if (view.screen == coverplayer::platform::Screen::Player) {
        sawTrackCover |= view.coverPath == "track-one.jpg";
        sawAlbumFallback |= view.coverPath == "two.jpg";
    }
    for (const auto& entry : browseProgress.saved)
        savedFirstPosition |= entry.first == "two-1.mp3" && entry.second.positionSeconds == 17;
    if (!sawTrackCover || !sawAlbumFallback || !savedFirstPosition) {
        std::cerr << "title cover fallback or outgoing progress failed\n"; return 1;
    }

    // End-of-track follows the playing identity, even with a different row selected.
    FakePlatform advancePlatform; FakeAudio advanceAudio; FakeProgress advanceProgress;
    advancePlatform.script = {nextCover, openBook, previousRow, play, rootBack, nextCover, {}, play, quit};
    advancePlatform.beforePoll = [&](std::size_t frame) { if (frame == 6) advanceAudio.finished = true; };
    coverplayer::app::Application advanceApp(advancePlatform, &advanceAudio, &advanceProgress, cache, fileSystem, "unused", "");
    advanceApp.run();
    if (advanceAudio.opens != std::vector<std::string>{"two-1.mp3", "two-2.mp3"} ||
        advancePlatform.views.back().screen != coverplayer::platform::Screen::Player) {
        std::cerr << "automatic advance in title list skipped or reopened a track\n"; return 1;
    }

    // Automatic advance must preserve a deliberately different browsing selection.
    FakePlatform selectionPlatform; FakeAudio selectionAudio; FakeProgress selectionProgress;
    selectionPlatform.script = {nextCover, openBook, previousRow, play, rootBack, nextCover, {}, quit};
    selectionPlatform.beforePoll = [&](std::size_t frame) { if (frame == 6) selectionAudio.finished = true; };
    coverplayer::app::Application selectionApp(selectionPlatform, &selectionAudio, &selectionProgress, cache, fileSystem, "unused", "");
    selectionApp.run();
    if (selectionPlatform.views.back().screen != coverplayer::platform::Screen::Tracks ||
        selectionPlatform.views.back().selected != 1 || selectionAudio.opened != "two-2.mp3") {
        std::cerr << "automatic advance changed browsing context\n"; return 1;
    }

    // The last track ends in the list without wrapping or reopening the selection.
    FakePlatform endPlatform; FakeAudio endAudio; FakeProgress endProgress;
    endPlatform.script = {nextCover, openBook, play, rootBack, previousRow, {}, {}, {}, quit};
    endPlatform.beforePoll = [&](std::size_t frame) { if (frame == 5) endAudio.finished = true; };
    coverplayer::app::Application endApp(endPlatform, &endAudio, &endProgress, cache, fileSystem, "unused", "");
    endApp.run();
    if (endAudio.opens != std::vector<std::string>{"two-2.mp3"} ||
        endPlatform.views.back().screen != coverplayer::platform::Screen::Tracks ||
        endPlatform.views.back().selected != 0 || endProgress.saved.size() > 5) {
        std::cerr << "end of album wrapped or changed selection\n"; return 1;
    }

    // Opening another row saves the outgoing track under its own media ID.
    FakePlatform skipPlatform; FakeAudio skipAudio; FakeProgress skipProgress;
    skipPlatform.script = {nextCover, openBook, previousRow, play, rootBack, nextCover, play, quit};
    skipPlatform.beforePoll = [&](std::size_t frame) { if (frame == 4) skipAudio.seekSeconds(29); };
    coverplayer::app::Application skipApp(skipPlatform, &skipAudio, &skipProgress, cache, fileSystem, "unused", "");
    skipApp.run();
    bool savedOutgoing = false;
    for (const auto& entry : skipProgress.saved)
        savedOutgoing |= entry.first == "two-1.mp3" && entry.second.positionSeconds == 29;
    if (!savedOutgoing || skipAudio.opened != "two-2.mp3" || skipAudio.positionSeconds() != 42) {
        std::cerr << "manual title switch lost progress or resume position\n"; return 1;
    }
    return 0;
}
