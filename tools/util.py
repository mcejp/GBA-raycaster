import numpy as np
from PIL import Image


def load_JASC(f):
    # check header and parse palette length
    assert next(f) == "JASC-PAL\n"
    assert next(f) == "0100\n"
    count = int(next(f))

    # parse entries
    colors = np.zeros((count, 3), dtype=np.uint8)
    for i in range(count):
        colors[i, :] = [int(chan) for chan in next(f).split(" ")[0:3]]

    # this ought to be the end of the file
    assert not f.read()

    return colors


# palette is Nx3 np.uint8
def match_color_to_palette(palette, rgb):
    # TODO: should use a better algorithm than RGB distance
    dist_list = [np.linalg.norm(palette[i] - rgb) for i in range(len(palette))]

    # find index of closest color
    return np.argmin(dist_list)


def match_image_to_palette(palette, image: Image.Image) -> Image.Image:
    # approach: build a 2D array of indices, then create indexed Image
    new_image = np.zeros((image.height, image.width), dtype=np.uint8)
    match_cache = {}

    if image.mode == "RGBA":
        for y, x in np.ndindex(new_image.shape):
            r, g, b, a = image.getpixel((x, y))

            if a >= 128:
                try:
                    new_image[y, x] = match_cache[(r, g, b)]
                except KeyError:
                    new_image[y, x] = match_color_to_palette(palette, (r, g, b))
                    match_cache[(r, g, b)] = new_image[y, x]
            else:
                # assuming index 0 as transparent
                new_image[y, x] = 0
    elif image.mode == "RGB":
        # TODO: For RGB we should be able to use Image.quantize
        for y, x in np.ndindex(new_image.shape):
            r, g, b = image.getpixel((x, y))

            try:
                new_image[y, x] = match_cache[(r, g, b)]
            except KeyError:
                new_image[y, x] = match_color_to_palette(palette, (r, g, b))
                match_cache[(r, g, b)] = new_image[y, x]
    else:
        raise ValueError(f"Image mode must be RGB(A), got '{image.mode}'")

    return Image.fromarray(new_image, mode="P")  # FIXME: this loses the palette
