#include <stdio.h>

#include "pico/stdlib.h"

#include "params.h"
#include "wifi_mgr.h"
#include "time_sync.h"
#include "sensors.h"

int main() {
    // wait up to ten seconds for the serial port to open
    stdio_init_all();
    if (uart_is_readable_within_us(uart0, INIT_WAIT_US)) {
        printf("Serial connection success!\n");
    }

    // try to connect to wifi
    if (!wifi_init()) {
        return -1;
    }

    // try to setup rtc
    if (!rtc_setup()) {
        return -1;
    }
    
    // try to initialize sensors
    if (!init_sensors()) {
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
