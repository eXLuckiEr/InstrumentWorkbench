#!/usr/bin/env python3
"""Draws the application icon (same picture as host/resources/app.svg, keep them in step)
and writes the platform files:  host/platform/app.ico (Windows), app.icns (macOS), app.png (Linux).
Needs Pillow:  python3 -m pip install pillow
Run from anywhere:  python3 tools/make_app_icon.py
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "host", "platform")
S = 1024                      # master size; the SVG uses a 64 x 64 grid
K = S / 64.0
BACKGROUND, LINE = (0x1F, 0x4E, 0x79, 255), (255, 255, 255, 255)
POINTS = [(8, 36), (18, 36), (23, 18), (31, 48), (37, 26), (41, 36), (50, 36)]
WIDTH = 4


def master():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, S - 1, S - 1], radius=14 * K, fill=BACKGROUND)
    pts = [(x * K, y * K) for x, y in POINTS]
    d.line(pts, fill=LINE, width=int(WIDTH * K), joint="curve")
    r = WIDTH * K / 2
    for x, y in pts:          # round caps and joins
        d.ellipse([x - r, y - r, x + r, y + r], fill=LINE)
    return im


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    im = master()
    im.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT, "app.png"))
    im.save(os.path.join(OUT, "app.ico"), sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    im.save(os.path.join(OUT, "app.icns"))
    print("written to", os.path.normpath(OUT))
