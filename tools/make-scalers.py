import argparse
import math
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument("-o", dest="output", type=Path, required=True)

args = parser.parse_args()

f = open(args.output, "wt")

f.write("""#include "../game.hpp"\n\n""")

# 16px to 1..16px downscaler

f.write("""\
[[gnu::section(".iwram.kernelScale2Col16")]]
[[gnu::target("arm")]]
void kernelScale2Col16_1to16(uint8_t const* column1, uint8_t const* column2, uint16_t* fb, int h) {
    switch (h) {
""")

COL_H = 16

for h in range(16 + 1):
    """
    Approach:
    - go over Screen Y
    - track where we are in V (Image Y)
    - at each point, select the V that should be shown
    - adjust image ptr to reach it
    - blit
    
    NOTE: For cases like 4:1, 2:1, 1:1 downscales, we don't need to do full unrolling
    """
    f.write(f"    case {h}:\n")
    for y in range(h):
        v = int(round((y + 0.49) / h * COL_H))

        f.write(f"""\
        *fb = column1[{v}] | (column2[{v}] << 8);
        fb += M4_WIDTH / 2;
""")
    f.write("        break;\n")

f.write("""
    }
}
""")

# 16/8/4px to 16..32px upscalers

"""
For 16px generates code like this:

 30023a4:	e5d1c002 	ldrb	ip, [r1, #2]
 30023a8:	e5d03002 	ldrb	r3, [r0, #2]
 30023ac:	e183340c 	orr	r3, r3, ip, lsl #8
 30023b0:	e282ce1e 	add	ip, r2, #480	; 0x1e0
 30023b4:	e1cc30b0 	strh	r3, [ip]
 30023b8:	e282ce2d 	add	ip, r2, #720	; 0x2d0
 30023bc:	e1cc30b0 	strh	r3, [ip]
"""

for COL_H in [16, 8, 4]:
    f.write(f"""\
[[gnu::section(".iwram.kernelScale2Col16")]]
[[gnu::target("arm")]]
void kernelScale2Col{COL_H}_16to32(uint8_t const* column1, uint8_t const* column2, uint16_t* fb, int h) {{
    int val;
    switch (h) {{\n""")

    for h in range(16, 32 + 1):
        f.write(f"    case {h}:\n")
        v_last = None
        for y in range(h):
            v = int(math.floor((y + 0.5) / h * COL_H))

            if v_last is None or v_last != v:
                f.write(f"""        val = column1[{v}] | (column2[{v}] << 8);\n""")
                v_last = v

            f.write(f"""\
        *fb = val;
        fb += M4_WIDTH / 2;\n""")

        f.write("        break;\n")

    f.write("""
    }
}\n\n""")


### kernel

f.write("""\
[[gnu::section(".iwram.kernelScale2Col32")]]
[[gnu::target("arm")]]
void kernelScale2Col32(uint8_t const* column1, uint8_t const* column2, uint16_t* fb, int h) {
    if (h <= 32) {
        int h1 = h / 2;
        kernelScale2Col16_1to16(column1, column2, fb, h1);
        fb += h1 * M4_WIDTH / 2;
        kernelScale2Col16_1to16(column1 + 16, column2 + 16, fb, h - h1);
    }\n""")
for out_h, tile_size in [(64, 16), (128, 8), (256, 4)]:
    steps = 32 // tile_size
    f.write(f"""\
    else if (h <= {out_h}) {{
        int yy = 0;
        for (int i = 0; i < {steps}; i++) {{
            int step = h * (i + 1) / {steps} - yy;
            kernelScale2Col{tile_size}_16to32(column1, column2, fb, step);
            column1 += {tile_size};
            column2 += {tile_size};
            fb += step * M4_WIDTH / 2;
            yy += step;
        }}
    }}\n""")
f.write("""\
}

""")
f.close()