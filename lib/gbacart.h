#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// using 16-bit values for bools as the min access size on the cart bus is 16-bit
struct CartAPI {
    volatile uint32_t fb_addr;
    volatile uint16_t vblank_flag; // set to 1 by GBA at the start fo vblank
    volatile uint16_t fb_pa, fb_pd; // scale params
    volatile uint16_t fb_height;

    volatile uint16_t buttons; // A, B, Select, Start, Right, Left, Up, Down, R, L (0 == pressed)
};

void gbacart_init();
void gbacart_start(bool wait_power);

struct CartAPI *gbacart_get_api();
uint32_t gbacart_to_gba_addr(void *ptr);

#ifdef __cplusplus
}
#endif