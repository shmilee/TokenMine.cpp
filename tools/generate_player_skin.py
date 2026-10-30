#!/usr/bin/env python3
"""Generate the player body skin assets (UX-3).

Deterministic (pure flat-color regions, no hash at all — the output is
byte-stable across runs and platforms by construction). Re-run after
editing this file to refresh the assets.

Outputs (5 files):
  assets/textures/player/player_skin.png — the 64x64 CLASSIC-layout
    placeholder skin (the Minecraft pre-1.8 single-layer map — the
    canonical region table, verified against the official 4px-arms
    template): the head (8x8 faces, y 0..16), the body (8x12 / 4x12 /
    8x4, y 16..32), the right arm + right leg (4x12 / 4x4, y 16..32),
    the left leg + left arm (4x12 / 4x4, y 48..64). Four colors only:
    skin, green shirt (body + sleeve rows), blue pants (leg rows
    0-8), dark-gray boots (leg rows 9-11). Everything else stays
    transparent (the classic unused-space convention).
  assets/textures/mobs/player_head.png / player_body.png /
    player_arm.png / player_leg.png — the four 16x16 DERIVED part tiles
    (same directory as the mob tiles = TextureAtlas::loadDirectory picks
    them up; the block_atlas.json entries name them). Each tile crops
    its part's face regions out of the 64x64 canvas above, so the tiles
    and the skin can never drift apart.

Tile packing (the box mesh samples the WHOLE 16x16 tile on every face —
the mob single-tile convention, makeMobModelGeometry):
  head: 2x2 grid of the four 8x8 side faces (front/right on the top row,
        back/left on the bottom row) — the 8x8 faces fill 16x16 exactly
        with NO scaling. (All six 8x8 faces would need 16x24; the top
        and bottom caps are not represented — the box top/bottom faces
        show the grid, a placeholder constraint.)
  body: front + back (8x12) on the top 12 rows, top + bottom caps (8x4)
        on the bottom 4 rows — 16x16 exactly, no gaps, no scaling.
  arm / leg: the four 4x12 side faces in a 16-wide x 12-tall row
        (front/right/back/left), then the two 4x4 caps; the remaining
        8x4 corner is filled with the part's base color.
Both arms share player_arm (the Minecraft convention); both legs share
player_leg.

The canonical 64x64 classic region map (x0, y0, w, h) — the standard
pre-1.8 single-layer layout:
  head:  top (8,0)    bottom (16,0)                — 8x8
         back (0,8)   front (8,8)  left (16,8) right (24,8) — 8x8
  body:  top (20,16)  bottom (28,16)               — 8x4
         left (16,20) front (20,20)  right (28,20) back (32,20) — 4/8x12
  rarm:  top (44,16)  bottom (48,16)               — 4x4
         left (40,20) front (44,20)  back (48,20)  right (52,20) — 4x12
  rleg:  top (4,16)   bottom (8,16)                — 4x4
         left (0,20)  front (4,20)   back (8,20)   right (12,20) — 4x12
  lleg:  top (4,48)   bottom (8,48)                — 4x4
         left (0,52)  front (4,52)   back (8,52)   right (12,52) — 4x12
  larm:  top (20,48)  bottom (24,48)               — 4x4
         left (16,52) front (20,52)  back (24,52)  right (28,52) — 4x12
"""
import hashlib
import os
import struct
import zlib

SKIN_SIZE = 64
TILE_SIZE = 16

# The placeholder palette (the four flat colors — named RGBA constants,
# the no-magic-number discipline; a future real skin replaces the whole
# file).
KIN = (224, 178, 132, 255)   # skin (head, hands)
SHIRT = (84, 140, 60, 255)   # green shirt (body + sleeve rows)
PANTS = (58, 92, 160, 255)   # blue pants
BOOTS = (72, 72, 78, 255)    # dark-gray boots
TRANSPARENT = (0, 0, 0, 0)

# Side-face rows (of the 12, top down) painted per part: the arm's
# sleeve band is the top 4 rows, the leg's boot band the bottom 3.
SLEEVE_ROWS = 4
BOOT_ROWS = 3

ROOT = os.path.dirname(__file__)
SKIN_PATH = os.path.join(ROOT, "..", "assets", "textures", "player",
                         "player_skin.png")
TILE_DIR = os.path.join(ROOT, "..", "assets", "textures", "mobs")


def chunk(tag: bytes, data: bytes) -> bytes:
    c = struct.pack(">I", len(data)) + tag + data
    c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return c


def write_png(path: str, width: int, height: int, pixels) -> None:
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    raw = bytearray()
    for row in pixels:
        raw.append(0)  # filter: None
        for (r, g, b, a) in row:
            raw += bytes((r, g, b, a))
    out = (sig + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw)))
           + chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)
    print(f"wrote {path} {width}x{height} "
          f"md5={hashlib.md5(out).hexdigest()}")


def blank(size: int, color=TRANSPARENT) -> list:
    return [list(color) for _ in range(size * size)]


def put(canvas, x0, y0, w, h, color) -> None:
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            canvas[y * SKIN_SIZE + x] = list(color)


def put_side(canvas, x0, y0, w, h, top_color, bottom_color,
             split_row) -> None:
    """A 4x12 limb side face: rows 0..split_row-1 top_color, the rest
    bottom_color (the arm sleeve / the leg boot band)."""
    for y in range(y0, y0 + h):
        color = top_color if (y - y0) < split_row else bottom_color
        for x in range(x0, x0 + w):
            canvas[y * SKIN_SIZE + x] = list(color)


def face(canvas, x0, y0, w, h) -> list:
    """Crop a w x h region out of the canvas (row-major pixels)."""
    rows = []
    for y in range(y0, y0 + h):
        rows.append([tuple(canvas[(y * SKIN_SIZE) + x])
                     for x in range(x0, x0 + w)])
    return rows


def paste(tile, x, y, region) -> None:
    for ry, row in enumerate(region):
        for rx, px in enumerate(row):
            tile[(y + ry) * TILE_SIZE + (x + rx)] = list(px)


def to_rows(pixels: list) -> list:
    """A flat 16x16 pixel list -> the 16 rows write_png expects."""
    return [pixels[i * TILE_SIZE:(i + 1) * TILE_SIZE]
            for i in range(TILE_SIZE)]


def make_skin() -> list:
    canvas = blank(SKIN_SIZE)
    # Head: six 8x8 faces, all skin.
    for (x0, y0) in [(8, 0), (16, 0), (0, 8), (8, 8), (16, 8), (24, 8)]:
        put(canvas, x0, y0, 8, 8, KIN)
    # Body: shirt (front/back 8x12, sides 4x12, caps 8x4).
    for (x0, y0, w, h) in [(20, 20, 8, 12), (32, 20, 8, 12),
                           (16, 20, 4, 12), (28, 20, 4, 12),
                           (20, 16, 8, 4), (28, 16, 8, 4)]:
        put(canvas, x0, y0, w, h, SHIRT)
    # Right arm: sleeves (rows 0-3) + skin hands (rows 4-11), shirt caps.
    for (x0, y0) in [(44, 20), (52, 20), (48, 20), (40, 20)]:
        put_side(canvas, x0, y0, 4, 12, SHIRT, KIN, SLEEVE_ROWS)
    for (x0, y0) in [(44, 16), (48, 16)]:
        put(canvas, x0, y0, 4, 4, SHIRT)
    # Right leg: pants (rows 0-8) + boots (rows 9-11).
    for (x0, y0) in [(4, 20), (12, 20), (8, 20), (0, 20)]:
        put_side(canvas, x0, y0, 4, 12, PANTS, BOOTS, 12 - BOOT_ROWS)
    put(canvas, 4, 16, 4, 4, PANTS)    # top cap (the waist)
    put(canvas, 8, 16, 4, 4, BOOTS)    # bottom cap (the soles)
    # Left leg (rows 48-64): pants + boots.
    for (x0, y0) in [(4, 52), (12, 52), (8, 52), (0, 52)]:
        put_side(canvas, x0, y0, 4, 12, PANTS, BOOTS, 12 - BOOT_ROWS)
    put(canvas, 4, 48, 4, 4, PANTS)
    put(canvas, 8, 48, 4, 4, BOOTS)
    # Left arm (rows 48-64): sleeves + skin, shirt caps.
    for (x0, y0) in [(20, 52), (28, 52), (24, 52), (16, 52)]:
        put_side(canvas, x0, y0, 4, 12, SHIRT, KIN, SLEEVE_ROWS)
    for (x0, y0) in [(20, 48), (24, 48)]:
        put(canvas, x0, y0, 4, 4, SHIRT)
    return canvas


def make_head_tile(canvas) -> list:
    tile = blank(TILE_SIZE, KIN)
    # 2x2 grid of the 8x8 side faces: front/right top row, back/left
    # bottom row (the faces fill 16x16 exactly, no scaling).
    paste(tile, 0, 0, face(canvas, 8, 8, 8, 8))     # front
    paste(tile, 8, 0, face(canvas, 24, 8, 8, 8))    # right
    paste(tile, 0, 8, face(canvas, 0, 8, 8, 8))     # back
    paste(tile, 8, 8, face(canvas, 16, 8, 8, 8))    # left
    return tile


def make_body_tile(canvas) -> list:
    tile = blank(TILE_SIZE, SHIRT)
    paste(tile, 0, 0, face(canvas, 20, 20, 8, 12))   # front
    paste(tile, 8, 0, face(canvas, 32, 20, 8, 12))   # back
    paste(tile, 0, 12, face(canvas, 20, 16, 8, 4))   # top
    paste(tile, 8, 12, face(canvas, 28, 16, 8, 4))   # bottom
    return tile


def make_limb_tile(canvas, base_color,
                   sides, caps) -> list:
    """An arm/leg tile: the four 4x12 side faces (front/right/back/left)
    across the top 12 rows, the two 4x4 caps in the bottom-left, the
    remaining corner filled with the part's base color."""
    tile = blank(TILE_SIZE, base_color)
    for x, (x0, y0) in zip(range(0, 16, 4), sides):
        paste(tile, x, 0, face(canvas, x0, y0, 4, 12))
    for x, (x0, y0) in zip((0, 4), caps):
        paste(tile, x, 12, face(canvas, x0, y0, 4, 4))
    return tile


def main() -> None:
    canvas = make_skin()
    skin_rows = [canvas[i * SKIN_SIZE:(i + 1) * SKIN_SIZE]
                 for i in range(SKIN_SIZE)]
    write_png(SKIN_PATH, SKIN_SIZE, SKIN_SIZE, skin_rows)
    write_png(os.path.join(TILE_DIR, "player_head.png"), TILE_SIZE,
              TILE_SIZE, to_rows(make_head_tile(canvas)))
    write_png(os.path.join(TILE_DIR, "player_body.png"), TILE_SIZE,
              TILE_SIZE, to_rows(make_body_tile(canvas)))
    write_png(os.path.join(TILE_DIR, "player_arm.png"), TILE_SIZE,
              TILE_SIZE, to_rows(make_limb_tile(
                  canvas, KIN,
                  [(44, 20), (52, 20), (48, 20), (40, 20)],
                  [(44, 16), (48, 16)])))
    write_png(os.path.join(TILE_DIR, "player_leg.png"), TILE_SIZE,
              TILE_SIZE, to_rows(make_limb_tile(
                  canvas, PANTS,
                  [(4, 20), (12, 20), (8, 20), (0, 20)],
                  [(4, 16), (8, 16)])))


if __name__ == "__main__":
    main()
