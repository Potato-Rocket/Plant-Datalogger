#pragma once

#include "pico/stdlib.h"

typedef enum
{
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
} LogLevel;

typedef enum
{
    LOG_SYSTEM,
    LOG_WIFI,
    LOG_NTP,
    LOG_SENSOR,
    LOG_RTC,
    LOG_BUTTON,
    LOG_LED,
} LogCategory;

void log_message(LogLevel lvl, LogCategory cat, const char *fmt, ...);
