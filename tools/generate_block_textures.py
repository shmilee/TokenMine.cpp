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


def farmland_top(x, y):
    # Tilled soil (Phase 50): darker than dirt, with horizontal furrow
    # ridges (a darker band every 4 rows — the Minecraft farmland look).
    r = clamp(110 + noise(x, y, 90, 10))
    g = clamp(78 + noise(x, y, 91, 10))
    b = clamp(52 + noise(x, y, 92, 8))
    if y % 4 == 0:
        r, g, b = int(r * 0.72), int(g * 0.72), int(b * 0.72)
    return (r, g, b, 255)


def farmland_side(x, y):
    # Dirt sides with the tilled top band along the top edge.
    if y < 3:
        return farmland_top(x, y)
    return dirt(x, y + 3)


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


def lava(x, y):
    # Glowing orange with bright hot spots (the Minecraft lava look; the
    # in-game glow comes from light_emission, not the texture).
    h = h2(x, y, 100)
    if h % 100 < 12:
        # Hot spot: yellow-white.
        return (clamp(250 + noise(x, y, 101, 5)),
                clamp(220 + noise(x, y, 102, 15)),
                clamp(60 + noise(x, y, 103, 30)), 255)
    if h % 100 < 30:
        # Bright orange.
        return (clamp(235 + noise(x, y, 104, 10)),
                clamp(120 + noise(x, y, 105, 20)),
                clamp(10 + noise(x, y, 106, 20)), 255)
    # Base: deep orange-red.
    return (clamp(190 + noise(x, y, 107, 25)),
            clamp(60 + noise(x, y, 108, 25)),
            clamp(noise(x, y, 109, 15)), 255)


def obsidian(x, y):
    # Near-black with a faint violet sheen and rare lighter flecks (the
    # Minecraft obsidian look).
    h = h2(x, y, 116)
    if h % 100 < 6:
        return (clamp(70 + noise(x, y, 117, 15)),
                clamp(40 + noise(x, y, 118, 12)),
                clamp(75 + noise(x, y, 119, 15)), 255)
    v = clamp(16 + noise(x, y, 120, 10))
    return (v + 6, v, v + 8, 255)


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


def furnace_top(x, y):
    # Stone-like top (Minecraft's furnace top is a stone brick face).
    return stone(x, y)


def furnace_side(x, y):
    # Stone + a dark firebox mouth in the lower middle, glowing embers at
    # the bottom of the mouth, a darker stone rim around it.
    if 3 <= x <= 12 and 8 <= y <= 14:
        if y >= 13 and 5 <= x <= 10:
            glow = h2(x, y, 58) % 40
            return (clamp(240 - glow), clamp(130 - glow),
                    clamp(48 - glow // 2), 255)
        if y >= 13:
            return (70, 45, 32, 255)
        return (38 + noise(x, y, 59, 8), 33 + noise(x, y, 60, 8),
                30 + noise(x, y, 61, 8), 255)
    if 2 <= x <= 13 and 7 <= y <= 15:
        v = 100 + noise(x, y, 62, 8)
        return (v, v, v, 255)
    return stone(x, y)


# --- dyed wool family (Phase 41, six colors added in Phase 45) --------------
# The Minecraft dyed-wool palette. Phase 45 completes the set: the six
# colors whose MC dye sources are in-world content added the same phase
# (lapis ore / cactus / allium / tulip / cornflower). Same fluff as the
# sheep body: one per-pixel noise on all channels (hue constant,
# brightness varies). White is the family base block ("wool", no suffix).
WOOL_PALETTE = {
    "wool": (232, 230, 224),
    "wool_red": (185, 54, 64),
    "wool_yellow": (232, 199, 60),
    "wool_orange": (240, 118, 19),
    "wool_pink": (234, 138, 195),
    "wool_brown": (119, 81, 51),
    "wool_gray": (62, 68, 71),
    "wool_black": (25, 23, 25),
    "wool_silver": (142, 142, 147),
    "wool_blue": (61, 90, 180),
    "wool_green": (117, 194, 74),
    "wool_purple": (154, 80, 192),
    "wool_magenta": (206, 82, 213),
    "wool_light_blue": (126, 174, 245),
    "wool_cyan": (74, 196, 197),
}


# Stable per-name salt (Phase 45): an EXPLICIT map. The original
# `100 + sorted(palette).index(name)` scheme shifted the salt of existing
# colors when the palette grew (silently changing their committed
# textures); new colors take the next free slots.
WOOL_SALT = {
    "wool": 100,
    "wool_black": 101,
    "wool_brown": 102,
    "wool_gray": 103,
    "wool_orange": 104,
    "wool_pink": 105,
    "wool_red": 106,
    "wool_silver": 107,
    "wool_yellow": 108,
    "wool_blue": 109,
    "wool_green": 110,
    "wool_purple": 111,
    "wool_magenta": 112,
    "wool_light_blue": 113,
    "wool_cyan": 114,
}
assert set(WOOL_SALT) == set(WOOL_PALETTE), "WOOL_SALT must cover every palette color"


def make_wool_tile(name: str):
    (br, bg, bb) = WOOL_PALETTE[name]
    salt = WOOL_SALT[name]

    def tile(x, y):
        n = noise(x, y, salt, 12)
        return (clamp(br + n), clamp(bg + n), clamp(bb + n), 255)
    return tile


# --- Phase 45: new material sources ------------------------------------------
# Lapis ore (blue dye source): stone with blue speckles.
def lapis_ore(x, y):
    return ore(x, y, 61, (58, 88, 210), 14)


# Cactus (green dye source): a full cube, green base with darker vertical
# ridges (the Minecraft cactus side look).
def cactus(x, y):
    n = noise(x, y, 62, 8)
    if x % 4 == 0:  # vertical ridge
        c = (58, 98, 40)
    else:
        c = (84, 134, 58)
    return (clamp(c[0] + n), clamp(c[1] + n), clamp(c[2] + n), 255)


# Cross-plant flowers (purple/magenta/light-blue dye sources). Each is a
# flower head over a green stem on a TRANSPARENT background; the mesher
# draws it as two crossing quads in the blending pass (alpha cutout via
# the standard src-alpha blend). The same texture is used on both quads.
def make_flower(petal, salt):
    stem = (66, 120, 48)

    def tile(x, y):
        # Stem: a 2-px green column from just below the head to the base.
        if 7 <= x <= 8 and 9 <= y <= 15:
            n = noise(x, y, salt, 8)
            return (clamp(stem[0] + n), clamp(stem[1] + n),
                    clamp(stem[2] + n), 255)
        # Flower head: a soft blob centred on (7.5, 4.5).
        dx = x - 7.5
        dy = y - 4.5
        if dx * dx + dy * dy <= 8.0:  # radius ~2.8
            n = noise(x, y, salt + 1, 10)
            return (clamp(petal[0] + n), clamp(petal[1] + n),
                    clamp(petal[2] + n), 255)
        return (0, 0, 0, 0)  # transparent
    return tile


def make_wheat(stage: int, salt: int):
    # Wheat crop (Phase 48): a central stem that grows taller with the
    # stage (1..7). Stages 1..6 are green (unripe); stage 7 is the golden
    # ripe head. The head is a small vertical cluster atop the stem.
    stem_green = (104, 158, 56)
    ripe = (226, 190, 78)
    stem_top = 16 - stage - 1      # stage 1 -> 14 (short), stage 7 -> 8 (tall)
    head_color = ripe if stage >= 7 else (140, 190, 84)

    def tile(x, y):
        # Stem: a 2-px green column from just below the head to the base.
        if 7 <= x <= 8 and stem_top + 1 <= y <= 15:
            n = noise(x, y, salt, 7)
            return (clamp(stem_green[0] + n), clamp(stem_green[1] + n),
                    clamp(stem_green[2] + n), 255)
        # Head: a soft vertical cluster centred on (7.5, stem_top - 1).
        dx = x - 7.5
        dy = y - (stem_top - 1)
        if dx * dx * 1.6 + dy * dy * 0.7 <= 5.5:
            n = noise(x, y, salt + 1, 9)
            return (clamp(head_color[0] + n), clamp(head_color[1] + n),
                    clamp(head_color[2] + n), 255)
        return (0, 0, 0, 0)  # transparent
    return tile


TEXTURES = {
    **{name: make_wool_tile(name) for name in WOOL_PALETTE},
    **{f"wheat_{s}": make_wheat(s, 70 + s) for s in range(1, 8)},
    "stone": stone,
    "dirt": dirt,
    "farmland_top": farmland_top,
    "farmland_side": farmland_side,
    "grass_top": grass_top,
    "grass_side": grass_side,
    "sand": sand,
    "wood_side": wood_side,
    "wood_top": wood_top,
    "leaves": leaves,
    "glass": glass,
    "water": water,
    "lava": lava,
    "obsidian": obsidian,
    "bedrock": bedrock,
    "coal_ore": coal_ore,
    "iron_ore": iron_ore,
    "gold_ore": gold_ore,
    "diamond_ore": diamond_ore,
    "lapis_ore": lapis_ore,
    "cactus": cactus,
    "allium": make_flower((154, 80, 192), 63),
    "tulip": make_flower((206, 82, 213), 64),
    "cornflower": make_flower((126, 174, 245), 65),
    "glowstone": glowstone,
    "planks": planks,
    "crafting_table_top": crafting_table_top,
    "crafting_table_side": crafting_table_side,
    "furnace_top": furnace_top,
    "furnace_side": furnace_side,
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
