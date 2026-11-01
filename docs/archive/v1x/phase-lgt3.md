# LGT-3 夜光重传播（v1.3.0，门 118）

v1.x 任务矩阵第三行（docs/BACKLOG.md）。出处 = KNOWN_ISSUES Phase 8 条
「昼夜是呈现层缩放，不是夜间重算光照」："Minecraft 会在夜间真正重传播
光照（例如日落时光线从洞口退出的动态变化）；本实现夜晚的天空光区域是
均匀变暗，无动态传播变化"。

## 核心定案：天空光场对源等级仿射（无损）

LGT-2 成本模型下所有天空光源共享同一源等级 S（rescan 顶灌 + sky BFS
种子同值），逐格成本（enterCost：vertical+sky+零 opacity 免费、其余
max(1,opacity)、流体 kFluidLightAttenuation）可加且与 S 无关 →
收敛场 `field_S(x) = max(0, S − minCost(x))`。正午存储场
`stored = field_15 = max(0, 15 − minCost)`。故：
- stored = v > 0 ⇔ minCost = 15−v（精确）→ `field_S = max(0, v − (15−S))`
- stored = 0 ⇔ minCost ≥ 15 ≥ S → `field_S = 0`
**= `field_S(x) = max(0, stored(x) − Δ)` 逐格精确无损，Δ = 15−S。**
confluence 单测（源等级 S 直接重传播 vs 仿射变换逐格相等）钉死。

**形态 = MC 同款日落撤退/黎明回涌**：Δ 随昼夜增大/减小，亮区沿
`stored = Δ` 等值线向洞口/冠隙收缩/回涌——**零重传播**（shader 逐像素
仿射变换，光场数据/管线/workflow 零改动）。

**MC 月光地板（用户实机定案 10-27 夜）**：初版深夜 S=0 全黑无法夜视
（"玩家什么也看不到，根本无法玩"）→ 定案 `kNightSkyLevel = 7`（MC
午夜露天月光 7/15）：**S = 7 + 8×dayFactor，Δ = 8(1−df)**；配套**敌对
spawn_max_light 0→7**（MC 配对：night open 7≤7 保夜险 / torch 14>7
免疫 / day 15>7 不生成 = 玩法闭环；passive 15 不动）。

## 实现（fixer 交付 ×2 轮，编排者逐 hunk 核证毕）

- `lighting.{hpp,cpp}`：`kNightSkyLevel = 7` + `kNightRetreatScale = 8/15`
  单一定义；`effectiveSkyLight`（整数，Δ=floor(8(1−df))，spawn/存档语义）
  + `effectiveSkyLightF`（归一化 float，shader 镜像源）；`propagateAll`
  加 `skySourceLevel = kMaxLight` 默认参 = confluence seam（生产恒默认
  零行为变化，assert 契约 [0,15]）；rescanColumn sourceLevel 参。
- `basic.frag`：天空项一行 `v_skyLight * u_dayFactor` →
  `max(v_skyLight − kNightRetreat×(1.0−u_dayFactor), v_blockLight)`
  （GLSL `kNightRetreat = 8.0/15.0` 命名常量回指 C++ 单一定义；C++/GLSL
  同舍入核过）；block/雾/波纹/gl_Position/depth/ambient 零重排
  （PRE-2 红线）。三处 u_dayFactor 绑定点核对：世界/掉落/手持/箭/图标
  全 sky 消费者一致；信标光束/云 = blockLight 1.0 恒白结构性免伤。
- `mob_spawning.cpp`：`currentSpawnLight` = effectiveSkyLight 镜像
  （生成光门 = 所见光 = MC 语义；df=1 与旧同值，df=0 = 月光 7 = 特性）。
- 内容：zombie/skeleton/spider spawn_max_light 0→7（JSON 值变更，
  零解析面）。
- 注释/文档同步：day_cycle.hpp 头注 / item_light.hpp / handIconMesh /
  MECHANICS 光照门+昼夜节 / KNOWN_ISSUES Phase 8（已解决注）+ Phase 10
  （sky=1.0 消费者 = 月光亮非全黑）。

**规格偏离（2 处，均核证接受）**：① 编排者规格 shader 字面式
`15.0*(1.0−u_dayFactor)` 量纲错误（v_skyLight = 0..1 归一化，mesh 烘焙
light/15 实证）→ fixer 纠正为归一化形态 `kNightRetreat×(1−df)`，level
空间核心公式不变；② 首轮"无月光地板"记档偏离被用户实机反馈推翻 →
kNightSkyLevel 7 + 阈值 7 配对。

## 浮点边界（编排者穷举验证）
烘焙 sky=1.0 消费者（mob/箭/手持图标/满天空掉落）：新式
fl(1−fl(1−kNightRetreat·(1−df)))——df=1 恒等、df=0 = 7/15 精确
（1−fl(8/15) 可表示性验证）；中间 df ≤1 ulp ≈ 8e-6/255 亚像素
（KNOWN_ISSUES 记档）。

## 测试（1704/1704 = 1695 + 9，-Werror 0 警告，×3 轮稳）
- confluence 主测 `AffineTransformMatchesDirectPropagation`（同 fixture
  源等级 S∈{7,10,0} 直接重传播 vs 仿射逐格相等 + fixture sanity 12 钉
  含玻璃底水平路由胜 rescan / 水列成本活边界）。
- `StoredZeroIsLossless`（两 chunk 隧道 cost 14/15 精确边界；S 全档
  stored-0 恒 0 + 撤退轮廓钉）。
- `EffectiveSkyLightIntegerPins`（df{0,.25,.5,.75,1}×stored{0,7,15}
  15 值；15@df0 = 7 = 月光点名）。
- `MidnightNoonIdentity`（正午恒等 + 旧式自证；深夜 = stored−8 钳 0；
  F 版归一化钉 1.0@0 → 7/15 精确）。
- spawn：`SpawnLightMatchesEffectiveSkyModel`（月光 7 / ramp 11·9·13 /
  火把 14 > 月光）+ `NightMoonlightBlocksZeroThresholdMob`（语义翻转
  改名）+ `NightMoonlightAllowsShippedHostileThreshold` /
  `TorchCircleBlocksHostileAtNight` / `DayBlocksShippedHostileThreshold`
  （三态玩法闭环）+ `MobContent.Lgt3MoonlightThresholdPinned`（真实
  资产 trio=7 / passive=15 钉死）。
- 既有 1695 零改动全绿（光照/spawn/流体/存档回归网）。

## E2E 门 118（全 PASS）
- **S1 PASS = 第 118 次结构性免疫免重锚**：fresh 60f 四方（NEW×2/
  OLD v1.2.0×2）RAW 字节一致（387726 B）+ base18 锚（9c64b0f8）连续 +
  savenorm EXACT（正午 df≡1.0 恒等点 = Δ≡0）。
- **S2① 深夜 0.75 A/B PASS**（mobs off 隔离 spawn 阈值变更，600f）：
  NEW×2 RAW 一致 + A/B diff **828,043 px 全部仅亮化**（darker=0，
  月光地板单调性 max(stored−8, block) ≥ max(0, block) 实证）+ 位置化
  open-sky 锚盒（681,100 邻域，蓝天 clear 零触碰）+ 存档 savenorm
  EXACT（RAW 差 = 仅 fires[].remaining real-dt，PITS 244 类）。
- **S2② 黄昏 0.50 A/B PASS**：NEW×2 + OLD×2 RAW 一致 + A/B diff
  828,043 px（本视图无 stored<8 深荫像素 = 亦全亮化；diff 像素集与深夜
  场景**同数** = 时间不变集，特征一致性侧证）+ 锚盒零 diff + 存档
  savenorm EXACT。
- **S3**：1704/1704（+9：confluence 主测 / stored-0 无损 / 整数钉 /
  恒等 / spawn 镜像 + 月光 4）。
- **S6**：无（spawn_max_light 0→7 = 既有字段值变更，零解析面）。
- **性能 A/B（收）**（用户硬要求）：fall 900f 三窗 v1.3.0 vs v1.2.0
  基线：fluid 段 616.6/1233.1/1859.9 vs 631.3/1267.1/1880.2 ms
  （−1..−3% 同档）+ upd/render/swap/stream 全同档 + **fluidCells
  123879/167782/236575 三窗逐位一致** + PIPE3-S0 流体 tick 计数与基线
  逐项同（tickMs 11.7/26.3 vs 12.6/24.9 = 噪声带）= 零成本变更实证
  （shader +1 乘加、spawn +1 函数调用）。
  注：首轮 win3 被并行仓库构建污染（upd 60ms / draws 计数溢出）弃用，
  干净重跑收（性能跑期间零并行 CPU 纪律）。

## 偏离/记档
- 分数 Δ 平滑撤退（MC 量化超集）。
- 月光地板 + 阈值 7 = 用户实机定案（MC 精确值）。
- 深夜 sky=1.0 烘焙消费者中间 df ≤1 ulp 亚像素差（穷举验证 + 记档）。
- 敌对夜间浅暗格（block 光 1-7）生成密度微调 = 阈值 7 的定案副作用
  （shipped 无 1-7 档光源块，实际零影响；记档）。

## 坑位（PITS 284-285 追加）
- **284** 门像素带断言 = 任意像素行 ≠ 语义边界（树冠剪影越带 + 热栏
  块立方图标按设计跟世界光 + 雾面山体；修正 = 隔离论证构造化 +
  位置化 open-sky 锚盒 + 方向断言先行数学）。
- **285** scratch 树无 build + 驱动指 $D/build = 启动即死无告警（门
  驱动二进制 = 冻结 bin 路径与资产树解耦；zsh f-string 拼版本号
  tm_v132_bin 式幻觉 → 完整字面量）。
