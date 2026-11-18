import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageFilter

HEIGHT = 72
OPAQUE = 3
INK_LOW = 40
INK_HIGH = 160
PAPER_FILTER = 15
PAPER_BLUR = 8


def ink_levels(image):
    gray = image.convert("L")
    paper = gray.filter(ImageFilter.MaxFilter(PAPER_FILTER)).filter(ImageFilter.GaussianBlur(PAPER_BLUR))
    levelled = []
    for pixel, light in zip(gray.getdata(), paper.getdata()):
        ink = 255 - min(255, pixel * 255 // max(light, 1))
        levelled.append(min(255, max(0, (ink - INK_LOW) * 255 // (INK_HIGH - INK_LOW))))
    result = Image.new("L", gray.size)
    result.putdata(levelled)
    return result


def cropped(ink):
    box = ink.getbbox()
    if box is None:
        raise ValueError("the logo has no ink")
    left, top, right, bottom = box
    return ink.crop((max(left - 1, 0), max(top - 1, 0), min(right + 1, ink.width), min(bottom + 1, ink.height)))


def scaled(ink, height=HEIGHT):
    width = max(1, round(ink.width * height / ink.height))
    return ink.resize((width, height), Image.Resampling.LANCZOS)


def quantised(value):
    return (value * OPAQUE + 127) // 255


def alpha_rows(image):
    values = [quantised(value) for value in image.getdata()]
    return [values[row * image.width:(row + 1) * image.width] for row in range(image.height)]


def pack_rows(rows):
    packed = bytearray()
    for row in rows:
        padded = list(row) + [0] * (-len(row) % 4)
        for index in range(0, len(padded), 4):
            first, second, third, fourth = padded[index:index + 4]
            packed.append(first << 6 | second << 4 | third << 2 | fourth)
    return bytes(packed)


def formatted(text, style):
    result = subprocess.run(["clang-format", f"--style=file:{style}", "--assume-filename=x.c"],
                            input=text, capture_output=True, text=True, check=True)
    return result.stdout


def render(source, style):
    image = scaled(cropped(ink_levels(Image.open(source))))
    packed = pack_rows(alpha_rows(image))
    stride = (image.width + 3) // 4
    header = ["#pragma once", "", "#include <stdint.h>", "", "enum {",
              f"    SPLASH_LOGO_WIDTH = {image.width},", f"    SPLASH_LOGO_HEIGHT = {image.height},",
              f"    SPLASH_LOGO_STRIDE = {stride}", "};", "",
              "extern const uint8_t SPLASH_LOGO_ALPHA[SPLASH_LOGO_STRIDE * SPLASH_LOGO_HEIGHT];", ""]
    lines = ['#include "splash_logo.h"', "",
             "const uint8_t SPLASH_LOGO_ALPHA[SPLASH_LOGO_STRIDE * SPLASH_LOGO_HEIGHT] = {"]
    for start in range(0, len(packed), 12):
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in packed[start:start + 12]) + ",")
    lines += ["};", ""]
    return {"splash_logo.h": formatted("\n".join(header), style),
            "splash_logo.c": formatted("\n".join(lines), style)}


def main():
    parser = argparse.ArgumentParser(description="Generate the start-up logo table")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--style", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    files = render(arguments.source, arguments.style)
    stale = [name for name, text in files.items()
             if not (arguments.output / name).exists() or
             (arguments.output / name).read_text(encoding="utf-8") != text]
    if arguments.check:
        for name in stale:
            print(f"out of date: {arguments.output / name}")
        return 1 if stale else 0
    for name, text in files.items():
        (arguments.output / name).write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote the start-up logo table to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
