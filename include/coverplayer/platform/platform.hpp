#pragma once

#include "coverplayer/platform/capabilities.hpp"

#include <string>
#include <optional>
#include <vector>

namespace coverplayer::platform {

struct InputActions {
    bool quit = false;
    bool togglePause = false;
    double seekSeconds = 0.0;
    std::string openMediaPath;
    int navigate = 0;
    bool accept = false;
    bool back = false;
    bool rescan = false;
    int changeTrack = 0;
    bool cycleSleepTimer = false;
    bool suspend = false;
    bool resume = false;
    bool resumeLast = false;
    bool openFolders = false;
    bool addBookmark = false;
    bool jumpBookmark = false;
    int volumeDelta = 0;
    std::string textInput;
    bool eraseText = false;
    bool clearText = false;
    bool saveText = false;
    int reorder = 0;
    bool deleteItem = false;
    bool openBluetooth = false;
    bool toggleBluetooth = false;
    bool refreshBluetooth = false;
    bool toggleLibraryView = false;
    bool background = false;
};

struct BluetoothDevice {
    std::string address;
    std::string name;
    bool connected = false;
    bool active = false;
};

struct BluetoothState {
    bool available = false;
    bool powered = false;
    bool audioActive = false;
    std::vector<BluetoothDevice> devices;
};

enum class Screen { Collections, CollectionManager, CollectionType, CollectionName, CollectionDelete, CoverFlow, AlbumList, Tracks, Player, Folders, Bluetooth };
struct ViewModel {
    Screen screen = Screen::Collections;
    std::string title;
    std::string subtitle;
    std::vector<std::string> items;
    std::vector<std::string> itemImages;
    std::size_t selected = 0;
    std::string message;
    std::string coverPath;
    bool immediate = false;
};

class Platform {
public:
    virtual ~Platform() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual Capabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual InputActions pollEvents() = 0;
    virtual void setPlaybackStatus(bool active, bool paused, double positionSeconds, double durationSeconds) = 0;
    virtual void setView(ViewModel view) = 0;
    virtual void setSleepTimer(int minutes) = 0;
    virtual void setPlayerDetails(int volumePercent, std::size_t bookmarkCount, std::string notice, std::optional<int> batteryPercent) = 0;
    [[nodiscard]] virtual std::optional<int> systemVolumePercent() const = 0;
    // Remaining charge as a 0-100 percentage, or nullopt when no battery is
    // present or reachable (desktop builds, or a handheld reporting through
    // an unrecognized power-supply layout) - callers must treat that as
    // "hide the indicator", never as "0%".
    [[nodiscard]] virtual std::optional<int> batteryPercent() const { return std::nullopt; }
    virtual std::optional<int> adjustSystemVolume(int deltaPercent) = 0;
    [[nodiscard]] virtual BluetoothState bluetoothState() { return {}; }
    virtual bool setBluetoothEnabled(bool) { return false; }
    virtual bool setBluetoothDeviceConnected(const std::string&, bool) { return false; }

    // Spawns an independent, detached process that keeps playing
    // trackPaths[startIndex], then trackPaths[startIndex + 1], and so on
    // through the rest of the album, after this (GUI) process exits - so
    // playback continues once EmulationStation regains the screen (a game
    // was launched, or the user backed out), advancing through the same
    // track order the GUI would have used instead of stopping the instant
    // the single backgrounded track ends. The caller always performs a
    // completely normal shutdown afterward regardless of the return value;
    // this is a fire-and-forget request, not a mode this process itself
    // enters. Returns false when unsupported (desktop/Windows) or when the
    // spawn itself failed to even start - either way, playback simply
    // stops with this process, exactly like the plain quit gesture this
    // replaces.
    virtual bool enterBackgroundPlayback(const std::vector<std::string>& trackPaths, std::size_t startIndex) {
        static_cast<void>(trackPaths);
        static_cast<void>(startIndex);
        return false;
    }
};

} // namespace coverplayer::platform
