#include <stdio.h>

#include "error_mgr.h"

#define LED_PIN 22
typedef enum {
    LED_OFF,
    LED_ON,
    LED_FLASH,
    LED_FLICKER
} LedState;

static uint8_t error_state = WARNING_INTIALIZING | WARNING_RECALIBRATING;
static bool led_on = false;
static LedState led_state = LED_OFF;
static repeating_timer_t led_timer;

static void update_led_state(void);

static void enter_state_off(void);
static void enter_state_on(void);
static void enter_state_flash(void);
static void enter_state_flicker(void);

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
        error_state |= code;
    } else {
        // Clear the error bit
        error_state &= ~code;
    }

    update_led_state();
}

static void update_led_state(void) {

    uint8_t warning = WARNING_INTIALIZING | WARNING_RECALIBRATING;
    // if in blocking setup processes
    if ((error_state & warning) != ERROR_NONE) {
        enter_state_flicker();
        return;
    }

    uint8_t error = (
        ERROR_WIFI_DISCONNECTED |
        ERROR_NTP_SYNC_FAILED   |
        ERROR_DHT11_READ_FAILED
    );
    // if user attention is needed, flash
    if ((error_state & error) != ERROR_NONE) {
        enter_state_flash();
        return;
    }
    
    // if the sensor is below the set threshold
    if ((error_state & NOTIF_SENSOR_THRESHOLD) != ERROR_NONE) {
        enter_state_on();
        return;
    }

    // if all is clear, turn the LED off
    if (error_state == ERROR_NONE) {
        enter_state_off();
        return;
    }

}

static void enter_state_off(void) {

    switch (led_state) {

        case LED_OFF:
            return;
            
        case LED_FLASH:
            cancel_repeating_timer(&led_timer);
            break;
        
        case LED_FLICKER:
            cancel_repeating_timer(&led_timer);
            break;
    }

    gpio_put(LED_PIN, 0);
    led_on = false;
    led_state = LED_OFF;
}

static void enter_state_on(void) {

    switch (led_state) {

        case LED_ON:
            return;
            
        case LED_FLASH:
            cancel_repeating_timer(&led_timer);
            break;
        
        case LED_FLICKER:
            cancel_repeating_timer(&led_timer);
            break;
    }
    
    gpio_put(LED_PIN, 1);
    led_on = true;
    led_state = LED_ON;
}

static void enter_state_flash(void) {

    switch (led_state) {
            
        case LED_FLASH:
            return;
        
        case LED_FLICKER:
            cancel_repeating_timer(&led_timer);
            break;

    }

    add_repeating_timer_ms(-500, led_timer_cb, NULL, &led_timer);
    led_state = LED_FLASH;
}

static void enter_state_flicker(void) {

    switch (led_state) {
            
        case LED_FLICKER:
            return;
        
        case LED_FLASH:
            cancel_repeating_timer(&led_timer);
            break;

    }

    add_repeating_timer_ms(-50, led_timer_cb, NULL, &led_timer);
    led_state = LED_FLICKER;
}

static bool led_timer_cb(repeating_timer_t* __unused) {
    led_on = !led_on;
    gpio_put(LED_PIN, led_on);
    return true;
}
