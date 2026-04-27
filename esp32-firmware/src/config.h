#pragma once

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ── Backend sunucu ────────────────────────────────────────────────────────────
// SERVER_HOST: backend'i calistiran bilgisayarin yerel IP adresi
// Bulmak icin: macOS terminalde  →  ipconfig getifaddr en0
#define SERVER_HOST     "192.168.1.45"   // <-- kendi IP'nizi yazin
#define SERVER_PORT     8000
#define SERVER_TLS_PORT 8443
#define SERVER_ENDPOINT "/api/anpr/process"
#define SERVER_USE_TLS  0   // 1 yaparsan HTTPS ile TLS_PORT'a baglanir

// SERVER_USE_TLS=1 oldugunda:
// - Asagidaki CA cert doldurulursa sertifika dogrulama aktif olur.
// - Bos birakilirsa istemci setInsecure() kullanir (sadece lokal test icin).
#define SERVER_CA_CERT_PEM ""

// ── AES-256 anahtari (backend/src/config.h ile BIREBIR eslesmeli) ─────────────
#define AES_KEY "0123456789ABCDEF0123456789ABCDEF"
#define AES_IV  "0000000000000000"

// ── HC-SR04 (STANDART 5V model — "+" degil!) ─────────────────────────────────
// ECHO pini 5V cikar; ESP32 GPIO'lari 3.3V.
// ZORUNLU voltaj bolucusu: ECHO --[10K]-- GPIO13 --[10K]-- GND
// Bu baglanti ile GPIO'ya 2.5V gider (guvenli).
// TRIG pini (GPIO12) dogrudan baglanabilir; 3.3V tetik icin yeterli.
#define SENSOR_TRIG_PIN      12
#define SENSOR_ECHO_PIN      13
#define TRIGGER_THRESHOLD_CM 50

// ── 5V Role ───────────────────────────────────────────────────────────────────
#define RELAY_PIN            2
#define RELAY_ACTIVE_TIME_MS 3000

// ── Manual override / reset button ─────────────────────────────────────────────
// Buton: GPIO -> GND, INPUT_PULLUP kullanilir (LOW=pressed)
#define OVERRIDE_BUTTON_PIN          33
#define OVERRIDE_MIN_PRESS_MS        80
#define RESET_HOLD_MS                4000

// ── SSH1106 OLED (1.3") — I2C ─────────────────────────────────────────────────
// Urun: SSH1106G surucu IC — Adafruit_SSD1306 ile CALISMAZ.
// Dogru kutuphane: Adafruit_SH110X (Arduino IDE Kutuphanesi)
#define OLED_SDA_PIN   14
#define OLED_SCL_PIN   15
#define OLED_ADDRESS   0x3C

// ── OV2640 kamera (AI-Thinker ESP32-CAM pin haritasi) ────────────────────────
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

#define CAMERA_FRAME_SIZE   FRAMESIZE_VGA
#define CAMERA_JPEG_QUALITY 12
