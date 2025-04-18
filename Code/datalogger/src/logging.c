#include "logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

static const char *log_level_str[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
};

static const char *log_category_str[] = {
    "SYSTEM",
    "WIFI",
    "NTP",
    "SENSOR",
    "RTC",
};

static const LogLevel log_level = LOG_DEBUG;

void log_message(LogLevel lvl, LogCategory cat, const char *fmt, ...)
{
    if (lvl > log_level) {
        return;
    }

    uint64_t timestamp = to_us_since_boot(get_absolute_time()) / 1000;
    stdio_printf("%u", timestamp);
    uint32_t hours = (uint32_t)(timestamp / 3600000ul);
    uint8_t minutes = (uint8_t)((timestamp / 60000ul) % 60);
    uint8_t seconds = (uint8_t)((timestamp / 10000u) % 60);
    uint16_t millis = (uint16_t)(timestamp % 1000u);

    char buffer[256];
    uint8_t len = snprintf(buffer, sizeof(buffer),
                           "[%u:%02u:%02u.%03u][%s][%s] ",
                           hours, minutes, seconds, millis,
                           log_level_str[lvl], log_category_str[cat]);

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer + len, sizeof(buffer) - len, fmt, args);
    va_end(args);
    
    stdio_puts(buffer);
}
