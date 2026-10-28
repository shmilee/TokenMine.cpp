#!/usr/bin/env python3
"""Generate the exploded-axonometric architecture diagram for TokenMine.cpp.
Projection per skill spec: iso(x,y,z) = (ox + x - y, oy + (x+y)/2 - z).
"""
import math

OX, OY = 320, 560
INK = (45, 49, 66)
ACC = (191, 69, 32)
WHITE = (255, 255, 255)

def mix(base, over, f):
    return tuple(round(base[i] + (over[i] - base[i]) * f) for i in range(3))

def hx(c):
    return '#%02x%02x%02x' % c

def iso(x, y, z):
    return (OX + x - y, OY + (x + y) / 2.0 - z)

def fmt(p):
    return f"{p[0]:.2f} {p[1]:.2f}"

# face fills (opaque base + overlay, per spec table)
F_TOP = hx(WHITE)
F_LEFT = hx(mix(WHITE, INK, 0.07))
F_RIGHT = hx(mix(WHITE, INK, 0.15))
FA_TOP = hx(mix(WHITE, ACC, 0.10))
FA_LEFT = hx(mix(WHITE, ACC, 0.20))
FA_RIGHT = hx(mix(WHITE, ACC, 0.32))
SIL = hx(INK)
INNER = "rgba(45,49,66,0.55)"
LEADER = "rgba(45,49,66,0.40)"
TRACE = "rgba(45,49,66,0.30)"

N_ARC = 12

def outline(x0, y0, x1, y1, r):
    """Outline points starting at (x0+r, y0), going back-edge → right-edge → front-edge → left-edge.
    Returns (pts, idx_right, idx_front, idx_left) where the idxs are the 45° extreme samples."""
    pts = []
    def arc(cx, cy, a0, a1):
        for i in range(1, N_ARC + 1):
            a = a0 + (a1 - a0) * i / N_ARC
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    D = math.radians
    pts.append((x0 + r, y0))
    pts.append((x1 - r, y0))
    br_start = len(pts)
    arc(x1 - r, y0 + r, D(-90), D(0))          # back-right
    idx_right = br_start + N_ARC // 2
    pts.append((x1, y1 - r))
    fr_start = len(pts)
    arc(x1 - r, y1 - r, D(0), D(90))           # front-right
    idx_front = fr_start + N_ARC // 2
    pts.append((x0 + r, y1))
    fl_start = len(pts)
    arc(x0 + r, y1 - r, D(90), D(180))         # front-left
    idx_left = fl_start + N_ARC // 2
    pts.append((x0, y0 + r))
    arc(x0 + r, y0 + r, D(180), D(270))        # back-left
    return pts, idx_right, idx_front, idx_left

def path_from(screen_pts, close=True):
    d = "M " + fmt(screen_pts[0])
    for p in screen_pts[1:]:
        d += " L " + fmt(p)
    if close:
        d += " Z"
    return d

def prism(x0, y0, x1, y1, r, z0, z1, top, left, right, sil_w=1.2):
    """Return SVG for one rounded prism (silhouette fill via faces, top, inner edges)."""
    pts, ir, ifr, il = outline(x0, y0, x1, y1, r)
    n = len(pts)
    def P(i, z):
        p = pts[i % n]
        return iso(p[0], p[1], z)
    # top face (all outline at z1)
    top_d = path_from([P(i, z1) for i in range(n)])
    # silhouette: top back-half (il → ir, decreasing/wrap), right vertical, bottom front-half (ir → il, increasing)
    top_half = []
    i = il
    while True:
        top_half.append(P(i, z1))
        if i == ir:
            break
        i -= 1
        if i < 0:
            i = n - 1
    bot_half = [P(i, z0) for i in range(ir, il + 1)]
    sil_d = path_from(top_half + [P(ir, z0)] + bot_half)
    # face A (lit, screen-left, +y face): il down, bottom il→ifr, ifr up, top ifr→il
    fa_top = []
    i = ifr
    while True:
        fa_top.append(P(i, z1))
        if i == il:
            break
        i -= 1
        if i < 0:
            i = n - 1
    faceA = [P(il, z1), P(il, z0)] + [P(i, z0) for i in range(il, ifr + 1)] + [P(ifr, z1)] + \
            [P(i, z1) for i in range(ifr - 1, il - 1, -1)]
    # face B (shade, screen-right, +x face): ifr down, bottom ifr→ir, ir up, top ir→ifr
    faceB = [P(ifr, z1), P(ifr, z0)] + [P(i, z0) for i in range(ifr, ir + 1)] + [P(ir, z1)] + \
            [P(i, z1) for i in range(ir, ifr, -1)]
    inner_top = path_from([P(i, z1) for i in range(n)])
    front_v = f'<line x1="{P(ifr,z1)[0]:.2f}" y1="{P(ifr,z1)[1]:.2f}" x2="{P(ifr,z0)[0]:.2f}" y2="{P(ifr,z0)[1]:.2f}" stroke="{INNER}" stroke-width="0.8"/>'
    out = []
    out.append(f'<path d="{sil_d}" fill="{right}"/>')
    out.append(f'<path d="{path_from(faceA)}" fill="{left}"/>')
    out.append(f'<path d="{path_from(faceB)}" fill="{right}"/>')
    out.append(f'<path d="{top_d}" fill="{top}"/>')
    out.append(f'<path d="{inner_top}" fill="none" stroke="{INNER}" stroke-width="0.8"/>')
    out.append(front_v)
    out.append(f'<path d="{sil_d}" fill="none" stroke="{SIL}" stroke-width="{sil_w}"/>')
    return "\n".join(out)

def inset(x0, y0, x1, y1, r, z):
    pts, _, _, _ = outline(x0, y0, x1, y1, r)
    d = path_from([iso(p[0], p[1], z) for p in pts])
    return f'<path d="{d}" fill="none" stroke="{INNER}" stroke-width="0.8"/>'

parts_svg = []

# trace lines first (left & right extremes, bottom part outline → underside of top part)
mx_r = (308 + 12 * math.cos(math.radians(-45)), 12 + 12 * math.sin(math.radians(-45)))
mx_l = (12 + 12 * math.cos(math.radians(135)), 188 + 12 * math.sin(math.radians(135)))
p_r = iso(*mx_r, 0); p_r_top = iso(*mx_r, 480)
p_l = iso(*mx_l, 0); p_l_top = iso(*mx_l, 480)
parts_svg.append(f'<line x1="{p_l[0]:.2f}" y1="{p_l[1]:.2f}" x2="{p_l_top[0]:.2f}" y2="{p_l_top[1]:.2f}" stroke="{TRACE}" stroke-width="0.8" stroke-dasharray="4,3"/>')
parts_svg.append(f'<line x1="{p_r[0]:.2f}" y1="{p_r[1]:.2f}" x2="{p_r_top[0]:.2f}" y2="{p_r_top[1]:.2f}" stroke="{TRACE}" stroke-width="0.8" stroke-dasharray="4,3"/>')

T = 28  # slab thickness
Z = [0, 160, 320, 480]  # slab bottoms

# L1 data (z 0..28): slabs + 2 chips
parts_svg.append(prism(0, 0, 320, 200, 12, Z[0], Z[0] + T, F_TOP, F_LEFT, F_RIGHT))
parts_svg.append(prism(36, 44, 148, 156, 6, Z[0] + T, Z[0] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))
parts_svg.append(prism(184, 44, 284, 156, 6, Z[0] + T, Z[0] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))
# L2 simulation (z 160..188): slab + 3 chips
parts_svg.append(prism(0, 0, 320, 200, 12, Z[1], Z[1] + T, F_TOP, F_LEFT, F_RIGHT))
parts_svg.append(prism(24, 56, 112, 144, 6, Z[1] + T, Z[1] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))
parts_svg.append(prism(136, 56, 216, 144, 6, Z[1] + T, Z[1] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))
parts_svg.append(prism(240, 56, 296, 144, 6, Z[1] + T, Z[1] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))
# L3 orchestration (z 320..348): focal slab + inset well
parts_svg.append(prism(0, 0, 320, 200, 12, Z[2], Z[2] + T, FA_TOP, FA_LEFT, FA_RIGHT))
parts_svg.append(inset(64, 52, 256, 148, 10, Z[2] + T))
# L4 presentation (z 480..508): slab + 2 chips
parts_svg.append(prism(0, 0, 320, 200, 12, Z[3], Z[3] + T, F_TOP, F_LEFT, F_RIGHT))
parts_svg.append(prism(36, 44, 148, 156, 6, Z[3] + T, Z[3] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))
parts_svg.append(prism(184, 44, 284, 156, 6, Z[3] + T, Z[3] + T + 12, F_TOP, F_LEFT, F_RIGHT, 1.0))

# labels + leaders (right column x=680)
COL_X = 680
labels = [
    (Z[0], "数据层", "注册表 · 核心库", False),
    (Z[1], "模拟层", "世界 · 生成 · 实体", False),
    (Z[2], "编排层", "主循环 Game", True),
    (Z[3], "呈现层", "输入/窗口 · 渲染器", False),
]
lab_svg = []
for z0, name, sub, focal in labels:
    y = p_r[1] - (z0 + T / 2)  # right extreme at mid thickness
    x = p_r[0]
    col = "#bf4520" if focal else "none"
    dot = f'<circle cx="{x + 6:.2f}" cy="{y:.2f}" r="1.5" fill="{"#bf4520" if focal else "rgba(45,49,66,0.55)"}"/>'
    lead = f'<line x1="{x + 8:.2f}" y1="{y:.2f}" x2="{COL_X - 12}" y2="{y:.2f}" stroke="{"#bf4520" if focal else LEADER}" stroke-width="0.8"/>'
    nm = f'<text x="{COL_X}" y="{y - 4:.1f}" fill="#2d3142" font-size="16" font-weight="600" font-family="\'Geist\', \'Noto Sans SC\', system-ui, sans-serif">{name}</text>'
    sb = f'<text x="{COL_X}" y="{y + 14:.1f}" fill="{"#bf4520" if focal else "#4f5d75"}" font-size="12" font-weight="500" font-family="\'Geist\', \'Noto Sans SC\', system-ui, sans-serif">{sub}</text>'
    lab_svg.append(dot + lead + nm + sb)

SVG_BODY = "\n".join(parts_svg) + "\n" + "\n".join(lab_svg)

HTML = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TokenMine.cpp 引擎架构（立体分层）· Exploded axonometric</title>
  <link href="https://fonts.googleapis.com/css2?family=Instrument+Serif:ital@0;1&family=Geist:wght@400;500;600&family=Geist+Mono:wght@400;500;600&family=Noto+Serif:ital@0;1&family=Noto+Serif+SC:wght@400;600&family=Noto+Sans+SC:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Geist', 'Noto Sans SC', system-ui, sans-serif;
      background: #f5f5f5;
      color: #2d3142;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 3rem 2rem;
    }
    .frame { max-width: 1100px; width: 100%; }
    .diagram-container { width: 100%; overflow-x: auto; }
    .eyebrow {
      font-family: 'Geist Mono', ui-monospace, monospace;
      font-size: 0.66rem;
      font-weight: 500;
      letter-spacing: 0.18em;
      text-transform: uppercase;
      color: #4f5d75;
      margin-bottom: 0.5rem;
    }
    h1 {
      font-family: 'Instrument Serif', 'Noto Serif', 'Noto Serif SC', serif;
      font-size: clamp(1.5rem, 2.4vw + 0.75rem, 2rem);
      font-weight: 400;
      letter-spacing: -0.02em;
      line-height: 1.15;
      color: #2d3142;
      margin-bottom: 1.5rem;
    }
    svg { width: 100%; min-width: 1000px; display: block; }
    @media print {
      .diagram-container { overflow-x: visible; }
      svg { min-width: 0; }
    }
  </style>
</head>
<body>
  <div class="frame">
    <p class="eyebrow">Exploded axonometric · 立体分层 · 数据 → 模拟 → 编排 → 呈现</p>
    <h1>TokenMine.cpp 引擎架构（立体分层）</h1>
    <div class="diagram-container">
      <svg viewBox="0 0 1000 872" xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="tokenmine-axo-title tokenmine-axo-desc">
        <title id="tokenmine-axo-title">TokenMine.cpp 引擎架构（立体分层）</title>
        <desc id="tokenmine-axo-desc">四层爆炸轴测：自下而上为数据层（注册表与核心库）、模拟层（世界/生成/实体）、编排层（主循环 Game，焦点）、呈现层（输入窗口与渲染器）；层顶小棱柱为该层子系统，沿竖直轴拉开，位置即依赖关系。</desc>
        <rect width="100%" height="100%" fill="#f5f5f5"/>
""" + SVG_BODY + """
        <text x="40" y="848" fill="#5a6580" font-size="14" font-style="italic"
              font-family="'Instrument Serif', 'Noto Serif', 'Noto Serif SC', serif">自下而上：数据 → 模拟 → 编排 → 呈现——沿轴位置即依赖方向，无箭头</text>
      </svg>
    </div>
  </div>
</body>
</html>
"""

if __name__ == "__main__":
    import argparse
    from pathlib import Path

    ap = argparse.ArgumentParser(description="Generate the exploded axonometric (4-layer) architecture diagram.")
    ap.add_argument("--out", default="docs/architecture-axonometric.html", help="output HTML path (repo root)")
    args = ap.parse_args()
    Path(args.out).write_text(HTML)
    print(f"wrote {args.out}")
