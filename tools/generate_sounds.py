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
   -- PRE-4 ambience (3 beds; quiet by design — the ambient sound sits
      UNDER the SFX; each a distinct fixed-seed LCG/sine composition,
      2-4 s long, slow attack + slow release envelope):
   ambience_cave.wav  low cave hum (~55 Hz + 110 Hz overtone, 0.4 Hz
                      wobble) + four LCG-placed falling water drips
                      (900 -> 300 Hz)
   ambience_day.wav   day birds: three LCG-placed chirp bursts (2-4 notes
                      of 2.4-4.2 kHz trills) over a double-lowpassed
                      wind bed
   ambience_night.wav crickets: five LCG-placed bursts of ~4.3 kHz 25 Hz
                      pulse trills over a quiet, slow night-wind bed
-- V2-4 weather (1 one-shot; the rain sound loop is the documented
      absence — the project's sound set has no looping-sound precedent):
   thunder.wav        rolling thunder (a 60 -> 40 Hz glide + a fixed-seed
                      double-lowpassed LCG rumble body, slow attack +
                      long decay over the ~3.5 s roll) — the distant
                      thunder one-shot (2D centered, never spatialized)
-- V2-13 bed (1 one-shot; the sleep action's cue — spatialized at the
      bed, the block-center pattern):
   sleep.wav           a soft low "sigh" (a 180 -> 90 Hz exponential glide
                      under a quiet 360 Hz partial, slow attack/release,
                      tone-only — the hushed bedroom feel)
-- V2-14 bat (1 one-shot; the ambient squeak — the flyCruise per-frame
      roll plays it spatialized at the bat, the dropped-item pattern):
   squeak.wav          a high short cave chirp (a 900 -> 1400 Hz rising
                      glide with a slight 30 Hz tremolo, 0.12 s, in the
                      0.1 ambient volume class — the Minecraft ambient
                      volume 0.1 reference, a quiet noise whisper under
                      the tone)
-- V2-15 sheep (1 one-shot; the shearing cue — played spatialized at
      the mob, the milk precedent):
   shear.wav           the short double "snick" (a bright fixed-seed
                      noise snap — the blades catching — then a muffled
                      lowpassed tap 55 ms later — the wool parting,
                      0.18 s, the SFX volume class)
 -- V2-16 snow (the 7th sound-material family — the snow block's dig /
      place / break, the sand-family paradigm one step softer: powder,
      not grains — the double lowpass, the quiet amplitudes):
   snow_dig.wav        soft shush (double lowpass, the lightest dig face)
   snow_place.wav      muffled soft plop 85 Hz (double lowpass)
   snow_break.wav      quiet powder crumble (double lowpass, slow decay)
-- V3-1 Phase A nether portal (1 ambient):
   portal_ambient.wav  the low portal hum (Minecraft's block.portal.
                       ambient class — a ~110 Hz body with a 5.2 Hz
                       swirl tremolo + a quiet 220 Hz overtone, under a
                       fixed-seed lowpassed noise grit, 1.2 s, in the
                       0.5 ambient volume class — the sample's master
                       bakes the volume, the squeak's 0.1-class twin x5):
                       played spatialized at the visible portal cell on
                       the per-frame roll (p = 1 / (2 s x 60))
-- V3-1 Phase B nether portal (2 one-shots; the travel pipeline's
      presentation cues — the Minecraft block.portal.trigger / travel
      pair, the design's D6 step 6):
   portal_trigger.wav  the entry cue (Minecraft's block.portal.trigger
                       class — the short vortex swish as the player
                       steps in and the countdown arms): a 140 -> 420 Hz
                       rising glide (the pull-in) under a fixed-seed
                       double-lowpassed noise whoosh, 0.35 s, in the
                       SFX volume class (the splash / bow precedent),
                       played spatialized at the portal cell
   portal_travel.wav   the travel cue (Minecraft's block.portal.travel
                       class — the fading-out vortex as the dimension
                       switch completes, the "fade" of the design's
                       D6): a 320 -> 90 Hz exponential descending glide
                       (the let-go) with a 6 Hz swirl tremolo under a
                       fixed-seed double-lowpassed noise decay, 1.0 s,
                       in the 0.5 ambient volume class (the
                       portal_ambient's twin — the master amplitude
                       bakes the volume), played 2D centered (the
                       switch is global, not a point in space — the
                       thunder / ambience precedent)
 -- V3-7 Part 2 nether deepening (11 files: the three material families
       + the evaporation cue + the nether ambience bed):
    nether_dig.wav      muffled low dig (double lowpass + 65 Hz body)
    nether_place.wav    muffled low thud 60 Hz (double lowpass)
    nether_break.wav    crisp break (lowpassed burst + 900 Hz ping)
    nylium_dig.wav      soft wet dig (double lowpass + 90 -> 60 Hz wobble)
    nylium_place.wav    wet low plop 55 Hz (double lowpass)
    nylium_break.wav    soft squelch (double lowpass, slow decay)
    stem_dig.wav        mid woody knock (lowpassed noise + 200 Hz tone)
    stem_place.wav      woody thud 130 Hz + noise
    stem_break.wav      woody crisp break (lowpassed burst + 350 -> 250
                        Hz wobble)
    evaporate.wav       the nether's water evaporation cue (hiss +
                        descending bubble tone, 0.4 s, SFX class)
    ambience_nether.wav the nether ambience bed (48 Hz rumble + distant
                        lava cracks, 2.0 s, 0.5 ambient class)
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


def amb_env(t: float, dur: float, attack=0.15, release=0.3) -> float:
    """PRE-4 ambience envelope: a slow linear attack + a slow linear
    release (the bed-like fade an ambient sound needs — no clicks at
    either edge, no exponential decay mid-bed)."""
    e = 1.0
    if t < attack:
        e *= t / attack
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
    # V2-16 follow-up repair (round 2): the legacy body rebuilt the Lcg
    # per sample (`[Lcg(0xD160).next() for ...]` — a fresh instance on
    # every iteration), so `noise` was a CONSTANT stream — and with a
    # negative constant, lowpass3's max(0.0, ...) clamp left the shipped
    # dig.wav SILENT (peak 0). One advancing instance (the _noise / snow
    # family paradigm, the adjacent place()'s pattern): the docstring's
    # short filtered-noise tap, actually noise now.
    lcg = Lcg(0xD160)
    noise = [lcg.next() for _ in range(int(RATE * 0.10))]
    out = [noise[i] * env(i / RATE, 0.10, 30.0) * 0.5 for i in range(len(noise))]
    return lowpass3(out)


def place():
    lcg = Lcg(0x7A1CE)

    def f(t, _):
        return (math.sin(2 * math.pi * 110 * t) * math.exp(-t * 40) * 0.6
                + lcg.next() * math.exp(-t * 90) * 0.2)
    return sound(0.09, f)


def break_sfx():
    # V2-16 follow-up repair (round 2): the legacy body rebuilt the Lcg
    # per sample (same defect as dig(), above) — the shipped break.wav
    # was SILENT (peak 0). One advancing instance: the docstring's
    # crumbly noise burst, actually noise now.
    lcg = Lcg(0xBEEF)
    noise = [lcg.next() for _ in range(int(RATE * 0.18))]
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


def smelt():
    """The smelt-complete ding (CRAFT-5, the furnace's product-done
    cue): a short two-partial bell — a bright 1320 Hz strike over a
    660 Hz body tone, both fast-decay, with a quiet LCG tick on the
    attack (the fixed-LCG / sine paradigm, dedicated seed)."""
    lcg = Lcg(0x51DE)
    dur = 0.35

    def f(t, _):
        tick = lcg.next() * math.exp(-t * 60) * 0.15
        bell = (math.sin(2 * math.pi * 1320.0 * t) * math.exp(-t * 14) * 0.35
                + math.sin(2 * math.pi * 660.0 * t) * math.exp(-t * 9) * 0.45)
        return (bell + tick) * env(t, dur, 9.0) * 0.8
    return sound(dur, f)


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
    # V2-16 follow-up repair: the legacy body rebuilt the Lcg per sample
    # (`[Lcg(seed).next() for ...]` — a fresh instance on every iteration),
    # so it returned a CONSTANT stream (the seed's first value, repeated).
    # Downstream that constant was either a texture-free decaying DC ramp
    # (the no-lowpass consumers) or, when the constant was NEGATIVE,
    # clamped to SILENCE by lowpass3's max(0.0, ...) — the shipped
    # sand_break / gravel_break files were exactly that (peak 0). The fix
    # is the single-advancing-instance paradigm (the sleep / squeak /
    # shear / snow precedent): same signature, same length, one genuine
    # noise stream. The eleven _noise consumers' bytes intentionally
    # change (degenerate -> the docstrings' intended noise); every other
    # file stays byte-identical. (The generic dig / break inlined the
    # same legacy pattern and were repaired the same way in round 2 of
    # the follow-up — their bytes change there, not here.)
    lcg = Lcg(seed)
    return [lcg.next() for _ in range(int(RATE * dur))]


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


# ---------------------------------------------------------------------------
# V2-16: the snow material (the 7th sound family — the snow block). The
# sand-family paradigm ONE STEP SOFTER (powder, not grains): the double
# lowpass, the quiet amplitudes, the lightest decays. Fixed unique seeds
# (the determinism discipline — a re-run leaves every other file
# byte-identical).
#
# NOTE: the noise layers use a SINGLE advancing Lcg instance — the
# paradigm the `_noise` helper was meant to carry (the helper's legacy
# per-sample rebuild returned a constant stream; it was repaired in the
# V2-16 follow-up, and this family matches that fix).
def snow_dig():
    """Soft shush: the lightest dig face (0.40, decay 22, double lowpass)
    — the sliding powder (the sand_dig paradigm, one step quieter)."""
    lcg = Lcg(0x75001)
    out = [lcg.next() * env(i / RATE, 0.11, 22.0) * 0.40
           for i in range(int(RATE * 0.11))]
    return lowpass3(lowpass3(out))


def snow_place():
    """Muffled soft plop: an 85 Hz body (between the sand's 75 and the
    gravel's 100 — the powder settling) + a double lowpassed noise."""
    lcg = Lcg(0x75111)

    def f(t, _):
        return (math.sin(2 * math.pi * 85.0 * t) * math.exp(-t * 28) * 0.5
                + lcg.next() * math.exp(-t * 45) * 0.28)
    return lowpass3(lowpass3(sound(0.12, f)))


def snow_break():
    """Quiet powder crumble: decay 14 over 0.18 s, double lowpass — the
    pile giving way silently (the sand_break paradigm, one step softer)."""
    lcg = Lcg(0x75222)
    out = [lcg.next() * env(i / RATE, 0.18, 14.0) * 0.5
           for i in range(int(RATE * 0.18))]
    return lowpass3(lowpass3(out))


# ---------------------------------------------------------------------------
# PRE-4: the ambience beds. Each is a fixed-seed LCG/sine composition
# (the generic set's determinism discipline — a re-run leaves the file
# byte-identical), quiet by design (the ambient sound sits UNDER the SFX:
# peak amplitudes ~0.15 vs ~0.6 for the effect taps), 2-4 s long, and
# faded in/out with amb_env (no clicks). The placements (drip / chirp /
# cricket burst start times) come from the per-sound LCG — spread evenly
# with jitter, so the rhythm is irregular (the Minecraft "about every
# other window" feel is already handled by the game-side 0.5 roll).
def ambience_cave():
    """Low cave hum + sparse water drips (3.0 s): a ~55 Hz body with a
    slow 0.4 Hz wobble + a quiet 110 Hz overtone, under four LCG-placed
    falling drip blips (900 -> 300 Hz, fast decay). The quietest bed —
    the enclosed-space feel."""
    lcg = Lcg(0xA7E1)
    dur = 3.0
    # Four drips spread over [0.35, 2.65] with jitter (the LCG decides).
    drips = []
    for i in range(4):
        u = lcg.next() * 0.5 + 0.5
        drips.append(0.35 + 0.60 * ((i + u) / 4.0))

    def f(t, _):
        hum = (math.sin(2 * math.pi * 55.0 * t)
               * (0.7 + 0.3 * math.sin(2 * math.pi * 0.4 * t))
               + math.sin(2 * math.pi * 110.0 * t) * 0.25)
        out = hum * 0.16
        for t0 in drips:
            u = t - t0
            if 0.0 <= u < 0.18:
                fr = 900.0 - 3600.0 * (u / 0.18)  # 900 -> 300 Hz glide
                out += math.sin(2 * math.pi * fr * u) * math.exp(-u * 28.0) * 0.14
        return out * amb_env(t, dur)
    return sound(dur, f)


def ambience_day():
    """Day birds + wind (3.0 s): three LCG-placed chirp bursts (each 2-4
    notes of 2.4-4.2 kHz trills) over a double-lowpassed wind bed (the
    same noise chain as the other sound taps, a slow 0.3 Hz swell)."""
    lcg = Lcg(0xD1A2)
    dur = 3.0
    n = int(RATE * dur)
    wind = lowpass3(lowpass3([lcg.next() for _ in range(n)]))
    # Three bursts spread over [0.35, 2.55], each 2-4 notes 0.07 s apart.
    bursts = []
    for b in range(3):
        u0 = lcg.next() * 0.5 + 0.5
        b0 = 0.35 + (2.2 * (b + u0)) / 3.0
        notes = []
        for k in range(2 + int(lcg.next() * 3)):  # 2-4 notes per burst
            u1 = lcg.next() * 0.5 + 0.5
            notes.append((b0 + k * 0.07, 2400.0 + 1800.0 * u1))
        bursts.append(notes)

    def f(t, _):
        i = int(t * RATE)
        out = wind[i] * 0.10 * (0.6 + 0.4 * math.sin(2 * math.pi * 0.3 * t))
        for notes in bursts:
            for t0, fr in notes:
                u = t - t0
                if 0.0 <= u < 0.06:
                    out += (math.sin(2 * math.pi * fr * u)
                            * math.exp(-u * 55.0) * 0.11)
        return out * amb_env(t, dur)
    return sound(dur, f)


def ambience_night():
    """Crickets + night wind (3.5 s): five LCG-placed bursts of ~4.3 kHz
    trills pulsed at 25 Hz (the sharpened sine gate), over the quietest
    bed — a double-lowpassed wind with a slow 0.2 Hz swell."""
    lcg = Lcg(0x0B17)
    dur = 3.5
    n = int(RATE * dur)
    wind = lowpass3(lowpass3([lcg.next() for _ in range(n)]))
    # Five bursts spread over [0.3, 3.0], each 0.30 s long.
    bursts = []
    for b in range(5):
        u = lcg.next() * 0.5 + 0.5
        bursts.append(0.30 + (2.7 * (b + u)) / 5.0)

    def f(t, _):
        i = int(t * RATE)
        out = wind[i] * 0.06 * (0.6 + 0.4 * math.sin(2 * math.pi * 0.2 * t))
        for t0 in bursts:
            u = t - t0
            if 0.0 <= u < 0.30:
                gate = max(0.0, math.sin(2 * math.pi * 25.0 * u)) ** 3
                out += (math.sin(2 * math.pi * 4300.0 * u)
                        * gate * 0.09)
        return out * amb_env(t, dur)
    return sound(dur, f)


# ---------------------------------------------------------------------------
# V2-4: the weather thunder. The distant-thunder one-shot (2D centered,
# never spatialized — the design decision): a 60 -> 40 Hz glide (the
# 40-60 Hz low body) under a fixed-seed double-lowpassed LCG rumble, all
# in a slow-attack + long-decay envelope over the ~3.5 s roll. The
# determinism discipline of the other sounds (the fixed seed; a re-run
# leaves the file byte-identical — the existing wavs' md5s unchanged).
def sleep():
    """V2-13: the sleep sound (a successful night-skip on the bed — the
    right-click action's cue). A soft, low sine "sigh" with a slow decay
    (the hushed bedroom feel): a 180 -> 90 Hz glide under a quiet second
    partial, no noise layer (the cloth rustle is the bed's place/break
    material sound — the sleep cue itself is tone-only). The fixed-LCG /
    sine paradigm (the smelt-ding precedent), dedicated seed."""
    lcg = Lcg(0x51EE)  # dedicated fixed seed (unique to this sound)
    dur = 0.40

    def f(t, _):
        fr = 180.0 * (90.0 / 180.0) ** (t / dur)  # exponential glide down
        tone = (math.sin(2 * math.pi * fr * t) * 0.55
                + math.sin(2 * math.pi * 360.0 * t) * math.exp(-t * 10) * 0.15)
        tick = lcg.next() * math.exp(-t * 40) * 0.06
        return (tone + tick) * amb_env(t, dur, 0.03, 0.15) * 0.7
    return sound(dur, f)


def thunder():
    lcg = Lcg(0x70DE)  # dedicated fixed seed (unique to this sound)
    dur = 3.5
    n = int(RATE * dur)
    noise = lowpass3(lowpass3([lcg.next() for _ in range(n)]))

    def f(t, _):
        i = int(t * RATE)
        fr = 60.0 - 20.0 * (t / dur)  # 60 -> 40 Hz glide (the low body)
        roll = math.sin(2 * math.pi * fr * t) * 0.55
        body = roll + noise[i] * 0.65
        return body * math.exp(-t * 1.4) * amb_env(t, dur, 0.08, 0.6) * 0.9

    return sound(dur, f)


def shear():
    """V2-15: the shears' cut (the sheep shearing — the right-click
    action's cue, played spatialized at the mob, the milk precedent):
    the short double "snick" (Minecraft's shears.shear class) — a
    bright noise snap (the blades catching) at the head, then a muffled
    lowpassed tap (the wool parting) 55 ms later, 0.18 s, in the SFX
    volume class. The fixed-LCG noise paradigm (the cloth_break
    precedent), dedicated seed."""
    lcg = Lcg(0x5B4A)  # dedicated fixed seed (unique to this sound)
    dur = 0.18
    n = int(RATE * dur)
    raw = [lcg.next() for _ in range(n)]
    lp = lowpass3(lowpass3(raw))  # the muffled parting layer
    out = []
    for i in range(n):
        t = i / RATE
        s = 0.0
        if t < 0.05:
            s += raw[i] * math.exp(-t * 55.0) * 0.55  # the bright catch
        u = t - 0.055
        if 0.0 <= u < 0.125:
            s += lp[i] * math.exp(-u * 26.0) * 0.5  # the wool parting
        out.append(s * 0.9)
    return out


def squeak():
    """V2-14: the bat's ambient squeak (Minecraft's bat ambience — the
    high, short cave chirp): a 900 -> 1400 Hz rising glide with a slight
    30 Hz tremolo, 0.12 s, in the 0.1 ambient volume class (the
    Minecraft ambient volume 0.1 reference — quieter than the SFX taps,
    the ambience beds' class), over a fixed-seed noise whisper (the
    chirp's grit). The fixed-LCG / sine paradigm (the sleep precedent),
    dedicated seed."""
    lcg = Lcg(0xB441)  # dedicated fixed seed (unique to this sound)
    dur = 0.12

    def f(t, _):
        fr = 900.0 + 500.0 * (t / dur)  # 900 -> 1400 Hz rising glide
        tremolo = 1.0 - 0.2 * math.sin(2 * math.pi * 30.0 * t)
        tone = math.sin(2 * math.pi * fr * t) * tremolo
        noise = lcg.next() * 0.15
        return (tone + noise) * env(t, dur, 22.0) * 0.1
    return sound(dur, f)


def portal_ambient():
    """V3-1 Phase A: the nether portal's ambience (Minecraft's
    block.portal.ambient class — the low humming vortex under the
    visible portal): a ~110 Hz body with a 5.2 Hz swirl tremolo + a
    quiet 220 Hz overtone, under a fixed-seed double-lowpassed noise
    grit (the vortex's texture), 1.2 s, in the 0.5 ambient volume class
    (the master amplitude bakes the volume — the squeak's 0.1 class one
    step up: the portal hum is louder than a bat, quieter than SFX).
    The fixed-LCG / sine paradigm (the ambience beds' precedent),
    dedicated seed — a re-run leaves every other file byte-identical."""
    lcg = Lcg(0xC4F1)  # dedicated fixed seed (unique to this sound)
    dur = 1.2
    n = int(RATE * dur)
    grit = lowpass3(lowpass3([lcg.next() for _ in range(n)]))

    def f(t, _):
        i = int(t * RATE)
        swirl = 0.65 + 0.35 * math.sin(2 * math.pi * 5.2 * t)
        body = (math.sin(2 * math.pi * 110.0 * t) * 0.55
                + math.sin(2 * math.pi * 220.0 * t) * 0.18) * swirl
        return (body + grit[i] * 0.22) * amb_env(t, dur, 0.12, 0.35) * 0.5
    return sound(dur, f)


def portal_trigger():
    """V3-1 Phase B: the portal entry cue (Minecraft's block.portal.
    trigger class — the short vortex swish as the player steps in and
    the countdown arms): a 140 -> 420 Hz rising glide (the pull-in)
    under a fixed-seed double-lowpassed noise whoosh, 0.35 s, in the
    SFX volume class (the splash / bow precedent). The fixed-LCG /
    sine paradigm, dedicated seed — a re-run leaves every other file
    byte-identical."""
    lcg = Lcg(0xC5A2)  # dedicated fixed seed (unique to this sound)
    dur = 0.35
    n = int(RATE * dur)
    noise = lowpass3(lowpass3([lcg.next() for _ in range(n)]))

    def f(t, _):
        i = int(t * RATE)
        fr = 140.0 + 280.0 * (t / dur)  # the pull-in glide
        tone = math.sin(2 * math.pi * fr * t) * 0.4
        return (tone + noise[i] * 0.5) * env(t, dur, 6.0) * 0.8
    return sound(dur, f)


def portal_travel():
    """V3-1 Phase B: the portal travel cue (Minecraft's block.portal.
    travel class — the fading-out vortex as the dimension switch
    completes): a 320 -> 90 Hz exponential descending glide (the
    let-go) with a 6 Hz swirl tremolo under a fixed-seed
    double-lowpassed noise decay, 1.0 s, in the 0.5 ambient volume
    class (the portal_ambient's twin — the master amplitude bakes the
    volume; the travel sound is 2D centered, the thunder precedent).
    The fixed-LCG / sine paradigm, dedicated seed — a re-run leaves
    every other file byte-identical."""
    lcg = Lcg(0xC6B3)  # dedicated fixed seed (unique to this sound)
    dur = 1.0
    n = int(RATE * dur)
    noise = lowpass3(lowpass3([lcg.next() for _ in range(n)]))

    def f(t, _):
        i = int(t * RATE)
        fr = 320.0 * (90.0 / 320.0) ** (t / dur)  # exponential glide down
        swirl = 0.7 + 0.3 * math.sin(2 * math.pi * 6.0 * t)
        tone = math.sin(2 * math.pi * fr * t) * 0.5 * swirl
        return (tone + noise[i] * 0.25) * amb_env(t, dur, 0.05, 0.4) * 0.5
    return sound(dur, f)


# ---------------------------------------------------------------------------
# V3-7 (v3.7.0): the nether sound face — the three NEW material families
# (nether / nylium / stem — the block sound_material reassignments, the
# v3.5.0 stone / grass reuse deviation's closure), the water evaporation
# cue (T1) and the nether ambience bed (T4). Every file is a fixed-seed
# LCG / sine composition (the paradigm: a re-run leaves every OTHER file
# byte-identical — the md5 discipline), one advancing LCG instance per
# noise face (the _noise repair's convention).
def nether_dig():
    """Muffled low dig (netherrack / magma block / ancient debris —
    Minecraft: the three share the nether-rock face): double-lowpassed
    noise (the muffled face — the sand paradigm, one step darker) under
    a ~65 Hz body (the hollow knock)."""
    lcg = Lcg(0x8E001)  # dedicated fixed seed (unique to this sound)
    n = int(RATE * 0.09)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, 0.09, 26.0) * 0.50
           + math.sin(2 * math.pi * 65.0 * (i / RATE))
           * math.exp(-(i / RATE) * 40.0) * 0.22
           for i in range(n)]
    return lowpass3(lowpass3(out))


def nether_place():
    """Muffled low thud: a 60 Hz body (the lowest place strike of the
    set) + a double-lowpassed noise."""
    lcg = Lcg(0x8E111)  # dedicated fixed seed (unique to this sound)

    def f(t, _):
        return (math.sin(2 * math.pi * 60.0 * t) * math.exp(-t * 30.0) * 0.55
                + lcg.next() * math.exp(-t * 45.0) * 0.28)
    return lowpass3(lowpass3(sound(0.11, f)))


def nether_break():
    """Crisp break: a fast-decay lowpassed noise burst + a 900 Hz ping
    (the crack flash — the nether rock snaps, it does not shatter
    bright like the stone family)."""
    lcg = Lcg(0x8E222)  # dedicated fixed seed (unique to this sound)
    n = int(RATE * 0.15)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, 0.15, 32.0) * 0.6
           + math.sin(2 * math.pi * 900.0 * (i / RATE))
           * math.exp(-(i / RATE) * 50.0) * 0.22
           for i in range(n)]
    return lowpass3(out)


def nylium_dig():
    """Soft wet dig (crimson / warped nylium — Minecraft's mushroom
    stone, grass-class but muffled): double-lowpassed noise under a
    slow 90 -> 60 Hz wet wobble."""
    lcg = Lcg(0x8F001)  # dedicated fixed seed (unique to this sound)
    n = int(RATE * 0.10)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, 0.10, 22.0) * 0.48
           + math.sin(2 * math.pi * (90.0 - 30.0 * (i / n)) * (i / RATE))
           * math.exp(-(i / RATE) * 30.0) * 0.20
           for i in range(n)]
    return lowpass3(lowpass3(out))


def nylium_place():
    """Wet low plop: a 55 Hz body (below the nether's 60 — the mush
    sets deeper) + a double-lowpassed noise."""
    lcg = Lcg(0x8F111)  # dedicated fixed seed (unique to this sound)

    def f(t, _):
        return (math.sin(2 * math.pi * 55.0 * t) * math.exp(-t * 28.0) * 0.52
                + lcg.next() * math.exp(-t * 42.0) * 0.26)
    return lowpass3(lowpass3(sound(0.12, f)))


def nylium_break():
    """Soft squelch: double-lowpassed noise, the slow 12 decay over
    0.18 s — the mush giving way wetly (the sand_break paradigm, one
    step darker)."""
    lcg = Lcg(0x8F222)  # dedicated fixed seed (unique to this sound)
    n = int(RATE * 0.18)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, 0.18, 12.0) * 0.55
           for i in range(n)]
    return lowpass3(lowpass3(out))


def stem_dig():
    """Mid woody knock (crimson / warped stem — Minecraft's nether
    stem, wood-class but crisp): single-lowpassed noise under a 200 Hz
    hollow-log tone (the wood_dig paradigm, one step brighter)."""
    lcg = Lcg(0x90001)  # dedicated fixed seed (unique to this sound)
    n = int(RATE * 0.08)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, 0.08, 34.0) * 0.38
           + math.sin(2 * math.pi * 200.0 * (i / RATE))
           * math.exp(-(i / RATE) * 50.0) * 0.26
           for i in range(n)]
    return lowpass3(out)


def stem_place():
    """Woody thud: a 130 Hz strike (below the wood's 150 — the stem
    is denser) + a single-lowpassed noise body."""
    lcg = Lcg(0x90111)  # dedicated fixed seed (unique to this sound)

    def f(t, _):
        return (math.sin(2 * math.pi * 130.0 * t) * math.exp(-t * 40.0) * 0.58
                + lcg.next() * math.exp(-t * 60.0) * 0.26)
    return lowpass3(sound(0.09, f))


def stem_break():
    """Woody crisp break: a lowpassed noise burst (decay 35 — snappier
    than the wood's 20) + a 350 -> 250 Hz split wobble (the grain
    parting)."""
    lcg = Lcg(0x90222)  # dedicated fixed seed (unique to this sound)
    n = int(RATE * 0.14)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, 0.14, 35.0) * 0.55
           + math.sin(2 * math.pi * (350.0 - 100.0 * (i / n)) * (i / RATE))
           * math.exp(-(i / RATE) * 30.0) * 0.20
           for i in range(n)]
    return lowpass3(out)


def evaporate():
    """T1 (v3.7.0): the nether's water evaporation cue (the water
    bucket empties — water cannot exist in the nether): a hiss (the
    fixed-seed double-lowpassed noise, the fast exponential decay) over
    a quiet descending bubble tone, 0.40 s, in the SFX volume class
    (the splash / ignite precedent). One advancing LCG instance,
    dedicated fixed seed — a re-run leaves every other file
    byte-identical."""
    lcg = Lcg(0x9151)  # dedicated fixed seed (unique to this sound)
    dur = 0.40
    n = int(RATE * dur)
    noise = [lcg.next() for _ in range(n)]
    out = [noise[i] * env(i / RATE, dur, 14.0) * 0.55
           + math.sin(2 * math.pi * (300.0 - 180.0 * (i / n)) * (i / RATE))
           * math.exp(-(i / RATE) * 10.0) * 0.12
           for i in range(n)]
    return lowpass3(lowpass3(out))


def ambience_nether():
    """T4 (v3.7.0): the nether ambience bed (the fourth context — the
    player in the nether, priority over cave / day / night): a low
    rumble (a 48 Hz body with a slow 0.3 Hz wobble + a quiet 96 Hz
    overtone) over a fixed-seed double-lowpassed rumble bed, with
    three LCG-placed distant lava cracks (400 -> 90 Hz fast glides,
    quiet — the ambience_cave drip precedent's nether twin), 2.0 s,
    faded with amb_env (no clicks), in the 0.5 ambient volume class
    (the portal_ambient precedent — the master amplitude bakes the
    volume). One advancing LCG instance, dedicated fixed seed — a
    re-run leaves every other file byte-identical."""
    lcg = Lcg(0x91A2)  # dedicated fixed seed (unique to this sound)
    dur = 2.0
    n = int(RATE * dur)
    bed = lowpass3(lowpass3([lcg.next() for _ in range(n)]))
    # Three cracks spread over [0.40, 1.60] with jitter (the LCG
    # decides — the ambience_cave placement pattern).
    cracks = []
    for i in range(3):
        u = lcg.next() * 0.5 + 0.5
        cracks.append(0.40 + 0.40 * ((i + u) / 3.0))

    def f(t, _):
        i = int(t * RATE)
        rumble = (math.sin(2 * math.pi * 48.0 * t)
                  * (0.75 + 0.25 * math.sin(2 * math.pi * 0.3 * t))
                  + math.sin(2 * math.pi * 96.0 * t) * 0.20)
        out = rumble * 0.14 + bed[i] * 0.10
        for t0 in cracks:
            u = t - t0
            if 0.0 <= u < 0.16:
                fr = 400.0 - 1875.0 * (u / 0.16)  # 400 -> 90 Hz glide
                out += (math.sin(2 * math.pi * fr * u)
                        * math.exp(-u * 24.0) * 0.12)
        return out * amb_env(t, dur, 0.2, 0.4) * 0.5
    return sound(dur, f)


# ---------------------------------------------------------------------------
# V3-3 (v3.8.0): the nether mob ambient family — the three per-mob
# ambients (the squeak's ground-mob twin: the runAi per-frame roll plays
# them spatialized at the mob). Every file is a fixed-seed LCG / sine
# composition, one advancing LCG instance per noise face, dedicated
# fixed seeds — a re-run leaves every OTHER file byte-identical (the
# md5 discipline).
def hoglin_grunt():
    """V3-3: the hoglin's ambient grunt (Minecraft's entity.hoglin.
    idle class — the low bovine grunt): a 120 -> 70 Hz descending glide
    with a 5 Hz larynx tremolo under a quiet fixed-seed
    double-lowpassed grit, 0.30 s, in the 0.25 ambient volume class
    (the squeak's 0.1 class one step up: the grunt is a bovine, not a
    whisper — the master amplitude bakes the volume)."""
    lcg = Lcg(0x9201)  # dedicated fixed seed (unique to this sound)
    dur = 0.30
    n = int(RATE * dur)
    grit = lowpass3(lowpass3([lcg.next() for _ in range(n)]))

    def f(t, _):
        i = int(t * RATE)
        fr = 120.0 - 50.0 * (t / dur)
        tremolo = 0.75 + 0.25 * math.sin(2 * math.pi * 5.0 * t)
        return (math.sin(2 * math.pi * fr * t) * tremolo * 0.6
                + grit[i] * 0.30) * env(t, dur, 18.0) * 0.25
    return sound(dur, f)


def piglin_whisper():
    """V3-3: the piglin's ambient whisper (Minecraft's entity.piglin.
    idle class — the soft high murmur): a 2100 -> 2600 Hz trill with a
    24 Hz murmur tremolo over a quiet fixed-seed double-lowpassed
    murmur bed, 0.25 s, in the squeak's 0.1 ambient volume class
    (the piglin's whisper sits under the hoglin's grunt)."""
    lcg = Lcg(0x9202)  # dedicated fixed seed (unique to this sound)
    dur = 0.25
    n = int(RATE * dur)
    murmur = lowpass3(lowpass3([lcg.next() for _ in range(n)]))

    def f(t, _):
        i = int(t * RATE)
        fr = 2100.0 + 500.0 * (t / dur)
        trill = 0.7 + 0.3 * math.sin(2 * math.pi * 24.0 * t)
        return (math.sin(2 * math.pi * fr * t) * trill * 0.25
                + murmur[i] * 0.45) * env(t, dur, 16.0) * 0.1
    return sound(dur, f)


def strider_hiss():
    """V3-3: the strider's ambient hiss (the lava-walker's breath —
    the hiss-over-undertone, the evaporate cue's quieter ambient
    twin): a fixed-seed single-lowpassed hiss (brighter than the
    whisper's double-lowpassed murmur) over a quiet 300 -> 150 Hz
    undertone, 0.35 s, in the 0.2 ambient volume class."""
    lcg = Lcg(0x9203)  # dedicated fixed seed (unique to this sound)
    dur = 0.35
    n = int(RATE * dur)
    hiss = lowpass3([lcg.next() for _ in range(n)])

    def f(t, _):
        i = int(t * RATE)
        return (hiss[i] * 0.55
                + math.sin(2 * math.pi * (300.0 - 150.0 * (t / dur)) * t)
                * 0.20) * env(t, dur, 14.0) * 0.2
    return sound(dur, f)


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
    # CRAFT-5: the smelt-complete ding (furnace presentation).
    "smelt": smelt,
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
    # V2-16: the snow material vocabulary (3 events — the sand-family
    # paradigm one step softer; the snow block's sound_material).
    "snow_dig": snow_dig,
    "snow_place": snow_place,
    "snow_break": snow_break,
    # PRE-4: the ambience beds (quiet by design — the ambient sound sits
    # under the SFX; the placements are LCG-decided per sound).
    "ambience_cave": ambience_cave,
    "ambience_day": ambience_day,
    "ambience_night": ambience_night,
    # V2-4: the weather thunder (the distant one-shot — 2D centered,
    # never spatialized; the rain loop is the documented absence).
    "thunder": thunder,
    # V2-13: the bed sleep cue (the successful night-skip — spatialized
    # at the bed, the block-center pattern).
    "sleep": sleep,
    # V2-14: the bat's ambient squeak (the flyCruise per-frame roll —
    # spatialized at the bat, the dropped-item pattern).
    "squeak": squeak,
    # V2-15: the shears' cut (the sheep shearing — the right-click
    # action's cue, spatialized at the mob, the milk precedent).
    "shear": shear,
    # V3-1 Phase A: the nether portal's ambience (the visible-portal
    # per-frame roll, spatialized at the portal cell — the bat squeak's
    # cadence pattern, the 0.5 volume class baked in).
    "portal_ambient": portal_ambient,
    # V3-1 Phase B: the travel pipeline's presentation cues (the
    # Minecraft block.portal.trigger / travel pair): the entry swish
    # (spatialized at the portal cell, the countdown-arm frame) and the
    # travel fade (2D centered, the dimension-switch completion frame).
    "portal_trigger": portal_trigger,
    "portal_travel": portal_travel,
    # V3-7 (v3.7.0): the nether material families (3 x 3 events — the
    # block sound_material reassignments, the v3.5.0 stone / grass reuse
    # deviation's closure: nether = netherrack / magma_block /
    # ancient_debris, nylium = the two nyliums, stem = the two stems).
    "nether_dig": nether_dig,
    "nether_place": nether_place,
    "nether_break": nether_break,
    "nylium_dig": nylium_dig,
    "nylium_place": nylium_place,
    "nylium_break": nylium_break,
    "stem_dig": stem_dig,
    "stem_place": stem_place,
    "stem_break": stem_break,
    # V3-7 (v3.7.0): T1 — the nether's water evaporation cue (the water
    # bucket empties in the nether — no cell touched, the 2D centered
    # path, the splash's twin).
    "evaporate": evaporate,
    # V3-7 (v3.7.0): T4 — the nether ambience bed (the fourth context —
    # the player in the nether, priority over cave / day / night).
    "ambience_nether": ambience_nether,
    # V3-3 (v3.8.0): the nether mob ambient family (the per-mob
    # ambient_sound carriers — the squeak's ground-mob twin; the magma
    # cube is the documented no-ambient).
    "hoglin_grunt": hoglin_grunt,
    "piglin_whisper": piglin_whisper,
    "strider_hiss": strider_hiss,
}


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in SOUNDS.items():
        write_wav(os.path.join(OUT_DIR, name + ".wav"), fn())
        print(f"wrote {OUT_DIR}/{name}.wav")


if __name__ == "__main__":
    main()
