import argparse
from dataclasses import dataclass
import json
import math
from pathlib import Path


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("name")
    parser.add_argument("-o", dest="output", type=Path, required=True)

    args = parser.parse_args()

    with open(args.input) as f:
        # Maybe we should use a library for this?
        # (see https://doc.mapeditor.org/en/stable/reference/support-for-tmx-maps/#python)
        # On the other hand, this would introduce additional compile-time dependencies
        map = json.load(f)

    TILE_W = map["tilewidth"]
    TILE_H = map["tileheight"]

    # TODO: is there a library for this? or even an existing tool?

    for layer in map["layers"]:
        if layer["name"] == "entities":
            assert layer["type"] == "objectgroup"
            sprite_layer = layer
        elif layer["name"] == "tiles":
            assert layer["type"] == "tilelayer"
            tile_layer = layer

    def get_properties(obj):
        properties = {}
        for p in obj.get("properties", {}):
            properties[p["name"]] = p["value"]
        return properties

    # it is the tile layer which determines the overall coordinate system
    # in-game (0, 0) is in its bottom-left corner
    # while the editor places (0, 0) in top left
    MAP_W = tile_layer['width']
    MAP_H = tile_layer['height']
    x_start = None
    y_start = None

    MAP_TILE_SIZE = 0x100       # engine constant

    def tiled_to_world_coords(x, y):
        return (x / TILE_W,
                MAP_H - y / TILE_H)

    n = args.name

    # would be cleaner to build an internal model first, instead of jumping straight to codegen

    @dataclass
    class Anchor:
        x: float
        y: float
        dir: float
        name: str

    anchors = []

    with open(args.output, "wt") as f:
        f.write("#include <game_structs.hpp>\n\n")
        f.write(f"static const uint8_t {n}_tiles[] = {{\n")

        for y in reversed(range(tile_layer["height"])):
            row = tile_layer["data"][y * tile_layer["width"]:(y + 1) * tile_layer["width"]]
            f.write("    " + ", ".join(f"{max(0, tile - 1):3d}" for tile in row) + ",\n")

        f.write("};\n")

        f.write("\n")
        f.write("Entity entities[] = {\n")
        num_sprites = 0
        for i, obj in enumerate(sprite_layer["objects"]):
            # correct for rotation
            # Tiled anchors sprites in bottom left of the cell, but in-game sprite origin (incl. for rotation) is at the center
            # therefore un-rotate the vector from corner to center
            xxx = obj["width"] / 2
            yyy = obj["height"] / 2
            aaa = -obj["rotation"] * 2 * math.pi / 360
            xxxx = xxx * math.cos(aaa) - yyy * math.sin(aaa)
            yyyy = xxx * math.sin(aaa) + yyy * math.cos(aaa)
            obj_x = obj["x"] + xxxx
            obj_y = obj["y"] - yyyy
            # print(f'{obj["x"]} {obj["y"]} {obj["rotation"]} => {obj_x} {obj_y}', file=sys.stderr)
            x, y = tiled_to_world_coords(obj_x, obj_y)

            props = get_properties(obj)

            MAGIC = 81          # eeeeeeek
            TILE_ANCHOR = MAGIC + 62
            TILE_PLAYER_START = MAGIC + 63

            if obj["gid"] == TILE_ANCHOR:
                anchors.append(Anchor(x=x, y=y, dir=aaa, name=props["name"]))
                continue
            elif obj["gid"] == TILE_PLAYER_START:
                x_start, y_start = x, y
                continue

            f.write("    {"
                    f'{hex(int(x * MAP_TILE_SIZE))}, '
                    f'{hex(int(y * MAP_TILE_SIZE))}, '
                    f'{hex(int(props["width"] / 2 * MAP_TILE_SIZE))}, '
                    f'{hex(int(props["height"] * MAP_TILE_SIZE))}, '
                    f'{obj["gid"] - MAGIC}' "},\n")
            num_sprites += 1

        f.write("};\n\n")
        for anchor in anchors:
            f.write(f"extern const Anchor a_{anchor.name} = {{.x = {hex(int(anchor.x * MAP_TILE_SIZE))}, "
                                                            f".y = {hex(int(anchor.y * MAP_TILE_SIZE))}, "
                                                            f".dir = {hex(int(anchor.dir * 0x8000 / math.pi) % 0x10000)}}};\n")
        f.write(f"static const Map {n} = {{\n")
        f.write(f"    .tiles = {n}_tiles, .entities = entities, .w = {MAP_W}, .h = {MAP_H}, .num_sprites = {num_sprites}, .x_start = {hex(int(x_start * MAP_TILE_SIZE))}, .y_start = {hex(int(y_start * MAP_TILE_SIZE))}\n")
        f.write("};\n")