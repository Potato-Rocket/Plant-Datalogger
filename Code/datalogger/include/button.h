#pragma once

#include "pico/stdlib.h"

/**
 * Initialize the button and it's callbacks.
 */
void init_button(void);

/**
 * Gets whether the button has been pressed, i.e. a rising edge.
 */
bool check_press(void);

/**
 * Gets whether the button has been long-presse, i.e. falling edge 3s < t < 10
 * after the last rising edge.
 */
bool check_long_press(void);
