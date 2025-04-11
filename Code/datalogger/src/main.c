#include <stdio.h>

#include "pico/stdlib.h"

#include "wifi_mgr.h"
#include "time_sync.h"
#include "sensors.h"

static const int32_t stdio_init_timeout_us = 5000000;  // 5sec

int main() {
    // wait up to ten seconds for the serial port to open
    stdio_init_all();
    if (uart_is_readable_within_us(uart0, stdio_init_timeout_us)) {
        printf("Serial connection success!\n");
    }

    // try to connect to WiFi
    if (!wifi_init()) {
        return -1;
    }

    // try to setup RTC
    if (!rtc_safe_init()) {
        return -1;
    }

    // initialize sensors
    init_sensors();

    // set up variables for the update loop
    uint64_t prev_loop = 0;

    while (true) {
        if (should_check_wifi()) wifi_check_reconnect();

        if (should_update_sensors()) {
            print_datetime();
            if (update_sensors()) {
                print_readings();
            }
        }
    }

}
