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
* Calibration sequence for the soil moisture sensor. Records an air meaurement,
* then a wet measurement, and sets the slope-intercept based on those. Maps the
* ADC range to a percentage range. Uses button input to trigger measurements.
*/
void calibrate_soil(void);

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
