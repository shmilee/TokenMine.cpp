# FLUID-6 流体种类扩展（v1.x 11/33，v1.11.0，门 126，FLUID 行 3/3 收尾）

v1.x 任务矩阵第十一项。"流体种类扩展"原始定义已佚 = 代理定案最小完整版：
**第三流体种类 = honey（MC 1.14+ 蜂蜜）**：1/3 速高粘度（帧锁定 tick
门控）、不参与水-熔岩反应（数据驱动：honey 块无 water_reaction）、可
桶舀/倒、无波纹（PRE-3 水专有）。

## 定案

- **编码三层化**（chunk.hpp）：FluidKind{Water=1, Lava=2, **Honey=3**}；
  编码带 0 / 水 1..8 / 熔岩 9..16 / **蜜 17..24**（uint8 够宽）；
  fluidCode/fluidLevelOf/fluidKindOf 纯算术三层泛化（水/熔岩带位级
  不变）；fluidKindName/ForName 词汇表 +honey（存档 fluids 段种类名
  直接生效 = 零 format）。
- **粘度 = tick 门控**（不是改物理参数）：`FluidSim::tick(...,
  honeyActive)` 第 4 参；Game 帧锁定门 `(m_frameCount /
  kFluidTickEveryNFrames) % kHoneyTickPeriodFluidTicks(=3) == 0` = 蜜
  每 3 个流体 tick 模拟一次（下落与扩散同 1/3 速，MC 高粘度的单门控
  记档简化）；水/熔岩路径零改动（门只控第三段 simulateType(Honey)）。
- **per-kind 容器 3→4**（chunk m_fluidCells/m_fluidHot/m_fluidWalkPending
  + world m_fluidChunks/m_fluidActive）——**坑 PITS 299**：容器以 kind
  值（1 基）为下标，尺寸须 = 种类数+1（哨兵槽 0）；3 种类时尺寸 3 =
  静默 OOB 段错（下游测试才炸，非新增代码行）。
- **CellReader (block,水,熔岩) → (block,水,熔岩,蜜) 4 元组** +
  kindLevel/otherLevel lambda 泛化（"他种" = 任一非自身种类 = max）；
  "他种类 = 阻挡"语义既有泛化 → 蜜/水/熔岩互不替换、各自把他种当支撑。
- **mesher 两种类派发 → 三种类**（chunk_mesher waterId/lavaId/
  honeyId；蜜顶面无波纹标记 = emitFluid 既有 kind==Water 门控天然
  排除）。
- **光照零代码**：衰减 = 全体种类同一 kFluidLightAttenuation 常量
  （注释 either→any）；发光 = fluid 块 light_emission 数据驱动
  （honey.json 无字段 = 不发光）。
- **内容**：honey.json 块（fluid:"honey"、非 solid/opaque、transparent、
  destroyable false、无 light_emission、无 water_reaction = 反应 pass
  天然惰性）+ atlas 空位 [15,1]（免重锚）+ honey 纹理（generate_block_
  textures.py honey()，琥珀光泽 15% 高光条，既有 PNG 字节零变化）+
  honey_bucket 物品（pours_fluid honey / pours_becomes bucket）+
  bucket.json collects_fluid +honey + **隐藏 starter 槽 16**（GEN-2
  范式；storage 范围 [9,26]——槽 27 越界被 parseStarterSlots 点名 =
  勘察发现）。honey 无 worldgen（桶源 = MC 无蜂箱时的记档偏离）。
- **桶交互继承**（FLUID-4/5 零改动）：蜜桶对空气格 = 放置（(a)）/
  对蜜流动格 = 变源（(b)）/ 蜜桶对水格或水桶对蜜格 = 替换倒（(c)
  泛化 else 分支）/ 水桶对熔岩格 = 反应倒（(c1) 特例不变）。

## 实现

src：world/chunk.hpp（编码三层 + 容器 4）、world/fluid.hpp/.cpp（tick
门控参 + CellReader 4 元组 + lambda + 注释）、world/world.cpp（2 处
per-kind 循环 +Honey）、world/chunk_mesher.cpp（3 种类派发）、
core/game.cpp（粘度门常量 + tick 调用）。内容：assets 4 JSON + 2 纹理
脚本 + 2 新 PNG。

## 测试（1774/1774 = 1766 + 8，-Werror 0 警告）

FluidTest +7：编码带位级（水/熔岩不变 + 蜜 17..24 往返）/ 门控关时
悬空蜜冻结 / 每门控 tick 落一格 / 每门控 tick 扩一环 / 蜜水 6-邻共存
互不替换（各自向干格扩 1 环即停）/ 蜜在他种上得支撑不坠 / 门控不影响
水（gate-off tick 水照扩）。ChunkMesherTest +1：蜜顶面无波纹标记
（24 顶点全 water==0）。契约演进重钉：ItemRegistryTest starter
10→11（+honey_bucket 槽 16）、桶对测试 +honey 派生伴侣、存档 fluids
往返 +honey 条目、streaming-gate per-kind 循环 +Honey。

## E2E 门 126

**S1 = 第 126 次结构性免疫免重锚**：fresh 60f 四方截图 ×2 387726 B
全字节一致（new v1.11.0 ≡ old v1.10.0 ≡ base18）+ savenorm new×2
EXACT / old×2 EXACT；new vs old 存档差 = **恰好 2 项已知内容演进**
（inventory slots[16] = honey_bucket 隐藏 starter——存储格不渲染 =
截图免疫保持，GEN-2 隐藏 starter 先例）。论证：honey 无 worldgen +
不在默认热栏 + 无输入 = 无桶操作 = (c) 分支与蜜模拟零执行。

**S2**（存档状态 = 权威断言；瞄准/落位几何 = CPU harness gen126
实证：pour 目标 (24,45,-8) / 40f 蜜 y39 水 y37 oracle / 无反应行
稳态）：
- **A 蜜倒**（FLUID-4 池几何空池 + 蜜桶，输入跑 new ×2）：流动…蜜
  源 (24,45,-8) **L8** + 邻格 (25,45,-8) **L7**（封源池稳态）+ slot0 =
  **bucket** + hp20；×2 RAW EXACT 407951 B。
- **W 水回归 A/B**（s124 水场景 + 水桶，输入跑 4 跑）：new ×2 与
  old ×2 各 RAW EXACT 409647 B（FLUID-4 行为两侧一致）+ **new_a ≡
  old_a 字节一致 = 蜜代码结构惰性证明**（零回归）。
- **B 落蜜**（密封竖井，无输入 40f ×2）：蜜 **(24,39,-8) L8 井中**
  （帧 0..39 = 5 个门控 tick = 5 格，oracle 逐位）+ 列外零蜜格；
  ×2 RAW EXACT 708974 B。
- **C 落水对照**（同井，无输入 40f ×2）：水 **(24,37,-8) L8 已落底**
  （21 帧即落定）；×2 RAW EXACT 708974 B。**B vs C = 1/3 速证明**
  （同帧 40：蜜 y39 仍在落 vs 水 y37 已落底，互斥终态）。
- **D 无反应行**（蜜 24 / 水 25 / 封石 26 / 熔岩 27 + 全封，无输入
  600f ×2）：三源恒 **8/8/8 原位** + 行内零 obsidian/stone 转化
  （honey 无 water_reaction = 反应 pass 对蜜惰性）；×2 RAW EXACT
  699617 B。
- **S6**：old v1.10.0 二进制 + 蜜场景（s126_d）= init 失败点名
  `unknown fluid 'honey' (vocabulary: "water" ...)` + 无 clean exit
  （新种类存档向前不兼容 = 记档）。

## 性能 A/B（发布前必做）

**PASS = 成本可忽略**（tm_v110_perf v1.10.0 插桩 vs tm_v111_perf
v1.11.0-dev 插桩【FLUID-6 fluid.cpp 移植 + step0 patch 手工补
hunk#6 的 honey tick 区】，同 seed（perf_ab17 剥 fluids →
gperf_ab21.json）fall 900f 三窗，PITS 15 每跑前重 cp）：fluidCells
**win2/3 逐位一致（29803=29803 / 29800=29800**——零 worldgen 改动 +
无蜜 = 同世界 oracle 兑现）+ win1 −0.02% 填充 real-dt 类；PIPE3
changesApplied 1404=1404 逐位 + fluidCellsSeen 43994=43994 逐位 +
tick med 6.4/5.8 ms 同档；upd/render/swap 三窗全同档；无输入无蜜 =
门控与第三模拟段零执行 = 结构惰性实证。

## 坑位（PITS 追加）

- **PITS 299（per-kind 固定容器尺寸 = 种类数+1，非种类数）**：容器以
  kind 枚举值（1 基）为下标、槽 0 = 哨兵 → 3 种类须尺寸 4。尺寸 3 时
  Honey(3) 下标静默 OOB（array 无边界检查）→ 段错在**下游**测试
  （RaycastTest，非流体代码路径）= 新增种类时的第一排查点。同款
  家族 = mesher 的两种类三元派发（(kind==Water)?A:B）第三种类落入
  错误分支（honey 格按 lavaId 查块 = 测试注册表无 lava → 0 顶点，
  单测揪出）。
- 隐藏 starter 槽范围 = [9,26]（parseStarterSlots 硬校验点名）——
  加 starter 物品先数槽。
