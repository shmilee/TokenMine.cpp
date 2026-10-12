# Phase 50 细节存档

> 范围（用户"按建议继续下两个阶段" = 建议 2，代理定案为最小完整版：
> **耕地·水·光照生长条件** = `farmland` 方块【dirt 合成】+ 生长引擎三道
> 数据驱动环境门【土壤硬门 / 光照门 / 干湿地】；加权多掉落列表/熔岩/
> 放置限制留后续）：
> ① block 三个新可选环境字段：`grows_on`（作物下方须为该土壤，空 =
> 无要求，init 硬校验存在 = grows_to 纪律）/ `growth_light`（作物格
> max(天空光,方块光) 最低值，[0,15]，0 = 无要求）/ `dry_slowdown`
> （无水时 roll 模数倍数，[1,64]，1 = 无减速）——均纯数据 + 严格解析
> 校验（整数性/范围/显式空串拒绝）；
> ② 生长引擎（`world/growth` 纯函数）roll 前过三门：土壤格（作物下方）
> 不是 `grows_on` 点名方块 = 不 roll（硬门）；作物格光照 < `growth_light`
> = 不 roll；土壤格同 y 切比雪夫半径 4（MC 耕地湿润半径）内任一流体
> （level ≥1）= 湿润 → 模数 `growth_ticks`，干 → × `dry_slowdown`
> （模数加倍不改 roll 值 → 干时通过帧是湿时的严格子集，确定性可离线
> 预计算）；
> ③ 内容：`farmland` 方块（solid cube、hardness 0.5 = MC、drops 回
> dirt；顶面犁地垄纹）+ 配方 `farmland`（shapeless 1 dirt → 1）+
> `wheat_1..6` 加 `grows_on: farmland` / `growth_light: 9`（MC 值）/
> `dry_slowdown: 2`（MC 干耕地半速）；
> ④ 无 format 变更（format 8：作物/耕地 = 方块编辑，水 = 流体编辑）。

## 阶段报告

- **`[2026-10-13, Phase 50]`** 耕地·水·光照生长条件
  — 909/909（+18），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 24 次
  结构性免疫免重锚 + S2 四门场景 3600f 无输入【A 湿=wheat_7 / B 干=
  wheat_4 / C 无耕地=wheat_1 / D 封光=wheat_1，全部 = C++ 探针离线
  预计算精确值】+ S3 init 失败干净报错）。

### 实现内容

**block schema（`block.hpp` + `block_registry.{hpp,cpp}`）**
- 3 个新可选字段（缺省 = 无要求 = P48 旧行为，现有作物数据不写即
  不变）：`growsOn`（土壤名，显式空串 = 笔误拒绝）、`growthLight`
  （整数 [0,15]，非整数拒绝）、`drySlowdown`（整数 [1,64]，非整数
  拒绝）——后两者用 restore_hunger 纪律（isNumber + 整数性 + 范围）。
- **init 硬校验**（`Game::init`，grows_to 同款）：`growsOn` 必须指向
  存在的方块（typo = 启动失败点名 "block 'X' grows on unknown block
  'Y'"）。

**生长引擎（`src/world/growth.{hpp,cpp}`）**
- `cropGrowthSteps` 每格 roll 前过三门（顺序：成熟检查 → 土壤 → 光照
  → 干湿 → roll）：
  - 土壤硬门：`world.getBlock(x, y-1, z)` 的块名 == `growsOn`（空 =
    跳过检查）；
  - 光照门：`max(world.getSkyLight, world.getBlockLight)` 于**作物格**
    ≥ `growthLight`（0 = 跳过检查；与渲染同款光组合）；
  - 干湿地：`cropIsWet`（土壤格同 y、切比雪夫半径 `kWetWaterRadius = 4`
    内任一 `getFluid > 0`）→ 模数 `growthTicks`，否则 `growthTicks ×
    drySlowdown`。
- 确定性不变：全纯函数（世界状态只读）、位置序 map 迭代、无 per-
  instance RNG/墙钟；无输入运行零活动（新世界无作物 = 结构性免疫）。

**内容（全 JSON + 纹理）**
- `farmland`（solid cube 默认、hardness 0.5 = MC、`drops: dirt` = MC
  语义；顶面 `farmland_top` 犁过的深色垄纹【每 4 行一条暗带】、侧面
  `farmland_side`【上 3 行犁地面 + 下 dirt】、底面复用 `dirt`）；
  atlas 空位 (7,4)/(7,5)（无 UV 移位，base18 免疫）。
- 配方 `farmland`（shapeless 1 dirt → 1 farmland；本项目无锄头，1:1
  合成 = 最小数据驱动替代，MC 是 dirt + 锄头）。
- `wheat_1..6` 加 `grows_on: "farmland"` / `growth_light: 9` /
  `dry_slowdown: 2`（`wheat_7` 成熟阶不加——不 roll，环境字段无意义）。
- 2 块纹理函数（`tools/generate_block_textures.py` 的 `farmland_top` /
  `farmland_side`，salt 90-92 避开既有）。

### 变更文件

- `src/block/block.hpp`（3 字段）、`src/block/block_registry.cpp`
  （解析 + 整数性/范围校验 + 显式空串拒绝 + cmath）、`src/world/
  growth.{hpp,cpp}`（三门 + `cropIsWet` + 头注释）、`src/core/game.cpp`
  （init 硬校验 grows_on 存在）
- 数据：`assets/data/blocks/wheat_1..6.json`（+3 字段）、`assets/data/
  blocks/farmland.json`（新）、`assets/data/recipes/farmland.json`（新）、
  `assets/data/textures/block_atlas.json`（+2 空位 tile）
- 纹理：`assets/textures/blocks/farmland_{top,side}.png`（新）、
  `tools/generate_block_textures.py`（2 函数）
- 测试：`tests/test_block.cpp`（+5：缺省/解析/显式空串拒绝/light 范围
  +整数性/slowdown 范围）、`tests/test_growth.cpp`（+10：缺省旧行为/
  缺土壤/错土壤/对土壤/光不足/光达标/block 光走 max/水去减速/半径 4
  边界/异层水不算）、`tests/test_crafting_grid.cpp`（+3：farmland
  配方匹配/多余 dirt 残留/空网格不中）
- 文档：`docs/CONTENT_GUIDE.md`（作物模板 +3 字段 + P50 专节）、
  `docs/GAMEPLAY.md`（小麦节重写：耕地/浇水/光照）、`docs/
  KNOWN_ISSUES.md`（+2 更新 2）、`docs/PITS.md`（+3）、`docs/
  ROADMAP.md`、`AGENTS.md`

### 测试

909/909（+18），`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
生长环境门测试用 `growth_ticks = 1` 的 2 阶作物链（roll 恒通过 =
环境是唯一变量），光照/流体直接经 `Chunk::setSkyLightAt` /
`World::setFluid` 布置；`stepsOver` 计数纯函数返回的 step 数（不
应用，作物恒处 stage a——roll 与阶段无关）。

### 构建与验证结果

E2E 批跑（`/tmp/opencode/p50_e2e.zsh`，日志 `p50_e2e.log`）全绿：

- **S1 base18 门**：P50 ×2 + P49 代码参考构建（own worktree @edad0cc）
  新世界 60f **三方字节一致 = 第 24 次结构性免疫（免重锚）** + skygate
  + 红心 0。结构性免疫论证：新世界无作物（非 worldgen 产出）→ m_crops
  空 → 三门从不评估；farmland 不入 worldgen；atlas 新 tile 落空位。
- **S2 四门场景**（3600f 正午 still mobs off、**无输入**、场景 =
  p27far 派生 + 4 作物隔离各条件）：
  - 地形经 C++ 探针（`p50probe4`）逐列验证真实 seed-9 世界（**整列
    8 chunk**——首版探针只生成 cy=0 只见 y 0..15，PITS 新坑）：
    (22,−3)/(24,−3) top=44、(22,−10) top=45、(22,−13) top=45（草）、
    (22,−18)/(24,−18) top=43；场景内 y≥45 全 air（无树）、y=45 无天然
    水（海平面 34 ≪ 44）。
  - roll 计数经 C++ 探针（`p50roll`，项目自身哈希链 = ground truth，
    seed 9、frame 0..3599——**0-based**，update 在 ++m_frameCount 前）：
    A 湿 mod360 = **14 次**（首帧 119 ≫ 光照 settle）；B 干 mod720 =
    **3 次**（604..3027；同格若湿 = 7 次——减速对比鲜明）。
  - 场景（全 p27far 派生，玩家静立 (22.5,45,0.5) 于生成的草顶 y=44）：
    **A** farmland (22,45,−3) + wheat_1 (22,46,−3) + 水 L8 (24,45,−3)
    （切比雪夫 2 同 y）→ 湿 + 光 15 → **wheat_7**（成熟）；
    **B** farmland (22,46,−10) + wheat_1 (22,47,−10)——土壤在 y=46 而
    水只水平扩散全在 y=45 = **结构性干燥**（免距离计算）→ 干 2× 慢 →
    **wheat_4**（恰好 3 次通过）；
    **C** stone (22,45,−13)（覆草）+ wheat_1 (22,46,−13) → 无耕地 →
    **wheat_1**（永不长）；
    **D** farmland (22,45,−18) + wheat_1 (22,46,−18) + 水 L8 (24,45,−18)
    （湿）+ 5 石盒（顶 + 四面）封光 → 光 0 < 9 → **wheat_1**（永不长）。
  - 存档 ×2 **全字节一致（确定性权威）**——无实体/效果、玩家静立、
    饥饿离散衰减（每 36 真实秒 -1 整点、累积器不存档；本机 ~35 fps
    vsync 下 3600f ≈ 102 s = 2 整点 = 18，两次运行同值）；红心 0 +
    notblack + skygate。
- **S3 init 失败**（scratch 资产副本）：wheat_1 `grows_on` = "nope" →
  rc≠0 + "block 'wheat_1' grows on unknown block 'nope'"；
  `growth_light` = 16 → rc≠0 + 范围报错（解析层拒绝）。

### 已知问题（本阶段新增/更新）

新增：
- **耕地无湿润视觉状态、不会变回泥土** — 湿润是逐作物计算的环境量，
  不是方块状态（视觉无变化；干耕地不 revert）；无 per-instance 状态。
- **湿润判定是简化几何** — 任一流体层级（不分源/流动）、同 y、切比雪夫
  4、不检查可达性（隔墙的水也算）。

更新：
- P48「作物无耕地/水/光照需求」= **本阶段解决**（生长条件全就位；
  条目移入本存档）。
- P48「作物可种在任何朝向的表面」= 生长已受限（裸石上的种子永不长），
  **放置仍不受限**（无 `plant_on` 类放置规则字段，留后续）。

### 坑位（PITS +3）

- **世界探针只生成 cy=0 的 chunk = 只看到 y 0..15**（kWorldHeight 128
  = 8 chunk/列；surfaceHeight 纯噪声函数不读 chunk → 与 getBlock 矛盾
  是症状；本批一度误判 seed 9 演示区是 y=15 海底，实际平台顶 y=44）。
- **生长帧计数器 0-based**（主循环 `update` 在 `++m_frameCount` 前 →
  N 帧 = frame 0..N-1；离线预计算差一帧可能正好错过一个通过点）。
- **FluidSim 水扩散到曼哈顿距离 7（level 1）> 湿润半径 4**（E2E 干对照
  须 MD ≥ 8 或换 y 层 = 结构性干燥；本批首版 B 位置 (14,−10) 即因远水
  膜污染被弃）。

### 下一步

剩余候选（SESSION_CONTEXT.md「下一步」）：熔岩流体系统（较大，熔岩
桶/黑曜石前置，消费 P47 遗留"只有水桶"）/ 加权多掉落列表（grass →
dirt + 偶尔种子 = MC 语义；P49 单条目 chance_drop 已铺路）/ 作物放置
限制（`plant_on` 类字段，消费本阶段"放置不受限"）/ 金苹果后续（吸收
层）。
