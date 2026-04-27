#pragma once
#include <Arduino.h>

bool wifi_connect();
bool wifi_ensure_connected();
String http_post_encrypted(const String& encrypted_b64);
