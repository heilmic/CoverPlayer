#pragma once

#include "coverplayer/platform/platform.hpp"

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include <array>
#include <string>
#include <vector>

namespace coverplayer::platform {

// Owns the SDL window/renderer/font lifecycle and every draw call. Knows
// nothing about controllers, keyboard input, or Linux shell integrations -
// it only turns a ViewModel plus a handful of status fields into pixels.
class SdlRenderer {
public:
    SdlRenderer();
    ~SdlRenderer();

    SdlRenderer(const SdlRenderer&) = delete;
    SdlRenderer& operator=(const SdlRenderer&) = delete;

    void setPlaybackStatus(bool active, bool paused, double positionSeconds, double durationSeconds);
    void setView(ViewModel view);
    void setSleepTimer(int minutes);
    void setPlayerDetails(int volumePercent, std::size_t bookmarkCount, std::string notice, std::optional<int> batteryPercent);
    void setHelpVisible(bool visible);
    void setBluetoothStatus(bool capable, bool audioActive);

    void present();

    [[nodiscard]] Screen currentScreen() const noexcept { return view_.screen; }

private:
    // Layout for one scrollable, selectable list: shared by every screen
    // that shows a cursor-navigable list, so row height, insets, and the
    // selection-marker style stay visually consistent across screens.
    struct ListLayout {
        int x;
        int width;
        int rowHeight;
        int firstY;
        std::size_t visibleRows;
        int maxCharacters;
        bool twoLine;
    };

    void renderHandheldUi();
    void renderCoverFlow();
    void drawCover(int x, int y, int width, int height);
    void drawCoverTexture(SDL_Texture* texture, int x, int y, int width, int height, Uint8 brightness = 255, double angle = 0.0);
    void drawPerspectiveCover(SDL_Texture* texture, float offset, Uint8 brightness, Uint8 alpha, bool reflection);
    void drawPlaybackSymbol(int centerX, int centerY, bool paused);
    void drawFittedText(const std::string& text, int x, int y, int maxCharacters, SDL_Color color, TTF_Font* font = nullptr);
    void drawText(const char* text, int x, int y, SDL_Color color, TTF_Font* font = nullptr);
    void drawSelectableList(const std::vector<std::string>& items, std::size_t selected, const ListLayout& layout);
    void drawBluetoothIcon(int x, int y);
    void drawBatteryIcon(int x, int y, int percent);
    void updateCover();
    void updateCoverFlowTextures();
    SDL_Texture* loadCoverTexture(const std::string& path);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    TTF_Font* titleFont_ = nullptr;

    bool playbackActive_ = false;
    bool playbackPaused_ = false;
    double playbackPositionSeconds_ = 0.0;
    double playbackDurationSeconds_ = 0.0;
    int sleepMinutes_ = 0;
    int volumePercent_ = 100;
    std::size_t bookmarkCount_ = 0;
    std::optional<int> batteryPercent_;
    std::string notice_;
    bool helpVisible_ = false;
    bool bluetoothCapable_ = false;
    bool bluetoothAudioActive_ = false;

    ViewModel view_;
    SDL_Texture* coverTexture_ = nullptr;
    std::string loadedCoverPath_;
    std::array<SDL_Texture*, 9> flowTextures_{};
    std::array<std::string, 9> loadedFlowPaths_{};
    bool flowAnimationActive_ = false;
    int flowAnimationDirection_ = 0;
    Uint32 flowAnimationStartedAt_ = 0;
};

} // namespace coverplayer::platform
