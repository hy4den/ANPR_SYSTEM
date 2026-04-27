#include "crypto.h"
#include "config.h"
#include <openssl/evp.h>
#include <cstring>
#include <stdexcept>
#include <array>

// ── Base64 (pure C++ — no external library needed) ────────────────────────────

static constexpr std::array<char, 64> B64_CHARS = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

static constexpr uint8_t B64_INV[256] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64, 0,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
};

std::vector<uint8_t> base64_decode(const std::string& b64) {
    size_t in_len = b64.size();
    if (in_len % 4 != 0)
        throw std::runtime_error("base64_decode: invalid length (not multiple of 4)");

    size_t out_len = (in_len / 4) * 3;
    if (in_len >= 1 && b64[in_len - 1] == '=') --out_len;
    if (in_len >= 2 && b64[in_len - 2] == '=') --out_len;

    std::vector<uint8_t> out(out_len);
    size_t j = 0;
    for (size_t i = 0; i < in_len; i += 4) {
        uint32_t a = B64_INV[static_cast<uint8_t>(b64[i])];
        uint32_t b = B64_INV[static_cast<uint8_t>(b64[i+1])];
        uint32_t c = B64_INV[static_cast<uint8_t>(b64[i+2])];
        uint32_t d = B64_INV[static_cast<uint8_t>(b64[i+3])];
        if (a == 64 || b == 64)
            throw std::runtime_error("base64_decode: invalid character");
        uint32_t triple = (a << 18) | (b << 12) | (c << 6) | d;
        if (j < out_len) out[j++] = static_cast<uint8_t>((triple >> 16) & 0xFF);
        if (j < out_len) out[j++] = static_cast<uint8_t>((triple >>  8) & 0xFF);
        if (j < out_len) out[j++] = static_cast<uint8_t>( triple        & 0xFF);
    }
    return out;
}

std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b  = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) b |= static_cast<uint32_t>(data[i+1]) << 8;
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i+2]);
        out += B64_CHARS[(b >> 18) & 0x3F];
        out += B64_CHARS[(b >> 12) & 0x3F];
        out += (i + 1 < len) ? B64_CHARS[(b >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? B64_CHARS[ b       & 0x3F] : '=';
    }
    return out;
}

// ── AES-256-CBC decrypt (OpenSSL EVP) ─────────────────────────────────────────

std::string aes256_decrypt_b64(const std::string& b64_ciphertext) {
    auto ciphertext = base64_decode(b64_ciphertext);

    if (ciphertext.empty() || ciphertext.size() % 16 != 0)
        throw std::runtime_error("aes256_decrypt: ciphertext not block-aligned");

    uint8_t key[32], iv[16];
    std::memcpy(key, AES_KEY, 32);
    std::memcpy(iv,  AES_IV,  16);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("aes256_decrypt: EVP_CIPHER_CTX_new failed");

    // EVP handles PKCS7 padding removal automatically (enabled by default)
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv)) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("aes256_decrypt: EVP_DecryptInit_ex failed");
    }

    std::vector<uint8_t> plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);
    int len1 = 0, len2 = 0;

    if (!EVP_DecryptUpdate(ctx, plaintext.data(), &len1,
                           ciphertext.data(),
                           static_cast<int>(ciphertext.size()))) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("aes256_decrypt: EVP_DecryptUpdate failed");
    }
    if (!EVP_DecryptFinal_ex(ctx, plaintext.data() + len1, &len2)) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("aes256_decrypt: EVP_DecryptFinal failed – wrong key or corrupt payload");
    }

    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(static_cast<size_t>(len1 + len2));
    return {plaintext.begin(), plaintext.end()};
}
