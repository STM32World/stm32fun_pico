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
#include "blink.pio.h"
#include "hardware/gpio.h" // For GPIO control
#include "hardware/pio.h"
#include "pico/multicore.h" // For multicore support
#include "pico/mutex.h"     // For mutexes
#include "pico/stdlib.h"    // For sleep and stdio initialization

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
 * @brief Callback function for the repeating timer.
 * This function is called every 500ms as set up in main().
 */
bool repeating_timer_callback(struct repeating_timer *t) {
    pico_toggle_led();
    return true;
}

/**
 * @brief Helper to convert frequency to PIO loop delay.
 */
uint32_t freq_to_pio_delay(float freq) {
    if (freq < 0.1f)
        freq = 0.1f;
    // 150MHz / 150 div = 1MHz. 2 cycles per loop.
    return (uint32_t)(500000.0f / freq);
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
            printf("Core 1 tick %lu (loop = %lu)\n", (unsigned long)now, (unsigned long)loop_cnt);
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

    // struct repeating_timer timer;

    int rc = pico_led_init(); // Initialize the LED GPIO

    hard_assert(rc == PICO_OK); // Ensure LED initialization was successful

    stdio_init_all(); // Initialize all standard I/O (UART, USB, etc.)

    // Explicitly override the baud rate for UART0 to 2M
    uart_set_baudrate(uart0, 921600);

    // --- PIO2 SETUP ---
    PIO pio = pio2;
    pio_set_gpio_base(pio, 16);

    uint offset = pio_add_program(pio, &blink_program);
    uint sm = pio_claim_unused_sm(pio, true);

    blink_program_init(pio, sm, offset, PICO_DEFAULT_LED_PIN, 150.0f);

    // Starting at 0.5 Hz
    float current_freq = 0.5f;

    pio_sm_set_enabled(pio, sm, true);
    // Initial push
    pio_sm_put_blocking(pio, sm, freq_to_pio_delay(current_freq));

    // Give UART a moment to stabilize
    sleep_ms(50);

    mutex_enter_blocking(&printf_mutex);
    printf("\n\n\nCore 0: Booting...\n");
    mutex_exit(&printf_mutex);

    // Launch core1_entry function on Core 1
    multicore_launch_core1(core1_entry);

    uint32_t now, loop_cnt = 0, next_tick = 1000;
    uint32_t next_freq_update = 2000;

    while (true) {

        now = time_ms_32();

        // 1. Frequency Logic (Cycle 0.5 Hz -> 10 Hz)
        if (now >= next_freq_update) {
            if (current_freq < 1.0f)
                current_freq = 1.0f;
            else if (current_freq < 2.0f)
                current_freq = 2.0f;
            else if (current_freq < 5.0f)
                current_freq = 5.0f;
            else if (current_freq < 10.0f)
                current_freq = 10.0f;
            else
                current_freq = 0.5f;

            if (!pio_sm_is_tx_fifo_full(pio, sm)) {
                pio_sm_put(pio, sm, freq_to_pio_delay(current_freq));
            }

            next_freq_update = now + 3000;
        }

        // 2. Tick Logic (Every 1 second)
        if (now >= next_tick) {
            mutex_enter_blocking(&printf_mutex);
            uint32_t current_pc = pio_sm_get_pc(pio, sm);
            printf("Core 0 tick %lu (loop = %lu) | Freq: %.1f Hz | PIO PC: %u (Offset: %u)\n", (unsigned long)now, (unsigned long)loop_cnt, current_freq, (unsigned int)current_pc, (unsigned int)offset);
            mutex_exit(&printf_mutex);
            loop_cnt = 0;
            next_tick = now + 1000;
        }

        ++loop_cnt;

        // Give the memory bus and Core 1 a chance to breathe
        tight_loop_contents();
    }
}

// vim: ts=4 et nowrap