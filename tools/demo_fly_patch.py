#!/usr/bin/env python3
"""V3-16d: the dmo's fly-toggle patcher (the gamepad's B's the
deterministic's the fly's ON/OFF's the insertion's).

WHY: the Space's double-tap's the fly's toggle's the wall-clock's window
(the 0.3-s's, the nowSeconds's the solo's read's — the sim's NOT): a
frame's stall (the 120-s's auto-save's write's, the startup's streaming's
burst's) straddling the tap's pair stretches the interval past the window
= the flake (the PITS 416's gate-catch's root). The gamepad's B
(SDL's GAMEPAD_BUTTON_EAST's the mapGamepadButton's the FlyToggle's —
the V3-8b's dedicated's toggle: a single press flips, NO window, NO jump
grant) is the sim-frame's deterministic's twin. But the AUTHOR's timeline
vocabulary is keyboard/mouse (the record's face's the gamepad's the
C++'s reject's — demo.cpp's "gamepad actions are the record's face"), so
the B's taps land by POST-PATCH: the builder emits the timeline (the
Space's holds' the movement's kept) + a SIDE-CAR of toggle frames, and
this tool inserts the gamepad_button's events into the authored dmo's.

The container's byte-parity (the determinism's first-class's the re-
author's must reproduce the committed dmo's bytes): the game's gzipEncode
= the pinned 10-B header (save.hpp's kGzipHeader) + the RAW deflate
(windowBits -15, level kSaveGzipLevel 6, memLevel 8, Z_DEFAULT_STRATEGY)
+ the CRC32/ISIZE trailer (the RFC 1952's little-endian's). Python's
zlib (the same system library 1.3.2) matches at level 6 / memLevel 8.
The JSON parity: the game's writer sorts keys + the compact separators +
the integral doubles' int notation + the shortest-round-trip's the rest —
normalize() mirrors it. The no-op self-check (zero toggles = byte-
identical output) is asserted on EVERY run (the parity's the live's
guard: a zlib / JSON drift's the loud's fail's, not the silent's).

Usage:
    python3 tools/demo_fly_patch.py BASE.dmo TOGGLES.json OUT.dmo
    python3 tools/demo_fly_patch.py BASE.dmo OUT.dmo            # no-op check
TOGGLES.json = [frame, ...] (the B's tap's frames' the builder's side-car).
"""
import json
import sys
import zlib

# The pinned 10-B container (save.hpp's kGzipHeader's the byte's twin):
# 1f 8b (magic) | 08 (CM deflate) | 00 (FLG) | 00*4 (MTIME) | 00 (XFL)
# | 03 (OS Unix).
K_GZIP_HEADER = bytes([0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x03])
K_LEVEL = 6      # save.cpp's kSaveGzipLevel
K_MEM_LEVEL = 8  # save.cpp's kSaveGzipMemLevel (the zlib's default's twin)
# The demo's synthetic gamepad (demo.hpp's kDemoSyntheticGamepadWhich) +
# the B (the SDL's GAMEPAD_BUTTON_EAST's the 1's — mapGamepadButton's
# FlyToggle's).
K_WHICH = 1
K_BUTTON_B = 1
K_2POW53 = 9007199254740992  # the game's writer's integral's bound's twin


def normalize(v):
    """The game's Json's number's notation's twin: the integral double's
    (|v| < 2^53's) the int's, the rest's the shortest-round-trip's (the
    json's repr's the %.*g's loop's the twin on this value's domain)."""
    if isinstance(v, bool):
        return v
    if isinstance(v, float):
        if v.is_integer() and abs(v) < K_2POW53:
            return int(v)
        return v
    if isinstance(v, list):
        return [normalize(x) for x in v]
    if isinstance(v, dict):
        return {k: normalize(x) for k, x in v.items()}
    return v


def gzip_encode(payload):
    co = zlib.compressobj(K_LEVEL, zlib.DEFLATED, -15, K_MEM_LEVEL, 0)
    body = co.compress(payload) + co.flush()
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    return (K_GZIP_HEADER + body +
            crc.to_bytes(4, "little") +
            (len(payload) & 0xFFFFFFFF).to_bytes(4, "little"))


def dump_json(obj):
    # The game's dump(-1)'s twin: the sorted keys' + the compact's + the
    # ASCII's (the dmo's strings' all the ASCII's — recorded_with / scene /
    # the type's names').
    return json.dumps(normalize(obj), sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True).encode("ascii")


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    base = sys.argv[1]
    out = sys.argv[3] if len(sys.argv) == 4 else sys.argv[2]
    toggles = []
    if len(sys.argv) == 4:
        toggles = sorted(json.load(open(sys.argv[2])))

    raw = open(base, "rb").read()
    doc = json.loads(_ungzip(raw))

    if not toggles:
        # The no-op self-check (the parity's the live's guard): the
        # re-dump's + the re-gzip's must reproduce the input's bytes'
        # EXACTLY (the JSON's format's + the zlib's version's the twin's
        # the loud's assert's).
        re = gzip_encode(dump_json(doc))
        if re != raw:
            for i, (a, b) in enumerate(zip(raw, re)):
                if a != b:
                    sys.exit(f"NO-OP PARITY FAIL: byte {i} "
                             f"{a:02x} != {b:02x} (len {len(raw)} vs "
                             f"{len(re)})")
            sys.exit(f"NO-OP PARITY FAIL: length {len(raw)} vs {len(re)}")
        if out != base:
            open(out, "wb").write(re)
        print(f"no-op: {base} byte-identical ({len(raw)} B)")
        return

    evs = doc["events"]
    for f in toggles:
        if not isinstance(f, int) or f < 0:
            sys.exit(f"toggle frame {f!r}: an integer >= 0 required")
        down = {"frame": f, "type": "gamepad_button", "which": K_WHICH,
                "button": K_BUTTON_B, "down": True}
        up = {"frame": f + 1, "type": "gamepad_button", "which": K_WHICH,
              "button": K_BUTTON_B, "down": False}
        # The base's file order = (frame, ...)'s the stable-sort's —
        # each event's inserted at its OWN frame's position (a shared
        # index would put the up's before the same-frame's no-op's,
        # the unsorted's the replay's frameBegin's break's).
        i = next((i for i, e in enumerate(evs) if e["frame"] >= f),
                 len(evs))
        evs.insert(i, down)
        j = next((j for j, e in enumerate(evs) if e["frame"] >= f + 1),
                 len(evs))
        evs.insert(j, up)
    # The dts' length's unchanged (the events' the frame's bound's inside —
    # the toggle's frames' the movement's holds' the same's).
    payload = dump_json(doc)
    open(out, "wb").write(gzip_encode(payload))
    print(f"patched: {len(toggles)} B tap(s) = {2 * len(toggles)} "
          f"gamepad events -> {out} "
          f"({len(gzip_encode(payload))} B, {len(evs)} events)")


def _ungzip(raw):
    # The decode's twin (the save's reader's the probe's + the raw's the
    # inflate's): the header's 10 B's the raw body's the trailer's 8 B's.
    assert raw[:2] == b"\x1f\x8b", "the gzip's magic's the absent's"
    body = zlib.decompress(raw[10:-8], -15)
    crc = int.from_bytes(raw[-8:-4], "little")
    size = int.from_bytes(raw[-4:], "little")
    assert zlib.crc32(body) & 0xFFFFFFFF == crc, "the CRC's the broken's"
    assert len(body) == size, "the ISIZE's the broken's"
    return body


if __name__ == "__main__":
    main()
