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

    // try to connect to WiFi
    if (!wifi_init()) {
        return -1;
    }

    // try to setup RTC
    if (!rtc_setup()) {
        return -1;
    }

    // try to initialize sensors
    if (!init_sensors()) {
        return -1;
    }

    // set up variables for the update loop
    uint64_t prev_loop = 0;
    dht_reading_t temp_humidity;

    while (true) {
        if (should_check_wifi()) {
            wifi_check_reconnect();
        }

        if (time_us_64() - prev_loop > LOOP_DELAY_US) {
            rtc_print();
            prev_loop = time_us_64();

            printf("Temperature: %.1fC  Humidity: %.1f%%\n",
            temp_humidity.temp_celsius, temp_humidity.humidity);
        }
    }

}
