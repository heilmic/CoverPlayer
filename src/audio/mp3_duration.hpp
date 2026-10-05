#pragma once

#include <string>

namespace coverplayer::audio {

// Reads only the MP3 header area. Xing/VBRI files are exact; CBR files use
// their first frame bitrate. No libmpg123 length/scan symbol is required.
double estimateMp3DurationSeconds(const std::string& path);

} // namespace coverplayer::audio
