#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <seven/prelude.h>
#include <seven/hw/bios/memory.h>
#include <seven/hw/dma.h>
#include <seven/hw/video.h>
#include <seven/hw/video/bg_bitmap.h>
#include <seven/hw/video/bg_transform.h>
#include <seven/hw/waitstate.h>

struct CartAPI {
    volatile uint32_t fb_addr;
    volatile uint16_t vblank_flag;
    volatile uint16_t buttons;
};

int main() {
    // values used by official software (according to gbatek)
    // (default would be 4/2)
    //REG_WAITCNT = WAIT_ROM_N_3 | WAIT_ROM_S_1 | WAIT_PREFETCH_ENABLE;
    REG_WAITCNT = WAIT_ROM_S_1 | WAIT_PREFETCH_ENABLE;

    REG_DISPCNT = VIDEO_MODE_BITMAP | VIDEO_BG2_ENABLE;

    // enable irq/vblank
    irqInitDefault();
    irqEnable(IRQ_VBLANK);
    REG_DISPSTAT = LCD_VBLANK_IRQ_ENABLE;

    struct CartAPI *cart_api = (struct CartAPI *)(MEM_ROM + 0xC0);

    while(true) {
        cart_api->buttons = REG_KEYINPUT;

        uint16_t *fb_ptr = (uint16_t *)cart_api->fb_addr;
        if(fb_ptr) {
            struct DMA screen_dma = {fb_ptr, MODE3_FRAME, 240 * 160, DMA_16BIT | DMA_ENABLE};
            dmaSet(3, screen_dma);
            cart_api->fb_addr = 0;
        }

        cart_api->vblank_flag = 1;

        biosVBlankIntrWait();
    }
}
