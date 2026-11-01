# FLUID-5 水熔岩反应（v1.x 10/33，v1.10.0，门 125，FLUID 行 2/3）

v1.x 任务矩阵第十项。模拟侧水-熔岩反应 P51 已完整（FluidSim reaction
pass：熔岩 6-邻水 → lava 块 `water_reaction` 数据驱动产物——邻水含 L8
源 = obsidian / 仅流动水 = stone，水不消耗；lava.json 已配
`{"source":"obsidian","flowing":"stone"}`）= **本项零新反应规则**。消费
FLUID-4 范围外记档"水桶对熔岩面 = no-op（kind 不匹配；水-熔岩反应交互
= FLUID-5 行）"。定案 = **桶×熔岩交互两规则（MC parity，纯函数扩展
`bucketPourTarget` 候选接受，返回型不变 = 待 setFluid 的 L8 格，game 写
路径零改动）**：

## 定案

- **(c1) 水桶对熔岩格 = 反应倒**：熔岩格成为候选（block 射线受击面邻格
  或 fluid 射线命中格，FLUID-4 既有选择规则）后，水源放在
  **candidate + 受击法向**（射线来向的一步 = 顶视池格时正上方、侧向受击
  时开阔侧；block 射线无命中 = +Y 约定），该格须 air 无流体 +
  canPlaceAt；熔岩由**既有 reaction pass 下 tick 转产物**（倒出的水是
  L8 源 → 项目规则"邻水含 L8 → obsidian"，单一规则源 = 与 pass 永不
  分叉）；无空气位 = no-op 桶保留（MC：水放不下即什么都不发生）。
- **(c2) 熔岩桶对水格 = 替换倒**：`setFluid` 既有"替换他种流体"语义把
  水格直接换成熔岩源（桶消耗），reaction pass 下 tick 按残留邻水转
  obsidian/stone（邻水含 L8 源 = obsidian / 仅流动 = stone / 无邻水 =
  熔岩留存）——MC 同款（熔岩替换水、随即反应）。
- **实心放置位 no-op 结构性不可达记档**：密封池场景里熔岩格只能经顶面
  射线成候选（placement = 熔岩正上方；有顶盖时射线先中顶盖成 legacy
  空气放置），实心 placement 分支由单测钉死（WaterOnLavaNoDryAirAbove）；
  E2E 守卫 = 可达的"placement 格持流体"分支（2 格熔岩池，placement =
  源熔岩格 → nullopt）。
- **范围外记档**：蒸汽（P51 既有偏离保留）/ 流动熔岩舀取维持 P47 现状
  （舀侧扩展另立项）/ 熔岩桶对熔岩面 = FLUID-4 既有 (b) 同种变源路径。

## 实现

`src/world/raycast.cpp`（bucketPourTarget (c) 分支扩展，~40 行）+
`raycast.hpp`（文档注释）+ `core/game.cpp`（方法头注释 1 段）。**注意
`World::getFluid` 返回的是任一种类的 level（0..8）而非 chunk 编码
（0/1..8/9..16）——种类须走 `getFluidKind`**（首轮实现用 `fluidKindOf`
读 getFluid 值 = 8 新测全败根因，单测揪出）。

## 测试（1766/1766 = 1756 + 10，改 1，-Werror 0 警告）

RaycastPourTargetTest +10：反应倒（流动/源熔岩同目标 = 产物由 pass 决
定与熔岩 level 无关）/ 无空气位 no-op / placement Y 越界 no-op /
placement 玩家重叠 no-op / placement 持流体 no-op（防御分支钉死）/ 水平
法向（侧向受击 = 开阔侧放置）/ 无 block 命中 +Y 约定 ×2 / 替换倒（源/
流动水格 = 水格本身）；改 1 = ForeignKindWaterOnLavaReactionPour（P47
异种 no-op → FLUID-5 反应倒，熔岩桶对自身种类部分保留 = 种类正确性）。

## E2E 门 125

**S1 = 第 125 次结构性免疫免重锚**：fresh 60f 四方 ×2 387726 B 全
字节一致（new v1.10.0 ≡ old v1.9.0 ≡ base18）+ savenorm EXACT（无输入
= 无右键 = tryFluidBucket 零执行；(c) 分支只在桶侧右键时运行）。

**S2 单次右键输入跑**（左点捕获协议 PITS 295；存档状态 = 权威断言；
瞄准几何 = CPU harness gen125_ray 实证：真实场景世界 + raycastBlock +
bucketPourTarget）：
- **A 反应倒**（单封熔岩源 (24,45,-8) L8 + 4 石封 + 观察柱 (23,46,-7) +
  **双层周界墙 y45+y46（36 石）**【首轮无墙：倒出的水沿平台漫出边缘
  无限扩散 = ×2 RAW 对因点击帧差失配（MISMATCH 336169 vs 335712 实证）
  → 加墙封闭；CPU 收敛探针 gen125_converge 实证场景区 **5 fluid tick
  （15 帧）达稳态** = 帧 600 状态与点击帧无关，基线世界自然流体 churn
  （105 格/tick 同帧锁定）双侧同帧数同状态不影响 RAW】+ 玩家
  (23.5,47,-7.5) 陡顶视 yaw−π/2 pitch−1.2089：射线穿 (24,46,-8) 空气
  入熔岩顶面打地板 (24,44,-8) +Y → 候选 = 熔岩格、placement =
  (24,46,-8) 干空气，harness 实证）：new ×2 熔岩 → **obsidian 块** +
  熔岩流体消失 + slot0 = **bucket** + 平台水漫稳态 / old v1.9.0 ×2
  熔岩恒 L8 + slot0 = water_bucket（FLUID-4 旧异种 no-op）——互斥终态；
- **B 替换倒**（FLUID-4 水【源 (25,45,-8) L8 + 流动 (24,45,-8) L7 + 6
  石封】+ 玩家 (27.5,45,-7.5) yaw+π/2 pitch−0.30：射线打西封石
  (23,45,-8) +X 面 → 候选 = 流动水格，harness 实证）：new ×2 流动水格 →
  **obsidian**（下 tick reaction：邻水 = 源 L8）+ 源水 (25,45,-8) 恒
  L8 + slot0 = **bucket** / old ×2 水恒【L7+L8】+ slot0 = lava_bucket
  ——互斥终态；
- **C 守卫 no-op**（2 格熔岩池【源 (25,45,-8) + 流动 (24,45,-8) + 6 石
  封】+ B 同款玩家几何：候选 = 流动熔岩、placement = 源熔岩格【持流体】
  → nullopt，harness 实证）：new 熔岩恒【L7+L8】+ slot0 = **water_bucket
  保留**（防御分支 E2E 实证）。
- 收集（clean binary v1.10.0）：A 新 ×2 RAW EXACT 207329 B（i=2/4）/
  A 旧 ×2 RAW EXACT 205364 B（i=1/3）/ B 新 ×2 RAW EXACT 404382 B
  （i=12/2——长黑窗 12 重试穿越）/ B 旧 ×2 RAW EXACT 408971 B（i=2/1）/
  C 400278 B（i=6）；A/B 截图 diff：A = 755,131 px 满帧（相机 pitch
  −1.2089 顶视 = 全视场即平台：中央带 old (120,107,98) 石棕 → new
  (45,91,136) 水蓝 = 平台浸水稳态 + 水衰减重光照全地面视场 + 热栏/手持
  图标；"天空带 540 px" = 分类器误判【顶视帧无真天空，采样实证 = 平台
  石→水蓝】）/ B = 30,774 px（手持图标 + 热栏槽 0 + 池区 obsidian/水面，
  bbox x[348,1207] y[205,719] 全为倒桶后果）。

## 性能 A/B（发布前必做）

**PASS = 成本可忽略**（tm_v190_perf v1.9.0 插桩 vs tm_v110_perf
v1.10.0-dev 插桩，同 seed（perf_ab17 剥 fluids → gperf_ab20.json）fall
900f 三窗，PITS 15 每跑前重 cp）：fluidCells **win2/3 逐位一致
（29803=29803 / 29800=29800**——零 worldgen 改动 = 同世界 oracle 兑现）
+ win1 51925 vs 51928（+0.006% 填充期 real-dt 类）；PIPE3
changesApplied 1404=1404 逐位 + visitedChunks 268=268 + tick med
5.3/5.9 ms 同档；upd/render/swap 三窗全同档（med ±10% 环境噪声带）；
无输入 = 无右键 = (c) 分支零执行 = 桶代码结构惰性实证。

## 坑位（PITS 追加）

- **PITS 297（输入跑场景流体必须达稳态）**：带持续流体活动的输入跑
  场景，流体未封闭时终态依赖点击帧（×2 RAW 帧差发散）——场景 A 首版
  无墙：倒出的水漫出平台沿自然地形无限扩散 → 新侧 ×2 RAW MISMATCH
  （336169 vs 335712 实证）；加 2 层周界墙 + CPU 收敛探针（FluidSim::
  tick 循环至零 changed = 场景区 5 tick 稳态）→ RAW EXACT 恢复。基线
  世界自然流体 churn（105 格/tick 同帧锁定）双侧同帧数同状态，不影响
  RAW。
- **PITS 298（World::getFluid 返回 level 非 chunk 编码）**：getFluid =
  任一种类的 level（0..8），chunk 编码才是 0/水 1..8/熔岩 9..16——
  `fluidKindOf(getFluid())` 对熔岩格恒返回 Water，(c) 分支首轮 8 新测
  全败（"替换倒"吞掉全部异种路径）= 单测揪出；种类判定走 getFluidKind。

## 发布

**v1.10.0 = 组 1.6–1.10 边界 → tag + GitHub release**（覆盖 GEN-7
基岩多层 v1.6.0 / GEN-8 地形互感 v1.7.0 / GEN-9 耕地湿润 v1.8.0 /
FLUID-4 倒向水面 v1.9.0 / FLUID-5 水熔岩反应 v1.10.0；10-28 定案 tag
策略 = 组内不打 tag、组边界 release 1:1）。矩阵 10/33，FLUID 行 2/3。
