# V3-16c demo 3: guide_house (full house build + bed, day1 #4).
#
# The scene is guide_house: the 12x12 platform (x0..11, z-11..1, top y54)
# + the crafting table at (0,54,-3) (the bed shaped recipe host, prepared
# start) + the player at (0.5,54,-2.5) at night (time 0.75) with h0
# planks 36 / h1 wool 3.
#
# The house is 4x3 (x2..5, z-6..-4), 2 wall tiers + flat roof 12, door at
# (3,-4) (the 2-tier gap). The 30-block plan (v15 — the roof rows from the
# elevated eye only: the ground's eye y55.62 < y56 intercepts the 2nd tier's
# side face = the stray's root, the v13 gate-catch):
#   D: bed craft FIRST (the table opens from the start cell, WWW/PPP,
#      the bed to h2, 3 planks out of 36)
#   A: 1st wall 9 (ground top-face aim from 1 m outside, v10-validated)
#   B: 2nd wall 9 (ground top-face aim at y55, v10-validated)
#   C1: scaffold S1 (1,54,-6) + S2 (1,55,-5) outside NW (ground aim,
#      v10-validated) — the 2-tier step-up block-catch lift
#   C2: climb S1 top -> S2 top (the only step-up chain, v13-validated)
#   C3: middle x2 + north x2 from the S2 top (the 2nd west/NW top faces,
#      v13-validated)
#   C3b: step-up onto the middle x2 top (y57) + the roof walk to the north
#        row line z-5.5
#   C3d: north x3/x4/x5 from the roof walk (the 2nd north top faces, 1 m
#        ahead — the v10 roof-walk's validated aim)
#   C4: descent (the roof west edge 2-drop onto the 1st west top y55, the
#        S1 top walk, the 1-drop to the ground)
#   C9: scaffold mine (the net 0-block, the magnet pick-up)
#   C7: interior middle x3/x4/x5 (the roof chain's +X faces from the door
#        stop (3.5,-4.7) + (4.5,-4.7))
#   C5: south x2/x3/x4/x5 from the ground door-gap column (3.5,-3.5)
#        (the middle +Z faces' look-up — the x3 column's both tiers air)
#   C8: the bed place (place_ahead +X from the door stop -> (4,54,-5))
#        + SLEEP (the night window: sleep at f < 10800, lands ~f6300)
#   E: the dawn tour (sleep does NOT move the player — the bed view +
#        the walk-around + the sunrise)
import math
from demo_author import Guide

g = Guide(scene="guide_house", pos=(0.5, 54, -2.5), yaw=-1.0, pitch=-0.1)

def place_ground(f, cell, ground_y=54.0):
    """One placement beat: ground aim (the cell top face center) + RMB
    place + settle."""
    g.look_ground(f, cell, ground_y=ground_y)
    g.click(f + 25, "right")
    return f + 70

def place_point(f, point):
    """One placement beat: exact point aim (a side/top face point) +
    RMB place + settle."""
    g.look_point(f, *point)
    g.click(f + 25, "right")
    return f + 70

YAW_N = 0.0           # -Z facing
YAW_W = math.pi / 2   # -X facing
YAW_S = math.pi       # +Z facing
YAW_E = -math.pi / 2  # +X facing

# -- the pointer capture (the 2-f face) ------------------------------------
g.capture(0)

# -- P0 the night opening (the short tour) ----------------------------------
g.sweep(20, 300, 0.70)
g.sweep(320, 280, -1.40)
g.at(600)
g.look_at(600, (1, 54, -1))                    # the table face

# -- Phase D: the bed craft FIRST (the prepared table at (1,54,-1)) -----------
# (the table's NOT at the start cell — the v12 gate-catch: the spawn's
#  inside-the-table overlap's the y55's push-up, the open's miss, the
#  stray's planks, the panel's 11's)
g.look_point(620, 1.5, 54.5, -1.05)            # the table -Z face
g.click(645, "right")                          # table open
f = 645 + 120
g.panel(f, "inventory", "h1", "left")          # carry the wool x3
f += 25
for c in ("g3", "g4", "g5"):                   # the WWW row
    g.panel(f, "table", c, "right")
    f += 25
g.panel(f, "inventory", "h0", "left")          # carry the planks x36
f += 25
for c in ("g6", "g7", "g8"):                   # the PPP row
    g.panel(f, "table", c, "right")
    f += 25
g.panel(f, "inventory", "h0", "left")          # the planks merge back
f += 25
g.panel(f, "table", "out", "left")             # take the bed
f += 25
g.panel(f, "inventory", "h2", "left")          # the bed to h2
f += 25
g.close_panel(f)
f += 120

# -- Phase A: the 1st wall's 9 (the ground top-face aim) ----------------------
f = g.walk_polyline(f, [(2.5, -2.5)])
g.face(f, YAW_N)
f = place_ground(f + 20, (2, 54, -4))
f = g.walk_polyline(f, [(4.5, -2.5)])
g.face(f, YAW_N)
f = place_ground(f, (4, 54, -4))
f = g.walk_polyline(f, [(5.5, -2.5)])
g.face(f, YAW_N)
f = place_ground(f, (5, 54, -4))
f = g.walk_polyline(f, [(6.5, -2.5), (6.5, -4.5)])
g.face(f, YAW_W)
f = place_ground(f, (5, 54, -5))
f = g.walk_polyline(f, [(6.5, -5.5)])
g.face(f, YAW_W)
f = place_ground(f, (5, 54, -6))
f = g.walk_polyline(f, [(6.5, -7.5), (4.5, -7.5)])
g.face(f, YAW_S)
f = place_ground(f, (4, 54, -6))
f = g.walk_polyline(f, [(3.5, -7.5)])
g.face(f, YAW_S)
f = place_ground(f, (3, 54, -6))
f = g.walk_polyline(f, [(2.5, -7.5)])
g.face(f, YAW_S)
f = place_ground(f, (2, 54, -6))
f = g.walk_polyline(f, [(1.5, -7.5), (1.5, -4.5)])
g.face(f, YAW_E)
f = place_ground(f, (2, 54, -5))

# -- Phase B: the 2nd wall's 9 (the 1st wall top-face aim) --------------------
f = g.walk_polyline(f, [(1.5, -2.5), (2.5, -2.5)])
g.face(f, YAW_N)
f = place_ground(f, (2, 55, -4), ground_y=55.0)
f = g.walk_polyline(f, [(4.5, -2.5)])
g.face(f, YAW_N)
f = place_ground(f, (4, 55, -4), ground_y=55.0)
f = g.walk_polyline(f, [(5.5, -2.5)])
g.face(f, YAW_N)
f = place_ground(f, (5, 55, -4), ground_y=55.0)
f = g.walk_polyline(f, [(6.5, -2.5), (6.5, -4.5)])
g.face(f, YAW_W)
f = place_ground(f, (5, 55, -5), ground_y=55.0)
f = g.walk_polyline(f, [(6.5, -5.5)])
g.face(f, YAW_W)
f = place_ground(f, (5, 55, -6), ground_y=55.0)
f = g.walk_polyline(f, [(6.5, -7.5), (1.5, -7.5), (1.5, -4.5)])
g.face(f, YAW_E)
f = place_ground(f, (2, 55, -5), ground_y=55.0)
f = g.walk_polyline(f, [(1.5, -7.5), (2.5, -7.5)])
g.face(f, YAW_S)
f = place_ground(f, (2, 55, -6), ground_y=55.0)
f = g.walk_polyline(f, [(3.5, -7.5)])
g.face(f, YAW_S)
f = place_ground(f, (3, 55, -6), ground_y=55.0)
f = g.walk_polyline(f, [(4.5, -7.5)])
g.face(f, YAW_S)
f = place_ground(f, (4, 55, -6), ground_y=55.0)

# -- C1: the scaffold (all 2 from the ground, v10-validated aim) --------------
f = g.walk_polyline(f, [(1.5, -7.5), (0.5, -7.5), (0.5, -5.5)])
g.face(f, YAW_E)
f += 10
f = place_ground(f, (1, 54, -6))                # S1 (stone top face)
f = g.walk_polyline(f, [(0.5, -4.5)])
g.face(f, YAW_E)
f += 10
f = place_point(f, (2.1, 55.4, -4.5))           # S2 (2nd west -X face)

# -- C2: the climb (S1 top + S2 top, the only step-up chain) ------------------
f = g.walk_polyline(f, [(0.5, -5.5)])
g.face(f, YAW_E)
f += 10
f = g.walk_leg(f, 1.5, -5.5, y=55)              # S1 top
g.face(f, YAW_S)
f += 10
f = g.walk_leg(f, 1.5, -4.0, y=56)              # S2 top

# -- C3: middle x2 + north x2 (the S2 top, the body clear of x2) --------------
f = place_point(f, (2.5, 56.0, -4.5))           # middle x2 (2nd west top)
f = place_point(f, (2.5, 56.0, -5.5))           # north x2 (2nd NW top)

# -- C3b: the middle x2 top step-up + the north row line (the elevated's eye)
f = g.walk_leg(f, 2.6, -4.0, y=57, hold=28)     # middle x2 top (the lift loss)
f = g.walk_leg(f, 2.6, -5.5, y=57)              # the north row line (flat)

# -- C3d: the north row x3/x4/x5 (the roof walk, the 2nd north top face) ------
f = place_point(f, (3.5, 56.0, -5.5))           # north x3
f = g.walk_leg(f, 3.6, -5.5, y=57)
f = place_point(f, (4.5, 56.0, -5.5))           # north x4
f = g.walk_leg(f, 4.6, -5.5, y=57)
f = place_point(f, (5.5, 56.0, -5.5))           # north x5

# -- C3e: the south row x5/x4 (the roof walk, the 2nd south top faces — the
# middle slot's still air = the y57 crossing's clear; south x3 = the door gap,
# the C5b's face) + the middle x5 (the middle x4 +X face — the steep aim's
# z-4.8, the v16 gate-catch: the z-5.0 crossing's 2-cm margin's the north
# floor's stray) --------------------------------------------
f = g.walk_leg(f, 5.5, -5.5, y=57)
f = place_point(f, (5.5, 56.0, -3.5))           # south x5
f = g.walk_leg(f, 4.5, -5.5, y=57)
f = place_point(f, (4.5, 56.0, -3.5))           # south x4
f = place_point(f, (5.0, 56.9, -4.25))          # middle x5 (middle x4 +X)

# -- C4: the descent (the 4-hops off the roof west edge — the W released
# before each edge = no fall drift, the v16/v17 gate-catch: the 16-f hold's
# 0.5-m drift's the C9's lost mine, the landing's (0.3,-5.5)'s tight belt) --
g.face(f, YAW_W)
f = g.walk_leg(f, 3.5, -5.5, y=57)
f = g.walk_leg(f, 2.6, -5.5, y=57)
f = g.walk_leg(f, 2.0, -5.5, y=55, hold=8)      # the 2-block drop (S1/1st west top)
f = g.walk_leg(f, 1.4, -5.5, y=55, hold=8)      # the S1 top's walk
f = g.walk_leg(f, 0.8, -5.5, y=55, hold=8)
f = g.walk_leg(f, 0.3, -5.5, y=54, hold=8)      # the 1-block drop (off S1)

# -- C9: the scaffold mine (the net 0-block, the magnet pick-up — the v16
# gate-catch: the C4 landing's drift 0.3-0.7, the walk-to's lost, the mine's
# from the landing's itself, the faces' robust to the x0.3-0.9's belt) ------
g.look_point(f, 1.0, 55.5, -4.5)                # S2 -X face
g.mine(f + 20, seconds=3.5)
f += 20 + 210 + 10
g.look_point(f, 1.0, 54.8, -5.7)                # S1 -X face
g.mine(f + 20, seconds=3.5)
f += 20 + 210 + 10

# -- C7: the interior middle row x3/x4 (the roof chain's +X faces — the y56
# crossing's the air slot, the v15 gate-catch: the placed middle x3's bottom)
# (the leg 2's target z-1.8 = the v18 gate-catch: the 3.5-m leg's 0.5-m's
#  walk-loss's the end z-2.6, the 1st south wall's z-4..-3's band's clamp)
f = g.walk_polyline(f, [(1.5, -5.5), (1.5, -1.8), (3.5, -1.8), (3.5, -4.7)])
f = place_point(f, (3.0, 56.4, -4.5))           # middle x3 (middle x2 +X)
f = g.walk_polyline(f, [(4.6, -4.7)])
f = place_point(f, (4.0, 56.4, -4.5))           # middle x4 (middle x3 +X)
f = g.walk_polyline(f, [(3.5, -4.7)])

# -- C5b: the south row x2/x3 (the ground door-gap column (3.5,-3.5), the
# middle +Z faces — the x3 column's both tiers air, the look-up's belt) ------
f = g.walk_polyline(f, [(3.5, -3.5)])
f = place_point(f, (2.5, 56.4, -4.05))          # south x2 (middle x2 +Z)
f = place_point(f, (3.5, 56.4, -4.05))          # south x3 (middle x3 +Z)
f = g.walk_polyline(f, [(3.5, -4.7)])

# -- C8: the bed place + the SLEEP (the night window, f < 10800) --------------
g.select(f, 3)                                  # the h2 bed
f += 40
g.face(f, YAW_E)
g.place_ahead(f, dist=1.0, ground_y=54.0)       # bed -> (4,54,-5)
f += 70
g.look_point(f, 4.0, 54.7, -4.6)                # the bed -X face
f += 30
g.click(f, "right")                             # the sleep trigger
f += 250

# -- E: the dawn tour (the sleep does not move the player) ---------------------
g.at(f + 600)
f += 600
g.look_at(f, (4, 54, -5))                       # the bed dawn face
g.at(f + 800)
f += 800
f = g.walk_polyline(f, [(3.5, -2.7)])           # out the door
g.face(f, YAW_N)
g.at(f + 400)
f += 400
g.tap(f + 20, "c")                              # third person
g.sweep(f + 80, 400, 0.90)                       # the house front (roof)
g.sweep(f + 480, 400, -0.90)
g.at(f + 900)
f += 900
g.tap(f, "c")                                   # first person
# the walk-around (the 4 sides — the sweep's hold's full, then the leg)
f = g.walk_polyline(f, [(7.5, -2.7)])           # east
g.face(f, YAW_W)
g.sweep(f + 20, 400, 0.90)
g.sweep(f + 500, 400, -0.90)
g.at(f + 900)
f += 900
f = g.walk_polyline(f, [(7.5, -8.7)])           # NE
g.face(f, YAW_N)
g.sweep(f + 20, 400, 1.10)
g.sweep(f + 500, 400, -1.10)
g.at(f + 900)
f += 900
f = g.walk_polyline(f, [(1.5, -8.7)])           # NW
g.face(f, YAW_N)
g.sweep(f + 20, 400, 0.90)
g.sweep(f + 500, 400, -0.90)
g.at(f + 900)
f += 900
f = g.walk_polyline(f, [(1.5, -2.7)])           # west
g.face(f, YAW_E)
g.sweep(f + 20, 400, 0.90)
g.sweep(f + 500, 400, -0.90)
g.at(f + 900)
f += 900
f = g.walk_polyline(f, [(3.5, -2.7)])           # the door front
g.tap(f + 20, "c")                              # third person
g.sweep(f + 80, 400, 0.70)
g.sweep(f + 480, 400, -0.70)
g.at(f + 900)
f += 900
g.tap(f, "c")                                   # first person
g.face(f, YAW_N)
g.at(f + 400)
f += 400
# the sunrise (the east +X — two sweeps' pair + the final still)
g.look_at(f, (11, 54, -3), y_off=2.0)
g.sweep(f + 20, 400, 0.50)
g.sweep(f + 500, 400, -0.50)
g.at(f + 1200)
f += 1200
g.sweep(f + 20, 400, 0.40)
g.sweep(f + 500, 400, -0.40)
g.at(f + 1400)
f += 1400
g.sweep(f + 20, 400, 0.60)
g.sweep(f + 500, 400, -0.60)
g.at(f + 2200)
f += 2200

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_house_timeline.json"))
