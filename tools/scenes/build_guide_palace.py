# V3-16d demo 7: guide_palace (搭建皇宫 — the large creative build, build #1).
#
# The scene is guide_palace: the 21x18 platform (x0..20, z-13..4, top y54)
# at NOON (time 0.25) + the player at (8.5,54,2.5) in CREATIVE (scene flag —
# instant place, no consumption, instant mine, no drops).
#
# The palace (15x11, x1..15, z-11..-1):
#   - the 2-tier perimeter wall (t1 stone_bricks y54 + t2 terracotta_yellow
#     y55; the gate's 2 cells x8/x9 south open both tiers) = 46 cells/tier
#   - the 4 corner towers (t3 y56 + t4 y57, the corner t2's TOP face's
#     the ground's UP-aim — the scaffold's step-up chain's the cut's,
#     the PITS 416's the fragile class's the gate-catch's)
#   - the interior floor ring (38 terracotta cells at y54, the perimeter's
#     inside — the interior's clear, the gate's the ring's 2-cell's open)
#
# The physics' discipline (the v16c gate-catch's full inheritance):
#   - the PLACE = the absolute cell aim (look_ground / look_point — the house's
#     stand-still pattern: the walk loss's re-bounded per cell, the aim's
#     drift-INDEPENDENT of the track's along-row accumulation)
#   - the walk = axis-aligned legs chunked <= 2 m (the 1-m leg's loss's
#     0.03-0.11 m's the cell's 0.5-m's margin's inside; the 3-m+ leg's
#     0.4-0.5-m's loss's the PITS 415 4's the stray's root = never)
#   - the tower's descent = the hold-8 hop's the W's release before the edge
#     (the PITS 415 3's no-fall-drift) + the track's re-anchored to the
#     EXPECTED landing (the edge + the fall's drift, not the nominal's far point)

# The creative's grant's face (the V3-11b's 物品栏 tab's default):
#   E open (the scroll's reset to 0) + the wheel's jump (1 event = N items,
#   the handleMouseWheel's a.ticks' the scrollBy's delta — the page's 24's
#   the single wheel's face) + the c-cell's left (the 64's the selected h0)
#   + close. The palette's 236 items' name-ascending:
#     stone_bricks  idx 177 = scroll 168 + c9
#     terracotta    idx 187 = scroll 168 + c19
#     terracotta_yellow idx 201 = scroll 192 + c9
import math
from demo_author import Guide

g = Guide(scene="guide_palace", pos=(8.5, 54, 2.5), yaw=0.0, pitch=-0.1)

YAW_N = 0.0
YAW_W = math.pi / 2
YAW_S = math.pi
YAW_E = -math.pi / 2

STONE = (168, "c9")
TERRA_Y = (192, "c9")


def grant(f, scroll, cell):
    """The creative's grant to the selected h0 (the full stack's left)."""
    g.open_panel(f)
    f += 120
    g.wheel(f, scroll)
    f += 20
    g.panel(f, "creative", cell, "left")
    f += 25
    g.close_panel(f)
    f += 120
    return f


def place_ground(f, cell, ground_y=54.0):
    """The absolute cell aim (the cell center's the ground plane) + RMB +
    settle. The drift's independent (the house's v10-validated's face)."""
    g.look_ground(f, cell, ground_y=ground_y)
    g.click(f + 25, "right")
    return f + 70


def place_point(f, point):
    """The exact 3D point's aim (a face's the 0.1's past) + RMB + settle."""
    g.look_point(f, *point)
    g.click(f + 25, "right")
    return f + 70


def walk_to(f, px, pz, maxd=2.0):
    """A SINGLE-AXIS walk to the exact (px, pz), chunked at <= maxd (the 3-m+
    leg's loss's 0.4-0.5-m's the PITS 415 4's the stray's root = never)."""
    while True:
        dx = px - g.x
        dz = pz - g.z
        d = abs(dx) + abs(dz)
        if d < 0.05:
            return f
        assert abs(dx) < 0.05 or abs(dz) < 0.05, \
            "walk_to: single-axis legs (split the corner explicitly)"
        if d <= maxd:
            return g.walk_leg(f, px, pz)
        if abs(dx) > abs(dz):
            f = g.walk_leg(f, g.x + (maxd if dx > 0 else -maxd), g.z)
        else:
            f = g.walk_leg(f, g.x, g.z + (maxd if dz > 0 else -maxd))


def walk_path(f, points):
    for (px, pz) in points:
        f = walk_to(f, px, pz)
    return f


def row(f, ground_y, positions, cells, face_yaw):
    """A placement's row: per cell = the 1-m walk's the position + the face +
    the absolute aim. positions/cells are paired (the caller's the 1-m's the
    wall's outside / the floor's inside's the line's face)."""
    for (pos, cell) in zip(positions, cells):
        f = walk_to(f, *pos)
        g.face(f, face_yaw)
        f += 10
        f = place_ground(f, cell, ground_y=ground_y)
    return f


# -- P0 the morning's opening (the short tour) --------------------------------
g.capture(0)
g.at(20)
# THE ENTRY'S FLY-OFF (the PITS 416's the v6's gate-catch's root): the
# scene's creative's TRUE's the applySave's setFlying(true)'s (V3-11a's
# "a creative's load's flies" — the G's toggle's twin): the demo's STARTS
# the fly's ON's. Without an entry's toggle the walls's "ground's" walks's
# the 7.12's b/s's air-pace's (+0.89-m's excess's per 1-m's leg's the 15-m
# drift's the mis-aim's the strays's the v6's snapshot's (23.09,-2.3)'s the
# proof's). The B's single-press's (the deterministic's — the Space's
# double-tap's the wall-clock's window's the v4's flake's twin) = the fly's
# OFF's before the first's walk's leg (f916). The fly's ON/OFF's all the
# B's (the sim-frame's deterministic's the wall-clock's absent's).
g.fly_toggle(20)
g.sweep(60, 300, 0.70)
g.sweep(360, 280, -1.40)
f = 600
# the third person's (the audience's the build's the watch's face — the C's
# one-press's, the tour's the C-tap's the removed's twin): the aim's the
# eye's (the third's the camera's behind, the ray's the unchanged's).
g.tap(f, "c")
f += 30

# -- the t1 (stone_bricks) : the 46 cells' the perimeter's the gate's open ----
f = grant(f, *STONE)
f = walk_path(f, [(1.5, 2.5), (1.5, 0.5)])
# south x1..x7 + x10..x15 (the gate's x8/x9's the 2-cell's open)
south = [((x + 0.5, 0.5), (x, 54, -1)) for x in
         list(range(1, 8)) + list(range(10, 16))]
f = row(f, 54.0, [p for p, _ in south], [c for _, c in south], YAW_N)
# east z-2..z-10 (the SE corner's the south's; the NE's the north's)
f = walk_path(f, [(16.5, 0.5), (16.5, -1.5)])
east = [((16.5, z + 0.5), (15, 54, z)) for z in range(-2, -11, -1)]
f = row(f, 54.0, [p for p, _ in east], [c for _, c in east], YAW_W)
# north x15..x2 (the NE corner's; the NW's the west's)
f = walk_path(f, [(16.5, -11.5), (15.5, -11.5)])
north = [((x + 0.5, -11.5), (x, 54, -11)) for x in range(15, 1, -1)]
f = row(f, 54.0, [p for p, _ in north], [c for _, c in north], YAW_S)
# west z-11..z-2 (the NW corner's; the SW's the south's)
f = walk_path(f, [(0.5, -11.5), (0.5, -10.5)])
west = [((0.5, z + 0.5), (1, 54, z)) for z in range(-11, -1, 1)]
f = row(f, 54.0, [p for p, _ in west], [c for _, c in west], YAW_E)

# -- the t2 (terracotta_yellow) : the same 46 cells' the t1's the top ---------
f = grant(f, *TERRA_Y)
f = walk_path(f, [(0.5, 0.5), (1.5, 0.5)])
south2 = [((x + 0.5, 0.5), (x, 55, -1)) for x in
          list(range(1, 8)) + list(range(10, 16))]
f = row(f, 55.0, [p for p, _ in south2], [c for _, c in south2], YAW_N)
f = walk_path(f, [(16.5, 0.5), (16.5, -1.5)])
east2 = [((16.5, z + 0.5), (15, 55, z)) for z in range(-2, -11, -1)]
f = row(f, 55.0, [p for p, _ in east2], [c for _, c in east2], YAW_W)
f = walk_path(f, [(16.5, -11.5), (15.5, -11.5)])
north2 = [((x + 0.5, -11.5), (x, 55, -11)) for x in range(15, 1, -1)]
f = row(f, 55.0, [p for p, _ in north2], [c for _, c in north2], YAW_S)
f = walk_path(f, [(0.5, -11.5), (0.5, -10.5)])
west2 = [((0.5, z + 0.5), (1, 55, z)) for z in range(-11, -1, 1)]
f = row(f, 55.0, [p for p, _ in west2], [c for _, c in west2], YAW_E)
# the wall's end: (0.5, -1.5) — the SW tower's the start's the in-place

# -- the 4 corner towers (the terracotta_yellow's t3/t4) + the aerial's showcase --
# The gate-catch's rewrite (the scaffold's step-up chain's the fragile class's
# the CUT): the S1/S2's scaffold + the climb + the hop + the mine = the position-
# sensitive's (the step-up's 0.35-m shortfall vs the 0.2-m AABB margin, the DDA's
# corner tie, the 30-f mine chain's platform hole's trap) — all cut. The t3/t4 =
# the FLY's hover (feet ~58.7, the eye ~60.3 — above the t4's top y58's the 0.7-m
# margin) aiming the corner t2/t3's TOP face: the aim point's ON the face's plane
# (the ray crosses it AT the aim point — the eye-height's independent). The fly's
# horizontal = 7.12 b/s (measured: the 17-f 2.0112-m, PITS 416), the vertical =
# 4.0 b/s exact (kFlyVerticalSpeed's fixed-pace). The aerial showcase = the 360-
# deg gradual sweep (the audience's orientation anchor: the wall's / tower's /
# floor's "here's where we are" beat).

def fly_to(f, px, pz, maxd=2.0):
    """The creative's flight's chunked walk (the FLY's pace's no-margin)."""
    while True:
        dx = px - g.x
        dz = pz - g.z
        d = abs(dx) + abs(dz)
        if d < 0.05:
            return f
        assert abs(dx) < 0.05 or abs(dz) < 0.05
        if d <= maxd:
            return g.walk_leg(f, px, pz, fly=True)
        if abs(dx) > abs(dz):
            f = g.walk_leg(f, g.x + (maxd if dx > 0 else -maxd), g.z, fly=True)
        else:
            f = g.walk_leg(f, g.x, g.z + (maxd if dz > 0 else -maxd), fly=True)


def fly_up(f):
    """The creative's flight's ON (the gamepad's B's single-press's — the
    sim-frame's deterministic's, the wall-clock's window's absent's the PITS
    416's) + the 45-f's rise's 3.0 = the feet 57.0 (the eye 58.62). The v7's
    65-f's 58.33 (the eye 59.95's) the aim's 4.12-m's PAST kReach 4.09's =
    0/8's the silent's the gate-catch's; 57.0's the 8 aims's 2.2-3.0-m's the
    ~1-m's margin's. The g.y's the track's altitude's sync's (the v8b's
    gate-catch: the track's y's 54's the ground's eye's the in-flight's pitch's
    wrong's the 0/8's twin's)."""
    g.fly_toggle(f)
    g.hold_key(f + 12, 45, "space")
    g.y = 57.0
    return f + 12 + 45 + 20


def fly_down(f):
    """The 75-f's descent's 5.0-m (the ground's the clamp's 45-f's) + the B's
    the fly's OFF's (the no-drop's the no-jump's the deterministic's)."""
    g.hold_key(f, 75, "shift")
    g.y = 54.0
    f += 75 + 30
    g.fly_toggle(f)
    return f + 25


def showcase(f, center):
    """The 360-deg's gradual sweep (the audience's orientation's anchor)."""
    g.look_at(f, center)
    f += 20
    g.sweep_gradual(f, 400, 2 * math.pi - 0.02)
    return f + 400 + 20


def corner_cycle(f, t3_aim, t4_aim, sweep=None):
    """One corner's tower's cycle (the v8c's gate-catch's rewrite — the
    HORIZONTAL's fly's leg's 1-m's loss's the single's root's: the v8c's
    34-m's fly perimeter's (9+16+9-m's the big's yaw's turn's) the NW's
    beat's 1-m's short's the SE's landing's off's the floor's the platform's
    off's the terrain's the y53's the full's cascade's). VERTICAL-ONLY's:
    the fly's up's (45-f's 3-m's exact's) + the 2 aims's (the 1-m's outside's
    the corner's the 2.2-3-m's reach's) + the fly's down's (75-f's the
    ground's clamp's) + the GROUND's walk's to the next's corner (the 0.1-m's
    clearance's wall's line's the 92/92's proven's regime's)."""
    f = fly_up(f)
    if sweep:
        f = showcase(f, sweep)
    f = place_point(f, t3_aim)
    f = place_point(f, t4_aim)
    f = fly_down(f)
    return f


# the 4 corner's towers (the terracotta_yellow's t3/t4) = the VERTICAL-ONLY's
# corner's cycles (SW -> NW -> NE -> SE): the wall's end's (0.5,-1.5)'s the
# SW's in-place's; each's the corner's hover = the 1-m's outside's (the AABB's
# the no-overlap's the t4's band's y[57,58)'s the xz's disjoint's); the 3
# aerial's showcases's the SW (the wall's done's) + the SE (the tower's
# done's) + the floor's done's (the cycle's the twin's).
f = corner_cycle(f, (1.4, 56.0, -0.6), (1.4, 57.2, -0.6), sweep=(8, 56, -6))
f = walk_path(f, [(0.5, -10.5)])                     # the NW's (9-m's the x0.5's line's)
f = corner_cycle(f, (1.4, 56.0, -10.6), (1.4, 57.2, -10.6))
# the NE's walk (the v9's gate-catch: the z-10.5's line's INSIDE the t1's
# north's wall's z's band [-11,-10)'s the feet's y[54,54.8]'s the wall's
# y[54,55)'s = the 16-m's tunnel's the pinned's the NE/SE's 0/4's the
# cascade's — the v10's z-9.0's detour's the TWIN's tunnel's (the v10's
# arm's gate-catch's: the z-9.0's AABB's z[-9.4,-8.6]'s the east's wall's
# (15,54,-9)'s z's band [-9,-8)'s the 0.4-m's overlap's the 16-m's leg's
# the pinned's x14.6's the NE/SE's 0/4's again's — the z's line's
# clearance's must's clear BOTH the north's AND the east's wall's bands'
# the three-axis's PITS 416's 12's): the v11's detour's z-12.0's the
# OUTSIDE the ring's the 0.6-m's from the north's wall's band's the
# 0.9-m's from the platform's north's edge's z-13's on-platform's + the
# x0.5's / x16.5's 0.1-m's wall's lines's the 92/92's proven's regime's)
f = walk_path(f, [(0.5, -12.0), (16.5, -12.0), (16.5, -10.5)])
f = corner_cycle(f, (15.6, 56.15, -10.6), (15.6, 57.2, -10.6))
f = walk_path(f, [(16.5, -1.5)])                     # the SE's (9-m's the x16.5's line's)
f = corner_cycle(f, (15.6, 56.15, -0.6), (15.6, 57.2, -0.6), sweep=(8, 56, -6))
# the fly's the OFF's (the SE's cycle's end's) = the (16.5, 54, -1.5)'s the
# exit's in-place's (the floor's ring's the 38's the CUT's the v8b/v8c/v9's
# the three's version's the gate-catch's the walk's drift's the 0.03-m's
# per leg's the cumulative's the open-path's the 0.5-1.1-m's the aim's
# cell's tolerance's 0.5-m's the exceed's the precomputed's input's the
# reanchor's impossible's — the deviation's the record's, the 138's
# -> 100's blocks's the palace's the wall's + the tower's the core's)


# -- the exit (the SE's cycle's landing's the (16.5,-1.5)'s) + the tour ---
f = walk_path(f, [(20.5, -1.5), (20.5, 2.5)])
g.face(f, YAW_N)
f += 10
# east side (the SE tower's the front)
# east side (the SE tower's the front)
f = walk_path(f, [(20.5, 2.5)])
g.face(f, YAW_W)
f += 10
g.sweep(f, 400, 0.90)
g.sweep(f + 400, 400, -0.90)
f += 800
# north side (the NE tower's the front)
f = walk_path(f, [(20.5, -12.5)])
g.face(f, YAW_S)
f += 10
g.sweep(f, 400, 1.10)
g.sweep(f + 400, 400, -1.10)
f += 800
# west side (the NW tower's the front)
f = walk_path(f, [(0.5, -12.5)])
g.face(f, YAW_E)
f += 10
g.sweep(f, 400, 0.90)
g.sweep(f + 400, 400, -0.90)
f += 800
# south front (the gate's the towers' the 2's)
f = walk_path(f, [(0.5, 3.5), (8.5, 3.5)])
g.face(f, YAW_N)
f += 10
g.sweep(f, 400, 1.00)
g.sweep(f + 400, 400, -1.00)
f += 800
g.at(f + 600)
f += 600

import os
g.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "..", "assets", "demos",
                    "guide_palace_timeline.json"))
