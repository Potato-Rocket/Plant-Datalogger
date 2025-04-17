#pragma once

#include "pico/stdlib.h"

/**
 * Returns whether a provided timestamp has passed.
 */
static inline bool is_timed_out(absolute_time_t timeout)
{
    return absolute_time_diff_us(timeout, get_absolute_time()) > 0;
}

/**
 * Returns the time in ms from one timestamp to another. Positive if `to` is after `from`.
 */
static inline int32_t absolute_time_diff_ms(absolute_time_t from, absolute_time_t to)
{
    return (int32_t)(to_ms_since_boot(to) - to_ms_since_boot(from));
}
