#include "../game.hpp"

[[gnu::section(".iwram.kernelScale2Col32")]]
[[gnu::target("arm")]]
void kernelScale2Col32(uint8_t const* column1, uint8_t const* column2, uint16_t* fb, int h) {
    static_assert(TEX_HEIGHT == 32);

    int v_offset_subpix = 0;        // 2**16 ~ 1 px

    // TODO: optimize division
    int dv_dy = TEX_HEIGHT * 0x10000 / h;  // v-coordinate increment per scanline (2**16 ~ 1px)

    if (h < TEX_HEIGHT) {
        // "short" case (source image minified)

        for (int yy = 0; yy < h; yy++) {
            // texture v-coordinate (top to bottom)
            auto v = (v_offset_subpix + yy * dv_dy) / 0x10000;

            uint8_t col = column1[v];
            uint8_t col2 = column2[v];

            // TODO: are there gains to be made by using 16-/32-bit loads of span data?
            *fb = col | (col2 << 8);
            fb += M4_WIDTH / 2;
        }
    }
    else {
        // "tall" case (source image magnified)

        // texture v-coordinate (top to bottom) in integer pixels
        auto v = v_offset_subpix / 0x10000;

        // texture v-coordinate subpixel, 2**16 ~ 1 px
        int v_frac_part = v_offset_subpix % 0x10000;

        // TODO: reduce y_bot if spans end earlier?
        for (int yy = 0; yy < h; ) {
            uint8_t col = column1[v];
            uint8_t col2 = column2[v];

            // this is the critical path and would likely benefit from partial unrolling
            for (; v_frac_part < 0x10000; v_frac_part += dv_dy) {
                *fb = col | (col2 << 8);
                fb += M4_WIDTH / 2;
                yy++;

                if (yy >= h) {
                    break;
                }
            }

            v_frac_part -= 0x10000;
            v++;
        }
    }
}