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


def spider_tile(salt: int, base, eyes=False) -> list:
    """Spider chitin (Phase 46): dark gray-brown base + noise; the head's
    front face gets two glowing red eyes (rows 5-6, the Minecraft spider
    silhouette)."""
    (br, bg, bb) = base
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if eyes and 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                (r, g, b) = (168, 40, 40)
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


# V2-5 pig (the data-driven content addition: new salt block 0x5011, the
# six existing mob tile families keep their salts and bytes — the md5
# pin in tests/test_player_body.cpp is the zero-change proof).
PIG_PINK = (228, 170, 160)  # MC pig pink (body/legs/head base)
PIG_EYE = (40, 32, 32)      # the pig's dark eyes (the V2-2 face palette)
PIG_SNOUT = (196, 130, 126)  # the dark-pink snout band

# The 8x8 pig face (the V2-2 Steve-face paradigm: explicit pixels, zero
# hash, 2x upscaled into the 16x16 tile so the features land on exact
# 4x4 / 8x4 pixels). Rows top->bottom: P = pink, E = eye, S = snout:
#   the 2x2 eyes sit in rows 2-3 (the outer columns x1-2 / x5-6) and the
#   4x2 dark-pink snout band in rows 5-6 (x2..5) — the MC pig's face.
PIG_FACE = [
    "PPPPPPPP",  # y0
    "PPPPPPPP",  # y1
    "PEEPPEEP",  # y2: 2x2 eyes (x1-2 / x5-6)
    "PEEPPEEP",  # y3: 2x2 eyes
    "PPPPPPPP",  # y4
    "PPSSSSPP",  # y5: dark-pink snout band (x2..5)
    "PPSSSSPP",  # y6: snout band
    "PPPPPPPP",  # y7
]
FACE_PIG_COLORS = {
    "P": PIG_PINK,
    "E": PIG_EYE,
    "S": PIG_SNOUT,
}


def pig_tile(salt: int) -> list:
    """Pig skin (V2-5): the MC pig pink base + light noise (smooth skin,
    the chicken's noise level)."""
    (br, bg, bb) = PIG_PINK
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 8)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def pig_face_tile() -> list:
    """The pig's per-face head tile (V2-2 `faces` consumption): the
    explicit 8x8 PIG_FACE grid 2x-upscaled to 16x16 (nearest neighbor,
    the ph_face packing — zero hash, deterministic by construction)."""
    up = []
    for gy in range(SIZE):
        src_row = PIG_FACE[gy // 2]
        row = []
        for gx in range(SIZE):
            (r, g, b) = FACE_PIG_COLORS[src_row[gx // 2]]
            row.append((r, g, b, 255))
        up.append(row)
    return up


# V2-14 bat (the project's first flying mob): the dark brown-black hide +
# the faint warm wing-membrane mottling. ONE tile serves body + head +
# both wings (no per-face `faces` consumption — the V2-2 capability stays
# unused, the documented choice: no face features, since the single tile
# would land eyes on the wings). New salt 0xB441 (the six existing
# families keep their salts and bytes — the md5/FNV pins are the
# zero-change proof).
BAT_HIDE = (40, 34, 30)      # the dark brown-black base (the MC bat's cave-dark)
BAT_MEMBRANE = (54, 44, 36)  # the warm wing-membrane mottling
BAT_MEMBRANE_ODDS = 30       # percent of cells that take the membrane shade


def bat_tile(salt: int) -> list:
    """Bat hide (V2-14): the dark brown-black base + light noise (the
    cave-mob's quiet texture) with a faint warm membrane mottling (the
    hash decides the patch boundaries — the cow-fur pattern)."""
    (br, bg, bb) = BAT_HIDE
    (wr, wg, wb) = BAT_MEMBRANE
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 8)
            if h2(x, y, salt + 3) % 100 < BAT_MEMBRANE_ODDS:
                r, g, b = clamp(wr + n), clamp(wg + n), clamp(wb + n)
            else:
                r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


# V2-15 sheep (the 16-color dyed sheep): the BARE (sheared) body tile —
# the pinkish sheep skin under the wool (the Minecraft bare-sheep look).
# ONE new tile; the three existing sheep tiles keep their salts and
# bytes (the FNV pins in the tests are the zero-change proof). New salt
# 0x5EE4 (the sheep family's next slot after 0x5EE1..0x5EE3).
SHEEP_SKIN = (226, 178, 164)  # the pinkish-tan bare-sheep base


def sheep_skin_tile(salt: int) -> list:
    """Bare sheep skin (V2-15): the pinkish-tan base + light noise
    (smooth skin — the pig's noise level, the sheep_wool_tile's salt
    discipline)."""
    (br, bg, bb) = SHEEP_SKIN
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


# V3-3 nether mob family (the four per-face heads + the five bodies, the
# new salt block 0x9A01..0x9A05): the head tiles reuse the pig face's
# 8x8 GRID SHAPE (the V2-2 paradigm) in each mob's own palette — zero
# hash, deterministic by construction; the body tiles are the base +
# noise families. The 17 existing tiles keep their salts and bytes —
# the FNV pins in the tests are the zero-change proof.
ZPIGLIN_HIDE = (128, 142, 96)     # the zombie-tinted pigman hide
ZPIGLIN_SNOUT = (104, 112, 74)    # the zombie-tinted snout band
PIGLIN_HIDE = (196, 148, 120)     # the MC piglin's tan-pink
PIGLIN_SNOUT = (156, 104, 88)     # the piglin's dark snout band
HOGLIN_HIDE = (122, 62, 48)       # the dark reddish-brown hog
HOGLIN_SNOUT = (90, 44, 36)       # the hog's dark snout band
HOGLIN_TUSK = (226, 220, 206)     # the hog's white tusks (row 7)
STRIDER_HIDE = (142, 96, 62)      # the brown strider hide
STRIDER_SADDLE = (104, 66, 42)    # the darker saddle band (rows 5-9)
STRIDER_SNOUT = (110, 70, 46)     # the strider's dark snout band
MOB_EYE = (36, 26, 22)            # the shared dark eye (the pig eye's nether twin)
ZPIGLIN_EYE = (35, 45, 30)        # the zombie eye (the skin_tile's eyes twin)
MAGMA_BASE = (112, 38, 30)        # the dark red magma mass
MAGMA_CRACK = (216, 120, 48)      # the bright lava crack
MAGMA_CRACK_ODDS = 10             # percent of cells that take the crack shade

# The hoglin's face grid: the pig shape + the white tusks at the snout
# corners (row 7 — the MC hoglin's look).
HOGLIN_FACE = [
    "PPPPPPPP",  # y0
    "PPPPPPPP",  # y1
    "PEEPPEEP",  # y2: 2x2 eyes
    "PEEPPEEP",  # y3: 2x2 eyes
    "PPPPPPPP",  # y4
    "PPSSSSPP",  # y5: dark snout band
    "PPSSSSPP",  # y6: snout band
    "PPTTPPTT",  # y7: the tusks (x2 / x13)
]


def face_grid_tile(grid: list, colors: dict) -> list:
    """An explicit 8x8 face grid 2x-upscaled to 16x16 (nearest
    neighbor, the pig face's zero-hash packing, the palette
    parameterized — the V2-2 per-face consumption)."""
    up = []
    for gy in range(SIZE):
        src_row = grid[gy // 2]
        row = []
        for gx in range(SIZE):
            (r, g, b) = colors[src_row[gx // 2]]
            row.append((r, g, b, 255))
        up.append(row)
    return up


# V3-10c (v3.25.0): the enderman's palette (the MC read — the near-black
# void body + the violet glowing eyes). The body tile reuses skin_tile's
# noise at the new salt 0x10C1; the face is the body with the two eye
# rows (5-6) in the glowing violet (0x10C2, the hash-free grid — the
# MOB_EYE pattern, the recon's R9 idempotence: every existing tile stays
# pixel-identical).
ENDERMAN_BODY = (28, 26, 32)
ENDERMAN_EYE = (200, 80, 220)


def enderman_face_tile() -> list:
    """The enderman's head face: the void body + the two glowing violet
    eyes (rows 5-6, the skin_tile eye-row convention)."""
    (br, bg, bb) = ENDERMAN_BODY
    (er, eg, eb) = ENDERMAN_EYE
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, 0x10C2, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if 5 <= y <= 6 and (3 <= x <= 5 or 10 <= x <= 12):
                (r, g, b) = (er, eg, eb)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


# V3-10d (v3.26.0): the ender dragon's palette (the MC read — the
# near-black void scales + the deep-violet wing membrane + the
# glowing magenta eye slits). New salts 0x10D1..0x10D3 — every
# existing tile stays pixel-identical.
DRAGON_BODY = (20, 18, 26)
DRAGON_WING = (48, 30, 64)
DRAGON_EYE = (226, 90, 230)


def dragon_face_tile() -> list:
    """The dragon's head face: the void scales + the two glowing
    magenta eye slits (rows 4-5, the wide slit = the boss's menace
    read, the skin_tile eye-row convention's extension)."""
    (br, bg, bb) = DRAGON_BODY
    (er, eg, eb) = DRAGON_EYE
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, 0x10D2, 10)
            r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            if 4 <= y <= 5 and (1 <= x <= 5 or 10 <= x <= 14):
                (r, g, b) = (er, eg, eb)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def zpiglin_face_tile() -> list:
    """The zombified piglin's head face (the pig shape, the zombie
    tint — the MC zombified piglin = the pigman face greened)."""
    return face_grid_tile(PIG_FACE, {
        "P": ZPIGLIN_HIDE, "E": ZPIGLIN_EYE, "S": ZPIGLIN_SNOUT})


def piglin_face_tile() -> list:
    """The piglin's head face (the pig shape, the tan-pink palette)."""
    return face_grid_tile(PIG_FACE, {
        "P": PIGLIN_HIDE, "E": MOB_EYE, "S": PIGLIN_SNOUT})


def hoglin_face_tile() -> list:
    """The hoglin's head face (the tusked grid, the dark red palette)."""
    return face_grid_tile(HOGLIN_FACE, {
        "P": HOGLIN_HIDE, "E": MOB_EYE, "S": HOGLIN_SNOUT,
        "T": HOGLIN_TUSK})


def strider_face_tile() -> list:
    """The strider's head face (the pig shape, the brown palette —
    the MC strider's pig-like head)."""
    return face_grid_tile(PIG_FACE, {
        "P": STRIDER_HIDE, "E": MOB_EYE, "S": STRIDER_SNOUT})


def strider_body_tile(salt: int) -> list:
    """Strider hide (V3-3): the brown base + light noise (the pig's
    smooth-skin level) with the darker SADDLE BAND across rows 5-9
    (the MC strider's saddle — the hash-free stripe, the design's
    'saddle band color block')."""
    (br, bg, bb) = STRIDER_HIDE
    (sr, sg, sb) = STRIDER_SADDLE
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 8)
            if 5 <= y <= 9:
                r, g, b = clamp(sr + n), clamp(sg + n), clamp(sb + n)
            else:
                r, g, b = clamp(br + n), clamp(bg + n), clamp(bb + n)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def magma_tile(salt: int) -> list:
    """Magma cube (V3-3): the dark red mass + light noise with the
    bright LAVA CRACKS (the hash decides the crack placement — the
    cow-fur patch pattern). ONE faceless tile serves the three blob
    boxes (the bat single-tile precedent — no per-face split)."""
    (br, bg, bb) = MAGMA_BASE
    (cr, cg, cb) = MAGMA_CRACK
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            n = noise(x, y, salt, 10)
            if h2(x, y, salt + 3) % 100 < MAGMA_CRACK_ODDS:
                r, g, b = clamp(cr + n), clamp(cg + n), clamp(cb + n)
            else:
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
    # Orange chicken legs + beak (Phase 99.7: the Minecraft chicken's
    # legs/beak are orange, not plumage).
    write_png(os.path.join(OUT_DIR, "chicken_legs.png"),
              chicken_tile(0xC1C3, (228, 148, 44)))
    # Dark brown cow legs (Phase 99.7: the Minecraft cow's legs are a
    # darker shade than the body fur).
    write_png(os.path.join(OUT_DIR, "cow_legs.png"),
              cow_fur_tile(0xC0F3, (94, 62, 42)))
    # Near-black sheep legs (Phase 99.7: the Minecraft sheep's legs are
    # black; the wool body stays white).
    write_png(os.path.join(OUT_DIR, "sheep_legs.png"),
              sheep_wool_tile(0x5EE3, (45, 42, 40)))
    # Bone-white skeleton (Phase 27; skull sockets on the head's front
    # face).
    write_png(os.path.join(OUT_DIR, "skeleton_body.png"),
              skeleton_tile(0x5C11, (205, 203, 193)))
    write_png(os.path.join(OUT_DIR, "skeleton_head.png"),
              skeleton_tile(0x5C12, (222, 220, 210), skull=True))
    # Dark gray-brown spider chitin (Phase 46; red eyes on the head's
    # front face).
    write_png(os.path.join(OUT_DIR, "spider_body.png"),
              spider_tile(0x5D11, (74, 66, 56)))
    write_png(os.path.join(OUT_DIR, "spider_head.png"),
              spider_tile(0x5D12, (58, 52, 44), eyes=True))
    # Pink pig (V2-5): the body/legs tile is the MC pig pink + noise
    # (new salt 0x5011); the head's front face is the explicit 8x8 pig
    # face 2x-upscaled (the V2-2 per-face paradigm, zero hash).
    write_png(os.path.join(OUT_DIR, "pig_body.png"), pig_tile(0x5011))
    write_png(os.path.join(OUT_DIR, "pig_face.png"), pig_face_tile())
    # Dark brown-black bat hide + wing membrane (V2-14): the single tile
    # (body + head + both wings — no per-face split), new salt 0xB441.
    write_png(os.path.join(OUT_DIR, "bat.png"), bat_tile(0xB441))
    # Pinkish-tan bare sheep skin (V2-15): the sheared sheep's body tile
    # (new salt 0x5EE4 — the sheep family's next slot).
    write_png(os.path.join(OUT_DIR, "sheep_skin.png"),
              sheep_skin_tile(0x5EE4))
    # V3-3: the nether mob family (5 mobs, 9 tiles — the per-face heads in
    # each mob's palette + the bodies; magma is the faceless blob's single
    # dark-red cracked tile). New salts 0x9A01..0x9A05.
    write_png(os.path.join(OUT_DIR, "zpiglin_body.png"),
              skin_tile(0x9A01, ZPIGLIN_HIDE))
    write_png(os.path.join(OUT_DIR, "zpiglin_face.png"),
              zpiglin_face_tile())
    write_png(os.path.join(OUT_DIR, "piglin_body.png"),
              skin_tile(0x9A02, PIGLIN_HIDE))
    write_png(os.path.join(OUT_DIR, "piglin_face.png"),
              piglin_face_tile())
    write_png(os.path.join(OUT_DIR, "hoglin_body.png"),
              skin_tile(0x9A03, HOGLIN_HIDE))
    write_png(os.path.join(OUT_DIR, "hoglin_face.png"),
              hoglin_face_tile())
    write_png(os.path.join(OUT_DIR, "strider_body.png"),
              strider_body_tile(0x9A04))
    write_png(os.path.join(OUT_DIR, "strider_face.png"),
              strider_face_tile())
    write_png(os.path.join(OUT_DIR, "magma_cube.png"),
              magma_tile(0x9A05))

    # V3-10c: the enderman (the End's only mob — the near-black void body
    # + the violet eye face; new salts 0x10C1/0x10C2).
    write_png(os.path.join(OUT_DIR, "enderman_body.png"),
              skin_tile(0x10C1, ENDERMAN_BODY))
    write_png(os.path.join(OUT_DIR, "enderman_face.png"),
              enderman_face_tile())

    # V3-10d: the ender dragon (the boss — the void scales + the violet
    # wing membrane + the magenta eye face; salts 0x10D1/0x10D2/0x10D3).
    write_png(os.path.join(OUT_DIR, "dragon_body.png"),
              skin_tile(0x10D1, DRAGON_BODY))
    write_png(os.path.join(OUT_DIR, "dragon_wing.png"),
              skin_tile(0x10D3, DRAGON_WING))
    write_png(os.path.join(OUT_DIR, "dragon_face.png"),
              dragon_face_tile())


if __name__ == "__main__":
    main()
