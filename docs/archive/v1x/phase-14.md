# Phase 14 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 14 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-04, Phase 14]` `world.view_distance` 未使用 / 世界边缘固定 / 新 chunk 触发全量重传播 / 光照更新全在游戏线程** — Phase 14 实现 chunk 流式 + 多线程：`ThreadPool`（`src/core/thread_pool.{hpp,cpp}`，固定 4 worker、`post`/`flush` 帧同步点、首个 job 异常在 flush 重抛、析构先跑完 pending job 再 join）+ 纯决策逻辑 `chunk_streaming`（`missingColumns` 距离优先确定性排序 / `nearestMissingDistance` 迟滞输入 / 玩家列 Chebyshev 几何）+ `World::insertChunk`（预建 chunk 插入 + 面邻居标脏防边界 Z-fighting）+ 帧管线（flush → 主线程写 World → 网格化/渲染只读 → post jobs，worker 只运行在 post→flush 窗口）。按"列"（整列 8 垂直 chunk）流式：worker 线程并行调纯函数 `generateChunk` 填 scratch chunk（零 World 接触），主线程集成 + 网格化；`world.view_distance`（默认 8 列）首次生效为流式半径；迟滞激活（≤1 列贴近缺失列 → 激活，半径环充满 → 解除），静止玩家零 job。新列光照 = 单 worker job 顺序 `updateRegion`（相邻区域重叠，光照 job 禁止并发），替代旧的"chunkCount 变化 → 游戏线程 `propagateAll`"（每新 chunk 50 ms+ 尖峰）。新增 21 个单测（ThreadPool 执行/flush 语义/worker 线程/异常传递/析构 join/5000 job 压力；流式几何排序/半径边界/迟滞；insertChunk 去重/邻居标脏/自标脏；增量光照 == 全量传播逐格等价 ×2 场景）+ E2E（无输入基线逐字节不变；seed 57 走 36 格跨边缘停在 1 格台阶、半径 8 环 273 列流满、两次运行截图逐字节一致、2312 chunk 14 s 静止；save 标志运行中性；读档玩家落在未加载区、流式 4 ms 内补地面、两次运行逐字节一致）。新简化记入「Chunk 流式（Phase 14）」节。

## 会话存档（原 SESSION_CONTEXT）

- 实测成本（seed 123456789，探针 `/tmp/opencode/probe_p14.cpp`）：`generateChunk`
  0.196 ms/chunk（8 worker 并行无压力）；`mesher.mesh` 0.010 ms/chunk（主线程可承受）；
  `Lighting::updateRegion`（整列 box）18.75 ms（主线程瓶颈 → 移 worker）；
  `propagateAll`（128 chunk）49.95 ms（旧"新 chunk→全量"的尖峰来源）。
- `src/core/thread_pool.{hpp,cpp}`（新）：固定 worker 数（游戏 4，`kStreamWorkerCount`）；
  `post(std::function<void()>)` 线程安全入队（inFlight++）；`flush()` = 等 inFlight==0
  （job 不 post job，主线程是唯一其他 poster 且阻塞中）+ 首个 job 异常重抛后清空；
  析构 = stopped+notify+join（pending job 先跑完）；workerLoop 空闲等 cv、取 job 解锁执行、
  catch 存首个异常、inFlight-- 后 notify。CMake：`find_package(Threads)` +
  `mc_core` 链接 `Threads::Threads`（PUBLIC）。
- `src/world/chunk_streaming.{hpp,cpp}`（新，纯逻辑）：`ColumnPos{x,z}`（operator</==，
  字典序）/`columnDistance`（Chebyshev）/`missingColumns(player,loaded,radius)`（环内缺失，
  (dist,x,z) 排序）/`nearestMissingDistance`（环内最近缺失距离，全载=radius+1 哨兵）/
  `streamingActive`（d≤radius 且 d≤trigger）。
- `src/world/world.{hpp,cpp}`：`insertChunk(ChunkPos, Chunk&&) -> bool`——已存在=false
  不改动；否则 emplace + self（构造即 dirty）+ 6 面邻居（存在者）markDirty。
- `src/core/game.{hpp,cpp}`：
  - 常量：`kStreamWorkerCount=4` / `kStreamColumnsPerFrame=1`（每帧 post 预算）/
    `kStreamTriggerDistance=1`（激活迟滞）/ `kVerticalChunksPerColumn=kWorldHeight/kChunkSize=8`。
  - 成员：`m_pool`（buildScene 在 propagateAll 后创建）/ `m_streamJobs`
    （`deque<unique_ptr<StreamJob>>`，StreamJob{column, unique_ptr<Chunk[]>, done}）/
    `m_streamPending`（set<ColumnPos> 在飞列）/ `m_streamRadius`（config
    `world.view_distance`，**首次生效**）/ `m_streamingEngaged`（迟滞状态）/
    `m_litChunks`（set<ChunkPos>，光照管线已覆盖的 chunk；buildScene 在 tryLoadSave
    **之后**初始化——否则加载存档多出的 chunk 首帧会灌 128 个光照 job）。
  - update() 重排：flush → integrateStreamedColumns（insertChunk 列，FIFO 确定性顺序，
    每列一条 INFO 日志）→ dayCycle → handleInteraction → fluid tick → updateChunkMeshes →
    player+camera → entities → streamChunks（迟滞判定 + 按 missingColumns 顺序 post 至多 1
    列生成 job：worker 调纯 `m_generator->generateChunk` 填 8 个 scratch chunk，done=true）
    → postLightJobs（`m_litChunks` diff 出本帧新 chunk → 按列合并 box → **1 个 job** 顺序
    updateRegion）。
  - run() 收尾：save **之前** `m_pool->flush()` + reset（job 异常在 flush 表面化；in-flight
    生成列不集成=正确，纯生成地形不入 diff）。
  - 删除：`m_lastLightedChunks` 与 update() 里"chunkCount 变化→propagateAll"块
    （applySave 里的同步行也删，propagateAll 保留）。
- 帧管线不变量（硬性约定 #13）：worker 只在 post→flush 窗口；生成 job 零 World 接触；
  光照 job 每帧 ≤1（区域重叠）；主线程写 World 在 flush 后、post 前。
- 测试 418/418（21 新增，见项目状态）；构建 0 警告。
- E2E（驱动 `/tmp/opencode/p14_e2e.zsh`；probe 脚本 `/tmp/opencode/probe_p14{,b,c,d}.cpp`）：
  - R1 无输入 60 帧（config seed 123456789）：与 `base3.png` **逐字节一致**（静止零 job）。
  - R2 走位（`--seed 57`，1200 帧，锁定指针按住 W 12 s）：玩家 -Z 走 36.8 格（53→35
    一路 1-2 格下降、z=-32 界平、z=-36 处 +1 台阶停，底心 (0.5,36,-34.7) 精确钳制）；
    半径 8 环 273 列流满（2312 chunk），日志 "engaged" + 273 条 "Streamed column"，
    填充完距截图 14 s 静止；**两次运行截图逐字节一致**（md5 同）。
  - R3 walksave（walk + `--save p14w`）：截图与 R2 逐字节一致（save 标志运行中性）；
    存档 = 0 方块/流体/实体编辑（流式 chunk=纯生成地形，diff 正确）、
    玩家 (0.5,36,-34.7) yaw 0 pitch -0.25、time=0.3333333333333339。
  - R4 读档×2（快照/恢复）：**逐字节一致**且与 R2 截图一致（正午平台+太阳出画）；
    读档玩家落在未加载列 (0,-3)、触发器 4 ms 内激活、首列 (0,-3) 在 ~2 帧集成、
    273 列流满、最终 2312 chunk（"玩家快于流式"路径验证）。
  - 注意：load 运行退出会写回存档（time 前进 1/12 天），确定性对比必须快照/恢复。
- 刻意简化（KNOWN_ISSUES「Chunk 流式（Phase 14）」节）：只增不卸载（内存随探索增长，
  半径 8 环 ≈35 MB）；新列首帧暗 1 帧（光照 job 在 mesh 后，帧锁定确定性 pop-in）；
  增量光照不能"消光"（封洞伪影，窄场景）；触发迟滞（静止玩家永不流式=基线确定性）；
  读档点未加载时先下落几帧；网格化留在游戏线程（预算限界 ~0.08 ms/帧）。
- 归档（KNOWN_ISSUES「已修复」）：view_distance 未使用 / 世界边缘固定 / 新 chunk 全量重
  传播 / 光照更新全游戏线程 / 光不传播进未生成区域（5 条合并为 P14 归档条目）。
