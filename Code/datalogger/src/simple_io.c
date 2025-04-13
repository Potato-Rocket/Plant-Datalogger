#include "simple_io.h"

#define LED_PIN 22
#define BUTTON_PIN 1

static bool prev_pressed = false;
static const uint32_t button_debounce_us = 20000;  // 20ms
static uint64_t debounce = 0;

static const uint32_t long_press_min_us = 3000000; // 3sec
static const uint32_t long_press_max_us = 10000000; // 10sec
static uint64_t press_start = 0;
static bool long_pressing = false;

static void rising_cb(uint __unused, uint32_t __unused);

static void falling_cb(uint __unused, uint32_t __unused);

void init_io(void) {

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    // set up button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

}

bool check_press(void) {
    if (time_us_64() < debounce) {
        return false;
    }

    bool pressed = !gpio_get(BUTTON_PIN);

    if (pressed != prev_pressed) {
        debounce = time_us_64() + button_debounce_us;
    }

    bool rising_edge = pressed && !prev_pressed;

    prev_pressed = pressed;
    return rising_edge;
}

bool check_long_press(void) {
    uint64_t current_time = time_us_64();
    if (current_time < debounce) {
        return false;
    }
    
    bool pressed = !gpio_get(BUTTON_PIN);\

    bool rising_edge = pressed && !prev_pressed;
    bool falling_edge = !pressed && prev_pressed;

    if (pressed != prev_pressed) {
        debounce = current_time + button_debounce_us;
    }

    if (rising_edge) {
        press_start = current_time;
    }

    bool long_press = (falling_edge &&
        current_time > press_start + long_press_min_us &&
        current_time < press_start + long_press_max_us);
    
    prev_pressed = pressed;
    return long_press;
}

static void rising_cb(uint __unused, uint32_t __unused) {

}

static void rising_cb(uint __unused, uint32_t __unused) {
    
}

