#ifndef MESHCORE_TELEMETRY_H
#define MESHCORE_TELEMETRY_H

#include <Arduino.h>
#include "xiao_pinout.h"
#include "mavlink_data_types.h"

class MeshCoreTelemetry {
public:
    MeshCoreTelemetry();
    
    // Initialize with Serial port
    void begin(Stream* serial, uint32_t baud = 115200);
    
    // Process and send telemetry at specified intervals
    void update(const MAVLinkData& data);

    // Check for incoming commands from MeshCore
    void handleSerialInput();

    // Event-based transmission
    void sendHome(const MAVLinkData& data);
    void sendTransition(const MAVLinkData& data);

    // Configuration
    void setInterval(uint32_t ms) { _interval = ms; }
    uint32_t getInterval() const { return _interval; }

private:
    Stream* _serial;
    unsigned long _lastSendTime;
    uint32_t _interval; // Interval in milliseconds
    String _inputBuffer; // Accumulator for incoming serial data

    // Helper functions for NMEA generation
    void sendGPGGA(const MAVLinkData& data);
    void sendGPRMC(const MAVLinkData& data);
    
    // Calculate NMEA checksum
    uint8_t calculateChecksum(const char* s);
    
    // Convert MAVLink lat/lon to NMEA degrees.minutes format
    void formatNMEACoord(int32_t coord, char* buf, bool isLat);
};

#endif // MESHCORE_TELEMETRY_H
