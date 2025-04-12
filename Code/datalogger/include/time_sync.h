#pragma once

#include "pico/stdlib.h"

/**
 * Initializes the RTC and sets the default time.
 * 
 * @return `true` if successful, `false` otherwise
 */
bool rtc_safe_init(void);

/**
 * Formats the current local time in a readable, printable format.
 * 
 * @return The pointer to a string buffer.
 */
char* get_pretty_datetime(void);

/**
 * Formats the current UTC according to ISO8601.
 * 
 * @return The pointer to a string buffer.
 */
char* get_timestamp(void);

/**
 * Whether the rtc has been synchronized within the defined time period.
 */
bool rtc_synchronized(void);

/**
 * Initializes the UDP control block used for NTP requests. Sets up callbacks.
 * Begins aggressively (no retry delay) trying to sync RTC with NTP. Will hang
 * if connection fails, is blocking.
 * 
 * @return `true` id successful, `false` otherwisee
 */
bool ntp_init(void);

/**
 * Runs the NTP sync routine. If there is request already in progress and not
 * timed out, or we are waiting to retry, do nothing. Otherwise, tries to
 * resolve the IP address for the NTP server.
 * 
 * If the IP address has already been resolved (in a previous attempt, for
 * example), immediately send the NTP request. Otherwise, wait for the DNS
 * callback.
 * 
 * If any error takes place, handle it gracefully.
 * 
 * @return `true` if an NTP request is in progress, `false` otherwise
 */
bool ntp_request_time(void);
