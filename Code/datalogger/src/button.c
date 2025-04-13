#include <stdio.h>

#include "button.h"

#define BUTTON_PIN 1

// debouncing time constand
static const uint32_t button_debounce_us = 20000;   // 20ms
// minimum duration of a long press
static const uint32_t long_press_min_us = 3000000;  // 3sec
// maximum duration of a long press, to avoid strange behavior
static const uint32_t long_press_max_us = 10000000; // 10sec

// when the next event can be registered
static uint64_t debounce = 0;
// when the current long press started
static uint64_t press_start = 0;

/**
 * Enumeration to keep track of what event the button should report
 */
typedef enum {
    BUTTON_IDLE,         // no events to report
    BUTTON_PRESSED,      // button pressed but not released or reported
    BUTTON_LONG_PRESSED, // button long pressed but not reported
} ButtonState;

// the current button state, i.e. any events to report
static volatile ButtonState button_state = BUTTON_IDLE;

/**
 * Button callback, whenever the button's state changes. Debounces, then if it
 * is a rising edge, registers a short press. If it is a falling edge, registers
 * a long press if the duration is correct, deregisters the short press.
 */
static void _button_cb(uint __unused, uint32_t events);

void init_button(void) {
    // set up button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // set up button callback
    gpio_set_irq_enabled_with_callback(BUTTON_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &_button_cb);

}

bool check_press(void) {
    // is there a short press to report?
    bool result = button_state == BUTTON_PRESSED;
    // reset the button state
    if (result) {
        button_state = BUTTON_IDLE;
    }
    return result;
}

bool check_long_press(void) {
    // is there a long press to report?
    bool result = button_state == BUTTON_LONG_PRESSED;
    // reset the button state
    if (result) {
        button_state = BUTTON_IDLE;
    }
    return result;
}

static void _button_cb(uint __unused, uint32_t events) {
    // do some debouncing
    uint64_t current_time = time_us_64();
    if (current_time < debounce) {
        return;
    }
    debounce = current_time + button_debounce_us;

    // if the button has been pressed
    if (events & GPIO_IRQ_EDGE_FALL) {
        // update the date and press time
        button_state = BUTTON_PRESSED;
        press_start = current_time;
    }

    // if the button has been released
    if (events & GPIO_IRQ_EDGE_RISE) {

        // if the short press was not reported
        if (button_state == BUTTON_PRESSED) {
            uint64_t time_delta = current_time - press_start;

            // if the press was the correct duration
            if (time_delta > long_press_min_us &&
                time_delta < long_press_max_us) {
                // register a long press
                button_state = BUTTON_LONG_PRESSED;

            } else {
                // otherwise return to idle
                button_state = BUTTON_IDLE;
            }

        }

    }

}
