#!/usr/bin/env python3
"""Generate the status-effect icon strip (RGBA PNG) for Phase 46.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/effects.png.

The HUD effect row (left-aligned on the heart row) renders one 16x16 tile
from the 48x16 strip per ACTIVE effect; the effect data's 'icon' field
indexes the tile (tile i occupies u in [i/3, (i+1)/3]):
  0: poison (purple skull with dark eye sockets — the Minecraft poison
     icon silhouette)
  1: weakness (dark blue down arrow)
  2: regeneration (green up arrow)
The background is transparent (the HUD draws over the world).
"""
import os
import struct
import zlib

W, H = 48, 16
OUT = os.path.join(os.path.dirname(__file__), "..", "assets",
                   "textures", "effects.png")

# Skull silhouette (8x7) for the poison icon, tile-local offset (4, 4).
SKULL = [
    ".XXXXXX.",
    "XXXXXXXX",
    "XXXXXXXX",
    "X.XXX.X.",  # eye sockets (the non-X gaps)
    "XXXXXXXX",
    ".XXXXXX.",
    ".XX.XX..",  # jaw teeth
]
SKULL_OX, SKULL_OY = 4, 4

# Down arrow (weakness, 8x6) and up arrow (regeneration, 8x6), tile-local
# offset (4, 5).
ARROW_DOWN = [
    "...XX...",
    "...XX...",
    "...XX...",
    "..XXXX..",
    ".XXXXXX.",
    "..XXXX..",
]
ARROW_UP = [
    "..XXXX..",
    ".XXXXXX.",
    "..XXXX..",
    "...XX...",
    "...XX...",
    "...XX...",
]
ARROW_OX, ARROW_OY = 4, 5

POISON = (168, 56, 216)
POISON_EYE = (44, 12, 56)
WEAKNESS = (44, 72, 184)
REGEN = (96, 208, 88)


def splitmix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return x ^ (x >> 31)


def h2(x: int, y: int, salt: int) -> int:
    return splitmix64(x * 374761393 + y * 668265263 + salt * 2246822519)


def noise(x: int, y: int, salt: int, amount: int) -> int:
    return (h2(x, y, salt) % (2 * amount + 1)) - amount


def clamp(c: int) -> int:
    return max(0, min(255, c))


def chunk(tag: bytes, data: bytes) -> bytes:
    c = struct.pack(">I", len(data)) + tag + data
    c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return c


def write_png(path: str, pixels) -> None:
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    raw = bytearray()
    for row in pixels:
        raw.append(0)  # filter: None
        for (r, g, b, a) in row:
            raw += bytes((r, g, b, a))
    out = (sig + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw)))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(out)


def setpx(px, x, y, color, salt, amount=8):
    px[y][x] = (clamp(color[0] + noise(x, y, salt, amount)),
                clamp(color[1] + noise(x, y, salt + 1, amount)),
                clamp(color[2] + noise(x, y, salt + 2, amount)), 255)


def in_shape(shape, ox, oy, tx, ty) -> bool:
    gy, gx = ty - oy, tx - ox
    if not (0 <= gy < len(shape) and 0 <= gx < len(shape[0])):
        return False
    return shape[gy][gx] == "X"


def tile_poison() -> list:
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if in_shape(SKULL, SKULL_OX, SKULL_OY, tx, ty):
                setpx(px, tx, ty, POISON, salt=ty * 16 + tx)
            elif (SKULL_OY + 3 <= ty < SKULL_OY + 4) and (
                    (SKULL_OX + 1 <= tx < SKULL_OX + 2)
                    or (SKULL_OX + 5 <= tx < SKULL_OX + 6)):
                setpx(px, tx, ty, POISON_EYE, salt=100 + ty * 16 + tx)
    return px


def tile_arrow(up: bool) -> list:
    shape = ARROW_UP if up else ARROW_DOWN
    color = REGEN if up else WEAKNESS
    salt0 = 300 if up else 200
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if in_shape(shape, ARROW_OX, ARROW_OY, tx, ty):
                setpx(px, tx, ty, color, salt=salt0 + ty * 16 + tx)
    return px


def main() -> None:
    px = [[(0, 0, 0, 0)] * W for _ in range(H)]
    for kind, tile in enumerate((tile_poison(), tile_arrow(False),
                                 tile_arrow(True))):
        for y in range(16):
            for x in range(16):
                px[y][kind * 16 + x] = tile[y][x]
    write_png(OUT, px)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
