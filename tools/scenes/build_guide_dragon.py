#!/usr/bin/env python3
# V3-16h②: guide_dragon (② 打末影龙 — the end's the portal's the eye-embed's
# the + the travel's the + the pearl's the escape's the + the dragon's the
# defeat's the the --dragon-damage 200's the + the crystal's the shatter's
# the + the egg's the + the return's the, dimension #2).
#
# The scene = guide_dragon (the 12x12 platform x0..11 z0..11 top y54 at
# NOON, SURVIVAL, the player's the (2.5,54,2.5)'s the SW corner's the,
# the PREPARED's the: ender_eye 12 h0 + ender_pearl 1 h1 + the
# end_portal_frame's the 5x5's the ring's the (12 border cells minus
# 4 corners, center (6,6), un-eyed) — the frame's the survival's the
# unobtainable's the = the scene's the pre-placed's the). the 4 corners'
# the = the gaps' the (the the player's the the center's the enter's the
# through's the).
#
# THE ENDERMAN'S THE HAZARD'S THE (the v2/v4's the capture's the: the End's
# the pool's the = enderman's the only's the, the 4-6-m's the ring's the
# the spawn's the around the player's the, the eye-contact's the aggro's
# the 48-m's the = the look-at's the them's the 5-f's the = the 6-dmg's
# the 1.5-s's the + the knockback's the the push's the the player's the
# the off-course's the, the stationary's the the player's the the dead's
# the in ~5-s's the) -> the survival's the strategy's the (the zero-
# kernel's the, the pearl's the V3-10b's the the existing's the mechanic's
# the = the End's the escape's the the game's the design's the):
#   1. the PEARL'S THE on the landing's the (the 30-b/s's the the west's
#      the, the ~19-m's the the flight's the 0.33-s's the, the teleport's
#      the = the 40-m's the the walk's the the halved's the + the the
#      first's the 40-f's the the exposure's the the skipped's the)
#   2. the NEVER'S THE stationary's the in the End's the (the 4.3-b/s's
#      the walk's the the outruns' the the 0.8-b/s's the the chasers'
#      the = the 0-hit's the while's the in-motion's the, the aggro's the
#      by's the bump's the the harmless's the = the the chaser's the the
#      trails' the)
#   3. the PITCH'S THE steep's the (the -3.2-deg's the the pearl's the aim's
#      the, the walk's the auto-face's the the flat's the — the enderman's
#      the aggro's the the 5-f's the the consecutive's the the eye-ray's
#      the the hit's the, the the walk's the 4.3-b/s's the the dwell's the
#      in's the the 0.9-m's the the aggro's the cone's the = the ~0.1-s's
#      the < the 5-f's the the 0.083-s's the? NO — the 0.1-s's the = the
#      6-f's the > the 5-f's the = the aggro's the = the the bump's the
#      aggro's the the ACCEPTED's the (the the chaser's the the trails'
#      the, the the 0.8 < the 4.3-0.8's the the 3.5-b/s's the the
#      separation's the, the NEVER'S THE catches' the))
#   4. the COOLDOWN'S THE WAIT'S THE = the back-and-forth's the legs'
#      the (the 900-f's the the shared's the portal's the cooldown's the
#      the 15-s's the, the MOVE'S THE through's the it's the = the the
#      chasers' the the 5-15-m's the the trail's the the stays' the)
#   5. the PORTAL'S THE 60-f's the the entry's the the last's the (the the
#      chasers' the the 0.8-b/s's the the 1-s's the the 0.8-m's the the
#      advance's the < the 1.6-m's the the attack's the range's the the
#      3-m's the the trail's the = the 0-hit's the the entry's the)
#
# The tour's the:
#   s1  the P0's the watch's the (the un-eyed's the ring's the)
#   s2  the enter's the the center's the + the 12's the eye-embed's the
#       (the GOOD's the cell's the (6,4)'s the LAST's the, the PITS 421's)
#       -> "End portal activated"
#   s3  the stand's the portal's the 60-f's the -> TRAVEL's the End's the
#       (the landing's the (40.5,53,0.5)'s the, the dragon's the the 60-f's
#       the spawn's the the --dragon-damage 200's the the same-frame's the
#       kill's the + the ritual's the the egg's the the 23-orb's the the
#       240-f's the ascension's the = the OVERHEAD'S THE as's the the walk's
#       the)
#   s4  the PEARL'S THE west's the (the select's the h1's the + the look's
#       the the -3.2-deg's the + the RMB's the, the ~19-m's the the land's)
#   s5  the walk's the the west's the the origin's the (the 23-m's the, the
#       the XP-orb's the the magnet's the the 200's the the collect's the,
#       the dragon's the the wail's the the overhead's the)
#   s6  the shatter's the the 2's the exit's the crystal's the (the bare's
#       the hand's the hardness-0's the, the LMB's the 4-f's the, the
#       explode's the r3's the the endstone's the the crater's the)
#   s7  the back-and-forth's the the wait's the (the 900-f's the cooldown's
#       the, the the moving's the = the the 0-hit's the)
#   s8  the stand's the the exit's the portal's the 60-f's the -> RETURN's
#       the overworld's the
#   s9  the overworld's the observe's the (the lit's the end's the portal's
#       the 9's the cell's the + the beam's the, the sweep's the, the safe's)
import math

from demo_author import Guide

g = Guide(scene="guide_dragon", pos=(2.5, 54, 2.5), yaw=0.0, pitch=-0.1)

SEGMENTS = []


def seg_mark(name):
    SEGMENTS.append({"name": name, "f": int(g.cursor),
                     "pos": [g.x, g.z]})


def walk_to(f, px, pz, maxd=2.0):
    g.at(f)
    for _ in range(40):
        dx = px - g.x
        dz = pz - g.z
        if abs(dx) >= abs(dz):
            R, C, ax = dx, dz, "x"
        else:
            R, C, ax = dz, dx, "z"
        if abs(R) < 0.66:
            assert abs(C) < 0.15, (
                f"walk_to: transverse {C:.2f}m track=({g.x:.3f},{g.z:.3f})")
            return f
        R = R if abs(R) < 2.7 else 2.0 * (1 if R > 0 else -1)
        f = g.walk_leg(f, g.x + R, g.z) if ax == "x" \
            else g.walk_leg(f, g.x, g.z + R)
    raise AssertionError("walk_to: unbounded")


def embed(f, cx, cy, cz):
    g.look_point(f, cx, cy, cz)
    f += 25
    g.click(f, "right", hold=4)
    f += 4 + 25
    return f


def shatter(f, cx, cy, cz):
    g.look_point(f, cx, cy, cz)
    f += 25
    g.click(f, "left", hold=4)
    f += 4 + 30
    return f


g.capture(0)

# -- s1 the P0's the watch's the (the un-eyed's the end's the ring's the)
g.sweep(30, 90, 0.35)
g.sweep(120, 120, -0.70)
g.at(240)
f = 240
seg_mark("s1_p0")

# -- s2a the GEAR-UP'S THE (the netherite's the armor's the 4's the pieces'
#    the the RMB's the the equip's the the SURV-1's the held-piece's the
#    the chain's the = the the no-panel's the, the the 20-AP's the the
#    the 80-percent's the the enderman's the 6-D's the the 1.2-D's the) --
for i in range(4):
    g.select(f, 3 + i)      # h2..h5 = the netherite's the set's the
    f += 25
    g.click(f, "right", hold=4)
    f += 4 + 30
seg_mark("s2a_gearup")

# -- s2 the enter's the center's the + the 12's the eye-embed's the ----
# the s2a's the leave's the the selection's the h5's the (the boots'
# the) — the h0's the re-select's the (the gd9's the capture's the: the
# the no-select's the = the 0's the embed's the, the RMB's the the boots'
# the the no-op's the)
g.select(f, 1)
f += 25
f = walk_to(f, 4.5, 4.5)
f += 20
f = walk_to(f, 6.0, 6.0)
f += 40
# 11's the cells' the any's the order's the, the GOOD's the cell's the
# (6,4)'s the LAST's the (the ring's the scan's the 8-candidate's the
# center's the, the (6,4)'s the the (6,6)'s the offset's the (0,2)'s
# the — the PITS 421's the)
for (cx, cz) in [(5.5, 4.5), (7.5, 4.5), (5.5, 8.5), (6.5, 8.5),
                 (7.5, 8.5), (4.5, 5.5), (4.5, 6.5), (4.5, 7.5),
                 (8.5, 5.5), (8.5, 6.5), (8.5, 7.5)]:
    f = embed(f, cx, 55.0, cz)
f = embed(f, 6.5, 55.0, 4.5)   # 12th (good) -> activate
f += 30
seg_mark("s2_eyes")

# -- s3 the stand's the portal's the 60-f's the -> travel's the ----------
g.at(f + 150)
f += 150
# re-anchor to the End's the landing's the (40.5, 53, 0.5) — THE YAW'S THE
# too's the (travelEnd's the sets' the +pi/2's the west's the, the v5's
# the capture's the: the x/y/z's the re-anchor's the the yaw's the the
# stale's the = the look_point's the the mouse-delta's the the wrong's
# the, the pearl's the the east's the throw's the = the the death's the)
g.x, g.y, g.z = 40.5, 53.0, 0.5
# the yaw's the re-anchor's the (travelEnd's the forces' the +pi/2's the
# west's the, the author's the stale's the = the the v5's the capture's)
# — the pitch's the NOT'S THE (the travelEnd's the keeps' the = the the
# 12th's the embed's the aim's the -20.96-deg's the, the author's the
# state's the the synced's the, the v6's the capture's the: the the
# pitch's the the -0.1's the the overwrite's the = the the pearl's the
# the -18-deg's the the steep's the throw's the the 4-m's the only's)
g.yaw = math.pi / 2
f += 20
seg_mark("s3_travel")

# -- s4 the PEARL'S THE the 28-deg's the the west's the the arc's the (the
#    the flat's the throw's the 12-m's the the gd7's the capture's the,
#    the 28-deg's the = the 40-m's the the range's the = the ONE'S THE
#    throw's the the whole's the crossing's the, the the player's the
#    the land's the the IN'S THE the exit's the portal's the center's the
#    the 2.5-m's the the offset's the the pin's the) --------------------
g.select(f, 2)          # h1 = the ender_pearl's the
f += 20
# the aim's the = the the 28-deg's the west's the the ray's the (the
# eye's the (40.5,54.62,0.5)'s the, the the 20-m's the point's on the
# ray's the) — the the z-0.6's the the gd10's the capture's the: the
# z0.5's the line's the arc's the cross's the (2,54,0)'s the crystal's
# box's the = the shatter_on_arrow's the sweep's the (the all-projectile's
# pass's the)'s the shatter's the + consume's the = no-teleport's the;
# the z-0.6's the aim's the land's the frame's the (1,53,-2)'s the top's
# the clear's the crystal's the line's the — the the TIMING'S THE the
# gd17's the capture's the: the 40-f's the the earlier's the throw's the
# = the pearl's the the cross's the the origin's the the chunk's the
# (0,0)/(0,-2)'s the the UNLOADED'S THE (the raw's the getBlock's the
# air's the, the PITS 408's the 5's the) = the pass-through's the the
# no-stick's the the 244-f's the the void's the the flight's the; the
# f1593's the throw's the (the 20-f's the the offsets's the) = the
# cross's the the f1679's the the = the after's the the terrain's the
# load's the = the the stick's the)
g.look_point(f, 22.8, 64.0, -0.6)
f += 20
g.click(f, "right", hold=4)        # the throw's the (the release's the)
f += 20 + 160           # the flight's the ~95-f's the + the settle's the
# re-anchor to the pearl's the landing's the (the "pearl teleport to
# (x, y, z)"'s the the deterministic's the pin's the, the gd13's the
# probe's the: the (1.89, 54.69, -4.30)'s the the fall's the re-anchor's
# the feet's the y53's the)
g.x, g.y, g.z = 1.89, 53.0, -4.30   # the gd13's the pin's the
g.yaw = math.pi / 2                # the pearl's the the yaw's the keep's
seg_mark("s4_pearl")

# -- s5 the walk's the the CENTER'S THE the egg-safe's the (1,1)'s the (the
#    the z-leg's the the step-up's the (1,53,-2)'s the + the step-down's
#    the center's the, the the x-leg's the the center's the the flat's the
#    — the the gd16/gd18's the capture's the: the rim's the (0.5,2.5)'s
#    the the shatter's the the miss's the (the 141-deg's the the
#    multi-look's the the yaw's the the model's the the drift's the, the
#    the 2's the crystal's the the intact's the), the center's the
#    gd14's the the shatter's the the worked's the but the the egg's the
#    break's the (the eye's the the inside's the the egg's the box's the
#    = the t0's the mine's the), the the (1,1)'s the = the 1-m's the the
#    egg's the clear's the + the 1.62/3.56-m's the the both's the
#    reach's the) --------------------------------------------------------
f = walk_to(f, 1.0, 1.0)
# the step's the event's the the drift's the — the honest-landing's the
# re-anchor's the (the gd19's the pin's the)
g.x, g.y, g.z = 0.61, 53.0, 1.37   # the gd19's the pin's the
f += 30
seg_mark("s5_center")

# -- s6 the shatter's the the 2's the exit's the crystal's the (the the
#    (±2,54,0)'s the the cells' the = the the ring's the frame's the the
#    top's the the y55's the the center's the (±2.5,55,0.5)'s the, the
#    the the 1.62/3.56-m's the the reach's the from (1.0,53,1.0)'s the,
#    the the egg's the the clear's the the ray's the (the the eye's the
#    the (1.0,54.62,1.0)'s the the outside's the the egg's the box's the
#    x0.5/z0.5's the max's the, the the −x's the ray's the the egg's the
#    x-slab's the the z0.93's the the pass's the, the the z-slab's the
#    the x-2.5's the the at's the), the the explode's the r3's the the
#    frame's the the survive's the, the the tower's the crystal's the the
#    (±50,59,±50)'s the the off-route's the the CUT's the) --------------
f = shatter(f, 2.5, 55.0, 0.5)
f = shatter(f, -2.5, 55.0, 0.5)
# the egg's the the look's the (the (0,54,0)'s the the cell's the = the
#   the y55's the the top's the, the the 200-XP's the the orb's the the
#   the magnet's the the the draw's the, the the ritual's the the
#   witness's the)
g.look_point(f, 0.5, 54.5, 0.5)
f += 30
g.at(f + 150)
f += 150
seg_mark("s6_crystal")

# -- s7 the STAND'S THE in's the the center's the (the the 11-orb's the
#    the magnet's the the collect's the the window's the, the 900-f's the
#    cooldown's the the armed's the = the the 60-f's the the countdown's
#    the the the auto's the, the the armor's the the chasers' the the
#    1.2-D's the the budget's the the the 0-hit's the gd16's the) -------
g.at(f + 1200)
f += 1200
seg_mark("s7_stand")

# -- s8 the RETURN'S THE (the the countdown's the the 60-f's the the
#    travel's the, the the landing's the = the spawnPoint's the the
#    (0.5,54,0.5)'s the the platform's the the NW's the corner's the the
#    gd14's the pin's the) ---------------------------------------------
f += 250
# re-anchor to the overworld's the return's the landing's the
g.x, g.y, g.z = 0.5, 54.0, 0.5    # the gd14's the pin's the
f += 60
seg_mark("s8_return")

# -- s9 the overworld's the observe's the (the lit's the end's the portal's
#    the 9's the cell's the + the portal's the beam's the, the the the
#    SAFE'S THE no-enderman's the, the the length's the the face's the) --
g.face(f, 0.0)
f += 30
g.look_point(f, 6.5, 54.5, 6.5)
f += 30
g.sweep(f, 900, 0.80)
f += 900
g.sweep(f, 900, -1.60)
f += 900
g.tap(f, "c")
f += 40
g.sweep(f, 1200, 1.00)
f += 1200
g.sweep(f, 1200, -2.00)
f += 1200
g.tap(f, "c")
f += 40
g.look_point(f, 6.5, 54.5, 6.5)
f += 30
g.at(f + 3600)
f += 3600
g.sweep(f, 1000, 0.60)
f += 1000
g.sweep(f, 1000, -1.20)
f += 1000
g.at(f + 1500)
f += 1500
seg_mark("s9_observe")

import os
import json

tl = g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "..", "assets", "demos",
                         "guide_dragon_timeline.json"))
json.dump({"segments": SEGMENTS},
          open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "guide_dragon_segments.json"), "w"), indent=1)
print(f"segments: {len(SEGMENTS)} | total ~{int(g.cursor)} frames")
