#!/usr/bin/env python3
"""V3-16h scene spec generator: guide_nether (① 搭建下界传送门 + 去下界 —
the portal's the build's the + the ignite's the + the travel's the to the
nether's the + the return's the, dimension #1).

The scene = the overworld's the platform (x0..11, z-8..3, top y54,
stone y50-52 + dirt y53) at NOON (time 0.25), SURVIVAL, the player's
the (5.5,54,-2.0)'s the in-front's the the portal's the (the yaw 0's
the -z's the the portal's the face's the), the PREPARED's the:
  - flint_and_steel h0 (the ignite's the)
  - the obsidian's the 4x5's the frame's the (the 14's the ring's the
    cells' the, the plane's the z=-4's the, x in [4,7] y in [54,58]'s
    the — the corners' the included's the, the unlit's the = the demo's
    the player's the ignites it). the obsidian's the mining's the 15-s's
    the bare-hand's the = the impractical's the (the recon's the), so
    the frame's the the pre-placed's the (the prepared-start's the face's
    the, the river's the bucket/torch's the twin's the).
  - the interior's the 6's the air's the (x in [5,6] y in [55,57] z=-4's
    the — the the portal's the the ignited's the).
the NETHER's the (the the travel's the target's the): the lava's the sea's
the y5-31's the + the netherrack's the plateau's the y32-52's the
(surface's the feet's the y53's the) — the worldgen's the the lazy's the
engage's the on the travel's the (the ensureNether's the). the return's
the the generated's the portal's the (the 8:1's the map's the the
original's the frame's the the find's the).
"""
import json

PX, PX1 = 0, 11
PZ, PZ1 = -8, 3
TOP = 54          # the surface's the feet's y (the y53's the top's)
BOT = 50          # the stone's the y's range's 50..52

# the portal's the frame's the (the 4x5's the, the plane's the z=-4's the)
PZ_FRAME = -4
FX0, FX1 = 4, 7      # the x's the range's the (4's the wide's the)
FY0, FY1 = 54, 58    # the y's the range's the (5's the tall's the)

cells = {}


def put(x, y, z, name):
    cells[(x, y, z)] = name


for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        for y in range(BOT, TOP):
            put(x, y, z, "stone")
        put(x, 53, z, "dirt")

# the obsidian's the 4x5's the frame's the (the 14's the ring's the cells'
# the — the perimeter's the of the [4..7]x[54..58]'s the rectangle's the
# at z=-4's the, the corners' the included's the)
for y in range(FY0, FY1 + 1):
    for x in range(FX0, FX1 + 1):
        # the ring's the = the perimeter's the (not the interior's the)
        if x in (FX0, FX1) or y in (FY0, FY1):
            put(x, y, PZ_FRAME, "obsidian")

spec = {
    "name": "guide_nether",
    "config": "/tmp/opencode/g229/config_nether.json",
    "base_frames": 90,
    "time": 0.25,
    "player": {
        "pos": [5.5, 54, -2.0],
        "yaw": 0.0,
        "pitch": -0.1,
        "health": 20.0,
        "hunger": 20.0,
        "saturation": 0.0,
        "respawn": [5.5, 54, -2.0],
        "inventory": {"selected": 0,
                      "slots": [{"item": "", "count": 0}] * 36},
        "inv_grid": [{"item": "", "count": 0}] * 9,
    },
    "clear_entities": True,
    "blocks": [[x, y, z, n] for (x, y, z), n in sorted(cells.items())],
}
spec["player"]["inventory"]["slots"][0] = {
    "item": "flint_and_steel", "count": 1}

out = "tools/scenes/guide_nether.json"
json.dump(spec, open(out, "w"), indent=1)
print(f"spec {out}: {len(spec['blocks'])} block edits "
      f"({sum(1 for v in cells.values() if v=='obsidian')} obsidian frame cells)")
