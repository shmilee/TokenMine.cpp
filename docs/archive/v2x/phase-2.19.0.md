# Phase V2-18 — AO 环境光遮蔽（v2.19.0，门 190 = 二批收官，全 PASS）

> 设计定案 = `/tmp/opencode/v218_design.md`（唯一范围权威，11-04 编排者
> wiki 核定 MC 机制：Light 页 smooth lighting 节——AO = "darkens
> corners… based on the blocks placed diagonally in front of the shaded
> side"；paintings/item frames/water surfaces 不受 smooth lighting 影响；
> Options 页 Smooth Lighting OFF/ON 默认 ON Beta 1.3 引入）。本阶段消费
> 「按面采样光照，无逐顶点插值/环境光遮蔽」记档（KNOWN_ISSUES [Phase 7]）。
> **门须重锚**（全 mesh 光照变化——世界光照视觉全变）。

## 实现

### 纯函数族（新 world/ao.{hpp,cpp}，mesher 唯一消费点）
- **经典 MC 顶点 AO 规则**（viewer 侧 = C+n 基）：面顶点 c（法线 n 的
  面上两边缘方向 e1/e2 = **背离网格单元于该顶点的方向**——顶点在单元 min
  端 → −axis，max 端 → +axis；三检查格 = 与顶点共享边、非 viewer 格的前方
  三格）：`side1 = V+e1 / side2 = V+e2 / corner = V+e1+e2`（V = C+n）。
  实现 = 逐轴取面表 corner 的 0/1 分量选向（`cornerAxis` + `edgeSign`），
  不依赖 face 表 edgeA/edgeB 与升序平面轴的对齐（+X 面 edgeA 沿 −Z、
  edgeB 沿 +Y——按 k 位取轴会跨轴错配，单测 `CellCornerLevelsMatchPerVertexRule`
  双路径交叉钉防回归）。
- **遮挡判据 = opaque ∧ 高度带**（与剔面同 `properties.opaque` 旗 = 项目
  自洽规则，非 MC full/half/stair 形词汇）：`aoOccluderBand(block, state)`
  → 满块/stairs/wall/fence [0,1]（wall/fence 格内实心区跨全格；**偏离 2**：
  stairs 斜面不做高度精化 = 斜面上沿过遮记档）/ slab state 0 [0,0.5] / 1
  [0.5,1] / ≥2 [0,1]（= Entity::solidRange 同款 {bottom,top}，kSlabHalf 单
  常量）/ air·越界·非 opaque 空带。slab 遮挡体状态读 = emitSlab 同款
  （World per-cell byte，unset → stateDefault 哨兵回落）。
- **高度带判 = 闭区间**（`aoBandCovers(y, rowOffset, band)`：vertex 世界
  y = cell.y + yLocal，查询格行 = cell.y + rowOffset，yInRow = yLocal −
  rowOffset ∈ [lo,hi] 含端点——全值域 0.5 倍数，浮点精确无 epsilon）：
  半块同款 MC「顶点只被同高度邻格遮」自动成立（底 slab 顶面 y+0.5 查
  上行，yInRow = −0.5 恒不遮；顶 slab 顶面 y+1 查上行 yInRow = 0 正常
  遮）；slab 接缝顶点 0.5 对 [0,0.5] 与 [0.5,1] 同遮（含端点 = 设计定案）。
- **系数 4 档**（`aoCornerLevel` + `aoFactor`，MC 精确值）：side1∧side2 →
  **level 3 = kAoCorner 0.3**（双邻特例，计数会说 2——MC 精确特判）；否则
  计数 0..2 → **kAoFull 1.0 / kAoOne 0.75 / kAoTwo 0.5**。
- **smooth = GPU 双线性**（顶点系数 rasterizer 插值）。**偏离 1**：MC 的
  4abcd 中心修正项省略（需 fragment 重构 4 顶点原值，attribute 模型不可
  得）；同条记档夜间语义：LGT-3 撤退作用于 AO 后的 sky（暗角夜间更暗 =
  合观感，曲线微差）。

### 编码：bake 入既有 skyLight/blockLight attribute（零 shader/顶点格式/uniform）
- 生效面顶点光 = 既有光值 × 该顶点 AO 系数（skyF×ao、blockF×ao 同乘；
  fragment 公式 `max(sky−u_nightRetreat×(1−u_dayFactor), block)` 不变照跑，
  AO 自然作用两路光）。**15 float 顶点格式零变更、shader 零变更、无新
  uniform、per-draw 光覆盖代码零触碰**。
- **偏离 3**：per-draw 光覆盖（信标光束 game_render.cpp:235-243 / 掉落物
  game_entities.cpp:654-666,715-723 / XP 球 :1536-1543）= 顶点阶段整体
  替换 → 覆盖值不含 AO = 那些面 AO 消失（特效光覆盖优先，MC 光束亦不
  AO）。记档于本文 + KNOWN_ISSUES 划解条目。

### 生效面决策（emitter ×ao 写点普查 = 生效 4 vs 豁免 6）
| emitter | AO | 写点（chunk_mesher.cpp 现行行域） |
|---|---|---|
| greedy 满块 emitMergedQuad | ✅ | MaskCell.ao[4] + 顶点 `skyF×aoF[i]` |
| slab 三态 emitSlab/Top/Full | ✅ | 每顶点 `aoVertexLevel(..., corner 表角, 发射 y)` |
| stairs emitStairs | ✅ | emitQuad/emitTri 每顶点（4 单位法线面 = 通用规则；**斜面 = +Y 面规则**，顶点实际 y：高沿 y1 查上行 yInRow 0 可遮【台阶上看台顶暗】/ 低沿 y0 查上行 yInRow −1 恒不遮） |
| fluid emitFluid | ❌ 免（MC 精确：water surfaces 不受 smooth lighting 影响） | 零触碰 |
| cross emitCross | ❌ 免（同款薄几何） | 零触碰 |
| wall emitWall | ❌ 免（**偏离 4** 薄几何范围外） | 零触碰 |
| fence emitFence | ❌ 免（偏离 4） | 零触碰 |
| glassPane emitGlassPane | ❌ 免（偏离 4） | 零触碰 |

- **MaskCell 合并键扩 ao[4]**（uint8 档位精确比较非浮点）：共享边角 AO
  不等 = run 断开（quad 变小 = MC greedy+AO 同款）；merged quad 顶点 i 取
  run 端 cell 的 corner i = **seed.ao[i]**（键强制 run 内 ao[4] 全等；端
  cell 角 i 的存储值 = 顶点自身 AO——e 向 = 背离端 cell 于该角 = 背离
  run 于该顶点，构造性一致）。**ao[4] 序 = 面表 corner 序 c0..c3 =
  emitMergedQuad 1×1 run 的顶点发射序**（单测钉）。共享边两半的值相等性
  另有构造保证（各侧「多查的那格」= 邻格的 viewer 格 = 面可见性不变式非
  opaque）。
- **快照零新依赖**：AO 查询 = 面邻格 ±1 ⊆ mesh job 既有 3×3 列快照
  （chunk 边界顶点 AO 读邻 chunk 快照既有机制）。worker 纯快照函数不变
  （同快照同值确定性 ✓；无新 RNG/流）。

### 配置门（render.ao）
- 严格 bool **默认 true**（weatherEnabled 范式）：absent = true（config.
  json 不动）/ present 非 bool = **点名拒**（S6 臂）；`aoEnabled(Config)`
  纯函数（world/ao）+ game_init 严格读 → `Game::m_aoEnabled` → postMeshJobs
  唯一 mesher 构造点传入。**off = 查询不跑 + ao[4] 恒零 + 系数恒 1.0 =
  pre-V2-19 位级恒等**（代码路径门控实证 = 本阶段自证①）。

## 变更文件
- 新 `src/world/ao.{hpp,cpp}`（纯函数族 + render.ao 严格读）。
- `src/world/chunk_mesher.{hpp,cpp}`：MaskCell +ao[4]/sameAs 扩；构造
  +aoEnabled 参（默认 true）；meshFaceDirection 掩码构建 aoCellCornerLevels
  （m_aoEnabled 门控）；emitMergedQuad 顶点光 ×aoF[i]；emitSlab/Top/Full
  每顶点 ×ao；emitStairs emitQuad/emitTri 每顶点 ×ao（斜面 +Y 规则）。
  fluid/cross/wall/fence/glassPane 顶点写点零触碰。
- `src/core/game.hpp`（m_aoEnabled + include）、`src/core/game_init.cpp`
  （render.ao 严格读）、`src/core/game_streaming.cpp`（构造传参）。
- `CMakeLists.txt`（VERSION 2.18.0→**2.19.0** 单点 + ao.cpp + test_ao.cpp）。
- `tests/test_ao.cpp`（新，16 例）、`tests/test_chunk_mesher.cpp`（+11 例
  V218 节 + include）、`tests/test_debug_overlay.cpp`（F3 钉 v2.19.0）。
- 文档：CHANGELOG v2.19.0 条 / BACKLOG V2-18 行 + 变更记录 / KNOWN_ISSUES:22
  划解 / MECHANICS 光照节 AO 段 / GAMEPLAY 环境视觉节 AO 条。
- **不动**：config/config.json（代码默认）、shaders/、mesh.hpp（15 float）、
  game_render.cpp per-draw 覆盖、opencode-sessions/。

## 偏离（记档 4 条 + fixer 自主定案 3 点）
1. **MC 4abcd 中心修正项省略**（attribute 模型不可重构 fragment 4 顶点原
   值）；同条：夜间 retreat 作用于 AO 后 sky 的曲线微差（暗角夜间更暗 =
   合观感）。
2. **stairs 斜面高度精化不做**（occluder 带恒 [0,1]——斜面高沿被上方
   遮挡体过遮记档）；本项目 AO 面用**顶点实际 y** 判带（slab 0/0.5/1、
   stairs 0/1）。
3. **per-draw 光覆盖面 AO 消失**（信标/掉落/XP 球——覆盖值不含 AO；MC 光
   束亦不 AO）。
4. **wall/fence/glassPane 免 AO**（薄几何范围外：fence/pane 顶点 y 带杂、
   视觉收益小）。

**fixer 自主定案（实现期，设计稿未钉死的点）**：
- **e 方向取「背离网格单元于顶点」= 与顶点共享边的前方三格**（设计稿括注
  「away from vertex」若按面内边方向读 =  staircase 实证错集——台阶竖面
  顶沿不被上方台阶遮、凹槽角不暗 = 与 MC 经典观感矛盾；按 MC 经典实现并
  以三具名场景钉：台阶竖面顶沿 0.75 / 凹槽双邻 0.3 / 抬升地板边 0.3+）。
  「MC 精确值」+「classic MC 块状 AO 观感」为稿内权威，取观感一致解。
- **slab/wall/fence 作遮挡体** = opaque 旗同款（wall/fence 格内实心跨全格
  → 带 [0,1]；设计稿未点名 wall/fence，按「与剔面同 opaque 旗」自洽规则
  归入满带）。
- **stairs 斜面 = +Y 面规则**（上行查询 + 顶点实际 y；斜面无同高行 → 低沿
  恒不遮、高沿被上方遮挡体遮——台阶观感实证：run 中斜面恒亮、竖面顶沿
  暗）。

## 测试（2482/2482，+27）
- `test_ao.cpp` 16 例：aoCornerLevel 8 组合表 / 4 档系数表 / 遮挡体带 ×
  形×state（full/glass/air/slab 0,1,2/无 state/stairs/wall/fence）/ 闭区
  间带判（0.5 接缝双遮 + 行偏移 ±1 端点 + 半块中平面恒不遮 + 空带）/
  孤立块 6 面 ×4 角恒 0 / +X 面 4 角具名查询集 / 双邻特例（s1∧s2∧¬c =
  3 非 2）/ −Z 面顶角查上行 / slab 中平面顶面恒 0 + 顶 slab 顶面正常 +
  侧缝 0.5 闭端点（底/顶 slab/满块三对照）/ 世界底/顶越界恒 0 / slab
  遮挡体状态 World 读（unset→default / state 2 / state 1 底角不遮）/
  8-fetch 与逐顶点双路径交叉钉 + 空世界 6 面恒零 / render.ao 严格读
  （absent→true / 显式 / 非 bool 点名 ×2）。
- `test_chunk_mesher.cpp` V218 节 11 例：孤立块 24 顶点光零改动（fixture
  红线索引实证）/ greedy corner 0.3 档 + block 光同乘（10/15×0.3）/ run
  断开（同光异 AO = 2 单面 + 断口端角 0.75 钉）/ merged 端角取值（3×1
  run 底沿 0.5 双端 + 顶沿 1.0 双端，tiled=1）/ slab 缝闭端点（底角 G∧F
  = 0.3 双端点 + 顶角 F 0.5 = 0.75）/ slab 顶面中平面 0 vs 顶 slab 行上
  0.3/0.75 + 顶 slab 底面中平面 0 / stairs 竖面顶沿 0.75 双顶角 + 底沿
  1.0（台阶梯度）/ fluid 免 AO 位级钉（corner 3 档世界仍 1.0）/ cross 免
  AO（8 顶点 1.0）/ wall·fence·pane 免 AO（wall 顶环 y5.5 会被上行满块
  0.5 遮 → 仍 1.0 = 判别形）/ **off 位级恒等**（off +Z 2×1 合并 + 光全
  1.0；on +Z 断开 + 端角 0.75；−Z 合并/断开口径；顶点流差 = 恰断 run）。
- 既有 ChunkMesherTest ~60 例 + 全套 2455 零改动全过 = 孤立块 fixture
  AO=1.0 零改动红线实证（默认 AO on 跑全旧钉）。

## 门 190（old = v2.18.0 49f4f15 tm_v2180_base + tm_v2180_perf_new
【v2.18.0 终版 + g139】/ new = tm_v2190_perf_new【v2.19.0 + g139】）—
全 PASS

### 门口径（立项定案）
- S1 = **重锚**（V2-10 叶冠 4185px 先例论证范式）+ **off 配置位级证明** +
  savenorm EXACT（零 format）。
- S2 = AO 视觉场景（无输入——AO 无交互面，输入 E2E 不适用，记档）：journal
  置 corner 场景（2 满块角 + glowstone 光 15 照亮 corner）→ 新 on ×2 恒等
  + on-vs-off diff 单调变暗审计。
- S3 全套 / S6 点名（render.ao 非 bool）。
- 性能 A/B：主对 900f（AO 成本 = mesh 期：greedy run 断裂 → quad/顶点数
  增 → render 成本）+ 密角场景自成本（600f）。

### S1 = 重锚（结构免疫不适用——世界光照视觉全变）
- old ×2 恒等 ≡ 旧锚 **179027 B** ✓（旧侧完好，门机 xenv 复现）
- new-on ×2 恒等（**224468 B**）✓
- **new-off ×2 ≡ 旧锚逐字节**（off-path 位级根：AO 路径全门控，V2-4
  weather off 先例）✓
- savenorm on vs off **EXACT**（零 format——AO 纯渲染）✓
- 单调变暗审计：world diff **23672 px** 全单调变暗（逐通道 on ≤ off+2，
  eps = shader 舍入头）；**HUD 实测 bbox（时钟带 / XP「0」/ XP 条）
  0 diff**（首版 y>600 启发式对 679×744 门窗口无效 = frustum 右缘
  85 世界 px 误判，裁剪实证非 HUD——PITS 384④）；变亮 **1 px** =
  z-fight 换面白名单（x9,y264 草顶面 (91,139,41)↔石侧面 (79,79,79)，
  屏左缘近 edge-on 地形深度平手，0.0042%，逐 px 留证——PITS 384③）
- **重锚定案：base18.png ← on 侧 224468 B**（谱系 179027 → 224468）

### S2 = AO corner 场景（无输入，记档）
- corner on ×2 恒等（383053 B，确定性 bake）✓
- on-vs-off：diff **107942 px**（y 86..743 全世界——corner 场景 =
  base18 世界 + 3 置块，corner 面 0.3/0.75 档在 unit test 族钉值）全
  单调变暗；变亮 **2 px** = 同款 z-fight 类（草↔草，0.0019%，留证）

### S3 套件
- **2482/2482**（2455 基线 + 27 V2-18：test_ao 16 纯函数族 +
  mesher 11 合并键/断 run/端角/slab 缝/中平面/stairs/豁免/off 位级；
  编排者独立复跑 ✓）

### S6 = 点名
- render.ao 非 bool ×2（字符串 "yes" / 整数 1）均 init 点名拒
  （"config render.ao must be a boolean (true/false), got a
  non-boolean"）✓

### 性能 A/B PASS 无可见系统增量（用户硬要求）
- 主对 900f 无输入同 seed 123456789：PERF-WIN 三窗 render med
  0.457/0.407/0.406 vs 0.410/0.417/0.411 ms（稳态窗 +2.3%/+1.3%，
  窗 1 −10% = 噪声带内，**无可见系统增量**）+ upd med 0.377/0.329/
  0.361 vs 0.382/0.375/0.386 + **changesApplied 9/1/3 逐位**
- 密角场景自成本（600f 无输入）：on render med 0.422 vs off 0.431 ms
  = 持平
- 终档集合恒等（AO = 纯渲染，journal 不受影响）：fluids 16/16 +
  blocks 36/36 + format 28/28 ✓

### fixer 自证（本环境，报告编排者前）
1. **off ≡ 旧二进制 逐字节**：old = `git worktree` 49f4f15 独立构建；
   同环境（本仓 cwd + 工作树 config 副本 + `render.ao=false`）60f 无输入
   截图：old 179309 B ≡ new-off 179309 B **cmp 零 diff**（锚 /tmp/
   opencode/base18.png 179027 B 在本环境旧二进制亦不复现——云带 + XP 条
   环境态漂移，与 AO 无关 = 相对证明为位级根）。
2. **new on ×2 恒等**：默认 config 60f 无输入 ×2 = 224764 B 逐字节 ✓。
3. **on-vs-off diff 单调变暗审计**（基线 = off ≡ old）：**36462 px**
   （4.9%）全落世界区（bbox y 3..613）；**HUD（y≥640）0 diff / 准星 0
   diff / 天空 0 diff**；36401 px（99.83%）逐通道 new ≤ old+1.5（纯变暗）；
   61 px（0.17%）= run 断裂重排引发的共面 z-fight 平手换面（50 明显换
   纹理色相 + 11 近同纹理 ≤7 单位，全在中景地形带 y 231..399，确定性、
   无光照反向）= 重锚论证的 diff 类别白名单（V2-10 叶冠先例同款口径）。
4. 构建 0 警告（-Wall -Wextra -Wpedantic -Werror）；全套 2482/2482；
   banner `=== TokenMine.cpp v2.19.0 ===`；save smoke format **28** 顶层
   键闭集不变。

## 坑位（本阶段新增，已收 PITS 384【五子项：① e+1 越界 ② e 向背离
单元语义 + edgeA/edgeB 轴错配 ③ z-fight 换面白名单 ④ 门窗口 UI 带
启发式无效【门 190 执行新发现】⑤ 世界坐标手算 7 连错】）
- **`e + 1` 索引越界**（e ∈ {−1,+1} → e+1 ∈ {0,2} 写 band[2] 越界 1 字
  = 栈破坏 → -O0 静默错值、-O2/3 段错误；独立隔离复现 + 编译器告警
  「iteration 1 invokes undefined behavior」定位。sign 数组索引用
  (e+1)/2 或 i∈{0,1} 显式循环）。
- **面表 edgeA/edgeB 与升序平面轴不对齐**（+X 面 edgeA 沿 −Z、edgeB 沿
  +Y）：按 k 位（k&1/k>>1）取轴方向会跨轴错配——必须按 corner 的实际
  分量逐轴选向（aoVertexLevel 与 aoCellCornerLevels 双实现交叉钉防回归）。
- **世界坐标手算陷阱**（测试期 7 连错）：cell 的 −X 面在 x=cell.x（非
  cell.x−1）；相邻 cell 共面但不同 slice（+Z 片 vs −Z 片永不相并）；
  occluder 自身面进同 normal 计数（+Z 计数 12 非 8）；setFluid 不建
  chunk（先 getOrCreateChunk）；stairs 烘**自身格**光非面邻格；共位顶点
  歧义（A 的 −X c1 与 O 的 +X c0 同位不同 normal；O 的 −X c0 与 A 的
  −X c1 同位同 normal = 发射序依赖，改用独占位钉）。
- **base 锚环境漂移**：锚 179027 B 本环境旧二进制亦不复现（云带 + XP 条
  色 = 时态/窗口态）——off 位级证明取「新 off ≡ 旧二进制 同环境」相对口
  径，绝对锚对留给门机（xenv 环境）。
