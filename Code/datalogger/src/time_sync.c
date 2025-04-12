#include <stdio.h>
#include <string.h>

#include "time_sync.h"

#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"

// NTP server configuration
#define NTP_PORT 123
#define NTP_SERVER "pool.ntp.org"  // NTP pool address

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

static const uint32_t rtc_init_timeout_us = 5000000;  // 5sec
static const uint32_t ntp_timeout_us = 15000000;  // 15sec
static const uint64_t sync_timeout_us = 60000000;  // 1min

static const uint32_t base_retry_delay = ntp_timeout_us;  // 1min
static const uint32_t max_retry_delay = 900000000;  // 1min

static uint32_t sync_retry_delay = base_retry_delay;
static uint8_t sync_attempts = 0;
static uint64_t timeout = 0;
static uint64_t sync_timeout = 0;

// Whether the RTC has been synced recently
static bool is_synchronized = false;
static bool init_flag = false;

// NTP constants
static const uint64_t timestamp_delta = 2208988800;  // Seconds between 1900 (NTP epoch) and 1970 (Unix epoch)

// UDP connection
static struct udp_pcb *ntp_pcb = NULL;
static ip_addr_t ntp_server_addr;
static bool ntp_request_pending = false;

static bool ntp_send_request(void);

static void ntp_handle_error(void);

static void ntp_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
    const ip_addr_t* addr, u16_t port);

static void ntp_dns_callback(const char* name, const ip_addr_t* addr,
    void *arg);

bool rtc_safe_init(void) {
    // initialize the RTC
    rtc_init();
    rtc_set_datetime(&t_default);
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

// TODO: Make return string instead
// TODO: Pretty-pringing should offset timezone
void print_datetime(void) {
    rtc_get_datetime(&t_current);
    datetime_to_str(datetime_str, sizeof(datetime_buf), &t_current);
    printf("Current time: %s\n", datetime_str);
}

// TODO: Add function to return ISO8601 standard timestamp

bool rtc_synchronized(void) {
    if (is_synchronized && time_us_64() > sync_timeout) {
        is_synchronized = false;
    }
    return is_synchronized;
}

bool ntp_init(void) {
    // Create a new UDP control block
    ntp_pcb = udp_new();
    if (ntp_pcb == NULL) {
        printf("Failed to create UDP PCB for NTP\n");
        return false;
    }

    // Sets up ntp recieved callback
    udp_recv(ntp_pcb, ntp_recv_callback, NULL);
    
    printf("NTP initialized\n");

    while (!is_synchronized) {
        ntp_request_time();
    }
    init_flag = true;

    return true;
}

bool ntp_request_time(void) {
    if (ntp_request_pending) {
        // Check if previous request timed out
        if (time_us_64() > timeout) {
            printf("NTP request timed out\n");
            ntp_handle_error();
        } else {
            return false; // Still waiting for response
        }
    }
    if (!ntp_request_pending && time_us_64() < timeout) {
        return false;
    }

    // Resolve NTP server address
    err_t err = dns_gethostbyname(NTP_SERVER, &ntp_server_addr, ntp_dns_callback, NULL);
    
    if (err == ERR_OK) {
        // Address already resolved, send NTP request immediately
        return ntp_send_request();
    } else if (err == ERR_INPROGRESS) {
        // DNS resolution in progress, callback will send request
        printf("Resolving NTP server address...\n");
        ntp_request_pending = true;
        timeout = time_us_64() + ntp_timeout_us;
        return true;
    } else {
        printf("DNS resolution failed with error %d\n", err);
        ntp_handle_error();
        return false;
    }
}

static void ntp_handle_error(void) {
    ntp_request_pending = false;
    if (sync_attempts > 0 && init_flag) {
        timeout = time_us_64() + sync_retry_delay;
        sync_retry_delay *= 2;
        if (sync_retry_delay > max_retry_delay) {
            sync_retry_delay = max_retry_delay;
        }
    } else {
        timeout = time_us_64();
    }
    sync_attempts++;
}

static void ntp_dns_callback(const char* name, const ip_addr_t* addr, void* arg) {
    if (addr == NULL) {
        printf("NTP server DNS resolution failed\n");
        ntp_request_pending = false;
        return;
    }

    // Save NTP server address
    memcpy(&ntp_server_addr, addr, sizeof(ip_addr_t));
    printf("NTP server resolved to %s\n", ipaddr_ntoa(addr));
    
    // Send NTP request
    ntp_send_request();
}

static bool ntp_send_request(void) {
    // Create packet buffer for payload size of NTP request (48 bytes)
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, sizeof(ntp_packet_t), PBUF_RAM);
    if (p == NULL) {
        printf("Failed to allocate packet buffer for NTP request\n");
        ntp_handle_error();
        return false;
    }

    // Initialize NTP request packet
    memset(p->payload, 0, sizeof(ntp_packet_t));
    ntp_packet_t* ntp_packet = (ntp_packet_t *)p->payload;
    
    // Set version number to 4 and mode to 3 (client)
    ntp_packet->li_vn_mode = 0x1B; // 00 011 011: LI=0, VN=3, Mode=3 (client)

    // Send the packet
    err_t err = udp_sendto(ntp_pcb, p, &ntp_server_addr, NTP_PORT);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send NTP request, error: %d\n", err);
        ntp_handle_error();
        return false;
    }
    
    printf("NTP request sent\n");
    ntp_request_pending = true;
    // Sends the second request immediately after the first because of weird networking
    if (sync_attempts > 0) {
        timeout = time_us_64() + ntp_timeout_us;
    } else {
        timeout = time_us_64();
    }
    return true;
}

static void ntp_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                              const ip_addr_t* addr, u16_t port) {

    ntp_request_pending = false;

    if (p == NULL) {
        printf("Received NULL NTP response\n");
        ntp_handle_error();
        return;
    }
    printf("Received NTP response, processing...\n");
    
    // Check packet size
    if (p->len != sizeof(ntp_packet_t)) {
        printf("NTP of incorrect size: %d bytes\n", p->len);
        pbuf_free(p);
        ntp_handle_error();
        return;
    }
    printf("Packet size OK: %d bytes\n", p->len);
    
    // Extract NTP packet from buffer
    ntp_packet_t* ntp_packet = (ntp_packet_t *)p->payload;
    
    // Swap the byte order
    uint32_t tx_seconds = ntohl(ntp_packet->tx_ts_sec);

    // Adjust for NTP epoch (1900) to Unix epoch (1970)
    uint32_t unix_seconds = tx_seconds - timestamp_delta;
    
    // Convert to datetime structure
    time_t unix_time = (time_t)unix_seconds;
    struct tm *tm = gmtime(&unix_time);
    
    if (tm == NULL) {
        printf("Failed to convert NTP time (gmtime returned NULL)\n");
        pbuf_free(p);
        ntp_handle_error();
        return;
    }
    
    // Create datetime for RTC
    datetime_t t = {
        .year  = tm->tm_year + 1900,
        .month = tm->tm_mon + 1,
        .day   = tm->tm_mday,
        .dotw  = tm->tm_wday,
        .hour  = tm->tm_hour,
        .min   = tm->tm_min,
        .sec   = tm->tm_sec
    };
    
    // Set RTC with synchronized time
    rtc_set_datetime(&t);
    is_synchronized = true;
    sync_timeout = time_us_64() + sync_timeout_us;
    sync_attempts = 0;
    sync_retry_delay = base_retry_delay;
    
    printf("RTC synchronized with NTP\n");
    print_datetime();
    
    pbuf_free(p);
    
}
