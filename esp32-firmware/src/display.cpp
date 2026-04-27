#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>  // SSH1106 — 1.3" ekran icin dogru kutuphane

// SSH1106G: 132 sutunlu cip, ancak 128x64 piksel gosterilir.
// Adafruit_SSD1306 ile CALISMAZ — suruculer farkli.
static Adafruit_SH1106G oled(128, 64, &Wire, -1);

bool display_init() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    // SH110X begin(i2c_address, reset) — SSD1306'dan farkli imza
    if (!oled.begin(OLED_ADDRESS, true)) {
        return false;
    }
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);
    oled.display();
    return true;
}

void display_message(const String& line1, const String& line2) {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.setTextSize(1);
    oled.println(line1);
    if (line2.length() > 0) {
        oled.setCursor(0, 20);
        oled.println(line2);
    }
    oled.display();
}

void display_clear() {
    oled.clearDisplay();
    oled.display();
}
