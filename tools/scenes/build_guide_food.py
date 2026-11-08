# V3-16b demo ⑤: guide_food (补充体力 — day1 #2).
#
# The scene = guide_food: the 12x12 platform (top y54) + the 6x4 wheat
# field (farmland at y53, wheat at y54: wheat_7 x8 [the harvestable's
# ripe] + the young's stages x16 [they GROW during the demo — the
# frame-locked growth's visual face]). The player starts at
# (0.5, 54, 0.5) facing the field with hunger 10 (the bar's half's
# face).
#
# The beats: tour -> harvest the 8 ripe (the instant break's tap) ->
# the collect's perimeter pass -> the E panel's bread x5 (16 wheat's
# 15's consume, 1's left) -> the eat's x2 (the 1.6-s's channel, the
# hunger's 10 -> 20's full) -> the field's tour's end.
from demo_author import Guide

g = Guide(scene="guide_food", pos=(0.5, 54, 0.5), yaw=2.5, pitch=-0.15)

# -- the pointer's capture ---------------------------------------------
g.capture(0)

# -- P0 the opening's tour (the field's face) ----------------------------
g.sweep(20, 240, 0.55)
g.sweep(260, 240, -1.10)
g.at(500)
g.sweep(500, 240, 0.55)
g.sweep(740, 240, -0.45)
g.at(1100)
g.sweep(1100, 300, 0.35)
g.sweep(1400, 300, -0.35)
g.at(1800)
g.sweep(1800, 400, 0.50)
g.sweep(2200, 400, -0.50)
g.at(2800)
g.sweep(2800, 400, 0.40)
g.sweep(3200, 400, -0.40)
g.at(3800)
# the field's face (the 8 ripe's wheat_7's the 1-m's ahead's look)
g.face(3800, -0.93)
g.look_at(3900, (4, 54, -1))
g.sweep(4000, 300, 0.30)
g.sweep(4300, 300, -0.30)
g.at(4800)

# -- P1 the harvest (the instant break's tap — the hardness-0's face) ----
# the south row (z-1): the 6 ripe, the stance line's z-0.5's 1-m's belt
f = g.walk_polyline(4810, [(2.5, 0.5)])
g.at(f + 60)
f += 60
# the south row's 6 ripe (the per-cell's 2-f's tap: the hardness-0's
# cascade's gate-catch — a 4-f's tap breaks the row's 2nd's plant)
for x in (2, 3, 4, 5, 6, 7):
    g.look_at(f, (x, 54, -1))
    g.click(f + 25, "left", hold=1)
    f += 90
    if x < 7:
        f = g.walk_polyline(f, [(float(x) + 1.5, 0.5)])
        f += 40
# the north row (z-4): the 2 ripe (the field's cross — the flush's face)
f = g.walk_polyline(f, [(7.5, -5.5), (2.5, -5.5)])
# the north row's face (the 2 ripe's the 2-m's belt)
g.at(f + 600)
f += 600
for x in (3, 2):
    g.look_at(f, (x, 54, -4))
    g.click(f + 25, "left", hold=1)
    f += 90
# the collect's perimeter pass (the west edge's z-4's drops + the south
# edge's z-1's drops — the 1.0-m's magnet's belt)
f = g.walk_polyline(f, [(2.5, 0.5), (7.5, 0.5)])
g.at(f + 120)
f += 120

# -- P2 the bread's craft (the E panel's 5 crafts — the 15 wheat's face) --
g.open_panel(f)
f += 30
for i in range(5):
    g.panel(f, "inventory", "h0", "left")     # carry the wheat
    f += 25
    for c in ("g0", "g1", "g2"):
        g.panel(f, "inventory", c, "right")   # the 3's place
        f += 25
    g.panel(f, "inventory", "h0", "left")     # the wheat's merge back
    f += 30
    g.panel(f, "inventory", "out", "left")    # craft the bread x1
    f += 25
    g.panel(f, "inventory", "h1", "left")     # the bread's place (h1)
    f += 100
g.close_panel(f)
f += 300
# the bread's x5's face (the h1's stack's still)
g.select(f, 2)
f += 60
g.at(f + 600)
f += 600

# -- P3 the eat (the 1.6-s's channel x2 — the hunger's 10 -> 20) ---------
g.select(f, 2)                                # the h1's bread
f += 40
g.look_at(f, (4, 54, -2))                      # the field's face while eat
f += 30
g.click(f, "right", hold=100)                  # eat #1 (the 96-f's channel)
f += 600
g.click(f, "right", hold=100)                  # eat #2 (the 20's full)
f += 600
# the full bar's face (the hunger's 20 + the bread's x3's still)
g.at(f + 600)
f += 600

# -- P4 the ending (the full bar's face + the field's tour's third person)
g.select(f, 1)                                # the h0's wheat's 1's face
f += 40
g.select(f, 2)                                # the h1's bread's 3's face
f += 60
g.tap(f + 20, "c")                            # the third person
g.sweep(f + 80, 300, 1.00)
g.sweep(f + 380, 300, -1.00)
g.at(f + 700)
g.sweep(f + 700, 300, 0.70)
g.sweep(f + 1000, 300, -0.70)
g.at(f + 1400)
# the field's face (the growth's 2nd's generation's tour — the frame's
# locked pace's visual)
g.tap(f + 1400, "c")                          # the first person
g.look_at(f + 1460, (4, 54, -2))
g.at(f + 2000)
g.sweep(f + 2000, 300, 0.50)
g.sweep(f + 2300, 300, -0.50)
g.at(f + 3000)
# the field's 2nd generation (the growth's 2nd's row's face)
g.look_at(f + 3000, (4, 54, -3))
g.at(f + 3800)
g.sweep(f + 3800, 300, 0.50)
g.sweep(f + 4100, 300, -0.50)
g.at(f + 4600)
g.sweep(f + 4600, 300, 0.50)
g.sweep(f + 4900, 300, -0.50)
g.at(f + 5400)
# the bread's x3 + the wheat's x1's final's face (the first person's
# still)
g.look_at(f + 5400, (4, 54, -1))
g.at(f + 7000)

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_food_timeline.json"))
