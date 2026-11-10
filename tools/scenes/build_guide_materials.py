# V3-16e demo 8: guide_materials (收集各种材料 — the multi-biome materials
# garden tour, build #3).
#
# The scene is guide_materials: the 30x26 platform (x0..29, z-25..0, top
# y54) at NOON (time 0.25) + the player at (15.5,54,-1.5) in SURVIVAL
# (the collection's face — real break times, the drops' magnet pickup)
# with a PREPARED wooden pickaxe in h1 (the ore's face — the pilot's
# prepared-tool's twin). The 7 biome beds (the flush surface patches +
# the specimens — the worldgen's biomes' content's the garden's):
#   B1 forest (x2..11, z-23..-15): grass + 3 oak (4,-19) (7,-17) (10,-19)
#   B2 desert (x14..23, z-23..-15): sand + cactus (16,-21) (21,-17)
#   B6 wool   (x25..28, z-23..-15): grass + wool (25/26/27,-18) +
#                                    glowstone (26/27,-15)
#   B3 taiga  (x2..11, z-10..-3):   snow + 2 spruce (5,-7) (9,-5) +
#                                    snow bumps (3,-10) (9,-10) (3,-3)
#                                    (the v3.20's relocation's — the gate-226
#                                    root 6's (9,-8)'s the 0.2-m's line's
#                                    the graze's the pin's the root's)
#   B4 ore    (x14..23, z-10..-3):  the stone's outcrop (x16..19,
#                                    z-8..-6, y54..55) + coal (16/19,54,-6)
#                                    + iron (17,54,-6)
#   B5 flower (x25..28, z-10..-3):  grass + allium (25,-8) (27,-5) +
#                                    cornflower (26,-7) + tulip (28,-4)
#                                    (26,-3) (28,-8)
# The tour: P0 -> B5 -> B4 -> B6 -> B2 -> B1 -> B3 -> the center's
# E-panel's showcase -> the 3rd person's 4-side's tour.
#
# The v3.20's rules (the gate-226's root 1-7's the distilled's):
#   - the walk's leg's class {1.0, 2.0} ONLY (the entry-aware's the
#     model's — the rest-entry's leg's the 0.19-m's the undershoot's the
#     root 7's the fix's the demo_author's face; the 0.5/1.5 = the
#     route's bug's the loud's assert's).
#   - the drop's the collection's: every drop's the <=1.0-m's the
#     proximity's the (the magnet's the ready's 0.5-s's the instant's
#     the pickup's — the 2.0-m's the v3.19's wood(10)'s the loss's the
#     root's): the stand's the <=1.0-m's OR the detour's leg's the
#     d=1.0's the pass's.
#   - the line's the clearance's: the 0.8-m's the preferred's the
#     (the intact's the y54/55's solid's the AABB's band's), the 0.2-m's
#     the flat-face's class's the accepted's (the B4's z-4.5's the
#     outcrop's the 12/12's the twin's — the entry-aware's the leg's
#     the +-0.03-m's the calibrated's the margin's the 0.17-m's the
#     structural's), the 0.6-m's class's the B3's z-4.5's the snow(3,-3)'s
#     the one's flagged's.
# The break's face (the aim's discipline): the aim = the target's FACE
# POINT (the side face's the player's side's the on, the top face's the
# center's the on). Every aim's the reach's the verified (the distance's
# the <= 2.6-m's the 4.09's the + the eye's steep's the miss's the
# avoid's).
# The break's time = the hardness's 1.5x + the 0.2-s's margin (the
# behind's crack's the deterministic's). The flower's hardness-0's =
# the 1-f's tap's (the PITS 414 3's the exactly-1's the twin).
import math
import json
import os
from demo_author import Guide

g = Guide(scene="guide_materials", pos=(15.5, 54, -1.5), yaw=0.0,
          pitch=-0.1)

YAW_N = 0.0
YAW_W = math.pi / 2
YAW_S = math.pi
YAW_E = -math.pi / 2

# -- the segment's manifest (the gate-226's the segmented's verify's face —
# the user's the 30-segment's the directive's): every boundary's a QUIET
# point (no hold's live's, no mine's live's), the prefix's dmo's replay's
# = the full's run's the frame-by-frame's the identical's (the
# determinism's) up to the boundary's.
SEGMENTS = []
BROKEN = []

DROPS = {
    (4, 54, -19): ("wood", 1), (7, 54, -17): ("wood", 1),
    (10, 54, -19): ("wood", 1),
    (3, 54, -15): ("dirt", 1), (8, 54, -15): ("dirt", 1),
    (16, 54, -21): ("cactus", 1), (21, 54, -17): ("cactus", 1),
    (16, 54, -19): ("sand", 1), (21, 54, -19): ("sand", 1),
    (19, 54, -16): ("sand", 1), (16, 54, -16): ("sand", 1),
    (16, 54, -6): ("coal", 1), (19, 54, -6): ("coal", 1),
    (17, 54, -6): ("iron_ingot", 1),
    (16, 55, -6): ("cobblestone", 1), (17, 55, -6): ("cobblestone", 1),
    (19, 55, -6): ("cobblestone", 1),
    (25, 54, -18): ("wool", 1), (26, 54, -18): ("wool", 1),
    (27, 54, -18): ("wool", 1),
    (26, 54, -15): ("glowstone", 3), (27, 54, -15): ("glowstone", 3),
    (25, 54, -8): ("allium", 1), (27, 54, -5): ("allium", 1),
    (26, 54, -7): ("cornflower", 1), (28, 54, -4): ("cornflower", 1),
    (26, 54, -3): ("tulip", 1), (28, 54, -8): ("tulip", 1),
    (5, 54, -7): ("spruce_trunk", 1), (9, 54, -5): ("spruce_trunk", 1),
    (3, 54, -10): ("snow", 1), (9, 54, -10): ("snow", 1),
    (3, 54, -3): ("snow", 1),
}


def seg_mark(name, x=None, z=None):
    """A segment's boundary (the QUIET's point's — the no-hold's the
    no-mine's the caller's discipline's): record the manifest's entry
    (the cumulative's broken's + the inventory's the derived's)."""
    inv = {}
    for (c, (item, n)) in DROPS.items():
        if c in BROKEN:
            inv[item] = inv.get(item, 0) + n
    inv["wooden_pickaxe"] = 1
    SEGMENTS.append({
        "name": name,
        "f": int(g.cursor),
        "pos": [x if x is not None else g.x,
                z if z is not None else g.z],
        "broken": [list(c) for c in BROKEN],
        "inv": inv,
    })


def walk_to(f, px, pz, maxd=2.0):
    # the v3.21's the walk_to (the vector's model's the root 8's the twin's):
    # the dominant's axis's the full's the EXACT's the solve's (the 2.0-m's
    # the chunks's), the sub-0.66-m's residual's the SNAP's (the 8-f's the
    # minimum's 0.66-m's the same-direction's the coast-entry's ~0.7-m/s's
    # the twin's — the 0.16-m's the micro's leg's the UNREACHABLE's the
    # assert's), the transverse's the >0.15-m's = the route's bug's the
    # loud's.
    for _ in range(40):
        dx = px - g.x
        dz = pz - g.z
        if abs(dx) >= abs(dz):
            R, C, ax = dx, dz, 'x'
        else:
            R, C, ax = dz, dx, 'z'
        if abs(R) < 0.66:
            assert abs(C) < 0.15, (
                f"walk_to: the transverse's {C:.2f}-m's the unreachable's "
                f"(the waypoint's the grid's the bug's) track=("
                f"{g.x:.3f},{g.z:.3f})")
            return f
        R = R if abs(R) < 2.7 else 2.0 * (1 if R > 0 else -1)
        if ax == 'x':
            f = g.walk_leg(f, g.x + R, g.z)
        else:
            f = g.walk_leg(f, g.x, g.z + R)
    raise AssertionError(
        f"walk_to: ({px},{pz})'s the bounded's the over's (the drift's "
        f"the route's bug's) track=({g.x:.3f},{g.z:.3f})")


def walk_path(f, points):
    for (px, pz) in points:
        f = walk_to(f, px, pz)
    return f


def break_at(f, point, seconds, cell):
    """The aim (the face point) + the hold-to-break. The settle = max(90,
    hold) + 10 — the gmat1's gate-catch: a 90-f settle under a >1.5-s
    hold overlapped the next mine's LMB-down into the live hold (the
    no-op down + the early up = the hold truncated, the 162-f stone's
    the 0/3, the coal's skipped beat — the LMB's state's the down/up
    re-entrant's the twin of the key's wasPressed's edge's)."""
    g.look_point(f, *point)
    f += 20
    hold = int(round(seconds * 60))
    g.mine(f, seconds=seconds)
    BROKEN.append(cell)
    return f + max(90, hold) + 10


def break_tap(f, point, cell):
    """The hardness-0's 1-f tap (the exactly-1 break's)."""
    g.look_point(f, *point)
    f += 20
    g.click(f, "left", hold=1)
    BROKEN.append(cell)
    return f + 70


def observe(f, yaw, dyaw=0.90):
    """The bed's observe sweep pair + the still."""
    g.face(f, yaw)
    f += 10
    g.sweep(f, 400, dyaw)
    g.sweep(f + 400, 400, -dyaw)
    f += 800
    g.at(f + 200)
    return f + 200


# -- P0 the noon's opening ----------------------------------------------------
g.capture(0)
g.sweep(20, 300, 0.70)
g.sweep(320, 280, -1.40)
f = 600

# -- the aim's discipline's helpers (the v3.4's gate-catch's the final's
# the fix's face): the JOG (the <=1.0-m's the last's leg's before the
# sensitive's aim's the position's re-pin's) + the pinned's stop's.
def jog(f, x1, z1, x2, z2):
    f = walk_to(f, x1, z1)
    return walk_to(f, x2, z2)


def stop_walk(f, px, pz):
    return walk_to(f, px, pz)


# -- B5 the flower's bed (the 6/6's the verified's the tap's the 1-f's) --------
f = walk_path(f, [(24.5, -1.5), (24.5, -7.5), (26.5, -7.5)])
f = break_tap(f, (25.5, 54.5, -7.5), (25, 54, -8))    # allium [1.0]
f = break_tap(f, (28.5, 54.5, -7.5), (28, 54, -8))    # tulip [2.0 the pass]
f = break_tap(f, (26.5, 54.5, -6.5), (26, 54, -7))    # cornflower [1.0]
f = walk_path(f, [(26.5, -4.5), (27.5, -4.5)])
f = break_tap(f, (27.5, 54.5, -4.5), (27, 54, -5))    # allium [0]
f = break_tap(f, (28.5, 54.5, -3.5), (28, 54, -4))    # cornflower [1.41]
f = break_tap(f, (26.5, 54.5, -2.5), (26, 54, -3))    # tulip [2.24 detour]
f = walk_path(f, [(27.5, -3.5), (26.5, -3.5)])
g.at(f + 60)         # the tulip(26,-3)'s + the cornflower(28,-4)'s the magnet's margin (the 1.41-m's the drift's face)
f += 60
f = walk_path(f, [(26.5, -6.5), (28.5, -6.5), (24.5, -6.5)])
f = walk_path(f, [(24.5, -4.5), (28.5, -4.5)])
f = observe(f, YAW_W)
seg_mark("s1_b5")

# -- B4 the ore's bed (the v3.17's the ore-first's the short-hold's the zero's
#    the over-dug's: the stone's the depth-1's the outcrop's the z-7/-8's
#    the row's the CUT's — the tail's the floor's the 2.2-%'s the partial's) --
f = walk_path(f, [(28.5, -1.5), (24.5, -1.5), (24.5, -4.5), (16.5, -4.5)])
g.select(f, 2)
f += 40
f = stop_walk(f, 16.5, -2.5)
f = stop_walk(f, 17.5, -2.5)
f = stop_walk(f, 17.5, -4.5)
f = break_at(f, (17.5, 54.7, -5.0), 1.25, (17, 54, -6))  # iron [1.86]
f = stop_walk(f, 17.5, -2.5)
f = stop_walk(f, 19.5, -2.5)
f = stop_walk(f, 19.5, -4.5)
f = break_at(f, (19.5, 54.7, -5.0), 1.25, (19, 54, -6))  # coal [1.86]
f = stop_walk(f, 19.5, -2.5)
f = stop_walk(f, 16.5, -2.5)
f = stop_walk(f, 16.5, -4.5)
f = break_at(f, (16.5, 54.7, -5.0), 1.25, (16, 54, -6))  # coal [1.86]
g.select(f, 1)
f += 40
f = break_at(f, (16.5, 55.4, -5.0), 2.3, (16, 55, -6))   # stone [1.33]
f = stop_walk(f, 16.5, -2.5)
f = stop_walk(f, 17.5, -2.5)
f = stop_walk(f, 17.5, -4.5)
f = break_at(f, (17.5, 55.4, -5.0), 2.3, (17, 55, -6))   # stone [1.33]
f = stop_walk(f, 17.5, -2.5)
f = stop_walk(f, 19.5, -2.5)
f = stop_walk(f, 19.5, -4.5)
f = break_at(f, (19.5, 55.4, -5.0), 2.3, (19, 55, -6))   # stone [1.33]
f = stop_walk(f, 19.5, -2.5)
f = walk_path(f, [(15.5, -2.5), (15.5, -4.5), (20.5, -4.5)])
f = observe(f, YAW_N)
seg_mark("s2_b4")

# -- B6 the wool's + the glowstone's bed (the wool's (26,-18)'s the drop's the
#    the exit's (26.5,-16.5)'s the pass's the 1.0-m's the pickup's face) -------
f = walk_path(f, [(20.5, -4.5), (23.5, -4.5), (23.5, -16.5), (24.5, -16.5)])
f = break_at(f, (25.0, 54.7, -17.5), 1.4, (25, 54, -18))  # wool -X [1.27]
f = break_at(f, (26.0, 54.7, -17.5), 1.4, (26, 54, -18))  # wool chain [1.27]
f = break_at(f, (27.0, 54.7, -17.5), 1.4, (27, 54, -18))  # wool chain [1.27]
f = walk_path(f, [(24.5, -12.5), (26.5, -12.5), (26.5, -13.5)])
f = break_at(f, (26.5, 54.7, -14.0), 0.6, (26, 54, -15))  # glowstone +Z [1.05]
f = walk_path(f, [(26.5, -12.5), (27.5, -12.5)])
f = break_at(f, (27.5, 54.7, -14.0), 0.6, (27, 54, -15))  # glowstone +Z [1.75]
f = walk_path(f, [(27.5, -16.5), (25.5, -16.5)])
f = observe(f, YAW_W)
seg_mark("s3_b6")

# -- B2 the desert's bed (the v3.20's route's: the cactus's (16,-21)'s the
#    the x14.5's column's the west's approach's the 1.2-m's clear's + the
#    the drop's the <=1.0-m's the detour's face) -------------------------------
f = walk_path(f, [(25.5, -12.5), (14.5, -12.5), (14.5, -20.5)])
f = break_at(f, (16.0, 54.5, -20.5), 0.75, (16, 54, -21))  # cactus -X [1.86]
f = walk_path(f, [(15.5, -20.5)])      # the drop's d=1.0's the pickup's frame
f = break_at(f, (16.0, 54.5, -18.5), 0.85, (16, 54, -19))  # sand -X [2.34]
f = walk_path(f, [(16.5, -20.5), (16.5, -18.5), (15.5, -18.5), (15.5, -14.5), (16.5, -14.5)])
f = break_at(f, (16.5, 54.5, -15.2), 0.85, (16, 54, -16))  # sand +Z [1.33]
f = walk_path(f, [(16.5, -15.5)])          # the drop's d=0's
f = walk_path(f, [(16.5, -14.5)])
seg_mark("s4_b2a")
f = walk_path(f, [(19.5, -14.5)])
f = break_at(f, (19.5, 54.5, -15.2), 0.85, (19, 54, -16))  # sand +Z [1.33]
f = walk_path(f, [(19.5, -15.5)])          # the drop's d=0's
f = walk_path(f, [(19.5, -18.5), (20.5, -18.5)])
f = break_at(f, (21.0, 54.5, -18.8), 0.85, (21, 54, -19))  # sand -X [1.28]
f = walk_path(f, [(21.5, -18.5)])          # the sand(21)'s drop's d=0's
f = break_at(f, (21.0, 54.5, -16.5), 0.75, (21, 54, -17))  # cactus -X [2.34]
f = walk_path(f, [(21.5, -17.5), (21.5, -16.5)])  # the cactus(21)'s drop's d=0's
f = walk_path(f, [(21.5, -14.5), (19.5, -14.5), (19.5, -12.5)])
f = observe(f, YAW_S)
seg_mark("s5_b2")

# -- B1 the forest's bed (the v3.20's route's: the x2.5's column's the west's
#    the approach's the trunk's clear's + the wood's (7,-17)'s the (7.5,-15.5)'s
#    the stand's the 1.0-m's the drop's fix's the v3.19's 3.0-m's the loss's +
#    the wood's (10,-19)'s the (10.5,-17.5)'s the detour's the 1.0-m's) --------
f = walk_path(f, [(19.5, -12.5), (3.5, -12.5)])
f = walk_path(f, [(3.5, -13.5)])
f = break_at(f, (3.5, 54.5, -14.2), 1.0, (3, 54, -15))     # grass +Z [1.0]
f = walk_path(f, [(3.5, -12.5), (2.5, -12.5), (2.5, -17.5)])
f = break_at(f, (4.5, 54.5, -18.0), 3.1, (4, 54, -19))     # wood +Z [1.23]
f = walk_path(f, [(4.5, -17.5)])       # the drop's d=1.0's (the tier's block's the d=0's)
g.at(f + 60)          # the magnet's margin's the dwell's
f += 60
f = walk_path(f, [(4.5, -15.5), (7.5, -15.5)])
f = break_at(f, (7.5, 54.7, -16.0), 3.1, (7, 54, -17))     # wood +Z [1.05]
g.at(f + 60)          # the drop's d=1.0's the magnet's margin (the tier's block's the d=0's)
f += 60
seg_mark("s6_b1a")
f = walk_path(f, [(7.5, -13.5), (8.5, -13.5)])
f = break_at(f, (8.5, 54.5, -14.2), 1.0, (8, 54, -15))     # grass +Z [1.0]
f = walk_path(f, [(10.5, -13.5), (10.5, -16.5)])
f = break_at(f, (10.0, 54.5, -18.5), 3.1, (10, 54, -19))  # wood -X [1.86]
f = walk_path(f, [(10.5, -17.5)])      # the drop's d=1.0's (the tier's block's the d=0's)
g.at(f + 60)          # the magnet's margin's the dwell's
f += 60
f = walk_path(f, [(10.5, -16.5), (12.5, -16.5)])
f = observe(f, YAW_E)
seg_mark("s7_b1")

# -- B3 the taiga's bed (the v3.20's route's: the z-13.5's line's the 2.8-m's
#    the clear's the entry's + the snow's pair's the z-10's the (3.5/9.5,-11.5)'s
#    the stand's the 0.8-m's + the drop's the <=1.0-m's the detour's face) -----
f = walk_path(f, [(12.5, -13.5), (9.5, -13.5), (9.5, -11.5)])
f = break_at(f, (9.5, 54.5, -9.2), 0.25, (9, 54, -10))     # snow +Z [2.55]
f = walk_path(f, [(9.5, -9.5)])        # the drop's d=0's
f = walk_path(f, [(9.5, -13.5), (3.5, -13.5), (3.5, -11.5)])
f = break_at(f, (3.5, 54.5, -9.2), 0.25, (3, 54, -10))     # snow +Z [2.55]
f = walk_path(f, [(3.5, -9.5)])        # the drop's d=0's
f = walk_path(f, [(3.5, -13.5)])
seg_mark("s8_b3a")
f = walk_path(f, [(1.5, -13.5), (1.5, -6.5), (1.5, -4.5), (5.5, -4.5)])
f = break_at(f, (5.5, 54.7, -6.0), 3.1, (5, 54, -7))       # spruce +Z [1.75]
f = walk_path(f, [(5.5, -5.5)])       # the drop's d=1.0's (the tier's block's the d=0's)
g.at(f + 60)          # the magnet's margin's the dwell's
f += 60
f = walk_path(f, [(5.5, -4.5), (7.5, -4.5)])
f = break_at(f, (9.0, 54.5, -4.5), 3.1, (9, 54, -5))       # spruce -X [1.86]
f = walk_path(f, [(7.5, -3.5), (9.5, -3.5)])
g.at(f + 150)        # the spruce(9)'s drop's the ready's margin (the d=1.0's the still's)
f += 150
seg_mark("s9_b3b")
f = walk_path(f, [(11.5, -3.5), (11.5, -1.5)])
f = walk_path(f, [(3.5, -1.5)])
f = break_at(f, (3.5, 54.5, -2.2), 0.25, (3, 54, -3))      # snow +Z [1.33]
g.at(f + 30)          # the drop's d=1.0's the magnet's margin (the 12-m's the exit's before)
f += 30
f = walk_path(f, [(15.5, -1.5)])
f = observe(f, YAW_E)

# -- the E-panel's showcase (the B3's exit's the z-1.5's line's the showcase's
# the walk's twin's) ----------------------------------------------------------------
g.open_panel(f)
f += 120
g.at(f + 900)
f += 900
g.close_panel(f)
f += 120
seg_mark("s10_show")

# -- the 3rd person's 4-side's tour ------------------------------------------------
g.tap(f, "c")
f += 30
f = walk_path(f, [(24.5, -1.5), (24.5, -23.5)])
f = observe(f, YAW_W, 1.10)
f = walk_path(f, [(1.5, -23.5)])
f = observe(f, YAW_S, 1.10)
f = walk_path(f, [(1.5, -1.5)])
f = observe(f, YAW_E, 1.10)
f = walk_path(f, [(15.5, -1.5)])
f = observe(f, YAW_N, 1.10)
g.at(f + 600)
f += 600
seg_mark("s11_full")

here = os.path.dirname(os.path.abspath(__file__))
tl = g.dump(os.path.join(here, "..", "..", "assets", "demos",
                         "guide_materials_timeline.json"))
json.dump({"segments": SEGMENTS},
          open(os.path.join(here, "guide_materials_segments.json"), "w"),
          indent=1)
print(f"segments: {len(SEGMENTS)} (the user's 30's the cap's) | "
      f"broken total: {len(BROKEN)}")
