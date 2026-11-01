# Phase 66 细节存档

> 范围（用户"下两个阶段是 建议1 和 建议3" = 建议 1，代理定案为最小
> 完整版：**火 v2 = 粒子 + 蔓延**，全量消费 P64 火 v1 遗留"无粒子/
> 无蔓延"）：
> ① 蔓延引擎（`world/fire_spread.{hpp,cpp}` 纯函数，镜像 P48 生长
>    引擎：每 6 帧每活体火对 5 邻【水平 4 + 上，不向下 = MC】flamable
>    格独立 roll（chance 0.05 = MC normal 值）→ `setBlock(fire)`）；
> ② 余烬粒子（`spawnFire` 2 粒/0.1 s/火格、负重力上升、寿命 0.3–0.5 s
>    + 燃尽/浇灭死亡爆发 = 复用 `spawnBreak`）+ `Particle.gravity`
>    逐粒字段（破碎 = 下落、余烬 = 上升）；
> ③ config 3 键（`fire.spread_interval_frames` / `fire.spread_chance` /
>    `fire.particles`）+ init 硬校验；
> ④ 渲染修复：cross 粒子面改假法线 (0,1,0) 满亮（见下，E2E 暴露）。
> **无 format 变更**（format 10，蔓延新火走既有 `fires` 条目）、
> **无内容/atlas 变更**（fire 块未动 = 无重锚）。

## 阶段报告

- **`[2026-10-14, Phase 66]`** 火 v2：粒子 + 蔓延 — 1048/1048（+12），
  `-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 39 次结构性免疫【无重锚】
  + S2 蔓延【帧 102 点燃 +Z 木、下方木存活，×2 savenorm】+ S3 余烬
  【A≠C + 火焰像素 margin，C×2 字节 / A×2 savenorm】+ S4 燃尽 + 死亡
  爆发【×2 RAW】+ S6 init 失败 ×2）。

### 实现内容

**蔓延（`src/world/fire_spread.{hpp,cpp}` 新文件，纯函数）**：
- `fireSpreadIgnitions(world, blocks, firePositions, seed, frame,
  chance)` → 本帧点燃格列表（位置序去重）。镜像 `cropGrowthSteps`
  范式：纯函数 over 世界 + 游戏持有簿记集（`m_fires`）。
- 邻集 = `{+X,-X,+Z,-Z,+Y}`（`kFireSpreadNeighbors`，**无 −Y**：MC
  地火不烧地板）。
- roll：`u = splitmix64(seed + frame×GOLDEN + hash3(x,y,z,kFireSpread
  RollSalt) + i×kFireSpreadNeighborAdvance) >> 11 / 2^53 < chance`——
  帧推进（PITS #4：splitmix64 是纯 finalizer）+ 位置哈希（火间去
  相关）+ 邻序推进（五 roll 去相关）+ **专用 salt**（P45/P49 分离
  纪律：不与生长/掉落/装饰 roll 追踪）。
- fast-exit：无火 / chance ≤ 0 / registry 无 flamable 块（熔岩 pass
  同款门）。
- 游戏侧 `Game::updateFireSpread()`（每帧、`updateFires` 之后）：
  `m_frameCount % m_fireSpreadInterval != 0` 门控 → 取 `m_fires` 键
  （map 位置序）→ 纯函数 → 逐格 `setBlock(fire)`（收口注册满寿命 =
  新火下帧起继续蔓延）+ `Lighting::updateLocal`。**无音效**（蔓延火
  线会刷屏一次性 `ignite`，文档化偏离）。

**粒子（`src/entity/particle.{hpp,cpp}`）**：
- `Particle.gravity` 逐粒字段（默认 `kDefaultGravity = 16` = 原
  `kGravity`，类常量改别名保测试兼容）；`update` 用 `p.gravity`。
- `spawnFire(cx,cy,cz,id,sky,blockLight,rngSeed)`：2 粒
  （`kFireEmissionCount`）、`kFireGravity = -2.0`（**负重力上升**）、
  起速 0.6–1.4 上 + 水平 ±0.25 漂移、寿命 0.3–0.5 s（`kFireMin/Max
  Life`）、起点 = 格中心偏下 0.1（火焰根部）、种子流 = 既有
  `uniform01` 推进式（PITS #4 同款）。
- 游戏侧（`updateFires` 内，固定帧步）：`m_fireParticles &&
  m_frameCount % kFireParticleEveryNFrames(=6) == 0` → 每**存活**火
  （位置序）`spawnFire`，种子 = 世界种子 ⊕ `hash3(x,y,z,kFireParticle
  RollSalt)` ⊕ 帧×GOLDEN（无 per-cell 状态：读档后发射相位重启 = 粒
  子本就不入存档）。光 = 火格自身（chunk 读取，null 回落 0.0/1.0 =
  自发光主导）。
- **死亡爆发**：`putOut` 应用循环（燃尽/浇灭）逐格 `spawnBreak(fire
  纹理, 16 粒)`，种子走共享 `m_particleBreakCounter`（同输入序列同
  爆发；P65 cross 分支渲染火焰纹理）。

**config（`src/core/game.cpp` init，fire 节）**：
- `fire.spread_interval_frames`（int，默认 6 = 0.1 s MC tick 节奏，
  `< 1` 点名拒绝）；
- `fire.spread_chance`（double，默认 0.05 = MC normal 每邻值，
  `[0,1]` 越界点名拒绝；0 = 无蔓延，合法）；
- `fire.particles`（bool，默认 true；`getBool` 静默回落 = mobs.
  enabled 同款项目惯例——E2E 对照组用）。

**渲染修复（cross 粒子偏暗，E2E 暴露）**：
- 症状：S3 首轮火焰像素计数**开粒子 < 关粒子**（2355 < 2383）——
  像素 diff 定位 = 火体上方 16×23 px 区、A 侧差色 (155,130,66) 级
  暗橙。
- 根因：`drawParticles` 粒子立方体用 `face.normal` → shader 面光
  （±Z 面 0.6）→ cross 纹理亮像素 (253,202,62) × 0.6 ≈ (152,121,37)
  过不了 r>180 滤镜，且盖住火体亮像素。chunk_mesher / item_mesh 的
  既有 cross 分支都用 `v.normal = {0,1,0}`（full-brightness）= cross
  渲染项目约定，唯独粒子路径（P65 加的 cross 分支）沿用了 cube 法线。
- 修复：`drawParticles` cross 分支 `v.normal = (0,1,0)`（~6 行，含
  约定注释）→ 余烬满亮，与火体一致。
- **PITS +2**（cross 粒子须假法线满亮 / 活粒子截图不可字节比 +
  config 键 A/C 对照组范式）。

### 变更文件

- 新：`src/world/fire_spread.{hpp,cpp}`、`tests/test_fire_spread.cpp`
- 改：`src/entity/particle.{hpp,cpp}`（gravity 字段 + spawnFire +
  常量）、`src/core/game.{hpp,cpp}`（members ×3、init 解析 ×3、
  updateFires 发射/死亡爆发、updateFireSpread、drawParticles cross
  法线修复、include/常量/asU64）、`CMakeLists.txt`（源 + 测试注册）
- 改：`tests/test_particle.cpp`（+4：spawnFire 确定性/参数/上升/
  破碎默认重力）
- 文档：ROADMAP、KNOWN_ISSUES（火 v1 → 火 v2 条）、PITS（+2）、
  MECHANICS（火节段）、GAMEPLAY（火条目）、AGENTS.md、本存档、
  SESSION_CONTEXT.md（本地）

### 测试

**1048/1048（+12）**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_fire_spread.cpp`（+8）：空集 / chance 0 恒不点燃（60 帧门）/
  chance 1 **恰 5 邻**（位置序，含下方负例）/ 非 flamable 永不点燃 /
  同参确定性 / 单邻 chance 0.5 × 30 帧去相关（2⁻³⁰ 非 flaky 界）/
  双火共邻去重 / 无 flamable fast-exit。
- `test_particle.cpp`（+4）：spawnFire 同种子确定性 / 参数域（负重力、
  上初速、寿命界、格内起点、光烘焙）/ update 后**上升**（before 按值
  拷贝——跨 update 持引用 = 悬空 UB）/ 破碎粒子保持默认重力。
- config 校验走 E2E S6（Game init 级，无单测 = P64/P65 同款）。

### 构建与验证结果

- 构建 0 警告；测试 1048/1048。
- E2E（批 2 全绿，批 1 因 cross 粒子偏暗 S3 火焰 margin FAIL → 修复
  后整批重跑）：
  - **S1 第 39 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节
    一致（无 block/worldgen/atlas 变更；新 config 只作用于持 fire 块
    的格 = fresh 世界 m_fires 恒空、双引擎 fast-exit）。
  - **S2 蔓延**（420f，MOV 寿命 300，场景 = fire@(22,46,-5) +
    wood@(22,46,-4) + wood@(22,45,-5)）：11 格全生成 air（worldgen
    探针验证）；**探针预计算** roll 时序 = **帧 102** 点燃 (22,46,-4)
    （确定性：种子 9 + 位置 + 帧）；终态 blocks = {fire×2, wood（下方
    存活 = 不向下蔓延）}、fires = 2 条目（收口注册）；×2 savenorm
    字节一致（remaining = 唯一 real-dt 字段）。
  - **S3 余烬**（午夜 600f，A = 默认 vs C = fire.particles false）：
    A≠C（字节）+ 火焰像素 A = 2426/2452 > C = 2383/2383（margin +10，
    两跑都过）+ C×2 截图字节（无粒子 = 静态）+ A×2 **savenorm**（活
    余烬 = real-dt 运动，截图不可字节比 = P54 场景条款 + 新 PITS）。
  - **S4 燃尽 + 死亡爆发**（600f，默认寿命 4.0 < 任何 600f 跑 =
    fps 无关终态）：终态 blocks [] / fires []（格回生成 air）+
    截图 ×2 RAW 字节（死亡爆发 + 末批余烬在燃尽后 0.75 s 内全灭，
    帧 600 时世界已静态 = 死亡爆发渲染无崩溃、不泄漏终态的证据）。
  - **S6 init 失败 ×2**：interval 0 → "config fire.spread_interval_
    frames must be >= 1 (got 0)"；chance 1.5 → "config fire.spread_
    chance must be in [0, 1] (got 1.500000)"。

### 已知问题（演进后）

- KNOWN_ISSUES 火 v1 条 → **火 v2 条**：余烬 = v1 视觉（2 粒/0.1 s
  单材质上升火星）、蔓延引燃无声、蔓延无难度/风/湿度调制、可放置、
  光恒定 15 无闪烁、无接触伤害。
- 其余条目不变。

### 下一步（P67 定案输入）

- P67 = 建议 3 = **斧类工具**（新工具型；消费 KNOWN_ISSUES 49 行 +
  62 行）：物品可选字段 `fast_against: [方块名]` +
  `fast_against_multiplier`（严格配对、仅 tool、init 逐条目点名）+
  `effectiveBreakSpeed(item, block)` 纯函数（item.hpp，
  breakDurationSeconds 旁）+ 木/石/铁斧（fast_against [wood, leaves]
  ×2.0 = MC、base break_speed 1.0 = 手速、attack 3/3.5/4）+ 3 配方
  （shaped PP/PX/.X 型，石 = cobblestone）+ 3 图标（generate_item_
  icons.py 加 axe()；图标 = 每物品独立 PNG，无 atlas 重锚）。E2E =
  第 40 次免疫 + 铁斧破木（1.5 s，hold 1.6）+ 铁斧对石对照（2.25 s
  > hold = 不破，证明倍率不越清单）+ init 失败 ×2。
