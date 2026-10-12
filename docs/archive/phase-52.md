# Phase 52 细节存档

> 范围（用户"按候选顺序继续后面两个 phase" = 候选 2，代理定案为最小
> 完整版：**加权多掉落列表** = `chance_drops` 多条目独立 roll + grass 改
> MC 语义；放置限制/熔岩后续留后续）：
> ① block 可选 `chance_drops` 数组（元素 = 与 P49 单条目 `chance_drop`
>    相同的严格三元组 `{item,count,chance}`；与 `chance_drop` **互斥**）；
> ② 每条目独立 roll（`rollChanceDropEntry`，列表专用 base salt + 条目索引
>    golden-ratio 推进 + splitmix64 finalize = 同格各条目去相关）；
> ③ breakBlock 循环条目 + init 每条目复用 drops 纪律（存在 + max_stack）；
> ④ 内容：grass 改 MC 语义（`drops: dirt` + `chance_drops: [wheat_1 ×1 @
>    0.125]`），leaves 保留单条目 = 两字段并存示范。

## 阶段报告

- **`[2026-10-13, Phase 52]`** 加权多掉落列表
  — 942/942（+21），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 26 次
  结构性免疫免重锚 + S2 破坏草输入运行【dirt ×1 + wheat_1 ×1，归一化
  ×2 一致】+ S3 init 失败 ×2 干净报错）。

### 实现内容

**`chance_drops` 纯数据**（`src/block/block.hpp` + `block_registry.cpp`）：
- `struct ChanceDrop { std::string item; int count; double chance; }`
  + `BlockProperties::chanceDrops`（`std::vector<ChanceDrop>`，缺省空）。
- 解析：`chance_drops` 可选**数组**；非数组 / 空数组（no-op 拒绝，ammo
  纪律）/ 任一元素非对象 / 未知键 / 缺三元组 → 逐条目点名
  `chance_drops[i]` 拒绝。
- 校验 pass：**互斥**——`chance_drops` 与 `chance_drop` 同时写 = 拒绝
  （"write exactly one of the two"）；每条目 item 非空、count ∈ [1,64]、
  chance ∈ (0,1]（与单条目同款范围）。

**每条目独立 roll**（`src/block/chance_drop.{hpp,cpp}`）：
- `rollChanceDropEntry(seed, x, y, z, entryIndex, chance)` 确定性纯函数
  （同 `rollChanceDrop` 契约：chance ≤0 恒假、≥1 恒真、per-cell 确定）。
- 盐设计：列表**专用 base salt** `kChanceDropsSalt = 0x7A1C0E5B3F9D2E46`
  （与单条目 `kChanceDropSalt` 分离 = 两套系统互不追踪）；per-entry salt
  = `splitmix64(kChanceDropsSalt + entryIndex × GOLDEN64)`（golden-ratio
  `0x9E3779B97F4A7C15` 推进，growth-roll 同款）→ 索引保证输入按条目
  变化（PITS #4：splitmix64 是纯 finalizer，输入必须变）= 同格各条目
  去相关。位置哈希 + finalizer 链与单条目同款（`hash3(x,y,z,entrySalt)`
  → splitmix64 → 顶 53 位 → [0,1)）。

**breakBlock 集成**（`src/core/game.cpp`）：单条目 `chance_drop` 块不动
（leaves 仍走）；其后新增 `chance_drops` 循环——每条目 `rollChanceDropEntry`
通过则 spawn 1 个 ItemEntity（条目 item/count，格中心 dropPos）。

**init 校验**（`src/core/game.cpp`）：chance_drop/chance_drops 校验循环
重构——`continue` 条件改为"两者皆空"（修一个真 bug：原 `continue` 只看
单条目会跳过"只有列表"的块）；每条目复用 drops 纪律（物品存在 + count ≤
max_stack，typo 点名 `chance_drops[i]` 拒启）。

**内容**（`assets/data/blocks/grass.json`）：grass 改 **MC 语义**——
`drops: wheat_1` → **`drops: dirt`** + `chance_drops: [{item: wheat_1,
count: 1, chance: 0.125}]`（草掉泥土 + 偶尔种子；0.125 = MC 草掉种子
基础概率）。种子来源 = 草概率掉 + 未熟作物（`wheat_1..6`）掉落。`leaves`
保留单条目 `chance_drop` = **两字段并存示范**。

**无 format 变更**（format 9；掉落 = 实体走既有 `entities`）。

### 变更文件

- `src/block/chance_drop.{hpp,cpp}`：+`rollChanceDropEntry`（列表 base salt
  + 条目索引 golden-ratio + splitmix64 finalize）。
- `src/block/block.hpp`：+`struct ChanceDrop` + `BlockProperties::chanceDrops`。
- `src/block/block_registry.cpp`：`chance_drops` 解析（数组 + 逐条目严格
  三元组）+ 校验 pass（互斥 + 每条目范围）。
- `src/core/game.cpp`：init 校验循环重构（continue 条件 + 每条目 drops
  纪律）+ breakBlock `chance_drops` 循环。
- `assets/data/blocks/grass.json`：`drops: dirt` + `chance_drops` 条目。
- `tests/test_block.cpp`（+13 解析）/ `tests/test_chance_drop.cpp`（+8
  entry roll）。
- 文档：CONTENT_GUIDE（方块模板 +`chance_drops` 节 + "多掉落"指引更新）/
  KNOWN_ISSUES（关闭 2）/ ROADMAP（P52 行 + 索引）/ AGENTS.md（现状）。

### 测试

942/942（+21）：
- **test_block.cpp（+13 解析）**：ParsesChanceDrops（2 条目数组）/
  ChanceDropsAbsentByDefault / MustBeArray / RejectsEmptyArray /
  RejectsNonObjectEntry / RejectsUnknownField / RequiresItem / RequiresCount /
  RequiresChance / CountOutOfRange / ChanceOutOfRange / ValidatesEveryEntry
  （坏第 2 条目也点名）/ MutuallyExclusiveWithChanceDrop。
- **test_chance_drop.cpp（+8 entry roll）**：ChanceZeroNeverDrops /
  ChanceOneAlwaysDrops / DeterministicPerCellPerEntry / EntriesAreIndependent
  （同格 entry 0 vs 1 有分歧格）/ EntriesAreIndependentOfSingleEntry（列表
  条目 vs 单条目 roll 有分歧格）/ DependsOnSeed / EmpiricalRateNearChance
  （100×100 @ 0.25 落 0.18..0.32 带 = 均匀 roll）/ EntryIndexChangesThePattern
  （索引必须改 roll = PITS #4 反例守护）。

### 构建与验证结果

- `-Wall -Wextra -Wpedantic -Werror` 恒 **0 警告**。
- **E2E 批跑全绿**（`/tmp/opencode/p52e2e/p52_e2e.log`，EXIT=0）：
  - **S1 base18 门**：P52 ×2 新世界 60f 无输入字节一致 + == base18 →
    **第 26 次结构性免疫（免重锚）**【grass drops 变数组不改 worldgen/atlas
    = 无输入零活动】+ 红心 0。
  - **S2 破坏草输入**（300f，预瞄准存档 + 按住左键 1.2s @ 帧时 2.9s 锚定
    `Entering main loop`；草 (22,46,−5) + 4 格射线走廊 (22,46,−1..−4) air；
    瞄 yaw 0 / pitch atan2(−0.12,5) = −0.0239954，**真实 raycastBlock 探针
    p52ray 先验证** hit=(22,46,−5) n=(0,0,1) dist 4.501 < reach 5；
    `chance_drops[0]` roll 在 (22,46,−5) seed 9 **PASS**【C++ 探针 p52roll
    预计算——该 −Z 轴上唯一通过格，z=−2..−7 中仅 −5】）：存档 ×2
    **归一化一致**（剥实体 age = 真实 dt，确定性权威）+ 存档精确（草消失 +
    **dirt ×1 + wheat_1 ×1** 落 (22.625/22.375, 45, −4.5) = 确定性 ±0.125
    散布，hunger 20 / health 20）+ 无输入 ctl ×2 一致（草存续、零实体）。
  - **S3 init 失败 ×2**（scratch 资产副本，仓库未动）：`chance_drops` +
    `chance_drop` 同时写 → rc=1 + "mutually exclusive"；
    `chance_drops[0].item = "nope"` → rc=1 + "chance_drops[0] names unknown
    item"。

### 已知问题（本阶段关闭 2）

- ~~草方块掉小麦种子（不再掉泥土）~~ → grass 改 `drops: dirt` +
  `chance_drops: [wheat_1 ×1 @ 0.125]`（MC 语义）。
- ~~概率次级掉落每方块至多一个条目~~ → `chance_drops` 列表（多条目独立
  roll）。

### 坑位（PITS +1）

- **init 校验循环的 `continue` 只看单条目**：P49 的循环
  `if (chanceDropItem.empty()) continue;` 在 P52 加列表后会把"只有列表、
  无单条目"的块（= grass）**整体跳过** → 列表的 init 校验静默失效。
  教训：给"二选一"的两个字段加校验时，`continue` 条件必须覆盖**两者皆空**，
  且每个字段的校验各自独立成块。

### 下一步

- 作物放置限制（`plant_on` 类字段，消费 P50 遗留"放置不受限"）。
- 金苹果后续（吸收层）。
- 熔岩后续（worldgen 熔岩湖 / 燃烧·下沉 / 蒸汽，消费 P51 边界）。
