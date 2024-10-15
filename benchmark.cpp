#include <tonc.h>

#include "mgba/mgba.h"

static u16 benchmark_id;

void bmarkInit() {
    irq_init(nullptr);
    irq_enable(II_VBLANK);
    VBlankIntrWait();
    irq_disable(II_VBLANK);

    // test cookie
    if (*(u16*)0x0203FFFC == 0xCAFE) {
        benchmark_id = *(u16*)0x0203FFFE;

        mgba_open();
    }
}

bool bmarkIsBenchmark() {
    return benchmark_id != 0;
}
