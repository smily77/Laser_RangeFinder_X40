# X40LaserDistanceMeter — Arduino Library

Arduino/ESP32 library for the **X-40 / Hi-AT 40 m UART laser distance meter**.  
The public API mirrors the [LaserRangefinder_SEN0366](https://github.com/smily77/Laser_Rangefinder_SEN0366_GJD-01) library so existing sketches need only minor changes.

> **Status: draft / unverified.**  
> The protocol is derived from community reports and similar 40 m/50 m modules.  
> Verify every command on your specific hardware before deploying. See [Open points](#open-points).

---

## ⚠️ Voltage warning

| Signal | Direction | Note |
|--------|-----------|------|
| VCC | → Module | **2.8 V preferred, 2.0–3.3 V max.  Never connect 5 V directly!** |
| GND | — | Common ground mandatory |
| TX (module) | → MCU RX | TTL/CMOS, 3.3 V level — safe for ESP32 |
| RX (module) | ← MCU TX | Reduce to 3.3 V if MCU runs at 5 V (voltage divider or level shifter) |
| EN (optional) | → Module | Power-enable GPIO, not present on all units |

A 1 kΩ–4.7 kΩ series resistor on MCU TX → Module RX is a safe and simple 5 V fix.

---

## UART settings

| Parameter | Value |
|-----------|-------|
| Baud rate | 19 200 |
| Data bits | 8 |
| Parity | none |
| Stop bits | 1 |
| Flow control | none |

---

## Installation

Copy or clone the repository into your Arduino `libraries/` folder, then restart the IDE.

---

## Quick start (ESP32)

```cpp
#include <X40LaserDistanceMeter.h>

HardwareSerial ModuleSerial(2);
X40LaserDistanceMeter laser(&ModuleSerial);

void setup() {
    Serial.begin(115200);
    laser.begin(19200, /*RX*/ 16, /*TX*/ 17);
    delay(300);
}

void loop() {
    float dist;
    if (laser.singleMeasurement(dist)) {
        Serial.print(dist * 1000.0f, 0);
        Serial.println(" mm");
    }
    delay(1000);
}
```

---

## API reference

### Constructor

```cpp
X40LaserDistanceMeter laser(&HardwareSerialInstance);
```

### Initialisation

```cpp
laser.begin(19200);                    // uses current RX/TX pins
laser.begin(19200, rxPin, txPin);      // ESP32: set pins explicitly
laser.setTimeout(4000);                // ms to wait for any response
laser.setIdleGap(40);                  // ms of bus silence = end of frame
```

### Measurement — SEN0366-compatible names

```cpp
bool singleMeasurement(float& distance);        // distance in metres
bool startContinuousMeasurement();              // turns laser on ('O')
bool readContinuousDistance(float& distance);   // single 'D' each call
bool stopContinuousMeasurement();               // turns laser off ('C')
void takeSingleMeasurementToCache();
bool readCache(float& distance);
```

### Measurement — extended X40 methods

```cpp
X40Measurement measure();           // 'D' — standard
X40Measurement measurePrecise();    // 'M' — optional, may return Unsupported
X40Measurement measureFast();       // 'F' — optional, may return Unsupported

int32_t readRangeMillimeters();     // -1 on error
float   readRangeMeters();          // NAN on error
```

`X40Measurement` fields:

| Field | Type | Description |
|-------|------|-------------|
| `ok` | `bool` | true if parse succeeded |
| `meters` | `float` | distance in metres |
| `millimeters` | `int32_t` | `round(meters * 1000)` |
| `signalQuality` | `int` | SQ value after comma; -1 if absent |
| `errorCode` | `int` | numeric Er.xx code; -1 if N/A |
| `raw` | `String` | raw response for debugging |

### Control

```cpp
bool controlLaser(X40_LASER_ON);    // sends 'O'
bool controlLaser(X40_LASER_OFF);   // sends 'C'
bool shutDown();                    // sends 'X' (optional command)
```

### Information

```cpp
bool readStatus(float* temperatureC, float* supplyV);   // sends 'S'
bool readVersion(String* versionText);                  // sends 'V'
bool readMachineNumber(char* buffer);                   // sends 'V', copies to char[32]
```

### SEN0366 config stubs (not supported, return false)

```cpp
bool setAddress(uint8_t);
bool setResolution(uint8_t);
bool setFrequency(uint8_t);
bool setDataReturnInterval(uint8_t);
bool setMeasurementStartingPoint(uint8_t);
bool setAutoStart(uint8_t);
uint8_t readParameter(uint8_t* data, uint8_t maxLen);
```

### Diagnostics

```cpp
Status      lastStatus();        // OK, Timeout, ParseError, DeviceError, UnexpectedResponse, Unsupported
int         lastErrorCode();     // numeric Er.xx code or -1
const String& lastRawResponse(); // always set after every command
```

---

## Protocol commands

| Byte | Command | Expected response |
|------|---------|------------------|
| `O` (0x4F) | Laser on | `OK` or `,OK!` |
| `C` (0x43) | Laser off | `OK` or `,OK!` |
| `S` (0x53) | Status | `18.0C, 2.7V` |
| `D` (0x44) | Measure | `12.345m,0079` |
| `M` (0x4D) | Precise measure *(optional)* | distance or timeout |
| `F` (0x46) | Fast measure *(optional)* | distance or timeout |
| `V` (0x56) | Version *(optional)* | free text or timeout |
| `X` (0x58) | Power down *(optional)* | none or `OK` |

SQ (signal quality) after the comma: **lower is better**.  
Error responses: `Er.01!`, `Er.XX!` — device-side error; see your module documentation.

---

## Troubleshooting

1. **No response at all** — swap RX/TX, check VCC level, verify 19 200 baud.  
2. **Garbled response** — check baud rate and level shifting; some units boot at a different rate.  
3. **`Er.` responses** — target out of range, too much ambient light, low supply voltage, or temperature limit exceeded.  
4. **`M`/`F`/`V`/`X` always timeout** — those commands are optional and may not be implemented in your firmware variant. Use `lastStatus() == Unsupported` to detect this.

---

## Open points

| Item | How to resolve |
|------|---------------|
| Exact line ending (CR / LF / CRLF / idle-gap only) | Capture a hex dump of the raw byte stream |
| Exact VCC requirement | Measure at the module pin or read PCB markings |
| M/F/V/X support | Test each command and log the timeout |
| Error code table | Fill in from official manual once available |
