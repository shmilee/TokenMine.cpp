# FLUID-2 游泳划水 — 设计定案（v1.0 任务表 11/29）

> 设计交付：oracle（ses_e9bb306e0ffeOm0wITVuwSFnfh，重派后全量核实）。
> 状态：设计定案，实现未开始（2026-10-23 晨）。

## 一句话结论

**补三件 + 一个终态：被动上浮（buoyancy，无输入空中水体 → 缓慢升水面）+
水面漂浮（L8 列顶几何钳制，fps 无关终态）+ 潜行下沉（crouch-dive）；
水平瞬时速度/Space 上浮/熔岩参数/右键链全部不动，零 format、零内容，
`player.water_gravity` 退役（P74 容忍先例）+ 3 个新键；核心语义用
"预置水下 → 无输入 → 漂浮终态 y == 水面整数常数"验证，门 89 结构性免疫。**

## 现状核实（file:line）

**消费原文**（KNOWN_ISSUES.md:408-410，流体桶节）：
> "**水中无游泳物理差异** — 玩家进水体只进入水中状态（重力减半，Phase 9），
> 无划水/呼吸机制；桶操作在水中同样可用（无水下交互限制）。`[Phase 47]`"

呼吸 → SURV-2（氧气）后续项；"桶操作同样可用" → 保持（out）；**划水 = 本项**。

**现有水物理基线**（P9 + P54，全部核实）：
- 状态：`m_inWater = isInWater()`（AABB 严格不等式扫流体格，entity.cpp:144-172），
  tick 级状态 player.cpp:188-189；熔岩优先（player.cpp:189 注释 + integrate 分支序）。
- `integrate`（player.cpp:557-593）：熔岩分支（P54 原样）→ 水分支：
  `vy -= waterGravity(12)*h`、clamp `-waterTerminalVelocity(4)`、Space →
  `vy = swimUpSpeed(4.5)` 持续（含贴地："浅池底不跳、上浮"）→ 空气分支
  （重力 32/终端 38/onGround 跳）。
- 水平：瞬时速度无惯性（player.cpp:178 注释 + 249-250）：水中
  `swimSpeed 3.1`（≈MC 3.2 ✓ 不缺）；**冲刺在任一流体内全禁**
  （player.cpp:230-232）、**下蹲速度缩放水中禁用**（player.cpp:242）。
- 缺什么：**① 无被动上浮**（无输入 = 以 4 b/s 沉底，与 MC"缓慢上浮到水面
  漂浮"相反——核心缺口）；**② 无水面漂浮**（浮到水面后无平衡态，会穿出
  水面回落振荡）；**③ 无潜行下沉**（crouch 水中无效果）；**④ 冲刺游泳**
  （输入项，YAGNI 缓做，见开放问题 Q1）。

**P54 熔岩参数**（保持零改动）：player.cpp:59-62（1.9/1.5/4/2）、灼烧
player.cpp:377-388、`cropIsWet` 只算水（P54）。
**Game 侧消费者**：updateEffects 点燃门 `inLava() || isInFire()` / 熄灭门
`inWater()`（game.cpp:4224-4231）——**不改**（漂浮者 AABB 恰不重叠水 =
熄灭边界微残，见风险 6）；99.5 水下覆盖层按**相机格**判定
（game.cpp:5788-5811）——漂浮者眼在水上 = 无覆盖层，**天然正确零改动**；
stepUp（player.cpp:297 需 onGround）/ 坠落伤害新检查（player.cpp:313）/
营火座位（player.cpp:204-207 需 onGround）——漂浮态 onGround 恒 false，
全部天然隔离。
**FLUID-1 水面形态**（设计基础，fluid.hpp:23-41）：沉降池 = 源托底列
**全 L8**（B==8 → 成源）；纯流动膜（L1-7，无 L8 托底）= 静态沉积。
**⇒ 漂浮面只认 L8 列顶**（浅膜不托浮 = MC 蹚水语义）。海洋 L8 填充
（world_generator.cpp:255-262，sea_level 34）与桶倒 L8 源（P47）皆符合。
**config.json:21-31**（已提交用户文件）：player 节**无任何 swim/water 键**
→ `water_gravity` 退役对用户零影响。

## In / Out

**In**
1. **被动上浮**：空中（!onGround）、水中（AABB 重叠）、无 Space 无 crouch →
   `vy = clamp(vy + waterBuoyancy·h, −waterTerminalVelocity, waterRiseSpeed)`。
   入水动量首子步钳 −4（**P9 入水行为逐位保留**）→ 随后反转上浮 1.5 b/s。
   贴底（onGround）→ `vy = 0` 站立（MC：池底站着不上浮，2 格深水站底同理）。
2. **水面漂浮 = 几何钳制**（fps 无关终态的核心）：`W'` = 脚下中心列最高
   **L8** 水格 +1（从 floor(feet) 向下扫 `kSurfaceScanDepth = 32`，
   getFluidOfKind 未装载/越界读 0 安全，world.hpp:70）；每 tick 算一次
   `m_surfaceY`/`m_atSurface`（tick 状态，与 m_inWater 同款新鲜度约定），
   子步内复核 `|feet − W'| ≤ kFaceEps && vy ≥ 0 && !m_onGround` →
   **`m_position.y = W'`（snap，碰撞钳制同款纪律）+ vy = 0**。固定点 =
   (y=W', vy=0, onGround F)：无输入逐 tick 复算复命中，real-dt 无关。
   Space 在水面 = **正常跳**（jumpSpeed 8.5，冲量计入 maxSpeed = P99 纪律；
   跳高 1.129 可出 1 格池沿 = MC"露头即跳"）。crouch 在水面 = **下潜**
   （打破漂浮）。
3. **潜行下沉**：水中/水面 crouch →
   `vy = max(vy − waterBuoyancy·h, −waterSinkSpeed)`（1.5 b/s，MC crouch-dive）。
4. **水面水速**：速度选择（player.cpp:235-241）/ 冲刺禁（232）/ 下蹲缩放
   排除（242）三处 `m_inWater` 并列加 `m_atSurface` = 表面游 3.1、无冲刺、
   crouch 不缩速（= 水中既有规则）。
5. **config**：+ `player.water_buoyancy` 12.0（= 旧 water_gravity 值，
   角色翻转为上浮/下潜加速度，值连续无缝）/ + `player.water_rise_speed` 1.5 /
   + `player.water_sink_speed` 1.5；**退役 `player.water_gravity`**
   （停止读取、字段删除、旧 config 容忍 = P74 fire_damage 先例）；
   `swim_speed` 3.1 / `swim_up_speed` 4.5 / `water_terminal_velocity` 4.0
   **不动**（P54 熔岩比值锚定 4.5，不重标定 = 非缺口）。
6. 测试 ~15 新 + 2 适配；文档：MECHANICS.md:856-858 游泳节重写、
   KNOWN_ISSUES:408 条 → 已解决（残余拆出）、phase 存档、BACKLOG 划掉。

**Out**（均文档化）
- **冲刺游泳**（~1.6×）：Q1 推荐缓做（纯输入项、无无输入验证价值、与
  "任一流体禁冲刺"既有纪律冲突需拆门）。
- **水平惯性/拖曳收敛**（MC 参照的"加速收敛"）：不做——项目移动模型 =
  瞬时速度无惯性（player.cpp:178），stepUp/下蹲护边/maxSpeed 估计全按
  瞬时假设；水专用惯性 = real-dt lerp 帧率相关轨迹 + 模型分裂，YAGNI。
  3.1 已 ≈ MC 3.2。
- 氧气/窒息（SURV-2）；mob/掉落物水体交互（ENT-4）；水面波纹（PRE-3）；
  熔岩参数（P54 原样——熔岩无输入下沉 = MC 熔岩语义）；右键链/桶操作水下
  行为（保持，消费条原文后半句）；**format 零变更**（m_surfaceY/
  m_atSurface = 派生状态不入档）；99.5 覆盖层与 updateEffects 门零改动。

## 状态机（integrate 分支定稿）

```
integrate(h)（player.cpp:557 替换水/空气段，熔岩分支逐位不动）:
  if (m_inLava) { …P54 原样… }
  else if (m_inWater || m_atSurface) {              // m_atSurface = |feet−W'|≤ε（tick 态）
      if (Space)  vy = m_inWater ? swimUpSpeed : jumpSpeed;      // 水下 4.5 持续（原样）/ 水面跳 8.5
      else if (m_crouching) vy = max(vy − waterBuoyancy·h, −waterSinkSpeed);   // 新：潜行下沉
      else if (m_atSurface && vy ≥ 0 && !m_onGround) { m_position.y = m_surfaceY; vy = 0; }  // 新：漂浮 snap
      else if (m_inWater && !m_onGround) vy = clamp(vy + waterBuoyancy·h, −waterTerminalVelocity, waterRiseSpeed); // 新：被动上浮（入水钳 −4 保留）
      else if (m_inWater) vy = 0;                  // 贴底/浅水站定（= 原碰撞钳制结果的显式化）
      else { /* 水面带内下落（vy<0 潜水）：保持 vy，下一 tick 态重解 */ }
  } else { …空气原样… }
```

优先级 = Space > crouch > 漂浮 > 被动上浮 > 站立。**边界行为**（全部推导
验证）：
- 半浸没（脚水里头上岸）：AABB 重叠 = 水中态（原样）；漂浮仅当脚在
  W'±ε 带内且 vy≥0 且非 onGround → 无新阈值项；
- 落水（vy<0 穿面）：不进漂浮（方向门）→ 入水钳 −4 → 沉底站定（MC 潜水
  语义）→ 空中无输入再上浮（S2c E2E 互斥对）；
- 桶倒水后站水里（P47）：L8 源沉降（FLUID-1）→ W' 有效 → 同款上浮/漂浮；
- 1 格深 L8 源托池：空中预置会浮到膜顶（**接受偏离**，Q2；落水仍沉底
  站定 = MC 主线）；纯流动膜（无 L8）：无 W' = 不托浮（Q2 的判别器
  测试 4）；
- 池边块顶恰 = W' 且中心列在水上（onGround T）：`!m_onGround` 门 →
  不漂浮、走地速 4.3（测试 14 钉死）；
- 营火座位/stepUp/坠落伤害/下蹲护边：全部 onGround 门控，漂浮态天然隔离
  （零改动）；
- **maxSpeed/vyAfter**（player.cpp:262-288，P99 纪律）：水分支估计改峰值集
  `max(|vy0|, waterTerminalVelocity, waterRiseSpeed, waterSinkSpeed,
  Space?swimUpSpeed:0)`；新增水面档 `m_atSurface ? max(|vy0|,
  Space?jumpSpeed:0) : …`；空气/熔岩公式逐位不动（避免静息空气态子步数
  1→2 的帧预算回退）。

## 测试计划（15 新 + 2 适配，test_player.cpp）

1. `RisesToSurfaceWithNoInput`：3 深 L8 池（W'=3），预置 feet 1.5 空中 →
   600f：终态 **y == 3.0 精确、vy == 0、onGround F**（几何终态断言范式）。
2. `FloatIsDepthIndependent`：同池两预置深度（1.5 / 0.5）→ 同终态 y == 3.0。
3. `StaysOnPoolFloorWhenNoInput`：feet 在底（onGround）无输入 → 恒
   y == 底、**不自动上浮**（MC 站底）。
4. `ShallowFlowFilmHasNoSurface`：密封盒内 L3 纯膜（无 L8）空中预置 →
   沉底站定、**从不浮膜顶**（L8-only W' 契约）。
5. `CrouchSinksInWater`：空中预置持 crouch → |vy| 收敛 1.5、y 单调降、不浮。
6. `CrouchDivesOutOfSurface`：漂浮态持 crouch → N 帧内 y < W'−0.05
   （打破漂浮）。
7. `SpaceFromSurfaceJumps`：漂浮态 Space 1 帧 → 离面瞬间 **vy == jumpSpeed
   8.5 精确**（判别：非 swimUpSpeed 4.5）+ 峰 ≥ W'+1.0 + 回落终态 = 池底
   站定（深水 3 格）。
8. `SpaceFromSubmergedUsesSwimUpSpeed`：feet W'−1 按 Space 1 帧 →
   vy == 4.5 精确（分支选择钉死）。
9. `SurfaceWalkUsesSwimSpeed`：漂浮态持 W 0.5 s → dx ≈ 1.55（3.1）NEAR 0.1
   （判别地走 2.15）。
10. `NoSprintAtSurface`：漂浮态 Shift+W 0.5 s → dx ≈ 1.55（判别冲刺 2.8）
    ——互斥对。
11. `FallingIntoPoolSinksToFloor`：预置 W'+2 无输入 → 入水期 vy ≥ −4
    （终端钳保留）+ 终态池底站定（≠ 漂浮，与 1 互斥）。
12. `LavaPoolNeverFloats`：2 深 L8 熔岩池空中预置 60f（< 首灼 tick 帧 61）
    → 终态池底、health 20、y < W'−0.5（熔岩无 buoyancy = P54 语义 + W'
    只认水）。
13. `FloatTerminalIsDtIndependent`：1/30 双长帧 vs 1/60 帧两种驱动跑同一
    上浮 → 终态 y 逐位 == W'（fps 无关范式）。
14. `EdgeBlockNoFloat`：feet 恰 W'、中心列下水、**onGround T**（池边块顶）
    → 持 W 0.5 s dx ≈ 2.15（地速，`!m_onGround` 门钉死）。
15. `OneDeepSourcePoolFloats`：1 深 L8 池空中预置 feet G+0.5 → 终态
    y == G+1（**接受偏离钉死**，Q2 若翻案改此测试）。
- 适配 ①：test_player.cpp:1089-1127 fromConfig 键集（water_gravity → 三新键
  + 断言值）；适配 ②：新增 `RetiredWaterGravityKeyTolerated`（旧键在 config
  不报错、值忽略，P74 同款）。存量 `SinksSlowlyInWater`/`SwimsUpWithSpace`/
  `SwimSpeedIsReduced`/`FallDamageClearedByWater` 经逐帧推导**全部原样
  通过**（注释级适配视实现需要）。

## E2E 计划（无输入为主，~10 min）

- **S1 门 89（免疫）**（~2 min）：fresh 60f ×2 全帧字节一致 + 与门 88 锚
  （门 84 链）字节一致 + 红心 0 + skygate。
- **S2 上浮漂浮终态**（~4 min）：scratch save 密封石盒 8×8，底 y41
  （feet 42），L8 水 y42..46（**W' = 47**），玩家预置 feet（中心,
  **42.5**, 中心）【水面下 4.5 格 ∈ 4-5】，无输入 600f：上升
  4.5/1.5 ≈ 3.5 s < 10−1.6 s 裕量 ✓ → **×2 RAW 字节一致（几何钳制终态
  fps 无关）+ savenorm EXACT + player.pos.y == 47.0 常数断言 + vy 0 +
  health 20**。
- **S2b 深度对照**（~3 min）：10 格池（L8 y37..46，W' = 47），预置 feet
  37.5【9.5 格】600f → 终态 **y == 47 同水面常数**（深度无关）×2 RAW +
  savenorm EXACT（上升 6.8 s < 8.4 s，紧；不稳则 900f）。
- **S2c 落水对照**（~2 min）：S2 同池，预置 feet **49.0**（水面上 2 格
  空中）无输入 600f → 潜水：入水钳 −4 沉底 → 终态 **y == 42（池底站定）
  ≠ 47**（与 S2/S2b 互斥 = 落水池不漂浮、空中水体才上浮）×2 RAW +
  savenorm EXACT。
- **晨间输入项（推后，用户在场 ~6 min）**：S3 水下持 W 2 s → dx ≈ 6.2 ±0.3 /
  S3b 预置水下持 Space 1 s → dy ≈ 4.5 / S3c 持 crouch 1 s → dy ≈ −1.5，
  各 ×2 savenorm + EXACT + yaw/pitch 漂移探测（存档权威）。

## 门 89 论证（结构性免疫）

fresh 出生点 = 干燥陆地（kSpawnCandidates：h > seaLevel ⇒ 出生列零流体
填充，world_generator.cpp:255 只填 h < seaLevel 列）⇒ 玩家 AABB 无
water/lava 重叠 ⇒ `m_inWater = m_inLava = false`；中心列扫描无水 ⇒
`m_surfaceY = −1`、`m_atSurface = false`。逐点核对改动面：integrate 分支序
`m_inLava ? … : (m_inWater || m_atSurface) ? … : 空气`——两态皆 false ⇒
**空气分支逐位不变**；速度选择/冲刺/下蹲缩放三门的 `|| m_atSurface` 皆
false ⇒ 走地公式不变；vyAfter 新增水面档不可达 ⇒ 空气估计式不变；W'
扫描 = 纯读零写入；零 format/零内容/零 worldgen/零渲染（99.5 覆盖层按
相机格 = 空气中 no-op）。**⇒ 无输入下零活动，新 ×2 与门 88 锚字节一致，
门 85-89 连锁**（与 FLUID-1 门 88 论证同形）。

## 风险

1. **per-dt 积分稳定性（kMaxDt 0.25，game.cpp:2588）**：水中各档 |vy| ≤ 4.5
   （Space）/ 峰值集进 maxSpeed ⇒ 长帧 0.25 s 切 4-5 子步，入水钳/上浮 cap
   每子步生效，无发散；漂浮 = 几何 snap 非积分平衡点 ⇒ 无 limit cycle
   （这是"动态平衡漂浮"方案被否的根因：naive buoyancy 在穿面时
   overshoot−回落振荡 ~0.01 块/0.1 s 周期，帧率相关，×2 RAW 必破——
   本设计以 W'±ε 带 + vy≥0 方向门 + !onGround 门三条件几何钳制消解）。
2. **漂浮终态水面高度确定性**：W' = 整数格顶 +1（float 精确表示）+
   FLUID-1 后密封场景流体冷态（SettledPoolGoesCold 类）⇒ 终态 (y, vy,
   onGround) = 纯几何固定点，real-dt 无关（测试 13 + S2/S2b ×2 RAW 双钉）。
3. **命名一致性**：lava 保留 `lava_gravity`（熔岩下沉 = MC）、水翻为
   `water_buoyancy`（水上浮）——族内不对称是**语义正确的不对称**（两种
   流体 MC 行为相反），值连续 12→12 无迁移痛；文档注明。
4. **`water_gravity` 退役**：已提交 config.json 无此键（核实）⇒ 用户零
   影响；旧文件容忍 + 测试钉死（P74 先例）。
5. **1 深源托池漂浮偏离**：正常落水路径仍沉底站定（MC 主线），仅空中
   预置/边缘几何浮膜顶——测试 15 显式钉死，Q2 留用户拍板。
6. **微残（文档化）**：漂浮者 feet 恰在 W' 无 AABB 重叠 ⇒ `inWater()` F ⇒
   燃烧玩家浮水面不被瞬时熄灭（可达性极低：入水即灭，先于漂浮）；不扩
   inWater() 语义 = updateEffects/99.5 零改动 + 免疫论证更干净。
7. **W' 扫描跨边界**：未装载 chunk 读 0（world.hpp:70 契约）安全；流送
   边缘 1 tick 陈旧（≤16 ms）与既有 m_inWater 陈旧同级，无输入终态远离
   池边 ⇒ 不影响 ×2 RAW。
8. **-Werror**：空 else 块带注释、新 constexpr 具名（kSurfaceScanDepth 32：
   覆盖 shipped 海洋 ≤34 列深 + 16 深湖，更深列 = 不浮文档化）——零警告
   面小。

## 开放问题（≤3，带推荐）

- **Q1 冲刺游泳（Shift+水 ~1.6×）本相做否？** 推荐**不做**：纯输入项
  （无输入 E2E 无验证价值）、需拆"任一流体禁冲刺"既有门、v1.0 剩余项
  （SURV-2 等）优先；记 KNOWN_ISSUES 残余一句话可恢复。
- **Q2 1 深 L8 源托池空中预置漂浮 = 接受偏离还是加"面下 ≥2 L8"深度门？**
  推荐**接受**：深度门需二次扫描/状态且 MC 对 1 深源池（桶倒水初态）
  同样可浮；落水主线行为已 = MC，测试 15 显式钉死语义。
- **Q3 水面 Space 跳用 jumpSpeed 8.5（峰 1.129，可出 1 格池沿）还是
  swimUpSpeed 4.5（峰 0.84，出不了）？** 推荐 **jumpSpeed**：MC"露头即
  正常跳"、P99 barrier 算术自洽（1.129 对 1.0 可跳/1.5 不可跳谱系）、
  与池底 Space 4.5 上浮形成清晰两段式（水下划水 4.5 → 露头跳 8.5）。

## 实现注意（orchestrator 补充，实现时核对）

- integrate 水分支替换在 player.cpp:557-593 区（熔岩分支逐位不动 =
  差分检查点）。
- W' 扫描用 `World::getFluidOfKind`（world.hpp:70，未装载/越界读 0 契约）。
- maxSpeed 峰值集纪律 = P99（99.9 帧预算：静息空气态子步数不得 1→2）。
- `kSurfaceScanDepth` = 32 命名常量（非魔法数字，规则 4）。
- `water_gravity` 退役容忍测试 = P74 `fire_damage` 先例（旧键不报错）。
- 场景断言 y==47 常数 = S2 的 E2E 权威（几何钳制 fps 无关终态）。

## 实现与验证（2026-10-23 夜，T450 会话）

**实现**（fixer worktree branch fluid-2，cherry-pick 至 main = 7806175；
3 文件 +575/−37：src/player/player.cpp / player.hpp /
tests/test_player.cpp）：设计案 In 清单全量落地。

**对设计案的偏差（5 处，全部机制级合理，commit message 有录）**：
1. 被动上浮加 `vy >= 0` 门——字面版（无门 rise clamp）会把入水 −4 动量在
   0.67 块内消掉反转上浮，设计案自己的测试 11 / S2c / 存量
   SinksSlowlyInWater 全不可达；加门后下落体走 P9 沉线逐位不变（潜水
   语义），静止/上升体才上浮 = 所有钉死测试唯一自洽读法。
2. 漂浮带 = kMaxSubstepMove 0.4（设计案 kFaceEps 未给值，字面 1e-4 会被
   子步位移 0.025~0.375 永久跳过 = 测试 1/13 不可达）；0.4 = 可证明的
   子步最大垂直位移界（峰值集含 terminal 4.0）。
3. 子步穿越复核 = 轨迹测试（`feet + vy·h ≥ W'−ε`）+ 面上 `vy ≤
   riseSpeed` 门——位置复核同 #2 粒度问题；vy 门分离水面跳（8.5）与
   上浮冒出（1.5），任意刷新率健壮。
4. 贴底门 = isRestingOnSolid() FRESH 支撑检查（tick m_onGround 是上一
   子步结果，预置/读档池底玩家无碰撞历史会被弹出上浮；池底站定用 P9
   下沉漂移线而非字面 vy=0——零 delta 永不 re-penetrate）。
5. 测试 14 字面几何（feet 恰 W' + 中心列下水 + onGround）体素世界不可
   构造（支撑格与 W' 源格必重合；板缘距 0.5 > 带 0.4）→ 最近池边块
   案例（边柱顶 = W'、中心列干 → 地速 2.15 钉死，行为断言一致）。

**测试**：1328/1328（1312 存量零回归 + 16 新：15 FLUID-2 + 
RetiredWaterGravityKeyTolerated【P74 先例】）、`-Wall -Wextra -Wpedantic
-Werror` 0 警告（干净重建）。熔岩分支 + 空气分支逐位未动
（integrate 差分 = 唯一水分支门 `m_inWater` → `m_inWater ||
m_atSurface` + 水分支体替换；orchestrator 独立 diff 审查 + 测试复跑）。

**E2E（无输入为主，T450 黑相保护 = 截图重试至绿相）**：
- **S1 门 89 = 结构性免疫不重锚**：FLUID-2 新 ×2 字节一致 + 与门 88 状态
  锚字节一致（锚重拍 = v0.6.0 二进制 fresh 60f ×2 字节一致对，P36 先例
  ——原 base18.png 及门 84 链锚文件已失于 /tmp 清理；birth 点零水体 ⇒
  两态皆 false ⇒ 空气分支逐位不变 = 论证成立）。
- **S2 上浮漂浮终态**：5 深池 feet 42.5 → 600f 无输入 → **y == 47 精确
  ×2 + health 20 + savenorm EXACT**（W' 几何钳制 fps 无关终态成立）。
- **S2b 深度无关**：10 深池 feet 37.5 → **同 y == 47 ×2 + savenorm
  EXACT**（深度无关性钉死）。
- **S2c 落水沉底互斥**：场景预置 bug（feet 49 头嵌盒顶 y50 → 碰撞推出
  站顶 51 未入水）→ 修 feet 48 → **y == 42 池底站定 ×2 + savenorm
  EXACT**（与 47 互斥 = 落水池不漂浮成立）。
- **S3/S3b/S3c 水下输入三项（hold W/Space/crouch）留新机器**：2fps 机
  墙钟 hold 的帧数抖动 = dx/dy 断言不稳（PITS 214 条有录）；输入机制
  已由输入 E2E 债清理（10-23：7 项输入链全验证）覆盖。

**性能**（发布前快照，T450 i5-5200U/HD5500 首条基线，vd16 快协议）：
- P1 冷填充（797 列环）：249 s（3.2 列/s；burst-stall 模式——标题 FPS
  均值与日志均速可差 6×，PITS 有录）
- P2 稳态：非流送 60.0 fps（vsync 封顶，旧跑数据）；流送期 1.0-2.0 fps
  （本次 P2 落 atb 备份 I/O 窗内取低值，caveat 已记）
- P3 存档退出：10 ms（PIPE-2 journal 路径）
- RSS：vd16 满环 655 MB；流送期 CPU 单核 98-102% 饱和
- 99.10（光照并行）将同协议复测 A/B（预期流送光照 16.7→~8.4 ms/帧）。
