/**
 * @file blink.c
 * @brief Example of using the Raspberry Pi Pico SDK to blink the onboard LED and print messages from both cores.
 * @author STM32World <lth@stm32world.com>
 * @date 2026
 *
 * Copyright (c) 2026 STM32World <lth@stm32world.com>
 *
 * Second example of using the Raspberry Pi Pico SDK to blink the onboard LED and print messages from both cores.  In the
 * example "blink.c", the LED is toggled in the main loop of Core 0, and both cores print messages every second.  In this
 * version, the LED is toggled using a repeating timer callback, which allows for more precise timing and frees up the
 * main loop to focus on printing messages.  The code also demonstrates how to use mutexes to synchronize access to
 * shared resources (like printf) between the two cores.
 *
 */

// Include necessary headers from the Pico SDK
#include "hardware/clocks.h" // For clock configuration
#include "hardware/gpio.h"   // For GPIO control
#include "hardware/vreg.h"   // Needed for voltage scaling
#include "pico/multicore.h"  // For multicore support
#include "pico/mutex.h"      // For mutexes
#include "pico/stdlib.h"     // For sleep and stdio initialization

// Include standard I/O for printf
#include <stdio.h>

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 500
#endif

// Mutex for synchronizing access to printf
auto_init_mutex(printf_mutex);

/**
 * @brief Initialize the Pico LED.
 * @return PICO_OK if successful, otherwise an error code.
 */
int pico_led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);              // The LED pin is defined in the board header as PICO_DEFAULT_LED_PIN
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); // Set the LED pin as an output
    gpio_put(PICO_DEFAULT_LED_PIN, 1);            // Start with the LED off (assuming active low)
    return PICO_OK;
}

/**
 * @brief Universal uS to mS for ARM and RISC-V.
 * This version is overflow-safe for 64-bit inputs and warning-free.
 */
static inline uint32_t time_ms_32(void) {
    // Constant: 0x418937 (approx 2^32 / 1000)
    // We multiply by 0x418937 and shift by 32.
    // This is mathematically: (us * 4294967) / 4294967296
    // It is very fast and overflow-safe for uptime up to 136 years.
    return (uint32_t)((time_us_64() * 0x418937ull) >> 32);
}

/**
 * @brief Toggles the state of the default LED.
 */
void pico_toggle_led() {
    gpio_xor_mask64(((uint64_t)1 << PICO_DEFAULT_LED_PIN));
}

/**
 * @brief Callback function for the repeating timer.
 * This function is called every 500ms as set up in main().
 */
bool repeating_timer_callback(struct repeating_timer *t) {
    pico_toggle_led();
    return true;
}

/**
 * @brief Entry point for Core 1.
 */
void core1_entry() {

    mutex_enter_blocking(&printf_mutex); // Synchronize with Core 0 for printing
    printf("Core 1: Booting...\n");
    mutex_exit(&printf_mutex);

    uint32_t now, loop_cnt = 0, next_tick = 1500;

    while (1) {

        now = time_ms_32(); // Unsure if mutex is needed here.

        if (now >= next_tick) {
            mutex_enter_blocking(&printf_mutex);
            printf("Core 1 tick %lu (loop = %lu)\n", now, loop_cnt);
            mutex_exit(&printf_mutex);
            loop_cnt = 0;
            next_tick = now + 1000;
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

    struct repeating_timer timer;

    // Boost voltage to 1.3V for stability at higher clocks
    // Standard is 1.1V; 1.3V is usually safe for 250MHz-350MHz
    vreg_set_voltage(VREG_VOLTAGE_1_30);

    // Set the frequency in kHz (e.g., 300,000 kHz = 300 MHz)
    // 'true' means it will wait for the clock to stabilize
    set_sys_clock_khz(300000, true);

    int rc = pico_led_init(); // Initialize the LED GPIO

    hard_assert(rc == PICO_OK); // Ensure LED initialization was successful

    stdio_init_all(); // Initialize all standard I/O (UART, USB, etc.)

    // Explicitly override the baud rate for UART0 to 2M
    uart_set_baudrate(uart0, 921600);

    // Negative delay means "run every X ms regardless of how long callback took"
    // Positive delay means "wait X ms after callback finishes"
    add_repeating_timer_ms(100, repeating_timer_callback, NULL, &timer);

    // Give UART a moment to stabilize
    sleep_ms(50);

    mutex_enter_blocking(&printf_mutex); // Mutex is not strictly necessary here since Core 1 hasn't started yet, but it's good practice to be consistent
    printf("\n\n\nCore 0: Booting...\n");
    printf("Running on %s at %d MHz\n",
#ifdef __riscv
           "RISC-V",
#else
           "Arm Cortex-M33",
#endif
           frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS) / 1000);

    mutex_exit(&printf_mutex);

    // Launch core1_entry function on Core 1
    multicore_launch_core1(core1_entry);

    uint32_t now, loop_cnt = 0, next_blink = LED_DELAY_MS, next_tick = 1000;

    while (true) {

        now = time_ms_32();

        if (now >= next_tick) {
            mutex_enter_blocking(&printf_mutex); // Ensure we've got exclusive access to printf
            printf("Core 0 tick %lu (loop = %lu)\n", now, loop_cnt);
            mutex_exit(&printf_mutex); // Release the mutex so Core 1 can print
            loop_cnt = 0;
            next_tick = now + 1000;
        }

        ++loop_cnt;

        // Give the memory bus and Core 1 a chance to breathe
        tight_loop_contents();
    }
}

// vim: ts=4 et nowrap
