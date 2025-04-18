#include "pico/stdlib.h"

#include "wifi_mgr.h"
#include "time_sync.h"
#include "sensors.h"
#include "logging.h"
#include "button.h"
#include "error_mgr.h"
#include "storage.h"

int main()
{
    // wait up to five seconds for the serial port to open
    stdio_init_all();
    sleep_ms(5000);
    log_message(LOG_INFO, LOG_SYSTEM, "Initializing datalogger...");

    // initialize error indicator state machine
    init_errors(WARNING_INTIALIZING | WARNING_RECALIBRATING);

    // try to connect to WiFi
    if (!wifi_init())
    {
        return -1;
    }

    // try to setup RTC
    if (!rtc_safe_init())
    {
        return -1;
    }

    // try to setup NTP
    if (!ntp_init())
    {
        return -1;
    }

    // initialize sensors
    init_button();
    init_sensors();

    while (true)
    {
        // checks once every ten seconds, blocking if reconnecting
        if (should_check_wifi())
        {
            wifi_check_reconnect();
        }

        // ntp needs wifi, if not synchronized update the ntp routine
        if (!rtc_synchronized())
        {
            ntp_request_time();
        }

        if (check_long_press())
        {
            calibrate_soil();
        }

        // reads sensors once per minute
        if (should_update_sensors())
        {
            // assume the rtc is more or less fine after init
            char buffer[64];
            get_pretty_datetime(&buffer[0], sizeof(buffer));
            log_message(LOG_INFO, LOG_RTC, "Local time: %s", buffer);

            // update the sensors, print readings only if successful
            if (update_sensors())
            {
                print_readings();
            }
        }

        sleep_ms(10);
    }
}
