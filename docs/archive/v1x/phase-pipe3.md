# PIPE-3 流体 per-cell（v1.2.0，门 117）

v1.x 任务矩阵第二行（docs/BACKLOG.md）。出处 = KNOWN_ISSUES「流体模拟 =
chunk 粒度活动门控」条：「粒度是 chunk 非 cell：一个 16×16×128 chunk 里
只有 1 格流动也走全 chunk（32K 格）；MC 的 per-cell 队列更省但重写 tick
架构，留待性能成为瓶颈时」。

## step-0 实测（v1.1.0 基线，tm_pipe3_s0 插桩，20Hz tick = 每 3 帧）

| 场景 | visited med/max | 流体格 med/max | changesApplied | tickMs med/max |
|---|---|---|---|---|
| fall 900f win1 | 20 | 235 | 1 | 1.17 |
| fill 7000f win1-4 | 20 / 16-20 | 243-251 | 0-4 | 0.95-1.71 / 1.25-2.34 |
| fill win5-6（沉降） | 2 | 0 | 0 | 0.30-0.48 / 0.49-0.91 |

- 活跃窗成本 = 20 visited × (2 遍 32K 全扫 + 32KB falling 数组堆分配)；
  真实流体格命中率 250/655360 ≈ 0.04%（每 chunk ~12.5 格 / 32768）。
- 沉降窗成本下界 = simulateTypeChunks 每 tick 遍历**全部**流体 chunk 集合
  做 hot/pending 双检（vd80 海世界 = 数千 chunk）→ 0.3-0.9 ms/tick 常驻。
- 多 tick 流动持续推进机制 = `Chunk::setFluidAt` 对旧新两 kind 置 hot
  （reheat 唯一来源；flag 消费在 walk 点）——C 设计须穷尽镜像。

## 设计（定案：B + C + tick 重写）

- **B per-chunk per-kind 流体格列表**（Chunk::setFluidAt/clearFluid 维护，
  0→n push、n→0 lazy stale、tick 过滤+列序排序+去重、50% 压缩）：tick
  两遍 32K 全扫 → 仅活格；reaction/burn pass 32K 扫 → 活格扫。
- **C per-kind 活动 chunk 集合**（World，6 插入点穷尽 = setFluid 双 kind /
  setBlock 排水 / setBlock UNSTICK / wakeFluidChunks / scanFluidChunks +
  eraseChunk purge；walk 点消费）：visited 构造 O(全部流体 chunk) →
  O(活动 + 邻)；flag 双检保留（belt）。
- **tick 重写**：falling 标志 = per-column colFlag[256]（列内自底向上依赖
  = 收集序保证）+ 跨 chunk 边界 topFlags[256]（替代 32KB fallingByChunk
  数组 map；引用语义 = 现行 map-miss 0 不变）；propose O(n) 线性合并 →
  map max 合并（序无关 = 位级同果）。
- **位级等价**：非流体格现行两 phase 零作用；fc 序 = 现行循环序限流体格；
  visited 集合经穷尽插入点同构；apply 排序+净变更检查不变 → 流体状态
  逐位不变（E2E S2 = 新旧二进制位级一致 oracle 终裁）。
- 零 format（19）/config/内容/音效；flag 语义保留（既有测试钉面）。

## 实现（fixer 交付，编排者逐 hunk 核证毕）

8 文件 + 1 新测（1686 → 1695）：

- `chunk.hpp/cpp`：`m_fluidCells[3]`（per-kind 活格列表，local index；slot 0
  空）；`setFluidAt` 参数 level→code 名实归位 + **push 条件 = 成员变化**
  （0→n 或换 kind；同 kind 换级不 push、drain 不删 = lazy stale）；
  `clearFluid` 三 vector 清空（scratch 再生成路径依赖）；`fluidCells(kind)` /
  `compactFluidCells(kind, live)`。顺带修潜伏 debug assert（旧
  `assert(level <= kMaxFluidLevel)` 对 lava 编码 9..16 必炸；参数本就是
  code → `code <= 2*kMaxFluidLevel`；NDEBUG 构建零变化）。
- `world.hpp/cpp`：`m_fluidActive[3]`（per-kind 活动 chunk 集合 = 99.8 flag
  的 set twin）；6 插入点穷尽镜像全部 flag 置位点——setFluid 成功路径双
  kind（code≠0 + level>0）/ setBlock 排水分支 / setBlock UNSTICK（目标双
  kind 无条件 + 6 邻同 gate）/ wakeFluidChunks / scanFluidChunks /
  eraseChunk 双 kind purge（+ clear() 同步 = 规格 f 的穷尽性补全）；
  `consumeFluidActive`（walk 点 = flag clear 同款点，无条目 no-op）+
  `fluidActiveChunks` const 访问器。
- `fluid.cpp`：`simulateTypeChunks` 遍历源 fluidChunks→fluidActiveChunks
  （集合快照迭代防 walk 消费失效；flag 双检保留 = stale 条目 belt 消费）；
  `columnOrderKey`（(lx*16+lz)*128+ly = 旧 (lx,lz,ly) 循环序限流体格，i
  上双射）+ `collectFluidCells`（活格过滤 + 列序 + 去重 +
  filtered*2<list.size() 或去重掉了即压缩）+ `collectLiveCells`（reaction/
  burn 用，序无关仅过滤）；`simulateType` 重写 = 收集 → Phase 1'
  （colFlag[256] 列内自底向上 + topFlags[cp]=colFlag 跨 chunk 边界，
  miss=0 同旧 fallingByChunk map-miss）→ Phase 2'（规则体逐字，
  falling[i]→flags[k]）→ propose `std::map<BlockPos,uint8_t>` max 合并
  （序无关 = 旧线性合并同果；pos 序 = 旧排序后 changes 序，sort 保留 =
  最小 diff）→ apply 逐字；reaction/burn 32K 三元循环 → 收集迭代（邻读
  逐字）；删 fallingByChunk 32KB 数组 map + 两遍 32K 循环 + 线性 propose。
- `fluid.hpp`：tick 复杂度注释更新。
- `tests/test_fluid_cell_list.cpp`（新，9 测）：列表维护（wet/drain/rewet/
  compact/kind 换/clearFluid）/ 跨 chunk y 边界 falling 链（topFlags 判别：
  缺读 = 池沉积无变化 = 终态可分）/ 排干 chunk 走一次即冷（活动集
  生命周期）/ setBlock 排水保活 / 卸载 purge / reaction+burn 跨 seam 收集
  精确终态 ×2 场景 / churn 200 tick 列表 ≤2×活格 / 手算 13 格双 tick 参考 /
  stale 条目 belt 消费（白盒构造：UNSTICK 插 + 带外清 flag）。
- `tests/test_mesh_job.cpp`：`is_trivially_copyable_v<Chunk>` static_assert
  放宽为 copy/move-constructible（vector 成员必然；快照插入 = 普通拷贝、
  快照列表从不被消费 = 位级不变语义同）。
- `CMakeLists.txt`：新测注册（显式路径 1 行）。

**规格偏离（fixer 申报，核证接受）**：compact 调用点传拷贝非 move（签名
值参，多一次小 vector 拷贝 = 可忽略）；reaction/burn 收集不去重（重复
setBlock = no-change + tick 去重 + journal observer 忽略空 mutation，论证
成立）；World::clear() purge（规格 f 未列的穷尽性卫生）。

## 测试（1686 → +N）

（待填）

## E2E 门 117

- **S1 PASS**：fresh 60f 四方（NEW×2/OLD×2）字节一致（387726 B）+
  base18 锚（9c64b0f8）连续 = **第 117 次结构性免疫免重锚**（出生点 dry
  highland 无输入零流体 churn = 活动集合空 + 列表仅生成期写入 = 双机制
  惰性）。
- **S2 位级一致 oracle PASS**（全无输入，3 场景）：
  - **fall 900f**（fire9999 = 无 burnout = spread 纯帧锁定位稳）：跨侧
    savenorm EXACT + 截屏 RAW **310980 B 字节同**（亮对）+ 侧内 ×2
    savenorm EXACT/侧（RAW 差 = 仅 fires[].remaining real-dt，PITS 244
    类）。
  - **fill 3000f fresh**（vd80 用户 config，流送填充期 = wake/scan 插入点
    密集）：跨侧 savenorm EXACT + 截屏 RAW **390588 B 字节同** + ×2
    savenorm EXACT/侧（同 fire real-dt 条款）。
  - **cascade 600f**（gate-116 熔岩级联，reaction + burn + 双 kind
    churn）：跨侧块日差别差 = **恰 1 格 fire-spread 燃尽 air
    (32,35,21)**，流体位级一致；**同 OLD 二进制 3 跑（同 fps 11.8-12.1）
    亦 1 vs 2 分歧** = fire burnout real-dt × 帧锁定 spread 的非位稳既有
    类（PITS 283，非 PIPE-3 回归）。
  - **cascn = cascade + fire.spread_chance 0（位稳 oracle）**：跨侧存档
    **RAW 854590 B 逐字节同**（无需剥离 = 最强证据：reaction/burn/双
    kind 仿真路径全验）。
- **S3**：套件 1695/1695（+9）。
- **S6**：无新解析面 → 无 S6。
- **性能 A/B（收）**（用户硬要求，发布前必做）：双侧插桩树
  （tm_pipe3_s0 = v1.1.0+插桩 / tm_v120_perf = v1.2.0+插桩，9911-STEP0 +
  PIPE3-S0 双计数，各 0 警告）：

  fall 900f（海岸 churn + streaming，vd80）三窗：

  | 指标（win1/win2/win3） | OLD | NEW |
  |---|---|---|
  | fluid 段 ms | 1261 / 2486 / 3785 | 631 / 1267 / 1880（**−50%**） |
  | tickMs med/max（win3 活跃窗） | 34.8 / 61.8 | 12.6 / 24.9（**2.8×**） |
  | fluidCells | 123879 / 167782 / 236575 | 同（**逐位一致**） |
  | upd med ms | 46.6 / 49.6 / 47.4 | 46.1 / 47.1 / 50.0（同档） |
  | render med ms | 0.376 / 0.564 / 0.740 | 0.380 / 0.572 / 0.772（同档） |
  | swap / flush / stream med | 同档（±5% 机器噪声内） | |

  fill 7000f（vd80 fresh 流送填充）七窗（PIPE3-S0 tickMs med）：

  | 窗 | OLD | NEW | 说明 |
  |---|---|---|---|
  | 1-4（churn：20 visited / 251 格） | 1.62 / 1.53 / 1.87 / 2.33 | 0.42 / 0.31 / 0.38 / 0.38 | **−77..−84%** |
  | 5-7（沉降：old 2 visited 0 格 / new 0） | 0.30 / 0.20 / 0.19 | 0.16 / 0.13 / 0.10 | **−45..−60%**（下界消除：全集合遍历 + 2 个可证 no-op 访问） |

  判读：fluidCellsSeen / changesApplied 逐窗相同 + 9911 fluidCells 逐位
  一致 + S2 位级 oracle = **无行为变更、纯收益**（−50..−84% 流体段，帧
  指标全同档）。old 沉降窗 2 visited / 0 cells = 可证 no-op 访问（零流体
  格 = 零 proposal 的数学恒等），new 活动集合天然不携 = 顺带消除。

## 偏离/记档

- 保留 99.8 活动 flag 双检（belt）：活动集合是 flag 的 set twin，flag 仍
  是权威（既有测试钉面 fluidHot/fluidWalkPending 零改动）。
- setBlock UNSTICK 目标 chunk 无条件双 kind 插入（可能不持 kind = 一次
  no-op walk）= 与现行 flag 语义逐字镜像（保守无害，StaleInsertConsumed
  钉死）。
- reaction/burn pass 收集不去重（重复 setBlock = no-change + tick 去重 +
  journal observer 忽略空 mutation，论证在 fluid.cpp 注释）。
- 顺带修 `Chunk::setFluidAt` 潜伏 debug assert（参数名 level 实收 code：
  lava 编码 9..16 > kMaxFluidLevel 8 必炸；NDEBUG 构建零变化，仅
  assert 修正 + 参数名归位）。
- `test_mesh_job` 的 `is_trivially_copyable_v<Chunk>` static_assert 契约
  演进 → copy/move-constructible（vector 成员必然；快照列表从不被消费 =
  位级不变语义同，注释钉死）。
- old 沉降窗的 2 个 no-op 访问（0 流体格 = 数学零 proposal）在新实现
  自然消失 = 性能收益的一部分（非行为变更）。
- 黑窗环境态（PITS 282）：S2 期间数分钟级黑帧周期，存档权威不受影响，
  像素对 = 亮窗对（fall/fill 皆亮对 RAW 字节同）。

## 坑位（PITS 280-283 追加）

- **280** scratch 树漏 data/saves → run2 场景拷贝静默失败 = fresh 假绿
  （cp 失败非致命 + 截图照常 GREEN + 数据仍"合理"掩盖；判别 = 跑后存档
  编辑计数 vs 场景源）。
- **281** pgrep -f 自匹配 = 等待循环永挂（模式字符串出现在等待 shell 自身
  命令行；PITS 259 第 4 次复现）。
- **282** away 组件黑窗态 = 数分钟周期（同场景同帧同二进制亮/黑随窗态
  翻转；帧照样完成 = 存档权威不受影响 → 黑跑存档照收 + 像素降级
  SKIPPED + 长跑批前亮窗热身探针）。
- **283** fire 蔓延终区 = real-dt burnout × 帧锁定 roll 非位稳类（同 fps
  同二进制跨跑 1 vs 2 分歧；位级 oracle 纪律 = lifetime 9999 无 burnout
  范式 或 spread_chance 0 范式；savenorm 剥 fires[].remaining 不够——
  燃尽 air 在 blocks 段；PITS 244 衍生）。
