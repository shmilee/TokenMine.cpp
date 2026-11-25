# V3-16b demo ⑥: guide_regen (受伤与回血 — day1 #3).
#
# V3-25 re-author (the 4.0 re-shoot batch's caliber — the guide_food's
# twin): the legacy g.sweep's the single-frame's snap's root cut + the
# gradual's aim + the per-beat's hint's (the action's face — the
# tutorial's mouse-agnostic's convention) + the C's no-op's removal (the
# demo's fixed's third person's the v3.52's follow-up) + the ending's
# 360's removal.
#
# The user's round 2's (11-25): ① the vantage's tour's halved (16b's
# 4700f → ~1300f, the single's sweep's — the right-left's back-and-forth
# cut) ② the regen's watch's view STATIC (no pans around the hearts'
# ticks — the audience watches the heart row + the column's base; the
# camera moves only ONCE: the land's turn toward the column).
#
# The scene = the 12x12's platform (top y54) + the 5-block's stone's
# column (10, 54..58, -8) — the 5-fall's ledge (fall 5 - offset 3 = 2
# damage). The player starts ON the column (10.5, 59, -7.5) facing -Z,
# hunger 14, h0 bread x2.
#
# The BEAT ORDER's the 16b's gate-catch (this re-shoot's probe's): the
# regen's threshold is hunger >= 6 (the config's regen_hunger_threshold
# — NOT the 18's "well-fed's" face), so the 16b's order (fall → long
# watch → eat) let the hp's 18 → 20's regen FINISH (f3600's probe)
# BEFORE the eat's (f4212's) — the "eat's then regen's" story's broken.
# The re-order = fall → EAT (right after the land, the channel straddles
# the first's tick) → the STATIC's watch's the 2nd's tick.
#
# The fall's HONEST landing (the probe's re-pin this re-author — the
# 16b's -10.04's pin was the pre-v3.36's walk model's; the vector's
# model's hold's the 0.05-m's shift): (10.5, 54, -10.087).
from demo_author import Guide

g = Guide(scene="guide_regen", pos=(10.5, 59, -7.5), yaw=0.0, pitch=0.05)

# -- the pointer's capture (the 2-f's face: zero mining's frame) --------
g.capture(0)

# -- P0 the opening's vantage (the halved's tour — the user's round's) --
g.hint(10, "受伤与回血")
g.sweep_gradual(20, 500, 1.5)                 # the single's sweep (no back-forth)
f = 20 + 500                                  # the 2x's pace (the user's round 3)
f = g.look_gradual(f, (10, 54, -10))          # the platform's edge's ahead
f += 20
g.hint(f, "走下高台，掉 2 颗心")
f += 30

# -- P1 the fall (the 5-block's ledge — the 2 damage's face) ------------
# the facing is already the -Z's (the walk's dir — the zero pre-turn's);
# the target's past the edge (the 1.6-m's flat's solve's the W's hold's
# the air's continue's the drift's the honest landing's belt).
f = g.walk_leg(f, 10.5, -9.1)
f = g._now() + 400                             # the air's margin (the land's inside)
g.x, g.y, g.z = 10.5, 54, -10.087              # the honest landing's re-anchor
# the ONE camera move after the land: the slow's turn's toward the
# column (the eat's + the watch's face — the static's hold's anchor).
g.sweep_gradual(f, 195, 2.5)                  # the 2x's pace (the user's round 3)
f += 195

# -- P2 the eat (RIGHT AFTER the land — the tick's order's the fix: the
#    the channel's must straddle the first's tick (the land+840's) for
#    the on-screen's "the bread's then the heart's" story).
g.hint(f, "吃面包回饥饿")
f += 10
g.select(f, 1)                                # the h0's bread
f += 15
f = g.look_gradual(f, (10, 54, -8))           # the column's BASE's face
f += 15
g.click(f, "right", hold=100)                 # the eat's 96-f's channel
f += 300

# -- P3 the regen's watch (STATIC — the user's round's: the view's the
#    hold, the hearts' the content: tick2 (the land+1080's) + the
#    margin's dwell). The damage's at the land's (the probe's ~hold+58);
#    tick1's the land+840's (the channel's inside), tick2's the
#    land+1080's (here, the 163-f's after the hint's). The user's round
#    4's: the 1400-f's the 20-s's post-tick's stare's the dead time —
#    the 500-f's = tick2's cover + the 300-f's full-bar's confirm.
g.hint(f, "饥饿满了会回血")
g.at(f + 500)
f += 500

# -- P4 the ending (the full's bar = the end's — the 360's cut) --
g.hint(f, "回血完成")
f += 200
# the tail's dwell (the finished's panel's dwell's belt — PITS 418's 16)
g.at(f + 240)

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_regen_timeline.json"))
