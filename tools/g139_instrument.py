#!/usr/bin/env python3
"""Gate performance A/B instrumentation patcher (g139 family).

Applies IDENTICAL segment timers to a perf tree so a release-vs-dev
performance A/B compares like for like:
  - PERF-WIN per 300-frame window: upd med/p95/max, render med/max,
    swap med/max, fluid/fires/spread/ents/stream med (ms per call)
  - PIPE3-S0 per window: tickMs med/max (FluidSim::tick) +
    changesApplied med/max (its returned changed-cell count)
Apply it to BOTH A/B trees. The anchors are structural (assert
count == 1 on each) — if a tree change breaks an anchor, the script
FAILS LOUDLY instead of silently instrumenting a mismatched side
(PITS 317 discipline: the perf trees must be rebuilt from repo +
commit + this same patcher after a /tmp wipe). The window counters
are measurement-only (no behavior change: no world state, no frame
logic).

LAYOUTS (UX-2 A2 retired the single game.cpp; the patcher auto-detects):
  - legacy: src/core/game.cpp present (v1.35.0 and earlier) — all six
    sites in that one file, the window block in an anonymous namespace
    (the original v1 patcher behavior, byte-identical output).
  - split:  no game.cpp (v1.36.0 onward, the 8 game_*.cpp TUs) — the
    window block lives in a GENERATED header src/core/g139win.hpp
    (namespace mc, all-inline, ODR-safe) included by the two TUs that
    carry the measurement sites: game_frame.cpp (5 sites: the update
    streaming segment / fluid tick / fires / spread / entities) and
    game_init.cpp (1 site: the run loop update/render/swap window
    print). Marker accounting is unchanged: 27 `g139_` tokens
    (g139win carries no underscore = outside the count, both layouts).

Proven on the CRAFT-4..UX-2 gate series (137-156): the same 27 marker
set applied to v1.21..v1.36 trees; the gate scripts parse the PERF-WIN
/ PIPE3-S0 log lines (see the phase archives' perf A/B sections).

Usage: tools/g139_instrument.py <tree-dir>
"""
import io
import os
import sys

tree = sys.argv[1]
legacy_path = tree + '/src/core/game.cpp'
is_split = not os.path.exists(legacy_path)


def read(p):
    return io.open(p, encoding='utf-8').read()


def write(p, s):
    io.open(p, 'w', encoding='utf-8').write(s)


def make_repper(sref):
    def rep(old, new):
        assert sref[0].count(old) == 1, 'anchor not unique/found: ' + old[:70]
        sref[0] = sref[0].replace(old, new, 1)
    return rep

# The window-stat machinery (namespace-agnostic body).
ANON_BODY = '''inline double g139_ms(const std::chrono::steady_clock::time_point& t0) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
}
struct PerfWin {
    std::vector<double> upd, render, swap, fluid, fires, spread, ents,
        stream, tickMs;
    std::vector<int> changes;
    static double med(std::vector<double> v) {
        if (v.empty()) {
            return 0.0;
        }
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    }
    static double mx(const std::vector<double>& v) {
        return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end());
    }
    static double p95(std::vector<double> v) {
        if (v.empty()) {
            return 0.0;
        }
        std::sort(v.begin(), v.end());
        return v[(v.size() * 95) / 100];
    }
    void record(int frame) {
        MC_LOG_INFO("PERF-WIN frame=" + std::to_string(frame) +
                    " upd med/p95/max=" + std::to_string(med(upd)) + "/" +
                    std::to_string(p95(upd)) + "/" + std::to_string(mx(upd)) +
                    " render med/max=" + std::to_string(med(render)) + "/" +
                    std::to_string(mx(render)) + " swap med/max=" +
                    std::to_string(med(swap)) + "/" + std::to_string(mx(swap)) +
                    " fluid=" + std::to_string(med(fluid)) +
                    " fires=" + std::to_string(med(fires)) +
                    " spread=" + std::to_string(med(spread)) +
                    " ents=" + std::to_string(med(ents)) +
                    " stream=" + std::to_string(med(stream)) + "ms");
        std::vector<double> c;
        for (int v : changes) {
            c.push_back(static_cast<double>(v));
        }
        MC_LOG_INFO("PIPE3-S0 win=300f tickMs med/max=" +
                    std::to_string(med(tickMs)) + "/" +
                    std::to_string(mx(tickMs)) + " changesApplied med/max=" +
                    std::to_string(med(c)) + "/" + std::to_string(mx(c)));
        upd.clear();
        render.clear();
        swap.clear();
        fluid.clear();
        fires.clear();
        spread.clear();
        ents.clear();
        stream.clear();
        tickMs.clear();
        changes.clear();
    }
};
inline PerfWin& g139win() {
    static PerfWin w;
    return w;
}
'''

RUN_LOOP_NEW = '''        const auto g139_tu = std::chrono::steady_clock::now();
        update(dt);
        g139win().upd.push_back(g139_ms(g139_tu));
        const auto g139_tr = std::chrono::steady_clock::now();
        render();
        g139win().render.push_back(g139_ms(g139_tr));
        const auto g139_ts = std::chrono::steady_clock::now();
        m_window.swapBuffers();
        g139win().swap.push_back(g139_ms(g139_ts));
        ++m_frameCount;
        if (m_frameCount % 300 == 0) {
            g139win().record(m_frameCount);
        }
'''
RUN_LOOP_OLD = '''        update(dt);
        render();
        m_window.swapBuffers();
        ++m_frameCount;
'''

STREAMING_NEW = '''    const auto g139_tst = std::chrono::steady_clock::now();
    collectLitColumns();
    integrateMeshJobs();
    integrateStreamedColumns();
    unloadFarColumns();
    g139win().stream.push_back(g139_ms(g139_tst));
'''
STREAMING_OLD = '''    collectLitColumns();
    integrateMeshJobs();
    integrateStreamedColumns();
    unloadFarColumns();
'''

FLUID_NEW = '''        const auto g139_tf = std::chrono::steady_clock::now();
        const std::vector<BlockPos> changed = FluidSim::tick(
            m_world, m_blocks, &m_flammableChunks, honeyActive);
        g139win().fluid.push_back(g139_ms(g139_tf));
        g139win().tickMs.push_back(g139_ms(g139_tf));
        g139win().changes.push_back(static_cast<int>(changed.size()));
'''
FLUID_OLD = '''        const std::vector<BlockPos> changed = FluidSim::tick(
            m_world, m_blocks, &m_flammableChunks, honeyActive);
'''

FIRES = ('''    const auto g139_tfi = std::chrono::steady_clock::now();
    updateFires(dt);
    g139win().fires.push_back(g139_ms(g139_tfi));
''', '    updateFires(dt);\n')

SPREAD = ('''    const auto g139_tsp = std::chrono::steady_clock::now();
    updateFireSpread();
    g139win().spread.push_back(g139_ms(g139_tsp));
''', '    updateFireSpread();\n')

ENTS = ('''    const auto g139_ten = std::chrono::steady_clock::now();
    updateEntities(dt);
    g139win().ents.push_back(g139_ms(g139_ten));
''', '    updateEntities(dt);\n')

if is_split:
    # Generated header (namespace mc, ODR-safe; perf-tree only, never
    # committed). Name + comment avoid the g139_ token so the marker
    # count stays the legacy 27 (measurement tokens only).
    write(tree + '/src/core/g139win.hpp',
          '// g139-STEP0: GENERATED by the g139 instrumentation patcher — '
          'perf-tree only, never committed.\n'
          '#pragma once\n\n'
          '#include <algorithm>\n'
          '#include <chrono>\n'
          '#include <vector>\n\n'
          '#include "core/logger.hpp"\n\n'
          'namespace mc {\n\n' + ANON_BODY + '} // namespace mc\n')
    frame = tree + '/src/core/game_frame.cpp'
    init = tree + '/src/core/game_init.cpp'
    for f in (frame, init):
        ref = [read(f)]
        make_repper(ref)('#include "core/game.hpp"\n',
                         '#include "core/game.hpp"\n'
                         '#include "core/g139win.hpp"\n')
        write(f, ref[0])
    ref = [read(frame)]
    rep = make_repper(ref)
    rep(STREAMING_OLD, STREAMING_NEW)
    rep(FLUID_OLD, FLUID_NEW)
    rep(FIRES[1], FIRES[0])
    rep(SPREAD[1], SPREAD[0])
    rep(ENTS[1], ENTS[0])
    write(frame, ref[0])
    ref = [read(init)]
    make_repper(ref)(RUN_LOOP_OLD, RUN_LOOP_NEW)
    write(init, ref[0])
    total = (read(frame).count('g139_') + read(init).count('g139_') +
             read(tree + '/src/core/g139win.hpp').count('g139_'))
    print('instrumented', tree,
          '— g139-STEP0 markers (split layout):', total)
else:
    p = legacy_path
    orig = read(p)
    s = [orig]
    rep = make_repper(s)
    # 1. <chrono> include
    rep('#include <algorithm>\n',
        '#include <algorithm>\n#include <chrono>\n')
    # 2. the window stats block (anonymous namespace, inside namespace mc)
    rep('namespace mc {\n',
        'namespace mc {\n\n'
        '// g139-STEP0: performance A/B window instrumentation '
        '(measurement only).\n'
        'namespace {\n' + ANON_BODY + '} // namespace\n\n')
    # 3-6. the measurement sites
    rep(RUN_LOOP_OLD, RUN_LOOP_NEW)
    rep(STREAMING_OLD, STREAMING_NEW)
    rep(FLUID_OLD, FLUID_NEW)
    rep(FIRES[1], FIRES[0])
    rep(SPREAD[1], SPREAD[0])
    rep(ENTS[1], ENTS[0])
    assert s[0] != orig
    write(p, s[0])
    print('instrumented', tree,
          '— g139-STEP0 markers (legacy layout):', s[0].count('g139_'))
