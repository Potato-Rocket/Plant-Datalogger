#pragma once

#include "pico/stdlib.h"

bool rtc_safe_init(void);

void print_datetime(void);

bool rtc_synchronized(void);

// New NTP functions
bool ntp_init(void);
bool ntp_request_time(void);
void ntp_check_response(void);