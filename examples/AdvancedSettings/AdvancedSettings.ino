/**
 * @file AdvancedSettings.ino
 * @brief Interactive serial menu demonstrating all X40 commands
 *
 * Covers standard commands (O/C/S/D) and optional commands (M/F/V/X).
 * Optional commands return status=Unsupported if the firmware variant
 * of your specific module does not implement them — this is handled
 * gracefully.
 *
 * Hardware Setup (ESP32):
 *   Module VCC  -> 3.3 V  (NOT 5 V!)
 *   Module GND  -> GND
 *   Module TX   -> GPIO 32 (RX2)
 *   Module RX   -> GPIO 33 (TX2)  use voltage divider if MCU is 5 V
 *
 * Open the Serial Monitor at 115200 baud after uploading.
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

bool menuActive = true;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("\n\n");
    Serial.println("=============================================");
    Serial.println("  X40 Laser Distance Meter — Advanced Demo");
    Serial.println("=============================================");
    Serial.println("WARNING: Module VCC = 2.0-3.3 V — NEVER 5 V!");
    Serial.println();

#if defined(ESP32)
    laser.begin(19200, RXD2, TXD2);
#else
    laser.begin(19200);
#endif
    delay(500);

    displayMenu();
}

void loop() {
    if (menuActive && Serial.available()) {
        char choice = Serial.read();
        while (Serial.available()) Serial.read(); // flush

        switch (choice) {
            case '1': readModuleStatus();            break;
            case '2': readVersionInfo();             break;
            case '3': testLaserControl();            break;
            case '4': demonstrateSingleMeasurement();break;
            case '5': demonstratePreciseMeasure();   break;
            case '6': demonstrateFastMeasure();      break;
            case '7': demonstrateContinuous();       break;
            case '8': demonstrateCache();            break;
            case '9': testShutdown();                break;
            case 'm': case 'M': displayMenu();       break;
        }
    }
    delay(100);
}

// ---- Menu ----

void displayMenu() {
    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║         X40 COMMAND MENU                 ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.println("║  Standard commands:                      ║");
    Serial.println("║  [1] Read status — 'S' (temp / voltage)  ║");
    Serial.println("║  [2] Read version — 'V' (optional)       ║");
    Serial.println("║  [3] Laser ON / OFF — 'O' / 'C'          ║");
    Serial.println("║  [4] Single measure — 'D'                ║");
    Serial.println("║                                          ║");
    Serial.println("║  Optional commands (may be unsupported): ║");
    Serial.println("║  [5] Precise measure — 'M'               ║");
    Serial.println("║  [6] Fast measure — 'F'                  ║");
    Serial.println("║                                          ║");
    Serial.println("║  Higher-level functions:                 ║");
    Serial.println("║  [7] Continuous measurement demo         ║");
    Serial.println("║  [8] Cache measurement demo              ║");
    Serial.println("║                                          ║");
    Serial.println("║  [9] Shutdown — 'X' (optional, careful!) ║");
    Serial.println("║  [M] Show this menu                      ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.print("Enter choice: ");
}

// ---- Status / Info ----

void readModuleStatus() {
    Serial.println("\n--- Status ('S') ---");
    float tempC, supplyV;
    if (laser.readStatus(&tempC, &supplyV)) {
        Serial.print("Temperature : ");
        Serial.print(tempC, 1);
        Serial.println(" °C");
        Serial.print("Supply voltage: ");
        Serial.print(supplyV, 2);
        Serial.println(" V");
    } else {
        printError("Status read failed");
    }
    waitForKey();
}

void readVersionInfo() {
    Serial.println("\n--- Version ('V') ---");
    String ver;
    if (laser.readVersion(&ver)) {
        Serial.print("Version string: \"");
        Serial.print(ver);
        Serial.println("\"");
    } else {
        printError("Version read failed (command may be unsupported)");
    }
    waitForKey();
}

// ---- Laser control ----

void testLaserControl() {
    Serial.println("\n--- Laser control ('O' / 'C') ---");

    Serial.print("Laser ON  → ");
    if (laser.controlLaser(X40_LASER_ON))  Serial.println("OK");
    else                                   printError("failed");
    delay(2000);

    Serial.print("Laser OFF → ");
    if (laser.controlLaser(X40_LASER_OFF)) Serial.println("OK");
    else                                   printError("failed");

    waitForKey();
}

// ---- Measurements ----

void demonstrateSingleMeasurement() {
    Serial.println("\n--- Single measure ('D') — 5 shots ---");
    for (int i = 1; i <= 5; i++) {
        Serial.print("  [");
        Serial.print(i);
        Serial.print("] ");
        printMeasurement(laser.measure());
        delay(500);
    }
    waitForKey();
}

void demonstratePreciseMeasure() {
    Serial.println("\n--- Precise measure ('M') — optional ---");
    Serial.println("  Sending 'M' — timeout if unsupported ...");
    X40Measurement m = laser.measurePrecise();
    if (m.ok) {
        printMeasurement(m);
    } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
        Serial.println("  Not supported by this firmware variant.");
    } else {
        printError("failed");
    }
    waitForKey();
}

void demonstrateFastMeasure() {
    Serial.println("\n--- Fast measure ('F') — optional ---");
    Serial.println("  Sending 'F' — timeout if unsupported ...");
    X40Measurement m = laser.measureFast();
    if (m.ok) {
        printMeasurement(m);
    } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
        Serial.println("  Not supported by this firmware variant.");
    } else {
        printError("failed");
    }
    waitForKey();
}

void demonstrateContinuous() {
    Serial.println("\n--- Continuous measurement (10 s, any key to stop) ---");
    Serial.println("  Starting laser ('O') ...");
    laser.startContinuousMeasurement();

    uint32_t t0  = millis();
    int      cnt = 0;

    while (millis() - t0 < 10000 && !Serial.available()) {
        X40Measurement m = laser.measure();
        if (m.ok) {
            cnt++;
            Serial.print("  [");
            Serial.print(cnt);
            Serial.print("] ");
            printMeasurement(m);
        }
        delay(50);
    }

    while (Serial.available()) Serial.read();
    laser.stopContinuousMeasurement();
    Serial.print("  Stopped. Total measurements: ");
    Serial.println(cnt);
    waitForKey();
}

void demonstrateCache() {
    Serial.println("\n--- Cache measurement ---");
    Serial.println("  Triggering measurement and caching result ...");
    laser.takeSingleMeasurementToCache();
    delay(300);

    float dist;
    if (laser.readCache(dist)) {
        Serial.print("  Cached distance: ");
        Serial.print(dist, 3);
        Serial.print(" m  (");
        Serial.print((long)(dist * 1000.0f + 0.5f));
        Serial.println(" mm)");
    } else {
        printError("Cache empty — measurement may have failed");
    }
    waitForKey();
}

void testShutdown() {
    Serial.println("\n--- Shutdown ('X') — optional ---");
    Serial.println("  WARNING: module may stop responding until power-cycled.");
    Serial.println("  Press 'Y' to confirm, any other key to cancel:");
    char c = waitForKey();
    if (c == 'Y' || c == 'y') {
        if (laser.shutDown()) {
            Serial.println("  Shutdown command sent (no response expected).");
        } else if (laser.lastStatus() == X40LaserDistanceMeter::Unsupported) {
            Serial.println("  Not supported by this firmware variant.");
        } else {
            printError("Shutdown failed");
        }
    } else {
        Serial.println("  Cancelled.");
    }
    waitForKey();
}

// ---- Helpers ----

void printMeasurement(const X40Measurement& m) {
    if (m.ok) {
        Serial.print(m.meters, 3);
        Serial.print(" m  (");
        Serial.print(m.millimeters);
        Serial.print(" mm)");
        if (m.signalQuality >= 0) {
            Serial.print("  SQ=");
            Serial.print(m.signalQuality);
        }
        Serial.println();
    } else {
        Serial.print("FAILED  status=");
        Serial.print(laser.lastStatus());
        if (laser.lastErrorCode() >= 0) {
            Serial.print("  errCode=");
            Serial.print(laser.lastErrorCode());
        }
        Serial.print("  raw: \"");
        Serial.print(laser.lastRawResponse());
        Serial.println("\"");
    }
}

void printError(const char* msg) {
    Serial.print("ERROR: ");
    Serial.print(msg);
    Serial.print("  raw: \"");
    Serial.print(laser.lastRawResponse());
    Serial.println("\"");
}

char waitForKey() {
    while (!Serial.available()) delay(10);
    char c = Serial.read();
    delay(100);
    while (Serial.available()) Serial.read();
    Serial.println();
    return c;
}
