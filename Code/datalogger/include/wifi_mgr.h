#pragma once

#include "pico/stdlib.h"

/**
 * Intializes cyw43 and connects to thd network.a64l
 * 
 * @return `true` upon success, `false` otherwise
 */
bool wifi_init(void);

/**
 * Whether it's been long enough since the wifi was last checked, or since
 * the last reconnection attempt.
 */
bool should_check_wifi(void);

/**
 * Checks the wifi connection and updates the flag. If disconnected, try to
 * reconnect.
 */
void wifi_check_reconnect(void);

/**
 * Returns whether the wifi connection was running when last checked.
 */
bool wifi_connected(void);
