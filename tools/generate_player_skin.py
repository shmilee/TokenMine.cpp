#!/usr/bin/env python3
"""Generate the player body skin assets (UX-3; V2-2 Steve skin).

Deterministic (explicit pixel data, no hash at all — the output is
byte-stable across runs and platforms by construction). Re-run after
editing this file to refresh the assets.

Outputs (8 files):
  assets/textures/player/player_skin.png — the 64x64 CLASSIC-layout
    Steve skin (the Minecraft pre-1.8 single-layer map — the canonical
    region table below, verified against the official 4px-arms template).
    Palette (the named constants): brown hair (dark + light), skin (+
    shade), eye white, eye blue, mouth, cyan shirt (+ dark collar/hem),
    blue pants, brown boots. Everything outside the table stays
    transparent (the classic unused-space convention).
  assets/textures/mobs/ph_face.png / ph_hair.png / ph_side.png /
    pb_shirt.png / pa_arm.png / pl_pants.png / pl_boot.png — the seven
    16x16 PER-FACE derived part tiles (same directory as the mob tiles =
    TextureAtlas::loadDirectory picks them up; the block_atlas.json
    entries name them). Each tile upscales ONE canvas region with
    nearest-neighbor floor mapping (dest (x,y) <- src
    (x*sw//dw, y*sh//dh)), so the flat-color regions (shirt / pants /
    boots / hair bands) carry over without distortion and the 8x8 head
    faces land on exact 2x pixels (the facial features are >= 2 source
    px wide, so the 2x tile keeps them readable — the E2E gate can
    classify eye white / eye blue / hair brown). Each box of
    data/player_body.json selects its per-face tile through the
    MobModel `faces` overrides (V2-2, makeMobModelGeometry).

The Steve design (V2-2, the user's "make it look like Steve" request):
  head front (8,8) 8x8 — the FACE, explicit pixels (rows top->bottom,
    H = hair, S = skin, B = eye blue (outer column), W = eye white
    (inner/nose-side column), N = skin shade (nose), M = mouth):
        row 0: H H H H H H H H     hair cap
        row 1: H H H H H H H H     hair cap
        row 2: H S S S S S S H     sideburn columns begin
        row 3: H S B W W B S H     2x2 eyes (outer blue, inner white)
        row 4: H S B W W B S H     2x2 eyes
        row 5: H S S N N S S H     2 px nose shade (the sideburns end)
        row 6: S S S M M S S S     2 px mouth row
        row 7: S S S S S S S S
    Hair tones: rows 0-1 the dark brown, the side columns (rows 2-5)
    the light brown. The head back (0,8) and both sides (16,8)/(24,8)
    are ALL HAIR in a 2x2 two-tone block checker (dark/light); the
    side faces carry the 2x1 px skin EARS at (x 3..4, y 4); the top
    (8,0) and bottom (16,0) caps are flat dark hair.
  body 8x12 front (20,20) / back (32,20) + 4x12 sides (16,20)/(28,20):
    the cyan shirt, row 0 = the dark collar band, row 11 = the dark hem
    band; the 8x4 caps (20,16)/(28,16) flat shirt.
  arms 4x12 all four sides (r: 40/44/48/52,20; l: 16/20/24/28,52):
    rows 0-3 shirt sleeves, row 4 the skin-shade transition, rows 5-11
    skin hands; the 4x4 caps (r: 44/48,16; l: 20/24,48) flat skin.
  legs 4x12 all four sides (r: 0/4/8/12,20; l: 0/4/8/12,52):
    rows 0-8 blue pants, rows 9-11 brown boots; top caps (4,16)/(4,48)
    flat pants, bottom caps (8,16)/(8,48) flat boots.

The canonical 64x64 classic region map (x0, y0, w, h) — the standard
pre-1.8 single-layer layout (V2-2 unchanged):
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

V2-2 retired the four UX-3 placeholder tiles (player_head/body/arm/leg
.png + their block_atlas.json cells): the 2x2-grid packing blurred the
face to sub-pixel features (no visible eyes or hair — the root cause of
the user's report). The tiles are regenerated from the canvas as the
seven per-face 16x16 tiles above (the canvas regions are the same
table; the packing is 1:1 per face).
"""
import hashlib
import os
import struct
import zlib

SKIN_SIZE = 64
TILE_SIZE = 16

# The Steve palette (the named RGBA constants — the no-magic-number
# discipline).
HAIR_DARK = (66, 49, 35, 255)     # brown hair (cap, top, bottom)
HAIR_LIGHT = (84, 63, 44, 255)    # brown hair (sideburns, checker tone)
SKIN = (164, 140, 116, 255)       # skin (face, hands, ears)
SKIN_SHADE = (140, 116, 94, 255)  # skin shade (nose, sleeve transition)
EYE_WHITE = (244, 244, 244, 255)  # eye white (the inner/nose-side column)
EYE_BLUE = (48, 98, 170, 255)     # eye iris (the outer column)
MOUTH = (112, 88, 70, 255)        # mouth row
SHIRT = (52, 148, 152, 255)       # Steve's cyan shirt
SHIRT_DARK = (38, 110, 114, 255)  # shirt collar + hem bands
PANTS = (58, 66, 140, 255)        # blue pants
BOOTS = (84, 64, 44, 255)         # brown boots
TRANSPARENT = (0, 0, 0, 0)

# The 8x8 Steve face (the canvas (8,8) region) — the explicit pixel
# grid from the file head (H hair, S skin, B eye blue, W eye white,
# N nose shade, M mouth).
STEVE_FACE = [
    "HHHHHHHH",  # y0: hair cap
    "HHHHHHHH",  # y1: hair cap
    "HSSSSSSH",  # y2: sideburns
    "HSBWWBSH",  # y3: 2x2 eyes
    "HSBWWBSH",  # y4: 2x2 eyes
    "HSSNNSSH",  # y5: nose (2 px)
    "SSSMMSSS",  # y6: mouth (2 px)
    "SSSSSSSS",  # y7
]
# The face character -> color map (the hair's two tones are positional:
# the cap rows are dark, the sideburn columns light).
FACE_COLORS = {
    "S": SKIN,
    "B": EYE_BLUE,
    "W": EYE_WHITE,
    "N": SKIN_SHADE,
    "M": MOUTH,
}

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


def paint_band(canvas, x0, y0, w, h, rows) -> None:
    """A w x h region with per-row colors (rows: a list of h colors,
    top to bottom) — the torso / limb flat bands."""
    for gy in range(h):
        for gx in range(w):
            canvas[(y0 + gy) * SKIN_SIZE + x0 + gx] = list(rows[gy])


def paint_face(canvas, x0, y0) -> None:
    """The 8x8 Steve front face from the explicit STEVE_FACE grid
    (the file head): the hair cap rows are the dark tone, the
    sideburn columns the light tone."""
    for gy, row in enumerate(STEVE_FACE):
        for gx, ch in enumerate(row):
            if ch == "H":
                color = HAIR_DARK if gy < 2 else HAIR_LIGHT
            else:
                color = FACE_COLORS[ch]
            canvas[(y0 + gy) * SKIN_SIZE + x0 + gx] = list(color)


def paint_hair_checker(canvas, x0, y0, size=8) -> None:
    """An all-hair region in 2x2 two-tone blocks (the head back/sides:
    the two-tone partition of the file head)."""
    for gy in range(size):
        for gx in range(size):
            color = (HAIR_DARK if ((gx // 2) + (gy // 2)) % 2 == 0
                     else HAIR_LIGHT)
            canvas[(y0 + gy) * SKIN_SIZE + x0 + gx] = list(color)


def paint_side(canvas, x0, y0) -> None:
    """A head side face: the hair checker + the 2x1 px skin ear at
    (x 3..4, y 4) (the file head)."""
    paint_hair_checker(canvas, x0, y0, 8)
    for gx in (3, 4):
        canvas[(y0 + 4) * SKIN_SIZE + x0 + gx] = list(SKIN)


def face(canvas, x0, y0, w, h) -> list:
    """Crop a w x h region out of the canvas (row-major pixels)."""
    rows = []
    for y in range(y0, y0 + h):
        rows.append([tuple(canvas[(y * SKIN_SIZE) + x])
                     for x in range(x0, x0 + w)])
    return rows


def upscale(rows, sw, sh, dw=16, dh=16) -> list:
    """Nearest-neighbor upscale of an sw x sh region (list of rows) to
    dw x dh: dest (x, y) <- src (x*sw//dw, y*sh//dh). Deterministic
    floor mapping — the flat-color regions (shirt / pants / boots /
    hair bands) carry over without distortion (the 8x8 head faces are
    exact 2x pixels)."""
    out = []
    for y in range(dh):
        sy = (y * sh) // dh
        srow = rows[sy]
        out.append([list(srow[(x * sw) // dw]) for x in range(dw)])
    return out


def tile_from(canvas, x0, y0, sw, sh, dw=TILE_SIZE, dh=TILE_SIZE) -> list:
    """The 16x16 tile rows for one canvas region (the V2-2 per-face
    packing: one face per tile, no grids)."""
    return upscale(face(canvas, x0, y0, sw, sh), sw, sh, dw, dh)


def make_skin() -> list:
    canvas = blank(SKIN_SIZE)
    # Head: the Steve face (front (8,8)) + the hair (back/top/bottom
    # caps, the side faces with the 2x1 skin ears).
    paint_face(canvas, 8, 8)                    # front
    paint_hair_checker(canvas, 0, 8)            # back (two-tone)
    put(canvas, 8, 0, 8, 8, HAIR_DARK)          # top cap
    put(canvas, 16, 0, 8, 8, HAIR_DARK)         # bottom cap
    paint_side(canvas, 16, 8)                   # left
    paint_side(canvas, 24, 8)                   # right
    # Body: the shirt (front/back 8x12, sides 4x12) with the dark
    # collar (row 0) + hem (row 11) bands; flat shirt caps.
    torso_rows = [SHIRT_DARK] + [SHIRT] * 10 + [SHIRT_DARK]
    paint_band(canvas, 20, 20, 8, 12, torso_rows)   # front
    paint_band(canvas, 32, 20, 8, 12, torso_rows)   # back
    paint_band(canvas, 16, 20, 4, 12, torso_rows)   # left
    paint_band(canvas, 28, 20, 4, 12, torso_rows)   # right
    put(canvas, 20, 16, 8, 4, SHIRT)               # top cap
    put(canvas, 28, 16, 8, 4, SHIRT)               # bottom cap
    # Arms: sleeves (rows 0-3) + the shade transition (row 4) + skin
    # hands (rows 5-11); flat skin caps.
    arm_rows = [SHIRT] * 4 + [SKIN_SHADE] + [SKIN] * 7
    for (x0, y0) in [(44, 20), (48, 20), (52, 20), (40, 20)]:
        paint_band(canvas, x0, y0, 4, 12, arm_rows)
    put(canvas, 44, 16, 4, 4, SKIN)
    put(canvas, 48, 16, 4, 4, SKIN)
    for (x0, y0) in [(20, 52), (24, 52), (28, 52), (16, 52)]:
        paint_band(canvas, x0, y0, 4, 12, arm_rows)
    put(canvas, 20, 48, 4, 4, SKIN)
    put(canvas, 24, 48, 4, 4, SKIN)
    # Legs: pants (rows 0-8) + boots (rows 9-11); top cap pants,
    # bottom cap boots.
    leg_rows = [PANTS] * 9 + [BOOTS] * 3
    for (x0, y0) in [(4, 20), (8, 20), (12, 20), (0, 20)]:
        paint_band(canvas, x0, y0, 4, 12, leg_rows)
    put(canvas, 4, 16, 4, 4, PANTS)    # top cap (the waist)
    put(canvas, 8, 16, 4, 4, BOOTS)    # bottom cap (the soles)
    for (x0, y0) in [(4, 52), (8, 52), (12, 52), (0, 52)]:
        paint_band(canvas, x0, y0, 4, 12, leg_rows)
    put(canvas, 4, 48, 4, 4, PANTS)
    put(canvas, 8, 48, 4, 4, BOOTS)
    return canvas


def main() -> None:
    canvas = make_skin()
    skin_rows = [canvas[i * SKIN_SIZE:(i + 1) * SKIN_SIZE]
                 for i in range(SKIN_SIZE)]
    write_png(SKIN_PATH, SKIN_SIZE, SKIN_SIZE, skin_rows)
    # The seven V2-2 per-face tiles (the retired UX-3 2x2-grid tiles
    # are no longer written — see the file head).
    write_png(os.path.join(TILE_DIR, "ph_face.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 8, 8, 8, 8))     # head front, 2x
    write_png(os.path.join(TILE_DIR, "ph_hair.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 0, 8, 8, 8))     # head back, 2x
    write_png(os.path.join(TILE_DIR, "ph_side.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 24, 8, 8, 8))    # head right, 2x
    write_png(os.path.join(TILE_DIR, "pb_shirt.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 20, 20, 8, 12))  # body front
    write_png(os.path.join(TILE_DIR, "pa_arm.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 44, 20, 4, 12))  # arm front
    write_png(os.path.join(TILE_DIR, "pl_pants.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 4, 20, 4, 12))   # leg front
    write_png(os.path.join(TILE_DIR, "pl_boot.png"), TILE_SIZE,
              TILE_SIZE, tile_from(canvas, 8, 16, 4, 4))    # leg sole cap, 2x


if __name__ == "__main__":
    main()
