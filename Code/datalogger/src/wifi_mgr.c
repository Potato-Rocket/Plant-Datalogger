#include <stdio.h>

#include "wifi_mgr.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// MAC address is 28:CD:C1:0E:C6:5B
#define SSID "WPI-PSK"
#define PASS "photosynthesize"

static bool is_connected = false;

static const uint32_t init_timeout_ms = 10000;  // 10sec

static const uint32_t base_retry_delay = 10000000;  // 10sec
static const uint32_t max_retry_delay = 300000000;  // 5min
static uint32_t retry_delay_us = base_retry_delay;
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

    timeout = time_us_64() + retry_delay_us;
    is_connected = true;
    return true;
}

bool should_check_wifi(void) {
    return time_us_64() > timeout;
}

void wifi_check_reconnect(void) {

    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        retry_delay_us = base_retry_delay;
        timeout = time_us_64() + retry_delay_us;
        is_connected = true;
        return;
    }
    printf("WiFi disconnected! Attempting reconnection...");

    if (cyw43_arch_wifi_connect_timeout_ms(SSID, PASS,
        CYW43_AUTH_WPA2_AES_PSK, init_timeout_ms) != 0) {
        printf("Wifi reconnection failed!\n");
        retry_delay_us *= 2;
        if (retry_delay_us > max_retry_delay) {
            retry_delay_us = max_retry_delay;
        }
        timeout = time_us_64() + retry_delay_us;
        is_connected = false;
        return;
    }
    retry_delay_us = base_retry_delay;
    timeout = time_us_64() + retry_delay_us;
    printf("Wifi reconnection success!\n");

    is_connected = true;
    return;
}

bool wifi_connected(void) {
    return is_connected;
}
