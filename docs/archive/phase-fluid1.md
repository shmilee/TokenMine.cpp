# Phase FLUID-1 — 水量守恒（v1.0 任务表 10/29）

> 设计案 = ora-2（oracle）定稿 + 代理开放问题拍板（用户夜间授权"按建议的来"）。
> 修 KNOWN_ISSUES:37（Phase 9 遗留）"水量凭空增减"。

## 一句话定案

**根因 = 三条无记账的体积变化路径**：落池抬升（fluid.cpp:594-596，入流把
池水位按等级拉高 = 唯一"凭空增多"路径）、浅膜吸收（592-593，MC 式合池但
无记账）、世界底漏排（540-541，游戏内不可达但测试可达）；**`level` 已是 MC
flow time 的反相编码（level 8 = 源/flow 0，level 1..7 = flow 7..1，
fluid.hpp:5-8）→ 零新状态、零 format**；修复 = **MC 源/流动状态机的落池
规则统一**（删抬升、吸收统一、源上补充保持【湖修复特征 P86/P54 依赖】、
桶/反应语义因 L8=源恒等零改动）+ **守恒不变量 = 逐 tick 会计恒等式**
（ΔF = σ 扩散创生 − α 落池合并 − κ 源转换 − δ 边界漏排，严格整数每 tick
可断言；"总量 ±0"在 MC 语义下是错误目标——流前缘创生与合池吸收是两侧记账
项）；**熔岩同改**（simulateType 按种类共享零 kind 参数——P64"水 8 vs 熔岩
1"经核实是场景涌现非 sim 参数）；**门 87 = 结构性免疫不重锚**（基线视图
零水体，LGT-2 探针截图实证）；核心 diff ~±30 行（fluid.cpp 落池段 + hpp
模型注释），成本在 ~16 条单测（会计恒等式核心）+ step-0 混沌探针（31k
编辑分类：一次性流送沉降 vs 永久瀑布轮换 vs 振荡残留）。

## 开放问题拍板（设计 §10，按推荐通过）

- **Q1 熔岩 = 同改**（机制天然统一；per-kind 保留抬升 = YAGNI 特例否决；
  代价 = P90 计数演进 + 熔岩瀑布 churn，test 14/15 先行钉死）。
- **Q2 flow→source 静止转换（MC 静水）= 本相不做**（已沉降流本就静止转冷；
  转换有扇缘再扩散再生成风险；probe 若显示瀑布 churn 为主体 = 未来项带
  改进规则）。
- **Q3 P90 永久 churn = 接受**（MC 忠实：瀑布即常流；×2 RAW 确定性不受影响；
  单 chunk hot 成本 probe 定量）。

## 根因定位（设计核实，file:line）

- 文档条目 = KNOWN_ISSUES.md:37（Phase 9 遗留，活跃）+ MECHANICS.md:1077 +
  fluid.hpp:28-29 自认（"sources create fluid"）。
- 三路径：① 浅膜吸收 592-593（`L < below → propose(wp, 0)` 池不变 = 无记账
  删除）② 世界底漏排 540-541（`wp.y > 0` 门，链达 y<0 出世界；游戏不可达
  【y=0 恒 bedrock】仅 fixture 可达）③ 落池抬升 594-596（`L >= below(<8) →
  propose(下方, L)` = 入流改写池水位，**无来源体积增益**；其注释自称防振荡
  补丁，但振荡防护真正载体 = 源上补充规则 590-591（below==8 → 该格变 8 =
  湖底修复），抬升分支功能重叠且引入增益）。
- 核实修正：桶范式（game.cpp:3895-3913 舀 = 仅命中 L8 源 / 3977 倒 = L8）、
  水岩反应（fluid.cpp:276-277 sourceWater = twater==8）全锚定 L8=源恒等 →
  零影响。既有测试钉了抬升行为：test_fluid.cpp:130/:302 → 契约演进适配。

## In / Out 清单

### In

1. **落池规则统一**（fluid.cpp:575-599 三目重构）：下方=源 L8 → 该格变 8
   （源上补充/湖修复保持）；下方=流 0<B<8 → 该格变 air（落池合并，池水位
   不变——**删抬升**）；下方=固体/空 → 现行 fall/spread 零改动。
2. **守恒不变量 = 逐 tick 会计恒等式**（§状态机节，单测核心）。
3. **fluid.hpp 模型注释重写**（落池语义 + 恒等式 + 瀑布轮换注记 + 保留
   deviations：full-cell-fall / 反应不耗水 / burn）。
4. **step-0 混沌探针**（提交前全删）：31k 编辑分类（流送沉降/瀑布轮换/振荡
   残留）+ 删抬升预演 + 翻转触发器（若删抬升引入振荡 = 回炉不盲发）+ 产出
   S2 精确终态常量 + P90 演化计数。
5. **测试 ~16 新 + 2 适配**（test_fluid:130/302 抬升断言 → 新语义）；文档
   （MECHANICS:1077 节重写 / KNOWN_ISSUES:37 → 已修复存档索引 / phase 存档）。

### Out

- flow→source 静止转换（Q2 本相不做）。
- 世界底漏排规则删除（保持，游戏不可达边界项，会计中具名 δ）。
- 熔岩特殊保留抬升（Q1 否决）。
- 源 = 有限体积化（MC 源即无限；桶/湖修复全锚定 L8=源）。
- 流体 sim 速率参数化（现状无此参数，不新增）。
- **存档 format 零变更**（level 即 flow 编码）；游戏侧（桶/反应/tick/
  重光照）零代码变更。

## 状态机规则 + 守恒不变量

```
格 p 持 kind 流体 level L，非下落：
  下方（快照读）同 kind level B：
    B == 8     → p 变 8    【源上补充：成源，湖修复特征，保持】
    0 < B < 8  → p 变 air  【落池合并：池水位 B 不变——删抬升，统一吸收】
  下方 = 固体/他类 → 水平扩散（L>1 → 邻居 air 得 L−1，L1 终端）【零改动】
  下方 = 空 → 下落 conveyor（零改动）；y<0 = drain【零改动，具名】
```

封闭区域 R 内 F_t = 流格数（1..7）、S_t = 源格数（resting 8）：
`F_{t+1} = F_t + σ − α − κ − δ`（严格整数等式，每 tick 可断言）/
`S_{t+1} = S_t + κ`（源数封闭内单调不减）。σ = 扩散创生（应用后计）/ α =
落池合并 / κ = 源转换 / δ = 边界漏排；conveyor 下落 = 移动（成对净零不入
四项）。派生断言：C1 稳态（连续 20 tick 零 change）/ C2 池不抬升（下方
level 全程非增）/ C3 总量解释性（无 σ/α/κ/δ 事件 tick → Σlevel 恒定）/
C4 源不可消失。明示：MC 状态机不守恒总流格数（流前缘创生 σ 是 MC 同款）——
"近似守恒" = 每格 diff 100% 归入 σ/α/κ/δ（"凭空"二字从代码删除）。

## 测试计划（~16 新 + 2 适配）

- 会计恒等式（封闭盒 fixture 逐 tick 事件分类）：SealedBox 300 tick 恒等式
  + C3 / **PoolNeverRaised**（L3 池 + L7 流落下 → 池全程 =3，旧代码 →7）/
  FilmMergesIntoPoolAccounted / ColumnOnSourceRefillsFull（湖修复回归）/
  **BrokenLakeFloorSettlesNoOscillation**（删抬升无振荡主断言 = 翻转触发器
  常设化）/ SourceCountMonotonicInClosedBox。
- 边界/回归：SealedBoxSingleSourceExactFinal（probe 常量）/
  FlowOnlyFanSettlesCold / WorldBottomDrainCounted / FallChainLevelsPreserved
  / SpreadTerminalFilm / ConflictMaxMergeApplied / WaterLavaReactionUnchanged
  / **LavaPoolNeverRaised**（Q1 钉死）/ **P90OverflowExactFinal**（probe 常量，
  计数演进测试侧先行）/ CrossChunkWaterfallConsistent / SettledPoolGoesCold
  （99.8 集成）。
- 适配：test_fluid:130/302（抬升 → "流落浅池被吸收、池不变"新语义）。

## E2E 计划（今晚 10 min 无输入 + 晨间输入）

- **S1 门 87（免疫）**（~2 min）：fresh 60f ×2 + 与门 86 锚字节一致 + 红心 0
  + skygate。论证 = 基线视图零水体（LGT-2 探针截图实证）+ 流体变更只作用
  持流体格 + 零 format/零游戏侧代码。
- **S2 守恒封闭盒场景**（~4 min）：scratch save 预置石盒 + 扰动（L8 源
  mid-air + 斜流柱 + 浅池上落流）600f ×2 RAW（确定性 + 已静止）+ 存档
  fluids == probe 精确终态常量 EXACT + 盒外零流体变更。
- **S4 湖/反应回归**（~3 min）：P88 湖场景东堤视角 600f ×2 RAW（静态被围
  保持 = 湖修复规则回归）+ P90 溢出场景 ×2 RAW（**计数按 test 15 演化值**）
  + 水/岩接触终态（obsidian/stone 不变）。
- **晨间（输入）**：S3 桶判别（舀流动格不命中 / 舀源命中换桶 / 倒桶成源 →
  落池吸收可见）×2 savenorm + EXACT。

## 风险清单

1. 删抬升引入新振荡（注释自称防振荡补丁）→ probe 翻转触发器前置 + 单测 5
   常设化；振荡存在 = 回炉不盲发。
2. P90 熔岩溢出永久 churn（池不抬升 → 瀑布轮换）→ MC 同款（瀑布常流）；
   ×2 RAW 确定性保持；单 chunk 每 tick 数格成本 probe 定量；P90 计数演进 =
   文档化契约更新（test 15 先行）。
3. 31k 主体若为一次性流送沉降（新列生成本征）→ 修复消除"无解释项"但不消除
   沉降成本 = 诚实记档（probe 分类定论）。
4. 湖修复回归（P86/P88 依赖源上补充）→ 规则保留 + S4 双湖 ×2 RAW + 单测
   4/5 三重钉死。
5. test_fluid 既有断言隐性依赖（P64 水漫高原场景）→ 全量套件 + S4 回归兜底
   = 契约演进点名。
6. 会计事件分类与代码路径 1:1（max-merge 按应用计 / conveyor 成对净零）→
   分类逻辑与 sim 同文件同注释防漂移。
7. -Werror：diff 极小（三目重构）。
8. 并行零文件交集（fluid.* vs GEN-3 ore 区 / KNOWN_ISSUES 各动各节）。
