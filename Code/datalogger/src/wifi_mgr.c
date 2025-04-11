#include <stdio.h>

#include "wifi_mgr.h"
#include "params.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define MAX_RETRY_DELAY 300000000

// MAC address is 28:CD:C1:0E:C6:5B
static const char* ssid = "WPI-PSK";
static const char* pass = "photosynthesize";

static uint32_t retry_delay = INIT_WAIT_US;
static uint64_t prev_wifi_check = 0;

bool wifi_init(void) {
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
        CYW43_AUTH_WPA2_AES_PSK, INIT_WAIT_MS)) {
        printf("Wifi connection failed!\n");
        return false;
    }
    printf("Wifi connection success!\n");

    prev_wifi_check = time_us_64();
    return true;
}

bool should_check_wifi(void) {
    return time_us_64() - prev_wifi_check > retry_delay;
}

bool wifi_check_reconnect(void) {

    prev_wifi_check = time_us_64();

    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        return true;
    }
    printf("WiFi disconnected! Attempting reconnection...");

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass,
        CYW43_AUTH_WPA2_AES_PSK, INIT_WAIT_MS)) {
        printf("Wifi reconnection failed!\n");
        retry_delay = 2;
        if (retry_delay > MAX_RETRY_DELAY) {
            retry_delay = MAX_RETRY_DELAY;
        }
        return false;
    }
    retry_delay = INIT_WAIT_US;
    printf("Wifi reconnection success!\n");

    return true;
}
