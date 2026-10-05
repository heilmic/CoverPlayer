#include "persistence/file_library_cache.hpp"

#include <SDL.h>

#include <chrono>
#include <filesystem>
#include <iostream>

int main(int, char**) {
    const auto directory=std::filesystem::temp_directory_path() /
        ("coverplayer-cache-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    coverplayer::persistence::FileLibraryCache cache(directory);
    coverplayer::library::Collection leaf{"Album", "/music/Artist/Album", "cover.jpg", "Artist",
        {{"Track", "/music/Artist/Album/track.mp3", "Artist", "Album", 1, 245.5}}};
    coverplayer::library::Collection artist{"Artist", "/music/Artist", "cover.jpg", "", {}, {leaf}};
    const std::vector<coverplayer::library::Collection> tree{artist};
    if(!cache.save("/music",42,tree)){std::cerr<<"recursive cache save failed\n";return 1;}
    const auto loaded=cache.load("/music",42);
    if(!loaded||loaded->size()!=1||loaded->front().children.size()!=1||
        loaded->front().children.front().tracks.size()!=1||
        loaded->front().children.front().tracks.front().path!="/music/Artist/Album/track.mp3"||
        loaded->front().children.front().tracks.front().durationSeconds!=245.5){
        std::cerr<<"recursive cache roundtrip failed\n";return 1;
    }
    if(cache.load("/music",43)){std::cerr<<"cache accepted a stale fingerprint\n";return 1;}
    cache.invalidate("/music");
    if(cache.load("/music",42)){std::cerr<<"cache invalidation failed\n";return 1;}
    std::error_code error;std::filesystem::remove_all(directory,error);
    return 0;
}
