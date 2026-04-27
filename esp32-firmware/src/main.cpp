#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "sensor.h"
#include "camera.h"
#include "crypto.h"
#include "display.h"
#include "relay.h"
#include "wifi_comm.h"

static String get_timestamp() {
    struct tm ti;
    if (!getLocalTime(&ti)) return "1970-01-01T00:00:00Z";
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &ti);
    return String(buf);
}

static String parse_json_string(const String& json, const String& key) {
    String search = "\"" + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf("\"", start);
    return (end > start) ? json.substring(start, end) : "";
}

static bool handle_manual_controls() {
    static bool pressed_prev = false;
    static unsigned long pressed_at_ms = 0;

    bool pressed = digitalRead(OVERRIDE_BUTTON_PIN) == LOW;
    if (pressed && !pressed_prev) {
        pressed_at_ms = millis();
    }

    if (!pressed && pressed_prev) {
        const unsigned long held_ms = millis() - pressed_at_ms;

        if (held_ms >= RESET_HOLD_MS) {
            display_message("Manual Reset", "Restarting...");
            delay(250);
            ESP.restart();
        }

        if (held_ms >= OVERRIDE_MIN_PRESS_MS) {
            Serial.println("[INFO] Manual override button pressed");
            display_message("Manual Override", "Opening Barrier");
            relay_open();
            delay(RELAY_ACTIVE_TIME_MS);
            relay_close();
            display_message("System Ready", "Monitoring...");
            pressed_prev = pressed;
            return true;
        }
    }

    pressed_prev = pressed;
    return false;
}

void setup() {
    Serial.begin(115200);

    if (!display_init()) {
        Serial.println("[WARN] OLED init failed, continuing without display");
    }
    display_message("ANPR System", "Initializing...");

    sensor_init();
    relay_init();
    pinMode(OVERRIDE_BUTTON_PIN, INPUT_PULLUP);

    if (!camera_init()) {
        display_message("Camera Error!", "Halted");
        Serial.println("[FATAL] Camera init failed");
        while (true) delay(1000);
    }

    display_message("Connecting WiFi...", WIFI_SSID);
    if (!wifi_connect()) {
        display_message("WiFi Failed!", "Check config");
        Serial.println("[FATAL] WiFi connect failed");
        while (true) delay(1000);
    }

    configTime(0, 0, "pool.ntp.org");

    display_message("System Ready", "Monitoring...");
    Serial.println("[INFO] ANPR system ready");
}

void loop() {
    if (handle_manual_controls()) {
        delay(200);
        return;
    }

    float dist = sensor_get_distance_cm();

    if (dist > 0.0f && dist < TRIGGER_THRESHOLD_CM) {
        // Debounce: confirm on second consecutive reading
        delay(200);
        float dist2 = sensor_get_distance_cm();
        if (dist2 <= 0.0f || dist2 >= TRIGGER_THRESHOLD_CM) return;

        Serial.printf("[INFO] Vehicle detected at %.1f cm\n", dist2);
        display_message("Vehicle Detected", "Processing...");

        camera_fb_t* fb = camera_capture();
        if (!fb) {
            display_message("Camera Error", "Skipping...");
            delay(2000);
            return;
        }

        String b64_img = base64_encode_buf(fb->buf, fb->len);
        camera_return_fb(fb);

        String xml      = build_xml_payload(b64_img, get_timestamp());
        String payload  = aes256_cbc_encrypt_b64(xml);

        display_message("Sending...", "");
        String resp = http_post_encrypted(payload);

        if (resp.length() == 0) {
            display_message("Network Error", "Check server");
            delay(3000);
            display_message("System Ready", "Monitoring...");
            return;
        }

        String plate = parse_json_string(resp, "plate");
        bool granted  = resp.indexOf("\"access_granted\":true") >= 0;

        if (granted) {
            display_message("ACCESS GRANTED", plate);
            relay_open();
            delay(RELAY_ACTIVE_TIME_MS);
            relay_close();
        } else {
            display_message("ACCESS DENIED", plate);
            delay(3000);
        }

        display_message("System Ready", "Monitoring...");
    }

    delay(200);
}
