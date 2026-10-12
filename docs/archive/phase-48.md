# Phase 48 细节存档

> 范围（用户"继续建议1"确认 = 排序第一的**面包**，代理定案为最小完整版：
> **小麦种植系统** = 作物方块 + 数据驱动生长引擎 + 收获 + 面包配方/食物；
> 金苹果需苹果+金来源、熔岩需整个熔岩流体系统，均更重，留后续）：
> ① 方块新属性 `grows_to`/`growth_ticks`（严格配对 + init 硬校验）；
> ② 生长引擎 `src/world/growth.{hpp,cpp}` 纯函数 + `Game::m_crops`
> （setBlock 收口维护 + 读档后全量重建，无存档字段——方块 id 即阶段）；
> ③ 内容：7 方块 `wheat_1`→…→`wheat_7`（cross 形状）+ `wheat`/`bread`
> 物品 + `bread` 配方（shapeless 3 wheat）+ 7 atlas tile + 2 图标 +
> 7 cross 纹理 + `grass` drops 改 `wheat_1`（v1 种子来源）；
> ④ 无 format 变更（format 8：作物 = 方块编辑、`table_grid` 既有）。

## 阶段报告

- **`[2026-10-12, Phase 48]`** 面包（小麦种植系统 v1）
  — 866/866（+19），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 22 次
  结构性免疫免重锚 + S2 生长全链路 + S3 种植全链路 + S4 init 失败干净
  报错）。

### 实现内容

**方块 schema（`block.hpp` + `block_registry.{hpp,cpp}`）**
- 2 个新可选字段：`growsTo`（下一阶方块名，成熟阶留空）与 `growthTicks`
  （平均多少 tick 进一阶，[1, 1'000'000]）。
- 严格配对（`parseProperties`）：`grows_to` 显式空串 = 笔误拒绝（ABSENT
  是"非作物"默认 = ammo 纪律）；`grows_to` 自引用（== 自身 id）拒绝；
  `grows_to` 缺 `growth_ticks` 拒绝；`growth_ticks` 缺 `grows_to` 拒绝
  （meaningless）；`growth_ticks` 越界（<1 或 >1'000'000）拒绝。
- **init 硬校验**（`Game::init`，ammo 模式）：`grows_to` 必须指向存在的
  方块（typo = 启动失败点名，S4 E2E 验证）。

**生长引擎（`src/world/growth.{hpp,cpp}` 纯函数 + `Game` 集成）**
- `cropGrowthSteps(world, blocks, cropCells, seed, frame)` 纯函数：对
  位置序的 `cropCells`（`map<BlockPos,BlockID>`）逐格 roll 确定性哈希
  `splitmix64(seed + frame×0x9E3779B97F4A7C15 + hash3(x,y,z,kCropRollSalt))
  % growth_ticks == 0` → 进一阶（概率 1/growth_ticks）。`frame` 推进
  state（PITS #4：splitmix64 是纯 finalizer，常量输入 = 常量流）；
  位置哈希让两株作物不同步；世界 seed 让不同世界去相关。过期项（世界
  格 ≠ tracked id）防御性跳过。
- `Game::m_crops`（`map<BlockPos,BlockID>` 位置序）：`Game::setBlock`
  收口维护（放作物 = 入集 / 破坏·成熟 = 出集，炉子同款单收口）；
  `rescanCrops()` 在 `buildScene` 末尾（`tryLoadSave` 之后）全量扫
  已加载 chunk 重建（无存档字段——方块 id 即阶段；新生世界扫到空集）。
- `Game::update` 每帧（固定 1/60 子步，昼夜类，无输入字节可复现）调
  `cropGrowthSteps` → 逐 `setBlock`（chunk dirty + 集维护）+
  `Lighting::updateLocal`（cross 非 opaque = 光不变，no-op 但保持一致）。

**内容（全 JSON + 纹理）**
- 7 方块 `wheat_1`→…→`wheat_7`（cross 形状、hardness 0.0 即破、
  `solid/opaque=false` + `transparent=true` = cross 强制）：
  `wheat_1..6` 掉 `wheat_1`（种子可再种）+ `grows_to wheat_{N+1}` +
  `growth_ticks 360`；`wheat_7` 成熟（无 `grows_to`）掉 `wheat`×2。
- 2 物品：`wheat`（material，收获物，非食物）/ `bread`（material，
  `restore_hunger` 5 = MC 值）。
- 配方 `bread`：shapeless 3 wheat → 1（**同一物品合并成一条
  `count:3`**——重复项解析拒绝 = PITS 新坑）。
- `grass` 的 `drops` 从 `dirt` 改为 `wheat_1`（v1 种子来源；见
  KNOWN_ISSUES）。
- 7 atlas tile（`block_atlas.json` 空位 [6,2]..[7,3]，无既有 UV 移位）+
  7 cross 纹理（`tools/generate_block_textures.py` `make_wheat(stage,salt)`
  茎随阶长高、`wheat_7` 金黄成熟头）+ 2 物品图标
  （`tools/generate_item_icons.py` `wheat` 金穗 / `bread` 面包）。

### 变更文件

- 核心：`src/block/block.{hpp,cpp→block_registry}`、
  `src/core/game.{hpp,cpp}`、`src/world/growth.{hpp,cpp}`（新）
- 数据：`assets/data/blocks/wheat_{1..7}.json`（新）、
  `assets/data/blocks/grass.json`（drops）、
  `assets/data/items/{wheat,bread}.json`（新）、
  `assets/data/recipes/bread.json`（新）、
  `assets/data/textures/block_atlas.json`（+7 tile）
- 纹理：`assets/textures/blocks/wheat_{1..7}.png`（新）、
  `assets/textures/items/{wheat,bread}.png`（新）
- 测试：`tests/test_growth.cpp`（新）、`tests/test_block.cpp`、
  `tests/test_crafting_grid.cpp`、`tests/test_item.cpp`
- 生成器：`tools/generate_block_textures.py`、`tools/generate_item_icons.py`
- 构建：`CMakeLists.txt`（+`growth.cpp` +`test_growth.cpp`）
- 文档：CONTENT_GUIDE / GAMEPLAY / KNOWN_ISSUES / PITS / ROADMAP /
  archive/phase-48 / AGENTS.md

### 测试

866/866（+19）：
- block 生长字段 8（配对解析 / 成熟无字段 / 缺 growth_ticks / 缺
  grows_to / 显式空串拒绝 / 自引用拒绝 / 越界拒绝×2 / 边界值接受×2）
- growth 纯函数 7（空集无步 / 成熟不 roll / 过期项无步 / 确定性 ×2 /
  长到成熟后停 / 随机节奏最终成熟 / 双株去同步）
- bread 配方 3（3 wheat 匹配并合成 1 / 缺料不匹配 / 多余 wheat 留存）
- bread/wheat 物品 1（bread `restore_hunger` 5 + wheat 非食物）

### E2E（批跑，全绿）

- **S1 base18 门**：P48 ×2 + P47 代码参考构建（own worktree @ef7ed3e，
  ref 从自己 worktree 跑 = base18 生成配置）×1 新世界 60f **三方字节
  一致 = 第 22 次结构性免疫（免重锚）**【新世界无作物（非 worldgen
  产出）、空 m_crops tick = no-op、无输入无种植、atlas 新 tile 落空位】
  + skygate + 红心 0。
- **S2 生长**（3600f 正午 still mobs off、场景 = p27far 派生 + wheat_1
  于 (22,45,0) 石上）：存档 ×2 **字节一致（确定性权威）** + 作物长到
  **wheat_7**（生长引擎游戏内全链路）。
- **S3 种植**（240f 同场景类、复用 P47 已验证几何 pitch −0.5 瞄石顶、
  slot0 `wheat_1` 种子、无预置作物）：s3ctl ×2 无输入 = 种子未动 +
  无种植；s3a/s3b 输入（锁 + 右键 @ 帧时 2.9s 锚定 `Entering main
  loop`）：存档**精确**（种子 → wheat 于 (22,45,−3)【240f 内已长 1 阶 =
  wheat_2，生长实时】、slot0 空）+ **×2 字节一致** + 红心。
- **S4 init 失败**（scratch 资产副本）：`grows_to` 未知目标 → rc≠0 +
  "grows to unknown block 'nope'"；删 `grows_to` 留 `growth_ticks` →
  rc≠0 + "'growth_ticks' without 'grows_to' is meaningless"。

### 坑（PITS 新增）

- **chunk 是 16×16×16，不是 16×16×128**——世界高度 128 = 8 层 chunk
  沿 `ChunkPos.y` 堆叠。`rescanCrops` 初版在单 chunk 内遍历
  `ly ∈ [0,128)` → `Chunk::get` 的 `assert(inBounds)` 在 Release
  （NDEBUG）下被裁掉 → 越界读 → **segfault**（gdb 定位到遍历函数）。
  正确范式见 `FluidSim::tick`：`baseY = cp.y*kChunkSize`，本地
  `ly ∈ [0,16)`，世界 y = `baseY+ly`。
- **配方同一物品重复项被拒**——3 小麦要写 `{"item":"wheat","count":3}`
  （一条），不是 3 条 `count:1`（"duplicate ingredient item"）。
  单测用同一条 parse 路径先抓到（本坑单测先红）。
- **测试拼 JSON 串：闭合引号/逗号最易漏**——用 `R"({"id":"wheat_)" +
  std::to_string(n) + ...` 拼 JSON，数字后闭合引号极易漏（`"wheat_1,`
  而非 `"wheat_1",`）→ parse 在奇怪列号报错。参数化测试 def 用显式 `+`
  拼接（每段引号/逗号都看得见）+ 写完先用 python `json.loads` 预验。

### 已知问题（KNOWN_ISSUES 新增）

- 草方块掉小麦种子（不再掉泥土）——v1 种子来源；多掉落（概率）系统未
  引入。
- 作物无耕地/水/光照需求（v1 简化，纯时间生长）。
- 作物可种在任何朝向的表面（通用 placeBlock 规则，不校验土壤）。
- 未熟作物掉种子（非 MC 的"不掉"）。
- `wheat_2..wheat_7` 有隐式 block 物品但不可获得（无掉落/合成来源）。
- 卸载/未加载 chunk 的作物不持久（回到世界生成态 = v1 无作物）。

### 下一步候选

- 金苹果（`gold_ingot` 已有，需苹果来源 + 配方；可作 P46 regeneration
  的游戏内来源，小而闭环）。
- 熔岩流体系统（较大，熔岩桶/黑曜石前置；消费 P47 遗留"只有水桶"）。
- 耕地/水/光照生长条件（消费本阶段 KNOWN_ISSUES 的作物简化）。
- 多掉落（概率）系统（恢复 grass → dirt + 偶尔种子 = MC 语义）。
