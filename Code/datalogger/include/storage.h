#pragma once

#include "pico/stdlib.h"

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

/**
 * Unmounts the SD card filesystem.
 */
void unmount_sd(void);
