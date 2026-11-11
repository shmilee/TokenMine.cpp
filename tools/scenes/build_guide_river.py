# V3-16f demo 9: guide_river (挖出河流 — the river's the dig + the
# bucket's the water's the bring + the flow's the watch + the water-fire's
# the contrast + the swim's the crossing, build #2).
#
# The scene is guide_river: the 24x15 platform (x0..23, z-15..-1, top
# y54, the dirt's surface) at NOON + the player at (1.5,54,-4.5) in
# SURVIVAL with the PREPARED bucket h0 + the torch h1 + the sealed 1-deep
# source pool (x18..21, z-13..-10, 16 sources) + the campfire at
# (10,53,-7) LIT (the trench's center, the dig's skip cell).
#
# The tour: P0 -> the pool's the view -> the dig's 15 (the x2's top-face
# from the bank + the x3..x9's the inside + the x10's pass-over + the
# x11..x17's) -> the scoop's 1 -> the pour's W -> the watch's the
# gradient + the campfire's out -> the scoop's 2 -> the pour's E -> the
# watch's E -> the torch's the place (the contrast's beat) -> the swim's
# crossing -> the E-panel -> the 3rd-person's 4-line's patrol -> ending.
#
# The probes' pins (the /tmp/opencode/g227's the probe A/B's the
# verified's):
#   - the trench's steady = [8,7,6,5,4,3,2,1, campfire_ext, 2,3,4,5,6,7,8]
#   - the campfire's out = the pour W's frame + ((3-F%3)%3) + 19
#   - the pool's the scoop's dent's = the level-7's self-heal (1-deep)
#   - the swim's holds = 15/252/space25/30 (the landing (0.75,54,-6.19))
#   - the pour's aims = (2.5,53.05,-6.2) / (17.5,53.05,-6.8)
#   - the scoop's aim = (18.5,53.5,-11.0) (the stand (17.5,54,-11.5))
#
# The walk's routes = the DRY's ONLY (the trench's row's z-7's the
# water-surface's the walk's the avoid — the feet's 54's = the level-8's
# the top's the strict-boundary's the no-overlap's, the 1-deep's the
# shallow's the dry's walk's the legal's but the ugly's; the row's
# z-5.5 / z-9.0's the bank's the, the column's x17.5 / x18.5's the).
import math
import json
import os
from demo_author import Guide

g = Guide(scene="guide_river", pos=(1.5, 54, -4.5), yaw=0.0, pitch=-0.1)

YAW_N = 0.0
YAW_W = math.pi / 2
YAW_S = math.pi
YAW_E = -math.pi / 2

SEGMENTS = []
BROKEN = []
DROPS = {(x, 53, -7): ("dirt", 1) for x in
         list(range(2, 10)) + list(range(11, 18))}
CAMPAIGN_OUT = None


def seg_mark(name, out_frame=None):
    inv = {}
    for (c, (item, n)) in DROPS.items():
        if c in BROKEN:
            inv[item] = inv.get(item, 0) + n
    inv["bucket"] = 1
    SEGMENTS.append({"name": name, "f": int(g.cursor),
                     "pos": [g.x, g.z], "broken": [list(c) for c in BROKEN],
                     "inv": inv})
    if out_frame is not None:
        SEGMENTS[-1]["campfire_out"] = int(out_frame)


def walk_to(f, px, pz, maxd=2.0):
    for _ in range(40):
        dx = px - g.x
        dz = pz - g.z
        if abs(dx) >= abs(dz):
            R, C, ax = dx, dz, 'x'
        else:
            R, C, ax = dz, dx, 'z'
        if abs(R) < 0.66:
            assert abs(C) < 0.15, (
                f"walk_to: transverse {C:.2f}m track=({g.x:.3f},{g.z:.3f})")
            return f
        R = R if abs(R) < 2.7 else 2.0 * (1 if R > 0 else -1)
        f = g.walk_leg(f, g.x + R, g.z) if ax == 'x' \
            else g.walk_leg(f, g.x, g.z + R)
    raise AssertionError("walk_to: unbounded")


def dig_dirt(f, point, cell):
    """The dirt's 0.75-s's hold-to-break (the aim = the face's point).
    The settle = 90 + 10 (the break's done's + the drop's the spawn's
    + the next's walk's the margin)."""
    g.look_point(f, *point)
    f += 20
    g.mine(f, seconds=0.75)
    BROKEN.append(cell)
    return f + 90 + 10


def sweep_still(f, dyaw=0.90, dwell=200):
    g.sweep(f, 400, dyaw)
    g.sweep(f + 400, 400, -dyaw)
    g.at(f + 800 + dwell)
    return f + 800 + dwell


# -- P0 the opening (the 1000-f's the sweep's pair) ----------------------
g.capture(0)
g.sweep(20, 450, 1.10)
g.sweep(470, 380, -1.95)
g.at(1400)
f = 1400
seg_mark("s1_p0")

# -- the pool's the view (the approach's the ROW_M's z-8.5's the dry's
#    line's — the pool's the x18..21's z-13..-10's the, the row's
#    z-8.5's the pool's the south's the clear's) ------------------------
f = walk_to(f, 1.5, -8.5)
f = walk_to(f, 17.5, -8.5)
g.look_point(f, 19.5, 53.5, -11.5)
f += 15
f = sweep_still(f, 0.75, 600)
seg_mark("s2_pool")

# -- the dig's start (the W-bank's stand (1.5,-6.3)'s the) --------------
#    (the pre-dug's the: the trench's the row's z-7's the dirt's the
#    intact's — any's route's the legal's)
f = walk_to(f, 1.5, -5.5)
f = walk_to(f, 1.5, -6.3)
seg_mark("s3_digstart")

# -- the dig's x2 (the top-face's from the bank's the) ------------------
f = dig_dirt(f, (2.5, 54.02, -6.4), (2, 53, -7))
# the step-in's (the bank's 54's -> the trench's 53's, the 1-m's pure-x's
# leg's + the drop's — the 16c's the W-drift's margin's the hold's 30)
f = g.walk_leg(f, 2.5, g.z, y=53, hold=18)
f += 15
# -- the dig's x3..x9 (the inside's side-face's the, the 1-m's advance's
#    the collect's d=0's the magnet's the)
for x in range(3, 10):
    f = dig_dirt(f, (float(x), 53.5, -6.5), (x, 53, -7))
    f = walk_to(f, x + 0.5, -6.5) + 10
seg_mark("s4_digw")
# -- the campfire's pass-over + the x11's the top-face's the (the
#    side-face's from the campfire's cell's = the ray's the campfire's
#    hit's — the steep's pitch's the y54's cross's x<11's the, the
#    any-position's the in-cell's the; the step-up's onto the x11's
#    dirt's + the top-face's dig's + the fall's back's into the x11's
#    (the g.y's 53's the honest's track's)) -----------------------------
f = walk_to(f, 10.5, -6.5) + 10
f = g.walk_leg(f, 11.5, g.z, y=54, hold=20)
f += 15
f = dig_dirt(f, (11.5, 54.02, -6.4), (11, 53, -7))
g.y = 53
# -- the dig's x12..x17 (the inside's side-face's, the x11's = air's the
#    own-cell's the safe's) ----------------------------------------------
for x in range(12, 18):
    f = dig_dirt(f, (float(x), 53.5, -6.5), (x, 53, -7))
    f = walk_to(f, x + 0.5, -6.5) + 10
# -- the exit's (the E-bank's step-up's, the 1.0-m's the model's the
#    hold's — the 30-f's the 2.0-m's the overshoot's the twin's) ------
f = g.walk_leg(f, 18.5, g.z, y=54, hold=20)
f = walk_to(f, 18.5, -6.3)
f += 30
seg_mark("s5_dige")

# -- the scoop's 1 (the pool's edge's stand (17.5,-11.5)'s the — the
#    x18.5's the column's the pool's the cross's the capture: the pool's
#    cells' the (18,53,-10/-11)'s the = the water's the non-solid's,
#    the feet's 54's = the no-support's the fall's the swim's 3.1-b/s's
#    the hold's the short's the stop's mid-pool's (s6's the capture's);
#    the column's x17.5's the = the pool's the west's the bank's the
#    solid's the, the trench's the dip's the z-7's the cross's the
#    dry's the legal's the dip-step-up's; the aim's the pool's the
#    corner's source's the DDA's the probe-verified's) ------------------
f = walk_to(f, 17.5, -6.3)
f = walk_to(f, 17.5, -11.5)
g.look_point(f, 19.5, 53.5, -12.5)
f += 20
g.click(f, "right")
f += 120
seg_mark("s6_scoop1")

# -- the pour's W (the stand (2.5,-6.3)'s the, the z-7's the row's the —
#    the z-5.5's the stand's the ray's the z-6's the bank's the top's
#    the first-hit's the geometric's the impossible's; the jog's the
#    20-f's the coast-settle's the + the 16-f's the south's the, the
#    west-coast's the z-jog's the 0.4-m's the scramble's the fix's) ----
f = walk_to(f, 17.5, -5.5)
f = walk_to(f, 2.5, -5.5)
g.at(f + 20)
f = g.walk_leg(f, g.x, -6.3, hold=16)
f += 15
g.look_point(f, 3.5, 53.05, -6.2)
f += 25
g.click(f, "right")
F_POUR_W = g.cursor
f += 120
# the honest re-anchor's (the v3.21's twin's): the pour's stand's the
# actual's the (1.58,-7.0)'s the probe-pinned's (the jog's the
# west-coast's + the south's the overshoot's the 0.6-m's the z's the
# — the s8's the row's the z-5.5's the bank's the re-aim's the
# mandatory's, the wet's the trench's the row's the cross's the
# swim's the stop's the mid-river's the avoid)
g.x = 1.58
g.z = -7.0
# the campfire's out's frame's pin (the probe A's law's: the x9's the
# wet's the = the T0 + 21's tick (the 8th's spread's, the x2's pour's
# the), the out's = the +1's pass's frame's the measured's the
# twin's: the f16's the x11's wet's the f15's the +1's)
T0 = F_POUR_W + ((3 - F_POUR_W % 3) % 3)
CAMPAIGN_OUT = T0 + 22
# -- the watch's the W's gradient (the C's 3rd's the + the sweep's) -----
g.tap(f, "c")
f += 20
g.look_point(f, 7.5, 53.5, -6.5)
f += 15
f = sweep_still(f, 1.05, 900)
seg_mark("s7_watchw", out_frame=CAMPAIGN_OUT)

# -- the scoop's 2 (the pool's return's the, the ROW_N's + the U's
#    detour's the twin's) ---------------------------------------------
g.tap(f, "c")
f += 15
g.tap(f, "c")
f += 20
f = walk_to(f, g.x, -5.5)
f = walk_to(f, 17.5, -5.5)
f = walk_to(f, 17.5, -11.5)
g.look_point(f, 20.5, 53.5, -12.5)
f += 20
g.click(f, "right")
f += 120
# -- the pour's E (the stand (18.5,-6.3)'s the column's the north's —
#    the (18,53,-7)'s the bank's the x18's the east's the no-dip's) ----
f = walk_to(f, 18.5, -6.3)
g.look_point(f, 17.5, 53.05, -6.8)
f += 25
g.click(f, "right")
# the 240-f's the dwell's (the cascade's the 6-tick's the 18-f's the +
# the margin's — the demo's finished state's the simulation's frozen's
# (the still-frame's semantics': the F+100's save's the frozen's the
# mid-cascade's the, the s8's capture's root')
f += 240
seg_mark("s8_poure")

# -- the watch's the E's gradient (the 1st's person's the sweep's) ------
g.look_point(f, 12.5, 53.5, -6.5)
f += 15
f = sweep_still(f, 1.05, 700)
seg_mark("s9_watche")

# -- the torch's the place (the contrast's beat: the campfire's out's vs
#    the torch's stays-lit's the; the torch's the (14,54,-5)'s the — the
#    stand's the (14.5,-3.5)'s the, the aim's the (14.5,53.0,-5.3)'s the:
#    the y54's the cross's the z-4.61's the cell's the (14,54,-5)'s the
#    air's the, the first's the solid's the (14,53,-5)'s the bank's the
#    top's the -> the target's the (14,54,-5)'s the; the stand's the
#    AABB's the z[-3.8,-3.2]'s the the cell's the band's the [-5,-4)'s
#    the zero's the overlap's the (the -4.0's the stand's the 0.3-m's
#    the overlap's the reject's the capture's)) --------------------------
g.select(f, 2)
f += 20
f = walk_to(f, 18.5, -3.5)
f = walk_to(f, 14.5, -3.5)
g.at(f + 20)
g.look_point(f, 14.5, 53.0, -5.3)
f += 20
g.click(f, "right")
f += 120
g.select(f, 1)
f += 20
seg_mark("s10_torch")

# -- the swim's crossing (the probe B's v3's the pinned's holds' the:
#    the step-in's 15's + the 14-m's 252's + the swim-up's 25's + the
#    step-out's 30's — the landing's (0.75,54,-6.19)'s the) -------------
# the swim's the entry's the (the PITS 418 the 13's the 2nd's the
# capture's the: the 4-m's the east's the leg's the coast's the the
# 20-f's the settle's the + the jog's the the entry's the x's the
# +1.77-m's the east's the overshoot's the (the 20.27's the actual's
# the vs the 18.5's the track's the) = the swim's the 252-f's the pin's
# the 1.77-m's the short's the stop's the x6.25's the mid-river's the;
# the fix's the = the column's the x18.5's the the z's the walk's the
# last's the: the same-axis's the 2.0+0.8-m's the the coast's the z's
# only's the, the entry's the x's the the column's the locked's)
f = walk_to(f, 18.5, -3.5)
f = walk_to(f, 18.5, -6.3)
g.face(f, math.pi / 2)
f += 15
sx = g.x - 1.0
f = g.walk_leg(f, sx, g.z, y=53, hold=15)
sx2 = sx - 14.0
f = g.walk_leg(f, sx2, g.z, y=53, hold=252)
g.hold_key(f, 25, "space")
f += 10
f = g.walk_leg(f, sx2 - 2.0, g.z, y=54, hold=30)
f += 60
# the honest re-anchor's (the v3.21's twin's): the swim's the model's
# mismatch's the landing's the 0.75-m's the west's the probe-pinned's
# (the track's the 1.5's vs the actual's the 0.75's) — the re-pin's the
# before the patrol's the (the drift's the propagation's the avoid)
g.x = 0.75
seg_mark("s11_swim")

# -- the E-panel's the showcase (the 15-dirt's + the bucket's) ----------
g.open_panel(f)
f += 30
g.at(f + 1620)
f += 1620
g.close_panel(f)
f += 120
seg_mark("s12_panel")

# -- the 3rd-person's the 4-line's the patrol ---------------------------
g.tap(f, "c")
f += 20
# the east's terminus's the 21.5's the (the 22.5's the capture's the:
# the open's the 20-m's the path's the drift's the 0.5-1.1-m's the
# (the 16e's the law's the fps's the function's the) + the swim's the
# landing's the x's the offset's the = the actual's the 23.8's the
# edge's the over's the fall's the y38's the hp13's the armA3's the
# capture's the — the 21.5's the 2.3-m's the edge's the margin's the)
f = walk_to(f, 1.5, -5.0)
f = walk_to(f, 21.5, -5.0)
g.look_point(f, 9.5, 53.0, -6.5)
f += 15
f = sweep_still(f, 0.80, 500)
f = walk_to(f, 21.5, -9.0)
f = walk_to(f, 1.5, -9.0)
g.look_point(f, 15.5, 53.5, -11.0)
f += 15
f = sweep_still(f, 0.80, 500)
f = walk_to(f, 1.5, -5.0)
g.at(f + 900)
f += 900
seg_mark("s13_patrol")

# -- the ending (the first-person's back's + the river's face's the) ----
g.tap(f, "c")
f += 15
g.tap(f, "c")
f += 20
f = walk_to(f, 1.5, -4.5)
g.look_point(f, 9.5, 53.5, -6.5)
f += 15
g.at(f + 1400)
f += 1400
g.sweep(f, 700, 0.55)
f += 700
seg_mark("s14_full")

here = os.path.dirname(os.path.abspath(__file__))
tl = g.dump(os.path.join(here, "..", "..", "assets", "demos",
                         "guide_river_timeline.json"))
json.dump({"segments": SEGMENTS, "campfire_out": CAMPAIGN_OUT},
          open(os.path.join(here, "guide_river_segments.json"), "w"),
          indent=1)
print(f"segments: {len(SEGMENTS)} (the user's 30's cap's) | "
      f"dirt broken: {len(BROKEN)} | campfire_out: {CAMPAIGN_OUT}")
