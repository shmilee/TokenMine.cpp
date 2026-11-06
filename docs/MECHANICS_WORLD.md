# 机制详解 · 世界与渲染（现状 v3.24.0）

> 玩家/系统侧（内容总览/操作与交互/实体与 Mob/生存系统/状态效果/弓与箭/流体桶与点燃/demo 回放/合成与容器/存档/难度/确定性约定）= `docs/MECHANICS.md`（主文件；头部「来源约定」全文在主文件）。

> 2026-11-07 增量刷新：现状 = **v3.24.0**（V3-10b 末地内容族 +
> 末影珍珠，门 214；其前 v3.23.0 = V3-10a 末地维度框架 + 真传送，门
> 213【format 32】；v3.22.0 = V3-14d strider 骑乘，门 212；v3.21.0 =
> V3-14c piglin 交易，门 211；v3.20.0 = V3-14b ghast，门 210；v3.19.0 =
> V3-14a 爆炸机制，门 209；v3.18.0 = V3-11b 创造物品栏，门 208；
> v3.17.0 = V3-11a 创造基础，门 207【format 31】；v3.16.0 = V3-15
> 生存/表现小批，门 206；v3.15.0 = V3-13 死亡统计屏，门 205；v3.14.0 =
> V3-8b 手柄映射，门 204【sim 2】；v3.13.0 = demo sim 版本，门 203；
> v3.12.0 = 实机报障阴影缩尺，门 202；3.x 矩阵收官于 v3.11.0；其前
> 2.x 两批 20 版收官 v2.0.0 → v2.19.0）、无输入基线门 **214** 次、
> 单元测试 **2899/2899**、`-Wall -Wextra -Wpedantic -Werror` 0
> 警告、存档 **format 32**（下界维度 format 30：player.dimension +
> nether 段 + nether_entities；创造 format 31：player.creative；末地
> format 32：dimension 词表 +end 按 format 分词 + end 段 +
> end_entities；gzip 编码 V3-7 起；下界/末地内容 = 世界生成演进零
> format）、base19 锚
> **225272 B**（重锚谱系 179116 → 178148【V2-10 门 181】→ 178676
> 【V2-19 门 182】→ 179027【V2-11 门 183】→ 224468【V2-18 门 190，AO
> 世界光照】→ **225272【V3-6 门 200，填充感知雾 = 3.x 首重锚】**；门
> 184–189 = 结构免疫第 65–70 次免重锚；门 191–199 = 第 72–80 次；门
> 201–214 = 第 81–95 次免重锚（对 base19）【v3.0.0 banner = 第 71 次】）、
> PITS **406**。上一轮全面刷新
> （2026-11-02）止于 v2.0.0、2026-11-03 增量止于 v2.3.0（下条），本次把
> v2.11.0 → v2.19.0 的机制（提前项 `[V2-19]` + 第二批 `[V2-11]`~`[V2-18]`，
> 逐版见 `docs/archive/v2x/CHANGELOG.md`，细节证据见 `docs/archive/v2x/`
> 20 份阶段存档）并入各节；3.x 十二版（v3.1.0–v3.12.0）
> 同样并入（逐版见 `docs/CHANGELOG.md`，细节见 `docs/archive/
> phase-3.x.0.md` 12 份）。
>
> **2026-11-03 增量（v2.1.0→v2.3.0）**：V2-1 前视角（门 172，`C` 三循环 +
> 翻转角 + aimReach）/ V2-2 Steve 皮肤（门 173，MobBox per-face 纹理）/
> **V2-3 教程系列化（门 174，tutorials/ 三系列 + format 25 + pickup./
> smelt. 事件）**；现状 = 测试 **2109/2109**、存档 **format 25**、门计数
> **174**（教程节/键位/存档 format 节/F3 行已同步；相机/皮肤细节见
> `docs/archive/v2x/phase-2.1.0.md` / `phase-2.2.0.md` / `phase-2.3.0.md`）。
> **阶段标签约定**：`Phase NN`（或 `[PNN]`）= v0.x 时代编号阶段；`[XXX-N]` =
> v1.x 任务表任务 ID（如 `[ENT-7]`/`[SURV-3]`/`[CRAFT-1]`/`[PRE-2]`/`[STRM-1]`）；
> `[99.N]` = P99 后性能审计系列；`[V2-N]` = 2.x 任务表任务 ID（第一批
> V2-1~V2-10 + 提前项 V2-19 + 第二批 V2-11~V2-18；任务号与版本号解耦——
> `[V2-19]` = v2.11.0 / `[V2-11]` = v2.12.0 / `[V2-18]` = v2.19.0）。
>
> **config 约定**：下文「代码默认」= 缺键回落值；`config/config.json` 是**已提交
> 文件**，含用户调值（`world.view_distance` 80【代码默认 8】/
> `world.day_length_seconds` 600【代码默认 240】/ `ui.scale` 1.5【代码默认 1.0】/
> `world.seed` 123456789 + `player` 节全默认值）——文档与行为以代码默认为准，
> 实际运行以已提交 config 为准。
## 世界与流送

- **世界**：16×16×16 chunk，世界高度 128（y ∈ [0,128)），X/Z 无限；同一 seed
  逐块复现（`WorldGenerator` 无状态纯函数 `generateChunk`，worker 并行调用 +
  单元测试 + 像素级截图比对保证）；**演示区 = 4×4 chunk 列**（world x,z ∈
  [−32,32)，启动同步生成）
- **流送圆盘 `[STRM-1]`**（v1.0 任务表首项）：流送环 = **圆盘**（纯整数谓词
  `dx²+dz² ≤ R²`，方形 (2R+1)² 环砍除）+ **远平面帽** `R_eff = min(view_-
  distance, 64)`（`kStreamCapRadius`；推导 = 远平面 `kFar` 1000 块——可绘制列
  全部 d ≤ 63.91，d ≥ 65 的列最近角 ≥ 1002 块原理上永不渲染，流送纯浪费；
  用户定案选项 b「只装可绘制列，地平线视觉零下行」）——vd80 实测 RSS **9.08 →
  4.59 GB（−49.5%）**；R64 圆盘 = **12853 列** ≈ 103k chunk
- **触发迟滞**：玩家距缺失列 ≤1（平方欧氏；整数偏移下 = 玩家列 + 4 轴邻列，
  对角 (1,1) 不触发）激活，半径圆盘充满前不解除；静止玩家（距边缘 ≥2 列）
  永不触发 = **无输入运行零 job、基线截图逐字节复现**
- **填充预算**：每帧 **2 列**（`kStreamColumnsPerFrame` 2，`[99.8]`），距离
  优先（最近先）
- **卸载 `[99.2]`**（旧版「只增不卸载」已推翻）：距玩家 Chebyshev >
  view_distance+1（`kUnloadHysteresis` 1 防抖带）的列卸载（pool flush 后主
  线程单写窗口）；保留 = 演示 4×4（重生安全）+ 玩家编辑列（m_editedColumns，
  存档 diff 所在）+ 五类 per-block 状态列（炉子/营火/作物/信标/火，存档交叉
  校验对象）+ 在途生成列；实测走路 85 s RSS 平台 **470–474 MB**
- **流体列保留 `[STRM-2]`**（仅流体模拟改动的列卸载后不再重置生成态）：
  驱逐时按 journal 格限域快照该列 live≠fresh diff（`retainColumnDiff` =
  `journalDiffChunk` 字节同语义，**存 diff 条目非 chunk = 内存有界**）→ 再
  集成回放（blocks 先 fluids 后 = applyWorldEdits 序、observer 再 journal 化
  = 自相似循环、**不插 m_editedColumns** = fluid-only 列保持可卸载）→ 退出
  存档双路径 `mergeRetained`（不变式：每列至多在 journal 或 retained 之一 =
  无重复格，`--save-full-diff` oracle 不失真）；**随场景首触发修复 99.8
  活动门流送边界活性缺口**（冷流前对 void 被拒后目标列到达不唤醒 = 级联永久
  卡边；修 = insertChunk/getOrCreateChunk 的 wakeFluidChunks 镜像 setBlock
  UNSTICK，唤醒仅在插入事件 = 性能面零回退）
- **worker 网格化 `[PIPE-1]`**：mesher 计算搬 **4 worker** 快照 job（每帧至多
  2 个列快照 job：目标列 **3×3 邻域 9 列 × 8 chunk** memcpy 进 job 私有临时
  World，worker 用**零改动的 ChunkMesher** 出 8 chunk 几何；次帧 flush 后主
  线程 move 进 m_chunkMeshData + 清脏 + **列合并 GL 上传**【GL 上下文只在主
  线程，~1–3 ms/列 = 不可搬走部分】）；选择规则 = 脏列投影 ∩ 非 unlit ∩ 距
  当帧 light-pending 列 Chebyshev ≥2（光 footprint 会改本帧快照 = 保持脏下帧
  入选）∩ 非在途，最近优先；帧时序 pop-in 晚 1 帧（帧锁定确定）；动机 =
  vd80 step-0 实测主线程 remesh 填充期 7–11.3 ms/帧 + 2.8 min 拖尾
- **lit pop-in `[99.4]`**：新列集成后进 `m_unlitColumns`，光照 worker job 完成
  前**不网格化**（chunk 无 mesh = 不可见，等同 void），job 完成后一次性网格为
  正确光照——消灭「首帧光 0 + 二次网格」黑面（每新列只 mesh 一次，工作量减半），
  帧锁定确定性 pop-in
- **渲染预算 `[99.9]`**（vd80 旧管线 2–30 FPS 坍缩三根因修复）：**视锥裁剪**
  （列级保守 AABB）+ **列合并 VBO**（draw ÷8）+ **光照 chunk 级重构**（每 16³
  块一次查找 + BFS last-chunk 缓存 + 既有地形重扫零写入跳过天空 BFS——
  新地形必须强制传播，`newTerrain` 参数）；实测 vd80 流送中 render 37 →
  1.5–2.5 ms/帧（不随环增长）、flush 25–30 → 13–17 ms、主线程 CPU 100% →
  40%（4 worker 各 16–23%）
- **流体 per-cell 活动 `[PIPE-3]`**（见「流体」节）：tick = O(活跃 chunk +
  活流体格)，vd80 流体段 −50%、活跃 tick 2.8×
- **死亡暂停 `[P33]`**：`streamChunks()` 在 `isDead()` 时提前返回（在迟滞逻辑
  之前，激活状态保留）——不暂停的话死亡画面背后会按 view_distance 继续填满
  半径盘（vd80 ≈ 12853 列 / 数 GB / 数小时）；在飞的 ≤1 列经 post→flush 窗口
  自然排空，`postLightJobs` 照常照亮（否则该列停在光 0）；`R` 重生后从保留的
  迟滞状态继续
- **放置到未生成区域**：同步生成该列（放置当帧生效，一列 ≈1.6 ms 生成尖峰），
  流式管线随后按正常路径补光照
- **读档点在演示区外**：触发器对「玩家自身列缺失」= 距离 0 立即激活，~2–4 帧
  补上，期间玩家从存档位置先下落几帧（实测 ~0.03 格，逐位精确落定；无死亡
  风险，远小于 −32 杀线）

## 物理与碰撞

- **玩家 AABB**：0.6×1.8（**position = 底面中心**，硬规则 6；重叠判定**严格
  不等式**无 epsilon）；**轴分离解析 X→Z→Y**（撞墙该轴停住、另一水平轴继续
  = 滑动；贴面精确解析无抖动）；**子步化防穿透**（每子步 ≤0.4 块、≤64
  子步，硬规则 6）；重力 32、终端速度 38、跳跃初速 8.5 → **跳高 ≈1.129 块**
  （可跳上 1 格台阶，跳不上 2 格）
- **跳跃冲量计入子步 maxSpeed `[P99]`**：pending 跳跃（onGround + Space +
  非流体）时子步 maxSpeed ≥ jumpSpeed——原长帧（流送卡顿 86 ms）单子步整帧
  跳速位移使顶点膨胀到 ~1.65 翻过 1.5 barrier = 帧率依赖物理（P99 物理加固
  ×2 之一）
- **移动惯性 `[ENT-7]`**（消费 Phase-5 记档「松键即停」）：地面/空中**指数
  趋近模型**（fps 无关 `exp(−k·dt)` 真实 dt）：有 wish `v += (dir×S − v) ×
  (1 − e^{−k·dt})`（地面 `kGroundAccelRate` 8.0 敏捷起步 / 空中
  `kAirSteerRate` 2.0 弱转向）；无 wish `v ×= e^{−k·dt}`（地面
  `kGroundDecelRate` 12.0 = MC 0.5448/tick 摩擦档，4.3 walk 松键滑 ~0.36 块 /
  空中 `kAirDecelRate` 1.9 = MC 0.91/tick 拖曳档，松键滑翔 ~2.3 块）；
  `kVelocityStopEps` 0.001 **恰好 0** = 静止态确定性红线；**FLUID-2 档保持
  瞬置**（流体模型独立调参 = 记档偏离）；mob 侧范围外（AI 每帧设精确速度 =
  既有设计）
- **1 格自动台阶（step-up）`[ENT-3]`**：非潜行 + 贴地 + 身体贴住整数块面
  （1e-4）+ 面前脚位 **`solidRange.top ≤ 1.0`**（`[P99]` 加固 ×2 之另一：原格
  solid 属性判定会踏上墙/栅栏 1.5 实心区的帽 = 翻越隐患）+ 其上方 2 格为空 →
  自动抬升 1 格并继续本 tick 水平移动（slab 0.5/满块 1.0/glass_pane 可踏、
  墙/栅栏 1.5 不可踏、楼梯脚位点采样；**`[V2-17]`** 顶 slab / 双 slab 实心区
  顶 = 1.0 同款可踏——抬升后脚贴顶 slab 顶面 y+1 整数脚高位级不变）；判定
  **纯几何上提 `Entity::stepUp`，player 与 mob 同一函数**（player 行为逐位
  不变 = test_player 零改动红线）；2 格仍须跳跃。**P16 两个潜伏 quirk
  `[V2-9]` 修复（v2.9.0）**（`[ENT-3]` 探针实证，player/mob 共享）：
  ① +X/+Z 方向恰 1 格宽台阶踏不过（front 格查 snapped+1 不对称）→ 修 =
  `front = snapped + (w > 0 ? 0 : −1)`（+dir 查贴住格本身，−dir 不变）；
  ② 非整数脚高（slab 顶/楼梯斜面）face 巧合对齐整数可触发凭空抬升 1.0 再
  落回 → 修 = **无状态真阻挡验**（front 列在身体 y 跨内 ∃ solid 格且顶 >
  feet 严格不等式，moveAxis 同款行；零新帧状态/不改调用点）；**整数脚高
  位级不变红线**（既有 step-up 单测族零改动全过）
- **barrier 块**：wall/fence `solidRange` **{0, 1.5}** = 不能穿过（AABB 与实心
  区 [y, y+1.5] 重叠）也不能跳过（1.129 < 1.5）；**帽伸进上方格**：
  `hasRoomAt`/`moveAxis` 的 y 扫描扩到 y0−1（AABB 在 y+1 格时查 y 格的帽；
  非墙格 top ≤1 为 no-op backward-compat）；glass_pane = 整格实心 **1.0 可跳**
- **斜面/区间碰撞**（楼梯 `[P87]`/`[P89]`；`[V2-17]` 扩 solidRange 区间）：
  `solidRange(id, mn, mx, cell)` 返 {bottom, top} 实心区区间——楼梯按
  facing 沿对应轴取 **AABB 覆盖的最高斜面点**（north = 1−mnLocalZ / south =
  mxLocalZ / east = mxLocalX / west = 1−mnLocalX——`[P89]` 修正低端边切入高端
  bug，top 现值位级）；slab state 0 = {0, 0.5}（现值）、state 1 = {0.5, 1}、
  state 2 = {0, 1}、无状态 slab（床）= {0, 0.5} 位级；满块 = {0, 1}（忽略
  AABB，backward-compat——pre-V2-17 形状全 bottom = 0）
- **坠落伤害**（`[P16]`/`[P36]`）：`max(0, 净下落格数 − fall_damage_offset)`
  （offset 代码默认 **3** = MC「3 格安全」，config 可覆盖），从离地帧记录
  `fallStartY` 到落地结算；**落入流体的落地取消伤害**（落地格 fresh 流体检查）；
  **读档经 `restorePosition` 重钉 `fallStartY`**（`[P36]`：空中读档没有「离地」
  转换，不重钉则保存的坠落满血落地）；掉出世界 **y < −32 = 直接死亡**
  （`damage(maxHealth×1000)`，走死亡/重生流程，背包保留 = keep-inventory
  记档）
- **Mob 坠落伤害 `[ENT-5]`**：落地块镜像玩家规则（净落差 > 3 且落地格 fresh
  无流体 → `damage()` 既有死亡/掉落管线）；mobs ephemeral = 构造一次钉
  fallStartY + 离地转换重钉（1 子步滞后 = 确定性紧公差）
- **灵魂沙减速 `[V3-2 Part 2]`（v3.7.0）**：块可选字段 `walk_speed_factor`
  （严格 (0,1] 解析，非数/越界/字符串点名拒；shipped 仅 **soul_sand
  0.4** = MC 同款刻度）：双 feet 格中任一在该块 = 取**最小**因子 × 水平速度
  （**仅步行/疾跑**——流体档/潜行/飞行既有门不变）；无此块 = ×1.0f 位级恒等
  （门 197 S2 步行比值 0.403 实证：soul sand 3.0 s W-hold 位移 4.321 vs
  普通 10.712）；玩家/mob 共享 `Entity::integrate` 路径，下界 strider
  熔岩面行走 = 同旗豁免（见 MECHANICS.md「实体与 Mob」）
- **流体交互 `[ENT-4]`/`[FLUID-2]`/`[FLUID-3]`**（玩家/ mob / 掉落物共享
  `Entity::refreshFluidState` + `integrateFluid`，`isRestingOnSolid` 纯上提）：
  inFluid = AABB **严格重叠**水或熔岩格（玩家 `inWater()/inLava()` 同款并集，
  水/熔岩并存时熔岩优先）；`W'` = 脚中心列最高 L8+1 真水面上扫（`[FLUID-3]`
  浸没体改真水面上扫，干身读逐位恒等）；**atSurface** = 脚面离 W' 0.4 带
  （kind 记忆：漂浮终态脚面离水由该带维持——否则熔岩漂浮终态塌）；
  优先级 = **浮漂 snap 不动点**（fps 无关；`[FLUID-3]` 浮漂终态 = **游泳位
  W'−h/2** 半浸没 = MC 游泳观感，原「脚面 = 水面干身踩水皮」= 用户报障
  「站在水上」已修）> **被动上浮**（buoyancy 12、帽 1.5、vy ≥ 0 门【带动量
  入水沉底 = MC dive 语义】+ 池底站立 fresh resting 门 + 流膜无面门【纯流动
  膜无 L8 支撑 = 无浮面无上浮，Phase-9 下沉】）> **下沉帽** 4（**流体中零
  重力**）；`[FLUID-3]` 下沉/上浮 **inFluid 门**（水面以上 0.4 干带 keep vy =
  全速过带，悬崖坠水「降落伞」修）+ **深度门**（池深 ≥2 的 resting 体上浮出
  底，1 格浅池站底不变 = E4 主线）。玩家数值（`player.*` config 可覆盖，代码
  默认 = 下表）：水 swim 3.1 / Space 上浮 4.5 / buoyancy 12 / 被动上浮帽 1.5 /
  潜行下潜帽 1.5 / 终端 4；**熔岩**（`[P54]`，比值镜像 MC 运动系数
  0.014/0.0231 ~0.6 + 弱浮力）swim **1.9** / 上浮 1.5 / gravity 4 / 终端 2 +
  **灼烧 4 HP/1.0s real-dt**（`lava_damage`/`lava_damage_interval_seconds`，
  走正常伤害路径；离开熔岩重置未到期直伤 tick——残留燃烧是 `[P57]` fire 效果，
  另一系统）+ 落入熔岩**取消坠落伤害** + 熔岩内抑制疾跑/下蹲；浅水（眼在
  水面上）= 蹚水走/换气（`[SURV-2]`），深水游。**Mob**：AI 状态机零改动 +
  涉水/水下水平 ×`kUnderwaterSpeedFactor` **0.6**（命名常量，AI 与击退同门，
  无 per-mob 字段 = YAGNI 记档）+ **`[FLUID-3]` 踏阶 `!m_inFluid` 门删除**
  （涉水 mob 可踏 1 格岸壁出池 = 玩家对齐；游泳位 mob onGround=false 仍无
  step-up = 无 swim-up 爬岸记档偏离）+ 站立 mob 不上浮（同 MC）；**掉落物**：
  同款浮力（预置静水浮面静止无漂移【摩擦保留】+ 带动量坠水沉底站立互斥）+
  **熔岩对称同浮力**（记档偏离）；**箭穿过水不动**（投射物边界，记档）
- **实体分离 `[P19]`**：每帧分离 pass（固定顺序：玩家先、再实体列表按索引；
  中心精确重合时确定性 tie-break）——重叠对沿**最小穿透轴**分离（两个可推动
  体 50/50、对不可推动体走全量面到面跨度；体只移动到其 AABB 可占据的位置
  `hasRoomAt`，被 solid 挡住的一侧份额由另一侧全量接管，两侧都挡则跳过该
  对）；重复 sweep 直到最大位移 <1e-6 块（亚视觉残差）或 32 次硬上限；
  参与/可推动为**数据位**（`collidesWithEntities`/`pushableByEntities`，不按
  类型硬编码）：投射物不参与（箭穿过实体）、**玩家不可推动**（mob 在玩家脸前
  停下）；玩家 ↔ 掉落物分离（`[P16]` 起源）= 沿最小穿透轴把物品推开（远离
  玩家中心），玩家自身不被动位移
- **出生点**（硬规则 10）：演示区 4×4 chunk 列内按 `kSpawnCandidates` **10 个
  候选列**（原点偏移 0/±4/±8 块）顺序找**干燥陆地**（h > seaLevel 且出生
  AABB 内无 solid = `hasRoomAt`，避开树干/树冠/流体）；**世界出生点**在读档前
  捕获 = 敌对保护环锚（读档不改变它，MC 语义；`[V2-13]` 床验床回退锚 =
  本点）

## 光照

- **双光场 0–15**：每 chunk 两个 16³ 字节数组（sky/block）。**天空光** = 列顶
  扫描（最高不穿透格上方 = 15，其下每层扣该格 `light_opacity`）+ BFS 传播；
  **方块光** = `properties.light_emission` 源格 BFS（同款进入格成本）；破坏/
  放置后只重传播受影响区域（天空光 = 变化点 ±16 **整列**——垂直天空光无距离
  限制；方块光 = ±16 盒），光值实际变化的 chunk 才重建 mesh
- **`light_opacity` `[LGT-2]`**（0–15 字段；**缺省由 opaque 派生** = opaque 15
  不穿透 / 非 opaque 0——未设内容逐位复现旧行为；统一**进入格成本**：流体
  **3**（各方向），否则垂直天空 = O、其余方向 = max(1, O)，O = 15 不穿透；
  emission 源格自身持值不受影响）。shipped 赋值：**leaves 族 2 / glass 1 /
  流体 3（固定，不读块字段）/ slab·stairs×4·wall·fence = 0**（`[LGT-5]` 半块
  透光 = MC 半块规则：光如空气穿过，竖直天空免费 + 每格成本 1 = 与空气逐值
  同，**零核心代码**；opaque 保持 true = 剔面/渲染零变更）
- **昼夜 = 天空光场仿射撤退 `[LGT-3]`→`[UX-1]`（零重传播）**：天空光场**恒按
  正午（源等级 15）存储**；有效源等级 **`S = L + (15−L)×dayFactor`**（`[UX-1]`
  起 **L = `render.night_sky_level`** int [0,15] **代码默认 7** = 对 v1.34.0 位
  级恒等；7 = `kNightSkyLevel` = MC 午夜露天月光——10-27 用户实机定案「深夜须
  可夜视」，初版全黑被推翻；**L 0 = 全黑夜**【环境底随地板归零 = 真纯黑无残
  光】/ L 15 = 夜同昼不生成敌对 = 用户配置选择；严格读：absent = 7 / 非
  number / 非有限 / 越界 / 非整数全点名拒，检查序 = 非有限 → 范围（double 域）
  → 整数 → int 投影【1e18 int cast = UB 路径】= SAVE-1 纪律，config.json 零改
  动）。所有天空光源共享源等级 S 且逐格成本与 S 无关 ⇒
  **`field_S(x) = max(0, stored(x) − (15−S))` 逐格精确**（confluence 单测 =
  源等级 S 直接重传播 vs 仿射逐格相等）——日落时亮区沿 `stored = 15−S`
  等值线向洞口/冠隙收缩（MC 同款撤退观感），零重传播、分数 Δ 平滑（MC 量化
  超集）。**主机纯函数尾参参数化**（`effectiveSkyLight(stored, df, nightLevel)`
  / `currentSpawnLight` / `findMobSpawnCell`，默认参 = `kNightSkyLevel` 既有调用
  点位级不变）+ **生成光门与渲染求值同一公式**（所见即所生）+ **spawn 成对语
  义任意 L 成立**（夜露天等级 = L 本身：L≤7 敌对照生 / 火把圈 14 恒免疫 /
  L=15 夜不生成）
- **面烘焙光公式**（basic.frag，归一化 0..1，硬规则 9）：`light = max(
  v_skyLight − u_nightRetreat×(1 − dayFactor), v_blockLight)`（`u_nightRetreat
  = (15−L)/15`，L = `[UX-1]` `render.night_sky_level` 默认 7 = 8/15 旧常量位级
  恒等；init bind→set→unbind 一次性注入 = 每 run 常量无 per-draw 变化）+
  **`[UX-9c]` 全遮挡底光**：`amb = light ≤ 0.001 ? u_caveAmbient : u_ambient`
  （**全遮挡区** = 洞穴/深水/室内烘焙光恒 0 的**底值单独可调**——`render.
  cave_ambient` double [0,1] **代码默认 0.03** = `kAmbientFloor` 单一真源、对
  v1.49.0 位级恒等；深水衰减到 0 自动同享零额外代码；夜晚露天 light > 0.001
  不触 = **与 night_sky_level 两键正交**；严格读 = UX-1 同范式 [0,1] 越界/非数
  全点名，config.json 零改动）+ `rgb = base × (amb + (1 − amb) × light ×
  faceShade)`（amb = 0.03 时 = 旧 `0.03 + 0.97×light×faceShade` 位级同式）；
  **faceShade** = 顶 1.0 / ±X 0.8 / ±Z 0.6 / 底
  0.5；环境底全暗区与虚空背景可区分；每面采样**面后方一格**的光值；
  **方块光不受昼夜影响**（glowstone 夜间也亮，MC 一致）
- **AO 环境光遮蔽 `[V2-18]`（v2.19.0）**：经典 MC 顶点 AO，**bake 入既有
  skyLight/blockLight 顶点 attribute**（mesh 期 CPU 计算：顶点光 = 面光 ×
  该顶点 AO 系数——fragment 公式零改动，AO 自然作用 sky/block 两路；**零
  shader/零顶点格式（15 float）/零新 uniform**）。每面顶点 3 邻格查询
  （**viewer 侧 = C+n 基**：side1 = V+e1 / side2 = V+e2 / corner =
  V+e1+e2；e 向 = **背离网格单元于顶点**——三检查格 = 与顶点共享边、非
  viewer 格的前方三格，MC 经典观感：台阶竖面顶沿被上台阶遮暗/凹槽角最
  暗/抬升地板边沿暗角）；**遮挡判据 = opaque ∧ 高度带（闭区间含端点）**
  （与剔面同 `opaque` 旗 = 项目自洽规则）：满块/stairs/wall/fence → [0,1]
  （**偏离 2**：stairs 斜面不精化 = 高沿过遮）、slab state 0/1/≥2 →
  [0,0.5]/[0.5,1]/[0,1]（= `Entity::solidRange` 同款 kSlabHalf 常量；slab
  遮挡体状态读 = emitSlab 同款 unset→stateDefault）、air/越界/非 opaque →
  不遮；**半块同款 MC「顶点只被同高度邻格遮」自动成立**（底 slab 顶面
  中平面 y+0.5 查上行 yInRow −0.5 恒不遮；顶 slab 顶面行上 yInRow 0 正常
  遮；接缝 0.5 对 [0,0.5] 与 [0.5,1] 同遮 = 含端点定案）。**4 档系数 =
  MC 精确值**：side1∧side2 → **0.3**（双邻特例，`kAoCorner`——计数会说
  2）；否则 1.0−0.25×(s1+s2+corner) → {**1.0, 0.75, 0.5**}（`kAoFull/
  One/Two`）。**smooth = GPU 双线性**（顶点系数 rasterizer 插值）；**偏
  离 1**：MC 的 4abcd 中心修正项省略（attribute 模型不可重构 fragment 4
  顶点原值；夜间 LGT-3 retreat 作用于 AO 后 sky = 暗角夜间更暗，曲线微差
  同条记档）。**生效面**：greedy 满块（**MaskCell 合并键扩 ao[4]** =
  uint8 档位精确比较非浮点：共享边角 AO 不等 = run 断开【MC greedy+AO 同
  款】；merged quad 顶点 i 取 run 端 cell 的 corner i = seed.ao[i]——键强
  制 run 内 ao[4] 全等；ao[4] 序 = 面表 c0..c3 = 1×1 run 顶点发射序，单
  测钉）+ slab 三态（发射 y 判带）+ stairs（顶点实际 y；**斜面 = +Y 面
  规则**：上行查询，高沿 yInRow 0 可遮/低沿 −1 恒不遮 = 台阶 run 斜面恒
  亮、竖面顶沿暗）；**fluid/cross 免 AO（MC 精确：water surfaces 不受
  smooth lighting 影响）+ wall/fence/glassPane 免 AO（偏离 4 薄几何范围
  外）** = 豁免 6 emitter 顶点写点零触碰。**`render.ao` 严格 bool 默认
  true**（weatherEnabled 范式：absent = true config.json 不动 / 非 bool
  点名拒）：**off = 查询不跑 + 系数恒 1.0 + ao[4] 恒零 = pre-V2-19 位级
  恒等**（fixer 自证：off 60f 截图 ≡ 旧 v2.18.0 二进制同环境逐字节）。
  AO 成本 = mesh 期（worker 纯快照函数不变：查询 ⊆ 3×3 列快照零新依赖；
  greedy run 断裂 → quad/顶点数增 = render 成本，性能 A/B mesh 期口径）。
  详见 `docs/archive/v2x/phase-2.19.0.md`
- **per-draw 光覆盖 `[PRE-6]`/`[LGT-4]`**（vert 端唯一门控分支，frag 零改动 =
  昼夜缩放/环境底/雾/波纹自动继承；**draw 后强制 rebind 复位** = per-draw 状态
  泄漏纪律 `[UXFIX-1]`）：**掉落物** = `itemDropLight` 纯函数采样 **AABB
  中心格**（position 底面 + height/2，逐轴 floor 负坐标安全）sky/block ÷ 15，
  **每帧采样无缓存**（光照变化自然跟随，2 world 读/掉落/帧可忽略）；**信标
  光束** = `beamLightSample` 采样**信标块上方空气格 (x, y+1, z)**（= beam
  最低格；首版「块自身格 opaque 恒 (0,0)」缺陷被门 119 E2E 揪出修复，
  OwnCellOpaqueAlwaysIgnored 回归单测钉死）——深夜月光 7/15（LGT-3 仿射
  继承）/ 近萤石亮化 / 白天恒白（df = 1 恒等根）；**XP 球 `[V2-11]`** =
  `itemDropLight` 同款纯函数采样 AABB 中心格（同族 per-draw 覆盖——`[V2-18]`
  AO 覆盖面豁免 = 偏离 3，MC 光束亦不 AO 同款）
- **并行与异步 `[99.10]`/`[99.11]`**：流送光照 job 与流体追赶光照 job **各至多
  2 个/帧**（同窗内两两位级独立判据 `lightRegionsIndependent` = 写 reach ±1
  列 + 块光 BFS 读环 +1 列双向不相交【gap ≥4 任轴】纯函数，依赖对回退单 job
  顺序 = pre-99.10 逐位行为）；净变 chunk 走 job-local `dirtyOut`（解共享写
  竞态），flush 后主线程统一 markChunkDirty → 列重烘；**流体光异步化
  `[99.11]`**：流体 tick changed 格 → **列粒度**待集 `m_fluidLightPending`
  （同列合并）+ 每帧 ≤`kFluidLightColumnsPerFrame` **2 列** worker 追赶
  （`newTerrain=true` 强制天空传播 = 流体洞修正）+ **幂等收敛**（排空 ==
  propagateAll，跨类同窗位级独立排除）；vd16 填充 244.7 → 44.6 s（5.5×）、
  update p95 7.6 s → 80 ms
- **sky rescan clobber `[LGT-1]` 潜伏 bug → `[V2-8]` 修复（v2.8.0）**：天花板
  板骑跨光照 footprint 边界时顺序两次 region 传播后一 rescan 把边界格 sky
  覆写为纯垂直值，1–2 格宽 stale-dark/stale-partial 暗带（`[LGT-1]` 设计时
  发现、暂留；`[V2-8]` 定向消除 = **sky BFS 种子盒 +1 块 shell 环**——
  rescan 仍 footprint-only、BFS 写界/no-op skip/propagateAll/块光段零改动，
  shell 格持收敛值 = 恢复边界条件，无 clobber 时位级不变；独立性判据零改
  动 = confluence 原样成立，详见 `docs/archive/v2x/phase-2.8.0.md`）
- **光源**（`light_emission`）：glowstone 15（hotbar 内）/ lava（流体参考块）
  15 / fire 15 / campfire 15 / torch **14**（MC 值）/ **nether portal 11
  `[V3-1]`** / **magma_block 3 `[V3-2 Part 2]`**（下界光景观 = 熔岩海自发光
  15 + magma 3 + glowstone 天花板 + portal 11；strider `spawn_max_light` 15
  与熔岩自发光配对，见 MECHANICS.md「实体与 Mob」）/ campfire_extinguished 0 /
  beacon 0（光束不提供方块光 = 不照亮周围，`[P83]` 记档；`[LGT-4]` 后光束
  自身按上方格受光）

## 流体

- **编码**：每 chunk 每种类 0–8 level 字节数组（8 = 源/满，1–7 = 流动；同一
  cell 水与方块互斥）：干 0 / **水 1..8 / 熔岩 9..16 / 蜂蜜 17..24**（`[FLUID-6]`
  三层化，水/熔岩带位级不变）；`setFluid` 正 level **替换异种**、0 只清本种
- **节奏**：`FluidSim::tick` 游戏线程**每 3 帧**（`kFluidTickEveryNFrames` 3，
  ≈20 Hz）快照式确定性（无输入运行逐字节可复现）
- **模拟规则**（水/熔岩/蜂蜜各自快照、按种过滤；**异种流体格按「方块」处理**
  = 支撑 + 挡扩散，防惰性配置跨种替换振荡）：下落（下方空/流动则沿列传送带下
  移 = 成对移动净零；链达世界底部 y<0 排出 = **δ** 记账；满格失撑也下落 = 对
  MC 刻意偏离）/ 静止水平扩散 L−1（1 级终端；扩散前缘创生/抬升浅流 = **σ**
  记账，冲突目标取 max）/ **统一池规则 `[FLUID-1]` 三分支**（静止在同种流体
  level B 上：① B==8 → 本格拉满到 8（**κ**：source 托底的柱是满的——破坏湖底
  后整列补满）② 0<B<8 且 L<B → 本格拉入池（**α**：池水位不动）③ L≥B →
  **静态沉积**（本格保留、零写入——`[FLUID-1]` 前「把池抬升到 L」分支删除 =
  唯一无源体积增益路径））。**守恒 = 逐 tick 会计恒等式** ΔF = σ−α−κ−δ
  （「总量 ±0」是错误目标——MC 流前缘创生邻居合法；封闭盒 64 格精确终态表 +
  P90 表面湖精确终态计数单测逐 tick 钉死；`[FLUID-1]` 路线 A = 只删抬升分支，
  静态沉积稳压器保留 = churn 回旧水位）
- **反应 pass `[P51]`**（tick 第一步）：快照扫所有熔岩格：6-邻域任有**水** →
  该熔岩格 `setBlock` 为熔岩块 `water_reaction` 的产物（`{source, flowing}`
  严格配对在**方块数据**上：邻水含 level 8 源 → **obsidian**，否则 →
  **stone**；**水不消耗、无蒸汽** = v1 记档偏离；无 `water_reaction` 的惰性
  熔岩不反应可邻水共存）；稳态不变式 = 无 6-邻接的水-熔岩对（反应必收敛出
  石头/黑曜石墙）。正面争夺同一空格 = apply 顺序（水先熔岩后 = 熔岩胜出）是
  确定性实现细节非数据规则
- **燃烧 pass `[P58]`**（tick 第二步）：快照扫所有熔岩格 6-邻 **BLOCK** 格
  `flamable: true` → 收集排序后 `setBlock`（`[P64]` 起 = **fire 块**，**无
  掉落**；注册表无 fire 块回落 air = 单测 fixture 语义）；无 flamable 块 =
  fast-exit
- **蜂蜜 `[FLUID-6]`**：第三流体种类 = **粘度帧锁定 tick 门控**（每 3 个流体
  tick 模拟一次 = `kHoneyTickPeriodFluidTicks` 3，水/熔岩零改动）；**无波纹**
  （emitFluid 波纹标记仅 kind==Water）/ **无 worldgen**（桶源 = honey_bucket，
  无蜂箱记档）/ 无 `water_reaction` 字段 = 反应 pass 数据驱动惰性；蜜/水/熔岩
  6-邻共存互不替换
- **活动门控 `[PIPE-3]`**（99.8 chunk 粒度 → per-cell）：per-chunk per-kind
  流体格列表（setFluidAt 成员变化 push / drain lazy stale / walk 过滤 + 列序 +
  去重 + 压缩，不变式 = 活格 ⇒ ≥1 条目）+ World per-kind 活跃 chunk 集合（6
  插入点穷尽 flag 置位 + eraseChunk purge）+ falling 标志 per-column 化
  （跨 chunk topFlags 引用语义位级同旧）+ reaction/burn 活格扫 + propose map
  max 合并（序无关 = 位级同果）；**tick = O(活跃 chunk + 活流体格)**；流体
  状态**逐位不变**（门 117 位级一致 oracle）；vd80 流体段 −50%（1261 → 631
  ms/300f 窗）、活跃 tick 34.8 → 12.6 ms（2.8×）
- **光照**：流体格光 = 该种类参考块的 `light_emission`（熔岩 15 = 发光池）；
  天空光**每穿一格水 −3**（`[LGT-2]` 进入成本 3，列顶扫描 + BFS 与不透明方块
  共用传播器）——深水湖底随深度变暗（截图可见浅水亮/深水暗渐变）
- **稳态与残余**：生成世界的水（海平面以下填充）处于平衡态（20 Hz tick 不
  扰动湖面）；**残余 churn** = 站流水瀑布/崖膜永久轮换（24 格类：视图外、
  ×2 RAW 确定、`[99.8]` 活动门只保对应 chunk 热——接受现状，见
  KNOWN_ISSUES 流体节）；表面熔岩湖溢出/燃烧 = 确定性但非静止（见「世界
  生成 · 熔岩湖」）
- **`World::setFluid`** 只写已存在 chunk 的 air 格；`setBlock` 会把所在格的水
  排掉

## 火系统

- **fire 块 `[P64]`**：cross + 火焰纹理（atlas 空位免重锚）、`light_emission`
  **15**（MC 值）、hardness 0（瞬破）、**无 drops**（破坏/燃尽都不掉）、
  非 solid/opaque/**非 flamable**（火是燃烧终态）；**来源**：① 点燃器右键
  flamable 块 ② 熔岩燃烧 pass ③ **玩家放置**（fire 有隐式 block item）④
  蔓延
- **蔓延 `[P66]`**（`world/fire_spread` 纯函数，镜像生长引擎范式）：每
  `fire.spread_interval_frames` 帧（**代码默认 6** = 0.1 s MC tick 节奏，固定
  帧步 `m_frameCount % interval` 门控）每活体火对 **5 邻**（水平 4 + **上**；
  **不向下**——MC 地火不烧地板）的 `flamable` 格独立 roll（splitmix64(世界
  种子 + 帧×GOLDEN + 位置哈希 + 邻序×GOLDEN2) < `fire.spread_chance`（**代码
  默认 0.05** = MC normal））→ `setBlock(fire)`（收口注册满寿命 = 新火继续
  蔓延）+ 局部光照；一格邻多火 = 每火各 roll、**只点燃一次**（输出位置序
  去重）；无火/无 fire 块/无 flamable/chance 0 全 fast-exit；**无音效**
  （防刷屏，记档）
- **余烬粒子 `[P66]`**（`fire.particles` 代码默认开）：每 **6 帧**（
  `kFireParticleEveryNFrames`）每活体火发 **2 粒**（0.12 立方体、火焰纹理、
  **负重力 −2 上升**、寿命 0.3–0.5 s、种子 = 世界种子 ⊕ 位置哈希 ⊕ 帧
  推进）；**燃尽/浇灭**死亡爆发 16 粒（复用 spawnBreak）；发射 = 固定帧步
  （确定性）、运动 = real dt（活余烬截图不可字节比 = PITS 条款）
- **燃尽**：`Game::m_fires` map（位置 → 剩余秒，setBlock 收口创建/删除 =
  炉子范式）；**real-dt** 倒计时（生存计时器类，硬规则 8）；`fire.
  lifetime_seconds` **代码默认 4.0**（>0 硬校验）归零 → `setBlock(air)`
  （无炭无掉落）
- **浇灭**：fire 的 6-邻任一格有**水**（任一级）→ 当帧瞬灭；**水胜火**
  （`[P57]` 规则）；**熔岩不浇灭**（池边的火是熔岩的产物，继续烧——MC 同款
  共存）
- **点燃（玩家 / mob 同节奏，`[P57]`/`[P74]`/`[P73]`/`[P58]`）**：`fire` 效果
  （1 HP/0.2 s = 5 HP/s 可致死，min_health 0，`negative: false` = **牛奶不
  灭火** MC 语义）+ 两数据标志（`ignites_on_lava` 至多一个注册效果可带 +
  `extinguished_by_water`，init 交叉校验）。**玩家**：`updateEffects` 每帧
  （count==0 早退前）**inLava() || isInFire()**（fire 块 AABB 严格重叠 =
  `[P74]`，`[P70]` 直伤机制整体退役——点燃走 P57 既有路径）→ **重施加**
  （duration = `player.fire_duration_seconds` 代码默认 **8 s** = MC Normal；
  重施加 = duration 重置 + tick 相位保留 → 火源内持续燃烧）；**inWater() 瞬时
  灭火且先于 tick**（水内零伤害、水胜熔岩）；离火源后 **8 s 残留** countdown
  烧至到期（MC 核心行为）。**Mob `[P58]`/`[P73]`**：生成时 `armFire`（参数
  从点火效果 + config 同源取）；燃烧 = **整数帧燃烧格**（`m_fireFrames` int
  计数，`llround(interval×60)`——double 累加相位 off-by-one 的修正）：
  火源内每帧重置（相位不清零）、离源 8 s 残留、水瞬时熄灭先于 tick、**可致
  死**（到期帧不 tick = `[P46]` 语义）；init 交叉校验：点火效果
  `tick_interval × 60` 必须整帧数（容差 1e-4 帧）
- **`flamable` 集 `[P63]`**（可选 bool 属性，非 bool 点名拒）：**wheat_1..7
  全 7 阶 + wood/leaves/grass**（MC 天然可燃面）——**羊毛刻意不燃**（染色
  产物保留不可燃，精确集单测钉死，记档偏离）；高草/火把（torch 非 flamable）
  未实现
- **存档**：format 10 `fires` 数组（{pos, remaining} 位置序、remaining > 0、
  与块编辑**双向**交叉校验 = 炉子键纪律；format 9 读档 = 空 + 手编旧文件
  fire 块补满寿命条目）
- **init 交叉校验**：registry 有**任何** flamable 块则**必须**有 `fire` 块
  （燃烧目标缺失 = 点名拒绝）；零 flamable 资产集合法（燃烧路径全惰性，fire
  块可选）

## 下界维度（`[V3-1]`–`[V3-3]`，v3.4.0–v3.8.0）

- **双世界架构**（D1 增量并行成员）：overworld 代码路径零 diff；
  nether = `NetherDim` 聚合（World 惰性 + generator + StreamingState +
  网格/集族 + MobSystem【v3.8.0 起 = 5 物种池，见 MECHANICS.md「实体与 Mob」】
  + entities + portals 追踪集）+ `DimCtx`
  访问器；**首传前零 chunk/零 mesh/零流送/零内存**（结构免疫最强形）。
- **坐标映射 8:1**（MC 精确）：主世界 (x,y,z) → 下界 (floorDiv(x,8),
  y, floorDiv(z,8))，Y 不缩放；逆映射 ×8；派生 seed `worldSeed ^
  kSaltNether` **不落盘**（parse 重建 = 单源真值）。
- **下界 worldgen**（专属纯函数，坐标纯函数 = 零噪声；**D9 带修订
  v3.5.0 = 可航行空腔**，世界生成演进先例）：基岩底 y[0,4] /
  **熔岩海 y[5,31] 全源 level 8**（静态源海 = 流体零 churn，岩板下
  挖穿可见）/ **netherrack 台地 y[32,52]**（hardness 0.4 MC 精确，
  drops 自身，音族复用 stone 记档）/ **空气空间 y[53,122]**（70 格
  可航行空腔）/ 天花板 **glowstone per-cell 团 y=122**（复用既有
  glowstone 块 light 15）/ 基岩顶 y[123,127]。
- **下界群系族（v3.5.0）**：`biomes_nether/` 独立池（主世界池零触碰）
  5 群系 MC 比例权重：nether_wastes 36（台地基线）/ crimson_forest 22
  （crimson_nylium 表面 + crimson_stem 柱 4..9）/ soul_sand_valley 17
  （soul_sand 表面 depth 3 + soul_soil·magma_block per-cell 团 48-52）
  / basalt_deltas 16（basalt 柱 5..30 自 y53）/ warped_forest 9
  （warped_nylium + warped_stem）；`netherBiomeAt` = biomeIndexAt 克隆
  （valueNoise2 + 独立盐 + 派生 seed；装饰门控 = 数据派生旗，fill 零
  name switch）。
- **下界矿脉（v3.5.0）**：per-cell lattice 8（GEN-3 先例克隆，台地
  y[32,52] only——熔岩海/基岩/覆写格不放）：quartz_ore p 0.0376（drops
  quartz）/ nether_gold_ore p 0.0208（drops gold_ingot MC 精确）/
  ancient_debris p 0.0026（hardness 10 = MC 50 的 obsidian /5 缩放
  先例；带 = 台地【MC 深带 8-22 在全源熔岩海无承载 = 记档偏离】）。
- **下界块族（v3.5.0 + 深化 v3.7.0）**：12 新块全 stateless（atlas
  124→136 幂二增长零移位）：nether_brick 2.0（netherrack 熔炼 1:1 + 2×2
  自压缩配方）/ magma_block 1.5（**light 3 `[V3-2 Part 2]`**）/ quartz_ore
  3.0（掉 quartz 物品）/ nether_gold_ore 3.0（掉 gold_ingot MC 精
  确）/ **ancient_debris 10 = MC 50 的 obsidian /5 缩放先例**（熔炼 →
  netherite_scrap，见 MECHANICS.md「合成与容器 · netherite 材料链」）/ soul_sand·
  soul_soil 0.5（**soul_sand `walk_speed_factor` 0.4 `[V3-2 Part 2]`**）/
  crimson·warped nylium 0.4 / basalt 1.2 / crimson·warped stem 0.4
  （**drops 自身** = 记档偏离，MC 不掉落）；**sound_material 重指派全钉**
  （nether_portal → glass `[V3-1]`；`[V3-2 Part 2]` 13 下界块：nether
  3【netherrack/magma/debris】/ nylium 2 / stem 2 / sand 2【soul
  sand·soil】/ stone 4【nether_brick/双 ore/basalt】= 9 新 wav + 既有族
  复用）；quartz 物品 = 手工 JSON（
  ancient_debris 名字被块占用 = 块物品，手工文件构建性拒收，记
  档偏离 1）。
- **skyless 光照**：下界 World 的 sky 光恒 0（void/顶读 0 + rescan 顶
  源 0 = 2 处参数）——无天穹无昼夜；block light 规则同款（熔岩 15 /
  portal 11 / 火把）。ambient 保持 0.03（记档偏离：MC 0.1，Phase C）。
- **传送**（Phase A 倒计时完成触发，单帧帧锁定）：目标维 **±16 格搜
  活跃 portal**（含 Y 全高盒，最近 = 框底中距离；无 → **生成 4×5 含
  角框 + 2×3 基岩平台**，走 setBlock 咽喉点 = editedColumns 保护）→
  落点 = 目标 portal 内域底中心（hitbox 安全 + step-up 上探）→ 3×3
  列盘同步 catch-up → Player.setWorld 换绑 + restorePosition（fall
  pin）+ 相机 → 流送自动 engage。正交 portal（axis 不同）= **yaw
  −π/2**（MC JE 顺时针 90°；往返 = 180° 观感 quirk 自然继承）。
  **900 帧冷却**（15 s，会话态不入档——重载即清零）。音效：trigger
  （进入）+ travel（抵达）。
- **仅 Player 跨维**：`Entity::setWorld`（m_world 引用→指针，单咽喉
  点）；per-dimension 实体 list，inactive 维**冻结**（不 tick = 时间
  胶囊，DayCycle/Weather 全局恒推进 = MC 时间连续）；掉落物不跨维
  （记档偏离）。
- **渲染门控**（5 点，零 shader 改动）：暗红 clear 色（0.16,0.04,
  0.04）/ 日月星不画 / 云不画 / 雨不画 / 雾 10→96 块；F3 dimension
  行（19→20）。
- **下界行为收口**（记档偏离族）：下界死亡 = **主世界重生**（MC 精
  确：重生点恒主世界床/出生点）；7 per-block state 族主世界 only
  （furnace/campfire/table/crop/sapling/moisture/beacon/fire）；下界
  火恒定不烧尽；**水蒸发 `[V3-2 Part 2]`**（下界水桶泼出 = 桶回空桶 +
  "evaporate" 音 + 日志，**零格触碰**点击恒消耗——水无下界世界语义，
  维度感知 FluidSim 范围外；熔岩桶照常放置）；床睡眠 no-op（MC 床爆炸
  无 TNT 机制跳过）；容器面板主世界 only；**下界 Mob = 5 物种池
  `[V3-3]`**（见 MECHANICS.md「实体与 Mob」）；**ambience 第 4 上下文
  `[V3-2 Part 2]`**（nether 优先，见「音频」）；**灵魂沙减速
  `[V3-2 Part 2]`**（`walk_speed_factor` 0.4，见「物理与碰撞」）；
  **下界原点保护圈**（MC 下界无保护 = 记档偏离，门 198）。
- **存档 format 30**：`player.dimension` 严格词表恒写（29 缺 =
  overworld 迁移 / 29 携 = version lie 点名 / 未知词点名）+ `nether`
  段 {blocks, fluids, states} 恒写空合法 + `nether_entities`（item
  kind）恒写空合法；两维 portal 载入 rescan（孤立 → air）；nether 内
  stateful 块须 nether.states 条目（PITS 383① 孪生）；gzip 编码零
  改动（V3-7 容器自动覆盖）。
- **首访体验（D9 修订后）**：Y 映射落空气空间（53..122）= 悬浮
  portal 房间（框 + 基岩平台自足，落点安全 = 框底行 + 出框 1 格落台
  地顶无伤）；开阔空腔 + 群系表面远地平线 + stem/basalt 柱 + 天花板
  glowstone（门 195 截图观感实证）；熔岩海 = 向下 21 格（台地）可达。

## 末地维度（`[V3-10a]`，v3.23.0）

- **第三维度**（下界维度基建的**第二次消费**）：`EndDim` = NetherDim
  12 成员镜像聚合 + `Dimension::End` + `playerInEnd()`/`inOverworld()`
  + `dimensionOfWorld()`（setBlock 咽喉点指针判别三值化重做）；**首
  传前零 chunk/零 mesh/零流送/零内存**（结构免疫最强形，S1 ≡ base19
  实证）。
- **坐标 = 1:1 独立原点**（MC 精确，无 8:1 映射）；派生 seed
  `worldSeed ^ kSaltEnd` **不落盘**（parse 重建）。
- **末地 worldgen**（`end_gen` 纯函数，坐标纯函数——10a 零噪声，
  10b 的 lattice roll = hash 纯函数）：120×120 末地石台地
  x/z∈[−60,60] y∈[48,52]（feet 53）+ **原点出口门**（y53 12 格
  frame_eyed 环 + 3×3 end_portal，常活——10d 前偏离）+ 岛外虚空 +
  **10b 内容族**：基岩洋 = 全维 y[0,4] 实心带（下界地板镜像，非
  流体零 churn；跌落死因 Void→Fall + 珍珠逃生面）/ **岛群 pass**
  （lattice 16 p 0.15 独立盐 kSaltEndIsland，半径 5..12 顶 y 44..56，
  顶 2 层 + 下锥 2 层悬浮岛；air-only 谓词 + **中央足迹护栏**保出口
  /平台区零岛；跨 chunk 走格外扩 rMax）/ **chorus 岛顶装饰**（独立
  盐 kSaltEndChorus p 0.6，cross stateless + chance_drops ≈ MC 0-2
  果 + 0-1 株；生长不做 = 主世界 only 生长 tick 后移 10c/10d）/
  **biomes_end 单群系池**（end_plains surface endstone no-op 覆写；
  MC 无群系偏离 + 10d 第二群系零核心缝）。
- **末地块族（10a 最小面 + 10b 内容面，块 127→133 / atlas
  166→172 零移位）**：endstone 0.5（stone 音族）/ end_portal
  （transparent/solid false/light 11/destroyable false，
  nether_portal 逐字镜像，glass 音族）/ end_portal_frame +
  end_portal_frame_eyed（**A-lite 双 stateless 块**——项目无
  per-state 纹理机制，偏离 MC 单块 16 态；destroyable false =
  **无去激活路**嵌眼单调）/ **end_stone_bricks**（10b：hardness
  1.25 MC，stone 音族，2×2 endstone 配方 count 4 MC 精确）/
  **chorus_plant**（10b：cross stateless + flamable + drops
  chorus_fruit + chance_drops 果 0.5/株 0.33 ≈ MC）。
- **入口门（A-lite）**：5×5 水平环（四角缺 12 frame 格）body 层 +
  中心 3×3；**嵌眼交互**（持 `ender_eye` 右键 frame：吞 1 眼 + 嵌眼
  态 + 8 候选中心环扫描）= **12 全 eyed + 中心全 air → 中心激活
  end_portal**（纯函数族 `end_portal.{hpp,cpp}`：scan/activation
  plan/cellValid 孤儿谓词）；`ender_eye` 物品（32×32 独立 icon +
  单盒 model，item 98→99）。**frame 生存不可得**（creative 授予 +
  门手术；10b/10c 复议）。
- **传送**（**新函数** `updateEndPortalEntry`/`travelEnd`——
  travelPortal 原文零触碰位级红线）：OW→End 站入口 60f 倒计时（复
  用）→ 入场平台 **5×5 obsidian @(40,52,0) 每次传送写入**（上方 3 层
  清 air；首访 end 段 journal diff = 恰 25 格，二次幂等——偏离 MC
  (100,49,0)）→ 落点 (40.5,53,0.5) **朝西 yaw +π/2**（MC 精确）→
  3×3 盘 catch-up → setWorld 换绑 → 翻维；End→OW 站原点出口 → 返回
  **主世界出生点**（床校验先例，D8 三值化）。900f 冷却复用（共享）
  + 声音复用 portal_trigger/travel（零新 wav）+ 日志 "Traveled to
  end at (40, 53, 0)" / "Traveled to overworld (end exit)"。
- **渲染 6 点**：暗紫黑 clear (0.05,0.03,0.06) / 雾 8..48（fill
  clamp 复用）/ 日月星云雨 inOverworld 门 / F3 dimension +end /
  **ambience 第 5 上下文**（AmbienceContext::End=4 + ambience_end.
  wav，声音 67→68）/ **u_ambient + u_caveAmbient 双 floor 0.25**
  （MC ambient_light 精确；shader 无光分支只读 u_caveAmbient——单设
  = 全黑门内捕获；OW/nether 配置值位级不变；set 点 = init + 翻维帧）。
- **末地行为收口**：死亡 = **主世界重生**（D8 三值化，MC「末地只能
  死回去」）；虚空坠落 = kill plane −32 + 「死于世界之外」既有
  （10b 基岩洋后 = 洋底终结「死于高处掉落」）；7 per-block state
  族主世界 only（末地闭集同款）；**水蒸发谓词 nether-scoped = 末地
  水不蒸发**（偏离 MC，10b 复议）；床/容器/营火 inOverworld 门；
  **生成池空槽**（10c 两段式，mob_spawning_end.json 后相位）；流体
  pass = 静态零 churn 纪律（10a worldgen 无流体，桶正常流动）。
- **末影珍珠投掷 `[V3-10b]`**（项目首个投掷物，item 99→101）：
  `throws_teleport` strict bool + throw_sound/teleport_sound 配对
  （init 交叉检：缺一 = 死字段点名 / wav 缺失点名）+ one-right-
  click 8 面互斥（food/drink/launcher/ignite/dye/bucket/shears/
  armor）+ material-only；挂点 = RMB 链潜行旋转后 placeBlock 前
  （MC 可交互块优先）；发射 = 眼位前 0.3 + **投掷者速度并入**
  （MC 1.9+）+ kPearlLaunchSpeed **30 b/s（MC 1.5 b/t 绝对值；箭的
  1/4 比例 = 7.5 仅 3 块弃）** + 冷却 60f 会话态（窗内再点 = 静默
  no-op 吞点击）+ creative 不消耗；Projectile 尾参 `teleports`
  （缺省 false = 箭/火球位级）+ stuck 复用 → **pearlLanding**：
  catchUpDisk 先行 + 落点 **2.5 球面均匀偏移**（particle counter
  流 2 抽帧锁定）+ hasRoomAt 上探 3（portal 先例）+ restorePosition
  **fall 复锚**（落地消坠伤）+ 落乘 + **无自伤**（MC 5HP 偏离）+
  probe 全败吞没不瞬移（偏离）；mob 扫掠跳过（穿 mob MC 孪生）+
  0.625× 青色发光立方渲染（火球 mesh 复用）+ 2 wav（68→70：
  throw whoop 320→980 Hz / teleport shimmer 1400→240 Hz）。
- **存档 format 32**：`player.dimension` 词表 **按 format 分词**
  （30/31 = overworld|nether 二词，携 "end" = version lie 点名；≥32
  = 三词）+ `end` 段 {blocks, fluids, states} 恒写空合法 +
  `end_entities`（item kind，XP 拒收）恒写空合法；pre-32 载入迁
  end 段空 dimension 不变；**末地内 stateful 块须 end.states 条目**
  （PITS 383 双向检）；入口环（OW 段）+ 出口环（end 段）载入
  rescan（孤儿 → air）；gzip 容器自动覆盖。

## 下界传送门（`[V3-1]` Phase A，v3.2.0；传送 = Phase B）

- **框**：竖直平面 obsidian 环，外廓 **4×5 .. 23×23**（长边 [4,23] ×
  高 [5,23]；"躺式" 5×4 拒 = 高度恒 ≥5，MC 精确）；**四角可选**（角
  任意内容环仍完整——玩家可省 4 块）；水平环不存在。判定 = 事件触发
  有界扫描（事件格两竖直平面各 45×45 窗口 + 前缀和 O(1) 候选判定，
  `core/portal` 纯函数族，<1 ms 级）。
- **激活**：完整框 ∧ 内有活 fire ∧ 内无 portal → 内 air+fire 格全部
  转 `nether_portal`（**state = 框 axis**：0 = 长边沿 X（z=const
  平面）/ 1 = 沿 Z；ENT-2 机制，走 format-15 states 段）；fire 格被
  替换（MC "one of them replacing the fire"）；bbox 一次重光（portal
  **light 11**）；info 日志。obsidian 与 fire 两事件都查（**偏离**：
  wiki "火须最后放" quirk 不复现——事件驱动模型下"最后"无意义，双向
  检查确定性更强）。**点火几何**（PITS 386①）：框内 air 与环共面 =
  射线不可直达——打火石命中环格、**命中面法向的相邻格**是框内 air 才
  点火（aim 环格朝框内的面）。
- **去激活**：非角环格脱离 obsidian（破坏/替换）→ 内 portal 全回 air +
  重光 + 日志；**角格破坏保留 portal**（MC 精确）；**fire 熄灭 ≠
  去激活**（portal 自持——fire 在内域非环）。
- **站入**：玩家 AABB 与 portal 格重叠 → **60 帧（1 s）帧锁定倒计时**
  （硬规则 8；MC 80-tick 生存延迟的帧锁定对应值，10~80 tick 史值区间
  内）；走出即取消；无冷却重计（Phase A 语义钉）；**GUI 互斥**：倒计时
  中开面板自动关（MC 传送门规则；closePanel 既有语义 = 指针释放，
  click 重捕获 = 项目既有 UX）；完成 = **真传送**（`[V3-1]` Phase B v3.4.0，管线见「下界维度 ·
  传送」）：日志 "Traveled to nether at (x, y, z)"（**8:1 映射**
  floorDiv，负坐标安全）+ Player 换绑 + 900 帧冷却。
- **块**：`nether_portal` = full-cell 半透盒（lava 先例）/ solid·opaque
  false / **destroyable false**（生存不可破）/ 无 drops / sound glass /
  light 11 / state_count 2（axis）。紫罗兰漩涡纹理（atlas [13,6]）+
  portal_ambient 氛围音（32 块可视域，per-frame 确定性 roll p=1/120，
  蝙蝠 squeak 节奏先例）。
- **载入 rescan**：孤立 portal 块（不严格位于完整框内域 = 手改档）→
  init 置 air（MC 更新语义 + 双向交叉检纪律）+ 追踪集重建 + 一次性
  重光。
- **存档**：**零 format**（29 不动）——portal = 普通 stateful 块走
  blockEdits + states 段；worldgen 不放 obsidian/portal（结构免疫根）。
- **UV 动画 `[V3-15]`（v3.16.0）**：portal 纹理 = **水平 2 周期**
  （生成器 `x % 8` 重写，左半 ≡ 右半）+ fragment 的 tile-rect 分支：
  `u_portalAnim`（config `portal.anim` 严格 bool 默认 true，**off = 分支
  不执行 = pre-V3-15 位级**）门控下，portal tile 内 `x += phase`
  （fract 回绕，2 周期 = 无缝）；phase = `m_frameCount % 120 / 120`
  纯函数（frame-locked，ripple 同款纪律）；tiled 与 legacy 单片双路径
  统一（sampleUv 收敛 atlas 空间后判定）。init fail-fast：atlas 缺
  `nether_portal` tile 点名。
- **蒸汽粒子 `[V3-15]`（v3.16.0）**：portal 格的氛围发射——
  `updatePortalSteam` 每 6 帧一次，玩家脚下 8×8×9 盒（y 非对称
  feet-4..feet+5 = portal 顶框行）扫描 portal 格，每格 1/10 确定性
  roll（fire 种子契约 + 专属盐）发射 **1 粒**（portal 自身纹理的
  0.12 半透立方体走 blend pass，慢升 -0.4 m/s²，0.8-1.4 s 寿命）；
  config `portal.steam` 严格 bool 默认 true（fire.particles 先例，off
  = 零发射）。局限：距离玩家 >4 块的 portal 不发射（盒限）。
- **传送门之外——全完成**：Phase B 真传送 + format 30（v3.4.0，见「下界
  维度」）/ Phase C Part 1 下界内容族（v3.5.0）/ netherite 材料链
  （v3.6.0）/ Part 2 下界深化（v3.7.0）/ 下界 mob（v3.8.0）。

## 世界生成

- **地形**：fBm 2D 噪声高度场（`height_octaves` **4**、`height_noise_scale`
  **0.02**、**base 40 ± 24 格**）+ **`[GEN-8]` 山峰互感 `terrain.passes`**
  （「地形互感」定案 = mountain passes；独立低频 **fbm2**（scale 0.008、3
  octaves、专用 salt kSaltPasses）> threshold 0.55 时 `bonus = floor(
  amplitude × (mask−threshold)/(1−threshold))` 0..amplitude **只升不降**，
  `heightAt = min(base + bonus, 128 − 16)` 纯函数零 chunk 数据 = 跨 chunk
  一致构造性成立；shipped {noise_scale 0.008, threshold 0.55, amplitude 6}
  探针单轮定案【mask 实测 [0.07,0.88]：0.72 首选全世零 bonus 死参数 → 0.55
  = 全世 ~10% 列 bonus ≥2 / max 4 / demo 区 242 列 +1】；**首版局部峰设计
  废弃**（lround 平顶杀严格局部最大，`[PITS 290]`）；缺省段 = disabled 位级
  legacy；逐条点名 enabled/noise_scale (0,0.05]/threshold (0,1)/amplitude
  [0,64]）；y = 0 `bottom_block` bedrock（`[GEN-7]` 多层）+ 深处 `deep_block`
  stone
- **`[GEN-7]` 基岩多层**：`terrain.bedrock_layers`（int [1,8]，缺省 1 = 旧
  行为位级复现，越界/非整数点名）；shipped **3 = MC 经典底**（y0/y2 恒床岩 +
  y1 每格确定性 50% roll `bedrockLayerRoll` 纯谓词，kSaltBedrock）；fillTerrain
  单分支；零 format（世界生成演进）
- **生物群系**：低频噪声（0.02）+ 权重选择（`assets/data/biomes/`：**desert**
  = sand 表层 / **forest** / **plains** = grass 表层 / **snowy_taiga**
  `[V2-16]` = snow 表层），各 biome 有自己的表层/次表层/树密度/高度系数。
  **下界 5 群系 = 独立池**（`assets/data/biomes_nether/`，主世界池零触
  碰——nether_wastes 36 / crimson_forest 22 / soul_sand_valley 17 /
  basalt_deltas 16 / warped_forest 9 = MC 比例权重和 100；
  `netherBiomeAt` = biomeIndexAt 克隆 + 独立盐 + 派生 seed；装饰门控 = 数据
  派生旗 fill 零 name switch——详见「下界维度 · 下界群系族」）。
  **`[V2-16]` 雪原/针叶林 biome（v2.17.0）**：weight **2**（= desert 稀有带
  先例，记档偏离 4：非 MC 频率；总权重 10→12）/ surface **snow** /
  subsurface dirt / depth 3 / tree_density **0.12**（= forest 项目密林值，
  记档偏离 3：非 MC 0.24 尺度）/ height_scale 1.1 / **新严格字段 `tree` 6
  键对象**（= sapling tree 段同款面：trunk spruce_trunk / leaves
  spruce_leaves / min_height 6 / height_range 3 / leaf_radius 2 / shape 词
  汇 {oak, spruce, birch}——缺/多/类型/范围/shape 全点名 `tree.<key>` 拒 +
  create 交叉检 trunk/leaves 存在 + ENT-2 拒 stateful）/ **新严格字段
  `exclude_spawn_area: true`**（严格 bool）= **演示区结构保护**（GEN-1
  spawn_clear 结构排除先例：`inSpawnClearZone` 纯谓词区内 biomeIndexAt 累
  计走查跳旗 → 区内 totalWeight 恒 10 = 现有 3 群系映射**逐位不变**双单测
  钉）/ 无 decorations（记档偏离 2：项目无 fern/berry 块）。**snow 满块**：
  hardness **0.1**（MC 精确）/ 新 "snow" 三件音族（snow_dig 0.11 s /
  snow_place 0.12 s / snow_break 0.18 s 确定性合成）/ drops 自身 / opaque /
  full_cube 单 tile（atlas 121→122 零移位）——**记档偏离 1**：MC 1–8 层雪
  层机制（shovel 每层 1 snowball / 光≥12 融 / 放块替换单层）→ 满块替代
  （项目无层机制/snowball 物品/降雪天气【weather 仅 CLEAR/RAIN】；无配
  方）；spruce 树苗 `plant_on` [dirt,grass] 不动（雪面不生长 = MC 同款）。
  **placeTrees 针叶路径**：同 kSaltTree 格网 + 同 h 密度门/高度位复用 +
  冠层 = `planTree(height, TreeSpec)` 逐 cell placeIfAir（= MC 高瘦 5–7 针
  叶；Oak 表 cell-for-cell 同构既有钉）+ **无 tree 字段 biome 原硬编码路径
  零触碰**（既有 3 群系位级红线）。**零 format**（biome 不入档 = 每次 seed
  重生成；旧档加载 = 区外雪带世界演进，演示区结构不变 = 结构免疫根）。同
  版记档子项：**音效缺陷族 13 件修复**（generate_sounds.py `_noise()`/
  `dig()`/`break_sfx()` LCG **逐采样重建 = 常值流** → 4 件静音 + 5 件 DC 退
  化 + 2 件部分受损，跨多版本潜伏【截图/存档不录音频 = 门盲区】；修复 =
  单 LCG 实例推进，md5 变更恰 13 件、其余 head-identical 幂等，PITS 382）
- **水**：低于海平面（**y = 34**）的柱体填充流体水（level 8）到海平面；生成
  即处于平衡态（游戏内 20 Hz 流体 tick 不扰动湖面）
- **洞穴 `[GEN-1]` 面条洞穴 → `[GEN-4]` 破碎度重标定**：单噪声奶酪洞 → **两
  独立 3D value-noise 中位带相交**管状隧道（`|n1−0.5|<T ∧ |n2−0.5|<T`，n2
  仅 n1 入带时求值——MC spaghetti 同款）；**`[GEN-4]`（用户报障「洞穴太多太
  碎」）= `noise_scale` 0.09 → 0.05 单值**（`tube_threshold` 0.06 保持，
  worldgen.json 唯一内容变更，src/ 零变更）——特征周期 1.8× = 管粗洞大、口袋
  并入主管；效果（96×96 四 seed 聚合）：挖除 5.82%→5.12% / 域数 35→8 /
  **pocket 110→10 格 −91%** / 中位洞体积 9-130×；`min_y` 2（底壳保留）/
  **`surface_guard` 0** = 洞口可达表面（shipped = MC 同款）/ **
  `spawn_clear_columns` 2** = 出生/演示区列（原点 ±2 列）整列不挖（结构保护，
  任意 seed 出生点恒安全）/ 每湖 lava 带排除盒不挖（保熔岩湖静态被围）；矿脉
  只替换 stone（洞壁矿暴露 = 自然成立、不填 air）；树/装饰在洞之后跑（
  `[GEN-5]` 守卫）；**旧存档加载得新洞穴**（世界生成演进先例：存档不重生成，
  只重演编辑，已编辑列不受影响）；海床洞穴一次性渗水（生成平衡态后静态，
  记档）
- **`[GEN-5]` 浮空树守卫**（用户报障「洞穴生成后树长半空」）：根因 =
  placeTrees 用 heightAt（与洞无关）选位 + surface_guard 0 洞口可达表面格 =
  悬空树（placeDecorations 有同款守卫，树漏网）；修复 = carveCaves 逐格判定
  提纯函数 **`caveCarvesCell(wx, wy, wz, height)`**（逐位搬移，carveCaves 改
  调 = 行为恒等）+ placeTrees 守卫【表面格被洞吃 = **跳过整棵树**；跨 chunk
  纯函数重算不读 chunk 数据 = 整树同生同灭无半截冠，worker 并行安全】；shipped
  seed 实证 346 树列 14 列（4.05%）被跳
- **`[GEN-6]` 树放置半区修复**（交付上报 bug 立项）：placeTrees/
  placeDecorations 内层 wz 循环上界误用 X 基 limit = **cz ≥ cx+1 对角半区整块
  不种树/不放装饰**（Phase 4 起潜伏、102 道门未发现——基线视图朝 -Z 恒在种
  树侧）；一行级修复（`zlimit = oz + kChunkSize [+ margin]` 2 处；wx/margin/
  格网/roll/GEN-5 守卫零改动）；shipped 实证：门场景盒树列 13→33（2.54×）/
  装饰 5→20、前瞻半区锥内 215 列（游戏内视野）；零 format（世界生成演进）
- **矿脉 `[GEN-3]`**：世界空间格网（`lattice` **8**）+ **MC 3×2×1 旋转椭球
  脉**（`world/ore_vein` 纯函数 `planOreVein`：整数椭球谓词 `4u²+9v²+36w²
  ≤36` = **恰 21 格**，半轴 (3,2,1) 按 6 轴置换定向，专用 salt + PITS #4
  推进流；概率门/格内均匀起点/盐流逐位沿用旧机制）+ `vein_size` = **椭球簇数
  K ∈ [1,8]**（首簇中心 = 胞内均匀起点，后续簇中心在前簇 ±2 内游走 = 21..168
  格/脉；shipped 全 K=1）；**单轮密度标定**（探针 per-ore 新旧格数比 →
  probability re-tune：定稿 **coal 0.154（max_y 80）/ iron 0.0855（56）/
  gold 0.0376（36）/ diamond 0.0101（20）/ lapis 0.0208（32）/ gravel 0.0445
  （80）**；**gravel 同改椭球**（石替换是替换目标属性非形状属性）；矿感 = 脉
  更大更稀（21 格/脉 vs 旧 9 格游走、概率 ~÷2-5 = MC 1.18+ chunky sparse，
  diamond 单次收益大）；残留越带：diamond 0.641× / lapis 0.732×（单轮纪律
  记档）；跨 chunk 无缝（世界坐标格网 + 世界坐标起点）；只替换 deep stone
  （不破坏洞穴 = 洞壁矿暴露契约不变）；脉的 block 名生成 setup 对 BlockRegistry
  解析，未知 = 启动报错；**旧存档未编辑 deep 石按新脉形生成**（worldgen 演进
  先例，记档）
- **树木**：世界空间格网（`lattice` **4**）候选 + 数据驱动密度；**树干 4~7
  格**（min 4 + range 3）+ 两层半径 2 树冠 + 顶帽半径 1；跨 chunk 边界由邻接
  chunk 各自补齐（margin 格网机制），**只写入 air**（不破坏地形）；默认
  biome 野外单橡种（birch 只经 `[GEN-2]` 种植落地 = 树种差异是内容维度非生
  成维度）；**`[V2-16]` 起 snowy_taiga = 野外 spruce 针叶**（biome `tree` 字
  段驱动 `planTree` 针叶路径，上条）
- **表面装饰 `[P45]`**（biome 数据驱动）：biome JSON 可选 `decorations:
  [{block, probability}]`（严格校验：未知键拒绝、probability ∈ [0,1]、block
  名生成 setup 对 BlockRegistry 解析，未知 = 启动报错）；放置走**树的同一格
  网**（`trees.lattice`，专用 salt kSaltDecoration，每 (格, 条目) 对一次独立
  hash3 roll——装饰 roll 与树 roll 不相关）；落在表面格**正上方一格**，前置
  条件：高于海平面 + 表面格仍是该 biome 的 surface 块（**洞穴啃掉的表面不
  放**，防悬空 = 既有检查直接生效）；在 placeTrees **之后**跑（树干/树冠占格
  时 placeIfAir 让位，装饰丢弃）。当前内容：**沙漠 cactus（0.03）**、**草原
  3 花（allium/tulip/cornflower 各 0.02）**、**森林 3 花（各 0.012）**
- **熔岩湖 `[P86]` 单湖 → `[P88]` 列表 → `[P90]` 表面湖 → `[P93]` 程序化**
  （`worldgen.json` 数据驱动）：`lava_lakes` = **湖列表**，每个湖 = 固定位置
  (center_x, center_z) 的满熔岩池——盆地 (2R+1)²（radius R），**盆底 = 区域
  最低地形 − depth**（纯 `heightAt`，确定性、跨 chunk 一致），盆底以上地形
  carve air、最低 `fill` 层注满熔岩（level 8）；`placeLavaLakes` 循环所有
  enabled 湖（各自独立 region/floor，重叠湖 carve 同格 = 确定性）。shipped：
  - **深湖（depth > fill）**：(40,40) R4 D4 F2 + (55,45) R3 D3 F2——熔岩埋于
    盆底、四周地形 ≥ 区域最低 → **被围、静态、不流动不蔓延不燃烧**（熔岩顶
    低于 rim，与 rim 顶不 6-邻）
  - **表面湖（depth == fill）**：(24,26) R1 D2 F2——熔岩顶 = 区域最低高度（与
    草 rim 顶**同高**）→ 运行时 **`[P58]` 燃烧 pass** 把 rim 点燃 = 草→火→
    燃尽**焦土环**；同时地表熔岩**沿下坡溢出盆地**（FluidSim 流动，流缘再
    点燃相邻可燃物）= MC 熔岩湖「流动 + 燃烧周围」落地。**偏离**：表面湖非
    静态（溢出 + 燃烧改世界，确定性但非静止）、溢出形态取决于地形坡度、深湖
    无此行为（埋没被围）
  - **程序化分布 `[P93]`**（`lava_lakes_procedural` 段）：`count` **5** 个湖在
    区域 **[60,90]²** 内**确定性随机**分布（`computeProceduralLakes` 构造时从
    seed 派生：splitmix64 + kSaltLavaLake + 金步推进 = PITS #4 纪律；每湖
    radius 1-3 / depth 2-4 / fill 1-2 各取范围内随机值 = MC 熔岩湖散布世界）；
    跨 run/chunk 一致（纯 seed 函数）
  - 全部湖放出生点默认 -Z 视野外（+Z 后方）→ 无输入基线门不受影响（免重锚）；
    每湖 `enabled: false` 可关；熔岩会烧（`[P58]` 接触伤害/点燃）
- **水底 sediment `[V3-5]`（v3.9.0，建筑材料链起点）**：`worldgen.json`
  新 `sediment` 节数据驱动（`enabled`（**absent = disabled 位级 legacy**）/
  `clay_probability` **0.06** / `mossy_cobblestone_probability` **0.008**，
  严格 [0,1] 非数/越界 init 点名 + create() fail-fast 解析 2 沉积块 + 拒
  stateful）：**生成链最后 pass**（placeLavaLakes 之后读最终湖底——记档偏
  离 3：既有顺序装饰先于湖，「湖后装饰前」不可满足）per-column 纯函数
  （新盐 kSaltSediment/kSaltSedimentMossy，hash2 零噪声，chunk 间顺序无
  关）：dry 列（h ≥ seaLevel）首行 continue 零触碰 / 表面格终数据 ∈ 4 词
  表（dirt/gravel/sand/stone，代码常量记档偏离——洞穴/矿脉/湖挖过的拒）/
  **两个熔岩湖谓词**（列顶流体 = water【熔岩带 ≥ seaLevel 拒】+ 水面接触
  地板【熔岩带占地板拒】）/ 双独立 roll **clay 优先**（同格双中确定性）/
  **顶 1 层**（MC 3–4 层简化记档偏离）→ 水底 clay/mossy_cobblestone =
  黏土链起点（破 clay 掉 clay_ball ×4，见 MECHANICS.md「合成与容器 · 建筑内容族」）；
  发行世界 census（seed 123456789 ±512² 真实 biome 词表面 = desert 沙洋底
  only）：eligible 3683 / clay 211 / mossy 26；零 format（生成内容 = 参考
  不入 diff）
- **出生点**（硬规则 10）：4×4 chunk 演示区列内按 `kSpawnCandidates` 10 候选
  自动找干燥陆地（h > seaLevel 且出生 AABB 内无 solid = 避开树干/树冠/流体），
  见「物理与碰撞 · 出生点」

## 渲染与表现

- **管线与 pass**：OpenGL 3.3 core；每列 chunk mesh **三桶**（`[PRE-5]`：
  opaque / cutout / blend——mesher 第三桶 + MaskCell cutout 字段，
  **cutout/blend 面永不并 quad**，每列 mesh 2→3）；**贪婪剔面 `[P18]`**（逐面
  片合并为同（纹理、sub-mesh、面光）run 的单 quad；200-chunk 实测三角形
  109,648 → 33,558 = 3.3×；**合并不跨 chunk 边界**（标准做法，MC 同款逐
  chunk）；**流体表面仍逐面片**（level 缩放几何，刻意不合并）；**两个相邻非
  opaque 方块之间仍双向出面**（每平面两独立 slice 分别合并，叶片-叶片/玻璃-
  玻璃双面保留，正确略浪费）；只渲染暴露给 air/非 opaque 邻居的面（跨 chunk
  边界经 `World` 查询剔除）；**`[PRE-5]` 透明 discard**（根因 = 全项目零
  depthMask → blend pass 的 alpha-0 像素（leaves/cross/glass 框）写深度 = 透明
  孔后几何被不可见深度遮挡）：**块 property `cutout` 数据驱动路由**（严格 bool
  点名 + cutout ⇒ transparent 配对拒 + shape 门控仅 full-cube/cross【其余 shape
  = 死字段 init 点名拒 = ammo 纪律】；shipped **20 块** = 3 叶 + 3 花 + 7 麦 +
  3 苗 + fire/torch + 2 营火【纹理 16×16 alpha 普查为据】；**glass/
  glass_pane/water/lava 保持 blend 桶** = 半透/流体不写深度 MC 纪律）+ **frag
  唯一新行 `if (u_discardEnabled != 0 && alpha < 0.5) discard`**（桶无关：
  glass 框/leaves 孔/fire 缝全修，水 0.78/glass 芯 0.86/云 0.55/光束 0.5 全不
  discard；雾/波纹/gl_Position/v_viewDepth 零触碰）+ **渲染序 cutout 段
  （discard + 深度写 GL 默认）先于 blend 段（`setDepthMask(false)` 新 API——
  云/水/玻璃/粒子透明段/信标/overlay 不写深度）**+ `render.alpha_test`
  getBool 代码默认 true（config.json 零改动），**off = v0.28.0 位级恒等**
  （discard 关 + depthMask 恒默认；含半透混合场景的 off 分段序偏离 = 记档）；
  MeshVertex 尾字段追加纪律（water 标记 location 7 → cutout location 8，默认
  0 既有生产方零改动）
- **`[UXFIX-1]` 用户实机两渲染 bug 修复**（10-26 报障，`[PRE-4]` 云 per-draw
  状态泄漏家族 v0.28.0 起潜伏）：① **白盒**（热栏/面板块立方图标 = 半透明白
  盒）= drawClouds 遗留 `u_useTexture=0` + `u_color` 白 + `u_alpha` 0.55，
  立方（唯一不设自身该三 uniform 的 basic draw）继承；② **黑块**（挖掘块面
  纯黑）= 云/光束尾部 disableBlending 泄漏进 drawWorldOverlay，裂纹立方
  (0,0,0,0) 透明底不透明写入——locked + 无输入门全盲（113 次免疫从未见过）；
  修复 = 云/光束删自持 blend（混合段所有权上移）/ overlay 自持混合 / 立方显式
  bind + u_useTexture=1 + u_color(1,1,1) + u_alpha=1.0（受害者侧防御）；无输入
  位级恒等
- **脏标记增量重网格**：`setBlock` 标记本 chunk（及共享面的邻居 chunk）dirty，
  下一帧只重建 dirty chunk 的 GPU mesh（mesher 计算走 worker 快照 = 「世界与
  流送 · `[PIPE-1]`」）
- **slab 三态网格 `[V2-17]`（v2.18.0）**：stone_slab `state_count 3` 三态
  mesh（state 0 底部既有几何位级 / 1 顶部 `emitSlabTop` 镜像·底面恒绘 /
  2 双 slab 满格 `emitSlabFull` 标准邻剔），slab pass 内绘制、贪婪满块路径
  跳过 slab 不变 = 拼合面无共面双绘——详见 MECHANICS.md「合成与容器 · 方块模型 · slab」
  与 `docs/archive/v2x/phase-2.18.0.md`
- **雾 `[PRE-2]`**：流送盘缘原始切面（「渲染条」）距离雾溶解——basic shader
  雾 4 uniform（`u_fogEnabled` per-draw 分支无分歧 / `u_fogStart` / `u_fogEnd`
  / **`u_fogColor` = 同帧 `m_dayCycle.skyColor()`** 昼夜跟随）+ **雾深度独立
  表达式**（`v_viewDepth = −(u_view×u_model×pos).z`，**gl_Position 表达式树一
  字未动** = 位级红线：重排浮点乘序改 ULP 可翻转共面/深度竞争像素）+ 纯函数
  `renderer/fog`：静态 end = min(kFar 1000, 16×min(view_distance,
  kStreamCapRadius 64))、**start = `kFogStartScale` 0.75×end**（shipped vd80
  = 750/1000 块）+ **填充感知 `[V3-6]`（v3.10.0，base19 重锚 225272 B）**：
  end = min(静态 end, `filledChebyshevRadius` 实际填充盘 cheb 半径【填充 rim
  语义下闭保守】×16)——无输入流送 = 16 列后零扩展（PIPE-1 纪律），锚图「浮
  空截面」= 演示区边界 32–45 块 < 旧 start 750 完全可见、静态 end 按目标盘
  算对未填充盘缘无效（门 200 编排者实证定案）；稳态盘满 64 → min(1000,
  1024) = 1000 **位级恒等**【65 滞回变体钉】+ 下界同款 clamp【首传 3×3
  半径 1 = end 16】；`fogFilledRange` 纯函数 + render 雾块列快照共享（零
  新树走查）+ 零配置新键/零 format/零新 worker 活动；门 200 重锚论证 =
  diff 20940 px 全盘缘带 + 前景/HUD 0 diff + 单调趋天空 0 违例 + 单雾因子
  100% ≤2LSB + **fog-off ≡ base18 三方闭环** + 存档 savenorm EXACT；
  `render.fog` getBool 代码默认 true（config.json 零改动）+ **off = v0.25.0
  全管线位级恒等**（结构免疫）；**偏离**：太阳/月亮/星星（sky program
  kSkyDistance 800）不雾化 = **天空即雾色本体 MC 同款**；仅线性距离雾（无高度
  雾/per-material 雾/fogStart 比例非 config）
- **水面波纹 `[PRE-3]`**：静态水面 → 帧锁定 shimmer——水纹理 = 纯白噪声
  （per-pixel hash，GL_NEAREST）故**顶面 UV 采样窗逐帧平移即闪烁**（幅度
  **0.06 tile** ≈ 1 texel / 波数 **0.8 rad/block** / 周期 **120 帧** =
  `m_frameCount % 120` 纯函数，硬规则 8，命名常量零 config）；**水顶面走既有
  tiled 采样路径编码**（emitFluid：uv = tile 原点 + uvLocal = 角内坐标 + tiled
  = 1 + **location 7 water 标记**；frag tiled 分支 `(v_uv + fract(v_uvLocal +
  v_rippleOff)) × u_tileSize` = fract 自动 tile 内 wrap 零 tile 识别；vert 算
  v_rippleOff = (sin 0.8x+φ, cos 0.8z−φ)×0.06 仅水顶面非零，(0,0) 时
  fract(x+0) = x 位级恒等）；`render.water_ripples` getBool 代码默认 true +
  off = v0.26.0 位级恒等；**偏离**：lava 不闪 / 侧底静 / 非顶点波；
  water.png 周期化重跑字节零变化（16 格网内恒等）
- **云 `[PRE-4]`→`[UX-9a]`**（v1.48.0 **架构翻转 = 独立 cloud program
  fragment 逐像素**，消费用户两轮实机报障「云像纸片非方块组成」+「看不出
  云层厚度」）：PRE-4 纯色半透明平板 mesh（16 块格 50% 密度）→ **两级颗粒**
  （region 16 格 **50%** ∧ sub 4 块 **75%**，世界锚定专用盐同 t 步，覆盖度
  **37.5%**）+ **厚度**（4 向边界带 **MC 侧壁着色** ±X 0.8/±Z 0.6 相机侧判定
  + 底面恒白；4 条常数近似记档）+ y = `kCloudHeight` **108** / 漂移 t =
  frame/`kCloudFramesPerCell` **6000** = **100 s/格** MC 节奏 / alpha **0.55**
  （全命名常量零 config）；**GLSL 330 无 64 位整数 = uvec2 对模拟位精确**移植
  CPU mask 链（三层核对 107,520 评估 + 131,072 像素 0 失配；纯场四函数
  mask/sub/height/timestep 保留 = CPU oracle）+ init 一次性 **16384² quad**
  （~240 B 全期唯一上传——门内走位探针揪出 **iGPU buffer 更新 ~140 ms/MB** =
  旧 mesh 每跨 16 块云格 139 ms 卡顿根因；修后跨格 render max 142.9 →
  **2.5 ms（57×）**）+ per-frame 平移锚 4 块格网（世界锚定零游移）+
  **云延伸地平线**（旧 ±384 块窗口硬边消失）；mesh 面整体移除（−175 行）；
  `render.clouds` getBool 代码默认 true，off = 零 draw 位级恒等；**偏离**：
  云不随昼夜变色（恒白）
- **掉落光照 `[PRE-6]`**：块立方/model 物品掉落顶点 skyLight = 1.0 硬编码恒白
  （洞穴石头立方亮如白昼）→ **vert 端 per-draw uniform 覆盖**（basic.vert
  唯一改动：u_useLightOverride 3 uniform + 1 行门控分支；**frag 零改动** =
  昼夜缩放/环境底/雾/波纹全自动继承）+ 纯函数 `entity/item_light`
  itemDropLight（AABB 中心格 → getSkyLight/getBlockLight ÷ 15；每帧采样无
  缓存 = 光照变化自然跟随）+ `render.drop_light` getBool 代码默认 true，
  **off = 3 uniform 从不 set = v0.29.0 位级恒等**【几何文件零改动 = 不重烘】；
  **参与遮挡半 = `[PRE-5]` 结构性闭合**（opaque/cutout 掉落写深度、blend 不写
  = MC 纪律）；**箭头/手持恒白** = 记档偏离（矩阵外）
- **第一人称手臂 `[V2-6]`（v2.6.0）**（消费「无手部皮肤——只渲染手持物
  品」记档）：`drawHeldHand` 加臂 viewmodel = 玩家数据模型 **`arm_r` box**
  （`data/player_body.json`，init 校验存在 = named-box 纪律，缺 = MC_FAIL
  点名）re-center 到原点走既有 box-model 管线（`makeMobModelGeometry` =
  chunk-mesher-verified UVs）——**Steve 皮肤 pa_arm tile 六面全贴，零新纹
  理**（V2-2 tile：上 6 行衫袖 (52,148,152) / 下 10 行肤 (164,140,116)，
  与第三人称臂同款映射）；**门控 = 既有手持门三态矩阵化**（`firstPersonArm-
  Visible(mode, locked, panel)` 纯函数 = 指针锁定 ∧ 无面板 ∧ **仅 First**，
  ThirdBack/ThirdFront 隐藏 = 世界空间身体可见，V2-1 范式）；姿态 = 臂中
  心 = 手持 offset（kHandOffset + 弓拉/进食通道调整）+ kArmCenterDelta
  {0.12, −0.28, 0.18}（右、下、后于手），rotateZ(π + 0.5 rad)——**π 翻转
  使肤端朝持物端**（原始 tile 映射袖在上）+ 0.5 rad 斜倾朝视野中心；**静
  态无行走摆动 = 记档偏离**，但继承手持 offset → 弓拉/进食通道携臂微动；
  无手持也画空手（肤手）；**懒构建**（首次锁定第一人称帧才建 mesh = 无输
  入运行永不建，zero GL calls 结构免疫）；臂/手恒亮烘焙（baked skyLight 1.0
  = mob 既有约定，记档偏离）
- **毒纱 `[PRE-7]`**（全屏效果 tint 通道数据驱动）：效果 JSON 可选 `tint`
  [r,g,b,a] + **no-op 规则演进**（正 alpha tint = 表现层动作，纯装饰纱合法；
  alpha 0 仍 no-op 拒）+ 内容 `poison.json` [0.63, 0.24, 0.86, 0.35]（其余 10
  效果无 tint）+ 渲染 = HUD pass 红纱前每活跃玩家 tinted 效果一个全屏 quad
  （`activeEffectTints` 纯函数收集）；详见 MECHANICS.md「状态效果」
- **mob 火焰 `[PRE-8]`**：`Game::updateMobFlames` 每 **36 帧**（
  `kMobFlameEveryNFrames`，`[CRAFT-5]` 周期）每燃烧 mob 一批上升余烬（
  `[P66]` `spawnFire` 复用，专用盐）+ **自发光**（block 光 1.0 深夜也亮 = MC
  观感 + 中心格天光环境项）+ `render.mob_flames` 代码默认 true，off = 零发射
  位级恒等；详见 MECHANICS.md「实体与 Mob」
- **箭三段 `[PRE-9]`**（MC 式箭形 = 棕杆 0.31 + 灰箭头 0.12 + 白尾羽 0.09，
  总长 0.55 原轮廓不变）：详见 MECHANICS.md「实体与 Mob · 箭」
- **昼夜天空**（`[P8]`/`[LGT-3]`）：游戏内时间按每帧**固定 1/60 s** 推进
  （不用墙钟 = 无输入逐字节可复现；非 60Hz 显示器昼夜真实速度按比例偏差，
  记档）；`world.day_length_seconds` 代码默认 240 s 一天（**已提交 config =
  600 s 用户调值**），`world.start_time_of_day` 代码默认 0.25 = 正午（已提交
  同），`--time <0..1>` 可覆盖起点；**dayFactor**：黎明 **[0.05, 0.15)** /
  黄昏 **[0.45, 0.55)** smoothstep 渐变，白天 = 1 恒等、深夜 = 0（**呈现
  消费点 `[V2-4]` 起单收口** = RAIN ×0.85 + 雷 5 帧脉冲，见「天气」）；天空
  背景色
  按时间关键帧插值（t = 0.00 夜深蓝 (0.02,0.03,0.08) → 0.10 黎明橙
  (0.85,0.45,0.25) → 0.15 昼蓝 (0.47,0.71,0.95) → 0.45 昼蓝 → 0.50 黄昏橙
  (0.85,0.40,0.20) → 0.60 夜深蓝 → 1.00 夜深蓝）；太阳/月亮 = 像素级圆形
  billboard（**40/28 px**，越靠近地平线越淡；太阳轨迹：t = 0 正东地平线 →
  正午天顶 → t = 0.5 正西地平线 → 午夜天底，月亮恒在太阳对面；无纹理；
  **月相 `[V2-6]`（v2.6.0）** = **8 相 = day cycle 累计天 index**（`DayCycle::
  dayIndex` = floor(start + total/len) 帧锁定派生，total 固定 1/60 步推进 =
  纯帧函数）**% 8**（`moonPhaseIndex/moonPhase` 纯函数；相序表
  `moonPhaseOffset`：新月同心 → 娥眉 0.25 → 上弦 0.5 → 盈凸 0.75 → 满月相
  切 → 亏凸 → 下弦 → 残月，盈 = +X 滑 / 亏 = −X 滑，y = 光弧宽月盘半径单
  位）；呈现 = 既有月 quad 上叠暗色「食相」quad（sky shader 纯色路径同款
  28px 圆盘，颜色 = 深夜天色 `kNightSky` (0.02,0.03,0.08) → 覆盖区读作天
  空 = 娥眉幻觉，沿 view-plane X 滑移，**零新纹理**）；**绘制序 = 食相先/
  月后**（两 quad 同 r=800 天空平面，near 0.1/far 1000 的 24-bit 深度该处
  只分辨 **~160 块**，亚块 nudge 被量化吞掉 = 实证废案，**PITS 371**）；
  同半径圆滑移（非 MC 椭圆明暗界线/直线 terminator = 记档偏离）；首夜 =
  新月（day 0 夜月全遮不可见，第 2 夜起娥眉 = 记档偏离：MC 无固定创世月
  相的近似）；正午基线月在地平线下（moonAlpha 0 → **整分支不执行** = 结
  构性免疫））；
  夜空 **300 颗确定性星星**（世界空间 r = 800 上半球，splitmix64 哈希生成，
  2 px，以世界原点为心 = 走路极小视差；无闪烁无银河，星不动 = 无星闪保持记
  档）；**天空不雾化**
  （`[PRE-2]` 偏离：天空即雾色本体）；**光照呈现** = `[LGT-3]` 仿射撤退
  （见「光照」）
- **天气 `[V2-4]`（v2.4.0，雨/雷）**：**状态机 = `core/weather` 纯函数**
  （campfire 范式）：splitmix64 **推进式流**（主流 seed = 世界 seed = 跨
  run 同世界同序列；雷 = 独立 salt 流 `kSaltWeatherThunder` 0x7E42B1D9）；
  周期（命名常量，帧）CLEAR ∈ [kClearMin **9000**, kClearMax **18000**)
  （150–300 s）/ RAIN ∈ [kRainMin **3000**, kRainMax **6000**)（50–100
  s）= MC 比例压缩到 day_length 600 s 量级；雷 = RAIN 内每 [kThunderMin
  **600**, kThunderMax **1800**) f 一次；CLEAR→RAIN→CLEAR 交替、**首段恒
  CLEAR**（时长 ≥9000 f ≥ 60 f 基线窗 = 无输入 60f 内永不进雨，结构免疫
  根）；`--weather clear|rain` CLI（`--time` 同款范式 + 词校验点名）仅覆盖
  **初始 state**（rain = 初始进雨态，framesLeft 取 kRainMin **3000f 固定**
  可复现，主流零消费——其后走流 = 无 override 序列前移一段）；F3 `weather
  clear|rain` 行（off = 机器不跑 → "clear"）；**零 format**（天气 = session
  派生，云漂移先例类；MC 持久天气 = 记档偏离——重载/新 run 一律首段 CLEAR
  重启）。**呈现**：雨滴 = 独立 shader program（`rain.{vert,frag}`，cloud
  第四先例）——静态 VBO 每滴参数（出生格偏移 + 相位，`kSaltRainDrop`
  0xD93A5E17 专用 salt 推进流，N = `kRainDropCount` **300**，init 一次上
  传）+ 顶点 shader 逐滴纯函数位置（`y = mod(offset.y + fallOffset +
  phase×16, 16)`，fallOffset = (frame % 16)×1.0 有界项 = 无缝回绕，
  boxOrigin = 玩家格心 16³ 盒）= **per-frame 零上传**；雨滴几何 = 0.02×0.5
  细竖条 quad（X-Y 面双面关裁剪，blend pass，alpha **0.35** 白蓝
  (200,210,235)，**无新纹理**）；云层之后 HUD 之前绘制（深度测试开）；
  **天空 dayFactor 消费点单收口** `weatherDayFactor()` = RAIN ×
  `kRainSkyDim` **0.85** + 雷 **5 帧线性衰减脉冲**（+0.15 起：
  0.15/0.12/0.09/0.06/0.03，第 6 帧归 0）与 dayFactor 同通道叠加——
  u_dayFactor/clear/雾色同源，shader 表达式树零改动（雾自动继承）；
  off/clear = IEEE 恒等（x×1.0 == x）= v2.3.0 位级。**交互四件全帧锁
  定**（消费既有系统）：雨灭火块（`extinguishedByWaterOrRain` 新纯谓词 =
  6-邻水 ∨ raining，1 f 即灭走既有 putOut 路径）/ 雨灭营火（水浇灭条件同
  一谓词扩展，烤制状态保留续烤同款语义）/ 雨灭燃烧效果（玩家 + mob
  `inWater ∨ raining`，re-ignite 先行保水胜火序；**不做屋檐遮蔽检查 = 记档
  偏离**）/ 雨润耕地（`soilIsWet`/`cropIsWet`/`cropGrowthSteps` raining 扩
  参默认 false = 旧语义位级不变——湿润视觉/湿度 sweep/放置 seed 三消费者不
  传 = 视觉零变，仅生长节奏消费活天气 → 雨中生长 **2×**（111/200→200/
  200 钉））；流体不注水（MC 同款记档）；雷无声外效果（无落点伤害 = 记档
  偏离）；**无雨声循环**（雷 = 一次性远雷 2D 中心不空间化，记档）。
  **`render.weather` 严格读默认 true**（SAVE-1 纪律：缺键 = true
  config.json 零改动 / 非 bool = init 点名拒）：**off = 状态机不跑 + 零雨
  滴 + 零交互 = v2.3.0 位级恒等**（门 175 三方恒等实证：new-on ≡ old ≡
  new-off ≡ base18 锚逐字节）。详见 `docs/archive/v2x/phase-2.4.0.md`
- **流体覆盖层 `[99.5]`**：相机格含流体时**全屏半透明染色**（水蓝
  (0.20, 0.42, 0.85, 0.45) / 熔岩橙 (0.95, 0.42, 0.05, 0.60)，HUD 之下、世界
  之上，MC 同款 fluid overlay）；缓解「完全浸没时同种流体面互剔 = 看到空气」
  （`[P9]` 既有行为）；无波纹/无深度渐变/无气泡（v1 记档）
- **HUD**（`[P15]` 起，**只在指针锁定 + 背包未开时渲染** = 无输入运行零新增
  绘制 → 基线逐字节不变；例外 = **24h 时钟 `[V2-19]` 恒可见**）：**MC 三行
  HUD 堆栈 `[PRE-10]`**（行距 `kIconPx` +
  `kStripGap` **22**，每行独占 = 任意图标数零碰撞——消费「效果图标行与吸收红
  心可视觉重叠」Phase-56 条）：**心行 hy**（10 颗心 16 px 2 px 间隔，三态
  贴图满/半/空取自 health.png 48×16 条带，填充 = `clamp(hp − 2i, 0, 2)` 半心
  0.5 HP 粒度；**吸收红心** `[P56]`/`[P72]` = 正常心带左侧 `ceil(池/2)` 颗，
  池 0 零绘制）| **效果行 hy−22**（`[P46]`，左锚 = 心行最左缘 = 池在时吸收带
  左缘，MC 布局）| **护甲行 hy−44**（`[SURV-1]`，仅占位槽且指针捕获时绘制）；
  **心行右侧同行**（带间 6 px）= 饥饿 10 鸡腿（hunger.png 48×16）或**氧气泡
  排替换原位**（`[SURV-2]`：oxygen < 150 时 10 泡 = ceil(oxygen/15)，单 tile
  无半态，出水同帧消失）；**窄窗回退 `[PRE-11]`**：纯函数 `core/ui_
  layout.heartRowLayout`——整行超出右缘时把行左移直至**右排（更紧急读数）右缘
  贴屏**，吸收带（行最左）最先被裁（记档 last resort），低于最小行宽心也裁
  （记档极限：无布局能救 sub-row 窗）；**宽窗返回 legacy 坐标位级精确**（无
  输入基线 + 所有 E2E 窗零触碰 = 结构免疫根）；右排宽度按本帧实渲染排预留
  （氧气 < 150 替换饥饿 = `[SURV-2]` 原位语义 10 图标）；UI 度量常量
  kUiIconPx 16/kUiIconGap 2/kUiStripGap 6 收口 ui_layout.hpp 单一真源；**耐久
  条 `[SURV-3]`**：图标底部 2 px 横条（满耐久零绘制）；**XP 条 `[V2-11]`**：
  热栏正下方（条填充 (total − T(L))/needed(L) + 等级数，全整数数学，**恒可见**
  = 门 183 唯一像素源，零新字形）；**hotbar**：底部 9 格
  （选中格橙色边框，堆栈数 < max 时显示数量）+ **手持物品**右下角大图标
  （76 px）；**物品图标**：block item = 带光照的倾斜 3D 立方体（复用掉落物
  cube mesh + 基本着色器，白天亮夜晚暗）/ 工具材料 = 32×32 图标纹理（
  `Item.icon` 数据字段 → `assets/textures/items/<icon>.png`，
  `tools/generate_item_icons.py` 程序化生成，`[P35]` 起 32×32）；**准星**（
  锁定屏幕中央白色十字）+ **目标方块描边**（24 顶点线框立方体，描出 raycast
  命中格）；**HUD 红心检查**（E2E 协议哨兵：心带 y[646,661] x[551,729] 无
  输入运行 = 0，>50 = 污染废拍）
- **`[PRE-12]` tooltip**：悬停格（炉子面板槽 / E 面板槽与格）显示 `name`
  （count 1）/ `name xN`（count > 1，配方行 " xN" 同款）+ 工具第二行
  `damage/max`（F3 held 行同款 d/max 读法，**满耐久 0/max 文本显示**【区别
  于 HUD 条满耐久零绘制】）——纯函数 `ui/item_tooltip.itemTooltip(item,
  stack)`（campfire 范式）；**渲染层修复 = 门内发现 Phase-15 潜伏缺陷**（旧
  文本与面板共享 kText batch【batch 首用于面板标题 = 绘制先于槽底 kSlotBg/
  边框/图标】+ 偏移 L(12) 随面板 fit scale 收缩【679×744 窗 s ≈ 0.286 文本
  4.6 px 紧贴光标全落槽内】双杀 = **悬停 tooltip Phase-15 起恒被覆盖不可
  见**）→ **tooltip 独立 batch**（title kText / 耐久行 kTextDim）**移至 2D
  batch 与 3D 块立方图标之后绘制**（UI 最顶层、`[PRE-7]` 纱前；无悬停 = 零
  batch = 无输入基线不变）；**记档偏离**：无描述/附魔文本（item JSON 无
  description = MC 同款无描述；`[V2-12]` 附魔实现侧 tooltip 零改动）+ 效果图
  标 hover tooltip 结构不可行（见 MECHANICS.md「状态效果 · HUD」）
- **demo 回放 UI `[V3-9]`（v3.11.0）**：回放模式普通运行零执行 = 结构免
  疫（三旗 absent 位级恒等）——**`DEMO x<速度>` 角标**（右上 24h 时钟下方
  y28，`demoSpeedLabel` 0.5/1.0/2.0——原 y8 与时钟重叠 = 记档偏离）+
  **暂停/结束面板**（全幅 0.55 暗帷 + title/name/speed/hint 4 行：「Demo
  paused / ESC to resume - Q to quit」「Demo finished / ESC to keep
  watching - Q to quit」）——两者 = **自包含独立批**（门 201 门内捕获缺
  陷：追加进主批冲刷循环之后 = 顶点永不消费 → 独立批插入序即绘制序 帷
  →字→badge；截图 = swap 后读 back buffer 落后一渲染帧既有语义 →
  headless 放完态退出需双渲染才可捕获，PITS 391）；字符串英文 = 字体
  427 字形 CJK 表无 demo 词汇（记档）
- **F3 调试 overlay**（`[P15]`，任意指针状态可切换，**英文保持** `[PRE-1]`
  记档）：**20 行** = `TokenMine.cpp v3.12.0`（banner）/ `%d FPS | frame %d`
  （FPS = 真实 dt 平滑取整）/ `chunks %d | draws %u | verts %llu` / `pos %.2f,
  %.2f, %.2f yaw %.2f pitch %.2f` / `col (x, z) seed N` / `time %.3f day %.2f
  locked|free` / `streaming engaged|idle pending N radius N` / `inventory
  open|closed particles N entities N` / `health X / X absorption X armor N`
  （`[P72]`）/ `oxygen N`（`[SURV-2]`）/ `hunger X / X` / `held name d/max`
  （`[SURV-3]`，无 = "(none)"）/ `difficulty %s`（`[P25]`）/ `effects name
  Ns 罗马 | ...`（无 = "(none)"；`[PRE-12]` 档位罗马）/ `stats deathCount N |
  kill.<name> N | ...`（无 = "(none)"；`[PRE-12]` stats 行）/ `tutorial
  off|done|<s>/<total> <id> p/c`（`[UX-7]`/`[V2-3]`：禁用 = off / 完成或
  跳过 = done / 进行中 = 系列序号（1 基）+ 步 id + 进度）/ `weather
  clear|rain`（`[V2-4]` 天气段）/ `gamepad none|<name>`（`[V3-8]` 手柄
  连接态 = SDL device name，F3 保持英文纪律）/ `dimension
  overworld|nether`（`[V3-1]` 当前维度）/ `cam <Mode> D x.x[  flying]`
  （`[V2-19]` 相机模式 + 滚轮缩放距离 + 飞行标记；无相机数据 = "cam
  (none)"）
- **死亡屏**（`[P16]`/`[PRE-1]`）：「**你死了！** / **按 R 重生**」中文化（红
  字——`[P36]` 文字着色修复：ui.frag textured 分支 = `texel.rgb × u_color.rgb`
  【此前 u_color 只调制 alpha，死亡屏红字被静默忽略渲成白字】，图标/心带/白字
  u_color.rgb 均为白 = 逐位不变）
- **中文 UI `[PRE-1]`**：**UTF-8 解码渲染**（纯函数 `utf8NextCodepoint` 严格
  1..4 字节——拒 overlong/surrogate/>U+10FFFF/5-6 字节/孤立续字节/截断，非法
  字节 nullopt + pos+1 = 每坏字节一个 `?` 旧逐字节回退语义保持）+ **CJK glyph
  静态表 401**（`[PRE-1]` **33** = 19 UI 字【你死了按重生熔炉工作台背包合成
  需要关闭——PRE-1 表仅 18 漏 `作`，`[CRAFT-4]` 补 = 34】+ 10 中文标点（，。！？
  ：；“”（））+ 4 余量【物品方块】→ **`[UX-6]` 392**（游戏文档语料 top 330 ∪
  教程全词集零缺字 ∪ 既有 33 ∪ 标点，codepoint 序 0x00B7..0xFF1F；**来源钉定**
  NotoSansCJK-Regular.ttc SHA-256 + 10 face/SC index 2/版本 2.004 提取前验漂移
  点名拒 + 既有 33 字 33/33 位级自校验先行；**加字 = 改 .py `CJK_CHARSET` 清单
  一条命令零代码**）→ **`[UX-8a]` 395**（排/摆/输）→ **`[UX-9b]` 401**（亮处
  把敌暗煤）；PIL FreeType PRE-1 参数光栅化 em 16 / line box (19,5) / rows
  5..20 / coverage ≥128，TTF 仅工具期）+ `glyphUv(uint32_t)` 泛化
  （ASCII 32..126 原 cell 公式零改动 / CJK lower_bound 二分 / 未知 `?` 旧格）+
  textWidth/makeTextVertices 按 codepoint 迭代（CJK 16 px 等宽 advance，ASCII
  宽度逐位不变）；**图集 = 尾部 append**（`[PRE-1]`/`[CRAFT-4]` 既有 34 字 =
  ASCII 控制字符死格 rows 0..3 复用【`作` 取 ASCII DEL 死格 127】；**`[UX-6]`
  font.png 256×512 → 256×2080**（新字按 codepoint 序填 cell 128..486，rows
  61..64 = 32 cell 余量 → 424 字内零代码加字；**顶 512 行逐字节不变** = ASCII
  UV 位级恒等 + 免重锚红线，raw 扫描线独立核验 + 双 FNV 锚复算；重跑 ×3 md5
  恒 704ca41e = 确定性）；**8 处 UI 字符串中文化**（死亡屏 You died! → 你死了！/
  Press R to respawn → 按 R 重生 + 面板 Furnace → 熔炉 / Crafting Table →
  工作台 / Inventory → 背包 / E to close ×2 → 按 E 关闭 / Crafting → 合成 /
  needs: → 需要：）；**F3 调试屏保持英文**（开发者工具记档偏离）+ 悬停/配方
  行物品名 = JSON 数据驱动英文 id 不译（翻译 = 内容变更矩阵外）
- **UI 坐标空间与缩放**（`[P34]`/`[P35]`）：全部 UI（hotbar/心·饥饿带/面板/
  F3/死亡屏/手持/准星）布局在「UI 像素」空间 = 窗口**逻辑**尺寸 ÷ `ui.scale`
  （config [1,4]，代码默认 1.0，**已提交 config = 1.5 用户调值**），正交投影
  拉伸到可绘制区——GPU 放大（NEAREST 纹理保持锐利），SDL 逻辑鼠标坐标 ÷
  ui.scale 做命中测试；**`[UX-8e]` 字体独立缩放 `ui.font_scale`**（double
  (0,4.0] **代码默认 1.0** = 对 v1.44.0 位级恒等；absent/非数/非有限/≤0/>4 全
  点名 = SAVE-1/UX-1 纪律，config.json 零改动）——**只缩文本不缩图标/面板
  几何**：renderUi 全部文本 scale 单点缝（addText 19 / textWidth 12【配对一致
  性 = fs≠1 居中仍精确】/ fitLabel 3 / tooltip 2）+ 三处文本堆栈行距 ×fs（F3 /
  教程 tip / tooltip，lround 恒等 at 1.0）；图标/槽位/面板/心行/护甲行/效果行
  间距零动 = 独立性本体；fs = 1.0 时 s×1.0 IEEE 精确 = 结构免疫根）；
  **高分 UI 素材 `[P35]`**：字体 16×16 字形（32 px cell
  8 px 偏移）、平面物品图标 32×32、文字缩放 double scale（分数缩放支持）；
  **面板自适应 `[P35]`**：面板 native 设计，UI 空间更小时整个面板（槽/间距/
  内边距/标题/配方行 + 按同一系数绘制的字体 scale 与图标尺寸）等比缩小（长度
  全程 double 运算、仅 rect 边界 lround），恒贴合 UI 空间（每边 ≥1 px 余量）；
  **世界渲染不受 ui.scale 影响**（始终全可绘制区）
- **纹理 atlas**：16×16 tile × **16×8** 网格（128 tile，`assets/data/
  textures/block_atlas.json` **布局数据驱动**——新增纹理 = 加 PNG + 加一行
  JSON，不改代码；atlas 扩容（4×8 → 8×8 `[P41]` → 16×8 `[P59]`）会移位全部
  UV = 基线重锚，空位追加免重锚 = 纪律）
- **半透明水体渲染**：`ChunkMesher::emitFluid` 输出 level/8 缩放的立方体
  （流动水顶面半高），水-水内部面剔除（相邻水位相同/更高不出面），流体面
  烘焙**自身格**的光；shader 用纹理 alpha（水 α ≈ 0.78 / glass 芯 α ≈ 0.86 /
  leaves ≈ 0.92 + 透明孔），湖面可透视湖底；**波纹** = `[PRE-3]`（见上）；
  叶子挨叶子内部面 **`[V2-10]` 修复（v2.10.0）**：剔除 v1 只判邻居 opaque，
  叶子（非 opaque）内部面全生成（~12% 透明孔后看到邻叶背面）→ 修 = 块
  JSON 新可选布尔 `cull_same_id`（严格解析，leaves/birch/spruce_leaves 三块
  首消费，shipped 恰三 = 零影响数据钉）+ mesher greedy pass 同 id 非 opaque
  邻剔面（纹理选择前 = merge 不见该面；glass-glass 不剔/水规则/cross·slab·
  stairs·wall·fence·pane 红线零触碰，详见 `docs/archive/v2x/phase-2.10.0.md`）
- **窗口标题**实时显示 FPS 与 draw call 数（`[99.6]`）

## 音频

- **SoundBank**（`[P15]`）：SDL3 `SDL_OpenAudioDeviceStream` 队列式播放（无
  回调）；初始化失败**优雅降级为静音 + WARN**（headless 音频降级 play 静默但
  日志照常发——环境音 E2E 证据走日志）；音量 `audio.volume`（代码默认 0.7，
  已提交 config 同）；**`[CRAFT-3]` mono → stereo**（44100 Hz 不变；旧
  `play(name)` = mono 复制双声道 = 居中听感位级等价）
- **63 个确定性 WAV**（`tools/generate_sounds.py` 固定种子 LCG/sine，**既有
  wav 字节零变化 + 重跑幂等**纪律逐版 md5 实证）：17 基础事件（dig/place/
  break/pickup/step/ui_open/ui_close/craft/eat/hurt/death/shoot/bow/splash/
  milk/cloth【`[P41]` 染色布料声】+ ignite【`[P60]`】）+ **18 材质音
  `[CRAFT-3]`**（6 材质 × 3 事件：stone 硬脆/raw noise/2200 Hz ping、wood 中
  频腔音、gravel 双 noise 层干颗粒、sand double lowpass 低 shush、cloth 最静
  rustle、glass 1900-3200 Hz 铃音）+ **3 环境音 `[PRE-4]`**（cave 低频嗡鸣 +
  4 水滴 / day 鸟鸣 + 双风床 / night 蟋蟀 + 弱风，amb_env 慢淡入淡出无咔哒，
  音量 = SFX 的 1/4~1/2 环境底噪定位）+ 1 **smelt** ding `[CRAFT-5]`（1320/
  660 Hz 双泛音钟 + LCG tick 种子 0x51DE）+ 1 **雷** `[V2-4]`（60→40 Hz 滚
  雷 3.5 s，种子 0x70DE，2D 中心不空间化）+ 1 **sleep** `[V2-13]`（180→90 Hz
  指数滑降 + 360 Hz 弱二次，种子 0x51EE，0.4 s 缓入缓释）+ 1 **squeak**
  `[V2-14]`（900→1400 Hz 滑升 + 30 Hz 微颤 0.12 s，种子 0xB441，bat ambient
  空间化）+ 1 **shear** `[V2-15]`（亮噪声 catch + 低通 parting 双「咔嚓」，
  种子 0x5B4A）+ **3 snow 族 `[V2-16]`**（snow_dig 0.11 s / snow_place
  0.12 s / snow_break 0.18 s，sand 族范式一阶更软，种子
  0x75001/0x75111/0x75222）+ **3 portal 族 `[V3-1]`**（portal_trigger
  进入 / portal_travel 抵达 / portal_ambient = 32 块可视域 per-frame 确定
  性 roll p=1/120，蝙蝠 squeak 节奏先例）+ **11 下界族 `[V3-2 Part 2]`**
  （nether/nylium/stem 3 材质族 ×3 事件 = 13 下界块 sound_material 重指
  派全钉 + evaporate 水蒸发 + ambience_nether 第 4 上下文音）+ **3 下界
  mob 氛围 `[V3-3]`**（piglin_whisper/hoglin_grunt/strider_hiss，runAi
  数据门控分支，种子 0x9201–3）+ **1 boom `[V3-14a]`**（爆炸闷响）+
  **2 ghast 族 `[V3-14b]`**（ghast 嘶鸣 / ghast_roar 发射咆哮）+ **1
  barter `[V3-14c]`**（piglin 交易 chime 880→1318.5 Hz）+ **1
  ambience_end `[V3-10a]`**（第 5 上下文音：40 Hz 低频体 + 0.2 Hz 颤 +
  2.17× 微光 + 2 远距 whoosh 220→55 Hz，种子 0xC0DE）+ **2 珍珠族
  `[V3-10b]`**（ender_pearl_throw 320→980 Hz whoop / ender_pearl_
  teleport 1400→240 Hz shimmer，首个投掷物的声音面）；**`[V2-16]` 音效缺陷族 13 件修复**（`_noise()`/
  `dig()`/`break_sfx()` LCG **逐采样重建 = 常值流** → 4 件静音 + 5 件 DC 退
  化 + 2 件部分受损，跨多版本潜伏【截图/存档不录音频 = 门盲区，PITS
  382】；修复 = 单 LCG 实例推进，md5 变更恰 13 件、其余 head-identical 幂等
  不变）
- **事件集**（数据驱动）：破坏（按住时每 `kDigSoundInterval` **0.3 s** 一个 dig
  tick + 完成时 break）、放置、拾取（含 Q 丢弃）、脚步（移动 + 贴地 + 锁定 +
  背包关闭时每 `kStepSoundInterval` **0.4 s**）、背包开/关、合成成功、受伤/
  死亡（`[P16]`）、射箭（`[P21]`）、拉弓弦音（`[P24]`，拉弓开始时响）、
  进食（`[P23]`）、染色布料声（`[P41]`）、牛奶咕嘟声（`[P43]`，饮用开始 +
  挤奶共用，取消也响过 = MC 一致）、点火（`[P60]`）、smelt 完成 + 复燃
  ignite（`[CRAFT-5]`）、破碎（`[SURV-3]` 复用 break，玩家侧护甲破碎无声
  记档）、雷（`[V2-4]`，2D 中心不空间化）、sleep（`[V2-13]`，床空间化）、
  squeak（`[V2-14]`，bat ambient 空间化）、shear（`[V2-15]`，mob 位空间
  化）、portal trigger/travel（`[V3-1]`，传送进/出空间化）、evaporate
  （`[V3-2 Part 2]`，下界水桶泼出空间化）、下界 mob 氛围 ×3（`[V3-3]`，
  mob 位空间化）、boom（`[V3-14a]`，爆点位空间化）、ghast_roar
  （`[V3-14b]`，发射前咆哮空间化）+ ghast 嘶鸣（ambient）、barter
  （`[V3-14c]`，交易 chime mob 位空间化）、strider_hiss 骑乘中保留
  （`[V3-14d]`）、ambience_end（`[V3-10a]`，第 5 上下文 2D 全局）
- **空间化 `[CRAFT-3]`**：块 JSON 可选 `properties.sound_material`（严格
  字符串 + 显式空串拒 = ammo 纪律）+ init 交叉校验（`block_sound.{hpp,cpp}`
  纯函数 `checkSoundMaterials`：引用材质 ⇒ `<m>_dig/_place/_break.wav` 3 件
  **文件级**存在【headless 音频降级时 init 照样严格】，点名 = registry 序首
  败块 + 事件序首缺件）+ **纯函数空间化**（`distanceGain` 线性衰减全 3D 距离
  cutoff = `kSoundCutoffBlocks` **16.0** 命名常量零 config / `panGains` 恒定
  增益线性声像 (1±t)/2 高度不参与前后居中 / `toStereo` 16-bit 钳制）+
  `play(name, std::optional<Vec3> origin)` 新 API + `setListener(相机)` 帧始
  同步【1 帧陈旧零成本】+ **6 调用点位世界空间化**（dig tick/破坏完成/放置/
  潜行旋转/丢弃 @脚位/物品拾取 @物品位，事件名 = `blockSoundName` 纯函数：无
  字段 = 通用名位级不变）；**内容 = 6 材质 × 3 事件 = 18 新 wav + 32 块赋值**
  （stone 8【含矿脉族 = MC 语义】/ wood 3 / gravel 1 / sand 3【sandstone 系 =
  MC】/ cloth 15【wool 全染变】/ glass 2；未赋值块 = 通用回退，obsidian/
  furnace/铁块/楼梯族等 = 下轮候选记档）；手持物品破碎/箭拾取/营火放取/
  step/shoot/eat/hurt/ui_* 保持 2D 位级不变
- **昼夜环境音 `[PRE-4]`**：**5 上下文**纯函数 `ambienceContext`（**end
  `[V3-10a]` 优先** = 玩家在末地 → ambience_end / **nether
  `[V3-2 Part 2]`** = 玩家在下界 → ambience_nether / **cave = 玩家
  脚格 raw skyLight 0 优先**【MC 无天空规则，洞口边缘抖动记档】/ day =
  dayFactor ≥ 0.5 / night；`inNether`/`inEnd` 默认参 false = 主世界位级
  恒等——门 197 30 组输入面单测 + 门 213 End 上下文单测 + S1 锚双保险）+ 每 **300 帧**（`kAmbienceIntervalFrames` = 5 s
  at 60 fps）`ambienceRoll(seed, frame, ctx)` 纯函数 < **0.5** → 2D play
  （**全局不空间化**）+ `Ambience: <name>` info 日志（帧锁定事件序列 = E2E
  证据）；`audio.ambience` getBool 代码默认 true；**偏离**：MC 不规则多音池
  = 3 上下文单音 + 5 s 帧锁定 roll 简化；kAmbienceSalt 选定使 shipped seed S2
  事件帧全 pass（E2E 可观测性设计选择，1000-roll 无偏证）
- **硬规则 8：音频不入帧状态**——截图/存档零影响；事件检测纯函数（tick 无
  副作用，`[CRAFT-5]` furnaceTickEvents 范式）；实际听感 = 用户实机（无头不
  可听记档）

