# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Compose simulator output into PNG panel pictures: the LCD on top, the
14 buttons and 27 keys below with their LEDs. Usage: panel_png.py DIR"""
import glob
import os
import sys

from PIL import Image, ImageDraw

BUTTONS = ["OCT-", "OCT+", "FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
BLACK = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25}
PARTS = ["BD", "SD", "LT", "HT", "RS", "CP", "CB", "CY", "OH", "CH", "ZP"]


def compose(ppm, leds_path, out):
    lcd = Image.open(ppm).resize((480, 480), Image.NEAREST)
    with open(leds_path) as f:
        lines = f.read().split()
    leds = [c == "1" for c in lines[0]]
    held = int(lines[1])
    img = Image.new("RGB", (640, 760), (28, 28, 30))
    img.paste(lcd, (80, 16))
    d = ImageDraw.Draw(img)
    for i, name in enumerate(BUTTONS):
        x, y = 12 + i * 44, 512
        lit = leds[i]
        fill = (240, 120, 40) if lit else (60, 60, 64)
        outline = (255, 255, 255) if held & (1 << i) else (90, 90, 96)
        d.rounded_rectangle((x, y, x + 38, y + 28), 5, fill=fill, outline=outline, width=2)
        d.text((x + 3, y + 33), name, fill=(200, 200, 200))
    whites = [k for k in range(27) if k not in BLACK]
    wx = {}
    for n, k in enumerate(whites):
        wx[k] = 12 + n * 38
    for k in whites:
        x = wx[k]
        lit = leds[14 + k]
        d.rectangle((x, 580, x + 34, 740), fill=(250, 250, 245) if not lit else (255, 205, 80), outline=(20, 20, 20))
    bi = 0
    for k in range(27):
        if k not in BLACK:
            continue
        x = wx[k - 1] + 24
        lit = leds[14 + k]
        d.rectangle((x, 580, x + 24, 670), fill=(255, 120, 40) if lit else (24, 24, 24), outline=(0, 0, 0))
        d.text((x + 5, 650), PARTS[bi], fill=(220, 220, 220) if not lit else (0, 0, 0))
        bi += 1
    img.save(out)


def main():
    folder = sys.argv[1]
    for ppm in sorted(glob.glob(os.path.join(folder, "*.ppm"))):
        base = ppm[:-4]
        compose(ppm, base + ".leds", base + ".png")
        print(base + ".png")


if __name__ == "__main__":
    main()
