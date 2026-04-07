#pragma once

#include <cstdint>
#include <vector>

namespace ghost {
namespace crypto {

void Initialize();

bool Encrypt(const uint8_t* plaintext, size_t len,
             std::vector<uint8_t>& ciphertext,
             const uint8_t* ad = nullptr, size_t ad_len = 0);

bool Decrypt(const uint8_t* ciphertext, size_t len,
             std::vector<uint8_t>& plaintext,
             const uint8_t* ad = nullptr, size_t ad_len = 0);

bool Compress(const std::vector<uint8_t>& input,
              std::vector<uint8_t>& output,
              int level = 9);

bool Decompress(const std::vector<uint8_t>& input,
                std::vector<uint8_t>& output);

bool DeriveMachineKey(uint8_t* key_out, size_t key_len);

void MutateCodeRegion(void* func_start, size_t func_size);

} // namespace crypto
} // namespace ghost