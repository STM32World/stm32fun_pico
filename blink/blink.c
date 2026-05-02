/**
 * Copyright (c) 2026 STM32World <lth@stm32world.com>
 */

#include "hardware/gpio.h"
#include "hardware/regs/sio.h" // Add this explicitly
#include "pico/multicore.h"    // Required header
#include "pico/stdlib.h"
#include <stdio.h>

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

// Perform initialisation
int pico_led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

/**
 * @brief Returns a 32-bit millisecond counter.
 * Equivalent to STM32's uwTick.
 * * This uses the 64-bit hardware timer (1MHz) and scales to ms.
 * Rollover occurs every ~49.7 days.
 */
static inline uint32_t time_ms_32(void) {
    // get_absolute_time() returns a 64-bit timestamp in microseconds.
    // to_ms_since_boot() handles the division by 1000.
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

void pico_toggle_led() {
    // SIO_GPIO_HI_XOR_OFFSET is the atomic toggle for pins 32-47
    // sio_hw->gpio_hi_togl = (1u << (PICO_DEFAULT_LED_PIN - 32));
    gpio_xor_mask64(((uint64_t)1 << PICO_DEFAULT_LED_PIN));
}

// This function will run on Core 1
void core1_entry() {
    uint32_t now, next_tick = 1500;
    
    while (1) {

        now = time_ms_32();

        if (now >= next_tick) {
            printf("Core 1 tick %lu\n", now / 1000);
            next_tick = now + 1500;
        }
        
        // Give the memory bus and Core 0 a chance to breathe
        tight_loop_contents(); 
    }
}

int main() {
    int rc = pico_led_init();

    hard_assert(rc == PICO_OK);

    stdio_init_all();

    // Explicitly override the baud rate for UART0 to 2M
    uart_set_baudrate(uart0, 2000000);

    // Give UART a moment to stabilize
    sleep_ms(50); 
    printf("Core 0: Booting...\n");

    // Launch core1_entry function on Core 1
    multicore_launch_core1(core1_entry);

    uint32_t now, next_blink = 500, next_tick = 1000;

    while (true) {

        now = time_ms_32();

        if (now > next_blink) {
            pico_toggle_led();
            next_blink = now + 500;
        }

        if (now >= next_tick) {
            printf("Core 0 tick %lu\n", now / 1000);
            next_tick = now + 1000;
        }
    }
}
