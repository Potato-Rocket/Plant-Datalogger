#include <stdio.h>

#include "error_mgr.h"

#define LED_PIN 22

/**
 * Enumeratioon to describe the state of the indicator light
 */
typedef enum {
    LED_OFF,      // steady off
    LED_ON,       // steady on
    LED_FLASH,    // toggles at 1Hz
    LED_FLICKER   // toggles at 10Hz
} LedState;

// the current error code mask
static uint8_t error_state = ERROR_NONE;
// whether was last turned on or turned off (for toggling)
static bool led_on = false;
// the current state of the LED
static LedState led_state = LED_OFF;
// the repeating timer to toggle the LED in the background
static repeating_timer_t led_timer;

/**
 * Update the LED state based on the current error state. Prioritizes
 * warnings over errors, over notifs, over none.
 */
static void _update_led_state(void);

/**
 * Safely switches the LED off.
 */
static void _enter_state_off(void);

/**
 * Safely switches the LED on.
 */
static void _enter_state_on(void);

/**
 * Safely makes the LED enter flashing mode.
 */
static void _enter_state_flash(void);

/**
 * Safely makes the LED enter flickering mode.
 */
static void _enter_state_flicker(void);

/**
 * Toggle the LED and update it's current state
 */
static bool _led_timer_cb(repeating_timer_t* __unused);

void init_errors(uint8_t code) {

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    // set the error code and update the LED
    error_state = code;
    _update_led_state();
}

void set_error(uint8_t code, bool enabled) {
    if (enabled) {
        // Set the error bit
        error_state |= code;
    } else {
        // Clear the error bit
        error_state &= ~code;
    }

    // update the led state
    _update_led_state();
}

static void _update_led_state(void) {

    uint8_t warning = WARNING_INTIALIZING | WARNING_RECALIBRATING;
    // if in blocking setup processes
    if ((error_state & warning) != ERROR_NONE) {
        _enter_state_flicker();
        return;
    }

    uint8_t error = (
        ERROR_WIFI_DISCONNECTED |
        ERROR_NTP_SYNC_FAILED   |
        ERROR_DHT11_READ_FAILED
    );
    // if user attention is needed, flash
    if ((error_state & error) != ERROR_NONE) {
        _enter_state_flash();
        return;
    }
    
    // if the sensor is below the set threshold
    if ((error_state & NOTIF_SENSOR_THRESHOLD) != ERROR_NONE) {
        _enter_state_on();
        return;
    }

    // if all is clear, turn the LED off
    if (error_state == ERROR_NONE) {
        _enter_state_off();
        return;
    }

}

static void _enter_state_off(void) {

    switch (led_state) {
        // if already in OFF state, do nothing
        case LED_OFF:
            return;
        
        // turn of the timer if the timer was on
        case LED_FLASH:
            cancel_repeating_timer(&led_timer);
            break;
        
        case LED_FLICKER:
            cancel_repeating_timer(&led_timer);
            break;
    }

    // set the LED to off and update the state
    gpio_put(LED_PIN, 0);
    led_on = false;
    led_state = LED_OFF;
}

static void _enter_state_on(void) {

    switch (led_state) {
        // if already in ON state, do nothing
        case LED_ON:
            return;
        
        // turn off the timer if the timer was on
        case LED_FLASH:
            cancel_repeating_timer(&led_timer);
            break;
        
        case LED_FLICKER:
            cancel_repeating_timer(&led_timer);
            break;
    }
    
    // set the LED to on and update the state
    gpio_put(LED_PIN, 1);
    led_on = true;
    led_state = LED_ON;
}

static void _enter_state_flash(void) {

    switch (led_state) {
        // if already in FLASH state, do nothing
        case LED_FLASH:
            return;
        
        // if timer on, cancel so it can be modified
        case LED_FLICKER:
            cancel_repeating_timer(&led_timer);
            break;

    }

    // start a new timer and update the state
    add_repeating_timer_ms(-500, _led_timer_cb, NULL, &led_timer);
    led_state = LED_FLASH;
}

static void _enter_state_flicker(void) {

    switch (led_state) {
        // if already in FLICKER state, do nothing
        case LED_FLICKER:
            return;
        
        // if timer on, cancel so it can be modified
        case LED_FLASH:
            cancel_repeating_timer(&led_timer);
            break;

    }

    // start a new timer and update the state
    add_repeating_timer_ms(-50, _led_timer_cb, NULL, &led_timer);
    led_state = LED_FLICKER;
}

static bool _led_timer_cb(repeating_timer_t* __unused) {
    led_on = !led_on;
    gpio_put(LED_PIN, led_on);
    return true;
}
