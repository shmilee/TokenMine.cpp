# Phase V2-13 — 床（睡觉跳夜 + 重生点）（v2.14.0，门 185 全 PASS）

> 二批 SURV 行（BACKLOG「中」：数据块 + 睡眠通道【夜间 RMB】+ 跳到
> 黎明 + 重生点更新【R 重生流既有】；消费「v1 无床」两处记档：
> 保护环注释 / 和平难度锚）。MC 机制 11-04 编排者 wiki 核定（Bed
> 页：sleeping/spawn point/explosions/crafting）入设计稿
> /tmp/opencode/v213_design.md；fixer 一次交付（ses_e5cd31324ffeG1T00
> HZMt6dmJI）。

## 实现

### 数据面
- **块 15 件**（bed 族）：`dye_base: bed` + `dye_color` 15 色（wool
  同款词表无 lime，white = base 无后缀）/ sound_material cloth /
  hardness 0.2（MC 精确）/ **shape Slab**（下半格盒 = MC 1×2 床的
  单格适配，记档偏离 1）/ drops 自身 / 无 interact（dye_base 互斥）
- **纹理 15 tile**：generate_block_textures.py `make_bed_tile`（彩色
  毯 = WOOL_PALETTE 同款色 + 头端白枕带，新 salt 985-999）；atlas
  104→119（16×8=128 余量内，既有 104 格零移位 md5 实证）
- **配方 15**：shaped `["   ","WWW","PPP"]`（3 同色 wool + 3 planks
  = MC 精确）
- **音**：新 `sleep.wav`（generate_sounds.py：180→90 Hz 指数滑降 +
  360 Hz 弱二次 + 微颗粒，固定种子 0x51EE，0.4 s 缓入缓释；既有
  40 wav 重跑字节不变）
- 染色：Phase 41 dye 机制免费（RMB 持染料于床 = 重染色，落回既有
  路径）+ init 染色家族交叉校验自动覆盖 15 变体

### 核心面（core/bed.{hpp,cpp} 纯函数 + game 薄适配）
- **trySleep RMB 动作**（game_interaction RMB 链纯插入：面板分支后、
  染色前；命中块 dye_base=="bed" 族判 + 持染料落回染色路径）三门
  定案序：
  - **门 b 距离**：玩家脚心 vs 床格中心 3D ≤ kSleepMaxDistance 3.0
    （MC 精确，平方比较含边界）
  - **门 c hostile 盒**：床格中心 水平 ±8 / 垂直 +5/−1 闭盒内无
    hostile（kSleepHostileRadius/Above/Below；hostile = mob 数据旗，
    spawn 保护同款判据，无物种 switch；被动/掉落物/箭豁免）
  - b+c 过 → **设重生点**（setSpawnPos 床格中心；MC：RMB 恒设点，
    夜或日）→ **门 a 夜窗**：t ∈ [kDuskEnd 0.55,1)∪[0,kDawnStart
    0.05)（MC sky≤11 = 项目坐标 [0.586,0.045]，项目夜窗略宽 =
    记档偏离 3，命名常量推导）：
    - 夜内 = **即时跳夜**（记档偏离 2：MC 101-tick 入睡动画 + 受击
      唤醒不做）：`setTimeOfDay(kDawnStart)`（= 06:00 黎明，ramp
      平台）+ 雨/雷天气 → `weatherInitialState(false, …)` 纯重置
      （CLEAR，MC"睡觉清雨"同款；晴天不动）+ "sleep" 音空间化于床
      + emitTutorialEvent("sleep") + info 日志（E2E 证据行）
    - 夜外 = 仅设点（MC 白天 RMB 语义）
  - **门 b/c 拒 = 全拒含设点**（记档偏离 6）
- **重生流**：`Player::setSpawnPos/spawnPos` 新访问器 + R 键收口
  `Game::respawnPlayer()`（验床纯函数 `bedRespawnValid`：world 只读
  campfire 范式——该位有任意床族块 + 房间空间【床格支撑豁免，
  Entity::hasRoomAt 先例】→ 床位；否则回退世界出生点 m_mobs.
  spawnPoint + setSpawnPos 重新钉位 = 床破坏/堵塞永久回世界出生
  点，MC 同款"仅 respawn 时查"）
- **"sleep" 教程词**入闭合格言裸词表（kTutorialBareGoals 4 词；
  系列 JSON 不动，语法错误文案从词表构建自动跟进）

### format 28（存档）
- `player.respawn` = [x,y,z] int 三元组**恒写**（floor 各分量；
  新档 = 世界出生点 init 写入）
- 三向：27 携 respawn → 白名单点名拒 / 28 缺 → 点名拒 / 27 载 =
  哨兵 -1 → applySave 后覆写世界出生点（parse 无法知世界出生点 =
  game 侧迁移）；分量非整数 / y∉[0,128) 点名拒；负 x/z 合法
- 测试侧：makeSave 填 playerRespawn + 各降级 strip 链加 "respawn"
  （~35 降级位自动覆盖）+ format 钉 27→28

## 变更文件（73）
新：core/bed.{hpp,cpp} / blocks/bed*.json ×15 / recipes/bed*.json
×15 / textures/blocks/bed*.png ×15 / sounds/sleep.wav /
test_bed.cpp（22 TEST）；改：player.{hpp,cpp}（spawnPos 访问器）/
game.hpp / game_interaction（trySleep + RMB 插入）/ game_ui
（respawnPlayer 收口）/ save.{hpp,cpp}（28）/ game_save
（collect/apply + 迁移）/ game_init（"no beds in v1" 注释更新）/
tutorial.{hpp,cpp}（词表）/ CMakeLists（2.14.0 + bed.cpp +
test_bed）/ generate_block_textures.py + generate_sounds.py（新
tile/音函数）/ block_atlas.json（119）/ 5 测试文件（format/strip/
钉）

## 门 185（old = v2.13.0 tm_v2140_base/perf = 72e0028；编排者执行）
— 全 PASS

### S1 = 结构免疫第 66 次（**免重锚**）
- 新侧 60f ×2 恒等 且 ≡ 现锚 179027 B 逐字节（无输入：床不入
  worldgen【15 块零 bed 引用 grep 实证】+ RMB 门永不开；新 tile/
  音不绘制不装载成本 = 目录惰性）
- savenorm（官方 gate_compare.py）= 恰 {format 28 vs 27,
  player.respawn}
- **门协议教训（PITS 379）**：首跑误用 tm_v2130_base（= v2.12.0
  format 26，门 184 旧侧）→ savenorm 3 键假象；旧侧参照树 = 前一
  版本 tag 且 banner 实证

### S2 = 三臂输入 E2E（PITS 376/378 协议）
- **臂 a 夜跳**：床 (0,54,-3) + t=0.70 + pose (0.5,54,-4) yaw 3.0
  pitch -0.353（g184 实证几何）+ 1200f：new 终档 **time 0.0796**
  （跳 0.05 + tail）+ **respawn [0,54,-3] = 床格**；old（无床同
  pose RMB air）time 0.7333 正常推进 + format 27 无 respawn = 形状
  互斥
- **臂 b 日设点**：t=0.30：new time 0.3333 **不跳** + respawn =
  床格（MC 白天 RMB 设点）；old 无 respawn
- **臂 c mob 拒**（PITS 373 scratch spider 协议重构几何）：玩家
  (0,52,-16) hp 60（距世界出生点 16 > 保护半径 12）+ **床置蜘蛛
  恒停格 (-1,51,-18)**（gate 183 实证 f300 恒停位 → 蜘蛛必在 8×5
  盒内）+ aim yaw 0.785 pitch −1.016（射线核算入 slab 前脸；slab
  半高 = 比整块瞄准更陡，PITS 379③）：new 终档 **time 0.7333 不
  跳**（拒睡）+ respawn [0,54,0] = 世界出生点（非床）+ **hp 22/60
  = 蜘蛛交战实证**（接近性证据）
- capture 单击对 hardness 0.2 床 = 0.1s 进度 < 0.3s 破块时长 + 释放
  重置 = 床不碎（PITS 379③）

### S3 套件
- **2330/2330**（2308 + 22 TEST；编排者独立复跑）

### S6 = 5/5 点名（真二进制 scratch + 存档手术）
- 缺染色变体（删 bed_red → 家族校验点名）/ bed 携 interact
  （dye_base 互斥点名）/ hardness −1（范围点名）/ 27 携 respawn /
  28 缺 respawn

### 性能 A/B PASS 同世界同成本（用户硬要求）
- v2.13.0（tm_v2140_perf）vs v2.14.0（tm_v2130_perf_new = 终版工作
  树 + g139，banner 双侧实证）：fall 900f 无输入 seed 123456789：
  - PERF-WIN 三窗全指标同档（upd med 0.54/… vs 0.63/0.58/0.58 ms；
    swap 15.2-15.4 vsync 同值；fluid/fires/spread/ents/stream 全同档）
  - **PIPE3-S0 changesApplied 三窗 max 9/1/3 双侧逐位**
  - 终档 fluids 16/16 + blocks 36/36 集合恒等 + respawn old=F/
    new=T 唯一预期 diff
  - **预置床场景自成本**（床 (0,54,-3) t=0.70 无输入 900f）：同档
    （RMB 门 + 验床仅 respawn 时 = 零成本实证）

## 偏离（记档 6 条）
1. 床 = 单格 Slab 形（MC 1×2 两格无项目先例，两格对触
   world/setBlock/存档红线）
2. 睡眠 = 即时跳夜（MC 101-tick 动画 + 受击唤醒不做）
3. 夜窗 = 项目夜带 [0.55,1)∪[0,0.05)（MC sky≤11 = [0.586,0.045]
   略窄）
4. hostile 盒 = 单格床整格作头位适配（MC 头 8×5）
5. mob 炸床不做（项目无爆炸机制；门 c 覆盖"附近有怪睡不着"）
6. 门 b/c 拒 = 全拒含设点（MC 白天设点仅对无怪情形）

## 消费记档
- game_init "there are no beds in v1" 注释 → V2-13 语义（世界出生
  点 = 新档重生点 + 验床回退锚）
- 和平难度 ENT-8 记档（保护锚 = 世界出生点）：不变（MC 保护环亦锚
  世界出生点，与床无关）——记档保留，"无床"前提注记更新
