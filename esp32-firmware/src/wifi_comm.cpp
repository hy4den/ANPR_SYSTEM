#include "wifi_comm.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#if SERVER_USE_TLS
#include <WiFiClientSecure.h>
#endif

bool wifi_connect() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_ensure_connected() {
    if (WiFi.status() == WL_CONNECTED) return true;
    for (int i = 0; i < 5; i++) {
        WiFi.reconnect();
        delay(2000);
        if (WiFi.status() == WL_CONNECTED) return true;
    }
    return false;
}

String http_post_encrypted(const String& encrypted_b64) {
    if (!wifi_ensure_connected()) return "";

    HTTPClient http;
#if SERVER_USE_TLS
    WiFiClientSecure tls_client;
    if (String(SERVER_CA_CERT_PEM).length() > 0)
        tls_client.setCACert(SERVER_CA_CERT_PEM);
    else
        tls_client.setInsecure(); // Local/dev fallback when no CA cert is pinned
    String url = String("https://") + SERVER_HOST + ":" + SERVER_TLS_PORT + SERVER_ENDPOINT;
    http.begin(tls_client, url);
#else
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_ENDPOINT;
    http.begin(url);
#endif
    http.addHeader("Content-Type", "text/plain");
    http.setTimeout(10000);

    int code = http.POST((uint8_t*)encrypted_b64.c_str(), encrypted_b64.length());

    String response = "";
    if (code == 200) {
        response = http.getString();
    } else {
        Serial.printf("[HTTP] POST failed, code: %d\n", code);
    }

    http.end();
    return response;
}
