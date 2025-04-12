#pragma once

#include "pico/stdlib.h"

/**
 * Initializes GPIO pins needed for sensor input. Also sets up the soil
 * indicator light.
 */
void init_sensors(void);

/**
 * Whether enough time has passed since the last sensor measurement.
 */
bool should_update_sensors(void);

/**
 * Updates all sensor readings.
 * 
 * @return `true` if successful, `false` otherwise
 */
bool update_sensors(void);

/**
 * Prints readings to serial. Temporary until more universal measurement
 * struct has been established.
 */
void print_readings(void);
