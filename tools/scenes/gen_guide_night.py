#!/usr/bin/env python3
"""V3-16g scene spec generator: guide_night (一次完整的黑夜历险 —
the dusk's the prep's the (the table's the + the sticks' the + the
torch's the + the bow's the + the arrows' the) + the night's the
spawn's the watch's the + the bow's the combat's the + the torch's
the light's the + the survive's the to the dawn's the, survival #1).

The scene = the 40x26 platform (x0..39, z-25..-1, top y54, stone
y50-52 + dirt y53) at the night's (time 0.55 = 19:12, the kDuskEnd's
0.55's — the night's the t in [0.55, 1.0)'s the = 18000-f's the, the
hour's the mapping's the (6+24t) mod 24's the: 0.75 = 00:00's the
midnight's the, 1.0/0.0 = 06:00's the dawn's the), SURVIVAL, the
player's the (19.5,54,-13)'s the center's the, SEED 7's the (the
seedhunt's the 8-seeds' the: the first's the 4's the spawns' the =
f60 sheep / f90 chicken / f120 chicken / f150 zombie (20.5,-17.5)
the LONE's the hostile's the — the no-skeleton's the (the wither's
the 10-s's the arrow's the the 9-s's the stand-still's the kill's) +
the no-spider's the (the 1.0-b/s's the beats' the the 11.5-m's the
column's the kite's the ledger's)), the PREPARED's the materials' the
(the craft's the chain's the inputs's the — the guide's the
prepared-start's the face's the):
  - planks 9 (the table's the 4 + the bow's the 1 + the sticks' the
    2x2's the 2-crafts' the = 9's the exact's the)
  - coal 1 (the torch's the 4's the)
  - flint 1 + feather 1 (the h2/h3's the) + flint 1 + feather 1 (the
    backpack's the b4/b5's the = the slots' the 13/14's the — the
    inv_grid's the = the CRAFT's the GRID's the not the backpack's
    the, the PITS 419's the 8's)
the OPEN's the platform's the (no's the walls's the: the kite's the
combat's the space's the + the 4-6-block's the spawn's the ring's the
the clear's the). the demo's the the f0's the 19:12's the -> the
f18300's the the 07:12's the dawn's the (the 0.508-day's the).
"""
import json

PX, PX1 = 0, 39
PZ, PZ1 = -25, 0
TOP = 54          # the surface's the feet's y (the y53's the top's)
BOT = 50          # the stone's the y's range's 50..52

cells = {}


def put(x, y, z, name):
    cells[(x, y, z)] = name


for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        for y in range(BOT, TOP):
            put(x, y, z, "stone")
        for y in range(TOP, TOP + 7):
            put(x, y, z, "air")

# the surface's layer (the dirt's — the 0.75-s's bare-hand's the
# twin's the, the night's the ground's the)
for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        put(x, 53, z, "dirt")

spec = {
    "name": "guide_night",
    "config": "/tmp/opencode/g228/config_night.json",
    "base_frames": 90,
    "time": 0.55,
    "player": {
        "pos": [19.5, 54, -13.0],
        "yaw": 0.0,
        "pitch": -0.1,
        "health": 20.0,
        "hunger": 20.0,
        "saturation": 0.0,
        "respawn": [19.5, 54, -13.0],
        "inventory": {"selected": 0,
                      "slots": [{"item": "", "count": 0}] * 36},
        "inv_grid": [{"item": "", "count": 0}] * 9,
    },
    "clear_entities": True,
    "blocks": [[x, y, z, n] for (x, y, z), n in sorted(cells.items())],
}
inv = spec["player"]["inventory"]["slots"]
# the inv_grid's the = the 3x3's the CRAFT's the GRID's the (the E's
# the 2x2's the = its cells' the 0/1/3/4's the, the 3-column's the
# scheme's the) — the spare's the flint/feather's the the BACKPACK's
# the slots' the (b4/b5 = the slots' the 13/14's the; the v11's the
# capture's the: the grid's the cell's the 4's the the g3's the the
# clash's the = the table's the pattern's the break's the)
inv[0] = {"item": "planks", "count": 9}
inv[1] = {"item": "coal", "count": 1}
inv[2] = {"item": "flint", "count": 1}
inv[3] = {"item": "feather", "count": 1}
inv[13] = {"item": "flint", "count": 1}
inv[14] = {"item": "feather", "count": 1}

out = "tools/scenes/guide_night.json"
json.dump(spec, open(out, "w"), indent=1)
print(f"spec {out}: {len(spec['blocks'])} block edits")
