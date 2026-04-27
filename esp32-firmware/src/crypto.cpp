#include "crypto.h"
#include "config.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

String base64_encode_buf(const uint8_t* data, size_t length) {
    size_t out_len = 0;
    mbedtls_base64_encode(nullptr, 0, &out_len, data, length);

    uint8_t* buf = new uint8_t[out_len + 1];
    mbedtls_base64_encode(buf, out_len + 1, &out_len, data, length);
    buf[out_len] = '\0';

    String result((char*)buf);
    delete[] buf;
    return result;
}

String aes256_cbc_encrypt_b64(const String& plaintext) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    uint8_t key[32];
    memcpy(key, AES_KEY, 32);
    mbedtls_aes_setkey_enc(&ctx, key, 256);

    // PKCS7 padding to next 16-byte boundary
    size_t len = plaintext.length();
    size_t padded_len = ((len / 16) + 1) * 16;
    uint8_t pad_val = (uint8_t)(padded_len - len);

    uint8_t* padded = new uint8_t[padded_len];
    memcpy(padded, plaintext.c_str(), len);
    for (size_t i = len; i < padded_len; i++) padded[i] = pad_val;

    uint8_t* encrypted = new uint8_t[padded_len];
    uint8_t iv[16];
    memcpy(iv, AES_IV, 16);

    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, padded_len, iv, padded, encrypted);
    mbedtls_aes_free(&ctx);

    String result = base64_encode_buf(encrypted, padded_len);
    delete[] padded;
    delete[] encrypted;
    return result;
}

String build_xml_payload(const String& b64_image, const String& timestamp) {
    String xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xml += "<anpr_payload>";
    xml += "<device_id>ESP32-CAM-001</device_id>";
    xml += "<timestamp>" + timestamp + "</timestamp>";
    xml += "<image>" + b64_image + "</image>";
    xml += "</anpr_payload>";
    return xml;
}
