#!/usr/bin/env python3
"""Generate the block-break crack overlay strip (RGBA PNG) for Phase 15.

Deterministic: crack polylines come from splitmix64 with fixed seeds, so
the output is stable across runs and platforms. Re-run after editing this
file to refresh assets/textures/cracks.png.

Image: 80x16 = five 16x16 tiles, one per crack stage (0..4). Stage s shows
the first s+1 cracks (cumulative, Minecraft-style progressive cracking).
Ink pixels are white with full alpha on a transparent background; the game
blends the current stage's tile over the target block while breaking.
"""
import os
import struct
import zlib

W, H = 80, 16
TILES = 5
OUT = os.path.join(os.path.dirname(__file__), "..", "assets",
                   "textures", "cracks.png")


def splitmix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return x ^ (x >> 31)


def chunk(tag: bytes, data: bytes) -> bytes:
    c = struct.pack(">I", len(data)) + tag + data
    c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return c


def write_png(path: str, pixels) -> None:
    """pixels: list of rows, each a list of (r, g, b, a) tuples."""
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


def make_crack(seed: int):
    """One crack as a list of (x, y) pixels: a random walk starting on a
    random edge point, biased toward the tile center."""
    rng = splitmix64(seed)
    side = rng % 4
    rng = splitmix64(rng)
    t = rng % 16
    if side == 0:
        x, y = t, 0
    elif side == 1:
        x, y = 15, t
    elif side == 2:
        x, y = t, 15
    else:
        x, y = 0, t
    pts = [(x, y)]
    for _ in range(7):
        rng = splitmix64(rng)
        dx = (rng % 3) - 1
        rng = splitmix64(rng)
        dy = (rng % 3) - 1
        # Bias toward the tile center so cracks cross the visible middle.
        if dx == 0:
            dx = 1 if x < 8 else -1
        if dy == 0:
            dy = 1 if y < 8 else -1
        x = max(0, min(15, x + dx))
        y = max(0, min(15, y + dy))
        pts.append((x, y))
    return pts


def main() -> None:
    px = [[(0, 0, 0, 0)] * W for _ in range(H)]
    for s in range(TILES):
        for k in range(s + 1):  # cumulative: stage s shows cracks 0..s
            x0 = s * 16
            for (cx, cy) in make_crack(0xC0FFEE00 + k):
                px[cy][x0 + cx] = (255, 255, 255, 255)
    write_png(OUT, px)
    print(f"wrote {OUT} ({W}x{H}, {TILES} stages)")


if __name__ == "__main__":
    main()
