#pragma once

#include <optional>

namespace coverplayer::platform {

// Reads remaining charge from the kernel's power-supply sysfs tree
// (/sys/class/power_supply/<name>/{type,capacity}). Returns nullopt on any
// platform or device without a reachable "Battery"-typed entry - including
// desktop Windows/Linux, where the whole tree is simply absent - so callers
// never need a separate capability flag to stay silent there.
std::optional<int> readBatteryPercent();

} // namespace coverplayer::platform
