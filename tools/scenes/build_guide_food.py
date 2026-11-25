# V3-16b demo ⑤: guide_food (种植与进食 — day1 #2).
#
# V3-25 re-author (the user's 11-25 ruling: the PLANTING's chain is
# the demo's face — dig the soil, the seed in the hand, till, plant,
# grow (WATCHED at the demo's pace — the dmo's world-spec's
# growth_speed 6, 720/6 = 120 f per stage), harvest, bread, eat. The
# bottom's hint's line is the demo's own face per beat (the dmo's
# hint events — the tutorial's default's "W to go forward / chop a
# tree" is DORMANT in the replay).)
#
# The scene (the V3-25's surgery on the 16b's asset): the 24-wheat's
# pre-planted field is UNTILLED (the wheat's journal's -> air: the
# empty's 6x4's farmland's face) + the 5-dirt's strip (the stone's ->
# dirt: (7,53,0) + (8,53,-4..-1), the field's east's extension) + the
# player's hand's h0 = wheat_1 x5 (the bootstrap's seed's prepared
# start — the world has NO wild wheat: the wheat_7's drops are the
# item's only, the seed's = the young's plant's break — the demo's
# own chain's face, the guide_nether's flint's precedent).
#
# The walk's discipline (the v3.36's vector model's belt): the
# axis-aligned's legs ONLY (the sub-axis's physics loss — the
# corner's split); the CORNER inherits the TRACK's coordinate
# (g.x / g.z — the honest landing's drift's self-consistency, the
# PITS 436's waypoint's refit's the static's twin). The aim's
# look_gradual reads the track's actual position (the drift's
# tolerance's belt).
from demo_author import Guide

g = Guide(scene="guide_food", pos=(0.5, 54, 0.5), yaw=2.5, pitch=-0.15,
          growth_speed=6)  # V3-25: the growth's 6x's watch's face

# The aim's facings (the V3-25's re-shoot's user's feedback's root: the
# walk_leg's legacy's auto-face's the walk's dir's the single-frame's
# snap's — the audience's "the view's suddenly's changed" + the aim's
# the side's. Every leg now walks toward the TARGET's facing (the
# w/s/a/d's relative's strafe), the turn's smooth (the 2.0 rad/s's
# tier's). EAST = the strip's column's face, NORTH = the field's row's).
EAST = -1.5707963
NORTH = 0.0

# -- the pointer's capture ---------------------------------------------
g.capture(0)

# -- P0 the opening's tour (the empty's field + the dirt's strip) -------
g.hint(10, "麦种在手里，先挖土")
g.sweep_gradual(20, 600, 1.8)               # the 100-deg's showcase
g.sweep_gradual(620, 600, -1.8)             # back to the field's face
f = 1240
f = g.look_gradual(f, (8, 54, -2))          # the strip's face (the dirt's y54's)

# -- P1 the dig (the 5 dirt's — the bare-hand's 0.75-s's hold) ----------
# the stances: (5.5,0.5) for the (7,0) cell; (6.5,z+0.5) for the
# (8,z) column (the 2-m's west, the reach's 4.09's margin).
g.hint(f + 40, "按 LMB 挖泥土")
f += 60
# the collect's per-dig dwells (the probe's gate-catch ×2: the 2.5-m's
# magnet's range is not a pickup range — the long-range's pull creeps
# 200-700 f's, the 2-m's drop outlived the till's panel). The (8,z)'s
# column digs from the 7.0's stance (the 1.5-m's pull's ~100-f's face),
# the (7,0)'s from the 5.5's (the 2-m's, the 240-f's dwell).
f = g.walk_leg(f, 5.5, 0.5, face=EAST)      # the east's face's the W's
f = g.look_gradual(f, (7, 54, 0))
f += 25
g.mine(f, 0.9)                              # the dirt's #1 (0.75-s + margin)
f += 240                                     # the 2-m's drop's collect's dwell
f = g.walk_leg(f, g.x, -0.5, face=EAST)     # the strafe's 'd' (the south's)
f = g.walk_leg(f, 7.0, g.z, face=EAST)      # the column's 1.5-m's stance (W)
for z in (-1, -2, -3, -4):                  # the (8,-1..-4) column
    f = g.look_gradual(f, (8, 54, z))
    f += 25
    g.mine(f, 0.9)
    f += 150                                 # the 1.5-m's drop's collect
    if z > -4:
        f = g.walk_leg(f, g.x, z - 0.5, face=EAST)  # the strafe's 'd' 
# the till's beat is IN PLACE (the P1's end's stance (7.0,-3.5))

# -- P2 the till (the E's dirt x5 -> farmland x5) ------------------------
# the 1-out-click's 1-recipe's belt (the UX-7's one-craft-per-click's —
# the 16b's bread's 5-round's the same face; the g0's leftover's would
# sit in the grid's inv_grid's, the panel-close's no-return's trap).
g.hint(f, "按 E 合成：泥土变耕地")
f += 60
g.open_panel(f)
f += 12
g.select(f, 1)                              # the h1's dirt (the sweep's x5)
f += 25
g.panel(f, "inventory", "h1", "left")       # carry the dirt's x5
f += 25
g.panel(f, "inventory", "g0", "left")       # the grid's g0 (x5 in)
for i in range(5):
    f += 25
    g.panel(f, "inventory", "out", "left")  # the farmland's x1 (one click)
    f += 25
    g.panel(f, "inventory", "h2", "left")   # the h2's stack's merge
f += 25
g.close_panel(f)
f += 60

# -- P3 the place (the 5 farmland's in the strip's holes) ---------------
# the (7,0)'s first (the (5.5,0.5)'s stance — the P2's in-place's end's
# walk-back), then the column's north-to-south's.
g.hint(f, "按 RMB 放下耕地")
# the east's facing's persist (the P1's end's face): the west's step is
# the backward's S (the target's side's the view's keep), the 4-m's leg's
# the strafe's 'd's (the south's), the column's hops's the 'a's.
f = g.walk_leg(f, 5.5, g.z, face=EAST)      # the backward's S (the west's)
f = g.walk_leg(f, g.x, 0.5, face=EAST)      # the (7,0)'s stance (the strafe's 'd')
g.select(f, 3)                              # the h2's farmland
f += 40
# the holes' bottoms (the y53's stone's top's face — ground_y 54's default)
g.place_at(f, (7, 53, 0))
f = g._now() + 25                            # the cursor's resync
f = g.walk_leg(f, g.x, -0.5, face=EAST)     # the strafe's 'a' (the north's)
f = g.walk_leg(f, 6.5, g.z, face=EAST)      # the column's stance (the W's)
for z in (-1, -2, -3, -4):
    # the RELATIVE place (the walk drift's belt — the probe's gate-catch:
    # the absolute cell's aim ran off the (8,z)'s face by the accumulated
    # 0.3-0.5-m's drift, the 8 clicks' the silent no-op's root). The
    # facing is exact (the yaw's deltas accumulate drift-free), the
    # place_ahead's target rides the actual position (the cell's 0.5-m's
    # margin absorbs the drift).
    g.place_ahead(f, 2.0, ground_y=54)      # the facing's east's persist
    f = g._now() + 25                        # the cursor's resync
    if z > -4:
        f = g.walk_leg(f, g.x, z - 0.5, face=EAST)  # the strafe's 'd' 

# -- P4 the plant (the 5 seed's in the field's north row) ----------------
# the probe's gate-catch x3: (a) the strip's (x8, z-1..-4)/(7,0)'s roll's
# sequences carry 1000-f's deterministic gaps (the stall's cells) — the
# FIELD's rows are the 16b's proven growth zone; the till'd strip stays
# as the empty's beds (the ending's "the next's generation's" face).
# (b) the west-stance's 2-m's ray crosses the ALREADY-PLANTED neighbor's
# cross block (the ray's first non-air = the neighbor = the placement's
# reject, the x3/x5's no-plant's root). (c) the direction-reversal's hop
# overshoots 0.45-m (the residual velocity's the model's breakdown, the
# P6's x-drift's root). The fix = the SOUTH's stance row (x+0.5, -2.5)
# facing the row's north: the parallel's 1-m's rays pass the (x,54,-3)'s
# air (above the row z-3's farmland) — zero occlusion, zero reversal
# (the x6-first's west's back), the 30-f's settle after the turn.
g.hint(f, "按 RMB 种下麦种")
g.select(f, 1)                              # the h0's wheat_1
f += 40
f = g.walk_leg(f, g.x, -2.5, face=NORTH)    # the row's south's step (the S's)
g.at(f + 30)
f += 30                                     # the residual's settle
g.place_ahead(f, 1.0, ground_y=54)           # the (6,54,-4)'s plant (smooth)
f = g._now() + 25
for x in (5, 4, 3, 2):
    f = g.walk_leg(f, x + 0.5, g.z, face=NORTH)  # the strafe's 'a' (west)
    g.place_ahead(f, 1.0, ground_y=54)      # the facing's north's persist
    f = g._now() + 25
PLANT_LAST = g._now() - 29                   # the 5th's RMB's down's frame
g.hint(f, "麦子开始长了")

# -- P5 the growth's watch (the 6 stage's at 120-f's = 720-f's) ----------
# the camera's the slow's pan's the whole's watch (the still-frame's
# audit's belt — the world's stage's jumps are the content). The pace-6's
# M's pin (the refO-era's probe's the pace-1's — the serialize's
# growth_speed's the absent's the gate-catch): the x2's (the last-
# planted's) ripe's in (8250, 8400]'s, the PLANT_LAST's 5527's offset's
# ~2800's, the M's 3000's margin's ~288-f's to the harvest's click.
g.sweep_gradual(f, 1600, 1.9)               # the row's slow's pan
f += 1600
f = g.look_gradual(f, (4, 54, -4))          # the row's middle's face
f += 100
g.sweep_gradual(f, 1000, -1.4)              # the pan's back
f += 1000
f = g.look_gradual(f, (2, 54, -4))
f += 200
# the all-ripe's frame M = the empirical's pin (the probe's truncated's
# run's read's — the pace's 6's the x2's last's ripe's + the margin's).
M = PLANT_LAST + 3000
if f < M:
    f = M
g.hint(f, "麦子熟了")
f += 60

# -- P6 the harvest (the 5 ripe's — the hardness-0's 1-f's tap) ---------
# the same south's stance's row (the P5's end's (2.5,-2.5) = the (2)'s
# stance in place), the east's back (the same direction's hops).
g.hint(f, "按 LMB 收麦子")
f = g.face_gradual(f, NORTH)                # the P5's end's facing's correct
f = g.look_point(f, g.x, 54.5, g.z - 1.0)   # the wheat's 1-m's north (smooth)
f += 25
g.click(f, "left", hold=1)                  # the (2)'s the 1-f's tap
f += 70
for x in (3, 4, 5, 6):
    f = g.walk_leg(f, x + 0.5, g.z, face=NORTH)  # the strafe's 'd' (east)
    f = g.look_point(f, g.x, 54.5, g.z - 1.0)  # the facing's north's persist
    f += 25
    g.click(f, "left", hold=1)              # the 1-f's tap = exactly 1
    f += 70
# the collect's dwell (the 5 drops' the 1-m's magnet's belt)
g.at(f + 150)
f += 150

# -- P7 the bread's x3 (the E's wheat x10 -> bread x3 + wheat x1) --------
g.hint(f, "按 E 合成面包")
f += 40
g.open_panel(f)
f += 12
# the 16b's proven round (the refN's gate-catch: the once-carry's +
# the right's place-3's lefts the hand's wheat's — the out's craft's
# click's rejected, the grid's leftover's trapped): each round CARRIES
# the whole stack, places 1 each (the right's), returns the REMAINDER
# to h0, crafts 1, stacks the bread in h2.
for i in range(3):                          # the 3-wheat's recipe's 3 rounds
    g.panel(f, "inventory", "h0", "left")   # carry the wheat (whole stack)
    f += 25
    for c in ("g0", "g1", "g2"):
        g.panel(f, "inventory", c, "right") # place the wheat's x1
        f += 25
    g.panel(f, "inventory", "h0", "left")   # the remainder's back to h0
    f += 25
    g.panel(f, "inventory", "out", "left")  # craft the bread's x1
    f += 25
    g.panel(f, "inventory", "h2", "left")   # the h2's bread's stack
    f += 25
g.close_panel(f)
f += 60

# -- P8 the eat (the 2 bread's — the 1.6-s's channel's x2) ---------------
g.hint(f, "吃合成的面包")
g.select(f, 3)                              # the h2's bread
f = g.look_gradual(f, (4, 54, -2))          # the field's face while eat
f += 25
g.click(f, "right", hold=100)               # the eat's #1 (the 96-f's)
f += 400
g.click(f, "right", hold=100)               # the eat's #2 (the 20's full)
f += 300

# -- P9 the ending (the user's 11-25: the eat's full = the end's — the
#    360's tour's the unnecessary's cut; the final's inventory's face
#    is the bread's 1 + the wheat's 1's the hotbar's dwell).
g.select(f, 1)                              # the h0's wheat's 1's face
f += 150
# the tail's dwell (the finished's panel's dwell's belt — PITS 418's 16)
g.at(f + 240)

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_food_timeline.json"))
