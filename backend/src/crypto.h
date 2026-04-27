#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Decodes a base64 string to raw bytes.
std::vector<uint8_t> base64_decode(const std::string& b64);

// Encodes raw bytes to base64.
std::string base64_encode(const uint8_t* data, size_t len);

// Decrypts AES-256-CBC base64 ciphertext → plaintext.
// Key and IV are taken from config.h (must match ESP32 firmware).
std::string aes256_decrypt_b64(const std::string& b64_ciphertext);
