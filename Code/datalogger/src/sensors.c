#include <stdio.h>

#include "sensors.h"

#include "hardware/adc.h"

#define LED_PIN 22
#define DHT_PIN 6
#define SOIL_PIN 26
#define BUTTON_PIN 1

// to store temperature and humidity readings
typedef struct {
    float humidity;
    float temp_celsius;
    float soil_moisture;
} measurement_t;

typedef struct {
    float slope;
    float intercept;
} calibration_t;

// how long to wait between measurements
static const uint64_t update_delay_us = 1000000;  // 1min
// how long to wait between measurement retries
static const uint64_t retry_delay_us = 1000000;  // 1sec
// tracks when to take the next measurement
static uint64_t timeout = 0;
// number of failed measurement attempts
static uint32_t attempts = 0;

// number of soil moisture meaurements to average
static const uint8_t soil_count = 100;
// the calibration for the soil sensor
static calibration_t soil_cal;

// stores the last recorded measurement
// TODO: Make only update when a measurement is logging
static measurement_t prev_measure = {
    .humidity = -1.0f,
    .temp_celsius = -1.0f,
    .soil_moisture = -1.0f
};
// stores the most recent reading, even if faulty
static measurement_t measure;

static int32_t read_soil(void);

/**
 * Calibration sequence for the soil moisture sensor. Records an air meaurement,
 * then a wet measurement, and sets the slope-intercept based on those. Uses
 * button input to trigger measurements.
 */
static void calibrate_soil(void);

/**
 * Reads from the DHT11. Single bus IO. Sends a start signal, waits for
 * acknowledgement, then reads 40 bits of sensor data. Ones and zeroes
 * determined by pulse length. Verifies checksum, then writes data to the
 * measurement struct.
 * 
 * @param result Pointer to the measurement struct
 * 
 * @return `true` if successful, `false` otherwise
 */
static bool read_dht(measurement_t* result);

void init_sensors(void) {

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);
    
    gpio_put(LED_PIN, 1);  // TODO: Make dependent on soil moisture level

    // set up button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // set up DHT11
    gpio_init(DHT_PIN);

    // set up soil moisture sensor
    gpio_init(SOIL_PIN);
    adc_init();
    adc_select_input(0);
    
    calibrate_soil();

}

bool should_update_sensors(void) {
    return time_us_64() > timeout;
}

bool update_sensors(void) {
    // read sensors and track whether successful
    if (!read_dht(&measure)) {
        // retry sooner if failed
        timeout = time_us_64() + retry_delay_us;
        attempts++;
        return false;
    }

    // read soil moisture level
    uint16_t raw = read_soil();
    measure.soil_moisture = raw * soil_cal.slope + soil_cal.intercept;

    // update timeout after sensor reading
    timeout = time_us_64() + update_delay_us;
    attempts = 0;
    return true;
}

// TODO: Add a function to determine whether a new measurement is called for

void print_readings(void) {
    // formats most recent measurement
    printf("Temperature: %.0f°C  Humidity: %.0f%%  Soil moisture: %.1f%%\n",
           measure.temp_celsius, measure.humidity, measure.soil_moisture);
}

void calibrate_soil(void) {
    uint16_t endpoints[2] = {0};
    printf("Calibrating soil sensor...\n");
    while (!gpio_get(BUTTON_PIN)) {
        tight_loop_contents();
    }
    printf("Please disconnect soil sensor and press button.\n");
    while (gpio_get(BUTTON_PIN)) {
        tight_loop_contents();
    }
    endpoints[0] = read_soil();
    printf("Dry reading: %d\n", endpoints[0]);
    while (!gpio_get(BUTTON_PIN)) {
        tight_loop_contents();
    }
    printf("Please reconnect soil sensor and place in a cup of water.\n");
    while (gpio_get(BUTTON_PIN)) {
        tight_loop_contents();
    }
    endpoints[1] = read_soil();
    printf("Wet reading: %d\n", endpoints[1]);
    soil_cal.slope = 100.0f / (float)(endpoints[1] - endpoints[0]);
    soil_cal.intercept = -soil_cal.slope * endpoints[0];
    printf("Soild sensor calibrated!\n");
}

static int32_t read_soil(void) {
    uint32_t sum = 0;

    // discard first reading to avoid incorrect measurements
    adc_read();

    for (uint8_t i = 0; i < soil_count; i++) {
        sleep_us(10);
        sum += adc_read();
    }
    return sum / soil_count;
}

static bool read_dht(measurement_t* result) {    
    // buffer for the 5 bytes (40 bits) of data
    uint8_t data[5] = {0};

    // prevent any possible interrupts to protect timing
    
    // start by switching to output and pulling pin low
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    gpio_put(DHT_PIN, 0);

    // MCU sends start signal and waits 20ms (min 18ms per datasheet)
    sleep_ms(20);

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
    for (uint8_t i = 0; i < 40; i++) {
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
