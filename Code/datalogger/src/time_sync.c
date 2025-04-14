#include <stdio.h>
#include <string.h>

#include "time_sync.h"
#include "wifi_mgr.h"
#include "error_mgr.h"

#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"

// NTP server configuration
#define NTP_PORT 123u
#define NTP_SERVER "pool.ntp.org"
#define TIME_ZONE_OFFSET -4l

// NTP packet structure (48 bytes)
typedef struct __attribute__((packed)) {
    uint8_t li_vn_mode;      // Leap Indicator, Version Number, Mode
    uint8_t stratum;         // Stratum level
    uint8_t poll;            // Poll interval
    uint8_t precision;       // Precision
    uint32_t root_delay;     // Root delay
    uint32_t root_dispersion; // Root dispersion
    uint32_t ref_id;         // Reference ID
    uint32_t ref_ts_sec;     // Reference timestamp seconds
    uint32_t ref_ts_frac;    // Reference timestamp fraction
    uint32_t orig_ts_sec;    // Originate timestamp seconds
    uint32_t orig_ts_frac;   // Originate timestamp fraction
    uint32_t rx_ts_sec;      // Receive timestamp seconds
    uint32_t rx_ts_frac;     // Receive timestamp fraction
    uint32_t tx_ts_sec;      // Transmit timestamp seconds
    uint32_t tx_ts_frac;     // Transmit timestamp fraction
} ntp_packet_t;

// create and set default date and time
static const datetime_t t_default = {
    .year  = 2025u,
    .month = 1u,
    .day   = 1u,
    .dotw  = 3u,
    .hour  = 0,
    .min   = 0,
    .sec   = 0
};

// how long to wait for the RTC to be running
static const uint32_t rtc_init_timeout_us = 5000000ul;  // 5sec
// how long to wait for NTP operations to timeout
static const uint32_t ntp_timeout_us = 15000000ul;  // 15sec
// how long to wait for the first NTP request sent to timeout
static const uint32_t ntp_init_timeout_us = 1000000ul;  // 1sec
// how long to wait before resyncing is needed
static const uint64_t sync_timeout_us = 86400000000ull;  // 24hr

// baseline wait between failed NTP requests
static const uint32_t base_retry_delay = ntp_timeout_us;  // 15sec
// maximum wait between failed NTP requests
static const uint32_t max_retry_delay = 900000000ul;  // 15min

// dynamic wait between failed NTP requests
static uint32_t sync_retry_delay = base_retry_delay;
// number of failed NTP requests
static uint8_t sync_attempts = 0;
// tracks when a the NTP request times out, or a new one can happen
static uint64_t timeout = 0;
// tracks when the system will need to be resynced
static uint64_t sync_timeout = 0;

// whether the RTC has been synced recently
static bool is_synchronized = false;
// whether the RTC has been synced at least once
static bool init_flag = false;

// offset between NTP epoch (1900) and the Unix epoch (1970)
static const uint64_t epoch_conversion = 2208988800ull;  // 70yr

// UDP control block
static struct udp_pcb *ntp_pcb = NULL;
// NTP server IP address
static ip_addr_t ntp_server_addr;
// whether an NTP action is in progress
static bool ntp_request_pending = false;

/**
 * Handles any sort of error from the NTP sync routine. Resets the pending flag,
 * updates the retry delay, and sets the timeout until the next attempt.
 */
static void _ntp_handle_error(void);

/**
 * Function to create an NTP request packet, and send it to the resolved IP
 * address via the UDP control block. Handles errors gracefully.
 * 
 * @return `true` if sent successfully, `false` otherwise
 */
static bool _ntp_send_request(void);

/**
 * Callback function for when an IP address for the NTP server is successfully
 * resolved. Checks that the address is valid, then sends the NTP request.
 */
static void _ntp_dns_callback(const char* name, const ip_addr_t* addr,
    void *arg);

/**
 * Callback function for when a NTP packet is recieved. Tries to set the RTC
 * based on the timestamp. Handles errors gracefully.
 */
static void _ntp_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
    const ip_addr_t* addr, u16_t port);

bool rtc_safe_init(void) {
    // initialize the RTC
    rtc_init();
    // set the default datetime
    rtc_set_datetime(&t_default);
    // wait for the RTC to start running
    timeout = time_us_64() + rtc_init_timeout_us;
    while (!rtc_running()) {
        if (time_us_64() > timeout) {
            printf("RTC init timed out!\n");
            return false;
        }
    }
    printf("RTC init success!\n");

    return true;
}

void get_pretty_datetime(char* buffer, size_t buffer_size) {
    // validate parameters
    if (buffer == NULL || buffer_size < 1) {
        printf("Error: Invalid buffer provided to get_pretty_datetime\n");
        return;
    }
    // make sure the buffer is null-terminated even if we fail
    buffer[0] = '\0';

    if (!init_flag) {
        strncpy(buffer, "RTC not yet initialized!", buffer_size);
        return;
    }

    // read from the RTC
    datetime_t t;
    rtc_get_datetime(&t);

    // convert to epoch time
    time_t epoch;
    datetime_to_time(&t, &epoch);
    // adjust to the local timezone
    epoch += TIME_ZONE_OFFSET * 3600l;
    // convert to a standard datetime struct
    struct tm dt = *gmtime(&epoch);

    // return the pointer to a formatted string
    strftime(buffer, buffer_size, "%A, %B %d, %Y  %H:%M:%S", &dt);
}

void get_timestamp(char* buffer, size_t buffer_size) {
    // validate parameters
    if (buffer == NULL || buffer_size < 1) {
        printf("Error: Invalid buffer provided to get_pretty_datetime\n");
        return;
    }
    // make sure the buffer is null-terminated even if we fail
    buffer[0] = '\0';

    if (!init_flag) {
        strncpy(buffer, "NULL_DATETIME", buffer_size);
        return;
    }

    // read from the RTC
    datetime_t t;
    rtc_get_datetime(&t);

    // convert to a standard datetime format
    struct tm dt;
    datetime_to_tm(&t, &dt);

    // return the pointer to a formatted string
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &dt);
}

bool rtc_synchronized(void) {
    // if it has been long enough since last synced, trip the flag
    if (is_synchronized && time_us_64() > sync_timeout) {
        is_synchronized = false;
    }
    return is_synchronized;
}

bool ntp_init(void) {
    // Create a new UDP control block
    ntp_pcb = udp_new();
    if (ntp_pcb == NULL) {
        printf("Failed to create UDP PCB for NTP!\n");
        return false;
    }

    // Sets up ntp recieved callback
    udp_recv(ntp_pcb, _ntp_recv_callback, NULL);
    
    printf("NTP initialized!\n");

    while (!is_synchronized) {
        ntp_request_time();
    }
    init_flag = true;
    set_error(WARNING_INTIALIZING, false);

    return true;
}

bool ntp_request_time(void) {

    // if a request is already pending
    if (ntp_request_pending) {
        // check if previous request timed out
        if (time_us_64() > timeout) {
            printf("NTP request timed out!\n");
            _ntp_handle_error();
        } else {
            return false; // Still waiting for response
        }
    }
    // check if waiting to retry
    if (!ntp_request_pending && time_us_64() < timeout) {
        return false;
    }
    
    // Check WiFi connectivity first
    wifi_check_reconnect();
    if (!wifi_connected()) {
        printf("NTP request failed: WiFi not connected\n");
        return false;
    }

    // resolve NTP server address
    err_t err = dns_gethostbyname(NTP_SERVER, &ntp_server_addr, _ntp_dns_callback, NULL);
    
    if (err == ERR_OK) {
        // address already resolved, send NTP request immediately
        return _ntp_send_request();
    } else if (err == ERR_INPROGRESS) {
        // DNS resolution in progress, callback will send request
        printf("Resolving NTP server address...\n");
        ntp_request_pending = true;
        timeout = time_us_64() + ntp_timeout_us;
        return true;
    } else {
        printf("DNS resolution failed with error %d\n", err);
        _ntp_handle_error();
        return false;
    }
}

static void _ntp_handle_error(void) {
    // reset the pending flag
    ntp_request_pending = false;
    // if not the first attempt, and this isn't the startup sequence
    if (sync_attempts > 0 && init_flag) {
        // update the timeout based on the retry delayt
        timeout = time_us_64() + sync_retry_delay;
        // double the next retry delay
        sync_retry_delay *= 2;
        // cap the retry delay
        if (sync_retry_delay > max_retry_delay) {
            sync_retry_delay = max_retry_delay;
            set_error(ERROR_NTP_SYNC_FAILED, true);
        }
    } else {
        // otherwise no retry delay
        timeout = time_us_64();
    }
    // increment the attempt counter
    sync_attempts++;
}

static void _ntp_dns_callback(const char* name, const ip_addr_t* addr, void* arg) {
    if (addr == NULL) {
        printf("NTP server DNS resolution failed!\n");
        ntp_request_pending = false;
        return;
    }

    // save NTP server address
    memcpy(&ntp_server_addr, addr, sizeof(ip_addr_t));
    printf("NTP server resolved to %s\n", ipaddr_ntoa(addr));
    
    // send NTP request
    _ntp_send_request();
}

static bool _ntp_send_request(void) {
    // create packet buffer for payload size of NTP request (48 bytes)
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, sizeof(ntp_packet_t), PBUF_RAM);
    if (p == NULL) {
        printf("Failed to allocate packet buffer for NTP request!\n");
        _ntp_handle_error();
        return false;
    }

    // allocate memory for the NTP request packet
    memset(p->payload, 0, sizeof(ntp_packet_t));
    ntp_packet_t* ntp_packet = (ntp_packet_t *)p->payload;
    
    // set version number to 3 and mode to 3 (client)
    ntp_packet->li_vn_mode = 0x1B; // 00 011 011: LI=0, VN=3, Mode=3 (client)

    // send the packet
    err_t err = udp_sendto(ntp_pcb, p, &ntp_server_addr, NTP_PORT);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send NTP request, error: %d\n", err);
        _ntp_handle_error();
        return false;
    }
    
    printf("NTP request sent...\n");
    ntp_request_pending = true;
    // sends the first request with a smaller timeout to avoid needless waiting
    if (sync_attempts > 0) {
        timeout = time_us_64() + ntp_timeout_us;
    } else {
        timeout = time_us_64() + ntp_init_timeout_us;
    }
    return true;
}

static void _ntp_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                              const ip_addr_t* addr, u16_t port) {

    if (p == NULL) {
        printf("Received NULL NTP response!\n");
        _ntp_handle_error();
        return;
    }
    printf("Received NTP response, processing...\n");
    
    // check packet size
    if (p->len != sizeof(ntp_packet_t)) {
        printf("NTP of incorrect size: %d bytes\n", p->len);
        pbuf_free(p);
        _ntp_handle_error();
        return;
    }
    printf("Packet size OK: %d bytes\n", p->len);
    
    // extract NTP packet from buffer
    ntp_packet_t* ntp_packet = (ntp_packet_t *)p->payload;
    
    // swap the byte order
    uint32_t tx_seconds = ntohl(ntp_packet->tx_ts_sec);

    // adjust from NTP epoch (1900) to Unix epoch (1970)
    uint32_t unix_seconds = tx_seconds - epoch_conversion;
    
    // convert from unixtime to RTC datetime structure
    time_t epoch = (time_t)unix_seconds;
    datetime_t t;
    time_to_datetime(epoch, &t);
    
    // Set RTC with synchronized time
    if (!rtc_set_datetime(&t)) {
        printf("Error: Invalid datetime!\n");
        _ntp_handle_error();
        return;
    }
    // sets the sync flag and timeout
    is_synchronized = true;
    sync_timeout = time_us_64() + sync_timeout_us;
    // resets the attempts and retry delay for next sync routing
    sync_attempts = 0;
    sync_retry_delay = base_retry_delay;
    set_error(ERROR_NTP_SYNC_FAILED, false);
    
    printf("RTC synchronized with NTP!\n");

    char buffer[32];
    get_timestamp(&buffer[0], sizeof(buffer));
    printf("UTC: %s\n", buffer);
    
    // frees memory allocated to package buffer
    pbuf_free(p);
    
}
