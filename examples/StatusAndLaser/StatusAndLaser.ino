/*
  StatusAndLaser — X40 Laser Distance Meter
  Demonstrates the full S → O → D → C command sequence:
    S  Read temperature and supply voltage
    O  Turn laser on
    D  Measure distance (single shot)
    C  Turn laser off

  Wiring (ESP32 example):
    Module VCC  -> 3.3 V supply  (NOT 5 V!)
    Module GND  -> GND
    Module TX   -> GPIO 16 (RX2)
    Module RX   -> GPIO 17 (TX2)  use voltage divider if MCU is 5 V
*/

#include <X40LaserDistanceMeter.h>

static const int RX_PIN = 16;
static const int TX_PIN = 17;

HardwareSerial ModuleSerial(2);
X40LaserDistanceMeter laser(&ModuleSerial);

void printStatus(const char* label, bool ok) {
    Serial.print(label);
    Serial.println(ok ? "OK" : "FAILED");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    laser.begin(19200, RX_PIN, TX_PIN);
    delay(300);

    // ---- S: read module status ----
    float tempC, supplyV;
    if (laser.readStatus(&tempC, &supplyV)) {
        Serial.print("Status  temp=");
        Serial.print(tempC, 1);
        Serial.print(" C  voltage=");
        Serial.print(supplyV, 2);
        Serial.println(" V");
    } else {
        Serial.print("Status read failed  raw: ");
        Serial.println(laser.lastRawResponse());
    }

    // ---- O: laser on ----
    printStatus("Laser ON  -> ", laser.controlLaser(X40_LASER_ON));
    delay(200);

    // ---- D: single measurement ----
    float distance;
    if (laser.singleMeasurement(distance)) {
        Serial.print("Distance: ");
        Serial.print(distance, 3);
        Serial.print(" m  (");
        Serial.print((int32_t)(distance * 1000.0f + 0.5f));
        Serial.println(" mm)");
    } else {
        Serial.print("Measurement failed  raw: ");
        Serial.println(laser.lastRawResponse());
    }

    // ---- C: laser off ----
    printStatus("Laser OFF -> ", laser.controlLaser(X40_LASER_OFF));

    // ---- Optional: version string ----
    String version;
    if (laser.readVersion(&version)) {
        Serial.print("Version: ");
        Serial.println(version);
    }

    Serial.println("Done.");
}

void loop() {}
