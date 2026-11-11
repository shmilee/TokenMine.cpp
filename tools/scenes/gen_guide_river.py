#!/usr/bin/env python3
"""V3-16f scene spec generator: guide_river (挖出河流 — the river's
the dig + the bucket's the water's the bring + the flow's the watch +
the water-fire's the interact + the swim's the crossing, build #2).

The scene = the 24x15 platform (x0..23, z-15..-1, top y54) at NOON
(time 0.25), SURVIVAL, the player at (1.5,54,-4.5) with the PREPARED
bucket h0 + the torch h1 (the river's the 2-items' the prepared-start
face — the pilot_mine's prepared-tool's twin):
  - the surface's layer = the dirt (the y53's, the 0.75-s's bare-hand's
    the dig's — the trench's the demo's the player's digs it)
  - the POOL (the sealed's source's pool, the PITS 390's the form):
    x18..21 z-13..-10, the 1-DEEP (the y53's water level 8 ×16, the
    y52's stone's floor — the 2-deep's the capture: the top's layer's
    source's "resting on a fluid" = the no-horizontal-spread = the
    scoop's dent's permanent; the 1-deep's dent's the neighbor-source's
    the level-7's self-heal's, the probe B's the pin's)
  - the CAMPFIRE at (10,53,-7) LIT (the trench's the center's — the
    dig's the 15-dirt's the x10's skip's the, the campfire's the
    cell's pre-placed; the out's the beat: the west's gradient's the
    x9's level-1's the contact's the t8's frame's, the probe A's the
    law's the)
  - the torch's the: the demo's places it at (14,54,-6) (the bank's
    the, the x14's water's the level-5's the neighbor's the — the
    water-fire's the CONTRAST's the beat: the campfire's out's vs the
    torch's the stays-lit's the, the MC's deviation's the education's)

The demo's beats (the build_guide_river.py's):
  P0's opening + the pool's the view's -> the dig's 15 (the x2's the
  top-face's from the bank's + the x3..x9's the inside's the + the
  x10's the campfire's the pass-over's the + the x11..x17's the) +
  the collect's (the drop's the trench's the floor's the, the player's
  the 1-m's the advance's the d=0's the magnet's) -> the scoop's 1's
  (the pool's the edge's the) -> the pour's W's (the (2,53,-7)'s the
  floor's the face's) -> the watch's W's gradient's + the campfire's
  out's -> the scoop's 2's -> the pour's E's (the (17,53,-7)'s) ->
  the watch's E's -> the torch's the place's -> the swim's crossing's
  (the probe B's the pinned's holds' the 15/252/space25/30's the) ->
  the E-panel's -> the 3rd-person's the patrol's -> the ending's.

The steady-state's the PIN (the probe A/B's the verified's):
  trench (x2..x17, y53): [8,7,6,5,4,3,2,1, campfire_extinguished,
                          2,3,4,5,6,7,8]
  pool: 14 sources + 2 level-7 dents (the scoop's cells' the)
"""
import json

PX, PX1 = 0, 23
PZ, PZ1 = -15, -1
TOP = 54          # the surface's the feet's y (the y53's the top's)
BOT = 50          # the stone's the y's range's 50..52

# the pool's interior (the 4x4's, the 1-deep's)
PX0, PX1_ = 18, 21
PZ0, PZ1_ = -13, -10

# the trench's row (the 16-cells' x2..x17, the z-7's, the DEMO's digs)
TX0, TX1 = 2, 17
TZ = -7

# the campfire's (the trench's center's x10, the y53's surface's layer)
CX = 10

cells = {}
fluids = []


def put(x, y, z, name):
    cells[(x, y, z)] = name


for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        for y in range(BOT, TOP):
            put(x, y, z, "stone")
        for y in range(TOP, TOP + 7):
            put(x, y, z, "air")

# the surface's layer (the dirt's — the dig's the 0.75-s's the)
for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        put(x, 53, z, "dirt")

# the pool's (the 1-deep's dig + the 16-sources')
for x in range(PX0, PX1_ + 1):
    for z in range(PZ0, PZ1_ + 1):
        put(x, 53, z, "air")
        fluids.append([x, 53, z, 8, "water"])

# the campfire's (the x10's dirt's replace — the dig's skip's cell)
put(CX, 53, TZ, "campfire")

spec = {
    "name": "guide_river",
    "config": "/tmp/opencode/rejoin/config_nomobs.json",
    "base_frames": 90,
    "time": 0.25,
    "player": {
        "pos": [1.5, 54, -4.5],
        "yaw": 0.0,
        "pitch": -0.1,
        "health": 20.0,
        "hunger": 20.0,
        "saturation": 0.0,
        "respawn": [1.5, 54, -4.5],
        "inventory": {"selected": 0,
                      "slots": [{"item": "", "count": 0}] * 36},
        "inv_grid": [{"item": "", "count": 0}] * 9,
    },
    "clear_entities": True,
    "blocks": [[x, y, z, n] for (x, y, z), n in sorted(cells.items())],
    "fluids": fluids,
    "campfires": [[CX, 53, TZ]],
}
spec["player"]["inventory"]["slots"][0] = {"item": "bucket", "count": 1}
spec["player"]["inventory"]["slots"][1] = {"item": "torch", "count": 1}

out = "tools/scenes/guide_river.json"
json.dump(spec, open(out, "w"), indent=1)
print(f"spec {out}: {len(spec['blocks'])} block edits, "
      f"{len(fluids)} fluid edits, 1 campfire")
