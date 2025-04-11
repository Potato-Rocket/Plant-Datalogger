#pragma once

#include "pico/stdlib.h"

bool rtc_safe_init(void);

void print_datetime(void);

bool rtc_synchronized(void);
