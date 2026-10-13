# Phase 53 细节存档

> 范围（用户"按你建议继续：候选1"，代理定案为最小完整版：**作物放置
> 限制** = `plant_on` 数据驱动放置土壤门，消费 P48/P50 遗留"放置不
> 受限"；熔岩后续/多掉落内容扩展留后续）：
> ① block 可选 `plant_on` 数组（方块名；放置时目标格正下方须为之一，
>    缺省 = 不限 = 通用 placeBlock 规则）；
> ② 门 = 纯函数 `canPlantAt`（`canPlaceAt` 同款可单测）；
> ③ init 每条目存在性校验（grows_on 纪律）；
> ④ 内容：`wheat_1..6` 加 `plant_on: ["farmland"]`（Minecraft 耕地
>    约束落地）。

## 阶段报告

- **`[2026-10-13, Phase 53]`** 作物放置限制
  — 954/954（+12），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 27 次
  结构性免疫免重锚 + S2a 放置成功【wheat_1 落耕地顶面，槽消耗，×2 字节
  一致】+ S2b 放置拒绝【石头上不放置、槽不消耗，×2 字节一致】+ S3
  init 失败 ×2 干净报错）。

### 实现内容

**`plant_on` 纯数据**（`src/block/block.hpp` + `block_registry.cpp`）：
- `BlockProperties::plantOn`（`std::vector<std::string>`，缺省空 =
  不限）。
- 解析：可选**数组**；非数组 / 空数组（no-op 拒绝，ammo 纪律）/ 任一
  元素非字符串或空串 → 逐条目点名 `plant_on[i]` 拒绝。
- **init 硬校验**（`src/core/game.cpp`，grows_on 校验后）：每条目必须
  指向**存在的方块**（typo 拒启点名，grows_on 同款纪律）。

**放置门 = 纯函数 `canPlantAt`**（`src/world/raycast.{hpp,cpp}`，
`canPlaceAt` 同款自由函数可单测）：
- `canPlantAt(world, blocks, cell, plantOn)`：`plantOn` 空 = 恒真（不
  限）；否则 `world.getBlock(cell.y-1)` 须等于某条目的 `idForName`。
- 下方越界（目标 y=0 → 下方 y=−1）经 `World::getBlock` 读 air → 拒绝。
- 名字在调用时经注册表解析（放置非热路径——每次右键至多 1 次）。

**`placeBlock` 集成**（`src/core/game.cpp`）：canPlaceAt 之后加
`canPlantAt` 门；**土壤不符 = 静默 no-op**（不消耗物品、无音效——与
任何被拒放置一致）。右键顺序不变（eat/drink/bow/milk/interact/dye/
bucket 之后才 placeBlock）。

**`plant_on`（放置门）与 `grows_on`（生长门）相互独立**：一个块可以
放在土壤 X 上却只在土壤 Y 上生长；本项目两者同值（farmland）=
Minecraft 语义。

**内容**（`assets/data/blocks/wheat_[1-6].json`）：6 个生长阶加
`"plant_on": ["farmland"]`（与 `grows_on` 同值）。wheat_7 成熟阶不可
放置（无获取途径且为终态）不写。实际玩家只持有 wheat_1（种子），
wheat_2..6 = 数据一致性（所有 grows_on 块同约束）。

**无 format 变更**（format 9）。

### 变更文件

- `src/block/block.hpp`：+`BlockProperties::plantOn`。
- `src/block/block_registry.cpp`：`plant_on` 解析（数组 + 逐条目
  非空字符串）。
- `src/world/raycast.{hpp,cpp}`：+`canPlantAt` 纯函数（+`<string>`/
  `<vector>` include）。
- `src/core/game.cpp`：init 每条目存在性校验 + `placeBlock` 加门。
- `assets/data/blocks/wheat_1..6.json`：+`plant_on: ["farmland"]`。
- `tests/test_block.cpp`（+6 解析）/ `tests/test_raycast.cpp`（+6
  canPlantAt）。
- 文档：CONTENT_GUIDE（wheat 模板 + 字段说明 + Phase 53 节 + 关闭
  "放置不受限"简化）/ GAMEPLAY（种子节改"只能种在耕地顶面"）/
  KNOWN_ISSUES（关闭 1）/ ROADMAP（P53 行 + 索引）/ AGENTS.md（现状）。

### 测试

954/954（+12）：
- **test_block.cpp（+6 解析）**：ParsesPlantOn（2 条目数组）/
  PlantOnAbsentByDefault / PlantOnMustBeArray / PlantOnRejectsEmptyArray /
  PlantOnRejectsNonStringEntry（点名 plant_on[0]）/
  PlantOnRejectsEmptyStringEntry（点名 plant_on[0]）。
- **test_raycast.cpp（+6 canPlantAt）**：CanPlantOnListedSoil /
  CannotPlantOnWrongSoil / CanPlantWithMultipleSoils（列表第 2 条目
  命中）/ EmptyPlantOnUnrestricted（空列表 + air 下方仍真）/
  CannotPlantWithAirBelow / CannotPlantBelowBottomRow（y=0 目标，下方
  越界读 air → 拒）。

### 构建与验证结果

- `-Wall -Wextra -Wpedantic -Werror` 恒 **0 警告**。
- **E2E 批跑全绿**（`/tmp/opencode/p53e2e/p53_e2e.log`，EXIT=0）：
  - **S1 base18 门**：P53 ×2 新世界 60f 无输入字节一致 + == base18 →
    **第 27 次结构性免疫（免重锚）**【plant_on 只影响放置，fresh 世界
    零放置 = 无输入零活动】+ 红心 0。
  - **S2a 放置成功**（240f，预瞄准存档 + 单次右键 @ 2.9s 锚定
    `Entering main loop`；耕地 (22,44,−2) 顶面 y=45，瞄 yaw 0 /
    pitch atan2(−1.62,2) = −0.680809，**真实 raycastBlock 探针 p53ray
    先验证** hit=(22,44,−2) n=(0,1,0) dist 2.574 < reach 5 → 放置目标
    (22,45,−2) air、below=耕地）：存档 ×2 **字节一致**（无实体 = 全字节
    即确定性权威）+ 存档精确（**wheat_1 落在 (22,45,−2) + 槽 0 消耗**
    （count 1→0）+ 耕地完好【锁定 click 未破坏土壤】+ 零实体）+
    无输入 ctl ×2 字节一致（未放置、槽完好）。
  - **S2b 放置拒绝**（同几何，土壤 = 石头 (22,44,−2)）：存档 ×2 字节
    一致 + 存档精确（**wheat_1 未放置**（(22,45,−2) air）+ **槽 0 不
    消耗** = 拒绝语义 + 石头完好 + 零实体）。
  - **S3 init 失败 ×2**（scratch 资产副本，仓库未动）：
    `wheat_1 plant_on: ["nope"]` → rc=1 + "plant_on names unknown
    block"；`wheat_1 plant_on: []` → rc=1 + "must not be an empty
    array"。

### 场景选位（p53scan 预计算 = 本阶段关键决策）

初版场景选在 (22,44,−3)：放置成功后断言 wheat_1 **失败**——存档里是
**wheat_2**。原因：放置发生在点击帧（≈2.9s × 实测 fps ≈ 帧 93..131，
fps 在 32..45 间跑间漂移），240f 运行剩余 ~110 帧里**生长引擎（P50）
roll 过了一帧**（干耕地模数 720），作物确定性地推进到 wheat_2（×2
字节一致 = 确定性成立，断言没算到）。
修正 = **场景选位预计算**（探针 p53scan 调真实 `cropGrowthSteps` 扫
z∈{−4,−3,−2,−1}）：z=−4 → 通过帧 147、z=−3 → 161（都与点击帧赛跑 =
fps 敏感，弃）；**z=−2（及 −1）在 [0,240) 无任何通过帧** → 选 z=−2
（距离 2.57、俯角 −39°，比 z=−1 的 −58° 陡角好）→ 作物**无论点击
落在哪一帧都保持 wheat_1** = fps 无关断言。
（探针 p53roll 同时确认门状态：soil 干【模数 720】、作物格光 15 ≥ 9。
首版探针忘了 `Lighting::propagateAll` = 光 0 全门假挡——纯生成世界
无光照数据。）

### 已知问题（本阶段关闭 1）

- ~~作物可种在任何朝向的表面~~ → `plant_on` 放置门（wheat_1..6 =
  `["farmland"]`，Minecraft 耕地约束落地）。

### 坑位（PITS +1）

- **E2E 输入跑中的放置物会被生长引擎继续推进**（P53）：放置发生在
  点击帧（= 2.9s × 实测 fps，fps 跑间漂移 32..45），运行剩余帧里
  作物可能 roll 过一阶（干耕地模数 720 下 ~15%）→ 断言"放下的块"
  失败（实为 wheat_2）。对策 = **选位预计算**：调真实
  `cropGrowthSteps` 扫候选位置，选 [0,frames) 内无生长通过帧的格 =
  fps 无关断言（或预计算通过帧 + 断言精确阶段）。相关：纯探针世界
  调 `cropGrowthSteps` 前必须 `Lighting::propagateAll`（生成世界无
  光照 = 光 0 全门假挡）。

### 下一步建议

- 候选 2：**熔岩后续**（worldgen 熔岩湖 / 燃烧·下沉 / 蒸汽——消费
  P51 边界，较大）。
- 候选 3：**多掉落内容扩展**（更多方块用 `chance_drops`，如 sand →
  glass 概率？需新物品；小）。
- 候选 4：作物放置的**侧向/顶向语义**细化（当前 = 目标格正下方，
  与 MC"必须顶面点击耕地"等价但实现更宽——点耕地侧面时目标格下方
  非耕地 → 已自然拒绝；点悬空支撑下方格 → 也拒绝；语义已 MC 一致，
  无需动作）。
