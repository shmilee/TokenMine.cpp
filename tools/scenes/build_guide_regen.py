# V3-16b demo ⑥: guide_regen (修复回血 — day1 #3).
#
# The scene = guide_regen: the 12x12 platform (top y54) + the 5-block
# stone column at (10, 54..58, -8) (the 5-fall's ledge — the
# fall-damage's 2's face: fall 5 - offset 3 = 2). The player starts ON
# the column (10.5, 59, -7.5) facing -Z, hunger 14, h0 bread x2.
#
# The beats: the column's tour -> walk off (the 5-fall, hp 20 -> 18) ->
# the eat (the 1.6-s's channel, hunger 14 -> 19) -> the 10-s's regen
# gate's wait (the natural regen's face: +1 HP / 4 s, the 1 hunger's
# cost per half-heart — the 18 -> 20's full) -> the ending.
from demo_author import Guide

g = Guide(scene="guide_regen", pos=(10.5, 59, -7.5), yaw=0.0, pitch=0.05)

# -- the pointer's capture (the 2-f's face: zero mining's frame) --------
g.capture(0)

# -- P0 the opening's tour (the column's vantage's face) -----------------
g.sweep(20, 260, 0.70)
g.sweep(280, 260, -1.40)
g.at(560)
g.sweep(560, 300, 0.70)
g.sweep(860, 300, -0.55)
g.at(1300)
g.sweep(1300, 300, 0.55)
g.sweep(1600, 300, -0.55)
g.at(2100)
g.sweep(2100, 300, 0.70)
g.sweep(2400, 300, -0.70)
g.at(3000)
# the platform's face (the -Z's ahead's look)
g.look_at(3000, (10, 54, -10), y_off=0.0)
g.at(3800)
g.sweep(3800, 300, 0.50)
g.sweep(4100, 300, -0.50)
g.at(4700)

# -- P1 the fall (the 5-block's ledge — the 2 damage's face) ------------
g.face(4710, 0.0)
g.at(4900)
g.walk_leg(4900, 10.5, -9.1)                      # past the column's edge
# the fall + the landing's observe (the hurt's flash + the 9/10's bar)
g.at(g.cursor + 2400)
f = g.cursor + 2400

# -- P2 the eat (the bread's x1 — the hunger's 14 -> 19) -----------------
g.select(f, 1)
f += 40
g.look_at(f, (10, 58, -8), y_off=0.5)             # the column's face
f += 30
g.click(f, "right", hold=100)                     # the 96-f's channel
f += 1200

# -- P3 the regen's wait (the 10-s's gate + the 2 x 4-s's intervals) -----
# (the damage's at ~f2060: the gate's open ~f2660, the +1 HP ~f2900, the
#  +1 HP ~f3140 — the 3.5-s's over-margin's still + the bar's watch)
g.at(f + 400)
f += 400
# the regen's watch (the 10-s's gate's open + the 2 x 4-s's ticks — the
# hearts' bar's 9/10 -> 10/10's the still's belt)
g.sweep(f, 300, 0.50)
g.sweep(f + 300, 300, -0.50)
f += 600
g.at(f + 800)
f += 800
g.sweep(f, 300, 0.50)
g.sweep(f + 300, 300, -0.50)
f += 600
g.at(f + 800)
f += 800
# the full bar's face (the 10/10's hearts's still)
g.at(f + 1000)
f += 1000

# -- P4 the ending (the full bar's face — the third person's tour) -------
g.select(f, 1)
f += 40
g.tap(f + 20, "c")
g.sweep(f + 80, 300, 0.90)
g.sweep(f + 380, 300, -1.80)
g.at(f + 700)
g.sweep(f + 700, 300, 0.90)
g.sweep(f + 1000, 300, -0.90)
g.at(f + 1400)
g.sweep(f + 1400, 300, 0.60)
g.sweep(f + 1700, 300, -0.60)
g.at(f + 2100)
g.sweep(f + 2100, 300, 0.40)
g.sweep(f + 2400, 300, -0.40)
g.at(f + 3100)

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_regen_timeline.json"))
