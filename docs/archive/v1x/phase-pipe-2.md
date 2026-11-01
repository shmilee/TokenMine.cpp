# Phase PIPE-2 — 存档增量（v1.0 任务表 第 3/29 项）

> 2026-10-21（夜）。来源 = `docs/BACKLOG.md` PIPE 行第 2 项：「退出全量
> diff → 增量编辑跟踪 O(编辑数)（P37 优化方向）」。PIPE 行 2/2 全清 =
> v1.0 任务表 3/29 完成。

## 基线（实现前实测，免插桩——既有日志时间戳提取）

- **vd80 全环 legacy 退出 = 6.2 s**（step-0b run 提取：`Frame limit
  reached` 04:12:19.050 → `World saved` 04:12:25.246；102,824 chunk /
  303 block + 1226 fluid 编辑）
- 构成（ora-1 推导 + 锚点）：① 楼梯 O(N) 世界扫描 ~200-800 ms（421M 格读
  推导值）② diffWorld 全量 diff 3.5-4.5 s（P33 锚点 0.14 ms/chunk 单线程，
  P37 4-worker 3.7×）③ 排序 + JSON + 落盘 ms 级
- **收益本质**：退出成本 = f(会话编辑数)，与视图距离/环面积脱钩——STRM-1
  后环越大本项收益越大（R_eff=30 档同理）

## 实现内容

- **World 层 cell-mutation observer**（world.{hpp,cpp}）：
  `CellMutation { BlockPos pos; BlockID block /*invalid=无块变更*/;
  optional<uint8_t> fluidCode /*nullopt=无流体变更，写后编码，0=排空*/ }` +
  `setCellMutationObserver`（m_generator 回调同款范式）。notify 点恰两个：
  setBlock 成功点（块字段仅 before≠id 时；**位移陷阱双上报**——内部
  chunk.setFluidAt 排空不走 setFluid，在 setBlock 处一并上报 level-0）+
  setFluid 成功点（值未变 false 静默）。FluidSim 三 pass 绕过 Game 收口
  直写 World = observer 挂 World 层才能抓住（99.2 残余① 同款根源）。
- **`SaveJournal`**（save.{hpp,cpp}）：`map<ColumnPos, set<BlockPos>>`
  块/流体双表（floorDiv 列键 = 坐标纪律 #1）+ entries 计数 + valid 标志；
  `recordBlock/recordFluid` 越软上限 `kSaveJournalMaxEntries = 262144`
  恰好一次失效 + INFO 日志（退出降级 legacy）；`evictColumn`（卸载驱逐）。
- **安装时序**：buildScene **顶部、先于 tryLoadSave**——demo 生成走生成器
  回调 chunk 级直写（不触发），读档重放 applyWorldEdits 经
  World::setBlock/setFluid = **自动播种零额外代码**（PITS 有录）。
- **`journalDiff`**（save.cpp）：K = journal 触及 ∧ 现存 chunk；每 chunk
  用 makeChunkGenerator() **同一回调**重生成参考（generateChunk 纯函数，
  与 diffWorld 参考构造方式同一）→ 仅比对 journal 格 → live≠fresh 才出
  编辑（逐位复刻 diffChunk 编码 + fluidKindOf(0)=Water 特例）→ pos 全序
  排序。**P37 并行部件复用**：一次性 4-worker 池（kSaveDiffWorkers）、
  K>256 判据（kSaveDiffParallelMinChunks 对 K 而非全世界）、每 job 私有
  part + 全序合并字节一致论证。
- **楼梯 O(N) 世界扫描 → journal 派生**：遍历 journal 块格，live 块
  shape==Stairs → {pos, 朝向（哨兵回落块默认，与旧扫描同一逻辑）} + pos
  排序。支撑不变式 = **worldgen 永不生成楼梯**（真实 assets 3×3 列 ×8 层
  72 chunk 逐格扫描 0 个 Stairs 格 + 非空 >1000 守卫，单测钉死）。
- **`--save-full-diff` CLI 旗**（main.cpp，无 config 键无 format 影响）：
  强制 legacy 路径 = 进程内等价 oracle + 用户逃生锤。
- **format 一字未动**（blocks/fluids/stairs 数组形状/语义/排序纪律）。

## 变更文件

- `src/world/world.{hpp,cpp}`（CellMutation/observer/setBlock+setFluid
  notify 点）
- `src/core/save.{hpp,cpp}`（kSaveJournalMaxEntries/SaveJournal/
  journalDiffChunk/journalDiff）
- `src/core/game.{hpp,cpp}`（GameArgs.saveFullDiff + m_saveJournal +
  buildScene 安装 + unloadFarColumns 驱逐 + collectSave 双分支）
- `src/core/main.cpp`（--save-full-diff 解析 + usage）
- `CMakeLists.txt`（tests + test_save_journal.cpp）
- `tests/test_save_journal.cpp`（新增 ~470 行，7 TEST）
- `docs/`（KNOWN_ISSUES 存档条 + 99.2 残余① 注记 / PITS +2 / ROADMAP /
  BACKLOG 勾选 / AGENTS / 本存档）

## 测试

1212/1212（1205 基线 + 7 新，无禁用/删除），`-Werror` 0 警告：
1. observer 收口捕获（setBlock 块变更/同 id 静默/流体格排空双上报/
   setFluid 值未变静默/越界 y 静默）
2. 读档自动播种（fixture save → applyWorldEdits → journal 含全部编辑）
3. 卸载驱逐（evictColumn 两表 + entries 回退）
4. **等价 oracle**：9 类变更 fixture（同值回改/挖掘/放置/桶倒水/排空
   Water 特例/位移双上报/燃烧级联/楼梯放置+旋转/远列编辑后 chunk 消失）
   双阶段（不驱逐 + evictColumn 后）→ journalDiff == diffWorld **逐字节**
5. 软上限 fallback（越线 → valid=false 恰好一次 + legacy 门控）
6. 楼梯不变式（真实 assets worldgen 无 Stairs 形状格）
7. 并行 == 串行（K>256 fixture）
（Game 级接线 = buildScene 安装/collectSave 分支/unload 循环：Game 不可
单测实例化——组成件全覆盖 + E2E 存档往返端到端。）

## 构建与验证结果（E2E）

### 调试发现（首轮黄金 A/B 的 journal "漏记" = 既有缺陷，非 journal bug）

- 首轮（fire.spread_chance 默认 0.05）：A（journal 新二进制）blocks=303 vs
  G（legacy 基线二进制）=304——journal 独缺 (24,35,19)='air'（相邻焦土格
  全在；fluids/stairs 三跑全同）。排查链：fires 数组两跑皆 0（都燃尽）→
  JDEBUG 全量变更轨迹（新二进制 1200f：该格**零写入**；焦环其余格各 1 写）
  → 结论：**该格在新二进制跑中根本没被烧**（蔓延没到）。根因 = 既有
  **real-dt 火寿命（P64）× 帧锁定蔓延 roll（P66，每 6 帧/火）= 一簇火寿命
  内 roll 次数 = 4 s × fps**——两二进制主线程负载不同 → fps 不同 → P90
  表面湖焦土环跨二进制差边界格（PITS 已收录）。C（读档往返）304/1226
  与 G 全同 = journal 记录/播种/过滤路径对同世界输入字节完备（反证 journal
  无漏）。
- **黄金 A/B 重跑 = scratch config `fire.spread_chance: 0`**（P74 "spread0"
  先例；直烧 = 帧锁定熔岩 burn pass，fps 无关）：4 跑（A2 journal / B2
  --save-full-diff 同 build 对照 / G2+G3 基线 ×2）——4 跑结果见下。

### E2E 结果（nospread 黄金批 + 读档往返）

- **A2/B2/G2/G3 六对两两 blocks/fluids/stairs 三数组全字节一致**（243 block +
  1226 fluid + 0 stairs，4 跑全 rc=0）：
  - A2 vs G2/G3 = 黄金跨 build（journal 路径 vs legacy diffWorld，不同二进制）✓
  - A2 vs B2 = 进程内 oracle（同 build journal vs `--save-full-diff` 强制 legacy）✓
  - G2 vs G3 = 基线自身 ×2（nospread 配置确实 fps 稳定 = 变量消除成功）✓
- **退出成本**：A2（journal）Frame limit → World saved = **17 ms**；G2（legacy）=
  **3.28 s**（3600f 场景 ≈ 5 万现存 chunk；全 16000f 环 legacy 基线 = 6.2 s，K 不变）
  → **≈190×**：退出成本 = O(会话编辑数)（K = 湖相关 ~100 chunk），与环大小脱钩 ✓
- C 读档往返（已完：载 G 存档 600f → 再退出，blocks/fluids/stairs 与源
  全同 = 播种 + 过滤幂等 ✓）
- 晨间补（输入驱动，away 模式需用户在场）：全写路径输入脚本黄金 A/B
  （破/放/旋转/染色/桶倒/点火）+ S5 远卸载一致性（fluid-only 列卸载后退出
  无该列编辑）

## 已知问题 / 偏离

- 99.2 残余①（仅流体模拟改动的远端列卸载回生成态）在新语义**自动一致**
  （卸载驱逐 journal + 退出过滤只查现存 chunk = diffWorld"只遍历现存
  chunk"同款语义）——不扩大卸载豁免（P90 型持续燃烧列永驻 = 内存回退）。
- 软上限越线 = 退出回 legacy 慢路径（确定、仅体验回退、INFO 日志明示）。
- **纪律**：生成后一切格级写必经 World::setBlock/setFluid（observer 的
  完备性前提）——新增系统若直写 Chunk& = 静默漏记；等价 oracle 测试是
  回归网（AGENTS/PITS 已立纪律）。
- 病态 journal 内存 = 软上限 20 MB 封顶，无崩溃路径。

## 下一步建议

- 晨间补跑：输入驱动全写路径 E2E（需用户在桌前 = away 模式休眠）+ 环境
  恢复（xset s on / +dpms / 桌面回 2）。
- BACKLOG 顺序下一项 = **LGT-1 union-region**（同 job 的 2 个相邻光照 box
  合并并集 region，省 ~25% 光照 job——PIPE-1 后流送 fps 的主杠杆）。
