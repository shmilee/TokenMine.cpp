# Phase V2-9 — step-up quirk 修复（v2.9.0，门 180 全 PASS）

> 2.x 第九项（ENT 行）：修 KNOWN_ISSUES 203/382 条「单独立项」的两个
> 潜伏 quirk（ENT-3 探针实证，player/mob 共享 `Entity::stepUp`）。
> 设计定案 = /tmp/opencode/v29_design.md（按 KNOWN_ISSUES 条重建版）。
> **player step-up 行为变更**（记档立项）——整数脚高位级不变红线。
> 零 format（25 不动）/ 零 config 新键 / 零新数据 / 零纹理 / 零新帧状态。

## 修复内容

### quirk ① +X/+Z 恰 1 格宽台阶踏不过
- 机制：stepUp 的 front 格 `snapped + (w > 0 ? 1 : -1)` 不对称——
  −dir 贴住面的对面格 = 贴住格（✓），+dir = 贴住格**之后那格**
  （✗）：+X/+Z 恰 1 格宽台阶的阻挡格永远不被检查
- 修复（src/entity/entity.cpp `stepUp`）：`front = snapped +
  (w > 0 ? 0 : -1)`（+dir = 贴住格；−dir 不变）
- 既有 TRUE-1-block 判定（frontTop ∈ (0, 1.0+tol] + fy+1/fy+2 空）
  原样作用于修正后的 front 格

### quirk ② 非整数脚高巧合对齐凭空抬升 1.0 再落回
- 机制：stepUp 调用点无条件（onGround && wish>0，非本 tick clamp
  门控）；「贴住整数块面」只验 `|face − snapped| ≤ kFaceEps`（位置
  判，非因果判）——整数脚高下 face 对齐 ⟹ 真 clamp（无问题）；
  非整数脚高（slab 顶 1.5 / 楼梯斜面）face 连续行走中巧合过整数
  + 前方脚行 solid（低半板/同高 slab）→ `m_position.y += 1.0` 凭空
  抬升再落回（mob slab 追逐 frame 606 实证）
- 修复（同款 stepUp，flush 位置测试后、TRUE-1-block 判定前）：
  **无状态真阻挡验**——front 列 (cx,cz) 在身体 y 跨内（行 y 从 fy
  起、y < feet+height = moveAxis 碰撞同款行）∃ solid 格且其顶
  `y + solidTopHeight(格) > feet`（**严格不等式**，hard rule 6；
  同高 slab 顶 = feet → 不重叠 → 拒）→ 无真实阻挡的巧合对齐
  continue。不加 per-tick 旗标、不改调用点、不加帧状态成员
- **整数脚高位级不变论证**（注释钉）：整数 feet 下 face 对齐 ⟹
  真 clamp ⟹ 阻挡格在 front 列脚行（顶 1.0 > feet 整）→ 新校验
  恒过 → 既有 step-up 单测族零改动全过
- player/mob 共享生效（ENT-3 同款共享纪律）

## 变更文件
- `src/entity/entity.cpp`：stepUp 两处（front 对称化 + 真阻挡验）+
  机制/位级不变论证注释
- `src/entity/entity.hpp`：stepUp 文档注释同步
- `tests/test_player.cpp`：fixture +slab 注册；+3 新测试
  - `StepsUpSingleCellStepPlusX`（单格石台阶 x=10 横向 2 格宽，+X
    起步 90f → 踏过 + 走下远端）
  - `StepsUpSingleCellStepPlusZ`（同款单格 z=10，yaw π 走 +Z）
  - `SlabTopWalkNoCoincidentalLift`（slab 长条顶 1.5 行走 300f →
    y 轨迹恒 1.5 无 1.0 脉冲【maxY 钉】；起步相位 8.17 = pre-fix
    树逐 0.01 扫相探针确定）
- `tests/test_mob.cpp`：`SingleCellStepPlusXStaysClampedAtTheQuirk`
  → **反转** `StepsUpSingleCellStepPlusX`（ENT-6 封死走廊保留，
  断言翻为踏过 + 无 pathfind 介入）；3 处旧 quirk 引用注释更新
  （零行为改动）
- `CMakeLists.txt`：2.8.0→2.9.0（单点）；`tests/test_debug_overlay.cpp`
  F3 钉 v2.9.0（PITS 355）

## 实现侧自证（编排者独立核证）
- 构建 `-Wall -Wextra -Wpedantic -Werror` 0 警告
- **2174/2174**（2171 + 3 新；反转不计增减；编排者独立复跑）
- TDD 证据链（pre-fix 树 FAIL = bug 复现）：
  - PlusX：x = 9.7（贴住 10.0 clamp，踏不过）
  - PlusZ：z = 9.7（同款）
  - SlabTopWalk：maxY = 2.5（frame 56 凭空 +1.0 抬升）
  - mob 反转测试旧语义：sawPathfind = true（物理拒踏 → BFS 介入）
- 既有 step-up 单测族零改动全过（整数脚高位级不变确认：player 侧
  ClimbsOneBlockButNotTwo / AlongZ / CrouchDoesNotStepUp /
  StepUpBlockedByLowHeadroom / CrouchStaysOnLedge + mob 侧 8 个，
  含红线 StepsUpSingleCellStepMinusX / wall·fence·2 格拒踏 /
  knockback 不穿透 / frame-locked 确定性）
- 冒烟 banner v2.9.0 + base18 恒等（PITS 375 暖机后 60f =
  179116 B IDENTICAL——无输入零位移 stepUp 永不触发 = 结构免疫）

## 门 180（编排者执行；old 侧 = v2.8.0 = 38a778a 树）— 全 PASS

### S1 = 第 180 次结构性免疫
- fresh 60f 四方（new v2.9.0 n1/n2 vs old v2.8.0 o1/o2）全 179116 B
  **RAW 全字节一致** + n1 ≡ base18 锚 + 跨版 savenorm **EXACT**
  （零 format——stepUp 只在水平位移时触发，无输入全惰性）

### S2 = 1 格台阶 E2E（键盘协议——PITS 368 已随 X 重起根治）
- 场景（存档编辑自建平台，probe180_terrain 地面真值）：斜坡地
  x8..22 z0 上铺 y45 石地台条（y46..60 清空）+ **恰 1 格宽台阶**
  x16 z−2..2 y46（y47..60 清空）；玩家 (12,46,0.5) yaw −π/2 朝 +X，
  W 键 3s
- **旧树（v2.8.0，quirk ① 在）：x = 15.700 ×2 恒等 = 台阶西面
  精确 clamp（16−0.3），y 46 不抬升——bug 实机命名复现**
- **新树（v2.9.0）：x = 25.12/25.08（>16.3 过线断言 ×2）= 踏过 +
  继续行走**（0.04 差 = 输入时延抖动，协议预期；y 36 = 过线后
  地台条 15 格挡不住 3s 行走，玩家走出条端沿坡下走——场景 artifact，
  过线证据 = x 坐标，步升本身由单测 maxY/踏过断言钉）
- 键盘 = 完整输入协议回归（门 177 起鼠标-only 绕过退役）

### S3 套件
- **2174/2174**（编排者独立复跑）

### S6 命名回归
- pre-fix TDD FAIL 四证（上「实现侧自证」）+ S2 旧侧实机 clamp
  15.700 = 双树命名互斥

### 性能 A/B PASS 同世界同成本（用户硬要求，发布前）
- v2.8.0（tm_v290_perf = 38a778a + g139 27 标记）vs v2.9.0
  （tm_v290_perf_new = 终版工作树 + g139 双 27 标记，banner 双侧
  实证）：fall 900f 无输入 seed 123456789：
  - PERF-WIN 三窗全指标同档（upd med 0.60/0.62/0.61 vs
    0.60/0.63/0.58 ms；p95 窗间摆动无系统偏移；render med
    0.51/0.52/0.48 vs 0.50/0.50/0.41；swap 15.4/15.3/15.4 vs
    15.4/15.4/15.6 = vsync 主导同值；fluid/fires/ents/stream 全
    同档——stepUp 无输入零触发 = 修复路径全惰性）
  - **PIPE3-S0 changesApplied 三窗 max 9/1/3 双侧逐位**
  - **终档 fluids 16/16 + blocks 36/36 集合恒等**（同世界最强形）
  - **台阶场景自成本**（deck+step 世界 900f 双侧 = 海岸流体追赶
    饱和窗）：upd med 98/120/92 vs 100/119/91 ms；fluid
    5.97/10.33/17.17 vs 5.63/10.34/16.56 = **双侧同款场景 churn
    同档**（99.11 预算追赶，修复无关）

## 偏离（记档）
（无——修复 = 记档两 quirk 的定向消除；既有行为面：潜行禁 step-up /
流体无门 / TRUE 1-block 顶高门 / 每 tick 至多一次 / X 先 Z = 全不变）
