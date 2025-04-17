#pragma once

#include "pico/stdlib.h"

/**
 * Error code masks for various error states.
 */
enum ErrorCode
{
    ERROR_NONE = 0b00000000,              // all systems nominal
    ERROR_WIFI_DISCONNECTED = 0b00000001, // wifi reconnection standoff maxed
    ERROR_NTP_SYNC_FAILED = 0b00000010,   // ntp retry standoff maxed
    ERROR_DHT11_READ_FAILED = 0b00000100, // dht failed ten times
    WARNING_RECALIBRATING = 0b00001000,   // in calibration mode
    WARNING_INTIALIZING = 0b00010000,     // doing initial system setup
    NOTIF_SENSOR_THRESHOLD = 0b00100000,  // soil too dry
};

/**
 * Initialize indicator LED and error state.
 *
 * @param code The starting error code mask
 */
void init_errors(uint8_t code);

/**
 * Set an error code and update the LED state if needed.
 *
 * @param code The error code to set
 * @param enabled Whether the code should be on or off
 */
void set_error(uint8_t code, bool enabled);
