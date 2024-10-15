import argparse
from pathlib import Path

from util import load_JASC


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", dest="output", type=Path, required=True)

    args = parser.parse_args()

    with open(args.input) as f:
        palette = load_JASC(f)

    with open(args.output, "wt") as f:
        f.write("static const COLOR palette[] = {\n")

        for rgbx in palette:
            r, g, b = rgbx[0:3]
            f.write(f"    RGB8({r:2d}, {g:2d}, {b:2d}),\n")

        f.write("};\n")
