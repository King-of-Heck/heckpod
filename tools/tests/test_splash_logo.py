import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

from PIL import Image, ImageDraw

import splash_logo


def square_on_paper():
    image = Image.new("L", (60, 60), 230)
    ImageDraw.Draw(image).rectangle((20, 24, 29, 33), fill=10)
    return image


class SplashLogoTest(unittest.TestCase):
    def test_paper_becomes_clear_and_ink_becomes_solid(self):
        ink = splash_logo.ink_levels(square_on_paper())
        self.assertEqual(0, ink.getpixel((2, 2)))
        self.assertEqual(255, ink.getpixel((25, 28)))

    def test_cropping_keeps_the_ink_and_a_one_pixel_margin(self):
        ink = splash_logo.cropped(splash_logo.ink_levels(square_on_paper()))
        self.assertEqual((12, 12), ink.size)

    def test_cropping_a_blank_image_is_refused(self):
        with self.assertRaises(ValueError):
            splash_logo.cropped(Image.new("L", (8, 8), 0))

    def test_scaling_sets_the_height_and_keeps_the_shape(self):
        scaled = splash_logo.scaled(Image.new("L", (30, 60), 255))
        self.assertEqual((36, 72), scaled.size)

    def test_levels_quantise_to_sixteen_steps(self):
        self.assertEqual(0, splash_logo.quantised(0))
        self.assertEqual(15, splash_logo.quantised(255))
        self.assertEqual(8, splash_logo.quantised(128))

    def test_rows_pack_two_pixels_per_byte_high_nibble_first(self):
        self.assertEqual(bytes([0xF0, 0x70, 0x12, 0x30]), splash_logo.pack_rows([[15, 0, 7], [1, 2, 3]]))


if __name__ == "__main__":
    unittest.main()
