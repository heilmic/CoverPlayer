#include "platform/sdl/sdl_renderer.hpp"

#include <SDL.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool saveBackBuffer(const std::string& path) {
    SDL_Window* window = nullptr;
    for (Uint32 id = 1; id < 32 && window == nullptr; ++id) window = SDL_GetWindowFromID(id);
    if (window == nullptr) return false;
    SDL_Renderer* renderer = SDL_GetRenderer(window);
    if (renderer == nullptr) return false;
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) return false;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) return false;
    const bool success = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
        surface->pixels, surface->pitch) == 0 && SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return success;
}

bool render(coverplayer::platform::SdlRenderer& renderer,
    coverplayer::platform::ViewModel view, const std::string& path) {
    renderer.setView(std::move(view));
    renderer.present();
    return saveBackBuffer(path);
}

} // namespace

#ifdef _WIN32
int SDL_main(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: coverplayer_screenshot_tool <output-dir> <source-cover-dir> [de|en]\n";
        return 2;
    }
    const std::filesystem::path output = std::filesystem::u8path(argv[1]);
    const std::filesystem::path source = std::filesystem::u8path(argv[2]);
    std::error_code error;
    std::filesystem::create_directories(output, error);
    if (error) return 1;

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    coverplayer::platform::SdlRenderer renderer;
    const bool english = argc == 4 && std::string(argv[3]) == "en";
    if (argc == 4) renderer.setLanguage(coverplayer::languageFromCode(argv[3]));
    const auto demo = [english](const char* de, const char* en) {
        return std::string(english ? en : de);
    };
    renderer.setBluetoothStatus(true, true);
    renderer.setPlayerDetails(64, 2, {}, 78);

    const std::vector<std::string> covers{
        (source / (english ? "hp-1.jpg" : "236.png")).u8string(), (source / (english ? "hp-2.jpg" : "237.png")).u8string(),
        (source / (english ? "hp-3.jpg" : "238.png")).u8string(), (source / (english ? "hp-4.jpg" : "239.png")).u8string(),
        (source / (english ? "hp-5.jpg" : "240.png")).u8string()
    };
    const std::vector<std::string> episodes{
        demo("236  Im Bann des Barrakudas|6 TITEL  7% GEHOERT", "Philosopher's Stone|17 CHAPTERS  7% LISTENED"),
        demo("237  Der rote Bueffel|1 TITEL  100% GEHOERT", "Chamber of Secrets|18 CHAPTERS  100% LISTENED"),
        demo("238  Falsche Schuld|7 TITEL  42% GEHOERT", "Prisoner of Azkaban|22 CHAPTERS  42% LISTENED"),
        demo("239  Sieben Palmen|6 TITEL  18% GEHOERT", "Goblet of Fire|37 CHAPTERS  18% LISTENED"),
        demo("240  Die schwarze Rose|1 TITEL  88% GEHOERT", "Order of the Phoenix|38 CHAPTERS  88% LISTENED")
    };

    coverplayer::platform::ViewModel flow;
    flow.screen = coverplayer::platform::Screen::CoverFlow;
    flow.title = demo("DIE DREI ???", "HARRY POTTER AUDIOBOOKS");
    flow.items = episodes;
    flow.itemImages = covers;
    flow.selected = 2;
    if (!render(renderer, flow, (output / "01-coverflow.bmp").u8string())) return 1;

    coverplayer::platform::ViewModel collections;
    collections.screen = coverplayer::platform::Screen::Collections;
    collections.title = demo("SAMMLUNGEN", "COLLECTIONS");
    collections.items = {
        demo("Die drei ???|HOERSPIEL  5 MEDIEN  62% GEHOERT", "Harry Potter|AUDIOBOOK  5 ALBUMS  62% LISTENED"),
        demo("Hoerbuecher|HOERBUCH  18 MEDIEN  31% GEHOERT", "Audiobooks|AUDIOBOOK  18 ALBUMS  31% LISTENED"),
        demo("Podcasts|PODCAST  12 MEDIEN", "Podcasts|PODCAST  12 ALBUMS"),
        demo("Musik|MUSIK  24 MEDIEN", "Music|MUSIC  24 ALBUMS")
    };
    collections.itemImages = {covers[1], covers[2], covers[3], covers[4]};
    collections.coverPath = covers[1];
    if (!render(renderer, collections, (output / "02-sammlungen.bmp").u8string())) return 1;

    coverplayer::platform::ViewModel list;
    list.screen = coverplayer::platform::Screen::AlbumList;
    list.title = demo("DIE DREI ???", "HARRY POTTER AUDIOBOOKS");
    list.items = {episodes[1], episodes[2], episodes[3], episodes[4]};
    list.itemImages = {covers[1], covers[2], covers[3], covers[4]};
    list.coverPath = covers.back();
    list.selected = 3;
    if (!render(renderer, list, (output / "03-albumliste.bmp").u8string())) return 1;

    coverplayer::platform::ViewModel tracks;
    tracks.screen = coverplayer::platform::Screen::Tracks;
    tracks.title = demo("239  SIEBEN PALMEN", "GOBLET OF FIRE");
    tracks.coverPath = covers[3];
    tracks.selected = 4;
    tracks.items = {
        demo("01  Ankunft auf der Insel  [FERTIG]", "01  A summer invitation  [DONE]"), demo("02  Der erste Hinweis  [FERTIG]", "02  A journey with friends  [DONE]"),
        demo("03  Spuren im Sand  [FERTIG]", "03  An unexpected visitor  [DONE]"), demo("04  Die Nachtwache  [FERTIG]", "04  The evening gathering  [DONE]"),
        demo("05  Eine neue Entdeckung  [8:14]", "05  A new discovery  [8:14]"), demo("06  Das Geheimnis", "06  The secret")
    };
    if (!render(renderer, tracks, (output / "04-titelliste.bmp").u8string())) return 1;

    renderer.setPlaybackStatus(true, false, 3668.0, 4169.0);
    renderer.setSleepTimer(30);
    coverplayer::platform::ViewModel player;
    player.screen = coverplayer::platform::Screen::Player;
    player.title = demo("Die schwarze Rose", "A message at dawn");
    player.subtitle = demo("240  DIE SCHWARZE ROSE  |  TITEL 1/1", "ORDER OF THE PHOENIX  |  CHAPTER 1/38");
    player.coverPath = covers.back();
    if (!render(renderer, player, (output / "05-jetzt-laeuft.bmp").u8string())) return 1;

    coverplayer::platform::ViewModel bluetooth;
    bluetooth.screen = coverplayer::platform::Screen::Bluetooth;
    bluetooth.title = demo("BLUETOOTH-KOPFHOERER", "BLUETOOTH HEADPHONES");
    bluetooth.subtitle = demo("BLUETOOTH AN  |  AUDIO AKTIV", "BLUETOOTH ON  |  AUDIO ACTIVE");
    bluetooth.items = {demo("Soundcore Q30  [AKTIV]", "Soundcore Q30  [ACTIVE]"), "JBL Flip 6", demo("WH-1000XM4  [VERBUNDEN]", "WH-1000XM4  [CONNECTED]")};
    if (!render(renderer, bluetooth, (output / "06-bluetooth.bmp").u8string())) return 1;

    const std::vector<std::string> kidsCovers{
        (source / "kids-070.jpg").u8string(), (source / "kids-071.jpg").u8string(),
        (source / "kids-103.jpg").u8string(), (source / "kids-086.jpg").u8string(),
        (source / "kids-066.jpg").u8string()
    };
    coverplayer::platform::ViewModel kids;
    kids.screen = coverplayer::platform::Screen::CoverFlow;
    kids.title = "DIE DREI ??? KIDS";
    kids.items = {
        demo("70  Aufbruch ins All|1 TITEL", "70  Aufbruch ins All|1 TRACK"), demo("71  Tatort Trampolin|6 TITEL", "71  Tatort Trampolin|6 TRACKS"),
        demo("103  SOS im Bike-Park|7 TITEL  36% GEHOERT", "103  SOS im Bike-Park|7 TRACKS  36% LISTENED"), demo("86  Riesen in Rocky Beach|1 TITEL", "86  Riesen in Rocky Beach|1 TRACK"),
        demo("66  Geheimnis im Meer|5 TITEL", "66  Geheimnis im Meer|5 TRACKS")
    };
    kids.itemImages = kidsCovers;
    kids.selected = 2;
    renderer.setSleepTimer(0);
    if (!render(renderer, kids, (output / "07-drei-fragezeichen-kids.bmp").u8string())) return 1;

    coverplayer::platform::ViewModel checkPod;
    checkPod.screen = coverplayer::platform::Screen::Player;
    checkPod.title = "CheckPod: Gaming";
    checkPod.subtitle = demo("CHECKER TOBI  |  PODCAST-FOLGE", "CHECKER TOBI  |  PODCAST EPISODE");
    checkPod.coverPath = (source / "checkpod.jpg").u8string();
    renderer.setPlaybackStatus(true, false, 812.0, 1496.0);
    renderer.setPlayerDetails(58, 1, {}, 76);
    if (!render(renderer, checkPod, (output / "08-checkpod.bmp").u8string())) return 1;

    const std::vector<std::string> internationalCovers{
        (source / "hp-1.jpg").u8string(), (source / "hp-2.jpg").u8string(),
        (source / "hp-3.jpg").u8string(), (source / "hp-4.jpg").u8string(),
        (source / "hp-5.jpg").u8string()
    };
    coverplayer::platform::ViewModel international;
    international.screen = coverplayer::platform::Screen::CoverFlow;
    international.title = "HARRY POTTER AUDIOBOOKS";
    international.items = {
        "Philosopher's Stone|17 CHAPTERS", "Chamber of Secrets|18 CHAPTERS",
        "Prisoner of Azkaban|22 CHAPTERS  64% LISTENED", "Goblet of Fire|37 CHAPTERS",
        "Order of the Phoenix|38 CHAPTERS"
    };
    international.itemImages = internationalCovers;
    international.selected = 2;
    if (!render(renderer, international, (output / "09-international-audiobooks.bmp").u8string())) return 1;

    const std::vector<std::string> musicCovers{
        (source / "music-foo.jpg").u8string(), (source / "music-linkin.jpg").u8string(),
        (source / "music-rhcp.jpg").u8string(), (source / "music-acdc.jpg").u8string(),
        (source / "music-stones.jpg").u8string()
    };
    coverplayer::platform::ViewModel music;
    music.screen = coverplayer::platform::Screen::CoverFlow;
    music.title = "MUSIC";
    music.items = {
        "Wasting Light|FOO FIGHTERS", "Hybrid Theory|LINKIN PARK",
        "Stadium Arcadium|RED HOT CHILI PEPPERS", "Back In Black|AC/DC",
        "Hot Rocks 1964-1971|THE ROLLING STONES"
    };
    music.itemImages = musicCovers;
    music.selected = 2;
    if (!render(renderer, music, (output / "10-music-coverflow.bmp").u8string())) return 1;
    renderer.setPlaybackStatus(true, false, 812.0, 1496.0);
    renderer.setPlayerDetails(58, 1, {}, 76);
    renderer.setHelpVisible(true);
    if (!render(renderer, player, (output / "11-help-player.bmp").u8string())) return 1;
    if (!render(renderer, collections, (output / "12-help-collections.bmp").u8string())) return 1;
    renderer.setHelpVisible(false);
    coverplayer::platform::ViewModel longTracks;
    longTracks.screen = coverplayer::platform::Screen::Tracks;
    longTracks.title = demo("239 - Sieben Palmen", "Goblet of Fire");
    longTracks.coverPath = covers[3];
    longTracks.selected = 2;
    longTracks.items = {
        demo("239 - Sieben Palmen (Teil 01) - Eine geheimnisvolle Entdeckung", "Goblet of Fire (Chapter 01) - A mysterious discovery"),
        demo("239 - Sieben Palmen (Teil 02) - Die Spur fuehrt zur Villa", "Goblet of Fire (Chapter 02) - The path to the old house"),
        demo("239 - Sieben Palmen (Teil 03) - Das Versteck unter den Palmen", "Goblet of Fire (Chapter 03) - A secret beneath the trees"),
        demo("239 - Sieben Palmen (Teil 04) - Das Raetsel wird geloest", "Goblet of Fire (Chapter 04) - The mystery is finally solved")
    };
    if (!render(renderer, longTracks, (output / "13-long-tracks.bmp").u8string())) return 1;
    coverplayer::platform::ViewModel longPlayer;
    longPlayer.screen = coverplayer::platform::Screen::Player;
    longPlayer.title = longTracks.items[longTracks.selected];
    longPlayer.subtitle = demo("239 - Sieben Palmen  |  TITEL 3/4", "GOBLET OF FIRE  |  CHAPTER 3/37");
    longPlayer.coverPath = longTracks.coverPath;
    renderer.setPlaybackStatus(true, false, 843.0, 1860.0);
    renderer.setPlayerDetails(45, 3, {}, 76);
    if (!render(renderer, longPlayer, (output / "14-long-player.bmp").u8string())) return 1;
    return 0;
}
