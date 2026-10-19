#!/usr/bin/env python3
"""Draw a Golden Pig HUD icon (96 x 96 RGBA). The mod's assets/pig.png is now the user's own picture
(300 x 300): this script writes pig_generated.png by default and never that file. Original
geometric art: a round golden pig seen from the front-side (body, head, ears, snout with nostrils,
eyes, legs, curly tail), a dark outline and a light from the top left, 4x4 supersampled. No game data.
usage: make_pig_icon.py [OUT.png]"""
import math
import os
import struct
import sys
import zlib

W = H = 96
SS = 4
OUTLINE = (70, 42, 8)
GOLD_LIGHT = (255, 236, 140)
GOLD = (240, 190, 50)
GOLD_DARK = (190, 130, 20)
SNOUT = (250, 210, 110)
NOSTRIL = (120, 70, 10)
EYE = (30, 20, 10)
SHINE = (255, 255, 235)


def ellipse(x, y, cx, cy, rx, ry):
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2


def gold(x, y, cx, cy, r):
    """Gold shaded by the distance to a light at the top left of the shape."""
    d = math.hypot(x - (cx - r * 0.45), y - (cy - r * 0.5)) / (r * 1.6)
    d = min(max(d, 0.0), 1.0)
    if d < 0.5:
        k = d / 0.5
        return tuple(int(GOLD_LIGHT[c] * (1 - k) + GOLD[c] * k) for c in range(3))
    k = (d - 0.5) / 0.5
    return tuple(int(GOLD[c] * (1 - k) + GOLD_DARK[c] * k) for c in range(3))


def shape(x, y):
    """The colour of a point (None: transparent), front to back."""
    # eyes and their shine
    for ex in (40.0, 56.0):
        if ellipse(x, y, ex, 41.0, 3.2, 4.0) <= 1.0:
            if ellipse(x, y, ex - 1.0, 39.6, 1.1, 1.3) <= 1.0:
                return SHINE
            return EYE
    # snout: nostrils, then its disc with an outline
    for nx in (44.5, 51.5):
        if ellipse(x, y, nx, 53.0, 2.0, 3.0) <= 1.0:
            return NOSTRIL
    s = ellipse(x, y, 48.0, 53.0, 10.0, 7.5)
    if s <= 1.0:
        return OUTLINE if s > 0.72 else SNOUT
    # head
    h = ellipse(x, y, 48.0, 46.0, 21.0, 18.0)
    if h <= 1.0:
        return OUTLINE if h > 0.86 else gold(x, y, 48.0, 46.0, 21.0)
    # ears: triangles on the head's top corners
    for side in (-1, 1):
        ax, ay = 48.0 + side * 18.0, 22.0
        bx, by = 48.0 + side * 6.0, 32.0
        cx, cy = 48.0 + side * 21.0, 36.0
        d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by)
        d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy)
        d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay)
        neg = d1 < 0 or d2 < 0 or d3 < 0
        pos = d1 > 0 or d2 > 0 or d3 > 0
        if not (neg and pos):
            return gold(x, y, 48.0 + side * 15.0, 30.0, 10.0)
    # curly tail on the right
    r = math.hypot(x - 82.0, y - 52.0)
    if 4.0 <= r <= 7.0 and not (x < 82.0 and y > 52.0):
        return OUTLINE if (r < 4.8 or r > 6.2) else GOLD
    # legs
    for lx in (34.0, 44.0, 54.0, 64.0):
        if abs(x - lx) <= 4.5 and 70.0 <= y <= 84.0:
            if y > 81.0:
                return OUTLINE
            return OUTLINE if abs(x - lx) > 3.3 else gold(x, y, lx, 72.0, 6.0)
    # body behind the head
    b = ellipse(x, y, 52.0, 60.0, 30.0, 20.0)
    if b <= 1.0:
        return OUTLINE if b > 0.88 else gold(x, y, 52.0, 60.0, 30.0)
    return None


def render():
    px = bytearray(W * H * 4)
    for py in range(H):
        for pxx in range(W):
            acc = [0, 0, 0, 0]
            for sy in range(SS):
                for sx in range(SS):
                    c = shape(pxx + (sx + 0.5) / SS, py + (sy + 0.5) / SS)
                    if c is None:
                        continue
                    acc[0] += c[0]
                    acc[1] += c[1]
                    acc[2] += c[2]
                    acc[3] += 1
            n = acc[3]
            if n:
                o = (py * W + pxx) * 4
                px[o:o + 4] = bytes((acc[0] // n, acc[1] // n, acc[2] // n, 255 * n // (SS * SS)))
    return px


def png(w, h, rgba):
    raw = b"".join(b"\0" + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        here, "..", "..", "examples", "guest-mods", "multiplayer-connection", "assets", "pig_generated.png")
    with open(out, "wb") as fh:
        fh.write(png(W, H, render()))
    print(out)


if __name__ == "__main__":
    main()
