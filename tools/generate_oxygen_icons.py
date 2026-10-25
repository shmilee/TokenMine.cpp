#!/usr/bin/env python3
"""Generate the oxygen bubble strip (RGBA PNG) for SURV-2.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this
file to refresh assets/textures/oxygen.png. This file writes ONLY its
own output — every other generated texture is untouched (the per-pixel
salt base is the next free value after the item icons' 104).

The HUD oxygen bar REPLACES the hunger row while the air is below full
(Minecraft: the hunger bar hides while the air bar shows): 10 bubbles,
icon i fills while oxygen > i * 15 (ceil(oxygen/15), no half-state), so
the strip is a SINGLE 16x16 tile — an unfilled position draws nothing:
  0: full bubble (blue circle + the top-left glint)
"""
import os
import struct
import zlib

W, H = 16, 16
OUT = os.path.join(os.path.dirname(__file__), "..", "assets",
                   "textures", "oxygen.png")

# The next free salt after the item icons' 104 (the master bow's marks);
# the per-pixel salt is SALT_BASE + ty * 16 + tx (the health/hunger
# convention — the tile index 0 factor drops out).
SALT_BASE = 105

# An air-bubble silhouette: the 10x10 circle (rows 3..12 of the tile),
# placed at offset (3, 3) inside the 16x16 tile (the heart/drumstick
# convention: the shape sits centered at (8, 8)). O = bubble.
BUBBLE = [
    "...OOOO...",
    ".OOOOOOOO.",
    ".OOOOOOOO.",
    "OOOOOOOOOO",
    "OOOOOOOOOO",
    "OOOOOOOOOO",
    "OOOOOOOOOO",
    ".OOOOOOOO.",
    ".OOOOOOOO.",
    "...OOOO...",
]
BUBBLE_OX, BUBBLE_OY = 3, 3


def splitmix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return x ^ (x >> 31)


def h2(x: int, y: int, salt: int) -> int:
    return splitmix64(x * 374761393 + y * 668265263 + salt * 2246822519)


def noise(x: int, y: int, salt: int, amount: int) -> int:
    return (h2(x, y, salt) % (2 * amount + 1)) - amount


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


def bubble_at(tx: int, ty: int) -> bool:
    """Is tile-local (tx, ty) inside the bubble silhouette?"""
    if not (0 <= tx < 16 and 0 <= ty < 16):
        return False
    gy, gx = ty - BUBBLE_OY, tx - BUBBLE_OX
    if not (0 <= gy < len(BUBBLE) and 0 <= gx < len(BUBBLE[0])):
        return False
    return BUBBLE[gy][gx] == "O"


def setpx(px, x, y, color, salt, amount=8):
    r = max(0, min(255, color[0] + noise(x, y, salt, amount)))
    g = max(0, min(255, color[1] + noise(x, y, salt + 1, amount)))
    b = max(0, min(255, color[2] + noise(x, y, salt + 2, amount)))
    px[y][x] = (r, g, b, 255)


BUBBLE_BLUE = (74, 145, 224)
BUBBLE_HI = (160, 210, 250)


def main() -> None:
    px = [[(0, 0, 0, 0)] * W for _ in range(H)]
    for ty in range(16):
        for tx in range(16):
            if not bubble_at(tx, ty):
                continue
            # The glint (the health heart's top-left highlight
            # convention): the two diagonal pixels at (4, 5) / (5, 4).
            c = BUBBLE_HI if (tx == 4 and ty == 5) or (tx == 5 and ty == 4) \
                else BUBBLE_BLUE
            setpx(px, tx, ty, c, salt=SALT_BASE + ty * 16 + tx)
    write_png(OUT, px)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
