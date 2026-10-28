#!/usr/bin/env python3
"""Restyle a flat light-skin SVG diagram to the dark hero skin (standalone).

Single-pass token remap (light → dark) so the #f5f5f5 <-> #2d3142 swap is safe
(each occurrence is replaced exactly once; the result is not re-scanned). The
color set matches the architecture-hero dark skin, for a cohesive README.

Usage (from repo root):
  python3 tools/svg_restyle.py docs/architecture.svg              # in place
  python3 tools/svg_restyle.py docs/x.svg --out docs/x-dark.svg
"""
import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

# standalone dark skin (diagram-design style-guide "Default (dark)") — paper
# stays the dark BACKGROUND here (unlike svg_to_plane.py, where the plate
# supplies the surface). Node fills go to paper-2 (#393e53), NOT paper.
DARK = {
    "#f5f5f5": "#2d3142",   # paper     -> dark paper (background / label masks)
    "#2d3142": "#f5f5f5",   # ink       -> light text / strokes / zones
    "#ffffff": "#393e53",   # node fill -> paper-2 (SKILL dark node surface)
    "#4f5d75": "#bfc0c0",   # muted     -> silver (arrows / secondary text)
    "#5a6580": "#949eb2",   # soft      -> light soft (sublabels)
    "#bf4520": "#f08a59",   # accent    -> coral (focal)
}


def restyle(s: str) -> str:
    keys = sorted(DARK, key=len, reverse=True)
    pat = re.compile("(" + "|".join(re.escape(k) for k in keys) + ")")
    return pat.sub(lambda m: DARK[m.group(1)], s)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input")
    ap.add_argument("--out", default=None, help="output path (default: in place)")
    args = ap.parse_args()
    text = Path(args.input).read_text()
    out_path = args.out or args.input
    Path(out_path).write_text(restyle(text))
    ET.parse(out_path)  # verify well-formed
    print(f"restyled {out_path} -> dark skin (well-formed)")


if __name__ == "__main__":
    main()
