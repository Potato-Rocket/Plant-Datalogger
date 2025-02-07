#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "pico/cyw43_arch.h"

#define LED_PIN 22

// MAC address is 28:CD:C1:0E:C6:5B
#define ssid "WPI-PSK"
#define pass "photosynthesize"

typedef struct {
    int count;
    datetime_t now;
    char datetime_buf[256];
    char* datetime_str;
} userdata_t;

int init_modules();

int init_wifi();

bool cb_update_time(struct repeating_timer *t);

int64_t cb_led_off(__unused alarm_id_t id, __unused void *userdata);

int main() {
    userdata_t data = {
        .count = 0,
        .now = {
            .year  = 2025,
            .month = 02,
            .day   = 02,
            .dotw  = 0,
            .hour  = 23,
            .min   = 45,
            .sec   = 00
        },
        .datetime_str = &data.datetime_buf[0]
    };

    init_modules();

    sleep_ms(5000);

    int init = init_wifi();
    if (init != 0) {
        return init;
    }
    
    rtc_set_datetime(&data.now);

    struct repeating_timer update_timer;
    add_repeating_timer_ms(-1000, cb_update_time, &data, &update_timer);

    while (true) {
        tight_loop_contents();
    }
}

int init_modules() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);
    
    rtc_init();
    return 0;
}

int init_wifi() {
    if (0 != cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("Wi-Fi init failed!\n");
        return -1;
    }
    printf("Wi-Fi init success!\n");

    cyw43_arch_enable_sta_mode();
    
    if (0 != cyw43_arch_wifi_connect_timeout_ms(ssid, pass,
        CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Wifi connection failed!\n");
        return -1;
    }
    printf("Wifi connection success!\n");
    
    return 0;
}

bool cb_update_time(struct repeating_timer *t) {
    userdata_t* data = (userdata_t*)t->user_data;
    rtc_get_datetime(&data->now);

    gpio_put(LED_PIN, 1);
    datetime_to_str(data->datetime_str, sizeof(data->datetime_buf), &data->now);
    printf("%ds since startup... %s\n", data->count, data->datetime_str);
    data->count++;
    add_alarm_in_ms(100, cb_led_off, NULL, true);
    return true;
}

int64_t cb_led_off(__unused alarm_id_t id, __unused void *userdata) {
    gpio_put(LED_PIN, 0);
    return 0;
}
