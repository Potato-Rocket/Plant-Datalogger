#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "logging.h"

#define LOG_BUFFER_SIZE (1024u * 8u)

// a circular character buffer for storing messages
typedef struct {
    char buffer[LOG_BUFFER_SIZE];
    uint16_t read_index;
    uint16_t write_index;
    uint16_t count;
} log_buffer_t;

static log_buffer_t log_buffer;

// the strings corresponding to LogLevel
static const char *log_level_str[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
};

// the strings corresponding to LogCategory
static const char *log_category_str[] = {
    "SYSTEM",
    "WIFI",
    "NTP",
    "SENSOR",
    "RTC",
    "BUTTON",
    "LED",
    "SD",
};

// the lowest log level to print
static const LogLevel print_level = LOG_DEBUG;
// the lowest log level to store
static const LogLevel store_level = LOG_INFO;
// the lowest log level to cache in flash if storage unavailable
static const LogLevel critical_level = LOG_ERROR;

/**
 * Writes a string to the circular log buffer.
 */
static bool _write_buffer(const char *message, size_t size);

/**
 * Reads a string from the circular log buffer.
 */
static bool _write_buffer(const char *message, size_t size);

bool init_log(void) {
    if (!(store_level >= critical_level && print_level >= store_level)) {
        return false;
    }
    log_buffer.read_index = 0;
    log_buffer.write_index = 0;
    log_buffer.count = 0;
    return true;
}

void log_message(LogLevel lvl, LogCategory cat, const char *fmt, ...)
{
    // don't print anything if below the current log level
    if (lvl > print_level) {
        return;
    }

    // decompose the micros timestamp
    uint64_t timestamp = to_us_since_boot(get_absolute_time());
    uint32_t hours = (uint32_t)(timestamp / 3600000000ull);
    uint8_t minutes = (uint8_t)((timestamp / 60000000ul) % 60);
    uint8_t seconds = (uint8_t)((timestamp / 1000000ul) % 60);
    uint16_t millis = (uint32_t)(timestamp % 1000000ul);

    // format the timestamp and metadata to a string buffer
    char buffer[MAX_MESSAGE_SIZE];
    size_t len = snprintf(buffer, sizeof(buffer),
                          "[%u:%02u:%02u.%06u][%5s][%6s] ",
                          hours, minutes, seconds, millis,
                          log_level_str[lvl], log_category_str[cat]);

    // append the specified output string
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer + len, sizeof(buffer) - len, fmt, args);
    va_end(args);
    
    // println the string to the terminal with no extra formatting
    stdio_puts(buffer);

    if (lvl <= store_level)
    {
        if (!_write_buffer(buffer, sizeof(buffer)))
        {
            flush_log_buffer();
            _write_buffer(buffer, sizeof(buffer));
        }
    }

}

void flush_log_buffer(void)
{

}

bool _write_buffer(const char *message, size_t size)
{
    if (size > MAX_MESSAGE_SIZE)
    {
        return false;
    }

    // where to start writing the next string
    char *write_position = &log_buffer.buffer[log_buffer.write_index];
    // how much space is left before wrapping is needed
    size_t space_available = LOG_BUFFER_SIZE - log_buffer.write_index;
    // size of string to pring, including the null terminator
    size_t len = strnlen(message, MAX_MESSAGE_SIZE) + 1;

    // if the string will overflow the buffer
    if (len > space_available)
    {
        // calculate how many characters will overflow
        size_t overflow = len - space_available;
        // if the read index is too close, don't write
        if (log_buffer.write_index <= log_buffer.read_index ||
            overflow > log_buffer.read_index)
        {
            return false;
        }
        // copy the part of the string that fits
        memcpy(write_position, message, space_available);
        // copy the part of the string that overflows
        memcpy(&log_buffer.buffer[0], message + space_available, overflow);
        // reset the write index
        log_buffer.write_index = overflow;
        return true;
    }
    else
    {
        // if the read index is too close, don't write
        if (log_buffer.write_index <= log_buffer.read_index &&
            log_buffer.write_index + len > log_buffer.read_index)
        {
            return false;
        }
        // copy the string
        memcpy(write_position, message, len);
        log_buffer.write_index += len;
    }
    
    log_buffer.count++;
    return true;
}

bool _read_buffer(char *buffer)
{
    size_t size = MAX_MESSAGE_SIZE;

    if (log_buffer.count == 0)
    {
        return false;
    }

    // where to start writing the next string
    char *read_position = &log_buffer.buffer[log_buffer.read_index];
    // how much space is left before wrapping is needed
    size_t space_available = LOG_BUFFER_SIZE - log_buffer.read_index;
    // finds the number of characters to read up to the end of the buffer
    size_t len = strnlen(read_position, (size > space_available) ? space_available : size) + 1;

    // if at end of buffer and over
    if (len > space_available) {
        // pointer to the start of the log buffer
        char *start = &log_buffer.buffer[0];
        // how many characters have overflowed, may be just null terminator
        size_t overflow = strnlen(start, size - space_available) + 1;
        // copy part of string before wrap
        memcpy(buffer, read_position, space_available);
        // copy part of string after wrap
        memcpy(buffer + len, start, overflow);

        log_buffer.read_index = overflow;
    }
    else
    {
        // otherwise, just copy the string
        memcpy(buffer, read_position, len);

        log_buffer.read_index += len;
    }

    log_buffer.count--;
    return log_buffer.count > 0;
}
