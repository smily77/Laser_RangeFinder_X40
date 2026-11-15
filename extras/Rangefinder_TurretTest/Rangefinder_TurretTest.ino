/*
 * Rangefinder_TurretTest — autonomer Test von LaserRangefinderAuto auf dem Pan/Tilt-Turm
 *
 * Erkennt selbst, ob ein X40 oder ein SEN0366 angeschlossen ist, und testet ihn
 * nur über die SEN0366-kompatible Schnittstelle (wie PanTiltHead_WM sie nutzt).
 * Kein WLAN. Ausgabe über USB-Serial 115200, 'r' + Enter = Test wiederholen.
 *
 * Ablauf:
 *   1. Sensor erkennen (Typ, Pins)
 *   2. Laser an/aus
 *   3. 5x singleMeasurement() an Pan 0°
 *   4. Scan Pan -30..+30° hin und zurück (gleiche Winkel = gleiche Werte)
 *   5. Nur-SEN0366-Befehle (beim X40 müssen sie false liefern)
 *   6. Laser aus, Zusammenfassung
 *
 * UART frei wählbar: LASER_UART unten (1 oder 2). Hier ist UART1 vom Servo-Bus
 * belegt, deshalb UART2.
 */

#include <Arduino.h>
#include <FeetechBus.h>
#include <FeetechST3020.h>
#include <FeetechSC15.h>
#include <LaserRangefinderAuto.h>

// ── Turm-Konfiguration (wie MapDiscovery/TurnBaseConfig.h) ───────────────────
#define SERVO_BUS_SPEED   1000000
#define PAN_ID            2
#define PAN_ORIGIN        2048
#define TILT_ID           4
#define TILT_ORIGIN       680
#define LASER_UART        2         // welche Hardware-UART der Entfernungsmesser bekommt

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define SERVO_RX_PIN  2
  #define SERVO_TX_PIN  1
  #define LASER_RX_PIN  7
  #define LASER_TX_PIN  8
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define SERVO_RX_PIN  17
  #define SERVO_TX_PIN  16
  #define LASER_RX_PIN  26
  #define LASER_TX_PIN  25
#else
  #error "Nur ESP32 classic und ESP32-S3"
#endif

static const int  SCAN_FROM_DEG = -30;
static const int  SCAN_TO_DEG   = 30;
static const int  SCAN_STEP_DEG = 10;
static const int  SETTLE_MS     = 800;
static const long TOL_MM        = 100;

FeetechBus    bus(Serial1);
FeetechST3020 pan(bus, PAN_ID);
FeetechSC15   tilt(bus, TILT_ID);

HardwareSerial       laserSerial(LASER_UART);
LaserRangefinderAuto laser(&laserSerial);

static int failures = 0;

static void panTo(float deg) {
    int32_t t = PAN_ORIGIN + (int32_t)(deg * (4096.0f / 360.0f));
    pan.moveTime((uint16_t)constrain(t, 0, 4095), 400, 600);
    delay(SETTLE_MS);
}

// Eine Messung wie in PanTiltHead_WM; gibt mm zurück, -1 bei Fehler
static long measure(const char* label) {
    float d = 0;
    uint32_t t0 = millis();
    bool ok = laser.singleMeasurement(d);
    uint32_t dt = millis() - t0;
    if (ok) {
        Serial.printf("  %-8s %7.3f m  %5lu ms\n", label, d, (unsigned long)dt);
        return lroundf(d * 1000.0f);
    }
    Serial.printf("  %-8s FEHLER    %5lu ms", label, (unsigned long)dt);
    if (laser.sensorType() == RANGEFINDER_X40)
        Serial.printf("  raw '%s'", laser.x40().lastRawResponse().c_str());
    Serial.println();
    return -1;
}

static void check(const char* what, bool ok) {
    Serial.printf("  %-34s %s\n", what, ok ? "OK" : "FEHLER");
    if (!ok) failures++;
}

static void runTest() {
    failures = 0;
    Serial.println("\n============ Rangefinder Turm-Test ============");

    Serial.println("\n[1] Sensor erkennen");
    uint32_t t0 = millis();
    bool found = laser.begin(LASER_RX_PIN, LASER_TX_PIN, true);
    Serial.printf("  Gefunden: %s  (RX=%d TX=%d, UART%d, %lu ms)\n", laser.sensorName(),
                  laser.rxPin(), laser.txPin(), LASER_UART, (unsigned long)(millis() - t0));
    if (!found) {
        Serial.println("  KEIN Entfernungsmesser gefunden - Verdrahtung/Versorgung prüfen.");
        return;
    }
    if (laser.sensorType() == RANGEFINDER_X40) {
        float t, v;
        if (laser.x40().readStatus(&t, &v)) Serial.printf("  X40-Status: %.1f C, %.2f V\n", t, v);
    } else {
        char sn[32];
        if (laser.readMachineNumber(sn)) Serial.printf("  SEN0366-Seriennummer: %s\n", sn);
    }

    Serial.println("\n[2] Laser");
    check("controlLaser(ON)",  laser.controlLaser(SEN0366_LASER_ON));
    delay(1000);
    check("controlLaser(OFF)", laser.controlLaser(SEN0366_LASER_OFF));

    Serial.println("\n[3] singleMeasurement() 5x an Pan 0°");
    panTo(0);
    long first = -1;
    for (int i = 0; i < 5; i++) {
        char lbl[8]; snprintf(lbl, sizeof(lbl), "#%d", i + 1);
        long mm = measure(lbl);
        if (mm < 0) failures++;
        else if (first < 0) first = mm;
        else if (labs(mm - first) > TOL_MM) { failures++; Serial.println("    ^ weicht ab"); }
    }

    Serial.println("\n[4] Scan hin und zurück");
    const int n = (SCAN_TO_DEG - SCAN_FROM_DEG) / SCAN_STEP_DEG + 1;
    long fwd[16], back[16];
    panTo(SCAN_FROM_DEG - 5);
    for (int i = 0; i < n; i++) {
        int deg = SCAN_FROM_DEG + i * SCAN_STEP_DEG;
        panTo(deg);
        char lbl[12]; snprintf(lbl, sizeof(lbl), "hin %+d", deg);
        fwd[i] = measure(lbl);
    }
    for (int i = n - 1; i >= 0; i--) {
        int deg = SCAN_FROM_DEG + i * SCAN_STEP_DEG;
        panTo(deg);
        char lbl[12]; snprintf(lbl, sizeof(lbl), "zur %+d", deg);
        back[i] = measure(lbl);
    }
    Serial.println("  Winkel     hin mm   zurück mm   diff");
    for (int i = 0; i < n; i++) {
        int deg = SCAN_FROM_DEG + i * SCAN_STEP_DEG;
        bool bad = fwd[i] < 0 || back[i] < 0 || labs(fwd[i] - back[i]) > TOL_MM;
        if (bad) failures++;
        Serial.printf("  %+4d      %7ld   %7ld   %6ld%s\n", deg, fwd[i], back[i],
                      (fwd[i] >= 0 && back[i] >= 0) ? fwd[i] - back[i] : 0L,
                      bad ? "  <-- FEHLER" : "");
    }
    panTo(0);

    Serial.println("\n[5] Nur-SEN0366-Befehle");
    // Beim SEN0366 nicht ausführen: würde die gespeicherte Auflösung ändern
    if (laser.sensorType() == RANGEFINDER_X40)
        check("setResolution -> false beim X40", !laser.setResolution(SEN0366_RESOLUTION_1MM));
    else
        Serial.println("  übersprungen (würde SEN0366-Einstellung ändern)");

    Serial.println("\n[6] Laser aus");
    check("controlLaser(OFF)", laser.controlLaser(SEN0366_LASER_OFF));

    Serial.printf("\n=========== ERGEBNIS %s: %s (%d Fehler) ===========\n",
                  laser.sensorName(), failures ? "NICHT OK" : "OK", failures);
    Serial.println("'r' + Enter = Test wiederholen");
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nRangefinder_TurretTest startet...");

    bus.beginPins(SERVO_BUS_SPEED, SERVO_RX_PIN, SERVO_TX_PIN);
    delay(100);
    Serial.printf("Pan ping: %s, Tilt ping: %s\n",
                  pan.ping() ? "OK" : "FAIL", tilt.ping() ? "OK" : "FAIL");
    pan.setMode(ServoMode::POSITION);
    tilt.setMode(ServoMode::POSITION);
    FeetechST3020::Profile pPan;
    pPan.statusReturnLevel = 1;
    pPan.returnDelayUnits  = 0;
    pPan.torqueOnAfterInit = true;
    pan.init(pPan);
    FeetechSC15::Profile pTilt;
    pTilt.statusReturnLevel = 1;
    pTilt.returnDelayUnits  = 3;
    pTilt.torqueOnAfterInit = true;
    tilt.init(pTilt);
    pan.moveTime(PAN_ORIGIN, 1500, 400);
    tilt.moveTime(TILT_ORIGIN, 1500, 400);
    delay(2000);

    runTest();
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') runTest();
    }
    delay(10);
}
