#!/usr/bin/env python3
"""V3-9: the shipped example demo's deterministic generator.

Regenerates assets/demos/example.dmo (+ the human-readable
example_timeline.json) — a re-run is BYTE-IDENTICAL (the demo's first-
play x2 acceptance is the C++ side's; this script's idempotence is the
asset's). The content = the "simple version" the design's contract
ships: the spawn area's walk + aim + mine hold + the inventory open /
close + the third-person view at the end (a portal / mining sequence
needs a saved scene — the v1 example stays fresh-world self-contained).

The container is the V3-7 DETERMINISTIC gzip (the hand-pinned 10-byte
header + the raw deflate level 6 memLevel 8 + the CRC32/ISIZE trailer —
the PITS 385 lesson: the zlib / Python gzip WRAPPERS drift their MTIME /
XFL / OS bytes, so the container is built by hand). The expansion rules
mirror src/core/demo.cpp's authorDemoFile EXACTLY (the dts = 1/60 fixed
step, the event order = (frame, action index, the hold's release = the
second half), frames = max(all at_frame, all until_frame) + 1).
"""
import json
import re
import struct
import sys
import zlib

# The pinned container header (the save's kGzipHeader — every byte).
GZIP_HEADER = bytes([0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03])

# The author's key vocabulary -> the SDL virtual keycode (the C++
# authorKeycode's table; the letters are ASCII = the SDLK values).
KEYCODES = {
    "w": 0x77, "a": 0x61, "s": 0x73, "d": 0x64,
    "q": 0x71, "e": 0x65, "r": 0x72, "c": 0x63, "t": 0x74,
    "space": 0x20, "shift": 0xE0043,
    "escape": 0x1B, "tab": 0x09, "f3": 0x7002D, "f5": 0x7002F,
    "1": 0x31, "2": 0x32, "3": 0x33, "4": 0x34, "5": 0x35,
    "6": 0x36, "7": 0x37, "8": 0x38, "9": 0x39,
}

# The author's button vocabulary -> the SDL button.
BUTTONS = {"left": 0x01, "right": 0x02, "middle": 0x04}

# The example's timeline (the design's contract's "simple version": the
# spawn area's look-around + the mine hold + the inventory + the end's
# third person). The seed = the QUIET gate config's world seed (the
# familiar terrain).
TIMELINE = {
    "world": {"type": "fresh", "seed": 123456789},
    "actions": [
        {"at_frame": 10, "action": "hold", "key": "w", "until_frame": 60},
        {"at_frame": 65, "action": "mouse_down", "button": "left"},
        {"at_frame": 68, "action": "mouse_move", "dx": 30, "dy": 200},
        {"at_frame": 210, "action": "mouse_up", "button": "left"},
        {"at_frame": 215, "action": "key_down", "key": "e"},
        {"at_frame": 220, "action": "key_up", "key": "e"},
        {"at_frame": 240, "action": "key_down", "key": "e"},
        {"at_frame": 245, "action": "key_up", "key": "e"},
        {"at_frame": 250, "action": "mouse_down", "button": "left"},
        {"at_frame": 255, "action": "key_down", "key": "c"},
        {"at_frame": 260, "action": "key_up", "key": "c"},
        {"at_frame": 262, "action": "mouse_up", "button": "left"},
        {"at_frame": 265, "action": "wheel", "ticks": 1},
    ],
}


def read_version():
    """The TM_VERSION single source (CMakeLists' project VERSION)."""
    with open("CMakeLists.txt", "r", encoding="utf-8") as f:
        m = re.search(r"project\(TokenMine VERSION ([0-9]+\.[0-9]+\.[0-9]+)", f.read())
    if not m:
        sys.exit("cannot read the project VERSION from CMakeLists.txt")
    return m.group(1)


def expand(world, actions, version):
    """The authorDemoFile twin (the rules' mirror, see the header)."""
    frames = 0
    for a in actions:
        frames = max(frames, a["at_frame"] + 1)
        if a["action"] == "hold" and a.get("until_frame") is not None:
            frames = max(frames, a["until_frame"] + 1)
    dts = [1.0 / 60.0] * frames
    evs = []  # (frame, action_index, part, event)

    def key_event(frame, part, is_down):
        # part 0 = the press (the key_down / the hold's first half),
        # part 1 = the release (the key_up / the hold's second half).
        return {"frame": frame,
                "type": "key_down" if is_down else "key_up",
                "sdl_key": KEYCODES[a["key"]]}

    for i, a in enumerate(actions):
        at = a["at_frame"]
        act = a["action"]
        if act in ("key_down", "key_up", "hold"):
            evs.append((at, i, 0, key_event(at, 0, act != "key_up")))
            if act == "hold" and a.get("until_frame") is not None:
                evs.append((a["until_frame"], i, 1,
                            key_event(a["until_frame"], 1, False)))
        elif act in ("mouse_down", "mouse_up"):
            evs.append((at, i, 0, {"frame": at, "type": act,
                                   "button": BUTTONS[a["button"]]}))
        elif act == "mouse_move":
            evs.append((at, i, 0, {"frame": at, "type": "mouse_move",
                                   "dx": a["dx"], "dy": a["dy"],
                                   "x": 0, "y": 0}))
        elif act == "wheel":
            evs.append((at, i, 0, {"frame": at, "type": "wheel",
                                   "ticks": a["ticks"]}))
        else:
            sys.exit("unknown action " + act)
    evs.sort(key=lambda t: (t[0], t[1], t[2]))
    return {
        "version": version,
        "world": world,
        "dts": dts,
        "events": [e for *_, e in evs],
    }


def gzip_container(payload):
    """The V3-7 deterministic container (the hand-built, see header)."""
    co = zlib.compressobj(6, zlib.DEFLATED, -15, 8)
    body = co.compress(payload) + co.flush()
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    return GZIP_HEADER + body + struct.pack("<II", crc, len(payload) & 0xFFFFFFFF)


def main():
    import os
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo)
    version = read_version()
    demo = expand(TIMELINE["world"], TIMELINE["actions"], version)
    payload = json.dumps(demo, separators=(",", ":")).encode("utf-8")
    blob = gzip_container(payload)
    os.makedirs("assets/demos", exist_ok=True)
    with open("assets/demos/example.dmo", "wb") as f:
        f.write(blob)
    with open("assets/demos/example_timeline.json", "w", encoding="utf-8") as f:
        json.dump(TIMELINE, f, indent=2)
        f.write("\n")
    print("wrote assets/demos/example.dmo (%d bytes, %d frames, %d events)"
          % (len(blob), len(demo["dts"]), len(demo["events"])))
    print("wrote assets/demos/example_timeline.json")


if __name__ == "__main__":
    main()
