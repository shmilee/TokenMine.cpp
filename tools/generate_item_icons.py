#!/usr/bin/env python3
"""Generate the 16x16 item icons (RGBA PNGs) for Phase 15.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/items/.

These icons cover the explicit (non-block) items: tools and materials have
no block texture, so the hotbar / inventory / held-item UI renders their
"icon" texture (an optional data field, validated at game init) instead of
a 3D block cube. The art is a simple procedural silhouette — recognizable
shape + item color, not detailed models (see docs/KNOWN_ISSUES).
"""
import os
import struct
import zlib

SIZE = 16
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


def line(px, x0, y0, x1, y1, color, salt, width=1):
    """Bresenham line with `width` (1 or 2 px thick)."""
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
    for y in (4, 8, 12):
        for x in range(SIZE):
            setpx(px, x, y, seam, salt=9, amount=4)
    for (x, y0, y1) in ((3, 0, 4), (10, 4, 8), (6, 8, 12), (12, 12, 16)):
        for y in range(y0, min(y1, SIZE)):
            setpx(px, x, y, seam, salt=11, amount=4)
    return px


def stick():
    px = blank()
    color = (140, 100, 58)
    line(px, 2, 13, 13, 2, color, salt=5, width=2)
    # A few darker fibers along the stick.
    for (x, y) in ((3, 12), (6, 9), (9, 6), (12, 3)):
        setpx(px, x, y, (104, 74, 42), salt=13, amount=3)
    return px


def coal():
    px = blank()
    base = (46, 46, 52)
    for y in range(SIZE):
        for x in range(SIZE):
            # Irregular chunk: a rounded square eroded by the hash.
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d < 4.5 + (h2(x, y, 21) % 3) - 1:
                setpx(px, x, y, base, salt=19)
    # A few bright glints.
    for (x, y) in ((5, 5), (9, 6), (7, 9)):
        setpx(px, x, y, (96, 96, 104), salt=23, amount=4)
    return px


def gem(c1, c2, c3, salt):
    """A diamond-shaped gem: body c1, upper facet c2, lower facet c3."""
    px = blank()
    for y in range(SIZE):
        for x in range(SIZE):
            d = abs(x - 7.5) + abs(y - 7.5)
            if d > 6.2:
                continue
            if y < 6:
                setpx(px, x, y, c2, salt=salt)
            elif y > 9:
                setpx(px, x, y, c3, salt=salt + 1)
            else:
                setpx(px, x, y, c1, salt=salt + 2)
    # Facet seams.
    for x in range(3, 13):
        setpx(px, x, 7, (255, 255, 255), salt=salt + 3, amount=2)
    return px


def rotten_flesh():
    """A pile of meat: two overlapping irregular lumps, pinkish-brown."""
    px = blank()
    base = (178, 116, 106)
    for (cx, cy, r) in ((6, 9, 4.5), (10, 7, 4.0)):
        for y in range(SIZE):
            for x in range(SIZE):
                d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                if d < r + (h2(x, y, 31) % 3) - 1:
                    setpx(px, x, y, base, salt=33)
    # A few dark specks (rot).
    for (x, y) in ((5, 8), (8, 10), (10, 6), (12, 8)):
        setpx(px, x, y, (118, 70, 64), salt=35, amount=4)
    return px


def beef():
    """Raw beef (Phase 28): a dark-red chunk with pale fat marbling."""
    px = blank()
    base = (166, 60, 52)
    fat = (232, 196, 184)
    # Chunk: an irregular rounded blob spanning roughly x 2..13, y 4..13.
    for y in range(4, 14):
        for x in range(2, 14):
            d = ((x - 7.5) ** 2) / 50.0 + ((y - 8.5) ** 2) / 32.0
            if d < 1.0 + (h2(x, y, 81) % 3) * 0.15:
                setpx(px, x, y, base, salt=81)
    # Fat marbling: short pale dashes across the chunk.
    for (x0, y0, x1) in ((4, 7, 7), (7, 10, 10), (5, 12, 6), (9, 6, 11)):
        for x in range(x0, x1 + 1):
            setpx(px, x, y0, fat, salt=82, amount=4)
    return px


def mutton():
    """Raw mutton (Phase 28): a pink slab with a white fat cap."""
    px = blank()
    meat = (214, 128, 120)
    fat = (240, 224, 212)
    # Slab: rounded rectangle x 2..13, y 5..12 (corners skipped).
    for y in range(5, 13):
        for x in range(2, 14):
            if (x in (2, 13)) and (y in (5, 12)):
                continue
            if y < 7:
                setpx(px, x, y, fat, salt=83)
            else:
                setpx(px, x, y, meat, salt=84)
    # Seam between the fat cap and the meat.
    for x in range(2, 14):
        setpx(px, x, 7, (196, 108, 100), salt=85, amount=4)
    return px


def chicken():
    """Raw chicken (Phase 28): a pale drumstick — round meat on a bone."""
    px = blank()
    meat = (240, 206, 172)
    bone = (250, 246, 236)
    # Meat: a circle centered (8, 6), radius ~4.
    for y in range(SIZE):
        for x in range(SIZE):
            d = ((x - 8) ** 2 + (y - 6) ** 2) ** 0.5
            if d < 4.2 + (h2(x, y, 86) % 2) * 0.3:
                setpx(px, x, y, meat, salt=86)
    # Bone: a 2-px shaft under the meat, knobbed at the end.
    line(px, 8, 9, 8, 13, bone, salt=87, width=2)
    for (x, y) in ((7, 13), (7, 14), (10, 13), (10, 14)):
        setpx(px, x, y, bone, salt=88, amount=4)
    return px


def diamond():
    return gem((100, 225, 235), (170, 245, 250), (58, 175, 190), salt=29)


def ingot(c_base, c_bevel, salt):
    """A trapezoid ingot: narrower on top, beveled upper edge."""
    px = blank()
    for y in range(5, 13):
        t = (y - 5) / 7.0  # 0 top .. 1 bottom
        half_top, half_bot = 4.0, 6.0
        half = half_top + (half_bot - half_top) * t
        x0 = max(0, int(7.5 - half + 0.5))
        x1 = min(15, int(7.5 + half + 0.5))
        for x in range(x0, x1 + 1):
            c = c_bevel if y == 5 else c_base
            setpx(px, x, y, c, salt=salt + (0 if y == 5 else 1))
    # Bottom shadow line.
    for x in range(2, 14):
        setpx(px, x, 12, (tuple(int(v * 0.7) for v in c_base)),
              salt=salt + 2, amount=4)
    return px


def gold_ingot():
    return ingot((238, 199, 66), (255, 226, 120), salt=37)


def iron_ingot():
    return ingot((214, 214, 220), (240, 240, 246), salt=41)


def pickaxe(head_color, head_bevel, salt):
    """A pickaxe: a convex-up head arc across the top + a 2px diagonal
    handle from the head's middle to the bottom-right."""
    px = blank()
    import math
    for x in range(1, 15):
        t = (x - 1) / 13.0
        y = int(round(6.0 - 5.0 * math.sin(math.pi * t)))
        setpx(px, x, y, head_bevel, salt=salt)
        if y + 1 < SIZE:
            setpx(px, x, y + 1, head_color, salt=salt + 1)
    line(px, 7, 7, 13, 14, (146, 110, 62), salt=salt + 2, width=2)
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
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d < 5.5 + (h2(x, y, 41) % 3) - 1:
                setpx(px, x, y, rim if d > 4.2 else base, salt=41)
    # Crease lines.
    line(px, 5, 6, 10, 5, (150, 112, 70), salt=43)
    line(px, 6, 9, 11, 10, (150, 112, 70), salt=45)
    return px


def wool():
    """A square of wool (Phase 26): a white fluffy slab, same silhouette
    logic as the leather hide but white, with a slightly darker rim."""
    px = blank()
    base = (232, 230, 224)
    rim = (190, 188, 180)
    for y in range(SIZE):
        for x in range(SIZE):
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d < 5.5 + (h2(x, y, 61) % 3) - 1:
                setpx(px, x, y, rim if d > 4.2 else base, salt=61)
    return px


def feather():
    """A feather (Phase 26): a cream diagonal quill (bottom-left to
    top-right) with a darker shaft along its center."""
    px = blank()
    cream = (240, 238, 228)
    shaft = (196, 192, 178)
    for i in range(11):
        x = 2 + i
        y = 13 - i
        setpx(px, x, y, cream, salt=62)
        if 1 <= i <= 9:
            setpx(px, x, y - 1, cream, salt=63)
            setpx(px, x, y + 1, cream, salt=64)
    for i in range(3, 11, 2):
        setpx(px, 2 + i, 13 - i, shaft, salt=65)
    return px


def bow():
    px = blank()
    stave = (110, 74, 42)
    string = (206, 200, 188)
    # The string: a light vertical line on the right.
    for y in range(2, 14):
        setpx(px, 11, y, string, salt=71)
    # The stave: a brown C-arc from the top tip to the bottom tip.
    for (x, y) in ((6, 2), (5, 3), (4, 4), (3, 5), (2, 6), (2, 7), (2, 8),
                   (2, 9), (3, 10), (4, 11), (5, 12), (6, 13)):
        setpx(px, x, y, stave, salt=72)
        setpx(px, x + 1, y, (94, 62, 36), salt=73)
    # The tips connect the stave to the string.
    for x in (8, 9, 10):
        setpx(px, x, 2, stave, salt=74)
        setpx(px, x, 13, stave, salt=75)
    return px


def arrow():
    """An arrow: a 2px diagonal shaft from the bottom-left to the
    top-right, a small steel tip at the top end and light fletching at
    the bottom end (Phase 24; the bow's ammo item)."""
    px = blank()
    shaft = (140, 100, 58)
    line(px, 3, 12, 12, 3, shaft, salt=77, width=2)
    # A few darker fibers along the shaft.
    for (x, y) in ((5, 10), (8, 7), (10, 5)):
        setpx(px, x, y, (104, 74, 42), salt=78, amount=3)
    # The tip: a small steel head at the top-right end.
    tip = (206, 206, 214)
    for (x, y) in ((12, 2), (13, 2), (13, 3), (14, 2), (14, 3)):
        setpx(px, x, y, tip, salt=79)
    # The fletching: light barbs at the bottom-left end.
    fletch = (228, 222, 208)
    for (x, y) in ((2, 13), (3, 14), (1, 12), (4, 12)):
        setpx(px, x, y, fletch, salt=80)
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
    "feather": feather,
    "beef": beef,
    "mutton": mutton,
    "chicken": chicken,
}


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in ITEMS.items():
        path = os.path.join(OUT_DIR, name + ".png")
        write_png(path, fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
