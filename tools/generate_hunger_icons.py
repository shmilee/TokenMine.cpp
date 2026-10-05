#!/usr/bin/env python3
"""Generate the hunger bar drumstick strip (RGBA PNG) for Phase 23.

Deterministic: per-pixel variation comes from a splitmix64 hash, so the
output is stable across runs and platforms. Re-run after editing this file
to refresh assets/textures/hunger.png.

The HUD hunger bar is 10 drumsticks (20 hunger); each renders one of three
16x16 tiles from the 48x16 strip (tile i occupies u in [i/3, (i+1)/3]) —
the same layout as the Phase 16 health strip:
  0: full drumstick (meat + bone)
  1: half drumstick (left half colored, right half dark)
  2: empty drumstick (dark)
"""
import os
import struct
import zlib

W, H = 48, 16
OUT = os.path.join(os.path.dirname(__file__), "..", "assets",
                   "textures", "hunger.png")

# A chicken-leg silhouette (11x9), placed at offset (2, 3) inside each
# 16x16 tile (same placement as the health heart). M = meat, B = bone.
DRUM = [
    "...MMMMMM..",
    "..MMMMMMMM.",
    ".MMMMMMMMM.",
    ".MMMMMMMMM.",
    ".MMMMMMMM..",
    "..MMMMMM...",
    "...MMMM....",
    "....MM.B...",
    ".....B..B..",
]
DRUM_OX, DRUM_OY = 2, 3
SPLIT_X = 7  # half-drumstick split: tile-local x <= SPLIT_X is the left half


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


def drum_at(tx: int, ty: int):
    """Tile-local (tx, ty): 'M', 'B' (bone), or None (outside the shape)."""
    if not (0 <= tx < 16 and 0 <= ty < 16):
        return None
    gy, gx = ty - DRUM_OY, tx - DRUM_OX
    if not (0 <= gy < len(DRUM) and 0 <= gx < len(DRUM[0])):
        return None
    c = DRUM[gy][gx]
    return c if c in ("M", "B") else None


def setpx(px, x, y, color, salt, amount=8):
    r = max(0, min(255, color[0] + noise(x, y, salt, amount)))
    g = max(0, min(255, color[1] + noise(x, y, salt + 1, amount)))
    b = max(0, min(255, color[2] + noise(x, y, salt + 2, amount)))
    px[y][x] = (r, g, b, 255)


MEAT = (168, 118, 72)
MEAT_HI = (192, 142, 94)
BONE = (228, 220, 204)
BONE_HI = (244, 238, 224)
DARK = (56, 56, 62)
DARK_HI = (86, 86, 94)


def tile(kind: int) -> list:
    """One 16x16 tile: 0 full, 1 half, 2 empty."""
    px = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for ty in range(16):
        for tx in range(16):
            part = drum_at(tx, ty)
            if part is None:
                continue
            left = tx <= SPLIT_X
            if kind == 0:                      # full
                if part == "B":
                    c = BONE_HI if (tx == 7 and ty == 11) else BONE
                else:
                    c = MEAT_HI if (tx == 4 and ty == 5) or (tx == 5 and ty == 4) \
                        else MEAT
                setpx(px, tx, ty, c, salt=kind * 100 + ty * 16 + tx)
            elif kind == 1:                    # half: left colored, right dark
                if part == "B":
                    c = BONE_HI if (tx == 7 and ty == 11) else (BONE if left else DARK)
                else:
                    c = MEAT_HI if (tx == 4 and ty == 5) else (MEAT if left else DARK)
                setpx(px, tx, ty, c, salt=kind * 100 + ty * 16 + tx)
            else:                              # empty
                c = DARK_HI if (tx == 4 and ty == 5) else DARK
                setpx(px, tx, ty, c, salt=kind * 100 + ty * 16 + tx)
    return px


def main() -> None:
    px = [[(0, 0, 0, 0)] * W for _ in range(H)]
    for kind in range(3):
        t = tile(kind)
        for y in range(16):
            for x in range(16):
                px[y][kind * 16 + x] = t[y][x]
    write_png(OUT, px)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
