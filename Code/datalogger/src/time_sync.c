#include <stdio.h>

#include "time_sync.h"
#include "params.h"

#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"

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

// buffer and pointer for storing the datetime string
static char datetime_buf[256];
static char* datetime_str = &datetime_buf[0];



bool rtc_setup(void) {
    // initialize the RTC
    rtc_init();
    rtc_set_datetime(&t_default);
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

void rtc_print(void) {
    rtc_get_datetime(&t_current);
    datetime_to_str(datetime_str, sizeof(datetime_buf), &t_current);
    printf("Current time: %s\n", datetime_str);
}

static void set_system_time(uint32_t sec) {
    time_t epoch = sec;
    struct tm *time = gmtime(&epoch);
    
    // Convert to Pico datetime structure
    datetime_t datetime = {
        .year  = (int16_t)(1900 + time->tm_year),
        .month = (int8_t)(time->tm_mon + 1),
        .day   = (int8_t)time->tm_mday,
        .dotw  = (int8_t)time->tm_wday,
        .hour  = (int8_t)time->tm_hour,
        .min   = (int8_t)time->tm_min,
        .sec   = (int8_t)time->tm_sec
    };
    
    // Set the RTC
    rtc_set_datetime(&datetime);
    
    // Print the updated time
    printf("SNTP time sync complete!\n");
    rtc_print();
}
