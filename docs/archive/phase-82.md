# Phase 82 细节存档

> 范围（用户"接下来的三个 phase, 按成本排序进行" = 第 2 个，代理定案
> 为最小完整版：**弓 v5 = 大师弓**——P77/P79 弓强化线的三合一顶点，
> 纯内容零核心代码，消费 P79"无组合弓"偏离）。

## 阶段报告

- **`[2026-10-15, Phase 82]`** 大师弓 — 1118/1118（+2：内容钉死 2），
  `-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 55 次结构性免疫【无重锚】+
  S2 大师弓 ×4【P77 拾取返还自见证】+ S2b 力量弓对照 ×2 + S6 init
  失败 ×2）。

### 实现内容

**内容（1 新物品 + 1 配方 + 1 图标，零核心代码）**：
- `assets/data/items/master_bow.json`：bow 同款数据（ammo arrow、
  max_stack 1、attack_damage 1）+ **`shoot_damage` 8**（力量 P79）+
  **`shoot_speed` 20**（速度 P79）+ **`infinite` true**（无限 P77）——
  三把强化弓的**三效果叠加**。发射语义全走 P21/P77/P79 既有数据驱动
  路径（`fireBow` 读 shootSpeed/shootDamage、infinite 跳过消耗）。
- 配方（shapeless，flint_and_steel / P61 链范式）：**power_bow +
  speed_bow + infinite_bow → 1 master_bow**——MC 附魔台的合成替身，
  **给三把强化弓一个用途**（它们不再是死胡同物品）。
- 图标（generate_item_icons.py，新 salt 101-104，既有图标确定性再生零
  变化）：`master_bow` = 弓 + **三色标记**（弓心橙红迸裂=力量、上方蓝
  钻=无限、下方青色线=速度）。
- **不在默认 hotbar**（结构性免疫，P67/P68/P77/P79 弓范式）。
- **零 format**（无限/力量/速度字段全既有）。

**测试（1116 → 1118）**：
- `MasterBowContentPinned`（真实资产：shootSpeed 20 / shootDamage 8 /
  infinite true / ammo arrow / maxStack 1 / icon）。
- `MasterBowRecipePinned`（真实配方：3 原料 power_bow/speed_bow/
  infinite_bow 各 ×1 → master_bow ×1）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p82_e2e.zsh`）

- **S2 大师弓 ×4**（输入 600f ×2 savenorm + EXACT）：俯角 pitch −1.53
  （P77 已证几何——箭插地 ≤1.0 拾取半径内）连射 2 次（右键满拉 1.7
  s）→ 2 箭射中脚边 → 拾取返还 +2。**master 是 infinite**：2 射 0 耗
  +2 返 = **arrow ×4**。拾取返还是射出可观测证明（P77 范式）。
- **S2b 力量弓对照 ×2**（同输入、power_bow = 普通发射器）：2 射 2 耗 +
  2 返 = **arrow ×2**。**×4 vs ×2 = 互斥**：若拾取没发生 S2 也会是 ×2
  → 自见证对；若大师弓没携带 infinite 属性也会是 ×2 → 证明三合一弓
  **确实继承了无限**。
- **S6 init 失败 ×2**（scratch 资产）：配方原料 "power_bowz" → "uses
  unknown ingredient item" 点名 / 产物 "master_bowz" → "outputs
  unknown item" 点名。
- **S1 第 55 次结构性免疫**：新物品不在默认 hotbar + 图标独立 PNG +
  配方只在 UI = 无输入零活动。
- 力量（8）/速度（20）数值由内容钉死；发射机制 = P21/77/79 共享已证
  路径。

### 变更文件

- `assets/data/items/master_bow.json`（新）、
  `assets/data/recipes/master_bow.json`（新）、
  `assets/textures/items/master_bow.png`（新，工具链产物）、
  `tools/generate_item_icons.py`
- `tests/test_item.cpp`（+2）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`AGENTS.md`、`docs/archive/phase-82.md`

### 已知问题（本阶段）

- 弓强化条"无组合弓"**偏离被消费**：master_bow = 力量+速度+无限三叠
  加组合弓（`[Phase 79]` → `[Phase 82]`）；穿透/火焰仍无。
- master_bow 的三效果由三把独立弓合成（P61 链范式）——需要玩家先合成
  三把强化弓（材料门槛高 = 顶点装备定位，合理）。

### 下一步

- P83 = 信标（成本第 3：方块实体系统 + 光束渲染 + 范围效果 + format）。
