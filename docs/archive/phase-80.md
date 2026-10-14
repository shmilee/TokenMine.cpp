# Phase 80 细节存档

> 范围（用户"好的,请继续按建议进行下两个phase" = 建议 2 内容扩展线，
> 代理定案为最小完整版：**砂岩 = P61 材料链范式的纯内容扩展**——
> sandstone 块 + 3 沙配方 + 镐家族成员；零核心代码）。

## 阶段报告

- **`[2026-10-15, Phase 80]`** 砂岩 — 1114/1114（+2：内容钉死 2 +
  P69 契约演进），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 53 次结构
  性免疫【无重锚】+ S2 铁镐破坏【×2 savenorm + EXACT】+ S2b 手挖对
  照【×2 savenorm + EXACT，P69 判别器】+ S6 init 失败 ×2）。

### 实现内容

**内容（1 新块 + 1 配方 + 1 纹理 + 3 镐契约演进，零核心代码）**：
- `assets/data/blocks/sandstone.json`：solid cube、**hardness 1.5**
  （沙 0.5 与石 2.0 之间 = MC 值）、**自掉**（drops sandstone——单向
  材料链：沙 → 砂岩，无锻回 = P61 cobblestone 同款偏离）、非 flamable、
  光 0。
- 纹理：`tools/generate_block_textures.py` 新 `sandstone()`——沙调色板
  的**三层经典切**（行 4-5 / 10-11 深色 bedding 线 + 略暖密的沙基色，
  新 salt 923/924，既有纹理确定性再生零变化）+ atlas **空位 [14,0]**
  （70→71 tile，无 UV 移位 = **无重锚**）。
- 配方：shapeless **3 sand → 1 sandstone**（P61 锻石范式；沙 = 既有
  块物品）。
- **3 镐 `fast_against` 加 sandstone**（wooden/stone/iron）：石类家族
  = MC 镐对砂岩有效（P69 obsidian 排除条款不适用——项目无工具等级
  断层）；P69 钉死的 **8 块精确集 → 9 块契约演进**（测试同步）+
  `Phase69PickaxeEffectiveBreakSpeed` 加 sandstone = 8.0 断言。

**测试（1112 → 1114）**：
- `SandstoneBlockContentPinned`（真实资产：hardness 1.5 / 自掉 /
  solid / opaque / 非 flamable / cube）。
- `SandstoneRecipeContentPinned`（真实配方：3 sand → 1）。
- P69 两测试契约演进（9 块集 + effectiveBreakSpeed 断言）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p80_e2e.zsh`）

- **S1 第 53 次结构性免疫**：砂岩永不生成 + 默认背包无 + atlas 空位
  无 UV 移位 + fast_against 增益只在镐挖砂岩时生效 = 无输入零世界
  状态，base18 字节一致。
- **S2 铁镐破坏**（输入 420f ×2 savenorm + EXACT）：P52 已验证瞄准
  格 (22,46,−5) 放砂岩、手持铁镐 **hold 1.0 s**——1.5×1.5/8.0 =
  **0.28125 s << hold** 破 → 块消失（edit 与生成态相消 blocks []）+
  **sandstone ×1 掉落停 P59/P61/P67 证明落点 (22.5,45,−4.5)**。
- **S2b 手挖对照**（同 hold、空手 break_speed 1.0：1.5×1.5/1 =
  **2.25 s > hold** → **块存活无掉落**）——hold 卡在两破坏时间之间
  = **P69 判别器**（×2 镐破 / ×2 手存，互斥终态）。
- **S6 init 失败 ×2**（scratch 资产）：drops "sandstonez" → "block
  'sandstone' drops unknown item"（P61 契约依赖方向同款）/ 配方原料
  "sandz" → "uses unknown ingredient item"。
- 首轮唯一失败 = 断言笔误（420f 的 time 常量写成 300f 值——非项目
  缺陷）。

### 变更文件

- `assets/data/blocks/sandstone.json`（新）、
  `assets/data/recipes/sandstone.json`（新）、
  `assets/textures/blocks/sandstone.png`（新，工具链产物）、
  `assets/data/textures/block_atlas.json`（+1 空位）、
  `assets/data/items/wooden_pickaxe.json` /
  `stone_pickaxe.json` / `iron_pickaxe.json`（fast_against +1）、
  `tools/generate_block_textures.py`
- `tests/test_item.cpp`（+2 新、2 改写）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/GAMEPLAY.md`、
  `AGENTS.md`、`docs/archive/phase-80.md`

### 已知问题（本阶段）

- 新条（世界/生成节）：**材料链单向**——sand → sandstone（与 P61
  stone → cobblestone 同款；MC 的砂岩→沙亦无 = 一致）。
- 无新偏离（镐加速 = MC 语义；自掉 = MC 语义）。

### 下一步建议

1. **砂岩后续**（同范式低成本）：smooth_sandstone（2 砂岩 → 1，MC
   1.14+ 平滑变体）或砂岩楼梯/墙（需台阶模型 = 中成本）。
2. **弓 v5**（组合弓：power_speed_bow / 全强化弓 = 三效果叠加物品，
   同范式）。
3. 信标（吸收第二来源，需方块实体系统 = 高成本）。
4. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
