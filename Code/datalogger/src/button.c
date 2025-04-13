#include "button.h"

#define BUTTON_PIN 1

static const uint32_t button_debounce_us = 20000;  // 20ms
static const uint32_t long_press_min_us = 3000000; // 3sec
static const uint32_t long_press_max_us = 10000000; // 10sec

static uint64_t debounce = 0;
static uint64_t press_start = 0;

static volatile bool short_press = false;
static volatile bool long_press = false;
static bool registered = false;

static void button_cb(uint __unused, uint32_t events);

void init_button(void) {
    // set up button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_set_irq_enabled_with_callback(BUTTON_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &button_cb);

}

bool check_press(void) {
    bool result = short_press;
    short_press = false;
    registered = true;
    return result;
}

bool check_long_press(void) {
    bool result = long_press && !registered;
    long_press = false;
    registered = false;
    return result;
}

static void button_cb(uint __unused, uint32_t events) {
    uint64_t current_time = time_us_64();
    if (current_time < debounce) {
        return;
    }
    debounce = current_time + button_debounce_us;

    // if the button has been pressed
    if (events & GPIO_IRQ_EDGE_FALL) {
        short_press = true;
        long_press = false;
        press_start = current_time;
    }
    // if the button has been released
    if (events & GPIO_IRQ_EDGE_RISE) {
        short_press = false;
        long_press = (
            current_time > press_start + long_press_min_us &&
            current_time < press_start + long_press_max_us
        );
    }
}
