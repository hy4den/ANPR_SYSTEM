#pragma once
#include <Arduino.h>

String base64_encode_buf(const uint8_t* data, size_t length);
String aes256_cbc_encrypt_b64(const String& plaintext);
String build_xml_payload(const String& b64_image, const String& timestamp);
