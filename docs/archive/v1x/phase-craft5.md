# CRAFT-5 炉子表现（v1.x 矩阵 23/33，v1.23.0，门 138，CRAFT 行 2/4）

## 定案

原始定义已佚 = 定案 **炉子的两个 MC 表现提示**（消费 KNOWN_ISSUES
Phase-40 条「无熔炼完成音效、无火焰动画、无经验——v1 的视觉提示只有
面板里的进度条」）：

- **炉口火焰粒子**：燃烧中炉子（`fuelSubsteps > 0`）每
  `kFurnaceParticleEveryNFrames = 36` 帧（0.6 s）从炉块**上方一格**发射
  一批上升余烬（**P66 `spawnFire` 原样复用**：2 粒、负重力、0.3-0.5 s
  寿命、火纹理）；种子 = 世界种子 ⊕ 位置哈希（**专用盐
  `kFurnaceParticleRollSalt`**——与火余烬流永不互追）⊕ 帧金步推进
  （P66 纪律）；运动 = 真实 dt（粒子类，硬规则 8 的既有例外条款）。
  **周期 36 的选择 = 寿命 2× 之外还留裕量**：初版 30 帧在 60 fps 下
  恰等于 0.5 s 寿命边界 → 一次 on×2 同侧 RAW 被前一批 125 px 残留余烬
  打破（差值 100% 在火焰簇内、世界零差 = 实证）→ 36 帧（0.6 s @60fps >
  0.5 s 寿命上限）= 任意 ≤60 fps 前批必死。**发射门 =
  `(m_frameCount + 1) % 36`**（帧计数器在 update/render **之后**自增：
  900 帧跑末帧见 m_frameCount=899、900%36=0 → 截图恒携 age-0 确定性
  批次 = ×2 RAW 位级可比，比 P66 更强的证据形）；**粒子光读上方空气格**
  （炉格本身 opaque 且无 light_emission = 读炉格烘出黑粒子，首版实证
  on≡off 像素 diff 0）。`render.furnace_flames` getBool 默认 true
  （render.fog 先例，config.json 零改动），**off = 零发射 = 对 v1.22.0
  位级恒等**（代码级免疫）。
- **熔炼完成音效 "smelt"**：新 wav（generate_sounds.py 确定性扩展：
  1320/660 Hz 双泛音钟 + LCG 攻击 tick，专用种子 0x51DE；**既有 38 wav
  字节零变化 md5 实证 + 重跑幂等**）+ 完成事件帧在炉块中心**空间化播
  放**（CRAFT-3 `play(name, origin)` 范式）+ info 日志 `Furnace smelt
  complete: <product> at (x, y, z)`（帧锁定事件 = E2E 证据，headless
  音频降级 play 静默但日志发 = PRE-4 Ambience 先例）。
- **复燃"咔"声**：燃料 0→燃烧转移帧复用既有 "ignite" wav（P60 燧石声）
  + 日志 `Furnace lit at (x, y, z)`（MC 同款点火声）。
- **事件检测 = 纯函数** `furnaceTickEvents(before, after)`
  （furnace.{hpp,cpp}，campfire 范式）：tick 本身保持无副作用（音效/日志
  归 game 循环，音频不入帧状态 = 硬规则 8）；`lit` = fuelSubsteps 0→>0
  （燃尽后复燃 = 同款转移）；`smeltCompleted` = 输出恰 +1 且同产物
  （空→×1 或 ×N→×(N+1)；快照在 tick 前同帧取 → UI 变更不可能伪装）。
- **无经验**：项目无 XP 系统（矩阵外，记档偏离——原文三条消费两条半）。

**结构惰性**：furnace 块永不 worldgen（biomes 零引用）→ fresh 世界
`m_furnaces` 恒空 → 事件循环零状态 + 火焰 pass 首行 fast-exit + 音效
零事件 = S1 免疫根。

## 实现

- `src/item/furnace.{hpp,cpp}`：`FurnaceTickEvents` + `furnaceTickEvents`
  纯函数。
- `src/core/game.{hpp,cpp}`：`updateFurnaceFlames()` pass（30 帧周期、
  专用盐、spawnFire 复用、`render.furnace_flames` 门）+ 炉子 tick 循环
  加 before/after 快照 → 事件 → 空间化音效 + 日志 + 常量 2 + config 读取
  1。
- `tools/generate_sounds.py` + `assets/sounds/smelt.wav`：新音效。
- 零 format（21）/config.json/内容/世界状态。

## 测试

1847/1847（+3：`FurnaceEventsTest.LitOnFuelIgnition`【点火/持续/燃尽
三态】/ `SmeltCompletedOnOutputGain`【空→1/同产物+1/无变化/异产物四例】/
`EventsTrackTheRealTick`【真 tick 600 子步端到端：首步 lit 非完成、第
599 步恰完成、燃料/输入/产物终态】），`-Werror` 0 警告。

## 门 138

- **S1 = 第 138 次结构性免疫免重锚**（fresh 60f 四方 ×2 **168140 B**
  全 EXACT + base18【679×744 锚连续 = PITS 315 态未变】+ savenorm 同
  侧/跨侧；furnace 永不生成 = m_furnaces 空双结构惰性）。
- **S2 = 炉子场景无输入跑**：scene（表 (21,39,-4) + 玩家 (21.5,39,-2.5)
  pitch −0.2；炉子预置 beef ×1 + progress 580/600 + fuel_substeps 3000
  = 燃烧中、frame 20 熔炼完成、frame 900 仍燃）900f 六跑：
  on×2 **savenorm EXACT** + RAW 差 4232 px **100% 含于火焰簇**（age-0
  批的亚帧位置 = 真实 dt 一帧位移，fps 依赖 = P66 活粒子类【P54 条款】，
  簇外世界逐像素恒等 = 状态像素级确定性证）+ off×2 RAW **235855 B
  EXACT** + **off ≡ old（v1.22.0）RAW 字节一致 = 代码级免疫** +
  on≡off savenorm EXACT（火焰不入档纯渲染证）+ off=old savenorm 仅
  熔岩湖 churn（blocks 集合恒等 + fluids 5 格流前 @ ~100 块视图外 =
  PITS 244/283 类）+ **on vs off 像素断言 PASS**（diff **4417 px**，
  bbox 130×150 紧凑簇、上中画面、不触热栏带）+ 日志 = new 侧恰 1 行
  smelt（frame 20 帧锁定）/ old 侧 0 行 + 存档终态六跑全部 output
  cooked_beef ×1 / fuel_substeps 2100 / progress 0 exact。
- **S3 = 套件 1847**。
- **无 S6**：零 parse 面（config bool = fire.particles 先例；smelt.wav =
  资产非数据引用，SoundBank 未知名/缺失 = 静默 no-op 既有语义）。
- **性能 A/B 必做 = PASS**（真 v1.22.0 tm_v122_perf vs v1.23.0-dev
  tm_v123_perf，banner 实证，fall 900f 三窗）：fluidCells **win1
  51927 = 51927 逐位** + win2/3 29703/29803 + 29700/29800（±100 = rim
  簇 churn 类，gate 131 先例）+ PIPE3 **changesApplied 1404 = 1404
  逐位** + 全指标同档 = 成本可忽略（结构惰性：fall 场景零炉子 = 火焰
  pass fast-exit、事件快照零执行）。

## PITS

- **PITS 316（性能树 step0 patch hunk 锚定 update() 区 + 黑窗 PNG 随位置变
  + 火焰 age-0 设计两坑）**：① CRAFT-5 在 `updateFires`/`updateFireSpread`
  间插入新 pass → step0 hunk #7 上下文失配 FAILED（#6 恒 FAIL 已知）→
  性能树 -Werror 死（set-but-unused）——性能树构建须验 marker 计数 15 +
  构建成功；新 pass 放测量块外保持新旧树数字可比。② 黑窗 PNG 字节随窗口
  位置变（+0+60 = 19817 B / +0+22 = 12037 B 纯黑）——sz 门用大小阈值
  （>100000）非字节恒等。③ 帧计数器 update/render 后自增 → `--frames N`
  末帧见 N−1：age-0 截图设计须门 `(m_frameCount+1) % period`；粒子光读
  炉格（opaque 无 emission）= 黑粒子 diff 0——读上方空气格；**period 取
  寿命 2× 会撞 60 fps 边界**（30 帧 = 0.5 s = 寿命上限 → 同侧 RAW 被
  125 px 残留余烬破，差值全在火焰簇内世界零差）——取 36 帧留裕量。
