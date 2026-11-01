# ENT-7 移动惯性（v1.x 14/33，v1.14.0，门 129，ENT 行 3/6）

v1.x 任务矩阵第十四项。"移动惯性"原始定义已佚 = 代理定案最小完整版：
**玩家地面/空中移动惯性**（MC 式加速/摩擦——现状 = 有 wish 瞬置
`v = dir×speed`、无 wish 瞬清零 = 起步无加速、松键零滑行，手感最
"瞬移"的子系统；mob 侧 AI 每帧设精确速度 = 既有设计，范围外记档）。

## 定案

- **指数趋近模型（fps 无关，real dt）**：有 wish 时
  `v += (dir×S − v)×(1 − e^{−k dt})`（地面 k=kGroundAccelRate 8.0 =
  敏捷起步【~0.4 s 达 95%】/ 空中 k=kAirSteerRate 2.0 = 弱转向）；
  无 wish 时 `v ×= e^{−k dt}`（地面 k=kGroundDecelRate 12.0 = MC
  0.5448/tick 摩擦档【walk 4.3 松键滑行 ~0.36 块】/ 空中
  k=kAirDecelRate 1.9 = MC 0.91/tick 拖曳档【松键滑翔 ~2.3 块】）；
  速度 < kVelocityStopEps(0.001) → **恰好 0**（纯衰减永不达 0 = 静止态
  确定性红线，存档字节约定）。knockback 衰减 std::exp 先例（entity.cpp）。
- **FLUID-2 档保持瞬置**（水/水面/熔岩的 fluid 集成是独立调参模型——
  偏离记档）；sitting/crouch edge guard/子步物理/stepUp 零改动；
  beacon speed 乘子（Phase 84）作用于目标速度 = 趋近目标随之缩放（免费
  继承）；跳跃冲量（vy）零改动。
- 零 format/config/内容/音效。

## 实现

`src/player/player.{hpp,cpp}`：5 命名常量 + wish 速度块重构（~40 行，
inFluidTier 门控分流）。

## 测试（1802/1802 = 1798 + 4 新，-Werror 0 警告）

PlayerInertiaTest：GroundReleaseSlidesThenRests（walk 满速 → 松键 →
滑行 ≈ 4.3/12 = 0.358 解析带 + velocity 恰好 0 + 非瞬停【> 0.2】）/
AirReleaseKeepsGliding（跳跃携水平速度，空中松键 0.267 s 后 vz > 2.0
【地面档同窗会衰到 < 0.4 = 档位互斥】）/ FluidTierKeepsTheInstantSet
（1 深水 walk 10f 即达 swim 档速度 = 偏离钉死）/ StopEpsilonRestsExactly
（衰减到恰好 0 + 静止位不漂移）。
**3 契约演进重钉**（固定 dt 确定性新值 + 注释）：SlidesAlongWall（斜向
滑行 2 s 减起步 ramp：z 4.3115）/ EdgeBlockNoFloat（0.5 s 地面程减
ramp：1.6049）/ SprintIsFasterThanWalk（1 s walk 3.7977 / sprint
4.9458，冲刺优势全秒保持）。

## E2E 门 129

**S1 = 第 129 次结构性免疫免重锚**：fresh 60f 四方截图 ×2 387726 B
全字节一致（new v1.14.0 ≡ old v1.13.0 ≡ base18）+ savenorm new×2 /
old×2 EXACT（无输入 = 无 wish = 速度恒 0 = 衰减分支 no-op 位级恒等）。

**S2 = 空中滑翔判别输入跑**（walk-off-edge air glide；场景 s129_glide：
平台 x22-33 顶 y45 + 深坑 x34-39 底 y37【8 格坠落 = 5 伤 hp 15】，
玩家 x31.5 yaw −π/2 朝 +X；首轮 ground-slide 判别废弃 = 松键时序
real-dt 抖动 ±0.22 块与 0.358 滑行同量级不可判别 → 改空中滑翔：
cross 由几何量主导【~1.3 块】、抖动二阶【释放时刻 cross 项近似
抵消】）：hold W 0.8 s（锚定捕获 +0.2）→ 玩家 ~0.77 s 出檐、
~0.22 s 空中松键、坠 8 入坑：
- old v1.13.0：无 wish 瞬清零 → 空中 vz = 0 → 直落【landing x ≈
  释放点】；
- new v1.14.0：弱空中衰减 kAirDecelRate 1.9 → 滑翔
  4.3×(1−e^{−1.9Δ})/1.9 ≈ 1.3-1.6 块【landing x 更 +X】；
- 断言：四跑皆 **y37.0 exact**（落坑底 = 8 格坠落双侧共享实证）+ z 0.5
  恒（无漂移）+ 落坑 x∈(34,39.5) + **per-pair cross (new−old) ∈
  (0.8,1.8)** ×2 + within-pair |Δx| < 0.6。
  （hp 断言移除 = PITS 303：1500f ≈ 46 s 墙钟 > 自然再生窗，坠落 5 伤
  终态被回血到 20——P72"长跑损伤终态"教训复现；y37 落面即坠落证明。）

**S3 = 放置零回归输入跑**（无移动键 = 惯性结构惰性）：s129_place 场景
（同平台 slot0 stone、玩家 (27.5,45,−2) yaw0 pitch−0.35）、单次右键
放置：new ×2 RAW EXACT 381031 B + old ×2 RAW EXACT 381031 B + **new_a
≡ old_a 字节一致**（玩家恒静止 = 速度恒 0 = 衰减分支 no-op）+
savenorm EXACT + 玩家位恒 + 放置块 (27,45,−7) 在档。

**S4 = 套件 1802/1802**（1.14.0 binary 终跑）。

## 性能 A/B（发布前必做）

tm_v113_perf（v1.13.0 插桩）vs tm_v114_perf（v1.14.0-dev 插桩【v113
插桩 fluid.cpp 移植 + step0 game hunks + hunk#6 honey 区手工补，同
先例】），同 seed（perf_ab17 剥 fluids → gperf_ab22.json）fall 900f
三窗：无输入 = 玩家无 wish = 速度恒 0 = 惯性代码结构惰性 → fluidCells
应逐位一致 + 全指标同档。[待填]

## 偏离（范围外记档）

- **FLUID-2 档（水/水面/熔岩）保持瞬置** = 流体模型独立调参，惯性不
  入流体（单测 FluidTierKeepsTheInstantSet 钉死）。
- **mob 侧无惯性**（AI 每帧设精确数据速度 + knockback 衰减既有 = 项目
  设计；给 mob 加速度会改追逐停点/攻击时序 = 另行立项）。
- 空中转向弱（k 2.0）但无 MC 的空中摩擦地面混合细节（onGround 两档
  二值切换 = 简化，1 帧滞后 = 确定性可接受）。
- 起步 ramp 使固定帧数的行走距离略短（3 单测重钉 = 契约演进；E2E
  行走场景的位移断言按 ramp 修正带）。

## 坑位（PITS 追加）

- **PITS 303（输入跑墙钟时长 > 自然再生窗 → 损伤终态被回血）**：
  ENT-7 S2 首版断言 hp 15（8 格坠落 = 5 伤）——1500f 跑 ≈ 46 s 墙钟，
  玩家再生窗（饥饿 tick 36 s + 回血节奏）在落地后 ~10-15 s 内把 5 伤
  回满 → 四跑全 hp 20。修 = 弃 hp 断言，改用 y37.0 exact 落面证坠落
  （P72"自然回血污染长跑损伤终态：run 时长须 < regen_delay 窗"的
  输入跑版复现）。
- **S2 首版 ground-slide 判别废弃（松键时序抖动与滑行同量级）**：
  地面松键滑行 0.358 块 < 独立 real-dt hold 抖动 ±0.22 块 → per-pair
  cross 不可判别（首轮实测 cross 负值 −0.18 实证）。改空中滑翔：
  cross 由几何量主导（~1.3 块）、释放时刻抖动在 cross 项近似抵消。
  教训 = 输入跑判别量须 ≫ 输入抖动量（≥3×）。
