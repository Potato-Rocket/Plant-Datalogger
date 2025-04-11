#include <stdio.h>

#include "wifi_mgr.h"
#include "params.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// MAC address is 28:CD:C1:0E:C6:5B
static const char* ssid = "WPI-PSK";
static const char* pass = "photosynthesize";

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

    return true;
}
