import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

from util import load_JASC


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", dest="output", type=Path, required=True)
    parser.add_argument("--scale", type=int, required=True)

    args = parser.parse_args()

    with open(args.input) as f:
        palette = load_JASC(f)

    cols = min(len(palette), 16)            # max 16 columns
    rows = math.ceil(len(palette) / cols)

    S = args.scale
    img = Image.new("RGB", (cols * S, rows * S))
    draw = ImageDraw.Draw(img)

    for i, rgb in enumerate(palette):
        yy = i // cols
        xx = i % cols
        draw.rectangle((xx * S, yy * S, (xx + 1) * S, (yy + 1) * S), fill=tuple(rgb))

    img.save(args.output)
