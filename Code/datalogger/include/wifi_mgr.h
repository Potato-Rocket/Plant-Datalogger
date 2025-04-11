#pragma once

#include "pico/stdlib.h"

bool wifi_init(void);

bool should_check_wifi(void);

bool wifi_check_reconnect(void);
