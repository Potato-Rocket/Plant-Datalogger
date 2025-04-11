#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "pico/cyw43_arch.h"

#define LED_PIN 22

#define LOOP_DELAY_US 1000000
#define INIT_WAIT_US 10000000
#define INIT_WAIT_MS 10000

// MAC address is 28:CD:C1:0E:C6:5B
#define ssid "WPI-PSK"
#define pass "photosynthesize"

bool init_modules();

bool init_wifi();

int main() {
    // set up sensors, output devices, etc.
    if (!init_modules()) {
        return -1;
    }

    // try to connect to wifi
    if (!init_wifi()) {
        return -1;
    }
    
    // create and set default date and time
    datetime_t now;

    // buffer and pointer for storing the datetime string
    char datetime_buf[256];
    char* datetime_str = &datetime_buf[0];

    // set up variables for the update loop
    uint64_t prev_time = 0;
    uint32_t count = 0;

    while (true) {
        if (time_us_64() - prev_time > LOOP_DELAY_US) {
            // update and print the current datetime
            rtc_get_datetime(&now);
            datetime_to_str(datetime_str, sizeof(datetime_buf), &now);
            printf("%ds since startup... %s\n", count, datetime_str);
            // update loop variables
            count++;
            prev_time = time_us_64();
        }
    }

}

bool init_modules() {
    // wait up to ten seconds for the serial port to open
    stdio_init_all();
    if (uart_is_readable_within_us(uart0, INIT_WAIT_US)) {
        printf("Serial connection success!\n");
    }

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);
    
    // initialize the RTC
    rtc_init();
    datetime_t t = {
        .year  = 2025,
        .month = 1,
        .day   = 1,
        .dotw  = 3,
        .hour  = 0,
        .min   = 0,
        .sec   = 0
    };
    rtc_set_datetime(&t);
    uint32_t start_time = time_us_32();
    while (!rtc_running()) {
        if (time_us_32() - start_time > INIT_WAIT_US) {
            printf("RTC init failed!\n");
            return false;
        }
    }
    printf("RTC init success!\n");

    return true;
}

bool init_wifi() {
    // initialize the WiFi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed!\n");
        return false;
    }
    printf("Wi-Fi init success!\n");

    // enable station mode
    cyw43_arch_enable_sta_mode();

    // connect to the network
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass,
        CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Wifi connection failed!\n");
        return false;
    }
    printf("Wifi connection success!\n");

    return true;
}
