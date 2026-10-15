# Phase 90 细节存档

> 范围（用户"按顺序把 建议1 、建议2、建议3 这三个 phase 完成" = 建议 2，
> 代理定案为最小完整版：**熔岩湖燃烧周围 = shipped 湖列表加 1 个表面湖**
> （depth == fill，熔岩顶与草 rim 同高），纯 worldgen.json 内容、零核心
> 代码，消费 P86"静态池不燃烧"偏离）。

## 阶段报告

- **`[2026-10-16, Phase 90]`** 熔岩湖燃烧周围 — 1148/1148（+1），
  `-Werror` 恒 0 警告；E2E 批跑全绿（无输入；S1 第 63 次结构性免疫 +
  S2 燃烧【12/12 rim 焦黑 + 56 溢出流体格 + 深湖不燃对照】）。

### 实现内容

**表面湖 = depth == fill 的湖**（`assets/data/worldgen.json`，纯数据）：
- shipped `lava_lakes` 由 2 深湖（(40,40) R4 depth4/fill2 + (55,45) R3
  depth3/fill2）加 **1 表面湖 (24,26) R1 depth2/fill2**（y=52 草高原平坦
  区，熔岩顶 = 区域最低高度 minH，与草 rim 顶**同高**）。
- 选位纪律：(24,26) 在出生点（seed 123456789 的 (0.5,*,0.5) 朝 -Z）的 **+Z
  后方** = 无输入基线视野外 → base18 免重锚。

**燃烧 + 溢出（既有机制，零核心代码）**：
- **P58 燃烧 pass**（`world/fluid.cpp` `burnFlamableNeighbors`，既有）：每帧
  扫所有 LAVA 格 6-邻 flammable BLOCK → 烧成 fire 块（游戏里 burnTo=fire，
  P64）。表面湖熔岩顶与草 rim 同高 → 12 草 rim 格 6-邻熔岩 → 点燃 = 草→火
  →4 s 燃尽→air（焦土环）。
- **FluidSim 流动**（既有）：地表熔岩沿**下坡**溢出盆地（56 新流体格，流缘
  再点燃相邻可燃物）= MC 熔岩湖"流动 + 燃烧周围"的落地。
- **深湖对照**：depth > fill 的湖熔岩埋于盆底（顶低于 rim），与 rim 顶不
  6-邻 → 不燃烧、不溢出（被围、静态）。

**测试（1147 → 1148，+1）**：
- `Phase90SurfaceLakeLavaTopAtRegionMin`：表面湖（depth==fill）lava 顶层 ==
  区域原始最低高度（与 rim 同高 = 燃烧前置）；对照埋湖（depth>fill）lava
  顶 < 区域最低（埋没、不燃烧）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p90_e2e.zsh`，**无输入**）

- **S1 第 63 次结构性免疫**：表面湖 + 焦土环 + 溢出在 (24,26)，seed
  123456789 出生点 (0.5,*,0.5) 朝 -Z 的**后方**（+Z）→ base18 字节一致。
- **S2 燃烧**（seed 9，600f，no input，`fire.spread_chance 0`）：fresh 世界
  生成表面湖 → P58 pass 把 **12/12 草 rim 焦黑**（grass→fire→air，存档
  blocks 字段）+ **56 溢出流体格**（熔岩沿下坡流出盆地，存档 fluids 字
  段）+ **深湖不燃**（(40,40)/(55,45) 周围无块/流体编辑 = 对照）。×2 savenorm
  字节一致。
- **无 S6**【P90 是 worldgen.json 变更——无新 parse/校验面】。

### 变更文件

- `assets/data/worldgen.json`（lava_lakes 加表面湖 (24,26) R1 depth2/fill2）
- `tests/test_world_generator.cpp`（+1：Phase90SurfaceLakeLavaTopAtRegionMin +
  `<climits>`）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、`AGENTS.md`、
  `docs/archive/phase-90.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 熔岩湖条「静态满熔岩」→「深湖静态 + 表面湖燃烧周围 + 溢出」
  （消费 P86"MC 熔岩湖会流动/燃烧周围 = 无"偏离）。

### 下一步

- P91 = 附魔台/信标多效果（建议 3）。
