#pragma once

#include "pico/stdlib.h"

enum error_code {
    ERROR_NONE               = 0b00000000,
    ERROR_WIFI_DISCONNECTED  = 0b00000001,
    ERROR_NTP_SYNC_FAILED    = 0b00000010,
    ERROR_DHT11_READ_FAILED  = 0b00000100,
    WARNING_RECALIBRATING    = 0b00001000,
    WARNING_INTIALIZING      = 0b00010000,
    NOTIF_SENSOR_THRESHOLD   = 0b00100000
};

void init_errors(void);

void set_error(uint8_t code, bool enabled);
