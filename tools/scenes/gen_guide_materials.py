#!/usr/bin/env python3
"""V3-16d scene spec generator: guide_materials (收集各种材料 — the
multi-biome materials garden, build chapter #3).

The scene is a 30x26 platform (x0..29, z-25..0, top y54) at NOON
(time 0.25) with the player at (15.5,54,-1.5) in SURVIVAL (the
collection's face — real break times, the drops' magnet pickup, the
E-panel's showcase) with a PREPARED wooden pickaxe in h1 (the ore's
face — the pilot_mine's prepared-tool's twin, the craft's ④'s overlap
avoided).

The 7 biome beds (the surface = the y53's replace, the top's y54's
flush; the specimens at y54's above):
  - B1 forest  (x2..11,  z-23..-15): grass 90 + 3 oak trees
  - B2 desert  (x14..23, z-23..-15): sand 90 + 2 cactus
  - B6 wool    (x25..28, z-23..-15): grass 36 + wool 3 + glowstone 2
  - B3 taiga   (x2..11,  z-10..-3):  snow 80 + 2 spruce trees + snow
                                       bumps (3,-10) (9,-10) (3,-3)
                                       (the v3.20's relocation's)
  - B4 ore     (x14..23, z-10..-3):  stone outcrop 4x2x3 (x16..19,
                                      z-8..-6, y54..55) + coal x2 +
                                      iron x1 (the z-6's south face)
  - B5 flower  (x25..28, z-10..-3):  grass 32 + allium 2 + cornflower 2
                                      + tulip 2
  - the walkways: z-14..-11 (the row's gap), z-2..0 (the start's belt),
    x0..1 / x12..13 / x24 / x29 (the column's gaps)
The collection's 36 breaks' the demo's the (build_guide_materials.py).

The cell's the dict's the face (the same-cell's the overwrite's the
clean's the journal's the duplicate's reject's avoid — the mk_scene's
the 16c ⑥'s the twin).
"""
import json

PX, PX1 = 0, 29          # the platform's x's the range
PZ, PZ1 = -25, 0         # the platform's z's the range
TOP = 54                 # the platform's the top's y (the surface's y54)
BOT = 50                 # the platform's the stone's the y's the range

cells = {}


def put(x, y, z, name):
    cells[(x, y, z)] = name


for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        for y in range(BOT, TOP):
            put(x, y, z, "stone")
        for y in range(TOP, TOP + 7):
            put(x, y, z, "air")


def bed(x0, x1, z0, z1, surface):
    for x in range(x0, x1 + 1):
        for z in range(z0, z1 + 1):
            put(x, 53, z, surface)


def oak(x, z):
    for dy in range(3):
        put(x, TOP + dy, z, "wood")
    for lx in range(x - 1, x + 2):
        for lz in range(z - 1, z + 2):
            put(lx, TOP + 3, lz, "leaves")
            put(lx, TOP + 4, lz, "leaves")


def spruce(x, z):
    # the leaves' the 3x3x3 (y57..59) + the the 1-top's (y60) FIRST, the
    # the trunk's the y54..59's the AFTER (the the center's cell's the
    # trunk's win — the leaves' the trunk's hide's avoid)
    for lx in range(x - 1, x + 2):
        for lz in range(z - 1, z + 2):
            for ly in range(TOP + 3, TOP + 6):
                put(lx, ly, lz, "spruce_leaves")
    put(x, TOP + 6, z, "spruce_leaves")
    for dy in range(6):
        put(x, TOP + dy, z, "spruce_trunk")


# the beds' the surfaces (the y53's the stone's the replace)
bed(2, 11, -23, -15, "grass")      # B1 forest
bed(14, 23, -23, -15, "sand")      # B2 desert
bed(25, 28, -23, -15, "grass")     # B6 wool
bed(2, 11, -10, -3, "snow")        # B3 taiga
# B4 ore: no surface's the replace (the the outcrop's the bed)
bed(25, 28, -10, -3, "grass")      # B5 flower

# the beds' the specimens (the y54's the air's the replace's the)
for (x, z) in ((4, -19), (7, -17), (10, -19)):
    oak(x, z)
for (x, z) in ((5, -7), (9, -5)):
    spruce(x, z)
for (x, z) in ((16, -21), (21, -17)):
    put(x, TOP, z, "cactus")
for (x, z) in ((25, -18), (26, -18), (27, -18)):
    put(x, TOP, z, "wool")
for (x, z) in ((26, -15), (27, -15)):
    put(x, TOP, z, "glowstone")
# the stone's the outcrop's the z-6's the 1-deep's shelf (the v3.18's the
# depth-1's: the z-7/-8's the row's the CUT's the stone's the beat's the
# tail's the rear's the stone's the (16,54,-7)'s the over-dug's the 1-cobble's
# the v3.17's the root's — the depth-1's the tail's the floor's the 2.2-%'s
# the partial's the zero's the over-dug's the structural's) + the ore's the
# the z-6's south face
for x in range(16, 20):
    put(x, TOP, -6, "stone")
    put(x, TOP + 1, -6, "stone")
put(16, TOP, -6, "coal_ore")
put(19, TOP, -6, "coal_ore")
put(17, TOP, -6, "iron_ore")
# the floor's the specimen's the y54's the bump's (the v3.17's the pit's
# the drop's the fix's: the y53's the break's the 1-m's the pit's the drop's
# the 120-s's the despawn's the magnet's the horizontal-only's the twin —
# the bump's the drop's the platform's flush's the flower's the wool's the
# cactus's the pattern's the collected's)
for (x, z) in ((3, -15), (8, -15)):
    put(x, TOP, z, "grass")
for (x, z) in ((16, -19), (21, -19), (19, -16), (16, -16)):
    put(x, TOP, z, "sand")
# the v3.20's relocation's (the gate-226's root 6's: the (9,-8)'s the z-8.5's
# line's the 0.2-m's the graze's the float-ulp's the corner's the pin's the
# root's) the open's the row's the (the z-10's the pair's the (3,-3)'s the
# the north's the belt's the approach's the 2.2-m's the clear's)
for (x, z) in ((3, -10), (9, -10), (3, -3)):
    put(x, TOP, z, "snow")
for (x, z) in ((25, -8), (27, -5)):
    put(x, TOP, z, "allium")
for (x, z) in ((26, -7), (28, -4)):
    put(x, TOP, z, "cornflower")
for (x, z) in ((26, -3), (28, -8)):
    put(x, TOP, z, "tulip")

blocks = [[x, y, z, n] for (x, y, z), n in sorted(cells.items())]

spec = {
    "name": "guide_materials",
    "config": "/tmp/opencode/rejoin/config_nomobs.json",
    "base_frames": 90,
    "time": 0.25,
    "player": {
        "pos": [15.5, 54, -1.5],
        "yaw": 0.0,
        "pitch": -0.1,
        "health": 20.0,
        "hunger": 20.0,
        "saturation": 0.0,
        "respawn": [15.5, 54, -1.5],
        "inventory": {
            "selected": 0,
            "slots": [{"item": "", "count": 0}] * 36
        },
        "inv_grid": [{"item": "", "count": 0}] * 9
    },
    "clear_entities": True,
    "blocks": blocks
}
spec["player"]["inventory"]["slots"][1] = \
    {"item": "wooden_pickaxe", "count": 1}

out = "tools/scenes/guide_materials.json"
json.dump(spec, open(out, "w"), indent=1)
print(f"spec {out}: {len(blocks)} unique cells")
