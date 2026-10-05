#!/usr/bin/env python3
"""Generate the 16x16 mob texture tiles (RGBA PNGs) for Phase 20.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/mobs/.

Mob model boxes sample one atlas tile on all six faces (same convention as
the item cubes): the art is a flat procedural texture — base color + noise,
plus a face on the head (the -Z face of the box is the "front"). See
KNOWN_ISSUES for the static-model / no-animation simplification.
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


if __name__ == "__main__":
    main()
