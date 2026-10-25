#!/usr/bin/env python3
"""Generate the status-effect icon strip (RGBA PNG) for Phase 46.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/effects.png.

The HUD effect row (left-aligned on the heart row) renders one 16x16 tile
from the W x 16 strip per ACTIVE effect; the effect data's 'icon' field
indexes the tile:
  0: poison (purple skull with dark eye sockets — the Minecraft poison
     icon silhouette)
  1: weakness (dark blue down arrow)
  2: regeneration (green up arrow)
  3: absorption (Phase 56, red heart with a white plus — the Minecraft
     absorption silhouette)
  4: fire (Phase 57, orange flame with a yellow core — the Minecraft
     fire icon silhouette)
  5: speed (Phase 84, cyan up-diamond)
  6: strength (Phase 84, red power block)
  7: resistance (Phase 84, blue shield)
  8: healing (SURV-4, red heart with a green cross — the instant heal)
  9: harming (SURV-4, red heart with a purple-black crack — instant harm)
The background is transparent (the HUD draws over the world).
"""
import os
import struct
import zlib

W, H = 160, 16
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
ABSORB = (216, 40, 48)
ABSORB_PLUS = (240, 240, 240)
FLAME = (240, 120, 24)
FLAME_CORE = (252, 216, 72)

# Flame silhouette (8x12) for the fire icon, tile-local offset (4, 2); a
# yellow core overwrites the lower center (the Minecraft fire mark).
FLAME_SHAPE = [
    "....X...",
    "...XX...",
    "...X.X..",
    "..XXX.X.",
    "..XXXXX.",
    ".XXXXXX.",
    ".XXXXXXX",
    "XXXXXXXX",
    ".XXXXXXX",
    ".XXXXXX.",
    "..XXXX..",
    "...XX...",
]
FLAME_OX, FLAME_OY = 4, 2

# Heart silhouette (9x7) for the absorption icon, tile-local offset (3, 4);
# a white plus overwrites its center (the Minecraft absorption mark).
HEART = [
    ".XX...XX.",
    "XXXX.XXXX",
    "XXXXXXXXX",
    "XXXXXXXXX",
    ".XXXXXXX.",
    "..XXXXX..",
    "...XXX...",
]
HEART_OX, HEART_OY = 3, 4

# Phase 84 beacon-effect icons: speed (cyan up-diamond), strength (red
# power block), resistance (blue shield).
SPEED_DIAMOND = [
    "...XX...",
    "..XXXX..",
    ".XXXXXX.",
    "XXXXXXXX",
    ".XXXXXX.",
    "..XXXX..",
    "...XX...",
]
SPEED_OX, SPEED_OY = 4, 4
STRENGTH_BLOCK = [
    "XXXXXX",
    "XXXXXX",
    "XXXXXX",
    "XXXXXX",
    "XXXXXX",
    "XXXXXX",
]
STRENGTH_OX, STRENGTH_OY = 5, 5
RESISTANCE_SHIELD = [
    "XXXXXXXX",
    "XXXXXXXX",
    "XXXXXXXX",
    ".XXXXXX.",
    ".XXXXXX.",
    "..XXXX..",
    "...XX...",
]
RESISTANCE_OX, RESISTANCE_OY = 4, 4

POISON = (168, 56, 216)
POISON_EYE = (44, 12, 56)
WEAKNESS = (44, 72, 184)
REGEN = (96, 208, 88)
ABSORB = (216, 40, 48)
ABSORB_PLUS = (240, 240, 240)
FLAME = (240, 120, 24)
FLAME_CORE = (252, 216, 72)
SPEED = (120, 200, 255)
STRENGTH = (220, 60, 60)
RESISTANCE = (90, 120, 220)

# SURV-4 (potions): the two instant effects. healing = a red heart with a
# GREEN cross (the instant heal mark); harming = a red heart with a
# purple-black diagonal CRACK (the instant damage mark). New salt blocks
# 1200+ / 1300+ (every existing tile keeps its salt — the determinism
# proof the re-run is).
HEAL = (216, 40, 48)
HEAL_PLUS = (96, 208, 88)
HARM = (200, 40, 48)
HARM_CRACK = (70, 24, 92)


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


def tile_absorption() -> list:
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if not in_shape(HEART, HEART_OX, HEART_OY, tx, ty):
                continue
            gy, gx = ty - HEART_OY, tx - HEART_OX
            # The plus: vertical bar (center column, 3 rows) + horizontal
            # bar (center row, 5 columns).
            on_plus = (gx == 4 and 2 <= gy <= 4) or (gy == 3 and 2 <= gx <= 6)
            setpx(px, tx, ty, ABSORB_PLUS if on_plus else ABSORB,
                  salt=400 + ty * 16 + tx)
    return px


def tile_flame() -> list:
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if not in_shape(FLAME_SHAPE, FLAME_OX, FLAME_OY, tx, ty):
                continue
            gy, gx = ty - FLAME_OY, tx - FLAME_OX
            # The yellow core: the lower-center block of the flame.
            on_core = (5 <= gy <= 9 and 2 <= gx <= 5)
            setpx(px, tx, ty, FLAME_CORE if on_core else FLAME,
                  salt=800 + ty * 16 + tx)
    return px


def tile_speed() -> list:
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if in_shape(SPEED_DIAMOND, SPEED_OX, SPEED_OY, tx, ty):
                setpx(px, tx, ty, SPEED, salt=900 + ty * 16 + tx)
    return px


def tile_strength() -> list:
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if in_shape(STRENGTH_BLOCK, STRENGTH_OX, STRENGTH_OY, tx, ty):
                setpx(px, tx, ty, STRENGTH, salt=1000 + ty * 16 + tx)
    return px


def tile_resistance() -> list:
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if in_shape(RESISTANCE_SHIELD, RESISTANCE_OX, RESISTANCE_OY,
                        tx, ty):
                setpx(px, tx, ty, RESISTANCE, salt=1100 + ty * 16 + tx)
    return px


def tile_healing() -> list:
    # SURV-4: the healing potion (Minecraft instant healing, +4 HP) — a red
    # heart with a GREEN cross over its center (the absorption heart
    # silhouette; the green cross marks the instant heal). Salt block 1200+.
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if not in_shape(HEART, HEART_OX, HEART_OY, tx, ty):
                continue
            gy, gx = ty - HEART_OY, tx - HEART_OX
            # The cross: vertical bar (center column, 3 rows) + horizontal
            # bar (center row, 5 columns) — the absorption-plus geometry.
            on_plus = (gx == 4 and 2 <= gy <= 4) or (gy == 3 and 2 <= gx <= 6)
            setpx(px, tx, ty, HEAL_PLUS if on_plus else HEAL,
                  salt=1200 + ty * 16 + tx)
    return px


def tile_harming() -> list:
    # SURV-4: the harming potion (Minecraft instant harm, -6 HP) — a red
    # heart with a purple-black diagonal CRACK across it (the instant
    # damage mark). Salt block 1300+.
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            if not in_shape(HEART, HEART_OX, HEART_OY, tx, ty):
                continue
            gy, gx = ty - HEART_OY, tx - HEART_OX
            # The crack: an X (both diagonals) across the heart's heart.
            on_crack = (gx == gy) or (gx + gy == 7)
            setpx(px, tx, ty, HARM_CRACK if on_crack else HARM,
                  salt=1300 + ty * 16 + tx)
    return px


def main() -> None:
    px = [[(0, 0, 0, 0)] * W for _ in range(H)]
    for kind, tile in enumerate((tile_poison(), tile_arrow(False),
                                 tile_arrow(True), tile_absorption(),
                                 tile_flame(), tile_speed(),
                                 tile_strength(), tile_resistance(),
                                 tile_healing(), tile_harming())):
        for y in range(16):
            for x in range(16):
                px[y][kind * 16 + x] = tile[y][x]
    write_png(OUT, px)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
