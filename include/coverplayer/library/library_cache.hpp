#pragma once
#include "coverplayer/library/library_scanner.hpp"
#include <optional>
#include <cstdint>
namespace coverplayer::library {
class LibraryCache { public: virtual ~LibraryCache()=default;virtual std::optional<std::vector<Collection>> load(const std::string& root,std::uint64_t fingerprint)=0;virtual bool save(const std::string& root,std::uint64_t fingerprint,const std::vector<Collection>& collections)=0;virtual void invalidate(const std::string& root)=0; };
}
