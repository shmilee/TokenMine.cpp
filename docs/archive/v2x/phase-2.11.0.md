# Phase V2-19 — 视角/移动/时间显示批（v2.11.0，门 182 全 PASS）

> 2.x 第二批 UX 行首项（任务号 V2-19 = 实机报障续编号；**版本 =
> 执行序 v2.11.0**——用户 11-03 实机诉求提前执行，先于 SURV 行
> V2-11 XP；任务号与版本号解耦 = V2-1/V2-2 同款先例）。三子项
> 全会话态 = **零 format / 零 config 新键 / 零新数据文件**。设计
> 定案 = /tmp/opencode/v219_design.md。

## 实现

### ① 第三视角滚轮缩放（"拉近拉远距离"）
- 输入：SDL_EVENT_MOUSE_WHEEL（y 轴）→ Input `m_wheelDelta`（int，
  frameBegin 复位 = 输入态纪律，无输入恒 0 = 结构免疫根）+
  `handleMouseWheel` + `wheelDelta()` 只读
- 状态：`Game::m_thirdPersonDistance` session float（默认
  kThirdPersonDistance 4.0，C 切模式不重置，不存档——mode 同款
  「session state never saved」先例）
- 相机：`resolveThirdPersonCamera` 加 **distance 默认参数**
  （offset 缩放/raycast reach/embedded clamp 三处消费；**既有 16
  单测零改动位级不变**——默认参数 = pre-V2-19 调用逐位）；
  `thirdPersonOrbitCenter` 签名未动（几何上 distance 无关 =
  orbit height 0.4 条款的必然结果，定案偏差 1）
- 调节：仅 ThirdBack/ThirdFront（First 滚轮 no-op）：clamp
  [kZoomMin 2.0, kZoomMax 8.0] step kZoomStep 0.5（MC 原版无滚轮
  缩放 = mod 惯例区间，记档偏离）
- **aimReach 联动**：ThirdFront = m_thirdPersonDistance + kReach
  （拉近够近、拉远够远 = V2-1 前视约定的泛化；First/ThirdBack =
  kReach 位级不变）；**瞄准射线仍自眼位（UX-8g 红线零触碰）**
- F3 调试行 +`cam <Mode> D <x.x>` 读法

### ② 玩家飞行（"中国人会飞"——快于走路探索）
- 开关：**双击 Space**（kFlyDoubleTapSeconds 0.3 real-dt 窗，
  CRAFT-7 kDoubleClickSeconds 同款输入态——纯函数
  core/fly.hpp `flyToggleDecision`，Game 薄适配；第二击吞跳跃
  `suppressJumpGrant` 一帧旗；仅 locked pointer + 存活，面板开
  不响应；MC creative 同款手势）
- 物理（Player，mob 零触碰）：重力跳过；Space 按住升 /
  LeftShift 按住降 = kFlyVerticalSpeed 4.0 b/s（**潜行机制照常**
  ——Shift 双义 = MC creative 同款）；水平 = walkSpeed ×
  kFlySpeedMultiplier 1.6（= 6.88 b/s > 冲刺 5.6，**快于走路 ✓
  用户诉求**；不走冲刺分支，crouch/beacon 缩放照常）；碰撞照常
  （AABB moveInSubsteps）
- **坠落伤害**：飞行中每帧 `m_fallStartY = m_position.y` 归零
  （悬停零累计；切出帧自切换高度起算 = 无爆发伤害，其后正常计
  坠）；落地路径 +`!flying` 免伤门（飞行中落地 = 受控下降，
  creative 同款）
- **水/熔岩自动切出**（inWater/inLava → m_flying = false，速度
  计算前——同帧已游泳节奏；MC creative 水下可飞 = 记档偏离）
- 死亡/重生复位；**不存档**（会话态；载档后空中自然落下、坠距
  从 0 计无伤害——MC 存 flying 标志 = 记档偏离，省 format）
- 无输入运行：双击永不触发 = 结构免疫
- F3 调试行 +`flying` 标记

### ③ 24 小时 HUD 时钟
- 纯函数 `DayCycle::clockString(t)`（hpp 注释 + 单测钉）：
  `hour24 = fmod((t − kDawnStart)×24 + 6, 24)`（t∈[0,1) 时原始值
  ∈ [4.8, 28.8) 无负 wrap）——**ramp 边精确对齐**：t=0.05（晨边）
  06:00 / t=0.3（白天中心）12:00 / t=0.55（昏边）18:00 / t=0.8
  （夜中心）00:00；分钟 floor 两位补零
  - ulp 钉：`clockString(0.0)` = **"04:47"**（double 0.05 比精确
    0.05 大 2.8e-18，t=0 落在 48 分钟地板下 1 ulp 带——按实际
    位级行为钉单测 + 注释论证，定案偏差 3）
- HUD：renderUi 末尾右上角（kF3Margin 8px 范式，kText 白，
  fScale 纪律）**恒可见**（每 pointer/panel/death 态——MC 无 HUD
  时钟 = 记档新功能）；定案偏差 2：绘制在 renderUi 而非
  drawWorldOverlay（后者 = world-space 裂纹通道，screen-space
  文本不可在其约束下绘制）
- 字体：':'（0x3A）= **ASCII 行在列实证**（UV 钉 cell (2,7) +
  font.png 墨点扫描，零字体扩展）
- 600 s 天 = 1 游戏分钟 25 帧（帧锁定推进，无输入也走 = 像素帧
  确定性函数）

## 变更文件
- `src/core/input.{hpp,cpp}`（wheel 帧态 + SDL 事件消费）
- `src/core/third_person.{hpp,cpp}`（distance 默认参数 + zoom 常量）
- `src/core/fly.{hpp,cpp}`（新：双击窗纯函数）
- `src/player/player.{hpp,cpp}`（飞行物理/坠伤归零/免伤门/复位）
- `src/world/day_cycle.{hpp,cpp}`（clockString 纯函数）
- `src/core/game.hpp`（m_thirdPersonDistance/m_lastSpacePressTime
  session 态）/ `src/core/game_frame.cpp`（两处 resolve 传参）/
  `src/core/game_ui.cpp`（滚轮缩放 + F3 cam 行 + renderUi 时钟）/
  `src/core/game_interaction.cpp`（aimReach 联动 + 双击 toggle）/
  `src/ui/debug_overlay.{hpp,cpp}`（cam/flying 字段 + 行）
- `CMakeLists.txt`：2.10.0→2.11.0（单点）+ fly.cpp + test_fly.cpp
- 测试：test_fly.cpp（新 6）/ test_input（+2）/ test_third_person
  （+4 纯追加）/ test_player（+10 纯追加）/ test_day_cycle（+2）/
  test_font（+1 ':' 字形）/ test_debug_overlay（版本钉 + cam 行）

## 实现侧自证（编排者独立核证）
- 构建 0 警告；**2207/2207**（2181 + 26；编排者独立复跑）
- 既有 ThirdPersonTest 16 个 + PlayerTest 族**零改动全过**（默认
  参数位级不变红线 git diff 实证 = 两测试文件纯追加）
- 冒烟 banner v2.11.0
- base18 自证：新侧 ×2 逐字节恒等 + 与现锚 diff **119 px 全落
  右上角时钟区** bbox 114×18@(544,12) 零泄漏（时钟读 "10:50" =
  帧 60 定格值 t=0.25167 → hour 10.84 ✓ 手算吻合）

## 门 182（编排者执行；old 侧 = v2.10.0 = 3a5504e 树）— 全 PASS

### S1 = **base18 重锚论证**（Phase 18 base3→base18 先例第 3 次）
- 旧侧（v2.10.0）60f ×2 恒等（178148 B）且 **o1 ≡ 现锚逐字节**
  （环境干净，diff = 纯代码归因）
- 新侧 ×2 恒等；**新 vs 旧 diff = 609 px，bbox (544,12)-(657,29)
  = 仅右上角时钟区**，区外（x<475 或 y>120）零像素；savenorm
  EXACT（世界/存档恒等——三子项全会话态不存档）
- **新锚 = 178676 B 采用**（base18 第 3 次重锚）

### S2 = 三子项输入判别（retry 捕获协议，PITS 376）
- **zoom**：new ThirdBack C-only vs C+wheel×4 场景 diff =
  (0,0,679,484) 全视区（相机距离 4.0→6.0 视差）；**old 同输入
  diff = None**（v2.10.0 无缩放 = 互斥终态）
- **flight**：new 双击 Space + 2 s 按住 = 终档 **y 62.13**（升空，
  ≈4.0×2 s 上升 + 起跳）vs old 同输入 **y 54.00**（两次跳跃落地
  = 互斥终态）
- **clock**：= S1 重锚区 diff 本体 + clockString 边界钉（06:00/
  12:00/18:00/00:00/23:59/ulp 04:47）

### S3 套件
- **2207/2207**（编排者独立复跑）

### S6
- 本批零新数据解析面（三子项全代码 + 既有字体字形）——S6 =
  clockString/':' 字形/双击窗纯函数的命名单测钉（S3 内）+ F3
  cam 行读法实证（S2 zoom 判别的旁证）

### 性能 A/B PASS 同世界同成本（用户硬要求，发布前）
- v2.10.0（tm_v2110_perf = 3a5504e + g139 27 标记）vs v2.11.0
  （tm_v2110_perf_new = 终版工作树 + g139 双 27 标记，banner 双侧
  实证）：fall 900f 无输入 seed 123456789：
  - PERF-WIN 三窗全指标同档（upd med 0.37/0.56/0.58 vs
    0.62/0.59/0.38 ms；p95 窗间摆动无系统偏移；render med
    0.39/0.49/0.45 vs 0.55/0.53/0.43；swap 15.8/15.5/15.5 vs
    15.2/15.4/15.7 = vsync 主导同值；fluid 0.038/0.046/0.044 vs
    0.044/0.040/0.038、fires/spread/ents/stream 全同档——**时钟
    每帧 +1 文本 draw 成本不可测**；zoom/flight 无输入全惰性）
  - **PIPE3-S0 changesApplied 三窗 max 9/1/3 双侧逐位**
  - **终档 fluids 16/16 + blocks 36/36 集合恒等**（同世界最强形）

### 门协议发现（PITS 376）
- 输入 E2E 三坑：① 指针 capture 首击失手 ~50-75% = 重试环（至多
  6 次 + 'Pointer captured' DEBUG 级 grep【--log-level info 下
  captured=0 是日志假象】）② 输入跑必须**帧限收尾**（截图只在
  frame-limit 写；SIGTERM 早杀 = 缺截图 + kill $tp = 杀 timeout
  壳孤儿化游戏【PITS 374③ 同款】）③ pgrep/WID 轮询 + $p 空时
  xdotool 静默丢输入 = 假 PASS 源（断言含 captured 计数回显）

## 偏离（记档）
1. 滚轮缩放 = MC 原版无（mod 惯例 2.0-8.0/0.5 区间）
2. 时钟绘制在 renderUi（screen-space 通道；drawWorldOverlay =
   world-space 裂纹通道不可绘文本）
3. clockString(0.0) = "04:47"（ulp 带按实际位级钉）
4. 飞行水/熔岩自动切出（MC creative 水下可飞）
5. 飞行态不存档（MC 存 flying 标志）
6. 时钟 = MC 无 HUD 时钟（用户诉求新功能）
