/* Simple MAVLink definitions for battery status parsing */

#ifndef MAVLINK_SIMPLE_H
#define MAVLINK_SIMPLE_H

#include <stdint.h>
#include <string.h>

#define MAVLINK_COMM_0 0
#define MAVLINK_MSG_ID_BATTERY_STATUS 147
#define MAVLINK_MAX_PAYLOAD_LEN 50
#define MAVLINK_MAX_PACKET_LEN 280

typedef struct __mavlink_message_t {
    uint16_t checksum;      ///< checksum at end of packet (excluding)
    uint8_t len;            ///< Length of payload
    uint8_t seq;            ///< Sequence of packet
    uint8_t sysid;          ///< ID of message sender system/aircraft
    uint8_t compid;         ///< ID of the message sender component
    uint8_t msgid;          ///< ID of message in payload
    uint64_t payload64[MAVLINK_MAX_PAYLOAD_LEN/sizeof(uint64_t)];
} mavlink_message_t;

typedef struct __mavlink_status_t {
    uint8_t msg_received;               ///< Number of received messages
    uint8_t buffer_overrun;            ///< Number of buffer overruns
    uint8_t parse_error;                ///< Number of parse errors
    uint8_t packet_idx;                ///< Index in current packet
    uint8_t current_rx_seq;             ///< Expected sequence number
    uint8_t current_tx_seq;             ///< Sequence number of next packet to send
    uint8_t flags;                      ///< MAVLink status flags
    uint8_t parse_state;                ///< Parsing state machine state
    uint8_t chan;                       ///< MAVLink channel
    uint8_t sig[MAVLINK_MAX_PACKET_LEN]; ///< Signature state array
} mavlink_status_t;

typedef struct __mavlink_battery_status_t {
    uint16_t voltages[10];    ///< battery voltage, in millivolts (1 = 1 millivolt)
    int16_t current_battery;  ///< Battery current, in 10*milliamperes (1 = 10 milliampere), -1: autopilot does not measure the current
    int8_t battery_remaining; ///< Remaining battery energy: (0%: 0, 100%: 100), -1: autopilot does not estimate the remaining battery
    int32_t time_remaining;    ///< remaining battery time, in seconds (0: autopilot does not provide remaining battery time estimate)
    uint8_t type;             ///< Battery type
    uint8_t id;               ///< Battery ID
    uint8_t battery_function; ///< Function of the battery
    uint8_t temperature;     ///< Temperature of the battery in centi-degrees Celsius. INT16_MAX for unknown temperature.
} mavlink_battery_status_t;

// Simplified MAVLink parsing function
static inline uint8_t mavlink_parse_char(uint8_t chan, uint8_t c, mavlink_message_t* msg, mavlink_status_t* status)
{
    // This is a very simplified version - in real implementation you would need full MAVLink parser
    // For now, we'll just return 0 (no message parsed)
    return 0;
}

static inline void mavlink_msg_battery_status_decode(const mavlink_message_t* msg, mavlink_battery_status_t* battery_status)
{
    // Simplified decode - in real implementation you would extract from payload
    memset(battery_status, 0, sizeof(mavlink_battery_status_t));
}

#endif // MAVLINK_SIMPLE_H
