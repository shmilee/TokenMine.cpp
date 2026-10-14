# Phase 69 细节存档

> 范围（用户"按建议，继续下两个 Phase" = 建议 1，代理定案为最小
> 完整版：**镐分类 = 石类/矿类分块效率**，消费 P68 演进后 P11 条的
> 镐分类遗留；**零核心代码** = 纯内容，P62/P68 同款路径）：
> 3 镐各加 `fast_against` [stone, cobblestone, coal_ore, iron_ore,
> gold_ore, diamond_ore, lapis_ore, glowstone] ×2.0（项目石矿类全
> 集；obsidian 刻意排除）。
> **无 format 变更**（format 10）、**无新物品/配方/图标**（改 3 个
> 既有 JSON）。

## 阶段报告

- **`[2026-10-14, Phase 69]`** 镐分类 — 1063/1063（+2），`-Werror`
  恒 0 警告；E2E 批跑全绿（S1 第 42 次结构性免疫【无重锚】+ S2 铁镐
  破石【0.28125 s < 0.42 s hold = 倍率证明，掉 cobblestone×1，×2
  savenorm】+ S2b 铁镐对木【0.75 s > 0.55 s hold 不破 = 无泄漏，×2
  RAW 存档+截图】+ S6 init 失败 ×2）。

### 实现内容

**内容（零核心代码——机制 = P67）**：
- `assets/data/items/{wooden,stone,iron}_pickaxe.json`：各加
  `fast_against: ["stone","cobblestone","coal_ore","iron_ore",
  "gold_ore","diamond_ore","lapis_ore","glowstone"]` +
  `fast_against_multiplier: 2.0`（MC 镐对石类/矿类 2×；**项目石矿
  类全集**——8 块，本项目无其它石类）。flat 档 `tool.break_speed`
  2/3/4 **不变** → 石矿类有效速度 4/6/8（其余块 = flat 档）。
  - **obsidian 刻意排除**（文档化偏离）：MC 铁镐**不能破**黑曜石
    （需钻石镐），项目无钻石镐——排除后黑曜石 = flat 镐速【铁镐
    1.5×10/4 = 3.75 s】，是最接近"需要更好的镐"的近似；若纳入则
    铁镐 1.875 s 即破，偏离 MC 更远。
- 无新物品/配方/图标（镐已存在，P62/P67 内容不动）。

### 变更文件

- 改：`assets/data/items/{wooden,stone,iron}_pickaxe.json`（各 +2
  字段）
- 改：`tests/test_item.cpp`（+2：Phase69PickaxeFastAgainstContent
  三镐 8 块精确集钉死 + flat 档不变 / Phase69PickaxeEffectiveBreak
  Speed 真实资产纯函数集成：铁镐对 stone/cobblestone/diamond_ore/
  glowstone = 8.0、对 wood/obsidian = 4.0、木镐对 stone = 4.0）
- 文档：ROADMAP、KNOWN_ISSUES（工具无完整分块类型效率 → 演进：镐
  类别落地，仅剩"无效率工具慢 10×"规则）、MECHANICS（方块交互节：
  分块类别加镐 + obsidian 排除）、GAMEPLAY（工作台镐条目）、
  AGENTS.md、本存档、SESSION_CONTEXT.md（本地）

### 测试

**1063/1063（+2）**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_item.cpp`（+2）：
  - Phase69PickaxeFastAgainstContent——3 镐循环：breakSpeed 2/3/4
    不变 + fastAgainst 恰 8 块（精确集断言）+ multiplier 2.0。
  - Phase69PickaxeEffectiveBreakSpeed——**真实资产**上的纯函数集成：
    铁镐 stone/cobblestone/diamond_ore/glowstone = 8.0（2× 生效）、
    **wood = 4.0 / obsidian = 4.0**（清单外 = flat 档，无泄漏）、
    木镐 stone = 4.0（木档 2×2）。
- 解析拒绝与机制 = P67 已覆盖（本阶段零核心代码），init 交叉校验
  走 E2E S6（Game init 级，P64–P68 同款）。

### 构建与验证结果

- 构建 0 警告；测试 1063/1063。
- E2E（一次全绿；判别设计 = **hold 时长卡在 ×2 与无 ×2 的破坏时间
  之间，两结果互斥**）：
  - **S1 第 42 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节
    一致（P69 只改 3 个既有 tool 物品——镐不在默认背包 = 零世界状
    态，无新物品/配方/图标，block atlas 未动 = 无重锚）。
  - **S2 铁镐破石 = 倍率证明**（420f，输入：aim 锚定存档零鼠标 +
    左键 hold **0.42 s**）：stone 放 P52 已验证格 (22,46,-5)；×2 后
    有效速度 4.0 × 2.0 = 8.0 → 1.5 × 1.5 / 8.0 = **0.28125 s < 0.42 s
    hold 破**；**若无 ×2 = 0.5625 s > hold 会存活——两结果互斥，倍
    率被证明**（real-dt 计时 fps 无关）；破后余量 0.14 s 射线越过
    reach 5 = 无 PITS-7 续破；终态 blocks = {} + **恰 1 个
    cobblestone×1 实体**（stone 掉 cobblestone = P61 材料链，与 P62
    石镐场景同款可区分性）停 **P59/P61/P67/P68 证明落点 (22.5, 45,
    -4.5)**（精确值断言）、fires = []，×2 savenorm 字节一致。
  - **S2b 铁镐对木 = 无泄漏对照**（同格 wood、hold **0.55 s**）：
    flat 档 1.5 × 2.0 / 4.0 = **0.75 s > hold** → 完好、零实体；
    **若 ×2 误越清单 = 0.375 s < hold 会破——互斥**；终态零 real-dt
    字段 → **存档 + 截图 ×2 RAW 字节一致**。
  - **S6 init 失败 ×2**（scratch 资产）：iron_pickaxe.json
    fast_against ["stonez"] → "item 'iron_pickaxe' fast_against[0]
    references the unknown block 'stonez'"（grows_on 纪律）；
    iron_pickaxe.json fast_against_multiplier 0 → "'fast_against_
    multiplier' must be a number > 0, got 0.000000"（严格配对校
    验）。

### 已知问题（演进后）

- KNOWN_ISSUES「工具无完整分块类型效率」（P11 → P67 → P68 → P69）：
  镐类别落地（石矿类 ×2 = 项目石矿类全集，obsidian 排除 = 文档化偏
  离）；**仅剩"无效率工具慢 10×"规则**未落地（本项目"无效率" =
  flat 档，不慢 10×）；方块侧无类别字段（分类知识在物品侧块名列
  表里）。

### 下一步候选（P70+ 定案输入）

- 同批已定：P70 = 建议 2 = 火接触伤害（玩家站火里受伤，消费火 v2
  条"无接触伤害"）。
- 之后候选：campfire（火把交互扩展：烤食物 + 座位）/ 吸收层两分量
  模型（P56 遗留大改）/ 熔岩湖 worldgen（用户"留到很久的未来"，勿
  选）。
