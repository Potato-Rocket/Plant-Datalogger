#pragma once

#include "pico/stdlib.h"

bool wifi_init(void);

bool should_check_wifi(void);

void wifi_check_reconnect(void);

bool wifi_connected(void);
