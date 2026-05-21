/**
 * @file blink.c
 * @brief Example of using the Raspberry Pi Pico SDK to blink the onboard LED using delay and GPIO control.
 * @author STM32World <lth@stm32world.com>
 * @date 2026
 *
 * Copyright (c) 2026 STM32World <lth@stm32world.com>
 *
 * Third blink example for the Raspberry Pi Pico, demonstrating:
 *
 * - Basic GPIO control to blink the onboard LED
 *
 */

// Include necessary headers from the Pico SDK
#include "hardware/clocks.h" // For clock frequency information
#include "hardware/gpio.h"   // For GPIO control
#include "hardware/vreg.h"   // Needed for voltage scaling
#include "pico/multicore.h"  // For multicore support
#include "pico/mutex.h"      // For mutexes
#include "pico/stdlib.h"     // For sleep and stdio initialization

#ifndef LED_DELAY
#define LED_DELAY 500 // 500ms
#endif

// Perform initialisation
int pico_led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);              // The LED pin is defined in the board header as PICO_DEFAULT_LED_PIN
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); // Set the LED pin as an output
    return PICO_OK;
}

void pico_set_led(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

/**
 * @brief Main entry point
 */
int main() {

    int rc = pico_led_init(); // Initialize the LED GPIO

    hard_assert(rc == PICO_OK); // Ensure LED initialization was successful

    while (true) {
        pico_set_led(true);
        sleep_ms(LED_DELAY);
        pico_set_led(false);
        sleep_ms(LED_DELAY);
    }
}

// vim: ts=4 et nowrap
