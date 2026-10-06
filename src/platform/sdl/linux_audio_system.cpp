#include "platform/sdl/linux_audio_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <sstream>

namespace coverplayer::platform {
namespace {

std::string runCommand(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen((command + " 2>nul").c_str(), "r");
#else
    FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r");
#endif
    if (pipe == nullptr) return {};
    std::string output;
    char buffer[1024]{};
    while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) output += buffer;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return output;
}

bool validBluetoothAddress(const std::string& address) {
    static const std::regex pattern("^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$");
    return std::regex_match(address, pattern);
}

std::string sinkAddress(std::string address) {
    std::replace(address.begin(), address.end(), ':', '_');
    return address;
}

std::string bluetoothDisplayName(const std::string& name, const std::string& address) {
    std::string value;
    for (const unsigned char character : name) if (character < 128U) value.push_back(static_cast<char>(character));
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return address;
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string trimLine(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) value.pop_back();
    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? std::string{} : value.substr(first);
}

std::optional<int> firstPercent(const std::string& output) {
    const auto percent = output.find('%');
    if (percent == std::string::npos) return std::nullopt;
    auto start = percent;
    while (start > 0 && output[start - 1] >= '0' && output[start - 1] <= '9') --start;
    if (start == percent) return std::nullopt;
    return std::clamp(std::stoi(output.substr(start, percent - start)), 0, 100);
}

std::optional<int> wpctlPercent(const std::string& output) {
    std::istringstream stream(output);
    std::string label;
    double fraction = 0.0;
    if (!(stream >> label >> fraction) || label != "Volume:" || !std::isfinite(fraction)) return std::nullopt;
    return static_cast<int>(std::lround(std::clamp(fraction, 0.0, 1.0) * 100.0));
}

bool validSinkName(const std::string& sink) {
    static const std::regex pattern("^[A-Za-z0-9_.:-]+$");
    return !sink.empty() && std::regex_match(sink, pattern);
}

struct AudioOutputState {
    std::string sink;
    int volume = 0;
    bool muted = false;
    bool valid = false;
};

AudioOutputState readDefaultAudioOutput() {
    AudioOutputState state;
    state.sink = trimLine(runCommand("LC_ALL=C /usr/bin/pactl get-default-sink"));
    const auto volume = firstPercent(runCommand("LC_ALL=C /usr/bin/pactl get-sink-volume @DEFAULT_SINK@"));
    const auto mute = runCommand("LC_ALL=C /usr/bin/pactl get-sink-mute @DEFAULT_SINK@");
    if (validSinkName(state.sink) && volume && (mute.find("yes") != std::string::npos || mute.find("no") != std::string::npos)) {
        state.volume = *volume;
        state.muted = mute.find("yes") != std::string::npos;
        state.valid = true;
    }
    return state;
}

std::string findBluetoothSink(const std::string& address) {
    const auto needle = sinkAddress(address);
    std::istringstream lines(runCommand("LC_ALL=C /usr/bin/pactl list short sinks"));
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        std::string index, sink;
        if (fields >> index >> sink && sink.find(needle) != std::string::npos && validSinkName(sink)) return sink;
    }
    return {};
}

bool applyAudioOutputState(const std::string& sink, const AudioOutputState& state) {
    if (!validSinkName(sink) || !state.valid) return false;
    const std::string prefix = "/usr/bin/pactl set-sink-";
    if (std::system((prefix + "mute " + sink + " 1 >/dev/null 2>&1").c_str()) != 0) return false;
    if (std::system((prefix + "volume " + sink + " " + std::to_string(state.volume) + "% >/dev/null 2>&1").c_str()) != 0) return false;
    if (!state.muted && std::system((prefix + "mute " + sink + " 0 >/dev/null 2>&1").c_str()) != 0) return false;
    return true;
}

} // namespace

LinuxAudioSystem::LinuxAudioSystem() {
    if (const char* backend = std::getenv("COVERPLAYER_VOLUME_BACKEND");
        backend != nullptr && std::string(backend) == "wpctl") {
        volumeBackend_ = VolumeBackend::Wpctl;
    } else if (std::getenv("COVERPLAYER_SYSTEM_VOLUME") != nullptr) {
        volumeBackend_ = VolumeBackend::Pactl;
    }
    systemVolumeEnabled_ = volumeBackend_ != VolumeBackend::None;
    bluetoothEnabled_ = std::getenv("COVERPLAYER_BLUETOOTH") != nullptr;
    if (const char* initialVolume = std::getenv("COVERPLAYER_INITIAL_VOLUME")) {
        char* end = nullptr;
        const long value = std::strtol(initialVolume, &end, 10);
        if (end != initialVolume) systemVolumePercent_ = std::clamp(static_cast<int>(value), 0, 100);
    }
    if (systemVolumeEnabled_) refreshSystemVolume();
    if (bluetoothEnabled_) refreshBluetoothAudioStatus();
}

std::optional<int> LinuxAudioSystem::adjustSystemVolume(int deltaPercent) {
    if (!systemVolumeEnabled_) return std::nullopt;
    if (!systemVolumePercent_) refreshSystemVolume();
    if (!systemVolumePercent_) return std::nullopt;
    const int target = std::clamp(*systemVolumePercent_ + deltaPercent, 0, 100);
    const bool useWpctl = volumeBackend_ == VolumeBackend::Wpctl;
    const std::string volumeCommand = useWpctl
        ? "/usr/bin/wpctl set-volume @DEFAULT_AUDIO_SINK@ " + std::to_string(target) + "% >/dev/null 2>&1"
        : "/usr/bin/pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(target) + "% >/dev/null 2>&1";
    const std::string muteCommand = (useWpctl
        ? "/usr/bin/wpctl set-mute @DEFAULT_AUDIO_SINK@ "
        : "/usr/bin/pactl set-sink-mute @DEFAULT_SINK@ ") + std::string(target == 0 ? "1" : "0") + " >/dev/null 2>&1";
    if (std::system(volumeCommand.c_str()) == 0 && std::system(muteCommand.c_str()) == 0) {
        systemVolumePercent_ = target;
        systemVolumeMuted_ = target == 0;
    }
    volumeRefreshAt_ = SDL_GetTicks();
    return systemVolumePercent_;
}

void LinuxAudioSystem::refreshSystemVolume() {
    volumeRefreshAt_ = SDL_GetTicks();
    if (volumeBackend_ == VolumeBackend::Wpctl) {
        const auto output = runCommand("LC_ALL=C /usr/bin/wpctl get-volume @DEFAULT_AUDIO_SINK@");
        const auto volume = wpctlPercent(output);
        if (!volume) return;
        systemVolumeMuted_ = output.find("[MUTED]") != std::string::npos;
        systemVolumePercent_ = systemVolumeMuted_ ? 0 : *volume;
        return;
    }
    // The display only needs the active output's volume and mute state.
    // Requiring get-default-sink as well can leave it stuck at the launcher's
    // initial value when that separate lookup briefly fails on Knulli. Force
    // the C locale: Knulli sets LC_ALL=de_DE.UTF-8, which overrides LANG=C
    // and otherwise makes pactl report "Mute: nein" instead of "Mute: no".
    const auto volume = firstPercent(runCommand("LC_ALL=C /usr/bin/pactl get-sink-volume @DEFAULT_SINK@"));
    const auto mute = trimLine(runCommand("LC_ALL=C /usr/bin/pactl get-sink-mute @DEFAULT_SINK@"));
    if (!volume || (mute != "Mute: yes" && mute != "Mute: no")) return;
    systemVolumeMuted_ = mute == "Mute: yes";
    systemVolumePercent_ = systemVolumeMuted_ ? 0 : *volume;
}

void LinuxAudioSystem::refreshBluetoothAudioStatus() {
    bluetoothRefreshAt_ = SDL_GetTicks();
    auto sink = runCommand("/usr/bin/pactl get-default-sink");
    while (!sink.empty() && (sink.back() == '\r' || sink.back() == '\n')) sink.pop_back();
    bluetoothAudioActive_ = sink.rfind("bluez_output.", 0) == 0;
}

BluetoothState LinuxAudioSystem::bluetoothState() {
    BluetoothState state;
    state.available = bluetoothEnabled_;
    if (!bluetoothEnabled_) return state;
    state.powered = runCommand("/usr/bin/timeout 5 /usr/bin/bluetoothctl show").find("Powered: yes") != std::string::npos;
    auto sink = runCommand("/usr/bin/pactl get-default-sink");
    while (!sink.empty() && (sink.back() == '\r' || sink.back() == '\n')) sink.pop_back();
    state.audioActive = sink.rfind("bluez_output.", 0) == 0;
    bluetoothAudioActive_ = state.audioActive;
    bluetoothRefreshAt_ = SDL_GetTicks();
    const auto listing = runCommand("/usr/bin/timeout 5 /usr/bin/knulli-bluetooth list");
    const std::regex devicePattern("<device[^>]*id=\"([^\"]+)\"[^>]*name=\"([^\"]*)\"[^>]*connected=\"([^\"]*)\"");
    for (auto match = std::sregex_iterator(listing.begin(), listing.end(), devicePattern);
         match != std::sregex_iterator(); ++match) {
        const std::string address = (*match)[1].str();
        if (!validBluetoothAddress(address)) continue;
        const auto info = runCommand("/usr/bin/timeout 3 /usr/bin/bluetoothctl info " + address);
        if (info.find("Audio Sink") == std::string::npos && info.find("Icon: audio-") == std::string::npos) continue;
        BluetoothDevice device;
        device.address = address;
        device.name = bluetoothDisplayName((*match)[2].str(), address);
        device.connected = (*match)[3].str() == "yes";
        device.active = sink.find(sinkAddress(address)) != std::string::npos;
        if (device.active) device.connected = true;
        state.devices.push_back(std::move(device));
    }
    std::sort(state.devices.begin(), state.devices.end(), [](const auto& left, const auto& right) {
        return left.name < right.name;
    });
    return state;
}

bool LinuxAudioSystem::setBluetoothEnabled(bool enabled) {
    if (!bluetoothEnabled_) return false;
    const std::string command = std::string("/usr/bin/timeout 10 /usr/bin/knulli-bluetooth ") +
        (enabled ? "enable" : "disable") + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

bool LinuxAudioSystem::setBluetoothDeviceConnected(const std::string& address, bool connected) {
    // This whole function fails safe rather than risking an uncontrolled
    // volume jump on the wrong output (see README: "Bluetooth output
    // changes use a safety handover"). Every early return below logs
    // exactly which check failed - without it, "connect blocked" and "the
    // safety check itself is broken" look identical from the UI alone.
    if (!bluetoothEnabled_ || !validBluetoothAddress(address)) {
        std::cerr << "CoverPlayer Bluetooth: rejected request for '" << address << "' (enabled="
            << bluetoothEnabled_ << ")\n";
        return false;
    }
    const auto previous = readDefaultAudioOutput();
    if (!previous.valid) {
        std::cerr << "CoverPlayer Bluetooth: current output state unreadable before "
            << (connected ? "connecting " : "disconnecting ") << address
            << " (sink='" << previous.sink << "') - refusing to switch output without a known volume to restore\n";
        return false;
    }
    const std::string command = std::string("/usr/bin/timeout 15 /usr/bin/knulli-bluetooth ") +
        (connected ? "connect " : "disconnect ") + address + " >/dev/null 2>&1";
    const int commandStatus = std::system(command.c_str());
    if (commandStatus != 0) {
        std::cerr << "CoverPlayer Bluetooth: 'knulli-bluetooth "
            << (connected ? "connect " : "disconnect ") << address << "' exited with status " << commandStatus << '\n';
        return false;
    }

    std::string targetSink;
    const auto addressToken = sinkAddress(address);
    // A2DP profile negotiation on first connect to a device can run past
    // the 4s this used to allow; 8s gives real hardware more room without
    // meaningfully lengthening the failure case (still fails fast whenever
    // the sink never appears at all).
    constexpr int maxAttempts = 80;
    for (int attempt = 0; attempt < maxAttempts && targetSink.empty(); ++attempt) {
        const auto current = readDefaultAudioOutput();
        if (current.valid && (connected ? current.sink.find(addressToken) != std::string::npos
                                       : current.sink.find(addressToken) == std::string::npos)) targetSink = current.sink;
        if (connected && targetSink.empty()) targetSink = findBluetoothSink(address);
        if (targetSink.empty()) SDL_Delay(100);
    }
    if (targetSink.empty()) {
        std::cerr << "CoverPlayer Bluetooth: no matching sink for " << address << " found within "
            << (maxAttempts * 100) << "ms after knulli-bluetooth reported success\n";
    }
    if (targetSink.empty() || !applyAudioOutputState(targetSink, previous)) {
        if (!targetSink.empty()) {
            std::cerr << "CoverPlayer Bluetooth: failed to copy volume/mute state onto sink '" << targetSink << "'\n";
        }
        std::system("/usr/bin/pactl set-sink-mute @DEFAULT_SINK@ 1 >/dev/null 2>&1");
        refreshSystemVolume();
        refreshBluetoothAudioStatus();
        return false;
    }
    systemVolumeMuted_ = previous.muted;
    systemVolumePercent_ = previous.muted ? 0 : previous.volume;
    refreshBluetoothAudioStatus();
    return true;
}

void LinuxAudioSystem::tick() {
    if (systemVolumeEnabled_ && SDL_GetTicks() - volumeRefreshAt_ >= 2000U) refreshSystemVolume();
    if (bluetoothEnabled_ && SDL_GetTicks() - bluetoothRefreshAt_ >= 5000U) refreshBluetoothAudioStatus();
}

} // namespace coverplayer::platform
