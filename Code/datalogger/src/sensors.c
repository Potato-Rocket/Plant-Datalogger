#include <stdio.h>

#include <sensors.h>

#define LED_PIN 22

bool init_sensors(void) {

    // set up indicator light
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    return true;
    
}
