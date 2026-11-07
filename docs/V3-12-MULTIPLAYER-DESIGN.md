# V3-12 多人 — 设计定案（12a = v3.28.0 门 218，2026-11-07）

> **状态 = 设计定案稿（纯文档无代码——BACKLOG 12a 定案）**。主机权威
> lockstep 方案（§2 三方案对比定案）；执行序 12a（本稿）→ 12b（2 人
> PoC = **v3.29.0 门 219**，范围/门臂 §4）→ 12c（完整化，范围 PoC 后
> 定案）。本文 = 立项侦察全文（390 行，代码引用 文件:行 实证）；门 219
> 实现的偏差以门内裁决记档于 phase-3.29.0.md。

# V3-12 多人 — 设计前置侦察（12a 立项设计稿）

仓库 HEAD = 3f9ae83（v3.27.0，V3-10 末地全 4 相位收官）。只读侦察；所有引用 = 文件:行。
MC 机制校正源 = minecraft.wiki/w/Server（2026-11-07 抓取）+ server-authoritative/lockstep 公开资料。
**任务书预判复核结论：方案 1（主机权威 lockstep）成立——且比预判更强：demo 内核（注入 + dts + pacing +
sim 门）= lockstep 客户端循环的现成形状，输入中继 = Input handle* 收口的第二消费方，世界传输 = 零
（worldgen 纯函数 + save journal 先例）。以下逐面实证。**

## 0. MC 多人机制校正（web 查证，作参照非照抄）

| 项 | MC 实值（wiki/资料） | 项目定案建议 |
|---|---|---|
| 架构 | **权威服务端**（dedicated 或进程内 integrated「Open to LAN」）：服务器 20 TPS 拥有全部状态，客户端发输入意图 + **本地预测**自身移动 + 服务器快照校正 + 远端实体 20Hz 插值；**MC 无 lockstep** | **主机权威 lockstep**（方案 1，§2）：确定性纪律 = 锁步免费午餐（BACKLOG 原话）；无预测 = 偏离 D3 |
| 玩家模型 | 第二人 = 服务端 player 实体（UUID + pos/vel 广播）；每客户端本地视角 | 第二人 = **全 Player 实例各机并算**（比 MC 强：状态是算出的不是传的）+ 本地视角（§3） |
| 加入 | 服务器发 level 数据（seed/区域），新玩家出生点 = 世界 spawn 或上次位置 | 握手 = {seed + journal}（fresh 世界 = 仅 seed 零字节，worldgen 纯函数）+ kSpawnCandidates[i] 出生（game_init.cpp:1835-1857 先例） |
| 踢人/权限 | operator /kick；whitelist；max-players=20；view-distance/simulation-distance = 服务器配置 | 主机权威 kick（CTRL 包）；**config parity hash**（两机模拟相关配置须一致 = 握手拒连，§2.3） |
| 聊天 | 服务器中继，256 字符限，spam 阈值 | 12c（12b 无，偏离 D4；载体 = Input::frameText 先例 input.hpp:313） |
| 断线 | 玩家离会、世界继续（dedicated）；LAN = 客户端离会主机继续 | 12b = 断线玩家**移出模拟**（物品掉落 + CTRL 包，R19）；主机断 = 全体退（D2）；重连 = 12c |

## 1. 确定性基建盘点（lockstep 可行性根）

### 1.1 双时钟体系逐面分类

主循环 = **wall-clock dt（钳制 kMaxDt 0.25）+ 每帧一次 update(dt)**（game_init.cpp:2930-2948）——
不是固定 1/60 主时钟；「固定 1/60」= 帧锁定系统每帧推进固定步长，real-dt 系统 = 消费实际 dt。
**lockstep 时间源定案（核心）：主机把自己的 clamped wall dt 以 raw float64 位每帧广播 = 唯一时间源**
（demo 先例原文：「the demo's dts are the ONLY time source」demo.hpp:17；回放双机字节恒等已实证
「同 dts + 同输入 = 同模拟」——S2 臂 ×2 RAW 的跨进程确定性就是靠这个）。real-dt 系统因此
**结构性零 desync，无需转帧锁定、无需主机结算**（每个系统的「主机权威时钟源」方案 = 同一条：
消费主机广播 dt）。

**帧锁定类（纯函数 of m_frameCount / 固定步长，game_init.cpp:2951 递增）**：
昼夜 kDayStepPerFrame=1/60（game_frame.cpp:23,247）/ 天气状态机（game_frame.cpp:249-）/
流体每 3 帧 kFluidTickEveryNFrames（game_frame.cpp:27,2581，纯世界态函数）/ 火蔓延
updateFireSpread()（game_frame.cpp:804，帧 + worldSeed roll）/ 火星粒子固定帧发射 /
mob/箭固定步 kMobStepPerFrame、kProjectileStepPerFrame（game_entities.cpp:30,33,167,177，
「position at frame N = pure function of N」game_entities.cpp:23-29）/ item bob/spin 帧
（game_entities.cpp:40-43）/ 溺水 drownStep(m_frameCount)（game_frame.cpp:425-433）/
portal 60f/900f 冷却 / pearl 60f 冷却 / mob 燃烧 tick 固定 1/60 网格（game_init.cpp:980）/
熔炉 1/60 substep（game_frame.cpp:2415）/ 作物生长 1/60（game_frame.cpp:2522,2545）/
教程 toast 120f / 龙仪式 600f 会话帧计时 / 环境音 300f roll（ambience.hpp:39）/
mesh/light job 的 post→flush 帧槽（§1.5）/ spawnCounter 流（game_entities.cpp:1261-1267）。

**real-dt 类（消费 update 的 dt 参数——全部经广播 dt = 一致）**：
玩家物理 m_player->update(dt,...)（game_frame.cpp:380：移动/跳跃/游泳/坠落/终端速度）/
粒子 m_particles.update(dt)（game_frame.cpp:473，「motion integrates the real dt」particle.hpp:11）/
火熄灭倒计时 updateFires(dt)（game_frame.cpp:523,2705）/ 生存 updateSurvival(dt)（game_ui.cpp:941：
hurtFlash/死亡判定/R 重生）/ 饥饿·回复（Player::update 内 real dt，player.cpp:588 注释）/
状态效果 updateEffects(dt)（game_ui.cpp:78：duration + 周期 tick，注释「ticks are REAL dt」）/
进食通道（game_interaction.cpp:2533）/ 挖掘 breakProgress（InteractionState 注释
game.hpp:2205-2212）/ 弓蓄力（game_interaction.cpp:637,857）/ 挥击冷却 / 脚步音
updateSoundTimers(dt)（game_ui.cpp:901）/ 实体 age·pickup delay（exp_orb.hpp:64）/
neutral 激怒 30s（mob_registry.hpp:388）/ 自动存档 updateAutoSave(dt)（game_ui.cpp:919，I/O 只读模拟）。

**wall-clock 直读（不经 dt）= 仅 2 处，lockstep 的确定性缺口**：
① Space 双击飞行窗 game_interaction.cpp:143 `now = nowSeconds()` + m_lastSpacePressTime
② 库存双击收集窗 game_ui.cpp:655 `now = nowSeconds()` + lastLeftClickTime
**定案（R2）：新增共享模拟钟 m_simTime（每帧 += 广播 dt）；多人会话内这两处改读 m_simTime，
solo 路径保留 nowSeconds 原文 = 位级红线**（门分支 = net 会话旗，冷路径输入驱动代码，S1 锚冷）。
（logger 时间戳 game_init 系 / save 耗时计量 = 输出面非模拟，不入清单。）

### 1.2 种子契约（多人世界共享 = 握手携带 worldSeed）

- worldSeed（config world.seed / --seed）→ worldgen 纯函数盐族 kSaltHeight/Biome/Cave/Ore/
  Tree/Decoration/LavaLake/Cave2/Bedrock/Passes/Sediment/Mossy（world_generator.hpp:58-79）
- 维度派生 seed 不落盘、parse 重建：netherSeed = worldSeed ^ kSaltNether（nether_gen.hpp:101,
  save.hpp:439），endSeed = worldSeed ^ kSaltEnd（end_gen.hpp:38）+ 内容盐族
  （end_gen.hpp:44-50 / nether_gen.hpp:107-113）+ 雷 salt（weather.hpp:88）+ 龙 kSaltDragon
  （end_gen.hpp:50）——**全 = worldSeed 的纯函数 → 两机各算各的，零传输，位级一致**
- 逐实体流：mob m_rng = worldSeed + spawnCounter×golden（game_entities.cpp:1261-1267，
  per-dimension MobSystem::spawnCounter game.hpp:1926）/ per-mob 帧流契约（每帧固定抽数，
  mob_entity.cpp:625-636 族）/ 粒子计数 m_particleBreakCounter（game_frame.cpp:585-590）/
  狼色出生首抽、龙 lap roll（mob_entity.cpp:1121）——**全 = 帧 + 事件历史的纯函数 = lockstep
  下自然一致**
- **新盐需求 = 1**：per-player xpSeed。现 = xpSeedInit(worldSeed)（enchanting.hpp:198,
  game_init.cpp:1872）单人独占；双人同值 = 附魔/耐久流碰撞。**定案：player i 的
  xpSeed = xpSeedInit(worldSeed ^ kSaltPlayerXp[i])，kSaltPlayerXp[0] = 0**（主机位 = 原文
  xor-0 = solo 位级红线，kSalt 族续位纪律 cloud.hpp:96-113 先例）

### 1.3 存档 format 32 + gzip（12b 零 format 定案）

- 现状：top level 封闭词表 {format, seed, time, player, ...}（save.cpp:1425），player =
  **单数**（写 save.cpp:1040 / 解析 :1472，format 30+ dimension、31 creative 恒写先例）
- 世界真值 = journal（PIPE-2：生成参考 diff，world.hpp:226-230 observer；save = seed + journal
  + 实体 + time，save.hpp:1-30 头注）——**journal 是 lockstep 共享世界态的天然摘要**
- **12b 定案 = format 32 不变、零 bump**：多人存档 = 主机单存档（journal = 共享世界，各机位级
  同；player 段 = 仅主机玩家）；远程玩家个人态（库存/pos/HP/教程）= **会话态不落盘**（偏离 D13，
  MC 先例对照：玩家文件在服务器——本项目服务器 = 主机进程）；客户端永不写档（--connect ×
  --save 互斥点名，S6 面）。**12c 复议 = format 33 players[] 数组**（index 0 = 主机 + per-player
  dimension/creative/tutorial，dimension 词表按 format 分词先例复用 save.hpp:474-489）
- gzip 确定性容器（PITS 385）：12c 重连快照 = WELCOME 携带主机 save 文档 payload 直传，零新基建

### 1.4 demo sim 版本（kDemoSimVersion = 2，demo.hpp:72）

- bump 纪律：仅帧锁定模拟语义变更（update/物理/AI/流体/生成/输入路由/demo pacing）
- **12b 定案 = 不 bump（保持 2）**：多人 = 新模式（net 族 × demo 族互斥，main.cpp:301-309
  先例），solo 路径位级不变（m_players size 1 + primary==local + net 冷路径 = 结构免疫根），
  canary 4 demo EXACT = 不 bump 的实证执法（v3.17/10d1「canary EXACT = 零 bump 最强形」先例）
- 输入路由条款的边界 = **回放语义**（录制的 gamepad 事件按当前解释重放，v3.14.0 bump 2 实例）；
  网络中继不改变任何录制事件的回放解释 → 不触发。若 12b 重构意外触碰 solo 语义（canary 破）=
  bump 3 + canary 重新 author（纪律现成，零新设计）

### 1.5 chunk streaming worker（post→flush 窗口纪律）

- worker job 只在 post→flush 窗口运行（硬规则 7）；flush() 等待全部 job（thread_pool.hpp:83）；
  生成 job = 纯 generator → 自带 scratch chunk（「no World access, no shared mutable state —
  the generator's members are immutable after create()」game_streaming.cpp:700-713）
  → **同 seed 同列 = 两机位级一致，跨机零世界数据**
- mesh job = 快照 World（3×3 列邻域）worker 算 + 主线程 GL 上传（mesh_job.hpp:1-30）；
  light job ≤2/帧位级独立对（硬规则 7 99.10）——快照取自主线程写后 = 帧最终态（game_streaming.cpp:
  postMeshJobs 头注）→ 两机快照同 = mesh 同
- **跨机一致性的唯一缺口 = 2 处「本地事实」入 post 决策**：
  ① 盘锚 = m_player->position()（game_streaming.cpp:631,900）→ **定案：锚 = primary 玩家
  （index 0，共享定义，R1 红线：禁本地玩家相对锚）**
  ② 回退门 `ctx.pool.pendingCount() >= kMaxStreamBacklog`（game_streaming.cpp:687）=
  本机池吞吐（慢机 job 滞留多 = 跳 post = 两机 post 序列分叉 → 列集成帧分叉 →
  流体 tick/生成扫描看到不同 chunk 集合 = **静默 desync 根**）
  → **定案（12b 核心改动①）：门改共享账本 `ctx.streaming.streamPending.size() >= kMaxStreamBacklog`**
  （streamPending = 已 post 未集成列，post+1 帧经 flush 等待确定性集成 = 两机位级同）；
  worker 速度只影响本机 wall 时间不影响模拟（headless 锚快跑 60× 先例同根）
- 迟滞 streamingEngaged = 盘 + loaded 列的纯函数（chunk_streaming.hpp:27-38）✓ 共享；
  view_distance（config 80 → min(64) kStreamCapRadius game_local.hpp:47）= 握手 config hash 内

### 1.6 输入面（中继 = demo 注入内核的第二消费方——项目现成缝）

- Input handle* 收口（input.hpp:239-264）：pump() 的 SDL 事件 / 单测 / **demo 回放**三路同路径
  （:239-241 头注）；demo tap = setEventTap（:275-277）；m_audienceInput = Game 第二 Input 实例
  先例（game.hpp:2755）
- **demo 回放循环形状 = 多人客户端循环的模板**（game_init.cpp:3188-3194）：
  `frameBegin() → injectForFrame(f, input) → dt = dtForFrame(f) → update(dt)`——
  把「文件」换成「网络」即得 NetPlayer（injectForFrame + dtForFrame 同签名）；
  pacing 语义现成：「dts are the ONLY time source / wall clock paces the rendering / a slow
  machine drops render frames, the simulation does not chase」（demo.hpp:17, game_init.cpp:3002-3016）
- InputEvent 8 型词表（input.hpp:66-85）= 原始事件记录；**中继定案 = 帧态（FrameState）非事件流**
  （§2.3）：Input 帧态 = frameBegin 重置面（isDown 按源位掩码 / wasPressed 边沿 / mouseDelta
  累积 / wheelDelta / frameText≤256），帧态幂等（无事件序问题）+ 紧凑（~40B）+ 手柄结构性折叠
  （左杆 = 数字化 WASD 源位 bit1、右杆 = 合成 mouseDelta，input.hpp:143-150，V3-8 先例——
  FrameState 零独立手柄字段）
- 帧态 → handle* 合成（接收侧）：按上一帧态 diff 出边沿 + 单条累积 MouseMove——demo 注入
  switch（demo.cpp:600-640）同构，零新 Input 面

## 2. 架构方案对比 + 定案

### 2.1 三方案

**方案 1 = 主机权威 lockstep ✅ 定案**（主机 = 时钟源 + 输入中继 + 种子/版本权威；所有客户端跑
同一 Game::update；desync 检测 = 周期状态哈希）：
- 项目确定性资产利用率 = 满：demo 内核（§1.6）/ 纯函数 worldgen（§1.2）/ journal 快照（§1.3）/
  sim 版本门（§1.4）/ FNV 哈希纪律（tests/test_font.cpp:55-57）全部直接消费
- 反作弊 = 哈希内建（任意机器篡改模拟 → 与主机广播哈希不符 → 点名退出；MC 权威服 = 服务器验证，
  本项目等价物 = 全机哈希对账）
- 成本有界（§4）：player 多人化 + streaming 共享纯度修正 + simTime 两处 + net/ 新目录
**方案 2 = 专用服权威状态同步 ❌**（MC 实际形状）：客户端预测 + 校正层项目零现有（玩家本地
预测 = 输入延迟下本地 sim 与服务器 sim 双轨 + reconcile = 本项目确定性纪律的反面用法）；
chunk 传输 = 初始 ~3MB/人（100 列 × 32KB，无网络压缩面）；实体 20Hz 同步 = 新协议面；
专用服二进制 = 新进程形态（headless --frames 先例可借但窗口/输入面全冷跑改造）；
带宽大且确定性资产 ~0 利用。唯一优势（防作弊）方案 1 哈希等价覆盖。
**方案 3 = 全 P2P lockstep ❌**：无主机 = 中途加入需任意对等方快照 + 断线后剩余方重锚
（时钟源谁出？固定 1/60 dt 可行 = author 先例 kDemoAuthorDt demo.hpp:82，但墙钟时间源丢失）
+ 版本/种子权威分散；2 人 PoC 零收益，12c 重连仍可借「主机快照」概念。

### 2.2 网络层（项目零网络代码——新基建面）

- **目录 = 新 src/net/**（平台边界，与 window/input 同层；硬规则 2 渲染隔离同款精神 = 模拟
  net-agnostic：net 只喂 Input 帧态 + dt + 哈希，Game 接线，与 demo 边界形状一致）。
  CMake：mc_core 加源文件，无新 target（threads 已链 CMakeLists.txt:17）
- **BSD socket（POSIX）定案**：项目 = Linux 目标（PROTOCOL.md 环境：GCC 16.2.1/X11/Mesa，
  无 Windows 代码路径）；SDL3 核心无网络 API；SDL3_net 未装于门机（pkg-config/ldconfig 实证：
  仅 libSDL3 + sdl2-compat）；原生 socket = 零新依赖（zlib 先例：系统库 PkgConfig 链路同款）
- **TCP 定案（12b）**：量 = 每帧每人 ~40-100B → 2 人总流量 ~KB/s，LAN 零压力；TCP = 顺序 +
  可靠免费（lockstep 帧序 = 协议前提，UDP 重排/重传 = 新风险面无收益）；PoC 范围 = 局域网
  （loopback/192.168，RTT <5ms 定案）；跨网（NAT 穿透/可靠 UDP）= 12c 复议且倾向不做（D1）
- **线程模型：无独立 net 线程**——主循环帧头 poll socket 非阻塞（Input pump 单消费者
  input.hpp:179 同款；recv → 按连接重组缓冲 → 包队列；TCP 流序 = 帧序，确定性）；
  主机 accept = 会话启动一次性（PoC 固定 1 客户端连接，N 连接 = 12c）

### 2.3 协议设计（12b schema 定案；小端固定头，无压缩，包 ≤512B）

- **HELLO**（client→host）：{magic u32, TM_VERSION str, sim i32(=2), assetFNV u64（assets/data/**
  全文件 FNV-1a 64——数据驱动内容 = 模拟语义，两机资产必须同）, simConfigHash u64（world.
  seed/view_distance/day_length/start_time + player.* 生存参数 8 键 + mobs.enabled/difficulty——
  render.vsync 不入 hash：它只影响各机 wall 节奏，模拟时钟 = 主机广播 dt 已归一）,
  windowW/H u16×2, uiScale f32, name ≤12 Latin}
- **WELCOME**（host→client）：{ok, worldSpec {type fresh|save, seed i64, journalBytes（save 形 =
  主机当前 save 文档 payload，fresh = 空——两机各自纯生成）}, players u8, slot u8（=1）,
  D u8（输入延迟 =2）, hashInterval u8（=30）} | **REJECT** {reason str——六面点名：magic/
  version/sim/assetFNV/configHash/window 各显式（save version lie 命名先例）}
- **INPUT**（client→host，每帧）：{frame u32（= 本机 N−D 帧）, FrameState, hash u64（本机该帧
  状态哈希——主机比对，双向检测零新包型）}
- **FRAME**（host→client，每帧）：{frame u32, dtBits u64（主机 clamped wall dt **raw double 位**——
  严格性：两机 bit 级同 double；int64 µs 往返不精确 = 否决，demo pacing 的 µs 只用于渲染节拍
  不入模拟）, FrameState[players]（全员含主机——本机玩家模拟统一取 N−D 历史环，包内态 = 对账
  冗余，§2.3 帧序律）, hash u64（每 hashInterval 帧非零）}
- **CTRL**（host→client，帧边界应用）：{type leave|kick|disconnect, slot u8}——12b 断线 =
  该玩家物品 100% 掉落 + 实例移除（R19，死亡掉落同路径）
- **FrameState（~40B）**：{keysDown u64（24 键×2 源位）, keysPressed u64, buttonsDown u8,
  buttonsPressed u8, buttonsReleased u8, mouseDx f32, mouseDy f32, mouseAx u32, mouseAy u32,
  wheel i8, textLen u8（12b ≡ 0：creative 多人禁用 D11，搜索不中继）}——鼠标 delta = raw f32
  px **零量化**（双端同感度 = 握手 config parity 的 player.mouse 默认 0.0025 rad/px 同值，
  R4）；手柄 = 结构性折叠（§1.6）
- **帧序律**：帧 N 的模拟 = {dt[N]（主机墙钟）, FrameState[N−D]×全员}；D = 2 会话常量——
  客户端 N−D 帧输入已于 N−D 发出，主机 RTT < 2×16.7ms 窗口内收齐（LAN 5ms ✓）；
  **本机玩家亦取 N−D 历史（统一规则，无本地特例 = 无预测，D3）**；客户端等包阻塞模拟
  （渲染持帧 = demo 0.5x hold 先例「second render-only」demo.hpp:86-89）
- **状态哈希（desync 检测）字段定案**（每 30 帧各机自算 + 交换；FNV-1a 64 过 64bit 块，
  tests/test_font.cpp:56 先例；字段 = 模拟真值全列）：m_frameCount / dayCycle time / weather
  状态+帧 / 每玩家（pos/vel/yaw/pitch raw f32 位、HP/hunger/sat、creative、xp、spawnPos、
  库存 36 格摘要）/ currentEntities()（插入序：type id、pos、mob HP/状态枚举、item age）/
  每维 journal（编辑数 + 条目流）/ spawnCounter×3 / 粒子计数 / fires map 摘要——
  **字段清单单测钉**（R15：漏字段 = 静默 desync；终权威 = 退出 savenorm 全比对）

### 2.4 帧率/延迟面

- 主机 60fps（用户 config render.vsync true）→ dt ≈ 16.7ms 钳 0.25（game_init.cpp:2938-2940）；
  主机掉帧 30fps → dt = 33ms 广播 → 全员 real-dt 系统 33ms/帧 + 帧锁定系统随帧钟减速
  （「at lower caps the body slows down with the frame clock」game_entities.cpp:23-29 既有语义）
  = **结构性一致，零新设计**（本项目低帧率语义直接成为多人时钟语义）
- 客户端 144Hz = 渲染 only hold（demo pacing 先例）；客户端慢机 = 渲染掉帧模拟不追
  （game_init.cpp:3010-3016 文档化偏离同款）；客户端追不上主机帧率 = 包滞留 = 模拟暂停等包
  （12b 无动态追帧，D3 族）
- 输入延迟 = D×16.7 + RTT/2 ≈ 38-42ms（LAN）——PoC 定案可接受（MC 权威服 = RTT 量级 + 预测；
  lockstep 无预测略高但简单一致）；12c 复议本地预测（复杂度 ↑↑，需校正层）

### 2.5 断线

- 12b = **断线即移除、会话继续**：TCP EOF → 主机 CTRL disconnect → 全员同帧移除玩家
  （物品掉落 = 共享世界态，确定性）；客户端见主机 EOF = 全体退出（主机单点，D2 定案）
- 无心跳（纯 LAN，EOF 覆盖拔线/关机；12c 跨网复议）
- 12c = 重连：re-HELLO → WELCOME save 形 journal 快照（KB-MB 一次）→ 客户端
  seed 生成 + applyWorldEdits 重放（save.hpp:999 载档同路径）→ 自下一帧边界并入（帧边界
  应用 = CTRL 同款纪律）

## 3. MC 多人语义校正落地（玩家模型 = 最大重构面）

### 3.1 Player 多人化定案：多 Player 实例 + 双别名（否决实体化）

- **否决方案**：Player 入实体列表（entity-化）——Player 的 Game 侧状态 10+ 成员（库存/效果/
  交互/溺水/教程/XP 种子）+ 354 处 m_player 引用（8 个 core 文件 grep 实证）全要改 +
  mob 渲染路径画不了 Steve 模型族（PlayerBody game.hpp:2128-2151 独立面）= 最大重构面
- **定案**：`std::vector<std::unique_ptr<Player>> m_players` + **双别名访问器**：
  - `primaryPlayer()` = m_players[0]（主机，**index 定 = 各机共享定义**；R1 红线：一切「the
    player」模拟语义锚 = primary，禁本地玩家相对——本地相对 = 双机分叉根）
  - `localPlayer()` = m_players[m_localSlot]（相机/HUD/死亡屏/F3/本地音/第一人称臂/第三人称体）
  - `m_player`（game.hpp:1628）保留 = primaryPlayer() 引用别名 → **354 处引用零改动**
    （solo：primary==local 同对象 = 位级；多人：m_player = 主机玩家，语义 = 「the player」
    全族自动指向 primary = 最小改动面）
  - `Player::update(dt, const Input&, bool)`（player.cpp:168）签名现成 = per-player 输入缝
    （m_input 指针成员已存在）
- **Game→Player 状态迁移（per-player 化清单，12b）**：m_inventory（game.hpp:2241）、
  m_playerEffects（2683）、m_interaction（2191-2224：挖掘/弓/进食/挥击/脚步）、m_drown（2675）、
  m_hurtFlash（2663）、m_pearlThrowCooldownFrames（1584）、m_ridingMount（2107）、
  m_sessionStats（2692）、m_tutorial 族（2715-2718）、m_lastSpacePressTime（2170，simTime 化 R2）
  + 新 lock 共享旗（R18）——**Player 已自持的不动**：XP（m_xpTotal/m_xpSeed player.hpp:405-432）、
  位置/朝向/血量/饥饿（Player 本征）
- **per-player Input**：local = m_input（pump 活设备）；remote = 每人 1 Input 实例
  （m_audienceInput 先例 game.hpp:2755），帧态 → handle* 合成注入（§1.6）；各机维护本机玩家
  帧态历史环（深 D）= 本机玩家模拟取 N−D（统一规则）
- **pass 归属定案（m_player=primary 不动 = 12b 零改）**：mob 目标 setPlayer
  （game_entities.cpp:165）/ 生成扫描锚 / despawn 距离（:680-692）/ 教程 / 龙 gameplay /
  portal 入口（12b 全局维度 = primary 维度 D9 → 远程 portal 禁用）/ 箭头拾取锚
- **per-player 循环化（12b 新增）**：玩家物理×N（game_frame.cpp:380）/ survival×N /
  effects×N / drown×N / beacon×N / 火熔岩×N / 眼接触×N（game_entities.cpp:105-135 每人 gaze
  可激怒）/ 拾取×N（item 归属 = 最近玩家，index tie-break 纯函数）/ ride pass×N（转向读该
  玩家 Input）/ Q 丢弃×N / 珍珠×N / 交互（挖掘/放置/使用/奶/交易/剪毛/染色）×N
  （handleInteraction → handleInteractionFor(player, input, dt)，game_interaction.cpp:82 起）
- **玩家间碰撞 = 无（D7）**；mob 分离 pass 锚 = primary（12b）；12c = 最近玩家目标 + 简单推离
- **出生点（R9）**：player i = kSpawnCandidates 第 i 个有效候选（game_init.cpp:1835-1857
  既有循环泛化——候选表 10 项 ≥ N=8 上限，纯函数零新盐）

### 3.2 聊天（12c）

T 键 + 输入面现成（frameText input.hpp:313 = 项目首个打字面先例）；中继 = 主机广播 + 本地显示；
字体 433/528 扩展区满（10d1 先例）→ 12c 定案点：Latin-only（零字形）或扩区（重锚论证面）；
MC 256 字符限 = 直接采用。12b 不做（D4）。

### 3.3 视角/摄像机

每客户端本地（MC 同款）：syncCamera（game_frame.cpp:102-）/ 相机模式 C / 第三人称距离
（game.hpp:2140-2160 会话态）/ F3 = localPlayer + 共享 sim 行——零网络。远程玩家体 = 各机渲染
（drawPlayerBody 泛化 per-player swing 态 + 12c 名牌）。

## 4. 相位拆分细化

- **12a = 设计定案（本稿，纯文档无代码定案）**：协议 schema = 文档级 bit 定义（§2.3），
  12b 实现落 net_proto.hpp；交付物 = 本文档 + BACKLOG 12b 行更新（范围/门臂）
- **12b = 2 人 PoC（单主机 + lockstep 验证），范围定案**：
  ① src/net/（协议 + socket，~800 行）② player 多人化重构（双别名 + 10 成员迁移 +
  per-player pass，~1200 行触点）③ streaming 共享纯度修正（§1.5 两处）④ simTime 两处（R2）
  ⑤ host 循环 = runGameplayLoop + 环 + 广播 + 远程 Input 喂入；client 循环 = runDemoReplay 形
  + net 源 ⑥ 断线移除 + 哈希交换 + 退出 savenorm ⑦ CLI：--listen <port> / --connect
  <host:port> / --script <timeline.json>（门臂输入源，× demo 族互斥）
  **12b 玩法面 = 全 sim 覆盖**（挖掘/放置/物品/生存/死亡/骑乘/珍珠/交易全 per-player——
  lockstep 验证需要全 sim 面，半残 sim = 哈希面不全 = PoC 失义；内部实现序 =
  net 骨架 → 多人化 → per-player pass → 门臂，单版单门）
  **12b 不做**：大厅/聊天/重连/预测/跨维度玩家/creative 多人/玩家碰撞（全 = 12c 候选）
- **12b 门臂（= 门 218 预判）**：
  - S1 ≡ base19 第 99 次结构免疫（多人 off 位级，最强形）
  - **S2 双进程 headless 同帧哈希臂（核心）**：host `--listen 9901 --script a.json --new --seed S
    --save slot` + client `--connect 127.0.0.1:9901 --script b.json`；脚本 = --demo-author
    时间线（确定性输入，零 xdotool 零人工——demo 内核第四消费方）；断言：
    ① 两进程哈希行（每 300f DEBUG `net hash f<N> <u64>`）逐字同 ×2 跑
    ② host 退出档 savenorm ×2 RAW EXACT（跨进程权威 = 哈希逐字 + 主机档恒等）
    ③ 各进程截图 ×2 RAW（本机视图可复现；**跨进程像素不比对** = 视图口不同，哈希即跨机权威）
    ④ 输入注入对跑：b 脚本移动/挖掘 → host 侧截图 crop 见玩家 2 体 + 块变化（世界收敛目检）
    ⑤ 断线臂：b 进程中途杀 → host CTRL 移除 + 物品掉落 ×2 RAW + host 续跑 savenorm EXACT
  - S6 点名（init rc 1）：sim 不匹配 / assetFNV 不匹配 / config parity 不匹配 / window 不匹配 /
    --connect×--save / --listen×demo 族 / 端口占用 / magic 错——八面
  - canary 4 demo EXACT（sim 2 不 bump 执法）+ 性能 A/B 同档（net 冷路径 = 1 null 检查）
- **12c = 完整化（范围 PoC 后定案，候选）**：大厅（MOTD/名单）/ 聊天 / 重连（journal 快照）/
  N=4（协议 ≤8）/ per-player 维度（Entity::setWorld 先例）/ 最近玩家目标 / 玩家碰撞 /
  协作挖掘 / 物品交换 / creative 多人（文本中继 + 面板 parity）/ format 33 players[] /
  本地预测复议 / 心跳

## 5. 风险清单（R1-R20）

- **R1 player 多人化 = 最大重构面**（354 处 m_player + 10 Game 成员迁移 + per-player pass 化）：
  既有先例 = 维度三人化 33 分派点（10a）/ MobSystem per-dimension 镜像（Phase B D1 增量并行
  成员）；建议 = 双别名最小化（§3.1）+ S1 锚位级执法 + 迁移逐成员回归单测（库存/效果/交互
  族现有单测改 per-player fixture）
- **R2 real-dt 时钟源**：广播 dt 方案 = 结构解（§1.1）；残余 = 2 处 wall 直读（simTime 化，
  solo 门分支位级）+ 主机 fps_cap/vsync 差异 → dt 序列差异 = 合法（主机权威，两机同序列）
- **R3 streaming 跨机一致性**：§1.5 两修正（锚 = primary + streamPending 门）后 post 序列 =
  共享纯函数；残余风险 = unloadFarColumns 迟滞带（±64-65 列）依赖 loaded 列集合 = 共享 ✓；
  门臂 = 双进程同跑 3000f 后 journal 逐格比对（savenorm 即执法）
- **R4 鼠标 delta/感度**：raw f32 px 零量化（精确）；感度 = Player::Params 默认 0.0025
  （player.hpp:132）config 无覆盖键（实证 config.json 无 mouse 键）→ 两机同二进制同值 ✓；
  12c 若感度 per-player 化 = FrameState 改携带 rad 或握手 per-player 参数（预留）
- **R5 版本锁**：握手 fingerprint {TM_VERSION, sim, assetFNV, simConfigHash}（§2.3）=
  v3.13.0 demo sim 门 + save version lie 点名先例复用；二进制不同 = 拒连（不降级）
- **R6 字体/音频本地面**：字体 = 资产（assetFNV 覆盖，不传）；音频 = 确定性生成 + 本地播放
  （PITS 382：LCG 逐采样重建，播放不入模拟）；远程玩家静默 = D8（12b 不做 3D 音传播）
- **R7 存档 × 多人**：host --save 允许（journal 共享 = 主机档完整世界态）；client --save 拒
  （点名）；会话中 autosave = 主机本地 I/O（只读模拟，real-dt 计时器 = 主机墙钟——**主机
  autosave 间隔不广播 = 两机无感（autosave 不写模拟态）**✓ 零冲突）
- **R8 window 几何 parity**：面板绝对坐标（uiMouse = 窗口 px × uiSize）两机窗口不同 =
  点击落格不同 → 握手 windowW/H + ui.scale 等值门（不等 = 点名拒连）；12b 会话中禁 resize
  （SDL 事件面 = 本地，不校验 → 12c 复议：帧态携带 ax/ay 校验）
- **R9 出生点重叠**：kSpawnCandidates[i]（§3.1）——候选不足 N 时回绕 + 偏移（纯函数定案，
  单测钉）；两人同点 = 无碰撞（D7）视觉重叠可接受（PoC）
- **R10 XP 种子碰撞**：kSaltPlayerXp[i] 族（[0]=0 位级，§1.2）——附魔台/耐久流双人并发
  不串流；盐值 = 碰撞检查全 kSalt* 族（cloud.hpp:96-113 纪律）
- **R11 跨维度**：12b 全局 = primary 维度（currentWorld() 三值分派 game.hpp:1346-1353 零改）；
  远程 portal 禁用（updatePortalEntry 锚 = m_player = primary ✓ 零改自然成立）；12c =
  per-player setWorld（Phase B D3 先例：Entity::setWorld 17 点机械适配）
- **R12 玩家间碰撞缺失**（D7）：两人穿体走（MC 推离）——PoC 记录即可；mob 分离 pass 不推
  玩家 2（锚 = primary）
- **R13 主机单点**：主机崩 = 全体 EOF 退出（D2）；主机卡死（sim 慢）= 客户端等包暂停
  （无心跳 = 12b 不可检测 → 记录；12c 心跳 + 超时退出）
- **R14 无预测输入延迟 38-42ms**（D3）：PoC 定案可接受；门臂脚本 = 帧锁定时间线，
  延迟对双方对称（不伤确定性）
- **R15 哈希面不全 = 静默 desync**：字段清单 §2.3 定案 + 单测钉（字段枚举 × 构造变异 =
  哈希必变，test_net 族）；终权威 = 退出 savenorm（哈希 = 快速守卫非唯一判据）
- **R16 TCP 流重组**：粘包/半包 = 标准缓冲循环（per-connection 字节队列，包头长度前缀）；
  单测 = 字节流切分注入（确定性，socket 不入单测——net_proto 纯函数面才单测）
- **R17 本地面板 × 远程库存**：12b 远程玩家面板禁用（E 键 = 该玩家 no-op，D11 族）——
  其库存变更仅经 Q 丢弃（键，无坐标）+ 拾取（世界态）；本地玩家面板 = 本地 UI 但**点击
  落格须两机同** → 经 FrameState 原始坐标 + R8 parity 门（ax/ay 入包）
- **R18 指针锁 = 本地事实入模拟**：handleInteraction 读 m_window.relativeMouseMode()
  （game_interaction.cpp:92）= 每机窗口态 → **定案：sim 读 per-player 共享 lock 旗**
  （事件驱动迁移：未锁 LMB = 捕获，与现语义同序列）；solo = 旗与 SDL 态同步保持（原码位级，
  旗为镜像写）；渲染面（准星/臂）读本地 SDL 态（表现层，不入 sim）
- **R19 断线掉落**：移除 = 库存 100% ItemEntity 掉落（死亡掉落同路径 game_entities.cpp 族）
  + 坐骑落乘 + 效果清除——全共享世界态确定性； CTRL 帧边界应用（等包纪律）
- **R20 门双进程环境**：xenv.sh X 显示 ×2 窗口 + TCP loopback；PITS 388 纪律（timeout 包装
  pid ≠ 子进程——双进程各直跑 tp=$!）；两进程各 8 worker 线程 = 16 核机负载（门机 18 核 ✓）；
  脚本输入 = --demo-author 时间线复用（PITS 403/404 demo 纪律族适用：观察红线/路径重写）

## 6. 偏离候选（预列供立项裁决，15 条）

- **D1 无 P2P / 无跨网**：单主机 LAN TCP；主机 = 权威（时钟 + 种子 + 版本 + kick）；
  12c 跨网 = 倾向不做（NAT/可靠传输 = 无项目先例的大面）
- **D2 主机单点故障**：主机断 = 全体退（MC dedicated = 服务器续 / LAN = 客户端退主机续——
  本项目 12b 取最简：主机 = 会话）
- **D3 无本地预测**：统一 N−D 延迟 38-42ms（MC = 客户端预测 + 服务器校正）；12c 复议
- **D4 12b 无聊天**（12c：frameText 载体现成）
- **D5 12b 无物品交换/交易**（12c 候选；barter 机制 = mob 面非玩家面，无先例）
- **D6 挖掘进度 per-player 独立**（MC 协作挖掘 = 共享进度条；12c 候选）
- **D7 无玩家间碰撞**（MC 有推离；12c 简单推离候选）
- **D8 远程玩家静默**（音效 = 本地玩家 only；MC = 3D 空间音传播）
- **D9 12b 全局维度 = primary**（远程 portal 禁用；per-player 维度 = 12c）
- **D10 12b mob 目标/生成锚 = primary 玩家**（MC = 最近玩家；12c 最近化）
- **D11 12b creative 多人禁用**（文本中继 + 面板 parity 复杂度；单机 creative 不动）
- **D12 12b 教程仅主机**（per-player 教程态迁移了但事件锚 = primary，12c 复议）
- **D13 12b 存档 = 主机单 player 段**（远程个人态不落盘；format 33 players[] = 12c）
- **D14 N 上限**：12b = 2；协议 N ≤ 8（FrameState 定长数组）；12c config 定案
  （MC max-players 20；lockstep 瓶颈 = 主机 CPU 每 +1 玩家 ≈ +1 玩家 sim pass）
- **D15 无游戏内发现**：IP:port 直连 + HELLO 携带 MOTD（MC = 服务器列表 + Realms）

（完）
