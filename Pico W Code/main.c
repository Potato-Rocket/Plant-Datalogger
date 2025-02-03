#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

#define LED_PIN 22

struct repeating_timer update_timer;
int count = 0;
datetime_t now = {
    .year  = 2025,
    .month = 02,
    .day   = 02,
    .dotw  = 0, // 0 is Sunday, so 5 is Friday
    .hour  = 23,
    .min   = 45,
    .sec   = 00
};
char datetime_buf[256];
char* datetime_str = &datetime_buf[0];

bool cb_update_time(__unused struct repeating_timer *t);

int64_t cb_led_off(__unused alarm_id_t id, __unused void *userdata);

int main() {
    stdio_init_all();
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    rtc_init();
    rtc_set_datetime(&now);

    add_repeating_timer_ms(-1000, cb_update_time, NULL, &update_timer);

    while (true) {
        tight_loop_contents();
    }

}

bool cb_update_time(__unused struct repeating_timer *t) {
    gpio_put(LED_PIN, 1);
    rtc_get_datetime(&now);
    datetime_to_str(datetime_str, sizeof(datetime_buf), &now);
    printf("%ds since startup... %s\n", count, datetime_str);
    count++;
    add_alarm_in_ms(100, cb_led_off, NULL, true);
    return true;
}

int64_t cb_led_off(__unused alarm_id_t id, __unused void *userdata) {
    gpio_put(LED_PIN, 0);
    return 0;
}
