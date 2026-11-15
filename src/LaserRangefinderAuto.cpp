#include "LaserRangefinderAuto.h"

static const uint32_t X40_BAUD        = 19200;
static const uint32_t PROBE_TIMEOUT   = 400;   // ms per probe
static const uint32_t X40_RUN_TIMEOUT = 4000;  // one X40 measurement: 0.6 s, no target 1.8 s
// SEN0366 measurements take 730..1000 ms on the device, up to 2.2 s on edges or
// weak targets (measured 2026-11-15). With the SEN0366 library's default of
// 1000 ms such an answer arrives late and is read by the NEXT measurement.
// The driver returns as soon as the answer is complete, so the longer limit
// only matters when the sensor is slow or silent.
static const uint32_t SEN_RUN_TIMEOUT = 4000;

LaserRangefinderAuto::LaserRangefinderAuto(HardwareSerial* serial)
    : _serial(serial), _sen(serial), _x40(serial), _type(RANGEFINDER_NONE),
      _havePins(false), _rx(-1), _tx(-1), _trySwap(false),
      _debugSerial(&Serial), _debug(false) {}

LaserRangefinderAuto::LaserRangefinderAuto(HardwareSerial* serial, uint8_t sen0366Address)
    : _serial(serial), _sen(serial, sen0366Address), _x40(serial), _type(RANGEFINDER_NONE),
      _havePins(false), _rx(-1), _tx(-1), _trySwap(false),
      _debugSerial(&Serial), _debug(false) {}

// ---- Detection ----

void LaserRangefinderAuto::openPort(uint32_t baud, int8_t rx, int8_t tx) {
    _serial->end();
#if defined(ESP32)
    if (_havePins) _serial->begin(baud, SERIAL_8N1, rx, tx);
    else           _serial->begin(baud, SERIAL_8N1);
#else
    (void)rx; (void)tx;
    _serial->begin(baud);
#endif
    delay(50);
    while (_serial->available()) _serial->read();
}

bool LaserRangefinderAuto::probeX40(int8_t rx, int8_t tx) {
    openPort(X40_BAUD, rx, tx);
    _x40.setTimeout(PROBE_TIMEOUT);
    float t = NAN, v = NAN;
    bool ok = _x40.readStatus(&t, &v);
    if (_debug) {
        _debugSerial->print(F("[Auto] X40 probe RX=")); _debugSerial->print(rx);
        _debugSerial->print(F(" TX=")); _debugSerial->print(tx);
        _debugSerial->print(F(": '")); _debugSerial->print(_x40.lastRawResponse());
        _debugSerial->println('\'');
    }
    _x40.setTimeout(X40_RUN_TIMEOUT);
    return ok;
}

// Laser on/off for the SEN0366, checked by command code and checksum.
// Observed on the device (2026-11-15): the answer is 80 06 85 01 F4 for BOTH
// on and off, and the address byte is the device address (0x80) even when the
// command was sent to broadcast 0xFA. LaserRangefinder_SEN0366::controlLaser()
// compares both bytes strictly and therefore always reports OFF as failed.
bool LaserRangefinderAuto::senLaser(uint8_t on, uint32_t timeoutMs) {
    uint8_t cmd[5] = { _sen.getDeviceAddress(), 0x06, 0x05, (uint8_t)(on ? 1 : 0), 0 };
    cmd[4] = (uint8_t)(~(cmd[0] + cmd[1] + cmd[2] + cmd[3]) + 1);
    while (_serial->available()) _serial->read();
    _serial->write(cmd, sizeof(cmd));
    _serial->flush();

    uint8_t r[5];
    uint8_t n = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs && n < sizeof(r)) {
        if (_serial->available()) {
            uint8_t b = _serial->read();
            if (n == 0 && b < 0x80) continue;      // frames start with an address >= 0x80
            r[n++] = b;
        } else {
            delay(1);
        }
    }
    if (_debug) {
        _debugSerial->print(F("[Auto] SEN0366 laser "));
        _debugSerial->print(on ? F("on") : F("off"));
        _debugSerial->print(F(" ->"));
        for (uint8_t i = 0; i < n; i++) { _debugSerial->print(' '); _debugSerial->print(r[i], HEX); }
        _debugSerial->println();
    }
    if (n < 5 || r[1] != 0x06 || r[2] != 0x85) return false;
    uint8_t sum = r[0] + r[1] + r[2] + r[3] + r[4];
    return sum == 0;                                // two's-complement checksum
}

bool LaserRangefinderAuto::probeSEN0366(int8_t rx, int8_t tx) {
    openPort(SEN0366_BAUD_RATE, rx, tx);
    // Laser off: harmless, answer 06 85 is checked including the checksum
    bool ok = senLaser(SEN0366_LASER_OFF, PROBE_TIMEOUT);
    if (_debug) {
        _debugSerial->print(F("[Auto] SEN0366 probe RX=")); _debugSerial->print(rx);
        _debugSerial->print(F(" TX=")); _debugSerial->print(tx);
        _debugSerial->println(ok ? F(": OK") : F(": no answer"));
    }
    return ok;
}

bool LaserRangefinderAuto::detect() {
    _type = RANGEFINDER_NONE;
    const int tries = (_havePins && _trySwap) ? 2 : 1;
    for (int i = 0; i < tries; i++) {
        int8_t rx = (i == 0) ? _rx : _tx;
        int8_t tx = (i == 0) ? _tx : _rx;
        // X40 first: an ASCII byte is harmless for the SEN0366, not vice versa
        if (probeX40(rx, tx)) {
            _type = RANGEFINDER_X40;
        } else if (probeSEN0366(rx, tx)) {
            _type = RANGEFINDER_SEN0366;
        }
        if (_type != RANGEFINDER_NONE) {
            _rx = rx; _tx = tx;
            if (_type == RANGEFINDER_X40) {
                _x40.controlLaser(X40_LASER_OFF);   // same start state as SEN0366 probe
            } else {
                _sen.setTimeout(SEN_RUN_TIMEOUT);
            }
            return true;
        }
    }
    return false;
}

#if defined(ESP32)
bool LaserRangefinderAuto::begin(int8_t rxPin, int8_t txPin, bool trySwappedPins) {
    _havePins = true;
    _rx = rxPin; _tx = txPin;
    _trySwap = trySwappedPins;
    return detect();
}
#endif

bool LaserRangefinderAuto::begin() {
    _havePins = false;
    return detect();
}

bool LaserRangefinderAuto::redetect() {
    return detect();
}

const char* LaserRangefinderAuto::sensorName() const {
    switch (_type) {
        case RANGEFINDER_SEN0366: return "SEN0366";
        case RANGEFINDER_X40:     return "X40";
        default:                  return "none";
    }
}

// ---- Forwarding ----

#define DISPATCH(senCall, x40Call, noneValue)            \
    switch (_type) {                                     \
        case RANGEFINDER_SEN0366: return senCall;        \
        case RANGEFINDER_X40:     return x40Call;        \
        default:                  return noneValue;      \
    }

bool LaserRangefinderAuto::singleMeasurement(float& d) {
    if (_type == RANGEFINDER_NONE) { d = NAN; return false; }
    DISPATCH(_sen.singleMeasurement(d), _x40.singleMeasurement(d), false)
}
bool LaserRangefinderAuto::startContinuousMeasurement() {
    DISPATCH(_sen.startContinuousMeasurement(), _x40.startContinuousMeasurement(), false)
}
bool LaserRangefinderAuto::readContinuousDistance(float& d) {
    if (_type == RANGEFINDER_NONE) { d = NAN; return false; }
    DISPATCH(_sen.readContinuousDistance(d), _x40.readContinuousDistance(d), false)
}
bool LaserRangefinderAuto::stopContinuousMeasurement() {
    DISPATCH(_sen.stopContinuousMeasurement(), _x40.stopContinuousMeasurement(), false)
}
void LaserRangefinderAuto::takeSingleMeasurementToCache() {
    if (_type == RANGEFINDER_SEN0366) _sen.takeSingleMeasurementToCache();
    else if (_type == RANGEFINDER_X40) _x40.takeSingleMeasurementToCache();
}
bool LaserRangefinderAuto::readCache(float& d) {
    if (_type == RANGEFINDER_NONE) { d = NAN; return false; }
    DISPATCH(_sen.readCache(d), _x40.readCache(d), false)
}

bool LaserRangefinderAuto::setAddress(uint8_t a)               { DISPATCH(_sen.setAddress(a), false, false) }
bool LaserRangefinderAuto::setResolution(uint8_t r)            { DISPATCH(_sen.setResolution(r), false, false) }
bool LaserRangefinderAuto::setFrequency(uint8_t f)             { DISPATCH(_sen.setFrequency(f), false, false) }
bool LaserRangefinderAuto::setDataReturnInterval(uint8_t i)    { DISPATCH(_sen.setDataReturnInterval(i), false, false) }
bool LaserRangefinderAuto::setMeasurementStartingPoint(uint8_t p) { DISPATCH(_sen.setMeasurementStartingPoint(p), false, false) }
bool LaserRangefinderAuto::setAutoStart(uint8_t e)             { DISPATCH(_sen.setAutoStart(e), false, false) }
bool LaserRangefinderAuto::reviseDistance(bool pos, uint8_t a) { DISPATCH(_sen.reviseDistance(pos, a), false, false) }

bool LaserRangefinderAuto::controlLaser(uint8_t on) {
    DISPATCH(senLaser(on, SEN0366_TIMEOUT_MS), _x40.controlLaser(on ? X40_LASER_ON : X40_LASER_OFF), false)
}
bool LaserRangefinderAuto::shutDown() {
    DISPATCH(_sen.shutDown(), _x40.shutDown(), false)
}
uint8_t LaserRangefinderAuto::readParameter(uint8_t* data, uint8_t maxLen) {
    DISPATCH(_sen.readParameter(data, maxLen), (uint8_t)0, (uint8_t)0)
}
bool LaserRangefinderAuto::readMachineNumber(char* buffer) {
    if (_type == RANGEFINDER_NONE) { if (buffer) buffer[0] = '\0'; return false; }
    DISPATCH(_sen.readMachineNumber(buffer), _x40.readMachineNumber(buffer), false)
}

void    LaserRangefinderAuto::setDeviceAddress(uint8_t a) { _sen.setDeviceAddress(a); }
uint8_t LaserRangefinderAuto::getDeviceAddress() const    { return _sen.getDeviceAddress(); }
bool    LaserRangefinderAuto::isBroadcast() const         { return _sen.isBroadcast(); }

void LaserRangefinderAuto::setTimeout(uint32_t ms) {
    if (_type == RANGEFINDER_SEN0366) _sen.setTimeout(ms);
    else if (_type == RANGEFINDER_X40) _x40.setTimeout(ms);
}

void LaserRangefinderAuto::setDebug(bool enable, Stream* debugSerial) {
    _debug = enable;
    _debugSerial = debugSerial ? debugSerial : &Serial;
    _sen.setDebug(enable, _debugSerial);
}
