from PIL import Image
import click

PALETTE = [
    (0, 0, 0),
    (255, 255, 255),
    (67, 138, 28),
    (100, 64, 255),
    (191, 0, 0),
    (255, 243, 56),
    (232, 126, 0),
    (194, 164, 244)
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
    return result


@click.command()
@click.option("--header", type=click.File('w'), required=True)
@click.option("--cpp-file", type=click.File('w'), required=True)
@click.argument("files", type=click.Path(exists=True, dir_okay=False), nargs=-1)
def main(header, cpp_file, files):
    num_images = len(files)
    header.write(f"""
#pragma once
struct Image {{
  const char *name; 
  const char data[{int(WIDTH * HEIGHT / 2)}];
  static constexpr auto NumImages = {num_images};
  static const Image Images[NumImages];
}};
""")
    cpp_file.write(f"""
#include "{header.name}"

const Image Image::Images[NumImages] = {{

""")
    for index, image in enumerate(files):
        im = Image.open(image)
        im = im.resize((WIDTH, HEIGHT))
        palette_image = Image.new('P', im.size)
        palette_image.putpalette(list(sum(PALETTE, ())) * 32)
        palette_image.paste(im, (0, 0) + im.size)
        converted = im.quantize(colors=8, palette=palette_image)
        cpp_file.write('{ "' + image + '", {\n')
        num_on_line = 0
        for byte in image_bytes(converted):
            cpp_file.write(f"0x{byte:02x}, ")
            num_on_line += 1
            if num_on_line > 20:
                cpp_file.write("\n  ")
                num_on_line = 0
        cpp_file.write("""
        }
    },
""")
    cpp_file.write("""
};
    """)


if __name__ == '__main__':
    main()
