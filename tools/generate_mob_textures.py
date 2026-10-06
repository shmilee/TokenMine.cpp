#!/usr/bin/env python3
"""Generate the 16x16 mob texture tiles (RGBA PNGs) for Phase 20.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/mobs/.

Mob model boxes sample one atlas tile on all six faces (same convention as
the item cubes): the art is a flat procedural texture — base color + noise,
plus a face on the head (the -Z face of the box is the "front"). See
docs/KNOWN_ISSUES for the static-model / no-animation simplification.
"""
import os
import struct
import zlib

SIZE = 16
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets",
                       "textures", "mobs")


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


def clamp(c: int) -> int:
    return max(0, min(255, c))


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
    print(f"wrote {path}")


def skin_tile(salt: int, base, eyes=False) -> list:
    """Zombie skin: green base + noise; the -Z "front" face also gets the
    two dark eyes (rows 5-6)."""
    (br, bg, bb) = base
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if eyes and 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                (r, g, b) = (35, 45, 30)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def cow_fur_tile(salt: int, base, eyes=False) -> list:
    """Cow fur (Phase 22): brown base with white mottled patches (the hash
    decides the patch boundaries); the head also gets two dark eyes
    (rows 5-6)."""
    (br, bg, bb) = base
    (wr, wg, wb) = (228, 222, 210)
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            patch = h2(x, y, salt + 7) % 100 < 26
            n = noise(x, y, salt, 10)
            if patch:
                r, g, b = clamp(wr + n), clamp(wg + n), clamp(wb + n)
            else:
                r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if eyes and 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                (r, g, b) = (40, 28, 20)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def sheep_wool_tile(salt: int, base, eyes=False) -> list:
    """Sheep wool (Phase 26): white base with a touch more noise than skin
    (fluff); the head also gets two dark neutral eyes (rows 5-6)."""
    (br, bg, bb) = base
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 12)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if eyes and 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                (r, g, b) = (40, 38, 34)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def chicken_tile(salt: int, base, face=False) -> list:
    """Chicken plumage (Phase 26): cream base + noise; the head ("face")
    also gets two dark eyes (rows 5-6), a red comb (rows 1-2) and an
    orange beak (rows 7-8, center column)."""
    (br, bg, bb) = base
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 8)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if face:
                if 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                    (r, g, b) = (35, 30, 25)
                elif 1 <= y <= 2 and 6 <= x <= 9:
                    (r, g, b) = (178, 44, 38)       # red comb
                elif 7 <= y <= 8 and 7 <= x <= 8:
                    (r, g, b) = (228, 148, 44)      # orange beak
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def skeleton_tile(salt: int, base, skull=False) -> list:
    """Skeleton bones (Phase 27): bone-white base + noise; the skull also
    gets two dark eye sockets (rows 5-6) and a dark nasal cavity (rows
    7-8, center)."""
    (br, bg, bb) = base
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if skull:
                if 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                    (r, g, b) = (25, 22, 20)       # eye sockets
                elif 7 <= y <= 8 and 7 <= x <= 8:
                    (r, g, b) = (35, 32, 28)       # nasal cavity
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def shirt_tile(salt: int, base) -> list:
    (br, bg, bb) = base
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    # Green zombie skin (eyes on the front face) + teal shirt.
    write_png(os.path.join(OUT_DIR, "zombie_head.png"),
              skin_tile(0x5EED, (110, 168, 84), eyes=True))
    write_png(os.path.join(OUT_DIR, "zombie_body.png"),
              shirt_tile(0x5EED, (52, 92, 112)))
    # Brown-and-white cow fur (eyes on the head's front face).
    write_png(os.path.join(OUT_DIR, "cow_body.png"),
              cow_fur_tile(0xC0F1, (122, 88, 62)))
    write_png(os.path.join(OUT_DIR, "cow_head.png"),
              cow_fur_tile(0xC0F2, (122, 88, 62), eyes=True))
    # White sheep wool (Phase 26; eyes on the head's front face).
    write_png(os.path.join(OUT_DIR, "sheep_body.png"),
              sheep_wool_tile(0x5EE1, (228, 226, 220)))
    write_png(os.path.join(OUT_DIR, "sheep_head.png"),
              sheep_wool_tile(0x5EE2, (228, 226, 220), eyes=True))
    # Cream chicken plumage (Phase 26; eyes + comb + beak on the head's
    # front face).
    write_png(os.path.join(OUT_DIR, "chicken_body.png"),
              chicken_tile(0xC1C1, (238, 236, 226)))
    write_png(os.path.join(OUT_DIR, "chicken_head.png"),
              chicken_tile(0xC1C2, (238, 236, 226), face=True))
    # Bone-white skeleton (Phase 27; skull sockets on the head's front
    # face).
    write_png(os.path.join(OUT_DIR, "skeleton_body.png"),
              skeleton_tile(0x5C11, (205, 203, 193)))
    write_png(os.path.join(OUT_DIR, "skeleton_head.png"),
              skeleton_tile(0x5C12, (222, 220, 210), skull=True))


if __name__ == "__main__":
    main()
