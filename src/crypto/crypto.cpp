#include "crypto.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include <windows.h>
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#include <wincrypt.h>
#include <vector>
#include <random>
#include <cstring>

namespace ghost {
namespace crypto {

void Initialize() {
    uint8_t dummyKey[32];
    DeriveMachineKey(dummyKey, 32);
}

bool Encrypt(const uint8_t* plaintext, size_t len,
             std::vector<uint8_t>& ciphertext,
             const uint8_t* ad, size_t ad_len)
{
    uint8_t key[32];
    if (!DeriveMachineKey(key, 32)) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    bool success = false;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!NT_SUCCESS(status)) return false;

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                               sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key, 32, 0);
    if (!NT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    uint8_t nonce[12];
    status = BCryptGenRandom(nullptr, nonce, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!NT_SUCCESS(status)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    uint8_t tag[16];

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ainfo;
    BCRYPT_INIT_AUTH_MODE_INFO(ainfo);
    ainfo.pbNonce = nonce;
    ainfo.cbNonce = 12;
    ainfo.pbTag = tag;
    ainfo.cbTag = 16;
    ainfo.pbAuthData = (PUCHAR)ad;
    ainfo.cbAuthData = (ULONG)ad_len;

    ciphertext.resize(12 + len + 16);
    ULONG outLen = 0;

    memcpy(ciphertext.data(), nonce, 12);

    status = BCryptEncrypt(hKey,
                           (PUCHAR)plaintext,
                           (ULONG)len,
                           &ainfo,
                           nullptr,
                           0,
                           ciphertext.data() + 12,
                           (ULONG)len,
                           &outLen,
                           0);

    if (NT_SUCCESS(status)) {
        memcpy(ciphertext.data() + 12 + len, tag, 16);
        success = true;
    } else {
        ciphertext.clear(); 
    }

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return success;
}

bool Decrypt(const uint8_t* ciphertext, size_t len,
             std::vector<uint8_t>& plaintext,
             const uint8_t* ad, size_t ad_len)
{
    if (len < 28) return false;

    uint8_t key[32];
    if (!DeriveMachineKey(key, 32)) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!NT_SUCCESS(status)) return false;

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                               sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key, 32, 0);
    if (!NT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    const uint8_t* nonce = ciphertext;
    const uint8_t* tag = ciphertext + len - 16;
    const uint8_t* data = ciphertext + 12;
    size_t data_len = len - 28;

    plaintext.resize(data_len);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ainfo;
    BCRYPT_INIT_AUTH_MODE_INFO(ainfo);
    ainfo.pbNonce = (PUCHAR)nonce;
    ainfo.cbNonce = 12;
    ainfo.pbTag = (PUCHAR)tag;
    ainfo.cbTag = 16;
    ainfo.pbAuthData = (PUCHAR)ad;
    ainfo.cbAuthData = (ULONG)ad_len;

    ULONG outLen = 0;

    status = BCryptDecrypt(hKey,
                           (PUCHAR)data,
                           (ULONG)data_len,
                           &ainfo,
                           nullptr,
                           0,
                           plaintext.data(),
                           (ULONG)data_len,
                           &outLen,
                           0);

    if (!NT_SUCCESS(status)) {
        plaintext.clear();
    }

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return NT_SUCCESS(status);
}

bool Compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) {
    output = input; 
    return true;
}

bool Decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output = input;
    return true;
}

bool DeriveMachineKey(uint8_t* key_out, size_t key_len) {
    if (!key_out || key_len < 32) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    BYTE hash[32];
    DWORD hashLen = 0, objLen = 0;

    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) return false;

    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &hashLen, 0);

    std::vector<BYTE> obj(objLen);

    if (!NT_SUCCESS(BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    wchar_t guid[64] = {0};
    DWORD size = sizeof(guid);

    LSTATUS regStatus = RegGetValueW(HKEY_LOCAL_MACHINE,
                                     L"SOFTWARE\\Microsoft\\Cryptography",
                                     L"MachineGuid",
                                     RRF_RT_REG_SZ,
                                     nullptr,
                                     guid,
                                     &size);

    if (regStatus == ERROR_SUCCESS) {
        BCryptHashData(hHash, (PUCHAR)guid, size, 0);
    } else {
        const char* fallback = "DefaultFallbackKey123!";
        BCryptHashData(hHash, (PUCHAR)fallback, strlen(fallback), 0);
    }

    BCryptFinishHash(hHash, hash, 32, 0);

    memcpy(key_out, hash, 32);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return true;
}

void MutateCodeRegion(void* func_start, size_t func_size) {
}

}
}