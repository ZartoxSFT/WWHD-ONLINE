#!/usr/bin/env python3
"""Draw the joker elite's eye icon (examples/guest-mods/multiplayer-connection/assets/eye.png, 64 x 48
RGBA, the crown's size). Original geometric art: an almond-shaped eye with a dark outline, a white
sclera, a violet iris and a black pupil with a highlight, 4x4 supersampled. No game data.
usage: make_eye_icon.py [OUT.png]"""
import math
import os
import struct
import sys
import zlib

W, H, SS = 64, 48, 4
OUTLINE = (24, 16, 32)
SCLERA = (250, 248, 240)
IRIS = (150, 60, 220)
IRIS_DARK = (90, 25, 150)
PUPIL = (10, 8, 14)
SHINE = (255, 255, 255)


def lid_half_height(x):
    """Half height of the almond at x (eye centre 0, half width 28): two arcs."""
    t = x / 28.0
    if abs(t) >= 1.0:
        return -1.0
    return 15.0 * (1.0 - t * t) ** 0.85


def sample(x, y):
    """Colour and alpha of a point, the eye centred at (0, 0)."""
    hh = lid_half_height(x)
    if hh < 0:
        # the outline's tips
        return None
    d = abs(y) - hh
    if d > 3.2:
        return None
    if d > 0:
        return OUTLINE
    r = math.hypot(x, y)
    if r < 3.2 and math.hypot(x + 3.5, y + 3.5) < 3.0:
        return SHINE
    if math.hypot(x + 3.5, y + 3.5) < 2.6:
        return SHINE
    if r < 5.0:
        return PUPIL
    if r < 10.5:
        k = (r - 5.0) / 5.5
        return tuple(int(IRIS[c] * (1 - k) + IRIS_DARK[c] * k) for c in range(3))
    if r < 11.6:
        return OUTLINE
    return SCLERA


def render():
    px = bytearray(W * H * 4)
    for py in range(H):
        for pxx in range(W):
            acc = [0, 0, 0, 0]
            for sy in range(SS):
                for sx in range(SS):
                    x = pxx + (sx + 0.5) / SS - W / 2
                    y = py + (sy + 0.5) / SS - H / 2
                    c = sample(x, y)
                    if c is None:
                        continue
                    acc[0] += c[0]
                    acc[1] += c[1]
                    acc[2] += c[2]
                    acc[3] += 1
            n = acc[3]
            o = (py * W + pxx) * 4
            if n:
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
        here, "..", "..", "examples", "guest-mods", "multiplayer-connection", "assets", "eye.png")
    with open(out, "wb") as fh:
        fh.write(png(W, H, render()))
    print(out)


if __name__ == "__main__":
    main()
