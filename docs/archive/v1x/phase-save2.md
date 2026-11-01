# SAVE-2 速度入档（v1.x 矩阵 26/33，v1.26.0，门 141，SAVE 行 1/2）

## 定案

原始定义已佚 = 定案 **玩家速度入档**（消费 KNOWN_ISSUES 存档节「玩家速度/onGround
不存」条的速度半；onGround 半 = 派生态不存 = 记档偏离）：

- **format 21→22 `player.velocity`**（3 数组，**恒写**——游戏恒有真值：静止 [0,0,0]，
  ENT-7 stop-eps 使静止恰为零）
- **键纪律**（oxygen/stats 同款）：format-22 文件 REQUIRE 键（缺失 = 版本谎报点名）；
  format ≤21 携键 = 版本谎报（unknown field 点名）
- **读档 = `restorePosition` 后 `setVelocity`**：restorePosition 的 teleport 本清零
  速度（Phase 36 fallStartY 重钉不变），单条重赋值 = 全部运动恢复；≤21 载入保持
  (0,0,0) 缺省 = legacy 零速 restore 值保真
- **损坏档区间** `kSavedVelocityMax = 100`（oxygen [0,150] 先例；游戏自身速度远低于
  此：终端坠落 38 / 跳跃冲量 8.5 / 疾跑 ~5.8）
- **onGround 不存**：位置派生（空中读档 onGround 自然 false；静止读档首子步碰撞
  置位 = 值保真）

## 实现

- `save.hpp`：`kSaveFormat = 22` + `kSavedVelocityMax` + `SaveGame::playerVelocity`
- `save.cpp`：`serializeSave` 恒写 `player.velocity`；`parseSave` ≥22 白名单 +
  必需/类型/区间三级点名
- `game.cpp`：写侧 `save.playerVelocity = m_player->velocity()`；读侧
  `restorePosition` 后 `m_player->setVelocity(save.playerVelocity)`
- **零运行期成本**：新代码只在存档 serialize/parse 路径（退出/载入一次性）；
  无输入 fresh 跑不载档 = 全路径结构惰性（S1 免疫论证根）

## 测试

**1872/1872（+6 `SaveFormat22Test`**：往返值保真 / 静止 = 零三元组 / 22 缺键点名 /
≤21 携键 = 版本谎报点名 / ≤21 载入 = 零（迁移）/ 区间边界 + 类型点名**）**，
`-Werror` 0 警告，banner v1.26.0。

契约演进：format 钉 21→22 ×9 + velocity 剥离 ×27（test_save 20 = stats 剥线 ×17 +
`downgradeTablesToFormat18`/`downgradeToFormat14` helper ×2 + `(*j)` 内联 ×1；
test_campfire 5；test_drown 3；test_durability 内联重建）。三批漏网 = 批量剥线正则
只认 `root[` 前缀，`root2` 变体 / `(*j)` 内联形 / 独立 helper 函数各漏一处——
同款坑第 4 次复现（PITS 251 类：场景/契约块改写须全形扫描）。

## 门 141

**S1 PASS = 第 141 次结构性免疫 + base18 锚刷新**（168140→387726 B = PITS 265/315
WM 平铺态：本轮游戏窗未平铺 1362×744 全宽，四方先行字节一致后刷新采用）：
- fresh 60f 无输入四方 ×2：new×2 RAW 387726 B EXACT / old×2 EXACT / **new≡old
  字节一致**（结构性免疫：无输入不载档 = 读侧惰性；写侧只在退出存档一次性）
- 同侧 savenorm ×2 EXACT ×2
- **跨版 save diff = 恰 2 项**：`root.format: 22 vs 21` +
  `root.player.velocity: only in new`（[0,0,0]，静止零三元组 = ENT-7 stop-eps
  结构保证）——格式升版无其他字段扰动（fires remaining 剥离后 = PITS 244 纪律）

**S2 PASS（−Z 漂移终态判别，f300，config nomobs+nodecay+regen 9999）**：
- s2a vel [0,0,−20] ×2：终位 **(22.5, 38, −7.96/−8.16)** hp 9 exact、vel
  [0,0,0] exact、x 22.5 全稳、**z ∈ [−11,−5] 带 + x2 spread 0.20 ≤ 0.5**
  （碰撞落地子步离散使终点 ±0.2 块 jitter = PITS 320 带断言根）
- s2b vel [0,0,0] ×2：终位 **(22.5, 38, 0.5) 全 exact**（整数平面落位 = 跨 run
  全稳）hp 9 exact + **×2 RAW 474376 B 字节一致**
- **s2a vs s2b = z 差 8.4-8.7 块互斥**（漂移 vs 原地 = 速度驱动物理实证）
- s2c fmt21 孪生（无 velocity 键）×2：终位 ≡ s2b 全 exact + **跨侧 player 段
  位级一致 ×2** + **twin PNG ≡ zero PNG 474376 B 字节一致**（legacy 零速
  restore 值保真 = 迁移免迁证明，视觉 + 状态双证）
- 跨侧世界 churn = 仅流体前缘 1-2 格 level/kind（熔岩湖 P90 溢流 + fire 块
  real-dt 寿命 = **PITS 244/283 类**：同场景同二进制两跑自分歧实证【zero
  run1 vs run2 流体前缘 82↔77 即分歧】= 环境 pacing 类非 legacy 路径差）

**S2d PASS**：旧 v1.25.0 二进制 + format 22 场景 = init 点名
`unsupported save format 22 (this build reads formats 1-21)`（负证据免费）

**S6 PASS ×2（新侧点名）**：
- fmt21 携 velocity 键（版本谎报）→ `unknown field 'velocity'`
- fmt22 vel [200,0,0]（越界）→ `player.velocity[0] must be in [-100, 100],
  got 200.000000`

**S3 PASS**：套件 1872/1872。

**性能 A/B PASS**（真 v1.25.0 tm_v125_perf vs v1.26.0 tm_v126_perf，banner
实证，同款 g139 插桩两侧同打，fresh --new --seed 123456789 900f 三窗 + 终档
集合）：PERF-WIN 全指标同档（upd med 0.365/0.381/0.421 vs 0.379/0.367/0.396
ms【win3 +4% = 窗口噪声级】/ render med 0.52-0.61 vs 0.58-0.63 / swap med
15.48-15.64 vs 15.48-15.54 vsync 锁定 / fluid 0.044-0.056 vs 0.050-0.057 /
fires 0.008-0.011 / spread 0.00035-0.00066 / ents 0.003-0.004 / stream
0.013-0.017）+ PIPE3-S0 tickMs med 0.044-0.056 vs 0.051-0.057 同档 +
**changesApplied max 9/3/1 三窗逐位一致** + **终档 fluids 18/18 + blocks
55/55 集合恒等（diff 0）= 同世界同成本**【结构惰性最强形：velocity 写只在
退出存档一次性、读只在载档一次性，无输入 fresh 跑零执行】= 成本可忽略，
零回归。

**S2 初版设计被探针推翻重做**（PITS 320）：初版 = 「vy −8 动量坠落 vs 零速，45f 时
一侧已落地一侧仍空中」——探针实证两侧 45f 均已落地同终态（y38/hp9）：玩家物理
dt = 真实墙钟（kMaxDt 0.25 钳、无固定步累加器），本环境 pacing ~20 fps 时 14 块
静止坠落 ~19-20 帧即落地（帧锁定推算 56 帧）= 中间帧判别结构性失效。重做 = **水平
漂移终态判别**（vel [0,0,−20]：空中漂移 8.7 块落 y38 平地，终位 z ∈ [−11,−5] 带
断言 + x2 spread ≤0.5；zero 侧整数平面终位 (22.5,38,0.5) 全 exact；fmt21 孪生
≡ zero 侧 = legacy 回归；旧二进制拒 format 22 = S6 负证据）。

## PITS

- **PITS 320（玩家物理 = 真实 dt 随 pacing；坠落场景中间帧判别在低 fps 环境
  结构性失效）**：game loop `dt = frameStart − lastFrame`（kMaxDt 0.25 钳、无固定步
  累加器）→ 玩家坠落轨迹的**帧索引**随环境帧率变：~20 fps 时 14 块静止坠落 19-20 帧
  落地（帧锁定语义下应 56 帧）——「45f 时空中/落地互斥」类判别量在两侧都已落地后
  恒同终态（探针实证 y38/hp9 ×2）。更：碰撞落地帧的子步离散使落地瞬间水平速度随
  run 抖动 → 漂移场景终点位置跨 run ±0.16 块（平地指数解 fps 无关，**碰撞落地**
  不是）。纪律 = **判别量只取终态 exact 值**（整数平面落位 / hp / vel [0,0,0] =
  跨 run 全稳）**或带**（漂移终点 ≥5 块裕量 + x2 spread ≤0.5），中间帧不门控；
  ×2 RAW 字节比要求终态结构静止（vel 恰 0 + 整数位置 + 无 mob/衰减/回血——
  config 禁 regen 9999 = PITS 303/311 同款消变量）。
