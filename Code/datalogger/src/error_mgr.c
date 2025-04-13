#include <stdio.h>

#include "error_mgr.h"

#define LED_PIN 22
typedef enum {
    LED_OFF,
    LED_ON,
    LED_FLASH,
    LED_FLICKER
} led_state;

static uint8_t error_status = WARNING_INTIALIZING | WARNING_RECALIBRATING;
static bool led_on = false;
static repeating_timer_t led_timer;

static void update_led_state(void);

static void set_led_state(led_state state);

static bool led_timer_cb(repeating_timer_t* __unused);

void init_errors(void) {

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    update_led_state();
}

void set_error(uint8_t code, bool enabled) {
    if (enabled) {
        // Set the error bit
        error_status |= code;
    } else {
        // Clear the error bit
        error_status &= ~code;
    }
    update_led_state();
}

static void update_led_state(void) {

    uint8_t warning = WARNING_INTIALIZING | WARNING_RECALIBRATING;
    // if in blocking setup processes
    if ((error_status & warning) != ERROR_NONE) {
        set_led_state(LED_FLICKER);
        return;
    }

    uint8_t error = (
        ERROR_WIFI_DISCONNECTED |
        ERROR_NTP_SYNC_FAILED   |
        ERROR_DHT11_READ_FAILED
    );
    // if user attention is needed, flash
    if ((error_status & error) != ERROR_NONE) {
        return;
    }
    
    // if the sensor is below the set threshold
    if ((error_status & NOTIF_SENSOR_THRESHOLD) != ERROR_NONE) {
        set_led_state(LED_ON);
        return;
    }

    // if all is clear, turn the LED off
    if (error_status == ERROR_NONE) {
        set_led_state(LED_OFF);
        return;
    }

}

static void set_led_state(led_state state) {

    cancel_repeating_timer(&led_timer);

    switch (state) {
        case LED_OFF:
            gpio_put(LED_PIN, 0);
            led_on = false;
            break;

        case LED_ON:
            gpio_put(LED_PIN, 1);
            led_on = true;
            break;
            
        case LED_FLASH:
            add_repeating_timer_ms(-500, led_timer_cb, NULL, &led_timer);
            break;
        
        case LED_FLICKER:
            add_repeating_timer_ms(-50, led_timer_cb, NULL, &led_timer);
            break;
    }

}

static bool led_timer_cb(repeating_timer_t* __unused) {
    led_on = !led_on;
    gpio_put(LED_PIN, led_on);
    return true;
}
