#!/usr/bin/env python3
"""Generate the game's sound effects (16-bit mono 44.1 kHz WAVs) for
Phase 15.

Deterministic: every sample comes from fixed-phase sine waves and a
seeded LCG noise source, so the output is stable across runs and
platforms. Re-run after editing this file to refresh assets/sounds/.

The set is small and generic by design (one dig/place/break sound for all
blocks, not per-material — see docs/KNOWN_ISSUES). Files:
  dig.wav      short filtered-noise tap (mining tick while breaking)
  place.wav    low thud + click (block placement)
  break.wav    crumbly noise burst (block destroyed)
  pickup.wav   two rising sine blips (item pickup / drop)
  step.wav     very soft low thud (footstep)
  ui_open.wav  short rising sine sweep
  ui_close.wav short falling sine sweep
  craft.wav    two-part sine chime (recipe crafted)
  hurt.wav     falling sawtooth-ish groan (player damaged, Phase 16)
  death.wav    long falling sweep (player died, Phase 16)
  shoot.wav    descending whoosh (arrow release, Phase 21)
  bow.wav      string twang (bow draw start, Phase 24)
  eat.wav      muffled chewing (food eaten, Phase 23)
  cloth.wav    soft rustle (dye recolor, Phase 41)
  milk.wav     liquid glug (milk drink start / cow milking, Phase 43)
  splash.wav   water splash (bucket fill / pour, Phase 47)
"""
import math
import os
import struct

RATE = 44100
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "sounds")


class Lcg:
    """Deterministic 32-bit LCG (fixed seed per sound)."""

    def __init__(self, seed: int):
        self.state = seed & 0xFFFFFFFF

    def next(self) -> float:
        self.state = (self.state * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.state / 0xFFFFFFFF * 2.0 - 1.0


def write_wav(path: str, samples) -> None:
    """samples: list of floats in [-1, 1] -> 16-bit mono PCM WAV."""
    n = len(samples)
    data = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        data += struct.pack("<h", int(v * 32767))
    header = (b"RIFF" + struct.pack("<I", 36 + n * 2) + b"WAVE"
              + b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE,
                                      RATE * 2, 2, 16)
              + b"data" + struct.pack("<I", n * 2))
    with open(path, "wb") as f:
        f.write(header + bytes(data))


def env(t: float, dur: float, decay: float, release=0.004) -> float:
    """Exponential decay with a short release tail (no clicks)."""
    e = math.exp(-decay * t)
    if t > dur - release:
        e *= (dur - t) / release
    return max(0.0, e)


def sound(dur: float, fn) -> list:
    n = int(RATE * dur)
    return [fn(i / RATE, i / n) for i in range(n)]


def lowpass3(x: list) -> list:
    """3-tap moving average (a gentle lowpass for the noise taps)."""
    return [max(0.0, (x[max(0, i - 1)] + x[i] + x[min(len(x) - 1, i + 1)]) / 3.0)
            for i in range(len(x))]


def dig():
    noise = [Lcg(0xD160).next() for _ in range(int(RATE * 0.10))]
    out = [noise[i] * env(i / RATE, 0.10, 30.0) * 0.5 for i in range(len(noise))]
    return lowpass3(out)


def place():
    lcg = Lcg(0x7A1CE)

    def f(t, _):
        return (math.sin(2 * math.pi * 110 * t) * math.exp(-t * 40) * 0.6
                + lcg.next() * math.exp(-t * 90) * 0.2)
    return sound(0.09, f)


def break_sfx():
    noise = [Lcg(0xBEEF).next() for _ in range(int(RATE * 0.18))]
    out = [noise[i] * env(i / RATE, 0.18, 15.0) * 0.65 for i in range(len(noise))]
    return lowpass3(out)


def pickup():
    def f(t, _):
        if t < 0.045:
            return math.sin(2 * math.pi * 660 * t) * math.exp(-t * 30) * 0.4
        u = t - 0.045
        return math.sin(2 * math.pi * 990 * u) * math.exp(-u * 30) * 0.4
    return sound(0.09, f)


def step():
    lcg = Lcg(0x57E7)

    def f(t, _):
        return ((math.sin(2 * math.pi * 85 * t) * math.exp(-t * 55) * 0.5
                 + lcg.next() * math.exp(-t * 70) * 0.15) * 0.7)
    return sound(0.06, f)


def sweep(f0: float, f1: float):
    dur = 0.07

    def f(t, _):
        f = f0 + (f1 - f0) * t / dur
        return math.sin(2 * math.pi * f * t) * math.exp(-t * 25) * 0.3
    return sound(dur, f)


def ui_open():
    return sweep(500, 800)


def ui_close():
    return sweep(800, 500)


def craft():
    def f(t, _):
        return ((math.sin(2 * math.pi * 880 * t)
                 + math.sin(2 * math.pi * 1320 * t) * 0.5)
                * math.exp(-t * 18) * 0.35)
    return sound(0.15, f)


def hurt():
    # A short falling groan: a descending 220->90 Hz sine with a gritty
    # noise layer, quick decay.
    lcg = Lcg(0x2A77)
    dur = 0.18

    def f(t, _):
        fr = 220.0 - 130.0 * (t / dur)
        return (math.sin(2 * math.pi * fr * t) * math.exp(-t * 14) * 0.45
                + lcg.next() * math.exp(-t * 30) * 0.12)
    return sound(dur, f)


def death():
    # A long, mournful falling sweep (440 -> 55 Hz) with a low thud.
    lcg = Lcg(0xD347)
    dur = 0.8

    def f(t, _):
        fr = 440.0 * (55.0 / 440.0) ** (t / dur)  # exponential glide down
        return ((math.sin(2 * math.pi * fr * t) * math.exp(-t * 3.5) * 0.4
                 + math.sin(2 * math.pi * 60 * t) * math.exp(-t * 6) * 0.25
                 + lcg.next() * math.exp(-t * 20) * 0.05))
    return sound(dur, f)


def shoot():
    """Short descending whoosh (the bow's arrow release, Phase 21)."""
    lcg = Lcg(0xB077)

    def f(t, _):
        fl = 700.0 * (1.0 - t / 0.12) + 150.0  # descending whoosh
        return (math.sin(2 * math.pi * fl * t) * 0.35
                + lcg.next() * 0.5) * math.exp(-t * 18)
    return lowpass3(sound(0.12, f))


def bow():
    """String twang (the bow's draw, Phase 24): a short plucked 180 Hz
    sine with a gritty noise layer, fast decay."""
    lcg = Lcg(0xB024)

    def f(t, _):
        return (math.sin(2 * math.pi * 180.0 * t) * math.exp(-t * 22) * 0.5
                + lcg.next() * math.exp(-t * 35) * 0.25)
    return sound(0.15, f)


def eat():
    """Muffled chewing (Phase 23): a lowpassed noise burst gated by a slow
    chew pulse, with a short release so it ends cleanly."""
    lcg = Lcg(0xE47C)
    dur = 0.30

    def f(t, _):
        # A ~8 Hz chew gate (3 chews over the 0.3 s), sharpened.
        chew = 0.5 + 0.5 * math.sin(2 * math.pi * 8.0 * t - math.pi / 2)
        gate = max(0.0, chew) ** 2
        # Low thump under each chew for body.
        thump = math.sin(2 * math.pi * 90.0 * t) * 0.25
        return (lcg.next() * 0.7 + thump) * gate * env(t, dur, 6.0) * 0.8
    return lowpass3(sound(dur, f))


def cloth():
    """Soft cloth rustle (dye recolor, Phase 41): a short, quiet lowpassed
    noise burst — softer than dig (recolored wool, not mined stone)."""
    lcg = Lcg(0xC107)
    dur = 0.10

    def f(t, _):
        return lcg.next() * env(t, dur, 25.0) * 0.35
    return lowpass3(sound(dur, f))


def milk():
    """Liquid glug (milk: drink start + cow milking, Phase 43): two low
    sine blips (the swallowed bubbles) with a quiet lowpassed noise
    splash under them, ending in a clean release."""
    lcg = Lcg(0x0CC0)
    dur = 0.28

    def f(t, _):
        # Two glug pulses: a ~7 Hz gate sharpened to two short bumps.
        gate = max(0.0, math.sin(2 * math.pi * 3.5 * t - math.pi / 2)) ** 3
        # Low, wobbling bubble tone under each glug.
        tone = math.sin(2 * math.pi * (150.0 - 180.0 * t) * t) * 0.5
        # Quiet splash of liquid noise.
        return (tone + lcg.next() * 0.4) * gate * env(t, dur, 8.0) * 0.7
    return lowpass3(sound(dur, f))


def splash():
    """Water splash (Phase 47, bucket fill/pour): a short bright noise
    burst with a quick exponential decay over a soft descending water
    tone — the same lowpass chain as the other noise taps."""
    lcg = Lcg(0xA511)
    dur = 0.22

    def f(t, _):
        tone = math.sin(2 * math.pi * (340.0 - 220.0 * (t / dur)) * t) * 0.18
        return (lcg.next() * 0.5 + tone) * env(t, dur, 22.0) * 0.8
    return lowpass3(sound(dur, f))


SOUNDS = {
    "dig": dig,
    "place": place,
    "break": break_sfx,
    "pickup": pickup,
    "step": step,
    "ui_open": ui_open,
    "ui_close": ui_close,
    "craft": craft,
    "hurt": hurt,
    "death": death,
    "shoot": shoot,
    "bow": bow,
    "eat": eat,
    "cloth": cloth,
    "milk": milk,
    "splash": splash,
}


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in SOUNDS.items():
        write_wav(os.path.join(OUT_DIR, name + ".wav"), fn())
        print(f"wrote {OUT_DIR}/{name}.wav")


if __name__ == "__main__":
    main()
