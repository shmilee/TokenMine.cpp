# Phase V3-2 Part 2 — 下界深化（v3.7.0，门 197 全 PASS）

> 设计定案 = `/tmp/opencode/v370_design.md`（T1-T5 + 门论证 + 记档偏离）。
> 前置：v3.5.0（下界内容族）+ v3.6.0（netherite 链）。本版收口
> v3.4.0/v3.5.0 的 4 条记档偏离（水 no-op / 音族 stone·grass 复用 /
> magma 无光 / soul sand 无减速）+ 下界 ambience 第 4 上下文。
> 全 nether-scoped，**overworld 位级不变**（单测 + S1 锚双保险），
> **零渲染器改动 / 零 format（30 不变）/ 零 config**。

## 实现

### T1 水在下界蒸发（收口 v3.4.0 偏离 8 no-op → MC 蒸发语义）
- 下界泼水桶 = **桶变空桶**（水消失）+ `evaporate` 音（新 1 件，
  嘶声 + 低通 0.4 s，种子 0x9151）+ info 日志
  `"Evaporated water bucket in the nether (water cannot exist there)"`。
- **零格触碰**（不放置/无流体 diff/无光更新 = 桶独占事件）+ 点击
  **恒消耗**（下界没有它的放置格）+ 无房 = isFull 既有模式。
- 纯谓词 `netherWaterPourEvaporates(kind, inNether)`（raycast.hpp，
  四形单测钉）；下界熔岩桶 = 正常放置（MC 同款，记档偏离保留）。
- 主世界泼水路径 = 同一语句同一顺序的 false 分支（位级恒等，锚实证）。

### T2 下界音效族（收口 v3.5.0 偏离 3/4 的 stone·grass 复用）
- 新合成器 9 件（generate_sounds 固定种子 0x8Exxx/0x8Fxxx/0x90xxx）：
  - `nether` 族（dig/place/break，低频闷挖 + 脆断）→
    **netherrack / magma_block / ancient_debris**
  - `nylium` 族（软泥低频湿声）→ **crimson_nylium / warped_nylium**
    （此前 = 缺省 grass 先例）
  - `stem` 族（中频木质脆断）→ **crimson_stem / warped_stem**
- **重指派既有族**（sound_material 字段数据驱动零核心）：
  soul_sand / soul_soil → `sand`（MC 灵魂沙 ≈ 沙）/ basalt /
  nether_brick / 双 ore → `stone`（MC 同款）。
- 13 下界块音族全钉（nether 3 / nylium 2 / stem 2 / sand 2 / stone 4）。
- SoundBank 49→**60**（11 新 wav 含 T1 evaporate + T4 ambience_nether；
  既有 49 件 md5 逐一不变 + 二次重跑零变更幂等实证）。

### T3 magma_block light_emission 3（收口 v3.5.0 偏离 5，MC 精确）
- 数据一行；下界内容场景光重烘 = nether-only 像素面（主世界无 magma
  块 = 位级不变）。

### T4 下界 ambience 第 4 上下文（优先级 nether > cave > day/night）
- `ambienceContext` 加 **默认参 `inNether = false`**（既有调用点
  位级不变；单测 OverworldContextBitIdenticalToPreT4 钉 30 组输入面
  2 参 ≡ 3 参 false）；`AmbienceContext::Nether = 3`（枚举值 0-2
  不变 → overworld roll 流不变）。
- 新 wav `ambience_nether`（48 Hz 隆隆 + 远熔裂 2.0 s，种子 0x91A2，
  纹理度量 PITS 382 守卫钉 88200 样本 peak 4699）。
- 调度常量复用（kAmbienceIntervalFrames 300f × p 0.5，命名常量不动）；
  game_frame updateAmbience 传 `playerInNether()`（主世界恒 false）。
- 帧锁定确定性实证（门内）：同场景 ×2 的 ambience_nether 事件帧
  逐一对（各 2 次）；fixer 冒烟 = activated-stand 场景 1200f 帧 ~900
  首事件（该 seed 纯 roll 预言 300/600 窗 FALSE、900 TRUE）。

### T5 soul sand 减速（收口 v3.5.0 偏离 D6，数据驱动）
- 块属性 **`walk_speed_factor`** 严格解析（block_registry，fuel_time
  纪律）：number (0,1]，absent = 1.0，越界/非数字 init 点名；
  **仅 soul_sand.json 携带 0.4**（MC 精确）。
- 物理消费（player.cpp 步行段）：**双 feet 格**（支撑行
  floor(y−kFaceEps)【站地 = 脚下块 / 空中 = 脚行统一公式】× AABB X 跨
  双端点严格重叠 × 中心 Z 列，fire 接触同款受限行）取**最小因子**；
  纯函数 feetWalkFactor / applyWalkSpeedFactor（player.hpp，单测钉）。
- **仅步行**（飞行/任何流体档/潜行豁免；疾跑 = 步行受减速 MC 同款，
  记档偏离 3）；`speed × 1.0f ≡ speed`（IEEE 754 有限值位级恒等）=
  主世界无携带块时的结构免疫根。

### 版本三件套
CMakeLists 3.6.0→3.7.0（TM_VERSION 单源：banner + F3 钉）。

## 门 197（结构免疫第 78 次；锚 224468 B 免重锚）

**结构免疫论证**：全 nether-scoped——T1 桶分支维度门（主世界
false 路径位级）/ T2 音族（音频不入像素）/ T3 magma 光（主世界无
magma 块）/ T4 ambience（音频 + overworld 上下文位级钉）/ T5 因子
（主世界无携带块 = 恒 1.0 路径）→ 无输入主世界 ≡ 锚。

- **S1** 无输入 60f ×2 ≡ 锚 224468 逐字节 + savenorm EXACT + 白名单
  4 键（format 30 不变）+ banner v3.7.0。
- **S2 下界场景臂**（nether 原生 platform 场景，build_scene197 5 变体）：
  - (a) **稳定 ×2 + ambience**：shot 场景（soul 5 + magma 1 + neth
    10 平台，y=60 空腔悬浮）无输入 1800f ×2 → **截图 RAW 恒等** +
    **ambience_nether 事件 a=2 b=2 逐一对**（帧锁定调度确定性）+
    终档 nether 段 16 块全保留（编辑持久）+ 玩家零位移。
  - (b) **soul sand 步行比值**（输入 E2E，全 soul 行 vs 全 neth 行，
    3.0 s W-hold）：soul dx 4.321 / neth dx 10.712 → **比值 0.403**
    （带 0.30-0.50；预期 0.4——T5 端到端实证，含加速惯性全程线性
    验证）。
  - (c) **桶蒸发臂**（输入 E2E）：下界平台 + water_bucket slot0 +
    RMB → `"Evaporated water bucket in the nether"` + 终档 slot0 =
    空桶 + nether 段零 water 流体；(c2) **主世界对照**：同姿态同
    输入 → 水**正常放置**（终档 root fluids 含 water）+ 零蒸发 =
    T1 维度门两面实证。
- **S3** 全套 2658/2658（编排者独立复跑）。
- **S6 点名 3/3**（scratch 资产手术，全 init 带路径 rc=1）：
  walk_speed_factor 1.5（"must be a number in (0, 1], got 1.500000"）/
  0（"...got 0.000000"）/ "0.4" 字符串（"must be a number"）。
- **性能 A/B（用户硬要求）**：旧 = v3.6.0（tm_v370_perf = 9d78d6d +
  g139，banner v3.6.0 实证），新 = v3.7.0（tm_v370_perf_new + g139，
  banner v3.7.0 实证）。主对 900f 无输入同 seed：**PERF-WIN 三窗同档**
  （upd med 0.621/0.533/0.549 vs 0.444/0.607/0.620——窗口间 ±0.08 ms
  机器噪声带，均值 0.568 vs 0.557 ms 新侧更低）+ render 同档
  （0.607/0.546/0.529 vs 0.528/0.621/0.555）+ swap vsync 同档 +
  fluid·fires·spread·ents·stream 同档 + **changesApplied 9/1/3 逐位
  恒等** + **终档 savenorm EXACT**（nether-scoped 零 format = 完全
  恒等最强形）。
  **门纪律注**：首跑与 S2 流送臂并行 = 300f 窗污染（v370 upd med
  0.806）弃用，独占复跑为上。

## 单测族（2640 → 2658，+18）

- T5：WalkSpeedFactorParses（0.4 收 / 1.0 边界收 / absent=1.0 / 1.5
  拒 / 0 拒 / "0.4" 字符串拒，全点名）+ WalkSpeedFactorTest 7（纯
  函数 1.0 位级 / 步行减速 2 s 位移比 0.4±0.005 / 边界双格任一脚 /
  **疾跑亦减速** / 潜行豁免轨迹 1e-4 恒等 / 飞行豁免轨迹恒等 / 涉水
  豁免 + swim 档 6.2±0.1）
- T4：AmbienceNetherTest 5（优先级四形 / **overworld 位级恒等钉**
  30 组输入面 / 发行 seed roll 钉【首 FALSE = 帧 2100 + 2099/2100
  边界 + 帧 2100 与 day 流去相关】/ 1000 窗概率带 / ambience_nether
  wav 钉 88200 样本 peak 4699）
- T2/T3：SoundNetherFamilyTest 4（9 材料 + evaporate 样本数/peak 精确
  钉 = PITS 382 守卫）+ census 49→60 ×2 有意识更新 + 13 块音族内容钉
  + magma light 3 钉（RealTwelveBlocks 更新）
- T1：NetherWaterPourEvaporationGate（四形）
- 有意识更新：Craft3SoundMaterialContent / V340NetherrackBlockContent
  Pinned（偏离 3 收口 → nether wav 面）/ F3 版本钉 v3.7.0

## 偏离/记档

1. 水蒸发**无蒸汽粒子**（粒子族扩展 = 后续）。
2. **portal UV 流动动画延后**（per-tile 动画基建 = 渲染器 scope，
   水波纹机制 game_render.cpp:750 是水面专用 draw 非 chunk tile 路径）。
3. 疾跑亦受 soul sand 减速（MC 同款；设计"仅步行"豁免清单 =
   飞行/流体/潜行，疾跑 = 步行变体）。
4. 双 feet 格取定形：支撑行 × X 跨双端点 × 中心 Z 列（≤2 格取最小，
   边界任一脚受减速测试钉）。
5. ambience_nether 优先级 = nether > cave（下界洞窟也是 nether 语境，
   MC 同款）。
6. **11 新 wav（49→60）非 10（49→59）**：设计稿交付清单算术未计 T1
   evaporate（其规格明文"新 1 件"）= 计数笔误；红线本质面（既有 49
   md5 逐一不变 + 固定集合 + 幂等）全满足。

## 变更文件（显式路径，30 + 11 wav）

核心 9：src/block/block.hpp（+walkSpeedFactor 字段默认 1.0f）/
src/block/block_registry.cpp（walk_speed_factor 严格解析）/
src/world/raycast.hpp（+netherWaterPourEvaporates 纯谓词）/
src/core/game_interaction.cpp（T1 桶分支维度门）/ src/world/
ambience.{hpp,cpp}（第 4 上下文 + 默认参）/ src/core/game_frame.cpp
（传 playerInNether）/ src/player/player.{hpp,cpp}（T5 双 feet 格
扫描 + 步行消费）。
数据 9：blocks/{netherrack,magma_block,ancient_debris,crimson_
nylium,warped_nylium,crimson_stem,warped_stem,soul_sand,soul_soil}.
json（音族重指派 / +light 3 / +walk_speed_factor 0.4）。
音效：tools/generate_sounds.py（+11 合成器）+ assets/sounds/ +11 wav。
测试 7 + CMakeLists（3.7.0）。

零改动红线核证：渲染零触碰（无 shader/纹理）/ 存档零 format /
config 零改动 / 主世界音族·速度·桶行为位级（锚 + 单测双证）。
