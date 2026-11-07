#!/usr/bin/env python3
"""E2E comparison helpers (rebuilt after the /tmp wipe, gate 139).

Usage:
  g117.py raw  A.png B.png          byte-exact PNG comparison
  g117.py norm A.save B.save        savenorm comparison (real-dt fields
                                    stripped: fires[*].remaining,
                                    entities[*].age, player.saturation —
                                    the PITS 244 / P56 discipline)

V3-12c2 (gate 221): the cross-VERSION savenorm (the performance A/B's
old-format-32 vs new-format-33) normalizes the schema's relocation:
  - the format key itself (the 32 vs 33's marker)
  - the per-player "players" array (format 33) -> the root "player"
    (format 32's shape): the primary's entry promoted, its nested
    "inventory" / "inv_grid" to the root, the respawn's exact float
    triple floored to the 32's int cell (the old writer's twin)
  - "end_xp_entities" (format 33's always-written segment; the 32's
    silent-skip deviation, empty on the no-input overworld's arm)
"""
import gzip
import json
import math
import sys


def load_save(path):
    """Load a save file, transparently gunzipping the V3-7 gzip encoding
    (the magic probe — the sole encoding criterion; a legacy plaintext
    file reads straight through, zero migration)."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] == b'\x1f\x8b':
        data = gzip.decompress(data)
    return json.loads(data)


def cmp_raw(a, b):
    da = open(a, 'rb').read()
    db = open(b, 'rb').read()
    if da == db:
        print(f'EXACT {a} \u2261 {b} ({len(da)} bytes)')
        return 0
    print(f'MISMATCH bytes {len(da)} vs {len(db)}')
    return 1


def strip_save(d):
    for f in d.get('fires', []):
        f.pop('remaining', None)
    for e in d.get('entities', []):
        e.pop('age', None)
    for e in d.get('nether_entities', []):
        e.pop('age', None)
    for e in d.get('end_entities', []):
        e.pop('age', None)
    for e in d.get('end_xp_entities', []):
        e.pop('age', None)
    # V3-12c2: the format-33 per-player relocation -> the format-32's
    # root shape (the cross-version's normalization: the primary's
    # entry promoted, the nested inventory / inv_grid to the root, the
    # respawn's exact float floored to the 32's int cell).
    players = d.get('players')
    if players:
        primary = dict(players[0])
        if 'inventory' in primary:
            d['inventory'] = primary.pop('inventory')
        if 'inv_grid' in primary:
            d['inv_grid'] = primary.pop('inv_grid')
        if 'respawn' in primary and isinstance(primary['respawn'], list):
            primary['respawn'] = [int(math.floor(v)) for v in primary['respawn']]
        d['player'] = primary
        del d['players']
    d.get('player', {}).pop('saturation', None)
    # V3-12c2: the format-33's always-written End's XP segment (the
    # 32's silent-skip deviation; empty on the no-input overworld).
    d.pop('end_xp_entities', None)
    # V3-12c2: the format key itself (the 32 vs 33's marker).
    d.pop('format', None)
    return d


def diff(a, b, path=''):
    out = []
    if type(a) != type(b):
        return [f'{path}: type {type(a).__name__} vs {type(b).__name__}']
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a:
                out.append(f'{path}.{k}: missing in A')
            elif k not in b:
                out.append(f'{path}.{k}: missing in B')
            else:
                out.extend(diff(a[k], b[k], f'{path}.{k}'))
    elif isinstance(a, list):
        if len(a) != len(b):
            out.append(f'{path}: len {len(a)} vs {len(b)}')
        for i in range(min(len(a), len(b))):
            out.extend(diff(a[i], b[i], f'{path}[{i}]'))
    else:
        if a != b:
            out.append(f'{path}: {a!r} vs {b!r}')
    return out


def cmp_norm(a, b):
    da = strip_save(load_save(a))
    db = strip_save(load_save(b))
    d = diff(da, db)
    if not d:
        print(f'EXACT {a} \u2261 {b} (savenorm)')
        return 0
    print(f'MISMATCH {len(d)} diffs (first 15):')
    for line in d[:15]:
        print('  ' + line)
    return 1


if __name__ == '__main__':
    mode, a, b = sys.argv[1], sys.argv[2], sys.argv[3]
    sys.exit(cmp_raw(a, b) if mode == 'raw' else cmp_norm(a, b))
