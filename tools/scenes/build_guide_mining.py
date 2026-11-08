# V3-16a pilot: guide_mining (the ④ 挖矿打造工具 — day1's first demo).
#
# The scene = pilot_mine: the 12x12 platform (x0-11, z-11..1, top y54,
# solid y50-53), the tree (the trunk's 6 at (8, 54..59, -8) + the crown's
# leaves y58-60), the mining row at z=-9 (x1-2 coal / x3 iron / x4-11
# stone — the x0's gap is the route's crossing), the sand's pillar at
# (8, 54-55, -5) (the furnace's smeltable — the pilot's 熔炼 face).
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

# -- P0 the opening's observe (the scene's tour) ----------------------------
g.sweep(20, 150, 0.50)
g.sweep(170, 150, -0.90)
g.sweep(320, 120, 0.40)
g.sweep(440, 150, -0.30)
g.at(700)

# -- P1 the approach (the tree's west face) ----------------------------------
f = g.walk_polyline(710, [(6.5, -2.0), (6.5, -7.5)])
g.at(f + 60)
f += 60

# -- P2 the chop (the bare-hand's wood x5 — the 3.3-s's hold's face) ---------
for y in (54, 55, 56):
    g.look_at(f, (8, y, -8))
    g.mine(f + 25, seconds=3.3)
    f += 25 + 198 + 30
f = g.walk_polyline(f, [(7.5, -7.5)])
for y in (57, 58):
    g.look_at(f, (8, y, -8))
    g.mine(f + 25, seconds=3.3)
    f += 25 + 198 + 30
# (the y59's stump's top stays — the treetop's visual's face)

# -- P2.5 the sand's pillar (the furnace's smeltable — the bare-hand's
#    0.75-s's face: the pickaxe's not crafted yet, the h0's wood's twin) --
g.look_at(f, (8, 54, -5))
g.mine(f + 25, seconds=1.0)
f += 25 + 60 + 25
g.look_at(f, (8, 55, -5))
g.mine(f + 25, seconds=1.0)
f += 25 + 60 + 25
# the collect's step (the sand's 1.4-m's belt + the wood's twin)
f = g.walk_polyline(f, [(7.5, -5.5)])
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
f = g.walk_polyline(f, [(5.5, -5.5)])
g.face(f, 0.0)                                 # the -Z's facing
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
f = g.walk_polyline(f, [(0.5, -5.5), (0.5, -9.5), (2.5, -9.5)])
# the west: the coal x2 (the 1.125-s + the 0.275-s's margin) + the iron
for x in (2, 1):
    g.look_at(f, (x, 54, -9))
    g.mine(f + 25, seconds=1.4)
    f += 25 + 84 + 25
g.look_at(f, (3, 54, -9))
g.mine(f + 25, seconds=1.4)
f += 25 + 84 + 25
# the stone's x8 (the 0.56-s + the 0.44-s's margin — the 1-m's leg's
#    walk per cell, the 1-m's face aim's belt)
for x in (4, 5, 6, 7, 8, 9, 10, 11):
    f = g.walk_polyline(f, [(float(x) + 0.5, -9.5)])
    g.look_at(f, (x, 54, -9))
    g.mine(f + 25, seconds=1.0)
    f += 25 + 60 + 25
g.at(f + 120)
f += 120

# -- P10 the furnace's craft (the ring's 8's cobble — the h7's face) ----------
f = g.walk_polyline(f, [(0.5, -9.5), (0.5, -5.5), (5.5, -5.5)])
g.face(f, 0.0)
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

# -- P11 the smelt (the sand x2 -> the glass x2, the sticks x6's fuel) --------
f = g.walk_polyline(f, [(4.5, -5.5)])
g.face(f, 0.0)
g.select(f, 7)                                 # the h6's furnace
f += 40
g.place_ahead(f)                               # the furnace -> (4,54,-7)
f += 100
g.open_ahead(f)                                # the furnace's open
f += 95
g.panel(f, "inventory", "h1", "left")          # carry the sand x2
f += 25
g.panel(f, "furnace", "in", "left")            # the input's place
f += 40
g.panel(f, "inventory", "h3", "left")          # carry the sticks x6
f += 25
g.panel(f, "furnace", "fuel", "left")          # the fuel's place
f += 40
g.close_panel(f)
f += 1380                                      # the 2 x 10-s + the 3-s
                                               # (the progress's face)
g.open_ahead(f)
f += 95
g.panel(f, "furnace", "out", "left")           # the glass x2's take
f += 25
g.panel(f, "inventory", "h8", "left")          # the glass's place (h8)
f += 40
g.close_panel(f)
f += 120

# -- P12 the ending (the third person's tour — the held pickaxe's face) -------
g.select(f, 5)                                 # the h4's pickaxe
f += 40
g.tap(f + 20, "c")                             # the third person
g.sweep(f + 80, 200, 0.70)
g.sweep(f + 280, 200, -1.40)
g.sweep(f + 480, 200, 0.70)
g.at(f + 900)                                  # the still's end

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_mining_timeline.json"))
