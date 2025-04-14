#include <stdio.h>
#include <math.h>

#include "sensors.h"
#include "button.h"
#include "error_mgr.h"

#include "hardware/adc.h"

#define DHT_PIN 6u
#define SOIL_PIN 26u

// to store temperature and humidity readings
typedef struct {
    float humidity;
    float temp_celsius;
    float soil_moisture;
} measurement_t;

// to store soil sensor calibration in slope-intercept form
typedef struct {
    float slope;
    float intercept;
} calibration_t;

// how long to wait between measurements
static const uint32_t update_delay_us = 6000000ul;  // 1min
// how long to wait between measurement retries
static const uint32_t retry_delay_us = 1000000ul;  // 1sec
// tracks when to take the next measurement
static uint64_t timeout = 0;
// number of failed measurement attempts
static uint32_t attempts = 0;

// number of soil moisture meaurements to average
static const uint8_t soil_count = 100u;
// minumum difference between endpoints
static const float min_cal_diff = 100.0f;
// the calibration for the soil sensor
static calibration_t soil_cal = {
    .slope = -1.0f / (1u << 12u),
    .intercept = (1u << 12u) - 1u,
};
// the threshold for soil to count as dry
static const float soil_threshold = 10.0f;

// stores the last recorded measurement
static measurement_t prev_measure = {
    .humidity = -1.0f,
    .temp_celsius = -1.0f,
    .soil_moisture = -1.0f,
};
// stores the most recent reading, even if faulty
static measurement_t measure;

/**
 * Records 100 ADC readings and returns the average.
 * 
 * @return The average ADC valus
 */
static float _read_soil(void);

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
static bool _read_dht(measurement_t* result);

void init_sensors(void) {

    // set up DHT11
    gpio_init(DHT_PIN);

    // set up soil moisture sensor
    gpio_init(SOIL_PIN);
    adc_init();
    adc_select_input(0);
    
    calibrate_soil();

}

void calibrate_soil(void) {
    set_error(WARNING_RECALIBRATING, true);
    float endpoints[2] = {0};

    printf("Calibrating soil sensor...\n");
    bool valid = false;
    while (!valid) {
        printf("Please wave soil sensor in air and press button.\n");
        while (!check_press()) tight_loop_contents();

        endpoints[0] = _read_soil();
        printf("Dry reading: %.2f\n", endpoints[0]);

        printf("Please reconnect soil sensor and place in a cup of water.\n");
        while (!check_press()) tight_loop_contents();

        endpoints[1] = _read_soil();
        printf("Wet reading: %.2f\n", endpoints[1]);
        
        if (fabsf(endpoints[1] - endpoints[0]) < min_cal_diff) {
            printf("Error: Measurements too similar!\n");
        } else {
            valid = true;
        }
    }

    soil_cal.slope = 100.0f / (endpoints[1] - endpoints[0]);
    soil_cal.intercept = -soil_cal.slope * endpoints[0];
    printf("Soil sensor calibrated! Slope: %.5f, Intercept: %.1f\n", 
           soil_cal.slope, soil_cal.intercept);

    set_error(WARNING_RECALIBRATING, false);
    
    timeout = time_us_64() + update_delay_us;
}

void print_readings(void) {
    // formats most recent measurement
    printf("Temperature: %.0f°C, Humidity: %.0f%%, Soil moisture: %.1f%%\n",
           measure.temp_celsius, measure.humidity, measure.soil_moisture);
}

bool should_update_sensors(void) {
    return time_us_64() > timeout;
}

bool update_sensors(void) {
    // read sensors and track whether successful
    if (!_read_dht(&measure)) {
        // retry sooner if failed
        attempts++;
        if (attempts == 10u) {
            set_error(ERROR_DHT11_READ_FAILED, true);
            timeout = time_us_64() + update_delay_us;
            attempts = 0;
            return false;
        }
        timeout = time_us_64() + retry_delay_us;
        return false;
    }
    set_error(ERROR_DHT11_READ_FAILED, false);

    // read soil moisture level
    float value = _read_soil() * soil_cal.slope + soil_cal.intercept;
    if (value > 100.0f) {
        value = 100.0f;
    } else if (value < 0.0f) {
        value = 0.0f;
    }
    set_error(NOTIF_SENSOR_THRESHOLD, value < soil_threshold);
    measure.soil_moisture = value;

    // update timeout after sensor reading
    timeout = time_us_64() + update_delay_us;
    attempts = 0;
    return true;
}

static float _read_soil(void) {
    uint32_t sum = 0;

    // discard first reading to avoid incorrect measurements
    adc_read();

    for (uint8_t i = 0; i < soil_count; i++) {
        sleep_us(10);
        sum += adc_read();
    }
    return (float)sum / soil_count;
}

static bool _read_dht(measurement_t* result) {
    // validate parameters
    if (result == NULL) {
        printf("Error: NULL pointer passed to read_dht\n");
        return false;
    } 
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
    timeout = time_us_64() + 50u;
    while (gpio_get(DHT_PIN) == 1) {
        if (time_us_64() > timeout) {
            printf("DHT failed to respond to start signal\n");
            return false;
        }
    }
    
    // DHT pulled low (80us), now waiting for it to pull high
    timeout = time_us_64() + 100u;
    while (gpio_get(DHT_PIN) == 0) {
        if (time_us_64() > timeout) {
            printf("DHT failed to complete low response signal\n");
            return false;
        }
    }
    
    // DHT pulled high (80us), now waiting for it to pull low again
    timeout = time_us_64() + 100u;
    while (gpio_get(DHT_PIN) == 1) {
        if (time_us_64() > timeout) {
            printf("DHT failed to complete high response signal\n");
            return false;
        }
    }
    
    // start reading the 40 bits (5 bytes) of data
    for (uint8_t i = 0; i < 40u; i++) {
        // each bit starts with a 50us low signal
        timeout = time_us_64() + 70u;
        while (gpio_get(DHT_PIN) == 0) {
            if (time_us_64() > timeout) {
                printf("Timeout waiting for bit %d start (low)\n", i);
                return false;
            }
        }
        
        // length of high signal determines bit value (26-28us for '0', 70us for '1')
        uint64_t high_start = time_us_64();
        timeout = high_start + 100u;
        
        while (gpio_get(DHT_PIN) == 1) {
            if (time_us_64() > timeout) {
                printf("Timeout waiting for bit %d end (high)\n", i);
                return false;
            }
        }
        
        uint64_t high_duration = time_us_64() - high_start;
        
        // determine bit value based on high signal duration
        if (high_duration > 40u) {
            data[i / 8u] |= (1u << (7u - (i % 8u)));
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
