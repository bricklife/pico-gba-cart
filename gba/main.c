#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <seven/prelude.h>
#include <seven/hw/video.h>
#include <seven/hw/video/bg_bitmap.h>
#include <seven/hw/waitstate.h>

int main() {
    // values used by official software (according to gbatek)
    // (default would be 4/2)
    //REG_WAITCNT = WAIT_ROM_N_3 | WAIT_ROM_S_1 | WAIT_PREFETCH_ENABLE;
    REG_WAITCNT = WAIT_PREFETCH_ENABLE;

    REG_DISPCNT = VIDEO_MODE_BITMAP | VIDEO_BG2_ENABLE;

    for(int y = 0; y < 16; y++) {
        for(int x = 0; x < 240; x++) {
            int v = x / 8;
            MODE3_FRAME[y][x] = v;
            MODE3_FRAME[y + 16][x] = v << 5;
            MODE3_FRAME[y + 32][x] = v << 10;

            MODE3_FRAME[y + 48][x] = v | v << 5;
            MODE3_FRAME[y + 64][x] = v | v << 10;
            MODE3_FRAME[y + 80][x] = v << 5 | v << 10;

            MODE3_FRAME[y + 96][x] = v | v << 5 | v << 10;
        }
    }

    // enable irq/vblank
    irqInitDefault();
    irqEnable(IRQ_VBLANK);
    REG_DISPSTAT = LCD_VBLANK_IRQ_ENABLE;

    while(true) {
        biosVBlankIntrWait();
    }
}
