# X40LaserDistanceMeter — Arduino Library

Arduino/ESP32 library for the **X-40 / Hi-AT 40 m UART laser distance meter**.  
The public API mirrors the [LaserRangefinder_SEN0366](https://github.com/smily77/Laser_Rangefinder_SEN0366_GJD-01) library so existing sketches need only minor changes.

> **Status: tested on hardware** (X-40 at 3.3 V on an ESP32, 2026-11-15).  
> `S`, `O`, `C` and `D` are verified; `M`, `F`, `V` are not supported by the tested unit.  
> See [Observed protocol](#observed-protocol-x-40-tested-2026-11-15) for the exact byte stream.

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

## Automatic detection: X40 or SEN0366 (`LaserRangefinderAuto`)

`LaserRangefinderAuto` detects at `begin()` whether an X-40 or a DFRobot SEN0366 is connected and then drives it through the **SEN0366 API**. One sketch works with both sensors.

```cpp
#include <LaserRangefinderAuto.h>

HardwareSerial       laserSerial(2);      // any free UART: Serial1, Serial2, HardwareSerial(1), ...
LaserRangefinderAuto laser(&laserSerial);

void setup() {
    laser.begin(rxPin, txPin);            // ESP32; optional 3rd argument true = also try RX/TX swapped
    Serial.println(laser.sensorName());   // "X40", "SEN0366" or "none"
    laser.controlLaser(SEN0366_LASER_ON);
}
void loop() {
    float m;
    if (laser.singleMeasurement(m)) Serial.println(m, 3);
}
```

* **UART:** the library only uses the `HardwareSerial` you pass in, so it works when `Serial2` is already taken. Do **not** call `begin()` on that port yourself: the library sets the baud rate (X40 19 200, SEN0366 9 600).
* **Detection** (~70 ms for an X40): first 19 200 baud + ASCII `S` (X40 status), then 9 600 baud + SEN0366 "laser off" frame. The X40 is probed first because a binary SEN0366 frame could be taken by the X40 as a letter command.
* **Migration from `LaserRangefinder_SEN0366`:** change the include and class name, remove `serialPort.begin(...)`, and replace `laser.begin()` with `laser.begin(rxPin, txPin)`. All other calls stay the same.
* Commands only the SEN0366 has (`setResolution`, `setFrequency`, `setAddress`, `reviseDistance`, ...) return `false` on an X40.
* `sensorType()` returns `RANGEFINDER_X40`, `RANGEFINDER_SEN0366` or `RANGEFINDER_NONE`; `x40()` / `sen0366()` give direct access to sensor-specific methods (e.g. `laser.x40().readStatus(&t, &v)`).
* `redetect()` runs detection again, e.g. after a sensor was swapped.
* **SEN0366 specifics (measured 2026-11-15 on the same turret):**
  * Detection takes ~0.5 s (X40 probe first, then SEN0366).
  * A measurement takes 730–1000 ms, on edges or weak targets up to ~3 s. The measurement timeout is therefore set to 4 s; the SEN0366 library default (1 s) lets late answers be read by the *next* measurement.
  * The answer to laser on **and** off is `80 06 85 01 F4` (address 0x80 even for broadcast, 4th byte always 01). `LaserRangefinder_SEN0366::controlLaser()` therefore reports OFF as failed; `LaserRangefinderAuto::controlLaser()` checks command code and checksum instead and returns the correct result.
* Requires the library [LaserRangefinder_SEN0366](https://github.com/smily77/Laser_Rangefinder_SEN0366_GJD-01).

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
laser.setIdleGap(60);                  // ms of bus silence = end of frame
laser.setMaxRange(0);                  // optional limit: readings above = OutOfRange; 0 = off (default)
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
| `ok` | `bool` | true if a valid distance was read (false for errors and OutOfRange) |
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
Status      lastStatus();        // OK, Timeout, ParseError, DeviceError, UnexpectedResponse, Unsupported, OutOfRange
int         lastErrorCode();     // numeric Er.xx code or -1
const String& lastRawResponse(); // always set after every command
```

---

## Protocol commands

| Byte | Command | Expected response |
|------|---------|------------------|
| `O` (0x4F) | Laser on | `O,OK!` |
| `C` (0x43) | Laser off | `C,OK!` |
| `S` (0x53) | Status | ``S: 24.8`C,3.3V`` |
| `D` (0x44) | Measure | `D: 1.559m,0613` or `D:Er08!` |
| `M` (0x4D) | Precise measure *(optional)* | tested unit: echo `M` only → `Unsupported` |
| `F` (0x46) | Fast measure *(optional)* | tested unit: echo `F` only → `Unsupported` |
| `V` (0x56) | Version *(optional)* | tested unit: echo `V` only → `Unsupported` |
| `X` (0x58) | Power down *(optional)* | not tested |

SQ (signal quality) after the comma: **lower is better**.  
Error responses: observed `:Er08!` (no usable reflection); the PDF draft also lists `Er.XX!`. Both are parsed.

### Observed protocol (X-40, tested 2026-11-15)

Each command letter is echoed first; the answer follows on the same line and ends with CR LF:

| Command | Bytes (time after sending) |
|---------|----------------------------|
| `S` | `S` (2 ms) … ``: 25.5`C,3.3V`` CR LF (16 ms) |
| `O` | `O` (2 ms) … `,OK!` CR LF (120 ms) |
| `D` | `D` (2 ms) … `: 1.557m,0684` CR LF (~500–620 ms) |
| `D`, no reflection | `D` … `:Er08!` CR LF (~1.8 s) |
| `C` | `C` (2 ms) … `,OK!` CR LF (9 ms) |

* The echo of the **first `D` after `O`** is sometimes a garbage byte (`0xF8`/`0xFC`) instead of `D`. The parser ignores it.
* The measurement reader does not stop at the echo: it waits until a number followed by `m` (or `Er`) has arrived, so a late answer is never left in the buffer for the next measurement.
* The module is rated for 40 m, so the library does not limit readings by default. In one setup a value of about **33.5 m** was seen without a target (not confirmed, possibly a disturbance). If your application needs it, set a limit with `setMaxRange(m)`: readings above it are returned as `OutOfRange` (`ok = false`, `meters` still filled).

---

## Troubleshooting

1. **No response at all** — swap RX/TX, check VCC level, verify 19 200 baud.  
2. **Garbled response** — check baud rate and level shifting; some units boot at a different rate.  
3. **`Er` responses** — `Er08` = no usable reflection (seen at open space / grazing angles). Others: target out of range, too much ambient light, low supply voltage, or temperature limit exceeded.  
4. **`M`/`F`/`V`/`X` return only the echo or time out** — those commands are optional and are not implemented on the tested unit. Use `lastStatus() == Unsupported` to detect this.  
5. **Implausible long readings (e.g. ~33.5 m without a target)** — set `setMaxRange()` to reject them as `OutOfRange`.

---

## Open points

| Item | How to resolve |
|------|---------------|
| Error code table (only `Er08` observed so far) | Fill in from official manual or further tests |
| `X` (power down) | Not tested yet |
