#pragma once

#include "pico/stdlib.h"

typedef struct {
    float humidity;
    float temp_celsius;
} dht_reading_t;

void init_sensors(void);

bool should_update_sensors(void);

bool update_sensors(void);

void print_readings(void);
