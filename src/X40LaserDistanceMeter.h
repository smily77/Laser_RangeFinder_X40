#pragma once
#include <Arduino.h>

#define X40_LASER_ON  1
#define X40_LASER_OFF 0

// Optional plausibility limit: readings above setMaxRange() are reported as
// OutOfRange (ok=false). Off by default (0) - the module is rated for 40 m.
// In CatFind ~33.5 m was once seen without a target; cause not confirmed.
#define X40_DEFAULT_MAX_RANGE_M 0.0f

struct X40Measurement {
    bool ok;
    float meters;
    int32_t millimeters;
    int signalQuality;  // -1 if not present in response
    int errorCode;      // -1 if no Er.xx device error
    String raw;
};

struct X40Status {
    bool valid;
    float temperatureC;
    float supplyV;
    String raw;
};

class X40LaserDistanceMeter {
public:
    enum Status {
        OK = 0,
        Timeout,
        ParseError,
        DeviceError,
        UnexpectedResponse,
        Unsupported,
        OutOfRange      // reading above setMaxRange() (only if a limit is set)
    };

    explicit X40LaserDistanceMeter(HardwareSerial* serial);

    // begin() with optional RX/TX pins (ESP32 / HardwareSerial with pin selection)
    void begin(uint32_t baudRate = 19200);
#if defined(ESP32)
    void begin(uint32_t baudRate, int rxPin, int txPin);
#endif

    // Timing configuration
    void setTimeout(uint32_t ms);   // default 4000 ms (one measurement takes ~1 s)
    void setIdleGap(uint32_t ms);   // default 60 ms after last byte
    void setMaxRange(float meters); // default 0 = no check

    // --- SEN0366-compatible measurement API ---
    bool singleMeasurement(float& distance);        // sends 'D', returns meters
    bool startContinuousMeasurement();              // sends 'O' (laser on)
    bool readContinuousDistance(float& distance);   // sends 'D', returns meters
    bool stopContinuousMeasurement();               // sends 'C' (laser off)
    void takeSingleMeasurementToCache();            // sends 'D', stores internally
    bool readCache(float& distance);                // returns last cached value

    // --- SEN0366-compatible control API ---
    bool controlLaser(uint8_t on);                  // X40_LASER_ON / X40_LASER_OFF
    bool shutDown();                                // sends 'X' (optional, may timeout)
    bool readMachineNumber(char* buffer);           // sends 'V', copies text to buffer (min 32 bytes)

    // --- SEN0366 config methods: not supported by X40 ---
    bool setAddress(uint8_t newAddress);
    bool setResolution(uint8_t resolution);
    bool setFrequency(uint8_t frequency);
    bool setDataReturnInterval(uint8_t interval);
    bool setMeasurementStartingPoint(uint8_t position);
    bool setAutoStart(uint8_t enable);
    uint8_t readParameter(uint8_t* data, uint8_t maxLen);

    // --- X40-specific extended methods ---
    bool readStatus(float* temperatureC, float* supplyV);   // sends 'S'
    bool readVersion(String* versionText);                  // sends 'V'

    X40Measurement measure();           // sends 'D'
    X40Measurement measurePrecise();    // sends 'M' (optional, may return Unsupported)
    X40Measurement measureFast();       // sends 'F' (optional, may return Unsupported)

    int32_t readRangeMillimeters();     // -1 on error
    float readRangeMeters();            // NAN on error

    // --- Diagnostics ---
    Status lastStatus() const;
    int lastErrorCode() const;
    const String& lastRawResponse() const;

private:
    HardwareSerial* _serial;
    uint32_t _timeout;
    uint32_t _idleGap;
    float _maxRangeM;
    Status _lastStatus;
    int _lastErrorCode;
    String _lastRaw;
    float _cachedDistance;
    bool _cacheValid;

    void flushInput();
    String sendCommand(char cmd);
    String readResponse();
    String readMeasurementResponse();
    X40Measurement runMeasurement(char cmd);
    X40Measurement parseDistance(const String& raw);
    bool parseOK(const String& raw);
};
