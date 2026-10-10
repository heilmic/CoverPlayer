#pragma once

#include "coverplayer/platform/platform.hpp"
#include "platform/sdl/cover_flow_motion.hpp"

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include <array>
#include <string>
#include <vector>
#include <future>
#include <map>

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
    void setLanguage(Language language);

    void present();
    void setDrawingEnabled(bool enabled) { drawingEnabled_ = enabled; }
    [[nodiscard]] bool coversLoading() const { return pendingCover_.valid(); }
    [[nodiscard]] bool animating() const { return std::abs(flowMotion_.offset()) > 0.001; }

    [[nodiscard]] Screen currentScreen() const noexcept { return view_.screen; }

private:
    bool drawingEnabled_ = true;
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
        bool middleEllipsis = false;
    };

    void renderHandheldUi();
    void renderCoverFlow();
    void drawCover(int x, int y, int width, int height);
    void drawCoverTexture(SDL_Texture* texture, int x, int y, int width, int height, Uint8 brightness = 255, double angle = 0.0);
    void drawPerspectiveCover(SDL_Texture* texture, float offset, Uint8 brightness, Uint8 alpha);
    void drawPlaybackSymbol(int centerX, int centerY, bool paused);
    void drawFittedText(const std::string& text, int x, int y, int maxCharacters, SDL_Color color, TTF_Font* font = nullptr);
    int drawWrappedText(const std::string& text, int x, int y, int width, int lineHeight, int maxLines, SDL_Color color, TTF_Font* font = nullptr);
    [[nodiscard]] int textCapacity(int width, TTF_Font* font = nullptr) const;
    void drawText(const char* text, int x, int y, SDL_Color color, TTF_Font* font = nullptr);
    void drawSelectableList(const std::vector<std::string>& items, std::size_t selected, const ListLayout& layout);
    void drawBluetoothIcon(int x, int y);
    void drawBatteryIcon(int x, int y, int percent);
    void updateCover();
    void updateCoverFlowTextures();
    float coverFlowOffset(Uint32 now);
    SDL_Texture* loadCoverTexture(const std::string& path);
    static SDL_Surface* decodeCover(const std::string& path);
    void collectCover();
    void trimCoverCache();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    // ABI layout of SDL_Vertex (not declared by the ARM build's SDL 2.0.14 headers).
    struct CoverVertex { SDL_FPoint position; SDL_Color color; SDL_FPoint tex_coord; };
    using RenderGeometry = int (SDLCALL *)(SDL_Renderer*, SDL_Texture*, const CoverVertex*, int, const int*, int);
    RenderGeometry renderGeometry_ = nullptr;
    void* geometryLibrary_ = nullptr;
    TTF_Font* font_ = nullptr;
    TTF_Font* titleFont_ = nullptr;
    int uiWidth_ = 640;

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
    Language language_ = Language::German;

    ViewModel view_;
    SDL_Texture* coverTexture_ = nullptr;
    std::string loadedCoverPath_;
    std::array<SDL_Texture*, 9> flowTextures_{};
    std::array<std::string, 9> loadedFlowPaths_{};
    CoverFlowMotion flowMotion_;
    Uint32 flowUpdatedAt_ = 0;
    struct CachedCover { SDL_Texture* texture; Uint64 used; };
    std::map<std::string, CachedCover> coverCache_;
    Uint64 coverUse_ = 0;
    std::future<SDL_Surface*> pendingCover_;
    std::string pendingCoverPath_;
    struct CachedText { SDL_Texture* texture; int width; int height; Uint64 used; };
    std::map<std::pair<TTF_Font*, std::string>, CachedText> textCache_;
    Uint64 textUse_ = 0;
};

} // namespace coverplayer::platform
