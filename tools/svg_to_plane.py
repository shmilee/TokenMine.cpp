#!/usr/bin/env python3
"""Lay an existing flat 2D SVG onto a 3D axonometric plate (hero-style).

The input SVG's content is wrapped in the 2:1 dimetric face matrix
matrix(1,0.5,-1,0.5,tx,ty) (diagram-design type-exploded "content flat on a
face"), so the diagram sits on the top face of a plate, tilted with it.
Arrows/markers transform with the group, so geometry stays correct.

Usage (from repo root):
  python3 tools/svg_to_plane.py docs/frame-pipeline.svg --dark --scale 0.55
  python3 tools/svg_to_plane.py docs/architecture.svg --scale 0.8 --out x.svg

Options:
  --scale s   content scale (default 0.55). Plate is sized to the content:
              W = cw·s + 24, D = ch·s + 24.
  --dark      remap the light-skin tokens to the dark skin (plate + content)
  --margin m  extra margin around the content on the plate (default 24)
  --out path  output svg (default: <input-stem>-plate.svg next to the input)

Honest limits:
- Readability caps density: at scale 0.55, 8px sublabels become ~4.4px.
  Good for a single-plate showcase / README hero; for dense reading keep
  scale >= 0.7 or shrink the source diagram first.
- Whole-diagram wrapping only (no per-node 3D, no skin reflow).
"""
import argparse
import math
import re
import sys
from pathlib import Path

# skins: (paper, top, left, right, silhouette, inner-edge)
LIGHT = dict(paper="#f5f5f5", top="#ffffff", left="#f0f1f2", right="#e0e0e3",
             sil="#2d3142", inner="rgba(45,49,66,0.55)")
DARK = dict(paper="#2d3142", top="#4d5164", left="#393e53", right="#33384a",
            sil="#f5f5f5", inner="rgba(245,245,245,0.45)")

DARK_REMAP = {
    "#f5f5f5": "#4d5164",   # paper / label masks -> plate top face
    "#ececec": "#393e53",   # paper-2
    "#ffffff": "#2d3142",   # white node fills -> dark nodes
    "#2d3142": "#f5f5f5",   # ink -> light text
    "#4f5d75": "#bfc0c0",
    "#5a6580": "#949eb2",
    "#bf4520": "#f08a59",
    "rgba(45,49,66,": "rgba(245,245,245,",
    "rgba(191,69,32,": "rgba(240,138,89,",
}

FONTS_IMPORT = ("@import url('https://fonts.googleapis.com/css2?"
               "family=Instrument+Serif:ital@0;1&amp;family=Geist:wght@400;500;600"
               "&amp;family=Geist+Mono:wght@400;500;600&amp;family=Noto+Serif:ital@0;1"
               "&amp;family=Noto+Serif+SC:wght@400;600&amp;family=Noto+Sans+SC:wght@400;500;600"
               "&amp;display=swap');")

T, R = 22, 14  # plate thickness / corner radius


def remap_dark(s: str) -> str:
    keys = sorted(DARK_REMAP, key=len, reverse=True)
    pat = re.compile("(" + "|".join(re.escape(k) for k in keys) + ")")
    return pat.sub(lambda m: DARK_REMAP[m.group(1)], s)


def extract(text: str):
    """Return (inner_markup, content_w, content_h)."""
    vb = re.search(r'viewBox="([^"]+)"', text)
    parts = vb.group(1).split() if vb else []
    cw, ch = (float(parts[2]), float(parts[3])) if len(parts) == 4 else (None, None)
    m = re.search(r"<svg[^>]*>(.*)</svg>", text, re.DOTALL)
    if not m:
        sys.exit("error: no <svg> element in input")
    inner = m.group(1)

    def _strip_bg(rm):
        # the plate supplies the surface: drop full-canvas background rects
        # (attribute-order agnostic, width/height may be % or absolute)
        tag = rm.group(0)
        w = re.search(r'width="([^"]+)"', tag)
        h = re.search(r'height="([^"]+)"', tag)
        if not (w and h and cw and ch):
            return tag

        def frac(v, ref):
            v = v.strip()
            return float(v[:-1]) / 100.0 if v.endswith("%") else float(v) / ref

        try:
            full = frac(w.group(1), cw) >= 0.99 and frac(h.group(1), ch) >= 0.99
        except ValueError:
            return tag
        return "" if full else tag

    inner = re.sub(r'<rect\b[^>]*/>', _strip_bg, inner)
    return inner, cw, ch


N_ARC = 12


def outline(w, d, r):
    """Same decomposition as generate_architecture_hero.py (midpoint = exact 45°)."""
    pts = []

    def arc(cx, cy, a0, a1):
        for i in range(1, N_ARC + 1):
            a = a0 + (a1 - a0) * i / N_ARC
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))

    d2 = math.radians
    pts.append((r, 0))
    pts.append((w - r, 0))
    br = len(pts)
    arc(w - r, r, d2(-90), d2(0))
    ir = br + N_ARC // 2
    pts.append((w, d - r))
    fr = len(pts)
    arc(w - r, d - r, d2(0), d2(90))
    ifr = fr + N_ARC // 2
    pts.append((r, d))
    fl = len(pts)
    arc(r, d - r, d2(90), d2(180))
    il = fl + N_ARC // 2
    pts.append((0, r))
    arc(r, r, d2(180), d2(270))
    return pts, ir, ifr, il


def plate(w, d, skin, ox, oy):
    """Slab at z 0..T (hero iso convention: screen_y = oy + (x+y)/2 - z, so the
    bottom face z=0 sits BELOW the top face z=T; top-face back corner is at
    screen (ox, oy - T))."""
    pts, ir, ifr, il = outline(w, d, R)
    n = len(pts)

    def P(i, z):
        x, y = pts[i % n]
        return (ox + x - y, oy + (x + y) / 2 - z)

    def pf(sp, close=True):
        s = "M " + f"{sp[0][0]:.2f} {sp[0][1]:.2f}" + "".join(" L " + f"{p[0]:.2f} {p[1]:.2f}" for p in sp[1:])
        return s + (" Z" if close else "")

    top_half = []
    i = il
    while True:
        top_half.append(P(i, T))
        if i == ir:
            break
        i -= 1
        if i < 0:
            i = n - 1
    bot_half = [P(i, 0) for i in range(ir, il + 1)]
    sil = pf(top_half + [P(ir, 0)] + bot_half)
    # visible side faces: top outline extreme→extreme, drop, bottom outline back
    faceA = [P(i, T) for i in range(il, ifr - 1, -1)] + [P(ifr, 0)] + \
            [P(i, 0) for i in range(ifr, il + 1)]
    faceB = [P(i, T) for i in range(ifr, ir - 1, -1)] + [P(ir, 0)] + \
            [P(i, 0) for i in range(ir, ifr + 1)]
    top = [P(i, T) for i in range(n)]
    return "\n".join([
        f'<path d="{pf(faceA)}" fill="{skin["left"]}"/>',
        f'<path d="{pf(faceB)}" fill="{skin["right"]}"/>',
        f'<path d="{pf(top)}" fill="{skin["top"]}"/>',
        f'<path d="{pf(top)}" fill="none" stroke="{skin["inner"]}" stroke-width="0.8"/>',
        f'<line x1="{top[ifr][0]:.2f}" y1="{top[ifr][1]:.2f}" x2="{P(ifr, 0)[0]:.2f}" y2="{P(ifr, 0)[1]:.2f}" stroke="{skin["inner"]}" stroke-width="0.8"/>',
        f'<path d="{sil}" fill="none" stroke="{skin["sil"]}" stroke-width="1.2"/>',
    ])


def main():
    ap = argparse.ArgumentParser(description="Wrap a flat 2D SVG onto a 3D axonometric plate.")
    ap.add_argument("input")
    ap.add_argument("--scale", type=float, default=0.55)
    ap.add_argument("--dark", action="store_true")
    ap.add_argument("--margin", type=float, default=24.0)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    text = Path(args.input).read_text()
    inner, cw, ch = extract(text)
    if cw is None:
        sys.exit("error: input svg has no 4-value viewBox")
    s, m = args.scale, args.margin
    if args.dark:
        inner = remap_dark(inner)
    skin = DARK if args.dark else LIGHT

    W, D = round(cw * s + m), round(ch * s + m)
    u0, v0 = m / 2, m / 2
    OX = 40 + D
    TOP_Y = 40.0  # hero-convention oy: top face back corner lands at (OX, TOP_Y - T)
    VBW = W + D + 80
    VBH = (W + D) / 2 + T + 80

    title = Path(args.input).stem
    out = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {VBW} {VBH}" role="img" aria-labelledby="plate-title plate-desc">
  <title id="plate-title">{title}（轴测板）</title>
  <desc id="plate-desc">Flat diagram "{title}" laid on a 3D axonometric plate via the 2:1 dimetric face matrix.</desc>
  <defs>
    <style>{FONTS_IMPORT}</style>
    <!-- restore the input svg's viewport clipping (local coords of the content group) -->
    <clipPath id="vp"><rect x="0" y="0" width="{cw}" height="{ch}"/></clipPath>
  </defs>
  <rect width="100%" height="100%" fill="{skin['paper']}"/>
{plate(W, D, skin, OX, TOP_Y)}
  <g transform="matrix(1,0.5,-1,0.5,{OX + u0 - v0},{TOP_Y + (u0 + v0) / 2 - T}) scale({s})" clip-path="url(#vp)">{inner}</g>
</svg>
'''
    out_path = args.out or str(Path(args.input).with_name(Path(args.input).stem + "-plate.svg"))
    Path(out_path).write_text(out)
    print(f"wrote {out_path}  (plate {W}×{D}, content {cw}×{ch} @ {s})")


if __name__ == "__main__":
    main()
