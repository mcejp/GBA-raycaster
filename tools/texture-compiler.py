import argparse
from pathlib import Path

import numpy as np
from PIL import Image

from util import load_JASC, match_image_to_palette


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("name")
    parser.add_argument("-o", dest="output", type=Path, required=True)
    parser.add_argument("--palette", type=Path, required=True)

    args = parser.parse_args()

    # if args.offset:
    #     offset_x, offset_y = (int(val) for val in args.offset.split(","))
    # else:
    #     offset_x, offset_y = 0, 0

    with open(args.palette) as f:
        pal = load_JASC(f)

    img = Image.open(args.input).convert("RGB")

    # map to palette

    img = match_image_to_palette(pal, img)

    def match_color(palette, rgb):
        # TODO: should use a better algorithm than RGB distance
        dist_index_list = [(np.linalg.norm(palette[i] - rgb), i) for i in range(len(palette))]

        # find index of closest color
        dist, index = sorted(dist_index_list)[0]

        return index

    TEXSZ = 32

    textures = []

    for row in range(img.height // TEXSZ):
        for col in range(img.width // TEXSZ):
            texture = img.crop((col * TEXSZ,
                                row * TEXSZ,
                                (col + 1) * TEXSZ,
                                (row + 1) * TEXSZ))

            texture_data = []

            for x in range(texture.width):
                for y in range(texture.height):
                    index = texture.getpixel((x, y))
                    texture_data.append(index)

            textures.append(texture_data)

    # generate C header

    n = args.name

    with open(args.output, "wt") as f:
        f.write("#include <game_structs.hpp>\n\n")

        for i, texture_data in enumerate(textures):
            prefix = f"{n}_{i}_"

            f.write(f'[[gnu::section(".ewram.{n}_data")]]\n')
            f.write(f"static const uint8_t {prefix}data[] = {{\n")
            f.write(f"    {','.join(str(x) for x in texture_data)}\n")
            f.write("};\n\n")

        f.write(f'[[gnu::section(".ewram.{n}_textures")]]\n')
        f.write(f"static const uint8_t* {n}_textures[] = {{\n")
        for i in range(len(textures)):
            prefix = f"{n}_{i}_"
            f.write(f"    {prefix}data,\n")
        f.write("};\n\n")
