#include <stdio.h>

#include "sensors.h"

#define LED_PIN 22
#define DHT_PIN 4

// to store temperature and humidity readings
typedef struct {
    float humidity;
    float temp_celsius;
} dht_reading_t;

static const uint64_t update_delay_us = 1000000;  // 1min
static uint64_t timeout = 0;

static bool read_dht(dht_reading_t *result);

// stores the last recorded measurement
// TODO: Make only update when a measurement is logging
static dht_reading_t prev_dht = {
    .humidity = -1.0f,
    .temp_celsius = -1.0f
};
// stores the most recent reading, even if faulty
static dht_reading_t current_dht;

void init_sensors(void) {

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);
    
    gpio_put(LED_PIN, 1);  // TODO: Make dependent on soil moisture level

    // set up DHT11
    gpio_init(DHT_PIN);

    timeout = time_us_64() + update_delay_us;
    
}

bool should_update_sensors(void) {
    return time_us_64() > timeout;
}

bool update_sensors(void) {
    // update previous reading
    prev_dht.humidity = current_dht.humidity;
    prev_dht.temp_celsius = current_dht.temp_celsius;

    // read sensors and track whether successful
    bool success = read_dht(&current_dht);

    // update timeout after sensor reading
    timeout = time_us_64() + update_delay_us;
    return success;
}

// TODO: Add a function to determine whether a new measurement is called for

void print_readings(void) {
    // formats most recent measurement
    printf("Temperature: %.0f°C  Humidity: %.0f%%\n",
        current_dht.temp_celsius, current_dht.humidity);
}

// TODO: Set up, calibrate soil moisture sensor

static bool read_dht(dht_reading_t *result) {
    // initialize result to invalid values
    result->humidity = -1.0f;
    result->temp_celsius = -1.0f;
    
    // buffer for the 5 bytes (40 bits) of data
    uint8_t data[5] = {0};

    // prevent any possible interrupts to protect timing
    
    // start by switching to output and pulling pin low
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    gpio_put(DHT_PIN, 0);

    // MCU sends start signal and waits 20ms (min 18ms per datasheet)
    uint64_t timeout = time_us_64() + 20000;
    while (time_us_64() < timeout) {
        tight_loop_contents();
    }

    // switch to input, external pull-up will bring voltage up
    gpio_set_dir(DHT_PIN, GPIO_IN);
    
    // MCU waits for DHT response (20-40us)
    timeout = time_us_64() + 50;
    while (gpio_get(DHT_PIN) == 1) {
        if (time_us_64() > timeout) {
            printf("DHT failed to respond to start signal\n");
            return false;
        }
    }
    
    // DHT pulled low (80us), now waiting for it to pull high
    timeout = time_us_64() + 100;
    while (gpio_get(DHT_PIN) == 0) {
        if (time_us_64() > timeout) {
            printf("DHT failed to complete low response signal\n");
            return false;
        }
    }
    
    // DHT pulled high (80us), now waiting for it to pull low again
    timeout = time_us_64() + 100;
    while (gpio_get(DHT_PIN) == 1) {
        if (time_us_64() > timeout) {
            printf("DHT failed to complete high response signal\n");
            return false;
        }
    }
    
    // start reading the 40 bits (5 bytes) of data
    for (int i = 0; i < 40; i++) {
        // each bit starts with a 50us low signal
        timeout = time_us_64() + 70;
        while (gpio_get(DHT_PIN) == 0) {
            if (time_us_64() > timeout) {
                printf("Timeout waiting for bit %d start (low)\n", i);
                return false;
            }
        }
        
        // length of high signal determines bit value (26-28us for '0', 70us for '1')
        uint64_t high_start = time_us_64();
        timeout = high_start + 100;
        
        while (gpio_get(DHT_PIN) == 1) {
            if (time_us_64() > timeout) {
                printf("Timeout waiting for bit %d end (high)\n", i);
                return false;
            }
        }
        
        uint64_t high_duration = time_us_64() - high_start;
        
        // determine bit value based on high signal duration
        if (high_duration > 40) {  // Threshold between 0 and 1
            data[i / 8] |= (1 << (7 - (i % 8)));  // Set bit
        }
    }
    
    // verify checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        printf("Checksum failed: calculated 0x%02x, received 0x%02x\n", checksum, data[4]);
        return false;
    }
    
    // for DHT11, the decimal parts are usually 0, but we'll include them anyway
    result->humidity = (float)data[0];
    result->temp_celsius = (float)(data[2] & 0x7F);
    
    // check for negative temperature (MSB of data[2])
    if (data[2] & 0x80) {
        result->temp_celsius = -result->temp_celsius;
    }

    return true;
}
