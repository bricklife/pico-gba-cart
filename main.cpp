#include <cstdio>

#include "pico/stdlib.h"

#include "hardware/sync.h"
#include "hardware/vreg.h"

#include "gbacart.h"

static uint16_t fb[240 * 160];

// run update code from RAM to avoid interrupt delays from XIP cache misses
// ... at least, I think that's what happens...
void __no_inline_not_in_flash_func(update)() {
    static int cursor_x = 0, cursor_y = 0;
    auto cart_api = gbacart_get_api();

    if(cart_api->vblank_flag) {
        auto button_state = ~cart_api->buttons;
    
        if(button_state & (1 << 5)) // left
            cursor_x = cursor_x == 0 ? 239 : cursor_x - 1;
        else if(button_state & (1 << 4)) // right
            cursor_x = cursor_x == 239 ? 0 : cursor_x + 1;

        if(button_state & (1 << 6)) // up
            cursor_y = cursor_y == 0 ? 159 : cursor_y - 1;
        else if(button_state & (1 << 7)) // down
            cursor_y = cursor_y == 159 ? 0 : cursor_y + 1;

        for(int y = 0; y < 160; y++) {
            for(int x = 0; x < 240; x++) {
                fb[x + y * 240] = (x == cursor_x || y == cursor_y) ? 0xFFFF : 0;
            }
        }

        cart_api->vblank_flag = 0;
        cart_api->fb_addr = gbacart_to_gba_addr(fb);
    }
}

int main() {
    set_sys_clock_khz(250000, true);
    // lowest stable?
    //set_sys_clock_khz(195000, true);

    //stdio_init_all();

    gbacart_init();

    gbacart_start(true);

    // draw something to framebuffer
    while(true) {
        update();
        __wfe();
    }

    return 0;
}
