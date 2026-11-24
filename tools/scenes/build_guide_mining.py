# V3-16a pilot: guide_mining (the ④ 挖矿打造工具 — day1's first demo).
#
# The scene = pilot_mine: the 12x12 platform (x0-11, z-11..1, top y54,
# solid y50-53), the tree (the trunk's 6 at (8, 54..59, -8) + the crown's
# leaves y58-60), the mining row at z=-9 (x1-2 coal / x3 iron / x4-11
# stone — the x0's gap is the route's crossing), the sand's block at
# (8, 54, -5) (the furnace's smeltable — the pilot's 熔炼 face; the
# repace V1's 11-20: the pillar's x2 -> the single's x1 — the 2 x 10-s's
# serial's 1380-f's frozen's elimination's scene face).
#
# The robustness's faces (the r1-r29's gate-catches' lessons):
#   - every walk leg is AXIS-ALIGNED (the diagonal's sub-axis physics
#     loss = the 4x's wander's root) and the TRACK models the EXACT
#     corner's arrival (the stop margin's 0.03-0.11 m's short's twin)
#   - every world PLACE / container OPEN is the RELATIVE aim
#     (place_ahead / open_ahead: the target 1 m ahead of the facing —
#     the absolute cell aim's 1-cell's miss's gate-catch: the aim
#     MOVES WITH the player, the drift's tolerance's structural root)
#   - the mine's hold's seconds = the break's time + the margin's belt
#     (the PITS 7's hold-break's twin: the continuation's under-the-
#     platform's out-of-reach)
#   - the pickup's ORDER = the slot's determinism (the first-empty's
#     assignment: wood h0 / sand h1 / table h2 / sticks h3 / pickaxe h4
#     / planks h5 / coal h0 / iron h2 (the table's placed's h2's free)
#     / cobble h6 / furnace h6 / glass h8)
from demo_author import Guide

g = Guide(scene="pilot_mine", pos=(0.5, 54, -2.0), yaw=0.0, pitch=-0.08)

# -- the pointer's capture (the look/panel's gate) -------------------------
g.capture(0)

# -- P0 the opening's observe (the scene's tour — the repace V1: the
#    GRADUAL's sweep, the single-event's snap's the 4.3-s's frozen's
#    elimination) ---------------------------------------------------------
g.sweep_gradual(20, 150, 0.50, steps=50)
g.sweep_gradual(170, 150, -0.90, steps=50)
g.sweep_gradual(320, 120, 0.40, steps=50)
g.sweep_gradual(440, 150, -0.30, steps=50)
g.at(700)

# -- P1 the approach (the tree's west face) ----------------------------------
f = g.walk_polyline(710, [(6.5, -2.0), (6.5, -7.5)])
g.at(f + 60)
f += 60

# -- P2 the chop (the bare-hand's wood x5 — the 3.3-s's hold's face) ---------
for y in (54, 55, 56):
    f = g.look_gradual(f, (8, y, -8))
    g.mine(f + 25, seconds=3.3)
    f += 25 + 198 + 30
# the upper trunk's (y57-58) chop from the in-place (the kReach 4.09's
# belt — the v3.36.0 walk-model's landing offset's the micro-leg's
# elimination, the repace V1's authoring face)
for y in (57, 58):
    f = g.look_gradual(f, (8, y, -8))
    g.mine(f + 25, seconds=3.3)
    f += 25 + 198 + 30
# (the y59's stump's top stays — the treetop's visual's face)

# -- P2.5 the sand's pillar (the furnace's smeltable — the bare-hand's
#    0.75-s's face: the pickaxe's not crafted yet, the h0's wood's twin) --
f = g.look_gradual(f, (8, 54, -5))
g.mine(f + 25, seconds=1.0)
f += 25 + 60 + 25
# the collect's step (the sand's 1.4-m's belt + the wood's twin — the
# corner's split: the walk-model's landing offset keeps each leg's
# axis-aligned)
f = g.walk_polyline(f, [(7.5, -7.55), (7.5, -5.5)])
g.at(f + 120)
f += 120

# -- P5 the E panel: the wood x5 -> the planks x20 (the h5's face) -----------
g.open_panel(f)
f += 30
for i in range(5):
    g.panel(f, "inventory", "h0", "left")      # carry the wood
    f += 25
    g.panel(f, "inventory", "g0", "right")     # place 1
    f += 25
    g.panel(f, "inventory", "h0", "left")      # the wood's merge back
    f += 30
    g.panel(f, "inventory", "out", "left")     # craft the planks x4
    f += 25
    g.panel(f, "inventory", "h5", "left")      # the planks' merge (h5)
    f += 100
g.close_panel(f)
f += 120

# -- P6 the table's craft (the 2x2's 4's place -> h2) -------------------------
g.open_panel(f)
f += 30
g.panel(f, "inventory", "h5", "left")          # carry the planks
f += 25
for c in ("g0", "g1", "g2", "g3"):
    g.panel(f, "inventory", c, "right")        # the 4's place
    f += 25
g.panel(f, "inventory", "h5", "left")          # the planks' merge back
f += 30
g.panel(f, "inventory", "out", "left")         # craft the table
f += 25
g.panel(f, "inventory", "h2", "left")          # the table's place (h2)
f += 40
g.close_panel(f)
f += 100

# -- P7 the table's place (the RELATIVE's face: the 1-m's ahead) --------------
# (the waypoint's z = the walk-model's landing's z — the repace V1's
#  authoring face: the axis-aligned leg's belt, the cell aim's the
#  1-m's tolerance's no-change)
f = g.walk_polyline(f, [(5.5, -5.44)])
f = g.face_gradual(f, 0.0)                     # the -Z's facing (the smooth's turn)
g.select(f, 3)                                 # the h2's table
f += 40
g.place_ahead(f)                               # the table -> (5,54,-7)
f += 100
g.open_ahead(f)                                # the table's open
f += 95

# -- P8 the sticks x8 (h3) + the wooden pickaxe (h4) ---------------------------
for i in range(2):
    g.panel(f, "inventory", "h5", "left")      # carry the planks
    f += 25
    g.panel(f, "table", "g0", "right")         # 1
    f += 25
    g.panel(f, "table", "g1", "right")         # 1
    f += 25
    g.panel(f, "inventory", "h5", "left")      # the planks' merge back
    f += 30
    g.panel(f, "table", "out", "left")         # craft the sticks x4
    f += 25
    g.panel(f, "inventory", "h3", "left")      # the sticks' merge (h3)
    f += 110
g.panel(f, "inventory", "h5", "left")          # the pickaxe: PPP + 2 handles
f += 25
for c in ("g0", "g1", "g2"):
    g.panel(f, "table", c, "right")
    f += 25
g.panel(f, "inventory", "h5", "left")
f += 30
g.panel(f, "inventory", "h3", "left")
f += 25
g.panel(f, "table", "g4", "right")
f += 25
g.panel(f, "table", "g7", "right")
f += 25
g.panel(f, "inventory", "h3", "left")
f += 30
g.panel(f, "table", "out", "left")             # craft the pickaxe
f += 25
g.panel(f, "inventory", "h4", "left")          # the pickaxe's place (h4)
f += 40
g.close_panel(f)
f += 120

# -- P9 the mine (the z-9.5's line: the 1-m's north's face aim — the
#    row's own wall's line-of-sight's gate-catch: a 3-m's lateral aim
#    passes through the neighbor's block at y54.7; mine each cell from
#    directly above, the drops' 1.0-m's magnet's belt's twin) -------
g.select(f, 5)                                 # the h4's pickaxe
f += 40
f = g.walk_polyline(f, [(0.5, -5.44), (0.5, -9.5), (2.5, -9.5)])
# the west: the coal x2 (the 1.125-s + the 0.275-s's margin) + the iron
for x in (2, 1):
    f = g.look_gradual(f, (x, 54, -9))
    g.mine(f + 25, seconds=1.4)
    f += 25 + 84 + 25
f = g.look_gradual(f, (3, 54, -9))
g.mine(f + 25, seconds=1.4)
f += 25 + 84 + 25
# the stone's x8 (the 0.56-s + the 0.44-s's margin — the 1-m's leg's
#    walk per cell, the 1-m's face aim's belt)
for x in (4, 5, 6, 7, 8, 9, 10, 11):
    f = g.walk_polyline(f, [(float(x) + 0.5, -9.59)])  # the model's landing's z
    f = g.look_gradual(f, (x, 54, -9))
    g.mine(f + 25, seconds=1.0)
    f += 25 + 60 + 25
g.at(f + 120)
f += 120

# -- P10 the furnace's craft (the ring's 8's cobble — the h7's face) ----------
f = g.walk_polyline(f, [(0.5, -9.59), (0.5, -5.5), (5.5, -5.5)])
f = g.face_gradual(f, 0.0)
f += 20
g.open_ahead(f)                                # the table's (5,54,-7) open
f += 95
for c in ("g0", "g1", "g2", "g3", "g5", "g6", "g7", "g8"):
    g.panel(f, "inventory", "h6", "left")      # carry the cobblestone
    f += 25
    g.panel(f, "table", c, "right")            # the ring's place
    f += 25
    g.panel(f, "inventory", "h6", "left")      # the cobblestone's merge
    f += 30
g.panel(f, "table", "out", "left")             # craft the furnace
f += 25
g.panel(f, "inventory", "h6", "left")          # the furnace's place (h6)
f += 40
g.close_panel(f)
f += 100

# -- P11 the smelt (the repace V1: the sand's x1's single form — the
#    2 x 10-s's serial's 1380-f FROZEN = the user's 11-20 report "the
#    screen's still, the time's flows" root. The 10-s's smelt's tick is
#    mechanic-mandatory (unshrinkable); the FILL's face = no still
#    > 3-s: the third person's flame's pan + the mid-smelt's progress
#    watch (the furnace's open — the arrow's visible) + the collect at
#    the 10-s (the frame-locked's belt)) --------------------------------
f = g.walk_polyline(f, [(4.5, -5.44)])
f = g.face_gradual(f, 0.0)
g.select(f, 7)                                 # the h6's furnace
f += 40
g.place_ahead(f)                               # the furnace -> (4,54,-7)
f += 100
g.open_ahead(f)                                # the furnace's open
f += 95
g.panel(f, "inventory", "h1", "left")          # carry the sand x1
f += 25
g.panel(f, "furnace", "in", "left")            # the input's place
f += 40
g.panel(f, "inventory", "h3", "left")          # carry the sticks x8
f += 25
g.panel(f, "furnace", "fuel", "right")         # the fuel's x1
f += 25
g.panel(f, "furnace", "fuel", "right")         # the fuel's x2
f += 25
g.panel(f, "furnace", "fuel", "right")         # the fuel's x3 (the 15-s's
f += 30                                        # the 1.5x's belt's twin)
g.panel(f, "inventory", "h3", "left")          # the sticks' x5's merge back
f += 40
g.close_panel(f)                               # t0 — the smelt's start
f += 60
# the FILL (the repace V2 — the third person's C's ELIMINATION: the
# C key's THREE-CYCLE (the V2-1's back/front/first) — the 2-tap's
# front-third's the aim's sky's miss's root, the PITS 434's gate-
# catch). The first person's the FLAME's the pan: the +/-0.5-rad's
# the gradual's twin (the net-zero's return — the open_ahead's the
# facing's belt), the furnace's fire's the front's the glass's the
# visible
g.sweep_gradual(f, 150, 0.50, steps=50)
f += 150
g.sweep_gradual(f, 120, -0.50, steps=50)
f += 120
g.open_ahead(f)                                # the progress's the mid-watch
f += 95 + 100
g.close_panel(f)                               # the glass's the ready (t0+735)
f += 60
g.open_ahead(f)
f += 95
g.panel(f, "furnace", "out", "left")           # the glass x1's take
f += 25
g.panel(f, "inventory", "h8", "left")          # the glass's place (h8)
f += 40
g.close_panel(f)
f += 120

# -- P12 the ending (the third person's tour — the repace V1's cut: the
#    900-f's still -> the 180-f's belt, the tail's freeze's elimination)
g.select(f, 5)                                 # the h4's pickaxe
f += 40
g.tap(f + 20, "c")                             # the third person (the ONE-tap's the back-third's — the ending's no-return)
f += 60
g.sweep_gradual(f, 150, 0.70, steps=50)
f += 150 + 40
g.sweep_gradual(f, 120, -0.70, steps=50)
f += 120 + 60
g.at(f)                                        # the tail's 60-f (the finished panel's the takeover)

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_mining_timeline.json"))
