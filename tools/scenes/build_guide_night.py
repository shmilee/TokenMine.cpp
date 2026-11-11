#!/usr/bin/env python3
# V3-16g: guide_night (一次完整的黑夜历险 — the lone-zombie's the night's
# the: the E-kite + the east-column's the S/N's the ping-pong's the
# kite-craft's the + the bow's the zombie's the kill's the + the torch's
# the light's the + the night's the watch's the + the survive's the to
# the dawn's the, survival #1).
#
# The scene = guide_night (seed 7, time 0.55 = 19:12 the night's, the
# 40x26's the platform's the x0..39 z-25..-1's the, the player's the
# P0 (19.5,-13)'s the center's the): the first's the 4's the spawns'
# the (the probe's the pinned's):
#   f60  sheep   (19.5,-18.5) (the ambience's the)
#   f90  chicken (16.5,-17.5) (the ambience's the)
#   f120 chicken (17.5,-17.5) (the ambience's the)
#   f150 zombie  (20.5,-17.5) (the LONE's the hostile's the — the 0.8-
#          b/s's the chase's the, the 20-hp's the = the 4-arrow's the
#          kill's the)
# the no-skeleton/no-spider's the seed's the (the hunt's the: the
# skeleton's the wither's the 10-s's the the 9-s's the kill's the, the
# spider's the 1.0-b/s's the the 11.5-m's the column's the the gap's
# the cap's the beats' the — the zombie's the 0.8's the the 6-m's the
# floor's the the OK's the).
#
# The KITE-CRAFT's the v9's the (the v1-v7's the capture's the: the
# rectangle's the the L's the corner's the cut's the + the head-on's
# the turn's the the E-W's the ping-pong's the death's the; the east's
# column's the x38.5's the the S/N's the ping-pong's the = the the
# pursuer's the the behind's the always's the, the gap's the cap's the
# 21.5-m's the the column's the length's the):
#   s1  the watch's the (the zombie's the f150's the the visible's the)
#   s2  the kite's the E's the 19-m's the + the W1 (the table's the
#       craft's the, the E's the panel's the) @ the E's the end's the
#   s3  the kite's the S's the 11.5-m's the + the BIG's the stop's the
#       @ the SE's the (the table's the place's the + the open's the +
#       the sticks' the x2's the + the bow's the, the 1150-f's the,
#       the gap's the 21.5->6.2's the)
#   s4  the kite's the N's the 23-m's the + the W4 (the arrows' the x1's
#       the + the torch's the, the E's the panel's the) @ the NE's the
#   s5  the kite's the S's the 23-m's the + the combat's the (the 4's
#       the arrows' the: the 2's the shots' the + the N's the kite's
#       the 23-m's the + the 2's the shots' the)
#   s6  the kite's the S+W's the the center's the + the torch's the x4's
#       the (the light's the ring's the)
#   s7  the night's the watch's the (the the night's the body's the)
#   s8  the dawn's the (the f18000's the t0.05's the = the 07:12's the)
import math

from demo_author import Guide

g = Guide(scene="guide_night", pos=(19.5, 54, -13.0), yaw=0.0, pitch=-0.1)

SEGMENTS = []


def seg_mark(name, out_frame=None):
    SEGMENTS.append({"name": name, "f": int(g.cursor),
                     "pos": [g.x, g.z]})
    if out_frame is not None:
        SEGMENTS[-1]["campfire_out"] = int(out_frame)


def walk_to(f, px, pz, maxd=2.0):
    # consume any gap's coast into the track first (the face/at's the
    # 20-f's the gap's the the z's the coast's the the walk_leg's the
    # internal's the _to's the divergence's the capture's the)
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


def craft_sticks(f):
    g.panel(f, "inventory", "h0", "left")
    f += 25
    g.panel(f, "table", "g0", "right")
    f += 25
    g.panel(f, "table", "g1", "right")
    f += 25
    g.panel(f, "inventory", "h0", "left")
    f += 25
    g.panel(f, "table", "out", "left")
    f += 25
    g.panel(f, "inventory", "h5", "left")
    f += 25
    return f


def craft_arrow(f, flint_src, feather_src):
    # the swap-free's the 9-tap's the (the 1-stack's the the flint's
    # the feather's the — the cursor's the never's the the 2nd's the
    # item's the the pick's the the before's the merge's the)
    for slot in (flint_src,):
        g.panel(f, "inventory", slot, "left")
        f += 25
    g.panel(f, "inventory", "g0", "right")
    f += 25
    g.panel(f, "inventory", "h5", "left")
    f += 25
    g.panel(f, "inventory", "g1", "right")
    f += 25
    g.panel(f, "inventory", "h5", "left")
    f += 25
    g.panel(f, "inventory", feather_src, "left")
    f += 25
    g.panel(f, "inventory", "g2", "right")
    f += 25
    g.panel(f, "inventory", "out", "left")
    f += 25
    g.panel(f, "inventory", "h7", "left")
    f += 25
    return f


g.capture(0)

# -- s1 the P0's the watch's the (the night's the opens' the, the mobs'
#    the spawn's the the view's the: the sheep's the f60's the, the
#    zombie's the f150's the the 4.7-m's the the south's the east's) --
g.sweep(30, 90, 0.35)
g.sweep(120, 120, -0.90)
g.at(240)
f = 240
seg_mark("s1_p0")

# -- s2 the kite's the N's the 10-m's the + the E's the 19-m's the (the
#    away's the axis's the first's the) + the W1: the table's the
#    craft's the (the E's the 2x2's the) @ the NE's the corner's the ---
# the N's the kite's the first's the (the zombie's the the south's the —
# the away's the axis's the the full's the 3.5-b/s's the net's the; the
# v9's the E-first's the the 70-deg's the off-axis's the the +6-m's the
# only's the capture's the)
g.face(f, 0.0)
f += 25
f = walk_to(f, 19.5, -23.0)
g.face(f, -math.pi / 2)
f += 25
f = walk_to(f, 38.5, -23.0)
g.at(f + 30)
f += 30
g.open_panel(f)
f += 30
g.panel(f, "inventory", "h0", "left")
f += 25
for c in ("g0", "g1", "g2", "g3"):
    g.panel(f, "inventory", c, "right")
    f += 25
g.panel(f, "inventory", "h0", "left")
f += 25
g.panel(f, "inventory", "out", "left")
f += 25
g.panel(f, "inventory", "h4", "left")
f += 25
g.close_panel(f)
f += 60
seg_mark("s2_kite_ne")

# -- s3 the kite's the S's the (the 23-m's the the east's the column's
#    the) + the retreat's the + the stop's the @ the SE's the: the
#    table's the place's the + the open's the + the sticks' the x1's the
#    + the bow's the (the g5's the MR's the cell's the — the "S P"'s the
#    P's the the c2's the, the v12's the g4's the center's the misread's
#    the) + the arrow's the x1's the = the MINIMUM's the combat's the
#    kit's the (the 1100-f's the — the zombie's the the N's the the
#    19-m's the wander's the, the detect's the 16-m's the the SE's the
#    outside's the, the no-approach's the the stop's the duration's the
#    the v11's the capture's the twin's the) ----------------------------
f = walk_to(f, 38.5, -1.5)
g.at(f + 30)
f += 30
f = walk_to(f, 38.5, -2.5)
g.select(f, 5)
f += 25
g.face(f, math.pi)
f += 25
g.place_ahead(f)
f += 60
g.open_ahead(f)
f += 95
f = craft_sticks(f)
# the bow's the (the g1/g3/g7's the sticks' the + the g5's the planks'
# the — the pattern's the "S P"'s the P's the the c2's the)
g.panel(f, "table", "h5", "left")
f += 25
g.panel(f, "table", "g1", "right")
f += 25
g.panel(f, "table", "g3", "right")
f += 25
g.panel(f, "table", "g7", "right")
f += 25
g.panel(f, "table", "h5", "left")
f += 25
g.panel(f, "inventory", "h0", "left")
f += 25
g.panel(f, "table", "g5", "right")
f += 25
g.panel(f, "inventory", "h0", "left")
f += 25
g.panel(f, "table", "out", "left")
f += 25
g.panel(f, "inventory", "h6", "left")
f += 25
g.close_panel(f)
f += 60
g.open_panel(f)
f += 30
f = craft_arrow(f, "h2", "h3")
g.close_panel(f)
f += 60
seg_mark("s3_combat_kit")

# -- s4 the combat (v2: the short-range's the burst's the — the bow's
#    the flat's the range's the v²/g = 11.25-m's the (the g20's the
#    15-b/s's the, the v14's the capture's the: the 19-m's the 4's
#    the all-miss's the = the arc's the drop's the; the fix's the =
#    walk's the N's the 11.5-m's the the 9-m's the stand's the + 4's
#    the tight's the full-draw's the shots' the (the 96-f's the hold's
#    the = the 1.6-s's the cap's the exactly's the + the 30-f's the
#    gap's the); the zombie's the the 16-m's the re-engage's the the
#    0.8-b/s's the chase's the — the kill's the ~422-f's the the
#    melee's the ~500-f's the before's the, the the 0-1's the hit's
#    the; the aim's the = the probe's the pin's the + the ballistic's
#    the lead's the (the arrow_sim's the py's the)) ---------------------
g.select(f, 7)
f += 30
f = walk_to(f, 38.5, -14.0)
g.at(f + 30)
f += 30
# the 4's the shots' the (the the mid-window's the aim's the pin's the
# from the arrow_sim's the — the zombie's the the straight-line's the
# chase's the (the no-knockback's the = the path's the aim's the
# independent's the), the yaw's the constant's the ~49-deg's the the
# pitch's the sweeps' the +11 -> -24-deg's the, the per-shot's the
# the +1.8/+0.9/+0.4/+0.2-m's the the ballistic's the lead's the)
# the hold=100 (the 1.667-s's the > the 1.6-s's the cap's the — the
# v19's the capture's the: the hold96's the the 1-frame's the the
# under-draw's the = the 99-power's the the 19.79-dmg's the the 0.2-hp's
# the the zombie's the the not-dead's the; the 100's the = the full's
# the power's the guaranteed's the)
for (ax, ay, az) in ((34.07, 56.35, -17.84), (34.40, 55.80, -16.55),
                     (35.84, 55.25, -15.60), (36.95, 54.95, -15.52)):
    g.look_point(f, ax, ay, az)
    f += 25
    g.click(f, "right", hold=100)
    f += 100 + 30
g.at(f + 150)
f += 150
seg_mark("s4_kill")

# -- s5 the walk's the S's the (the 11.5-m's the — the zombie's the dead's
#    the, the safe's the) + the stop's the @ the SE's the table's the:
#    the sticks' the x1's the + the arrow's the x1's the + the torch's the
#    (the spares' the — the v20's the capture's the: the NE's the stop's
#    the 21-m's the open_ahead's the = the kReach's the 4.09's the the
#    out-of-reach's the reject's the; the table's the the SE's the only's)
f = walk_to(f, 38.5, -2.5)
g.at(f + 30)
f += 30
# the v21's the capture's the: the bow's the still's the selected's the
# (the s4's the) — the RMB's the the launcher's the = the draw's the not
# the open's the; the sticks' the select's the (the h5's the the digit's
# the 6's the, the non-block's the non-launcher's the) the open's the
g.select(f, 6)
f += 25
g.face(f, math.pi)
f += 20
g.open_ahead(f)
f += 95
f = craft_sticks(f)
g.close_panel(f)
f += 60
g.open_panel(f)
f += 30
f = craft_arrow(f, "b4", "b5")
g.panel(f, "inventory", "h1", "left")
f += 25
g.panel(f, "inventory", "g0", "right")
f += 25
g.panel(f, "inventory", "h5", "left")
f += 25
g.panel(f, "inventory", "g1", "right")
f += 25
g.panel(f, "inventory", "h5", "left")
f += 25
g.panel(f, "inventory", "out", "left")
f += 25
g.panel(f, "inventory", "h8", "left")
f += 25
g.close_panel(f)
f += 60
seg_mark("s5_kit_se")





# -- s6 the kite's the W's the (the 19-m's the the center's the) + the
#    torch's the x4's the (the cells' the (19,-13)/(13,-13)/(26,-13)/
#    (19,-19)'s the — the stand's the 2-m's the south's the the AABB's
#    the zero's the overlap's the, the 16f's the twin's the) ------------
f = walk_to(f, 19.5, -13.0)
g.at(f + 30)
f += 30
g.select(f, 9)
f += 30
# the v22's the capture's the: the walk-to's the the spot's the the
# PLACE's the before's the (the the place-after's the walk's the the
# 6-7-m's the aim's the = the kReach's the 4.09's the out's the reject's
# the, the 1-of-4's the the torch's the the placed's the)
for (cx, cz) in ((26, -13), (19, -13), (13, -13), (19, -19)):
    f = walk_to(f, cx + 0.5, cz + 2.5)
    g.face(f, 0.0)
    f += 25
    g.look_point(f, cx + 0.5, 53.5, cz + 0.5)
    f += 25
    g.click(f, "right", hold=4)
    f += 60
g.select(f, 7)
f += 30
seg_mark("s7_torches")

# -- s7 the night's the watch's the (the 3rd's the person's the sweeps'
#    the + the E's the panel's the showcase's the + the still's the —
#    the night's the body's the, the the torch's the ring's the the
#    view's the) --------------------------------------------------------
g.tap(f, "c")
f += 40
g.sweep(f, 900, 1.00)
f += 900
g.sweep(f, 900, -2.00)
f += 900
g.sweep(f, 900, 1.00)
f += 900
g.tap(f, "c")
f += 40
g.open_panel(f)
f += 40
g.at(f + 1500)
f += 1500
g.close_panel(f)
f += 120
g.sweep(f, 1100, 0.70)
f += 1100
g.sweep(f, 1100, -1.40)
f += 1100
g.at(f + 1200)
f += 1200
seg_mark("s8_watch")

# -- s8 the dawn's the (the f18000's the t0.05's the = the 07:12's the
#    ramp's the start's the — the east's the face's the sunrise's the) --
g.at(17800)
f = 17800
g.face(f, -math.pi / 2)
f += 30
g.at(18300)
f = 18300
seg_mark("s9_dawn")

import os
import json

tl = g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "..", "assets", "demos",
                         "guide_night_timeline.json"))
json.dump({"segments": SEGMENTS},
          open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "guide_night_segments.json"), "w"),
          indent=1)
print(f"segments: {len(SEGMENTS)} | total ~{int(g.cursor)} frames")
