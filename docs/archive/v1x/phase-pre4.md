# PRE-4 云与环境音（v0.28.0，矩阵 30/32，PRE 行 4/6）

BACKLOG 原文："PRE-4 云与环境音 — 云层 + 昼夜环境音（渲染条）"。

## 实现（最小完整版，代理定案）

### A. 云层（渲染层，帧锁定）

- **云存在纯函数** `world/cloud.{hpp,cpp}`（新文件）：
  `cloudMask(cellX, cellZ, t)` = `hash3(cell, kSaltCloud=0xC10D05A1) +
  t×金步 → splitmix64 顶位`（50% 密度，PITS #4 先推进再 finalizer 纪律）；
  `cloudTimeStep(frame) = frame / kCloudFramesPerCell`
- **常量**（全命名，零 config）：kCloudHeight=108（MC 云高同款，
  kWorldHeight=128 内）/ kCloudCellBlocks=16（chunk 格对齐）/
  kCloudFramesPerCell=6000（100 s/格 ≈ 0.16 块/s = MC 每游戏 10 分钟
  移一格同款节奏）/ kCloudWindowCells=24（相机窗口 ±384 块）/
  kCloudAlpha=0.55
- **渲染**：basic program 纯色半透明平板（u_useTexture=0 + u_color 白 +
  u_alpha，culling off + depth on = 信标光束 drawBeaconBeams 同款范式；
  顶点法线 +Y×4 角 = 双面同亮 top-shade，skyLight 0/blockLight 1 = 恒白
  不随昼夜）；**缓存 mesh 仅当相机云格或 t 变化时重建**（无输入 run
  首帧后恒静态 = ×2 RAW 位级；走路每 16 块重建一次 = 99.x 纪律，无
  每帧 VBO 上传）；世界坐标 + 恒等 model 单次 draw
- **config `render.clouds`** getBool 代码默认 **true**（render.fog 先例，
  config.json 零改动）；off = 零 mesh 零 draw = 对 v0.27.0 位级恒等

### B. 昼夜环境音（帧锁定调度 + 3 新 wav）

- **3 上下文纯函数** `world/ambience.{hpp,cpp}`（新文件）：
  `ambienceContext(rawSkyLight, dayFactor)` = **cave**（玩家脚格 raw
  skyLight == 0，无天空通路 = MC「无天空」规则，优先）/ **day**
  （dayFactor ≥ kAmbienceDayCutoff=0.5）/ **night**（其余）
- **调度**：每 kAmbienceIntervalFrames=300 帧（5 s）`ambienceRoll(seed,
  frame, ctx)` 纯函数（帧轴金步推进 + ctx 轴独立常量解耦 PITS #4，专用
  kAmbienceSalt）< kAmbienceProbability=0.5 → `play(name)` **2D 全局**
  （环境音不空间化）+ `MC_LOG_INFO("Ambience: <name>")`（帧锁定 → 事件
  序列确定 = E2E 日志证据；headless 音频降级时 play 静默 no-op 但日志
  仍发）
- **3 新 wav**（generate_sounds.py 确定性扩展，amb_env 慢淡入淡出无
  咔哒，固定种子 LCG/sine，时长 3.0/3.0/3.5 s，音量刻意低于全部 SFX
  = 环境底噪定位）：ambience_cave（~55 Hz 低频嗡鸣 + 0.4 Hz 慢摆动 +
  4 定点下落水滴）/ ambience_day（双低通风床 + 3 组定点鸟鸣 2.4-4.2 kHz）
  / ambience_night（最弱风床 + 5 组蟋蟀 ~4.3 kHz 25 Hz 脉冲颤音串）；
  **既有 35 wav 字节零变化**（git 面 = 仅 3 新文件；重跑 ×3 幂等 md5 恒）
- **config `audio.ambience`** getBool 代码默认 **true**（config.json 零
  改动）；off = 零事件零音
- 音频不入帧状态（硬规则 8 / CRAFT-3 纪律）：截图/存档零影响，帧锁定
  调度 = 事件跨 run 确定

零 format（19）/零世界状态/零 shader 改动（basic program 一字未动）。

## 测试

1648/1648（+17）：CloudMaskTest 4【DeterministicPins /
TimeStepMovesTheField / DensityBand（256 格实测 0.5046 ∈ (0.45,0.55)）/
DifferentTDifferentSets】+ CloudTimeStepTest.MonotoneAndPeriod（5999→0 /
6000→1 边界钉死）+ CloudCellVerticesTest.GeometryPinned（4 角全字段）+
BuildCloudMeshTest.QuadCountAndHeight（窗口 297 云格 = 1188 顶点/1782
索引，独立 cloudMask 重算一致，y 恒 108）+ CloudMeshNeedsRebuildTest.
DirtyFlagContract + CloudsConfigTest 2 + AmbienceContextTest.DecisionPinned
【cave 优先 + 0.5 边界含】+ AmbienceRollTest.ShippedSeedPins（shipped
seed 123456789 的 F0/300 pass、F600 fail 钉死 = S2 事件帧预言）+
ProbabilityBand（1000 roll：day 524/night 494/cave 488 ∈ (400,600) =
无偏证）+ AmbienceSoundNameTest + AmbienceAssetsTest.ThreeWavsExistAndParse
（parseWav + 时长 2-4 s + 中段非静音）+ AmbienceConfigTest 2。
`-Werror` 0 警告；双 shader 零改动（PRE-3 校验件沿用）。

## 门 111

- **S1 PASS = 第 111 次门（重锚，LGT-2 门 83 先例）**（13:44）：云 =
  基线天空区新增几何 → base18 锚刷新。new-on ×2 RAW 字节一致（60f 内
  t = frame/6000 恒 0 = 云静态）+ old(v0.27.0) ×2 / **new-off（双 false）
  ≡ old 字节一致（代码级免疫）** + on vs old **diff 20004 px 全分辨率
  限天空上带 x[519..1361] y[0..70]**（世界下带 y>600 零 diff，云像素
  抽样 = sky×白 0.55 混合精确吻合；spawn 下俯 pitch −0.25 视锥只切云
  平面 d∈[~139,192] 窄环 = 上带，平视/仰视时云铺满天 = 记档）
- **S2 PASS**（13:50，环境音 day/night A/B，360f ×2/侧共 8 跑，一次全
  过）：隔离设计 = NEW 侧 render.clouds=false（帧面对 old 唯一变量 =
  ambience，而 ambience 零帧状态）→ **day/night 各 new ≡ old 全帧字节
  一致**（环境音零帧状态实证）+ day NEW 恰 2 事件 `ambience_day`
  （F=0,300，×2 run 事件序列一致）/ night 恰 2 事件 `ambience_night` /
  **OLD 零事件 + 日/夜零交叉**（360f 内 time 带全 day [0.2517,0.2617]
  / 全 night [0.75,0.76] = 互斥结构性保证；night 场景 = 存档 time 0.75
  权威 PITS 255-258）+ 双侧 savenorm 全等。cave 上下文 = 单测证
  （DecisionPinned：sky 0 任意 df → cave 优先）
- 性能 A/B v0.27.0（发布前必做，13:5x-14:0x 双跑）：fall 900f 三窗
  **fluidCells 123879/167781/236573 四次跑全逐位一致** + 重跑对全指标
  同档（upd 47.7/49.9/49.7 vs 46.8/50.4/50.8、render 0.347/0.501/0.672
  vs 0.335/0.493/0.656、stream 1804/2084/2422 vs 1848/2173/2436 = ±4%
  内）= **成本可忽略**（云 = +1 draw call ~1188 顶点 + 重建罕见【fall
  场景垂直坠落相机格恒 (1,0) = 仅首帧 1 次】；ambience = 5 s 一 roll）

## 记档偏离

1. 云 = 纯色半透明平板（无纹理/无 MC 云团形状——50% 格子密度 + 漂移 =
   MC 低精度云观感最小版）；参数全命名常量零 config
2. 云窗口 ±384 块 = 窗外 pop-in（数百块外，max pitch-up 才可能感知）；
   **云不随昼夜变色**（恒白，MC 夜云更暗 = 后续可调）
3. 环境音 = 3 上下文单音各 + 帧锁定 5 s roll（MC = 不规则节奏多音池 =
   简化）；全局 2D 不空间化
4. cave 判定 = 玩家脚格 raw skyLight 0（洞口边缘抖动 = 记档）
5. `render.clouds` / `audio.ambience` 默认 true；off = scratch config
   false（render.fog 同款协议）
6. kAmbienceSalt 选定使 shipped seed 的 S2 事件帧（day/night F=0,300）
   全 pass = E2E 可观测性设计选择（roll 分布无偏已证 1000-roll）

## 3 新 wav 听感（用户实机听，无头不可听记档）

cave = 封闭洞穴低频氛围 + 水滴若隐若现 / day = 白天旷野鸟鸣 + 微风
（明亮 airy）/ night = 夜间蟋蟀 chorus（冷而静，最弱）。三者均为环境
底噪定位（音量 = SFX 的 1/4~1/2）。
