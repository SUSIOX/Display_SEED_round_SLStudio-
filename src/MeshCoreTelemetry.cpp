#include "MeshCoreTelemetry.h"
#include <stdio.h>

// KISS Protocol Constants
#define KISS_FEND  0xC0  // Frame End
#define KISS_FESC  0xDB  // Frame Escape
#define KISS_TFEND 0xDC  // Transposed Frame End
#define KISS_TFESC 0xDD  // Transposed Frame Escape
#define KISS_TYPE_DATA 0x00  // Data frame type

MeshCoreTelemetry::MeshCoreTelemetry() : _serial(nullptr), _lastSendTime(0), _interval(30000) {
    _inputBuffer.reserve(32); // Pre-allocate for efficiency
}

void MeshCoreTelemetry::begin(Stream* serial, uint32_t baud) {
    _serial = serial;
    // Note: The hardware serial instance (Serial2) should be initialized 
    // by the caller (main.cpp) before calling this.
}

// KISS Framing helpers
void MeshCoreTelemetry::kissWriteEscaped(uint8_t c) {
    if (c == KISS_FEND) {
        _serial->write(KISS_FESC);
        _serial->write(KISS_TFEND);
    } else if (c == KISS_FESC) {
        _serial->write(KISS_FESC);
        _serial->write(KISS_TFESC);
    } else {
        _serial->write(c);
    }
}

void MeshCoreTelemetry::kissBeginFrame(uint8_t type) {
    if (!_serial) return;
    _serial->write(KISS_FEND);
    _serial->write(type);
}

void MeshCoreTelemetry::kissEndFrame() {
    if (!_serial) return;
    _serial->write(KISS_FEND);
}

void MeshCoreTelemetry::sendKissFrame(const char* data, size_t len) {
    if (!_serial) return;
    kissBeginFrame(KISS_TYPE_DATA);
    for (size_t i = 0; i < len; i++) {
        kissWriteEscaped(data[i]);
    }
    kissEndFrame();
}

void MeshCoreTelemetry::sendKissNMEASentence(const char* sentence, const char* prefix) {
    if (!_serial) return;
    
    // Build full NMEA sentence with $ and checksum
    char fullSentence[150];
    uint8_t checksum = calculateChecksum(sentence);
    snprintf(fullSentence, sizeof(fullSentence), "%s%s*%02X\r\n", prefix, sentence, checksum);
    
    sendKissFrame(fullSentence, strlen(fullSentence));
}

void MeshCoreTelemetry::handleSerialInput() {
    if (!_serial) return;

    while (_serial->available()) {
        char c = (char)_serial->read();
        if (c == '\n' || c == '\r') {
            if (_inputBuffer.length() > 0) {
                // Command processing: !SET_INT:ms
                if (_inputBuffer.startsWith("!SET_INT:")) {
                    String valStr = _inputBuffer.substring(9);
                    uint32_t newInterval = valStr.toInt();
                    char response[80];
                    if (newInterval >= 1000 && newInterval <= 300000) {
                        _interval = newInterval;
                        snprintf(response, sizeof(response), "# OK: Interval set to %ums", _interval);
                    } else {
                        strcpy(response, "# ERROR: Interval out of range (1000-300000ms)");
                    }
                    sendKissFrame(response, strlen(response));
                }
                _inputBuffer = ""; // Clear for next command
            }
        } else {
            // Guard against buffer overflow
            if (_inputBuffer.length() < 32) {
                _inputBuffer += c;
            } else {
                _inputBuffer = ""; // Reset if too long
            }
        }
    }
}

void MeshCoreTelemetry::sendHome(const MAVLinkData& data) {
    if (!_serial || !data.home_set) return;

    char latBuf[20], lonBuf[20];
    formatNMEACoord(data.home_lat, latBuf, true);
    formatNMEACoord(data.home_lon, lonBuf, false);

    char sentence[100];
    snprintf(sentence, sizeof(sentence), "GPHOME,%s,%s,%.1f", 
             latBuf, lonBuf, data.home_alt / 1000.0);

    sendKissNMEASentence(sentence, "$");
    Serial.println("[MeshCore] Sent $GPHOME (KISS)");
}

void MeshCoreTelemetry::sendTransition(const MAVLinkData& data) {
    if (!_serial || !data.data_valid) return;

    char latBuf[20], lonBuf[20];
    formatNMEACoord(data.gps_lat, latBuf, true);
    formatNMEACoord(data.gps_lon, lonBuf, false);

    char sentence[100];
    snprintf(sentence, sizeof(sentence), "GPTRN,%s,%s,%.1f", 
             latBuf, lonBuf, data.gps_alt / 1000.0);

    sendKissNMEASentence(sentence, "$");
    Serial.println("[MeshCore] Sent $GPTRN (VTOL Transition, KISS)");
}

void MeshCoreTelemetry::update(const MAVLinkData& data) {
    if (!_serial || !data.data_valid) return;
    
    unsigned long currentTime = millis();
    if (currentTime - _lastSendTime >= _interval) {
        sendGPGGA(data);
        sendGPRMC(data);
        _lastSendTime = currentTime;
    }
}

void MeshCoreTelemetry::sendGPGGA(const MAVLinkData& data) {
    char latBuf[20], lonBuf[20];
    formatNMEACoord(data.gps_lat, latBuf, true);
    formatNMEACoord(data.gps_lon, lonBuf, false);

    // Time from Unix usec (HHMMSS)
    uint32_t seconds = (data.time_unix_usec / 1000000ULL) % 86400;
    int hh = seconds / 3600;
    int mm = (seconds % 3600) / 60;
    int ss = seconds % 60;

    char sentence[120];
    snprintf(sentence, sizeof(sentence), 
        "GPGGA,%02d%02d%02d.00,%s,%s,%d,%02d,1.0,%.1f,M,0.0,M,,",
        hh, mm, ss, latBuf, lonBuf, 
        data.gps_fix_type > 1 ? 1 : 0, 
        data.gps_satellites,
        data.gps_alt / 1000.0);

    sendKissNMEASentence(sentence, "$");
}

void MeshCoreTelemetry::sendGPRMC(const MAVLinkData& data) {
    char latBuf[20], lonBuf[20];
    formatNMEACoord(data.gps_lat, latBuf, true);
    formatNMEACoord(data.gps_lon, lonBuf, false);

    uint32_t seconds = (data.time_unix_usec / 1000000ULL) % 86400;
    int hh = seconds / 3600;
    int mm = (seconds % 3600) / 60;
    int ss = seconds % 60;

    char sentence[120];
    snprintf(sentence, sizeof(sentence), 
        "GPRMC,%02d%02d%02d.00,A,%s,%s,0.0,%.1f,070426,,,A",
        hh, mm, ss, latBuf, lonBuf, (float)data.heading / 100.0f);

    sendKissNMEASentence(sentence, "$");
}

uint8_t MeshCoreTelemetry::calculateChecksum(const char* s) {
    uint8_t checksum = 0;
    while (*s) {
        checksum ^= (uint8_t)*s++;
    }
    return checksum;
}

void MeshCoreTelemetry::formatNMEACoord(int32_t coord, char* buf, bool isLat) {
    double decimalDegrees = (double)coord / 10000000.0;
    double absDegrees = fabs(decimalDegrees);
    int degrees = (int)absDegrees;
    double minutes = (absDegrees - degrees) * 60.0;
    
    char direction;
    if (isLat) {
        direction = (decimalDegrees >= 0) ? 'N' : 'S';
        snprintf(buf, 20, "%02d%07.4f,%c", degrees, minutes, direction);
    } else {
        direction = (decimalDegrees >= 0) ? 'E' : 'W';
        snprintf(buf, 20, "%03d%07.4f,%c", degrees, minutes, direction);
    }
}
