#include "sensor.h"
#include "config.h"

void sensor_init() {
    pinMode(SENSOR_TRIG_PIN, OUTPUT);
    pinMode(SENSOR_ECHO_PIN, INPUT);
    digitalWrite(SENSOR_TRIG_PIN, LOW);
}

float sensor_get_distance_cm() {
    digitalWrite(SENSOR_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(SENSOR_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SENSOR_TRIG_PIN, LOW);

    // Timeout at 30ms → max ~515 cm, beyond sensor range
    long duration = pulseIn(SENSOR_ECHO_PIN, HIGH, 30000UL);
    if (duration == 0) return -1.0f;

    return (duration * 0.0343f) / 2.0f;
}
