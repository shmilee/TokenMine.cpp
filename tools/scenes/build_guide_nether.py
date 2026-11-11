#!/usr/bin/env python3
# V3-16h①: guide_nether (搭建下界传送门 + 去下界 — the portal's the
# ignite's the + the travel's the to the nether's the + the nether's the
# observe's the, dimension #1).
#
# The scene = guide_nether (the 12x11 platform x0..11 z-8..3 top y54 at
# NOON, SURVIVAL, the player's the (5.5,54,-2.0)'s the in-front's the
# the portal's the, the PREPARED's the: flint_and_steel h0 + the
# obsidian's the 4x5's the frame's the (14 ring cells, plane z=-4,
# x[4,7] y[54,58], unlit) + the interior's the 6's the air's the). the
# obsidian's the mining's the 15-s's the bare-hand's the = the
# frame's the the pre-placed's the (the prepared-start's the face's the).
#
# The tour's the:
#   s1  the P0's the watch's the (the unlit's the portal's the)
#   s2  the ignite's the (the flint's the steel's the on the bottom's
#       row's the the top's the face's the, the same-frame's the
#       "Portal activated"')
#   s3  the walk-in's the the portal's the + the 60-f's the countdown's
#       the + the TRAVEL's the to the nether's the (the 8:1's the map's
#       the, the landing's the the generated's the portal's the frame's)
#   s4  the step-off's the the frame's the onto the nether's the ground's
#   s5  the nether's the observe's the (the walk's the loop's the + the
#       3rd's the person's the sweeps' the the landscape's the)
#   s6  the end's the (the player's the the nether's the the ground's the)
import math

from demo_author import Guide

g = Guide(scene="guide_nether", pos=(5.5, 54, -2.0), yaw=0.0, pitch=-0.1)

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


g.capture(0)

# -- s1 the P0's the watch's the (the night's the... no, the noon's the —
#    the unlit's the obsidian's the frame's the the view's the) ----------
g.sweep(30, 90, 0.35)
g.sweep(120, 120, -0.70)
g.at(240)
f = 240
seg_mark("s1_p0")

# -- s2 the ignite's the (the flint's the steel's the the h0's the, the
#    aim's the the bottom's the row's the the top's the face's the
#    (5.5,55,-3.5)'s the -> the adj's the (5,55,-4)'s the the interior's
#    the = the "Portal activated"') --------------------------------------
g.look_point(f, 5.5, 55.0, -3.5)
f += 30
g.click(f, "right", hold=4)
f += 90
# the portal's the lit's the — the brief's the observe's the
g.sweep(f, 120, 0.4)
f += 120
seg_mark("s2_ignite")

# -- s3 the walk-in's the the portal's the + the travel's the (the 60-f's
#    the countdown's the, the landing's the the nether's the frame's) ----
f = walk_to(f, 5.5, -3.5)
f += 30
g.at(f + 150)
f += 150   # travel (lands on the nether's generated frame (1,55,-0.5))
# re-anchor the author's track to the nether's the landing's the
g.x, g.y, g.z = 1.0, 55.0, -0.5
seg_mark("s3_travel")

# -- s4 the step-off's the the frame's the onto the nether's the ground's
#    (the walk's the the +z's the the 4-m's the, the 2-m's the drop's the
#    the no-fall-damage's the, the nether's the surface's the y53's) ----
f = walk_to(f, 1.0, 3.5)
f += 60
# re-anchor to the nether's the ground's the (the drop's the)
g.x, g.y, g.z = 1.0, 53.0, 3.5
seg_mark("s4_stepoff")

# -- s5 the nether's the observe's the (the walk's the loop's the E/S/W/N
#    the + the 3rd's the person's the sweeps' the the landscape's the:
#    the lava's the sea's the + the netherrack's the plateau's the + the
#    generated's the portal's the) ---------------------------------------
f = walk_to(f, 8.0, 3.5)
f += 30
# 3rd person's the sweep's the (the landscape's the the view's the)
g.tap(f, "c")
f += 40
g.sweep(f, 1000, 1.00)
f += 1000
g.sweep(f, 1000, -2.00)
f += 1000
g.tap(f, "c")
f += 40
f = walk_to(f, 8.0, 10.5)
f += 30
f = walk_to(f, 1.0, 10.5)
f += 30
# the 2nd's the loop's the (the W's the the further's the explore's the)
g.tap(f, "c")
f += 40
g.sweep(f, 1000, 0.80)
f += 1000
g.sweep(f, 1000, -1.60)
f += 1000
g.tap(f, "c")
f += 40
f = walk_to(f, 1.0, 3.5)
f += 30
# the generated's the portal's the the look-back's the (the the return's
#    the path's the the view's the, the nether's the the skyless's the
#    ceiling's the)
g.tap(f, "c")
f += 40
g.sweep(f, 1200, 1.20)
f += 1200
g.sweep(f, 1200, -2.40)
f += 1200
g.tap(f, "c")
f += 40
# the still's the observe's the (the nether's the the ambience's the the
#    long's the hold's the)
g.at(f + 2400)
f += 2400
seg_mark("s5_observe")

# -- s6 the end's the (the player's the the nether's the the ground's the
#    the, the the dimension's the the travel's the the complete's the) --
g.at(f + 300)
f += 300
seg_mark("s6_end")

import os
import json

tl = g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "..", "assets", "demos",
                         "guide_nether_timeline.json"))
json.dump({"segments": SEGMENTS},
          open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "guide_nether_segments.json"), "w"),
          indent=1)
print(f"segments: {len(SEGMENTS)} | total ~{int(g.cursor)} frames")
