# Phase V2-10 — 网格修复批（v2.10.0，门 181 全 PASS）

> 2.x 收官前最后一项（PRE 行）。两子项 = KNOWN_ISSUES 渲染节两条
> 记档修复；**门须重锚论证**（① 改叶冠像素——演示区有树）。
> 设计定案 = /tmp/opencode/v210_design.md。零 format（25 不动）/
> 零 config 新键 / 图集·纹理·字体零变化。

## 实现

### ① 叶子-叶子内部面剔除（KNOWN_ISSUES 18 条）
- **数据驱动**（hard rule 3）：块 JSON 新可选布尔 `cull_same_id`
  （严格解析，cutout 先例 checkField；非布尔点名 "must be a
  boolean" S6 实证）；leaves/birch_leaves/spruce_leaves 三块各加
  一行（首个消费，其余块零改动）
- **mesher 规则**（chunk_mesher 全方块 greedy pass，opaque 邻判定
  之后、mask cell 生成之前）：邻居 block id == 自身 id **且** 自身
  cullSameId → 剔面（MC fast-graphics 叶子语义）；**纹理选择前
  剔除 = greedy merge 不见该面**；对称性由共享 registry 条目构造
  保证（双面同剔）
- **红线零触碰**（数据钉 + 单测钉）：glass-glass 不剔（无属性）/
  水-水流体规则（Phase 9）/ cross·slab·stairs·wall·fence·pane
  emitter 各自几何路径 / shipped 集 = 恰三 leaves（零影响证明）

### ② alpha cutout discard = 零代码确认（KNOWN_ISSUES 29 条划解）
- 编排者侦察定案：`render.alpha_test`（**代码默认 true**，
  getBool 先例，config 零新键）ON 时 `alpha < 0.5 → discard`
  **桶无关**作用于全部 basic-program draw（含 blend section——
  game_render.cpp PRE-5 注释 + 单 uniform 实证）
- **纹理普查**：含 0-alpha 像素的块纹理 21 个 = 20 纯 cutout
  （mid_a = 0 → cutout pass，discard + 深度写开）+ glass 唯一非
  纯（187 零 + 9 中 → blend pass，已被桶无关 discard 覆盖）；
  water/glass_pane/honey 中 alpha 全 uniform 无 0 孔（discard 不
  触发）→ **29 条（花/十字植物/玻璃孔遮挡）已被 PRE-5 全覆盖**
- **普查单测钉**（test_alpha_cutout `EveryHoleTextureIsDiscardCovered`）：
  含 0 孔纹理 ⊆ {纯 cutout → 所有引用块 cutout==true} ∪ {blend
  pass ∧ alpha_test 代码默认 true 双钉}——防未来新纹理把 0 孔漏
  进无 discard 路径

## 变更文件
- `src/block/block.hpp`（+cullSameId 默认 false + 红线注释）/
  `src/block/block_registry.cpp`（严格布尔解析）/
  `src/world/chunk_mesher.cpp`（greedy pass 同 id 剔面点）
- `assets/data/blocks/{leaves,birch_leaves,spruce_leaves}.json`
  （各 +1 行属性）
- `CMakeLists.txt`：2.9.0→2.10.0（单点）；`tests/test_debug_overlay.cpp`
  F3 钉 v2.10.0
- `tests/test_chunk_mesher.cpp`（+6：ChunkMesherV210Test 解析/
  缺省/剔面几何/跨 chunk/leaves-glass 双面保留/红线数据钉/
  shipped 恰三钉）；`tests/test_alpha_cutout.cpp`（+1 普查钉）

## 实现侧自证（编排者独立核证）
- 构建 0 警告；**2181/2181**（2174 + 7；编排者独立复跑）
- 冒烟 banner v2.10.0 + format 25
- 叶冠面数减证（单测几何钉）：LeafLeaf 24v = 6 quads vs pre-fix
  32v；跨 chunk 20v vs 24v

## 门 181（编排者执行；old 侧 = v2.9.0 = 08466fe 树）— 全 PASS

### S1 = **base18 重锚论证**（Phase 18 base3→base18 先例）
- 旧侧（v2.9.0）60f ×2 恒等（179116 B）且 **o1 ≡ 现锚逐字节**
  （环境干净，diff = 纯代码归因）
- 新侧（v2.10.0）60f ×2 恒等（**178148 B**）
- **新 vs 旧 diff = 4185 px（>30 阈值），bbox (0,0)-(679,380)
  = 画面上部叶冠区**；不变量全过：**old 侧天空蓝泄漏 = 0**
  （每 diff 像素旧值非天空 = 叶冠轮廓内）+ **UI 区（y≥380）
  零 diff**（准星/热栏不涉）
- 像素归因（fixer 侧，同场景）：5101 px 全叶冠——1730 叶-叶再
  着色 + 2681 透孔后天空【旧「邻叶背面暗块」→ 新「天空」=
  透明孔透视修复直接实证】+ 690 树干木色（受光雾值）
- 跨版 savenorm **EXACT**（零 format，世界恒等）
- **新锚 = 178148 B 采用**（base18 第 2 次重锚 = 第 N 次过门
  记档先例）

### S2 = 叶冠像素判别
- ① 叶冠 E2E = S1 重锚论证的 diff 本体（无输入演示区树冠 =
  场景；无独立输入场景——剔除是网格几何，无输入即呈现）
- ② discard 覆盖 = 普查单测钉（S3 内，零代码确认的权威证据）

### S3 套件
- **2181/2181**（编排者独立复跑）

### S6 精确点名（真实二进制，scratch 资产）
- leaves.json `cull_same_id: "yes"`（非布尔）→ init 拒：**「block
  file 'data/blocks/leaves.json': block property 'cull_same_id'
  must be a boolean」**

### 性能 A/B PASS 同世界同成本（用户硬要求，发布前）
- v2.9.0（tm_v2100_perf = 08466fe + g139 27 标记）vs v2.10.0
  （tm_v2100_perf_new = 终版工作树 + g139 双 27 标记，banner 双侧
  实证）：fall 900f 无输入 seed 123456789（演示森林世界 = 叶冠
  剔除激活窗）：
  - PERF-WIN 三窗全指标同档（upd med 0.62/0.62/0.58 vs
    0.59/0.63/0.65 ms；p95 同款摆动；render med 0.53/0.54/0.45 vs
    0.51/0.51/0.50；swap 15.4/15.4/15.6 vs 15.4/15.4/15.4 = vsync
    主导同值；fluid 0.047/0.040/0.043 vs 0.043/0.040/0.047、
    fires/ents 同档；stream 0.0130/0.0121/0.0123 vs
    0.0128/0.0140/0.0140 = 同档——叶冠面数减无可测回归信号）
  - **PIPE3-S0 changesApplied 三窗 max 9/1/3 双侧逐位**
  - **终档 fluids 16/16 + blocks 36/36 集合恒等**（同世界最强形——
    剔除是网格几何，世界/光/流体零改动）

## 偏离（记档）
- 叶子剔除 = **MC fast-graphics 语义**（fancy 模式叶叶不剔——
  记档选择 fast 侧 = 记档 18 条「结果正确但多发了面」的修复
  定向；fancy 观感不在本批范围）
