#pragma once

#include "pico/stdlib.h"

typedef struct {
    float humidity;
    float temp_celsius;
} dht_reading_t;

bool init_sensors(void);

void read_from_dht(dht_reading_t *result);
