#pragma once

#include "pico/stdlib.h"

#define MAX_MESSAGE_SIZE 256u

/**
 * The levels of log messages.
 */
typedef enum
{
    LOG_ERROR,  // system at risk of failure without user intervention
    LOG_WARN,   // undesirable state but no user intervention required
    LOG_INFO,   // general output
    LOG_DEBUG,  // low level messages for debugging use
} LogLevel;

/**
 * The categories of log messages.
 */
typedef enum
{
    LOG_SYSTEM,  // related to high level operation
    LOG_WIFI,    // related to the wifi manager
    LOG_NTP,     // related to the NTP requests
    LOG_SENSOR,  // related to the sensors
    LOG_RTC,     // related to the RTC
    LOG_BUTTON,  // related to the button
    LOG_LED,     // related to the indicator light
    LOG_SD,      // related to the SD card
} LogCategory;

/**
 * Initialize the logging system. Ensures that log levels are set up correctly
 * and allocates memory for a message storage buffer.
 */
bool init_log(void);

/**
 * Structured logging function. If the log level is above the defined logging
 * level, the message will be printed with a timestamp, the message level, and
 * the message category. Formats strings using `printf`, appends a newline
 * character automatically.
 * 
 * @param lvl The log level of the message
 * @param cat The category of the message
 * @param fmt A string to print
 * @param ... Additional formatting parameters
 */
void log_message(LogLevel lvl, LogCategory cat, const char *fmt, ...);

void flush_log_buffer(void);
