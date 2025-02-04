#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#define LED_PIN 22

typedef struct {
    int count;
    datetime_t now;
    char datetime_buf[256];
    char* datetime_str;
} userdata_t;

void init_all();

bool cb_update_time(struct repeating_timer *t);

int64_t cb_led_off(__unused alarm_id_t id, __unused void *userdata);

int main() {
    init_all();

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
    
    rtc_set_datetime(&data.now);

    struct repeating_timer update_timer;
    add_repeating_timer_ms(-1000, cb_update_time, &data, &update_timer);

    while (true) {
        tight_loop_contents();
    }

}

void init_all() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);
    
    rtc_init();
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
