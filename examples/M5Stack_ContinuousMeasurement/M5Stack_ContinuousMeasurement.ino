/**
 * @file M5Stack_ContinuousMeasurement.ino
 * @brief Continuous distance measurement display on M5Stack Core2 — X40 laser module
 *
 * Hardware Setup:
 *   Module VCC  -> 3.3 V  (NOT 5 V — use Port A 3V3 pin or external LDO!)
 *   Module GND  -> GND
 *   Module TX   -> G32  (Port A, RX)
 *   Module RX   -> G33  (Port A, TX)
 *
 * Button mapping:
 *   BtnA (left)   — Laser ON / OFF toggle
 *   BtnB (middle) — Single measurement
 *   BtnC (right)  — Start / Stop continuous measurement
 */

#include <M5Unified.h>
#include <X40LaserDistanceMeter.h>

#define RXD2 32
#define TXD2 33

X40LaserDistanceMeter laser(&Serial2);

// Display constants
static const int    SCR_W = 320;
static const int    SCR_H = 240;
static const uint16_t C_BG       = TFT_BLACK;
static const uint16_t C_TEXT     = TFT_WHITE;
static const uint16_t C_DIST     = TFT_CYAN;
static const uint16_t C_UNIT     = TFT_YELLOW;
static const uint16_t C_OK       = TFT_GREEN;
static const uint16_t C_ERR      = TFT_RED;

// State
float    currentDistance    = 0.0f;
int      currentSQ          = -1;
bool     measurementActive  = false;
bool     laserEnabled       = false;
uint32_t lastUpdateTime     = 0;
uint32_t lastMeasureTime    = 0;
int      measureCount       = 0;
int      errorCount         = 0;

LGFX_Sprite distSprite(&M5.Display);

// ---- Forward declarations ----
void drawMainScreen();
void updateDistanceDisplay();
void displayStatus(const char* text, uint16_t color);
void updateLaserSymbol();
void displayStartupScreen();

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Speaker.begin();
    M5.Speaker.setVolume(128);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(C_BG);
    M5.Display.setTextDatum(middle_center);

    displayStartupScreen();
    delay(2000);

    Serial.begin(115200);
    Serial.println("X40 Laser Distance Meter — M5Stack Core2");
    Serial.println("Port A: RX=G32, TX=G33, 19200 baud");

    laser.begin(19200, RXD2, TXD2);
    delay(500);

    // Read and log module status (optional)
    float tempC, supplyV;
    if (laser.readStatus(&tempC, &supplyV)) {
        Serial.print("Module status: ");
        Serial.print(tempC, 1);
        Serial.print(" C  ");
        Serial.print(supplyV, 2);
        Serial.println(" V");
    }

    distSprite.createSprite(SCR_W, 140);
    distSprite.setTextDatum(middle_center);

    drawMainScreen();
    displayStatus("IDLE", TFT_DARKGREY);

    Serial.println("Ready — use buttons to measure");
}

void loop() {
    M5.update();

    // BtnA — Laser toggle
    if (M5.BtnA.wasPressed()) {
        laserEnabled = !laserEnabled;
        laser.controlLaser(laserEnabled ? X40_LASER_ON : X40_LASER_OFF);
        Serial.println(laserEnabled ? "Laser ON" : "Laser OFF");
        updateLaserSymbol();
        delay(200);
    }

    // BtnB — Single measurement (only when continuous mode is off)
    if (M5.BtnB.wasPressed() && !measurementActive) {
        displayStatus("MEASURING", TFT_YELLOW);
        laserEnabled = true;
        updateLaserSymbol();

        X40Measurement m = laser.measure();
        if (m.ok) {
            currentDistance = m.meters;
            currentSQ       = m.signalQuality;
            lastMeasureTime = millis();
            updateDistanceDisplay();
            M5.Speaker.tone(2000, 100);
            Serial.print("Single: ");
            Serial.print(m.meters, 3);
            Serial.print(" m  SQ=");
            Serial.println(m.signalQuality);
        } else {
            Serial.print("Single measurement failed  raw: ");
            Serial.println(laser.lastRawResponse());
        }

        laserEnabled = false;
        updateLaserSymbol();
        displayStatus("IDLE", TFT_DARKGREY);
        delay(200);
    }

    // BtnC — Start / Stop continuous
    if (M5.BtnC.wasPressed()) {
        if (measurementActive) {
            laser.stopContinuousMeasurement();
            measurementActive = false;
            laserEnabled      = false;
            updateLaserSymbol();
            Serial.println("Continuous stopped");
            displayStatus("IDLE", TFT_DARKGREY);
        } else {
            laser.startContinuousMeasurement();
            measurementActive = true;
            measureCount      = 0;
            laserEnabled      = true;
            updateLaserSymbol();
            Serial.println("Continuous started");
            displayStatus("RUNNING", C_OK);
        }
        delay(200);
    }

    // Continuous read — measure() sends one 'D' and returns distance + SQ
    if (measurementActive) {
        X40Measurement m = laser.measure();
        if (m.ok) {
            currentDistance = m.meters;
            currentSQ       = m.signalQuality;
            measureCount++;
            lastMeasureTime = millis();
            errorCount      = 0;
            if (measureCount % 10 == 0) {
                Serial.print("[");
                Serial.print(measureCount);
                Serial.print("] ");
                Serial.print(m.meters, 3);
                Serial.print(" m  SQ=");
                Serial.println(m.signalQuality);
            }
        } else {
            if (measureCount > 0 && millis() - lastMeasureTime > 2000) {
                errorCount++;
                if (errorCount % 20 == 0) {
                    Serial.print("Warning: no data  errorCount=");
                    Serial.println(errorCount);
                }
            }
        }
    }

    // Periodic display refresh
    if (millis() - lastUpdateTime >= 100) {
        lastUpdateTime = millis();
        updateDistanceDisplay();
        if (measurementActive) displayStatus("RUNNING", C_OK);
    }

    delay(10);
}

// ---- Display helpers ----

void displayStartupScreen() {
    M5.Display.fillScreen(C_BG);
    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.drawString("X40 Laser", SCR_W / 2, 60);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString("Distance Meter", SCR_W / 2, 100);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawString("Initializing ...", SCR_W / 2, 140);
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("Port A: G32/G33  19200 baud  3.3 V", SCR_W / 2, 185);
}

void drawLaserSymbol(int x, int y) {
    M5.Display.fillCircle(x, y, 5, TFT_RED);
    M5.Display.drawLine(x - 10, y, x - 6, y, TFT_RED);
    M5.Display.drawLine(x + 6,  y, x + 10, y, TFT_RED);
    M5.Display.drawLine(x - 7, y - 7, x - 4, y - 4, TFT_RED);
    M5.Display.drawLine(x + 4, y + 4, x + 7, y + 7, TFT_RED);
}

void updateLaserSymbol() {
    M5.Display.fillRect(SCR_W / 2 + 80, 15, 25, 20, C_BG);
    if (laserEnabled) drawLaserSymbol(SCR_W / 2 + 90, 25);
}

void drawMainScreen() {
    M5.Display.fillScreen(C_BG);

    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString("X40 DISTANCE", SCR_W / 2, 25);

    updateLaserSymbol();

    M5.Display.drawLine(20, 45, SCR_W - 20, 45, TFT_DARKGREY);

    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("Laser",     40,  225);
    M5.Display.drawString("Single",    160, 225);
    M5.Display.drawString("Start/Stop",280, 225);
    M5.Display.drawString("Status:",   20,  200);
}

void updateDistanceDisplay() {
    distSprite.fillSprite(C_BG);

    // Main distance value
    distSprite.setTextColor(C_DIST);
    distSprite.setFont(&fonts::FreeSansBold24pt7b);

    char buf[20];
    if (currentDistance < 1.0f)       snprintf(buf, sizeof(buf), "%.3f", currentDistance);
    else if (currentDistance < 10.0f) snprintf(buf, sizeof(buf), "%.2f", currentDistance);
    else                              snprintf(buf, sizeof(buf), "%.1f", currentDistance);

    distSprite.drawString(buf, SCR_W / 2, 45);

    // Unit
    distSprite.setTextColor(C_UNIT);
    distSprite.setFont(&fonts::FreeSansBold12pt7b);
    distSprite.drawString("m", SCR_W / 2, 88);

    // cm / mm line
    distSprite.setTextColor(TFT_DARKGREY);
    distSprite.setFont(&fonts::FreeSans9pt7b);
    char sub[40];
    snprintf(sub, sizeof(sub), "(%.1f cm / %ld mm)",
             currentDistance * 100.0f, (long)(currentDistance * 1000.0f + 0.5f));
    distSprite.drawString(sub, SCR_W / 2, 115);

    // Signal quality
    if (currentSQ >= 0) {
        char sq[20];
        snprintf(sq, sizeof(sq), "SQ = %d", currentSQ);
        distSprite.setTextColor(TFT_DARKGREY);
        distSprite.drawString(sq, SCR_W / 2, 133);
    }

    distSprite.pushSprite(0, 50);
}

void displayStatus(const char* text, uint16_t color) {
    M5.Display.fillRect(70, 190, 200, 20, C_BG);
    M5.Display.setTextColor(color);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString(text, 160, 200);
}
