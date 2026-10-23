# Phase 99.x 细节存档（性能审计系列）

> 范围：P99（玻璃板）完成后的性能审计与修复系列（99.1–99.9）。用户
> "Phase 100 前自主决策，逐个修复，逐个提交"授权；99.4–99.9 由用户
> 反馈/提问驱动（走路跑观察、三问、多核单核热报告）。提交链：
> `2a98641` 99.1 / `70e4445` 99.2 / `4bebb32` 99.3 / `3c345bb` 99.4 /
> `07d1697` 99.5 / `3591c29` 99.6 / `1689121` 99.7（第一版 `7f18c4d`
> 作废重提）/ `e6e9bb3` 99.8 / `25681d3` 99.9。系列终态：测试
> 1195/1195、`-Werror` 0 警告、base18 门对 99.9 树字节一致（第 80 次
> 结构性免疫）、vd80 流送中 ~30 FPS 稳定（修复前 2-30 FPS 坍缩）。

## 99.1 栅栏/玻璃板破坏崩溃（`2a98641`）

thin-shape（fence/glass_pane 恰 1 个 `side` 纹理，top/bottom 空是设计）
破坏时，破碎粒子（drawParticles）+ 掉落物 item cube
（makeItemCubeGeometry）读 cube 面 → `AtlasLayout::uvMin("")` →
`unordered_map::at` 抛异常 → terminate（core 回溯确认）。P65 修过同款
（cross 形状粒子纹理），本条 = 该模式的第二次出现（thin-shape 版）；
P98/P99 的 E2E 从不破坏这两块所以没测到。修复 = 粒子/item-mesh 的
side 纹理回退分支 + 单测。E2E pfence：×2 形状破坏，掉落物停
(22.5,45,−4.5)，×2 字节一致。

## 99.2 流送性能与内存（`70e4445`）

用户报告：探索时内存无限增长（11144 chunks 且升）+ 环视卡顿。
- **远端卸载** `unloadFarColumns`（pool flush 后的主线程单写窗口；
  距玩家 > view_distance+1 的列；保护列 = 演示 4×4 + 玩家编辑列 +
  五类 per-block 状态列 + 在途列）。实测走路 85 s：RSS 平台 470-474
  MB（修复前无限增长），9 次卸载事件 = 步行节奏。
- **列完整性**：streaming 按"任 chunk 存在 = 已加载"计数 → 单 chunk
  创建留下 7 chunk 虚空洞 + "engaged 零 post"停摆。修 = 列完整性
  不变式（`World::columnComplete/ensureColumn`，setBlock 收口 +
  applyWorldEdits 两处创建点）。
- **重 mesh 帧预算** `kRemeshBudgetPerFrame=8`（距玩家最近优先）：
  无预算时单帧 100-400 ms 主线程卡顿。
- **worker 积压上限** `kMaxStreamBacklog=8`（flush 等全部 in-flight，
  无上限积压 = 无上限主线程阻塞）。
- **全扫描消除**：流体 per-kind chunk 集 + 六邻（替代每 tick 全
  chunk 扫描）、World 脏列集、增量 m_loadedColumns、显式
  m_lightPendingColumns。
- **光照绕脏集黑面**（markChunkDirty 经 World API）。
环满 update 151→21 ms；走路 38 FPS。

## 99.3 存档往返修复（`4bebb32`）

自己写的存档加载失败：① P90 熔岩烧填格 = 合法编辑对（块→air +
流体>0），校验过严拒绝；② 流体-only 编辑列无 chunk（加载需
getOrCreateChunk 同步生成）；③ level-0 编辑对干格 = no-op 跳过。
揪出既有同格单测假阳性（流体编辑 lambda 传 "stone" → 5 元素畸形块
编辑）。真实 92 块/262 流体存档往返验证。

## 99.4 lit pop-in 黑面修复（`3c345bb`）

用户走路跑报告："一排侧面黑/打邻居正常"。99.2 的帧预算把"光照后
重网格"排在整条脏块积压之后 = 秒级黑面。修：unlit 列
（m_unlitColumns + LightJob done 标志 + collectLitColumns）在光照
job 完成前**不网格化**，完成后一次性网格为正确光照——黑面消失 +
网格工作量减半（每新列只 mesh 一次）。E2E pblack：边缘玩家 600f
（599 列流送在画面内）零纯黑像素 ×2 + x2 RAW。

## 99.5 流体覆盖层（`07d1697`）

用户报告："走进深水后周边像空气一样透明"（同种流体面互剔 = 深水中
满水格零面）。修：相机格含流体 → UI pass 首批次全屏半透明染色（水
(0.20,0.42,0.85,0.45) / 熔岩 (0.95,0.42,0.05,0.60)，MC 同款 fluid
overlay）。E2E pwater：中心像素 B 通道 +97 = tint 理论值；×2 RAW
×2 场景。

## 99.6 日志精简 + 标题 FPS 仪表（`3591c29`）

- 逐列 integrated / 逐事件 Unloaded → DEBUG；streamChunks 每 300 帧
  一条 INFO 进度（"Streaming: N/M columns (C chunks, U unloaded)"）。
- **标题 FPS 仪表是死的**：SDL3 X11 静默拒绝非 ASCII 标题（em dash
  "—" → "Failed to convert title to UTF8"，返回值被忽略）→ 窗口名
  永远是初始 "TokenMine.cpp"，"N FPS | M draw calls" 从未显示过
  （历次 harness 标题轮询 FPS 全 0 = 当时误判为 harness 问题）。改
  ASCII 连字符；实测 "TokenMine.cpp - 39 FPS | 629 draw calls" 正常
  刷新（0.5 s 节奏）。PITS：SDL setter 返回值不可忽略。

## 99.7 Mob 四肢重做（`1689121`，第一版 `7f18c4d` 作废）

用户："腿太丑，请重新添加动物的腿（四条腿的），以及 2 条腿的小鸡；
请搜索网页，用我的世界的图片作参考"。第一版（牙签腿 + Phase 10 面包
体）作废（未推送，reset）。重做参考 = Minecraft bedrock geometry
镜像（geometry.chicken/cow/quadruped/spider 的 cubes，16px = 1 块
换算；mcmeta 镜像不含 mob 模型，最终从 InventivetalentDev/MineRender
的 res/tools/bedrock/mobs.json 取到精确数据）：
- chicken 7 box：直立体 + **2 橙粗腿**（MC 3×5px）+ **2 翼** + 头 +
  **橙喙**；
- cow 6 box：横躺面包体改**直立体**（MC 12×18×10px 比例）+ 4 深褐腿
  （前后错位同原版）+ 头；
- sheep 6 box：直立羊毛体 + 4 黑腿 + 头；
- spider 10 box：8 细腿沿身体 4 排重分布；zombie/skeleton 躯干本已通地。
- 3 腿纹理 tile（chicken_legs 橙 / cow_legs 深褐 / sheep_legs 黑，MC
  原色；generate_mob_textures.py 确定性再生，既有 12 tile 字节零
  变化）；atlas 空位 [8,4-6] 免重锚（第一版腿用身体纹理 = 整个身体图
  缩在腿上，丑的直接原因之一）。
- AABB 不变 = 碰撞不变（box 全在模型 AABB 内，既有校验纪律）。
内容钉死 ShippedModelsReachTheGroundWithLegs（box 数 7/6/6/10/2/2 +
最低 box 底 = 0 贴地）。E2E plegs：白天 900f A（mobs 开）×2 RAW +
C（关）×2 RAW，A vs C 世界区像素 diff 32889 px。摆腿/摆臂动画 =
文档化偏离（per-type mesh 静态缓存）。

## 99.8 流体活动门控 + 流送 2 列/帧 + 完成通告（`e6e9bb3`）

用户三问之一（"是否有元素一直在算、浪费计算资源"）的正面答案：
P90 熔岩峡谷流流到静态平衡后（228 格 level 1-8 梯度逐格 150 s 与
300 s 完全相同），流体模拟仍每 tick 全走 = 恒定 40 ms/tick（稳态
FPS 直接砍半）。
- 第一版门控 "level ≥ 2 = 活跃" 抓不住**静态流动梯度**（平衡态保留
  2-7 梯度，实测 5000+ 帧成本纹丝不动）——PITS 教训。
- 终版 = **变化热**（chunk 粒度 MC cell-queue 性质）：hot（上 tick
  有单元格被 setFluid 写入，setFluidAt 统一维护含 kind 互换/排空
  双侧加热）+ walk-pending（块变更扰动：World::setBlock 对目标 + 六邻
  含流体 chunk 强制一次遍历——静置水片下方被挖支撑仍会下落，单测
  SettledFilmFallsWhenSupportBroken 钉死）。遍历后消费 hot。
- reaction pass 水邻接门控 + burn pass 可燃列门控
  （m_flammableChunks，集成列扫描/放置收口/卸载移除/buildScene
  rescan；burn 只减不增 = 有意 over-approx）。
- 实测：lava 遍历 59→7 chunks、流体 40→~2 ms/tick、稳态 18→51 FPS。
- 流送 1→2 列/帧（kStreamColumnsPerFrame；vd16 环填充 100 s→26 s；
  光照 box 重叠竞态纪律不变 = 单 job 串行）。
- 环满打显式 "Streaming complete" 行（此前静默 = "停摆"误诊源，99.2
  排查弯路两次）。

## 99.9 多核负载分配（`25681d3`）

用户报告："游戏大部分计算跑在第一个 CPU 核心上，导致单核温度很高，
需要优化为多核利用，尽量平均分配游戏的计算"。插桩实测（vd80 真实
场景）定位三个根因并全修：

1. **渲染无视锥裁剪 + 每 chunk 一 draw call**：vd80 加载 ~207k chunk
   全部提交（iGPU 每 draw ~10-30 µs + 小 VBO 页表开销，随环无上限
   增长）→ render 相 8→37 ms/帧持续恶化（满环 = 0.15 FPS 坍缩）。
   - 新 `src/core/math/frustum.hpp`：MVP 六平面提取（Gribb-Hartmann，
     列主存**行 = 步长 4 访问**——首版把 `m.m+4k` 当行使 6 平面全
     错，identity 单测恰好掩盖，非对称 MVP 单测才暴露，PITS 191）+
     保守 AABB 测试（整列在视锥外才剔 = 被剔列零屏幕覆盖，像素逐
     字节不变）+ 3 Frustum 单测。
   - render() 两个 chunk pass 改**列 pass**：m_loadedColumns 迭代
     （不再每帧快照 + 排序 ~207k chunk）+ 列级视锥测试 + columnModel。
2. **网格 per-chunk GL mesh → per-column 合并**（draw ÷8）：
   m_chunkMeshData（CPU 几何 per-chunk）+ m_columnMeshes（GL：8 chunk
   拼 1 opaque + 1 transparent，顶点烘焙层 Y 偏移，模型矩阵只平移
   X/Z——首版 X/Z 双烘焙 = 81% 像素差，一测即知）+
   m_columnRebuildPending（预算溢出列的上传延迟）。卸载路径同步改列
   级。ColumnPos 补 std::hash 特化（chunk_streaming.hpp）。
3. **光照 worker 太慢（flush 等待 = 主线程大头 25-30 ms/帧）**：
   全部 region 扫描改 **chunk 级**（每 16³ 块一次 map 查找 + 直接数组
   访问；旧代码每格一次 findChunkMut ≈ 12 ms/box）+ BFS **last-chunk
   缓存**（邻居 15/16 次同块）+ **既有地形重扫零写入 → 跳过天空
   BFS**（新地形必须强制传播 = `updateRegion` 加 `newTerrain` 参数：
   整列天生暗——天花板下——时重扫零写入，侧邻列的光仍要流进来；
   两轮回归教训 PITS 192：±R rescan footprint 与 region 全有光格种子
   是 void→terrain 陈旧光所需，不可减；worklist BFS 只能从入队格
   传播）。

实测（vd80 pblack 场景，流送中 52-62k chunk）：
- render 37→**1.5-2.5 ms/帧（不随环增长）**；draw calls 2216→~1100。
- flush 25-30→**13-17 ms**（光照 2 box 并行于 4 worker）。
- **主线程 CPU 100%→38-43%，4 worker 各 16-23%**（3 窗口采样实证）。
- ~30 FPS 流送中不再恶化（满环后 flush/remesh → 0，预期 45-60 FPS）。
- 环填充 60 列/秒（2 列/帧 × 30 FPS 满载）。

验证：1195/1195（+3 Frustum 单测）、`-Werror` 0 警告、**base18 第
80 次字节一致**（裁剪 + 列合并 + 光照重构全保像素）、pblack（pop-in
零黑面）+ plegs（mob 确定性）E2E 全过。

## 99.10 光照 job 并行（`78e0155`，2026-10-24，用户点单：走路流送卡顿治本）

用户报告"走一段路后晃镜头很卡，FPS 三十多也卡"。本机复现铁证 =
burst-stall（标题 13 FPS EMA 恒显 vs 日志 300 帧 = 152 s 实际均速
2 fps）：主线程 pool-flush 同步等光照 worker，流送期光照 job
**16.7 ms/帧硬下限**（LGT-1 step-0：单 worker 顺序跑 2 box，per-box
~8.37 ms）+ CPU 争抢时 worker 调度延迟 ms→s = 秒级尖峰。治本 =
99.10：LGT-1 已证 99.9% 流送帧的 2 列**远隔**（|Δz| 主导 15..60）→
两 region 位级独立时拆 2 个 job 双 worker **并行**，flush 下限
16.7 → ~10 ms（per-box + FIFO 启动延迟）。

**判据（`world/light_job.{hpp,cpp}` 纯函数，mesh_job 同款模式）**：
写 reach = 列 box ±kLightUpdateRadius = ±1 列；block 光 BFS 边界种子环
再 **+1 列读 reach**（LGT-1 的 footprint 判据只覆盖写 reach，gap 3
写不相交但读环相交 = 并行竞态 → 修正为 `kLightParallelColumnGap =
2·(reach+1) = 4`：任轴 gap≥4 = 写/写 + 写/读双向零共享 chunk）。
`planLightJobs`：恰 2 列且独立 → 2 job；依赖对/3+ 列 → 1 job 顺序
（pre-99.10 逐位不变，含 LGT-1 定案不修的 overlap clobber 语义）。

**并发写竞态**：两并行 job 不能同时 `World::markChunkDirty`（共享
std::set）→ `updateRegion` 加 `dirtyOut` 默认 nullptr 参数（worker
job-local 收集净变 chunk；`collectLitColumns` 在 flush 后主线程写窗口
统一 apply）——其余光照路径（流体/编辑/树生长）默认参数逐位不变。
硬规则 7 修订：≤1/帧 → **≤2/帧且仅位级独立**（AGENTS/PROTOCOL 同步）。

**位级等价**：confluence（LGT-1 证明：独立 region 最终光 = 插入前世界
的纯函数，完成顺序无关）+ 单测 `DisjointPairPathsBitIdentical`
（2 job 序/逆序/强制单 job 三轨全光值逐位一致，逆序轨 = 并行完成
顺序无关证明）+ `DisjointPairMatchesFullPropagation`（propagateAll
oracle）+ `DependentPairSequentialUnchanged`（回退轨钉死）。
验证：1339/1339（+11）、`-Werror` 0 警告、门 90（无输入出生点惰性
= 对锚字节一致 + 流送场景 ×2 确定 + **新旧二进制 300f 流送 A/B
字节一致 = confluence 的 E2E 级证明**）。
性能 A/B（vd16 冷填充墙钟，T450）：旧 253.3 s → 新 244.7 s（−3.4%）
——**本机 CPU 饥饿下收益被 burst-stall（worker 调度秒级尖峰，PITS 有
录）吞没**：16.7→~10 ms 的下限削减只占实际帧时（~500 ms）的零头。
用户实机（30 fps、CPU 余量大）该下限占比高，预期收益大于本机测量；
本机实感的深层卡顿源 = 99.11 剖析对象（step-0 四路插桩 + 走路/扫
镜头工作负载）。发布 v0.8.0。

## 99.11 流体光照异步化（`86c8d31`，2026-10-24 夜，用户"找深层次卡顿原因"授权）

step-0 四路插桩（帧三段/flush 直方图/六组件细段/tick 墙钟+churn 规模）
定案根因：流体 tick 后对**全部 changed 格的主包围盒**做一次主线程同步
`Lighting::updateRegion`。shipped 海洋世界（seed 123456789）海岸线 film
period-2 级联（FLUID-1 定案的接受现状，海岸线尺度）= 713→918→1357
格/tick 且增长，散布几百列跨度 → 巨型 box 重扫 = 每 300 帧窗口 fluid 段
**130s→396s→661s**（tick 本体仅 20-38 ms，**99% 成本在 re-light**）→
update() p95/max = 2.3-8.1 s 帧、双峰分布（med ~50 ms vs p95 秒级）。
其它组件全部排除（fires 851 ms/300f、stream 1.1-1.8 s/300f、player 3 ms、
flush 等待全 <100 ms——原"burst-stall/worker 调度"假设被数据证伪）。
= 用户"跑一段距离后转摄像头很卡、FPS 三十多也卡"的深层根因（每 3 帧
一次秒级主线程尖峰；标题 FPS = 0.95 EMA 均值吞尖峰）。

**修复 = 光的传导路径异步化**（FluidSim/燃烧/光照本体零改动）：
列粒度待集 `m_fluidLightPending`（`changedCellColumns` 同列合并，burn
pass 的 fire 格同入集 = 光 15 源）+ 每帧至多
`kFluidLightColumnsPerFrame = 2` 列的 worker 追赶 job（复用 99.10
`columnLightBox`/`planLightJobs`/`dirtyOut` 基础设施）+ 删除巨型同步调用。
机制级补充（fixer 审查确认）：① 跨类同窗排除（流体 job 与流送 job 同窗
必须两两位级独立，否则共写光数组 UB；阻断候选不耗预算留待下帧）②
fluid job 用 `newTerrain=true`（修正 99.9 no-op 跳过在洞内水膜的暗面
漏光）③ POST 时取列（防预算重取 in-flight 列 = 同列双 job 竞态）④
`LightJob.fluid` 标志分账（两类收割互不触碰对方簿记）⑤ 卸载驱逐待集
死列防无界增长。
**确定性/收敛论证**（`light_job.hpp` 头 + 单测 oracle）：每列 re-light
自足（rescan 严格重置竖直 + 单调 strict-increase BFS 升 stale-low，
存储值永不超真值）→ 待集排空 == `propagateAll` 全量；海岸 churn
（~100-170 列/tick）≫ B=2/帧 → 待集饱和于海岸列数（有界）→ 光滞后
有界、逐列收敛；period-2 film 光值近乎不变 = 滞后视觉零差（文档化
偏离：流体光从 tick 同步变 ≤2 列/帧追赶，99.4 pop-in 先例）。
硬规则 7 修订：光照 job = 流送与流体追赶两类各至多 2/帧、同窗两两位级
独立（AGENTS/PROTOCOL 同步）。
**PIPE-4 记档不立项**：step-0 render med 0.5 ms + 扫镜头 draw calls
150→150 无跳变 = GPU 侧数据不支持（99.11 即完整修复）。
验证：1350/1350（+11：预算/合并/B 截断/跨类阻断/收敛 oracle/顺序无关
confluence/burn 列）、`-Werror` 0 警告、门 91 全绿：A 出生点 60f ×2
对 gate88 锚**字节一致**（结构性免疫：出生点 dry highland、无输入零
流体 churn）/ B 流送 300f ×2 字节一致 + **状态 vs v0.8.0 逐位相同**
（FluidSim 本体未动的直接证据）/ **C 新旧二进制 300f 像素 diff = 0**
（追赶 vs 同步 re-light 视觉零差实证）。
性能（发布前测试）：vd16 冷填充 **244.7 s → 44.6 s（5.5×）**；同世界
900f 静置三窗口 402 s/405 s/644 s → **15.5 s/15.5 s/16.2 s（~26×
实时）**、fluid 段 130/396/661 s → **1.0/1.9/2.9 s**（残余 = set 插入）、
update med/p95/max 52/2075/2564 ms → **50/72-92/90-167 ms**（秒级尖峰
消除）；churn 规模（71326/91766/135654 格）与修复前逐 tick 一致 =
流体行为零变更。发布 v0.9.0。

## 系列横向数据

- 测试 1184 → **1195**（+11：thin-shape 纹理回退 / 列完整性 / 存档
  往返重写 / 流体门控回归 / mob 内容钉死 / Frustum ×3 等）
- base18：第 72 次 → **第 80 次**过门（99.x 全免重锚）
- PITS 188 → **193**（+5：99.2×2 / 99.6×1 / 99.8×1 / 99.9×3 中归并
  记录——实际 +5 条见 PITS 99.x 标注）
- 稳态（vd16 环满）：6-7 FPS（环满 update 151 ms + 活熔岩湖）→
  **~51 FPS**（99.8 流体门控）
- vd80 流送中：2-30 FPS 坍缩（render 无界增长 + 单核 100%）→
  **~30 FPS 稳定 + 负载分布**（99.9）

## 残余（Phase 100+ 候选，见 KNOWN_ISSUES）

- 方形环 12 GB（vd80 = 25921 列；MC 球形渲染距离同半径列数 ~1/9）
  ——环形状决策
- 光照 union-region（同 job 相邻 2 box 合并并集 region，省 ~25%）
- worker 网格化（mesher 计算搬离主线程；光照 region 与 mesh 读的
  竞争边界 = unlit 列 3×3 邻域，需快照式设计）
- mob 摆腿/摆臂动画（per-type mesh 静态缓存的限制）

## 变更文件清单（系列合计）

- `src/core/game.{hpp,cpp}`：流送管线（卸载/列完整性/预算/积压上限/
  进度日志/完成通告）、lit pop-in、流体覆盖层、标题 FPS、2 列/帧、
  流体门控维护（m_flammableChunks）、Frustum 接入、列合并渲染、
  newTerrain 传参
- `src/core/math/frustum.hpp`：新（视锥六平面 + AABB 测试）
- `src/world/world.{hpp,cpp}`：columnComplete/ensureColumn、脏列集、
  流体 chunk 集、setBlock 流体失效
- `src/world/chunk.{hpp,cpp}`：流体活动标志（hot/walk-pending）
- `src/world/fluid.{hpp,cpp}`：活动门控三 pass、tick 签名 +flammable
- `src/world/lighting.{hpp,cpp}`：chunk 级重构 + BFS 缓存 + newTerrain
- `src/core/thread_pool.{hpp,cpp}`：（未改——99.9 首测 SIGSEGV 是
  插桩误删 flush 所致，池本身正确，PITS 193）
- `src/renderer/ui_renderer.cpp`（99.5 覆盖层）、`src/window.cpp`
  （99.6 ASCII 标题）、`src/entity/`（99.1 thin-shape 回退、99.7
  腿纹理加载）
- `assets/`：mob JSON ×4 + 腿纹理 ×3 + atlas 清单（99.7）
- `tools/generate_mob_textures.py`（99.7 +3 tile）
- `tests/`：test_world/test_save/test_fluid/test_mob/test_math/
  test_lighting/test_entity（系列 +11 测试）
- `docs/`：KNOWN_ISSUES（条目更新 + 99.x 新条）、PITS（188→193）、
  本存档
