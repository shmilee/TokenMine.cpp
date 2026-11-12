# Phase v3.46.0 — V3-17d 主菜单：多人页 + MOTD（门 236，V3-17 全 4 相位收官）

> V3-17 相位 D/4（17a 标题 v3.43.0 / 17b 设置 v3.44.0 / 17c 登陆 +
> 教学 v3.45.0 / **17d 多人 + MOTD = 本版，矩阵收官项 23/23**）。
> BACKLOG 规格：⑤ 多人页（主机 --listen / 加入 --connect host:port +
> 名字；12c1 大厅 MOTD 意念并入此项）。**零字体**（创建/游戏/加入
> 全在 457 表——「游戏」二字 17a 侦察缺口靠「创建游戏/加入游戏」
> 措辞核证补全，无新字形）= 跨树 RAW 恒 EXACT 门面延续。

## 实现

### 多人页 + 加入子页（两级 = 教学同款形态）
- **多人页**：名字字段（m_menuName——与登陆页共享同一名字态，
  文本输入生命周期 m_textInputOn 扩至 {Login, Multiplayer, Join}，
  每页单字段 = 隐式焦点形）+「创建游戏」+「加入游戏」+「返回」。
- **加入子页**：host:port 字段 +「host:port」hint +「开始」+「返回」
  + 解析拒收行（"invalid host:port" Latin，kSelected 橙）。
- **menuParseHostPort 纯函数**：host = [A-Za-z0-9.-]{1..255}，
  port = 纯数字 1..65535（单冒号分割在 loop 面）；test 钉 7 合法 +
  10 拒收面。
- 创建游戏 = m_args.newWorld + listenPort kNetDefaultPort（具名
  5555，MC 读；CLI --listen <port> 显式面不变，既存档主机 =
  CLI-only D16 先例延续）→ menuExit 3；加入 = host:port → m_args
  .connectHost/connectPort → menuExit 4。

### 网络接线（提取单源 = 17c applyDemoArgs 范式）
- **netClientStart()** = init 的 --connect 块原文提取（握手 +
  WELCOME 种子覆写 config）；**netPostScene()** = init 的 --listen
  块 + REJOIN 快照 + 客户端全玩家体原文提取 + MOTD 首聊行消费。
  CLI init 原位调用（位级不变），菜单 run() menuExit 3/4 调用
  （host = buildScene 后 / client = 握手在 buildScene 前 = WELCOME
  种子权威）。
- **名字消费**：m_menuNetActive 旗（菜单驱动 net 的 marker）——
  客户端 hello.name = 菜单名（CLI 恒 "client" 位级红线）；主机
  WELCOME.hostName = 菜单名（CLI 空 = 客户端回落 "host" 位级
  红线）；主机自身 m_playerNames[0] 同款。
- **WELCOME +2 字段**：hostName（str8 ≤12，空 = 旧形）+ motd
  （str16 ≤ kNetMotdMax 64）。混版 = HELLO TM_VERSION 指纹先拒
  （既有 face）→ schema 增长兼容性免费。
- **MOTD** = 主机 config `net.motd`（getString 默认 "TokenMine.cpp"
  ——键缺省不写用户 config，P32 纪律）latinOnly + 64 cap；客户端
  消费 = 日志点名 + **世界建成后的首聊行** displayChat(0, motd)
  【12c1 大厅 MOTD 意念的最小形——服务器列表 ping 面不做，记录
  偏离；表现层不入哈希（12c1 D21 先例）】。

### 死码清除
menuStubLayout/menuStepStub/kMenuLaterFace（17a 的多人 stub）+ 其
2 单测删除——17d 实面取代（项目无死码纪律；教学 stub 17c 同款先
例）。

### 测试（+4 = 3021）
MenuMultiplayerTest Layout/Step + MenuJoinTest Layout/Step/
ParseHostPort（10 面）+ NetProto.WelcomeHostNameAndMotd（round-
trip + 空形 + cap 拒收 2 面 + 边界 12/64 含界）；Stub 2 删（净
+4）。

## 门内捕获（PITS 428 四坑）
① **WM 二窗互杀 + reparent BadWindow**（OpenChamber 环境实证，4 连
踩）：第二个游戏进程窗口映射时**第一个**游戏窗口从 X 树消失（进
程存活、SDL 正常，search 恒空）——双窗 xdotool 臂只有单向可行：
受驱动窗 = 存活窗。定式 = 菜单主机臂【菜单先启 + CLI 客后启】/
菜单客机臂【CLI 主机先启 + 菜单客后启】，反序必败；窗口 id 随 WM
reparent 变 BadWindow（windowactivate 后首点击常成、随后 stale）
= findw 重试循环（search --pid + getwindowname 活性验证）必备。
② **R7 菜单面漏网**：「client never writes」原仅 argv 层执法
（--connect×--save 拒收）——菜单加入运行时才设 connectHost，
--save 静默通过 = 客机退出写档可覆主机槽（同 slot 名形）；修 =
写档单收口（run 尾 writeSave 门）+ `connectHost.empty()` 守卫
（CLI 客 argv 先拒 + 菜单客运行时名 = 双形单闸）；**直接实证**：
连上的菜单客机带 --save 跑完整会话 → 槽不创建 PASS。③ **60fps
机的菜单臂帧预算**：机载漂移致菜单渲染 20→60fps，--frames 300
的窗在操作序（~8-10s）中途退出 = BadWindow 假象（窗口死 = 进程
退出的首征）——菜单操作臂一律 --frames 700。④ **xdotool 名搜索
环境污染**：用户 Firefox 标签「TokenMine.cpp | OpenChamber」含
产品名 = search --name "TokenMine" 假命中——锚定正则 ^TokenMine.
cpp$ + 双进程臂异标题 config 变体（window.title "TokenMine.cpp
HOST"，HELLO 指纹不含 title 安全）+ search --pid（无 timeout 包装
直跑 = _NET_WM_PID 可用，PITS 388① 正读）三路冗余。

## 门结论（门 236 全 PASS）

**3021/3021**（+4 净：MenuMultiplayerTest Layout/Step + MenuJoinTest
Layout/Step/ParseHostPort + NetProto.WelcomeHostNameAndMotd；Stub 2
删；banner re-pin 1）。

**S1 重锚 base20 = v3.46.0 标题**【f60 无输入 ×2 RAW 逐字节 + 旧锚
diff 65 px 单行 rows 164-169 = 版本行 v3.45.0→v3.46.0 有界断言 +
defined mouse (10,10) + 槽不创建 → 21917 B】。

**登陆臂回归**【17c 面未动：单人 → gate → 开始 → 600-f：日志
name 'gate' + **存档 ≡ --new savenorm EXACT**】。

**网络臂三形×2（菜单驱动 net 全 E2E）**：
- **A 菜单主机 + CLI 客**：hgate 名 → 客机 `net chat[hgate]:
  TokenMine.cpp` 首聊行【**WELCOME hostName 消费实证**】+ MOTD 日
  志 + 主机 `client 'client' joined`【CLI legacy 名位级】+ hash
  f300/f600 跨机逐字 ×2 + 主机 save savenorm ×2。
- **B CLI 主机 + 菜单客机**：cgate 名 → 主机 `client 'cgate'
  joined`【**HELLO name 消费实证**】+ 客机 `net chat[host]:
  TokenMine.cpp`【**空 hostName 旧形回落位级**】+ hash 逐字 ×2 +
  主机 save savenorm ×2（客机不写 = R7 守卫生效）。
- **D 纯 CLI**：legacy "host"/"client" + 默认 MOTD 首聊行 + hash
  逐字 ×2 + save savenorm ×2 = **CLI 位级形实证**（17d 对 --listen/
  --connect 启动零行为差）。
- f300 跨臂差 = 世界首 5s 流送 churn 的 real-dt 域（changesApplied
  9/3/1 同款），f600 全 6 臂恒同 = 确定性稳态；臂内 r1≡r2 为权威。

**Canary 4×2 新≡旧（tm_v345 v3.45.0）**【solo 路径 net 面全冷 =
结构免疫】：树内 RAW r1==r2 EXACT ×4 + savenorm 跨树 EXACT ×4 +
**跨树 RAW EXACT ×4**（零字体 = v-UV 不变，PITS 425④ 不复发）。

**性能 A/B 同档**【OW 900f --new 双树 g139：new upd med 0.607/
0.599/0.599 vs old 0.591/0.597/0.584（±0.02 内，当日轮间漂移带
0.72-0.78 量级远大于树间差）+ render 0.708/0.760/0.739 vs
0.689/0.751/0.702 同档 + swap 15.11/15.13/15.13 vs 15.20/15.15/
15.19 同档 + **changesApplied new 9/3/1 vs old 9/2/1**【win2 =
streaming 边界 real-dt 域，门 207/210/217/235 先例】+ **终档跨树
savenorm EXACT** = 多人页/MOTD 零世界面结构零成本】。
