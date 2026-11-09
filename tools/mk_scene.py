#!/usr/bin/env python3
"""V3-16a: the guide's SCENE surgery.

A scene = a prepared start-point for a guide demo (the committed asset
assets/demos/scenes/<name>.json): a FRESH world run (no input, the
config's seed) + a SPEC's mutations (the player's state / the time /
block journal entries) re-serialized in the game's deterministic gzip
container (the PITS 385 recipe: the pinned 10-B header + the raw
deflate 6 + the CRC32/ISIZE trailer — byte-reproducible).

Usage:
  python3 tools/mk_scene.py <spec.json> [--out assets/demos/scenes/<name>.json]

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
   "blocks": [[x, y, z, "name"], ...]}

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
        out_path = os.path.join(ROOT, 'assets', 'demos', 'scenes',
                                name + '.json')
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
