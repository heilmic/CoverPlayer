#include "platform/sdl/linux_battery.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace coverplayer::platform {
namespace {

bool isBatteryEntry(const std::filesystem::path& entry) {
    std::ifstream type(entry / "type");
    std::string value;
    return static_cast<bool>(std::getline(type, value)) && value == "Battery";
}

} // namespace

std::optional<int> readBatteryPercent() {
    std::error_code listError;
    std::filesystem::directory_iterator entries("/sys/class/power_supply", listError);
    if (listError) return std::nullopt;
    for (const auto& entry : entries) {
        if (!isBatteryEntry(entry.path())) continue;
        std::ifstream capacity(entry.path() / "capacity");
        int percent = -1;
        capacity >> percent;
        if (!capacity.fail() && percent >= 0 && percent <= 100) return percent;
    }
    return std::nullopt;
}

} // namespace coverplayer::platform
