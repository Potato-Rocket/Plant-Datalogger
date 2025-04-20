#pragma once

#include "pico/stdlib.h"

/**
 * A callback type to use for writing lines to a file.
 * 
 * @param buffer The string buffer to write to
 * @param size The size of the string buffer
 * 
 * @return Whether to write another line
 */
typedef bool (*line_getter_callback_t)(char *buffer, size_t size);

/**
 * Mounts the SD card filesystem for the first time, sets up the sd type.
 * Prints the total and available space on the card.
 * 
 * @return `true` if successful, `false` otherwise
 */
bool init_sd(void);

/**
 * Mounts the SD card filesystem.
 * 
 * @return `true` if successful, `false` otherwise
 */
bool mount_sd(void);

void sd_get_log_fname(char *fname, size_t size);

bool sd_write_lines(const char *fname, line_getter_callback_t get_line, size_t max_lines);

/**
 * Unmounts the SD card filesystem.
 */
void unmount_sd(void);
