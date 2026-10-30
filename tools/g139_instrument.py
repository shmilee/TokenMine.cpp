#!/usr/bin/env python3
"""Gate performance A/B instrumentation patcher (g139 family).

Applies IDENTICAL segment timers to a perf tree's src/core/game.cpp so a
release-vs-dev performance A/B compares like for like:
  - PERF-WIN per 300-frame window: upd med/p95/max, render med/max,
    swap med/max, fluid/fires/spread/ents/stream med (ms per call)
  - PIPE3-S0 per window: tickMs med/max (FluidSim::tick) +
    changesApplied med/max (its returned changed-cell count)
Apply it to BOTH A/B trees. The anchors are structural (assert
count == 1 on each) — if a game.cpp change breaks an anchor, the
script FAILS LOUDLY instead of silently instrumenting a mismatched
side (PITS 317 discipline: the perf trees must be rebuilt from
repo + commit + this same patcher after a /tmp wipe). The window
counters are measurement-only (no behavior change: no world state,
no frame logic).

Proven on the CRAFT-4..PRE-12 gate series (137-148): the same 27
marker set applied to v1.21..v1.33 trees; the gate scripts parse the
PERF-WIN / PIPE3-S0 log lines (see the phase archives' perf A/B
sections).

Usage: tools/g139_instrument.py <tree-dir>
"""
import io
import sys

tree = sys.argv[1]
p = tree + '/src/core/game.cpp'
s = io.open(p, encoding='utf-8').read()
n0 = s


def rep(old, new):
    global s
    assert s.count(old) == 1, 'anchor not unique/found: ' + old[:70]
    s = s.replace(old, new, 1)


# 1. <chrono> include
rep('#include <algorithm>\n', '#include <algorithm>\n#include <chrono>\n')

# 2. the window stats struct (inside namespace mc, top of the file)
rep('namespace mc {\n', '''namespace mc {

// g139-STEP0: performance A/B window instrumentation (measurement only).
namespace {
inline double g139_ms(const std::chrono::steady_clock::time_point& t0) {
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
PerfWin& g139win() {
    static PerfWin w;
    return w;
}
} // namespace

''')

# 3. run loop: update/render/swap timers + 300f window print
rep('''        update(dt);
        render();
        m_window.swapBuffers();
        ++m_frameCount;
''', '''        const auto g139_tu = std::chrono::steady_clock::now();
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
''')

# 4. the streaming segment
rep('''    collectLitColumns();
    integrateMeshJobs();
    integrateStreamedColumns();
    unloadFarColumns();
''', '''    const auto g139_tst = std::chrono::steady_clock::now();
    collectLitColumns();
    integrateMeshJobs();
    integrateStreamedColumns();
    unloadFarColumns();
    g139win().stream.push_back(g139_ms(g139_tst));
''')

# 5. the fluid tick
rep('''        const std::vector<BlockPos> changed = FluidSim::tick(
            m_world, m_blocks, &m_flammableChunks, honeyActive);
''', '''        const auto g139_tf = std::chrono::steady_clock::now();
        const std::vector<BlockPos> changed = FluidSim::tick(
            m_world, m_blocks, &m_flammableChunks, honeyActive);
        g139win().fluid.push_back(g139_ms(g139_tf));
        g139win().tickMs.push_back(g139_ms(g139_tf));
        g139win().changes.push_back(static_cast<int>(changed.size()));
''')

# 6. fires / spread / entities
rep('''    updateFires(dt);
''', '''    const auto g139_tfi = std::chrono::steady_clock::now();
    updateFires(dt);
    g139win().fires.push_back(g139_ms(g139_tfi));
''')
rep('''    updateFireSpread();
''', '''    const auto g139_tsp = std::chrono::steady_clock::now();
    updateFireSpread();
    g139win().spread.push_back(g139_ms(g139_tsp));
''')
rep('''    updateEntities(dt);
''', '''    const auto g139_ten = std::chrono::steady_clock::now();
    updateEntities(dt);
    g139win().ents.push_back(g139_ms(g139_ten));
''')

assert s != n0
io.open(p, 'w', encoding='utf-8').write(s)
print('instrumented', tree, '— g139-STEP0 markers:', s.count('g139_'))
