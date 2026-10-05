#pragma once
#include "coverplayer/library/library_cache.hpp"
#include <filesystem>
namespace coverplayer::persistence { class FileLibraryCache final:public library::LibraryCache { public: FileLibraryCache();explicit FileLibraryCache(std::filesystem::path directory);std::optional<std::vector<library::Collection>> load(const std::string& root,std::uint64_t fingerprint) override;bool save(const std::string& root,std::uint64_t fingerprint,const std::vector<library::Collection>& collections) override;void invalidate(const std::string& root) override;private:std::filesystem::path pathFor(const std::string& root) const;std::filesystem::path directory_;}; }
