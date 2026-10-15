#!/usr/bin/env python3
"""Builds the sea chart marker of the multiplayer-connection guest mod (our own drawing, no game art).

Output, in the mod package:
  assets/map_player.png  64 x 64: an arrowhead pointing up (north at rotation 0), white body with a
                         dark outline. The HUD multiplies it by the player's cursor colour: the body
                         takes the colour, the outline stays dark. The mod rotates it to the player's
                         heading (mod.c draw_chart_players).

Needs Pillow.
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PACKAGE = ROOT.parent.parent / "examples" / "guest-mods" / "multiplayer-connection"
SIZE = 64
SUPER = 8  # supersampling for smooth edges


def arrow(scale, inset):
    """The arrowhead: tip at the top, notched tail, centred on the image."""
    s = SIZE * scale
    pts = [(0.50, 0.06), (0.88, 0.90), (0.50, 0.70), (0.12, 0.90)]
    cx, cy = 0.5, 0.56
    out = []
    for x, y in pts:  # shrink towards the centre for the inner (fill) shape
        out.append(((cx + (x - cx) * (1 - inset)) * s, (cy + (y - cy) * (1 - inset)) * s))
    return out


def main():
    big = SIZE * SUPER
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon(arrow(SUPER, 0.0), fill=(24, 26, 32, 255))      # outline
    d.polygon(arrow(SUPER, 0.22), fill=(255, 255, 255, 255))  # body (tinted by the HUD)
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    out = PACKAGE / "assets" / "map_player.png"
    img.save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
