#include <stdio.h>
#include <math.h>

#include "sensors.h"
#include "utils.h"
#include "button.h"
#include "error_mgr.h"

#include "hardware/adc.h"

#include "dht.h"

#define DHT_PIN 6u
#define DHT_MODEL DHT11
#define SOIL_PIN 26u

// to store temperature and humidity readings
typedef struct
{
    float humidity;
    float temp_celsius;
    float soil_moisture;
} measurement_t;

// to store soil sensor calibration in slope-intercept form
typedef struct
{
    float slope;
    float intercept;
} calibration_t;

// how long to wait between measurements
static const uint32_t update_delay_ms = 6000ul; // 1min
// how long to wait between measurement retries
static const uint32_t retry_delay_ms = 1000ul; // 1sec
// tracks when to take the next measurement
static absolute_time_t timeout = 0;
// number of failed measurement attempts
static uint8_t attempts = 0;
// dht sensor object
static dht_t dht;

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
 * @param measure Pointer to the measurement struct
 *
 * @return `true` if successful, `false` otherwise
 */
static bool _read_dht(measurement_t *measure);

void init_sensors(void)
{
    // set up DHT11
    dht_init(&dht, DHT_MODEL, pio0, DHT_PIN, false);

    // set up soil moisture sensor
    gpio_init(SOIL_PIN);
    adc_init();
    adc_select_input(0);

    calibrate_soil();
}

void calibrate_soil(void)
{
    set_error(WARNING_RECALIBRATING, true);
    float endpoints[2] = {0};

    printf("Calibrating soil sensor...\n");
    bool valid = false;
    while (!valid)
    {
        printf("Please wave soil sensor in air and press button.\n");
        while (!check_press())
            tight_loop_contents();

        endpoints[0] = _read_soil();
        printf("Dry reading: %.2f\n", endpoints[0]);

        printf("Please reconnect soil sensor and place in a cup of water.\n");
        while (!check_press())
            tight_loop_contents();

        endpoints[1] = _read_soil();
        printf("Wet reading: %.2f\n", endpoints[1]);

        if (fabsf(endpoints[1] - endpoints[0]) < min_cal_diff)
        {
            printf("Error: Measurements too similar!\n");
        }
        else
        {
            valid = true;
        }
    }

    soil_cal.slope = 100.0f / (endpoints[1] - endpoints[0]);
    soil_cal.intercept = -soil_cal.slope * endpoints[0];
    printf("Soil sensor calibrated! Slope: %.5f, Intercept: %.1f\n",
           soil_cal.slope, soil_cal.intercept);

    set_error(WARNING_RECALIBRATING, false);

    timeout = make_timeout_time_ms(update_delay_ms);
}

void print_readings(void)
{
    // formats most recent measurement
    printf("Temperature: %.0f°C, Humidity: %.0f%%, Soil moisture: %.1f%%\n",
           measure.temp_celsius, measure.humidity, measure.soil_moisture);
}

bool should_update_sensors(void)
{
    return is_timed_out(timeout);
}

bool update_sensors(void)
{
    // read sensors and track whether successful
    if (!_read_dht(&measure))
    {
        // retry sooner if failed
        attempts++;
        if (attempts == 10u)
        {
            set_error(ERROR_DHT11_READ_FAILED, true);
            timeout = make_timeout_time_ms(update_delay_ms);
            attempts = 0;
            return false;
        }
        timeout = make_timeout_time_ms(retry_delay_ms);
        return false;
    }
    set_error(ERROR_DHT11_READ_FAILED, false);

    // read soil moisture level
    float value = _read_soil() * soil_cal.slope + soil_cal.intercept;
    if (value > 100.0f)
    {
        value = 100.0f;
    }
    else if (value < 0.0f)
    {
        value = 0.0f;
    }
    set_error(NOTIF_SENSOR_THRESHOLD, value < soil_threshold);
    measure.soil_moisture = value;

    // update timeout after sensor reading
    timeout = make_timeout_time_ms(update_delay_ms);
    attempts = 0;
    return true;
}

static float _read_soil(void)
{
    uint32_t sum = 0;

    // discard first reading to avoid incorrect measurements
    adc_read();

    for (uint8_t i = 0; i < soil_count; i++)
    {
        sleep_us(10);
        sum += adc_read();
    }
    return (float)sum / soil_count;
}

static bool _read_dht(measurement_t *measure)
{
    dht_start_measurement(&dht);
    dht_result_t result = dht_finish_measurement_blocking(&dht, &measure->humidity, &measure->temp_celsius);
    switch (result) {
        case DHT_RESULT_OK:
            return true;
        case DHT_RESULT_BAD_CHECKSUM:
            printf("DHT read failed: Bad checksum");
            return false;
        case DHT_RESULT_TIMEOUT:
            printf("DHT read failed: DHT timed out");
            return false;
    }
}
