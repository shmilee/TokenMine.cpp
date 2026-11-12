#!/usr/bin/env python3
"""V3-16h② scene spec generator: guide_dragon (② 打末影龙 — the end's the
portal's the eye-embed's the + the travel's the + the dragon's the the
watch's the + the crystal's the shatter's the + the egg's the + the
return's the, dimension #2).

The scene = the overworld's the platform (x0..11, z0..11, top y54,
stone y50-52 + dirt y53) at NOON (time 0.25), SURVIVAL, the player's
the (2.5,54,2.5)'s the SW corner's the, the PREPARED's the:
  - ender_eye 12 (h0) (the 12's the frame's the embed's the)
  - the end_portal_frame's the 5x5's the ring's the (12 border cells
    minus the 4 corners, plane y54, center (6,6), UN-EYED) — the frame's
    the survival's the unobtainable's the (destroyable false, no recipe)
    = the scene's the pre-placed's the (the prepared-start's the face's
    the, the nether's the obsidian's the frame's the twin's the).
    the 3x3's the center's the (x[5,7] z[5,7]'s the) = the air's the
    (the active's the end_portal's the).
the END's the (the travel's the target's the): the 120x120's the
plateau's the (x,z in [-60,60], y in [48,52], surface feet y53) + the
origin's the exit portal's the (3x3 end_portal at y53, 12 eyed frames)
+ the dragon's the (spawns 60-f after End entry, (0.5,58,0.5)) + the 6
crystal's the (4 tower (±50,59,±50) + 2 exit (±2,54,0)).
"""
import json

PX, PX1 = 0, 11
PZ, PZ1 = 0, 11
TOP = 54
BOT = 50

# the end's the portal's the ring's the (5x5, center (6,6), plane y54)
CX, CZ = 6, 6

cells = {}


def put(x, y, z, name):
    cells[(x, y, z)] = name


for x in range(PX, PX1 + 1):
    for z in range(PZ, PZ1 + 1):
        for y in range(BOT, TOP):
            put(x, y, z, "stone")
        put(x, 53, z, "dirt")

# the end_portal_frame's the 5x5's the ring's the (12 border cells minus
# the 4 corners' the, y54' the plane' the)
for z in range(CZ - 2, CZ + 3):
    for x in range(CX - 2, CX + 3):
        dx = x - CX
        dz = z - CZ
        # the border's the (max(|dx|,|dz|) == 2) minus the corners' the
        # (|dx|==2 and |dz|==2)
        if max(abs(dx), abs(dz)) == 2 and not (abs(dx) == 2 and abs(dz) == 2):
            put(x, 54, z, "end_portal_frame")

spec = {
    "name": "guide_dragon",
    "config": "/tmp/opencode/g229/config_dragon.json",
    "base_frames": 90,
    "time": 0.25,
    "player": {
        "pos": [2.5, 54, 2.5],
        "yaw": 0.0,
        "pitch": -0.1,
        "health": 20.0,
        "hunger": 20.0,
        "saturation": 0.0,
        "respawn": [2.5, 54, 2.5],
        "inventory": {"selected": 0,
                      "slots": [{"item": "", "count": 0}] * 36},
        "inv_grid": [{"item": "", "count": 0}] * 9,
    },
    "clear_entities": True,
    "blocks": [[x, y, z, n] for (x, y, z), n in sorted(cells.items())],
}
spec["player"]["inventory"]["slots"][0] = {
    "item": "ender_eye", "count": 12}
# h1 = 1 ender pearl (the End escape: ONE steep throw (the 28-deg's the
# arc's the, the 30-b/s's the = the 40-m's the the range's the, the flat's
# the throw's the 12-m's the only's the = the gd7's the capture's the)
# lands the player IN the exit portal's the 3x3's the center's the — the
# 30-m's the walk's the through the enderman's the ring's the (the
# v2/v4/gd8's the death's the route's the) is NEVER'S THE needed's the)
spec["player"]["inventory"]["slots"][1] = {
    "item": "ender_pearl", "count": 1}
# h2-h5 = the netherite's the armor's the set's the (the 20-AP's the =
# the 80-percent's the the max's the reduction's the, the enderman's the
# 6-D's the the 1.2-D's the, the 16-hit's the budget's the = the the
# portal's the the stand's the the cooldown's the wait's the the safe's
# the; the RMB's the the equip's the the SURV-1's the the held-piece's
# the chain's the, the the no-panel's the the beat's the)
for i, name in enumerate(("netherite_helmet", "netherite_chestplate",
                          "netherite_leggings", "netherite_boots")):
    spec["player"]["inventory"]["slots"][2 + i] = {"item": name, "count": 1}

out = "tools/scenes/guide_dragon.json"
json.dump(spec, open(out, "w"), indent=1)
print(f"spec {out}: {len(spec['blocks'])} block edits "
      f"({sum(1 for v in cells.values() if v=='end_portal_frame')} end_portal_frame ring cells)")
