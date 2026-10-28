#!/usr/bin/env python3
"""Generate docs/architecture-hero.html — TokenMine.cpp hero-style composite.

Dark skin, big serif title left, two 2:1 dimetric DIAMOND (corner-facing)
axonometric plates stacked as parallel layers — architecture overview (bottom)
and frame pipeline (top). Flat mini-diagrams are laid on each top face via
matrix(1,0.5,-1,0.5,ox,ty).

Both plates are the SAME size (no whole-plate scaling). Within-plate
near-large-far-small: the far (back, top-of-screen) corner is never drawn
larger than the near (front, bottom) corner, so each plate reads as a level
horizontal layer rather than tilting up toward the viewer.

Usage:  python3 tools/generate_architecture_hero.py [--out docs/architecture-hero.html]
"""
import argparse
import math
from pathlib import Path

# ---------------------------------------------------------------- geometry
# 2:1 dimetric diamond plates, both the SAME size — compact BANNER layout
# (wide, short; ~2.1:1) for the top of README.md
OY = 330
W0, D0, R0, T = 484, 286, 13, 18   # plate model size / corner radius / thickness
OX0 = 911                           # plate anchor x (both plates aligned)
Z_STEP = 264                         # top plate offset (1.10× the touching value)
N_PLANES = 2
TAPER_FAR = 0.18                     # within-plate 近大远小: far (back) end pulled toward
                                     # center → drawn smaller than the near end (a flat
                                     # receding floor, not a plate tilted up at the back)
TAPER_NEAR = 0.06                    # near (front, lowest) corner lifted slightly
CW, CH = 500, 320                    # content model size (plane_* draw in this space, scaled onto the plate)
CANVAS_W, CANVAS_H = 1600, 750

# dark skin (axonometry.py SKINS["dark"])
PAPER = "#2d3142"
INK = "#f5f5f5"
MUTED = "#bfc0c0"
SOFT = "#949eb2"
ACCENT = "#f08a59"
TOP_FACE = "#4d5164"
LEFT_FACE = "#393e53"
RIGHT_FACE = "#33384a"
SIL = INK
INNER = "rgba(245,245,245,0.45)"
NODE = "#2d3142"
NODE_STROKE = "rgba(245,245,245,0.55)"
ARROW = "rgba(245,245,245,0.50)"
ARROW_DASH = "rgba(245,245,245,0.45)"
TRACE = "rgba(245,245,245,0.30)"

SANS = "'Geist', 'Noto Sans SC', system-ui, sans-serif"
MONO = "'Geist Mono', ui-monospace, monospace"
SERIF = "'Instrument Serif', 'Noto Serif', 'Noto Serif SC', serif"


def f(p):
    return f"{p[0]:.2f} {p[1]:.2f}"


def path_from(sp, close=True):
    d = "M " + f(sp[0]) + "".join(" L " + f(p) for p in sp[1:])
    return d + (" Z" if close else "")


def iso(x, y, z, ox):
    return (ox + x - y, OY + (x + y) / 2.0 - z)


N_ARC = 12


def outline(x0, y0, x1, y1, r):
    pts = []

    def arc(cx, cy, a0, a1):
        for i in range(1, N_ARC + 1):
            a = a0 + (a1 - a0) * i / N_ARC
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))

    d2 = math.radians
    pts.append((x0 + r, y0))
    pts.append((x1 - r, y0))
    br = len(pts)
    arc(x1 - r, y0 + r, d2(-90), d2(0))
    ir = br + N_ARC // 2
    pts.append((x1, y1 - r))
    fr = len(pts)
    arc(x1 - r, y1 - r, d2(0), d2(90))
    ifr = fr + N_ARC // 2
    pts.append((x0 + r, y1))
    fl = len(pts)
    arc(x0 + r, y1 - r, d2(90), d2(180))
    il = fl + N_ARC // 2
    pts.append((x0, y0 + r))
    arc(x0 + r, y0 + r, d2(180), d2(270))
    return pts, ir, ifr, il


def taper_pts(pts, w, d):
    """Within-plate 近大远小: pull the FAR end (back/top-of-screen, model (0,0))
    toward the plate center so it is drawn smaller than the near end (a flat
    receding floor, not a plate tilted up at the back); the NEAR end (front,
    lowest corner) is lifted slightly (TAPER_NEAR) so the plate sits flatter."""
    cx, cy = w / 2, d / 2
    l2 = w * w + d * d            # |A→D|², A=(0,0) far, D=(w,d) near
    out = []
    for (x, y) in pts:
        tproj = (x * w + y * d) / l2          # 0 at far corner, 1 at near corner
        factor = TAPER_FAR * (1 - tproj) + TAPER_NEAR * tproj
        out.append((x + factor * (cx - x), y + factor * (cy - y)))
    return out


def slab(z0, w, d, r, t, ox):
    z1 = z0 + t
    pts, ir, ifr, il = outline(0, 0, w, d, r)
    pts = taper_pts(pts, w, d)                # 近大远小 taper
    n = len(pts)

    def P(i, z):
        p = pts[i % n]
        return iso(p[0], p[1], z, ox)

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
    sil = path_from(top_half + [P(ir, z0)] + bot_half)
    faceA = [P(il, z1), P(il, z0)] + [P(i, z0) for i in range(il, ifr + 1)] + [P(ifr, z1)] + \
            [P(i, z1) for i in range(ifr - 1, il - 1, -1)]
    faceB = [P(ifr, z1), P(ifr, z0)] + [P(i, z0) for i in range(ifr, ir + 1)] + [P(ir, z1)] + \
            [P(i, z1) for i in range(ir, ifr, -1)]
    top = path_from([P(i, z1) for i in range(n)])
    return "\n".join([
        f'<path d="{sil}" fill="{RIGHT_FACE}"/>',
        f'<path d="{path_from(faceA)}" fill="{LEFT_FACE}"/>',
        f'<path d="{path_from(faceB)}" fill="{RIGHT_FACE}"/>',
        f'<path d="{top}" fill="{TOP_FACE}"/>',
        f'<path d="{top}" fill="none" stroke="{INNER}" stroke-width="0.8"/>',
        f'<line x1="{P(ifr,z1)[0]:.2f}" y1="{P(ifr,z1)[1]:.2f}" x2="{P(ifr,z0)[0]:.2f}" y2="{P(ifr,z0)[1]:.2f}" stroke="{INNER}" stroke-width="0.8"/>',
        f'<path d="{sil}" fill="none" stroke="{SIL}" stroke-width="1.2"/>',
    ])


# ---------------------------------------------------------------- flat helpers
def node(u, v, w, h, name, sub, focal=False, sub2=None):
    st = ACCENT if focal else NODE_STROKE
    sw = 1.2 if focal else 1
    nm = ACCENT if focal else INK
    out = [f'<rect x="{u}" y="{v}" width="{w}" height="{h}" rx="4" fill="{NODE}" stroke="{st}" stroke-width="{sw}"/>']
    if sub2:
        out.append(f'<text x="{u + w / 2}" y="{v + 22}" fill="{nm}" font-size="12" font-weight="600" font-family="{SANS}" text-anchor="middle">{name}</text>')
        out.append(f'<text x="{u + w / 2}" y="{v + 38}" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">{sub}</text>')
        out.append(f'<text x="{u + w / 2}" y="{v + 52}" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">{sub2}</text>')
    elif sub:
        out.append(f'<text x="{u + w / 2}" y="{v + h / 2 - 2}" fill="{nm}" font-size="12" font-weight="600" font-family="{SANS}" text-anchor="middle">{name}</text>')
        out.append(f'<text x="{u + w / 2}" y="{v + h / 2 + 12}" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">{sub}</text>')
    else:
        out.append(f'<text x="{u + w / 2}" y="{v + h / 2 + 4}" fill="{nm}" font-size="12" font-weight="600" font-family="{SANS}" text-anchor="middle">{name}</text>')
    return "\n".join(out)


def arrow_h(x1, x2, y, dash=False, label=None, ly=None):
    st = ARROW_DASH if dash else ARROW
    da = ' stroke-dasharray="4,3"' if dash else ""
    mk = "arrow-d" if dash else "arrow-l"
    out = [f'<line x1="{x1}" y1="{y}" x2="{x2}" y2="{y}" stroke="{st}" stroke-width="1"{da} marker-end="url(#{mk})"/>']
    if label:
        out.append(f'<text x="{(x1 + x2) / 2}" y="{ly}" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle" letter-spacing="0.08em">{label}</text>')
    return "\n".join(out)


def arrow_v(y1, y2, x, dash=False):
    st = ARROW_DASH if dash else ARROW
    da = ' stroke-dasharray="4,3"' if dash else ""
    mk = "arrow-d" if dash else "arrow-l"
    return f'<line x1="{x}" y1="{y1}" x2="{x}" y2="{y2}" stroke="{st}" stroke-width="1"{da} marker-end="url(#{mk})"/>'


def elbow(pts, dash=False):
    st = ARROW_DASH if dash else ARROW
    da = ' stroke-dasharray="4,3"' if dash else ""
    mk = "arrow-d" if dash else "arrow-l"
    d = "M " + f(pts[0]) + "".join(" L " + f(p) for p in pts[1:])
    return f'<path d="{d}" fill="none" stroke="{st}" stroke-width="1"{da} marker-end="url(#{mk})"/>'


# ---------------------------------------------------------------- plane contents
def plane_pipeline():
    c = []
    c.append(f'<text x="24" y="44" fill="{SOFT}" font-size="10" font-family="{MONO}" letter-spacing="0.14em">FRAME PIPELINE · ONE FRAME 1/60</text>')
    # stage boxes
    c.append(f'<rect x="24" y="64" width="144" height="72" rx="4" fill="{NODE}" stroke="{NODE_STROKE}" stroke-width="1"/>')
    c.append(f'<text x="96" y="88" fill="{INK}" font-size="12" font-weight="600" font-family="{SANS}" text-anchor="middle">update · 帧锁定</text>')
    c.append(f'<text x="96" y="106" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">input → player → entity</text>')
    c.append(f'<text x="96" y="122" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">fluid → content → journal</text>')
    c.append(f'<rect x="180" y="64" width="144" height="72" rx="4" fill="rgba(240,138,89,0.08)" stroke="{ACCENT}" stroke-width="1.4"/>')
    c.append(f'<text x="252" y="88" fill="{ACCENT}" font-size="12" font-weight="600" font-family="{SANS}" text-anchor="middle">worker ×4 · 帧窗口</text>')
    c.append(f'<text x="252" y="106" fill="{MUTED}" font-size="8" font-family="{MONO}" text-anchor="middle">post → flush</text>')
    c.append(f'<text x="252" y="122" fill="{MUTED}" font-size="8" font-family="{MONO}" text-anchor="middle">mesh 2 cols · light ≤2</text>')
    c.append(f'<rect x="336" y="64" width="144" height="72" rx="4" fill="{NODE}" stroke="{NODE_STROKE}" stroke-width="1"/>')
    c.append(f'<text x="408" y="88" fill="{INK}" font-size="12" font-weight="600" font-family="{SANS}" text-anchor="middle">render</text>')
    c.append(f'<text x="408" y="106" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">opaque→cutout→blend</text>')
    c.append(f'<text x="408" y="122" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle">→ entity · hand · UI</text>')
    c.append(arrow_h(168, 180, 100))
    c.append(arrow_h(324, 336, 100))
    # update-order chips
    chips = [("流送 2列/帧", 24), ("实体 AI", 118), ("FluidSim", 212), ("内容 pass", 306), ("journal", 400)]
    for name_c, cx in chips:
        c.append(f'<rect x="{cx}" y="160" width="84" height="36" rx="4" fill="{NODE}" stroke="rgba(245,245,245,0.40)" stroke-width="1"/>')
        c.append(f'<text x="{cx + 42}" y="183" fill="{INK}" font-size="12" font-weight="500" font-family="{SANS}" text-anchor="middle">{name_c}</text>')
    for i in range(4):
        c.append(f'<line x1="{chips[i][1] + 84}" y1="178" x2="{chips[i][1] + 94}" y2="178" stroke="{ARROW}" stroke-width="1" marker-end="url(#arrow-l)"/>')
    # dashed posts to worker box
    c.append(elbow([(66, 160), (66, 148), (216, 148), (216, 136)], dash=True))
    c.append(f'<text x="140" y="144" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle" letter-spacing="0.06em">post mesh</text>')
    c.append(elbow([(254, 160), (254, 148), (288, 148), (288, 136)], dash=True))
    c.append(f'<text x="326" y="144" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="middle" letter-spacing="0.06em">post light</text>')
    # note + legend
    c.append(f'<text x="24" y="232" fill="{SOFT}" font-size="12" font-family="{SANS}">窗口内：主线程只读 World（硬规则 7）· gap≥4 位级独立，依赖对回退顺序</text>')
    c.append(f'<line x1="24" y1="276" x2="52" y2="276" stroke="{ARROW}" stroke-width="1" marker-end="url(#arrow-l)"/>')
    c.append(f'<text x="60" y="280" fill="{SOFT}" font-size="12" font-family="{SANS}">帧内</text>')
    c.append(f'<line x1="124" y1="276" x2="152" y2="276" stroke="{ARROW_DASH}" stroke-width="1" stroke-dasharray="4,3" marker-end="url(#arrow-d)"/>')
    c.append(f'<text x="160" y="280" fill="{SOFT}" font-size="12" font-family="{SANS}">post（worker 窗口）</text>')
    c.append(f'<rect x="320" y="270" width="14" height="10" rx="2" fill="rgba(240,138,89,0.08)" stroke="{ACCENT}" stroke-width="1"/>')
    c.append(f'<text x="342" y="280" fill="{SOFT}" font-size="12" font-family="{SANS}">worker job（焦点）</text>')
    return "\n".join(c)


def plane_architecture():
    c = []
    c.append(f'<text x="24" y="52" fill="{SOFT}" font-size="10" font-family="{MONO}" letter-spacing="0.14em">ARCHITECTURE OVERVIEW</text>')
    c.append(f'<text x="476" y="52" fill="{SOFT}" font-size="8" font-family="{MONO}" text-anchor="end" letter-spacing="0.06em">— call · -- worker · coral = focal</text>')
    # row 1
    c.append(node(24, 72, 104, 44, "输入 / 窗口", "SDL3→GameKey"))
    c.append(node(192, 72, 128, 44, "主循环 Game", "update→render", focal=True))
    c.append(node(384, 72, 104, 44, "渲染器", "3-pass · fog · cloud"))
    c.append(arrow_h(128, 192, 94))
    c.append(arrow_h(320, 384, 94, label="DRAW", ly=88))
    # row 2
    c.append(node(56, 144, 104, 44, "世界", "chunk·light·fluid"))
    c.append(node(204, 144, 104, 44, "实体", "player·mob·item"))
    c.append(node(364, 144, 124, 44, "数据注册表", "blocks·items·mobs", focal=True))
    c.append(elbow([(224, 116), (224, 130), (108, 130), (108, 144)]))
    c.append(elbow([(304, 116), (304, 138), (256, 138), (256, 144)]))
    c.append(arrow_v(144, 116, 426))
    # row 3
    c.append(node(56, 216, 104, 44, "核心库", "save·audio·workers"))
    c.append(arrow_v(216, 188, 108, dash=True))
    c.append(f'<text x="24" y="288" fill="{SOFT}" font-size="12" font-family="{SANS}">主循环驱动三大系统 · 数据注册表横向供给（零核心代码）</text>')
    return "\n".join(c)


# ---------------------------------------------------------------- assembly
def build(out_path):
    svg = []
    # plates bottom → top, both the SAME size (tapered diamond); 顺序 = 地基→细节
    contents = [plane_architecture, plane_pipeline]
    sx, sy = W0 / CW, D0 / CH        # content model (CW×CH) scaled onto the plate (W0×D0)
    sc = 1 - TAPER_FAR               # taper: content scaled about the top-face center —
                                     # far corner lands exactly on the tapered plate,
                                     # near/side corners end up inside (verified)
    for k, fn in enumerate(contents):
        z0 = k * Z_STEP
        z1 = z0 + T
        svg.append(slab(z0, W0, D0, R0, T, OX0))
        ty = OY - z1
        Ox = OX0 + (W0 - D0) / 2     # top-face center (screen)
        Oy = ty + (W0 + D0) / 4
        a, b = sc * sx, sc * sx / 2
        c, d = -sc * sy, sc * sy / 2
        e = sc * OX0 + (1 - sc) * Ox
        ff = sc * ty + (1 - sc) * Oy
        svg.append(f'<g transform="matrix({a:.4f},{b:.4f},{c:.4f},{d:.4f},{e:.2f},{ff:.2f})">\n{fn()}\n</g>')

    serif_title = f'''
<text x="138" y="256" fill="{INK}" font-size="67" font-weight="400" font-family="{SERIF}">TokenMine.cpp</text>
<text x="138" y="312" fill="{INK}" font-size="30" font-weight="400" font-family="{SERIF}">确定性体素沙盒 · C++ Minecraft</text>
<text x="138" y="363" fill="{SOFT}" font-size="14" font-family="{SANS}">内容全数据驱动——加方块 / 物品 / 配方 / Mob = JSON + 纹理；</text>
<text x="138" y="388" fill="{SOFT}" font-size="14" font-family="{SANS}">确定性是一级要求：无输入运行逐帧字节可复现。</text>
<line x1="138" y1="425" x2="541" y2="425" stroke="rgba(245,245,245,0.15)" stroke-width="1"/>
<text x="138" y="469" fill="{ACCENT}" font-size="15" font-family="{MONO}">01</text>
<text x="174" y="469" fill="{INK}" font-size="14" font-family="{SANS}">架构总览 — 主循环驱动三大系统</text>
<text x="138" y="499" fill="{ACCENT}" font-size="15" font-family="{MONO}">02</text>
<text x="174" y="499" fill="{INK}" font-size="14" font-family="{SANS}">帧管线 — update → worker → render</text>
<text x="138" y="716" fill="{SOFT}" font-size="12" font-family="{MONO}" letter-spacing="0.04em">立体分层合成图 · 细节图：docs/architecture.svg · frame-pipeline.svg · lighting.svg · worker-window.svg</text>
'''

    html = f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TokenMine.cpp · 立体分层合成图</title>
  <link href="https://fonts.googleapis.com/css2?family=Instrument+Serif:ital@0;1&family=Geist:wght@400;500;600&family=Geist+Mono:wght@400;500;600&family=Noto+Serif:ital@0;1&family=Noto+Serif+SC:wght@400;600&family=Noto+Sans+SC:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: {SANS};
      background: {PAPER};
      min-height: 100vh;
      display: flex; align-items: center; justify-content: center;
      padding: 3rem 2rem;
    }}
    .frame {{ max-width: 1660px; width: 100%; }}
    .diagram-container {{ width: 100%; overflow-x: auto; }}
    svg {{ width: 100%; min-width: 1600px; display: block; }}
    @media print {{ .diagram-container {{ overflow-x: visible; }} svg {{ min-width: 0; }} }}
  </style>
</head>
<body>
  <div class="frame">
    <div class="diagram-container">
      <svg viewBox="0 0 {CANVAS_W} {CANVAS_H}" xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="tokenmine-hero-title tokenmine-hero-desc">
        <title id="tokenmine-hero-title">TokenMine.cpp 立体分层合成图</title>
        <desc id="tokenmine-hero-desc">暗色 hero 风格合成图：左侧大标题 + 简介 + 规格表 + 两层索引；右侧两块同尺寸菱形轴测板平行叠层——底板架构总览（主循环驱动世界 / 实体 / 渲染，数据注册表横向供给），顶板帧管线（update → worker post→flush → render）；板内近大远小（后角远、前角近），整体水平、不翘起。</desc>
        <defs>
<pattern id="dots" width="22" height="22" patternUnits="userSpaceOnUse">
  <circle cx="1" cy="1" r="0.8" fill="rgba(245,245,245,0.10)"/>
</pattern>
<marker id="arrow-l" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><polygon points="0 0, 8 3, 0 6" fill="{ARROW}"/></marker>
<marker id="arrow-d" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><polygon points="0 0, 8 3, 0 6" fill="{ARROW_DASH}"/></marker>
</defs>
        <rect width="100%" height="100%" fill="{PAPER}"/>
        <rect width="100%" height="100%" fill="url(#dots)" opacity="0.5"/>
{chr(10).join(svg)}
{serif_title}
      </svg>
    </div>
  </div>
</body>
</html>
'''
    Path(out_path).write_text(html)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default="docs/architecture-hero.html")
    args = ap.parse_args()
    build(args.out)
