#include <stdio.h>

#include "time_sync.h"

#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"

static const uint64_t init_timeout_us = 5000000;  // 5sec

// create and set default date and time
static const datetime_t t_default = {
    .year  = 2025,
    .month = 1,
    .day   = 1,
    .dotw  = 3,
    .hour  = 0,
    .min   = 0,
    .sec   = 0
};
static datetime_t t_current;

static bool is_synchronized = false;

// buffer and pointer for storing the datetime string
static char datetime_buf[256];
static char* datetime_str = &datetime_buf[0];

bool rtc_safe_init(void) {
    // initialize the RTC
    rtc_init();
    rtc_set_datetime(&t_default);
    uint64_t timeout = time_us_64() + init_timeout_us;
    while (!rtc_running()) {
        if (time_us_64() > timeout) {
            printf("RTC init timed out!\n");
            return false;
        }
    }
    printf("RTC init success!\n");

    return true;
}

void print_datetime(void) {
    rtc_get_datetime(&t_current);
    datetime_to_str(datetime_str, sizeof(datetime_buf), &t_current);
    printf("Current time: %s\n", datetime_str);
}

bool rtc_synchronized(void) {
    return is_synchronized;
}
