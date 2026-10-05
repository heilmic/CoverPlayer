#include "coverplayer/library/library_scanner.hpp"
#include "coverplayer/platform/file_system.hpp"
#include <iostream>
#include <map>

class FakeFileSystem final : public coverplayer::platform::FileSystem {
public:
    std::map<std::string,std::vector<coverplayer::platform::DirectoryEntry>> entries;
    std::map<std::string,coverplayer::platform::MediaMetadata> metadataByPath;
    std::vector<coverplayer::platform::DirectoryEntry> list(const std::string& path) const override { const auto found=entries.find(path);return found==entries.end()?std::vector<coverplayer::platform::DirectoryEntry>{}:found->second; }
    std::string parent(const std::string& path) const override { return path; }
    coverplayer::platform::MediaMetadata metadata(const std::string& path) const override {const auto found=metadataByPath.find(path);return found==metadataByPath.end()?coverplayer::platform::MediaMetadata{}:found->second;}
};

int main() {
    FakeFileSystem fs;
    fs.entries["root"]={{"Series","root/Series",true},{"loose.txt","root/loose.txt",false}};
    fs.entries["root/Series"]={{"10 Finale.MP3","root/Series/10 Finale.MP3",false},{"2 Anfang.mp3","root/Series/2 Anfang.mp3",false},{"cover.png","root/Series/cover.png",false},{"._2 Anfang.mp3","root/Series/._2 Anfang.mp3",false}};
    const auto result=coverplayer::library::LibraryScanner(fs).scan("root");
    if(result.size()!=1||result[0].name!="Series"||result[0].tracks.size()!=2||result[0].tracks[0].name!="2 Anfang"||result[0].tracks[1].name!="10 Finale"||result[0].coverPath!="root/Series/cover.png"){
        std::cerr<<"library scanner contract failed\n";return 1;
    }
    FakeFileSystem tagged;
    tagged.entries["music"]={{"Album","music/Album",true}};
    tagged.entries["music/Album"]={{"b.mp3","music/Album/b.mp3",false},{"a.mp3","music/Album/a.mp3",false},{"bad.mp3","music/Album/bad.mp3",false}};
    tagged.metadataByPath["music/Album/a.mp3"]={true,"Second","Artist","Tagged Album",2,"embedded.jpg"};
    tagged.metadataByPath["music/Album/b.mp3"]={true,"First","Artist","Tagged Album",1,""};
    tagged.metadataByPath["music/Album/bad.mp3"]={false};
    const auto taggedResult=coverplayer::library::LibraryScanner(tagged).scan("music");
    if(taggedResult.size()!=1||taggedResult[0].name!="Album"||taggedResult[0].artist!="Artist"||taggedResult[0].coverPath!="embedded.jpg"||taggedResult[0].tracks.size()!=2||taggedResult[0].tracks[0].name!="First"){
        std::cerr<<"metadata, embedded cover, or corrupt-file handling failed\n";return 1;
    }
    FakeFileSystem cyclic;
    cyclic.entries["cycle"]={{"again","cycle",true},{"track.mp3","cycle/track.mp3",false}};
    const auto cyclicResult=coverplayer::library::LibraryScanner(cyclic).scan("cycle");
    if(cyclicResult.size()!=1||cyclicResult[0].tracks.size()!=1){
        std::cerr<<"cyclic directory protection failed\n";return 1;
    }

    FakeFileSystem hierarchy;
    hierarchy.entries["library"]={{"Artist","library/Artist",true},{"Flat Album","library/Flat Album",true},{"Series","library/Series",true}};
    hierarchy.entries["library/Artist"]={{"Album 2","library/Artist/Album 2",true},{"Album 1","library/Artist/Album 1",true}};
    hierarchy.entries["library/Artist/Album 1"]={{"track.mp3","library/Artist/Album 1/track.mp3",false}};
    hierarchy.entries["library/Artist/Album 2"]={{"track.mp3","library/Artist/Album 2/track.mp3",false}};
    hierarchy.entries["library/Flat Album"]={{"track.mp3","library/Flat Album/track.mp3",false}};
    hierarchy.entries["library/Series"]={{"Folge 101","library/Series/Folge 101",true},{"Folge 102.mp3","library/Series/Folge 102.mp3",false},{"Folge 103","library/Series/Folge 103",true}};
    hierarchy.entries["library/Series/Folge 101"]={{"part.mp3","library/Series/Folge 101/part.mp3",false}};
    hierarchy.entries["library/Series/Folge 103"]={{"part.mp3","library/Series/Folge 103/part.mp3",false}};
    const auto tree=coverplayer::library::LibraryScanner(hierarchy).scan("library");
    if(tree.size()!=3||tree[0].name!="Artist"||tree[0].children.size()!=2||
        tree[0].children[0].name!="Album 1"||tree[0].children[0].tracks.size()!=1||
        tree[1].name!="Flat Album"||tree[1].tracks.size()!=1||
        tree[2].name!="Series"||tree[2].children.size()!=3||
        tree[2].children[1].name!="Folge 102"||tree[2].children[1].tracks.size()!=1) {
        std::cerr<<"recursive or mixed-depth hierarchy failed\n";return 1;
    }
    return 0;
}
