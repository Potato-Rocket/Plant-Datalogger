#include <stdio.h>
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#define LED_PIN 22

int main() {
    stdio_init_all();
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_12MA);

    while (true) {
        printf("LED on!\n");
        gpio_put(LED_PIN, true);
        sleep_ms(1000);

        printf("LED off!\n");
        gpio_put(LED_PIN, false);
        sleep_ms(1000);
    }
}
