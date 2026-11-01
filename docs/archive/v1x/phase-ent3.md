# Phase ENT-3 存档（mob 踏阶）

> v1.0 任务表 13/29（BACKLOG ENT 行第三项）。commit `60a69f5`，发布 v0.11.0。
> 任务条：「mob 1 格 step-up，修追逐卡台阶（Mob 条）」。

## 定案

最小完整版 = **mob 获得与 player 完全同款的 1 格踏阶**：step-up 纯几何从
`Player` 上提为 `Entity` 受保护方法（player 行为**逐位不变**），MobEntity
在水平移动受阻路径镜像调用。修追逐/游走卡 1 格台阶。

## 实现

- **`Entity::stepUp(wishDir, distPerTick)`**（受保护）：Player 原函数体
  逐行搬移，仅 3 处机械替换（`isSolidCell` → `isSolid(m_world.getBlock)`
  语义逐位相同、字面量 → 命名常量 `kFaceEps=1e-4f` / `kStepUpMinWish=0.01f`
  / `kStepUpTopTol=1e-6f` 同值）。**player 调用点零改动**（非限定名解析基类）
  → `tests/test_player.cpp` git diff 为空 = 逐位不变钉死。
- **mob 调用**（`MobEntity::update`）：`moveInSubsteps` 前捕获
  `preMove`（被 clamp 轴在子步内清零，须用 pre-move 值 = player 同款）；
  之后 `if (m_onGround && lengthSq(preMove) > 0) stepUp(normalized(preMove),
  preMoveSpeed × dt)`——覆盖 chase/wander/retreat/knockback 一切水平驱动。
- **判定语义与 player 同一函数**（P99 TRUE-1-block）：可踏 = 脚位行 front
  格 `solidTopHeight` 点采样 ≤ 1.0+1e-6 且 > 0（slab 0.5 ✓ / 满块 1.0 ✓ /
  玻璃板 1.0 ✓ / 墙·栅栏 1.5 ✗ / 楼梯 = 脚中心点采样斜面高）+ front 上方 2
  格空（shipped 最高实体 1.8）+ face 贴整数 ±1e-4 + X 先 Z 后每 tick 至多
  一次 + 抬升恰 1.0 后单 moveAxis 重放本 tick 距离。
- **无穿透论证**：mob 固定 1/60 帧步 → 重放距离 ≤ 终端速度 38/60 ≈ 0.633
  < 1.0 → 单 moveAxis 无穿透（硬规则 6 共存；击退入阶单测 `hasRoomAt`
  全程无嵌入钉死）。
- **零 format / 零 config / 零内容**（mob ephemeral，物理行为不改存档面）；
  帧锁定纯几何（无 real dt / 无随机 = 硬规则 8）；AI/速度/子步节奏零改动。

## 顺带记档：P16 两潜伏 quirk（探针实证，player/mob 共享，**不修**）
1. **+X/+Z 方向 front 格查 snapped+1 → 恰 1 格宽台阶踏不过**（−X/−Z
   正常）。修复 = player 行为变更 → 单独立项。E2E 台阶场景须用 2 格厚
   （PITS 有录）。
2. **非整数脚高行走 face 巧合对齐整数 → 凭空抬升 1.0 再落回**（轨迹恢复，
   无持久影响）。

## 变更文件（60a69f5，7 文件 +382/−68）
- `src/entity/entity.{hpp,cpp}`：stepUp 上提 + 3 命名常量
- `src/player/player.{hpp,cpp}`：删原 stepUp（−63）
- `src/entity/mob_entity.cpp`：+19 镜像调用
- `tests/test_mob.cpp`：+272（9 新 `MobStepUpTest`）
- `docs/KNOWN_ISSUES.md`：Mob 条「无 step-up」标已修复 + quirk 记档

## 测试
1376/1376（+9：踏满块跨越 x 区间钉死 / 踏 slab 垂直贴顶 1.5 exact / 不踏墙
1.5 / 不踏栅栏 1.5 / 不踏 2 格高 / 帧锁定双实例位级一致（含踏阶帧）/ 击退
入阶 hasRoomAt 全程无嵌入 / 单格 −X 可踏对照 / 单格 +X 钉死 quirk）、
`-Werror` 0 警告、test_player.cpp 零改动全过（player 逐位不变）。

## E2E（门 93，竞技场设计）
**竞技场** = 12×12 块平坦区（石填洞 + 空气清高台）+ 3 高围墙（顶 41 =
踏不过 + 超出地板生成带 40 = 墙上不可生成）+ **满宽** 2 格厚 barrier
（z=−3,−2 全行：台阶变体 y38 stone / 墙变体 stone_wall 1.5）——封闭 =
zombie 无路可绕（前两轮场景缺陷：9×11 小 patch 两侧 1 块绕行缺口 =
zombie 全绕行杀死玩家三态全灭）。zombie-only 生成（max 1，skeleton 远程
箭杀 idle 玩家 = 前轮污染）；zombie 出生 (22.5,38,−4.5)（探针 seed
123456789，−Z 侧 = 相机可见侧 + 扫描序先于 barrier 出生格不变）。
- **三态互斥 PASS**：S1 new+台阶 hp=0（zombie 踏阶跨越对角 7.8 块路径
  至攻击位连击致死 <20 ✓ = 跨越证明）/ S1o **old v0.10.0 二进制**+同场景
  hp=20（卡台阶 ✓ 互斥）/ S2 new+全墙 hp=20（1.5 不可踏 ✓）；S1 ×2 RAW
  字节一致 + savenorm EXACT
- **探针轨迹实证**（93PROBE 30 帧粒度，用后删零残留）：zombie 出生
  (22.5,38,−4.5) → y38 行走 → **y39 踏上 2 格厚台阶**（z=−2.87 台阶顶）→
  下降 y38 z=−0.09（距玩家 1.59 块 = 攻击位）
- **base 门**：spawn 60f ×2 对 gate88 锚字节一致（结构性免疫：基线帧无 mob）
- **同门连带（ENT-1b）**：模型 A/B 40,703 world px（vs v0.10.0 资产，
  比例修正可见）+ 摆动 on/off 157,158 world px（config 三验协议）
- **性能（插桩 900f，发布前测试）**：render med 0.47-0.60 ms / ents
  11-26 ms/300f / update med/p95 50-54/85-97 ms = 踏阶+动画成本可忽略；
  插桩零残留 + 1389 绿

## 偏离
- quirk ①（+X/+Z 1 格宽台阶踏不过）= player 共享行为，记档不修（单独立项）
- spider 爬墙未做（MC 有，出范围 = KNOWN_ISSUES 既有边界）
- 踏阶速度不随 mob 类型缩放（与 player 同款 1.0 抬升，帧锁定）
