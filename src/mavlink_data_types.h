#ifndef MAVLINK_DATA_TYPES_H
#define MAVLINK_DATA_TYPES_H

#include <Arduino.h>

// Shared MAVLink data structure
struct MAVLinkData {
    float voltage;
    float current;
    uint8_t percent;
    unsigned long last_update;
    bool data_valid;
    unsigned long last_heartbeat;
    bool heartbeat_received;
    // Extended battery info
    int16_t temperature;        // Temperature in 0.01 degrees C
    int32_t time_remaining;     // Seconds
    uint8_t charge_state;       // Charge state
    uint16_t cell_voltages[6];  // First 6 cell voltages in mV
    uint8_t cell_count;         // Number of cells detected
    // Heartbeat info
    uint8_t autopilot_type;     // MAV_AUTOPILOT enum
    uint8_t system_type;        // MAV_TYPE enum
    uint8_t base_mode;
    uint8_t system_status;
    uint32_t heartbeat_count;
    // Autopilot version info
    uint32_t flight_sw_version;
    uint32_t board_version;
    uint16_t vendor_id;
    uint16_t product_id;
    bool version_received;
    // CPU Temperatures
    float esp32_temperature;
    float mavlink_cpu_temp;
    // SYS_STATUS (ID 1)
    uint16_t load;
    uint16_t voltage_battery;
    int16_t current_battery;
    uint16_t drop_rate_comm;
    uint16_t errors_comm;
    // SCALED_PRESSURE (ID 29)
    float pressure_abs;
    int16_t temperature_press;
    // RAW_IMU (ID 27)
    int16_t temperature_imu;
    // SYSTEM_TIME (ID 2)
    uint64_t time_unix_usec;
    // GLOBAL_POSITION_INT (ID 33)
    int32_t altitude_relative;
    int32_t heading;
    // GPS data
    int32_t gps_lat;            // Latitude in degrees * 1e7
    int32_t gps_lon;            // Longitude in degrees * 1e7
    int32_t gps_alt;            // Altitude MSL in mm
    int32_t gps_vx;
    int32_t gps_vy;
    int32_t gps_vz;
    uint8_t gps_satellites;
    uint8_t gps_fix_type;       // GPS fix type: 0-1=no fix, 2=2D fix, 3=3D fix
    // Attitude
    float roll;
    float pitch;
    float yaw;
    // Home position
    int32_t home_lat;           // Home latitude in degrees * 1e7
    int32_t home_lon;           // Home longitude in degrees * 1e7
    int32_t home_alt;           // Home altitude MSL in mm
    bool home_set;              // Flag if home position is received
    // VTOL state
    uint8_t vtol_state;         // MAV_VTOL_STATE enum
    bool is_transitioning;      // Flag if currently transitioning
    // Thread-safe event flags for Core 1 task
    bool pending_home_send;
    bool pending_transition_send;
};

#endif // MAVLINK_DATA_TYPES_H
