#include "persistence/file_progress_store.hpp"
#include <SDL.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int, char**) {
    const auto root = std::filesystem::temp_directory_path() /
        ("coverplayer-state-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{root};
    const auto state = root / std::filesystem::u8path(u8"Einstellungen Hörbücher");
    auto require = [](bool result, const char* message) { if (!result) throw std::runtime_error(message); };
    try {
        // Explicit storage must work independently of SDL_GetPrefPath/cwd,
        // as required by the Switch SDL build whose preference API is a stub.
        coverplayer::persistence::FileProgressStore store(state);
        require(store.error().empty(), "Explicit state directory was rejected");
        require(store.saveCollections({{u8"Hörbücher", "sdmc:/media/Books", "audiobook"}}), "Cannot create collections");
        require(store.saveCollections({{"Music", "sdmc:/media/Music", "music"}, {u8"Hörbücher", "sdmc:/media/Books", "audiobook"}}), "Cannot replace collections");
        require(store.saveLanguage("en"), "Cannot save language");
        require(store.saveLanguage("de"), "Cannot replace language");
        require(store.save("sdmc:/media/Books/chapter.mp3", {28.5, false}), "Cannot save playback progress");
        require(store.addBookmark("sdmc:/media/Books/chapter.mp3", 12.25), "Cannot save bookmark");
        coverplayer::persistence::FileProgressStore reopened(state);
        const auto collections = reopened.collections();
        require(collections.size() == 2 && collections[0].path == "sdmc:/media/Music" && collections[1].name == u8"Hörbücher", "Collections did not survive reopening");
        const auto progress = reopened.load("sdmc:/media/Books/chapter.mp3");
        require(progress && progress->positionSeconds == 28.5, "Progress did not survive reopening");
        require(reopened.bookmarks("sdmc:/media/Books/chapter.mp3") == std::vector<double>{12.25}, "Bookmark did not survive reopening");
        require(reopened.language() == "de", "Language did not survive reopening");
        require(std::filesystem::exists(state / "collections-v2.txt"), "Wrong collections file location");
        require(!std::filesystem::exists(state / "collections-v2.txt.tmp"), "Temporary file left behind");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
