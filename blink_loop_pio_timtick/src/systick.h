#ifndef SYSTICK_H
#define SYSTICK_H

#include "hardware/pio.h"
#include "hardware/structs/dma.h"
#include "pico/platform.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTICK_MAX_TICKS 0x0FFFFFFF

// Direct hardware pointer to the transfer count register.
// Declared as extern so both cores can access it instantly without pointer math.
extern volatile uint32_t *systick_trans_count_reg;

void systick_init(PIO pio);

/**
 * @brief Ultra-optimized inline getter for the uptime ticker.
 * Compiles down to just 3-4 raw assembly instructions.
 */
static inline uint32_t __not_in_flash_func(get_millis)(void) {
    // 1. Read directly from the calculated hardware address (1 cycle)
    // 2. Mask the upper 4 bits away (1 cycle)
    // 3. Subtract from max ticks (1 cycle)
    return SYSTICK_MAX_TICKS - (*systick_trans_count_reg & 0x0FFFFFFF);
}

#ifdef __cplusplus
}
#endif

#endif // SYSTICK_H