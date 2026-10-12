#!/usr/bin/env python3
"""Generate app icons: scan-frame glyph on electric-blue rounded square.

Draws at 4x then downscales (LANCZOS) for smooth anti-aliased edges;
lines get round caps via endpoint circles.
"""
from PIL import Image, ImageDraw

ACCENT = (47, 91, 255)      # #2F5BFF
WHITE = (255, 255, 255)
SS = 4                      # supersample factor


def _cap_line(d, x0, y0, x1, y1, w, fill):
    """Line with round caps (circles at endpoints)."""
    w = int(round(w))
    d.line([x0, y0, x1, y1], fill=fill, width=w)
    r = w / 2
    for x, y in ((x0, y0), (x1, y1)):
        d.ellipse([x - r, y - r, x + r, y + r], fill=fill)


def glyph(size, maskable=False):
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = S * (0.5 if maskable else 0.235)   # maskable fills the safe circle
    d.rounded_rectangle([0, 0, S - 1, S - 1], radius=r, fill=ACCENT)

    # scan-frame corner brackets (short arms: tighter side gaps).
    # maskable keeps extra padding for the 80% safe circle.
    inset = S * (0.22 if maskable else 0.19)
    ext = S * (0.12 if maskable else 0.13)
    w = S * 0.034
    for sx, sy in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
        cx = inset if sx > 0 else S - inset
        cy = inset if sy > 0 else S - inset
        _cap_line(d, cx, cy, cx + sx * ext, cy, w, WHITE)
        _cap_line(d, cx, cy, cx, cy + sy * ext, w, WHITE)

    # three text lines: airy spacing, edges flush with bracket inner ends,
    # last line shorter like a paragraph ending
    lx0 = S * 0.32
    lens = (0.36, 0.36, 0.22)
    for i, y in enumerate(S * f for f in (0.395, 0.50, 0.605)):
        _cap_line(d, lx0, y, lx0 + S * lens[i], y, w, WHITE)

    return img.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    glyph(512).save(os.path.join(here, "icon-512.png"))
    glyph(192).save(os.path.join(here, "icon-192.png"))
    glyph(512, maskable=True).save(os.path.join(here, "icon-maskable-512.png"))
    glyph(180).save(os.path.join(here, "apple-touch-icon.png"))
    glyph(64).save(os.path.join(here, "favicon-64.png"))
    print("icons written")
