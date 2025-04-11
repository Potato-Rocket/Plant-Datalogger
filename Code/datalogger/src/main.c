#include <stdio.h>

#include "pico/stdlib.h"

#include "params.h"
#include "wifi_mgr.h"
#include "rtc_ntp_sync.h"

#define LED_PIN 22

int main() {
    // wait up to ten seconds for the serial port to open
    stdio_init_all();
    if (uart_is_readable_within_us(uart0, INIT_WAIT_US)) {
        printf("Serial connection success!\n");
    }

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    // try to connect to wifi
    if (!rtc_setup()) {
        return -1;
    }

    // try to connect to wifi
    if (!wifi_init()) {
        return -1;
    }

    // set up variables for the update loop
    uint64_t prev_time = 0;

    while (true) {
        if (time_us_64() - prev_time > LOOP_DELAY_US) {
            rtc_print();
            prev_time = time_us_64();
        }
    }

}
