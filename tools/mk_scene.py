#!/usr/bin/env python3
"""V3-16a: the guide's SCENE surgery.

A scene = a prepared start-point for a guide demo (the committed asset
assets/demos/scenes/<name>.json.gz): a FRESH world run (no input, the
config's seed) + a SPEC's mutations (the player's state / the time /
block journal entries) re-serialized in the game's deterministic gzip
container (the PITS 385 recipe: the pinned 10-B header + the raw
deflate 6 + the CRC32/ISIZE trailer — byte-reproducible).

Usage:
  python3 tools/mk_scene.py <spec.json> [--out assets/demos/scenes/<name>.json.gz]

The spec (the closed face):
  {"name": "pilot_mine",
   "config": "path/to/config.json",
   "base_frames": 90,
   "time": 0.25,
   "player": {"pos": [x, y, z], "yaw": 0.0, "pitch": -0.15,
              "health": 20.0, "hunger": 20.0, "saturation": 0.0,
              "respawn": [x, y, z],
              "inventory": {"selected": 0, "slots": [[slot, item, count], ...]},
              "inv_grid": []},
   "clear_entities": true,
   "blocks": [[x, y, z, "name"], ...],
   "fluids": [[x, y, z, level, "water"|"lava"], ...],
   "campfires": [[x, y, z], ...]}

The base run's slot is __scene_tmp (overwritten each run — the scene
is the source of truth, never the base's slot).
"""
import gzip
import json
import os
import re
import struct
import subprocess
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, 'build', 'bin', 'tokenmine')
GZIP_HEADER = b'\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03'
TMP_SLOT = '__scene_tmp'


def gzip_pin(payload):
    co = zlib.compressobj(6, zlib.DEFLATED, -15)
    body = co.compress(payload) + co.flush()
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    return GZIP_HEADER + body + struct.pack('<II', crc, len(payload) & 0xFFFFFFFF)


def load_save(path):
    data = open(path, 'rb').read()
    if data[:2] == b'\x1f\x8b':
        data = gzip.decompress(data)
    return json.loads(data)


def apply_spec(doc, spec):
    if 'time' in spec:
        doc['time'] = spec['time']
    if 'player' in spec:
        p = doc['players'][0]
        pl = spec['player']
        for k in ('pos', 'yaw', 'pitch', 'health', 'hunger', 'saturation',
                  'respawn', 'inventory', 'inv_grid', 'selected',
                  'creative'):
            if k in pl:
                if k == 'selected':
                    p['inventory']['selected'] = pl['selected']
                else:
                    p[k] = pl[k]
    if 'clear_entities' in spec and spec['clear_entities']:
        # The item drops' clear ONLY (the fires stay: a fire block
        # owns its 'fires' life entry — the integrity check's twin;
        # the far-cave fire's deterministic tick is the demo's dt's
        # face, never the wall clock).
        doc['entities'] = []
    if 'blocks' in spec:
        for e in spec['blocks']:
            doc['blocks'].append([int(e[0]), int(e[1]), int(e[2]), e[3]])
    # V3-16c: the container states' passthrough (the table's block
    # cross-check: every crafting_table owns a tables entry — the
    # scene's prepared table's twin).
    if 'tables' in spec:
        doc['tables'] = spec['tables']
    if 'furnaces' in spec:
        doc['furnaces'] = spec['furnaces']
    # V3-16f: the fluid journal passthrough (the pool's prepared water —
    # the PITS 390 sealed form): entries [x, y, z, level, "kind"]
    # (level 0 = the drain). The base worldgen's journal cells are
    # authoritative — a spec collision is a loud reject.
    if 'fluids' in spec:
        fls = doc.setdefault('fluids', [])
        base_cells = {(e[0], e[1], e[2]) for e in fls}
        for i, e in enumerate(spec['fluids']):
            if len(e) != 5:
                raise SystemExit('scene %s: fluids entry #%d must be '
                                 '[x, y, z, level, "kind"]' % (name, i))
            x, y, z, level, kind = e
            if not all(isinstance(v, int) for v in (x, y, z, level)):
                raise SystemExit('scene %s: fluids entry #%d = integers'
                                 % (name, i))
            if kind not in ('water', 'lava'):
                raise SystemExit('scene %s: fluids entry #%d unknown kind'
                                 ' %r' % (name, i, kind))
            if not 0 <= level <= 8:
                raise SystemExit('scene %s: fluids entry #%d level %d out'
                                 ' of [0, 8]' % (name, i, level))
            if (x, y, z) in base_cells:
                raise SystemExit('scene %s: fluids entry #%d at (%d,%d,%d)'
                                 ' collides with the base worldgen'
                                 ' journal' % (name, i, x, y, z))
            fls.append([x, y, z, level, kind])
    # V3-16f: the prepared campfires (lit): the four-slot
    # cooking-state entry (the format 13 cross-check: every
    # campfire-family block owns a state entry). The block journal
    # entry is the spec's own (a campfires entry without a "campfire"
    # block at the cell desyncs; a second block edit at the cell is
    # the load's loud duplicate reject — the probe A's capture).
    if 'campfires' in spec:
        cfs = doc.setdefault('campfires', [])
        have = {tuple(c['pos']) for c in cfs}
        block_cells = {(e[0], e[1], e[2]): e[3] for e in doc['blocks']}
        for i, p in enumerate(spec['campfires']):
            if len(p) != 3 or not all(isinstance(v, int) for v in p):
                raise SystemExit('scene %s: campfires entry #%d must be'
                                 ' [x, y, z]' % (name, i))
            if tuple(p) in have:
                raise SystemExit('scene %s: campfire at %s duplicate'
                                 % (name, p))
            if block_cells.get(tuple(p)) != 'campfire':
                raise SystemExit('scene %s: campfire at %s requires a'
                                 ' "campfire" block at the cell in the'
                                 ' spec blocks' % (name, p))
            cfs.append({'pos': list(p),
                        'slots': [{'item': '', 'count': 0,
                                   'progress_substeps': 0}] * 4})
    return doc


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    spec_path = sys.argv[1]
    out_path = None
    if '--out' in sys.argv:
        out_path = sys.argv[sys.argv.index('--out') + 1]
    spec = json.load(open(spec_path))
    name = spec['name']
    cfg = spec.get('config', 'config/config.json')
    cfg = cfg if os.path.isabs(cfg) else os.path.join(ROOT, cfg)

    # 1) the base world (the fresh run, no input).
    base = os.path.join(ROOT, 'data', 'saves', TMP_SLOT + '.json')
    subprocess.run(
        [BIN, '--config', cfg, '--frames', str(spec.get('base_frames', 90)),
         '--new', '--save', TMP_SLOT],
        cwd=ROOT, check=True, capture_output=True)
    doc = load_save(base)

    # 2) the mutations.
    doc = apply_spec(doc, spec)

    # 3) the deterministic re-serialize (the game's compact face: the
    #    alphabetical keys, the shortest-round-trip numbers).
    payload = json.dumps(doc, sort_keys=True, separators=(',', ':'),
                         ensure_ascii=False).encode('utf-8')
    if out_path is None:
        # V3-18: the scene's extension rides the save family's rename
        # (the .json.gz — the gzip container's visible extension; the
        # game's two-stage find still reads a legacy .json).
        out_path = os.path.join(ROOT, 'assets', 'demos', 'scenes',
                                name + '.json.gz')
    out_path = out_path if os.path.isabs(out_path) else os.path.join(ROOT, out_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(gzip_pin(payload))
    # the idempotence pin (a second run is byte-identical)
    with open(out_path + '.idem', 'wb') as f:
        f.write(gzip_pin(payload))
    a = open(out_path, 'rb').read()
    b = open(out_path + '.idem', 'rb').read()
    os.remove(out_path + '.idem')
    print('scene %s: %d B (payload %d B, %d block edits, idempotent %s)'
          % (name, len(a), len(payload), len(doc['blocks']),
             a == b))
    return 0


if __name__ == '__main__':
    sys.exit(main())
