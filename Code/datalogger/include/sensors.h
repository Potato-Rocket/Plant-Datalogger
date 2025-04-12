#pragma once

#include "pico/stdlib.h"

void init_sensors(void);

bool should_update_sensors(void);

bool update_sensors(void);

void print_readings(void);
