# STRM-2 流体列保留（v1.1.0，门 116）

v1.x 任务矩阵第一行（docs/BACKLOG.md）。出处 = KNOWN_ISSUES「只增不
卸载」条残余①：仅流体模拟改动（远端熔岩流动无 fire 条目）的列卸载后
重置为生成态（v1 曾接受：存档在玩家身边写，远端 diff 本就只覆盖已加
载列）。

## 根因（两处）

1. **主根因（驱逐丢失）**：`Game::unloadFarColumns` 卸载圆盘外列时
   `m_saveJournal.evictColumn(col)` 直接丢该列全部 journal 条目——纯
   流体列（无 block 编辑/五类 per-block 状态/桶操作，经
   `World::setFluid` 绕过 Game setBlock choke point 的写：流体模拟、
   燃烧 pass、水熔岩反应）没有任何保护资格 → 列回归生成态，退出存档
   随之丢失（journal 已驱逐，journalDiff 只见现存 chunk）。
2. **潜伏活性缺陷（99.8 活动门 × 流送边界，场景验证中插桩实证）**：
   99.8 流体活动门（`simulateTypeChunks`：chunk 仅 HOT/PENDING 才
   walk）下，一个 settled（冷）流前对着 void 的 spread 被
   `World::setFluid` 拒绝（"the void stays dry"）→ 该 tick 零写入 →
   chunk 转冷；**新 chunk 流送集成（`World::insertChunk`）不触碰任何
   fluidWalkPending** → 流前永久卡在流送边界（实证：石砌熔岩渠道级联
   L8→L7→L6→L5，tick 4 propose 越界被拒后 chunk 转冷，目标列 frame ~15
   已集成，其后 1185 帧/395 tick 级联纹丝不动；反向同病 = 带生成海
   的新 chunk 插入后自身冷，不与邻接 settled 流体互扰）。

## 修复（定案）

**主修复 = 驱逐时快照、再集成时回放、退出存档合并**（保留的是 diff
条目，不是 chunk——内存有界；"整列保留"替代方案 = ~128 KB/列 永久驻
留，否决）：

1. 纯函数 `retainColumnDiff`（save.cpp，直接复用 `journalDiffChunk`
   逐格对比 = 与 journalDiff 字节同语义）：列的 journal 两表限域，
   live≠fresh 的格快照为 `RetainedColumnDiff{blocks, fluids}`（pos
   排序；全还原列 = 空 diff 不入表）。
2. `Game::m_retainedColumns`（map<ColumnPos, RetainedColumnDiff>）+
   不变式：**每列至多在 journal 或 retained 之一**（unload 先快照后
   evict；integrate 先应用后 erase）→ 退出存档合并无重复格。
3. `unloadFarColumns` 钩子（先于 `m_world.eraseChunk`——快照读 live
   chunk）：journal 两表全空的列 = 双 map 查找零成本快路径（绝大多数
   卸载列 = 纯生成地形）。
4. `integrateStreamedColumns` 钩子（unlit 注册后）：命中保留表 →
   先 blocks 后 fluids（applyWorldEdits 同序：block 排挤 fluid）经
   `m_world.setBlock/setFluid` 应用（observer 自动再 journal 化 =
   下次卸载再快照，自相似循环）→ erase。**不插 m_editedColumns**
   （整列永久驻留 = 内存回退；fluid-only 列保持可卸载）。光照 = 列既
   在 m_lightPendingColumns/m_unlitColumns（新地形强制传播），同帧
   postLightJobs 覆盖，无 lit-pop-in。
5. `collectSave` 双路径（journal + --save-full-diff legacy oracle）均
   `mergeRetained`（append + 重排序；空表 = pre-STRM-2 位级形状）。
6. **99.8 活性补洞**：`World::insertChunk`/`getOrCreateChunk` 镜像
   setBlock 的 UNSTICK 范式（world.cpp:54-90 注释"A block change can
   UNSTICK fluid"）——新 chunk 出现后对两 kind：自身（持流体时）+ 6
   面邻（持该 kind 流体时）`invalidateFluidWalk`。一次 walk 代价有界
   （streaming 节奏 2 列/帧；settled 格只读再冷）。`insertSnapshotChunk`
   （worker 快照 World，maintenance indices 不被读）不改。

零 format（19）/零 config/零内容/零音效/零纹理；读档路径零改动
（`applyWorldEdits` 既有 getOrCreateChunk + 顶满列 + m_editedColumns
保护即可消费含保留 diff 的存档）。

## 测试（1677 → 1686，+9 = 主体 6 + 活性 3）

主体（tests/test_retained_column.cpp，RetainedColumnDiffTest）：
1. `SnapshotsOnlyTheJournalCellsSorted`（限域 + 排序）
2. `AllRevertsYieldAnEmptyDiff`（空 diff 不入表语义）
3. `MatchesJournalDiffPerColumn`（与 journalDiff 逐条相等 = 字节同语义）
4. `MergeSortsTheInterleavedUnion`（合并全局排序 + 无重复格）
5. `MergeOfAnEmptyMapIsANoOp`（pre-STRM-2 位级形状）
6. `RetainedShapeSaveRoundTrips`（retain→merge→serialize→parse→
   applyWorldEdits 全列逐格一致，含三种合法 cell 对形）

活性补洞（tests/test_fluid_streaming_gate.cpp）：
1. 前沿跨边界（根因判别，pre-fix FAIL 实证）
2. 生成海互扰（反向）
3. 冷态零唤醒回归（99.8 性能面不回退）

## E2E 门 116

场景（scratch 探针定几何 + 逐段步行冒烟实证，seed 123456789 真实资产，
vd8 场景 config；级联经 300f 冒烟实证 = 4 tick 落定 10 格膜）：
熔岩池 L8 3 格 @ 列 (11,0)（x=186..188 y=46）+ 石砌渠道/围墙 19 格
（全在列 (11,0) → 保护列）；级联 L7..L5 沿渠道东流，**跨列边界在
C=(12,0) 落 10 格对角扇**（z=0 主线 (192,46)L4/(193,46)L3 + 两侧折线，
L1 终止缘；+z 侧被天然沙面 y=46 挡 = 不对称；扇底全 sand/dirt 非
flamable、6-邻无可燃 = **纯流体 diff、无 fire 条目** = 列 C 无保护
资格）。
步行基础设施（z 三行 −1..+1 抗转向后 z 漂移【实测 0.5→−0.9】，全在
列 3-9，非 C）：3-row 石板路（y=35 top 36，x=53..159）+ 东出口阶梯——
**玩家无法从 1 深水中 step-up 上岸**（player.cpp:349 stepUp 门
`m_onGround`；泳态 snap 脚位恒 = 水顶−0.9，岸顶 > 脚位即墙，+0.9 也
是墙；实测 6 轮卡 x=45.7/43.7/48.7/49.7）= 出水须"河床缓升平底出 +
上岸后旱地阶梯"：x=46 挖 (46,34) 岸顶 34 = 泳脚 34.1 平入 → x=47 挖
(47,35) + x=48 石（P35 宽 2）→ x=49 **天然悬浮草洞**（挖草 = 顶落 33
成 3 深坑，实测坠卡 y=33）= 填 (49,34)(49,35) 石 + 挖草 → x=50 挖
(50,36)（P36 宽 2）→ x=51 天然 37 + x=52 石（P37 宽 2）→ x=53..159
板（1 降平走）。每级 +1 平台**宽 ≥2** = P16 宽 1 +X 阶免疫（首轮
单格桥/窄台历次实证 P16 卡死）。转向点 x≤22（水带西端旱地，
dist(C)=11 > 9 = C 卸载）；东终点 x≥155（板行至 172.7 天然凹陷 =
确定性停点）。共 368 块编辑 + 3 流体编辑。场景设计坑全录 = PITS
276-279。

- **S1 PASS**：fresh 60f 无输入 ×2 RAW（new 对）+ new≡old v1.0 同批字节
  一致 = **第 116 次结构性免疫**（出生点无输入永不流送 PITS 261 → 无
  integrate/unload → 保留表恒空 + 活性补洞只在新 chunk 插入时触发 = 双
  机制结构惰性）。首轮 vs base18 锚（ca639e50）差异 = PITS 265 远云带
  机器态类（与变更无关，四批同构）→ 门收后 base18 锚刷新
  ca639e50→9c64b0f8（= s1_new_a.png）。
- **S2 三跑**（输入协议 10-26 定案 + 门 116 补强：away 组件换窗 XID =
  WID 每次点击按 PID 重解【PITS 273】；pkill 输入跑 = X 键态残留须跑前
  清键【PITS 275】；转向/终点判定 = **F5 位置探针**（帧精确、fps 无关）
  而非墙钟偏移【PITS 272】；存档为确定性权威）：
  - **Run A PASS（4/4）**（NEW ×2 / OLD v1.0 ×2，6300f）：西走断言
    全过——NEW 退出存档 C 10 格扇膜在档 ×2（unload 时 journal diff
    快照保留 + 退出 mergeRetained 合并）∧ 渠道对照 (189-191,L7/6/5)
    双侧 3/3 ∧ C 列零 block 编辑（纯流体）∧ 四跑玩家终位逐位同
    (22.3,38,0.5)（x≤47，dist(C)≥10 = 已卸载）；OLD ×2 C 膜缺（现行
    v1 接受行为反例）。
  - **Run B PASS（5/5）**（NEW ×2 / OLD ×2 / CTRL=NEW+OLD 场景 ×1，
    无输入 1200f，载 Run A 退出存档）：NEW 侧 C 膜在档（读档回放 =
    既有 applyWorldEdits 顶满列机制，零新代码）；OLD/CTRL 侧缺；
    ×2 savenorm EXACT/侧 + CTRL≡OLD（特性惰性对照）。
  - **Run C PASS（4/4）**（NEW ×2 / OLD ×2，5400f，XTEST 标定 180°
    转向 dx180=1256.64）：F5 位置探针双相步行——西相 83.8→55.4→
    32.4（z=0.5 恒定，转向点 dist(C)=10 = C 卸载 + 保留）→ 转向
    （yaw 精调免纠）→ 东相 50.7→79.1→107.5→135.8→164.5（z 微漂
    0.47→0.29 在三行 deck 内，终点 col 10、dist(C)=2 = C 回流加载）。
    NEW ×2 退出存档 C 膜 10 格在档（**会话内 卸载→保留→回流→
    integrate 回放 全链实证**）∧ C 区流体/块 NEW 对逐格 IDENTICAL；
    OLD ×2 膜缺（C 回流 fresh = 现行行为）。四跑步行轨迹逐探针一致
    （跨二进制确定性）。
- **S3**：套件 1686/1686（+9）。
- **S6**：无新解析面（零 format/config/内容）→ 无 S6。
- **性能 A/B（收）**（用户硬要求，发布前必做）：fall 900f 三窗
  （tm_v0320_src 插桩基线 vs tm_v110_src STRM-2 插桩树，同场景同
  config，各 GREEN try 1）：

  | 指标（win1/win2/win3） | OLD | NEW |
  |---|---|---|
  | fluidCells | 123879 / 167781 / 236573 | 123879 / 167782 / 236575 |
  | upd med ms | 47.4 / 49.6 / 52.0 | 47.4 / 49.5 / 48.0 |
  | render med ms | 0.376 / 0.574 / 0.773 | 0.354 / 0.540 / 0.721 |
  | swap med ms | 0.485 / 0.512 / 0.525 | 0.460 / 0.478 / 0.512 |
  | flush med ms | 36.7 / 35.9 / 33.6 | 37.7 / 34.7 / 30.1 |
  | fluid ms | 1270.7 / 2572.6 / 3913.8 | 1279.2 / 2539.6 / 3796.5 |
  | stream ms | 1929 / 2233 / 2564 | 1857 / 2116 / 2446 |

  判读：upd/render/swap 同档或更优、flush −3..−10%、stream −3..−6%；
  fluidCells win1 逐位一致、win2/3 +1/+2 = **99.8 活性补洞的行为
  语义**（静态流梯度提前一格越流送边界——pre-fix 该前沿永久卡边，
  fall 场景海岸/洼地边界处一次越界 = 1-2 格终态差，非 churn 放大；
  量级判别 = 每窗 1-2 格 ≪ churn 数百格）。结论 = 成本可忽略，无
  回退。

## 偏离/记档

- 保留表条目 = 驱逐时刻的 diff（该列流体此后不再模拟——列已卸载，
  流送回来前状态定格；与 v1 既有"远端 diff 只覆盖已加载列"的存档
  语义对齐，现在改为"覆盖已加载 + 已驱逐保留列"）。
- 99.8 活性补洞 = 独立潜伏 bug 随 STRM-2 场景首触发修复（矩阵外，
  KNOWN_ISSUES 记档）。
- 长会话保留表上界：每保留列 ≤ 该列 journal 格数（典型几~几十格；
  极端 = 整列灌满 32768 格 ≈ 256 KB/列），玩家尺度可忽略。

## 坑位（PITS 271-279 追加）

- **271** 99.8 活动门流送边界活性缺口（setFluid void 拒后冷 chunk 不
  被 insertChunk 唤醒；CellReader 缺失读 air = 表面条件全满足而写入
  静默失败，排障须探针 setFluid 拒绝分支）。
- **272** 帧锁定物理 × 低真实帧率 = 墙钟输入计时失真（1f = 0.08-0.09 s
  墙钟 @11-13fps；转向/终点判定必须 F5 位置探针化，--frames 按最坏
  fps 算）。
- **273** away 组件换窗 XID = 缓存 WID 静默失效（按 PID 重解 + 必建
  TM_PRE_WINDOWS 快照取 TM_GAME_CLASS，否则 unmap_covers 守卫短路
  反向可伤 SDL3 helper 窗）。
- **274** `--new` 必须带 `--save <slot>`（rc=2 Argument error）。
- **275** pkill 输入跑 = X server 键态残留（下一"无输入"跑静默变步行
  跑；跑前 `xdotool keyup w W a s d`，杀输入跑后立即清键）。
- **276** 场景桥/路面高度按"实际接近侧"定（首轮只核西口忘东涉水口 =
  整跑白走）。
- **277** 玩家无法从 1 深水中 step-up 上岸（stepUp `m_onGround` 门 +
  泳态 snap 脚位恒 = 水顶−0.9，岸顶 > 脚位即墙；出水 = 河床缓升平底
  + 上岸后旱地阶梯）。
- **278** 场景降顶/挖格前必须垂直剖面探针（x=49 悬浮草下 3 格空腔 =
  挖草成 3 深坑坠卡；surface 探针只给 top）。
- **279** 转向后 z 漂移 + 甲板车道宽度（XTEST 180° 后 z 0.5→−0.4..
  −1.3，单行 deck 漂出落回地形 P16 卡死；三行 −1..+1 车道结构性免疫
  + 转向后 yaw 精调免纠）。
