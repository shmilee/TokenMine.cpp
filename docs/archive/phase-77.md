# Phase 77 细节存档

> 范围（用户"按你的建议，继续下两个 phase" = 建议 1，代理定案为最小
> 完整版：**弓 v3 = 无限弓（Infinity）**——MC 弓的 Infinity 附魔的
> 数据驱动替身（项目无附魔系统 = 独立物品），全量消费 P42 遗留"无箭
> 矢强化"的 Infinity 部分）：物品字段 `infinite` + 内容
> `infinite_bow`（bow + diamond 合成）+ 发射不消耗弹药（仍须持有
> ≥1 支箭）。

## 阶段报告

- **`[2026-10-15, Phase 77]`** 弓 v3：无限弓 — 1109/1109（+5：解析
  3 + 内容钉死 2），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 50 次
  结构性免疫【无重锚】+ S2 无限弓俯角连射【×2 savenorm + EXACT】+
  S2b 普通弓对照【×2 savenorm + EXACT】；无 S6 = 零新 init parse
  面）。

### 实现内容

**数据（1 新物品字段 + 2 新内容文件 + 1 新图标）**：
- `src/item/item.{hpp, item_registry.cpp}`：新可选物品字段
  **`infinite`**（严格 bool，缺省 false）——发射器的 Infinity：发射
  **不消耗**弹药，但仍须**持有至少 1 个** `ammo` 物品（MC Infinity
  规则：1 支箭永不耗尽）。解析纪律：**要求 `ammo` 非空**——无 ammo
  的发射器本就无限（P21 前语义），flag 在其上是 no-op typo（ammo 纪
  律点名拒绝）。
- 内容：`assets/data/items/infinite_bow.json`（bow 同款数据：
  shoot_speed 15 / shoot_damage 5 / attack_damage 1 / max_stack 1 /
  ammo arrow + `infinite: true`）+ `assets/data/recipes/infinite_bow.
  json`（**shapeless bow + diamond → 1**——Infinity 附魔的可合成替
  身，diamond = 项目顶级材料）+ 图标 `infinite_bow()`（`tools/
  generate_item_icons.py`：弓 + 弓心的 3×3 蓝色钻石微光，新 salt
  91/92，既有图标确定性再生零变化）。
- **不在默认 hotbar**（结构性免疫，P67/P68 工具范式）。

**核心（1 处）**：
- `Game::fireBow`（game.cpp）：弹药查找逻辑不变（找到第一个持有 ammo
  的槽 = 起射门），**`infinite` 时跳过消耗**（`if (!item.infinite)
  take(...)`）。无 ammo 不能起拉 / 中途耗尽取消 = 既有语义不变（
  无限弓同样适用——0 支箭 = 射不出，1 支箭 = 无限）。

**测试（1104 → 1109）**：
- `ParsesInfinite` / `InfiniteDefaultsToFalse` / `RejectsBadInfinite`
  （非 bool 拒；无 ammo 的发射器带 infinite 点名拒）。
- `InfiniteBowContentPinned`（真实 JSON：数据字段全钉）+
  `InfiniteBowRecipePinned`（真实配方：bow + diamond → 1）。
- -Wall -Wextra -Wpedantic -Werror 0 警告。

### E2E（全绿；批跑脚本 `/tmp/opencode/p77_e2e.zsh`）

- **射出的可观测性设计**：箭**不入存档**（瞬态实体，P21）——"射出
  与否"无可存档断言。方案 = **俯角连射 + P42 插地拾取**：玩家 pitch
  −1.53（近垂直向下）射 2 箭 → 箭插脚边地面 → 拾取半径 1.0 内**返
  还 1 支箭/箭**（拾取只返还射出的箭 = 射出证明）。
- **S2 无限弓**（输入 600f ×2 savenorm + EXACT）：infinite_bow +
  arrow ×2，2 次拉弓（右键按住 1.7 s > 满蓄力 1.6 s，间隔 1.9 s）
  → 2 射 **0 耗** + 2 拾取返还 → 终态 **arrow ×4**。
- **S2b 普通弓对照**（同输入）：bow + arrow ×2 → 2 射 2 耗 + 2 拾取
  返还 → 终态 **arrow ×2**（不变——**自见证对**：若拾取机制没发生，
  S2 也会停在 2；4 vs 2 = 互斥）。
- **S1 第 50 次结构性免疫**：新物品不在默认 hotbar + 图标独立 PNG
  （无 block-atlas tile）+ 配方只在 UI + fireBow 跳过消耗需选中弓 =
  无输入零活动。
- **首轮失败与 PITS +1**：射击动作用了**左键**（`mousedown 1`）——
  P24 起"发射器右键恒为拉弓"，左键 = 近战挥击（合法操作、无日志）
  → 20 次挥拳无箭，终态与"没射"同值。修 = `mousedown 3`。

### 变更文件

- `src/item/item.hpp`、`src/item/item_registry.cpp`
- `src/core/game.cpp`（fireBow 消耗跳过）
- `assets/data/items/infinite_bow.json`（新）、
  `assets/data/recipes/infinite_bow.json`（新）、
  `assets/textures/items/infinite_bow.png`（新，工具链产物）、
  `tools/generate_item_icons.py`
- `tests/test_item.cpp`（+5）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/CONTENT_GUIDE.md`、`docs/PITS.md`（+1）、
  `AGENTS.md`、`docs/archive/phase-77.md`

### 已知问题（本阶段）

- 新文档化偏离（弓/箭节）：**Infinity = 独立物品而非附魔**（项目无附
  魔系统）；无附魔等级/概率/其它弓附魔（力量/速度/穿透 = 无）。
- 既有弓/箭条不变（箭不入存档 / 不自伤 / 箭无纹理 / 手部 billboard /
  拾取返还 / 释放共享寿命 / 弹药槽序 / 右键恒拉弓）。

### 下一步建议

1. **营火四槽图标全渲染**（P75 遗留视觉项：只渲染第一个非空槽图标 →
   4 个图标排布，纯渲染层，无输入基线须论证免疫）。
2. 弓 v4（P42 遗留的其它强化：力量 = 伤害倍率 / 速度 = 射速，同
   `infinite` 范式的数据驱动字段，低成本）。
3. 信标（吸收第二来源，需方块实体系统 = 高成本）。
4. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
