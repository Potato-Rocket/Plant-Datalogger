#include <stdio.h>

#include "wifi_mgr.h"
#include "error_mgr.h"

#include "pico/cyw43_arch.h"

// MAC address is 28:CD:C1:0E:C6:5B
#define SSID "WPI-PSK"
#define PASS "photosynthesize"

// flag for whether the wifi was connected when last checked
static bool is_connected = false;

// timeout for library wifi functions
static const uint32_t init_timeout_ms = 10000ul;  // 10sec

// baseline wait between reconnection attempts
static const uint32_t base_retry_delay = 3600000000ul;  // 1hr
// maximum wait between reconnection attempts
static const uint32_t max_retry_delay = 300000000ul;  // 5min

// dynamic wait between reconnection attempts 
static uint32_t retry_delay_us = base_retry_delay;
// tracks when a new wifi check can happen
static uint64_t timeout = 0;

bool wifi_init(void) {
    // initialize the WiFi chip
    if (cyw43_arch_init() != 0) {
        printf("Wi-Fi init failed!\n");
        return false;
    }
    printf("Wi-Fi init success!\n");

    // enable station mode
    cyw43_arch_enable_sta_mode();

    // connect to the network
    if (cyw43_arch_wifi_connect_timeout_ms(SSID, PASS,
        CYW43_AUTH_WPA2_AES_PSK, init_timeout_ms) != 0) {
        printf("Wifi connection failed!\n");
        return false;
    }
    printf("Wifi connection success!\n");

    // set flag and recheck timeout
    timeout = time_us_64() + retry_delay_us;
    is_connected = true;
    return true;
}

bool should_check_wifi(void) {
    return time_us_64() > timeout;
}

void wifi_check_reconnect(void) {

    // check whether the wifi connection is up
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        // if up, reset delay, timout, flag
        retry_delay_us = base_retry_delay;
        timeout = time_us_64() + retry_delay_us;
        is_connected = true;
        set_error(ERROR_WIFI_DISCONNECTED, false);
        return;
    }
    printf("WiFi disconnected! Attempting reconnection...");

    // otherwise, attempt to reconnect
    if (cyw43_arch_wifi_connect_timeout_ms(SSID, PASS,
        CYW43_AUTH_WPA2_AES_PSK, init_timeout_ms) != 0) {
        printf("Wifi reconnection failed!\n");
        // if failed, double the delay until the next retry
        retry_delay_us *= 2;
        // cap the maximum retry duration
        if (retry_delay_us > max_retry_delay) {
            retry_delay_us = max_retry_delay;
            set_error(ERROR_WIFI_DISCONNECTED, true);
        }
        // update the timeout and flag
        timeout = time_us_64() + retry_delay_us;
        is_connected = false;
        return;
    }
    // if successfully reconnected, reset delay, timeout, flag
    retry_delay_us = base_retry_delay;
    timeout = time_us_64() + retry_delay_us;
    printf("Wifi reconnection success!\n");

    is_connected = true;
    set_error(ERROR_WIFI_DISCONNECTED, false);
    return;
}

bool wifi_connected(void) {
    return is_connected;
}
