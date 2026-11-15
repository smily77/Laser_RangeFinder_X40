/*
 * AutoDetect — one sketch for both the X-40 and the DFRobot SEN0366
 *
 * LaserRangefinderAuto finds out at begin() which rangefinder is connected
 * and then offers the SEN0366 API for both. Distances are in metres.
 *
 * UART: pass any free HardwareSerial. Do not call serial.begin() for it -
 * the library sets the baud rate itself (X40 19200, SEN0366 9600).
 *
 * Wiring (ESP32): sensor TX -> RX_PIN, sensor RX -> TX_PIN, GND, VCC 3.3 V.
 * Needs the library LaserRangefinder_SEN0366.
 */

#include <LaserRangefinderAuto.h>

#define RX_PIN 16
#define TX_PIN 17

#if defined(ESP32)
HardwareSerial       sensorSerial(2);          // or Serial1, HardwareSerial(1), ...
LaserRangefinderAuto laser(&sensorSerial);
#else
LaserRangefinderAuto laser(&Serial1);          // e.g. Arduino Mega: any free SerialN
#endif

void setup() {
    Serial.begin(115200);
    delay(500);

#if defined(ESP32)
    // true = also try with RX/TX swapped
    bool found = laser.begin(RX_PIN, TX_PIN, true);
#else
    bool found = laser.begin();
#endif
    Serial.print("Rangefinder: ");
    Serial.println(found ? laser.sensorName() : "not found");
    if (!found) while (true) delay(1000);

    laser.controlLaser(SEN0366_LASER_ON);
}

void loop() {
    float distance;
    if (laser.singleMeasurement(distance)) {
        Serial.print(laser.sensorName());
        Serial.print(": ");
        Serial.print(distance, 3);
        Serial.println(" m");
    } else {
        Serial.println("measurement failed");
    }
    // The SEN0366 switches the laser off after a single measurement
    laser.controlLaser(SEN0366_LASER_ON);
    delay(1000);
}
