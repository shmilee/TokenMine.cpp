#!/usr/bin/env python3
"""Generate 16x16 placeholder block textures (RGBA PNG) for Phase 2.

Deterministic: per-pixel variation comes from a hash of (x, y, salt), so the
output is stable across runs and platforms. Re-run after editing this file to
refresh assets/textures/blocks/.
"""
import os
import struct
import sys
import zlib

TILE = 16
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets",
                       "textures", "blocks")


def chunk(tag: bytes, data: bytes) -> bytes:
    c = struct.pack(">I", len(data)) + tag + data
    c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return c


def write_png(path: str, pixels) -> None:
    """pixels: list of rows, each a list of (r, g, b, a) tuples."""
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", TILE, TILE, 8, 6, 0, 0, 0)
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


def h2(x: int, y: int, salt: int) -> int:
    """Deterministic 32-bit hash in [0, 2^32)."""
    h = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFFFFFF


def noise(x: int, y: int, salt: int, amount: int) -> int:
    """Centered noise in [-amount, +amount]."""
    return (h2(x, y, salt) % (2 * amount + 1)) - amount


def clamp(v: int) -> int:
    return max(0, min(255, v))


def tile_of(fn):
    return [[fn(x, y) for x in range(TILE)] for y in range(TILE)]


# --- textures ---------------------------------------------------------------

def stone(x, y):
    v = 125 + noise(x, y, 1, 10)
    return (v, v, v, 255)


def dirt(x, y):
    return (clamp(134 + noise(x, y, 2, 12)),
            clamp(96 + noise(x, y, 3, 12)),
            clamp(67 + noise(x, y, 4, 10)), 255)


def grass_top(x, y):
    return (clamp(95 + noise(x, y, 5, 12)),
            clamp(159 + noise(x, y, 6, 12)),
            clamp(53 + noise(x, y, 7, 10)), 255)


def grass_side(x, y):
    if y < 4:
        return grass_top(x, y)
    return dirt(x, y + 4)


def sand(x, y):
    return (clamp(219 + noise(x, y, 8, 8)),
            clamp(207 + noise(x, y, 9, 8)),
            clamp(163 + noise(x, y, 10, 8)), 255)


def wood_side(x, y):
    # Vertical bark stripes with noise.
    stripe = 10 if (x // 3) % 2 == 0 else 0
    return (clamp(104 + stripe + noise(x, y, 11, 8)),
            clamp(82 + stripe + noise(x, y, 12, 8)),
            clamp(49 + stripe + noise(x, y, 13, 6)), 255)


def wood_top(x, y):
    # Concentric growth rings around the center.
    dx, dy = x - 7.5, y - 7.5
    ring = int((dx * dx + dy * dy) ** 0.5)
    light = 20 if ring % 2 == 0 else -15
    return (clamp(152 + light + noise(x, y, 14, 6)),
            clamp(120 + light + noise(x, y, 15, 6)),
            clamp(74 + light + noise(x, y, 16, 6)), 255)


def leaves(x, y):
    # Green with alpha holes (~12%) so it exercises the blending pass.
    if h2(x, y, 17) % 100 < 12:
        return (0, 0, 0, 0)
    return (clamp(60 + noise(x, y, 18, 14)),
            clamp(143 + noise(x, y, 19, 14)),
            clamp(72 + noise(x, y, 20, 12)), 255)


def glass(x, y):
    # Border frame + a couple of highlight streaks, transparent interior.
    border = x == 0 or y == 0 or x == TILE - 1 or y == TILE - 1
    if border:
        return (210, 235, 255, 255)
    if (x == y and 2 <= x <= 6) or (x + y == TILE - 3 and 6 <= x <= 9):
        return (255, 255, 255, 220)
    return (200, 225, 255, 0)


def water(x, y):
    return (clamp(47 + noise(x, y, 21, 10)),
            clamp(119 + noise(x, y, 22, 10)),
            clamp(189 + noise(x, y, 23, 10)), 200)


def bedrock(x, y):
    # Dark, high-contrast mottling.
    v = clamp(52 + noise(x, y, 44, 30))
    return (v, v, v, 255)


def ore(x, y, salt, color, density):
    # Stone base with colored speckles (ore lumps).
    if h2(x, y, salt) % 100 < density:
        return (clamp(color[0] + noise(x, y, salt + 1, 6)),
                clamp(color[1] + noise(x, y, salt + 2, 6)),
                clamp(color[2] + noise(x, y, salt + 3, 6)), 255)
    return stone(x, y)


def coal_ore(x, y):
    return ore(x, y, 40, (48, 48, 48), 18)


def iron_ore(x, y):
    return ore(x, y, 41, (216, 178, 142), 14)


def gold_ore(x, y):
    return ore(x, y, 42, (252, 238, 79), 12)


def diamond_ore(x, y):
    return ore(x, y, 43, (93, 236, 245), 10)


def glowstone(x, y):
    # Warm orange-yellow base with bright glowing speckles.
    if h2(x, y, 50) % 100 < 16:
        return (clamp(255 + noise(x, y, 51, 10)),
                clamp(224 + noise(x, y, 52, 12)),
                clamp(120 + noise(x, y, 53, 14)), 255)
    return (clamp(196 + noise(x, y, 54, 12)),
            clamp(160 + noise(x, y, 55, 12)),
            clamp(66 + noise(x, y, 56, 10)), 255)


def planks(x, y):
    # Horizontal boards (4px each) with a dark seam line on the top edge.
    if y % 4 == 0:
        return (101, 77, 46, 255)
    board = y // 4
    v = noise(x, y, 57 + board, 8)
    return (clamp(168 + v),
            clamp(134 + v),
            clamp(82 + v), 255)


def crafting_table_top(x, y):
    # Planks + the 2x2 work surface: a 2px frame border and a center cross.
    border = x in (0, 1, 14, 15) or y in (0, 1, 14, 15)
    cross = x in (7, 8) or y in (7, 8)
    if border or cross:
        return (94, 71, 43, 255)
    return planks(x, y)


def crafting_table_side(x, y):
    # Planks + a 2x2 grid emblem in the middle (an 8x8 square with cross).
    if 4 <= x <= 11 and 4 <= y <= 11:
        border = x in (4, 11) or y in (4, 11)
        cross = x == 7 or y == 7
        if border or cross:
            return (94, 71, 43, 255)
    return planks(x, y)


TEXTURES = {
    "stone": stone,
    "dirt": dirt,
    "grass_top": grass_top,
    "grass_side": grass_side,
    "sand": sand,
    "wood_side": wood_side,
    "wood_top": wood_top,
    "leaves": leaves,
    "glass": glass,
    "water": water,
    "bedrock": bedrock,
    "coal_ore": coal_ore,
    "iron_ore": iron_ore,
    "gold_ore": gold_ore,
    "diamond_ore": diamond_ore,
    "glowstone": glowstone,
    "planks": planks,
    "crafting_table_top": crafting_table_top,
    "crafting_table_side": crafting_table_side,
}


def main() -> int:
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in sorted(TEXTURES.items()):
        path = os.path.join(OUT_DIR, name + ".png")
        write_png(path, tile_of(fn))
        print(f"OK: {os.path.relpath(path, os.path.dirname(OUT_DIR))} "
              f"({os.path.getsize(path)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
