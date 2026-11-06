# Phase v3.22.0 — V3-14d strider 骑乘（门 212 全 PASS，2026-11-06）

## 范围与定案

**strider 骑乘 = 项目首个骑乘面（新基建）**：玩家右键 rideable
mob 骑上（承载 snap + steer 接管 + 落乘收口），WASD 直控
（W-hold 前进 / 松开漂停），Space/LeftControl 落乘，熔岩面
骑乘全程（点火豁免/行走 snap 既有物理保留）。

- **数据**：strider.json +`"rideable": true`（strict bool 默认
  false，walks_on_lava 孪生解析范式）；ride_height **不做数据
  化**——首骑乘物背位 = 身体盒顶 1.05（strider.json model 盒），
  core 常量 `kRideCarryHeight`（YAGNI，MC 亦按物种固定）。
- **承载 = 会话态，零 format**（31 不变）——**唯一自洽解**非妥
  协：mob 不入档（format 30 设计），重载 strider 即消失，骑乘
  关系无从持久（存了 = 悬空引用）；先例族 = portal 冷却 900f /
  飞行 / 羊毛实例态。
- **Player `m_carried` 会话旗**（setFlying 孪生）：update 里
  wish 锁零 + moveInSubsteps/stepUp 旁路 + fall 复锚每帧（落乘
  1.05 落差永不坠伤）+ integrate no-op（重力跳过，比 fly 更锁——
  垂直键亦无效）；保留 look / survival 计时器（熔岩灼烧/饥饿/
  void——落乘进熔岩会烧 = MC 语义）/ swing（位移驱动腿摆白送）。
- **MobEntity `m_ridden` + riderSteer**（runAi 顶部分支，
  flyCruise 纯新分支先例）：forward = 面向 rider 航向 + 数据 speed
  沿航向（AI set-velocity 约定）；松开 = kRideDecel 0.9 指数衰减
  → kRideStopEps 0.02 清零（帧锁定停帧 = release+38）；**ambient
  roll 保留**（per-mob 流契约 PITS 403⑤——骑乘中 hiss 照放，1800f
  = 13 次钉）；wander nextMix **不耗**（AI 被接管，正确）；
  **collidesWithEntities() override → !m_ridden**（分离 pass opt-
  out——骑手盒与坐骑盒设计性重叠，漏 = 逐帧顶离，风险①）。
- **Game ride pass（updateRide）= 单收口**（handleInteraction 后、
  player update 前每帧跑；无骑乘 = 一次 null 检查）：落乘条件
  （坐骑死 / 玩家死 / 飞行 toggle / 面板开 / 坐骑入水 inFluid ∧
  ¬onLavaSurface【熔岩面保持骑乘 = MC 定案】/ portal arm 帧）+
  steer 推送（玩家键愿 × 相机系 flatFwd/right 镜像）+ 承载 snap
  （setPosition 背位 + setVelocity 水平 = swing/落乘漂移）。
- **交互**：tryRide（RMB 链 tryBarter 后，tryMilk 范式：数据谓词
  rideable，非 rideable = 穿透位级；已骑 = 本坐骑吞点击/异坐骑穿
  透；飞行中拒骑吞点击）；**Space 落乘挂在双击飞行窗记录之前**
  （落乘 tap 不得武装飞行 toggle——风险④）；LeftControl = MC
  dismount 键孪生。
- 渲染零改动：第一人称相机 = eyePosition 自动（承载 snap 后）/
  第三人称 orbit 自动 / drawPlayerBody 站姿背位 1.05 自动（骑姿
  模型不做 = 偏离 6）。

**MC 语义偏离 6 记档**（recon §7 查证 minecraft.wiki）：① 无
saddle 直接骑 ② WASD 直控（无 fungus stick 转向/加速/耐久）③
水伤/下沉不做（落乘触发保留；下界无水 E2E 不可达 = 单测臂）④
原地落乘（无「面向可达固体块安全落乘」判定）⑤ 无 step/step_lava
脚步音族（67 件 wav 不动）⑥ 站姿背位（无坐姿 pose 变体）。

## 实现（代码 6 + 数据 1 + 测试 3 + CMakeLists 1）

- `src/entity/mob_registry.{hpp,cpp}`：rideable 字段 + strict 解析。
- `src/entity/mob_entity.{hpp,cpp}`：m_ridden/setRidden/setRider-
  Steer/riderSteer/kRideDecel/kRideStopEps/MobState::Ridden 尾钉/
  collidesWithEntities override。
- `src/player/player.{hpp,cpp}`：m_carried + carried 分支（wish 锁 /
  move+stepUp 旁路 / fall 复锚 / integrate no-op）。
- `src/core/game.hpp`：m_ridingMount 会话指针 + updateRide/tryRide/
  dismountRide 声明。
- `src/core/game_interaction.cpp`：tryRide + RMB 挂点 + Space/
  LeftControl 落乘（双击窗绕过）。
- `src/core/game_frame.cpp`：updateRide（kRideCarryHeight 1.05）+
  dismountRide + 帧序插入 + portal arm 落乘门。
- assets：strider.json（+rideable，FNV 重钉 b0f3fef8c3449477）。
- tests：+9（MobRegistry rideable ×2 / MobRideTest ×5 / PlayerTest
  carried ×2）+ StriderDataPinned +rideable + NetherMobAssetsPinned。

## 门 212（全 PASS）

- **S1 结构免疫第 93 次**：60f 无输入 ×2 ≡ **base19 字节恒等** +
  savenorm EXACT（骑乘全链冷：无 RMB → 无 m_ridingMount → ride
  pass 一次 null 检查；strider 下界惰性）。
- **S2 骑乘臂 = authored demo**（lava 沟场景：x40 z34-38 五列挖
  y32-52 = 105 air + 桥 netherrack y31 ×4【z35-38 顶 y32 骑行
  通道 + 玩家起点】+ glowstone 灯排 y38 ×5【脚格光 9 ≤ 15 spawn
  门不变】；strider 独占池变体 run 目录【PITS 403② fresh 拷】；
  玩家 (40.5,32,38.5) 桥面 pitch −0.5）：f30 LMB 捕获【瞄准
  下方桥面=打空】→ f40 MouseMove dy−114【pitch −0.215 = strider
  体心 atan2(−0.87,4.0)，kReach 4.09 界内】→ **f60 strider 生
  (40.5,32.0,34.5)**【5-6m 环列全被 netherrack headroom 拒 + 桥
  列 headroom 拒 = (40,34) 唯一合法列，首尝试首生】→ f120 RMB =
  **`rode the 'strider'`** → f130 180° 转向（dx −1257px，单帧无
  delta 钳制）→ f140-260 W hold 120f【strider +Z 2.0 块沿桥，
  承载 snap 逐帧】→ f320 Space = **`dismounted the 'strider'`**
  + 1.05 落桥【零 burn 零伤】→ f400 终帧。**×2 截图 RAW 恒等 +
  rode/dismounted 逐字恒等**（帧锁定全程）；第三人称视觉臂（f200
  C + f210 低头）= 玩家立于 strider 背、沟内灯亮实证。终态权威 =
  截图 + DEBUG 行（**demo save spec 红线不回写** PITS 403①）。
- **S2 无输入臂**（同场景 1800f ×2）：截图 RAW + savenorm EXACT +
  7/7（hp 20 桥面零 burn / deathCount 0 / 玩家原位 / 沟 5/5 / 桥
  4/4 / 灯 5/5 / fires []）。
- **S6 点名**：rideable "yes" → init rc 1 `mob_registry.cpp: mob
  file 'data/mobs/strider.json': 'rideable' must be a boolean`。
- **S2b canary 4 demo** ×2 RAW + savenorm EXACT（**sim 不 bump
  实证**——4 canary 全主世界无 strider 无骑乘路径）。
- **S3 全套 2856/2856**（+9），`-Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = 11c1840 v3.21.0 + g139 vs 新 =
  工作树 + g139（单次插桩 + 300f 冒烟验 PERF-WIN），900f 无输入
  tools/perf_gate_config.json 独占——banner 实证 v3.21.0/v3.22.0 +
  PERF-WIN 三窗 upd med 0.589/0.762/0.702 vs 0.690/0.679/0.709 +
  render med 0.637/0.795/0.773 vs 0.760/0.714/0.749 + swap med
  15.21/14.91/14.93 vs 14.96/15.09/14.99（同档）+ fluid·fires·
  spread·ents·stream 同档 + **changesApplied 9/3/1 vs 9/3/1 全逐位**
  + **终档 savenorm EXACT**（零 format = 最强形——骑乘全冷路径
  结构性零成本）。

## 门内捕获（PITS 404）

1. **git checkout「还原」temp 改的数据文件 = 未提交的功能加法一
   并回退**——v3.21（piglin.json barter）+ v3.22（strider.json
   rideable）两连踩：S6 臂 `git checkout --` 后 rideable 消失，
   探跑静默用旧数据（tryRide 谓词假 = 瞄准全对也不骑）。纪律 =
   temp 改动用 python 原地改 + 手工还原 + **grep 回验字段在**。
2. **骑乘场景视线几何 = 沟 + 桥 + 灯三件**：1×1/21 深沟的 rim
   遮挡（射线降过台面在沟口外）→ 5 列贯通沟（z34-38）+ 4 桥格
   （顶面 y32 = 骑行通道 + 玩家起点，熔岩顶 +1 免 burn）+ glow-
   stone 灯排 y38（18 格上 = 光 0 不可见 → 6 格上 = 光 9，spawn
   光门 ≤15 不变——灯位须对 spawn 门验算）。
3. **demo 瞄准换算**：pitch = atan2(dy, distH) 按**体心**（feet
   + height/2）非 feet；delta px = Δrad / mouseSensitivity 0.0025
   （180° = 1257px 单帧——mouseDelta 无钳制）；LMB 捕获点击先
   打空（下方桥面/熔岩，勿打坐骑 = 1 HP + 击退污染）。
4. **Space 落乘绕过双击飞行窗**（风险④）：落乘 tap 若记
   m_lastSpacePressTime = 落乘后速按误开飞行；挂点必须在记录之前
   分支。
5. **分离 pass 顶坐骑**（风险①）：骑手盒（背位起 1.8）与坐骑盒
   （1.5）设计性重叠 → 逐帧推离；collidesWithEntities() opt-out
   弹道先例（entity.hpp 虚门）首消费于 mob。

## 记档偏离

- MC 6 项偏离（上节）+ **落乘保留坐骑水平速度**（漂移——无击退面
  项目的自然出口，落乘 1.05 落桥后滑 ~速度×衰减）。
- **落乘入水 = ride pass 一行谓词**（inFluid ∧ ¬onLavaSurface）；
  下界无水（worldgen 无 + 水桶蒸发定案）E2E 不可达，无独立单测
  臂（组成面 inFluid/onLavaSurface 既有 V3-3 strider 测试族钉；
  为 5 个 bool 参数写 64 行真值表 = 剧场，弃）。

## 变更文件（显式路径，11）

见 git commit。
