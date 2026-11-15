#include "X40LaserDistanceMeter.h"
#include <math.h>

X40LaserDistanceMeter::X40LaserDistanceMeter(HardwareSerial* serial)
    : _serial(serial),
      _timeout(4000),
      _idleGap(60),
      _maxRangeM(X40_DEFAULT_MAX_RANGE_M),
      _lastStatus(OK),
      _lastErrorCode(-1),
      _cachedDistance(NAN),
      _cacheValid(false)
{}

void X40LaserDistanceMeter::begin(uint32_t baudRate) {
    _serial->begin(baudRate, SERIAL_8N1);
    delay(100);
}

#if defined(ESP32)
void X40LaserDistanceMeter::begin(uint32_t baudRate, int rxPin, int txPin) {
    _serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
    delay(100);
}
#endif

void X40LaserDistanceMeter::setTimeout(uint32_t ms) { _timeout = ms; }
void X40LaserDistanceMeter::setIdleGap(uint32_t ms) { _idleGap = ms; }
void X40LaserDistanceMeter::setMaxRange(float meters) { _maxRangeM = meters; }

// ---- Internal communication ----

void X40LaserDistanceMeter::flushInput() {
    while (_serial->available()) _serial->read();
}

String X40LaserDistanceMeter::readResponse() {
    String s;
    s.reserve(32);
    uint32_t t0 = millis();
    uint32_t lastByte = millis();
    bool got = false;

    while (millis() - t0 < _timeout) {
        while (_serial->available()) {
            char c = (char)_serial->read();
            // Accept printable chars and line endings; skip bare control chars
            if (c >= 32 || c == '\r' || c == '\n') s += c;
            got = true;
            lastByte = millis();
            // CR or LF = end of response
            if (c == '\r' || c == '\n') {
                s.trim();
                return s;
            }
        }
        // Idle-gap: no new bytes for _idleGap ms — but only if we have more than
        // 1 byte already. A single byte is likely the command echo; don't exit early
        // before the actual response (which arrives after the module's processing delay).
        if (got && s.length() > 1 && (millis() - lastByte) >= _idleGap) break;
        delay(1);
    }
    s.trim();
    return s;
}

String X40LaserDistanceMeter::sendCommand(char cmd) {
    flushInput();
    _serial->write((uint8_t)cmd);
    return readResponse();
}

// Position of the distance unit: an 'm'/'M' directly preceded by a digit.
// A bare 'M' (echo of the M command) or other letters are ignored.
static int findUnitPos(const String& s) {
    for (int i = 1; i < (int)s.length(); i++) {
        if ((s[i] == 'm' || s[i] == 'M') && isDigit(s[i - 1])) return i;
    }
    return -1;
}

// True when s already contains a complete-looking distance or a device error.
static bool hasDistanceOrError(const String& s) {
    return findUnitPos(s) >= 0 || s.indexOf("Er") >= 0;
}

// The X40 answers a measurement command in two parts: first the echo of the
// command letter (e.g. "D", possibly with its own line ending), then ~1 s later
// the result ": 6.286m,1923" + line ending. readResponse() would stop after the
// echo, so measurements use this reader: it keeps reading across line endings
// until a distance (digit + 'm') or "Er" has arrived, then stops at the next
// line ending or after _idleGap ms of silence. Overall limit is _timeout.
String X40LaserDistanceMeter::readMeasurementResponse() {
    String s;
    s.reserve(32);
    uint32_t t0 = millis();
    uint32_t lastByte = 0;
    bool got = false;

    while (millis() - t0 < _timeout) {
        while (_serial->available()) {
            char c = (char)_serial->read();
            got = true;
            lastByte = millis();
            if (c == '\r' || c == '\n') {
                if (hasDistanceOrError(s)) {
                    s.trim();
                    return s;
                }
                // Line ending after the echo: keep parts apart, keep waiting
                if (s.length() > 0 && s[s.length() - 1] != ' ') s += ' ';
                continue;
            }
            if (c >= 32) s += c;
        }
        if (got && hasDistanceOrError(s) && (millis() - lastByte) >= _idleGap) break;
        delay(1);
    }
    s.trim();
    return s;
}

X40Measurement X40LaserDistanceMeter::runMeasurement(char cmd) {
    flushInput();
    _serial->write((uint8_t)cmd);
    String raw = readMeasurementResponse();
    _lastRaw = raw;
    return parseDistance(raw);
}

// ---- Parsers ----

X40Measurement X40LaserDistanceMeter::parseDistance(const String& raw) {
    X40Measurement result;
    result.ok = false;
    result.meters = NAN;
    result.millimeters = 0;
    result.signalQuality = -1;
    result.errorCode = -1;
    result.raw = raw;

    if (raw.length() == 0) {
        _lastStatus = Timeout;
        return result;
    }

    // Check for device error.
    // Observed format: ":Er08!"  (colon prefix, no dot between Er and code)
    // PDF-documented:  "Er.XX!"  (dot separator)
    // Both variants are handled: look for "Er" and optionally skip a following dot.
    int erPos = raw.indexOf("Er");
    if (erPos >= 0) {
        _lastStatus = DeviceError;
        String codeStr;
        int i = erPos + 2;
        if (i < (int)raw.length() && raw[i] == '.') i++; // skip optional dot (PDF format)
        while (i < (int)raw.length() && (isDigit(raw[i]) || isUpperCase(raw[i]))) {
            codeStr += raw[i++];
        }
        // Only map to int when the code consists purely of digits (e.g. "08" → 8)
        bool allDigits = (codeStr.length() > 0);
        for (int j = 0; j < (int)codeStr.length(); j++) {
            if (!isDigit(codeStr[j])) { allDigits = false; break; }
        }
        result.errorCode = allDigits ? codeStr.toInt() : -1;
        _lastErrorCode = result.errorCode;
        return result;
    }

    // Find unit marker: 'm' or 'M' directly after a digit (skips a command echo "M")
    int mpos = findUnitPos(raw);
    if (mpos < 0) {
        _lastStatus = ParseError;
        return result;
    }

    // Walk backwards from 'm' to collect the numeric string
    int start = mpos;
    while (start > 0) {
        char c = raw[start - 1];
        if (isDigit(c) || c == '.' || c == '-' || c == ' ') start--;
        else break;
    }
    String numStr = raw.substring(start, mpos);
    numStr.trim();
    if (numStr.length() == 0) {
        _lastStatus = ParseError;
        return result;
    }

    result.meters = numStr.toFloat();
    result.millimeters = (int32_t)round(result.meters * 1000.0f);

    // Signal quality after comma: "12.345m,0079"
    int commaPos = raw.indexOf(',', mpos);
    if (commaPos >= 0) {
        String sqStr = raw.substring(commaPos + 1);
        sqStr.trim();
        if (sqStr.length() > 0) {
            result.signalQuality = sqStr.toInt();
        }
    }

    _lastErrorCode = -1;

    // Optional plausibility limit (off by default, see setMaxRange()).
    if (_maxRangeM > 0.0f && result.meters > _maxRangeM) {
        _lastStatus = OutOfRange;   // meters/millimeters stay filled for diagnostics
        return result;
    }

    result.ok = true;
    _lastStatus = OK;
    return result;
}

bool X40LaserDistanceMeter::parseOK(const String& raw) {
    if (raw.length() == 0) {
        _lastStatus = Timeout;
        return false;
    }
    // Accept "OK", ",OK!", or any response containing "OK" (case-insensitive)
    String upper = raw;
    upper.toUpperCase();
    if (upper.indexOf("OK") >= 0) {
        _lastStatus = OK;
        return true;
    }
    _lastStatus = UnexpectedResponse;
    return false;
}

// ---- SEN0366-compatible measurement API ----

bool X40LaserDistanceMeter::singleMeasurement(float& distance) {
    X40Measurement m = measure();
    if (m.ok) {
        distance = m.meters;
        return true;
    }
    distance = NAN;
    return false;
}

bool X40LaserDistanceMeter::startContinuousMeasurement() {
    String raw = sendCommand('O');
    _lastRaw = raw;
    return parseOK(raw);
}

bool X40LaserDistanceMeter::readContinuousDistance(float& distance) {
    X40Measurement m = measure();
    if (m.ok) {
        distance = m.meters;
        return true;
    }
    distance = NAN;
    return false;
}

bool X40LaserDistanceMeter::stopContinuousMeasurement() {
    String raw = sendCommand('C');
    _lastRaw = raw;
    return parseOK(raw);
}

void X40LaserDistanceMeter::takeSingleMeasurementToCache() {
    X40Measurement m = measure();
    if (m.ok) {
        _cachedDistance = m.meters;
        _cacheValid = true;
    } else {
        _cacheValid = false;
    }
}

bool X40LaserDistanceMeter::readCache(float& distance) {
    if (_cacheValid) {
        distance = _cachedDistance;
        return true;
    }
    distance = NAN;
    return false;
}

// ---- SEN0366-compatible control API ----

bool X40LaserDistanceMeter::controlLaser(uint8_t on) {
    String raw = sendCommand(on ? 'O' : 'C');
    _lastRaw = raw;
    return parseOK(raw);
}

bool X40LaserDistanceMeter::shutDown() {
    String raw = sendCommand('X');
    _lastRaw = raw;
    // X command may return nothing or "OK" — both are acceptable
    if (raw.length() == 0) {
        _lastStatus = OK;
        return true;
    }
    return parseOK(raw);
}

bool X40LaserDistanceMeter::readMachineNumber(char* buffer) {
    String raw = sendCommand('V');
    _lastRaw = raw;
    if (raw.length() <= 1) {        // nothing, or only the command echo "V"
        _lastStatus = raw.length() ? Unsupported : Timeout;
        if (buffer) buffer[0] = '\0';
        return false;
    }
    if (buffer) {
        strncpy(buffer, raw.c_str(), 31);
        buffer[31] = '\0';
    }
    _lastStatus = OK;
    return true;
}

// ---- SEN0366 config stubs (not supported by X40 single-letter protocol) ----

bool X40LaserDistanceMeter::setAddress(uint8_t)              { _lastStatus = Unsupported; return false; }
bool X40LaserDistanceMeter::setResolution(uint8_t)           { _lastStatus = Unsupported; return false; }
bool X40LaserDistanceMeter::setFrequency(uint8_t)            { _lastStatus = Unsupported; return false; }
bool X40LaserDistanceMeter::setDataReturnInterval(uint8_t)   { _lastStatus = Unsupported; return false; }
bool X40LaserDistanceMeter::setMeasurementStartingPoint(uint8_t) { _lastStatus = Unsupported; return false; }
bool X40LaserDistanceMeter::setAutoStart(uint8_t)            { _lastStatus = Unsupported; return false; }
uint8_t X40LaserDistanceMeter::readParameter(uint8_t*, uint8_t) { _lastStatus = Unsupported; return 0; }

// ---- X40-specific extended methods ----

bool X40LaserDistanceMeter::readStatus(float* temperatureC, float* supplyV) {
    String raw = sendCommand('S');
    _lastRaw = raw;
    if (raw.length() == 0) {
        _lastStatus = Timeout;
        return false;
    }

    float temp = NAN, volt = NAN;

    // Temperature: number before 'C'. Up to 3 degree-like characters between
    // number and 'C' are skipped: "18.0C", "18.0'C", "24.8`C" (observed), "18.0°C"
    int cpos = raw.indexOf('C');
    if (cpos > 0) {
        int end = cpos;
        for (int k = 0; k < 3 && end > 0 && !isDigit(raw[end - 1]) && raw[end - 1] != ','; k++) end--;
        int start = end;
        while (start > 0) {
            char c = raw[start - 1];
            if (isDigit(c) || c == '.' || c == '-') start--;
            else break;
        }
        if (start < end && isDigit(raw[end - 1])) temp = raw.substring(start, end).toFloat();
    }

    // Voltage: first float before 'V'
    int vpos = raw.indexOf('V');
    if (vpos > 0) {
        int start = vpos;
        while (start > 0) {
            char c = raw[start - 1];
            if (isDigit(c) || c == '.' || c == '-' || c == ' ') start--;
            else break;
        }
        String vStr = raw.substring(start, vpos);
        vStr.trim();
        if (vStr.length() > 0) volt = vStr.toFloat();
    }

    if (isnan(temp) && isnan(volt)) {
        _lastStatus = ParseError;
        return false;
    }
    if (temperatureC) *temperatureC = temp;
    if (supplyV)      *supplyV = volt;
    _lastStatus = OK;
    return true;
}

bool X40LaserDistanceMeter::readVersion(String* versionText) {
    String raw = sendCommand('V');
    _lastRaw = raw;
    if (raw.length() == 0) {
        _lastStatus = Timeout;
        return false;
    }
    if (raw.length() <= 1) {        // only the command echo "V": not supported
        _lastStatus = Unsupported;
        return false;
    }
    if (versionText) *versionText = raw;
    _lastStatus = OK;
    return true;
}

X40Measurement X40LaserDistanceMeter::measure() {
    return runMeasurement('D');
}

X40Measurement X40LaserDistanceMeter::measurePrecise() {
    X40Measurement m = runMeasurement('M');
    // No answer (or only the echo) on an optional command: likely unsupported
    if (!m.ok && (_lastStatus == Timeout ||
                  (_lastStatus == ParseError && m.raw.length() <= 1)))
        _lastStatus = Unsupported;
    return m;
}

X40Measurement X40LaserDistanceMeter::measureFast() {
    X40Measurement m = runMeasurement('F');
    if (!m.ok && (_lastStatus == Timeout ||
                  (_lastStatus == ParseError && m.raw.length() <= 1)))
        _lastStatus = Unsupported;
    return m;
}

int32_t X40LaserDistanceMeter::readRangeMillimeters() {
    X40Measurement m = measure();
    return m.ok ? m.millimeters : -1;
}

float X40LaserDistanceMeter::readRangeMeters() {
    X40Measurement m = measure();
    return m.ok ? m.meters : NAN;
}

// ---- Diagnostics ----

X40LaserDistanceMeter::Status X40LaserDistanceMeter::lastStatus() const { return _lastStatus; }
int X40LaserDistanceMeter::lastErrorCode() const                        { return _lastErrorCode; }
const String& X40LaserDistanceMeter::lastRawResponse() const            { return _lastRaw; }
