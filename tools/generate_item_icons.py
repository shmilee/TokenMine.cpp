#!/usr/bin/env python3
"""Generate the 32x32 item icons (RGBA PNGs) for Phase 35.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/items/.

These icons cover the explicit (non-block) items: tools and materials have
no block texture, so the hotbar / inventory / held-item UI renders their
"icon" texture (an optional data field, validated at game init) instead of
a 3D block cube. The art is a simple procedural silhouette — recognizable
shape + item color, not detailed models (see docs/KNOWN_ISSUES).

Phase 35: the icons are re-sampled at 32x32 (2x the original 16x16 design
space): shape predicates are evaluated at 2x resolution, so arcs, circles
and diagonals gain real edge detail instead of 2x2 upscaled blocks, and
the per-pixel noise is 4x denser.
"""
import os
import struct
import zlib

SIZE = 32
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets",
                       "textures", "items")


def splitmix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return x ^ (x >> 31)


def h2(x: int, y: int, salt: int) -> int:
    """Deterministic 32-bit hash in [0, 2^32)."""
    return splitmix64(x * 374761393 + y * 668265263 + salt * 2246822519)


def noise(x: int, y: int, salt: int, amount: int) -> int:
    """Centered noise in [-amount, +amount]."""
    return (h2(x, y, salt) % (2 * amount + 1)) - amount


def chunk(tag: bytes, data: bytes) -> bytes:
    c = struct.pack(">I", len(data)) + tag + data
    c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return c


def write_png(path: str, pixels) -> None:
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
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


def blank():
    return [[(0, 0, 0, 0)] * SIZE for _ in range(SIZE)]


def setpx(px, x, y, color, salt, amount=10):
    """Write a pixel with per-pixel noise around `color` (r,g,b)."""
    if not (0 <= x < SIZE and 0 <= y < SIZE):
        return
    r = max(0, min(255, color[0] + noise(x, y, salt, amount)))
    g = max(0, min(255, color[1] + noise(x, y, salt + 1, amount)))
    b = max(0, min(255, color[2] + noise(x, y, salt + 2, amount)))
    px[y][x] = (r, g, b, 255)


def setpx2(px, x, y, color, salt, amount=10):
    """Write the 2x2 block over design pixel (x, y) (the original 16x16
    art's single-pixel features, doubled)."""
    for ox in range(2):
        for oy in range(2):
            setpx(px, 2 * x + ox, 2 * y + oy, color, salt, amount)


def line(px, x0, y0, x1, y1, color, salt, width=1):
    """Bresenham line with `width` px thick (2x the 16x16 design width)."""
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        for ox in range(width):
            for oy in range(width):
                setpx(px, x0 + ox, y0 + oy, color, salt + ox * 31 + oy * 17)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def planks():
    px = blank()
    base = (168, 126, 74)
    for y in range(SIZE):
        for x in range(SIZE):
            setpx(px, x, y, base, salt=7)
    # Three horizontal seams (plank edges) + staggered vertical joints.
    seam = (122, 88, 50)
    for y in (8, 16, 24):
        for x in range(SIZE):
            setpx(px, x, y, seam, salt=9, amount=4)
    for (x, y0, y1) in ((6, 0, 8), (20, 8, 16), (12, 16, 24), (24, 24, 32)):
        for y in range(y0, min(y1, SIZE)):
            setpx(px, x, y, seam, salt=11, amount=4)
    return px


def stick():
    px = blank()
    color = (140, 100, 58)
    line(px, 4, 26, 26, 4, color, salt=5, width=4)
    # A few darker fibers along the stick.
    for (x, y) in ((3, 12), (6, 9), (9, 6), (12, 3)):
        setpx2(px, x, y, (104, 74, 42), salt=13, amount=3)
    return px


def coal():
    px = blank()
    base = (46, 46, 52)
    for y in range(SIZE):
        for x in range(SIZE):
            # Irregular chunk: a rounded square eroded by the hash.
            d = max(abs(x - 15.0), abs(y - 15.0))
            if d < 9.0 + (h2(x, y, 21) % 3) * 2 - 2:
                setpx(px, x, y, base, salt=19)
    # A few bright glints.
    for (x, y) in ((5, 5), (9, 6), (7, 9)):
        setpx2(px, x, y, (96, 96, 104), salt=23, amount=4)
    return px


def gem(c1, c2, c3, salt):
    """A diamond-shaped gem: body c1, upper facet c2, lower facet c3."""
    px = blank()
    for y in range(SIZE):
        for x in range(SIZE):
            d = abs(x - 15.0) + abs(y - 15.0)
            if d > 12.4:
                continue
            if y < 12:
                setpx(px, x, y, c2, salt=salt)
            elif y > 18:
                setpx(px, x, y, c3, salt=salt + 1)
            else:
                setpx(px, x, y, c1, salt=salt + 2)
    # Facet seams.
    for x in range(6, 26):
        setpx(px, x, 14, (255, 255, 255), salt=salt + 3, amount=2)
    return px


def rotten_flesh():
    """A pile of meat: two overlapping irregular lumps, pinkish-brown."""
    px = blank()
    base = (178, 116, 106)
    for (cx, cy, r) in ((12, 18, 9.0), (20, 14, 8.0)):
        for y in range(SIZE):
            for x in range(SIZE):
                d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                if d < r + (h2(x, y, 31) % 3) * 2 - 2:
                    setpx(px, x, y, base, salt=33)
    # A few dark specks (rot).
    for (x, y) in ((5, 8), (8, 10), (10, 6), (12, 8)):
        setpx2(px, x, y, (118, 70, 64), salt=35, amount=4)
    return px


def beef():
    """Raw beef (Phase 28): a dark-red chunk with pale fat marbling."""
    px = blank()
    base = (166, 60, 52)
    fat = (232, 196, 184)
    # Chunk: an irregular rounded blob spanning roughly x 4..27, y 8..27.
    for y in range(8, 28):
        for x in range(4, 28):
            d = ((x - 15.0) ** 2) / 200.0 + ((y - 17.0) ** 2) / 128.0
            if d < 1.0 + (h2(x, y, 81) % 3) * 0.3:
                setpx(px, x, y, base, salt=81)
    # Fat marbling: short pale dashes across the chunk.
    for (x0, y0, x1) in ((8, 14, 14), (14, 20, 20), (10, 24, 12),
                         (18, 12, 22)):
        for x in range(x0, x1 + 1):
            setpx(px, x, y0, fat, salt=82, amount=4)
    return px


def mutton():
    """Raw mutton (Phase 28): a pink slab with a white fat cap."""
    px = blank()
    meat = (214, 128, 120)
    fat = (240, 224, 212)
    # Slab: rounded rectangle x 4..27, y 10..25 (corners skipped).
    for y in range(10, 26):
        for x in range(4, 28):
            if (x in (4, 27)) and (y in (10, 25)):
                continue
            if y < 14:
                setpx(px, x, y, fat, salt=83)
            else:
                setpx(px, x, y, meat, salt=84)
    # Seam between the fat cap and the meat.
    for x in range(4, 28):
        setpx(px, x, 14, (196, 108, 100), salt=85, amount=4)
    return px


def chicken():
    """Raw chicken (Phase 28): a pale drumstick — round meat on a bone."""
    px = blank()
    meat = (240, 206, 172)
    bone = (250, 246, 236)
    # Meat: a circle centered (16, 12), radius ~8.
    for y in range(SIZE):
        for x in range(SIZE):
            d = ((x - 16) ** 2 + (y - 12) ** 2) ** 0.5
            if d < 8.4 + (h2(x, y, 86) % 2) * 0.6:
                setpx(px, x, y, meat, salt=86)
    # Bone: a 4-px shaft under the meat, knobbed at the end.
    line(px, 16, 18, 16, 26, bone, salt=87, width=4)
    for (x, y) in ((7, 13), (7, 14), (10, 13), (10, 14)):
        setpx2(px, x, y, bone, salt=88, amount=4)
    return px


def cooked_beef():
    """Cooked beef (Phase 40): a seared brown steak with char marks."""
    px = blank()
    base = (139, 82, 58)
    char = (74, 45, 32)
    # Chunk: the raw beef's blob, browned.
    for y in range(8, 28):
        for x in range(4, 28):
            d = ((x - 15.0) ** 2) / 200.0 + ((y - 17.0) ** 2) / 128.0
            if d < 1.0 + (h2(x, y, 89) % 3) * 0.3:
                setpx(px, x, y, base, salt=89)
    # Char marks: the fat-marl positions, now dark.
    for (x0, y0, x1) in ((8, 14, 14), (14, 20, 20), (10, 24, 12),
                         (18, 12, 22)):
        for x in range(x0, x1 + 1):
            setpx(px, x, y0, char, salt=90, amount=4)
    return px


def cooked_mutton():
    """Cooked mutton (Phase 40): a browned slab, fat cap crisped."""
    px = blank()
    meat = (156, 96, 72)
    fat = (222, 186, 148)
    # Slab: the raw mutton's shape, browned.
    for y in range(10, 26):
        for x in range(4, 28):
            if (x in (4, 27)) and (y in (10, 25)):
                continue
            if y < 14:
                setpx(px, x, y, fat, salt=91)
            else:
                setpx(px, x, y, meat, salt=92)
    # Seam between the fat cap and the meat.
    for x in range(4, 28):
        setpx(px, x, 14, (110, 66, 50), salt=93, amount=4)
    return px


def cooked_chicken():
    """Cooked chicken (Phase 40): a golden-brown drumstick on a bone."""
    px = blank()
    meat = (188, 134, 84)
    bone = (250, 246, 236)
    # Meat: the raw chicken's circle, browned.
    for y in range(SIZE):
        for x in range(SIZE):
            d = ((x - 16) ** 2 + (y - 12) ** 2) ** 0.5
            if d < 8.4 + (h2(x, y, 94) % 2) * 0.6:
                setpx(px, x, y, meat, salt=94)
    # Bone: a 4-px shaft under the meat, knobbed at the end.
    line(px, 16, 18, 16, 26, bone, salt=95, width=4)
    for (x, y) in ((7, 13), (7, 14), (10, 13), (10, 14)):
        setpx2(px, x, y, bone, salt=96, amount=4)
    return px


def diamond():
    return gem((100, 225, 235), (170, 245, 250), (58, 175, 190), salt=29)


def ingot(c_base, c_bevel, salt):
    """A trapezoid ingot: narrower on top, beveled upper edge."""
    px = blank()
    for y in range(10, 26):
        t = (y - 10) / 14.0  # 0 top .. 1 bottom
        half_top, half_bot = 8.0, 12.0
        half = half_top + (half_bot - half_top) * t
        x0 = max(0, int(15.0 - half + 0.5))
        x1 = min(31, int(15.0 + half + 0.5))
        for x in range(x0, x1 + 1):
            c = c_bevel if y < 12 else c_base
            setpx(px, x, y, c, salt=salt + (0 if y < 12 else 1))
    # Bottom shadow line.
    for x in range(4, 28):
        setpx(px, x, 24, (tuple(int(v * 0.7) for v in c_base)),
              salt=salt + 2, amount=4)
    return px


def gold_ingot():
    return ingot((238, 199, 66), (255, 226, 120), salt=37)


def iron_ingot():
    return ingot((214, 214, 220), (240, 240, 246), salt=41)


def pickaxe(head_color, head_bevel, salt):
    """A pickaxe: a convex-up head arc across the top + a 4px diagonal
    handle from the head's middle to the bottom-right."""
    px = blank()
    import math
    for x in range(2, 30):
        t = (x - 2) / 28.0
        y = int(round(12.0 - 10.0 * math.sin(math.pi * t)))
        for oy in range(4):
            setpx(px, x, y + oy, head_bevel if oy < 2 else head_color,
                  salt=salt + (0 if oy < 2 else 1))
    line(px, 14, 14, 26, 28, (146, 110, 62), salt=salt + 2, width=4)
    return px


def wooden_pickaxe():
    return pickaxe((150, 114, 66), (190, 152, 96), salt=47)


def iron_pickaxe():
    return pickaxe((206, 206, 212), (238, 238, 244), salt=53)


def leather():
    """A piece of leather hide: a tan irregular slab with a darker rim and
    a few crease lines."""
    px = blank()
    base = (196, 158, 108)
    rim = (140, 104, 62)
    for y in range(SIZE):
        for x in range(SIZE):
            d = max(abs(x - 15.0), abs(y - 15.0))
            if d < 11.0 + (h2(x, y, 41) % 3) * 2 - 2:
                setpx(px, x, y, rim if d > 8.4 else base, salt=41)
    # Crease lines.
    line(px, 10, 12, 20, 10, (150, 112, 70), salt=43, width=2)
    line(px, 12, 18, 22, 20, (150, 112, 70), salt=45, width=2)
    return px


def wool():
    """A square of wool (Phase 26): a white fluffy slab, same silhouette
    logic as the leather hide but white, with a slightly darker rim."""
    px = blank()
    base = (232, 230, 224)
    rim = (190, 188, 180)
    for y in range(SIZE):
        for x in range(SIZE):
            d = max(abs(x - 15.0), abs(y - 15.0))
            if d < 11.0 + (h2(x, y, 61) % 3) * 2 - 2:
                setpx(px, x, y, rim if d > 8.4 else base, salt=61)
    return px


def bone():
    """A bone (Phase 38): a white diagonal shaft (bottom-left to
    top-right) with a pair of rounded knobs at each end — the
    Minecraft bone silhouette, same diagonal as the feather family."""
    px = blank()
    white = (245, 243, 235)
    # Shaft: (10,22) to (22,10), 3 px thick.
    line(px, 10, 22, 22, 10, white, salt=63, width=3)
    # Knobs: two lobes at each end, perpendicular to the shaft.
    for (cx, cy) in ((9, 23), (23, 9)):
        for (ox, oy) in ((-2, -2), (2, 2)):
            lx, ly = cx + ox, cy + oy
            for y in range(SIZE):
                for x in range(SIZE):
                    if (x - lx) ** 2 + (y - ly) ** 2 < 6:
                        setpx(px, x, y, white, salt=63)
    return px


def feather():
    """A feather (Phase 26): a cream diagonal quill (bottom-left to
    top-right) with a darker shaft along its center."""
    px = blank()
    cream = (240, 238, 228)
    shaft = (196, 192, 178)
    for i in range(21):
        x = 4 + i
        y = 26 - i
        setpx(px, x, y, cream, salt=62)
        if 2 <= i <= 18:
            for oy in (-2, -1, 1, 2):
                setpx(px, x, y + oy, cream, salt=63 + abs(oy))
    for i in range(6, 20, 4):
        setpx(px, 4 + i, 26 - i, shaft, salt=65)
    return px


def bow():
    px = blank()
    stave = (110, 74, 42)
    string = (206, 200, 188)
    # The string: a light vertical line on the right.
    for y in range(4, 28):
        setpx(px, 22, y, string, salt=71)
    # The stave: a brown C-arc from the top tip to the bottom tip (4 px
    # wide: light outer band + dark inner band).
    for (x, y) in ((12, 4), (10, 6), (8, 8), (6, 10), (4, 12), (4, 14),
                   (4, 16), (4, 18), (6, 20), (8, 22), (10, 24), (12, 26)):
        setpx(px, x, y, stave, salt=72)
        setpx(px, x + 1, y, stave, salt=72)
        setpx(px, x + 2, y, (94, 62, 36), salt=73)
        setpx(px, x + 3, y, (94, 62, 36), salt=73)
    # The tips connect the stave to the string.
    for x in range(16, 22):
        setpx(px, x, 4, stave, salt=74)
        setpx(px, x, 26, stave, salt=75)
    return px


def arrow():
    """An arrow: a 4px diagonal shaft from the bottom-left to the
    top-right, a steel tip at the top end and light fletching at the
    bottom end (Phase 24; the bow's ammo item)."""
    px = blank()
    shaft = (140, 100, 58)
    line(px, 6, 24, 24, 6, shaft, salt=77, width=4)
    # A few darker fibers along the shaft.
    for (x, y) in ((5, 10), (8, 7), (10, 5)):
        setpx2(px, x, y, (104, 74, 42), salt=78, amount=3)
    # The tip: a steel head at the top-right end.
    tip = (206, 206, 214)
    for (x, y) in ((12, 2), (13, 2), (13, 3), (14, 2), (14, 3)):
        setpx2(px, x, y, tip, salt=79)
    # The fletching: light barbs at the bottom-left end.
    fletch = (228, 222, 208)
    for (x, y) in ((2, 13), (3, 14), (1, 12), (4, 12)):
        setpx2(px, x, y, fletch, salt=80)
    return px


ITEMS = {
    "planks": planks,
    "stick": stick,
    "rotten_flesh": rotten_flesh,
    "coal": coal,
    "diamond": diamond,
    "gold_ingot": gold_ingot,
    "iron_ingot": iron_ingot,
    "wooden_pickaxe": wooden_pickaxe,
    "iron_pickaxe": iron_pickaxe,
    "bow": bow,
    "arrow": arrow,
    "leather": leather,
    "wool": wool,
    "bone": bone,
    "feather": feather,
    "beef": beef,
    "mutton": mutton,
    "chicken": chicken,
    "cooked_beef": cooked_beef,
    "cooked_mutton": cooked_mutton,
    "cooked_chicken": cooked_chicken,
}


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in ITEMS.items():
        path = os.path.join(OUT_DIR, name + ".png")
        write_png(path, fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
