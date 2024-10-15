import argparse
from pathlib import Path

import numpy as np
from PIL import Image

from util import load_JASC


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("name")
    parser.add_argument("-o", dest="output", type=Path, required=True)
    parser.add_argument("--palette", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=1)
    # parser.add_argument("--offset", help="offset into source image, e.g. '32,64'")

    args = parser.parse_args()

    # if args.offset:
    #     offset_x, offset_y = (int(val) for val in args.offset.split(","))
    # else:
    #     offset_x, offset_y = 0, 0

    with open(args.palette) as f:
        pal = load_JASC(f)

    img = Image.open(args.input).convert("RGBA")

    ANIMATIONS = {
        "idle": (0, 0),
        "walk": (64, 32),
        "attk": (0, 96),
        "dead": (64, 96),
    }

    sprite_animations = {}

    for anim_name, (offset_x, offset_y) in ANIMATIONS.items():
        data = []

        for i in range(args.frames):
            IMG_W, IMG_H = 32, 32
            frame = img.crop((offset_x + i * IMG_W,
                              offset_y,
                              offset_x + i * IMG_W + IMG_W,
                              offset_y + IMG_H))

            # map to palette

            def match_color(palette, rgb):
                # TODO: should use a better algorithm than RGB distance
                dist_index_list = [(np.linalg.norm(palette[i] - rgb), i) for i in range(len(palette))]

                # find index of closest color
                dist, index = sorted(dist_index_list)[0]

                return index

            frame_data = []

            for x in range(frame.width):
                # find first non-transparent pixel
                for y in range(frame.height):
                    r, g, b, a = frame.getpixel((x, y))

                    if a >= 128:
                        break
                else:
                    frame_data.append(None)
                    continue

                y_start = y
                col = dict(pixels=[])

                for y in range(y_start, frame.height):
                    r, g, b, a = frame.getpixel((x, y))

                    if a >= 128:
                        # Note: this should be done ideally once for each color and not for each pixel
                        index = match_color(pal, (r, g, b))
                    else:
                        index = 0

                    col["pixels"].append(index)

                    if a >= 128:
                        y_end = y

                count = y_end + 1 - y_start
                col["pixels"] = col["pixels"][0:count]
                frame_data.append(col)

                col["span"] = (y_start, count)
                del y_end

            data.append(frame_data)

        sprite_animations[anim_name] = data

    # generate C header

    n = args.name

    with open(args.output, "wt") as f:
        f.write("#include <game_structs.hpp>\n\n")

        for anim_name, data in sprite_animations.items():
            for frame_num, frame_data in enumerate(data):
                prefix = f"{n}_{anim_name}_{frame_num}_"
                for i, col in enumerate(frame_data):
                    if col is not None:
                        f.write(f"static const uint8_t {prefix}data_{i}[] = {{\n")
                        f.write(f"    {','.join(str(x) for x in col['pixels'])}\n")
                        f.write("};\n")

                f.write(f"\n")
                f.write(f"static const SpriteSpan {prefix}spans[{frame.width}] = {{\n")

                for i, col in enumerate(frame_data):
                    f.write(f"    // col {i}\n")

                    if col is not None:
                        f.write(f"    {{{col['span'][0]}, {col['span'][1]}, {prefix}data_{i}}},\n")
                    else:
                        f.write(f"    {{0, 0, 0}},\n")

                f.write("};\n\n")
                # f.write(f"static const AnimFrame {n} = {{ .spans = {prefix}spans }};\n")

            f.write(f"static const SpriteFrame {n}_{anim_name}_frames[] = {{\n")
            for frame_num in range(args.frames):
                prefix = f"{n}_{anim_name}_{frame_num}_"
                f.write(f"    {{ .spans = {prefix}spans }},\n")
            f.write("};\n\n")

        f.write(f"static const SpriteAnim {n}_anims[] = {{\n")

        # pray that this will match engine order
        for anim_name in sprite_animations.keys():
            f.write(f"    {{ .frames = {n}_{anim_name}_frames, .num_frames = {args.frames} }},\n")
        f.write("};\n\n")

        f.write(f"static const Sprite {n} = {{ .anims = {n}_anims, }};\n")
