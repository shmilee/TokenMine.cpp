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
import os
import sys

SENS = 0.0025          # radians / pixel (the mouseSensitivity's default)
WALK = 4.3             # blocks / second (the walkSpeed's default)
EYE = 1.62             # the eye's height over the feet
WALK_STOP_MARGIN = 2  # the hold's stop margin (frames): the hold's
                       # start's input-registration's loss (~2.5f, the
                       # calibration's face: 103f -> 100.4f eff) means a
                       # hold of round(d/WALK*60)+2 frames stops 0.03-
                       # 0.11 m SHORT of the leg's end — inside the cell
                       # (the relative aim's tolerance). The TRACK models
                       # the exact corner arrival (the face below).
                       # (the axis-aligned probe's face: a hold's first
                       # ~2.5 frames move ~0 — the track deducts 3)
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
        self.actions = []
        self._last_panel_click = -10 ** 9

    # -- the state's face ------------------------------------------------
    def at(self, f):
        """Advance the cursor to frame f (the still's observe beat).
        The demo's length extends to f via a zero wheel tick (the
        pure no-op event — the wall's dts' face, the sim's zero)."""
        f = int(f)
        assert f >= 0, "a frame is not negative"
        if f >= self.cursor:
            # the extension's event (a zero wheel = the inject's
            # handleMouseWheel(0)'s no-op, the length's carrier)
            self.actions.append({"at_frame": f, "action": "wheel",
                                 "ticks": 0})
        self.cursor = max(self.cursor, f)
        return self

    def _now(self):
        return self.cursor

    def _track_walk(self, frames, yaw=None):
        """Advance the position (the W/A/S/D's intent — the caller
        passes the held key's set; the flat ground's assumption: the
        guide scenes are the platform's face, the y's constant)."""
        if yaw is None:
            yaw = self.yaw
        fx = -math.sin(yaw)
        fz = -math.cos(yaw)
        rx = -fz
        rz = fx
        dist = WALK * frames / 60.0
        return (fx, fz, rx, rz, dist)

    # -- the capture's face (the pointer's lock) --------------------------
    def capture(self, f):
        """The initial LMB tap (the pointer's capture — the shipped
        demos' frame-10's face: the look's deltas + the panel's keys
        only run while the pointer is locked, the demo's first click
        is the capture, not a world action)."""
        self.at(f)
        self.actions.append({"at_frame": f, "action": "mouse_down",
                             "button": "left"})
        self.actions.append({"at_frame": f + 5, "action": "mouse_up",
                             "button": "left"})
        self.cursor = f + 5
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

    def walk_polyline(self, f, points, seg=1.6):
        """A multi-leg walk (the axis-aligned's discipline — the
        sub-axis's component's physics drop's gate-catch: every leg is
        due E/W/N/S, the segments' 1.6-m's, the re-aim's between).
        points = the (x, z)'s float's list (the corners' exact — the
        cell-center's +0.5's the caller's face). The track's advance =
        the EFFECTIVE distance (the reg's loss's deduct — the segment's
        mx's full-ratio's drift's gate-catch: the step's target is the
        current track's eff's ahead, not the leg's k/n). Returns the
        end's frame."""
        for (tx, tz) in points:
            px, pz = self.x, self.z
            total = math.hypot(tx - px, tz - pz)
            if total < 0.05:
                continue
            ux = (tx - px) / total
            uz = (tz - pz) / total
            while math.hypot(tx - self.x, tz - self.z) > 0.05:
                step = min(seg, math.hypot(tx - self.x, tz - self.z))
                frames = int(round(step / WALK * 60)) + WALK_REG_LOSS - 2
                if frames < WALK_REG_LOSS + 4:
                    frames = WALK_REG_LOSS + 4
                mx = self.x + ux * step
                mz = self.z + uz * step
                # the segment's re-aim (the same line's yaw)
                dx = mx - self.x
                dz = mz - self.z
                if math.hypot(dx, dz) > 0.01:
                    yaw = math.atan2(-dx, -dz)
                    d = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
                    if abs(d) > 0.001:
                        self.actions.append({
                            "at_frame": f, "action": "mouse_move",
                            "dx": round(-d / SENS, 3), "dy": 0.0})
                        self.yaw = yaw
                eff = max(0, frames - WALK_REG_LOSS) * WALK / 60.0
                self.actions.append({"at_frame": f, "action": "hold",
                                     "key": "w", "until_frame": f + frames})
                self.x += ux * eff
                self.z += uz * eff
                f += frames + 8  # the segment's settle's pause
        self.cursor = f
        return f

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
        self.cursor = f + dur
        return self

    # -- the walk's face -------------------------------------------------
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

    def walk_leg(self, f, tx, tz):
        """One axis-aligned walk leg to the EXACT point (tx, tz) (the
        cell-center's +0.5's the caller's face). The hold's length =
        the distance's frames + the stop margin (the reg's loss's twin:
        the actual stop is 0.03-0.11 m short, the cell's inside); the
        track models the exact corner (the relative aim's tolerance)."""
        self.at(f)
        dx = tx - self.x
        dz = tz - self.z
        d = math.hypot(dx, dz)
        assert d > 0.05, "walk_leg: a zero-length leg"
        assert abs(dx) < 0.05 or abs(dz) < 0.05, \
            "walk_leg: axis-aligned legs only (the diagonal's sub-axis" \
            " physics loss — split the corner)"
        yaw = math.atan2(-dx, -dz)
        dyaw = (yaw - self.yaw + math.pi) % (2 * math.pi) - math.pi
        if abs(dyaw) > 0.001:
            self.actions.append({
                "at_frame": f, "action": "mouse_move",
                "dx": round(-dyaw / SENS, 3), "dy": 0.0})
            self.yaw = yaw
        frames = int(round(d / WALK * 60)) + WALK_STOP_MARGIN
        assert frames >= 8, "walk_leg: a too-short leg"
        self.actions.append({"at_frame": f + 1, "action": "hold",
                             "key": "w", "until_frame": f + 1 + frames})
        self.x = tx
        self.z = tz
        f = f + 1 + frames + 8  # the settle's pause
        self.cursor = f
        return f

    def walk_polyline(self, f, points):
        """Axis-aligned legs to the EXACT (x, z)'s in order (the
        corners' list — the cell-center's +0.5's). Returns the end's
        frame."""
        for (px, pz) in points:
            f = self.walk_leg(f, px, pz)
        return f

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
        self.cursor = f + gap
        return self

    def hold_key(self, f, dur, key):
        self.at(f)
        self.actions.append({"at_frame": f, "action": "hold",
                             "key": key, "until_frame": f + dur})
        self.cursor = f + dur
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
        self.cursor = f + frames
        return self

    def click(self, f, button="left", hold=4):
        """A world's RMB/LMB tap (the place / the interact — the aim's
        the caller's)."""
        self.at(f)
        self.actions.append({"at_frame": f, "action": "mouse_down",
                             "button": button})
        self.actions.append({"at_frame": f + hold, "action": "mouse_up",
                             "button": button})
        self.cursor = f + hold
        return self

    def pick_up_walk(self, f, dur=50):
        """A slow circle over the ground's drops (the magnet's pull —
        the walk_dir's w/s/a/d's quarter turns)."""
        self.walk_dir(f, dur // 4, "w")
        self.walk_dir(f + dur // 4, dur // 4, "s")
        self.cursor = f + dur
        return self

    # -- the panel's face ------------------------------------------------
    def _panel_raw(self, f, panel, slot, button="left"):
        self.at(f)
        self.actions.append({"at_frame": f, "action": "panel_click",
                             "panel": panel, "slot": slot,
                             "button": button})
        self.cursor = f + 1
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
        self.cursor = f + 35
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
        import json
        json.dump(doc, open(path, "w"), indent=2)
        total = max(a["at_frame"] +
                    (a.get("until_frame", a["at_frame"]) + 1
                     if a["action"] == "hold" else 1)
                    for a in doc["actions"])
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
