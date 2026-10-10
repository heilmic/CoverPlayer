#include "platform/sdl/sdl_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coverplayer::platform {
namespace {

// Counts UTF-8 codepoints (not bytes) so text fitting never splits a
// multi-byte sequence, which would otherwise corrupt the trailing
// character of a truncated German umlaut/ID3-tag string.
std::size_t utf8Length(const std::string& text) {
    std::size_t count = 0;
    for (const unsigned char character : text) if ((character & 0xC0U) != 0x80U) ++count;
    return count;
}

std::size_t utf8ByteOffsetOfCodepoint(const std::string& text, std::size_t codepointIndex) {
    std::size_t byteIndex = 0;
    std::size_t seen = 0;
    while (byteIndex < text.size() && seen < codepointIndex) {
        ++byteIndex;
        while (byteIndex < text.size() && (static_cast<unsigned char>(text[byteIndex]) & 0xC0U) == 0x80U) ++byteIndex;
        ++seen;
    }
    return byteIndex;
}

// The mint-green brand accent means "selected" everywhere (list cursor,
// CoverFlow underline, header accent). Playback/Bluetooth activity gets its
// own color so "this is on/playing" is never visually confused with "this
// is the cursor position".
constexpr SDL_Color kActiveAccent{86, 163, 255, 255};

struct HelpRow {
    const char* key = nullptr;
    const char* action = nullptr;
};

std::string formatTime(double value) {
    const int seconds = std::max(0, static_cast<int>(value));
    const int hours = seconds / 3600;
    const int minutes = (seconds / 60) % 60;
    const int remainder = seconds % 60;
    if (hours > 0) return std::to_string(hours) + ":" + (minutes < 10 ? "0" : "") +
        std::to_string(minutes) + ":" + (remainder < 10 ? "0" : "") + std::to_string(remainder);
    return std::to_string(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + std::to_string(remainder);
}

} // namespace

SdlRenderer::SdlRenderer() {
    if (TTF_Init() != 0) {
        throw std::runtime_error(std::string("SDL_ttf initialization failed: ") + TTF_GetError());
    }
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

    int windowWidth = 640;
    int windowHeight = 480;
    SDL_DisplayMode display{};
    const bool hasDisplayMode = SDL_GetCurrentDisplayMode(0, &display) == 0;
    const char* requestedWidth = SDL_getenv("COVERPLAYER_UI_WIDTH");
    if (requestedWidth != nullptr && std::string(requestedWidth) == "720") {
        uiWidth_ = windowWidth = 720;
    } else if (requestedWidth == nullptr && hasDisplayMode &&
        display.w >= 720 && display.w <= 960 && display.h >= 480 && display.h <= 640 &&
        std::abs(static_cast<float>(display.w) / display.h - 1.5F) < 0.03F) {
        // Use the available width on compact 3:2 handheld screens. The
        // original 640x480 design remains unchanged on 4:3 displays.
        uiWidth_ = 720;
        windowWidth = display.w;
        windowHeight = display.h;
    }

    window_ = SDL_CreateWindow(
        "CoverPlayer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth,
        windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        const std::string message = SDL_GetError();
        TTF_Quit();
        throw std::runtime_error("SDL window creation failed: " + message);
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    SDL_RenderSetLogicalSize(renderer_, uiWidth_, 480);
    SDL_Log("CoverPlayer UI: %dx480, window: %dx%d", uiWidth_, windowWidth, windowHeight);
    if (renderer_ == nullptr) {
        const std::string message = SDL_GetError();
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        TTF_Quit();
        throw std::runtime_error("SDL renderer creation failed: " + message);
    }

    font_ = TTF_OpenFont("assets/fonts/RobotoMono-Bold.ttf", 18);
    if (font_ == nullptr) {
        const std::string message = TTF_GetError();
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        renderer_ = nullptr;
        window_ = nullptr;
        TTF_Quit();
        throw std::runtime_error("Font loading failed: " + message);
    }
    titleFont_ = TTF_OpenFont("assets/fonts/RobotoMono-Bold.ttf", 26);
    if (titleFont_ == nullptr) {
        const std::string message = TTF_GetError();
        TTF_CloseFont(font_);
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        font_ = nullptr;
        renderer_ = nullptr;
        window_ = nullptr;
        TTF_Quit();
        throw std::runtime_error("Title font loading failed: " + message);
    }
    // The device can have a newer SDL than our build headers. Resolve its
    // batching API at runtime instead of permanently selecting the strip path.
#ifdef _WIN32
    geometryLibrary_ = SDL_LoadObject("SDL2.dll");
#else
    geometryLibrary_ = SDL_LoadObject("libSDL2-2.0.so.0");
#endif
    if (geometryLibrary_) renderGeometry_ = reinterpret_cast<RenderGeometry>(
        SDL_LoadFunction(geometryLibrary_, "SDL_RenderGeometry"));
    SDL_Log("CoverPlayer covers: %s", renderGeometry_ ? "batched geometry" : "strip fallback");
}

SdlRenderer::~SdlRenderer() {
    if (pendingCover_.valid()) SDL_FreeSurface(pendingCover_.get());
    for (const auto& entry : coverCache_) SDL_DestroyTexture(entry.second.texture);
    for (const auto& entry : textCache_) SDL_DestroyTexture(entry.second.texture);
    TTF_CloseFont(titleFont_);
    TTF_CloseFont(font_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    if (geometryLibrary_) SDL_UnloadObject(geometryLibrary_);
    TTF_Quit();
    IMG_Quit();
}

void SdlRenderer::setPlaybackStatus(bool active, bool paused, double positionSeconds, double durationSeconds) {
    playbackActive_ = active;
    playbackPaused_ = paused;
    playbackPositionSeconds_ = positionSeconds;
    playbackDurationSeconds_ = durationSeconds;
}

void SdlRenderer::setView(ViewModel view) {
    const Uint32 now = SDL_GetTicks();
    coverFlowOffset(now);
    const bool sameCollection = view.screen == Screen::CoverFlow && view_.screen == Screen::CoverFlow &&
        view.title == view_.title && view.itemImages == view_.itemImages && view.items.size() == view_.items.size();
    if (sameCollection) {
        flowMotion_.shift(static_cast<double>(view.selected) - static_cast<double>(view_.selected));
    } else {
        flowMotion_.reset();
    }
    view_ = std::move(view);
    if (view_.immediate) present();
}

void SdlRenderer::setSleepTimer(int minutes) { sleepMinutes_ = minutes; }

void SdlRenderer::setPlayerDetails(int volumePercent, std::size_t bookmarkCount, std::string notice, std::optional<int> batteryPercent) {
    volumePercent_ = volumePercent;
    bookmarkCount_ = bookmarkCount;
    notice_ = std::move(notice);
    batteryPercent_ = batteryPercent;
}

void SdlRenderer::setHelpVisible(bool visible) { helpVisible_ = visible; }
void SdlRenderer::setLanguage(Language language) { language_ = language; }

void SdlRenderer::setBluetoothStatus(bool capable, bool audioActive) {
    bluetoothCapable_ = capable;
    bluetoothAudioActive_ = audioActive;
}

void SdlRenderer::present() {
    if (!drawingEnabled_) return;
    collectCover();
    updateCover();
    updateCoverFlowTextures();
    trimCoverCache();
    renderHandheldUi();
}

void SdlRenderer::updateCover() {
    loadedCoverPath_ = view_.screen == Screen::CoverFlow ? std::string{} : view_.coverPath;
    coverTexture_ = loadCoverTexture(loadedCoverPath_);
}

SDL_Texture* SdlRenderer::loadCoverTexture(const std::string& path) {
    if (path.empty()) return nullptr;
    const auto found = coverCache_.find(path);
    if (found != coverCache_.end()) {
        found->second.used = ++coverUse_;
        return found->second.texture;
    }
    if (!pendingCover_.valid()) {
        pendingCoverPath_ = path;
        pendingCover_ = std::async(std::launch::async, [path] { return decodeCover(path); });
    }
    return nullptr;
}

void SdlRenderer::collectCover() {
    if (!pendingCover_.valid() || pendingCover_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    SDL_Surface* surface = pendingCover_.get();
    SDL_Texture* texture = surface ? SDL_CreateTextureFromSurface(renderer_, surface) : nullptr;
    if (texture) SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    SDL_FreeSurface(surface);
    // Cache failures too, so a missing cover never starts an endless decode loop.
    coverCache_[pendingCoverPath_] = {texture, ++coverUse_};
}

void SdlRenderer::trimCoverCache() {
    // At most ~14 MiB of RGBA artwork; retain recent covers for quick reversals.
    while (coverCache_.size() > 24) {
        auto oldest = coverCache_.end();
        for (auto it = coverCache_.begin(); it != coverCache_.end(); ++it) {
            if (it->first == loadedCoverPath_ ||
                std::find(loadedFlowPaths_.begin(), loadedFlowPaths_.end(), it->first) != loadedFlowPaths_.end()) continue;
            if (oldest == coverCache_.end() || it->second.used < oldest->second.used) oldest = it;
        }
        if (oldest == coverCache_.end()) break;
        SDL_DestroyTexture(oldest->second.texture);
        coverCache_.erase(oldest);
    }
}

SDL_Surface* SdlRenderer::decodeCover(const std::string& path) {
    SDL_Surface* source = IMG_Load(path.c_str());
    if (source == nullptr) return nullptr;
    constexpr int maximumSide = 384;
    const float scale = std::min(1.0F, std::min(
        static_cast<float>(maximumSide) / source->w,
        static_cast<float>(maximumSide) / source->h));
    const int width = std::max(1, static_cast<int>(source->w * scale));
    const int height = std::max(1, static_cast<int>(source->h * scale));
    SDL_Surface* resized = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (resized == nullptr) { SDL_FreeSurface(source); return nullptr; }
    // Area filtering preserves small lettering when reducing large artwork.
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
    if (rgba == nullptr) { SDL_FreeSurface(resized); SDL_FreeSurface(source); return nullptr; }
    SDL_LockSurface(rgba);
    SDL_LockSurface(resized);
    for (int y = 0; y < height; ++y) {
        const double top = double(y) * rgba->h / height;
        const double bottom = double(y + 1) * rgba->h / height;
        auto* output = static_cast<Uint8*>(resized->pixels) + y * resized->pitch;
        for (int x = 0; x < width; ++x) {
            const double left = double(x) * rgba->w / width;
            const double right = double(x + 1) * rgba->w / width;
            double channels[4]{};
            for (int sy = int(top); sy < int(std::ceil(bottom)); ++sy) {
                const double wy = std::min(bottom, double(sy + 1)) - std::max(top, double(sy));
                const auto* row = static_cast<const Uint8*>(rgba->pixels) + sy * rgba->pitch;
                for (int sx = int(left); sx < int(std::ceil(right)); ++sx) {
                    const double weight = wy * (std::min(right, double(sx + 1)) - std::max(left, double(sx)));
                    for (int c = 0; c < 4; ++c) channels[c] += row[sx * 4 + c] * weight;
                }
            }
            const double area = (right - left) * (bottom - top);
            for (int c = 0; c < 4; ++c) output[x * 4 + c] = static_cast<Uint8>(std::clamp(std::lround(channels[c] / area), 0L, 255L));
        }
    }
    SDL_UnlockSurface(resized);
    SDL_UnlockSurface(rgba);
    SDL_FreeSurface(rgba);
    SDL_FreeSurface(source);
    return resized;
}

void SdlRenderer::updateCoverFlowTextures() {
    constexpr long long centerSlot = 4;
    std::array<std::string, 9> desiredPaths{};
    for (std::size_t slot = 0; slot < flowTextures_.size(); ++slot) {
        if (view_.screen == Screen::CoverFlow && !view_.itemImages.empty()) {
            const auto count = static_cast<long long>(view_.itemImages.size());
            const auto offset = static_cast<long long>(slot) - centerSlot;
            const auto index = static_cast<long long>(view_.selected) + offset;
            if (index >= 0 && index < count) desiredPaths[slot] = view_.itemImages[static_cast<std::size_t>(index)];
        }
    }
    loadedFlowPaths_ = std::move(desiredPaths);
    // Decode the selection first, then immediate neighbors. Do not queue stale
    // work when the user scrolls quickly; one worker serves the current view.
    for (const int slot : {4, 3, 5, 2, 6, 1, 7, 0, 8})
        flowTextures_[slot] = loadCoverTexture(loadedFlowPaths_[slot]);
}

float SdlRenderer::coverFlowOffset(Uint32 now) {
    flowMotion_.advance(static_cast<double>(now - flowUpdatedAt_) / 1000.0);
    flowUpdatedAt_ = now;
    return static_cast<float>(flowMotion_.offset());
}

void SdlRenderer::drawText(const char* text, int x, int y, SDL_Color color, TTF_Font* font) {
    TTF_Font* activeFont = font != nullptr ? font : font_;
    const auto key = std::make_pair(activeFont, std::string(text));
    auto found = textCache_.find(key);
    if (found == textCache_.end()) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(activeFont, text, SDL_Color{255, 255, 255, 255});
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        const int width = surface->w, height = surface->h;
        SDL_FreeSurface(surface);
        if (!texture) return;
        if (textCache_.size() >= 64) {
            const auto oldest = std::min_element(textCache_.begin(), textCache_.end(),
                [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
            SDL_DestroyTexture(oldest->second.texture);
            textCache_.erase(oldest);
        }
        found = textCache_.emplace(key, CachedText{texture, width, height, 0}).first;
    }
    auto& cached = found->second;
    cached.used = ++textUse_;
    SDL_SetTextureColorMod(cached.texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(cached.texture, color.a);
    const SDL_Rect destination{x, y, cached.width, cached.height};
    SDL_RenderCopy(renderer_, cached.texture, nullptr, &destination);
}

void SdlRenderer::drawFittedText(const std::string& text, int x, int y, int maxCharacters, SDL_Color color, TTF_Font* font) {
    std::string fitted = text;
    if (static_cast<int>(utf8Length(fitted)) > maxCharacters) {
        const auto keep = static_cast<std::size_t>(std::max(1, maxCharacters - 3));
        fitted = fitted.substr(0, utf8ByteOffsetOfCodepoint(fitted, keep)) + "...";
    }
    drawText(fitted.c_str(), x, y, color, font);
}

int SdlRenderer::textCapacity(int width, TTF_Font* font) const {
    int glyphWidth = 0;
    int glyphHeight = 0;
    TTF_SizeUTF8(font != nullptr ? font : font_, "M", &glyphWidth, &glyphHeight);
    return std::max(1, width / std::max(1, glyphWidth));
}

int SdlRenderer::drawWrappedText(const std::string& text, int x, int y, int width, int lineHeight, int maxLines, SDL_Color color, TTF_Font* font) {
    const int capacity = textCapacity(width, font);
    std::string remainder = text;
    for (int line = 0; line < maxLines; ++line) {
        if (static_cast<int>(utf8Length(remainder)) <= capacity) {
            drawText(remainder.c_str(), x, y + line * lineHeight, color, font);
            return line + 1;
        }
        if (line == maxLines - 1) {
            if (capacity > 6) {
                // Keep the end of long filenames: it often distinguishes
                // audiobook parts whose opening words are identical.
                const std::size_t tailStart = utf8ByteOffsetOfCodepoint(remainder, utf8Length(remainder) - static_cast<std::size_t>(capacity - 3));
                remainder = "..." + remainder.substr(tailStart);
            }
            drawFittedText(remainder, x, y + line * lineHeight, capacity, color, font);
            return line + 1;
        }
        std::size_t split = utf8ByteOffsetOfCodepoint(remainder, static_cast<std::size_t>(capacity));
        const std::size_t wordBreak = remainder.rfind(' ', split);
        if (wordBreak != std::string::npos && wordBreak >= utf8ByteOffsetOfCodepoint(remainder, static_cast<std::size_t>(capacity / 2))) {
            split = wordBreak;
        }
        drawText(remainder.substr(0, split).c_str(), x, y + line * lineHeight, color, font);
        remainder.erase(0, split);
        remainder.erase(0, remainder.find_first_not_of(' '));
    }
    return maxLines;
}

void SdlRenderer::drawCover(int x, int y, int width, int height) {
    drawCoverTexture(coverTexture_, x, y, width, height);
}

void SdlRenderer::drawCoverTexture(SDL_Texture* texture, int x, int y, int width, int height, Uint8 brightness, double angle) {
    SDL_SetRenderDrawColor(renderer_,10,13,20,255);const SDL_Rect frame{x-5,y-5,width+10,height+10};SDL_RenderFillRect(renderer_,&frame);
    if(texture==nullptr){SDL_SetRenderDrawColor(renderer_,43,50,64,255);const SDL_Rect empty{x,y,width,height};SDL_RenderFillRect(renderer_,&empty);if(width>=180)drawText(tr(language_,"NO COVER"),x+width/2-50,y+height/2-10,SDL_Color{130,140,155,255});return;}
    int sourceWidth=0,sourceHeight=0;SDL_QueryTexture(texture,nullptr,nullptr,&sourceWidth,&sourceHeight);if(sourceWidth<=0||sourceHeight<=0)return;
    const float scale=std::min(static_cast<float>(width)/sourceWidth,static_cast<float>(height)/sourceHeight);const int w=static_cast<int>(sourceWidth*scale),h=static_cast<int>(sourceHeight*scale);const SDL_Rect destination{x+(width-w)/2,y+(height-h)/2,w,h};
    SDL_SetTextureColorMod(texture,brightness,brightness,brightness);SDL_RenderCopyEx(renderer_,texture,nullptr,&destination,angle,nullptr,SDL_FLIP_NONE);SDL_SetTextureColorMod(texture,255,255,255);
}

void SdlRenderer::drawPerspectiveCover(SDL_Texture* texture, float offset, Uint8 brightness, Uint8 alpha) {
    const float distance = std::min(5.0F, std::abs(offset));
    const float sideDistance = std::min(1.0F, distance);
    const float direction = offset < 0.0F ? -1.0F : 1.0F;
    // A rotating plane under a pinhole camera. Continuous at the middle
    // and first side slot, without the old pow() acceleration kink.
    const float travel = (distance <= 1.0F
        ? 158.0F * distance + 110.0F * distance * distance * (1.0F - distance)
        : 158.0F + (distance - 1.0F) * 48.0F) * static_cast<float>(uiWidth_) / 640.0F;
    const float centerX = static_cast<float>(uiWidth_) * 0.5F + direction * travel;
    const float smoothTurn = sideDistance * sideDistance * (3.0F - 2.0F * sideDistance);
    const float angle = direction * smoothTurn * 0.98F;
    const float depth = 1.0F + 0.13F * distance;
    const float halfSize = view_.items.size() == 1 ? 132.0F : 128.0F;
    constexpr float focalLength = 520.0F;
    constexpr float centerY = 214.0F;
    const auto project = [&](float u) {
        const float localX = (u * 2.0F - 1.0F) * halfSize;
        const float scale = focalLength / (focalLength * depth - localX * std::sin(angle));
        return std::array<float, 3>{centerX + localX * std::cos(angle) * scale,
            centerY - halfSize * scale, centerY + halfSize * scale};
    };
    const auto leftEdge = project(0.0F);
    const auto rightEdge = project(1.0F);
    const float left = leftEdge[0], right = rightEdge[0];
    const float topLeft = leftEdge[1], topRight = rightEdge[1];
    const float bottomLeft = leftEdge[2], bottomRight = rightEdge[2];
    const float visibleWidth = right - left;

    if (texture == nullptr) {
        SDL_SetRenderDrawColor(renderer_, 38, 45, 57, alpha);
        const int columns = std::max(1, static_cast<int>(right - left));
        for (int column = 0; column <= columns; ++column) {
            const float t = static_cast<float>(column) / columns;
            const int x = static_cast<int>(left + (right - left) * t);
            const int y1 = static_cast<int>(topLeft + (topRight - topLeft) * t);
            const int y2 = static_cast<int>(bottomLeft + (bottomRight - bottomLeft) * t);
            SDL_RenderDrawLine(renderer_, x, y1, x, y2);
        }
        return;
    }

    int sourceWidth = 0;
    int sourceHeight = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &sourceWidth, &sourceHeight);
    if (sourceWidth <= 0 || sourceHeight <= 0) return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(texture, brightness, brightness, brightness);
    SDL_SetTextureAlphaMod(texture, alpha);

    if (distance < 0.001F) {
        const SDL_Rect target{int(std::lround(left)), int(std::lround(topLeft)),
            int(std::lround(visibleWidth)), int(std::lround(bottomLeft - topLeft))};
        SDL_RenderCopy(renderer_, texture, nullptr, &target);
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        return;
    }

    if (renderGeometry_) {
    // Small subdivided mesh: approximate perspective-correct UVs without a
    // custom OpenGL context. One SDL draw per cover instead of ~200 strips.
    constexpr int strips = 24;
    std::array<CoverVertex, (strips + 1) * 2> vertices{};
    std::array<int, strips * 6> indices{};
    for (int strip = 0; strip <= strips; ++strip) {
        const float u = static_cast<float>(strip) / strips;
        const auto point = project(u);
        float top = point[1], bottom = point[2];
        vertices[strip * 2] = {{point[0], top}, {brightness, brightness, brightness, alpha}, {u, 0.0F}};
        vertices[strip * 2 + 1] = {{point[0], bottom},
            {brightness, brightness, brightness, alpha}, {u, 1.0F}};
        if (strip < strips) {
            const int base = strip * 2;
            const std::array<int, 6> quad{base, base + 1, base + 2, base + 2, base + 1, base + 3};
            std::copy(quad.begin(), quad.end(), indices.begin() + strip * 6);
        }
    }
    if (renderGeometry_(renderer_, texture, vertices.data(), static_cast<int>(vertices.size()),
            indices.data(), static_cast<int>(indices.size())) == 0) {
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        return;
    }
    }
    // Older SDL versions/backends retain the scanline fallback.
    const int sliceCount = std::min(sourceWidth, std::max(32, static_cast<int>(visibleWidth)));
    for (int slice = 0; slice < sliceCount; ++slice) {
        const float t0 = static_cast<float>(slice) / sliceCount;
        const float t1 = static_cast<float>(slice + 1) / sliceCount;
        const int sourceX0 = static_cast<int>(sourceWidth * t0);
        const int sourceX1 = std::max(sourceX0 + 1, static_cast<int>(sourceWidth * t1));
        SDL_Rect source{sourceX0, 0, std::min(sourceWidth, sourceX1) - sourceX0, sourceHeight};
        const auto start = project(t0);
        const auto end = project(t1);
        const float x0 = start[0], x1 = end[0];
        float y0 = start[1];
        float y1 = start[2];
        SDL_Rect destination{
            static_cast<int>(std::floor(x0)), static_cast<int>(std::floor(y0)),
            std::max(1, static_cast<int>(std::ceil(x1)) - static_cast<int>(std::floor(x0))),
            std::max(1, static_cast<int>(std::ceil(y1 - y0)))};
        SDL_RenderCopyEx(renderer_, texture, &source, &destination, 0.0, nullptr,
            SDL_FLIP_NONE);
    }
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_SetTextureColorMod(texture, 255, 255, 255);
}

void SdlRenderer::renderCoverFlow() {
    if (view_.items.empty()) {
        drawFittedText(view_.message, 100, 225, 50, SDL_Color{143,154,170,255});
        return;
    }
    const auto count = view_.items.size();
    const Uint32 now = SDL_GetTicks();
    const float animationOffset = coverFlowOffset(now);
    constexpr int centerSlot = 4;
    std::vector<int> slots;
    for (int slot = 0; slot < static_cast<int>(flowTextures_.size()); ++slot) {
        const auto index = static_cast<long long>(view_.selected) + slot - centerSlot;
        if (index >= 0 && index < static_cast<long long>(count)) slots.push_back(slot);
    }
    std::sort(slots.begin(), slots.end(), [&](int leftSlot, int rightSlot) {
        return std::abs(static_cast<float>(leftSlot - centerSlot) + animationOffset) >
            std::abs(static_cast<float>(rightSlot - centerSlot) + animationOffset);
    });

    const SDL_Rect artworkArea{0, 70, uiWidth_, 288};
    SDL_RenderSetClipRect(renderer_, &artworkArea);
    for (const int slot : slots) {
        const float offset = static_cast<float>(slot - centerSlot) + animationOffset;
        const Uint8 brightness = static_cast<Uint8>(std::clamp(255.0F - std::abs(offset) * 42.0F, 100.0F, 255.0F));
        drawPerspectiveCover(flowTextures_[static_cast<std::size_t>(slot)], offset, brightness, 255);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    const auto captionIndex = static_cast<std::size_t>(std::clamp(
        std::lround(static_cast<float>(view_.selected) - animationOffset), 0L,
        static_cast<long>(count - 1)));
    const auto separator = view_.items[captionIndex].find('|');
    const auto name = view_.items[captionIndex].substr(0, separator);
    const auto detail = separator == std::string::npos ? std::string{} : view_.items[captionIndex].substr(separator + 1);
    int textWidth = 0, textHeight = 0;
    TTF_SizeUTF8(titleFont_, name.c_str(), &textWidth, &textHeight);
    drawFittedText(name, std::max(20, (uiWidth_ - textWidth) / 2), 362, 38 + (uiWidth_ - 640) / 14, SDL_Color{244,247,251,255}, titleFont_);
    const std::string meta = detail + "    " + std::to_string(captionIndex + 1) + "/" + std::to_string(count);
    TTF_SizeUTF8(font_, meta.c_str(), &textWidth, &textHeight);
    drawText(meta.c_str(), std::max(20, (uiWidth_ - textWidth) / 2), 394, SDL_Color{143,154,170,255});
}

void SdlRenderer::drawPlaybackSymbol(int centerX, int centerY, bool paused) {
    SDL_SetRenderDrawColor(renderer_, kActiveAccent.r, kActiveAccent.g, kActiveAccent.b, kActiveAccent.a);
    const SDL_Rect button{centerX - 24, centerY - 24, 48, 48};
    SDL_RenderFillRect(renderer_, &button);
    SDL_SetRenderDrawColor(renderer_, 10, 18, 20, 255);
    if (paused) {
        for (int row = -11; row <= 11; ++row) SDL_RenderDrawLine(renderer_, centerX - 7, centerY + row, centerX + 11, centerY);
    } else {
        const SDL_Rect left{centerX - 8, centerY - 11, 6, 22};
        const SDL_Rect right{centerX + 4, centerY - 11, 6, 22};
        SDL_RenderFillRect(renderer_, &left); SDL_RenderFillRect(renderer_, &right);
    }
}

void SdlRenderer::drawSelectableList(const std::vector<std::string>& items, std::size_t selected, const ListLayout& layout) {
    if (items.empty() || layout.visibleRows == 0) return;
    const std::size_t offset = (layout.visibleRows - 1) / 2;
    std::size_t first = selected > offset ? selected - offset : 0;
    if (first + layout.visibleRows > items.size()) {
        first = items.size() > layout.visibleRows ? items.size() - layout.visibleRows : 0;
    }
    const int highlightHeight = layout.rowHeight - (layout.twoLine ? 4 : 2);
    for (std::size_t row = 0; row < layout.visibleRows && first + row < items.size(); ++row) {
        const auto index = first + row;
        const int y = layout.firstY + static_cast<int>(row) * layout.rowHeight;
        const bool isSelected = index == selected;
        if (isSelected) {
            SDL_SetRenderDrawColor(renderer_, 45, 74, 67, 255);
            const SDL_Rect highlight{layout.x, y - 4, layout.width, highlightHeight};
            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 92, 211, 151, 255);
            const SDL_Rect marker{layout.x, y - 4, 4, highlightHeight};
            SDL_RenderFillRect(renderer_, &marker);
        }
        const SDL_Color primaryColor = isSelected ? SDL_Color{244, 247, 251, 255} : SDL_Color{158, 168, 184, 255};
        std::string name = items[index];
        std::string detail;
        if (layout.twoLine) {
            const auto separator = items[index].find('|');
            name = items[index].substr(0, separator);
            detail = separator == std::string::npos ? std::string{} : items[index].substr(separator + 1);
        }
        if (layout.middleEllipsis && static_cast<int>(utf8Length(name)) > layout.maxCharacters && layout.maxCharacters > 8) {
            const std::size_t headCount = static_cast<std::size_t>((layout.maxCharacters - 3) * 2 / 3);
            const std::size_t tailCount = static_cast<std::size_t>(layout.maxCharacters - 3) - headCount;
            name = name.substr(0, utf8ByteOffsetOfCodepoint(name, headCount)) + "..." +
                name.substr(utf8ByteOffsetOfCodepoint(name, utf8Length(name) - tailCount));
        }
        drawFittedText(name, layout.x + 16, y, layout.maxCharacters, primaryColor);
        if (layout.twoLine && !detail.empty()) {
            drawFittedText(detail, layout.x + 16, y + 20, layout.maxCharacters, SDL_Color{125, 137, 154, 255});
        }
    }
}

void SdlRenderer::drawBluetoothIcon(int x, int y) {
    if (!bluetoothAudioActive_) return;
    SDL_SetRenderDrawColor(renderer_, kActiveAccent.r, kActiveAccent.g, kActiveAccent.b, kActiveAccent.a);
    SDL_RenderDrawLine(renderer_, x + 8, y, x + 8, y + 28);
    SDL_RenderDrawLine(renderer_, x + 8, y, x + 17, y + 8);
    SDL_RenderDrawLine(renderer_, x + 17, y + 8, x + 2, y + 21);
    SDL_RenderDrawLine(renderer_, x + 2, y + 7, x + 17, y + 20);
    SDL_RenderDrawLine(renderer_, x + 17, y + 20, x + 8, y + 28);
}

void SdlRenderer::drawBatteryIcon(int x, int y, int percent) {
    // A low charge is the one header status worth calling out in a warning
    // color - everything else here (Bluetooth, volume, sleep) is neutral
    // informational text, but running out of battery mid-audiobook is the
    // one condition a handheld user actually needs to react to.
    const SDL_Color outline = percent <= 15 ? SDL_Color{224, 96, 96, 255} : SDL_Color{171, 181, 196, 255};
    const SDL_Color fill = percent <= 15 ? SDL_Color{224, 96, 96, 255} : SDL_Color{92, 211, 151, 255};
    const SDL_Rect body{x, y, 21, 15};
    const SDL_Rect interior{x + 2, y + 2, 17, 11};
    const SDL_Rect nub{x + 21, y + 4, 3, 7};
    SDL_SetRenderDrawColor(renderer_, outline.r, outline.g, outline.b, outline.a);
    SDL_RenderFillRect(renderer_, &body);
    SDL_RenderFillRect(renderer_, &nub);
    SDL_SetRenderDrawColor(renderer_, 24, 30, 42, 255);
    SDL_RenderFillRect(renderer_, &interior);
    const int fillWidth = std::clamp((percent * 15 + 99) / 100, 0, 15);
    if (fillWidth > 0) {
        const SDL_Rect level{x + 3, y + 3, fillWidth, 9};
        SDL_SetRenderDrawColor(renderer_, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer_, &level);
    }
    // A single cut pixel at each outer corner keeps the compact icon from
    // looking like an unstyled rectangle at the handheld's low resolution.
    SDL_SetRenderDrawColor(renderer_, 24, 30, 42, 255);
    SDL_RenderDrawPoint(renderer_, x, y);
    SDL_RenderDrawPoint(renderer_, x + 20, y);
    SDL_RenderDrawPoint(renderer_, x, y + 14);
    SDL_RenderDrawPoint(renderer_, x + 20, y + 14);
}

void SdlRenderer::renderHandheldUi() {
    const int extraWidth = uiWidth_ - 640;
    SDL_SetRenderDrawColor(renderer_,12,15,22,255);SDL_RenderClear(renderer_);
    SDL_SetRenderDrawColor(renderer_,24,30,42,255);const SDL_Rect header{0,0,uiWidth_,70};SDL_RenderFillRect(renderer_,&header);
    SDL_SetRenderDrawColor(renderer_,92,211,151,255);const SDL_Rect accent{0,68,uiWidth_,3};SDL_RenderFillRect(renderer_,&accent);
    drawText("COVER",20,20,SDL_Color{92,211,151,255});drawText("PLAYER",86,20,SDL_Color{244,247,251,255});
    const char* section = "JETZT LAEUFT";
    if (view_.screen == Screen::Collections) section = "BIBLIOTHEK";
    else if (view_.screen == Screen::CollectionManager) section = "SAMMLUNGEN";
    else if (view_.screen == Screen::CollectionType) section = "TYP";
    else if (view_.screen == Screen::CollectionName) section = "NAME";
    else if (view_.screen == Screen::CollectionDelete) section = "LOESCHEN";
    else if (view_.screen == Screen::CoverFlow) section = "COVERFLOW";
    else if (view_.screen == Screen::AlbumList) section = "ALBUMLISTE";
    else if (view_.screen == Screen::Tracks) section = "TITEL";
    else if (view_.screen == Screen::Folders) section = "ORDNER";
    else if (view_.screen == Screen::Bluetooth) section = "BLUETOOTH";
    if(view_.screen==Screen::Player){
        drawText(tr(language_,section),252+extraWidth,22,SDL_Color{155,166,184,255});drawBluetoothIcon(400+extraWidth,20);
        const std::string volume="VOL "+std::to_string(volumePercent_)+"%";drawText(volume.c_str(),430+extraWidth,22,SDL_Color{203,210,220,255});
        if(batteryPercent_){drawBatteryIcon(528+extraWidth,24,*batteryPercent_);const std::string battery=std::to_string(*batteryPercent_)+"%";drawText(battery.c_str(),558+extraWidth,22,SDL_Color{203,210,220,255});}
        if(sleepMinutes_>0){const std::string sleep=std::to_string(sleepMinutes_)+"m";drawText(sleep.c_str(),607+extraWidth,22,SDL_Color{242,190,92,255});}
    } else {
        drawBluetoothIcon(433+extraWidth,20);
        // The section label and the sleep badge each get a fixed slot so
        // neither hides the other - previously the sleep timer replaced the
        // screen name entirely, which lost the "where am I" context.
        drawFittedText(tr(language_,section),458+extraWidth,22,12,SDL_Color{155,166,184,255});
        if(sleepMinutes_>0){const std::string sleep=std::to_string(sleepMinutes_)+"m";drawText(sleep.c_str(),606+extraWidth,22,SDL_Color{242,190,92,255});}
    }

    if(view_.screen==Screen::CoverFlow){
        renderCoverFlow();
    } else if(view_.screen==Screen::Player){
        // Keep the cover prominent and wrap the track details within the
        // remaining width, including on the narrower 640px handheld.
        drawCover(25,100,225,225);
        const int detailX=275, detailWidth=uiWidth_-detailX-24;
        // Switch sooner than the raw three-line capacity: breaking on words
        // can otherwise elide an important word even when the character
        // count alone suggests it should fit.
        TTF_Font* trackFont=utf8Length(view_.title)>static_cast<std::size_t>(2*textCapacity(detailWidth,titleFont_))?font_:titleFont_;
        const int titleLineHeight=trackFont==titleFont_?32:25;
        const int titleLines=drawWrappedText(view_.title,detailX,100,detailWidth,titleLineHeight,3,SDL_Color{244,247,251,255},trackFont);
        const int subtitleY=100+titleLines*titleLineHeight+12;
        const int subtitleLines=drawWrappedText(view_.subtitle,detailX,subtitleY,detailWidth,24,2,SDL_Color{143,154,170,255});
        const int bookmarkY=subtitleY+subtitleLines*24+8;
        drawText((std::string(tr(language_,"LESEZEICHEN"))+"  "+std::to_string(bookmarkCount_)).c_str(),detailX,bookmarkY,SDL_Color{143,154,170,255});
        const int statusY=bookmarkY+28;
        if(!view_.message.empty())drawFittedText(view_.message,detailX,statusY,textCapacity(detailWidth),SDL_Color{242,118,109,255});
        else if(!notice_.empty())drawFittedText(notice_,detailX,statusY,textCapacity(detailWidth),SDL_Color{242,190,92,255});

        // The rail is a clean, uninterrupted bar; the play/pause symbol gets
        // its own row below it instead of sitting on top of the rail, with
        // the elapsed/remaining time flanking it on the same row.
        const int railX=24, railY=335, railWidth=uiWidth_-48;
        SDL_SetRenderDrawColor(renderer_,42,49,62,255);const SDL_Rect rail{railX,railY,railWidth,8};SDL_RenderFillRect(renderer_,&rail);
        if(playbackDurationSeconds_>0.0){const auto progress=std::min(1.0,playbackPositionSeconds_/playbackDurationSeconds_);SDL_SetRenderDrawColor(renderer_,kActiveAccent.r,kActiveAccent.g,kActiveAccent.b,kActiveAccent.a);const SDL_Rect filled{railX,railY,static_cast<int>(railWidth*progress),8};SDL_RenderFillRect(renderer_,&filled);}
        const int transportY=375;
        drawPlaybackSymbol(uiWidth_/2,transportY,playbackPaused_);
        drawText(formatTime(playbackPositionSeconds_).c_str(),railX,transportY-11,SDL_Color{203,210,220,255});
        const auto duration=playbackDurationSeconds_>0.0?formatTime(playbackDurationSeconds_):"--:--";int durationWidth=0,durationHeight=0;TTF_SizeUTF8(font_,duration.c_str(),&durationWidth,&durationHeight);drawText(duration.c_str(),uiWidth_-24-durationWidth,transportY-11,SDL_Color{203,210,220,255});
    } else if(view_.screen==Screen::Collections||view_.screen==Screen::AlbumList) {
        drawFittedText(view_.title,24,82,32+extraWidth/16,SDL_Color{244,247,251,255},titleFont_);
        drawCover(24,126,122,122);
        drawSelectableList(view_.items,view_.selected,{166,uiWidth_-190,49,120,6,39+extraWidth/11,true});
        if(!view_.message.empty())drawFittedText(view_.message,24,391,58+extraWidth/11,SDL_Color{143,154,170,255});
    } else if(view_.screen==Screen::CollectionManager) {
        drawFittedText(view_.title,24,82,30+extraWidth/16,SDL_Color{244,247,251,255},titleFont_);
        if(!view_.coverPath.empty())drawCover(uiWidth_-140,124,104,104);
        drawSelectableList(view_.items,view_.selected,{24,uiWidth_-188,43,126,6,38+extraWidth/11,true});
        if(!view_.message.empty())drawFittedText(view_.message,24,389,57+extraWidth/11,SDL_Color{143,154,170,255});
    } else if(view_.screen==Screen::Bluetooth) {
        drawFittedText(view_.title,24,84,30+extraWidth/16,SDL_Color{244,247,251,255},titleFont_);
        drawFittedText(view_.subtitle,24,132,60+extraWidth/11,SDL_Color{143,154,170,255});
        drawSelectableList(view_.items,view_.selected,{24,uiWidth_-48,31,184,7,57+extraWidth/11,false});
        if(!view_.message.empty())drawFittedText(view_.message,24,397,64+extraWidth/11,SDL_Color{242,190,92,255});
    } else if(view_.screen==Screen::CollectionName) {
        drawText(tr(language_,"NAME DER SAMMLUNG"),24,94,SDL_Color{143,154,170,255});
        SDL_SetRenderDrawColor(renderer_,31,38,50,255);const SDL_Rect nameField{24,121,uiWidth_-48,42};SDL_RenderFillRect(renderer_,&nameField);
        SDL_SetRenderDrawColor(renderer_,92,211,151,255);const SDL_Rect nameMarker{24,121,4,42};SDL_RenderFillRect(renderer_,&nameMarker);
        drawFittedText(view_.subtitle,40,131,52+extraWidth/11,SDL_Color{244,247,251,255});
        for(std::size_t index=0;index<view_.items.size();++index){
            const int column=static_cast<int>(index%10),row=static_cast<int>(index/10);
            const int cellWidth=(uiWidth_-48)/10;
            const int x=24+column*cellWidth,y=177+row*31;
            if(index==view_.selected){SDL_SetRenderDrawColor(renderer_,45,82,70,255);const SDL_Rect selected{x,y,cellWidth-5,27};SDL_RenderFillRect(renderer_,&selected);}
            const auto& key=view_.items[index];
            drawFittedText(key,x+(key.size()>1?3:(cellWidth-19)/2),y+4,6,index==view_.selected?SDL_Color{244,247,251,255}:SDL_Color{158,168,184,255});
        }
        if(!view_.message.empty())drawFittedText(view_.message,24,401,65+extraWidth/11,SDL_Color{242,190,92,255});
    } else if(view_.screen==Screen::Tracks) {
        // Keep the album artwork as context, then give every track the full
        // 640/720px list width. The selected filename is also shown above
        // the list on two lines so similar long names remain distinguishable.
        if(!view_.coverPath.empty()) drawCover(25,88,84,84);
        const int headingX=view_.coverPath.empty()?24:130;
        drawFittedText(view_.title,headingX,88,textCapacity(uiWidth_-headingX-24,titleFont_),SDL_Color{244,247,251,255},titleFont_);
        if(view_.selected<view_.items.size()) {
            drawWrappedText(view_.items[view_.selected],headingX,128,uiWidth_-headingX-24,24,2,SDL_Color{203,210,220,255});
        }
        drawSelectableList(view_.items,view_.selected,{24,uiWidth_-48,31,199,7,textCapacity(uiWidth_-80),false,true});
        if(!view_.message.empty())drawFittedText(view_.message,headingX,174,textCapacity(uiWidth_-headingX-24),SDL_Color{143,154,170,255});
    } else {
        const bool hasCover=!view_.coverPath.empty();const int listX=hasCover?272:24;const int listWidth=hasCover?uiWidth_-294:uiWidth_-48;const int maxCharacters=(hasCover?20:37)+extraWidth/11;
        if(hasCover) drawCover(25,100,225,225);
        drawFittedText(view_.title,hasCover?280:24,88,(hasCover?20:37)+extraWidth/16,SDL_Color{244,247,251,255},titleFont_);
        drawSelectableList(view_.items,view_.selected,{listX,listWidth,31,134,9,maxCharacters,false});
        if(!view_.message.empty())drawFittedText(view_.message,25,345,65+extraWidth/11,SDL_Color{143,154,170,255});
    }
    SDL_SetRenderDrawColor(renderer_,19,23,32,255);const SDL_Rect footer{0,423,uiWidth_,57};SDL_RenderFillRect(renderer_,&footer);
    const char* primaryHint = "A OEFFNEN   Y VERWALTEN   X SCANNEN";
    switch (view_.screen) {
        case Screen::Player: primaryHint = "A PLAY/PAUSE   B ZURUECK"; break;
        case Screen::Folders: primaryHint = "A OEFFNEN   Y ORDNER WAEHLEN"; break;
        case Screen::CollectionManager: primaryHint = "A BEARBEITEN   Y PFAD   B ZURUECK"; break;
        case Screen::CollectionType: primaryHint = "A TYP WAEHLEN   B ZURUECK"; break;
        case Screen::CollectionName: primaryHint = "A ZEICHEN   Y SPEICHERN   B ABBRECHEN"; break;
        case Screen::CollectionDelete: primaryHint = "A LOESCHEN   B ABBRECHEN"; break;
        case Screen::Bluetooth: primaryHint = "A VERBINDEN   X AN/AUS   B ZURUECK"; break;
        case Screen::CoverFlow: primaryHint = "A OEFFNEN   Y LISTE   B ZURUECK"; break;
        case Screen::AlbumList: primaryHint = "A OEFFNEN   Y COVERFLOW   B ZURUECK"; break;
        case Screen::Tracks: primaryHint = "A ABSPIELEN   B ZURUECK"; break;
        default: break;
    }
    drawFittedText(tr(language_,primaryHint),18,view_.screen==Screen::Player?427:442,40+extraWidth/11,SDL_Color{171,181,196,255});
    if (view_.screen==Screen::Player)
        drawFittedText(tr(language_,"START KURZ SLEEP / LANG HINTERGRUND"),18,451,40+extraWidth/11,SDL_Color{171,181,196,255});
    SDL_SetRenderDrawColor(renderer_,37,71,61,255);const SDL_Rect helpBadge{uiWidth_-162,430,146,38};SDL_RenderFillRect(renderer_,&helpBadge);
    drawText(tr(language_,"SELECT HILFE"),uiWidth_-156,440,SDL_Color{232,247,239,255});
    if(helpVisible_){
        SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer_,7,9,14,255);
        const SDL_Rect backdrop{0,70,uiWidth_,410};SDL_RenderFillRect(renderer_,&backdrop);
        SDL_SetRenderDrawColor(renderer_,19,25,34,255);
        const SDL_Rect panel{24,76,uiWidth_-48,342};SDL_RenderFillRect(renderer_,&panel);
        drawText(tr(language_,"BEDIENUNG"),44,88,SDL_Color{92,211,151,255});
        drawText(tr(language_,"TASTE"),58,121,SDL_Color{143,154,170,255});
        drawText(tr(language_,"AKTION"),220,121,SDL_Color{143,154,170,255});
        SDL_SetRenderDrawColor(renderer_,51,61,73,255);
        SDL_RenderDrawLine(renderer_,44,145,uiWidth_-44,145);
        std::array<HelpRow,7> rows{};
        const char* note = view_.screen == Screen::Player ? "Linker Stick = Steuerkreuz"
            : "Kreuz / linker Stick: halten = schneller";
        switch (view_.screen) {
            case Screen::Player: rows={{{"A","Wiedergabe / Pause"},{"LINKS/RECHTS","10 Sek. spulen"},{"L1/R1","30 Sek. spulen"},{"OBEN/UNTEN","Titel wechseln"},{"Y / X","Lesezeichen setzen / naechstes"},{"START KURZ","Sleep-Timer"},{"START 2s","Hintergrundwiedergabe"}}}; break;
            case Screen::Collections: rows={{{"A","Sammlung oeffnen"},{"STEUERKREUZ","Sammlung waehlen"},{"Y","Sammlungen verwalten"},{"X","Bibliothek scannen"},{"B","App bleibt geoeffnet"}}}; break;
            case Screen::CollectionManager: rows={{{"A","Typ und Namen bearbeiten"},{"Y","Pfad bearbeiten / neu anlegen"},{"X","Sammlung loeschen"},{"L1/R1","Reihenfolge verschieben"},{"B","Zurueck zur Bibliothek"}}}; break;
            case Screen::CollectionType: rows={{{"A","Sammlungstyp waehlen"},{"STEUERKREUZ","Typ auswaehlen"},{"B","Zurueck"}}}; break;
            case Screen::CollectionDelete: rows={{{"A","Sammlung entfernen"},{"B","Abbrechen"}}}; note="Mediendateien werden nie geloescht"; break;
            case Screen::CollectionName: rows={{{"A","Zeichen anfuegen"},{"X","Letztes Zeichen loeschen"},{"L1","Namen komplett leeren"},{"Y","Sammlung speichern"},{"B","Abbrechen"}}}; break;
            case Screen::Bluetooth: rows={{{"A","Verbinden / trennen"},{"X","Bluetooth an / aus"},{"Y","Status aktualisieren"},{"B","Zurueck"}}}; note="Neue Geraete in Knulli koppeln"; break;
            case Screen::CoverFlow:
            case Screen::AlbumList: rows={{{"A","Ordner / Medium oeffnen"},{"B","Eine Ebene zurueck"},{"LINKS/RECHTS","Eintrag waehlen"},{"Y","CoverFlow / Liste"},{"X","Sammlung neu scannen"}}}; break;
            case Screen::Tracks: rows={{{"A","Abspielen / Fortsetzen"},{"B","Zurueck zur Albumansicht"},{"STEUERKREUZ","Titel auswaehlen"}}}; note="Fortsetzbarer Titel ist vorausgewaehlt"; break;
            case Screen::Folders: rows={{{"A","Ordner oeffnen"},{"B","Zurueck"},{"Y","Diesen Ordner auswaehlen"}}}; break;
        }
        for (std::size_t index=0;index<rows.size() && rows[index].key!=nullptr;++index) {
            const int y=151+static_cast<int>(index)*31;
            if (index%2==0) {
                SDL_SetRenderDrawColor(renderer_,25,32,42,255);
                const SDL_Rect stripe{40,y-3,uiWidth_-80,30};SDL_RenderFillRect(renderer_,&stripe);
            }
            SDL_SetRenderDrawColor(renderer_,39,72,64,255);
            const SDL_Rect keycap{48,y,153,25};SDL_RenderFillRect(renderer_,&keycap);
            SDL_SetRenderDrawColor(renderer_,92,211,151,255);SDL_RenderDrawRect(renderer_,&keycap);
            const char* key=tr(language_,rows[index].key);
            int keyWidth=0,keyHeight=0;
            TTF_SizeUTF8(font_,key,&keyWidth,&keyHeight);
            drawFittedText(key,48+std::max(4,(153-keyWidth)/2),y+3,13,SDL_Color{232,247,239,255});
            drawFittedText(tr(language_,rows[index].action),220,y+3,34+extraWidth/11,SDL_Color{244,247,251,255});
        }
        if (note!=nullptr) drawFittedText(tr(language_,note),48,365,48+extraWidth/11,SDL_Color{143,154,170,255});
        drawFittedText(tr(language_,"START+SELECT  App beenden (Hilfe zu)"),48,390,48+extraWidth/11,SDL_Color{242,190,92,255});
        SDL_SetRenderDrawColor(renderer_,19,23,32,255);const SDL_Rect helpFooter{0,423,uiWidth_,57};SDL_RenderFillRect(renderer_,&helpFooter);
        drawText(language_==Language::German?"Y ENGLISH":"Y DEUTSCH",18,442,SDL_Color{92,211,151,255});
        if(bluetoothCapable_&&view_.screen!=Screen::Bluetooth)drawText("X BLUETOOTH",180,442,SDL_Color{171,181,196,255});
        drawText(tr(language_,"B / SELECT SCHLIESSEN"),uiWidth_-254,442,SDL_Color{171,181,196,255});
    }
    SDL_RenderPresent(renderer_);
}

} // namespace coverplayer::platform
