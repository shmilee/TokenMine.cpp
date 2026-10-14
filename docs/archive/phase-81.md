# Phase 81 细节存档

> 范围（用户"接下来的三个 phase, 按成本排序进行" = 第 1 个最低成本，
> 代理定案为最小完整版：**光滑砂岩 = P80 砂岩链的抛光步**——
> smooth_sandstone 块 + 2 砂岩配方 + 镐家族成员；零核心代码，P61/P80
> 材料链范式）。

## 阶段报告

- **`[2026-10-15, Phase 81]`** 光滑砂岩 — 1116/1116（+2：内容钉死 2 +
  P69 契约演进），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 54 次结构
  性免疫【无重锚】+ S2 铁镐破坏【×2 savenorm + EXACT】+ S2b 手挖对照
  【×2 savenorm + EXACT，P80 判别器】+ S6 init 失败 ×2）。

### 实现内容

**内容（1 新块 + 1 配方 + 1 纹理 + 3 镐契约演进，零核心代码）**：
- `assets/data/blocks/smooth_sandstone.json`：solid cube、**hardness
  1.5**（MC 值）、**自掉**（单向链 sand → sandstone → smooth_sandstone
  的最后一环）、非 flamable、光 0。
- 纹理：`tools/generate_block_textures.py` 新 `smooth_sandstone()`——砂
  岩调色板**无三层 bedding 线**（抛光面，略亮更均匀，新 salt 925/926，
  既有纹理确定性再生零变化）+ atlas **空位 [15,0]**（71→72 tile，无 UV
  移位 = **无重锚**）。
- 配方：shapeless **2 sandstone → 1 smooth_sandstone**（MC 1.14+ 抛光
  变体；砂岩 = 既有块物品）。
- **3 镐 `fast_against` 加 smooth_sandstone**（wooden/stone/iron）：石
  类家族；P80 钉死的 **9 块精确集 → 10 块契约演进**（测试同步）+
  `Phase69PickaxeEffectiveBreakSpeed` 加 smooth_sandstone = 8.0 断言。

**测试（1114 → 1116）**：
- `SmoothSandstoneBlockContentPinned`（真实资产：hardness 1.5 / 自掉 /
  solid / opaque / 非 flamable / FullCube）。
- `SmoothSandstoneRecipeContentPinned`（真实配方：2 sandstone → 1）。
- P69 两测试契约演进（10 块集 + effectiveBreakSpeed 断言）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p81_e2e.zsh`）

- **S1 第 54 次结构性免疫**：光滑砂岩永不生成 + 默认背包无 + atlas 空
  位无 UV 移位 + fast_against 增益只在镐挖光滑砂岩时生效 = 无输入零世
  界状态，base18 字节一致。
- **S2 铁镐破坏**（输入 420f ×2 savenorm + EXACT）：P52 已验证瞄准
  格 (22,46,−5) 放光滑砂岩、手持铁镐 **hold 1.0 s**——1.5×1.5/8.0 =
  **0.28125 s << hold** 破 → 块消失（edit 与生成态相消 blocks []）+
  **smooth_sandstone ×1 掉落停 P59/P61/P67 证明落点 (22.5,45,−4.5)**。
- **S2b 手挖对照**（同 hold、空手 break_speed 1.0：1.5×1.5/1 =
  **2.25 s > hold** → **块存活无掉落**）——hold 卡在两破坏时间之间 =
  **P80 判别器**（互斥终态）。
- **S6 init 失败 ×2**（scratch 资产）：drops "smooth_sandstonez" →
  "block 'smooth_sandstone' drops unknown item" 点名 / 配方原料
  "sandstonez" → "uses unknown ingredient item" 点名。

### 变更文件

- `assets/data/blocks/smooth_sandstone.json`（新）、
  `assets/data/recipes/smooth_sandstone.json`（新）、
  `assets/textures/blocks/smooth_sandstone.png`（新，工具链产物）、
  `assets/data/textures/block_atlas.json`（+1 空位）、
  `assets/data/items/wooden_pickaxe.json` / `stone_pickaxe.json` /
  `iron_pickaxe.json`（fast_against +1）、`tools/generate_block_textures.py`
- `tests/test_item.cpp`（+2 新、2 改写）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/GAMEPLAY.md`、
  `AGENTS.md`、`docs/archive/phase-81.md`

### 已知问题（本阶段）

- "材料链单向"条扩展：`sand → sandstone → smooth_sandstone`（与 P61
  stone → cobblestone 同款；MC 的砂岩→沙、光滑砂岩→砂岩亦无 = 一致）。
- 无新偏离（镐加速 = MC 语义；自掉 = MC 语义；hardness 1.5 = MC 值）。

### 下一步

- P82 = 弓 v5 组合弓（成本第 2，纯内容）；P83 = 信标（成本第 3，方
  块实体系统）。
