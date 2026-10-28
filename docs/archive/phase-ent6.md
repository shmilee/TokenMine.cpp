# ENT-6 AI 补强（v1.x 13/33，v1.13.0，门 128，ENT 行 2/6）

v1.x 任务矩阵第十三项。"AI 补强"原始定义已佚 = 代理定案最小完整版：
**mob 追逐寻路**（KNOWN_ISSUES 记档缺口"每只 mob 只以玩家为对象……
追墙会沿墙滑"= 直线追逐无寻路；其余 AI 面——被动 mob 逃跑/无跳跃/
mob 间仇恨——范围外记档）。

## 定案

- **`entity/pathfind.{hpp,cpp}` 新纯函数模块（campfire 范式）**：
  `cellStandable`（feet 格 + bodyCells 头空 = air + 下方 = solid FullCube
  = 整数地板——楼梯/半块 = 非整数 feet 出图 = 记档偏离）+
  `pathfindBFS`（统一代价 BFS，固定邻居序 -X/-Z/+Z/+X × flat/step-up/
  fall 三族边：flat = 同 feet y 四邻 / step-up = +1【物理 stepUp 可踏
  1.0 = ENT-3 同款判定模型】/ fall = 1..kPathMaxFall(8) 深走廊格全 air
  【坠落穿越空地】；target zone = target Chebyshev ≤1 任一 standable 节点
  【末段直线 chase 收】；展开上限 kPathMaxNodes(512)；无随机无 float =
  帧锁定安全（硬规则 8）。空路径 = from 已在 zone（调用方继续直线
  chase）/ 错误 = 无路（调用方 lost 冷却）。
- **`MobEntity` 卡住触发集成**（runAi 重构，全部 chase 上下文内）：
  直线追逐窗 kStuckWindowSeconds(0.5s) 内水平 progress <
  kStuckProgressFactor(0.25)×期望（速度×窗）= 被地形挡 →
  `tryPathfind`（from = 当前 standable 节点【离整数晶格 = 无效 → lost
  冷却】）；成功 → `MobState::Pathfind`（新枚举值，无外部消费面）跟随
  路径节点（速度 = 数据 speed 指向下一节点中心、帧锁定）+ 每
  kRepathSeconds(0.5s) 重规划（目标移动跟进）；到 zone = 路径消费 →
  直线 chase/attack 接管；BFS 失败 = kLostCooldownSeconds(1.0s) 节流。
  **节点超时逃逸 kNodeTimeoutSeconds(2.0s)**：模型可能错——共享 step-up
  的 +X/+Z 单格 quirk（P16 潜伏记档）使 A* 规划的 step 节点物理不可
  执行 → 节点 2 s 未达 = 弃路径回直线 chase（quirk 几何下 mob 保持
  ENT-6 前行为 = 单测钉死）。
- **结构惰性**（零回归保证）：BFS 只在 chase 卡住时执行；未卡住 =
  逐位恒等直线追逐（单测 FlatChaserNeverPathfinds + E2E flat 场景
  逐值 A/B 双证）。wander/attack/ranged 状态零触碰（ranged 分支前置
  清陈旧路径）。
- 零 format（19）/config/内容/音效。

## 实现

`src/entity/pathfind.{hpp,cpp}`（新，~150 行）+
`src/entity/mob_entity.{hpp,cpp}`（MobState::Pathfind + 6 成员 + 4
helper + runAi chase 分支重构，~120 行）+ CMakeLists 2 行。

## 测试（1798/1798 = 1779 + 19，-Werror 0 警告）

PathfindStandableTest ×5（FullCube 地板 / slab 拒 / feet 实心拒 / 头空
拒 + 1 格体过 / 无地板拒）+ PathfindBFSTest ×10（直线 / 绕墙 8 节点 /
密封盒无路 / 1 深坑 fall+cross 3 节点 / 2 深坑绕行【fall 走廊被 void
拒】/ step-up 平台 / 确定性双跑同路 / zone 内空路径 / from 非
standable 拒 / 节点上限）+ MobPathfindTest ×4（**卡住追逐找到路径**：
墙场景 mob 绕墙到玩家 attack range 命中 / **平地追逐永不寻路**：结构
惰性 + 命中 / **密封追逐 lost**：BFS 失败 → 冷却 → 恒夹墙零命中 /
**玩家离场弃路径**）。
**4 契约演进重钉**（MobStepUpTest，ENT-3 原"恒夹墙"期望 = ENT-6 意图
行为变更）：DoesNotStepUpWall/Fence/TwoBlockStep → "绕行而非翻越"
（sawPathfind + **maxY 恒 1.0 = 原判定存活 pin**：1.5 barrier/2 步无
FullCube 顶节点 = A* 不规划翻越）+ DoesNotStepSingleCellStepPlusX →
改名 SingleCellStepPlusXStaysClampedAtTheQuirk（走廊双侧封墙 = 唯一
通路是 quirk 步 → A* 规划 step 节点 → 物理拒绝 → 节点超时弃路径 →
**x 9.7 恒夹 = quirk pin 在 ENT-6 后存活**；quirk 本体修复仍 = player
行为变更单独立项）。

## E2E 门 128

**S1 = 第 128 次结构性免疫免重锚**：fresh 60f 四方截图 ×2 387726 B
全字节一致（new v1.13.0 ≡ old v1.12.0 ≡ base18）+ savenorm new×2 /
old×2 EXACT（nomobs config：无 mob = 无 chase = BFS 结构惰性）。

**S2 = CPU harness A/B 矩阵**（gen128 单源双库【state 整数面：旧枚举
无 Pathfind 值 4 = sawPathfind 旧侧构造性 false】，2400f ×2/侧，窗口
免逐值确定性；PITS 302 树 cwd 纪律）：
- **wall**（2 高 8 宽有限墙 x=8 z2..9，mob 西玩家东）：NEW
  hits=19 sawPathfind=true maxY=1.000 pos=(12.188,1,7.409) state=2
  Attack【绕墙末端到达并攻击】vs OLD hits=0 sawPathfind=false
  pos=(7.700,1,6.500) state=1 Chase【恒夹墙 x7.7 = 8.0−0.3】——互斥
  终态 = 寻路行为定案（maxY 1.000 双侧 = 从未翻越）；
- **flat**（无墙 = 未阻塞直线追逐）：NEW ≡ OLD **逐值全同**
  （hits=29 / pos=(11.913,1,6.500) / state=2 / sawPathfind=false）
  = 结构惰性的二进制级证明；
- **sealed**（密封盒）：NEW ≡ OLD **逐值全同**（hits=0 /
  pos=(2.700,1,6.500) 恒夹 / state=1）= 无路时 lost 路径回落 =
  ENT-6 前行为逐位保持。

**S3 = 围栏竞技场零回归 A/B**（复用 s127_arena：12×12 石墙 + 夜
0.75 + mobs on g115_cfg，900f 无输入）：敌对 mob 自然生成 + 追逐 +
击杀 idle 玩家（围栏内部无障 = 无 chase 卡住 → BFS 结构惰性）——
new ×2 RAW EXACT 235680 B + old ×2 RAW EXACT 235680 B + **new_a ≡
old_a 字节一致** + savenorm EXACT（真实生成/追逐/攻击/死亡全链零
回归）。

## 性能 A/B（发布前必做）

**PASS = 成本可忽略**（tm_v112_perf v1.12.0 插桩 vs tm_v113_perf
v1.13.0-dev 插桩【v112 插桩 fluid.cpp 移植 + step0 game hunks + hunk#6
honey 区手工补，同 v111/v112 先例】，同 seed（perf_ab17 剥 fluids →
gperf_ab22.json）fall 900f 三窗，PITS 15 每跑前重 cp）：fluidCells
win2/3 **29803=29803 / 29800=29800 逐位一致**（重跑复现；win1
51925/51926 = ±1 填充 real-dt 类）+ PIPE3 visitedChunks 268=268 /
changesApplied 1404=1404 逐位 + fluidCellsSeen ±2 + 全指标同档
（win3 upd med 首轮 3.44 vs 5.50 = 重跑 6.27 vs 4.96 双向摆动 =
环境噪声重跑证伪 = 既有纪律）；终档流体 4 格差（(81..84,36,73..77)
= PITS 283 类 churn 簇，gate 127 同款 (83,36,76) 邻域）= 代码成本
零（fall 场景无 chase 卡住 = BFS 结构惰性实证）。

## 偏离（范围外记档）

- 寻路只覆盖 chase 状态（wander 卡墙 = 每 2.5 s 重掷方向自然逃逸 /
  ranged kite 不寻路 / 被动 mob 无逃跑 AI = KNOWN_ISSUES 既有条）。
- 无跳跃边（MC mob 亦不跳）；step-up 边 = 1.0 全格（slab/楼梯地板出
  图 = 整数晶格纪律；楼梯上的追逐维持直线 + 物理 stepUp 现状）。
- A* 模型与物理的已知裂缝 = +X/+Z 单格 step quirk（节点超时逃逸兜底
  = quirk 几何下行为与 ENT-6 前逐位一致，单测钉死）。
- fall 走廊深帽 kPathMaxFall 8（更深坠落 = 直线 chase 亦坠，无需规划）。
- 未加载 chunk = air（World::getBlock 语义）：寻路区域 = 玩家视野内
  （追逐 ≤16 块 = 流送恒加载），无实际裂缝。

## 坑位（PITS 追加）

无新增（既有坑全部复用：PITS 15 性能 seed 重 cp / PITS 302 harness 树
cwd / PITS 283 类终档 churn）。设计期预判并化解的裂缝 = A* 模型与
物理的 +X/+Z 单格 step quirk 失配 → 节点超时逃逸（单测
SingleCellStepPlusXStaysClampedAtTheQuirk 钉死：quirk 几何下行为与
ENT-6 前逐位一致）。
