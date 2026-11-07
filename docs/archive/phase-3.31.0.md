# Phase v3.31.0 — V3-12c2 断线恢复 REJOIN + format 33 players[]（门 221 全 PASS，2026-11-08）

## 范围与定案

**V3-12 多人收官相位 c2 = 矩阵收官项**（V3-12 全 4 相位 a/b/c1/c2 完成）。
format 32 → 33，sim 2 不 bump（canary 4 EXACT 执法），全 sim 覆盖。

定案（承接 12c1 的 D 系列，本版增补 D24/D25）：

- **format 33 players[]**：root `player` 对象 + root `inventory` /
  `inv_grid` 段迁入 per-player `players` 数组（ALWAYS 写，≥1 entry）。
  entry 0 = PRIMARY（legacy 字段不动，applySave 主路径零改——354 处
  `save.player*` 读取经 mirrorPrimary 落回 legacy 成员）；entries 1.. =
  远程玩家（12b D13 的「主机单人档」lift——远程个人态 IS 世界态）。
  version lie 三面：≤32 携 `players` / ≥33 缺 `players` / ≥33 携 root
  `player`·`inventory`·`inv_grid`。entry 白名单 fmt≥33 加
  `inventory`/`inv_grid`（嵌套）。kSavePlayerCap = 8（协议 kNetMaxPlayers
  的孪生）。序列化 fallback：`save.players` 空 = 从 legacy 主字段派生
  单条（单测 fixture 兼容 + belt；collectSave ALWAYS 填）。
- **末地 XP orb 持久化**（10d1 后移复议兑现，非矩阵编号尾巴）：新
  `end_xp_entities` 段恒写（empty valid），overworld xp entry 形
  verbatim（parseXpEntry 单源提取复用）。format-32 文件携此键 = version
  lie。10d1 的「120-s 引信限界损失」偏离闭合。
- **REJOIN 快照**（save 形 WELCOME = D16 复议：12b 的「净世界 fresh
  only」面 → 主机 --listen × --save 转正）：WELCOME 携主机当前文档
  （`collectNetSnapshot` = collectSave + 会话计数族 + 活粒子）。客户端
  buildScene（worldgen 主机种子）后 `applyNetSnapshot`（parseSave +
  applySave 既有路径 100% 复用 + 计数族/粒子恢复 + `m_frameCount =
  startFrame` 帧限绝对值语义自动对齐）。**重连者 = 新体 + 起始库存**
  （D25——原体物品 100% 掉落仍在文档 entities 可捡回；偏离 MC 完整
  rejoin 记档）。
- **会话计数族**（on-disk 不携，net 文档携，presence-gated）：weather
  5 字段 {raining, frames_left, thunder_left, stream, thunder_stream}
  【机器纯 step = over (state, streams)，三可见字段欠定下次 roll——双
  splitmix 流必携】+ spawnCounter×3 + particleBreakCounter。on-disk
  collectSave 留缺省 → 不写 → S1 锚免重锚。
- **per-player 会话计时器族**（`net_players` 段，index-aligned）：
  prevHealth / hurtFlash / lastSpacePressTime / pearlThrowCooldownFrames /
  drownTicks / flying——哈希逐玩家面而持久 entry 不携的会话态。
- **活粒子**（`particles` 段）：rejoin 客户端重建主机活系统 bit-for-bit
  （哈希 m_particles.size() + update 的消亡帧——real-dt 积分在广播 dt
  序列下两机同消亡）。
- **Join/Renumber CTRL 广播**：N 增长（中会话加入）/ 移除（slot 重编号）
  的既有客户端通知——行随玩家移（compact erase）、输入历史跟走（无冲洗）。
- **net 主机抑制幽灵**：重启载多玩家档 = stale 幽灵（其他玩家连接已死）
  的 slot/m_nets 错乱 → segfault；net 主机（m_args.listenPort != 0）
  applySave 跳过幽灵循环，玩家集 = {自身} + {新连接}，与 solo 档 host 等价。
- **feedJournal 改哈希 live-diff**：journal ∪ retained 对生成参考的 diff
  （journalDiff 单线程 twin），非 journal 原始 cell——load 不变面。
- **respawn 精确 float**（fmt≥33）：≤32 int cell + bedSpawnPosition 对非
  cell-center 值（net spawn 整数 y）丢 0.5-y；≥33 写/读精确 float triple。
- **mob-quiet 合同**（D24）：活 mob 在场 = 中会话 rejoin 点名拒收 REJECT
  （「rejoin snapshot cannot carry mobs」）——全 mob 快照传输（AI 态族：
  path/dragon timer/carry/wool 实例）后移，门臂 mobs-off 实证。

## 变更文件

- `src/core/save.hpp`：kSaveFormat 33 + SavePlayer（22 字段）+ SavedParticle +
  SavedNetCounters + SavedNetPlayer + SaveGame.players/endXpEntities/particles/
  netCounters/netPlayers。
- `src/core/save.cpp`：makePlayerJson（含嵌套 inventory+inv_grid）+ makeXpEntityJson
  + parsePlayerFields/parseInventoryFields/parseXpEntry 提取 + players[] 双分派
  + mirrorPrimary + end_xp_entities 写/读 + net 段 {particles/weather/
  spawn_counters/particle_break_counter/net_players} 条件写/严格读 + respawn
  per-format（≥33 float / ≤32 int cell）+ ≤32 lie 面 + journalDiff 提升公共。
- `src/core/game_save.cpp`：collectSave players[] 填充 + 末地 orb 收集去 skip +
  applyGhostPlayer（+ setWorldSeed + prevHealth）+ **net 主机抑制幽灵**
  （listenPort != 0 跳幽灵循环）+ collectNetSnapshot（计数族 + 粒子）+
  applyNetSnapshot（parse/apply + 计数/粒子恢复 + m_frameCount 采纳）。
- `src/core/game.hpp`：applyGhostPlayer/collectNetSnapshot/applyNetSnapshot/
  netAddPlayer(slot, joinFrame)/netApplyRenumber/netApplyJoin/netTryMidSessionAccept
  声明 + m_netSnapshotPayload/m_netStartFrame/m_listenFd 成员。
- `src/core/game_net.cpp`：save 形 WELCOME（初始窗 + 中会话）+ 持久 listener +
  中会话非阻塞 accept（pre 面 + barrier 停滞 select 双节拍）+ Join/Renumber 广播
  + 客户端 snapshot 捕获 + CTRL 消费（Join/Renumber）+ netApplyRenumber/Join +
  feedJournal live-diff 调用点。
- `src/core/game_init.cpp`：client applyNetSnapshot 调用点（buildScene 后、body
  循环前）+ body 循环跳幽灵（已有体跳过）+ 析构 close listener + unistd include。
- `src/net/net_proto.{hpp,cpp}`：NetWelcome.startFrame（u32）+ NetCtrl::Kind
  {Renumber=2, Join=3} + Join 的 name（16-cap）+ codec + feedJournal live-diff
  签名（world/reference/blocks 参数）。
- `src/net/net_session.{hpp,cpp}`：startHost 拆 openListener（持久）+
  acceptFromListener(listenFd, timeoutMs)【0ms = 非阻塞 quiet face】。
- `src/core/enchanting.hpp`：kSaltPlayerXp[2]→[8]（A8..AD 续）+ i%8。
- `src/core/json.hpp`：非 const `Json& at(size_t)`（测试 seam 消费）。
- `src/entity/particle.hpp`：addRestored（快照恢复 seam，kMaxParticles belt）。
- 测试：test_save.cpp（SaveFormat33Test 9 + NetSnapshot 4 + 3 seam helpers
  primaryEntry/withPrimaryEntry/legacyShape + re-pin 族）、test_net.cpp（codec
  扩展）、test_bed/drown/durability/enchant_save/campfire/save_gzip（re-pin）。
- `tools/gate_compare.py`：savenorm 跨版本归一化（players→player 提升 + 嵌套
  inventory/inv_grid 提根 + respawn floor + end_xp_entities 剥 + format 剥 +
  nether/end entities age 剥）。

## 门 221 结果

- **S1 结构免疫第 102 次**：无输入 60f ×2 截图 ≡ base19 逐字节（225272 B）
  + savenorm EXACT + on-disk format 33 档无 net 段（presence-gated 省略）+
  respawn 精确 float。
- **canary 4 demo ×2 RAW + savenorm EXACT**（sim 2 不 bump 执法——example /
  tutorial_build / tutorial_dig / tutorial_fly 全 EXACT；串行跑避 WM 平铺几何
  噪声 PITS 411⑦）。
- **迁移臂**：v3.30.0（d316cfc）二进制造 format 32 档（root player + 34
  blockEdits）→ v3.31.0 载入 → 重存 format 33。值保真：pos [0.5,54,0.5] /
  yaw 0 / 34 blockEdits 逐位 / inventory slots[0] {stone,64} 全等 + root
  player→players[0] 迁移正确 + end_xp_entities [] + 无 root player。跨版本
  savenorm 唯一 diff = .time（恰 1 帧 day cycle 推进 0.0000278 = 1/60/600，
  预期）。
- **REJOIN 臂**：host --listen --save（载 300f solo 档）+ C1 初始窗 + C2 中会话
  重连（slot 2 of 3 @ frame 366）+ 「rejoin snapshot applied (the host's frame
  366)」startFrame 行 + **三向哈希 f600 ad509c664ccdeb4b host≡C1≡C2 逐字**
  + 零 desync + 双 rc=0。C1（--frames 450）移除的 Renumber 广播 + 双端 100%
  drop 同步（c1@420 无 desync 实证 client renumber 跑 drop）。
- **S6 面**：listen×save 转正（旧「save-form WELCOME is the 12c's face」拒收
  移除，全程 rejoin 测试实证）+ WELCOME payload 损坏 → client 点名（save 解析
  族覆盖）+ **mob 在场拒收命名实证**（mobs-on config 首跑：「the rejoin was
  refused (...cannot carry mobs...)」D24）+ net 主机载多玩家档幽灵抑制
  （ghosts restored = 0，无 segfault——重启前 stale 幽灵的 slot/m_nets 错乱修）。
- **性能 A/B 同档**（旧 = d316cfc v3.30.0 + g139 双树，独占跑 PITS 388④）：
  OW 900f 无输入 PERF-WIN 三窗 upd med 旧 0.640/0.638/0.650（旧树复跑噪声带
  0.648/0.702/0.696）vs 新 0.707/0.701/0.642（新树复跑 0.717/0.722/0.721）
  同档【win2 在旧树带内、win3 低于带、win1 略高于带 = warmup 窗噪声】+ render
  0.633/0.725/0.718 vs 0.697/0.790/0.662 同档 + swap ~15.1 vs ~15.0 同档 +
  changesApplied 9/3/1 逐位【新树复跑 win2 单 cell 9/2/1 = real-dt 流体沉降
  抖动 PITS 385 域，3/4 跑逐位】= format 33/net 面全冷 solo 路径结构性零成本
  【全部改动 net-only 或 save-only，solo 无输入路径零执行】。net 自成本（新树
  mobs-off 600f）：solo upd med 0.696/0.727 vs 2P 主机 0.773/0.783 = +~0.07ms/
  帧（per-player sim pass + per-remote 轮询/屏障 + socket，sub-ms 同档）+ 零
  desync。
- **2987/2987**（+13：SaveFormat33Test 9 + NetSnapshot 4 + NetProto codec
  扩展 re-pin 0 净增——codec 扩入既有 HelloWelcomeRejectCtrlRoundTrip）。
  `-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。

## 偏离记档（12c2 增补 D24-D25）

- **D24 全 mob 快照传输后移**：REJOIN 快照携粒子/计数/玩家会话态，但**不携
  mob**（AI 态族规模大：chase path / dragon lap+ascend timer / enderman carry /
  wool 实例 / 骑乘关系 / per-mob rngState）——活 mob 在场 = 中会话 rejoin 点名
  拒收（mob-quiet 合同），门臂 mobs-off 实证。初始窗（帧 0）结构性 mob-free
  （format 30 的 ephemeral 设计：载入档无 mob）不受限。全 mob 快照 = 后续面。
- **D25 重连者 = 新体 + 起始库存**：rejoin 不恢复原体的个人态（位置/库存/
  生存），而是新体（per-index spawn + 起始库存 + per-index xp salt）；原体物品
  100% 掉落（removal 的 drop）仍在文档 entities 可捡回。偏离 MC 的完整 rejoin
  （恢复原体态）。理由：原体态在 removal 时已 drop（100% 掉落纪律），恢复需
  反向 drop（物品 dup 风险）——新体 + 捡回旧物 = 无 dup 的自洽解。
- **net 主机载多玩家档 = stale 幽灵抑制**：重启后存档的其他玩家连接已死，
  保留为幽灵体 = slot/m_nets 错乱（segfault 门内捕获）——net 主机抑制幽灵
  （玩家集 = {自身}+{新连接}）；solo 加载多玩家档的幽灵面不变（记档面）。

## 后移（V3-16 / V3-17 及 12c2 候选）

- **12c2 后移候选复议定案（全后移，12c2 只做断线恢复 + format 33）**：
  per-player 维度 D9 / 最近玩家目标 D10 / 玩家碰撞 D7 / 协作挖掘 D6 / 物品
  交换 D5 / creative 多人 D11 / 本地预测 D3 / swing 体 D18 / aim 相机形 D17。
- **D24 全 mob 快照传输**（REJOIN 的 mobs-on 面）。
- **V3-16 扩展指导 demo 族**（11-07 用户实机诉求，10 主题 3-10 分钟级）。
- **V3-17 主菜单系统**（11-07 用户实机诉求，标题/设置/登陆/教学/多人五页）。
