# Phase 55 细节存档

> 范围（用户"熔岩湖不着急，先记录下来，留到很久的未来。下一步先进行
> 建议2的扩展。" = 建议 2，代理定案为最小完整版：**多掉落内容扩展 =
> 3 方块 MC 语义内容、零代码**，消费 P52 遗留"多掉落内容扩展"；
> worldgen 熔岩湖 = 用户明示"留到很久的未来"）：
> ① `leaves` 单条目 `chance_drop`（apple）→ `chance_drops` 双条目
>    列表（stick + apple，MC 1.14+ 语义）；
> ② `lapis_ore` → 2..4 lapis（MC 同款数量分布）；
> ③ `glowstone` → 2..4 glowstone（MC 同款数量，物品 = block item）；
> ④ 3 个内容钉死单测（加载真实资产文件断言字段精确值）。

## 阶段报告

- **`[2026-10-13, Phase 55]`** 多掉落内容扩展
  — 967/967（+3），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 29 次
  结构性免疫免重锚 + S2 破坏青金石矿【lapis ×3 与预计算相等】+ S3
  破坏荧石【glowstone ×3】+ S4 破坏树叶【leaves ×1 + stick ×1】；
  每场景无输入对照 ×2 字节一致 + 输入 ×2 归一化存档一致）。

### 实现内容

**纯数据，零代码**（`assets/data/blocks/*.json`）：
- `leaves.json`：`chance_drop`（单条目 apple）整体移除 →
  `chance_drops: [{stick,1,0.02},{apple,1,0.0078125}]`。依据：
  Minecraft 1.14+ 橡木树叶破坏额外掉木棍 2% + 苹果 0.5%（Fandom/
  wiki 口径 2%）；本项目无 sapling = **文档化偏离**（主掉落仍是树叶
  block item）；苹果保留 P49 项目值 1/128（本阶段不重调已定值）。
- `lapis_ore.json`：加 `drops_count: 2` +
  `chance_drops: [{lapis,1,0.5},{lapis,1,0.5}]` → 每次破坏 2..4 lapis
  **均匀分布**（base 2 + 两个独立 50% roll = MC 青金石矿 2-4 的精确
  分布）。
- `glowstone.json`：同款模式 → 2..4 glowstone（MC 荧石 2-4 数量；
  本项目无粉尘物品，掉落 = block item）。

**内容钉死单测**（`tests/test_block.cpp`，+3）：
- `Phase55LeavesDropContent` / `Phase55LapisOreDropContent` /
  `Phase55GlowstoneDropContent`：`AssetManager::setRoot("assets")` +
  `loadDirectory("data/blocks")` 加载**真实资产**，断言 drops /
  dropsCount / chanceDrops 逐字段精确值（leaves 还断言单条目字段已
  清空）。动机：概率笔误（0.2 vs 0.02）能过解析与 init 交叉校验，
  只有内容级断言能在单测层抓住；测试须从仓库根目录运行（与游戏
  同款相对资源路径约定，失败信息明示）。

### 变更文件

- `assets/data/blocks/leaves.json`、`lapis_ore.json`、`glowstone.json`
  （掉落字段）
- `tests/test_block.cpp`（+3 内容测试）
- 文档：`AGENTS.md`（现状 P55 / 967 / 第 29 次免疫）、`docs/ROADMAP.md`
  （P55 条目 + 索引）、`docs/KNOWN_ISSUES.md`（P52 关闭项备注更新：
  leaves 迁入列表后单条目字段无现役使用者）、`docs/PITS.md`（+3）、
  `docs/CONTENT_GUIDE.md`（chance_drops 使用者清单 + 合并提醒）、
  `docs/GAMEPLAY.md`（青金石 2–4 / 树叶木棍 2%）、本文件

### 构建与验证结果

- `-Wall -Wextra -Wpedantic -Werror` 恒 **0 警告**。
- 测试 **967/967**（+3 内容钉死）。
- **E2E 批跑全绿**（`/tmp/opencode/p55e2e/p55_e2e.log`，EXIT=0）：
  - **S1 base18 门**：P55 ×2 新世界 60f 无输入字节一致 + == base18 →
    **第 29 次结构性免疫（免重锚）**【掉落只作用于破坏，fresh 世界
    零破坏 = 无输入零活动】+ 红心 0。
  - **场景几何**（P52 已验证瞄准复用 + p55scan 复验）：玩家
    (22.5,45,0.5)、yaw 0、pitch atan2(−0.12,5) = −0.0239954；真实
    raycastBlock 对 3 个目标全 HIT (22,46,−5) +z 面 dist 4.5013 <
    reach 5。目标格天然 air（列顶 y=44）= 场景存档 block diff 放置；
    破坏后射线到 reach 限（t=5 → z≥−4.5）内只余 air = 按住余量不
    误破。玩家持 iron_pickaxe（break_speed 4）：破坏时长
    1.5×hardness÷4 = lapis 1.125 s / glowstone 0.1125 s /
    leaves 0.075 s → 按住 1.7 / 0.7 / 0.6 s（+~0.55 s 余量）。
  - **roll 预计算**（p55scan，真实 `rollChanceDropEntry`，seed 9，
    cell (22,46,−5)）：entry0@0.5 **PASS** / entry1@0.5 **FAIL** →
    lapis×3、glowstone×3（同物品合并 = 单实体 count 3）；
    entry0@0.02 **PASS** / entry1@1/128 **FAIL** → leaves×1 +
    stick×1（不同物品不合并 = 2 实体）。
  - **S2 青金石矿** / **S3 荧石** / **S4 树叶**（各 420f 输入 ×2 +
    240f 无输入对照 ×2）：对照 ×2 存档字节一致（目标完好 + 零实体）+
    输入 ×2 **归一化存档一致**（剥实体 age = 真实 dt，确定性权威）+
    存档精确（目标格从 diff 消失 + 实体列表与预计算**逐项相等**：
    S2 `[{lapis,3}]`、S3 `[{glowstone,3}]`、S4 `[{leaves,1},{stick,1}]`，
    落点列 (22.5±0.3, 44.9..46.6, −4.5±0.05)，±0.125 x 散布复现
    P49/P52 观察）。
  - 无 S5：P55 无新解析/新 config（字段校验全部是 P49/P52 存量）。

### 关键决策

- **leaves 迁入列表 = 消费"两字段并存示范"使命**：P52 特意保留 leaves
  单条目作示范；P55 的 MC 语义扩展需要多条目，示范使命由**单测**
  （合成 JSON 解析两字段 + 互斥拒绝）继续承担，内容层不再为示范
  牺牲 MC 对齐。
- **苹果概率不重调**：MC 是 0.5%（1/200），项目自 P49 起用 1/128；
  本阶段 = 内容扩展而非重调，保持已定值并在 CONTENT_GUIDE 注明。
- **glowstone 的 E2E 用放置而非天然**：glowstone 不在 worldgen 矿石
  列表（无天然来源），场景存档放置 = 与 lapis/leaves 同几何同流程
  （也证明数据驱动：同一代码路径三块全过）。
- **同物品掉落合并是断言语义**：kMergeDistance 0.75 → lapis/glowstone
  的 2..4 在存档里是**单实体 count**（不是多个实体）；PITS +1。
- **熔岩湖推迟**：用户明示"留到很久的未来"，SESSION_CONTEXT 已记录
  （排在火系统之后；世界生成变更 → 届时需 base18 重锚）。

### 已知问题（本阶段关闭 0 / 新增 0 / 备注更新 1）

- P52 关闭项备注更新：单条目 `chance_drop` 在 `[Phase 55]` leaves 迁入
  列表后**无现役内容使用者**（系统 + 单测保留）。
- 活跃项无变化（详见 KNOWN_ISSUES.md）。

### 下一步建议

- 金苹果后续（吸收层，小）/ 脱离熔岩残留火（火系统，较大）/
  多掉落内容再扩展（如 gravel→flint 需新物品，属新内容阶段）。
- **worldgen 熔岩湖 = 用户明示"留到很久的未来"**（排在火系统之后）。
