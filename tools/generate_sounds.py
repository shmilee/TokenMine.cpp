#!/usr/bin/env python3
"""Generate the game's sound effects (16-bit mono 44.1 kHz WAVs) for
Phase 15.

Deterministic: every sample comes from fixed-phase sine waves and a
seeded LCG noise source, so the output is stable across runs and
platforms. Re-run after editing this file to refresh assets/sounds/.
Re-running after ANY edit must leave the pre-existing files byte-
identical (the pure functions carry their own fixed seeds — verify with
md5sum before/after).

The generic set (one dig/place/break sound for all blocks without a
sound_material — the block property, CRAFT-3) + the material set
(<material>_dig/_place/_break.wav, a material is a complete three-piece
vocabulary; blocks name one via `sound_material`):
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
  ignite.wav   crackling burst (flint and steel, Phase 60)
  -- CRAFT-3 materials (6 x 3 events; the timbres are distinct by design:
     stone = hard, brittle, bright / wood = mid knock / gravel = dry
     rattle / sand = low shush / cloth = soft rustle / glass = high bell)
  stone_dig.wav  bright sharp tap (no lowpass = the hard, brittle face)
  stone_place.wav hard thud 170 Hz + click
  stone_break.wav brittle shatter (bright noise + 2200 Hz ping)
  wood_dig.wav   mid knock (lowpassed noise + 240 Hz tone)
  wood_place.wav knock thud 150 Hz + noise
  wood_break.wav crack (lowpassed burst + 300 -> 200 Hz wobble)
  gravel_dig.wav dry rattle (two interleaved noise layers)
  gravel_place.wav rattle thud (noise + 100 Hz)
  gravel_break.wav long tumble (slow-decay noise)
  sand_dig.wav   low shush (double lowpass)
  sand_place.wav muffled low thud 75 Hz (double lowpass)
  sand_break.wav low crumble (slow decay, double lowpass)
  cloth_dig.wav  soft quiet rustle (double lowpass)
  cloth_place.wav muffled soft thud 95 Hz (double lowpass)
  cloth_break.wav soft tear rustle (double lowpass)
  glass_dig.wav  high tinkle (2600 Hz bell + bright tick)
  glass_place.wav clear click + 1900 Hz bell
  glass_break.wav bright shatter (no lowpass + 3200 Hz ring)
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


def ignite():
    """Fire ignition (Phase 60, flint and steel on a flammable block):
    a short CRACKLING burst — bright noise with a fast exponential
    decay over a high descending pop, pitched higher than the water
    splash (the same lowpass chain as the other noise taps)."""
    lcg = Lcg(0xF1DE)
    dur = 0.18

    def f(t, _):
        tone = math.sin(2 * math.pi * (920.0 - 560.0 * (t / dur)) * t) * 0.12
        return (lcg.next() * 0.72 + tone) * env(t, dur, 30.0) * 0.7
    return lowpass3(sound(dur, f))


# ---------------------------------------------------------------------------
# CRAFT-3: the material sounds. Each material is a complete three-piece
# vocabulary (<material>_dig / _place / _break) — the block's optional
# `sound_material` property names one; blocks without it keep the generic
# dig / place / break above. The timbres are distinct by design within the
# fixed LCG / sine paradigm:
#   stone   = hard, brittle, bright (NO lowpass on the noise faces)
#   wood    = a mid-range knock (lowpassed noise + a ~200 Hz tone)
#   gravel  = a dry rattle (two decorrelated noise layers)
#   sand    = a low shush (double lowpass, slow decay)
#   cloth   = a soft rustle (double lowpass, quiet, longer release)
#   glass   = a high bell (bright noise + a 1900-3200 Hz ring)
# Every LCG seed is fixed (and unique to its sound) — the determinism
# discipline of the generic set.
def _noise(seed: int, dur: float) -> list:
    return [Lcg(seed).next() for _ in range(int(RATE * dur))]


def stone_dig():
    """Bright sharp tap: raw noise (no lowpass = the brittle face), the
    fastest decay of the dig family (harder than the generic dig)."""
    noise = _noise(0x5709E, 0.08)
    return [noise[i] * env(i / RATE, 0.08, 45.0) * 0.55 for i in range(len(noise))]


def stone_place():
    """Hard thud: a 170 Hz strike (higher than the generic 110 Hz) + a
    bright click layer."""
    lcg = Lcg(0x57107)

    def f(t, _):
        return (math.sin(2 * math.pi * 170.0 * t) * math.exp(-t * 45) * 0.55
                + lcg.next() * math.exp(-t * 90) * 0.35)
    return sound(0.08, f)


def stone_break():
    """Brittle shatter: a fast-decay bright noise burst + a 2200 Hz ping
    (the crack flash) + a gritty tick layer."""
    noise = _noise(0x5739B, 0.16)
    lcg = Lcg(0x5739C)
    out = []
    for i in range(len(noise)):
        t = i / RATE
        out.append(noise[i] * math.exp(-t * 50) * 0.6
                   + math.sin(2 * math.pi * 2200.0 * t) * math.exp(-t * 60) * 0.25
                   + lcg.next() * math.exp(-t * 80) * 0.15)
    return out


def wood_dig():
    """Mid knock: lowpassed noise under a decaying 240 Hz tone (the hollow
    log tap — warmer than the generic dig)."""
    noise = _noise(0x57001, 0.09)
    out = [noise[i] * env(i / RATE, 0.09, 30.0) * 0.35
           + math.sin(2 * math.pi * 240.0 * (i / RATE))
           * math.exp(-(i / RATE) * 45) * 0.25
           for i in range(len(noise))]
    return lowpass3(out)


def wood_place():
    """Knock thud: a 150 Hz strike + a lowpassed noise body."""
    lcg = Lcg(0x57110)

    def f(t, _):
        return (math.sin(2 * math.pi * 150.0 * t) * math.exp(-t * 38) * 0.6
                + lcg.next() * math.exp(-t * 55) * 0.25)
    return lowpass3(sound(0.10, f))


def wood_break():
    """Crack: a lowpassed noise burst (decay 20) + a 300 -> 200 Hz wobble
    (the splitting grain)."""
    noise = _noise(0x57221, 0.16)
    out = [noise[i] * env(i / RATE, 0.16, 20.0) * 0.55
           + math.sin(2 * math.pi * (300.0 - 150.0 * (i / RATE) / 0.16)
                      * (i / RATE)) * math.exp(-(i / RATE) * 25) * 0.2
           for i in range(len(noise))]
    return lowpass3(out)


def gravel_dig():
    """Dry rattle: two decorrelated noise layers (70/30) — the tumbling
    grains; no lowpass (brighter than sand, grittier than stone)."""
    a = _noise(0x6701A, 0.10)
    b = _noise(0x6701B, 0.10)
    return [(a[i] * 0.7 + b[i] * 0.3) * env(i / RATE, 0.10, 28.0) * 0.5
            for i in range(len(a))]


def gravel_place():
    """Rattle thud: a noise burst under a 100 Hz body."""
    lcg = Lcg(0x67220)

    def f(t, _):
        return (lcg.next() * math.exp(-t * 40) * 0.5
                + math.sin(2 * math.pi * 100.0 * t) * math.exp(-t * 35) * 0.35)
    return sound(0.10, f)


def gravel_break():
    """Long tumble: the slowest noise decay of the break family (0.22 s,
    decay 12) — the grains settling."""
    noise = _noise(0x67333, 0.22)
    out = [noise[i] * env(i / RATE, 0.22, 12.0) * 0.6 for i in range(len(noise))]
    return lowpass3(out)


def sand_dig():
    """Low shush: quiet noise (0.45), decay 18, DOUBLE lowpass (the soft
    sliding grains — the lowest face of the dig family)."""
    noise = _noise(0x73001, 0.12)
    out = [noise[i] * env(i / RATE, 0.12, 18.0) * 0.45 for i in range(len(noise))]
    return lowpass3(lowpass3(out))


def sand_place():
    """Muffled low thud: a 75 Hz body (the lowest place strike) + a double
    lowpassed noise."""
    lcg = Lcg(0x73111)

    def f(t, _):
        return (math.sin(2 * math.pi * 75.0 * t) * math.exp(-t * 30) * 0.55
                + lcg.next() * math.exp(-t * 45) * 0.3)
    return lowpass3(lowpass3(sound(0.12, f)))


def sand_break():
    """Low crumble: decay 10 over 0.20 s, double lowpass — the pile
    giving way quietly."""
    noise = _noise(0x73222, 0.20)
    out = [noise[i] * env(i / RATE, 0.20, 10.0) * 0.6 for i in range(len(noise))]
    return lowpass3(lowpass3(out))


def cloth_dig():
    """Soft quiet rustle: the quietest dig (0.32), decay 15, double
    lowpass — handled wool, not mined stone."""
    noise = _noise(0xC1001, 0.12)
    out = [noise[i] * env(i / RATE, 0.12, 15.0) * 0.32 for i in range(len(noise))]
    return lowpass3(lowpass3(out))


def cloth_place():
    """Muffled soft thud: a 95 Hz body + a quiet double lowpassed noise
    (the bundle landing)."""
    lcg = Lcg(0xC1112)

    def f(t, _):
        return (math.sin(2 * math.pi * 95.0 * t) * math.exp(-t * 22) * 0.4
                + lcg.next() * math.exp(-t * 35) * 0.3)
    return lowpass3(lowpass3(sound(0.12, f)))


def cloth_break():
    """Soft tear rustle: decay 12, 0.15 s, double lowpass, quiet (0.35) —
    the wool parting, not cracking."""
    noise = _noise(0xC1223, 0.15)
    out = [noise[i] * env(i / RATE, 0.15, 12.0) * 0.35 for i in range(len(noise))]
    return lowpass3(lowpass3(out))


def glass_dig():
    """High tinkle: a 2600 Hz bell (decay 35) + a bright tick layer — the
    fingernail on the pane (the highest face of the dig family)."""
    lcg = Lcg(0x61001)

    def f(t, _):
        return (math.sin(2 * math.pi * 2600.0 * t) * math.exp(-t * 35) * 0.4
                + lcg.next() * math.exp(-t * 70) * 0.15)
    return sound(0.08, f)


def glass_place():
    """Clear click + bell: a 1900 Hz ring (decay 28) over a short bright
    click."""
    lcg = Lcg(0x61111)

    def f(t, _):
        return (math.sin(2 * math.pi * 1900.0 * t) * math.exp(-t * 28) * 0.45
                + lcg.next() * math.exp(-t * 90) * 0.25)
    return sound(0.10, f)


def glass_break():
    """Bright shatter: RAW noise (no lowpass — the sharp shards), fast
    decay 40, over a 3200 Hz ring (decay 25)."""
    noise = _noise(0x61222, 0.18)
    out = [noise[i] * math.exp(-(i / RATE) * 40) * 0.55
           + math.sin(2 * math.pi * 3200.0 * (i / RATE))
           * math.exp(-(i / RATE) * 25) * 0.3
           for i in range(len(noise))]
    return out


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
    "ignite": ignite,
    # CRAFT-3: the six material vocabularies (3 events each).
    "stone_dig": stone_dig,
    "stone_place": stone_place,
    "stone_break": stone_break,
    "wood_dig": wood_dig,
    "wood_place": wood_place,
    "wood_break": wood_break,
    "gravel_dig": gravel_dig,
    "gravel_place": gravel_place,
    "gravel_break": gravel_break,
    "sand_dig": sand_dig,
    "sand_place": sand_place,
    "sand_break": sand_break,
    "cloth_dig": cloth_dig,
    "cloth_place": cloth_place,
    "cloth_break": cloth_break,
    "glass_dig": glass_dig,
    "glass_place": glass_place,
    "glass_break": glass_break,
}


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in SOUNDS.items():
        write_wav(os.path.join(OUT_DIR, name + ".wav"), fn())
        print(f"wrote {OUT_DIR}/{name}.wav")


if __name__ == "__main__":
    main()
