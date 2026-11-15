/**
 * LaserRangefinderAuto — one API for the DFRobot SEN0366 and the X-40 / Hi-AT
 *
 * Detects at begin() which rangefinder is connected and forwards every call to
 * the matching driver. The public API is the one of LaserRangefinder_SEN0366,
 * so existing SEN0366 sketches only change the class name and begin().
 *
 * UART: the library uses exactly the HardwareSerial you pass in (Serial1,
 * Serial2, HardwareSerial myUart(1), ...). It never picks a port by itself.
 * It does set the baud rate itself (X40 19200, SEN0366 9600) - do NOT call
 * serial.begin() in the sketch for this port.
 *
 * Detection order (safe for both devices):
 *   1. 19200 baud, ASCII 'S' (read status)   -> X40 answers "S: 25.5`C,3.3V"
 *   2.  9600 baud, SEN0366 "laser off" frame -> SEN0366 answers 80 06 85 01 F4
 * The X40 is probed first because a binary SEN0366 frame could be read by an
 * X40 as a letter command; a single ASCII byte is discarded by the SEN0366
 * (checksum).
 *
 * Requires the library LaserRangefinder_SEN0366 (smily77/Laser_Rangefinder_SEN0366_GJD-01).
 */
#pragma once
#include <Arduino.h>
#include <LaserRangefinder_SEN0366.h>
#include "X40LaserDistanceMeter.h"

enum RangefinderType : uint8_t {
    RANGEFINDER_NONE = 0,
    RANGEFINDER_SEN0366,
    RANGEFINDER_X40
};

class LaserRangefinderAuto {
public:
    explicit LaserRangefinderAuto(HardwareSerial* serial);
    // SEN0366 with individual address (0x80..0xF9); X40 ignores the address
    LaserRangefinderAuto(HardwareSerial* serial, uint8_t sen0366Address);

    // Detect the sensor. Returns true if one was found.
#if defined(ESP32)
    // trySwappedPins: also try with RX/TX exchanged (unknown wiring)
    bool begin(int8_t rxPin, int8_t txPin, bool trySwappedPins = false);
#endif
    bool begin();                       // uses the port's current/default pins
    bool redetect();                    // run detection again with the same pins

    RangefinderType sensorType() const { return _type; }
    const char*     sensorName() const;
    bool            isConnected() const { return _type != RANGEFINDER_NONE; }
#if defined(ESP32)
    int8_t rxPin() const { return _rx; } // pins actually in use after begin()
    int8_t txPin() const { return _tx; }
#endif

    // ---- SEN0366-compatible API (distance in METERS) ----
    bool singleMeasurement(float& distance);
    bool startContinuousMeasurement();
    bool readContinuousDistance(float& distance);
    bool stopContinuousMeasurement();
    void takeSingleMeasurementToCache();
    bool readCache(float& distance);

    bool setAddress(uint8_t newAddress);            // SEN0366 only
    bool setResolution(uint8_t resolution);         // SEN0366 only
    bool setFrequency(uint8_t frequency);           // SEN0366 only
    bool setDataReturnInterval(uint8_t interval);   // SEN0366 only
    bool setMeasurementStartingPoint(uint8_t pos);  // SEN0366 only
    bool setAutoStart(uint8_t enable);              // SEN0366 only
    bool reviseDistance(bool positive, uint8_t adj);// SEN0366 only

    bool controlLaser(uint8_t on);                  // SEN0366_LASER_ON / _OFF
    bool shutDown();
    uint8_t readParameter(uint8_t* data, uint8_t maxLen); // SEN0366 only
    bool readMachineNumber(char* buffer);           // buffer >= 32 bytes

    void    setDeviceAddress(uint8_t address);      // SEN0366 only
    uint8_t getDeviceAddress() const;
    bool    isBroadcast() const;
    void    setTimeout(uint32_t timeoutMs);         // for the detected sensor
    void    setDebug(bool enable, Stream* debugSerial = &Serial);

    // Direct access for sensor-specific features (e.g. x40().readStatus())
    X40LaserDistanceMeter&    x40()     { return _x40; }
    LaserRangefinder_SEN0366& sen0366() { return _sen; }

private:
    HardwareSerial*          _serial;
    LaserRangefinder_SEN0366 _sen;
    X40LaserDistanceMeter    _x40;
    RangefinderType          _type;
    bool                     _havePins;
    int8_t                   _rx, _tx;
    bool                     _trySwap;
    Stream*                  _debugSerial;
    bool                     _debug;

    void openPort(uint32_t baud, int8_t rx, int8_t tx);
    bool probeX40(int8_t rx, int8_t tx);
    bool probeSEN0366(int8_t rx, int8_t tx);
    bool senLaser(uint8_t on, uint32_t timeoutMs);
    bool detect();
};
