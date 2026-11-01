# Phase ENT-4 存档（水体交互）

> v1.0 任务表 14/29（BACKLOG ENT 行 4/4 收尾项）。commit `e2bc754`，发布 v0.12.0。
> 任务条：「mob/掉落物游泳、浮力（实体条）」= KNOWN_ISSUES「实体与流体无交互」+
> 「mob 无游泳/水交互」。

## 定案

最小完整版 = **玩家 FLUID-2 的无输入孪生**：把玩家流体浸入模型上提为
`Entity` 共享（`refreshFluidState` + `integrateFluid` + `isRestingOnSolid`），
MobEntity/ItemEntity 按「流体中或水面」切换物理分支。AI 状态机、P58 燃烧、
箭、format/config/内容零改动。陆上行为逐位不变（存量 1389 零回归 + 双 inert
锚单测）。

## 实现

- **`Entity::refreshFluidState()`**：inFluid = AABB 严格重叠水或熔岩格
  （player isInWater/isInLava 同款扫描）+ W' 表面平面（中心列最高 L8 +1，
  扫深 32，同款循环）+ atSurface 0.4 带（`kMaxSubstepMove`）+ **kind 记忆**
  （漂浮终态脚面已离水格，由 atSurface 带单独维持——空中沿用最后浸没
  kind，否则熔岩漂浮终态会塌，首轮单测实证）。
- **`Entity::integrateFluid(h)`**：浮漂 snap（player 交叉重查 + 上浮速门，
  终态 y=W'/vy=0/onGround=false = fps 无关不动点）> 被动上浮（buoyancy 12
  帽 1.5，vy≥0 门 = 沉水体继续下沉 MC dive 语义 + 池底站立 fresh resting
  门 + 流膜无面门）> 下沉（同值下漂帽 −4 = 入水动量钳制）；流体中零重力。
- **`isRestingOnSolid`** 从 Player 纯上提 Entity（player 继承逐位不变 =
  FLUID-2 全套单测零改动全过）。
- **mob**（`MobEntity`）：AI 状态机零改动（追逐/游走照常）；水平速度门
  ×`kUnderwaterSpeedFactor`（0.6f 命名常量，AI 与击退同门——无 per-mob
  数据字段 = 文档化 YAGNI 偏离）；ENT-3 step-up 加 `!m_inFluid` 门（水下
  踏阶无意义且破坏漂浮终态）；子步峰值速度流体中改 cap 峰集（陆上公式
  逐位未动）。
- **item**（`ItemEntity`）：同款 fluid 分支 + 摩擦保留（浮面静止无漂移 =
  MC 简化记档）；熔岩对称同浮力（player 熔岩是弱节奏——文档化偏离）。
- **箭不动**（投射物穿过水 = 任务条边界，文档化偏离）。
- **零 format**（mob/item ephemeral 无存档面）、**零 config**（命名常量）、
  **零内容**；帧锁定（1/60 帧步，无 real dt/无随机 = 硬规则 8）。

## 水下行为状态机（tick：refresh → 门 → 子步；子步：snap > rise > sink）

- 空中（!inFluid && !atSurface）：重力 32 / 终端 38（旧模型逐位）
- 沉水体（inFluid, vy<0）：下沉帽 −4 → 池底 resting 门接管 = 站立
- 静水体（inFluid, vy≥0, 非 resting, 有 L8 列）：上浮 +12h 帽 1.5 → 0.4
  带内穿越 → snap
- 漂浮终态（y=W', vy=0, onGround=false）：snap 不动点，fps 无关；水平
  = AI/击退 ×0.6（水面游泳）；step-up 禁
- 1 格浅膜涉水（feet 在膜内且 resting）：站立不下沉不上浮，水平 ×0.6

**MC 对称性**：站立 mob 不上浮（同 MC 陆地生物踩水底）；mob 上浮只经
击退（上抛动量 vy≥0 后 buoyancy 接管）；预置静水体（vy=0 非 resting）
上浮——item 预置水中 = 浮面（单测 + E2E 钉死）。

## 变更文件（e2bc754，12 文件 +698/−76）

- `src/entity/entity.{hpp,cpp}`：共享流体模型（+197）
- `src/player/player.{hpp,cpp}`：isRestingOnSolid 纯上提（−40/+3）
- `src/entity/mob_entity.{hpp,cpp}`：fluid 分支 + ×0.6 门 + step-up 门（+61/−17）
- `src/entity/item_entity.{hpp,cpp}`：同款分支 + 摩擦保留（+42/−10）
- `tests/test_mob.cpp`（+277，MobFluidTest 7）/ `tests/test_entity.cpp`（+114，item 流体 4）
- `docs/KNOWN_ISSUES.md`（实体-流体条 + mob 水交互条关闭）/ `docs/BACKLOG.md`

## 测试

1400/1400（+11）、`-Werror` 0 警告、干净重建。新测试：mob 7 = 脚格/身格/
熔岩/干判定、沉水上浮 W'=3 exact 终态 + 不动点、坠水沉池底站立（与漂浮
互斥）、涉水深速 ×0.6 ratio 钉死（15×0.8×0.6/60）、水下不踏阶 vs 陆上
跨越互斥、双体同种子 300 帧逐位一致（含入水/漂浮转变帧）、陆上 inert 锚；
item 4 = 浮面 y=W' exact + 滑行静止、坠水沉底互斥、熔岩对称浮、陆上 inert。

## E2E（门 94）

场景 = arena#2（arena 北侧新平地 x14-32, z-25..-4）：5×5 岛（x20-24, z-19..-15,
level 39）+ 满宽水膜环（x14-32, z-25..-10，水 y38 L8，表面 39）+ **高架干道**
（x21-23, z-4..-13, level 39 = 跨膜干桥，水进不了实体干道）+ **1 行水隙**
（x21-23, z-14）隔岛面。玩家 (22.5, 39, -17.5) 岛中排（各面 clamp Δ≥2.8>1.6）。
**设计三轮失效教训**（PITS 243/244）：① 生成环审计须用 spawner 精确语义
（整数格偏移环 dx²+dz²∈[25,36]、floor band ±2、headroom 2 格含流体、
未编辑自然地形=潜在生成平台、环边界 ±0.5 jitter）——墙顶/岛缘/自然地形
生成平台曾致三态全 hp20 两轮；② 膜水沿同层干道漫灌（L8→L7 流，探针
f38=7 实证）→ 通道变水道 → 零生成——高架干道（level 39 实体桥）根治。
`tm_scene_check.py` v2 跑前预校验（秒级拦全部 4 类失效，负例验证过）。

- **S1 base 门**：fresh spawn 60f ×2 对 **gate88 锚字节一致**（结构性免疫：
  基线 fresh 无水下 mob【生成排除流体 headroom】+ 无 item 入水 + 陆上公式
  逐位未动）。首跑 `--new` 漏 `--save` 无效（PITS 245），修正后过。
- **S2 item 浮力**（无输入 day mobs-off，池 x19-26 z0..3，W'=39）：
  **new 浮面 y=39.0 exact / old v0.11.0 沉底 y=36.0 exact 互斥**（×2 各
  RAW 字节一致 + savenorm EXACT）；fire=9999s 对照 ×2 blocks 段 EXACT
  （924/924）。
- **S3 涉水三态**（night zombie max1，唯一生成格=干道环格 4 选 1 确定性，
  900f ×2/态）：**st1 new+膜**：zombie 生成干道 (21.58, 39, -12.89) →
  涉水隙（y38 inFluid）→ **卡岛南面（Δ=3.80>1.6，y=38 涉水，静止）hp=20**
  【禁踏阶生效】/ **st2 old+膜**：同场景同生成点 → 涉水全速 + 踏阶上岛
  （y=39）→ Δ=0.99 攻击 **hp=0** / **st3 new 无膜**：干地生成 (22.5,38,
  -23.1) → 陆上踏阶上岛 → **hp=0**——三态互斥 + ×2 RAW 字节一致 ×3 +
  savenorm2 EXACT ×3（DIFF 归因 = saturation/hunger/fires 余量 real-dt，
  剥除后字节一致）。
- **存档 lake 火漂移**（非缺陷，PITS 244）：shipped 表面熔岩湖 (24,26)
  帧 0 点燃（backtrace 实证 FluidSim::tick→setBlock），火寿命 4s = real
  dt → 跨墙钟跑存档 blocks 段 ±13 格漂移；fire=9999s 对照 EXACT。
- **性能（插桩 A/B v0.11.0，发布前测试）**：S_step 涉水 900f（3 窗口）
  render med 0.41/0.57/0.72 ms（old 0.42/0.58/0.84 同档）、ents 2.71/3.43/
  3.29 ms/300f（含 1 涉水 zombie；old 2.26/2.58/3.57 同档）、upd med
  47-49 ms、stream 1.9-2.5 s；vd16 fresh 填充 600f 双跑全指标一致
  （render 0.46/0.62 vs 0.52/0.55、ents 1.5/1.9 vs 1.6/1.8、stream
  26-45 ms 双方同档）——**ENT-4 成本可忽略，无回归**。

## 偏离

- ×0.6 = 命名常量，无 per-mob 数据字段（YAGNI；player 走 config）
- 熔岩对称同浮力（player 熔岩弱节奏 g4/tv2 无浮力——mob/item 两类流体
  同一水模型）
- 无输入分支（mob 不 dive/swim-up：Space/crouch 门删除）
- 箭穿过水不动（任务条 = mob/掉落物；投射物残余记档 KNOWN_ISSUES）
- item 浮面无漂移（MC 物品随流漂——简化记档）
