/**
 * Copyright (c) 2026 STM32World <lth@stm32world.com>
 *
 * First example of using the Raspberry Pi Pico SDK to blink the onboard LED and print messages from both cores.
 *
 */

// Include necessary headers from the Pico SDK
#include "hardware/gpio.h"  // For GPIO control
#include "pico/multicore.h" // For multicore support
#include "pico/mutex.h"     // For mutexes
#include "pico/stdlib.h"    // For sleep and stdio initialization

// Include standard I/O for printf
#include <stdio.h>

#ifndef LED_DELAY
#define LED_DELAY 500000 // 500ms in microseconds
#endif

// Mutex for synchronizing access to printf
auto_init_mutex(printf_mutex);

// Perform initialisation
int pico_led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);              // The LED pin is defined in the board header as PICO_DEFAULT_LED_PIN
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); // Set the LED pin as an output
    return PICO_OK;
}

/**
 * @brief Returns a 32-bit millisecond counter.
 * Equivalent to STM32's uwTick.
 * * This uses the 64-bit hardware timer (1MHz) and scales to ms.
 * Rollover occurs every ~49.7 days.
 */
static inline uint32_t time_ms_32(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

/**
 * @brief Toggles the state of the default LED.
 */
void pico_toggle_led() {
    gpio_xor_mask64(((uint64_t)1 << PICO_DEFAULT_LED_PIN));
}

/**
 * @brief Entry point for Core 1.
 */
void core1_entry() {

    mutex_enter_blocking(&printf_mutex); // Synchronize with Core 0 for printing
    printf("Core 1: Booting...\n");
    mutex_exit(&printf_mutex);

    uint64_t now, loop_cnt = 0, next_tick = 1500000;

    while (1) {

        now = get_absolute_time(); // Unsure if mutex is needed here.

        if (now >= next_tick) {
            mutex_enter_blocking(&printf_mutex);
            printf("Core 1 tick %lu (loop = %lu)\n", (uint32_t)(now / 1000), loop_cnt);
            mutex_exit(&printf_mutex);
            loop_cnt = 0;
            next_tick = now + 1000000;
        }

        ++loop_cnt;

        // Give the memory bus and Core 0 a chance to breathe
        tight_loop_contents();
    }
}

/**
 * @brief Main entry point for Core 0.
 */
int main() {

    int rc = pico_led_init(); // Initialize the LED GPIO

    hard_assert(rc == PICO_OK); // Ensure LED initialization was successful

    stdio_init_all(); // Initialize all standard I/O (UART, USB, etc.)

    // Explicitly override the baud rate for UART0 to 2M
    uart_set_baudrate(uart0, 921600);

    // Give UART a moment to stabilize
    sleep_ms(50);

    mutex_enter_blocking(&printf_mutex); // Mutex is not strictly necessary here since Core 1 hasn't started yet, but it's good practice to be consistent
    printf("\n\n\nCore 0: Booting...\n");
    mutex_exit(&printf_mutex);

    // Launch core1_entry function on Core 1
    multicore_launch_core1(core1_entry);

    uint64_t now, loop_cnt = 0, next_blink = LED_DELAY, next_tick = 1000 * 1000;

    while (true) {

        now = get_absolute_time(); // Unsure if mutex is needed here.

        if (now >= next_blink) {
            pico_toggle_led();
            next_blink = now + LED_DELAY;
        }

        if (now >= next_tick) {
            mutex_enter_blocking(&printf_mutex); // Ensure we've got exclusive access to printf
            printf("Core 0 tick %lu (loop = %lu)\n", (uint32_t)(now / 1000), loop_cnt);
            mutex_exit(&printf_mutex); // Release the mutex so Core 1 can print
            loop_cnt = 0;
            next_tick = now + 1000000;
        }

        ++loop_cnt;
    }
}

// vim: ts=4 et nowrap
