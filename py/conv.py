from PIL import Image
import click
import zlib

PALETTE = [
    (0x1f, 0x1f, 0x1f),  # black
    (0xb6, 0xb6, 0xb6),  # white
    (0x52, 0x5f, 0x2d),  # green
    (0x32, 0x30, 0x50),  # blue (less sure on this rgb)
    (0x67, 0x49, 0x49),  # red
    (0xc4, 0xbb, 0x39),  # yellow (also not sure)
    (0x9b, 0x6e, 0x26),  # orange (aka brown)
    (0x75, 0x55, 0x68),  # not a real colour (pinkish? used for "clean")
]

WIDTH = 600
HEIGHT = 448


def image_bytes(converted):
    result = []
    for y in range(0, HEIGHT):
        for x in range(0, WIDTH, 2):
            p1 = converted.getpixel((x, y)) & 0xf
            p2 = converted.getpixel((x + 1, y)) & 0xf
            result.append((p1 << 4) | p2)
    return bytes(result)


@click.command()
@click.option("--header", type=click.File('w'), required=True)
@click.option("--cpp-file", type=click.File('w'), required=True)
@click.argument("files", type=click.Path(exists=True, dir_okay=False), nargs=-1)
def main(header, cpp_file, files):
    num_images = len(files)
    header.write(f"""#pragma once

#include <cstdlib>
#include <cstdint>
    
struct Image {{
  const char *name; 
  const uint8_t *compressed_data;
  size_t compressed_size;
  bool portrait;
  static constexpr auto NumImages = {num_images};
  static const Image Images[NumImages];
}};
""")
    cpp_file.write(f"""
#include "{header.name}"

""")
    images = []
    frame_ratio = WIDTH / HEIGHT
    for index, image in enumerate(files):
        im = Image.open(image)
        portrait = im.height > im.width
        if portrait:
            im = im.transpose(Image.ROTATE_90)
        image_ratio = im.width / im.height
        if image_ratio > frame_ratio:
            # Wider, so scale to height, then cut the middle bit out.
            scale_height = HEIGHT
            scale_width = int(scale_height * image_ratio)
        else:
            scale_width = WIDTH
            scale_height = int(scale_width / image_ratio)
        im = im.resize((scale_width, scale_height))
        if scale_width > WIDTH:
            lhs = (scale_width / 2) - (WIDTH / 2)
            rhs = lhs + WIDTH
            im = im.crop((lhs, 0, rhs, HEIGHT))
        elif scale_height > HEIGHT:
            top = (scale_height / 2) - (HEIGHT / 2)
            bot = top + HEIGHT
            im = im.crop((0, top, WIDTH, bot))
        palette_image = Image.new('P', im.size)
        palette_image.putpalette(list(sum(PALETTE, ())) * 32)
        palette_image.paste(im, (0, 0) + im.size)
        converted = im.quantize(colors=8, palette=palette_image)
        num_on_line = 0
        image_data = image_bytes(converted)
        compressed = zlib.compress(image_data, 9)
        print(
            f"{image} compressed to {len(compressed)} "
            f"({100 * len(compressed) / (WIDTH * HEIGHT / 2):.1f}%)")

        cpp_file.write(f"static const uint8_t image_data_{index}[] = {{\n  ")
        images.append((image, len(compressed), portrait))
        for byte in compressed:
            cpp_file.write(f"0x{byte:02x}, ")
            num_on_line += 1
            if num_on_line > 12:
                cpp_file.write("\n  ")
                num_on_line = 0
        cpp_file.write("""
};
""")

    cpp_file.write(f"""

const Image Image::Images[NumImages] = {{

""")

    for index, (image, size, portrait) in enumerate(images):
        cpp_file.write(
            f'{{ "{image}", image_data_{index}, {size}, '
            f'{"true" if portrait else "false"} }},\n')

    cpp_file.write("""
};
    """)


if __name__ == '__main__':
    main()
