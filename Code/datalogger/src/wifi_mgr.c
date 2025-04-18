#include "wifi_mgr.h"
#include "utils.h"
#include "error_mgr.h"
#include "logging.h"

#include "pico/cyw43_arch.h"

// MAC address is 28:CD:C1:0E:C6:5B
#define SSID "WPI-PSK"
#define PASS "photosynthesize"

// flag for whether the wifi was connected when last checked
static bool is_connected = false;

// timeout for library wifi functions
static const uint32_t init_timeout_ms = 10000ul; // 10sec

// baseline wait between reconnection attempts
static const uint32_t base_retry_delay_ms = 3600000ul; // 1hr
// maximum wait between reconnection attempts
static const uint32_t max_retry_delay_ms = 300000ul; // 5min

// dynamic wait between reconnection attempts
static uint32_t retry_delay = base_retry_delay_ms;
// tracks when a new wifi check can happen
static absolute_time_t timeout = 0;

bool wifi_init(void)
{
    // initialize the WiFi chip
    log_message(LOG_INFO, LOG_WIFI, "Initializing Wi-Fi...");
    if (cyw43_arch_init() != 0)
    {
        log_message(LOG_ERROR, LOG_WIFI, "Wi-Fi init failed!");
        return false;
    }
    log_message(LOG_INFO, LOG_WIFI, "Wi-Fi init success");

    // enable station mode
    cyw43_arch_enable_sta_mode();

    // connect to the network
    log_message(LOG_INFO, LOG_WIFI, "Connecting to Wi-Fi network...");
    if (cyw43_arch_wifi_connect_timeout_ms(SSID, PASS,
                                           CYW43_AUTH_WPA2_AES_PSK, init_timeout_ms) != 0);
    {
        log_message(LOG_ERROR, LOG_WIFI, "Wi-Fi connection failed!");
        return false;
    }
    log_message(LOG_INFO, LOG_WIFI, "Wi-Fi connection success");

    // set flag and recheck timeout
    timeout = make_timeout_time_ms(retry_delay);
    is_connected = true;
    return true;
}

bool should_check_wifi(void)
{
    return is_timed_out(timeout);
}

void wifi_check_reconnect(void)
{

    // check whether the wifi connection is up
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP)
    {
        // if up, reset delay, timout, flag
        retry_delay = base_retry_delay_ms;
        timeout = make_timeout_time_ms(retry_delay);
        is_connected = true;
        log_message(LOG_INFO, LOG_WIFI, "Wi-Fi connection nominal");
        set_error(ERROR_WIFI_DISCONNECTED, false);
        return;
    }
    log_message(LOG_WARNING, LOG_WIFI, "Wi-Fi disconnected, attempting reconnection...");

    // otherwise, attempt to reconnect
    if (cyw43_arch_wifi_connect_timeout_ms(SSID, PASS,
                                           CYW43_AUTH_WPA2_AES_PSK, init_timeout_ms) != 0)
    {
        // if failed, double the delay until the next retry
        retry_delay *= 2;
        // cap the maximum retry duration
        if (retry_delay > max_retry_delay_ms)
        {
            retry_delay = max_retry_delay_ms;
            log_message(LOG_ERROR, LOG_WIFI, "Wi-Fi reconnection repeatedly failed!");
            set_error(ERROR_WIFI_DISCONNECTED, true);
        }
        else
        {
            log_message(LOG_WARNING, LOG_WIFI, "Wi-Fi reconnection failed");
        }
        // update the timeout and flag
        timeout = make_timeout_time_ms(retry_delay);
        is_connected = false;
        return;
    }
    // if successfully reconnected, reset delay, timeout, flag
    retry_delay = base_retry_delay_ms;
    timeout = make_timeout_time_ms(retry_delay);
    log_message(LOG_INFO, LOG_WIFI, "Wi-Fi reconnection success");

    is_connected = true;
    set_error(ERROR_WIFI_DISCONNECTED, false);
    return;
}

bool wifi_connected(void)
{
    return is_connected;
}
