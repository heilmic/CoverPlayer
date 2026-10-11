// Compiled only for the Linux/desktop handheld layout.
#include "platform/sdl/sdl_renderer.hpp"
#include "platform/sdl/ui_content.hpp"
#include <algorithm>

namespace coverplayer::platform {
namespace {
constexpr SDL_Color kActiveAccent{86, 163, 255, 255};
}
using ui::formatTime;
using ui::utf8Length;

void SdlRenderer::renderHandheldUi() {
    const int extraWidth = uiWidth_ - 640;
    SDL_SetRenderDrawColor(renderer_, 12, 15, 22, 255);
    SDL_RenderClear(renderer_);
    SDL_SetRenderDrawColor(renderer_, 24, 30, 42, 255);
    const SDL_Rect header{0, 0, uiWidth_, 70};
    SDL_RenderFillRect(renderer_, &header);
    SDL_SetRenderDrawColor(renderer_, 92, 211, 151, 255);
    const SDL_Rect accent{0, 68, uiWidth_, 3};
    SDL_RenderFillRect(renderer_, &accent);
    drawText("COVER", 20, 20, SDL_Color{92, 211, 151, 255});
    drawText("PLAYER", 86, 20, SDL_Color{244, 247, 251, 255});
    const char *section = ui::sectionLabel(view_.screen);
    if (view_.screen == Screen::Player) {
        drawText(tr(language_, section), 252 + extraWidth, 22, SDL_Color{155, 166, 184, 255});
        drawBluetoothIcon(400 + extraWidth, 20);
        const std::string volume = "VOL " + std::to_string(volumePercent_) + "%";
        drawText(volume.c_str(), 430 + extraWidth, 22, SDL_Color{203, 210, 220, 255});
        if (batteryPercent_) {
            drawBatteryIcon(528 + extraWidth, 24, *batteryPercent_);
            const std::string battery = std::to_string(*batteryPercent_) + "%";
            drawText(battery.c_str(), 558 + extraWidth, 22, SDL_Color{203, 210, 220, 255});
        }
        if (sleepMinutes_ > 0) {
            const std::string sleep = std::to_string(sleepMinutes_) + "m";
            drawText(sleep.c_str(), 607 + extraWidth, 22, SDL_Color{242, 190, 92, 255});
        }
    } else {
        drawBluetoothIcon(433 + extraWidth, 20);
        // The section label and the sleep badge each get a fixed slot so
        // neither hides the other - previously the sleep timer replaced the
        // screen name entirely, which lost the "where am I" context.
        drawFittedText(tr(language_, section), 458 + extraWidth, 22, 12, SDL_Color{155, 166, 184, 255});
        if (sleepMinutes_ > 0) {
            const std::string sleep = std::to_string(sleepMinutes_) + "m";
            drawText(sleep.c_str(), 606 + extraWidth, 22, SDL_Color{242, 190, 92, 255});
        }
    }

    if (view_.screen == Screen::CoverFlow) {
        renderCoverFlow();
    } else if (view_.screen == Screen::Player) {
        // Keep the cover prominent and wrap the track details within the
        // remaining width, including on the narrower 640px handheld.
        drawCover(25, 100, 225, 225);
        const int detailX = 275, detailWidth = uiWidth_ - detailX - 24;
        // Switch sooner than the raw three-line capacity: breaking on words
        // can otherwise elide an important word even when the character
        // count alone suggests it should fit.
        TTF_Font *trackFont =
            utf8Length(view_.title) > static_cast<std::size_t>(2 * textCapacity(detailWidth, titleFont_))
                ? font_
                : titleFont_;
        const int titleLineHeight = trackFont == titleFont_ ? 32 : 25;
        const int titleLines = drawWrappedText(view_.title, detailX, 100, detailWidth, titleLineHeight, 3,
                                               SDL_Color{244, 247, 251, 255}, trackFont);
        const int subtitleY = 100 + titleLines * titleLineHeight + 12;
        const int subtitleLines = drawWrappedText(view_.subtitle, detailX, subtitleY, detailWidth, 24, 2,
                                                  SDL_Color{143, 154, 170, 255});
        const int bookmarkY = subtitleY + subtitleLines * 24 + 8;
        drawText((std::string(tr(language_, "LESEZEICHEN")) + "  " + std::to_string(bookmarkCount_)).c_str(),
                 detailX, bookmarkY, SDL_Color{143, 154, 170, 255});
        const int statusY = bookmarkY + 28;
        if (!view_.message.empty())
            drawFittedText(view_.message, detailX, statusY, textCapacity(detailWidth),
                           SDL_Color{242, 118, 109, 255});
        else if (!notice_.empty())
            drawFittedText(notice_, detailX, statusY, textCapacity(detailWidth),
                           SDL_Color{242, 190, 92, 255});

        // The rail is a clean, uninterrupted bar; the play/pause symbol gets
        // its own row below it instead of sitting on top of the rail, with
        // the elapsed/remaining time flanking it on the same row.
        const int railX = 24, railY = 335, railWidth = uiWidth_ - 48;
        SDL_SetRenderDrawColor(renderer_, 42, 49, 62, 255);
        const SDL_Rect rail{railX, railY, railWidth, 8};
        SDL_RenderFillRect(renderer_, &rail);
        if (playbackDurationSeconds_ > 0.0) {
            const auto progress = std::min(1.0, playbackPositionSeconds_ / playbackDurationSeconds_);
            SDL_SetRenderDrawColor(renderer_, kActiveAccent.r, kActiveAccent.g, kActiveAccent.b,
                                   kActiveAccent.a);
            const SDL_Rect filled{railX, railY, static_cast<int>(railWidth * progress), 8};
            SDL_RenderFillRect(renderer_, &filled);
        }
        const int transportY = 375;
        drawPlaybackSymbol(uiWidth_ / 2, transportY, playbackPaused_);
        drawText(formatTime(playbackPositionSeconds_).c_str(), railX, transportY - 11,
                 SDL_Color{203, 210, 220, 255});
        const auto duration = playbackDurationSeconds_ > 0.0 ? formatTime(playbackDurationSeconds_) : "--:--";
        int durationWidth = 0, durationHeight = 0;
        TTF_SizeUTF8(font_, duration.c_str(), &durationWidth, &durationHeight);
        drawText(duration.c_str(), uiWidth_ - 24 - durationWidth, transportY - 11,
                 SDL_Color{203, 210, 220, 255});
    } else if (view_.screen == Screen::Collections || view_.screen == Screen::AlbumList) {
        drawFittedText(view_.title, 24, 82, 32 + extraWidth / 16, SDL_Color{244, 247, 251, 255}, titleFont_);
        drawCover(24, 126, 122, 122);
        drawSelectableList(view_.items, view_.selected,
                           {166, uiWidth_ - 190, 49, 120, 6, 39 + extraWidth / 11, true});
        if (!view_.message.empty())
            drawFittedText(view_.message, 24, 391, 58 + extraWidth / 11, SDL_Color{143, 154, 170, 255});
    } else if (view_.screen == Screen::CollectionManager) {
        drawFittedText(view_.title, 24, 82, 30 + extraWidth / 16, SDL_Color{244, 247, 251, 255}, titleFont_);
        if (!view_.coverPath.empty())
            drawCover(uiWidth_ - 140, 124, 104, 104);
        drawSelectableList(view_.items, view_.selected,
                           {24, uiWidth_ - 188, 43, 126, 6, 38 + extraWidth / 11, true});
        if (!view_.message.empty())
            drawFittedText(view_.message, 24, 389, 57 + extraWidth / 11, SDL_Color{143, 154, 170, 255});
    } else if (view_.screen == Screen::Bluetooth) {
        drawFittedText(view_.title, 24, 84, 30 + extraWidth / 16, SDL_Color{244, 247, 251, 255}, titleFont_);
        drawFittedText(view_.subtitle, 24, 132, 60 + extraWidth / 11, SDL_Color{143, 154, 170, 255});
        drawSelectableList(view_.items, view_.selected,
                           {24, uiWidth_ - 48, 31, 184, 7, 57 + extraWidth / 11, false});
        if (!view_.message.empty())
            drawFittedText(view_.message, 24, 397, 64 + extraWidth / 11, SDL_Color{242, 190, 92, 255});
    } else if (view_.screen == Screen::CollectionName) {
        drawText(tr(language_, "NAME DER SAMMLUNG"), 24, 94, SDL_Color{143, 154, 170, 255});
        SDL_SetRenderDrawColor(renderer_, 31, 38, 50, 255);
        const SDL_Rect nameField{24, 121, uiWidth_ - 48, 42};
        SDL_RenderFillRect(renderer_, &nameField);
        SDL_SetRenderDrawColor(renderer_, 92, 211, 151, 255);
        const SDL_Rect nameMarker{24, 121, 4, 42};
        SDL_RenderFillRect(renderer_, &nameMarker);
        drawFittedText(view_.subtitle, 40, 131, 52 + extraWidth / 11, SDL_Color{244, 247, 251, 255});
        for (std::size_t index = 0; index < view_.items.size(); ++index) {
            const int column = static_cast<int>(index % 10), row = static_cast<int>(index / 10);
            const int cellWidth = (uiWidth_ - 48) / 10;
            const int x = 24 + column * cellWidth, y = 177 + row * 31;
            if (index == view_.selected) {
                SDL_SetRenderDrawColor(renderer_, 45, 82, 70, 255);
                const SDL_Rect selected{x, y, cellWidth - 5, 27};
                SDL_RenderFillRect(renderer_, &selected);
            }
            const auto &key = view_.items[index];
            drawFittedText(key, x + (key.size() > 1 ? 3 : (cellWidth - 19) / 2), y + 4, 6,
                           index == view_.selected ? SDL_Color{244, 247, 251, 255}
                                                   : SDL_Color{158, 168, 184, 255});
        }
        if (!view_.message.empty())
            drawFittedText(view_.message, 24, 401, 65 + extraWidth / 11, SDL_Color{242, 190, 92, 255});
    } else if (view_.screen == Screen::Tracks) {
        // Keep the album artwork as context, then give every track the full
        // 640/720px list width. The selected filename is also shown above
        // the list on two lines so similar long names remain distinguishable.
        if (!view_.coverPath.empty())
            drawCover(25, 88, 84, 84);
        const int headingX = view_.coverPath.empty() ? 24 : 130;
        drawFittedText(view_.title, headingX, 88, textCapacity(uiWidth_ - headingX - 24, titleFont_),
                       SDL_Color{244, 247, 251, 255}, titleFont_);
        if (view_.selected < view_.items.size()) {
            drawWrappedText(view_.items[view_.selected], headingX, 128, uiWidth_ - headingX - 24, 24, 2,
                            SDL_Color{203, 210, 220, 255});
        }
        drawSelectableList(view_.items, view_.selected,
                           {24, uiWidth_ - 48, 31, 199, 7, textCapacity(uiWidth_ - 80), false, true});
        if (!view_.message.empty())
            drawFittedText(view_.message, headingX, 174, textCapacity(uiWidth_ - headingX - 24),
                           SDL_Color{143, 154, 170, 255});
    } else {
        const bool hasCover = !view_.coverPath.empty();
        const int listX = hasCover ? 272 : 24;
        const int listWidth = hasCover ? uiWidth_ - 294 : uiWidth_ - 48;
        const int maxCharacters = (hasCover ? 20 : 37) + extraWidth / 11;
        if (hasCover)
            drawCover(25, 100, 225, 225);
        drawFittedText(view_.title, hasCover ? 280 : 24, 88, (hasCover ? 20 : 37) + extraWidth / 16,
                       SDL_Color{244, 247, 251, 255}, titleFont_);
        drawSelectableList(view_.items, view_.selected, {listX, listWidth, 31, 134, 9, maxCharacters, false});
        if (!view_.message.empty())
            drawFittedText(view_.message, 25, 345, 65 + extraWidth / 11, SDL_Color{143, 154, 170, 255});
    }
    SDL_SetRenderDrawColor(renderer_, 19, 23, 32, 255);
    const SDL_Rect footer{0, 423, uiWidth_, 57};
    SDL_RenderFillRect(renderer_, &footer);
    const char *primaryHint = ui::primaryHint(view_.screen);
    drawFittedText(tr(language_, primaryHint), 18, view_.screen == Screen::Player ? 427 : 442,
                   40 + extraWidth / 11, SDL_Color{171, 181, 196, 255});
    if (view_.screen == Screen::Player)
        drawFittedText(tr(language_, "START KURZ SLEEP / LANG HINTERGRUND"), 18, 451, 40 + extraWidth / 11,
                       SDL_Color{171, 181, 196, 255});
    SDL_SetRenderDrawColor(renderer_, 37, 71, 61, 255);
    const SDL_Rect helpBadge{uiWidth_ - 162, 430, 146, 38};
    SDL_RenderFillRect(renderer_, &helpBadge);
    drawText(tr(language_, "SELECT HILFE"), uiWidth_ - 156, 440, SDL_Color{232, 247, 239, 255});
    if (helpVisible_) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer_, 7, 9, 14, 255);
        const SDL_Rect backdrop{0, 70, uiWidth_, 410};
        SDL_RenderFillRect(renderer_, &backdrop);
        SDL_SetRenderDrawColor(renderer_, 19, 25, 34, 255);
        const SDL_Rect panel{24, 76, uiWidth_ - 48, 342};
        SDL_RenderFillRect(renderer_, &panel);
        drawText(tr(language_, "BEDIENUNG"), 44, 88, SDL_Color{92, 211, 151, 255});
        drawText(tr(language_, "TASTE"), 58, 121, SDL_Color{143, 154, 170, 255});
        drawText(tr(language_, "AKTION"), 220, 121, SDL_Color{143, 154, 170, 255});
        SDL_SetRenderDrawColor(renderer_, 51, 61, 73, 255);
        SDL_RenderDrawLine(renderer_, 44, 145, uiWidth_ - 44, 145);
        const auto help = ui::helpContent(view_.screen, language_);
        const auto &rows = help.rows;
        const auto *note = help.note;
        for (std::size_t index = 0; index < rows.size() && rows[index].key != nullptr; ++index) {
            const int y = 151 + static_cast<int>(index) * 31;
            if (index % 2 == 0) {
                SDL_SetRenderDrawColor(renderer_, 25, 32, 42, 255);
                const SDL_Rect stripe{40, y - 3, uiWidth_ - 80, 30};
                SDL_RenderFillRect(renderer_, &stripe);
            }
            SDL_SetRenderDrawColor(renderer_, 39, 72, 64, 255);
            const SDL_Rect keycap{48, y, 153, 25};
            SDL_RenderFillRect(renderer_, &keycap);
            SDL_SetRenderDrawColor(renderer_, 92, 211, 151, 255);
            SDL_RenderDrawRect(renderer_, &keycap);
            const char *key = tr(language_, rows[index].key);
            int keyWidth = 0, keyHeight = 0;
            TTF_SizeUTF8(font_, key, &keyWidth, &keyHeight);
            drawFittedText(key, 48 + std::max(4, (153 - keyWidth) / 2), y + 3, 13,
                           SDL_Color{232, 247, 239, 255});
            drawFittedText(tr(language_, rows[index].action), 220, y + 3, 34 + extraWidth / 11,
                           SDL_Color{244, 247, 251, 255});
        }
        if (note != nullptr)
            drawFittedText(tr(language_, note), 48, 365, 48 + extraWidth / 11, SDL_Color{143, 154, 170, 255});
        drawFittedText(tr(language_, "START+SELECT  App beenden (Hilfe zu)"), 48, 390, 48 + extraWidth / 11,
                       SDL_Color{242, 190, 92, 255});
        SDL_SetRenderDrawColor(renderer_, 19, 23, 32, 255);
        const SDL_Rect helpFooter{0, 423, uiWidth_, 57};
        SDL_RenderFillRect(renderer_, &helpFooter);
        drawText(language_ == Language::German ? "Y ENGLISH" : "Y DEUTSCH", 18, 442,
                 SDL_Color{92, 211, 151, 255});
        if (bluetoothCapable_ && view_.screen != Screen::Bluetooth)
            drawText("X BLUETOOTH", 180, 442, SDL_Color{171, 181, 196, 255});
        drawText(tr(language_, "B / SELECT SCHLIESSEN"), uiWidth_ - 254, 442, SDL_Color{171, 181, 196, 255});
    }
    SDL_RenderPresent(renderer_);
}

} // namespace coverplayer::platform
