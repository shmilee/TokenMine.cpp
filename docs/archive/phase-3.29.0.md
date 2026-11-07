# Phase v3.29.0 — V3-12b 2 人 PoC（门 219 全 PASS，2026-11-07）

## 范围与定案

**矩阵最后项 V3-12 多人相位 12b = 2 人局域网 lockstep PoC**（设计权威
`docs/V3-12-MULTIPLAYER-DESIGN.md` §4 全采纳）：主机权威 lockstep——
主机 = 时钟源（clamped wall dt raw f64 位广播）+ 输入中继（FrameState
~40B/帧/人，D=2 统一延迟无预测）+ 种子/版本权威（HELLO 四指纹）；
客户端循环 = demo 回放形状 + net 源；世界传输零（worldgen 纯函数同
seed 位级）。**format 32 零 bump**（主机单存档，远程个人态不落盘
= D13）/ **sim 2 不 bump**（canary 4 EXACT 执法——solo 路径位级不变
= 结构免疫根）/ 协议 N ≤ 8（12b 恒 2，D14）/ **12b 玩法面 = 全 sim
覆盖**（挖掘/放置/物品/生存/死亡/骑乘/珍珠/交易全 per-player——
lockstep 验证需全 sim 面）/ **12b 不做** = 大厅/聊天/重连/预测/
跨维度玩家/creative 多人/玩家碰撞（全 12c 候选，D1-D15 预列）。

### 实现面（按设计 §4 实现序）

1. **src/net/ 新目录（~1200 行，零新依赖 POSIX TCP，无 net 线程）**：
   - `net_proto`（纯函数层，socket 不入单测）：小端固定头包
     {magic TMN1, type, len} / HELLO 四指纹 {TM_VERSION, sim, assetFNV
     （assets/** 排序树 FNV-1a 64）, simConfigHash（sim 相关键固定表——
     world.seed 豁免【WELCOME 种子 = 主机权威】，render.* 豁免【dt
     广播归一】）} / WELCOME（fresh = 仅 seed 零字节）/ INPUT{frame,
     FrameState, hash} / FRAME{frame, dtBits, states×N, hash} / CTRL /
     REJECT（点名串）；FrameState 40B 定长 {keysDown/Pressed u64,
     buttons 3×u8, mouseDx/Dy f32 raw 零量化（R4）, mouseAx/Ay u32
     （R8 面板落格）, wheel, textLen（12b 恒 0）, locked u8（R18）}；
     **状态哈希字段清单**（§2.3 定案：frameCount / day time / weather
     3 件 / 每玩家 pos/vel/yaw/pitch/HP/hunger/sat/creative/flying/
     dead/xp/xpSeed/spawn + 会话计时器族 + 36 格库存摘要 + 效果实例 /
     每维实体插入序 {kind tag, pos, 逐型字段, mob rngState} / 每维
     journal 全集 / spawnCounter×3 / particle counter + 粒子数 / fires
     计数）——FNV-1a 64 原始位喂入。
   - `net_session`（传输层）：阻塞握手（accept/connect/HELLO 10s 超时
     点名）+ 帧头零超时 poll + **流重组 feed/drain 缝**（R16 半包/粘
     包，单测面）+ magic 破 = 流损坏旗点名 + 全循环 send（MSG_NOSIG-
     NAL，EPIPE 点名）。
2. **player 多人化重构（双别名最小方案，设计 §3.1）**：
   - `m_players` vector + `m_localSlot`；`m_player` = **raw pointer
     primary 别名**（354 处 `m_player->` 零改动；emplace→push_back
     1 处 + has_value→!=nullptr 1 处）。
   - **PlayerSession 结构体**（吸收 10 成员族：inventory/selectedSlot/
     interaction/panel/effects/drown/hurtFlash/prevHealth/pearlCD/
     ridingMount/sessionStats/stats/death 统计/tutorial 三件/
     lastSpacePress + 新 inputLocked R18 旗）；`m_sessions` 平行
     vector；**14 个引用别名绑 session 0**（primary——~430 处引用零
     改动；solo primary==local 位级根）。
   - **pass 归属二分**：primary 锚不动（mob 目标 setPlayer / 生成锚 /
     教程事件锚（D12 actor 门）/ 龙 / portal（D9 全局维度）/ 分离
     pass）；**×N 循环化**：物理（含 R18 锁镜 + 教程 walk/jump 事件
     primary 锚）/ 溺水 / survival（死亡计数 per-player，死亡声/释锁
     本地化 D8）/ 效果（含 beacon ×N）/ ride（dismountRideFor）/ 珍珠
     冷却 / 交互全链（handleInteraction + 17 子函数 For 化：挖掘/
     攻击/弓/放置/桶/奶/交易/骑乘/嵌眼/珍珠/剪毛/染色/吃/喝/睡/盔甲
     + tryCampfireCook + openEnchant/Table/FurnacePanel + regenEnchant/
     applyEnchant/returnEnchant）/ 拾取磁吸+三拾取（最近玩家 + 逐玩家
     接纳，D8 拾取声本地）/ 眼接触（每人 gaze 可激怒）/ 箭扫掠（任一
     玩家可中）/ 教程 toast / 面板（E/ESC = 本地玩家 R17，远程 E
     no-op；面板点击走 per-player 输入）/ despawn 环（任一玩家近处
     即留）/ F3+HUD+死亡屏+手臂+雨盒+雾（localSession 读）。
   - **per-player aim 族**：solo = 相机路径位级（aimForward/aimReach/
     targetBlock 原读）；net = 第一人称形（p.yaw/pitch 前向 + kReach，
     **12b 偏离 D17**：相机模式本地不中继）——aimDir/aimReachV 门
     注入全部 ×N 函数。
   - **R2 simTime**：`m_simTime += dt`（net = 广播 dt 累加两机同序）；
     Space 双击 + 库存双击两 wall 直读点 net 门读 m_simTime，solo
     原文位级。
   - **R10 xpSeed 盐族**：`kSaltPlayerXp[2] = {0, 0x10D0E11D5E4D73A7}`
     （[0]=0 solo 位级；A7 = 0x10D 族 A2-A6 后首空位碰撞检）。
   - **R9 出生点**：kSpawnCandidates 全有效候选收集（m_spawnCandi-
     dates）+ spawnPositionForIndex(i) 回绕（player i = 第 i 候选）。
3. **streaming 跨机纯度修正（§1.5）**：盘锚经 m_player 别名自动 =
   primary（零改动）；回退门 `pool.pendingCount()`（本机吞吐 = 静默
   desync 根）→ `streamPending.size()`（共享账本）。
4. **host/client 双循环（runGameplayLoop 单循环 + net 钩子）**：
   - 帧头：poll（主机：收 INPUT 入账 + **lockstep 屏障**——等客户端
     INPUT[N−D] 水印达标才推帧【门内捕获：无头主机快跑吃掉未发输入
     = 冷零假象】；客户端：waitPacket 阻塞等 FRAME{N}【等包阻塞模拟
     = 设计定案】，帧号失序点名）。
   - 帧尾：主机发 FRAME{N, dt, states[N−D]×N, hash}；客户端发
     INPUT{N, 本帧捕获, hash}。
   - **哈希交换**：每 30 帧各机自算 + 随包交换（双向检测零新包型）；
     每 300 帧 DEBUG `net hash f<N> <u64>`（门臂断言面）；不符 =
     点名退出（含两侧十六进制）。
   - **断线（R19/D2）**：主机 EOF/客户端 RST（send EPIPE 同形）→
     CTRL 语义移除 player 1（库存 100% ItemEntity 掉落 + 落乘 + 效果
     清）+ 会话 solo 续跑；客户端见主机 EOF = 全体退出（主机单点）。
   - **CLI**：`--listen <port>` / `--connect <host:port>` / `--script
     <timeline.json>`（demo 时间线第四消费方——确定性输入源，× demo
     族互斥；--connect×--save 点名 R7；--listen×--save 允许【主机
     单存档】但 save 形 WELCOME = 12c 面点名【**D16**：12b 净世界
     恒 fresh】）。
5. **12b 偏离增补（D16-D20）**：D16 净世界 fresh only / D17 net aim
   第一人称形 / **D18 远程玩家体 = 静态姿态**（swing VBO 单实例，
   12c per-player）/ **D19 window parity = config 请求几何**（无头
   WM 平铺竞态 = 环境噪声，PITS 388 族）/ **D20 客户端干净退出 =
   TCP RST**（主机 send 失败 = 移除，EOF 同形）。

## 变更文件

新增 5：`src/net/net_proto.{hpp,cpp}` / `src/net/net_session.{hpp,cpp}`
/ `src/core/game_net.cpp` / `tests/test_net.cpp`。
重构 ~20：game.hpp（PlayerSession + 双别名 + 14 引用别名 + friend
哈希面 + net 成员族）/ game_init.cpp（循环接线 + 握手 ×2 + script
注入 + 出生索引 + fillStarterInventory 提取）/ game_frame.cpp（×N
pass + inputFor + m_simTime）/ game_interaction.cpp（17 函数 For 化
+ aim 族 + R18/R2 门）/ game_ui.cpp（×N survival/effects/panel/
UiKeys/renderUi local 化 + For 族）/ game_entities.cpp（磁吸/三拾取/
眼接触/箭/despawn/玩家体 ×N）/ game_streaming.cpp（回退门）/
demo.{hpp,cpp}（parseScriptTimeline）/ input.hpp（preloadFrame 注入
缝）/ projectile.hpp（throwerIndex）/ mob_entity.hpp（rngState）/
enchanting.hpp（kSaltPlayerXp）/ window.hpp（requestedW/H）/
main.cpp（CLI 三面 + 互斥）/ CMakeLists（3.29.0 + 4 源 + 测试）。

## 门 219 结果

- **S1 结构免疫第 100 次**：≡ base19 逐字节 ×2 + savenorm EXACT（净
  面全冷：m_net null + m_netActive false + size 1，loop 钩子零进入）。
- **S2 双进程 headless 锁步臂（核心，全 loopback 127.0.0.1）**：
  - **无输入臂**（seed 11 无火扫描定案，600f，×2 run）：两进程
    `net hash` 行 **f0/f300 逐字同** + 主机退出档 **savenorm 跨 run
    EXACT** + 各进程截图 RAW ×2 + 零 desync 行。
  - **输入注入臂**（host a.json：LMB 捕获 + W 280f + A 30f；client
    b.json：LMB 捕获 + S 260f + LMB 挖掘 65f，mobs-on，600f ×2
    run）：两进程哈希逐字同 + **client 挖掘经中继落主机档 journal
    【[0,25,10] air——跨进程世界收敛铁证】** + 双玩家位移（主机
    z=−14.1 / 客户端向南）+ 零 desync；跨 run 玩家位差 = real-dt
    积分域（广播 dt 两机同序，D2 主机权威，记档）。
  - **断线臂**（client --frames 300 干净退出 = 确定性断线，host
    --frames 600 续跑，×2 run）：`player 1 removed` f302 + **起始
    库存 100% 掉落 20 ItemEntity**（stone/dirt/grass/...×64）+ 主机
    截图 RAW ×2 + 主机档 savenorm 跨 run EXACT。
- **S6 八面点名（全 rc 1 点名）**：sim 不匹配（假 HELLO sim=3：
  "sim version mismatch (the host 2 / the client 3)"）/ magic 破
  （"the stream's magic broke"）/ assetFNV 不匹配（变异资产 client：
  "asset fingerprint mismatch"）/ config parity（walk_speed 9.9：
  "sim config parity mismatch"）/ window parity（width 1024："window
  parity mismatch"）/ --connect×--save / --listen×demo 族 / 端口占用
  （"net bind : Address already in use"）。
- **canary 4 demo ×2 RAW + savenorm EXACT**（sim 2 不 bump 执法——
  多人化重构未触回放语义）。
- **性能 A/B 同档**（旧 = a8259c8 v3.28.0 + g139 双树：banner 实证 v3.28.0/v3.29.0 + OW 900f 无输入：PERF-WIN 三窗 upd med 0.524/0.560/0.581 vs 0.544/0.582/0.586 同档 + render 0.556/0.674/0.637 vs 0.564/0.666/0.656 同档 + swap 15.3 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm EXACT 零 format 最强形 = 多人化重构的 solo 路径（净面全冷 m_net null + size 1）结构性零成本；净自成本（新树 mobs-off 600f）：solo upd med 0.574/0.587 vs 2P 主机 0.642/0.638 = +~0.06ms/帧（player-2 sim pass + 环 + socket，sub-100µs 同档），render/swap 同档）。
- **2974/2974**（+10：NetProto 8 + NetSession 2）；
  `-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- **门内捕获 9 坑全 PITS 410**：① **client 环槽 store 偏 1**（收到
  主机帧态存 f%3 应存 ringSlot(f)——主机侧 f%3 正确因消费帧 f+D
  的读槽恰合，客户端读槽 = ringSlot(N) 差 1 = 主机玩家输入晚 1 帧，
  f150 哈希点名抓获）② **主机快跑屏障**（无头主机 55fps 跑赢客户端
  0.3s 启动偏斜 → 吃冷零输入 = 静默 desync 根；lockstep 屏障 =
  INPUT 水印等待，因果链 F−D < F 无死锁）③ **客户端干净退出 = RST
  非 FIN**（主机 send ECONNRESET 原当致命 → 会话死；与 poll EOF 同
  形 = 移除续跑）④ **WM 平铺竞态打 window parity**（无头双窗尺寸
  各异 = 环境噪声；parity 改 config 请求几何 = D19）⑤ **HELLO
  uiScale 时序**（client 握手先于 buildScene——m_uiScale 未设 = 0.0
  误拒；改 config 直读）⑥ **world.seed 入 parity 误拒**（client 本地
  config 种子 ≠ 主机 --seed；WELCOME 种子 = 权威，豁免）⑦ **无输入
  臂跨 run savenorm 需无火种子**（fire real-dt 域，PITS 385/390 族——
  seed 11/22 无火，33/44/55 有火扫描实证）⑧ **--new 须 --save 卡净
  主机**（净会话世界恒 fresh，--new 无 slot = 合法形）⑨ **起始库存
  per-session**（netAddPlayer 空 Inventory = 断线零掉落静默；
  fillStarterInventory 提取共享）。

## MC 偏离记档（12b 全量）

设计稿 D1-D15 全文（无 P2P/主机单点/无预测 38-42ms/无聊天/无物品交换/
挖掘独立进度/无玩家碰撞/远程静默/全局维度=primary/mob 锚 primary/
creative 多人禁用/教程主机 only/主机单 player 档/N=2 协议≤8/无发现）
+ 本版增补 **D16-D20**（见实现面 5）。

## 后移（12c 候选，BACKLOG 定案）

大厅（MOTD/名单）/ 聊天（frameText 载体）/ 重连（save 形 WELCOME
journal 快照 = D16 复议）/ N=4（协议≤8）/ per-player 维度（Entity::
setWorld 先例）/ 最近玩家目标（D10 复议）/ 玩家碰撞（D7）/ 协作挖掘
（D6）/ 物品交换（D5）/ creative 多人（D11）/ format 33 players[]
（D13）/ 本地预测复议（D3）/ 心跳（R13）/ per-player swing 体（D18）/
aim 相机形中继（D17）。
