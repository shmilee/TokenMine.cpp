#!/usr/bin/env python3
"""E2E comparison helpers (rebuilt after the /tmp wipe, gate 139).

Usage:
  g117.py raw  A.png B.png          byte-exact PNG comparison
  g117.py norm A.save B.save        savenorm comparison (real-dt fields
                                    stripped: fires[*].remaining,
                                    entities[*].age, player.saturation —
                                    the PITS 244 / P56 discipline)
"""
import gzip
import json
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
    d.get('player', {}).pop('saturation', None)
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
