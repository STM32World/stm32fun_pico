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
 * version, the LED is toggled using a hardware PWM slice, which allows for precise timing and frees up the
 * main loop to focus on printing messages.  The code also demonstrates how to use mutexes to synchronize access to
 * shared resources (like printf) between the two cores.
 *
 */

// Include necessary headers from the Pico SDK
#include "hardware/gpio.h"  // For GPIO control
#include "hardware/pwm.h"   // For Hardware PWM control
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
 * @brief Flexible PWM setup for Pin 33.
 * Achieves frequencies as low as 0.5Hz by using the clkdiv mode.
 */
void pwm_setup(float freq) {
    const uint pin = 33;
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config config = pwm_get_default_config();

    /**
     * For 0.5Hz at 150MHz:
     * Total division needed = 150,000,000 / 0.5 = 300,000,000.
     * We can use clkdiv up to 255.9.
     * Let's set clkdiv to 250. 150MHz / 250 = 600,000Hz.
     * We still need to divide by 1,200,000 to get 0.5Hz.
     * Wrap is 16-bit (max 65535).
     * By enabling Phase Correct mode, the period is 2 * wrap.
     * To reach 0.5Hz, we'd need wrap = 600,000.
     * * REAL SOLUTION: We must slow the input clock to the slice.
     * On RP2350, we can use the 'divmode'.
     */

    // Set divider to maximum
    float div = 250.0f;
    pwm_config_set_clkdiv(&config, div);

    // If freq is low, we enable phase correct to double the period
    bool phase_correct = (freq < 5.0f);
    pwm_config_set_phase_correct(&config, phase_correct);

    // Calculate wrap based on the 150MHz clock, our divider, and phase correction
    // wrap = sys_clk / (div * (phase ? 2 : 1) * freq)
    float denom = div * (phase_correct ? 2.0f : 1.0f) * freq;
    uint32_t wrap = (uint32_t)(150000000.0f / denom);

    // If wrap still exceeds 16-bit, we have to cap it.
    // Note: To truly hit 0.5Hz at 150MHz, PIO is actually the better tool,
    // but this will get you the slowest possible PWM blink.
    if (wrap > 65535)
        wrap = 65535;
    if (wrap < 1)
        wrap = 1;

    pwm_config_set_wrap(&config, (uint16_t)wrap);
    pwm_init(slice_num, &config, true);

    pwm_set_gpio_level(pin, wrap / 2);
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

    int rc = pico_led_init(); // Initialize the LED GPIO

    hard_assert(rc == PICO_OK); // Ensure LED initialization was successful

    stdio_init_all(); // Initialize all standard I/O (UART, USB, etc.)

    // Explicitly override the baud rate for UART0 to 2M
    uart_set_baudrate(uart0, 921600);

    // Give UART a moment to stabilize
    sleep_ms(50);

    // Setup hardware PWM at 0.5Hz (or as close as 16-bit allows)
    pwm_setup(0.5f);

    mutex_enter_blocking(&printf_mutex);
    printf("\n\n\nCore 0: Booting (Hardware PWM Active)...\n");
    mutex_exit(&printf_mutex);

    // Launch core1_entry function on Core 1
    multicore_launch_core1(core1_entry);

    uint32_t now, loop_cnt = 0, next_blink = LED_DELAY_MS, next_tick = 1000;

    while (true) {

        now = time_ms_32();

        if (now >= next_tick) {
            mutex_enter_blocking(&printf_mutex); // Ensure we've got exclusive access to printf
            printf("Core 0 tick %lu (loop = %lu)\n", (unsigned long)now, (unsigned long)loop_cnt);
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