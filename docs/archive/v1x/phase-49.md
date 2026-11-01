# Phase 49 细节存档

> 范围（用户"按建议继续下两个阶段" = 建议 1，代理定案为最小完整版：
> **金苹果** = 概率次级掉落系统（树叶偶尔掉苹果，确定性 per-cell roll）
> + 苹果食物 + 金苹果（shaped 镶金配方 + 吃时施加再生效果，接 P46 框架）；
> 耕地/水/光照生长条件 = 下阶段（P50），加权多掉落列表/熔岩留后续）：
> ① block 新属性 `chance_drop`（`{item,count,chance}` 严格三元组 +
> 确定性 roll 纯函数 + init 硬校验）；
> ② item 新字段 `grants_effect`（`{name,duration}` 严格配对，仅食物，
> 进食完成时经 P46 `StatusEffects::apply` 施加）；
> ③ 内容：`leaves` 加 1/128 掉苹果 + `apple`/`golden_apple` 物品 +
> `golden_apple` shaped 配方（GGG/GAG/GGG）+ 2 图标；
> ④ 无 format 变更（format 8：实体走既有 `entities`、效果走既有
> `player.effects`）。

## 阶段报告

- **`[2026-10-13, Phase 49]`** 金苹果（概率掉落 + 吃时给效果）
  — 891/891（+25），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 23 次
  结构性免疫免重锚 + S2 概率掉落全链路【树叶破 → leaves×1 + apple×1】
  + S3 金苹果进食全链路【hunger 12→16 + regeneration 授予】
  + S4 init 失败干净报错）。

### 实现内容

**block schema（`block.hpp` + `block_registry.{hpp,cpp}`）**
- 3 个新可选字段：`chanceDropItem`（次级掉落物品名，空 = 无概率掉落
  默认）、`chanceDropCount`（[1,64]）、`chanceDropChance`（(0,1]）。
- 严格三元组（`parseProperties`）：`chance_drop` 必须是对象（否则拒绝）；
  未知键拒绝；`item` 非空、`count` ∈ [1,64]、`chance` ∈ (0,1]（1.0 =
  保底掉落合法）。
- **init 硬校验**（`Game::init`，drops 纪律）：`chanceDropItem` 必须是
  已注册物品且 `chanceDropCount` ≤ 其 max_stack（typo = 启动失败点名）。

**roll 纯函数（`src/block/chance_drop.{hpp,cpp}`）**
- `rollChanceDrop(seed, x, y, z, chance)`：`chance ≤ 0` 恒 false、
  `chance ≥ 1` 恒 true（边界 no-op）；否则
  `r = (splitmix64(seed + hash3(asU64(x), asU64(y), asU64(z),
  kChanceDropSalt)) >> 11) / 2^53`（[0,1)）与 `chance` 比较。
- 专用 salt `0x01B3E0D5E77C2A93`（与作物生长/worldgen 的 roll 去相关）；
  位置哈希保证同格同果、邻格去同步（单测有实证率断言）。

**item schema（`item.hpp` + `item_registry.{hpp,cpp}`）**
- 2 个新可选字段：`grantsEffect`（效果名）、`grantsEffectDuration`
  （秒）。
- 严格配对（`parseGrant`，on_hit_effect 纪律）：`grants_effect` 必须是
  对象；未知键拒绝；`name` 非空字符串；`duration` 数字 > 0；**仅食物**
  （`restore_hunger > 0`）——可饮用物品喝下清效果、通道互斥（非食物带
  该字段 = 解析拒绝，dead field）。
- 顶层键白名单加 `grants_effect`；**init 硬校验**（ammo 模式）：
  效果名必须已在 EffectRegistry 注册。

**Game 集成（`game.cpp`）**
- `breakBlock`：主掉落之后，若 `chanceDropItem` 非空且
  `rollChanceDrop(m_generator->seed(), ...)` 通过 → 同方块中心再生成
  一个 `ItemEntity`（主/次掉落相互独立，一次破坏最多两个掉落实体）。
- `updateEatChannel` 完成分支（食物侧，恢复饥饿同处）：若
  `grantsEffect` 非空 → `m_playerEffects.apply(m_effects.idForName(...),
  duration)`——与 mob `on_hit_effect` 同一条 P46 路径。

**内容（全 JSON + 纹理）**
- `leaves` 加 `"chance_drop": {"item":"apple","count":1,"chance":0.0078125}`
  （1/128；MC 橡树叶掉苹果 ~0.5% 同款量级）。
- 2 物品：`apple`（material，`restore_hunger` 4 = MC，max_stack 64）/
  `golden_apple`（material，`restore_hunger` 4 + `grants_effect:
  {name: regeneration, duration: 10}`）。
- 配方 `golden_apple`：shaped `GGG/GAG/GGG`（G=`gold_ingot`，A=`apple`，
  MC 同款 3×3 镶金；位置必须精确——单测验证缺 1 金/苹果不在中心不中）。
- 2 图标：`tools/generate_item_icons.py` 加 `apple`（红果 + 茎叶）/
  `golden_apple`（金果 + 底缘暗金）函数 + ITEMS 表项。

### 变更文件

- 核心：`src/block/block.hpp`（3 字段）、`src/block/block_registry.cpp`
  （解析+三元组校验）、`src/block/chance_drop.{hpp,cpp}`（新，roll
  纯函数）、`src/item/item.hpp`（2 字段）、`src/item/item_registry.{hpp,
  cpp}`（parseGrant + 白名单）、`src/core/game.{hpp,cpp}`（breakBlock
  次级掉落 + 进食施加 + init 双硬校验）、`CMakeLists.txt`（chance_drop.cpp
  + test_chance_drop.cpp）。
- 数据：`assets/data/blocks/leaves.json`（chance_drop）、
  `assets/data/items/{apple,golden_apple}.json`（新）、
  `assets/data/recipes/golden_apple.json`（新）。
- 纹理：`assets/textures/items/{apple,golden_apple}.png`（生成器）。
- 工具：`tools/generate_item_icons.py`（apple/golden_apple）。
- 测试：`tests/test_chance_drop.cpp`（新，6）、`tests/test_block.cpp`
  （+9）、`tests/test_item.cpp`（+7）、`tests/test_crafting_grid.cpp`
  （+3）。
- 文档：CONTENT_GUIDE（chance_drop/grants_effect 字段 + P49 专节 +
  效果来源 2→3）、GAMEPLAY（苹果与金苹果节 + 再生来源行）、
  KNOWN_ISSUES（+4 更新 2）、PITS（+3）、ROADMAP（P49 行 + 索引）、
  AGENTS.md、本文件。

### 测试

891/891（+25），`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_chance_drop`（6）：chance 0 恒不掉 / 1 恒掉（边界 no-op）、
  同格确定性 ×10 + 8×8 格非常数（PITS #4 防御）、seed 去相关（两 seed
  必有分歧格）、经验率 0.25 ∈ [0.18,0.32]（100×100 格，~16σ 带宽）、
  负坐标良定义（二补码双射）。
- `test_block`（+9）：chance_drop 三元组解析/缺省空/非对象/未知键/
  缺 item/缺 count/缺 chance/count 越界（0 与 65）/chance 越界（0 与
  1.5，1.0 合法）。
- `test_item`（+7）：grants_effect 解析（golden_apple 全字段）/缺省空/
  非对象/未知键/缺 name（含显式空串）/duration 非正（缺/0/负）/仅食物
  （非食物拒绝 + 可饮用拒绝）。
- `test_crafting_grid`（+3）：8 金围 1 苹果全格匹配并消耗干净/缺 1 金
  不中/苹果在角不中（位置必须精确）。

### 构建与验证结果

- 构建：0 警告（-Werror）。
- **S1 base18 门**：P49 ×2 + P48 代码参考构建（own worktree @2eff9f8）
  ×1 新世界 60f **三方字节一致 = 第 23 次结构性免疫（免重锚）**——
  chance_drop 只在破坏时 roll（需输入）、grants_effect 只在完整吃下时
  施加（需输入），无输入运行结构性免疫；skygate + 红心 0。
- **S2 概率掉落**（240f，正午 still mobs off）：场景 = p27far 派生 +
  leaves 于 (22,46,−4) + 射线路径 3 格 air；瞄准 yaw 0 / pitch −0.03
  为**离线预计算**（Python 复刻 DDA + 与 C++ `rollChanceDrop` 探针
  **全区域对拍**后选定：该格 r=0.0025003 < 1/128 PASS、t=3.502 <
  kReach 5）。s2ctl ×2 无输入 = 树叶完好 + 无实体 + 字节一致；
  s2a/s2b 输入（锁 + 按住左键 0.6s @ 帧时 2.9s，树叶 hardness 0.2 =
  0.3s 即破，余量 0.3s 对后方格仅 13% 裂纹不破坏）：存档精确（树叶
  消失 + leaves×1 + **apple×1** 双掉落实体，落位列 y∈(44.9,46.6)
  【生成点 46.5 落平台面 45】x 散布 ±0.125）+ **×2 归一化一致**
  （剥 `entities[].age`——真实 dt 字段，输入运行字节一致不可能）
  + skycount + 红心。
- **S3 金苹果进食**（240f 同场景类，slot0 golden_apple×1、hunger 12
  【满饥饿 = tryEat no-op】）：s3ctl ×2 无输入 = 苹果未动 + 无效果 +
  hunger 12 + 字节一致；s3a/s3b 输入（锁 + 按住右键 1.8s @ 帧时 2.0s，
  1.6s 进食通道）：存档精确（slot0 空 + **hunger 12→16** +
  `player.effects = [regeneration, remaining ≈ 7.95/7.99 ∈ (4,10)`】
  = 吃时给效果游戏内全链路）+ **×2 归一化一致**（剥
  `effects[].remaining`）+ 红心。
- **S4 init 失败**（scratch 资产副本）：golden_apple
  `grants_effect.name`="nope" → rc=1 + "item 'golden_apple'
  grants_effect references the unregistered effect 'nope'"；leaves
  `chance_drop.item`="nope" → rc=1 + "block 'leaves' chance_drop names
  unknown item 'nope'"。

### 已知问题（本阶段新增/更新）

- chance_drop 单条目无加权多掉落列表（"偶尔掉 B 或 C" 不行）。
- 概率掉落 per-cell 确定性（破坏-重种同格结果不变，无重 roll）。
- 金苹果无吸收（MC 的额外心层未实现），仅再生 10 s。
- 吃时给效果单一固定时长（无 amplifier 档位）。
- 更新：草掉种子条目（概率系统已就位，grass→dirt+偶尔种子技术上可行）；
  再生来源条目（金苹果 = 游戏内来源）。

### 坑位（PITS +3）

- 离线预计算 roll：Python 任意精度 int 不环绕 ≠ C++ uint64——哈希链
  每步 `(h + CONST) ^ X` 必须 `& M`（溢出位经 `x>>30` 混回低位）；
  症状 = 部分格一致部分不一致 + 预计算"通过"格游戏里 fail；对策 =
  小 C++ 探针全区域对拍。
- 输入运行存档含真实 dt 字段（entity age / effect remaining）→ 字节
  一致不可能 = 归一化比较（剥字段后逐字段全等）。
- E2E 瞄准 kReach=5 限距 + 掉落落位 ≠ 生成点（重力落面）+ 同点双
  掉落 ±0.125 x 散布（断言用列/盒区间，精确值交给 ×2 归一化比较）。

### 下一步

- **P50 候选**：耕地/水/光照生长条件（farmland 方块 + 生长引擎环境
  判定）；加权多掉落列表（grass→dirt+偶尔种子的 MC 语义）；熔岩流体
  系统（较大，留档）。
