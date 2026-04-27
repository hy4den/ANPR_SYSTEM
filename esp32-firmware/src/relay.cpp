#include "relay.h"
#include "config.h"

void relay_init() {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
}

void relay_open() {
    digitalWrite(RELAY_PIN, HIGH);
}

void relay_close() {
    digitalWrite(RELAY_PIN, LOW);
}
