# 坑位合集（都踩过，勿再踩）

> Phase 30 文档整理：从 `SESSION_CONTEXT.md` **原样**移入（未改写）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/*.zsh`）指向当时状态；当前文档布局见
> `README.md`「文档地图」。
> pre-3.x 条目（PITS 1–384）→ `docs/archive/PITS_PRE3X.md`；本文件 = 3.x 时代 PITS 385–412，新条目继续前插于尾部块首。

- **gzip 容器可钉 / zlib 宏缺失 / 双跑 RAW 适用域（PITS 385，门 191
  V3-7 存档 gzip）**——① **zlib 自带 gzip 封装（windowBits 16+15）
  的 MTIME/XFL/OS 字节随 zlib 版本/发行版漂移**：确定性一级纪律要求
  同 payload 逐字节可复现 → 必须**手写容器**（固定 10B 头
  `1f 8b 08 00 00 00 00 00 00 03`【FLG=0/MTIME=0/XFL=0/OS=3】+ raw
  deflate【deflateInit2 windowBits=−15】+ 自算 CRC32/ISIZE 尾），
  且解码侧同 10B 头**逐字节全钉**（系统 gzip CLI 产物的真 MTIME =
  点名拒收，不静默容忍——机器生成文件纪律）。② **本机 zlib 1.3.2
  的 zconf.h 缺 `Z_DEFAULT_MEM_LEVEL` 宏**（上游 zlib 有，发行版
  裁剪；`#include <zlib.h>` 后引用 = 编译错实证）→ memLevel 用命名
  常量 `kSaveGzipMemLevel = 8`（= zlib 文档默认档）钉死，勿依赖该
  宏。③ **存档双跑 RAW 逐字节恒等的 E2E 适用域 = 无 real-dt 字段
  漂移的场景**：demo 区场景的 `fires[*].remaining`（硬规则 8 实火
  real-dt）必漂 = 既有 savenorm 剥离族（PITS 244/P56）——门 S1 口径
  = savenorm EXACT + 头 10B 钉 + 单测容器恒等（`SameInputYields-
  IdenticalOutput`），勿把「双跑 RAW 恒等」断言在有火场景当门标准
  （会假红）。
- **手柄实机验收三坑（PITS 395，V3-8a 11-06，用户山湾克隆
  045e:028e）**——① **SDL3 的 BUTTON/AXIS 事件须显式
  `SDL_OpenGamepad` 后才投递**（SDL 3.4.18 实机实证：PRE-OPEN 零事件 /
  POST-OPEN 全到，两段探针 = /tmp/opencode/gamepad_demo）：
  `GAMEPAD_ADDED` + 名字无 open 也有（"Gamepad connected" 日志正常）=
  **connected ≠ 可用**——门 193 合成事件单测永不暴露此 = "实机验收归
  用户"的结构性缺口。修 = V3-8a（d5a5ea6）：ADDED + open（失败 WARN
  点名、跟踪态保留）/ REMOVED + close + `gamepadOpen()` 诊断 getter +
  1 新测；诊断纪律 = 手柄"连接"日志正常 ≠ 输入通道正常，必验按键事件
  行。② **SDL 打开 evdev 节点失败只静默返 NULL**（首案 "Unable to open
  /dev/input/event15" = 用户不在 input 组）：旧代码不 open 不报错 =
  "连上但无反应"日志零线索——SDL 边界 NULL 返回必点名
  `SDL_GetError()`；排查先跑 `id -nG | grep input` +
  `ls -l /dev/input/by-id/`。③ **克隆件：未 open + 按键 = USB 自断**
  （~6 s 周期、每次全量重枚举 which 递增、dmesg = `USB disconnect` 在
  先 xpad `-19` 在后 = 设备先消失非驱动 reset）——**四象限实证**：未
  open+静置 = 稳（实验 A）/ 未 open+按键 = 周期断（游戏 + demo#1 open
  失败态）/ open+按键 = 5 分钟稳（demo#2）→ open 状态 = 判别变量；
  机制（xpad 输出报告流 × 克隆固件）未实证 = 记档。误诊记录：
  `wine control` 可用 ≠ 游戏可用（Wine 不走 SDL）；A/B 双机混淆（内核
  层检查全在 A 机、现象在 B 机）+ TLP/autosuspend 首嫌被实机数据推翻
  = **硬件问题排查先钉死"现象在哪台机器、哪一层"**。
- **portal 框几何三坑（PITS 386，门 192 V3-1 Phase A 门框）**——①
  **点火瞄准 = 面法向相邻格方向**：框内 air 与 obsidian 环共面（单格
  厚），射线穿过内 air 先命中环块的**面**——命中面法向的相邻格必须是
  框内 air 才点火（aim 环格朝框内的面）；瞄准环格外侧面（法向邻格 =
  框外）= 机制正确地无反应（门内首跑假"失败" = 瞄准点选在外侧面
  （−1,55.5,−2.5）而非内侧面（0,55.5,−1.5）；aim 点 = 环格朝内面的
  面上点，不是内 cell 的"格中心"）。② **站入位置 cell 索引 vs 格中心
  差 0.5**：框平面 z=−2（cell 索引，跨 −2..−1）——玩家脚 z 须 ∈
  (−2,−1)（格中心 −1.5），写 −2.5 = 前格（z=−3）= 与 portal 零重叠
  （无倒计时 + 坠地假"物理异常"；门内首跑实证）。③ **V3-7 起门脚本
  读存档必须过 gzip 魔数**：场景 builder / 终档断言直接 json.load
  落盘档 = UnicodeDecodeError 0x8b（魔数第二字节）——gate_compare.py
  load_save 同款探测（raw[:2] == 1f8b → gzip.decompress）；场景文件
  手写明文仍可（零迁移），但 30f base 档是 gzip。
- **g139 插桩器锚点随调用点形态演进（PITS 387，门 194 V3-1 Phase B）**
  ——Phase B 的 DimCtx 参数化改了 update() 的 drain 调用点（collectLit
  Columns(); → collectLitColumns(overworldCtx()); + nether 分支）→
  `tools/g139_instrument.py` 的 STREAMING 锚点 loudly-fail（PITS 317
  "锚点断裂 = 失败不静默" 纪律按预期触发，非 bug）。修法 = **插桩器
  加双形态锚**（新形态优先匹配，旧形态保留 = 旧侧树可重建），随版提交
  入仓；每个 perf 树用**自己 commit 的插桩器**（git archive 含
  tools/）重建 = PITS 317 纪律的自然推论。FLUID 锚（m_world 调用形）
  在 Phase B 后仍唯一（nether 调用 = *m_nether.world 异文）——改
  调用点形态前先 grep 全部 g139 锚点核一遍。
- **REJOIN 快照面 + 幽灵/renumber/accept 节拍 + journal live-diff +
  respawn 0.5-y 八坑（PITS 412，门 221 v3.31.0 V3-12c2 断线恢复 +
  format 33）**——① **Json(bool) 指针陷阱**：`Json x = obj->find("k")`
  （const Json* 未解引用）= 隐式 bool 构造 → 后续 .at()/.find() 变体崩
  "std::get: wrong index for variant"；一律 `*obj->find("k")`（本会话
  test_durability 首踩，re-pin 族系统性排查）。② **legacyShape 改形不
  改 format 键**：33 format + legacy root 形 = 假 lie（"format 33 must
  not carry a root 'player'"）——当前 format 的负向测试必须走
  withPrimaryEntry（entry 级，format 保 33），legacyShape 仅用于降级
  声明 ≤32 的测试。③ **respawn int-cell 丢 0.5-y**：format 28 的
  int cell triple + bedSpawnPosition 还原对**非 cell-center 值**（net
  spawn 位整数 y = 地面顶）丢 0.5（host c1 spawnPos (4.5,52,0.5) →
  档 (4,52,0) → client 还原 (4.5,52.5,0.5) = hash player1 分歧根）；
  format 33 改精确 float triple，≤32 保 int cell（per-format 解析分
  支 + 测试 legacyShape 重编码 floor）。④ **removal 的 slot 重编号破
  旧客户端**：compact erase 使 slot 移位 → 已有客户端 ring 越界写
  （3 玩家帧 states 写入 2 行 ring = 堆破坏 → 后续 segfault）+ N 增长
  不知情 → 必须 **Join/Renumber CTRL 广播**（行随玩家移、输入历史跟
  走、无冲洗）。⑤ **中会话 accept 节拍**：barrier 停滞的 host 不再跑
  per-frame accept → backlog 连接等 15s（门内实证）——**listener fd
  入 barrier 的 select**（停滞期也 accept）。⑥ **client renumber 必须
  跑 100% drop**（调 netRemovePlayer 非裸 erase）——裸 erase 只 host
  drop、client 无 → ents 分歧（门内实证 c1@420）。⑦ **net 主机载多玩
  家档 = stale 幽灵 → slot/m_nets 错乱 → segfault**：重启后存档的其
  他玩家连接已死、slot 1.. 无连接但占位 → 新客 netAddPlayer(1) 覆写
  幽灵 + m_nets index=slot-1 映射破 → **net 主机（m_args.listenPort
  != 0）抑制幽灵**（玩家集 = {自身}+{新连接}，与 solo 档 host 等价；
  重连者 = 新体 D25，其旧物在 removal 的 100% drop 已在 entities 可捡
  回）。⑧ **feedJournal 须哈希 live-diff**（journal ∪ retained 对生成
  参考的 diff，journalDiff 单线程）非 journal 原始 cell——rejoin client
  回填 journal = 文档 cell、host live journal 含回滚编辑的幽灵 cell →
  原始 cell 集合不等（门内实证 journals 节分歧）；retained 本身即 diff
  逐字入。环境面：本机 Mesa GL init ~10s 恰超 host 10s 首客窗（竞态，
  host+client 同起或 client 迟 2s 入窗）；多进程负载下 client 首帧
  worker 饥饿 15s（PITS 388④ 独占纪律，门臂串行）。

- **accept 窗单次 break + ms/s 混用 + 客户端全槽体 + E isLocal 门
  + typed 'e' 截断 + Mesa 读回竞态 + WM 几何噪声 + 尾帧 broken
  pipe + 屏障 min 水印九坑（PITS 411，门 220 v3.30.0 V3-12c1 大厅/
  聊天/N 泛化/心跳）**——① **accept 窗 250ms 静默尝试单次即 break**：
  首客后窗内无新客的第一次静默 accept 就关窗 → c2 ~1s 后连接 =
  refused 实证；修 = 静默尝试**截止前重试**（`now - windowStart <
  3s → continue`），窗纪律 = 首客 10s + 3s 窗 + WELCOME 窗后统一
  flush（同 N = lockstep 前提）。② **首客 accept 超时 ms/s 混
  用**：`kNetStallTimeoutSec`（秒）直接传给 startHost 的毫秒参
  数 = "no client in 10 ms" 实证；纪律 = 超时常量单位入名（Sec/
  Ms 后缀）+ 传参处显式换算。③ **客户端 netAddPlayer(1) 硬编
  码**（12b 双人形）：N>2 下 slot 2 客户端缺自己的体（localPlayer()
  越界 SIGSEGV）+ 客户端只模拟 2 玩家 vs 主机 3（f0 哈希 desync
  孪生）——修 = 客户端**全槽** netAddPlayer(1..n-1)（lockstep = 每
  机模拟每玩家，player 0 = 主机体 = 各机本机构造）；纪律 = 人数
  泛化时逐处 grep 硬编码 slot。④ **E 面板 isLocal 门 = 主机玩家
  面板 desync**：12b 的 `isLocal &&` 门让客户端忽略主机玩家的面
  板边沿（主机按 E 暂停 player 0 物理，客户端不暂停）——f570 哈希
  点名实证；修 = **E/ESC 边沿对全部玩家处理**（边沿 = 属主机物理
  按键经捕获中继全机对称）+ 窗口/声音动作 **localOwner 自门**
  （`&p == localPlayer()`——远程玩家的面板状态是 sim，其窗口动作不
  碰本机指针/文本输入/声音）。⑤ **typed 'e' 开面板截断消
  息**：聊天打字含映射键（'e'=E 面板）→ 面板 open 动作关 SDL 文
  本输入 → 后续字符全丢（"hello gate"→"he" 实证）+ 键边沿进 sim
  （desync 孪生）；修 = **Input 打字抑制**（可打印域 0x20..0x7e
  在 owner 机 FrameState 捕获前本地吞，ESC/Enter/Backspace/功能键
  存活；抑制旗 on 时清 held 键防卡键）——纪律 = **文本输入期一切实
  键边沿必须在捕获前裁决**（表现/模拟二分在 Input 边界做）。⑥
  **Mesa 无头前缓冲读回竞态**：3 进程并行 load 下偶发截图 = 纯彩
  块垃圾帧（进程轮换波及，单批必有一中）——**权威 = net hash 行 +
  savenorm**（垃圾帧批次哈希/存档全绿），截图 RAW ×2 取各进程干净
  批次；环境面非代码缺陷，不修（前缓冲语义 PITS 388⑧ 不变）。
  ⑦ **WM 平铺几何噪声**：3 窗并发 tiled 尺寸各异（679×245 vs
  679×744）打截图 RAW 面——门臂 config 用 `resizable=false` 钉
  1280×720（D19 requested parity 的截图延伸；base19 锚 = solo 窗
  口不受扰）。⑧ **尾帧 send vs 对端干净退出竞态**：双方同
  --frames N 终帧，主机 frame-limit break 先关 socket → 客户端最
  后 INPUT send = broken pipe ERROR 噪声（模拟已完成 = 无害）；修
  = finalFrame 容错（F+1 == frames 的 send 失败 = 干净退出）。
  ⑨ **屏障 anyProgress 死锁形**：stall 检测用「任一远端达标」基准
  → 健康同伴持续进展遮死者的静默 = 最滞后永不移除永久阻塞；修 =
  **min 水印进展基准**（最滞后者的水印 10s 无进展 = 移除，移除后
  重基准）。- **client 环槽偏 1 + 主机快跑屏障 + RST≠FIN + WM 平铺 + uiScale 时
  序 + seed 豁免 + 无火种子 + --new 净主机 + 起始库存 per-session
  九坑（PITS 410，门 219 v3.29.0 V3-12b 2 人多人 PoC）**——① **lock-
  step 环槽 store 偏 1**：主机侧客户端 INPUT{f} 存 f%3 正确（消费帧
  f+D 的读槽恰合），但**客户端侧收到的主机帧态（属帧 f−D）必须存
  ringSlot(f) 而非 f%3**（读槽 = ringSlot(N) = (N+1)%3，差 1 = 主机
  玩家输入晚 1 帧）——f150 哈希点名抓获（f120 前 = W 按前位差未积
  累，首哈希违例在动作后第 3 个哈希窗）；纪律 = **环槽写读对称性
  用消费帧验证**（写 slot 的帧 X 必须被读 slot 的帧 X+D 消费）。②
  **无头主机快跑吃未发输入**：主机 55fps 跑赢客户端 0.3s 启动偏斜
  → 消费客户端 ring 冷零 = 静默 desync 根；修 = **lockstep 屏障**
  （主机 pre(N) 等 INPUT[N−D] 水印达标，waitPacket 10s 点名超时；
  因果链 N−D < N 无死锁）。③ **客户端干净退出 = TCP RST 非 FIN**
  （进程退出时缓冲区有未读数据 = 内核 RST）：主机 send ECONNRESET
  原当致命 → 会话死；与 poll EOF 同形 = 移除 player + solo 续跑。
  ④ **WM 平铺竞态打 window parity**：无头双窗被 WM 各自平铺（679×
  744 vs 1280×720）= 环境噪声（PITS 388 族）→ parity 改 **config
  请求几何**（requestedWidth/Height，D19 记档）。⑤ **HELLO 的
  uiScale 时序**：client 握手先于 buildScene（种子权威要求）→
  m_uiScale 未设 = 0.0 误拒 window parity；改 config 直读
  （uiScaleFromConfig）。⑥ **world.seed 入 simConfigHash = 误拒**
  （client 本地 config 种子 ≠ 主机 --seed；WELCOME 种子 = 权威 →
  豁免出 parity 键表）。⑦ **无输入臂跨 run savenorm EXACT 需无火
  种子**（fire 熄灭 real-dt 域，PITS 385/390 族——同种子同 run 双
  进程同序 ✓，跨 run 位差 ✗）：seed 11/22 无火 / 33/44/55 有火
  扫描定案。⑧ **--new 须 --save 卡净主机**（净会话世界恒 fresh，
  --new 无 slot 合法 = WELCOME fresh 形的 CLI 门）。⑨ **起始库存
  per-session**：netAddPlayer 空 Inventory（默认构造）= 断线移除
  零掉落静默（门臂 entities=0 抓获）；fillStarterInventory 提取
  buildScene 块共享（hotbar.json items + inventory 段）。
- **负 cell 中心 + 无蛋 ≠ 首访 + demo 死点击 + 旋转 dy 累计 +
  /tmp 路径重写五坑（PITS 409，门 217 v3.27.0 V3-10d2 召唤与复
  杀）**——① **负坐标 cell 中心 = -N + 0.5 非 -N - 0.5**（硬规
  则 1 的 floorDiv 纪律的中心侧）：10d1 的 kEndCrystalSlots/
  kEndTowerSlots 负侧中心全写成 -2.5/-50.5（= cell -3/-51 的中
  心）——晶普查走整数 cell 词表（kCrystalCells）故玩法正确，但
  箭碎扫掠 floor(中心) 在 -X/-Z 侧**静默测错格**（-X 晶箭永不
  碎）；10d2 面晶词表的单测 pin（floor == 意图 cell）首跑抓获；
  纪律 = **Vec3 中心词表入库前逐格 floor 回验**。② **无蛋 ≠ 首
  访**：10d1 生成门「无蛋 → 生成」在玩家破蛋后 60f 把龙无召唤
  复生（MC：破蛋 ≠ 复生，只有召唤复生）——门臂首跑 f381 实证；
  修 = **败亡会话旗**封闭首访门 + 召唤显式延迟（重载复位旗 =
  D6 重载读保持）。③ **demo 同帧 mouse_down+mouse_up = 死点
  击**（边沿同帧抵消——press 不落地）：点击须 ≥2 帧 hold；**hold
  续帧按新瞄准重新取目标**（创造即挖沿射线续破 reach 内下一块
  ——破晶 → 续帧自动破蛋 = 一 hold 两用，门臂据此设计）。④
  **demo 旋转 dy 逐目标累计**（mouse_move dy 改 pitch 是绝对姿
  势增量——两个同 pitch 目标之间只发 dx；门内对第二目标重发
  dy = pitch 过旋 0.16 rad 瞄准落空 1 次返工）。⑤ **门脚本的
  /tmp 路径被重写**（`mv x /tmp/x.bak` 的 bak 落源目录——/tmp
  前缀被吃 = 文件"失踪"假象 + 后续 case 串染）：备份用
  cp+rm 到显式目录（/tmp/opencode 子目录）+ 每 case 后 diff -rq
  资产完整性回验。
- **init 交叉检顺序 + 数值累加器泄漏 + demo 存档红线 + 流送依赖
  锚点 + 哨兵首帧 delta + 同帧 spawn+kill + 血条 margin + CJK
  边界八坑（PITS 408，门 216 v3.26.0 V3-10d1 末影龙）**——①
  **init 交叉检必须后置于所检子系统的 load**：dragon_breath 效果
  存在性检放在 `m_effects.loadDirectory` 之前 = 假 fail 首跑实锤
  （「breath effect missing」，效果根本未载）；wav 检走
  `m_assets.exists`（文件面）无此问题——**检对象未载 ≠ 对象不
  存在**。② **多字段共享累加器 = 缺值泄漏**：readDragonNumber
  缺键不动 out，共享 `double v` 吃上字段值（fireball 缺省吃
  breath_radius 的 2 过了 >0 配对门）——负例单测首跑抓获；纪律
  = 每读前 reset 或每字段独立局部。③ **demo 存档红线实证面**：
  replay 的 save 参考 demo 的 `--save` 被 init reset（PITS 407①
  同款机制的存档侧）= 跑完 slot 文件恒为输入副本——**demo 臂
  无终档断言**（权威 = 截图 + DEBUG 行）；canary（fresh 参考）
  不触发 reset 有退出写。④ **流送依赖的世界锚点须卸载无关**：
  raw `getBlock` 未载 chunk 读 air = 晶普查 0→6 随流送漂（假
  crystalDelta → 假 strafe + 驻坐概率漂移）+ 蛋门未载读 air →
  假复生白嫖洞；正解 = **未载 = worldgen 新鲜 = 完好**（晶不可
  在未载 chunk 破——破路径全读 chunk 态）；applyWorldEdits 走
  getOrCreateChunk 强制载编辑列 = 重载臂蛋门天然正确。⑤ **-1
  哨兵首帧 delta**：census 初值 -1 的首帧「-1→6」≠ 碎晶 = 假
  变化（strafe 触发器）——首帧禁报（`last >= 0` 门）。⑥ **同帧
  spawn+kill 漏账**：局部指针在 spawn 段前捕获 = 同帧死的龙
  不跑仪式账（蛋永不落 → f119 复生死循环首跑实证）；正解 =
  spawn/seam 后重捕获 + despawn 对**任何** dead-boss 保持（含
  ascension -1）+ 记账单收口 game pass（despawn 永不计 boss
  死）。⑦ **血条 CJK 名行 margin**：by=8 的名行 y=8−16=−8 屏外
  裁切（截图首跑实证）——margin 24。⑧ **CJK charset 解析边
  界**：`CJK_CHARSET = [...]` 尾部有意重复项 + `];` 越界匹配
  （s.find('];') 过界到别处）= raw 扫描 433 伪影（真实 430
  唯一）——插入/校验用引号正则 + 真实列表边界（`'\n]'`），
  勿按 raw >127 扫描计数。
- **demo 槽不回写 + 火脚级重叠 + 拾格带含脚下 + 遮挡首格语义
  + 下探穿固体虚空 + 截图尺寸/按住 LMB 七坑（PITS 407，门 215
  v3.25.0 V3-10c 末影人）**——① **demo 引用槽永不回写（假测量
  根）**：demo replay world=save 时 `m_args.saveSlot` 被
  reset（game_init.cpp 的引用槽红线）= 跑完读 slot 文件读到的
  **永远是输入场景副本**（yaw/pitch 恒不动的假象 → 误判
  mouse_move 无效，绕了两小时插桩）；判别 = g213 旧 demo 复跑
  （12 格不同嵌入实证 mouse_move 有效）；**demo 臂终态权威 =
  DEBUG 事件行 + 截图**，勿读 slot。② **火须放脚级（air 层）
  才有严格 AABB 重叠**：mob 脚 y53.0 站 y52 块顶——火格 y52
  与身体盒仅面接触（y=53 边界）= 严格不等式拒（inFire 永不真，
  点燃零实证）；火格 y53（脚所占地块）= 真重叠点燃。③ **拾格
  带须含脚下（dy −1）**：feet 53 的站立体下方地块 = y52；摸取
  扫描带 dy 0..2 = 纯空气层 → 恒不触发（首 probe 零 carried
  实证）；dy −1..1 含脚下才活。④ **遮挡检 = ray 首非 air 须
  == 目标格**：`hasLineOfSight` 的语义是「段内无非 air」——
  目标格自身即首非 air = 恒拒（箭撞击语义）；「mob 看得见
  块」须 `raycastBlock(eye→格心)` 首命中 == 目标格（更近命中
  = 遮挡）。⑤ **瞬移「向下首个 roomy」探针穿固体掉虚空**：
  从候选位下探到首个站得下的层 = 台地实体层之下（y45 虚空）
  → 落点 >40 距静默 despawn（门内实证：#1 f436 后消失，
  无任何日志）；正解 = 候选位本身须空旷（hasRoomAt ∧ 无
  流体）+ **16 格内 footprint 有地面**（任一 solid 支撑 =
  站立约定），过隙候选拒收；全败吞没 = 永久 DEBUG 行
  （swallowed）= 吞没可观测。⑥ **无头截图 = 679×744 渲染目
  标（非窗口 1280×720）**：aim 投影 crop 数学须读**实际图像
  尺寸**（identify）——按窗口尺寸 crop 全偏（门内 4 连踩）。
  ⑦ **按住 LMB 会挖准星所指（创造即挖）**：截图转身前须先
  松 LMB——f908 按住到 f1501 的 LMB 在 f1200 转身瞬间开始挖
  掉落物下方地板（f1203 地板格被挖、item 下坠 1 块 = 视觉
  谜团根）；timeline 纪律 = 观察段无按下键。
- **lattice 位切 int 截断 + JS 三元误写 + stale assets 再踩三坑
  （PITS 406，门 214 v3.24.0 V3-10b 末地内容族）**——① **hash 位切
  直接 int 截断 = 负值模运算**：`static_cast<int>(h >> 24) % span`
  的 `h >> 24` 最高 2^40——int 截断取低 32 位可负 → `%` 负余数 →
  参数出带（半径 -2..12 实测，单测边界断言首跑全炸）；纪律 =
  **模运算先于 int 转换**（`(h >> 24) % span` 再 cast），nether
  盐族位切同纪律普查。② **JS 三元误写进 Python 生成器**（`? :`
  语法错）——纹理生成器是 Python，函数体从 C++ 思维带过来的语法
  杂音须先 `py_compile` 再跑（md5 幂等纪律只在跑成后才起作用）。
  ③ **run 目录 stale assets 三连踩**（PITS 403② 再验）：10b 新
  biomes_end/ 未同步 g213 run 目录 → init rc 1「requires at least
  one biome」= fail-fast 按预期工作（非 bug）；每版资产面变更后
  run 目录 rm -rf + cp -r 重建 + 新目录存在性 grep 回验。
- **场景手术先探地形 + shader 无光分支双 floor + GCC-16 假阳性结构
  隔离 + 存档伪造只剥对应后键 + 场景派生同步 fluids 五坑（PITS 405，
  门 213 v3.23.0 V3-10a 末地维度框架）**——① **场景手术选位 = 先探
  地形**：fresh 世界出生点邻域是丘陵（feet 54 起爬升）——body 层环 +
  地板手术被高处地形埋没（黑屏 + RMB 全落空，假象极似「没瞄准」）；
  纪律 = **7×7 挖深坑**（目标层..70 air + 目标层−1 石地板）平顶化，
  与绝对高度无关（门内首探 → 二探实证）。② **shader 无光分支只读
  u_caveAmbient**：basic.frag `light <= 0.001 → amb = u_caveAmbient`
  ——无光源维度（10a 末地全岛 light 0）的 ambient floor 须 **u_ambient
  + u_caveAmbient 双 uniform 同设**，单设 u_ambient = 岛面仍走
  config 0.03 cave floor = 全黑截图（门内黑屏首因之二）。③ **GCC-16
  -O3 maybe-uninitialized 假阳性族**：新代码增量改变 -O3 内联决策，
  **既有** serializeSlotEntry 的 Json variant 假阳性复发（`-Wall
  -Wextra -Wpedantic -Werror` 下报错但行为正确）；`noinline` 属性
  无效，**结构隔离有效**（大段序列化抽独立静态函数 + 测试 TU 的 cell
  构造抽助手）；判别纪律 = 先验 HEAD 树同编译器干净（真回归 vs 假阳
  性），再动结构。④ **存档伪造真形 = 只剥对应后键**：format-30 伪造
  若用 pre-30 strip 助手（剥 nether/dimension = 30 必携键）= 缺键假
  失败；正确 = 只剥 31/32 键（creative/end/end_entities）保留 30 键；
  pre-32 strip 助手（5 文件本地副本）须随 format bump 扩新键。⑤ **场
  景派生须同步 fluids**：从带 fluids 编辑的 base 派生新场景清 blocks
  不清 fluids → init rc 1「cell occupied」（fluid 落生成固体格）——
  派生时 blocks/fluids/fires 三节同清。
- **git checkout 还原回退未提交功能 + 骑乘场景视线三件 + demo 瞄准
  换算 + Space 双击窗 + 分离 opt-out 五坑（PITS 404，门 212
  v3.22.0 V3-14d strider 骑乘）**——① **`git checkout --` 还原
  temp 改的数据文件 = 未提交的功能加法一并回退**：S6 点名臂
  （temp 改坏 → 跑 → `git checkout --` 还原）把**同文件里未提交的
  功能键**也回退了（v3.21 piglin.json 的 barter、v3.22 strider.json
  的 rideable 两连踩）——后续探跑静默用旧数据（谓词假 = 瞄准全对
  也不触发，假象极似「没打中」）。纪律 = temp 改动用 python 原地
  改 + **手工字符串还原**（非 git）+ 还原后 **grep 回验功能字段
  仍在**。② **骑乘场景视线三件 = 贯通沟 + 桥 + 灯**：1×1/浅沟的
  rim 遮挡（射线降过台面 y 在沟口外 0.09-1.1m）→ 贯通沟（目标列 +
  进深列全挖到流体顶，射线沿线每列空气）+ **桥格**（流体顶 +1 的
  固体顶面 = 玩家起点/骑行通道，免 burn 且同层可行走）+ **灯排**
  （glowstone 高位照亮——18 格上 = 光 0 不可见，6 格上 = 光 9；
  **灯位须对 spawn 光门验算**：strider 15 门，脚格光 ≤15 不变）。
  ③ **demo 瞄准换算**：pitch/yaw 按**体心**（feet + height/2）非
  feet（feet 距 5.54 > kReach 5.0 但体心 5.0 恰界 = MISS 假象的
  反面）；delta px = Δrad / mouseSensitivity **0.0025**（180° =
  1257px 单帧——mouseDelta 无钳制）；LMB 捕获点击**先打空**（下方
  桥面/流体，勿打坐骑 = 1 HP + 击退污染）。④ **Space 落乘须绕过
  双击飞行窗记录**：落乘 tap 若记 m_lastSpacePressTime = 落乘后速
  按误开飞行——落乘分支在记录之前 return。⑤ **分离 pass 顶坐骑**：
  骑手盒（背位起 1.8）与坐骑盒（1.5）设计性重叠 → separateEntity-
  List 逐帧把坐骑从骑手脚下推出；`collidesWithEntities()` 虚门
  （弹道 opt-out 先例）首消费于 mob = `!m_ridden`。
- **demo 观察红线 + run 目录 stale 资产 + kReach 瞄准 + 生成环
  视线 + ambient 耗流五坑（PITS 403，门 211 v3.21.0 V3-14c piglin
  交易）**——① **demo 引用存档槽 = 永不回写**（game_init
  saveSlot.reset 观察红线）：CLI「drop --save or match it」的
  match = 红线不写而非写档——交易臂终态权威 = 终帧截图 ×2 RAW +
  DEBUG 动作行（`bartered with 'piglin' (...)` 逐字恒等 = 帧锁定
  roll 实证），勿等「World saved」。② **run 目录 stale assets**：
  池变体/副本树每次仓内资产改动后必须**整体重拷**（rm -rf + cp -r
  + 重打池补丁）——两连探跑静默用旧 piglin.json（targetMob HIT +
  无 barter + gold 未动 = 数据缺而非瞄准错，假象极似「没打中」）；
  判别 = 拷后 grep 目标数据字段存在性。③ **kReach 边界瞄准**：
  眼-mob feet 距 5.54 > kReach 5.0 但眼-体心 5.0 恰界内——瞄准
  验算按**体心**（feet + height/2）非 feet；校准工具 = 临时插桩
  HIT/MISS + 近 mob 位打印（PITS 383③ 定界后移除重建纪律，门内
  实证 3 轮：MISS(aim 偏 0.47x) → HIT 无 barter(stale 资产) →
  HIT + barter(数据齐)）。④ **生成环视线几何**：scan 序 = dz 降
  dx 升（ring ceil(maxRadius) 圈）——钉目标 cell 须拒**全部
  scan-序先行格**（本门 12 拒生沟：dz=-6 行 1 + dz=-5 行 7 +
  dz=-4 行 4 = 先行有效格全灭，目标 (dx=0,dz=-4) 首中）；1×1
  洞**不可见**（2 深挖洞的 rim 遮挡：射线降过台地顶 y53 的 z 点
  = 洞口远端外 0.09-1.1m）→ **1×3 视线沟**（超环 cell【dist 3 <
  min 4】自由挖，不扰生成）。⑤ **ambient 每帧耗流**：ground
  ambient 每帧 roll uniform01（流态 = 点击帧的函数）——xdotool
  ±帧抖动破 roll 恒等（两次 run 掷出不同物 = savenorm 必裂）；
  **断言 roll 结果的输入臂必用 authored demo**（帧锁定，roll 逐字
  恒等），xdotool 只用于结果不变量型断言（「某物入包」级）。
- **下界决定性场景 + 字体码点笔误 + canary --frames 五坑（PITS 402，
  门 210 v3.20.0 V3-14b ghast）**——① **下界生成决定性构造三件**：
  **环清三带**（kSpawnFloorDelta 2 → 地面 pick 的 floor top 须 ∈
  [player±2]；只清一层沟底 top 仍在带内 = 地面 mob 饱和 cap 吞 ghast
  抽选【scene v1 实证：4 只地面 mob 后 cap reached】；清 y50-52 三
  带 = 沟底 top 50 出带 = 地面 pick 全败 cap 永不饱和）+ **obsidian
  顶盖**（in-air 生成 = 柱内顶向下扫描首个 airCells 全空气格；无
  顶盖 ghast 生 y121 天花板下【scene v2】，顶盖 y58 起 feet 56 =
  kDetectVertical 2.0 眼差 3.0 不可见不发射【scene v3】，y57 起 feet
  55 = 恰线 dy 2.0 可见 = 定案）+ **场景手术必需字段**（player.
  effects format 8+ / absorption format 12+，pop 掉 = init 点名拒载
  ——两次踩坑）。② **mob 不入档 = 「同位重生」假说陷阱**：format 30
  nether_entities 只存 items（设计项，deviation 6）——两次「同 cell
  ghast spawn」= 新抽选落同确定性 cell（前一只漂移出 kSpawnClearance
  1.0），非重生；**死亡反证法** = 死 mob 必留 XP orb（xp>0 恒发，
  120s 寿命）+ 掉落（0-1 roll 可 0）——终档 orb 缺席 = 未死。③
  **爆点 = 火球盒底位**（arrow->position()，非发射眼非盒中心）：正/
  负对照格的判定距离按盒底算（玩家接触爆点在玩家盒边 → 头顶 1.2-3.0
  的 netherrack 架被破、br 3 obsidian 平台/顶盖恰界存活；顶盖卡壳
  爆点在盒底 → 距发射 ghast 中心 1.91 = 5 自伤【MC 免疫自身火球 =
  记档偏离】）。④ **字体一致性 pin 抓不到码点语义错**：V3-14a 加的
  「第二爆炸字」= 爽 U+723D（本意 炸 U+70B8）——charset/表/PNG/
  单测四处一致地错（430 排序连续 + FNV 全自洽），首渲染（死因屏
  crop）才显「爆?杀死」。**加 CJK 字形必目检新字 glyph + 首渲染
  E2E crop**（生成器 emit snippet 后 grep 目标字 U+XXXX 回验码点）。
  修复面 = charset 换入 + 九 glyph 重格 +1（0x70B9..0x7206）+ top-
  512 红线 131072 px 恒等 + **S1 锚字节恒等**（锚场景无消费方文本 =
  免重锚）。⑤ **canary headless 必 --frames > demo total**：--demo
  无 --frames = 播完进 finished panel 观察循环等观众 ESC/Q，
  headless 永等（10 分钟无输出假 hang）；timeout 杀进程留的 mid-run
  autosave 双跑「EXACT」= 假阳性证据陷阱（同模拟时刻 auto-save 恒
  等 ≠ 终态）——demo total = Σdts×60 帧（example 266 / dig 561 /
  build 571 / fly 541），--frames 700 通吃。
- **爆炸门场景三纪律（PITS 401，门 209 v3.19.0 V3-14a 爆炸机制）**
  ——① **森林生物群系 = 到处可燃**：blast 必点火 → real-dt 火寿命域
  （PITS 385：RAW 不适用，savenorm 权威）——**密封石室场景**（房间
  全 stone 编辑填充 blast 全域 + 玩家空气口袋 + 场景手术去火
  【PITS 398①：移除 fire 日志条目，漏移 = init 点名"fire block has
  no fires entry"】）= 无火确定性域。② **blast 破回生成态 = 日志条
  目消失**（blocks journal = 与生成世界的 DIFF，非事件日志）：断言
  破块用 `blocks.get(cell) == None` + **界外对照格**（恰好 radius 处
  local 强度 0 = 存活 pin，(3,56,0) 先例）+ 房内石墙残留对照，勿断
  "air 编辑存在"。③ **地下生成火簇回落**：场景去火后载档世界生成
  重放 3 团火（(23,35,28) 簇 + 1 蔓延）= 存档 fires[] 恒 4 条
  real-dt——断言域 = savenorm（剥 fires[*].remaining）+ 截图 RAW
  （火在 y35 地下不可见）。
- **demo 输入四坑 + X grab 屏捕 + 门机窗口口径（PITS 400，门 208
  v3.18.0 V3-11b 创造物品栏）**——① **demo 无初始指针锁定**：游戏
  启动指针未锁（首次 LMB = 捕获帧，game_interaction 的 capture 路径），
  一切 locked-pointer 门控键（G/E/C/挖掘）在 demo 里必须先有一击
  捕获 LMB 才生效——门 207 的 S2 "G 切换"因此从未在 demo 里生效
  （其"即挖"实为 f21 捕获次帧，"结果存档"实为场景副本，F5 被
  save-spec 红线禁用 = 见②，记录已修正，补证 = 门 208 S2c）。②
  **demo save-spec = 观察红线禁 F5**：replay 的 world = save slot 时
  `m_args.saveSlot.reset()`（该槽永不回写）→ F5 handler 同判空报
  "no save slot"（WARN 不显 = 默认 info 级漏看，PITS 392 族）；
  **结果捕获只存在于 fresh spec + --save**（退出写 = 回放结果的唯一
  落盘点）。③ **xdotool 数字键双投递**：`xdotool key 2` = KEYDOWN
  （热栏选择）+ TEXT_INPUT（'2' 字符）——搜索聚焦态下数字入搜索串
  （"app2" 无匹配 = 授予全灭假象）；定案 = 物品栏 tab 开时数字 =
  文本不切热栏（MC 搜索焦点语义），槽位须在开面板前选。④ **X
  指针抓握阻塞外来 X 客户端**：游戏捕获指针期间 `import -window`
  等屏捕恒挂 EAGAIN（X grab 纪律）——面板态视觉证据 = **游戏自身
  退出截图**（退出时面板仍开 = 帧定格捕获）；屏捕只许在指针自由
  窗口。⑤ **门机实际窗口 = 679×744**（config 请求 1280×720 被 WM
  约束不生效）——布局探针/点击坐标必须按**截图实测尺寸**算 UI
  空间（÷ui.scale），且臂必须 `--config` committed 口径（用户
  ui.scale 2.0 = PITS 394 族，漏 --config 坐标全错）。
- **format bump 测试面 4 类降级点 + demo save-spec 结果捕获（PITS 399，
  门 207 v3.17.0 V3-11a 创造基础）**——① **存档 format 跳变 = 测试
  面全家**（首跑全套 61 失败全此族，非实现 bug）：4 类降级点须同
  commit 盘点——(a) **strip 助手本地复制 ×5 文件**（test_save/bed/
  campfire/drown/enchant_save 各持一份 `stripFormat30Keys` = 升
  `stripFormat31Keys` + 新键 strip）；(b) **手动 withoutKey 链**
  （SaveFormat8Test 的 player rebuild 链加 creative 一环）；(c)
  **inline rebuild**（test_durability player30 块 `k != "dimension"`
  加 `&& k != "creative"`）；(d) **gzip identity 测试**（29→30 rebase
  29→31：模拟旧档 strip creative + format 断言 + 往返断言）。盘点
  口诀 = grep tests/ 的 strip 助手 / `"format\": 30` 断言 /
  `asInt(), 30` 断言 / withoutKey player 链。② **demo save-spec 回放的结果捕获**：world = save slot spec 时
  `--save` 须匹配槽（rc 2 点名）且退出**永不回写槽**（观察红线）——
  门 208 实证红线同时禁用了 F5（saveSlot.reset = F5 判空 WARN，
  门 207 的"F5 结果捕获"记录无效 = PITS 400①②）；**结果捕获 =
  fresh world spec + `--save` 的退出写**（唯一落盘点）。fresh world
  spec（4 canary）无此问题（常规退出写档）。
- **场景手术三坑 + 动画内容的门协议（PITS 398，门 206 v3.16.0
  V3-15 小批）**——① **新种子 123456789 的 fresh 档自带 3 团地下火**
  （(23,35,28)/(24,35,28)/(26,35,27) y35 视角外，worldgen 产物，全
  config 同现）：场景存档继承火 = fire 家族 real-dt（PITS 385）破 ×2
  RAW 域（fires 3↔4 蔓延漂移实证）——**场景手术先去火**：移除
  `blocks` 里的 fire 条目（单元格回落生成值）+ `fires: []`；**勿加
  air 覆盖**（同格双日志 = "duplicate block edits" 拒载）。② **存档
  手术格式**：流体日志 = `[x,y,z,level,name]`（**level 在 name 前**，
  熔岩满级 = 7）进 `fluids` 数组——块数组写流体名 = 静默错位；
  `player.armor` = **命名 dict**（head/chest/legs/feet）非数组
  （写索引键 = "not a known slot" 拒载）；池区地形表面 = 53（种子
  实况）——流体格落 solid = "cell occupied" 拒载（PITS 390 续），须
  先挖 air 再填流体 + 石环保险。③ **动画内容的门协议**：退出渲染帧
  数 off-by-one 抖动（PITS 391② 族）× 高频纹理（portal 漩涡）= 截图
  ×2 RAW 差 ~1300 px 全在动画 tile 区（相位差 1/120 tile）——**动画
  内容（UV 滚动/粒子）截图 ×2 恒等不是有效断言**；权威三件套 = 存档
  ×2 RAW（模拟确定性）+ **动画关控制臂 ×2 RAW**（管线确定性，
  `portal.anim/steam: false` 同场景复跑恒等实证）+ 单跑动画目检 /
  双帧 diff > 0（f400 vs f460 = 3560 px）。另：`MC_LOG_*` 宏 = 单参
  消息串（无 printf 格式）——插桩日志用 `+ std::to_string()` 拼接。
- **fScale 不含 ui.scale + 字体加字工作流（PITS 397，门 205 v3.15.0
  死亡统计屏）**——① **`fScale(s) = s × m_fontScale`（仅 ui.font_scale，
  不含 ui.scale）**：makeTextVertices 的字形物理尺寸 = 16px × ui.scale
  × fScale 参数，但**行距/坐标是调用方给的物理 px**——按「fScale(1.0)
  = 一行物理高度」算布局 = 低估 1 个 ui.scale 倍（死亡屏 8 行首版全叠
  在一行，门内 ×2 RAW 恒等 + 裁剪目检捕获）；文本行布局用**固定物理
  偏移**（demo 面板先例 h/2±N，行距 ≥ 26px 覆盖 ui.scale 2 的 32px
  字形 cap 高）。② **加 CJK 字工作流**：字形表查缺（418→428 字表在
  `tools/generate_font.py` CJK_CHARSET，代码点排序）→ charset +1 +
  CHARSET_COUNT +1 → 重跑生成器（TTF pin + legacy 33 位级自检 +
  top-512 红线 + 扩展区**重排格**：新字之后全部 +1 格）→ snippet 手贴
  `src/ui/font.hpp` kCjkGlyphs（tools 永不碰 src/）→ **test_font 4 锚
  重钉**（PngBytes FNV / TablePinned 尺寸+成员格 / ExtendedCellsPinned
  全向量 / UvCellPositions）——ExtendedCellsPinned 的 vector **末元素
  无尾逗号**（`{0x732A, 384}};`）= 正则重钉必漏，单独查。③ 扩展区重
  排格**零像素面**论证 = 无输入渲染无 CJK 文本（教程 = locked pointer
  才显）→ S1 锚恒等（门 205 实证）。
- **demo sim 镜像常量漏 bump + GameKey 尾钉 kKeyCount（PITS 396，
  门 204 v3.14.0 V3-8b 手柄映射）**——① **`tools/make_example_demo.py`
  的 `SIM_VERSION` = kDemoSimVersion 的 Python 镜像**：C++ bump
  （1→2）后忘记同步 Python 常量 → 重新 author 的 example.dmo 静默
  写成 sim 1（recorded_with 却是新版 = 自相矛盾的资产）——门 S2 首跑
  即被 sim 门点名拒收**捕获**（正是 sim 门的设计目的 = 金丝雀先于
  用户发现）；教训 = **bump kDemoSimVersion 时 grep 全仓镜像**（C++
  常量 + Python 工具 + 文档计数）。② **`kKeyCount = GameKey::T + 1`
  = 尾成员钉**：GameKey 加成员（FlyToggle）须同步 kKeyCount——漏更
  新 = 新键索引越出 `std::array<..., kKeyCount>` 边界（-Werror 零
  报警，静默 OOB）；enum 尾部「Keep in sync with kKeyCount」注释是
  唯一守卫，加成员先 grep kKeyCount。
- **S1 锚假失败 = 用户 config 调值 + 时钟映射误读（PITS 394，门 203
  v3.13.0 demo sim 版本）**——base19 锚 = **committed config** 捕获
  （ui.scale 1.5）；用户其后把工作树 config.json 调成 ui.scale 2.0
  （规则：永不提交/还原）→ 无输入截图时钟/XP 文本 4/3× 放大 + 位移 →
  S1 ≠ 锚**假失败**（差 3846 px 全在 UI 文本带：右上时钟 + 底部 XP
  「0」两带，世界/HUD 图标零 diff）。判别三件套：① 像素 diff 仅 UI
  文本带 ② glyph 尺寸比 = ui.scale 比精确（152/114 = 2.0/1.5）③
  `git show HEAD:config/config.json` vs 工作树 diff = 恰 ui.scale 单键
  → **S1 锚比较失败先 diff config**（committed vs 工作树），有 UI 几何
  调值 = 用 committed config 副本 + `--config` 重跑 S1（锚语义 =
  committed config 态）。伴生误读教训：`clockString` = dawn 对齐映射
  （hour24 = (t−kDawnStart 0.05)×24+6）——start_time 0.25 的表读
  **10:48 非直觉的 6:00**（day-plateau 天空 ≠ 正午表读；noon = t
  0.30），时间断言先套公式推（`day_cycle.hpp` 默认注释「// noon」已
  随本版更正）。
- **demo 编写（--demo-author）几何三坑（PITS 393，11-06 预设教程
  demo）**——① **初始俯仰 = −0.25 rad 非 0**（spawn 默认 pitch）：
  瞄准像素换算从 0 推 = 放置/挖掘全偏（实证：dy 158 以为 −0.395 实为
  −0.645，4 个放置点 3 个偏 1 格 + 1 个侧脸命中）；校准 = **单点微
  demo 实证**（锁指针 + 单发 mouse_move + RMB 放置 → 读存档实际落
  块，反推有效像素窗——环 1 格地面放置窗 dy ∈ [330,508]@sens 0.0025
  不含偏移 / 含偏移 [145,315]）。② **按住 Space = 第三次 tap**：
  双击窗 0.3 s（18 帧）内的任何 space key_down 都参与 flyToggle
  判定——双击起飞后 <18 帧内再按（升空 hold）= 立刻切回飞行关
  （实证：f130/f142 起飞 + f155 hold = 13 帧飞行后走路；修法 = hold
  起始 ≥ 第二 tap + 20 帧）。③ **demo 版本指纹 = TM_VERSION**：
  升版后旧 .dmo 拒播（「recorded with … incompatible」点名，设计
  纪律）——**提交仓的 .dmo 须随版重新 author**（--demo-author 逐字节
  可复现 = md5 恒等实证，时间线 JSON 即人类可读源）。另：探针算
  角度的 double-normalize bug（归一化后向量再除 len = 角度系统性
  偏浅）——**角度输出前核一遍归一化**，或直接用 raycast 命中格
  反推（probe4 范式：扫 dy 窗报命中，不手算）。
- **门日志 grep 假阴性（PITS 392，门 202 阴影缩尺报障）**——
  `mob spawn f...` / 多数子系统事件行 = **MC_LOG_DEBUG**：info 级
  门跑里 grep 事件计数恒 0 ≠ 事件未发生（首跑 "0 spawns" 误判，
  crop 目检实证 3 只羊在场）——事件计数断言的门跑加
  `--log-level debug`，或截图 crop 目检互补（日志计数与像素证据
  二选一须在场，勿以 info 级零计数当"未发生"证据）。
- **demo 门脚本六坑（PITS 391，门 201 V3-9 demo 回放）**——①
  **UI 批绘制序**：renderUi 的 addText/addQuad 经 batchFor 追加进
  `batches`，但主批冲刷循环在调用点之前已遍历完毕 = 顶点永不被
  drawUiBatch 消费（demo 角标/面板/暗帷全不可见；冒烟"面板像素"
  声称与门 E2E 矛盾 = claims 非 evidence 纪律实证）——修复 =
  自包含独立批（局部 vector 块内自冲刷）或移到冲刷循环之前。
  ② **截图 = swap 后读 back buffer = 落后一次渲染帧**（既有全部门
  语义，跑间恒等从未暴露）——headless 放完态退出（--frames 超 demo
  长度）的退出帧内容（帷/面板）需 still 帧**双渲染**才可被截图
  捕获（交互运行零影响；中段截图 = 第 N-1 帧内容）。③ **录制带
  --save = 闭环起点漂移**：槽会话 demo world spec = save 引用，而
  槽在录制退出时被终态覆写——录制→回放闭环须 **fresh 会话**（无
  --save，spec = fresh seed）。④ **author 输出名 = 输入基名 +
  .dmo**（timeline.json → timeline.dmo，非 --demo-record 的
  显式/时间戳名）。⑤ **g139 插桩标记锚 gameplay 循环**：V3-9 把
  run() 拆 runGameplayLoop + runDemoReplay，回放循环无标记 = demo
  自成本 PERF-WIN 零输出——perf 树手工镜像 3 测量点（scratch 不入
  库，PITS 387 族延伸；改循环结构前先 grep g139 锚）。⑥ **暂停态
  + --frames = 设计不退出**：headless 退出仅 `m_demoFrame ≥ total`
  触发——门脚本暂停探针（ESC 后不再恢复）会挂到 timeout；暂停帷
  像素证据 = 结束面板共路径覆盖（m_demoPaused 与 endPanel 共用
  demoQuad/demoText 块）。
- **场景手术几何三坑（PITS 390，门 199 V3-5 建筑内容）**——①
  **场景室坐标须查参考地形**：demo 区表面 y=53（seed 123456789
  发行值）——首跑水室放 y=50/51 = 参考石内，流体编辑 load 拒
  （save.cpp "cannot apply save fluid edit ... (cell occupied or
  level unchanged)"，窗口即毁 = 输入臂 BadWindow 假象）——场景室放
  y≥55 参考 air 区（块编辑覆盖任何参考均可；**流体编辑拒 solid
  参考**）。② **开放水源必漫溢**：源水（level 8）每 tick 扩同层邻
  air——无围堰"池"32 格 → 300f 后 402 格流体编辑（×2 恒等但断言
  计数全错 + 视觉淹没场景）——密封池 = 满底 + 顶周围墙环
  （24+24 格实证 32 恒定 60f+）。③ **发货世界自带确定性 fluid-
  settle 编辑**：程序湖边缘非源水（level 6-7）在 base 30f 跑中结算
  入档（seed 123456789 实证 x25-30, y34, z27-28 共 9 格，距 spawn
  ~30 块）——场景终档断言须**自格 scoped**（池格坐标精确匹配），非
  水格全量计数。
- **门脚本生成臂五坑（PITS 389，门 198 V3-3 下界 mob）**——① **门
  QUIET 配置 mobs.enabled = false 是静默纪律**：锚/无输入基线跑用
  g179_quiet.json（mob 生成全禁 = 基线干净），**生成臂须 mobs-on 变
  体配置**（g198_mobs.json = quiet + mobs.enabled true）——首跑零
  spawn 且无尝试日志 = 查配置先于查代码（插桩实证 enabled=0）。②
  **sed 派生门脚本的 OUT 目录要换**：g198_s1 首跑 OUT 仍指 s197/
  = PNG 落别处，`cmp -s 缺失文件` 静默假 DIFF（"S1 ANCHOR DIFF" =
  文件不存在）——cmp 前先 `ls` 验文件。③ **spawn 行坐标正则须含负
  号**：`at (x, y, z)` 的 z/x 可负（`[0-9.-]*`），`[0-9.]*` 漏全部
  负坐标行 = "NO SPAWNS" 假红（实际 4 只已生成）；另 "Registered
  mob" init 行 ≠ spawn 行（池隔离断言须锚 "mob spawn f"）。④
  **maxMobs cap 吞后续尝试**：4 只存活后每次尝试 "cap reached"（mob
  不跑 40 块不 despawn）——低权重 mob（strider 1/31）只在首批 ~4
  次抽选中有机会（8×600f 零命中 = 0.31 幂次运气内）；**决定性
  构造 = 环全清**（4704 air 编辑清掉生成环台地，留玩家立柱）→
  陆地 pick 全 "no valid cell" → cap 永不饱和 → 每次尝试新抽
  （1200f 场景 P≈71%，3 场景 99.9%）。⑤ **传送落点 = 生成压制
  位**：portal 光 11 场 + 下界原点 12 保护圈（hostile）= 落点周边
  环全拒（尝试全 "no valid cell"，机制正确非 bug）——下界生成臂用
  远离落点的高原场景（(13.5, 53, 0.5) 级）。air 编辑合法
  （落盘档 blocks 段 'air' 名 = 显式清空，实证 63-4704 格保留）。
- **输入门脚本四坑（PITS 388，门 197 Part 2 下界深化）**——①
  **timeout 包装使 `$!` = timeout 的 pid ≠ 游戏 pid**：`xdotool
  search --pid` 按 _NET_WM_PID 匹配 = 全败（FOCUS-FAIL / 窗搜空）——
  输入臂的游戏**直跑**（`--frames` 自限，无需 timeout 守护），
  `tp=$!` 才是游戏 pid。② **短 hold 的按键注册抖动**：1.0 s W-hold
  的灵魂沙步行比值实测 0.823（非 0.4）——排查终档坐标/平台几何全对
  后 = 输入时序伪影（XTEST keydown 相对窗口就绪的注册延迟 ±0.5 s
  量级）；3.0 s hold 稀释后比值 0.403 = 物理真值（单测 0.4±0.005
  同款）。**比值类输入断言 hold ≥3 s**（抖动/hold ≤ ~15%）。③ **水
  放置守卫拒玩家重叠格**：yaw 0 射线 x 恒等 = 命中格 x 恒 = 玩家格
  x → 顶面落位 = 玩家自身 feet 格 = 放置被拒（桶保留假"无反应"）——
  aim 几何须落**非玩家格**（双行平台 + pitch −1.0 让落位在前方第二
  行，验算 z = 0.5 − 1.62/tan(pitch) 落格 + 玩家 AABB 不重叠）。
  下界蒸发臂无此坑（T1 谓词在目标解析前 = aim 无关）。④ **性能 A/B
  不得与流送/输入负载臂并行**：同机并行 1800f 下界流送跑使 300f 窗
  upd med 0.444→0.806（+80%）——changesApplied/savenorm 不受影响但
  时序窗失真，独占复跑才入档。
