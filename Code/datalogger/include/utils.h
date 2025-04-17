#pragma once

#include "pico/stdlib.h"

/**
 * Returns whether a provided timestamp has passed.
 */
static inline bool is_timed_out(absolute_time_t timeout) {
    return absolute_time_diff_us(timeout, get_absolute_time()) > 0;
}
