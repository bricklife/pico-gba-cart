#include <cmath>
#include <cstdio>

#include "pico/stdlib.h"

#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"

#include "gbacart.h"

int main() {
    set_sys_clock_khz(250000, true);
    // lowest stable?
    //set_sys_clock_khz(195000, true);

    //stdio_init_all();

    gbacart_init();

    gbacart_start(true);

    while(true) {
        __wfe();
    }

    return 0;
}
