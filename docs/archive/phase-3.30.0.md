# Phase v3.30.0 — V3-12c1 大厅/聊天/N 泛化/心跳（门 220 全 PASS，2026-11-07）

## 范围与定案

**V3-12 多人收官相位 c1 = 大厅 + 聊天 + N 泛化 + 心跳**（12c 拆分定案
的 1/2；c2 = v3.31.0 门 221 断线恢复 + format 33 players[] → 矩阵
全编号完成）：

- **N 泛化**：`kNetMaxPlayers` 2→8（协议形——FrameState 数组随
  FRAME 的 u8 计数走，codec 早已泛化）；**accept 窗 N 定形**：首客
  10s 阻塞 accept（设计行为 = 主机等玩家）+ 3s 窗 250ms 截止重试
  （静默尝试不关窗——门内捕获单次 break 坑）+ **窗关闭后统一
  WELCOME flush**（所有客户端看到同一 N = lockstep 前提；窗内拒
  收 = REJECT 点名续窗）。
- **大厅**：第三人称**名牌**（world→screen 投影，kFovDegrees/
  kNear/kFar 三常量从 game_render 移入 game_local.hpp 共享 = 投影单
  源；本地玩家无牌 = 第一人称缺席 MC 孪生）+ **F3 第 22 行
  players**（`"players N: host; name..."` Latin 零新字形——528/528
  满区不碰，test re-pin 21→22 行）。**MOTD 意念并入 V3-17 主菜单项**
  （11-07 用户实机诉求立号——菜单系统才是大厅/MOTD/设置/登录的家）。
- **聊天**：T 切换（net 下 T = 聊天，教程跳过 = solo only 门
  D12 延伸）+ 活 pump 本地面（m_chatInputActive/m_chatBuffer +
  frameText 累积 + frameBackspace + Enter 发送 + ESC 取消 + SDL 文
  本输入仅聊天期开——V3-11b 创造搜索先例同款）+ **CHAT 包
  type 7** {slot u8, u16 len, ≤256 字节}（客户端→主机，主机重广播
  其余 + 本地 displayChat：8 行环 m_chatLines 300f 淡出左下浮层）
  + **Latin only 零字形**（latinOnly 净化工具移 net_proto.hpp 共享；
  非 Latin 字节丢弃，空 = 发送 no-op）+ **聊天 = 表现层结构不入哈
  什**（不入 sim 不入 FrameState——textLen 恒 0 的 12b 预留位保持
  0，CHAT 独立包型 = 设计裁决）+ **打字抑制**（门内捕获：typed
  'e' 开背包面板 → 面板 open 动作关文本输入 → 消息截断
  "hello gate"→"he"；修 = Input 可打印域 0x20..0x7e 在 owner 机
  FrameState 捕获前本地吞，ESC/Enter/Backspace/功能键保持存活——
  抑制是 owner 本地动作，中继态 = 干净零，跨机零分歧）。
- **心跳（R13）**：lockstep 屏障 stall 检测 = **min 水印**（最滞后
  远端的水印 10s 无进展 = 点名移除最滞后者，会话续跑；门内捕获
  anyProgress 逻辑——健康同伴的进展遮死者的静默 = 永久死锁形，
  改 min 水印进展基准）。客户端等包 waitPacket 10s 超时点名退
  出（D2 主机单点面）。
- **E 面板中继修（门内捕获的真 desync 根）**：12b 的 E 分支
  `isLocal &&` 门控 = 客户端忽略主机玩家的面板边沿（主机用户按 E
  → 主机暂停 player 0 物理，客户端不暂停 → f570 哈希点名）。修 =
  **E/ESC 边沿对全部玩家处理**（边沿 = 属主机的物理按键经捕获中
  继，全机对称）+ **窗口/声音动作 localOwner 自门**
  （openPanelFor/closePanelFor 内 `&p == localPlayer()`——远程玩家
  的面板状态是 sim，其窗口动作不碰本机指针/文本输入/声音）。
- **客户端全槽体（门内捕获的 segfault 根）**：12b 的
  `netAddPlayer(1)` 硬编码在 N>2 下 = slot 2 的客户端缺自己的体
  （localPlayer() = m_players[2] 越界 → SIGSEGV）+ 客户端只模拟
  2 玩家 vs 主机 3（f0 哈希 desync 孪生）。修 = 客户端
  `netAddPlayer(1..n-1)` 全槽（lockstep = 每机模拟每玩家，player 0
  = 主机体 = 各机 buildScene 本机构造）。

**format 32 不变**（大厅/聊天/名牌全表现层）/ **sim 2 不 bump**
（canary 4 EXACT 执法——打字抑制/聊天/E 修 solo 全冷）。

## 变更文件

重构 ~10：game_net.cpp（全量重写 = N 泛化：m_nets vector 每远端一
连接 + netConsumeRemote per-remote + 屏障 per-remote 水印 + stall
min 心跳 + CHAT 重广播 + accept 窗 + 尾帧容错）/ game.hpp（net 成
员泛化 m_frameRings/m_netInputs/m_remoteFrameMax vector +
m_playerNames + ChatLine+m_chatLines + m_chatInputActive/
m_chatBuffer + 新方法族）/ game_ui.cpp（E 分支去 isLocal 门 +
openPanelFor/closePanelFor 窗口动作 localOwner 自门 + 聊天输入块 +
聊天浮层/名牌渲染 + F3 players 填充）/ input.{hpp,cpp}
（GameKey::Enter = FlyToggle 前【mapKey 永不返回纪律不动】+
mapKey SDLK_RETURN/KP_ENTER + setTypingSuppressed 打字抑制 +
preloadFrame 不变）/ net_proto.{hpp,cpp}（kNetMaxPlayers 8 +
NetType::Chat=7 + packChat/unpackChat + Reader::str8_len +
latinOnly 共享 + unpackWelcome players 界）/ net_session.{hpp,cpp}
（startHost timeoutMs 参数化【select 式 accept】+ fd() +
selectReady 静态多路 + waitPacket 超时参数化）/ game_init.cpp
（客户端全槽 netAddPlayer + accept 窗接线不变）/ game_local.hpp
（kFovDegrees/kNear/kFar 三常量共享）/ game_render.cpp（三常量移
出）/ debug_overlay.{hpp,cpp}（DebugInfo.players + 第 22 行）/
main.cpp（不变——CLI 面 12b 已齐）。

测试：test_debug_overlay F3 22 行 re-pin + banner 3.30.0。

## 门 220 结果

- **S1 结构免疫第 101 次**：≡ base19 逐字节 ×2 + savenorm EXACT
  （聊天/名牌/F3 行/打字抑制全冷——netActive() 门 + 可打印抑制旗
  solo 恒 false）。
- **canary 4 demo ×2 RAW + savenorm EXACT**（sim 2 不 bump 执法——
  E 修/打字抑制触碰 demo 路径【tutorial 系列有 E 操作】位级实证）。
- **mobs-on 无输入 900f ×2 RAW + EXACT**（实体 pass 位级）。
- **3P 锁步臂（host + 2 clients，N=3，600f，×4 批次 19933-19936）**：
  三进程 `net hash` f0/f300【+ f600 大厅臂】**跨批逐字恒等**
  （58942a79ad3d04bd / 5b4f23f30d7db37a——4 个独立批次同 digest =
  lockstep 最强形）+ 主机档 savenorm 跨批 EXACT + 零 desync 行。
  截图 RAW ×2：各进程至少一批全干净（host 19933/19934，c1
  19935/19936，c2 19934/19936）；跨批个别文件的纯彩块垃圾帧 =
  **Mesa 无头前缓冲读回竞态**（PITS 411⑥——进程轮换波及，哈希/
  savenorm 权威不受影响）；几何噪声面 = config_p3
  resizable=false 钉 1280×720（PITS 388 WM 平铺族 D19 延伸）。
- **大厅截图臂（2P，--script 确定性视角）**：host 脚本 = LMB 捕获
  + mouse_move dx 620（yaw -88.8° 正对 +X 远程玩家，存档实证）+
  C 第三人称 + F3 → 截图 ×2 RAW + 主机档 EXACT + 哈希逐字；OCR
  实证：**名牌 "client"** + **F3 players 行 "players 2: host;
  client"** + camera 行 "ThirdBack D 4"。
- **聊天臂（2P，xdotool 实打——PITS 400 先例）**：LMB 捕获 + C +
  F3 + T + type "hello gate" + Return → **双进程日志
  `net chat[host]: hello gate` 逐字** + 双截图 OCR "host: hello
  gate" 浮层 + 零 desync + 尾帧竞态容错后双方 rc 0 干净退出。
- **S6 八面复验（新二进制全 rc 1 点名）**：sim 不匹配（假 HELLO
  sim=3："sim version mismatch (the host 2 / the client 3)"）/
  magic 破（"the stream's magic broke"）/ assetFNV（变异资产
  "asset fingerprint mismatch"）/ config parity（walk_speed 9.9
  "sim config parity mismatch"）/ window parity（width 1024
  "window parity mismatch"）/ --connect×--save / --listen×demo 族 /
  端口占用（"net bind : Address already in use"）。
- **accept 窗 2 新面**：2 客户端窗内 → N=3（"slot 1 of 3" + "slot
  2 of 3" 实证）/ 单客户端 + 窗 3.01s 超时 → N=2 续跑（连接→
  WELCOME 间隔实证 = 截止纪律）。
- **性能 A/B 同档**（旧 = 1f2e10f v3.29.0 + g139 双树：banner 实证 v3.29.0/v3.30.0 + OW 900f 无输入：PERF-WIN 三窗 upd med 0.426/0.444/0.426 vs 0.399/0.402/0.497 同档 + render 0.495/0.480/0.433 vs 0.464/0.456/0.507 同档 + swap 15.6 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm EXACT 零 format 最强形 = 大厅/聊天/N 泛化/打字抑制的 solo 路径（净面全冷 + 抑制旗 solo 恒 false）结构性零成本；净自成本（新树 mobs-off 600f）：solo upd med 0.472/0.424 vs 2P 主机 0.687/0.729 vs 3P 主机 0.736/0.402 = +~0.2-0.26ms/帧（per-player sim pass + per-remote 轮询/屏障 + socket，sub-ms 同档），render 0.452/0.525/0.561 同档 + swap med 15.5/14.9/14.6 同档【net 跑 swap max ~30ms = 屏障等待尖峰预期】+ 零 desync）。
- **2974/2974**（12b 基线不变——c1 零新增单测面：codec CHAT 往返
  由门臂 E2E 执法 + F3 行 re-pin 既有测试覆盖）；
  `-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- **门内捕获 9 坑全 PITS 411**：① accept 窗 250ms 单次静默即
  break（c2 ~1s 后连接 = refused 实证——静默尝试须截止前重试）
  ② 首客 accept 超时 ms/s 混用（"no client in 10 ms" 实证——
  kNetStallTimeoutSec 是秒，startHost 参数是毫秒）③ 客户端
  netAddPlayer(1) 硬编码（slot 2 缺体 SIGSEGV + f0 哈希 desync 孪
  生实证）④ E 面板 isLocal 门 = 主机玩家面板 desync（f570 实证）
  ⑤ typed 'e' 开面板截断消息（"hello gate"→"he" 实证——打字抑制
  = owner 机捕获前本地吞）⑥ Mesa 无头前缓冲读回竞态（纯彩块垃圾
  帧进程轮换，哈希/savenorm 权威不受扰）⑦ WM 平铺几何噪声
  （resizable=false 钉几何 + requested parity D19 延伸）⑧ 尾帧
  send vs 对端干净退出竞态（broken pipe = named harmless——
  finalFrame 容错）⑨ 屏障 anyProgress 死锁形（健康同伴遮死者——
  min 水印基准）。

## 偏离记档（12c1 增补 D21-D23）

- **D21 聊天 = 表现层**：不入 sim 不入 FrameState（textLen 恒 0）
  不入状态哈希——CHAT 独立包型；打字抑制 = owner 机本地（捕获前
  吞，中继态干净零）；Latin only 零字形（CJK 聊天 = V3-17 菜单项
  字体面复议）。
- **D22 名牌 = 第三人称 + Latin**：本地玩家无牌（第一人称缺席）；
  投影常量三件套与 3D pass 同源（game_local.hpp 共享）。
- **D23 accept 窗 N 定形**：首客 10s + 3s 窗 250ms 重试 + WELCOME
  窗后统一 flush（同 N）；窗内拒收 = REJECT 续窗不占名额。
- **MOTD 并入 V3-17**：12c1 大厅 = 名牌 + F3 行 + 聊天（最小形）；
  MOTD/服务器列表/设置/登录/联机页 = 主菜单系统（11-07 用户实机
  诉求立号）。

## 后移（V3-16 / V3-17 及 12c2 候选）

**V3-16 扩展指导 demo 族**（11-07 用户实机）：10 主题 3-10 分钟
级（下界门/末影龙/房子+床/挖矿工具/体力/回血/皇宫/材料收集/河流/
完整黑夜历险）——authored timeline 管线 + 分组元数据。
**V3-17 主菜单系统**（11-07 用户实机）：标题/设置/登陆/教学/多人
五页 + MOTD 并入——大项立项时相位拆分。
**12c 其余候选**（12c2 复议）：per-player 维度（D9）/ 最近玩家目
标（D10）/ 玩家碰撞（D7）/ 协作挖掘（D6）/ 物品交换（D5）/
creative 多人（D11）/ 本地预测（D3）/ swing 体（D18）/ aim 相机
形（D17）。
