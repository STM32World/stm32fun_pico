#include "systick.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "systick.pio.h"

static int main_dma_chan = -1;
static volatile uint32_t dummy_destination_buffer = 0;

// Allocate space for the ultra-fast global register pointer
volatile uint32_t *systick_trans_count_reg = NULL;

static void __not_in_flash_func(dma_systick_isr)(void) {
    if (dma_hw->ints0 & (1u << main_dma_chan)) {
        dma_hw->intf0 = (1u << main_dma_chan);
        dma_hw->ch[main_dma_chan].al1_transfer_count_trig = SYSTICK_MAX_TICKS;
    }
}

void systick_init(PIO pio) {
    // 1. Claim and Setup PIO
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &systick_program);
    systick_program_init(pio, sm, offset);

    // 2. Claim and Setup DMA
    main_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(main_dma_chan);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    pio_sm_clear_fifos(pio, sm);

    // CRITICAL OPTIMIZATION: Calculate the exact hardware register address once right now.
    // This assigns the direct memory mapped IO pointer so get_millis() skips all array math.
    systick_trans_count_reg = &dma_hw->ch[main_dma_chan].transfer_count;

    dma_channel_configure(
        main_dma_chan,
        &c,
        (void *)&dummy_destination_buffer,
        (const void *)&pio->rxf[sm],
        SYSTICK_MAX_TICKS,
        false);

    // 3. Configure Interrupts
    dma_channel_set_irq0_enabled(main_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_systick_isr);
    irq_set_enabled(DMA_IRQ_0, true);

    // Start the DMA channel
    dma_channel_start(main_dma_chan);
}
