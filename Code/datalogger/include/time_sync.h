#pragma once

#include "pico/stdlib.h"

bool rtc_safe_init(void);

char* get_pretty_datetime(void);

char* get_timestamp(void);

bool rtc_synchronized(void);

bool ntp_init(void);

bool ntp_request_time(void);
