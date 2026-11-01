# Phase 47 细节存档

> 范围（用户"按你排序的建议继续下一步"确认 = 排序第一的**流体桶**，
> 代理定案为最小完整版 = **水桶**：bucket 框架 P43 已在，依赖最少；
> 熔岩需整个熔岩流体系统、面包需种植系统、金苹果需苹果+金来源，均更重，
> 留后续）：① 物品数据 `collects_fluid`/`collects_becomes`（空桶侧）+
> `pours_fluid`/`pours_becomes`（满桶侧），**对偶对称 init 硬校验**；
> ② `raycastFluid`（桶目标射线：首个「非 air 方块**或**带流体」的格，
> 墙壁遮挡 = MC 方块目标语义；与 `raycastBlock` 共享 DDA 骨架）；
> ③ `Game::tryFluidBucket`（舀 = 命中 level 8 源格 → 移除源 + 换物品；
> 倒 = 放置目标格为 air 无流体且不重叠玩家 → 放 level 8 源 + 换物品；
> 均 "splash" 音；右键顺序在 interact 之后——手持桶对着炉子仍开面板）；
> ④ 内容：`water_bucket` 物品 + 蓝色水面桶图标 + `splash.wav`；
> ⑤ 无 format 变更（物品按名 + 流体走既有 `fluids` 编辑 = format 8）。

## 阶段报告

- **`[2026-10-12, Phase 47]`** 水桶（流体桶 v1）
  — 847/847（+12），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 21 次
  结构性免疫免重锚 + S2 舀水全链路 + S3 倒水全链路 + S4 init 失败干净
  报错）。

### 实现内容

**物品 schema（`item.hpp` + `item_registry.{hpp,cpp}`）**
- 4 个新可选字段：`collectsFluid`/`collectsBecomes`（空桶侧：舀 → 满桶）
  与 `poursFluid`/`poursBecomes`（满桶侧：倒 → 空桶）。
- `parseFluidBucket`（在 parseFood/parseDrink 之后 = 通道互斥可见）：
  流体名 **v1 词汇表 = 仅 `"water"`**（项目唯一模拟流体；其他名 =
  拼写错误/未来特性，拒绝）；`*_becomes` 非空物品名且**要求对应
  `*_fluid` 存在**（dead field 拒绝 = dye 配对纪律）；两侧**互斥**
  （一个物品只是一对的一侧）；桶侧与**食物/可饮用互斥**（右键通道
  归属，同 food/drink 纪律）。白名单 +4 键。
- **init 对偶对称交叉校验**（`Game::init`，ammo 模式扩展为对）：
  伴侣必须存在**且反向指回**（同流体名、`*_becomes` == 本物品名）——
  单边/错配 = 启动失败，错误信息点名双方（S4 E2E 验证）。

**raycast（`raycast.{hpp,cpp}`）**
- 共享 DDA 骨架 `ddaFirstCell(origin, dir, maxDistance, test)`（原
  `raycastBlock` 循环体提取；`raycastBlock` = test「非 air 方块」，
  语义逐位不变——既有 raycast 测试全过为证）。
- `raycastFluid`：test「非 air 方块**或** fluid > 0」——**墙壁遮挡**
  （MC：桶用方块目标，不能隔墙舀；普通方块射线穿水而过）。命中格由
  调用方读 fluid level（8 = 可舀源）。

**Game（`game.{cpp,hpp}`）**
- `tryFluidBucket()`：选中桶侧 → 伴侣 `isFull` 探测（满背包 = no-op 吞
  点击，MC）→ **舀**：`raycastFluid`（kReach 5）命中格 fluid ==
  `kMaxFluidLevel`（流动水 <8 不能舀，MC）→ `setFluid 0` +
  `Lighting::updateLocal` + 手持 1 换伴侣 + "splash"；**倒**：
  `targetBlock` 受击面邻格 = `canPlaceAt` + block==air + fluid==0 →
  `setFluid 8` + `Lighting::updateLocal` + 换伴侣 + "splash"。落空
  （无源/目标无效）= 不吞点击继续后续（material 不可放置 = 实际
  no-op）。倒下的源走正常 **FluidSim**（20 Hz 帧确定性：下落/四向
  扩散 level 递减）。
- 右键顺序（Phase 47 更新）：食物 → 饮用 → launcher → 挤奶 →
  interact → 染色 → **流体桶** → 放置。

**内容（全 JSON + 工具生成）**
- `assets/data/items/water_bucket.json`（material，max_stack 1，
  `pours_fluid "water"` / `pours_becomes "bucket"`）；`bucket.json`
  加 `collects_fluid "water"` / `collects_becomes "water_bucket"`。
- `assets/textures/items/water_bucket.png`（`generate_item_icons.py`
  的 `bucket(water=True, salt=59)` = 蓝色水面 (70,140,220) 椭圆；
  独立 PNG，无 atlas/UV 影响）。
- `assets/sounds/splash.wav`（`generate_sounds.py` 新 `splash()`：
  0.22 s 带通噪声 burst + 下滑水音，固定 LCG seed 0xA511 确定性；
  声音目录扫描自动加载，无注册代码）。

**测试（+12 = 847/847）**
- `test_item.cpp` +6：`ParsesFluidBucketPair`（两侧字段值）/
  `FluidBucketFieldsDefaultToEmpty`（P47 前物品不受影响）/
  `RejectsUnknownFluidName`（"lava"/空串）/ `RejectsBecomesWithoutFluid`
  / `BucketSidesAreMutuallyExclusive` / `BucketConflictsWithFoodAndDrinkable`。
- `test_raycast.cpp` +6：`RaycastFluidTest.*`（命中流体格 / **方块遮挡**
  / 首个遮挡格优先 / 超距无命中 / 空世界 / **原点格跳过**）。

### E2E（批跑，全绿）

- **S1 base18 门**：P47 ×2 + P46 代码参考构建（own worktree @2c98c1c，
  ref 从自己 worktree 跑 = ref 资产恰为 base18 的生成配置）×1 新世界
  60f **三方字节一致 = 第 21 次结构性免疫（免重锚）**【新物品不在默认
  hotbar → 不渲染；bucket.json 字段/新图标文件/splash.wav 均不影响
  无输入基线】+ skygate + 红心 0。
- **S2 舀水**（正午 still mobs off 240f；场景 = p27far 派生 + **显式
  几何**：石头支撑 (22,44,−3) + 玩家平台 (22,44,−2) + 射线路径格 air +
  源 level 8 于 (22,45,−3)；**玩家站源旁 1 格** (22.5,45,−1.5)、
  pitch −0.6——源的扩散水挡视线坑见 PITS）：
  - s2ctl ×2 无输入：存档字节一致；源**存续** level 8、slot0 仍 bucket。
  - s2a/s2b 输入（锁 + 右键一次，锚定 `Entering main loop` @ 帧时
    2.9 s）：存档**精确**（slot0 = **water_bucket**、源格**消失**
    【被邻格 L6 扩散回填——源移除后 (22,45,−2) L7 持续向 (22,45,−3)
    补 L6，断言 = 非 8】、扩散格存续）+ **×2 字节一致**（流体 = 帧
    确定性，无 real-dt 字段需剥离）+ "scooped water at (22, 45, -3)"
    日志 + skycount + 红心。
- **S3 倒水**（同场景类，玩家 p27far 原位、pitch −0.5 瞄石头面、
  slot0 = water_bucket、无初始流体）：
  - s3ctl ×2 无输入：无流体、slot0 仍 water_bucket。
  - s3a/s3b 输入：存档**精确**（slot0 = **bucket**、**(22,45,−3)
    level 8 源**【倒在石头上 = 直接搁置不下落】+ 扩散格）+ ×2 字节
    一致 + "poured water at (22, 45, -3)" 日志。
- **S4 init 失败**（scratch 资产副本，不动仓库）：① 错配对（
  water_bucket.pours_becomes → "nope"）→ rc≠0 + 错误点名
  "item 'bucket' collects into 'water_bucket', but that item does not
  pour back to 'bucket'"；② 未知流体（bucket.collects_fluid → "lava"）
  → rc≠0 + "'collects_fluid': unknown fluid 'lava'"。

### 变更文件（显式路径）

- `src/item/item.hpp`、`src/item/item_registry.{hpp,cpp}`（字段 + 解析 +
  白名单）
- `src/world/raycast.{hpp,cpp}`（DDA 骨架提取 + `raycastFluid`）
- `src/core/game.{hpp,cpp}`（`tryFluidBucket` + init 对偶校验 + 右键
  顺序）
- `assets/data/items/bucket.json`、`assets/data/items/water_bucket.json`
- `assets/textures/items/water_bucket.png`（新）、
  `assets/sounds/splash.wav`（新）
- `tools/generate_item_icons.py`、`tools/generate_sounds.py`
- `tests/test_item.cpp`、`tests/test_raycast.cpp`
- 文档：MECHANICS（流体桶节 + 右键顺序）/ CONTENT_GUIDE（新增流体桶节）/
  GAMEPLAY（水桶节）/ KNOWN_ISSUES（流体桶 4 条）/ PITS（+3：流体桶
  E2E 视线 / 存档同格 block+fluid edit 拒绝 / 天空锚点改计数）/
  ROADMAP P47 行 / AGENTS.md 现状

### 已知问题（本阶段新增，详见 KNOWN_ISSUES）

- 只有水桶（v1 词汇表 = water；熔岩桶需熔岩流体系统）
- 流动水不能舀 + 源的扩散水挡远处视线（MC 行为；场景设计注意）
- 倒水只能倒向方块受击面（MC 对水面的倒法未实现）
- 水中无游泳物理差异（Phase 9 状态不变）
