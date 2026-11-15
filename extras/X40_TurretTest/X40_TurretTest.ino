/*
 * X40_TurretTest — autonomer Test der X40-Library auf dem Pan/Tilt-Turm
 *
 * Kein WLAN, kein Controller: läuft nach dem Start einmal durch und gibt alles
 * über USB-Serial (115200) aus. Mit 'r' im Seriellen Monitor erneut starten.
 *
 * Ablauf:
 *   1. X40 suchen (Befehl S, beide Pin-Richtungen)
 *   2. Protokoll-Rohlog: S, O, D, C mit jedem Byte und Zeitstempel
 *      (zeigt Echo, Zeilenenden, Pause bis zur Messantwort)
 *   3. 5x measure() an derselben Stelle (Konstanz, Dauer je Messung)
 *   4. Scan Pan -30..+30° in 10°-Schritten, hin und zurück.
 *      Gleiche Winkel müssen gleiche Werte liefern. Der alte Fehler (Antwort
 *      der vorherigen Messung) würde hier als Versatz um einen Schritt auffallen.
 *   5. Optionale Befehle M, F, V
 *   6. Laser aus, Zusammenfassung
 *
 * Hardware: PanTiltHead-Turm (ESP32 classic oder XIAO ESP32-S3),
 *   Pan ST3020 ID 2, Tilt SC15 ID 4, X40 an 3,3 V, 19200 8N1.
 * Libraries: SerialServo (smily77), X40LaserDistanceMeter (diese Library).
 * Liegt in extras/, damit die Arduino IDE es nicht als Library-Beispiel baut.
 */

#include <Arduino.h>
#include <FeetechBus.h>
#include <FeetechST3020.h>
#include <FeetechSC15.h>
#include <X40LaserDistanceMeter.h>

// ── Turm-Konfiguration (wie MapDiscovery/TurnBaseConfig.h) ───────────────────
#define SERVO_BUS_SPEED   1000000
#define PAN_ID            2
#define PAN_ORIGIN        2048      // 4096 ticks / 360°
#define TILT_ID           4
#define TILT_ORIGIN       680       // SC15: 1024 ticks / 180°

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define SERVO_RX_PIN  2
  #define SERVO_TX_PIN  1
  #define X40_RX_PIN    7
  #define X40_TX_PIN    8
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define SERVO_RX_PIN  17
  #define SERVO_TX_PIN  16
  #define X40_RX_PIN    26
  #define X40_TX_PIN    25
#else
  #error "Nur ESP32 classic und ESP32-S3"
#endif

// ── Testparameter ────────────────────────────────────────────────────────────
static const int   SCAN_FROM_DEG  = -30;
static const int   SCAN_TO_DEG    = 30;
static const int   SCAN_STEP_DEG  = 10;
static const int   SETTLE_MS      = 800;    // nach jeder Pan-Bewegung
static const int   REPEAT_COUNT   = 5;
static const long  SCAN_TOL_MM    = 100;    // erlaubte Abweichung hin/zurück

FeetechBus    bus(Serial1);
FeetechST3020 pan(bus, PAN_ID);
FeetechSC15   tilt(bus, TILT_ID);

HardwareSerial x40Serial(2);
X40LaserDistanceMeter x40(&x40Serial);
int x40Rx = X40_RX_PIN, x40Tx = X40_TX_PIN;

static int failures = 0;

// ── Hilfen ───────────────────────────────────────────────────────────────────

static const char* statusName(X40LaserDistanceMeter::Status s) {
    switch (s) {
        case X40LaserDistanceMeter::OK:                 return "OK";
        case X40LaserDistanceMeter::Timeout:            return "Timeout";
        case X40LaserDistanceMeter::ParseError:         return "ParseError";
        case X40LaserDistanceMeter::DeviceError:        return "DeviceError";
        case X40LaserDistanceMeter::UnexpectedResponse: return "Unexpected";
        case X40LaserDistanceMeter::Unsupported:        return "Unsupported";
        case X40LaserDistanceMeter::OutOfRange:         return "OutOfRange";
    }
    return "?";
}

static void panTo(float deg) {
    int32_t t = PAN_ORIGIN + (int32_t)(deg * (4096.0f / 360.0f));
    pan.moveTime((uint16_t)constrain(t, 0, 4095), 400, 600);
    delay(SETTLE_MS);
}

// Eine Messung ausgeben; gibt mm zurück (-1 bei Fehler)
static long measureAndPrint(const char* label) {
    uint32_t t0 = millis();
    X40Measurement m = x40.measure();
    uint32_t dt = millis() - t0;
    Serial.printf("  %-8s ", label);
    if (m.ok) {
        Serial.printf("%7.3f m  %6ld mm  SQ %4d  %4lu ms  raw '%s'\n",
                      m.meters, (long)m.millimeters, m.signalQuality,
                      (unsigned long)dt, m.raw.c_str());
        return m.millimeters;
    }
    Serial.printf("FEHLER %-11s err %d  %4lu ms  raw '%s'\n",
                  statusName(x40.lastStatus()), m.errorCode,
                  (unsigned long)dt, m.raw.c_str());
    return -1;
}

// Befehl senden und jedes empfangene Byte mit Zeitstempel ausgeben
static void rawLog(char cmd, uint32_t listenMs) {
    while (x40Serial.available()) x40Serial.read();
    Serial.printf("  '%c' ->", cmd);
    uint32_t t0 = millis();
    x40Serial.write((uint8_t)cmd);
    int n = 0;
    while (millis() - t0 < listenMs) {
        while (x40Serial.available()) {
            uint8_t c = x40Serial.read();
            uint32_t t = millis() - t0;
            if (n % 8 == 0) Serial.printf("\n     ");
            if (c == '\r')      Serial.printf(" %4lums:\\r", (unsigned long)t);
            else if (c == '\n') Serial.printf(" %4lums:\\n", (unsigned long)t);
            else if (c >= 32 && c < 127) Serial.printf(" %4lums:'%c'", (unsigned long)t, c);
            else                Serial.printf(" %4lums:0x%02X", (unsigned long)t, c);
            n++;
        }
        delay(1);
    }
    Serial.printf("%s\n", n ? "" : " (keine Antwort)");
}

static bool tryPins(int rx, int tx) {
    x40Serial.end();
    x40.begin(19200, rx, tx);
    delay(150);
    float t = NAN, v = NAN;
    bool ok = x40.readStatus(&t, &v);
    Serial.printf("  RX=%d TX=%d: S -> '%s'", rx, tx, x40.lastRawResponse().c_str());
    if (ok) Serial.printf("  = %.1f C, %.2f V", t, v);
    Serial.println();
    if (ok || x40.lastRawResponse().length()) { x40Rx = rx; x40Tx = tx; return true; }
    return false;
}

// ── Testlauf ─────────────────────────────────────────────────────────────────

static void runTest() {
    failures = 0;
    Serial.println("\n================ X40 Turm-Test ================");

    Serial.println("\n[1] X40 suchen");
    if (!tryPins(X40_RX_PIN, X40_TX_PIN) && !tryPins(X40_TX_PIN, X40_RX_PIN)) {
        Serial.println("  KEIN X40 gefunden – Verdrahtung/Versorgung prüfen. Abbruch.");
        return;
    }

    Serial.println("\n[2] Protokoll-Rohlog (Zeit ab Senden)");
    rawLog('S', 1500);
    rawLog('O', 1500);
    rawLog('D', 2500);
    rawLog('C', 1500);

    Serial.println("\n[3] measure() 5x an Pan 0°");
    panTo(0);
    x40.controlLaser(X40_LASER_ON);
    long first = -1;
    for (int i = 0; i < REPEAT_COUNT; i++) {
        char lbl[8]; snprintf(lbl, sizeof(lbl), "#%d", i + 1);
        long mm = measureAndPrint(lbl);
        if (mm < 0) failures++;
        else if (first < 0) first = mm;
        else if (labs(mm - first) > SCAN_TOL_MM) { failures++; Serial.println("    ^ weicht ab"); }
    }

    Serial.println("\n[4] Scan hin und zurück");
    const int nSteps = (SCAN_TO_DEG - SCAN_FROM_DEG) / SCAN_STEP_DEG + 1;
    long fwd[16], back[16];
    panTo(SCAN_FROM_DEG - 5);   // immer von derselben Seite anfahren
    for (int i = 0; i < nSteps; i++) {
        int deg = SCAN_FROM_DEG + i * SCAN_STEP_DEG;
        panTo(deg);
        char lbl[12]; snprintf(lbl, sizeof(lbl), "hin %+d", deg);
        fwd[i] = measureAndPrint(lbl);
    }
    for (int i = nSteps - 1; i >= 0; i--) {
        int deg = SCAN_FROM_DEG + i * SCAN_STEP_DEG;
        panTo(deg);
        char lbl[12]; snprintf(lbl, sizeof(lbl), "zur %+d", deg);
        back[i] = measureAndPrint(lbl);
    }
    Serial.println("  Vergleich   hin mm   zurück mm   diff");
    for (int i = 0; i < nSteps; i++) {
        int deg = SCAN_FROM_DEG + i * SCAN_STEP_DEG;
        bool bad = fwd[i] < 0 || back[i] < 0 || labs(fwd[i] - back[i]) > SCAN_TOL_MM;
        if (bad) failures++;
        Serial.printf("  %+4d°     %7ld   %7ld   %6ld %s\n", deg, fwd[i], back[i],
                      (fwd[i] >= 0 && back[i] >= 0) ? fwd[i] - back[i] : 0L,
                      bad ? " <-- FEHLER" : "");
    }
    panTo(0);

    Serial.println("\n[5] Optionale Befehle");
    X40Measurement m = x40.measurePrecise();
    Serial.printf("  M: %s  raw '%s'\n", m.ok ? String(m.meters, 3).c_str() : statusName(x40.lastStatus()), m.raw.c_str());
    m = x40.measureFast();
    Serial.printf("  F: %s  raw '%s'\n", m.ok ? String(m.meters, 3).c_str() : statusName(x40.lastStatus()), m.raw.c_str());
    String ver;
    bool vOk = x40.readVersion(&ver);
    Serial.printf("  V: %s  raw '%s'\n", vOk ? "Antwort" : statusName(x40.lastStatus()), x40.lastRawResponse().c_str());

    Serial.println("\n[6] Laser aus");
    bool off = x40.controlLaser(X40_LASER_OFF);
    Serial.printf("  C -> %s  raw '%s'\n", off ? "OK" : "FEHLER", x40.lastRawResponse().c_str());

    Serial.printf("\n=========== ERGEBNIS: %s (%d Fehler) ===========\n",
                  failures ? "NICHT OK" : "OK", failures);
    Serial.println("'r' + Enter = Test wiederholen");
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nX40_TurretTest startet...");

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
