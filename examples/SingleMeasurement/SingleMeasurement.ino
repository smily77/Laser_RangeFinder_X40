/**
 * @file SingleMeasurement.ino
 * @brief Basic single measurement example for X40 Laser Distance Meter
 *
 * Performs one distance measurement every 2 seconds and prints the
 * result in metres, centimetres, and millimetres together with the
 * signal quality (SQ) value.
 *
 * Hardware Setup (ESP32):
 *   Module VCC  -> 3.3 V  (NOT 5 V!)
 *   Module GND  -> GND
 *   Module TX   -> GPIO 32 (RX2)
 *   Module RX   -> GPIO 33 (TX2)  use voltage divider if MCU is 5 V
 *
 * Hardware Setup (M5Stack Core2, Port A):
 *   Module TX   -> G32
 *   Module RX   -> G33
 *
 * Works with any board that has a HardwareSerial instance.
 */

#include <X40LaserDistanceMeter.h>

#if defined(ESP32)
  #define RXD2 32
  #define TXD2 33
  X40LaserDistanceMeter laser(&Serial2);
#elif defined(__AVR_ATmega2560__)
  X40LaserDistanceMeter laser(&Serial1);
#else
  #error "Please add HardwareSerial pins for your board"
#endif

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("=================================");
    Serial.println(" X40 Single Measurement Demo");
    Serial.println("=================================");
    Serial.println();
    Serial.println("WARNING: Module VCC must be 2.0-3.3 V — never 5 V!");
    Serial.println();

#if defined(ESP32)
    laser.begin(19200, RXD2, TXD2);
#else
    laser.begin(19200);
#endif
    delay(500);

    Serial.println("Ready — measurements every 2 s:");
    Serial.println("----------------------------------");
}

void loop() {
    X40Measurement m = laser.measure();

    if (m.ok) {
        Serial.print("Distance: ");
        Serial.print(m.meters, 3);
        Serial.print(" m | ");
        Serial.print(m.meters * 100.0f, 1);
        Serial.print(" cm | ");
        Serial.print(m.millimeters);
        Serial.print(" mm");
        if (m.signalQuality >= 0) {
            Serial.print(" | SQ=");
            Serial.print(m.signalQuality);
            Serial.print(" (lower=better)");
        }
        Serial.println();

        if (m.meters < 0.1f) {
            Serial.println("  → Very close!");
        } else if (m.meters < 1.0f) {
            Serial.println("  → Close range");
        } else if (m.meters < 10.0f) {
            Serial.println("  → Medium range");
        } else {
            Serial.println("  → Long range");
        }
    } else {
        Serial.print("Measurement failed  status=");
        Serial.print(laser.lastStatus());
        if (laser.lastErrorCode() >= 0) {
            Serial.print("  errorCode=");
            Serial.print(laser.lastErrorCode());
        }
        Serial.print("  raw: \"");
        Serial.print(laser.lastRawResponse());
        Serial.println("\"");
    }

    Serial.println();
    delay(2000);
}
