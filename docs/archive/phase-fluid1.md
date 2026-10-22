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

## 构建与验证结果（实现 + E2E）

**实现（路线 A 定案，fixer 交付）**：
- 翻转触发器**触发后回炉**（设计风险 #1 前置探针的预期功能）：统一吸收规则把
  悬崖薄膜级联从静态沉积变成永久 period-2 棋盘翻转（环 24→196 churn 格、总
  编辑 18.6k→118k/600tick、P90 1→74 churn）= 新增海水崖膜 churn 类，超出
  Q3 授权范围 → **路线 A**（只删 `L>B → propose(下方,L)` 抬升一分支，`L≥B →
  该格保留`静态沉积稳压器保留）：守恒目标完整达成（唯一无记账增益路径删除）
  + churn 回旧规则水位。**净核心 diff = fluid.cpp 一分支**。
- 路线 A probe 四组复验全绿：① 海崖 trace tick400≡401 静态薄膜柱恢复 ② 环
  churn = 24 格且与旧规则 churn 集合 diff 为空（零新振荡）③ 封闭盒 64 格表
  逐位一致（规则无关常量）④ P90 与旧规则完全一致（66/473/3.42 per tick/1
  churn/157 溢出）。
- 1312/1312（15 新 + 2 适配【FlowingRaisesPoolBelow → FlowDepositsOnPoolWithoutRaising】
  + 存量 1295 零回归）、干净重建 0 警告、5 文件 +785/−49、scratch 探针全在
  /tmp 零入库。游戏侧零代码、零 format。

**E2E（夜间/上班时段无输入批，9 跑零白帧——用户在场 away 组件闲置 = 干净
条件）**：
- **S1 base18 门 88 = 结构性免疫（不重锚）**：fresh 60f ×2 全帧字节一致 +
  **与门 84 锚（s1_l2c_r1.png）字节一致**（门 85-88 连锁）+ 红心 0 + skygate
  75,348 严格。
- **S2 封闭盒守恒场景**（石盒 x[8,17]×y[40,46]×z[8,17] + 14 格水初态 +
  盒内玩家）：600f ×2 RAW 字节一致 + savenorm **EXACT**（剥 fires remaining
  = real-dt，P57 先例）+ **盒内流体 64 格与 probe 精确终态表逐位一致**
  （extra=0 missing=0，tick 11 达稳态）= 守恒核心断言 E2E 级。
- **S4a P88 深湖静态**（既有场景 600f ×2 RAW 字节一致 = 静态被围契约保持，
  全 L8 源湖不触发落池规则）。
- **S4b P90 表面湖新旧等价**：新 ×2 RAW + savenorm EXACT + **新 vs 旧
  （v0.3.0 二进制 + v0.3.0 资产 scratch，同 seed 9 spread0 fresh 600f）
  core 逐段 identical**（blocks 30=30 / fluids 12=12）= "P90 场景从未触发
  被删抬升分支"的 probe 级主张 E2E 确认。
- 晨间输入项（桶判别 S3：舀流动格不命中 / 舀源命中）= 等输入 E2E 窗口。

**遗留（输入 E2E 积压，环境原因）**：2026-10-22 上午用户 30 min 免键鼠窗口
中，ENT-2 潜旋 lock 成功但点击落在 ROOT 窗口（重启后桌面态漂移）+ xdotool
键名 `Left_Control` 重启后不认（已修 `Control_L`）+ lock 修复分支 xprop 回退
（已修）——输入 E2E 全积压（ENT-2 潜旋 / GEN-2 放置+潜旋催熟 / GEN-1 进洞 /
FLUID-1 桶判别 / 可选 LGT-2 放叶 / GEN-3 挖矿 / GEN-2 破坏），等桌面态稳定
+ 免键鼠窗口重跑。
