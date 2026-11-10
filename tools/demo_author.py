#!/usr/bin/env python3
"""V3-16a: the guide demo's AUTHOR DSL.

A guide demo is a *scripted player* (the V3-9 author timeline, the
--demo-author's face): this helper computes the mouse's deltas from
the player's TRACKED state (the position / the yaw / the pitch — the
aiming's single source), so the timeline stays geometry-free for the
LOOK (the panel's clicks are the panel_click's slot addressing — the
uiScale / window's independence, the V3-16a face).

The tracked state's discipline (the determinism's root):
  - the look = yaw -= dx * 0.0025 (the mouseSensitivity's default —
    the committed config carries no player.mouse_sensitivity; a
    scene's config that changes it BREAKS the look, named at review)
  - the walk = 4.3 blocks/second along the yaw (the walkSpeed's
    default, no sprint)
  - the eye = the feet + 1.62 (the player's eye height)
  - every action is a FRAME (the 1/60's step) — a float frame is a bug

Usage:
  python3 tools/demo_author.py <build_script.py>
the build script:
  from demo_author import Guide
  g = Guide(scene="pilot_mine", pos=(0.5, 54, -2), yaw=0.0, pitch=-0.08)
  ...  g.look_at(100, (8, 54, -8)); g.walk_to(120, (8, 54, -7)); ...
  g.dump("assets/demos/guide_mining_timeline.json")
"""
import math
import json
import os
import sys

SENS = 0.0025          # radians / pixel (the mouseSensitivity's default)
# The ENT-7's ground walk's calibration model (the player.cpp 358-394's
# twin's): the exponential approach to the walkSpeed (the kGroundAccelRate
# 8.0's k) + the release's slide (the kGroundDecelRate 12.0's k, the
# kVelocityStopEps 0.001's rest's). The per-leg's distance is the hold's
# length's FUNCTION (the inertia's — a constant m/s's WRONG): the 33-f's
# hold's 2.2056-m's steady's, the gate-226's probe's ground truth's (7
# chained 33-f's legs = 15.4922 m's the measured's vs the model's 15.4917
# = 0.0005-m's match). The old's flat WALK constant's (the 4.3's nominal's
# / the 3.55's the mis-attributed's) both the wrong's = the v3.1-v3.11's
# 6/6's gate-catch's root's (PITS 417).
K_WALK_ACCEL = 8.0     # the kGroundAccelRate's (player.hpp)
K_WALK_DECEL = 12.0    # the kGroundDecelRate's (player.hpp)
WALK_TARGET = 4.3      # the walkSpeed's param's (player.hpp's 4.3f's)
_WALK_DT = 1.0 / 60.0  # the demo's dt's (the created dmo's 16.6667-ms's uniform's)
_WALK_FA = 1.0 - math.exp(-K_WALK_ACCEL * _WALK_DT)
_WALK_FD = math.exp(-K_WALK_DECEL * _WALK_DT)
_WALK_EPS = 0.001      # the kVelocityStopEps's (player.hpp's)
_WALK_COAST = 9        # the release's frames per leg: the up's frame's + the 8-f's settle's pause (the next leg's face frame = the 9th's, the W-up's)


def _walk_dd(n, vx, vz, axis):
    """The n-f's W hold's + the 9-f's coast's (the game's per-frame's the
    mirror's): returns (axis's dist, perp's dist, exit's vx, exit's vz).
    axis = +1/-1 = the x's leg, +2/-2 = the z's leg. The game's face
    (player.cpp 358-394): per frame the axis's component ramps
    (T - v)*FA toward the wish's, the PERP's (the old's direction's —
    the corner's entry's) decays v*(1-FA) while W's the held's, then the
    coast's 9-f's both decay's FD's the kVelocityStopEps's the zero's —
    the v3.21's gate-226's root 8: the entry's the velocity's at the
    corner's is the PERP's (the previous's leg's the coast's exit's),
    NOT the along-axis's (the scalar's model's the corner's the along's
    0.06-0.16-m's the UNDERSHOOT's + the perp's 0.07-0.08-m's the
    DRIFT's the twin's; the S1's the per-leg's the probe's the rest-entry
    17-f's the +0.010-vs-model-+0.0096's the per-frame's exact's
    re-verified's)."""
    T = WALK_TARGET
    s = 1.0 if axis > 0 else -1.0
    # the entry's the component's ALONG the axis's (the sign's the flip's
    # for the negative's axis's — the v3.21's gate-226's the 11-m-at-8-f's
    # the mis-signed entry's the loud's the assert's the twin's)
    if axis in (1, -1):  # the x's leg
        va, vp = vx * s, vz
    else:                # the z's leg
        va, vp = vz * s, vx
    da = dp = 0.0
    for _ in range(n):
        va += (T - va) * _WALK_FA
        vp *= (1.0 - _WALK_FA)
        da += va * _WALK_DT
        dp += vp * _WALK_DT
    for _ in range(_WALK_COAST):
        va *= _WALK_FD
        if abs(va) < _WALK_EPS:
            va = 0.0
        vp *= _WALK_FD
        if abs(vp) < _WALK_EPS:
            vp = 0.0
        da += va * _WALK_DT
        dp += vp * _WALK_DT
    s = 1.0 if axis > 0 else -1.0
    if axis in (1, -1):  # the x's leg: axis = x, perp = z
        evx, evz = s * va, vp
    else:                # the z's leg: axis = z, perp = x
        evx, evz = vp, s * va
    return s * da, dp, evx, evz


def _walk_hold_for(d, vx, vz, axis):
    """The hold's frames for the door-to-door's distance d along `axis`
    from the ACTUAL entry velocity vector (the vector's model's the face
    — the v3.21's root 8's the fix's), within the +-0.04-m's the
    assert's face."""
    best = None
    besterr = None
    for n in range(8, 160):
        da, dp, evx, evz = _walk_dd(n, vx, vz, axis)
        err = abs(da) - d
        if besterr is None or abs(err) < abs(besterr):
            best, besterr = n, err
    assert abs(besterr) <= 0.04, (
        "walk_leg: %s m along axis %s from entry (%.3f, %.3f) not "
        "reachable within tolerance (the nearest's %d f = %s m)"
        % (round(d, 2), axis, vx, vz, best, round(abs(da), 3)))
    return best
# The creative fly's horizontal: the walkSpeed x kFlySpeedMultiplier's
# NOMINAL 6.88, the MEASURED 7.12 (the 17-f hold's 2.0112-m's twin, the
# PITS 416's calibration's). The fly's v is the isDown's direct-set's (no
# registration's, no ramp's — the vertical's kFlyVerticalSpeed 4.0's twin):
# a hold of round(d/FLY*60) lands within 0.01 m (the walk's +2 margin's NOT
# for the fly — the fly's self-anchoring's face).
FLY = 7.12             # blocks / second (the creative fly's horizontal)
EYE = 1.62             # the eye's height over the feet
MIN_CLICK_GAP = 20     # the panel's click's spacing (the 0.3-s
                       # double-click collect window's 18-frame's margin)


class Guide:
    def __init__(self, scene=None, seed=None, pos=(0.5, 54, 0.5),
                 yaw=0.0, pitch=0.0):
        assert (scene is None) != (seed is None), \
            "the world's spec: a scene name XOR a fresh seed"
        self.scene = scene
        self.seed = seed
        self.x, self.y, self.z = float(pos[0]), float(pos[1]), float(pos[2])
        self.yaw = float(yaw)
        self.pitch = float(pitch)
        self.cursor = 0
        # the tracked velocity VECTOR (the v3.21's root 8's face — the
        # scalar's the _walkV's the corner's the perp's the loss's the
        # twin's): the axis's the ramp's + the perp's the decay's the
        # game's the mirror's the (player.cpp 358-394's).
        self._walkVx = 0.0
        self._walkVz = 0.0
        self.actions = []
        self._last_panel_click = -10 ** 9
        # The fly's toggles' frames (the gamepad's B's the post-patch's face —
        # the timeline's vocabulary's the keyboard/mouse's only, the dmo's
        # demo_fly_patch.py's the insertion's; the PITS 416's wall-clock's
        # double-tap's window's the flake's the gate-catch's root).
        self.fly_toggles = []

    # -- the state's face ------------------------------------------------
    def at(self, f):
        """Advance the cursor to frame f (the still's observe beat).
        The demo's length extends to f via a zero wheel tick (the
        pure no-op event — the wall's dts' face, the sim's zero)."""
        f = int(f)
        assert f >= 0, "a frame is not negative"
        if f > self.cursor:
            self._coast(f - self.cursor)
        if f >= self.cursor:
            # the extension's event (a zero wheel = the inject's
            # handleMouseWheel(0)'s no-op, the length's carrier)
            self.actions.append({"at_frame": f, "action": "wheel",
                                 "ticks": 0})
        self.cursor = max(self.cursor, f)
        return self

    def _now(self):
        return self.cursor

    def _coast(self, frames):
        """The release's slide (the player.cpp 375-394's twin's): the
        tracked velocity's exp decay over `frames` of no-W — the
        position's the ADVANCE's too (the v3.21's root 8: the settle's
        gap's the residual's the 0.05-m's the slide's the S1's the f938-f
        1028's the +0.052-m's the probe's the twin's — the old's
        no-position's _coast's the track's the 0.05-m's the off's)."""
        x, z = self.x, self.z
        vx, vz = self._walkVx, self._walkVz
        for _ in range(int(frames)):
            vx *= _WALK_FD
            if abs(vx) < _WALK_EPS:
                vx = 0.0
            vz *= _WALK_FD
            if abs(vz) < _WALK_EPS:
                vz = 0.0
            x += vx * _WALK_DT
            z += vz * _WALK_DT
        self.x, self.z = x, z
        self._walkVx, self._walkVz = vx, vz

    def _to(self, f):
        """Advance the cursor to f, coasting the gap (the release's
        slide's track's twin's — the walk's calibration's state stays
        honest across every non-walk beat)."""
        f = int(f)
        if f > self.cursor:
            self._coast(f - self.cursor)
        self.cursor = max(self.cursor, f)

    def fly_toggle(self, f):
        """Record a gamepad-B fly toggle at frame f (the sim-frame's
        deterministic's face). The Space's double-tap's the toggle's the
        wall-clock's window's the flake's (a 120-s auto-save stall's or the
        startup burst's straddling the pair = the PITS 416's gate-catch), so
        the fly's ON/OFF goes through the B's single-press (no window, no
        jump grant). It lands as a SIDE-CAR frame list (the timeline's author
        vocabulary's keyboard/mouse only — the record's face); demo_fly_patch.
        py inserts the gamepad_button events into the authored dmo. No
        movement, no cursor advance (a zero-duration input)."""
        self.at(f)
        self.fly_toggles.append(int(f))

    # -- the capture's face (the pointer's lock) --------------------------
    def capture(self, f):
        """The initial LMB tap (the pointer's capture — the shipped
        demos' frame-10's face: the look's deltas + the panel's keys
        only run while the pointer is locked, the demo's first click
        is the capture, not a world action)."""
        self.at(f)
        # 2f: the down's edge captures (the !locked's branch), the up's
        # at f+1 leaves ZERO locked-mining frame (the 5-f's hold's
        # gate-catch: f1-f4's locked isDown = the aim's ray's instant
        # break — the hardness-0's crop's 3-plant's destroy's root).
        self.actions.append({"at_frame": f, "action": "mouse_down",
                             "button": "left"})
        self.actions.append({"at_frame": f + 1, "action": "mouse_up",
                             "button": "left"})
        self._to(f + 1)
        return self

    # -- the look's face -------------------------------------------------
    def look_rel(self, f, dyaw=0.0, dpitch=0.0):
        """A relative look (the delta's mouse_move — the yaw/pitch's
        update, the tracked state's advance)."""
        self.at(f)
        # the game's face: yaw -= dx * sens (the look's sign — the
        # +dyaw's turn needs the negative dx, the twin's disc)
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": round(-dyaw / SENS, 3),
            "dy": round(-dpitch / SENS, 3)})
        self.yaw += dyaw
        self.pitch = max(-1.5, min(1.5, self.pitch + dpitch))
        return self

    def look_at(self, f, cell, y_off=0.5):
        """Aim the crosshair at the cell's center (the yaw/pitch's
        absolute set — the delta's computation from the tracked state)."""
        tx = cell[0] + 0.5
        ty = cell[1] + y_off
        tz = cell[2] + 0.5
        dx = tx - self.x
        dy = ty - (self.y + EYE)
        dz = tz - self.z
        yaw = math.atan2(-dx, -dz)
        # the shortest arc's yaw (the +-pi's wrap)
        d = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
        pitch = math.atan2(dy, math.hypot(dx, dz))
        pitch = max(-1.5, min(1.5, pitch))
        self.at(f)
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": round(-d / SENS, 3),
            "dy": round(-(pitch - self.pitch) / SENS, 3)})
        self.yaw = yaw
        self.pitch = pitch
        return self

    def look_ground(self, f, cell, ground_y=54.0):
        """Aim the crosshair at the ground's point in the cell (the
        PLACE's face: the ray's hit on the platform's top face, the
        block's land in the cell — the center's aim flies 0.5 over
        and lands a cell off, the probe's gate-catch)."""
        self.at(f)
        dx = cell[0] + 0.5 - self.x
        dy = ground_y - (self.y + EYE)
        dz = cell[2] + 0.5 - self.z
        yaw = math.atan2(-dx, -dz)
        d = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
        pitch = max(-1.5, min(1.5, math.atan2(dy, math.hypot(dx, dz))))
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": round(-d / SENS, 3),
            "dy": round(-(pitch - self.pitch) / SENS, 3)})
        self.yaw = yaw
        self.pitch = pitch
        return self

    def place_at(self, f, cell, ground_y=54.0):
        """The RMB's place in the cell (the ground's aim + the click)."""
        self.look_ground(f, cell, ground_y)
        return self.click(f + 30, "right")

    def sweep(self, f, dur, dyaw):
        """A slow look's sweep (the observe beat — the dyaw over dur)."""
        self.at(f)
        # the sweep's sign (the look_rel's twin: the +dyaw's turn =
        # the negative dx)
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": round(-dyaw / SENS, 3), "dy": 0.0})
        self.yaw += dyaw
        self._to(f + dur)
        return self

    # -- the walk's face -------------------------------------------------
    def sweep_gradual(self, f, dur, dyaw, steps=12):
        """A GRADUAL look sweep (the showcase's face: the 360-deg's the
        12-step's — the sweep's single-event's snap's the audience's
        the disorient's twin). The 12 mouse_moves spread over dur, the
        track's yaw's per-step's advance."""
        self.at(f)
        step = dyaw / steps
        for i in range(steps):
            fi = f + (i * dur) // steps
            self.actions.append({
                "at_frame": fi, "action": "mouse_move",
                "dx": round(-step / SENS, 3), "dy": 0.0})
            self.yaw += step
        self._to(f + dur)
        return self

    def face(self, f, yaw):
        """Set the tracked yaw's absolute (the delta's look emit)."""
        self.at(f)
        d = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
        if abs(d) > 0.001:
            self.actions.append({
                "at_frame": f, "action": "mouse_move",
                "dx": round(-d / SENS, 3), "dy": 0.0})
            self.yaw = yaw
        return self

    def walk_leg(self, f, tx, tz, y=None, hold=None, fly=False):
        """One axis-aligned walk leg to the EXACT point (tx, tz) (the
        cell-center's +0.5's the caller's face). The hold's length =
        the ENT-7 model's solve for the door-to-door's distance (the
        inertia's twin: the accel's ramp + the release's 9-f slide —
        the +-0.03-m's land, the gate-226's probe's calibrated); the
        track models the exact corner (the relative aim's tolerance).
        y: the caller's KNOWLEDGE of the leg's END's feet height (the
        step-up's +1 / the drop's -N — the track's flat assumption's
        lift, the V3-16c's roof's face); hold: an explicit hold's
        frames (the short-hop's drop: the w's release before the edge,
        the air's drift's the calibration's twin); fly: the creative's
        flight's leg (the FLY's pace's the no-margin's — the fly's v's
        the isDown's direct-set's the no-registration's face)."""
        self.at(f)
        dx = tx - self.x
        dz = tz - self.z
        d = math.hypot(dx, dz)
        assert d > 0.05, "walk_leg: a zero-length leg"
        assert abs(dx) < 0.05 or abs(dz) < 0.05, \
            "walk_leg: axis-aligned legs only (the diagonal's sub-axis" \
            " physics loss — split the corner)"
        axis = (1 if dx > 0 else -1) if abs(dx) > abs(dz) \
            else (2 if dz > 0 else -2)
        yaw = math.atan2(-dx, -dz)
        dyaw = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
        if abs(dyaw) > 0.001:
            self.actions.append({
                "at_frame": f, "action": "mouse_move",
                "dx": round(-dyaw / SENS, 3), "dy": 0.0})
            self.yaw = yaw
        if hold is None:
            if fly:
                hold = int(round(d / FLY * 60))
            else:
                hold = _walk_hold_for(d, self._walkVx, self._walkVz, axis)
        frames = hold
        assert frames >= 8, "walk_leg: a too-short leg"
        self.actions.append({"at_frame": f + 1, "action": "hold",
                             "key": "w", "until_frame": f + 1 + frames})
        if fly:
            self._walkVx, self._walkVz = 0.0, 0.0  # the fly's exit's velocity's unmodeled's
        else:
            da, dp, evx, evz = _walk_dd(frames, self._walkVx,
                                        self._walkVz, axis)
            self._walkVx, self._walkVz = evx, evz
            # the HONEST track's (the v3.21's root 8's face): the axis's
            # lands at the model's da (≈ d's the +-0.04-m's the solve's
            # granularity's — the walk_to's 0.05's snap's absorbs it),
            # the PERP's drift's dp's the OLD's direction's the adds to
            # the position's (the track's the game's the frame-by-frame's
            # twin's now — the S1's the 0.07-m's the boundary's the
            # self-correcting's the walk_to's the residual's the face).
            x0, z0 = self.x, self.z
            if axis in (1, -1):
                self.x, self.z = x0 + da, z0 + dp
            else:
                self.x, self.z = x0 + dp, z0 + da
        if y is not None:
            self.y = y
        f = f + 1 + frames + 8  # the settle's pause (the 9th coast frame = the next leg's face frame, modeled in _walk_dd)
        self.cursor = f
        return f

    def walk_polyline(self, f, points):
        """Axis-aligned legs to the EXACT (x, z)'s in order (the
        corners' list — the cell-center's +0.5's). Returns the end's
        frame."""
        for (px, pz) in points:
            f = self.walk_leg(f, px, pz)
        return f

    def look_point(self, f, x, y, z):
        """Aim the exact 3D point (the FACE-aim's face: a side face's
        point 0.1 past the plane — the roof's middle row's placement's
        gate-catch: the top face's aim needs the eye's above, the
        side face's works from below)."""
        dx = x - self.x
        dy = y - (self.y + EYE)
        dz = z - self.z
        yaw = math.atan2(-dx, -dz)
        d = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
        pitch = max(-1.5, min(1.5, math.atan2(dy, math.hypot(dx, dz))))
        self.at(f)
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": round(-d / SENS, 3),
            "dy": round(-(pitch - self.pitch) / SENS, 3)})
        self.yaw = yaw
        self.pitch = pitch
        return self

    def place_ahead(self, f, dist=1.0, ground_y=54.0):
        """The RELATIVE place (the drift's robustness's face): aim the
        ground's point `dist` ahead in the facing + RMB — the block
        lands in the cell ahead, the aim's target MOVES WITH the player
        (the absolute cell aim's 1-cell's miss's gate-catch: a +-0.3-m's
        position drift shifts the hit off the cell's edge)."""
        self.at(f)
        pitch = math.atan2(ground_y - (self.y + EYE), dist)
        pitch = max(-1.5, min(1.5, pitch))
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": 0.0, "dy": round(-(pitch - self.pitch) / SENS, 3)})
        self.pitch = pitch
        return self.click(f + 30, "right")

    def open_ahead(self, f, dist=1.0):
        """The RELATIVE container open (the place_ahead's twin): aim the
        block's center `dist` ahead (ground-level's block: center = the
        feet's +0.5) + RMB — the table/furnace's panel's open."""
        self.at(f)
        pitch = math.atan2(0.5 - EYE, dist)
        self.actions.append({
            "at_frame": f, "action": "mouse_move",
            "dx": 0.0, "dy": round(-(pitch - self.pitch) / SENS, 3)})
        self.pitch = pitch
        return self.click(f + 30, "right")

    # -- the hands' face -------------------------------------------------
    def tap(self, f, key, gap=6):
        self.at(f)
        self.actions.append({"at_frame": f, "action": "key_down",
                             "key": key})
        self.actions.append({"at_frame": f + gap, "action": "key_up",
                             "key": key})
        self._to(f + gap)
        return self

    def hold_key(self, f, dur, key):
        self.at(f)
        self.actions.append({"at_frame": f, "action": "hold",
                             "key": key, "until_frame": f + dur})
        self._to(f + dur)
        return self

    def mine(self, f, seconds=3.5):
        """A hold-to-break (the aim first — the caller's look_at — the
        LMB's hold, the break's second's margin: the bare-hand wood's
        3-s, the pickaxe's ore's 1.1-s + the margin's twin)."""
        frames = int(round(seconds * 60))
        self.at(f)
        self.actions.append({"at_frame": f, "action": "mouse_down",
                             "button": "left"})
        self.actions.append({"at_frame": f + frames, "action": "mouse_up",
                             "button": "left"})
        self._to(f + frames)
        return self

    def click(self, f, button="left", hold=4):
        """A world's RMB/LMB tap (the place / the interact — the aim's
        the caller's)."""
        self.at(f)
        self.actions.append({"at_frame": f, "action": "mouse_down",
                             "button": button})
        self.actions.append({"at_frame": f + hold, "action": "mouse_up",
                             "button": button})
        self._to(f + hold)
        return self

    def pick_up_walk(self, f, dur=50):
        """A slow circle over the ground's drops (the magnet's pull —
        the walk_dir's w/s/a/d's quarter turns)."""
        self.walk_dir(f, dur // 4, "w")
        self.walk_dir(f + dur // 4, dur // 4, "s")
        self._to(f + dur)
        return self

    def wheel(self, f, ticks):
        """A wheel's tick (the creative's grid scroll — the V3-16d's
        grant's face: 1 tick = 1 item)."""
        self.at(f)
        self.actions.append({"at_frame": f, "action": "wheel",
                             "ticks": int(ticks)})
        self._to(f + 1)
        return self

    # -- the panel's face ------------------------------------------------
    def _panel_raw(self, f, panel, slot, button="left"):
        self.at(f)
        self.actions.append({"at_frame": f, "action": "panel_click",
                             "panel": panel, "slot": slot,
                             "button": button})
        self._to(f + 1)
        return self

    def panel(self, f, panel, slot, button="left"):
        """A geometry-free panel click (the V3-16a face — the slot's
        name, the uiScale / window's independence)."""
        if f - self._last_panel_click < MIN_CLICK_GAP:
            raise AssertionError(
                "panel click at f%d is inside the double-click collect"
                " window (>= %d frames apart, or use panel_collect)"
                % (f, MIN_CLICK_GAP))
        self._last_panel_click = f
        return self._panel_raw(f, panel, slot, button)

    def panel_collect(self, f, panel, slot, target):
        """The double-click's collect (the CRAFT-7's face — the item's
        consolidation from every slot into the target's): the first
        left's click arms the pairing, the second (the 0.3-s's within)
        collects, the third's places the carried's whole into the
        target (the merge's twin)."""
        self._panel_raw(f, panel, slot, "left")
        self._panel_raw(f + 10, panel, slot, "left")
        self._panel_raw(f + 35, panel, target, "left")
        self._last_panel_click = f + 35
        self._to(f + 35)
        return self

    def open_panel(self, f):
        return self.tap(f, "e")

    def close_panel(self, f):
        """The E's close + the re-capture's click (the closePanelFor's
        pointer stays FREE — the next locked-dependent act, the re-open
        'e' / the world's RMB, needs the capture's twin: the LMB's tap
        in the free state is the capture, not a world action)."""
        self.tap(f, "e")
        return self.capture(f + 25)

    def select(self, f, slot1):
        """The hotbar's select (the digit key — slot1 = 1..9)."""
        return self.tap(f, str(slot1))

    # -- the dump's face -------------------------------------------------
    def dump(self, path):
        world = ({"type": "scene", "name": self.scene} if self.scene
                 else {"type": "fresh", "seed": self.seed})
        doc = {"world": world,
               "actions": sorted(self.actions,
                                 key=lambda a: a["at_frame"])}
        # the frame's non-decreasing (the author's parse's discipline)
        for i in range(1, len(doc["actions"])):
            assert doc["actions"][i]["at_frame"] >= \
                doc["actions"][i - 1]["at_frame"], \
                "the timeline's out-of-order frame (the author's reject)"
        json.dump(doc, open(path, "w"), indent=2)
        # The fly's toggle's side-car (the dmo's post-patch's input —
        # demo_fly_patch.py's): written next to the timeline (the same
        # directory's the .fly_toggles.json's suffix's).
        if self.fly_toggles:
            tp = os.path.join(os.path.dirname(os.path.abspath(path)),
                              os.path.basename(path) + ".fly_toggles.json")
            json.dump(sorted(self.fly_toggles), open(tp, "w"))
            print("fly toggles: %d (-> %s)"
                  % (len(self.fly_toggles), os.path.basename(tp)))
        # the dmo's length = the C++'s twin (authorDemoFile): the max's
        # at_frame+1 over every action, the hold's until_frame+1 the bound.
        total = max([a["at_frame"] + 1 for a in doc["actions"]] +
                    [a["until_frame"] + 1 for a in doc["actions"]
                     if a["action"] == "hold" and a.get("until_frame", -1) >= 0])
        print("timeline %s: %d actions, ~%d frames (%.1f min)"
              % (os.path.basename(path), len(doc["actions"]), total,
                 total / 3600.0))
        return path


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    g_path = os.path.abspath(sys.argv[1])
    ns = {"__file__": g_path}
    exec(compile(open(g_path).read(), g_path, "exec"), ns)
