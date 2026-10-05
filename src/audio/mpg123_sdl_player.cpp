#include "audio/mpg123_sdl_player.hpp"
#include "audio/mp3_duration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace coverplayer::audio {

namespace {
constexpr int outputEncoding = MPG123_ENC_SIGNED_16;
constexpr std::size_t minimumDecodeBuffer = 16 * 1024;

#ifdef _WIN32
// libmpg123's mpg123_open() takes a plain narrow-char path and, on Windows,
// resolves it through a narrow (ANSI code page) file API rather than UTF-8
// - the same class of bug fixed for estimateMp3DurationSeconds, but this
// one is a C API with no std::filesystem overload to fall back on. A
// filename with a non-ASCII character (e.g. a German umlaut) then fails to
// open even though the file exists.
//
// The 8.3 "short" path for any existing file is pure ASCII by definition,
// so converting to it sidesteps the encoding problem entirely without
// needing mpg123's fd-based open (and the file-ownership questions that
// would come with it). Falls back to the original UTF-8 path unchanged if
// short-name generation is unavailable (rare, e.g. disabled at the volume
// level) - exactly the behavior before this fix for an ASCII-only path,
// since GetShortPathNameW is a no-op for those.
std::string toOpenablePath(const std::string& utf8Path) {
    const int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
    if (wideLength <= 0) return utf8Path;
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, wide.data(), wideLength);

    std::wstring shortPath(MAX_PATH, L'\0');
    DWORD shortLength = GetShortPathNameW(wide.c_str(), shortPath.data(), static_cast<DWORD>(shortPath.size()));
    if (shortLength == 0) return utf8Path;
    if (shortLength >= shortPath.size()) {
        shortPath.resize(shortLength);
        shortLength = GetShortPathNameW(wide.c_str(), shortPath.data(), static_cast<DWORD>(shortPath.size()));
        if (shortLength == 0) return utf8Path;
    }
    std::string ascii(shortLength, '\0');
    for (DWORD i = 0; i < shortLength; ++i) ascii[i] = static_cast<char>(shortPath[i]);
    return ascii;
}
#else
std::string toOpenablePath(const std::string& utf8Path) { return utf8Path; }
#endif

// A handful of files that are not actually MP3 despite the extension (seen
// in practice: an M4A/AAC file saved by a browser video-downloader
// extension) have no valid MPEG frame anywhere in them - no resync limit,
// however large, would ever find one. Checking a well-known container
// signature in the first few bytes costs one tiny read and fails instantly
// with a clear reason, instead of mpg123 grinding through megabytes of
// "junk" only to report an opaque resync error.
std::string detectUnsupportedContainer(const std::string& path) {
    std::ifstream probe(std::filesystem::u8path(path), std::ios::binary);
    std::array<unsigned char, 12> header{};
    probe.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (probe.gcount() < 8) return {};
    if (header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p') return "M4A/MP4";
    if (header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') return "WAV";
    if (header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S') return "OGG";
    if (header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'C') return "FLAC";
    return {};
}
}

Mpg123SdlPlayer::Mpg123SdlPlayer() {
    if (mpg123_init() != MPG123_OK) {
        throw std::runtime_error("mpg123 initialization failed");
    }
}

Mpg123SdlPlayer::~Mpg123SdlPlayer() {
    close();
    mpg123_exit();
}

bool Mpg123SdlPlayer::open(const std::string& path) {
    close();
    error_.clear();
    if (const auto container = detectUnsupportedContainer(path); !container.empty()) {
        setError("Keine MP3-Datei (" + container + " erkannt)");
        return false;
    }
    int decoderError = MPG123_OK;
    decoder_ = mpg123_new(nullptr, &decoderError);
    if (decoder_ == nullptr) {
        setError("mpg123_new failed: " + std::to_string(decoderError));
        return false;
    }
    // mpg123's default is to give up looking for the first valid MPEG frame
    // after 64 KiB of unrecognized leading bytes ("resync limit"). Real
    // files occasionally exceed that - e.g. ones saved by browser video-
    // downloader extensions, which can prepend non-standard leading data
    // with no ID3v2 tag framing mpg123 could skip via a declared size -
    // and fail to open entirely even though the audio itself is fine.
    // Raise the search window well past any realistic case rather than
    // scanning genuinely unbounded (still finite, unlike this app's other
    // "never scan the whole file" guarantees, which are about avoiding a
    // *repeated* full-file scan during playback, not this one-time,
    // bounded search at open time).
    mpg123_param(decoder_, MPG123_RESYNC_LIMIT, 8L * 1024 * 1024, 0.0);
    if (mpg123_open(decoder_, toOpenablePath(path).c_str()) != MPG123_OK) {
        setError(std::string("MP3 open failed: ") + mpg123_strerror(decoder_));
        close();
        return false;
    }
    if (!configureOutput()) {
        close();
        return false;
    }
    durationSeconds_ = estimateMp3DurationSeconds(path);
    open_ = true;
    paused_ = false;
    eofDecoded_ = false;
    finished_ = false;
    update();
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

bool Mpg123SdlPlayer::configureOutput() {
    int encoding = 0;
    if (mpg123_getformat(decoder_, &sampleRate_, &channels_, &encoding) != MPG123_OK) {
        setError(std::string("MP3 format failed: ") + mpg123_strerror(decoder_));
        return false;
    }
    if (channels_ < 1 || channels_ > 2) {
        setError("Unsupported MP3 channel count: " + std::to_string(channels_));
        return false;
    }
    mpg123_format_none(decoder_);
    if (mpg123_format(decoder_, sampleRate_, channels_, outputEncoding) != MPG123_OK) {
        setError(std::string("MP3 output format failed: ") + mpg123_strerror(decoder_));
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(sampleRate_);
    desired.format = AUDIO_S16SYS;
    desired.channels = static_cast<Uint8>(channels_);
    desired.samples = 2048;
    desired.callback = nullptr;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (device_ == 0) {
        setError(std::string("SDL audio open failed: ") + SDL_GetError());
        return false;
    }

    bytesPerSecond_ = static_cast<std::size_t>(sampleRate_) * static_cast<std::size_t>(channels_) * 2;
    targetQueueBytes_ = bytesPerSecond_ / 2;
    decodeBuffer_.resize(std::max(minimumDecodeBuffer, mpg123_outblock(decoder_)));
    return true;
}

void Mpg123SdlPlayer::update() {
    if (!open_ || paused_ || finished_ || device_ == 0) {
        return;
    }
    if (eofDecoded_) {
        finished_ = SDL_GetQueuedAudioSize(device_) == 0;
        if (finished_) durationSeconds_ = std::max(durationSeconds_, positionSeconds());
        return;
    }
    while (SDL_GetQueuedAudioSize(device_) < targetQueueBytes_) {
        std::size_t decodedBytes = 0;
        const int result = mpg123_read(decoder_, decodeBuffer_.data(), decodeBuffer_.size(), &decodedBytes);
        if (decodedBytes > 0 && volumePercent_ < 100) {
            auto* samples = reinterpret_cast<std::int16_t*>(decodeBuffer_.data());
            const auto sampleCount = decodedBytes / sizeof(std::int16_t);
            for (std::size_t i = 0; i < sampleCount; ++i) {
                samples[i] = static_cast<std::int16_t>((static_cast<int>(samples[i]) * volumePercent_) / 100);
            }
        }
        if (decodedBytes > 0 && SDL_QueueAudio(device_, decodeBuffer_.data(), static_cast<Uint32>(decodedBytes)) != 0) {
            setError(std::string("SDL audio queue failed: ") + SDL_GetError());
            finished_ = true;
            return;
        }
        if (result == MPG123_DONE) {
            eofDecoded_ = true;
            return;
        }
        if (result != MPG123_OK && result != MPG123_NEW_FORMAT) {
            setError(std::string("MP3 decode failed: ") + mpg123_strerror(decoder_));
            finished_ = true;
            return;
        }
        if (decodedBytes == 0 && result == MPG123_OK) {
            return;
        }
    }
}

void Mpg123SdlPlayer::togglePause() {
    if (!open_ || finished_) {
        return;
    }
    paused_ = !paused_;
    SDL_PauseAudioDevice(device_, paused_ ? 1 : 0);
}

void Mpg123SdlPlayer::seekSeconds(double delta) {
    if (!open_ || finished_ || sampleRate_ <= 0) {
        return;
    }
    double target = std::max(0.0, positionSeconds() + delta);
    if (durationSeconds_ > 0.0) target = std::min(target, durationSeconds_);
    SDL_ClearQueuedAudio(device_);
    if (mpg123_seek(decoder_, static_cast<off_t>(std::llround(target * sampleRate_)), SEEK_SET) < 0) {
        setError(std::string("MP3 seek failed: ") + mpg123_strerror(decoder_));
        return;
    }
    eofDecoded_ = false;
    finished_ = false;
    update();
}

void Mpg123SdlPlayer::setVolumePercent(int percent) { volumePercent_=std::max(0,std::min(100,percent)); }
int Mpg123SdlPlayer::volumePercent() const noexcept { return volumePercent_; }
double Mpg123SdlPlayer::durationSeconds() const noexcept { return durationSeconds_; }

double Mpg123SdlPlayer::positionSeconds() const noexcept {
    if (!open_ || decoder_ == nullptr || sampleRate_ <= 0) {
        return 0.0;
    }
    const off_t decodedSample = mpg123_tell(decoder_);
    const double queuedSeconds = device_ == 0 || bytesPerSecond_ == 0
        ? 0.0
        : static_cast<double>(SDL_GetQueuedAudioSize(device_)) / static_cast<double>(bytesPerSecond_);
    return std::max(0.0, static_cast<double>(decodedSample) / sampleRate_ - queuedSeconds);
}

bool Mpg123SdlPlayer::isOpen() const noexcept { return open_; }
bool Mpg123SdlPlayer::isPaused() const noexcept { return paused_; }
bool Mpg123SdlPlayer::isFinished() const noexcept { return finished_; }
std::string Mpg123SdlPlayer::error() const { return error_; }

void Mpg123SdlPlayer::setError(const std::string& message) { error_ = message; }

void Mpg123SdlPlayer::close() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    if (decoder_ != nullptr) {
        mpg123_close(decoder_);
        mpg123_delete(decoder_);
        decoder_ = nullptr;
    }
    decodeBuffer_.clear();
    open_ = false;
    paused_ = false;
    eofDecoded_ = false;
    finished_ = false;
    durationSeconds_ = 0.0;
}

} // namespace coverplayer::audio
