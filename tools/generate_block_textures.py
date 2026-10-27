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


def cobblestone(x, y):
    # Cobble (Phase 61): rounded cobbles separated by dark crack seams —
    # a staggered 5px grid (the Minecraft cobblestone look); the base is
    # slightly darker than bare stone, each cobble lightest at its centre.
    v = 108 + noise(x, y, 91, 12)
    cx = (x + (y // 5) * 2) % 5
    cy = y % 5
    if cy == 4 or cx == 0:
        v = 72 + noise(x, y, 92, 8)      # crack seam
    else:
        v += (2 - abs(cx - 2)) * 3 + (2 - abs(cy - 2)) * 3
    return (clamp(v), clamp(v), clamp(v), 255)


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


def farmland_wet(x, y):
    # GEN-9: the WET farmland top (the moisture_top variant): the same
    # tilled soil, dampened — a darker, moister tone. The SAME noise
    # salts as farmland_top (90/91/92) so a wet patch reads as the very
    # same field watered, not a different block (the Minecraft wet-
    # farmland look: ~0.72x the dry tone, furrow band unchanged).
    r = clamp(78 + noise(x, y, 90, 10))
    g = clamp(56 + noise(x, y, 91, 10))
    b = clamp(38 + noise(x, y, 92, 8))
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

def sandstone(x, y):
    """Phase 80: sandstone (compressed sand — the P61 material-chain
    pattern, 3 sand -> 1 sandstone). The sand palette in THREE
    horizontal layers (Minecraft's classic cut) separated by darker
    bedding lines; slightly warmer/denser than the loose sand block.
    New salts 923/924, every existing texture pixel-identical."""
    if y in (4, 5, 10, 11):  # the bedding lines between the layers
        base = (198, 183, 141)
    else:
        base = (222, 209, 168)
    n = noise(x, y, 923, 8)
    n2 = noise(x, y, 924, 4)
    return (clamp(base[0] + n + n2), clamp(base[1] + n + n2),
            clamp(base[2] + n + n2), 255)



def smooth_sandstone(x, y):
    """Phase 81: smooth sandstone (the P80 chain's polished step,
    2 sandstone -> 1 smooth_sandstone, Minecraft 1.14+). The sandstone
    palette WITHOUT the bedding lines — a flat, polished surface, slightly
    lighter/uniform than the cut block. New salts 925/926, every existing
    texture pixel-identical."""
    base = (228, 216, 178)  # a touch lighter + smoother than sandstone
    n = noise(x, y, 925, 6)
    n2 = noise(x, y, 926, 3)
    return (clamp(base[0] + n + n2), clamp(base[1] + n + n2),
            clamp(base[2] + n + n2), 255)


def iron_block(x, y):
    """Phase 83: the iron block (the canonical beacon pyramid material,
    Minecraft). A bright, uniform metallic silver with a faint brushed
    noise. New salts 927/928, every existing texture pixel-identical."""
    base = (212, 212, 216)  # the Minecraft iron-block silver
    n = noise(x, y, 927, 5)
    n2 = noise(x, y, 928, 3)
    v = n + n2
    return (clamp(base[0] + v), clamp(base[1] + v),
            clamp(base[2] + v + 2), 255)


def beacon(x, y):
    """Phase 83: the beacon block (the effect source). A cyan-glass frame
    with a bright glowing centre — the classic beacon crystal look. New
    salts 929/930/931/932, every existing texture pixel-identical."""
    # A bright white-cyan core (the nether-star light) in the centre.
    cx, cy = abs(x - 7.5), abs(y - 7.5)
    if cx <= 2 and cy <= 2:
        base = (220, 255, 255)
    elif cx <= 3.5 or cy <= 3.5:
        base = (120, 220, 235)  # the glowing ring
    else:
        base = (60, 150, 175)   # the glass frame edge
    n = noise(x, y, 929, 4)
    n2 = noise(x, y, 930, 3)
    v = n + n2
    return (clamp(base[0] + v), clamp(base[1] + v), clamp(base[2] + v), 255)



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
    # PRE-3: the ripple wander wraps the sample window INSIDE this tile
    # (the shader's tiled fract wrap), so the tile is generated PERIODIC:
    # the noise hash takes the coordinates modulo the tile size and the
    # wrap seam is water-on-water (no discontinuity beyond the noise's
    # own grain).
    return (clamp(47 + noise(x % TILE, y % TILE, 21, 10)),
            clamp(119 + noise(x % TILE, y % TILE, 22, 10)),
            clamp(189 + noise(x % TILE, y % TILE, 23, 10)), 200)


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


def gravel(x, y):
    # Gravel (Phase 59): a stone-like gray base with scattered pebbles —
    # darker 2x2 clusters and lighter flecks (the Minecraft gravel look).
    v = 105 + noise(x, y, 93, 8)
    bx, by = x // 2, y // 2
    if h2(bx, by, 94) % 10 < 3:      # dark pebble
        v -= 22 + noise(x, y, 95, 5)
    elif h2(bx, by, 96) % 14 < 2:    # light fleck
        v += 12 + noise(x, y, 97, 4)
    return (clamp(v), clamp(v), clamp(v), 255)


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


def glass_pane(x, y):
    # Phase 99: the glass pane — a 1px border frame + a translucent interior
    # (alpha 90, unlike the full glass block's alpha 0) + two highlight
    # streaks. Flat by construction (no salt needed).
    if x == 0 or y == 0 or x == TILE - 1 or y == TILE - 1:
        return (210, 235, 255, 255)
    if (x == y and 2 <= x <= 6) or (x + y == TILE - 3 and 6 <= x <= 9):
        return (255, 255, 255, 200)
    return (200, 225, 255, 90)


def fence(x, y):
    # Wooden plank base (salt 933) + a dark X brace along both diagonals
    # (salt 934). Phase 98, the fence block; every existing texture is
    # pixel-identical (new salts only).
    v = noise(x, y, 933, 8)
    r, g, b = clamp(150 + v), clamp(118 + v), clamp(74 + v)
    if x == y or x + y == TILE - 1:
        w = noise(x, y, 934, 6)
        return (clamp(r - 40 + w), clamp(g - 40 + w),
                clamp(b - 40 + w), 255)
    return (r, g, b, 255)


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


def fire(x, y):
    """Phase 64: the flame tile (the `fire` block, a cross model). A
    teardrop flame rising from the base: a deep-red rim, an orange body,
    a yellow core and a near-white heart at the bottom centre. The
    centreline leans with a deterministic per-row hash (a stylised
    flicker, static like every other texture). Transparent elsewhere."""
    if y > 15:
        return (0, 0, 0, 0)
    t = min((15 - y) / 13.0, 1.0)  # 0 at the base row, 1 at the tip
    lean = (h2(0, y, 901) % 5 - 2) * 0.35 * t  # -1.05..+1.05 px at the tip
    cx = 7.5 + lean
    half = 5.5 * (1.0 - t) ** 0.85  # narrows to a point at the tip
    if half <= 0.1:
        return (0, 0, 0, 0)
    dx = abs(x + 0.5 - cx)
    if dx >= half:
        return (0, 0, 0, 0)
    d = dx / half
    n = noise(x, y, 902, 14)
    if d < 0.32:  # the near-white heart (only near the base)
        base = (255, 236, 170) if t < 0.45 else (255, 214, 110)
    elif d < 0.62:
        base = (255, 204, 64)
    elif d < 0.85:
        base = (242, 122, 30)
    else:
        base = (196, 58, 22)
    return (clamp(base[0] + n), clamp(base[1] + n), clamp(base[2] + n), 255)


def campfire(x, y):
    """Phase 71: the campfire tile (the `campfire` block, a cross model).
    A ring of logs (rows 12..15: a horizontal log pair with dark ends)
    under a teardrop flame (rows 1..11) — the same layered palette as the
    bare `fire` tile (deep-red rim, orange body, yellow core, a near-white
    heart at the base). The flame's centreline leans with a deterministic
    per-row hash (a stylised flicker, static like every other texture).
    Transparent elsewhere."""
    # Logs: two stacked horizontal bars across the base; the left/right
    # ends are darker (log cut faces) with a per-pixel bark jitter.
    if 12 <= y <= 15:
        n = noise(x, y, 920, 9)
        if x in (1, 2, 13, 14):
            base = (96, 64, 36)
        else:
            base = (152, 106, 62) if y <= 13 else (130, 90, 52)
        return (clamp(base[0] + n), clamp(base[1] + n),
                clamp(base[2] + n), 255)
    # Flame: a teardrop rising from the log row (the log top is row 12).
    if 1 <= y <= 11:
        t = min((11 - y) / 10.0, 1.0)  # 0 at the base row, 1 at the tip
        lean = (h2(0, y, 921) % 5 - 2) * 0.3 * t  # -0.6..+0.6 px at the tip
        cx = 7.5 + lean
        half = 4.6 * (1.0 - t) ** 0.85  # narrows to a point at the tip
        if half <= 0.1:
            return (0, 0, 0, 0)
        dx = abs(x + 0.5 - cx)
        if dx >= half:
            return (0, 0, 0, 0)
        d = dx / half
        n = noise(x, y, 922, 12)
        if d < 0.32:  # the near-white heart (only near the base)
            base = (255, 236, 170) if t < 0.45 else (255, 214, 110)
        elif d < 0.62:
            base = (255, 204, 64)
        elif d < 0.85:
            base = (242, 122, 30)
        else:
            base = (196, 58, 22)
        return (clamp(base[0] + n), clamp(base[1] + n),
                clamp(base[2] + n), 255)
    return (0, 0, 0, 0)  # transparent


def campfire_extinguished(x, y):
    """Phase 75: the unlit campfire tile (the `campfire_extinguished`
    block, a cross model). The SAME log ring as the lit tile (rows
    12..15, identical pixel data — the physical logs are unchanged when
    the flame goes out), no flame. Transparent elsewhere."""
    if 12 <= y <= 15:
        n = noise(x, y, 920, 9)
        if x in (1, 2, 13, 14):
            base = (96, 64, 36)
        else:
            base = (152, 106, 62) if y <= 13 else (130, 90, 52)
        return (clamp(base[0] + n), clamp(base[1] + n),
                clamp(base[2] + n), 255)
    return (0, 0, 0, 0)  # transparent


def torch(x, y):
    """Phase 65: the torch tile (the `torch` block, a cross model). A
    brown stick (2-px column, rows 6..15) topped by a small flame (rows
    1..5): a yellow core, an orange body and a deep-red rim. The flame's
    centreline leans with a deterministic per-row hash (a stylised
    flicker, static like every other texture). Transparent elsewhere."""
    # Stick: a 2-px brown column; the base rows are slightly darker.
    if 6 <= y <= 15 and 7 <= x <= 8:
        n = noise(x, y, 910, 7)
        base = (152, 106, 62) if y <= 13 else (130, 90, 52)
        return (clamp(base[0] + n), clamp(base[1] + n),
                clamp(base[2] + n), 255)
    # Flame: a small teardrop above the stick (the stick top is row 6).
    if 1 <= y <= 5:
        t = min((5 - y) / 4.0, 1.0)  # 0 at the base row, 1 at the tip
        lean = (h2(0, y, 911) % 3 - 1) * 0.3 * t  # -0.3..+0.3 px at the tip
        cx = 7.5 + lean
        half = 2.4 * (1.0 - t) ** 0.8 + 0.5  # narrows toward the tip
        dx = abs(x + 0.5 - cx)
        if dx >= half:
            return (0, 0, 0, 0)
        d = dx / half
        n = noise(x, y, 912, 10)
        if d < 0.4:
            base = (255, 224, 120)
        elif d < 0.75:
            base = (248, 150, 40)
        else:
            base = (210, 84, 24)
        return (clamp(base[0] + n), clamp(base[1] + n),
                clamp(base[2] + n), 255)
    return (0, 0, 0, 0)  # transparent


# --- GEN-2: the tree-growth content (3 saplings + 2 trunks x2 + 2 leaves) ----
# New salts 935..956 (after the fence's 934); every existing texture stays
# pixel-identical.

def make_sapling(salt, leaf):
    """GEN-2: a small sprout (the sapling cross tile): a 2-px green stem
    from the base up to mid-tile, topped by three leaf blobs (left, right,
    centre). Transparent elsewhere — the mesher draws it as two crossing
    quads in the blending pass, the wheat/flower pattern."""
    stem = (88, 128, 52)

    def tile(x, y):
        if 7 <= x <= 8 and 10 <= y <= 15:
            n = noise(x, y, salt, 7)
            return (clamp(stem[0] + n), clamp(stem[1] + n),
                    clamp(stem[2] + n), 255)
        for (cx, cy, r2) in ((4.5, 7.0, 5.0), (10.5, 7.0, 5.0),
                             (7.5, 3.5, 4.5)):
            dx, dy = x - cx, y - cy
            if dx * dx + dy * dy <= r2:
                n = noise(x, y, salt + 1, 9)
                return (clamp(leaf[0] + n), clamp(leaf[1] + n),
                        clamp(leaf[2] + n), 255)
        return (0, 0, 0, 0)  # transparent
    return tile


def spruce_trunk_side(x, y):
    """GEN-2: the spruce trunk side — the oak bark stripe pattern in a
    darker, cooler brown (salts 941/942/943)."""
    stripe = 8 if (x // 3) % 2 == 0 else 0
    return (clamp(84 + stripe + noise(x, y, 941, 7)),
            clamp(62 + stripe + noise(x, y, 942, 7)),
            clamp(40 + stripe + noise(x, y, 943, 5)), 255)


def spruce_trunk_top(x, y):
    """GEN-2: the spruce trunk top — concentric growth rings, darker than
    the oak rings (salts 944/945/946)."""
    dx, dy = x - 7.5, y - 7.5
    ring = int((dx * dx + dy * dy) ** 0.5)
    light = 14 if ring % 2 == 0 else -10
    return (clamp(104 + light + noise(x, y, 944, 5)),
            clamp(78 + light + noise(x, y, 945, 5)),
            clamp(48 + light + noise(x, y, 946, 5)), 255)


def birch_trunk_side(x, y):
    """GEN-2: the birch trunk side — pale white bark (salt 947) with dark
    horizontal lenticel dashes (salts 948/949, a deterministic per-row
    roll) — the Minecraft birch look."""
    n = noise(x, y, 947, 6)
    if h2(x // 4, y, 948) % 7 < 2 and 3 <= y <= 13:
        m = noise(x, y, 949, 8)
        return (clamp(120 + m), clamp(116 + m), clamp(108 + m), 255)
    return (clamp(226 + n), clamp(222 + n), clamp(210 + n), 255)


def birch_trunk_top(x, y):
    """GEN-2: the birch trunk top — pale concentric rings
    (salts 950/951/952)."""
    dx, dy = x - 7.5, y - 7.5
    ring = int((dx * dx + dy * dy) ** 0.5)
    light = 8 if ring % 2 == 0 else -8
    return (clamp(214 + light + noise(x, y, 950, 4)),
            clamp(210 + light + noise(x, y, 951, 4)),
            clamp(198 + light + noise(x, y, 952, 4)), 255)


def spruce_leaves(x, y):
    """GEN-2: the spruce leaves — the oak leaf pattern (salts 17/18/19/20)
    in a darker green (salts 953/954)."""
    if h2(x, y, 953) % 100 < 12:
        return (0, 0, 0, 0)
    return (clamp(44 + noise(x, y, 954, 12)),
            clamp(96 + noise(x, y, 954, 12)),
            clamp(48 + noise(x, y, 954, 10)), 255)


def birch_leaves(x, y):
    """GEN-2: the birch leaves — a lighter, yellow-er green
    (salts 955/956)."""
    if h2(x, y, 955) % 100 < 12:
        return (0, 0, 0, 0)
    return (clamp(96 + noise(x, y, 956, 12)),
            clamp(170 + noise(x, y, 956, 12)),
            clamp(72 + noise(x, y, 956, 10)), 255)


TEXTURES = {
    **{name: make_wool_tile(name) for name in WOOL_PALETTE},
    **{"oak_sapling": make_sapling(935, (110, 165, 60)),
       "spruce_sapling": make_sapling(937, (72, 120, 52)),
       "birch_sapling": make_sapling(939, (140, 185, 80)),
       "spruce_trunk_side": spruce_trunk_side,
       "spruce_trunk_top": spruce_trunk_top,
       "birch_trunk_side": birch_trunk_side,
       "birch_trunk_top": birch_trunk_top,
       "spruce_leaves": spruce_leaves,
       "birch_leaves": birch_leaves},
    **{f"wheat_{s}": make_wheat(s, 70 + s) for s in range(1, 8)},
    "stone": stone,
    "cobblestone": cobblestone,
    "dirt": dirt,
    "farmland_top": farmland_top,
    "farmland_wet": farmland_wet,
    "farmland_side": farmland_side,
    "grass_top": grass_top,
    "grass_side": grass_side,
    "sand": sand,
    "sandstone": sandstone,
    "smooth_sandstone": smooth_sandstone,
    "iron_block": iron_block,
    "beacon": beacon,
    "wood_side": wood_side,
    "wood_top": wood_top,
    "leaves": leaves,
    "glass": glass,
    "water": water,
    "lava": lava,
    "obsidian": obsidian,
    "bedrock": bedrock,
    "gravel": gravel,
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
    "fire": fire,
    "campfire": campfire,
    "campfire_extinguished": campfire_extinguished,
    "torch": torch,
    "fence": fence,
    "glass_pane": glass_pane,
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
