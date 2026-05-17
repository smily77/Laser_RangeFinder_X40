/**
 * @file M5Stack_FullDemo.ino
 * @brief Full X40 command demo on M5Stack Core2 — tests all functions
 *
 * Works through every X40 command in sequence and shows the result on
 * the display.  Optional commands (M / F / V / X) are tested and the
 * display clearly indicates whether each is supported.
 *
 * Command sequence tested:
 *   S  — Status (temperature, supply voltage)
 *   O  — Laser ON
 *   D  — Standard measure (10 shots)
 *   M  — Precise measure (optional)
 *   F  — Fast measure (optional)
 *   V  — Version / info string (optional)
 *   Continuous — 10-second continuous measurement demo
 *   Cache — takeSingleMeasurementToCache + readCache
 *   X  — Shutdown (optional, step can be skipped)
 *   C  — Laser OFF
 *
 * Hardware Setup (M5Stack Core2, Port A):
 *   Module VCC  -> 3.3 V  (NOT 5 V!)
 *   Module GND  -> GND
 *   Module TX   -> G32  (Port A RX)
 *   Module RX   -> G33  (Port A TX)
 *
 * Button mapping:
 *   BtnA (left)   — previous step
 *   BtnB (middle) — run / execute current step
 *   BtnC (right)  — next step
 */

#include <M5Unified.h>
#include <X40LaserDistanceMeter.h>

#define RXD2 32
#define TXD2 33

X40LaserDistanceMeter laser(&Serial2);

// ---- Screen layout ----
static const int    SCR_W = 320;
static const int    SCR_H = 240;
static const uint16_t C_BG     = TFT_BLACK;
static const uint16_t C_TITLE  = TFT_CYAN;
static const uint16_t C_OK     = TFT_GREEN;
static const uint16_t C_WARN   = TFT_YELLOW;
static const uint16_t C_ERR    = TFT_RED;
static const uint16_t C_INFO   = TFT_WHITE;
static const uint16_t C_DIM    = TFT_DARKGREY;

// ---- Step definitions ----
enum Step {
    STEP_STATUS = 0,
    STEP_LASER_ON,
    STEP_MEASURE_D,
    STEP_MEASURE_M,
    STEP_MEASURE_F,
    STEP_VERSION,
    STEP_CONTINUOUS,
    STEP_CACHE,
    STEP_SHUTDOWN,
    STEP_LASER_OFF,
    STEP_COUNT
};

const char* STEP_NAMES[STEP_COUNT] = {
    "S — Status",
    "O — Laser ON",
    "D — Measure (10x)",
    "M — Precise (opt.)",
    "F — Fast (opt.)",
    "V — Version (opt.)",
    "Continuous (10 s)",
    "Cache measure",
    "X — Shutdown (opt.)",
    "C — Laser OFF"
};

int   currentStep = 0;
bool  stepDone[STEP_COUNT] = {};
bool  stepOK[STEP_COUNT]   = {};

LGFX_Sprite resultSprite(&M5.Display);

// ---- Forward declarations ----
void drawFrame();
void showResult(const String& line1, const String& line2,
                const String& line3, uint16_t color);
void runStep(int step);
void runStatus();
void runLaserOn();
void runMeasureD();
void runMeasureM();
void runMeasureF();
void runVersion();
void runContinuous();
void runCache();
void runShutdown();
void runLaserOff();
String statusName(X40LaserDistanceMeter::Status s);

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Speaker.begin();
    M5.Speaker.setVolume(100);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(C_BG);
    M5.Display.setTextDatum(middle_center);

    // Startup
    M5.Display.setTextColor(C_TITLE);
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.drawString("X40 Full Demo", SCR_W / 2, 70);
    M5.Display.setTextColor(C_INFO);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawString("Connecting to laser module ...", SCR_W / 2, 120);
    M5.Display.setTextColor(C_WARN);
    M5.Display.drawString("3.3 V only — never 5 V!", SCR_W / 2, 150);

    Serial.begin(115200);
    laser.begin(19200, RXD2, TXD2);
    delay(600);

    resultSprite.createSprite(SCR_W, 130);
    resultSprite.setTextDatum(middle_center);

    drawFrame();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        currentStep = (currentStep - 1 + STEP_COUNT) % STEP_COUNT;
        drawFrame();
        delay(200);
    }
    if (M5.BtnC.wasPressed()) {
        currentStep = (currentStep + 1) % STEP_COUNT;
        drawFrame();
        delay(200);
    }
    if (M5.BtnB.wasPressed()) {
        runStep(currentStep);
        drawFrame();
        delay(200);
    }

    delay(20);
}

// ---- Step runner ----

void runStep(int step) {
    stepDone[step] = true;
    switch (step) {
        case STEP_STATUS:     runStatus();     break;
        case STEP_LASER_ON:   runLaserOn();    break;
        case STEP_MEASURE_D:  runMeasureD();   break;
        case STEP_MEASURE_M:  runMeasureM();   break;
        case STEP_MEASURE_F:  runMeasureF();   break;
        case STEP_VERSION:    runVersion();    break;
        case STEP_CONTINUOUS: runContinuous(); break;
        case STEP_CACHE:      runCache();      break;
        case STEP_SHUTDOWN:   runShutdown();   break;
        case STEP_LASER_OFF:  runLaserOff();   break;
    }
}

// ---- Individual step implementations ----

void runStatus() {
    showResult("Running 'S' ...", "", "", C_WARN);
    float tempC = NAN, supplyV = NAN;
    if (laser.readStatus(&tempC, &supplyV)) {
        stepOK[STEP_STATUS] = true;
        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "Temp:    %.1f C", tempC);
        snprintf(l2, sizeof(l2), "Voltage: %.2f V", supplyV);
        showResult(l1, l2, "", C_OK);
        Serial.print("Status: "); Serial.print(tempC); Serial.print(" C  ");
        Serial.print(supplyV); Serial.println(" V");
    } else {
        stepOK[STEP_STATUS] = false;
        showResult("Status FAILED", "raw: " + laser.lastRawResponse(), "", C_ERR);
    }
}

void runLaserOn() {
    showResult("Sending 'O' ...", "", "", C_WARN);
    if (laser.controlLaser(X40_LASER_ON)) {
        stepOK[STEP_LASER_ON] = true;
        showResult("Laser ON", "OK", "", C_OK);
        Serial.println("Laser ON: OK");
    } else {
        stepOK[STEP_LASER_ON] = false;
        showResult("Laser ON FAILED", laser.lastRawResponse(), "", C_ERR);
    }
}

void runMeasureD() {
    showResult("Sending 'D' x10 ...", "", "", C_WARN);
    float sumM = 0;
    int   good = 0;
    int   lastSQ = -1;
    for (int i = 0; i < 10; i++) {
        X40Measurement m = laser.measure();
        if (m.ok) { sumM += m.meters; good++; lastSQ = m.signalQuality; }
        delay(100);
    }
    if (good > 0) {
        stepOK[STEP_MEASURE_D] = true;
        float avg = sumM / good;
        char l1[32], l2[32], l3[32];
        snprintf(l1, sizeof(l1), "Avg: %.3f m", avg);
        snprintf(l2, sizeof(l2), "    (%ld mm)", (long)(avg * 1000.f + .5f));
        snprintf(l3, sizeof(l3), "SQ=%d  OK %d/10", lastSQ, good);
        showResult(l1, l2, l3, C_OK);
        Serial.print("D avg="); Serial.print(avg, 3);
        Serial.print(" m  SQ="); Serial.print(lastSQ);
        Serial.print("  ok="); Serial.print(good); Serial.println("/10");
    } else {
        stepOK[STEP_MEASURE_D] = false;
        showResult("Measure D FAILED", "0/10 OK", "raw: " + laser.lastRawResponse(), C_ERR);
    }
}

void runMeasureM() {
    showResult("Sending 'M' ...", "(optional, please wait)", "", C_WARN);
    X40Measurement m = laser.measurePrecise();
    if (m.ok) {
        stepOK[STEP_MEASURE_M] = true;
        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "%.3f m  (%ld mm)", m.meters, m.millimeters);
        snprintf(l2, sizeof(l2), "SQ = %d", m.signalQuality);
        showResult("M — Precise OK", l1, l2, C_OK);
        Serial.print("M: "); Serial.print(m.meters, 3); Serial.println(" m");
    } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
        stepOK[STEP_MEASURE_M] = false;
        showResult("M — NOT supported", "by this firmware", "", C_WARN);
        Serial.println("M: Unsupported");
    } else {
        stepOK[STEP_MEASURE_M] = false;
        showResult("M — FAILED", statusName(laser.lastStatus()),
                   "raw: " + laser.lastRawResponse(), C_ERR);
    }
}

void runMeasureF() {
    showResult("Sending 'F' ...", "(optional, please wait)", "", C_WARN);
    X40Measurement m = laser.measureFast();
    if (m.ok) {
        stepOK[STEP_MEASURE_F] = true;
        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "%.3f m  (%ld mm)", m.meters, m.millimeters);
        snprintf(l2, sizeof(l2), "SQ = %d", m.signalQuality);
        showResult("F — Fast OK", l1, l2, C_OK);
        Serial.print("F: "); Serial.print(m.meters, 3); Serial.println(" m");
    } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
        stepOK[STEP_MEASURE_F] = false;
        showResult("F — NOT supported", "by this firmware", "", C_WARN);
        Serial.println("F: Unsupported");
    } else {
        stepOK[STEP_MEASURE_F] = false;
        showResult("F — FAILED", statusName(laser.lastStatus()),
                   "raw: " + laser.lastRawResponse(), C_ERR);
    }
}

void runVersion() {
    showResult("Sending 'V' ...", "(optional, please wait)", "", C_WARN);
    String ver;
    if (laser.readVersion(&ver)) {
        stepOK[STEP_VERSION] = true;
        showResult("V — Version OK", ver, "", C_OK);
        Serial.print("V: "); Serial.println(ver);
    } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
        stepOK[STEP_VERSION] = false;
        showResult("V — NOT supported", "by this firmware", "", C_WARN);
        Serial.println("V: Unsupported");
    } else {
        stepOK[STEP_VERSION] = false;
        showResult("V — FAILED", statusName(laser.lastStatus()), "", C_ERR);
    }
}

void runContinuous() {
    showResult("Continuous 10 s", "Laser ON — measuring ...", "", C_WARN);
    laser.startContinuousMeasurement();

    float  sumM = 0;
    int    cnt  = 0;
    int    lastSQ = -1;
    float  minM = 1e9f, maxM = -1e9f;
    uint32_t t0 = millis();

    while (millis() - t0 < 10000) {
        X40Measurement m = laser.measure();
        if (m.ok) {
            sumM += m.meters;
            cnt++;
            lastSQ = m.signalQuality;
            if (m.meters < minM) minM = m.meters;
            if (m.meters > maxM) maxM = m.meters;

            // Live update every 5 readings
            if (cnt % 5 == 0) {
                char buf[40];
                snprintf(buf, sizeof(buf), "  [%d] %.3f m  SQ=%d", cnt, m.meters, m.signalQuality);
                M5.Display.fillRect(0, 100, SCR_W, 25, C_BG);
                M5.Display.setTextColor(C_INFO);
                M5.Display.setFont(&fonts::Font2);
                M5.Display.drawString(buf, SCR_W / 2, 112);
            }
        }
        delay(30);
    }

    laser.stopContinuousMeasurement();
    stepOK[STEP_CONTINUOUS] = cnt > 0;

    if (cnt > 0) {
        char l1[40], l2[40], l3[40];
        snprintf(l1, sizeof(l1), "Avg: %.3f m  n=%d", sumM / cnt, cnt);
        snprintf(l2, sizeof(l2), "Min: %.3f m  Max: %.3f m", minM, maxM);
        snprintf(l3, sizeof(l3), "Last SQ = %d", lastSQ);
        showResult(l1, l2, l3, C_OK);
        Serial.print("Continuous: avg="); Serial.print(sumM/cnt,3);
        Serial.print(" m  n="); Serial.println(cnt);
    } else {
        showResult("Continuous FAILED", "0 measurements", "", C_ERR);
    }
}

void runCache() {
    showResult("Cache measure", "Triggering 'D' ...", "", C_WARN);
    laser.takeSingleMeasurementToCache();
    delay(300);

    float dist;
    if (laser.readCache(dist)) {
        stepOK[STEP_CACHE] = true;
        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "%.3f m", dist);
        snprintf(l2, sizeof(l2), "  (%ld mm)", (long)(dist * 1000.f + .5f));
        showResult("Cache OK", l1, l2, C_OK);
        Serial.print("Cache: "); Serial.print(dist, 3); Serial.println(" m");
    } else {
        stepOK[STEP_CACHE] = false;
        showResult("Cache FAILED", "measurement error", "", C_ERR);
    }
}

void runShutdown() {
    // Show confirmation screen
    M5.Display.fillScreen(C_BG);
    M5.Display.setTextColor(C_WARN);
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString("Shutdown ('X')", SCR_W / 2, 60);
    M5.Display.setTextColor(C_INFO);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawString("Module may stop responding", SCR_W / 2, 100);
    M5.Display.drawString("until power-cycled!", SCR_W / 2, 118);
    M5.Display.setTextColor(C_OK);
    M5.Display.drawString("[B] Confirm send 'X'", SCR_W / 2, 160);
    M5.Display.setTextColor(C_ERR);
    M5.Display.drawString("[A] or [C] Cancel", SCR_W / 2, 182);

    // Wait for button
    uint32_t t0 = millis();
    bool confirmed = false;
    while (millis() - t0 < 10000) {
        M5.update();
        if (M5.BtnB.wasPressed()) { confirmed = true;  break; }
        if (M5.BtnA.wasPressed() || M5.BtnC.wasPressed()) { break; }
        delay(20);
    }

    if (!confirmed) {
        stepOK[STEP_SHUTDOWN] = true; // skipped is not a failure
        showResult("Shutdown skipped", "(cancelled by user)", "", C_DIM);
        Serial.println("X: Skipped by user");
        return;
    }

    showResult("Sending 'X' ...", "(optional, please wait)", "", C_WARN);
    if (laser.shutDown()) {
        stepOK[STEP_SHUTDOWN] = true;
        showResult("X — Shutdown sent", "Module may be offline", "Power-cycle to restart", C_WARN);
        M5.Speaker.tone(440, 300);
        Serial.println("X: Shutdown sent");
    } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
        stepOK[STEP_SHUTDOWN] = false;
        showResult("X — NOT supported", "by this firmware", "", C_WARN);
        Serial.println("X: Unsupported");
    } else {
        stepOK[STEP_SHUTDOWN] = false;
        showResult("X — FAILED", statusName(laser.lastStatus()), "", C_ERR);
    }
}

void runLaserOff() {
    showResult("Sending 'C' ...", "", "", C_WARN);
    if (laser.controlLaser(X40_LASER_OFF)) {
        stepOK[STEP_LASER_OFF] = true;
        showResult("Laser OFF", "OK", "", C_OK);
        Serial.println("Laser OFF: OK");
        M5.Speaker.tone(800, 100);
    } else {
        stepOK[STEP_LASER_OFF] = false;
        showResult("Laser OFF FAILED", laser.lastRawResponse(), "", C_ERR);
    }
}

// ---- Display helpers ----

void drawFrame() {
    M5.Display.fillScreen(C_BG);

    // Title bar
    M5.Display.setTextColor(C_TITLE);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("X40 Full Demo", SCR_W / 2, 14);
    M5.Display.drawLine(0, 26, SCR_W, 26, TFT_DARKGREY);

    // Step list (mini sidebar-style, scrolling window of ±2)
    M5.Display.setFont(&fonts::Font2);
    int yBase = 35;
    for (int i = 0; i < STEP_COUNT; i++) {
        int  y      = yBase + i * 14;
        bool active = (i == currentStep);
        uint16_t col = C_DIM;
        if (active)              col = TFT_WHITE;
        if (stepDone[i] && stepOK[i])  col = C_OK;
        if (stepDone[i] && !stepOK[i]) col = C_ERR;

        M5.Display.setTextColor(active ? TFT_WHITE : col);
        String label = (active ? "> " : "  ");
        label += STEP_NAMES[i];
        if (stepDone[i]) label += (stepOK[i] ? " v" : " x");
        M5.Display.drawString(label, SCR_W / 2, y);
    }

    // Separator
    M5.Display.drawLine(0, 212, SCR_W, 212, TFT_DARKGREY);

    // Button labels
    M5.Display.setTextColor(C_DIM);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("< Prev",   50,  226);
    M5.Display.setTextColor(C_OK);
    M5.Display.drawString("RUN",      160, 226);
    M5.Display.setTextColor(C_DIM);
    M5.Display.drawString("Next >",   270, 226);
}

void showResult(const String& l1, const String& l2,
                const String& l3, uint16_t color) {
    // Overlay result panel over step list area
    M5.Display.fillRect(0, 27, SCR_W, 184, C_BG);

    M5.Display.setTextColor(color);
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString(l1, SCR_W / 2, 80);

    if (l2.length() > 0) {
        M5.Display.setTextColor(C_INFO);
        M5.Display.setFont(&fonts::FreeSans9pt7b);
        M5.Display.drawString(l2, SCR_W / 2, 116);
    }
    if (l3.length() > 0) {
        M5.Display.setTextColor(C_DIM);
        M5.Display.setFont(&fonts::Font2);
        M5.Display.drawString(l3, SCR_W / 2, 140);
    }

    // Instruction to continue
    M5.Display.setTextColor(C_DIM);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("Press A/C to navigate, B to re-run", SCR_W / 2, 195);

    // Brief beep on success
    if (color == C_OK) M5.Speaker.tone(1500, 80);
}

String statusName(X40LaserDistanceMeter::Status s) {
    switch (s) {
        case X40LaserDistanceMeter::OK:                 return "OK";
        case X40LaserDistanceMeter::Timeout:            return "Timeout";
        case X40LaserDistanceMeter::ParseError:         return "ParseError";
        case X40LaserDistanceMeter::DeviceError:        return "DeviceError";
        case X40LaserDistanceMeter::UnexpectedResponse: return "Unexpected";
        case X40LaserDistanceMeter::Unsupported:        return "Unsupported";
        default:                                        return "Unknown";
    }
}
