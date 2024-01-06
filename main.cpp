#include <cstdio>

#include "pico/stdlib.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"

#include "gba-cart.pio.h"

alignas(4) extern char _binary_gba_rom_gba_start[];
static auto rom_ptr = _binary_gba_rom_gba_start;

static constexpr int cs_pin = 26;
static constexpr int rd_pin = 25;
static constexpr int wr_pin = 24;

static uint32_t rom_addr;

// SMs and DMA channels accessed in IRQ handlers should be pre-allocated constants
static constexpr int rom_cs_sm = 0, rom_rd_sm = 1;
static int rom_wr_sm;
static const PIO gba_cart_pio = pio0;

static constexpr int rom_read_dma_channel = 0, rom_write_dma_channel = 1;
static int rom_addr_dma_channel, rom_addr_sniff_dma_channel;

static uint16_t fb[240 * 160];

static void __not_in_flash_func(dma_irq_handler)() {
    dma_sniffer_set_data_accumulator((uint32_t)rom_ptr);
    dma_channel_acknowledge_irq0(rom_addr_dma_channel);

    // also set up the write channel (less time sensitive)
    dma_channel_set_write_addr(rom_write_dma_channel, rom_ptr + rom_addr, true);
}

static void __not_in_flash_func(pio_irq0_handler)() {
    // abort DMA and clear FIFO

    // this saves a couple of instructions
    static const auto dma_abort = &dma_hw->abort;
    *dma_abort = 1u << rom_read_dma_channel | 1u << rom_write_dma_channel;

    // assume TX FIFO was full so no transfers were left in flight
    //while(dma_channel_hw_addr(rom_read_dma_channel)->ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS);
    
    pio_interrupt_clear(gba_cart_pio, 0);
    pio_sm_clear_fifos(gba_cart_pio, rom_rd_sm);

    // GBA might have requested a bad address
    // happens during boot
    hw_set_bits(&dma_hw->ch[rom_read_dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_READ_ERROR_BITS);
}

static void pio_init() {
    pio_claim_sm_mask(gba_cart_pio, 1 << rom_cs_sm | 1 << rom_rd_sm);

    rom_wr_sm = pio_claim_unused_sm(gba_cart_pio, true);

    // cs
    auto offset = pio_add_program(gba_cart_pio, &gba_rom_cs_program);
    auto cfg = gba_rom_cs_program_get_default_config(offset);

    sm_config_set_in_shift(&cfg, false, true, 25);

    pio_sm_init(gba_cart_pio, rom_cs_sm, offset, &cfg);

    // rd
    offset = pio_add_program(gba_cart_pio, &gba_rom_rd_program);
    cfg = gba_rom_rd_program_get_default_config(offset);

    sm_config_set_out_pins(&cfg, 0, 16);
    sm_config_set_out_shift(&cfg, false, false, 16);

    pio_sm_init(gba_cart_pio, rom_rd_sm, offset, &cfg);

    // wr
    offset = pio_add_program(gba_cart_pio, &gba_rom_wr_program);
    cfg = gba_rom_wr_program_get_default_config(offset);

    sm_config_set_in_shift(&cfg, false, true, 16);

    pio_sm_init(gba_cart_pio, rom_wr_sm, offset, &cfg);

    // init all io
    pio_sm_set_pins(gba_cart_pio, rom_cs_sm, 0);
    pio_sm_set_consecutive_pindirs(gba_cart_pio, rom_cs_sm, 0, 32, false);

    for(int i = 0; i < 30; i++)
        pio_gpio_init(gba_cart_pio, i);

    // bypass synchroniser
    hw_set_bits(&gba_cart_pio->input_sync_bypass, 0x3FFFFFF);
}

static void dma_init() {
    dma_claim_mask(1 << rom_read_dma_channel | 1 << rom_write_dma_channel);
    rom_addr_dma_channel = dma_claim_unused_channel(true);
    rom_addr_sniff_dma_channel = dma_claim_unused_channel(true);

    // read data
    auto config = dma_channel_get_default_config(rom_read_dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_dreq(&config, pio_get_dreq(gba_cart_pio, rom_rd_sm, true));
    dma_channel_configure(rom_read_dma_channel, &config, &gba_cart_pio->txf[rom_rd_sm], rom_ptr, 0x10000, false);

    // write data
    config = dma_channel_get_default_config(rom_write_dma_channel);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_dreq(&config, pio_get_dreq(gba_cart_pio, rom_wr_sm, false));
    dma_channel_configure(rom_write_dma_channel, &config, rom_ptr, &gba_cart_pio->rxf[rom_wr_sm], 0x10000, false);

    // read address through sniffer to add base ptr
    // destination also used for write setup 
    config = dma_channel_get_default_config(rom_addr_dma_channel);
    channel_config_set_dreq(&config, pio_get_dreq(gba_cart_pio, rom_cs_sm, false));
    channel_config_set_read_increment(&config, false);

    // chain to sniffer read
    channel_config_set_chain_to(&config, rom_addr_sniff_dma_channel);

    dma_channel_configure(rom_addr_dma_channel, &config, &rom_addr, &gba_cart_pio->rxf[rom_cs_sm], 1, false);

    // transfer sniffed address to trigger read channel
    config = dma_channel_get_default_config(rom_addr_sniff_dma_channel);
    channel_config_set_read_increment(&config, false);

    // chain to addr read (keep the address reads going forever)
    channel_config_set_chain_to(&config, rom_addr_dma_channel);

    dma_channel_configure(rom_addr_sniff_dma_channel, &config, &dma_hw->ch[rom_read_dma_channel].al3_read_addr_trig, &dma_hw->sniff_data, 1, false);

    // sniffer
    dma_sniffer_enable(rom_addr_dma_channel, 0xF/*addition*/, true);
    dma_sniffer_set_data_accumulator((uint32_t)rom_ptr);

    // irq to reset sniff
    dma_channel_set_irq0_enabled(rom_addr_dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

inline uint32_t to_gba_addr(void *ptr) {
    // this assumes the "ROM" data is before anything we want to pass through
    return reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(rom_ptr) + 0x8000000;
}

int main() {
    set_sys_clock_khz(250000, true);
    // lowest stable?
    //set_sys_clock_khz(195000, true);

    //stdio_init_all();

    pio_init();
    dma_init();

    // cs high
    pio_set_irq0_source_enabled(gba_cart_pio, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq0_handler);
    irq_set_enabled(PIO0_IRQ_0, true);

    // patch framebuffer addr into "ROM" data
    *reinterpret_cast<uint32_t *>(rom_ptr + 0xC0) = to_gba_addr(fb);

    // wait for CS to go high (GBA turned on)
    while(!gpio_get(cs_pin));
    sleep_ms(1);

    dma_channel_start(rom_addr_dma_channel);
    pio_set_sm_mask_enabled(gba_cart_pio, 1 << rom_cs_sm | 1 << rom_rd_sm | 1 << rom_wr_sm, true);

    // draw something to framebuffer
    auto vblank_flag = reinterpret_cast<volatile uint16_t *>(rom_ptr + 0xC4);

    int t = 0;
    while(true) {
        if(*vblank_flag) {
            for(int y = 0; y < 160; y++) {
                for(int x = 0; x < 240; x++) {
                    fb[x + y * 240] = (x == t || y == t) ? 0xFFFF : 0;
                }
            }

            t = (t + 1) % 240;
            *vblank_flag = 0;
        }
        __wfe();
    }

    return 0;
}
