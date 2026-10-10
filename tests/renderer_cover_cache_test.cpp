#include "platform/sdl/sdl_renderer.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char**) {
    SDL_SetMainReady();
    if (argc == 1) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << SDL_GetError() << '\n'; return 1; }
    const auto directory = std::filesystem::temp_directory_path() /
        ("coverplayer-render-test-" + std::to_string(SDL_GetPerformanceCounter()));
    std::filesystem::create_directories(directory);
    try {
        {
            coverplayer::platform::SdlRenderer renderer;
            coverplayer::platform::ViewModel view;
            view.screen = coverplayer::platform::Screen::CoverFlow;
            view.items = {"Cache test"};
            auto select = [&](const std::string& path) {
                view.itemImages = {path}; renderer.setView(view); renderer.present();
            };
            auto finish = [&] {
                const auto start = SDL_GetTicks();
                while (renderer.coversLoading() && SDL_GetTicks() - start < 5000) {
                    SDL_Delay(1); renderer.present();
                }
                require(!renderer.coversLoading(), "decode did not finish");
            };
            SDL_Surface* source = SDL_CreateRGBSurfaceWithFormat(0, 512, 512, 32, SDL_PIXELFORMAT_RGBA32);
            require(source != nullptr, "source surface");
            SDL_FillRect(source, nullptr, SDL_MapRGBA(source->format, 200, 50, 20, 255));
            for (int i = 0; i < 30; ++i) {
                const auto path = (directory / (std::to_string(i) + ".bmp")).string();
                require(SDL_SaveBMP(source, path.c_str()) == 0, "save fixture");
                select(path); require(renderer.coversLoading(), "new cover was not asynchronous"); finish();
                select(path); require(!renderer.coversLoading(), "cached cover decoded again");
            }
            SDL_FreeSurface(source);
            select((directory / "0.bmp").string());
            require(renderer.coversLoading(), "cache did not evict oldest cover"); finish();
            const auto missing = (directory / "missing.jpg").string();
            select(missing); finish(); select(missing);
            require(!renderer.coversLoading(), "failed cover retried endlessly");
            // Leave a new decode pending: destruction must join it before SDL_image shuts down.
            select((directory / "1.bmp").string());
        }
        std::filesystem::remove_all(directory);
        SDL_Quit();
        std::cout << "Async covers, cache hits/eviction, missing artwork and shutdown passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::filesystem::remove_all(directory);
        SDL_Quit(); std::cerr << e.what() << '\n'; return 1;
    }
}
