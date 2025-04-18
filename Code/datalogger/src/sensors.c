#include <math.h>
#include <stdio.h>

#include "sensors.h"
#include "utils.h"
#include "button.h"
#include "error_mgr.h"
#include "logging.h"

#include "hardware/adc.h"
#include "hardware/dma.h"

#include "dht.h"

#define DHT_MODEL DHT11
#define DHT_PIN 6u
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
static const uint16_t soil_count = 1000u;
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

    log_message(LOG_INFO, LOG_SENSOR, "Calibrating soil sensor...");
    bool valid = false;
    while (!valid)
    {
        log_message(LOG_INFO, LOG_SENSOR, "Please wave soil sensor in air and press button");
        while (!check_press())
        {
            tight_loop_contents();
        }

        endpoints[0] = _read_soil();
        log_message(LOG_INFO, LOG_SENSOR, "Dry reading: %.2f", endpoints[0]);

        log_message(LOG_INFO, LOG_SENSOR, "Please place soil sensor in a cup of water");
        while (!check_press())
        {
            tight_loop_contents();
        }

        endpoints[1] = _read_soil();
        log_message(LOG_INFO, LOG_SENSOR, "Wet reading: %.2f", endpoints[1]);

        if (fabsf(endpoints[1] - endpoints[0]) < min_cal_diff)
        {
            log_message(LOG_WARN, LOG_SENSOR, "Measurements too similar, please try again");
        }
        else
        {
            valid = true;
        }
    }

    soil_cal.slope = 100.0f / (endpoints[1] - endpoints[0]);
    soil_cal.intercept = -soil_cal.slope * endpoints[0];
    log_message(LOG_INFO, LOG_SENSOR, "Soil sensor calibrated. Slope: %.5f, Intercept: %.1f",
           soil_cal.slope, soil_cal.intercept);

    set_error(WARNING_RECALIBRATING, false);

    timeout = make_timeout_time_ms(update_delay_ms);
}

void print_readings(void)
{
    // formats most recent measurement
    log_message(LOG_INFO, LOG_SENSOR, "Temperature: %.0f°C, Humidity: %.0f%%, "
        "Soil moisture: %.1f%%",
        measure.temp_celsius, measure.humidity, measure.soil_moisture);
}

bool should_update_sensors(void)
{
    return is_timed_out(timeout);
}

bool update_sensors(void)
{
    // try to read dht11
    if (!_read_dht(&measure))
    {
        return false;
    }

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

    for (uint16_t i = 0; i < soil_count; i++)
    {
        sleep_us(10);
        sum += adc_read();
    }
    return (float)sum / soil_count;
}

static bool _read_dht(measurement_t *measure)
{
    // start the dht measurement
    dht_start_measurement(&dht);
    // store the result of the dht measurement
    dht_result_t result = dht_finish_measurement_blocking(&dht, &measure->humidity, &measure->temp_celsius);
    // report success if the measurement succeeded
    if (result == DHT_RESULT_OK) {
        log_message(LOG_INFO, LOG_SENSOR, "DHT read successful");
        set_error(ERROR_DHT11_READ_FAILED, false);
        return true;
    }
    
    // add a string to a buffer depending on the failure mode
    char msg[256];
    switch (result)
    {
        case DHT_RESULT_BAD_CHECKSUM:
            snprintf(&msg[0], sizeof(msg), "DHT read failed due to bad checksum");
            break;
        case DHT_RESULT_TIMEOUT:
            snprintf(&msg[0], sizeof(msg), "DHT read timed out");
            break;
    }
    // if tenth failure, report an error and try again later
    attempts++;
    if (attempts == 10u)
    {
        set_error(ERROR_DHT11_READ_FAILED, true);
        log_message(LOG_ERROR, LOG_SENSOR, "%s! (%d)", msg, attempts);
        timeout = make_timeout_time_ms(update_delay_ms);
        attempts = 0;
        return false;
    }
    // otherwise, report a warning
    log_message(LOG_WARN, LOG_SENSOR, "%s (%d)", msg, attempts);
    timeout = make_timeout_time_ms(retry_delay_ms);
    return false;

}
