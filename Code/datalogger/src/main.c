#include <stdio.h>

#include "pico/stdlib.h"

#include "wifi_mgr.h"
#include "time_sync.h"
#include "sensors.h"
#include "logging.h"
#include "button.h"
#include "error_mgr.h"
// TODO: Add module for OLED display
// TODO: Add module for SD card reader
// TODO: Add module to manage EEPROM caching
// TODO: Add module to manage configuration
// TODO: Add module to manage logging messages
// TODO: Add module to manage combined serial and logging

int main() {
    // wait up to five seconds for the serial port to open
    stdio_init_all();
    sleep_ms(5000);
    printf("Initializing startup...\n");

    // initialize error indicator state machine
    init_errors(WARNING_INTIALIZING | WARNING_RECALIBRATING);

    // try to connect to WiFi
    if (!wifi_init()) {
        return -1;
    }

    // try to setup RTC
    if (!rtc_safe_init()) {
        return -1;
    }

    // try to setup NTP
    if (!ntp_init()) {
        return -1;
    }
    
    // initialize sensors
    init_button();
    init_sensors();

    while (true) {
        // checks once every ten seconds, blocking if reconnecting
        if (should_check_wifi()) wifi_check_reconnect();

        // ntp needs wifi, if not synchronized update the ntp routine
        if (!rtc_synchronized()) ntp_request_time();

        if (check_long_press()) calibrate_soil();

        // reads sensors once per minute
        if (should_update_sensors()) {
            // assume the rtc is more or less fine after init
            char buffer[64];
            get_pretty_datetime(&buffer[0], sizeof(buffer));
            printf("\nLocal time: %s\n", buffer);

            // update the sensors, print readings only if successful
            if (update_sensors()) print_readings();
            
        }

        sleep_ms(10);
    }

}
