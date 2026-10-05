#pragma once

namespace coverplayer::platform {

struct Capabilities {
    bool systemVolume = false;
    bool bluetoothManagement = false;
    bool suspendResume = false;
};

} // namespace coverplayer::platform
