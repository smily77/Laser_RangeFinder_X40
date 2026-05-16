/*
  BasicRead — X40 Laser Distance Meter
  Performs a single distance measurement every second and prints
  the result in millimetres, metres, and signal quality (SQ).

  Wiring (ESP32 example):
    Module VCC  -> 3.3 V supply  (NOT 5 V!)
    Module GND  -> GND
    Module TX   -> GPIO 16 (RX2)
    Module RX   -> GPIO 17 (TX2)  use voltage divider if MCU is 5 V
*/

#include <X40LaserDistanceMeter.h>

// Change these pins to match your board
static const int RX_PIN = 16;
static const int TX_PIN = 17;

HardwareSerial ModuleSerial(2);
X40LaserDistanceMeter laser(&ModuleSerial);

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    laser.begin(19200, RX_PIN, TX_PIN);
    delay(300);

    Serial.println("X40 BasicRead ready");
}

void loop() {
    X40Measurement m = laser.measure();

    if (m.ok) {
        Serial.print("Distance: ");
        Serial.print(m.millimeters);
        Serial.print(" mm  (");
        Serial.print(m.meters, 3);
        Serial.print(" m)");
        if (m.signalQuality >= 0) {
            Serial.print("  SQ=");
            Serial.print(m.signalQuality);
        }
        Serial.println();
    } else {
        Serial.print("Error [status=");
        Serial.print(laser.lastStatus());
        if (laser.lastErrorCode() >= 0) {
            Serial.print("  code=");
            Serial.print(laser.lastErrorCode());
        }
        Serial.print("]  raw: ");
        Serial.println(laser.lastRawResponse());
    }

    delay(1000);
}
