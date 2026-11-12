# AGENTS.md — 代理快速接收

> Phase 30 文档整理产物。本文件是代理接手本项目的**第一入口**：读完本页即可开工，
> 细节按「文档地图」按需加载。所有规则原文见 `docs/PROTOCOL.md`。

## 项目一句话

**TokenMine.cpp**——本地 LLM 编写的 C++ 版 Minecraft：可运行、可扩展的 3D 体素沙盒，
C++20 / SDL3 / OpenGL 3.3 core，
自研 `Result`/JSON/PNG，**内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，
不改核心代码），**确定性是一级要求**（无输入运行逐帧字节可复现）。

## 现状（3.x「世界扩展」期——v3.1.0 = V3-7 存档 gzip（门 191，11-04）；
v3.2.0 = V3-1 Phase A 下界门框（门 192，11-05）；v3.3.0 = V3-8 手柄
支持（门 193，11-05）；v3.4.0 = V3-1 Phase B 下界世界 + 真传送（门
194，11-05，format 30）；**v3.5.0 = V3-2/Phase C Part 1 下界内容族（门 195，11-05：5 群系 + 12
块 + 3 矿脉 + 5 装饰 + D9 空腔带修订 = 下界可航行）；**v3.6.0 = V3-4
netherite 材料链（门 196，11-05：debris→scrap→ingot→工具 2031/护甲
×1.5 刻度，零核心代码纯数据）；v3.7.0 = V3-2 Part 2 下界深化（门
197，11-05：水蒸发/音效族 11 件/magma 光 3/ambience 第 4 上下文/soul
sand walk_speed_factor 0.4 数据驱动减速，全 nether-scoped overworld
位级不变）；**v3.8.0 = V3-3 下界 mob（门 198，11-05：5 种 + neutral/
walks_on_lava 旗 + 下界生成池零新基建——Phase B 预留槽兑现）；
v3.9.0 = V3-5 建筑内容族（门 199，11-05：黏土链/石砖/苔石/15 色
陶瓦染色族/水底 sediment 生成 pass，核心仅 worldgen 4 文件）；
v3.10.0 = V3-6 世界边缘雾（门 200，11-05：填充感知 fog end，
**base19 重锚首重锚**，稳态位级恒等）；**v3.11.0 = V3-9 demo
回放（门 201，11-05：注入内核 + 编写/录制/回放三入口 + 观众只观察
双 Input 路由 = 矩阵收官）；v3.12.0 = 实机报障阴影缩尺（门
202，11-06：kMobShadowSideScale 1.5→1.0，物理 AABB 质心探针定案
不动——MC 同款）；**v3.13.0 = demo 独立 sim 版本（门 203，11-06：
仅模拟语义变更才 bump + legacy 版本指纹零迁移直放 + --demo-force +
4 demo canary）；**v3.14.0 = V3-8b 手柄习惯映射定案（门 204，11-06：
L3 蹲伏 + RT 疾跑 + B 飞行专用钮【用户三项定案，config 自定义不做】+
SDL 三大布局归一化单表 + 首个实际 sim bump 1→2）；**v3.15.0 = V3-13 死亡统计屏（门 205，11-06：致死原因行 9 措辞 + 会话统计 5 行 + 字体 +1 字形「于」428）；**v3.16.0 = V3-15 生存/表现小批（门 206，11-06：netherite 火·熔岩免疫【fire_resistant armor-only + 三抑制 + 熔岩死因覆写】+ portal 2 周期纹理 UV 滚动 + 蒸汽粒子 8×8×9 盒）；**v3.17.0 = V3-11a 创造基础（门 207，11-06：G 切换 + 存档 flag format 31 player.creative 恒写 + 全源伤害门 + 饥饿静态满 + 即挖 + 无掉落/无耐久 + 放置不消耗 + 入场飞行；sim 不 bump canary 4 demo EXACT）；**v3.18.0 = V3-11b 创造物品栏（门 208，11-06：E 面板双 tab 物品栏/背包 + 前缀搜索【SDL 文本输入首个打字面，仅创造面板期开】+ 6×4 网格滚轮翻页 + 左键满组/右键单个授予选中热栏位 + 搜索焦点数字语义 + demo author x/y 绝对位与 g 键）= 矩阵 ⬜ 项小→大推进 4/6，V3-11 创造模式全项完成；**v3.19.0 = V3-14a 爆炸机制（门 209，11-06：纯核心 scanExplosion/explosionDamage【局域强度 radius−dist 破块 + 线性衰减整读伤害 + flamable 点火】+ 块属性 blast_resistance【obsidian 3 数据存活】+ Game::explodeAt 接线 + --explode CLI seam【帧 0，--weather 孪生】+ 死因「死于爆炸」+ 字体 +2 爆/炸 430 + boom 音 64；零 format/sim 不 bump/无击退记档）= V3-14 相位 A/4**）；**v3.20.0 = V3-14b ghast（门 210，11-06：flyShoot 悬停射击分支【flying∧shootRange>0 数据门，bat cruise 位级红线】+ explode_radius/shoot_sound 数据属性【ammo 规则豁免爆炸射手 + init wav 交叉检】+ 火球 = Projectile 复用三爆炸接线【stuck 即爆/玩家接触爆/mob 扫描跳过】+ 发光立方渲染 + 内容族【声音 2 件 64→66/池 +2 总 33/atlas 166/膜物品】；**门内捕获 = V3-14a 字体码点笔误修复**【爽 U+723D→炸 U+70B8，九 glyph 重格 +1，top-512 红线 + S1 ≡ base19 字节恒等】；零 format/sim 不 bump canary 4 EXACT）= V3-14 相位 B/4**）；**v3.21.0 = V3-14c piglin 交易（门 211，11-06：barter_input 单输入 gold_ingot + barter_outputs 12 项加权表【MC 比例项目近似，netherite_scrap 1 头奖，数量 [1,64]】+ checkBarter 纯函数 init 交叉检 + tryBarter（tryMilk 范式）+ barterItem() 无状态 roll + **不激怒 = 结构性质**【零 damage 路径】+ barter.wav 67；零 format/sim 不 bump canary 4 EXACT）= V3-14 相位 C/4**）；**v3.22.0 = V3-14d strider 骑乘（门 212，11-06：rideable strict bool + Player m_carried 承载旁路【wish 锁 + move/stepUp 旁路 + integrate no-op + fall 复锚 = 落乘 1.05 落差永不坠伤】+ m_ridden + riderSteer【WASD 直控 + kRideDecel 0.9 漂停 + ambient 流契约 + collidesWithEntities opt-out = 分离 pass 不顶坐骑】+ ride pass 单收口【落乘 6 条件 + 承载 snap 背位 1.05 + steer 推送】+ tryRide（tryMilk 范式）+ Space/LeftControl 落乘【绕双击飞行窗】+ **承载 = 会话态零 format**【mob 不入档 = 唯一自洽解】+ 渲染零改动；MC 偏离 6 记档【无 saddle/WASD 直控/无水伤下沉/原地落乘/无脚步音/站姿背位】；零 format/sim 不 bump canary 4 EXACT）= V3-14 相位 D/4 全 4 相位收官**）；**v3.23.0 = V3-10a 末地维度框架 + 真传送（门 213，11-07：项目第三维度 = 下界维度基建第二次消费【end_gen 纯函数 + EndDim 12 成员镜像 + 33 分派点三值化】+ 入口 A-lite【双 stateless frame 块 + 数据谓词 end_frame/end_frame_eyed/end_portal + 5×5 水平环纯函数族 + ender_eye 物品 + 嵌眼交互 12 全 eyed 激活】+ 最小末地【120×120 台地 + 原点出口门常活 + 虚空 + 入场平台 5×5 obsidian 每次写入，偏离 MC (100,49,0)】+ 传送新函数【1:1 无 8:1 / 返程出生点床校验 / 60f 倒计时 + 900f 冷却复用 / travelPortal 原文零触碰】+ 渲染 6 点【暗紫黑 clear + 雾 8..48 + inOverworld 门族 + F3 +end + ambience 第 5 上下文 + **u_ambient/u_caveAmbient 双 floor 0.25 按维度**】+ **format 32**【dimension 词表按 format 分词 + end/end_entities 段恒写 + PITS 399 四类降级点同 commit 盘点】；块 127→131 / atlas 166→170 零移位 / item 98→99 / 声音 67→68；门内捕获 ×2【shader 无光分支只读 u_caveAmbient = 单设 u_ambient 全黑 + 场景手术丘陵埋没 = 挖深坑】；sim 不 bump canary 4 EXACT）= V3-10 相位 A/4**）；**v3.24.0 = V3-10b 末地内容族（门 214，11-07：基岩洋全维 y[0,4]【下界地板镜像，跌落死因 Void→Fall + 珍珠逃生面】+ 岛群 pass【lattice 16 p0.15 独立盐，r 5..12 顶 44..56，顶 2 层 + 下锥 2 层悬浮岛 + air-only 谓词 + 中央足迹护栏 + 跨 chunk 外扩 rMax】+ chorus 岛顶装饰【独立盐 p0.6，cross stateless + chance_drops ≈ MC，生长不做记偏离】+ biomes_end 单群系池【10d 缝】+ end_stone_bricks【2×2 配方 count 4 MC 精确】+ **末影珍珠全链 = 项目首个投掷物**【throws_teleport strict bool + 双音配对 + one-right-click 8 面互斥 + material-only + init 交叉检；挂点潜行旋转后 placeBlock 前；投掷者速度并入 + 30 b/s MC 绝对值 + 冷却 60f；Projectile 尾参 teleports + stuck 复用 → pearlLanding：catchUpDisk + 2.5 球面均匀偏移 counter 流 + hasRoomAt 上探 3 + fall 复锚消坠伤 + 落乘 + probe 全败吞没 + 无自伤【MC 5HP 偏离】+ 0.625× 青色发光立方渲染 + 2 wav 68→70】；format 32/sim 2 不变 canary 4 EXACT；门内捕获 = lattice 位切 int 截断负值【模先于转换】）= V3-10 相位 B/4**）；**v3.25.0 = V3-10c 末影人（门 215，11-07：End 首个 mob = neutral + 瞬移 + 携格【数据全旗核心零物种 switch】：注视激怒 eye_contact_range 48 = kEndFogEnd 定案【5 连续帧零流计数眨眼复位 + 全 AABB 瞄准射线 + 瞄准块 t 严格遮挡】→ 30s 窗复用 / 瞬移 interval 2-6 严格配对存在性 = 能力门【受击 100% damage 只置旗 + update 固定槽帧序钉 + 流契约 8 候选 × 3 抽 + 1 重掷 = 25 抽/尝试全败亦消耗 + 平静 ±16/±8 自位锚 / 激怒 ±8/±4 朝玩家 12 点锚项目缩放 + 候选须空旷 ∧ 16 格内有地面【下探穿固体虚空重写】+ fall 复锚 + 落点音 + 全败 swallowed 行】/ 携格 10c-lite【carries_blocks + 块 carriable 双严格旗缺省冷 + stateful carriable init 点名拒 + 集 {endstone, chorus_plant} + 摸每帧 4×3×4 dy 含脚下 x→y→z 定序 + 首格遮挡 + 放 6000 帧全局长拍 + 死亡 100% 掉所携 + 0.9× 头顶立方双 pass】/ End 池 mob_spawning_end.json 单种族 = 10a 两相位槽消费 + 掉落 pearl 0-1 + eye 0-1【生存唯一来源，10a/10b 记档闭合】+ xp 5 + 声音 70→71 仅 enderman_teleport【ambient/scream/stare/endermite/定身/跨维池后移 10d】；format 32/sim 2 不变 canary 4 EXACT / atlas 172→174 零移位 / 块 133 不变 / 门内捕获 7 坑全 PITS 407【demo 槽不回写假测量 / 火脚级重叠 / 拾格带含脚下 / 遮挡首格语义 / 下探虚空 / 截图 679×744 / 按住 LMB 挖掘】）= V3-10 相位 C/4**）；**v3.26.0 = V3-10d1 末影龙核心 + 首杀闭环（门 216，11-07：矩阵唯一真大项首相位 = 龙 boss【第 16 mob 纯数据：200 HP + 8 组数据旗 + 8 盒 8×4 双翼，核心零物种 switch】：dragonFlight 三态复用 MobState【runAi flying 分派最顶纯新分支，ghast/bat 位级红线；盘旋定心 30【门内裁决 40+pull→30】/侧移 3.5s 火球 2.5s/驻坐出口锚 咆哮→脚位吐息云 + 三起飞】+ 流契约每帧 ambient 1 抽 + 跨圈恰 2 抽【strafe 命中亦消费 u2 = 门内重构纯函数 rollDragonLapDecision】/ 水晶 = 新块非实体【6 固定槽 4 塔顶 + 2 出口 D3 非 MC 10 + 破块爆半径 3 无连锁结构性质 + 箭扫掠碎 + 治疗源晶碎反伤 10 + 晶上永久火 D9 + 普查卸载无关【门内捕获 raw getBlock 0→6 漂】】/ 龙息云 = 零物理新实体【leavesCloud 尾参缺省 false 位级红线 + dragon_breath 3HP/s 玩家-only 效果 11→12】/ **首杀闭环 = format 32 零 bump 最强形**【击败态 = 蛋块世界态 (0,54,0) + 升天 60f 飞 + 180f 悬停 + 23 orb = 22×9+2 = 200 整 + wail + 240f 延迟门；重载 = 蛋在无复生；末地 orb/云存档静默跳过偏离记档 format 33 后移 d2】/ 血条 renderUi 段【186×8 十段 + 「末影龙」字体 +3 = 433 扩展区 528/528 满 + margin 8→24 门内裁名修 + 2s 淡出】/ **--dragon-damage CLI seam**【--explode 孪生——龙战斗 demo 不可手编 = 耦合系统定案】/ 生成门 playerInEnd∧mobs∧无龙∧非蛋→60f→(0.5,58,0.5) kSaltDragon；块 133→135 / mob 15→16 / atlas 174→179 零移位 / 声音 71→75 / 效果 11→12 / 配方 113→114；门内捕获 ×8【init 交叉检顺序 / 数值累加器泄漏 / demo 存档红线 / 流送依赖锚点 / -1 哨兵 delta / 同帧 spawn+kill 死循环 / 血条 margin / CJK 边界】= PITS 408；sim 2 不 bump canary 4 EXACT）= V3-10 相位 D1/2**）；**v3.27.0 = V3-10d2 召唤与复杀（门 217，11-07：crystal_base 块旗 4 块 + requires_crystal_base = placeBlock 数据门【end_crystal 生存可放置】/ 4 出口面晶 ∧ 无蛋 ∧ 无龙 → 60f 倒计时【面晶 ±X = arena 晶算数 + ±Z worldgen 缺席；任一损失取消】→ 4 晶引爆【相邻爆波及 = MC 同爆读】+ 蛋计时清除 + 缺位塔晶回写 + 显式 60f 复生 / **败亡旗**【首杀置位重载复位——无蛋 ≠ 首访，门内捕获破蛋 60f 无召唤复生】/ 蛋 60s 会话重生【玩家破蛋 → 3600f 回写，重载复位 D7】+ 蛋 blast_resistance 3【晶爆 r3 存活】/ **复杀 XP = 8**【单 8 值 orb，D5 1/24×200；首杀 200 burst 不变】/ --dragon-damage 每生成施加 / F3 第 21 行 dragon【全英文零新字形，528/528 满区不碰】；format 32 不变全会话态 / sim 2 不 bump canary 4 EXACT；门内捕获 5 坑全 PITS 409【负 cell 中心 -N+0.5【10d1 词表负侧偏格静默修】/ 无蛋 ≠ 首访 / demo 同帧死点击 + hold 续取目标 / 旋转 dy 累计 / /tmp 路径重写】）= V3-10 相位 D2/2 全 4 相位收官**）；**v3.28.0 = V3-12a 多人设计定案（门 218，11-07：矩阵最后项 V3-12 唯一「超大」架构级首相位 = 纯文档版【v3.0.0 开启版同款，零代码零模拟面】：设计权威入仓 docs/V3-12-MULTIPLAYER-DESIGN.md【390 行实证】——**方案 1 主机权威 lockstep 定案**【项目确定性纪律 = 锁步免费午餐：demo 内核 = 客户端循环形状 + 输入中继 = Input handle* 第二消费方 + 世界传输零 worldgen 纯函数同 seed 位级】+ 时间源 = 主机广播 clamped wall dt raw f64 位【real-dt 结构性零 desync，wall 直读 2 处 simTime 化 solo 位级】+ POSIX TCP 无 net 线程 + 协议 schema【HELLO 四指纹 / FrameState ~40B / D=2 无预测 38-42ms / 30 帧状态哈希字段清单】+ Player 双别名最小方案【m_player = primaryPlayer() 354 处零改动 + 10 成员迁移 + pass 归属二分表】+ streaming 跨机 2 处修正【primary 锚 + streamPending 共享账本】；12b = v3.29.0 门 219【format 32 零 bump + sim 2 不 bump + 全 sim 覆盖】；R1-R20 / D1-D15）；**v3.28.0 = 设计稿版本 banner 3.28.0**）；**v3.29.0 = V3-12b 2 人多人 PoC（门 219，11-07：矩阵收官前最后一版 = 主机权威 lockstep 全实现【设计稿 §4 全采纳】：net/ 新目录 ~1200 行【POSIX TCP 无 net 线程 + 流重组 R16 + HELLO 四指纹握手【TM_VERSION/sim/assetFNV/simConfigHash——seed 豁免 WELCOME 权威 + render 豁免 dt 广播归一】/ FrameState 40B【mouse raw f32 零量化 R4 + Ax/Ay R8 + locked R18】/ D=2 无预测 38-42ms / 状态哈希字段清单每 30 帧双向交换 + 300f DEBUG 行】+ player 多人化重构【m_players + m_localSlot + m_player raw pointer primary 别名 354 处零改动 + PlayerSession 吸收 10 成员族 + 14 引用别名绑 session 0 ~430 处零改动 + pass 归属二分【primary 锚不动/×N 循环化全 sim 覆盖：物理/生存/效果/骑乘/珍珠/交互 17 函数/拾取磁吸三拾取/眼接触/箭扫掠/面板/教程 toast/渲染 local 化】+ per-player aim【solo 相机路径位级 / net 第一人称形 D17】+ R2 simTime 2 处 + R10 xpSeed 盐 + R9 出生点 i】+ streaming 回退门共享账本修正【盘锚经别名自动 primary】+ host/client 双循环【lockstep 屏障【主机快跑吃未发输入= 门内捕获】+ 客户端等包阻塞 + 断线移除【EOF/RST 同形：100% 掉落 solo 续跑 / 客户端见主机 EOF 全退 D2】+ CLI --listen/--connect/--script【demo 时间线第四消费方 = 确定性输入】；format 32 零 bump D13 / sim 2 不 bump canary 4 EXACT / 偏离 D1-D20【D16 净世界 fresh only + D17-D20 增补】；门内捕获 9 坑全 PITS 410【client 环槽偏 1【f150 哈希点名】/主机快跑屏障 / RST≠FIN / WM 平铺竞态 / uiScale 时序 / seed 豁免 parity / 无火种子扫描 / --new 净主机 / 起始库存 per-session】）**）；**v3.30.0 = V3-12c1 大厅/聊天/N 泛化/心跳（门 220，11-07：kNetMaxPlayers 2→8 协议形 + accept 窗 N 定形【首客 10s 阻塞 + 3s 窗 250ms 截止重试【单次静默 break = 门内捕获坑】+ WELCOME 窗后统一 flush 同 N】+ 客户端全槽体【lockstep = 每机模拟每玩家——netAddPlayer(1) 硬编码 slot 2 segfault + f0 哈希孪生门内捕获修】+ 名牌【第三人称 world→screen 投影，kFov/kNear/kFar 三常量 game_local.hpp 共享单源，本地玩家无牌】+ F3 第 22 行 players【Latin 零新字形 528/528 满区不碰，test re-pin】+ 聊天【T/Enter + frameText 活 pump 本地面 + CHAT 包 type 7 客户端→主机重广播 + 8 行环浮层 300f 淡出 + Latin only 零字形 + **打字抑制**【Input 可打印域 0x20..0x7e owner 机捕获前本地吞——typed 'e' 开面板截断消息门内捕获修】+ 表现层结构不入哈希】+ 心跳【屏障 stall **min 水印** 10s 点名移除最滞后——anyProgress 健康同伴遮死者死锁形门内捕获修】+ **E 面板中继修**【isLocal 门 = 主机玩家面板 desync f570 门内捕获根——E/ESC 边沿全玩家处理 + 窗口/声音动作 localOwner 自门】；format 32 不变 / sim 2 不 bump canary 4 EXACT / 大厅 MOTD 意念并入 V3-17 主菜单项 / 门内捕获 9 坑全 PITS 411【accept 窗单次 break / ms-s 混用 / 客户端全槽体 / E isLocal 门 / typed 'e' 截断 / Mesa 无头读回竞态 / WM 平铺几何 / 尾帧 broken pipe / 屏障 min 水印】）= V3-12c 相位 1/2**）；**v3.31.0 = V3-12c2 断线恢复 REJOIN + format 33
  players[]（门 221，11-08：format 33 players[]【root player + inventory/
  inv_grid 迁入 per-player entry，entry 0 = primary 镜像 + 三面 version
  lie + kSavePlayerCap 8】+ 末地 XP orb 持久化【end_xp_entities 恒写，
  10d1 后移复议兑现】+ REJOIN 快照【save 形 WELCOME = 磁盘档 + 会话
  计数族 {weather 5 字段含双 splitmix 流 / spawnCounter×3 /
  particleBreakCounter} + 活粒子 + per-player 会话计时器族——全
  presence-gated on-disk 不变】+ 重连流程【持久 listener + 中会话非
  阻塞 accept + startFrame 采纳 + 重连者 = 新体 D25】+ Join/Renumber
  CTRL 广播【slot 重编号】+ net 主机抑制幽灵【载多玩家档 segfault
  修】+ feedJournal 哈希 live-diff + respawn 精确 float + mob-quiet
  合同 D24【活 mob 在场 = 中会话 rejoin 点名拒收】；门内捕获八坑全
  PITS 412；2987/2987（+13）；S1 第 102 次 + canary 4 EXACT + 迁移臂
  32→33 值保真 + REJOIN 臂三向哈希 f600 逐字 + 性能 A/B 同档）=
  V3-12c 相位 2/2 全 4 相位收官·矩阵收官项**）；**v3.32.0 =
  V3-16a 扩展指导 demo 族：数据层 + scene + panel_click + pilot ④
  （门 222，11-08：demo 世界 spec 第三形 scene【committed 确定性存
  档资产 assets/demos/scenes/ 只读 = 观察红线结构根，--save 可组合】
  + **panel_click 事件**【几何无关格地址：panel/slot/button 词表严
  格 + resolver 开面板检查 + layout rect 中心当前几何孪生 + 真实
  Input closure 合成点击——uiScale/窗口任意值鲁棒；record tap 永不
  产生 = author 面独占；net --script 拒收【40B 无 panel 字段边界】；
  **sim 2 不 bump**【增量事件型 canary 4 EXACT 执法】】+ **索引数
  据层**【index.json 四轴 schema {category,seq,mode,tags,file,
  length_frames} + 5 章封闭词表【day1/build/dimension/survival/
  coop = 4+1 定案】+ init 每次 boot 全量交叉检查【文件 + strict
  load + SIM 门 + length + scene 存在，nether 池 fail-fast 传统】+
  --list-demos 独立无头旗】+ **工具链**【mk_scene.py 场景手术幂等 +
  demo_author.py Guide DSL【轴对齐行走 + 相对放置/开容器 + 逐格
  贴线挖矿】+ scenes/ beat 脚本】+ **pilot ④ 挖矿打造工具 11387f
  03:09**【砍树 5→砂 2→木板 20→工作台→木棍 8+木镐→逐格挖矿 11→
  熔炉环 8→熔砂 2 玻璃；终态钉 h0 煤 2/h2 铁 1/h4 镐 dmg 11/h5 木
  板 9/h8 玻璃 2 + 表(5,54,-7)+炉(4,54,-7) 空态 + 零实体】；2998/
  2998（+11）；S1 第 103 次【f60 ×2 ≡ base19 + 旧树三方闭环——
  base19 锚 = f60 门内定案】+ canary 4 EXACT + pilot ×2 RAW/savenorm
  + S6 五面 + 性能 A/B 同档；门内捕获 PITS 413 十九坑【f0 捕获 /
  关面板不重锁 / look 符号 / 硬编码帧重叠 / 斜行 4× / cursor 不同步
  / REG 2.5f 标定 / 放置瞄地面 / 相对放置 / 行墙挡视线 / 磁吸衰减 /
  工具态破碎时长 / 续挖穿平台 / mobs 误击耐久 / furnace 共享格三开
  态 / base19 f60 / --frames 白等 / journal 同态移除 / scene 完整性
  约束】）；**v3.33.0 = V3-16b 扩展指导 demo 族 ⑤⑥（门 223，
  11-08：day1 章节第二三支配送——内核零变更【16a scene/panel_click/
  索引全复用】纯内容 + 工具链增量：**⑤ guide_food 18262f 05:04**
  【6×4 麦田 scene【farmland 挖洞铺——同格 journal 重复点名】+
  wheat_7×8 可收 + 年轻梯度 ×16 demo 期间帧锁定生长视觉长熟】→
  逐格收割【1f tap = 恰 1 破——硬度 0 连锁坑】→ E 面板面包 ×5 →
  进食 ×2【1.6s 通道 96f】hunger 10→20 满】+ **⑥ guide_regen
  18344f 05:05**【5 格石柱 5 落差台 → 走下台 hp 20→18 → 进食 →
  **10s 门 + 2×4s 自然回血**【每半心耗 1 饥饿】hp 18→20 满】+
  工具链【**capture 2f 化** = 零 locked-mining 帧——f0 捕获 hold 尾
  部瞬破 0 硬度作物 ×3 的门内捕获根 / 旧分段 walk_polyline 死码清
  除】；终态双钉【⑤ h0 麦 1/h1 面包 3/hunger 17/田 16 熟 / ⑥ hp 20/
  hunger 13/面包 1 余】；2998/2998【内容相位零新增】+ S1 第 104 次
  + canary 4 EXACT + ⑤⑥ ×2 RAW/savenorm + 性能 A/B 同档；门内捕获
  PITS 414 四坑【f0 捕获尾部挖掘 / scene journal 同格重复 / 1f vs
  2f tap 硬度 0 连锁 + 年轻麦 f3000 全熟 / perf A/B cwd 陷阱】）
  + **v3.34.0 = V3-16c 扩展指导 demo 族 ③完整房子+床（门 224，
  11-09：guide_house 18875f 05:14 = day1 ④⑤⑥③ 全 4 支完成——床预
  制先做【夜窗时序】+ 二段墙 18 地面顶面 + 脚手架唯一 step-up 链 +
  屋顶 12 = 北行屋顶 walk/南行二段南顶面+middle x5 链面/middle 行
  室内链+南 x2/x3 门缺口列仰面【地面眼 y55.62<y56 侧底面截断 =
  PITS 415② 可行面谱】+ 4-hops 下行无坠漂 + 落点原位脚手架双挖净
  0 块 + Slept at the bed (4,54,-5) + 黎明巡视；walk_leg 尾参
  y/hold + mk_scene 容器态透传；门内捕获 PITS 415 七坑【出生点表格
  重叠静默顶 y55 全链死 / 侧底面截断 / 坠腿 W 漂 / 长 leg 损耗 / 屋
  顶 walk z stop 1cm 生死 / 容器态三处同步 / gzip 0x8b】）；
  **v3.35.0 = V3-16d 扩展指导 demo 族 ⑦皇宫（门 225，11-09：
  guide_palace 19536f 05:25 = build 章首支，**100 块 = 92 墙 + 8
  四角塔**【内圈地板 38 块 CUT 偏离记档 = 走位漂移与 1-m 格 aim
  容差根本冲突】——双层围墙 92【t1 stone_bricks + t2
  terracotta_yellow，门洞 x8/x9 双层开，地面行走绝对格 aim =
  92/92 ×4 版实证】+ 四角塔 = 垂直-only 角循环【B 单按 fly ON →
  45f 升 3.0m 精确（脚 57.0 眼 58.62——65f 眼 59.95 时 8 aim 全
  4.12-4.15m 超 kReach 4.09 = 0/8 门内捕获反解）→ aim t2 顶面
  【点在面 = 命中与眼高无关】→ B 单按 fly OFF → 0.1-m 墙线地面
  走到下一角 9/16/9-m】×4（SW→NW→NE→SE；NW→NE 走线 z-12.0 外圈
  北绕行【v9 z-10.5 北墙隧道 + v10 z-9.0 东墙隧道双门内捕获——
  内圈 z 线必撞东墙，逐线三轴核】）+ 航拍展示 2 + 结尾四面巡视
  【SE 落地即出，终姿 (8.55,54,3.8)】；飞行 toggle 全 gamepad B
  单按【creative scene = 入场飞行 PITS 416①，无墙钟窗 sim 确定】+
  dmo 后贴 demo_fly_patch【pinned gzip 字节 parity】；门内捕获
  PITS 416 十三坑【入场飞行 / 飞行 7.12 实测 / 脚手架死路 / 帧停
  诊断 + autosave 覆写 / scene inventory 严格形 / 后贴 parity /
  纯注入 / checker 名称比 / 飞行 aim 双坑 / 窗焦点臂杀手 / 走位
  漂移 fps 函数 / 走线墙隧道 / 元数核对】；2998/2998【内容相位
  零新增 + banner re-pin】+ S1 第 106 次【f60 ×2 ≡ base19】+
  canary 4 EXACT + guide_palace ×2 RAW/savenorm EXACT + 性能 A/B
  同档【终档跨树 savenorm EXACT ×2 = 结构性零成本最强形——src/
  逐文件恒等仅 banner 差】）；**v3.36.0 = V3-16e 扩展指导 demo 族
⑧材料（门 226，11-11：guide_materials 25383f 07:03 = 7 biome 床
33 标本收集族【森林/沙漠/羊毛/针叶林/矿脉 depth-1/花田/走道】+
SURVIVAL 收集面【真破格时长 + 磁吸收集 + E 面板展示 + C 第三人称
巡视】+ 内核 1 行×10 demo 恒锁定【初 3 处 + canary RAW diff 捕获
7 处【C 相机会话态 = 根】——12 处 relativeMouseMode() 审计全表】+
demo_author 向量行走模型【_walk_dd 角进入速度轴归属 + 0.66m 可
表达带】+ 分段前缀验证 11 段【用户定案 ≤30 段】；门内捕获 PITS
417 二十二坑；2998/2998【banner re-pin 1】+ S1 第 107 次【f60 ×2
≡ base19】+ canary 4×2 新≡旧二进制 RAW+savenorm EXACT +
guide_materials ×2 RAW/savenorm EXACT + 性能 A/B 同档【终档跨树
savenorm EXACT = 结构性零成本】）**；**v3.37.0 = V3-16f 扩展指导 demo 族
⑨河流（门 227，11-11：guide_river 19841f 05:30 = build 章第三支，24×15
平台 + dirt 表层 + 1-deep 密封源水池 16 + campfire LIT + SURVIVAL
bucket/torch 预置；tour = 挖河 15 + 舀水 1 + 倒水 W + 观西梯度 +
舀水 2 + 倒水 E + 观东梯度 + 火把 (14,54,-5) 水火对比 + 游泳横渡
【probe v3 pin 实测落点 (0.65,54,-6.16)】+ E 面板 15 泥土 + 第三人
称双线巡视；终态钉 对称 V 沟形 + 池 14 源 2 痕 + campfire_
extinguished + 零实体；零内核【16a 全复用】+ mk_scene
fluids/campfires 两 strict 面 + 14 段前缀验证；门内捕获 PITS 418 十
六坑【mid-coast jog 漂移放大器两实例 + 游泳入口末腿同轴修 + 池穿列
落水 + finished 模拟冻结 dwell 240 + campfire 侧面邻格挖 + y54 穿越
点落格】；2998/2998【banner re-pin 1】+ S1 第 108 次【f60 ×2 ≡
base19】+ canary 4×2 新≡旧二进制 RAW+savenorm EXACT + guide_river ×2
RAW/savenorm EXACT + 性能 A/B 同档【终档跨树 savenorm EXACT = 结构
性零成本】）**；**v3.38.0 = V3-16g 扩展指导 demo 族 ⑩完整黑夜
历险（门 228，11-12：guide_night 18301f 05:05 = survival 章首支，
40×26 平台 + time 0.55 入夜 + SURVIVAL + seed 7【seedhunt 8 粒：首批
4 = sheep/chicken/chicken/zombie f150 唯一 hostile，无 skeleton
wither/无 spider kite 账本】+ 预置 planks 9/coal 1/flint 2/feather
2；tour = P0 开场 → kite NE 造 table → SE 停扎最小 combat kit
【table (38,54,-2) + sticks + bow（P=g5）+ arrows×1】→ **弓箭
combat**【N 走到 (38.5,-14) 7-m，4 箭 full-draw 站射击杀 zombie
f3524——**弓 = 短程 v²/g 11.25-m**，箭 release 发射按 release 帧
aim + hold=100 满力 + 弹道 lead + knockback 定点 5 轮迭代】→ 回 SE
造余 kit【select 非弓再开容器】→ **火把环 ×4**【先走到点再放】→
黑夜巡视【C 第三人称 + E 面板展示】→ 黎明 face E（t0.058）；终态
钉 hp20 存活 + stick 2/bow(dur380)/arrow 4 + 4 火把 + table +
entities 0【掉落 120s 消散】；零内核【16a 全复用】+ arrow_sim/
aim_from_log 弓箭弹道 aim pin 探针族 + 8 段前缀验证；门内捕获 PITS
419 八坑；2998/2998【banner re-pin 1】+ S1 第 109 次【f60 ×2 ≡
base19】+ canary 4×2 新≡旧二进制 RAW+savenorm EXACT + guide_night ×2
RAW(169498 B)/savenorm EXACT + 性能 A/B 同档【changesApplied 9/3/1
vs 9/2/1，终档跨树 savenorm EXACT = 结构性零成本】）**；**v3.39.0 =
V3-16h① 扩展指导 demo 族 ①搭建下界传送门 + 去下界（门 229，11-12：
guide_nether 10865f 03:01 = dimension 章首支，16h 拆两版①下界门/②末影
龙；12×11 平台正午 SURVIVAL + 预置 flint_and_steel h0 + obsidian 4×5
帧【14 ring 格 z=-4 未点燃】；tour = P0 开场 → 点火【aim 底行顶面
(5.5,55,-3.5) 朝内面 → "Portal activated at (5,54,-4) (4x5)"】→ 站入
60-f 传送【"Traveled to nether at (1,55,-1) from (5,55,-4)" 8:1 映射落
生成帧底中心】→ 下 frame 到 nether 地面 y53【2-m 落无坠伤】→ 观下界
【walk 回路 + C 第三人称 sweep ×3：玄武岩柱 + 熔岩海 + 红天 + 生成
portal】→ 收束（player 下界 (0.97,53,2.87) hp20）；**返程不做**【下界
生成 portal 返程 = 900-f 冷却 + 2-m 上台帧难 + auto-re-travel 乒乓，②
走 end exit 更自然】；零内核【V3-1 Phase B 基建全复用】+ 传送后 author
track 手动 re-anchor；门内捕获 PITS 420 四坑【点火 aim 底行顶面/传送
后 track 失效/返程乒乓 = 下 frame 规避/frame 悬空 2-m】；2998/2998
【banner re-pin 1】+ S1 第 110 次【f60 ×2 ≡ base19】+ canary 4×2 新≡旧
二进制 RAW+savenorm EXACT + guide_nether ×2 RAW/savenorm EXACT + 性能
A/B 同档【终档跨树 savenorm EXACT = 结构性零成本】）**；**v3.40.0 = V3-16h② 扩展指导 demo 族 ②打末影龙（门 230，11-12：guide_dragon 15214f 04:13 = dimension 章第二支 = **V3-16 10 主题全完成**，16h 拆两版收官；12×12 平台正午 SURVIVAL mobs ON + 预置 ender_eye 12 + ender_pearl 1 + netherite 甲 4【RMB 即穿 80% 减伤 6→1.2/击】+ end_portal_frame 环 12 格【survival 不可得预置】；tour = 装备 → 嵌 12 眼【GOOD cell (6,4) 末位 PITS 421】→ 传送【Traveled to end (40,53,0)】→ dragon --dragon-damage 200 同帧杀 + 仪式【egg (0,54,0) + 23 orb + ascension】→ **珍珠 28° 弧 40-m 一掷**【flat 仅 12-m，pearl teleport (1.89,54.69,-4.30)】→ 入 center【出口门环 = 1-block 实心墙 step-up/down】→ 破 2 出口水晶【f1960/f2019 explode r3 爆坑 28 格；tower 晶 off-route CUT】→ 观蛋【egg 完好 = 站位眼位离 egg box ≥1-m】→ 等 900-f 冷却 → 返程 exit portal【落 (0.5,54,0.5)】→ 观点亮门；终态钉 hp20 / xp 92【12 早落 orb 穿未载 chunk 沉基岩洋 y5 持久 = streaming 时序偏离】/ 甲 4 件各 6.5 损 / inv 空 / lit 9/9 + eyed 12/12；**--dragon-damage 200 必带**；零内核【V3-10a-d + V3-10b + SURV-1 基建全复用】+ pearl 轨迹探针族；门内捕获 PITS 422 八坑【视图 re-anchor yaw+位置 / 珍珠仰角 40-m / 珍珠穿晶 shatter 吞 / egg t0 挖 / chunk 载竞 / step 漂移 0.5-m / rim 大 yaw miss / 环 1-block 墙】；2998/2998【banner re-pin 1】+ S1 第 111 次【f60 ×2 ≡ base19】+ canary 4×2 新≡旧二进制（tm_v339 v3.39.0）RAW+savenorm EXACT + guide_dragon ×2 RAW/savenorm EXACT + 性能 A/B 同档【upd med 0.523/0.576/0.565 vs 0.527/0.583/0.557，终档跨树 savenorm EXACT = 结构性零成本】）**；**v3.41.0 = V3-19
死亡屏计时（门 231，11-12：11-08 用户报障「死亡界面存活时间不停增加」
= 表现层 3 点修零 format——根因 存活行活读 m_frameCount（死亡后世界
模拟照跑）；修 = PlayerSession + deathFrame 快照【-1 回落 = 旧活读位
级】+ 转移块 deathCount++ 旁钉帧 + 死亡屏读改快照
Player::survivalStatsLine 纯函数【deathCauseLine 孪生——cause 行读
DAMAGE INFO 此行读 FRAME】；**死亡屏两帧臂**【8×8 平台 y84 坠死 f≈97
无输入确定性 + day_length 1e7 冻日：新树 f254 vs f434 存活行 crop
AE=0 冻结 vs 旧树 tm_v340 同臂 AE 43/42 px 爬升复现【「4 秒→7 秒→
10 秒」秒位字形差 = TDD 前置】】；2999/2999（+1）+ S1 第 112 次【f60
×2 ≡ base19——死亡屏只在死时画锚零像素】+ canary 4×2 新≡旧 RAW+
savenorm EXACT + 性能 A/B 同档 + PITS 423 两坑【行 y 名义位 ≠ 渲染
bbox scale-2.0 标题行步下推 / 两帧臂日冻结 day_length 1e7】）**；**v3.42.0 = V3-18 存档扩展名（门 232，11-12：11-08 用户诉求「.json 扩展名体现不出压缩后的 json」——零 format / 零像素：slotWritePath/slotReadPath 路径族【.json.gz 新写 + 两级探测零迁移】+ 管理面双扩展名 + scene 资产 11 个随改 .json.gz + demoSceneReadPath 参数化单源；3005/3005（+6）+ S1 第 113 次【WM 平铺漂移 = config 请求锚几何 679×744 钉死 PITS 424④】+ canary 4×2 新≡旧 RAW+savenorm EXACT + 迁移臂 + scene 臂 + 性能 A/B 同档；PITS 424 四坑）**；**v3.43.0 = V3-17a 主菜单系统：标题页 + 教学入口
（门 233，11-12：11-07 用户实机诉求首相位——交互 boot 进主菜单 = 第 4 循环
runMenuLoop【window+renderer+UI 资产后 buildScene 前】+ renderMenu【clear +
UiBatch only 禁 render/renderUi 世界未建；原语 = renderUi 拷贝原文零触碰】；
标题页 = logo + 版本行 + 单人/多人/设置/教学/退出【hover + click/Enter +
ESC；零新 GameKey】；**单人 = 直接 fresh world-start（--new 形）——click
臂 单人≡--new savenorm EXACT = 菜单零模拟贡献最强形**；多人/设置 = stub
17b/17d；**教学 stub = kDemoCategories 5 章标题**（两级 = 17c）；
menuEnabled 五形绕开【demo/net/script/--new/管理旗 = 结构免疫根】；
buildUiAssets 幂等提取【buildScene 头：ui shader+font+双 scale】；菜单
退出 save 写跳过【槽不创建断言】；**字体 +21**【人设教学返主距气声缩
窗联创继第大维远征历协】= 代码点序插入 + 后缀 +1..+21 重格【侦察
「尾部追加」否决 = lower_bound 有序不变量 PITS 425③】+ atlas 2112→
2208【region 528/528 满 V3-10d 起】+ top-512 红线 131072 px 恒等 +
font.png FNV re-pin 9d0c9e32976679ff；**S1 重锚 base20**【f60 无输入 =
标题 21900 B——菜单无输入纯函数 ×2 逐字节 + defined mouse face
(10,10) no-hover warp 纪律 PITS 425②】；3010/3010（+5 菜单族）+ canary
4×2 新≡旧（tm_v342 v3.42.0）树内 RAW + savenorm EXACT + 跨树 RAW =
字体 v-UV 重缩放文本边缘白名单【92-118 px 0.01% 逐 px 审计 100% 文本
边缘 + 零世界像素 PITS 425④，17b 起恢复 EXACT】+ 性能 A/B 同档
【changesApplied 9/3/1 逐位 ×2 + 终档跨树 savenorm EXACT】；PITS 425
四坑【beginFrame drawable 双坐标系 / hover 鼠标位置依赖 / 字体尾部追加
否决 / atlas v-UV 重缩放】）**；**v3.44.0 = V3-17b 主菜单：设置页
（门 234，11-12：BACKLOG 规格 config 表单化编辑——设置页 8 行表单
【视图距离 4..64 ±4 / 白天长度 60..86400s ±60 / 天气 toggle / 声音
0..10/10 / 界面缩放 1.0..4.0x ±0.5 / 全屏 toggle / 窗口宽高 ±64】+
保存钮 + hint；**零文本输入**（stepper/toggle，SDL 文本面不碰）+ ESC
返回 / Enter 保存；纯布局+状态机 test 孪生（gate 点击同源公式）；
**ui.scale = 唯一运行时生效面**（m_uiScale 每帧读 = 菜单 reflow 活体
证明，世界启动同值；其余 = 世界启动/下次启动生效）；**写回 = 既存
未用 Config::save**（保存 confirm 后整档 dump——std::map 键序字母
归一记录偏离，值全保 = 未动键存活；整值出整数）；**字体 +3 屏宽项**
= 代码点序插入 + 后缀重格，**atlas 2208 不增长**【69 行 headroom
承接 max cell 551】= v-UV 不变 → 门 233 跨树文本边缘白名单**不复
发**【canary 跨树 RAW 恢复 EXACT ×4 = PITS 425④ 一次性门面实证】；
3012/3012（+2）+ S1 base20 重锚【×2 逐字节 + 旧锚 diff 95 px 单行
= 版本行有界断言——PITS 426② 菜单期 S1 逐版重锚】+ 设置臂【天气关
+ 缩放 2.0 LIVE reflow + 保存 @2.0 几何：写回断言 PASS + a3 臂 scale
2.0 标题 ×2 RAW EXACT】+ canary 4×2 新≡旧（tm_v343）树内 RAW +
savenorm + 跨树 RAW EXACT ×4 + 性能 A/B 同档【median 交错 +
changesApplied 9/3/1 逐位 + 终档跨树 savenorm EXACT】；PITS 426 两
坑【value 列中心叠字 / 标题版本串】）**

  ——**3.x 矩阵
21 项 = 原 11 项全 ✅（v3.1.0–v3.11.0 门 191–201）+ 矩阵后 v3.12.0
（门 202）/ v3.13.0（门 203）/ v3.14.0（门 204）/ v3.15.0（门 205）
/ v3.16.0（门 206）/ v3.17.0（门 207）/ v3.18.0（门 208）/ v3.19.0
（门 209）/ v3.20.0（门 210）/ v3.21.0（门 211）/ v3.22.0（门 212）
/ v3.23.0（门 213）/ v3.24.0（门 214）/ v3.25.0（门 215）
+ 11-06 扩容 7 项（V3-8b ✅ v3.14.0 + V3-13 ✅ v3.15.0 + V3-15 ✅
v3.16.0 + V3-11 ✅ v3.17.0/v3.18.0 + V3-14a ✅ v3.19.0 + V3-14b ✅
v3.20.0 + V3-14c ✅ v3.21.0 + V3-14d ✅ v3.22.0【V3-14 全 4 相位完成】+
V3-10a ✅ v3.23.0 + V3-10b ✅ v3.24.0 + V3-10c ✅ v3.25.0 + V3-10d1 ✅ v3.26.0 + V3-10d2 ✅ v3.27.0【V3-10 全 4 相位完成】+ V3-12a ✅ v3.28.0 + V3-12b ✅ v3.29.0【2 人 PoC 完成】+ V3-12c1 ✅ v3.30.0【大厅/聊天/N/心跳】+ V3-12c2 ✅ v3.31.0【断线恢复
REJOIN + format 33 players[] = 矩阵收官项·V3-12 全 4 相位完成】+
**11-07 用户实机诉求 2 项 ⬜【V3-16 扩展指导 demo 族
【10 主题 3-10 分钟级：下界门/末影龙/房子+床/挖矿工具/体力/回血/皇
宫/材料/河流/完整黑夜历险——**16a ✅ v3.32.0 = 内核 + 工具链 +
pilot ④（门 222）+ 16b ✅ v3.33.0 = ⑤体力⑥回血（门 223）+
16c ✅ v3.34.0 = ③房子+床（门 224，day1 全 4 支完成）
+ 16d ✅ v3.35.0 = ⑦皇宫（门 225，build 章首支，100 块）
+ 16e ✅ v3.36.0 = ⑧材料（门 226，build 章第二支，7 biome 床
33 标本）+ 16f ✅ v3.37.0 = ⑨河流（门 227，build 章第三支，19841f）
+ 16g ✅ v3.38.0 = ⑩黑夜历险（门 228，survival 章首支，18301f，弓箭
 combat + 火把环 + 活到天亮，PITS 419 八坑）+ 16h① ✅ v3.39.0 = ①下界门（门 229，dimension 章首支，10865f，obsidian 帧预置 + flint_and_steel 点火 + 站入传送 8:1 落生成帧 + 下 frame 观下界，返程不做，PITS 420 四坑）+ 16h② ✅ v3.40.0 = ②末影龙（门 230，15214f，嵌 12 眼【GOOD cell 末位】+ --dragon-damage 200 仪式 + 珍珠 28° 40-m 一掷 + 破 2 出口水晶 + netherite 甲 enderman 对策，PITS 422 八坑）** = V3-16 10 主题全完成】+ V3-19 死亡屏计时 ✅ v3.41.0（门 231，存活行快照 3 点修，两帧臂 AE 0 vs 旧树 43px 复现）+ V3-18 存档扩展名 ✅ v3.42.0（门 232，.json.gz 新写 + 两级探测零迁移 + scene 资产随改，PITS 424 四坑）+ V3-17 主菜单系统【17a ✅ v3.43.0 标题页 + 教学入口 + 字体 +21
（门 233，S1 重锚 base20，PITS 425）+ 17b ✅ v3.44.0 设置页（门 234，8 行
表单 + Config::save 写回 + 字体 +3 不增 atlas，PITS 426）/ 17c-d ⬜；
标题/设置/登陆/教学/多人五页 + MOTD 并入】】**【大项已相位拆分 V3-10a~d /
V3-11a~b / V3-12a~c / V3-14a~d / V3-16a~h，BACKLOG 待执行项明细】**）

- **v2.0.0 = 1.x 收官里程碑**（无新玩法）：1.x 任务表 40/40 + UX-8g/UX-9
  实机报障 4 项全完成（v1.50.0 = 最后 v1.x 版本）+ 2.0.0 扫尾三件事——
  ① 1.x 阶段文档归档 `docs/archive/v1x/`（181 份 phase + `HISTORY.md`
  全史）② **第三人称准星不压头**（MC 同款：轨道点沿相机 up 抬高 0.4 块，
  头降到屏幕中心下 29-96px；瞄准射线仍自眼位 = UX-8g 不变，TP 固定视差
  = MC 同款记偏离）③ 活文档刷新到 2.0 状态。门 171 全 PASS。
- **任务只从 `docs/BACKLOG.md` 的 v2.x 节取**（11-03 三期分工定案：2.x
  「上手与体验补全」V2-3~V2-10 八项**全部完成（11-03 收官，
  tag v2.10.0）**；**11-03 收官日用户定案：原 3.x 八项并入 2.x
  = 第二批 V2-11~V2-18（XP/附魔/床/蝙蝠/染色羊/雪原 biome/顶部
  slab/AO，`docs/BACKLOG.md` 执行权威）**；**用户实机诉求提前项
  V2-19 视角/移动/时钟批已完成 = v2.11.0 门 182**（滚轮缩放 +
  双击 Space 飞行 + 24h HUD 时钟，全会话态零 format，base18 第
  3 次重锚）+ **V2-11 XP 系统已完成 = v2.12.0 门 183**（stash
  半成品恢复 + 全整数 MC 曲线 + 经验球 + 三发放源 + format 26 +
  XP 条 HUD，base18 第 4 次重锚 179027 B）+ **V2-12 附魔已完成 =
  v2.13.0 门 184**（7 附魔数据驱动 + 附魔台/书架 + 确定性 offer
  全整式 + format 27 + 面板，结构免疫第 65 次免重锚）+ **V2-13 床
  已完成 = v2.14.0 门 185**（15 色染色族 Slab 床 + 睡眠三门即时跳
  夜 + format 28 respawn + 验床重生，结构免疫第 66 次免重锚）+
  **V2-14 蝙蝠已完成 = v2.15.0 门 186**（项目首只飞行 mob：悬停
  巡航 AI + 洞穴 in-air 生成 + 0.9×0.5 展翼扇翅 + squeak 氛围音，
  零 format savenorm EXACT，结构免疫第 67 次，详见
  `docs/archive/v2x/phase-2.15.0.md`）+ **V2-15 染色羊已完成 = v2.16.0
  门 187**（shears 238 耐久 + 剪毛 1-3 彩毛 + 出生毛色流首抽 + 染色
  bare 拒染 + 死亡彩毛钩子 + per-entity 变体网格，零 format savenorm
  EXACT，结构免疫第 68 次，详见 `docs/archive/v2x/phase-2.16.0.md`）+
  **V2-16 雪原已完成 = v2.17.0 门 188**（snowy_taiga 数据驱动 +
  exclude_spawn_area 演示区结构保护 + snow 满块 + planTree 针叶 +
  音效缺陷族 13 件修复，零 format 世界演进，结构免疫第 69 次，详见
  `docs/archive/v2x/phase-2.17.0.md`）+ **V2-17 顶部 slab/双 slab 已完
  成 = v2.18.0 门 189**（stone_slab state_count 3 + drops_count_by_
  state 破掉 2 + 放置瞄准规则 + 同格合并 + partial-aware slab 射线，
  零 format bump，结构免疫第 70 次，详见
  `docs/archive/v2x/phase-2.18.0.md`）+ **V2-18 AO 环境光遮蔽已完成
  = v2.19.0 门 190（二批收官：第二批 8/8 全完成）**（经典 MC 顶点 AO
  4 档 1.0/0.75/0.5/0.3 双邻特例 + opaque ∧ 高度带闭区间 + bake 入
  既有光 attribute = 零 shader/零顶点格式，greedy MaskCell+ao[4] 合并
  键，fluid/cross/wall/fence/pane 免 AO，render.ao 严格 bool 默认
  true，**off = pre-V2-19 位级恒等**【门内 off ≡ 旧锚实证】，零
  format，**门 190 重锚 base18 ← 224468 B**【单调变暗审计 + 1 px
  z-fight 白名单】，4 偏离记档，PITS 384，详见
  `docs/archive/v2x/phase-2.19.0.md`）；**2.x 全史
  （v2.0.0→v2.19.0，两批 20 版）11-04 收官** /
  3.x「世界扩展」= **新执行权威** `docs/BACKLOG.md`（11-04 晋升 +
  逐项定案 + **同日用户重编号定案：原 4.x 命名改 3.x**【2.x 已吸收
  原 3.x 八项，回归自然序号】）：6 项全保留 + 用户实机诉求 **V3-7
  存档 gzip 压缩（默认开 = format 29，旧明文档魔数探测零迁移）已完成
  = v3.1.0（门 191 全 PASS，11-04，实测门档 6.3×/大档 5.4×）**，其
  后 V3-1 下界维度锚【超大】拆分立项【门框→下界世界→内容三段；立项
  调研已交付 `/tmp/opencode/v31_recon.md`】；**v3.0.0 = 3.x 开启版已
  发布**（11-04：重编号 + README 里程碑精简，纯文档 + banner，零玩
  法变更，tag + release）；2.x 总量 24 < pre-v2.0 的 78-84 = 用户
  采纳判据 ✓）；2.x 全史 =
  `docs/archive/v2x/HISTORY.md`（+ 同目录 BACKLOG/CHANGELOG/phase
  存档 20 份，全两批）；1.x 历史全在 `docs/archive/v1x/HISTORY.md`
  （只读，别把历史细节当现状）。
- 测试 **3012/3012**（2777 基线 + V3-8a 手柄 open 修复 1【用户 11-06 并行提交 d5a5ea6】+ v3.13.0 demo sim 族 6 + v3.14.0 V3-8b 手柄映射族 3 + v3.15.0 死亡统计屏族 5 + v3.16.0 V3-15 小批族 3 + v3.17.0 V3-11a 创造基础族 6 + v3.18.0 V3-11b 创造物品栏族 10 + v3.19.0 V3-14a 爆炸机制族 10 + v3.20.0 V3-14b ghast 族 12 + v3.21.0 V3-14c piglin 交易族 14 + v3.22.0 V3-14d strider 骑乘族 9 + v3.23.0 V3-10a 末地族 29 + v3.24.0 V3-10b 末地内容族 14 + v3.25.0 V3-10c 末影人族 31 + v3.26.0 V3-10d1 末影龙族 29 + v3.27.0 V3-10d2 召唤复杀族 5 + v3.28.0 banner re-pin 0 + v3.29.0 V3-12b 多人 net 族 10 + v3.30.0 V3-12c1 大厅聊天 re-pin 0【F3 22 行既有面】+ v3.31.0 V3-12c2 REJOIN/format 33 族 13【SaveFormat33 9 + NetSnapshot 4】+ v3.32.0 V3-16a 指导 demo 族 11【词表/展开/编解码/script 拒收/scene/索引拒收/交叉检查】+ banner re-pin 1 + v3.34.0 banner re-pin 1【V3-16c 内容相位零新增】+ v3.35.0 banner re-pin 1【V3-16d 内容相位零新增】+ v3.36.0 banner re-pin 1【V3-16e 内容相位零新增】+ v3.37.0 banner re-pin 1【V3-16f 内容相位零新增】+ v3.38.0 banner re-pin 1【V3-16g 内容相位零新增】+ v3.39.0 banner re-pin 1【V3-16h① 内容相位零新增】+ v3.40.0 banner re-pin 1【V3-16h② 内容相位零新增】+ v3.41.0 V3-19 死亡屏计时族 1【survivalStatsLine 纯函数】+ v3.42.0 V3-18 存档扩展名族 6【slot 路径族/双形枚举去重/复制重命名迁移/删除全槽/scene 两级探测】+ v3.43.0 V3-17a 主菜单族 5【布局门几何/状态机 11+4 面/词表章表 +
banner re-pin 1】+ v3.44.0 V3-17b 设置页族 2【布局门几何 8 面 hit/
状态机 11 面 + banner re-pin 1】），`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告；
  存档 format 33（V3-12c2 多人玩家态：players[] per-player 数组【root player + inventory/inv_grid 迁入 entry，entry 0 = primary 镜像】+ end_xp_entities 恒写【末地 orb 持久化】+ respawn 精确 float【≤32 int cell】；net 快照段 {particles/weather/spawn_counters/particle_break_counter/net_players} 仅 WELCOME 文档携、on-disk 省略；schema 其余与 32 一致）；
  存档 format 32（V3-10a 末地：dimension 词表按 format 分词【30/31
  二词，31 携 "end" = version lie；≥32 三词】+ end 段 {blocks,
  fluids,states} + end_entities 恒写 + endSeed 不落盘；schema 其余与
  31 一致）；format 31 = V3-11a 创造（player.creative strict bool
  恒写，pre-31 载迁 false，30 携键 = version lie 点名；schema 其余
  与 30 一致）；format 30 = V3-1 Phase B 下界维度（player.dimension 严格
  词表恒写 + nether 段 {blocks,fluids,states} + nether_entities 恒写 +
  派生 seed 不落盘；schema 其余与 29 一致，29 携新键 = version lie
  点名）；format 29 = V3-7 gzip 时代（schema 与 28 一致——编码与
  format 号正交，on-disk = 确定性 gzip 容器默认开【10B 头全钉 + raw
  deflate 6 + CRC32/ISIZE 尾】+ 加载魔数 `1f 8b` 探测零迁移 +
  `save.compression: "gzip"|"none"` 严格读【absent = gzip 用户定案】
  + 实测 6.3× 门档/5.4× 大档；28 = V2-13 床 respawn 恒写；27 = 附魔
  xp_seed+enchants；26 = XP；V2-14 蝙蝠 / V2-15 染色羊 / V2-16
  雪原 / V2-17 slab / V2-18 AO 均零 format【slab state = format 15
  states 机制；AO = 纯渲染 bake 入既有光 attribute】）；
  base19 锚 **225272 B**（679×744 WM tile 态——重锚谱系：179116 →
  178148【V2-10 门 181】→ 178676【V2-19 门 182】→ 179027【V2-11
  门 183】→ 224468【V2-18 门 190，AO 世界光照】→ **225272【V3-6
  门 200，填充感知雾 = base18 后首重锚：无输入视图盘缘截面雾化（20940
  px diff 全盘缘带 + 单调趋天空 0 违例），稳态位级恒等，fog-off ≡
  base18 三方闭环】**；门 184-189 = 结构免疫第 65-70 次免重锚；门
  191【V3-7】= 第 72 次 + 门 192【V3-1
  Phase A 门框】= 第 73 次 + 门 193【V3-8 手柄】= 第 74 次 + 门 194
  【V3-1 Phase B 下界】= 第 75 次 + 门 195【V3-2 Part 1 下界内容族】
  = 第 76 次 + 门 196【V3-4 netherite 材料链】= 第 77 次 + 门 197【V3-2 Part 2 下界深化】= 第 78 次 + 门 198【V3-3 下界 mob】= 第 79 次 + 门 199【V3-5 建筑内容族】= 第 80 次 + 门 201【V3-9 demo 回放】= 第 81 次 + 门 202【实机报障阴影缩尺】= 第 82 次免重锚
  【v3.0.0 banner = 第 71 次】——存档/门框/手柄/惰性下界/下界内容/
  材料链/下界深化/下界 mob = 渲染无关零像素【下界内容 = nether-only + 惰性下界；atlas
  16×16 幂二增长像素行恒等；材料链 = 纯物品内容不在默认热栏；下界深化 = nether-scoped 全维度门；下界 mob = 下界池 + atlas 空格零移位；建筑内容 = 水底生成不可见 + 生成内容不入 diff；demo 回放 = 三旗 absent 位级恒等（tap 判空分支 + demo 批零构造）；阴影缩尺 = 锚无 mob 从不渲染**）；PITS
  375-379 + **PITS 380**（spawn 成功行无 'pick' / 门场景洞穴密封 /
  CAVE config seed 归属 / bob diff 带色相审计）+ **PITS 381**（热栏
  key→RMB ≥0.15s / 活体掉落物品不可作终档断言 / 羊静止窗）+
  **PITS 382**（确定性音效 LCG 逐采样重建 = 静音/退化 wav 缺陷族
  13 件【音频门盲区】）+ **PITS 383**（stateful 块场景 journal 须
  states 条目【init 拒 = 输入静默丢失】/ cell DDA 半块全遮挡 =
  合并不可达【partial-aware 射线修】/ 临时插桩定界后移除重建）+
  **PITS 384**（e+1 索引越界【sign 数组 (e+1)/2 纪律】/ e 向 = 背离
  网格单元于顶点 + 面表 edgeA/edgeB 轴错配双实现交叉钉 / 重锚
  z-fight 换面白名单【单调变暗审计豁免上限 + 逐 px 留证】/ 门窗口
  679×744「y>600 = UI 带」启发式无效【实测 HUD bbox】/ 世界坐标
  手算 7 连错）+ **PITS 385**（gzip 容器可钉【zlib 封装 MTIME/
  XFL/OS 随版本漂移 = 手写 10B 头 + raw deflate -15 + 自算
  CRC32/ISIZE，解码侧头全钉】/ 本机 zlib 1.3.2 缺
  Z_DEFAULT_MEM_LEVEL 宏【命名常量 8】/ 存档双跑 RAW 恒等适用域
  = 无火场景【fires real-dt = savenorm 族】）+ **PITS 386**（portal
  框几何三坑【点火 = 面法向相邻格方向，aim 环格朝内面非内格中心 /
  站入脚位 cell 索引 vs 格中心差 0.5【框平面 z=−2 跨 −2..−1，脚 z
  须 −1.5】/ V3-7 起门脚本读落盘档须过 gzip 魔数 = UnicodeDecodeError
  0x8b 先兆】）+ **PITS 387**（g139 插桩器锚点随调用点形态演进【Phase
  B DimCtx 参数化断 STREAMING 锚 = PITS 317 loudly-fail 按预期触发】
  → 双形态锚 + 每 perf 树用自己 commit 的插桩器重建 + 改调用点前先
  grep 全部锚点）；**PITS 388**（timeout 包装 pid ≠ 子进程 pid【_NET_WM_PID 窗搜
  全败 = 输入臂 FOCUS-FAIL——游戏直跑 tp=$!】/ 短 hold 按键注册抖动
  【1.0 s W 比值 0.823 伪影 vs 3.0 s 0.403 真值——长 hold 稀释】/ 水
  放置守卫拒玩家重叠格【aim 几何须避自身 feet 格：yaw 0 射线 x 恒等
  → 第二行 + pitch 落位】/ 性能 A/B 不得与流送负载臂并行【300f 窗
  0.444→0.806 污染弃用——独占复跑】；**PITS 389**（门 QUIET 配置 mobs.enabled = false 是静默
  纪律【生成臂须 mobs-on 变体配置】/ sed 派生门脚本 OUT 目录未换 =
  PNG 落别处假 DIFF【cmp 前先验文件存在】/ spawn 行坐标正则须含负
  号【z 可负】/ maxMobs cap 吞后续尝试【1/31 权重 mob 只在首批抽
  中——环全清 = 陆地 pick 全失败 cap 不饱和 = 决定性】/ 传送落点 =
  portal 光 11 + 原点保护圈 = 生成压制位【下界生成臂用远离落点高原
  场景】；**PITS 390**（场景手术几何三坑：① demo 区表面 y=53——地下场景
  编辑【流体格落 solid 参考】load 拒 "cell occupied"【场景室放
  y≥55 参考 air 区】/ ② 开放水源漫溢【源水扩同层邻 air，无围堰池
  32→402 格】——密封池 = 满底 + 顶周围墙环 / ③ 发货世界程序湖
  ~30 块外 fluid-settle 编辑入档【x25-30 y34 level 6-7 确定性】——
  场景断言须自格 scoped 非全量计数；**PITS 391**（demo 门脚本六坑：① UI 批绘制序——addText/
  addQuad 追加进已冲刷批次 = 顶点永不消费【独立批自含冲刷】/ ② 截图
  = swap 后读 back 落后一渲染帧既有语义【放完态退出需双渲染才可
  捕获】/ ③ 录制带 --save = 槽 spec 被退出写覆写终态 = 闭环起点漂
  移【fresh 录制】/ ④ author 输出 = 输入基名 + .dmo / ⑤ g139 标记
  锚 gameplay 循环——新循环（回放）perf 树手工镜像 / ⑥ 暂停态 +
  --frames = 设计不退出【headless 退出仅 m_demoFrame ≥ total】；**PITS 392**（门日志 grep 假阴性：`mob spawn f` 等事件行 =
  MC_LOG_DEBUG——info 级跑 grep 计数恒 0 ≠ 事件未发生；事件计数
  断言须 --log-level debug 跑，或截图 crop 目检互补）；
  门计数 195（**V3-2/Phase C Part 1 下界内容族 v3.5.0** 门 195 全
  PASS：**D9 带修订 = 下界可航行**【编排者布局裁决：v3.4.0 实心岩板
  埋传送落点实证不可探索 → MC 下界废土形态 = 熔岩海 5-31 + netherrack
  台地 32-52【群系表面 = 台地顶】+ 空气空间 53-122 + 天花板 glowstone
  122 + 基岩顶 123-127；世界生成演进先例】+ 5 群系独立池【biomes_
  nether/，MC 比例 36/22/17/16/9，netherBiomeAt 克隆 + 独立盐，装饰
  门控数据派生旗】+ 12 新块【全 stateless，ancient debris 10 = MC 50
  的 /5 缩放先例】+ 3 矿脉 lattice 8 台地 only + 5 装饰【stem/basalt
  柱自 y53 可见 + per-cell blob】+ quartz 物品 + 2 配方 + **atlas
  16×8→16×16 幂二增长零移位**【像素行恒等 = 锚免重锚实证】+ **init
  fail-fast**【门内 S6 实证发现：surface 存在性/stateful 拒收从首传
  惰性前移 buildScene】；2617/2617（+35）；S1 ≡ 锚 + 白名单 4 键；S2
  双臂【传送空腔截图 ×2 恒等 + 观感实证 / 下界采矿 quartz 入库存 +
  PITS 7 续破 + PITS 377 磁力边缘实证】；S6 5/5 init 点名；详见
  `docs/archive/phase-3.5.0.md`）；
  门计数 202（**实机报障阴影缩尺 v3.12.0** 门 202 全 PASS：用户
  报障"阴影比 mob 大、落体时阴影角压高处方块 = 看似按阴影判
  落"——**探针定案物理 = AABB 质心规则**【一次性单测后撤：失接地
  = 身体中心恰过边缘 0.3000 = 半宽，MC 同款；blobShadow* 仅渲染
  消费】所见 = 1.5× 阴影半宽 0.45 > 身体 0.3 的落体瞬间视觉误导；
  **用户定案物理不动只缩阴影**：kMobShadowSideScale 1.5f → 1.0f
  【身体同宽足迹，玩家第三人称阴影同收拢】1 常量 + 3 重钉；2777/
  2777；结构免疫第 82 次【S1 ≡ base19——锚无 mob 阴影从不渲染】；
  S2 前后对比【2896 px = 3 簇地面阴影带 + 单调变亮 0 违例 + 天空带
  零 diff + crop 3× 目检同场景同羊同位】；性能 A/B 同档【changes-
  Applied 逐位 + 终档 savenorm EXACT】；PITS 392【DEBUG 事件行
  grep 假阴性】；详见 `docs/archive/phase-3.12.0.md`）；
  门计数 220（**V3-12c1 大厅/聊天/N 泛化/心跳 v3.30.0** 门 220 全
PASS【V3-12 收官相位 1/2——c2 = v3.31.0 门 221 断线恢复 + format 33
players[] = 矩阵全编号完成】：**N 泛化** = kNetMaxPlayers 2→8 协议
形【FRAME 的 u8 计数走】+ accept 窗 N 定形【首客 10s 阻塞 + 3s 窗
250ms 截止重试 + WELCOME 窗后统一 flush 同 N = lockstep 前提；窗内
拒收 = REJECT 点名续窗】+ 客户端全槽体【每机模拟每玩家——player 0
= 主机体各机本机构造】；**大厅** = 第三人称名牌【world→screen 投
影，三相机常量 game_local.hpp 共享单源，本地玩家无牌 = 第一人称缺
席】+ F3 第 22 行 players【"players N: host; name..." Latin 零新字
形，test re-pin 21→22】；**聊天** = T 切换【net 下 T = 聊天，教程
跳过 solo only D12 延伸】+ 活 pump 本地面【m_chatInputActive/
m_chatBuffer + frameText 累积 + frameBackspace + Enter 发送 + ESC
取消 + SDL 文本输入仅聊天期开】+ CHAT 包 type 7【slot u8, u16 len,
≤256B 客户端→主机重广播其余 + 本地 displayChat 8 行环 300f 淡出左
下浮层】+ Latin only 零字形【latinOnly 净化工具 net_proto.hpp 共
享】+ **打字抑制**【Input 可打印域 0x20..0x7e owner 机 FrameState
捕获前本地吞——typed 'e' 开背包面板 → 面板动作关文本输入 → 消息截
断 "hello gate"→"he" 门内捕获修；ESC/Enter/Backspace/功能键存活；
on 时清 held 键防卡键】+ 表现层结构不入哈希【textLen 恒 0 预留位保
持，CHAT 独立包型】；**心跳** = 屏障 stall **min 水印** 10s 点名
移除最滞后【anyProgress 健康同伴遮死者 = 永久死锁形门内捕获修；移
除后重基准】+ 客户端 waitPacket 10s 超时点名退 D2；**E 面板中继
修** = 门内捕获的真 desync 根【isLocal 门 = 客户端忽略主机玩家的面
板边沿——主机按 E 暂停 player 0 物理客户端不暂停 f570 哈希点名；修
= E/ESC 边沿全玩家处理【边沿 = 属主机物理按键经捕获中继全机对称】
+ 窗口/声音动作 localOwner 自门【&p == localPlayer()——远程玩家面板
状态是 sim，其窗口动作不碰本机指针/文本输入/声音】】；format 32 不
变 / sim 2 不 bump canary 4 EXACT / 偏离 D21-D23【D21 聊天表现层 +
D22 名牌第三人称 Latin + D23 accept 窗 N 定形】/ 大厅 MOTD 意念并
入 V3-17 主菜单项；2974/2974（F3 行 re-pin 既有面，c1 零新增单
测——CHAT codec 门臂 E2E 执法）；结构免疫第 101 次【S1 ≡ base19 逐
字节 ×2 + savenorm EXACT——聊天/名牌/F3 行/打字抑制全冷】+ canary 4
EXACT【E 修/打字抑制触碰 demo 路径【tutorial 有 E 操作】位级实证】
+ mobs-on 900f EXACT；**3P 锁步臂 ×4 批次**【host + 2 clients N=3
600f：三进程 net hash f0/f300 **跨批逐字恒等** 5894…/5b4f…【4 独立
批次同 digest = lockstep 最强形】+ 主机档 savenorm 跨批 EXACT + 零
desync；截图 RAW ×2 各进程至少一批全干净——Mesa 无头前缓冲读回竞
态纯彩块垃圾帧 = 进程轮换环境面 PITS 411⑥，权威 = 哈希 + savenorm】
+ **大厅截图臂**【--script 确定性视角：LMB 捕获 + mouse_move dx620
= yaw -88.8° 存档实证正对 +X 远程玩家 + C 第三人称 + F3，×2 RAW +
EXACT；OCR 实证名牌 "client" + F3 "players 2: host; client" +
"ThirdBack D 4"】+ **聊天臂**【xdotool T+type+Return 实打：双进程
日志 net chat[host]: hello gate 逐字 + 双截图 OCR 浮层 + 零 desync
+ 尾帧容错双 rc 0】；S6 八面复验【sim=3 假 HELLO / magic 破 / 变异
资产 / walk_speed 9.9 config / width 1024 window / --connect×
--save / --listen×demo / 端口占用 全 rc 1 点名】+ accept 窗 2 新面
【2 客窗内 → N=3 "slot 1 of 3"+"slot 2 of 3" / 单客 + 窗 3.01s 超
时 → N=2 续跑】；性能 A/B 同档【旧 = 1f2e10f v3.29.0 + g139 双树：banner 实证 v3.29.0/v3.30.0 + OW 900f 无输入：PERF-WIN 三窗 upd med 0.426/0.444/0.426 vs 0.399/0.402/0.497 同档 + render 0.495/0.480/0.433 vs 0.464/0.456/0.507 同档 + swap 15.6 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm EXACT 零 format 最强形 = 大厅/聊天/N 泛化/打字抑制的 solo 路径（净面全冷 + 抑制旗 solo 恒 false）结构性零成本；净自成本（新树 mobs-off 600f）：solo upd med 0.472/0.424 vs 2P 主机 0.687/0.729 vs 3P 主机 0.736/0.402 = +~0.2-0.26ms/帧（per-player sim pass + per-remote 轮询/屏障 + socket，sub-ms 同档），render 0.452/0.525/0.561 同档 + swap med 15.5/14.9/14.6 同档【net 跑 swap max ~30ms = 屏障等待尖峰预期】+ 零 desync】；PITS 411【九
坑：accept 窗 250ms 单次静默即 break【c2 ~1s 后连接 refused 实证——
静默尝试须截止前重试】/ 首客 accept 超时 ms/s 混用【"no client in 10
ms" 实证——超时常量单位入名】/ 客户端 netAddPlayer(1) 硬编码【slot 2
缺体 SIGSEGV + f0 哈希 desync 孪生——人数泛化逐处 grep 硬编码
slot】/ E 面板 isLocal 门 = 主机玩家面板 desync【f570 实证——边沿全
玩家 + 窗口动作 localOwner 自门】/ typed 'e' 开面板截断消息【"hello
gate"→"he" 实证——文本输入期一切键边沿在捕获前裁决】/ Mesa 无头前
缓冲读回竞态【纯彩块垃圾帧进程轮换——权威 = 哈希 + savenorm】/ WM
平铺几何噪声【门臂 config resizable=false 钉 1280×720，D19 截图延
伸】/ 尾帧 send vs 对端退出竞态【broken pipe = named harmless——
finalFrame 容错】/ 屏障 anyProgress 死锁形【健康同伴遮死者——min
水印基准】】；详见 `docs/archive/phase-3.30.0.md`）；
  门计数 234（**V3-17b 主菜单：设置页 v3.44.0** 门 234 全 PASS【V3-17
  相位 B/4——BACKLOG 规格 = config 表单化编辑 view_distance/
  day_length/天气/声音/ui.scale/窗口 + 严格读 + 写回 config.json】：
  **8 行表单**【视图距离 4..64 ±4 / 白天长度 60..86400s ±60 / 天气
  toggle / 声音 0..10/10 ±1 / 界面缩放 1.0..4.0x ±0.5 / 全屏 toggle /
  窗口宽高 ±64】+ 保存钮 + 「部分项重启后生效」hint——**零文本输入**
  【数值全 stepper/toggle，SDL 文本输入面不碰】+ ESC 返回 / Enter 保存
  便利；纯布局 + 状态机 = menu_layout.hpp test 孪生（gate xdotool 点击
  同源公式——控件 id = row*2 minus/toggle、row*2+1 plus【toggle 行
  dead】、保存 32）；**ui.scale = 唯一运行时生效面**【m_uiScale 每帧
  uiSize() 读——点击 + 后菜单布局实时 reflow = 活体证明；世界启动
  buildScene 重读 config 同值；其余行 = 世界启动/下次启动生效——
  streamRadius/帧锁定步长/Window::init only 记录】；**写回 = 既存未用
  的 Config::save**（config.cpp:57——保存钮 confirm 后整档 dump：
  Json::Object = std::map → 键序字母归一 + 2-space 缩进 = 记录偏离，
  **值全保**【未动键原样存活——gate 断言 view_distance 80 / day_length
  600 / seed / window 尺寸 / player 节全在】；整值出整数 80 非 80.0 +
  非整最短往返 0.7 非 0.700000 = Json::dump 现形）；**字体 +3 屏宽项**
  【设置页词表核证缺口——全屏/窗口宽/部分项重启后生效】= 代码点序插入
  + 后缀 +1..+3 重格，**atlas 2208 不增长**【69 行 headroom 承接：
  max cell 551 = row 68——457 表】= 归一化 v-UV 不变 → **门 233 跨树
  文本边缘白名单不复发（PITS 425④ 一次性门面验证）**；FNV re-pin
  6b4fde4b271b467e + test_font 全 re-pin；3012/3012（+2 MenuSettings
  Test：Layout 8 面 hit【含 toggle plus dead】/ StepActions 11 面；
  banner re-pin 1）；**S1 base20 重锚**【f60 无输入 = 标题 ×2 RAW 逐字
  节 + 与旧锚 diff = 95 px 单行 rows 164-168 = 版本行 v3.43.0→v3.44.0
  有界断言 → base20 = v3.44.0 标题 21917 B；**PITS 426②：菜单标题含
  版本串 = 菜单期 S1 逐版重锚**（≠ 游戏期结构免疫免重锚——标题是像
  素活面）】+ **设置臂**【xdotool 设置 → 天气 开→关 → 界面缩放 1.5→
  2.0【LIVE reflow——点击序 = 布局稳定形：reflow 控件在 @1.5 几何点击
  中最后】→ 保存 @2.0 几何 (340,640)：`settings saved to` + 写回断言
  PASS + **a3 臂**【修改后 config 标题 ×2 RAW EXACT @ scale 2.0】】+
  canary 4×2 新≡旧（tm_v343 v3.43.0）【设置页只在点击后画 = 结构免
  疫：树内 RAW r1==r2 EXACT ×4 + savenorm 跨树 EXACT ×4 + **跨树 RAW
  EXACT ×4**（同字体 = v-UV 不变）】+ 性能 A/B 同档【OW 900f：new upd
  med 0.589/0.558/0.604 vs old 0.552/0.606/0.602 median 交错入带 +
  render/swap 同档 + changesApplied 9/3/1 逐位 + **终档跨树 savenorm
  EXACT** = 设置页零世界面结构零成本】；**门内捕获 PITS 426 两坑**
  【①value 列中心 = 标签右缘与按钮左缘中点【初版 (w-150)/2 与标签叠
  字——截图冒烟捕获】②标题版本串 = S1 逐版重锚】；详见
  `docs/archive/phase-3.44.0.md`）；
  门计数 233（**V3-17a 主菜单系统：标题页 + 教学入口 + 字体 +21 v3.43.0**
  门 233 全 PASS【11-07 用户实机诉求首相位；V3-17 拆 4 相位 17a-d，本相 =
  菜单架构 + 标题页 + 3 stub，**表现层零模拟**】：**第 4 循环** =
  runMenuLoop（window+renderer+buildUiAssets 后、buildScene 前——
  menuEnabled 五形绕开【demo/net/script/--new/管理旗 = 结构免疫根】）+
  renderMenu【clear kMenuClear 0.04/0.05/0.08 + UiBatch only——禁
  render/renderUi 世界未建；调色板/原语 = renderUi 文件局部 lambda 的
  拷贝，renderUi 原文零触碰 = 游戏面位级根】；**标题页** = logo
  「TokenMine.cpp」scale 2.0 + 版本行 + 5 项单人/多人/设置/教学/退出
  【hover 白条 + click/Enter 激活 + ESC 返回/退出；零新 GameKey】；
  **单人 = 直接 fresh world-start**【--new 形：config 种子、存档不触
  碰、m_args.newWorld = true + buildScene——m_frameCount 自世界首帧 0
  起 = 直接 --new 位级孪生】；多人/设置 = stub「the later face」
  （17b/17d）；**教学 stub = kDemoCategories 5 章标题渲染**（两级
  demo 列表 + mode 过滤 + 需联机 badge = 17c）；**buildUiAssets =
  buildScene 头提取幂等**【ui shader + font 纹理 + m_uiScale/m_fontScale
  ——菜单 init 建一次，单人启动 buildScene 重入跳过 = GL 资源单加载】
  + run() 分支【菜单退出 = save 写跳过 + 无 loop；单人 = buildScene +
  runGameplayLoop】；**字体 +21**【人设教学返主距气声缩窗联创继第大
  维远征历协——菜单五页 + 5 章标题全字核证】= 代码点序插入 + 后缀
  +1..+21 重格【侦察「尾部追加」否决：glyphUv = lower_bound 有序表——
  v3.15.0 于 / v3.14b 九重格机制先例】+ atlas 2112→2208【region
  V3-10d 起 528/528 满——font.hpp 预定「next addition must grow」形】
  + **top-512 红线 131072 px 逐字节恒等**【生成器自检 + 区哈希
  74b5d1418347dab6 不变】+ font.png FNV re-pin 9d0c9e32976679ff +
  test_font 全 re-pin 32/32【锚 10 + 向量 56 + 新增 3 成员钉 主147/
  学256/远498 + 尺寸 2208】；assetFNV 变 = 混版 net 互拒（预期记档）；
  **S1 重锚 base20**【f60 无输入 = 标题菜单 21900 B @ 679×744 锚几何
  ——论证 = 菜单无输入渲染纯函数【17a 全静态无墙钟入像素，fps_cap 只
  pacing】×2 RAW 逐字节恒等 + **defined mouse face**（S1 两跑各
  xdotool warp (10,10) 全按钮外 no-hover——hover 是首个鼠标位置入像素
  的无输入锚面 PITS 425②）+ **槽不创建断言 PASS**【菜单退出 = 世界
  未建 = --save 槽不触碰——新 S1 面】】；**click 臂**【xdotool 单人
  中心（UI (227,214) = 窗口 (340,321)，layout 纯函数同源 test 钉）→
  600-f 无输入退出存档 vs 直接 --new 600-f → **savenorm EXACT** =
  菜单零模拟贡献最强形】；3010/3010（+5 菜单族：TitleGeometryAtGate
  Space / StubGeometry / TitleActions 11 面 / StubBack 4 面 / TitleLabe
  lsAndChapters 词表 + 5 章单源；banner re-pin 1）；canary 4×2 新≡旧
  二进制（tm_v342 v3.42.0）【demo 绕菜单 = 结构免疫实证：树内 RAW
  r1==r2 EXACT ×4 + savenorm 跨树 EXACT ×4 = 零世界差；跨树 RAW =
  **字体 v-UV 重缩放文本边缘白名单**【92-118 px = 0.010-0.013%——
  atlas 长度变 = 全部字形归一化 v（y/高度）重缩放 → 线性过滤亚 texel
  偏移 → 文本抗锯齿边缘差【finished 面板/toast/clock/badge】；逐 px
  审计 100% 文本边缘【邻域 text-like 灰白】+ 零世界像素 + 心行/图标 0
  差 + diffmap 留证；17b 起两树同 2208 字体 = 恢复 EXACT】】；性能 A/B
  同档【旧 = tm_v342 v3.42.0 + g139 双树两轮：round1 new upd med
  0.361/0.513/0.573 vs old 0.389/0.396/0.349（new 轮内升 = 机载漂移
  loadavg 2.1）+ control 轮 0.579/0.611/0.586 vs 0.427/0.614/0.605
  （median 交错入带）+ render/swap 同档 + **changesApplied 9/3/1 逐位
  ×2** + **终档跨树 savenorm EXACT**（control 轮）= 结构零成本——菜单
  = 第 4 循环 world pass 位级不变】；**门内捕获 PITS 425 四坑**【①
  beginFrame viewport = DRAWABLE【renderMenu 初版用 UI 空间 453×496 =
  glViewport 压缩左上角——render() 双尺寸分工】②hover 鼠标位置依赖面
  = S1 锚须 mouse-warp 纪律 ③字体尾部追加否决 = lower_bound 有序不变
  量 + 侦察 cell 号混淆 ④atlas 长度变 = v-UV 重缩放跨二进制文本边缘
  差】；详见 `docs/archive/phase-3.43.0.md`）；
  门计数 232（**V3-18 存档扩展名 v3.42.0** 门 232 全 PASS【11-08 用户
  诉求「.json 扩展名体现不出压缩后的 json，与没压缩的混淆」；**零
  format**【容器 = V3-7 确定性 gzip 不变】/ **零像素**【纯 IO】/ 零
  config】：**路径族单源** = slotWritePath/slotReadPath（save_slot.hpp
  公开）——新写形 `<slot>.json.gz` + 读**两级探测**（.json.gz 先、
  legacy .json 后 = **零迁移**——legacy 槽可读，下次写自动换名温和
  迁移，V3-7 魔数探测同款先例）；Game::saveWritePath/saveReadPath
  委托（writeSave/autosave/exit 共享写恒新形，tryLoadSave + demo
  save-spec 检查走探测）；**SAVE-3 管理面双扩展名**【listSaveSlots
  两形枚举 std::set 去重（.json.gz 赢摘要）/ readSaveSlotSummary
  探测读（缺失 = "no such save slot" 探测先行点名）/ copySaveSlot
  探测读 + **新形写**【复制即 legacy 槽名迁移】/ renameSaveSlot +
  deleteSaveSlot **消费全槽**【双形皆存在删孪生】】；**demo scene
  资产随改**【assets/demos/scenes/ 11 个 committed 资产 git mv →
  .json.gz + mk_scene.py 输出新形 + demoScenePath = 新形 +
  demoSceneReadPath(name, dir = "assets/demos/scenes") 两级探测
  参数化【dir 面 = 测试 scratch】+ init 交叉检（loadDemoIndexChecked）
  走探测】；文案 --help --save 行 + CONTENT_GUIDE_CONFIG /
  ARCHITECTURE（目录树 + 存档节）/ README 存档槽行；gate_compare.py
  已魔数探测零改动；3005/3005（+6：SaveSlotTest 5【写形/两级探测/
  双形枚举去重/复制重命名迁移含全槽消费/删除全槽】+ V316aDemo
  ScenePathTwoStageFind + 2 既有断言 re-pin【缺失槽点名 = 探测先行 /
  demoScenePath 后缀】）；结构免疫第 113 次【S1 ≡ base19 逐字节 ×2 +
  savenorm EXACT——纯 IO 零像素；**门内捕获 PITS 424④**：WM 平铺漂移
  679×744→1280×720 = S1 几何破锚【IHDR 宽字段 DIFF 内容全同】= S1 门
  config 直接请求锚几何 679×744（config_s1.json 换版，不再赌 WM
  心情）S1 ≡ base19 恢复】+ canary 4×2 新≡旧二进制（tm_v341
  v3.41.0）RAW+savenorm EXACT + **迁移臂**【新写 .json.gz 5336 B
  payload → 764 B on disk / legacy .json 名可读 / 干净退出写回双形
  并存 list 去重 / copy 新形 / delete 全槽 / --list-saves 双扩展名】+
  **scene 臂**【guide_mining 11387f 自 pilot_mine.json.gz 回放 +
  --list-demos 交叉检 OK 10 scene 全 .json.gz】+ 性能 A/B 同档【clean
  轮 win1/2 ±0.003 恒等 + win3 new ≤ old + changesApplied 9/3/1 逐位
  + 终档跨树 savenorm EXACT = 纯 IO 结构性零成本】；**门内捕获 PITS
  424 四坑**【①路径族 = 参数化探测单源【scene 交叉检初版硬编码
  .json 后缀 = 11 资产重命名后全索引假拒 + 测试 scratch 面恒假拒两
  测试红】②双形槽消费语义【copy 新形写 / rename+delete 全槽删孪生
  = 否则残留旧形污染 list】③list 去重 .json.gz 赢 ④WM 平铺漂移 = S1
  几何破锚】；详见 `docs/archive/phase-3.42.0.md`）；
  门计数 231（**V3-19 死亡屏计时 v3.41.0** 门 231 全 PASS【11-08 用户报
  障「死亡界面存活时间不停增加」；**表现层 3 点修**——零 format / 零
  config / 零新资产】：根因 = 死亡屏「存活」行活读 m_frameCount
  （game_ui.cpp:2042）——死亡后世界模拟照跑帧继续推进数字一直涨，其余
  四行（采挖/放置/击杀 = 会话计数器、行走 = 物理冻结）天然静止唯此行
  脱缰；修 = ① PlayerSession + `int deathFrame = -1`【会话态不存档
  = 死亡屏会话面 V3-13 同款偏离】② alive→dead 转移块（SURV-7
  deathCount++ 旁）`s.deathFrame = m_frameCount` ③ 死亡屏读改快照
  `Player::survivalStatsLine(deathFrame, frameCount)`【**纯函数** =
  deathCauseLine 的孪生——cause 行读死亡的 DAMAGE INFO，此行读死亡的
  FRAME；**-1 回落 = 旧活读位级红线**】；**死亡屏两帧臂（新断言，
  BACKLOG 规格）**：8×8 平台 + 玩家 y84 悬空（fall 30 → dmg 27 > 20
  无输入无 mob 确定性坠死 @ f≈97，base 30-f 场景）+ day_length 1e7
  冻日——**新树** --frames 254 vs 434 存活行 crop（y468-498 实测
  bbox）**AE=0 逐像素冻结**（两帧同文「存活 0 分 1 秒」）vs **旧树
  对照（TDD 前置复现）** tm_v340 同臂 **AE 43/42 px**（活读爬升
  「4 秒→7 秒→10 秒」秒位字形差 = 修复前行为实证——既有 ×2 RAW 同
  帧协议抓不到跨帧漂移，本臂专治）；2999/2999（+1
  SurvivalLineIsDeathFrameSnapshot：快照帧独立 + 回落活读 + 整式边界
  3600f = 1 分 0 秒）；结构免疫第 112 次【S1 ≡ base19 逐字节 ×2 +
  savenorm EXACT——死亡屏只在死时画锚场景无死亡零像素】+ canary 4×2
  新≡旧二进制（tm_v340 v3.40.0）RAW+savenorm EXACT【无死亡 = 存活行
  冷路径】+ 性能 A/B 同档【表现层零自成本】；**门内捕获 PITS 423 两
  坑**【①死亡屏行 y 名义位（h/2+74）≠ 渲染 bbox（实际 top ≈ h/2+108
  = 标题 scale-2.0 的 32-px 行步下推——按名义位 crop 裁到「行走」恒
  0 行 = 假 EXACT 新旧全同文修复不可见；crop 带先截图目测定 bbox
  再裁，PITS 384⑤ 续篇）②两帧臂 = 日冻结配置 day_length 1e7（太阳
  <0.001° 位移 = 0.55 黑罩后背景 0-diff，crop 可严格 EXACT）】；详见
  `docs/archive/phase-3.41.0.md`）；
  门计数 230（**V3-16h② 扩展指导 demo 族 ②打末影龙 v3.40.0** 门 230 全
  PASS【V3-16 相位 H/7 收官支 = **10 主题全完成**，16h 拆两版①下界门
  /②末影龙；**零内核**——末地嵌眼/travelEnd/龙 boss/水晶/珍珠/爆炸/
  netherite 甲全既有 V3-10a-d + V3-10b + SURV-1 基建，16a 的
  scene/panel_click/索引全复用】：**guide_dragon 15214f 04:13**【scene
  = 12×12 平台 x0..11 z0..11 顶 y54 正午 SURVIVAL **mobs ON** + 预置
  ender_eye 12 (h0) + ender_pearl 1 (h1) + **netherite 甲 4 (h2-h5)**
  【RMB 即穿 SURV-1 held-piece 链无面板，20 AP = 80% 减伤 enderman
  6→1.2/击 16-hit budget】+ **end_portal_frame 5×5 环 12 格** center
  (6,6) 未嵌眼【frame survival 不可得 destroyable-false 无 recipe = 预
  置，nether obsidian 帧同款；4 角 = 缺口入 center 通道】；tour = P0
  → **装备 4** → **嵌 12 眼**【select h0 先【gd9 捕获：s2a 后 selection
  = h5 忘切 = 0 嵌眼】+ 入 center + 11 格任意序 + **GOOD cell (6,4) 末
  位**【PITS 421】→ 同帧 "End portal activated at (6, 54, 6)"】→ 站入
  60-f 传送【"Traveled to end at (40, 53, 0)" 1:1 落 entry platform
  (40.5,53,0.5) 朝西；**dragon --dragon-damage 200 同帧杀 f1442** +
  egg (0,54,0) + 23 orb 200XP + 240-f ascension 仪式】→ **珍珠 28°
  弧西掷 40-m**【flat 仅 12-m【gd7】，28° = v²sin2θ/g + 高度 = 40-m
  全程一掷【gd13】；z-0.6 aim 绕行【gd10 捕获：z0.5 弧穿 (2,54,0) 晶
  盒 = shatter_on_arrow 全 projectile 扫掠吞珍珠】；throw timing 相对
  流送钉【gd17 捕获：早 40-f = 穿未载 origin chunk 244-f 虚空飞
  PITS 408⑤】→ "pearl teleport to (1.89, 54.69, -4.30)" 2.5-m offset
  落 frame 顶 + 跌落复锚 y53】→ **入 center**【step-up frame (1,53,-2)
  + step-down 入 3×3 中心【出口门环 = 1-block 实心墙 PITS 422⑧】→
  (0.61,53,1.37) honest-landing pin【gd15 捕获：step 漂移 0.49-m】】→
  **破 2 出口水晶**【(2,54,0) f1960 + (-2,54,0) f2019 bare-hand
  hardness-0 瞬破 explode r3 爆坑 28 格 endstone + frame destroyable-
  false 存活；**egg 完好**【gd14 捕获：center 站位眼位入 egg box =
  LMB ray t0 挖蛋 hardness-0 瞬破 + 3600-f 重生——站位 (1,1) 眼位离
  egg box ≥1-m 解】；tower 晶 (±50,59,±50) off-route CUT 偏离记档】
  → 观蛋 + 11/23 orb 磁吸【xp 92；**12 早落 orb 穿未载 chunk 沉基岩
  洋 y5 持久 end_xp_entities** = streaming 时序产物记档偏离】→ 站
  center 等 900-f 共享冷却 → **返程 exit portal**【"Traveled to
  overworld (end exit) at (0, 54, 0) from (0, 53, 1)" 落 spawnPoint
  (0.5,54,0.5) 平台 NW 角】→ 观点亮门【lit 9/9 + eyed 12/12】收束；
  终态钉 overworld (0.5,54,0.5) **hp20**【0-5 击 1.2/击 甲内 + 主世界
  自然回血满】/ xp 92 / hunger 8 / **netherite 甲 4 件各 6.5 损** /
  inv 空【12 眼 + 1 珍珠消耗】/ end 段 egg 完好 + 水晶 0 + 32 air 编
  辑【爆坑 28 + enderman carry 4】+ 12 orb y5 / time 0.6726；**--
  dragon-damage 200 = 必带 CLI seam**【龙战不可手编 = 耦合移动目标，
  10d1 同款；无 flag 回放龙不杀 = demo 不完整记档】】+ **门内捕获
  PITS 422 八坑**【①传送后视图 re-anchor = yaw + 位置【pitch 已同步勿
  覆写——gd5 yaw stale 珍珠飞东南死 + gd6 pitch 覆写 -18° 珍珠 4-m】
  ②珍珠射程仰角主导【flat 12-m / 28° 40-m】③珍珠穿晶 = shatter 吞
  【gd10】④egg box = LMB ray t0 陷阱【gd14】⑤chunk 载竞 = 珍珠穿未载
  + orb 沉基岩洋【gd17】⑥step 漂移 0.5-m【gd15】⑦rim 大 yaw 多 look
  模型漂 = 破晶全 miss + journal 无晶编辑假 "crystals 0"【gd16/18】
  ⑧出口门环 = 1-block 实心墙】；2998/2998【banner re-pin 1，内容相
  位零新增】+ S1 第 111 次【f60 ×2 ≡ base19 逐字节 + savenorm EXACT】
  + canary 4×2 新≡旧二进制（tm_v339 v3.39.0）RAW+savenorm EXACT +
  guide_dragon ×2 RAW+savenorm EXACT【15214f mobs-on + --dragon-
  damage 臂全帧确定性】+ 性能 A/B 同档【OW 900f 三窗 upd med
  0.523/0.576/0.565 vs 0.527/0.583/0.557 + swap 15.25-15.38 同档 +
  终档跨树 savenorm EXACT = 结构性零成本】；详见
  `docs/archive/phase-3.40.0.md`）；
  门计数 229（**V3-16h① 扩展指导 demo 族 ①下界门 v3.39.0** 门 229 全
  PASS【V3-16 相位 H/7 = dimension 章节首支，16h 拆两版①下界门/②末影
  龙；**零内核**——下界传送门/8:1 映射/下界 worldgen/flint_and_steel
  点火全既有 V3-1 Phase B 基建，16a 的 scene/panel_click/索引全复用】：
  **guide_nether 10865f 03:01**【scene = 12×11 平台 x0..11 z-8..3 顶
  y54 正午 SURVIVAL + 预置 flint_and_steel h0 + **obsidian 4×5 传送门
  框**【14 ring 格平面 z=-4，x∈[4,7] y∈[54,58] 含角未点燃——obsidian
  挖掘 15-s/格 = 帧预置 prepared-start】；tour = P0 开场 → **点火**
  【flint_and_steel aim **底行顶面** (5.5,55,-3.5) 朝内面 → 命中面法向
  邻格 (5,55,-4) = interior → 同帧 "Portal activated at (5,54,-4)
  (4x5)"】→ **站入 + 传送**【walk 入 portal z-3.5，60-f AABB 重叠
  倒计时 "Portal entry: travel countdown armed" → "Traveled to nether
  at (1,55,-1) from (5,55,-4)" 8:1 映射落生成帧底中心 (1,55,-0.5)】→
  **下 frame** 到 nether 地面 y53【walk +z 4-m，2-m 落无坠伤】→ **观
  下界**【walk 回路 E/S/W/N + C 第三人称 sweep ×3：玄武岩柱 + 熔岩海 +
  无天穹红天 + 生成 portal】→ 收束（player 下界 (0.97,53,2.87) hp20
  flint_and_steel dur 63）；**返程不做**【下界生成 portal 返程 = 900-f
  共享冷却 + 2-m 上台帧难 + auto-re-travel 乒乓，② 走 end exit portal
  更自然，本支聚焦"去下界"】】+ **门内捕获 PITS 420 四坑**【①点火 aim
  底行顶面朝内面【外侧面 = 机制正确无反应，PITS 386① 同款】②传送 = 瞬
  移 author track 失效须手动 re-anchor【直设 g.x/y/z 到落点，无公开
  set_pos】③返程 auto-re-travel 乒乓【落点在 portal 内 → 900-f 冷却后
  auto 返程 → 再 auto 去下界，实测三振】= 下 frame 到地面规避 ④下界
  frame 悬空 2-m【生成帧 y54-58 底行 obsidian y54 顶 y55，nether 面
  y53，落点 y55 站底行非浮空】】；2998/2998【banner re-pin 1，内容相
  位零新增】+ S1 第 110 次【f60 ×2 ≡ base19 逐字节 + savenorm EXACT——
  banner = F3 文本面零像素】+ canary 4×2 新≡旧二进制（tm_v338 v3.38.0）
  RAW+savenorm EXACT + guide_nether ×2 RAW+savenorm EXACT + 性能 A/B 同
  档【OW 900f 三窗 upd/render/swap 同档 + changesApplied 逐位 + 终档跨
  树 savenorm EXACT = 零内核结构性零成本】；详见
  `docs/archive/phase-3.39.0.md`）；
  门计数 228（**V3-16g 扩展指导 demo 族 ⑩完整黑夜历险 v3.38.0** 门
  228 全 PASS【V3-16 相位 G/7 = survival 章节首支；**零内核**——弓/
  箭/火把/knockback/mob 生成/磁吸/自然回血全既有，16a 的
  scene/panel_click/索引全复用，纯内容 + 工具链增量】：**guide_night
  18301f 05:05**【scene = 40×26 平台 x0..39 z-25..-1 顶 y54 **time
  0.55 入夜** SURVIVAL **seed 7**【seedhunt 8 粒：首批 4 = sheep/
  chicken/chicken/**zombie f150 (20.5,-17.5) 唯一 hostile**，无
  skeleton【wither 箭站桩 9s 死】/无 spider【1.0-b/s 打不过 11.5-m
  列 kite 账本】】+ 预置 planks 9/coal 1/flint 2/feather 2；tour = P0
  开场 → kite NE 造 table → SE 停扎最小 combat kit【table
  (38,54,-2) + sticks + bow（P=g5 非 g4）+ arrows×1】→ **弓箭
  combat**【N 走到 (38.5,-14) 7-m，4 箭 full-draw 站射击杀 zombie
  **f3524**】→ 回 SE 造余 kit【select 非弓再开容器】→ **火把环 ×4**
  【(26,-13)/(19,-13)/(13,-13)/(19,-19)，先走到点再放】→ 黑夜巡视
  【C 第三人称 + E 面板展示——f3540 新 spawn = cow 被动，玩家 hp20
  存活至 dawn t0.058】→ 黎明 face E；终态钉 hp20 + stick 2/
  bow(dur380)/arrow 4 + 4 火把 + table + entities 0【掉落 120s 消散】】+
  **combat 力学 = 16g 核心创新**【**弓 = 短程 v²/g 11.25-m**
  【shoot_speed 15 + g 20，14-19-m 任何 ≤12-m lead 不中，v14 19-m
  4 箭全 miss 实证】+ 箭 **release 发射非 press**【aim→release 121-f
  追击推进 1.6-m，按 release 帧位置 aim】+ **hold=100 保满力**
  【hold96 1-frame 欠拉 = 99-power 19.79-dmg 0.2-hp 存活，v19
  实证】+ 弹道 lead mid-window【7-m +1.85/5.6-m +1.0/4.3-m +0.65/
  2.5-m +0.25】+ **knockback 定点 5 轮迭代**【arrowKnockback 4-b/s +
  2.5-lift，zombie 轨迹依赖哪几箭中，v15→v19 2→2→3→3→4 命中收敛，
  arrow_sim.py/aim_from_log.py 探针族】】+ **工具链增量**【gen_guide_
  night.py 场景 + build_guide_night.py beat 脚本 + arrow_sim.py/
  aim_from_log.py 弓箭弹道 aim pin 探针 + 8 段前缀验证】；2998/
  2998【banner re-pin 1，内容相位零新增】+ S1 第 109 次【f60 ×2 ≡
  base19 + banner 零像素】+ canary 4×2 新≡旧二进制（tm_v337
  v3.37.0）RAW+savenorm EXACT + guide_night ×2 RAW(169498 B)/savenorm
  EXACT + 性能 A/B 同档【OW 900f 三窗 upd med 0.575/0.582/0.586 vs
  0.562/0.583/0.579 + render/swap 同档 + changesApplied 9/3/1 vs
  9/2/1【f600 3v2 = real-dt streaming 抖动带，门 207 先例】+ 终档跨
  树 savenorm EXACT = 结构性零成本】；**门内捕获 PITS 419 八坑**
  【①弓短程 11.25-m 19-m 全 miss ②release 发射 ③hold96 欠拉 0.2-hp
  存活 ④knockback 定点迭代 ⑤持弓 RMB = 拉弓非开容器 ⑥火把先走到点
  再放 ⑦zombie 自 spawn 即追 s2 受 6 伤 ⑧inv_grid = 制作网格非背包
  + panel tap ≥25-f + bow P = g5】；详见
  `docs/archive/phase-3.38.0.md`）；
  门计数 227（**V3-16f 扩展指导 demo 族 ⑨河流 v3.37.0** 门 227 全
PASS【V3-16 相位 F/7 = build 章节第三支；**零内核**——16a 的
scene/panel_click/索引全复用，纯内容 + 工具链增量，内核零变更】：
**guide_river 19841f 05:30**【scene = 24×15 平台 x0..23 z-15..-1 顶
y54 正午 + stone 垫 y50-52 + **dirt 表层 y53**【裸手 0.75-s 挖掘面】
+ **1-deep 密封源水池** x18..21 z-13..-10 16 源【2-deep 弃用：顶层
源 resting-on-fluid 无水平传播 = 舀痕永久空洞——1-deep 痕 = 邻源
1 tick 补 level-7 稳，probe B 实证】+ campfire (10,53,-7) LIT +
SURVIVAL 预置 bucket h0 + torch h1；tour = 池巡视 → **挖河 15**【x2
顶面自 bank + x3..x9 沟内侧面带 + x10 过营火 + **x11 顶面挖**【门
内捕获：campfire 格内侧面挖邻格 = 射线首 non-air 必中 cross 块】+
x12..x17；磁吸随进随收】→ **舀水 1**【池远角源 (19,53,-13)——
level-7 痕不可舀（isSource = level==8 严格），第二舀必换格】→
**倒水 W** (2,53,-7)【站位 y54 穿越点落格盘算 PITS 418⑭ + re-anchor
pin (1.58,-7.0)】→ 观西梯度【8..1 铺 8 格，营火邻格 x9 湿 = T0+21
tick，**灭 = T0+22 帧**】→ 舀水 2 (20,53,-13) → 倒水 E (17,53,-7)
→ 观东梯度【2..8】→ **火把置沟北岸 (14,54,-5)**【水火对比：营火灭
vs 火把亮——MC 偏离记档；站位 (14.5,-3.5) AABB 零重叠盘算】→
**游泳横渡**【step-in 15-f + 14-m 252-f + space 25-f 升 + step-out
30-f——probe v3 pin，实测落点 (0.65,54,-6.16)，游后诚实 re-anchor
（v3.21 纪律）】→ E 面板展示 15 泥土 → C 第三人称双线巡视【终点
21.5 = 东缘 2.3-m margin——门内捕获 22.5 开放路径漂移冲过平台缘
y38 hp13】→ 收束；终态钉 对称 V 沟形【(x2..x17,y53) = 8,7,6,5,
4,3,2,1,campfire_extinguished,2,3,4,5,6,7,8】+ 池 14 源 2 痕 +
dirt 15 + bucket 空 + 零实体 + 终姿 (1.36,54,-4.76) hp 20】+
**工具链增量**【mk_scene fluids/campfires 两 strict 面【幂等 10232
B】+ 分段前缀验证 14 段【≤30 用户定案】+ **行走路线 = 列/行 2-m
分块 + 末腿同轴纪律**【PITS 418⑬ 两实例：mid-coast 1-m 横向 jog =
漂移放大器——s7 倒水三轮 + 游泳入口 4-m 东腿 coast 的 at(20) settle
不足 = 入口 x +1.77-m 东冲 → 252-f 泳 pin 短停河心 x6.25 → 后续
20-m 巡视东腿冲过平台缘；修 = 入口列 x18.5 走 z 末腿同轴（coast 仅
z 向，x 列锁定）】】；**门内捕获 PITS 418 十六坑**【journal 重复块
响亮拒 / 2-deep 顶层源不传播 / 第二舀必换源格 / 沟端列长穿线挖后
卡死 / fire remaining real-dt / --demo-author 输出形 / scene 名
committed 目录 / timeline 键词表小写 / zsh $GC 空 / 游泳腿显式
hold + 相对坐标 + pos 断言带 y + 平台域 / walk_leg 显式 hold = 帧非
米 / campfire 侧面邻格挖 / mid-coast jog 漂移放大器 / y54 穿越点落
格 / 池格非实心 = 选列验整列身下实心【x18.5 列穿池落水停池心】/
**finished = 模拟冻结 dwell ≥240-f**【s8 段 F+100 存档 cascade 仅
1 tick 实证】】；2998/2998【内容相位零新增 + banner re-pin 1】+
**S1 第 108 次**【f60 ×2 ≡ base19 + banner 零像素复验】+ canary
4×2 新≡旧二进制（tm_v336 v3.36.0）RAW+savenorm EXACT + guide_river
×2 RAW/savenorm EXACT【gmA≡gmB 433812 B】+ **性能 A/B 同档**【OW
900-f 无输入三窗 upd med 0.555/0.609/0.583 vs 0.599/0.626/0.625 +
render 0.563/0.708/0.642 vs 0.620/0.706/0.683 + swap 15.32/15.23/
15.27 vs 15.23/15.18/15.18 + changesApplied 9/3/1 逐位 + 终档跨树
savenorm EXACT = 结构性零成本】；详见
`docs/archive/phase-3.37.0.md`）；
  门计数 226（**V3-16e 扩展指导 demo 族 ⑧材料 v3.36.0** 门
  226 全 PASS【V3-16 相位 E/7 = build 章节第二支；**内核零变更**——
16a 的 scene/panel_click/索引全复用，纯内容 + 工具链增量 + **内
核 1 行×10 = demo 恒锁定**【初 3 处 + canary RAW diff 捕获 7 处
【C 相机会话态 = 根】——物理锁窗 flap（失焦/OS 释放）吞注入事
件 = 5/5 复现的"随机脱轨"红鲱鱼源，demo 确定性环境无关化；12
处 relativeMouseMode() 读取审计全表，2 处有意不门控】】：**guide_
materials 25383f 07:03**【scene = 30×26 平台 x0..29 z-25..0 顶
y54 正午 + SURVIVAL【收集面——真破格时长/磁力收集/E 面板展示】+
预置 wooden_pickaxe h2 + 8580 cells；**7 biome 床 33 标本** = B1
森林【橡木 3 + 草 2】/ B2 沙漠【仙人掌 2 + 砂 4】/ B3 针叶林【云
杉 2 + 雪 3】/ B4 矿脉【石出露 4×2 depth-1 + 煤 2 铁 1】/ B5 花
田【葱绒/矢车菊/郁金香 各 2 non-solid】/ B6 羊毛【羊毛 3 + 荧石
2】；tour = B5 六花 1f tap → B4 **ore 先 stone 后**【单拍链破 3
格修复形 PITS 417⑮】→ B6 → B2 → B1 → B3 → E 面板 1020f 展示 →
C 第三人称四面巡视 → 收束；终态钉 33 全破 + 保留格（出露 x18
双层 + 树干 y55/56 层 + (9,53,-5) 雪地板 67% 部分破）+ 地板零
挖 + 零实体（33 drop 全收集）+ 库存精确 + 终姿 (15.5,-1.5)±1.0
hp 20】+ **22 轮根因迭代（v3.1-v3.22）= PITS 417 二十二坑**【ENT-7
镜像模型 / 探针协议 / 锁 flap / AABB pin 死路 / 花 tap DDA 边界 /
g139 参数 / 旧树重建纪律 / STAND-ON 踩层 / aim 面带 / tour×AABB /
探针 pos≠世界 pos / cell 范围读反 / **demo 确定性 = 物理锁读取全
门控（C 相机会话态捕获）** / 掉落 120s 消散 + 磁力纯水平 / B4
链破工具切换时序 / aim 角点 DDA 跳格 / rest-entry 短走 / 磁吸速
度模型 / 自家树干 y55 拦体带 / 分段 trim 纪律 / 角进入速度向量
模型 / 新增 leg 必过审计；分段链 11/11 全段 pos 偏差 ≤0.017m】
+ 工具链增量【demo_author _walk_dd/_walk_hold_for 全向量化 +
_coast 带位移 + 诚实落点 + walk_to 0.66m 可表达带 / 审计脚本族
【碰撞 + 33 首 solid + 磁吸 1f 模拟 + 间距分级】/ **分段前缀验
证用户定案形 ≤30 段 = 11 段**】；2998/2998【内容相位零新增 +
banner re-pin 1】+ S1 第 107 次【f60 ×2 ≡ base19 + banner 零像素
复验】+ canary 4×2【新新 RAW EXACT + **新≡旧二进制（tm_v335
v3.35.0）RAW+savenorm EXACT = 锁定修冷路径位级证明**】+
guide_materials ×2 RAW/savenorm EXACT【gmA≡gmB】+ index 6 条
boot 交叉检 OK【--list-demos：文件 + strict load + SIM 门 +
length 25383 匹配 + scene 存在】+ **性能 A/B 同档**【旧 = 00cb62a
v3.35.0 worktree + g139 双树 27 markers：banner 实证 v3.35.0/
v3.36.0 + OW 900f 无输入邻窗复跑定档 upd med f300/f600 0.407/
0.412 vs 0.386/0.387 同档 + render/swap 同档 + changesApplied
9/3/1 逐位 + **终档跨树 savenorm EXACT ×2【三方 = 结构性零成
本】**】；详见 `docs/archive/phase-3.36.0.md`）；
  门计数 225（**V3-16d 扩展指导 demo 族 ⑦皇宫 v3.35.0** 门
  225 全 PASS【V3-16 相位 D/7 = build 章节首支；**16d 拆两版用户定
  案**（v3.35.0 = ⑦皇宫 / v3.36.0 = ⑧材料）；**内核零变更**——16a
  的 scene/panel_click/索引全复用，纯内容 + 工具链增量；**用户在场
  定案简化 = 100 块（92 墙 + 8 塔）**，内圈地板 38 块 CUT【走位漂移
  0.02-0.07m/leg（fps 函数）开放路径累积 0.5-1.1m > 1-m 格 aim 容差
  0.5m + 预计算输入无法自校正 = 结构性边界，记偏离】】：**guide_
  palace 19536f 05:25**【scene = 21×18 扩台平台 x0..20 z-13..4 顶
  y54 正午 + creative flag + 8301 edits；开场第三人称 + 双层围墙
  92【t1 stone_bricks + t2 terracotta_yellow，门洞 x8/x9】+ 四角塔
  8 = 垂直-only 角循环【B 单按 fly ON → 45f 升 3.0m（脚 57.0 眼
  58.62，65f 眼 59.95 超 kReach 4.09 = 0/8 反解）→ aim t2 顶面
  2.2-3.0m → fly OFF → 0.1-m 墙线地面走到下一角】×4 + NW→NE 走线
  z-12.0 外圈北绕行【v9 z-10.5 北墙隧道 + v10 z-9.0 东墙隧道双门内
  捕获 = PITS 416⑫】+ 航拍展示 2 + 四面巡视 SE 落地即出】+ **工具
  链增量**【demo_author FLY 7.12 实测 + walk_leg(fly) + sweep_gradual
  + fly_toggle side-car + demo_fly_patch.py dmo 后贴器【pinned gzip
  字节 parity + no-op 自校】+ mk_scene creative 透传】；飞行 toggle
  全 gamepad B 单按【creative scene = 入场飞行根因 PITS 416①，无墙
  钟窗 sim 帧确定 = 双击 flake 族结构性免疫】；终态钉 100 块 + 零
  stray + 平台零挖 + 零实体 + 终姿 (8.55,54,3.8) yaw 12.566 = 4 扫视
  对之和】；2998/2998【内容相位零新增 + banner re-pin】；S1 第 106
  次【f60 ×2 ≡ base19 225272 B + banner 零像素】+ canary 4 EXACT
  【sim 2 不 bump——B toggle 仅本 demo】+ guide_palace ×2 RAW
  【487726 B】/savenorm EXACT + **性能 A/B 同档**【旧 = 7e4a1a2
  v3.34.0 worktree + g139 双树 27 markers：banner 实证 v3.34.0/
  v3.35.0 + OW 900f 无输入三窗 upd 0.577-0.642 vs 0.636-0.714 同档
  + render 同档 + swap 15.0-15.1 同档 + changesApplied 9/3/1
  【old1 win2=2 = 旧树自身 run-to-run real-dt 域】+ **终档跨树
  savenorm EXACT ×2** = src/ 逐文件恒等仅 banner 差的结构零成本最
  强形】；PITS 416【十三坑：creative 入场飞行 + 双击墙钟窗 flake /
  飞行 7.12 vs 6.88 名义 / 脚手架 step-up 死路 / 帧停诊断 + autosave
  覆写陷阱 / scene inventory 严格对象形 / dmo 后贴字节 parity / demo
  纯注入 = 用户鼠标仅帧节奏例外 / checker stray 名称比 / 飞行 aim
  双坑【kReach 超 6cm + track 高度不同步】/ 游戏窗焦点臂杀手
  【isolate_win + grep finished】/ 走位漂移 fps 函数 / 走线墙内隧
  道双捕获【逐线三轴核】/ 集合元数核对【3-tuple vs 4-tuple】】）；
  门计数 224（**V3-16c 扩展指导 demo 族 ③完整房子+床 v3.34.0** 门
  224 全 PASS【V3-16 相位 C/7 = day1 章节 ③ 一支，**day1 ④⑤⑥③ 全 4
  支完成**；**内核零变更**——16a 的 scene/panel_click/索引全复用，纯
  内容 + 工具链增量】：**guide_house 18875f 05:14**【scene = 12×12
  平台（x0..11,z-11..1,顶 y54）+ 工作台 (1,54,-1)【**门内捕获⑥：原
  (0,54,-3) 与出生点 xz 重叠 = 静默顶 y55 全链死——移格 + 场景
  journal blocks/tables/air 三处同步**】+ 夜 0.75 + h0 planks 36/h1
  wool 3；**定案结构 = 床预制先做**【夜窗时序面：sleep f5794 ≪
  10800 余量 5000f】→ 一段墙 9 + 二段墙 9【地面顶面 aim，全版稳
  定】→ 脚手架 S1/S2 + 唯一 step-up 链 → S2 顶 middle x2 + north
  x2【elevated 眼 y57.62】→ 屋顶 walk 北行 x3-5【二段北顶面
  t=|v|】→ 南行 x5/x4【二段南顶面——y57 floor-crossing 须过
  middle 空格】+ middle x5【链 +X 面，**aim 陡化 = 捕获⑤**】→
  4-hops 下行【**W 提前释放 = 无坠漂，捕获③**】→ 落点原位脚手架
  双挖【净 0 块】→ 室内 middle x3/x4【链 +X 面，**长 leg 目标过
  补偿 = 捕获④**】→ 门缺口列南 x2/x3【middle +Z 仰面——南 x3 二段
  门缺口无顶面 = 唯一可行面，捕获②】→ 床 place_ahead (4,54,-5) +
  **Slept at the bed**【夜跳 dawn】→ 黎明巡视【四面 walk-around +
  日出】；终态钉 house 30 planks y54 9/y55 9/y56 12 零 stray + 床 +
  respawn (4.5,54.5,-4.5) + h0 3 + 零实体 + hp 20】+ **工具链增
  量**【demo_author walk_leg 尾参 y/hold【step-up 抬离 + 短跳 W 释
  放，既有 demo 零影响】+ mk_scene tables/furnaces 容器态透传】；
  2998/2998【内容相位零新增单测——init 交叉检查 4 条目全量 boot 复
  验】；S1 第 105 次【f60 ×2 ≡ base19 + banner 3.34.0 零像素复
  验】+ canary 4 EXACT【3.34.0 二进制复跑】+ guide_house ×2
  RAW/savenorm + 性能 A/B 同档【旧 = 477082c v3.33.0 worktree +
  g139 双树：OW 900f 无输入三窗 upd/render/swap ×2 跑全同档 +
  changesApplied 9/3/1 逐位 + 终档跨树 savenorm EXACT = 结构性零成
  本】；PITS 415【七坑：出生点容器重叠静默顶出 / 地面眼侧底面截断
  = 屋顶可行面谱 / 坠腿 W 漂 / 长 leg 损耗 0.4-0.5m / 屋顶 walk z
  stop 0-0.5m = 1cm 边距生死 / 容器态三处同步 / gzip 资产 Python 读
  0x8b】）；
   门计数 223（**V3-16b 扩展指导 demo 族 ⑤⑥ v3.33.0** 门 223 全
  PASS【V3-16 相位 B/7 = day1 章节 ⑤体力 + ⑥回血两支；**内核零变
  更**——16a 的 scene/panel_click/索引全复用，纯内容 + 工具链增
  量】：**⑤ guide_food 18262f 05:04**【scene guide_food：6×4 麦田
  （farmland 24 格 y53【平台挖洞——**同格 journal 重复点名拒收**的
  门内捕获②】+ wheat y54：wheat_7×8 可收 + 年轻梯度 ×16【wheat_6×5
  / wheat_4×5 / wheat_2×4 / wheat_1×2——帧锁定生长 360 拍/级 roll
  在 demo 期间视觉长熟 = 教学细节，f3000 后全熟 = 后方也硬度 0】）
  + player (0.5,54,0.5) 起姿 yaw 2.5 朝空旷侧【**门内捕获①：起姿
  朝麦田 = f0 捕获点击 hold 尾部瞬破 3 株**——5f LMB = f0 捕获
  （!locked return）+ f1-f4 locked 挖掘，硬度 0 作物瞬破；f60 无
  demo 对照跑完好 vs demo f300 缺株二分定位；16a pilot 起姿朝树 =
  硬度 2.0 无瞬破未暴露】+ hunger 10 / 空库存；timeline 249
  actions：巡视 → **逐格收割 8 株**【**1f tap = 恰 1 破**——门内
  捕获③：2f tap = 2 破帧顺破射线后方（f3046 实证 (2,-3) wheat_5
  被顺破 = wheat_1 混入库存污染合成节奏 h1 wheat_1×3】→ 周长收集
  → E 面板面包 ×5【16 麦耗 15 剩 1】→ **进食 ×2**【RMB hold 100f
  ≥ 1.6s 通道 96f，hunger 10→20 满 + sat 12→衰减 8】→ 满条巡视收
  尾；**终态钉** h0 麦 1 / h1 面包 3 / sel h1 / hp 20 / hunger 17 /
  sat 8【8 tick 衰减确定性值】/ 田 16 株全熟 / farmland 24 完好 /
  z-1 行 + z-4 x2,x3 空 / 零实体】+ **⑥ guide_regen 18344f 05:05**
  【scene guide_regen：5 格石柱 (10,54..58,-8)【**5 落差台 = fall 5
  − offset 3 = 2 伤害**的精确教学几何】+ player 柱顶 (10.5,59,-7.5)
  + hunger 14 + **h0 bread ×2**【场景预置 = guide 的 prepared start
  面】；timeline 82 actions：柱顶巡视 → **走下台**【walk_leg 1.6m
  越台缘 + 自由落体 ~50f，hp 20→18】→ 落地观察 → 进食【hunger
  14→19 sat 6】→ **回血等待**【10s 门 + 2×4s tick = hp 18→20，每
  半心耗 1 饥饿——自然回血完整教学面】→ 满条第三人称收尾；**终态
  钉** hp 20 / hunger 13 / sat 2 / h0 面包 1 / 落点 (10.5,54,-10.04)
  / 柱 5 格完好 / 零实体】+ **工具链增量**【**capture() 2f 化**
  （down f / up f+1 = 零 locked-mining 帧——场景起姿任意安全；16a
  pilot 已 author dmo 不受影响）+ 旧分段 walk_polyline 死码清除
  （引用已改名常量 = 雷）】；2998/2998【内容相位零新增单测——init
  交叉检查 3 条目全量 boot 复验】；S1 结构免疫第 104 次【f60 ×2 ≡
  base19 + savenorm EXACT + banner 3.33.0 零像素】+ canary 4 EXACT
  + **⑤⑥ ×2 RAW + savenorm EXACT** + 终态断言 + **性能 A/B 同档**
  【旧 = 75266af v3.32.0 worktree + g139 双树 27 markers：OW 900f
  无输入 PERF-WIN 三窗 upd med 0.559/0.588/0.585 vs 0.537/0.570/
  0.564 同档 + render 0.584/0.683/0.651 vs 0.585/0.666/0.624 同档 +
  swap 15.33/15.22/15.24 vs 15.34/15.24/15.31 同档 + changesApplied
  9/3/1 逐位 + 终档 savenorm EXACT = 内容相位结构性零成本；**门内
  捕获④ = perf cwd 陷阱**【`;` 后 run 用 B 的 cwd 二进制 = 假
  A/B，显式全路径重跑才入档】】；PITS 414【四坑：f0 捕获尾部挖掘
  / scene journal 同格重复 / 1f vs 2f tap 硬度 0 连锁 + 年轻麦 f3000
  全熟 / perf A/B cwd 陷阱】）；
  门计数 222（**V3-16a 扩展指导 demo 族：数据层 + scene + panel_click
  + pilot ④ v3.32.0** 门 222 全 PASS【V3-16 相位 A/7 = 10 主题 3-10
  分钟级教学 demo 族的内核 + 工具链 + 首支 pilot ④ 挖矿打造工具】：
  **scene 世界 spec 第三形**【`{type:"scene",name}` → committed 确定
  性存档资产 assets/demos/scenes/<name>.json 只读 = 观察红线结构根；
  tryLoadSave scene 分支 + replay setup 存在性点名 + main 桥接不碰
  saveSlot（--save 可组合 = 退出写回放结果）】+ **panel_click 事件**
  【几何无关格地址：Kind 尾增【record tap 永不产生 = author 独占】+
  panel/slot/button 词表严格【0..35 三面板共享 + 100..103 E 2×2 +
  110 out + 120..122 furnace + 200..208 table 3×3 + 300..323
  creative】+ 编解码/展开/script 拒收【40B 无 panel 字段】+ resolver
  = 开面板检查【panel id 0 三开态 Inventory|Table|**Furnace**——门内
  捕获②③】+ layout rect 中心【FurnaceLayout 共享格——门内捕获③】+
  真实 Input closure 合成点击 + 错误串 = 回放 ERROR 行】+ **索引数
  据层**【index.json {demos:[{category,seq,mode,tags,file,
  length_frames}]} 封闭键集 + 5 章词表 kDemoCategories【day1/build/
  dimension/survival/coop 4+1 定案】+ seq 类内连续 + init 每次 boot
  loadDemoIndexChecked 全量交叉检查【文件存在 + 完整 strict load +
  SIM 门 + length 匹配 + scene 存在】+ --list-demos 独立无头旗
  【mm:ss @1/60 + 空章 "(empty — the later face)"】】+ **工具链**
  【tools/mk_scene.py 场景手术【fresh 基线 + spec 变异 + gzip_pin 重
  序列化幂等双跑字节恒等】+ tools/demo_author.py Guide DSL【track
  状态 + capture/look_at/look_ground/place_ahead/open_ahead/
  walk_polyline 轴对齐/mine/panel/panel_collect/tap/click/select +
  at() 零 wheel 延长】+ tools/scenes/ beat 脚本】+ **pilot ④
  11387f 03:09**【scene pilot_mine【12×12 平台 y50-53 实心 + 树 6 +
  叶冠 + 矿排 z-9 煤 2 铁 1 石 8【x0 空 = 穿越口】+ 砂柱 2】+
  timeline 488 actions：砍树 5【留顶格】→挖砂 2【裸手 0.75s——门内
  捕获⑫】→E 面板木板 ×20→工作台→**相对放置**【面朝 1m 前——绝对格
  1 格漂移死根】→木棍 8+木镐→**逐格贴线挖矿 11**【行墙挡横向视线】
  →熔炉环 8→**熔砂 ×2 玻璃 10s×2**【燃料余燃至 f10846 = 终态空炉
  确定性】；终态钉 h0 煤 2/h2 铁 1/h4 镐 dmg 11【门内捕获⑭ mobs 误
  击 +2 排除】/h5 木板 9/h8 玻璃 2 + 表(5,54,-7)+炉(4,54,-7) + 零
  实体 + hunger 17 + time 0.4163】；**2998/2998（+11 V316aDemo：
  词表/展开/编解码/拒收族/script 拒/resolver 面/scene/索引拒收/交叉
  检查五面）**；S1 结构免疫第 103 次【**f60** ×2 ≡ base19 逐字节 +
  savenorm EXACT + 旧树 v3.31.0 f60 亦 ≡ base19 三方闭环——门内捕
  获⑯：base19 锚 = f60 非 f900【时钟 10:50 反解；900f = 15937px 假
  DIFF 太阳/树冠/时钟】】+ canary 4 EXACT【bump 后复验 ×2 RAW +
  savenorm——sim 2 不 bump 执法】+ **pilot ×2 RAW + savenorm EXACT**
  + 终态内容断言 + **S6 五面 rc 1 点名**【缺 dmo / length 错 / 未知
  category / scene 缺 / sim 错单测】+ **性能 A/B 同档**【旧 =
  1f36bf2 v3.31.0 worktree + g139 双树 27 markers：banner 实证 + OW
  900f 无输入 PERF-WIN 三窗 upd med 0.564/0.576/0.599 vs 0.576/
  0.581/0.590 同档 + render 0.579/0.628/0.658 vs 0.628/0.693/0.739
  在旧树 2× spread 带内【0.698/0.721/0.736 环境漂移实证】+ swap
  15.3/15.2/15.2 vs 15.2/15.2/15.1 同档 + changesApplied 9/3/1 vs
  9/2/1【win2 = real-dt 抖动带 门 207 先例】+ 终档 savenorm EXACT =
  结构性零成本】；**PITS 413【十九坑全 pilot authoring：f0 捕获 /
  关面板不重锁 / look 符号 / 硬编码帧×动态行走重叠 / 斜行子轴 4× /
  walk_polyline cursor 不同步 / REG 2.5f 标定 + stop margin / 放置
  瞄地面 / 相对放置 / 行墙挡视线 / 磁吸 (1-d/2.5)² 衰减 / 工具态破
  碎时长 / 续挖穿平台 / mobs 误击耐久 / furnace 共享格三开态 /
  base19 f60 / --frames = demo+500 / journal 同态移除 / scene 完整
  性约束】**）；
  门计数 221（**V3-12c2 断线恢复 REJOIN + format 33 players[] v3.31.0**
  门 221 全 PASS【V3-12 全 4 相位收官 = 矩阵收官项】：**format 33
  players[]**【root player + inventory/inv_grid 迁入 per-player entry，
  entry 0 = primary 镜像 applySave 主路径零改 + entries 1.. = 远程玩家；
  三面 version lie + kSavePlayerCap 8 + 序列化 fallback】+ **末地 XP
  orb 持久化**【end_xp_entities 恒写，10d1 后移复议兑现】+ **REJOIN
  快照**【save 形 WELCOME = 磁盘档 + 会话计数族 {weather 5 字段含双
  splitmix 流 / spawnCounter×3 / particleBreakCounter} + 活粒子 +
  per-player 会话计时器族——全 presence-gated on-disk 不变】+ 重连
  流程【持久 listener + 中会话非阻塞 accept + startFrame 采纳 + 重连
  者 = 新体 D25】+ **Join/Renumber CTRL 广播**【slot 重编号行随玩家
  移】+ **net 主机抑制幽灵**【载多玩家档 stale 幽灵 segfault 修】+
  feedJournal 哈希 live-diff + respawn 精确 float【≤32 int cell 0.5-y
  丢失修】+ **mob-quiet 合同 D24**【活 mob 在场 = 中会话 rejoin 点名
  拒收，全 mob 快照后移】；门内捕获八坑全 PITS 412【Json(bool) 指针
  陷阱 / legacyShape-format 矛盾 / respawn 0.5-y / slot 重编号 ring
  越界 / barrier 停滞 accept 节拍 / client renumber 无 drop / net 主机
  幽灵 segfault / feedJournal 原始 cell】；2987/2987（+13）；结构免疫
  第 102 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT + on-disk 无 net
  段】+ canary 4 EXACT【sim 2 不 bump】+ 迁移臂【32→33 值保真
  pos/yaw/34 blockEdits/inventory】+ **REJOIN 臂**【slot 2 of 3 @ frame
  366 + 三向哈希 f600 ad509c664ccdeb4b 逐字 + 零 desync】+ S6【listen×
  save 转正 + mob 拒收命名】+ 性能 A/B 同档【旧 = d316cfc v3.30.0 +
  g139 双树：OW 900f upd 在旧树噪声带内 + changesApplied 9/3/1 + net
  自成本 +~0.07ms/帧 sub-ms + 结构零成本 solo】；详见
  `docs/archive/phase-3.31.0.md`）；
  门计数 219（**V3-12b 2 人多人 PoC v3.29.0** 门 219 全 PASS【矩阵收官前最后一版 = 主机权威 lockstep 全实现，设计稿 §4 全采纳】：**net/ 新目录 ~1200 行零新依赖**【POSIX TCP 无 net 线程 + 流重组 feed/drain 缝 R16 + magic 破点名 + HELLO 四指纹【TM_VERSION/sim/assetFNV assets 树排序 FNV/simConfigHash sim 键固定表——world.seed 豁免【WELCOME 种子 = 主机权威】+ render.* 豁免【dt 广播归一】】/ WELCOME fresh = 仅 seed 零字节世界传输 / FrameState 40B【keys u64×2 + buttons 3 + mouseDx/Dy raw f32 零量化 R4 + Ax/Ay R8 面板落格 + wheel + locked R18 + textLen 恒 0】/ D=2 无预测 38-42ms / **状态哈希字段清单**【§2.3：frame/day/weather/每玩家全态 + 会话计时器 + 36 格库存 + 效果/每维实体插入序含 mob rngState/每维 journal 全集/spawnCounter×3/粒子/fires】每 30 帧双向交换 + 300f DEBUG 行 + 不符点名退出】；**player 多人化重构（双别名最小方案）**：m_players + m_localSlot + m_player raw pointer primary 别名【354 处零改动】+ PlayerSession 吸收 10 成员族 + 14 引用别名绑 session 0【~430 处零改动】+ pass 归属二分【primary 锚不动：mob 目标/生成/教程 actor 门 D12/龙/portal D9；×N 循环化全 sim 覆盖：物理【R18 锁镜】/生存【死亡计数 per-player + 声/释锁本地 D8】/效果/beacon/骑乘 dismountRideFor/珍珠冷却/交互 17 函数 For 化/拾取磁吸三拾取【最近玩家 + 逐玩家接纳】/眼接触每人 gaze/箭扫掠任一玩家/面板 E=本地玩家 R17/despawn 任一近处留/渲染 local 化 F3+HUD+死亡屏+手臂+雨盒+雾】+ per-player aim【solo 相机路径位级 / net 第一人称形 D17 门注入】+ R2 simTime 2 处【Space 双击 + 库存双击，solo 原文位级】+ R10 xpSeed 盐 kSaltPlayerXp{0, A7 首空位} + R9 出生点 i 回绕；**streaming 回退门修正**：pool.pendingCount()【本机吞吐 = 静默 desync 根】→ streamPending.size() 共享账本【盘锚经 m_player 别名自动 = primary 零改动】；**host/client 双循环**：单 runGameplayLoop + net 钩子【主机 poll + **lockstep 屏障**【等客户端 INPUT[N−D] 水印——无头主机快跑吃冷零输入 = 门内捕获 desync 根，因果链 N−D<N 无死锁】/ 客户端 waitPacket 等包阻塞模拟 + 帧号失序点名】+ 断线移除【主机 EOF/**send ECONNRESET 同形**【client 干净退出 = RST 非 FIN】→ 库存 100% 掉落 + 落乘 + 效果清 + solo 续跑 R19 / 客户端见主机 EOF = 全退 D2】+ CLI --listen/--connect/--script【demo 时间线第四消费方 = 确定性输入，× demo 族互斥 + --connect×--save 点名 R7 + --new 净主机合法形】；format 32 零 bump D13 / sim 2 不 bump canary 4 EXACT / 偏离 D1-D20【D16 净世界 fresh only【save 形 WELCOME = 12c 重连面点名】/ D17 net aim 第一人称形 / D18 远程体静态姿态【swing VBO 单实例】/ D19 window parity = config 请求几何【WM 平铺竞态豁免】/ D20 client 退出 = RST】；2974/2974（+10 NetProto 8 + NetSession 2：codec 往返/40B 布局/截断拒收/重组分裂粘包/magic 破/FNV/assetFNV 稳/simConfigHash parity【render 豁免实证】）；结构免疫第 100 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——净面全冷 m_net null + size 1】；**S2 双进程 headless 锁步三臂**【无输入 600f 无火种子 11 ×2 run：两进程 net hash f0/f300 逐字同 + 主机档 savenorm 跨 run EXACT + 各进程截图 RAW ×2 + 零 desync / 输入注入 ×2【host W 走 + A / client S 走 + LMB 挖掘 65f，mobs-on】：哈希逐字同 + **client 挖掘经中继落主机 journal [0,25,10] air = 跨进程世界收敛铁证** + 双玩家位移 / 断线 ×2【client 300f 退出 = 确定性断线】：player 1 removed f302 + 起始库存 20 件 100% 掉落 + 主机续跑 600f + 主机档跨 run EXACT】；S6 八面点名【sim=3 假 HELLO / magic 破 / 变异资产 client / walk_speed 9.9 config / width 1024 window / --connect×--save / --listen×demo / 端口占用 bind 全 rc 1 点名】；性能 A/B 同档【旧 = a8259c8 v3.28.0 + g139 双树：banner 实证 v3.28.0/v3.29.0 + OW 900f 无输入：PERF-WIN 三窗 upd med 0.524/0.560/0.581 vs 0.544/0.582/0.586 同档 + render 0.556/0.674/0.637 vs 0.564/0.666/0.656 同档 + swap 15.3 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm EXACT 零 format 最强形 = 多人化重构的 solo 路径（净面全冷 m_net null + size 1）结构性零成本；净自成本（新树 mobs-off 600f）：solo upd med 0.574/0.587 vs 2P 主机 0.642/0.638 = +~0.06ms/帧（player-2 sim pass + 环 + socket，sub-100µs 同档），render/swap 同档】；PITS 410【九坑：client 环槽 store 偏 1【收到主机帧态属帧 f−D 须存 ringSlot(f) 非 f%3——主机侧恰合客户端差 1 = 主机玩家输入晚 1 帧，f150 哈希点名抓获，纪律 = 环槽写读对称性用消费帧验证】/ 主机快跑屏障 / RST≠FIN / WM 平铺 / uiScale HELLO 时序【client 握手先于 buildScene】/ seed 豁免 parity / 无火种子扫描【fire real-dt 域 PITS 385 族】/ --new 净主机 / 起始库存 per-session【netAddPlayer 空 Inventory = 断线零掉落静默，fillStarterInventory 提取共享】；详见
  `docs/archive/phase-3.29.0.md`）；
  门计数 218（**V3-12a 多人设计定案 v3.28.0** 门 218 全 PASS
  【矩阵最后项首相位 = **纯文档版**，v3.0.0 开启版同款零代码零模
  拟面】：设计权威入仓 `docs/V3-12-MULTIPLAYER-DESIGN.md`（390 行，
  代码引用 文件:行 实证——方案 1 主机权威 lockstep / 时间源 = 主机
  广播 clamped wall dt raw f64 位 / POSIX TCP 协议 schema / Player
  双别名最小方案 / streaming 跨机 2 处修正 / 12b 范围 + 门臂定案 /
  R1-R20 / D1-D15）；编排者裁定 12b = v3.29.0 门 219（侦察门号顺
  延修正）；S1 结构免疫第 99 次【≡ base19 逐字节 ×2 + savenorm
  EXACT——banner = F3 文本面零像素，v3.0.0 banner 先例同款】/
  canary 4 ×2 RAW + EXACT【零模拟面执法】/ 性能 A/B 同档【旧 =
  3f9ae83 v3.27.0 + g139：banner 实证 + 二进制差异仅 F3 文本串——
  OW 900f PERF-WIN 三窗 upd med 0.563/0.663/0.649 vs 0.520/0.610/0.580 同档 + render 0.562/0.744/0.726 vs 0.543/0.682/0.602 + swap 15.3 同档 + changesApplied 9/3-2/1【win2 = streaming 边界 real-dt 域，门 207 先例带】+ 终档 savenorm EXACT = 零性能面实证】/
  2964/2964（banner re-pin 1 项零新增）；详见
  `docs/archive/phase-3.28.0.md`）；
  门计数 217（**V3-10d2 召唤与复杀 v3.27.0** 门 217 全 PASS【V3-10
  全 4 相位收官，矩阵仅剩 V3-12 多人】：**crystal_base 块旗**
  【strict bool 缺省冷，obsidian/bedrock/出口双帧 4 块置 true】+
  **requires_crystal_base**【end_crystal 独占】= placeBlock 数据门
  放置规则【零物种 switch——end_crystal 生存可放置，10d1 配方闭环】+
  **蛋 blast_resistance 3**【晶爆 r3 局域 1.0 < 3 存活 = MC 蛋近不
  可晶爆毁读，门内发现】；**召唤** = 4 出口面晶（kEndSummonFace-
  Slots：±X = arena 出口晶算数 + ±Z worldgen 缺席只能玩家放置）∧
  无蛋 ∧ 无龙 → 60f 倒计时（任一损失取消）→ 4 晶引爆【相邻爆波
  及 = MC 4 晶同爆读，门内实证 2 shatter 行 + 2 波及】+ 蛋计时清
  除 + 缺位塔晶回写（已载 chunk only）+ 显式 60f 复生；**败亡旗**
  【首杀置位不清除重载复位——10d1 生成门「无蛋→生成」= 首访读，
  门内捕获破蛋 60f 无召唤复生 f381 实证】；**蛋 60s 会话重生**
  【玩家破蛋 → 3600f 回写 (0,54,0)，重载复位 = D7】；**复杀 XP =
  8**【单 8 值 orb——MC 500/12000 比例 1/24 × 200 的 D5 读；首杀
  200 burst 不变】；--dragon-damage 每生成施加【单次生成行为不
  变，同跑可走全循环】；F3 第 21 行 dragon【none/spawn Nf/orbit/
  strafe/perch/ascend N/240/summon Nf/egg/egg respawn Nf——全英文
  零新字形，528/528 满区不碰 R7 规避】；format 32 不变全会话态 /
  sim 2 不 bump canary 4 EXACT / 块 135 不变 / 零 config；2964/
  2964（+5：DragonSummon 4 + DragonLine 1）；结构免疫第 98 次
  【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——召唤/放置/蛋门全
  冷】；S2 召唤+复杀臂 = authored demo【f59 杀1 + 200XP burst 23
  orb 逐位 / f321 破晶【蛋 br3 存活】/ f322 破蛋 → f3922 调度 /
  f340-373 三面晶放置 / f373 summon armed / f433 sequence complete /
  f493 杀2 + **8XP 单 orb value 8 f558** + 蛋2】×2 RAW + 逐字 +
  S2a 无输入回归（summon 行 0 = 门惰性）+ 10d1 三臂回归全同
  【kill f59 + 200XP / 破晶 f151→strafe f152 + 火球逐位 / 重载蛋
  在无复生】；S6 十六面点名【10d1 十四面复验 + crystal_base 字
  符串 + requires_crystal_base 数值 2 新面全 rc 1】；性能 A/B 同档【旧 = 94dc3d9 v3.26.0 + g139：banner 实证 v3.26.0/v3.27.0 + 双场景——OW 900f 无输入：PERF-WIN 三窗 upd med 0.561/0.562/0.571 vs 0.556/0.571/0.573 同档 + render 0.619/0.656/0.661 vs 0.630/0.697/0.638 + swap 15.3 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形；末地战斗 600f（mobs-on 变体，双侧龙在场）：upd med 0.870/1.017 vs 0.841/1.023 同档 + render 0.502/0.584 vs 0.503/0.569 + swap 15.2 同档 + ents 0.39/0.46 vs 0.36/0.47 同档 = 10d2 增量（放置门/蛋/召唤/F3 全冷）无可测自成本】；PITS 409【五坑：负 cell 中心 -N+0.5 非
  -N-0.5【10d1 kEndCrystalSlots/kEndTowerSlots 负侧偏 1 格静默——
  普查走整数 cell 词表故玩法正确，箭碎扫掠 floor(中心) -X 侧测错
  格；10d2 面晶词表单测抓获，词表修正后 10d1 全臂回归同值】/
  无蛋 ≠ 首访 / demo 同帧 down+up 死点击 + hold 续帧重新取目标
  【破晶→续帧自动破蛋 = 一 hold 两用】/ demo 旋转 dy 逐目标累计
  / 门脚本 /tmp 路径被重写】；详见
  `docs/archive/phase-3.27.0.md`）；
  门计数 216（**V3-10d1 末影龙核心 + 首杀闭环 v3.26.0** 门 216 全
  PASS【矩阵唯一真大项首相位 D1/2，龙 boss = 第 16 mob 纯数据核心零
  物种 switch】：**dragonFlight 三态**【runAi flying 分派最顶纯新分
  支 ghast/bat 位级红线：盘旋定心 30【门内裁决 40+player-pull 恒过入
  场点 → 30 移 pull】/侧移 3.5s + 火球 2.5s 冷却/驻坐出口锚 + 咆哮
  1.25s → 玩家脚位吐息云 + 三起飞 连吐 4/离 150/累计伤 >50；流契约
  每帧 ambient 1 抽 + 跨圈帧恰 2 抽【strafe 命中亦消费 u2 = 门内重构
  纯函数 rollDragonLapDecision】】+ **水晶新块非实体**【6 固定槽
  4 塔顶 + 2 出口 D3 非 MC 10；break explode_radius 3 块自身引信 +
  无连锁结构性质 + shatter_on_arrow 箭扫掠消费箭 + 治疗源晶碎反伤
  10 + 晶上永久火 D9 零 churn + **普查卸载无关**【门内捕获 raw
  getBlock 未载 chunk 读 air = 0→6 漂假碎晶】】+ **龙息云零物理新实
  体**【r0+0.15·age；leavesCloud 尾参缺省 false 箭/ghast/珍珠位级红
  线；dragon_breath 3HP/s 玩家-only 效果 11→12 + icon 12】+ **首杀
  闭环 format 32 零 bump 最强形**【m_dead 帧蛋 (0,54,0) R14 0 帧
  差【门内捕获同帧 spawn+kill 漏账 = f119 复生死循环首跑实证】+ 升
  天 60f 飞 + 180f 悬停 + 23 orb = 22×9+2 = 200 整 D5 + wail + 240f
  延迟门移除；重载 = 蛋在无复生；末地 orb/云存档静默跳过 = 偏离记档
  format 33 后移 10d2 复议】+ **血条**【renderUi 段 186×8 十段 +
  「末影龙」字体 +3 = 433【扩展区 528/528 恰好满】+ margin 8→24
  门内裁名修 + 2s 升天淡出】+ **--dragon-damage CLI seam**【--explode
  孪生——龙战斗 demo 不可手编 = 耦合移动目标定案；管理旗互斥】+ 生成
  门 playerInEnd∧mobs∧无龙∧非蛋 → 60f → (0.5,58,0.5) kSaltDragon；
  块 133→135 / mob 15→16 / atlas 174→179 零移位 / 声音 71→75 / 效果
  11→12 / 配方 113→114 / 零 config；2959/2959（+29：dragon 族 27 +
  塔晶 1 + 无连锁 1）；结构免疫第 97 次【S1 ≡ base19 逐字节 ×2 +
  savenorm EXACT——末地全链 playerInEnd 门 + 数据旗缺省冷位级根】；
  S2 四臂【a 无输入末地 600f 龙轨道 + 血条 + enderman 共存 ×2 RAW +
  EXACT / b 破晶 demo 臂 shatter f151 → strafe f152 + 火球 f152/f303
  冷却逐位 + f363 回轨 ×2 RAW + 逐字【demo 存档红线：save 参考 demo
  的 --save 被 reset = 终态权威截图 + DEBUG 行】/ c 击杀仪式臂
  --dragon-damage 200：f59 死 + 蛋 + 23 orb f124..f234 逐位 + 终档
  恰 6 键 ×2 RAW + EXACT / 重载臂 蛋在无复生 + 血条缺席 ×2 RAW】；
  S6 十四面点名【配对 ×3/域 ×3/晶 stateful/wav ×3/效果/块 ×2/atlas/
  contact 全 rc 1 点名】；canary 4 ×2 RAW + savenorm EXACT【sim 2 不
  bump——全 OW 无末地】；性能 A/B 同档【旧 = 23c4e7c v3.25.0 + g139：banner 实证 v3.25.0/v3.26.0 + 双场景——OW 900f 无输入：PERF-WIN 三窗 upd med 0.594/0.548/0.586 vs 0.576/0.574/0.583 同档 + render 0.637/0.544/0.651 vs 0.617/0.640/0.654 + swap 15.3 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形；末地战斗 600f（mobs-on 变体）：旧（无龙仅 enderman）upd med 0.525/0.687 vs 新（龙 + enderman）0.905/1.012 = boss 自成本 ~+0.4 ms upd + ~0.1 ms render（beam/云/血条），swap 15.2 同档 + changesApplied 0/0 逐位】；PITS 408
  【八坑：init 交叉检顺序 / 数值累加器泄漏 / demo 存档红线 / 流送依
  赖锚点 / -1 哨兵 delta / 同帧 spawn+kill / 血条 margin / CJK 边界】；
  详见 `docs/archive/phase-3.26.0.md`）；
  门计数 215（**V3-10c 末影人 v3.25.0** 门 215 全 PASS【V3-10
  相位 C/4，**End 首个 mob** = neutral + 瞬移 + 携格，数据全旗核心
  零物种 switch】：**注视激怒**【eye_contact_range 48 = kEndFogEnd
  「看见 = 激怒」定案 + 5 连续帧零流计数眨眼复位 + game pass 全 AABB
  瞄准射线 + 瞄准块 t 严格遮挡 → m_angry + 30s 窗复用】+ **瞬移**
  【interval 2/6 严格配对存在性 = 能力门 + 动作非状态 MobState 零新增
  + 受击 100% damage 只置旗 update 固定槽帧序钉 + 流契约 8 候选 × 3
  抽 + 1 重掷 = 25 抽/尝试全败亦消耗 + 平静 ±16/±8 自位锚 / 激怒
  ±8/±4 朝玩家 12 点锚项目缩放 + executeMobTeleport game 缝：catch-
  UpDisk → 候选须空旷 ∧ 16 格内有地面【门内重写：下探穿固体掉台地下
  虚空静默 despawn】→ fall 复锚 + 落点音 + DEBUG；全败 swallowed 行】
  + **携格 10c-lite**【carries_blocks + 块 carriable 双严格旗缺省冷 +
  stateful carriable init 点名拒 + 集 {endstone, chorus_plant} + 摸每
  帧 4×3×4 dy 含脚下 x→y→z 定序 + 首格遮挡 + 放 6000 帧全局长拍 +
  死亡 100% 掉所携 + 0.9× 头顶立方 Opaque/Cutout 双 pass】+ **End
  池**【mob_spawning_end.json 单种族 = 10a 两相位槽消费，loadEndSpawn-
  Pool 下界 load 第 3 次消费 fail-fast】+ **掉落 pearl 0-1 + eye 0-1
  = 生存唯一来源**【10a 嵌眼/10b 珍珠两条记档闭合】+ xp 5 + 声音
  70→71 仅 enderman_teleport【init 对一切瞬移 mob 交叉检 wav】；
  format 32/sim 2 不变 canary 4 EXACT【无 --demo-force】/ atlas
  172→174 零移位 / 块 133 不变 / 零 config；2930/2930（+31 enderman
  族 26 + end 池 5）；结构免疫第 96 次【S1 ≡ base19 逐字节 ×2 +
  savenorm EXACT + 恰白名单 3 键——末影人全链 playerInEnd 门 + 数据
  旗缺省冷位级根】；S2 无输入末地臂 600f【spawn 4 + provoked ×2 +
  teleported ×8 + carried ×4 = end 段恰 4 air 格 + ×2 RAW + 逐字】+
  **火杀 demo 臂**【chorus 点火 y53 脚级严格重叠【y52 火不点燃 PITS
  407②】+ 双点火覆盖战斗【burn 39 伤 < 40 HP】+ death f1016
  (37.695915, 53.306664, 3.944854) 逐字 ×2 + 所携 endstone x1 掉落
  逐字 ×2 + pearl/eye roll 0-0 合法 + teleported ×51 + ×2 RAW + crop
  目检】；S6 七面点名【池缺文件 / 池未知 mob / interval min>max /
  eye 非数 / wav 缺失 / carriable stateful / drops 未知 item 全 rc 1
  点名】；性能 A/B 同档【旧 = e2e2071 v3.24.0 + g139：banner 实证 v3.24.0/v3.25.0 + PERF-WIN 三窗 upd med 0.307/0.352/0.315 vs 0.366/0.349/0.334 + render 0.387/0.407/0.371 vs 0.455/0.433/0.379 + swap 15.8 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形】；PITS 407【七坑：demo
  引用槽永不回写 = 读 slot 假测量【误判 mouse_move 无效 2h，g213
  复跑 12 格嵌入反证】/ 火脚级 / 拾格带含脚下 / 遮挡首格语义 / 下探
  虚空 / 无头截图 679×744 非窗口 1280×720 / 按住 LMB 挖准星所指】；
  详见 `docs/archive/phase-3.25.0.md`）；
  门计数 214（**V3-10b 末地内容族 v3.24.0** 门 214 全 PASS【V3-10
  相位 B/4，10a 最小 fill 换真内容 + **项目首个投掷物**】：**基岩
  洋**【全维 y[0,4] 实心带 = 下界地板逐字镜像，非流体海零 churn；
  跌落死因 Void→Fall【措辞字体表内零字形】+ 珍珠可落洋面 = MC 逃
  生面】+ **岛群 pass**【lattice 16 p0.15 独立盐 kSaltEndIsland，
  半径 5..12 顶 y 44..56，顶 2 层实心 + 下锥 2 层悬浮岛；air-only
  谓词自保 + **中央足迹护栏** bbox 触 |x|,|z|≤60 即弃 = 出口门 +
  入场平台区零岛【R2】+ 跨 chunk 走格外扩 s+rMax+2【R1 双 chunk
  恒等单测】】+ **chorus 岛顶装饰**【独立盐 kSaltEndChorus p0.6，
  cross stateless + chance_drops ≈ MC 0-2 果 + 0-1 株；生长不做
  = 主世界 only 生长 tick 后移 10c/10d 记偏离】+ **biomes_end 单
  群系池**【end_plains surface endstone no-op 覆写；MC 无群系偏
  离 + 10d 第二群系零核心缝；空池 init 点名】+ **end_stone_
  bricks**【hardness 1.25 MC / 2×2 配方 count 4 MC 精确 / stone 音
  族】+ **末影珍珠全链**【`throws_teleport` strict bool + throw/
  teleport_sound 配对【缺一 = 死字段点名】+ **one-right-click 8 面
  互斥**【food/drink/launcher/ignite/dye/bucket/shears/armor 全点名
  】+ material-only【tool 携旗点名】+ init 交叉检【双音 + wav 存
  在性】；挂点 = RMB 链潜行旋转后 placeBlock 前【MC 可交互块优先
  】；发射 = 眼位前 0.3 + **投掷者速度并入**【MC 1.9+】+ kPearl-
  LaunchSpeed 30 b/s【MC 1.5 b/t 绝对值——箭 1/4 比例 = 7.5 仅 3
  块弃】+ 冷却 60f 会话态【窗内再点 = 静默 no-op 吞点击实证】+
  creative 不消耗；Projectile 尾参 teleports【缺省 false = 箭/火
  球位级红线】+ stuck 复用零改动 → **pearlLanding**：catchUpDisk
  先行 + **2.5 球面均匀偏移**【particle counter 流 2 抽帧锁定】+
  hasRoomAt 上探 3【portal 先例】+ restorePosition fall 复锚【落地
  消坠伤】+ 落乘【MC 1.10】+ **无自伤**【零新 DamageSource/死因/
  字体，MC 5HP 偏离】+ probe 全败吞没不瞬移【封闭洞偏离】；mob
  扫掠跳过扩门【穿 mob MC 孪生】+ 0.625× 青色发光立方渲染【火球
  mesh 复用，stuck 即瞬移从不渲染】+ 2 wav 68→70【throw whoop
  320→980 Hz / teleport shimmer 1400→240 Hz】】；format 32 不变 /
  **sim 2 不 bump**【canary 4 EXACT——全 OW 无珍珠】/ 零 config /
  块 131→133 / atlas 170→172【[0..1,11] 零移位】/ item 99→101 /
  配方 112→113 / cutout 集 +1；2899/2899（+14 EndGen 10b ×8 /
  Pearl ×6）；结构免疫第 95 次【S1 ≡ base19 逐字节 ×2 + savenorm
  EXACT + 恰白名单 3 键——10b 内容 = 生成参考零 diff 最强形】；S2a
  去程臂【10a demo 复跑：activated + Traveled to end ×2 RAW + 逐字
  + 末地新内容雾内可见】+ S2b 珍珠臂【fresh 档热栏 16 珍珠 + f60
  RMB = `threw a 'ender_pearl'` + `pearl teleport to (1.395792,
  54.893940, -1.207714)` ×2 逐字恒等 + f70 RMB 冷却窗内 = 静默
  no-op 实证 + 截图 RAW 恒等 + 心行全满无自伤 + 热栏 16→15 crop】；
  S6 六面点名【biomes_end 缺失 / 群系 surface 未知块 / chorus_plant
  缺失 / pearl 缺一音 / wav 缺失 / endstone stateful 拒】；S2b
  canary 4 demo ×2 RAW + savenorm EXACT；性能 A/B 同档【旧 =
  d0c0435 v3.23.0 + g139 三形态锚：banner 实证 v3.23.0/v3.24.0 +
  PERF-WIN 三窗 upd med 0.575/0.585/0.594 vs 0.569/0.587/0.597 +
  render/swap 同档 + changesApplied 9/2/1 全逐位 + 终档 savenorm
  EXACT 零 format 最强形】；
  PITS 406【lattice 位切 int 截断负值 = 模运算先于 int 转换【半径
  -2..12 实测单测首跑全炸】+ JS 三元误写进 Python 生成器 + run 目
  录 stale assets 三连踩 = fail-fast 按预期工作】；详见
  `docs/archive/phase-3.24.0.md`）；
  门计数 213（**V3-10a 末地维度框架 + 真传送 v3.23.0** 门 213 全
  PASS【V3-10 相位 A/4，**项目第三维度** = 下界维度基建第二次消
  费，规模 ≈ V3-1 Phase B 门 194】：**入口 A-lite**【`end_portal_
  frame`（空）/`end_portal_frame_eyed`（嵌眼）= 双 stateless 块【项
  目无 per-state 纹理机制——单块 state_count 2 否决】+ 数据谓词
  end_frame/end_frame_eyed/end_portal strict bool 配对检 + `end_
  portal` 块【nether_portal 逐字镜像 light 11/destroyable false】+
  5×5 水平环纯函数族【ring/center cells + scan + 8 候选中心反解 +
  cellValid 孤儿谓词 + activation plan】+ `ender_eye` 物品【32×32
  独立 icon + 单盒 model，item 98→99】+ **嵌眼交互**【持眼右键
  frame：吞 1 眼 + 嵌眼态 + 环扫描；12 全 eyed + 中心 3×3 air →
  激活；frame 不可破坏 = 无去激活路记偏离】】+ **最小末地**【120×
  120 末地石台地 y[48,52] + 原点出口门 worldgen 常活【10d 前偏
  离】+ 虚空无地板 + 入场平台 5×5 obsidian @(40,52,0) **每次传送
  写入**【首访 end 段 journal diff = 恰 25 格，二次幂等——风险⑦
  实证；偏离 MC (100,49,0)】+ 落点 (40.5,53,0.5) 朝西 +π/2】+
  **传送新函数**【updateEndPortalEntry/travelEnd——travelPortal
  原文零触碰位级红线；1:1 独立原点无 8:1 / 返程 = 主世界出生点床
  校验【D8 三值化】/ 60f 倒计时 + 900f 冷却复用 / 声音复用
  portal_trigger/travel】+ **format 32**【dimension 词表**按 format
  分词**：30/31 携 "end" = version lie；≥32 三词 + end 段 {blocks,
  fluids,states} + end_entities 恒写 + endSeed 不落盘 + PITS 399
  四类降级点同 commit 盘点【strip 助手 ×5 扩 end 键 + gzip identity
  29→32 rebase】】+ **渲染 6 点**【暗紫黑 clear 0.05/0.03/0.06 +
  雾 8..48 + 日月星云雨 inOverworld 门 + F3 +end + ambience 第 5
  上下文 End=4 +ambience_end.wav 67→68 + **u_ambient/u_caveAmbient
  双 floor 0.25 按维度**【OW/nether 位级不变；set 点 = init + 翻维
  帧】】；块 127→131 / atlas 166→170（[10..13,10] 零移位）/ item
  98→99 / 声音 67→68；**门内捕获 ×2**【① shader 无光分支只读
  u_caveAmbient——单设 u_ambient = 末地全黑截图 ② 场景手术丘陵埋没
  = 7×7 挖深坑平顶】；2885/2885（+29 EndGen ×10 / EndPortal ×11 /
  SaveFormat32 ×6 + census re-pin 8 文件）；结构免疫第 94 次【S1 ≡
  base19 逐字节 ×2 + savenorm EXACT + 终档恰白名单 3 键 {format,
  end, end_entities}——末地惰性最强形】；S2 去程 = authored demo【7×
  7 坑场景 + 12×【瞄准+RMB】：12 嵌眼【4 次「no complete ring」= 环
  未全时正确语义】→ f100 `End portal activated at (3, 54, 3)` →
  f175 `Traveled to end at (40, 53, 0) from (3, 54, 3)` ×2 逐字恒等
  + 截图 RAW 恒等 + 末地场景视觉 + 热栏 13→1 眼 crop】+ 返程零输入
  臂【玩家站 worldgen 出口中心：`Traveled to overworld (end exit)
  at (0, 54, 0)` ×2 + 终档 pos 出生点精确 + end 段空】+ 末地持久化
  臂【end 段 = 恰 25 obsidian 平台格集合精确】；S6 五面点名【end
  stone 缺失 / 31 携 "end" 词 / 31 携 end 段 / 32 缺 end 段 / end
  stateful 缺 states 条目】；S2b canary 4 demo ×2 RAW + savenorm
  EXACT【sim 不 bump 实证】；性能 A/B 同档【旧 = bc26bed v3.22.0 +
  g139 三形态锚【STREAMING_OLD3/NEW3，PITS 387 第三演进】：banner
  实证 v3.22.0/v3.23.0 + PERF-WIN 三窗 upd med 0.327/0.323/0.324 vs
  0.533/0.573/0.575【旧树同二进制复跑 2× spread = 环境漂移带实证，
  新在带内 = 同档裁决】+ render/swap 同档 + changesApplied 9/3/1
  全逐位 + 终档 savenorm **恰白名单 3 键**【format 31→32 + end +
  end_entities——门 207「恰白名单 2 键」先例的 format bump 最强形】】；
  PITS 405【场景手术
  先探地形 + shader 无光分支双 floor + GCC-16 假阳性结构隔离 + 存
  档伪造只剥对应后键 + 场景派生同步 fluids】；详见
  `docs/archive/phase-3.23.0.md`）；
  门计数 212（**V3-14d strider 骑乘 v3.22.0** 门 212 全 PASS【V3-14
  相位 D/4，**全 4 相位收官**，项目首个骑乘面 = 新基建】：**rideable
  strict bool**【walks_on_lava 孪生解析；strider 首个/唯一骑乘物】+
  **Player m_carried 承载旁路**【setFlying 会话旗孪生：wish 锁 +
  moveInSubsteps/stepUp 旁路 + integrate no-op【比 fly 更锁：垂直键
  亦无效】+ fall 复锚每帧 = 落乘 1.05 落差永不坠伤；保留
  look/survival 计时器【落乘进熔岩会烧 = MC 语义】/swing 位移驱动】
  + **MobEntity m_ridden + riderSteer**【runAi 顶部纯新分支：WASD
  直控——数据 speed 沿航向 + kRideDecel 0.9 漂停【停帧 = release+38
  钉 0.9/0.81/0.729...】+ ambient 流契约保留【1800f = 13 hiss 钉】+
  **collidesWithEntities opt-out**【分离 pass 不顶坐骑——骑手盒设计
  性重叠，弹道虚门首消费于 mob】】+ **Game ride pass 单收口**
  【handleInteraction 后每帧；无骑乘 = 一次 null 检查 = 结构免疫根；
  落乘 6 条件：坐骑死/玩家死/飞行 toggle/面板开/坐骑入水 inFluid ∧
  ¬onLavaSurface【熔岩面保持骑乘 = MC 定案】/portal arm 帧 + 承载
  snap 背位 kRideCarryHeight 1.05【strider 身体盒顶，ride_height 不
  数据化 = YAGNI】+ steer 推送【玩家键愿 × 相机系 flatFwd/right】】+
  **tryRide**【RMB 链 tryBarter 后，tryMilk 范式：rideable 数据谓
  词，非 rideable = 穿透位级；已骑 = 本坐骑吞点击/异坐骑穿透；飞行
  拒骑吞点击】+ **Space/LeftControl 落乘**【Space 挂双击飞行窗记录
  之前 = 落乘 tap 不武装飞行 PITS 404④】；**承载 = 会话态零
  format**【31 不变——mob 不入档 = 骑乘关系无从持久，唯一自洽解非
  妥协；先例 = portal 冷却/飞行/羊毛实例态】；渲染零改动【相机/第
  三人称/站姿体 = 承载 snap 后自动】；MC 语义偏离 6 记档【无 saddle
  直骑 / WASD 直控无 fungus stick / 水伤下沉不做【落乘触发保留，
  下界无水 = 单测臂】/ 原地落乘 / 无脚步音族 / 站姿背位无坐姿
  pose】+ 落乘保留坐骑水平速度（漂移出口）；零 format / **sim 不
  bump**【canary 4 EXACT——全主世界无 strider 无骑乘路径】/ 零
  config / 零新 wav / 零新纹理；2856/2856（+9 RideParse ×2 /
  MobRide ×5【分离 opt-out/朝向速度/停帧 38/ambient 13 hiss/twin
  位级】/ PlayerCarried ×2）；结构免疫第 93 次【S1 ≡ base19 逐字节
  ×2 + savenorm EXACT——骑乘全链冷】；S2 骑乘臂 = authored demo
  【lava 沟场景 = 5 列贯通沟 x40 z34-38 挖 y32-52 + 桥 netherrack
  y31 ×4【顶 y32 骑行通道 + 玩家起点】+ glowstone 灯排 y38 ×5【脚格
  光 9 ≤ 15 spawn 门不变——灯位须对光门验算 PITS 404②】+ strider
  独占池变体 run 目录：f60 strider 生 (40.5,32,34.5)【(40,34) 唯一
  合法列——5-6m 环列 netherrack headroom 拒 + 桥列 headroom 拒】→
  f120 RMB = `rode the 'strider'`【kReach 4.09 界内，瞄准按体心
  atan2(−0.87,4.0) PITS 404③】→ f130 180° 转向【dx −1257px 单帧，
  mouseDelta 无钳制】→ f140-260 W hold 120f【strider +Z 2.0 块沿
  桥】→ f320 Space = `dismounted the 'strider'` + 1.05 落桥零伤；
  ×2 截图 RAW 恒等 + rode/dismounted 逐字恒等 + 第三人称骑乘视觉；
  **终态权威 = 截图 + DEBUG 行【demo save 红线不回写 PITS 403①】**】
  + 无输入臂 1800f ×2 RAW + savenorm EXACT 7/7【hp 20 桥面零 burn /
  沟 5/5 / 桥 4/4 / 灯 5/5 / fires []】；S6 点名【rideable "yes" →
  init rc 1】；S2b canary 4 demo ×2 RAW + savenorm EXACT；性能 A/B
  同档【旧 = 11c1840 v3.21.0 + g139：banner 实证 v3.21.0/v3.22.0 +
  PERF-WIN 三窗 upd med 0.589/0.762/0.702 vs 0.690/0.679/0.709 +
  render/swap 同档 + **changesApplied 9/3/1 全逐位** + 终档 savenorm
  EXACT【零 format = 最强形】】；PITS 404【git checkout 还原 = 未提交功能回退两连踩【v3.21
  piglin.json / v3.22 strider.json】+ 骑乘视线三件【沟/桥/灯】+ demo
  瞄准换算【体心 + 0.0025 px/rad + LMB 先打空】+ Space 双击窗绕过 +
  分离 opt-out 首消费】；详见 `docs/archive/phase-3.22.0.md`）；
  门计数 211（**V3-14c piglin 交易 v3.21.0** 门 211 全 PASS【V3-14
  相位 C/4，neutral 类叠加交易面】：**数据驱动 barter 面**【barter_
  input 单输入 gold_ingot【现行 MC 无三档表 = 立项假设证伪弃做】+
  barter_outputs 12 项加权表【MC 比例项目近似 Σ=249：gravel 80×8-16
  / nether_brick 40 / obsidian 40 / string 20×3-9 / leather 20×2-4 /
  soul_sand 10×2-8 / quartz 10×5-12 / ghast_membrane 10 / hoglin_
  meat 10 / water_bottle 6 / golden_apple 2 / netherite_scrap 1 头奖
  ；数量 [1,64]——drops 0 概率式拒收 = 付金无货非机制】+ 解析纪律 =
  milk 配对 + wool_weights 字段族 + drops 两形式互斥】+ **checkBarter
  纯函数**【checkWoolDrops 范式：input/output 存在 + countMax ≤
  maxStack，首个失败 mob 点名，S6 实机 rc 1 直证】+ **tryBarter**
  【tryMilk 范式挂点 tryDyeMob 后：targetMob kReach + 数据谓词；错
  手持 = 吞点击 failed-milk 规则；非 barter mob = 穿透位级】+
  **barterItem() 无状态 roll**【rollBarterOutput 单 uniform01 累计
  权重 + rollDropCount 单 nextMix，固定数量零流消耗，非 barter mob
  零流红线】+ **不激怒 = 结构性质**【激怒单收口 damage()，barter
  路径零调用——挤奶先例同款，无豁免字段】+ **产出走地面
  ItemEntity + V2-6 磁力**【MC 抛出孪生非直接入包】+ **barter.wav**
  【0x6A11 定种双音 chime 880→1318.5 Hz，66→67 幂等既有 66 件字节
  恒等】；即时交易（MC 6s 检视 = 记档偏离）；零 format 31 不变 /
  **sim 不 bump**【canary 4 EXACT】/ 零 config / 零新纹理【物品图标
  全既有，atlas 166 不动】；2847/2847（+14 BarterCheck ×6 / 解析 ×4 /
  roll 纯函数 ×1 / MobBarter ×2 / SoundBarter ×1 + FNV 重钉）；结构
  免疫第 92 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——barter 活
  在 wasPressed(Right) 下全冷】；S2 交易臂 = **authored demo**【猪灵
  独占池变体 run 目录 + 1×3 视线沟【1×1 洞被 rim 遮挡不可见 = 射线
  几何 PITS 403④】+ 12 拒生沟【scan 序钉生成 cell】+ 临时插桩校准
  准星至 kReach 界内【眼-体心 5.0 恰界，feet 距 5.54 = MISS 假象】：
  f60 猪灵生沟 (40.5,51,34.5) → f120 RMB = `bartered with 'piglin'
  (gold_ingot -> obsidian x1)` ×2 逐字恒等【帧锁定 roll】+ 终帧截图
  RAW 恒等 + 热栏 crop 目检 gold 3→2 + 心满 HP 20 不激怒】+ 无输入
  臂 1800f ×2 RAW + savenorm EXACT 6/6；S6 点名【temp 假 output →
  init rc 1】；S2b canary 4 demo ×2 RAW + EXACT；性能 A/B 同档【旧 =
  113ed11 v3.20.0 + g139：banner 实证 v3.20.0/v3.21.0 + PERF-WIN 三窗
  upd med 0.561/0.584/0.571 vs 0.552/0.567/0.567 + render/swap 同档 +
  changesApplied 9/3-2/1【win2 = streaming 边界 real-dt 域】+ 终档
  savenorm EXACT【零 format = 最强形】】；
  PITS 403【demo 观察红线 = 无退出写 + run 目录 stale assets 两连探
  踩坑 + kReach 边界瞄准按体心 + 生成环视线几何 + ambient 每帧耗流
  = roll 断言必 authored demo】；详见
  `docs/archive/phase-3.21.0.md`）；
  门计数 210（**V3-14b ghast v3.20.0** 门 210 全 PASS【V3-14
  相位 B/4，下界首个爆炸射手】：**flyShoot 分支**【runAi 飞行
  dispatch 内 flying ∧ shootRange>0 数据门——bat shootRange 0 走
  flyCruise 原文位级红线：seesPlayer【alive + detect + kDetect
  Vertical 2.0 + shootRange】→ hover 保持（水平速度归零）+ 面向
  + 4.0s 冷却发射；否则 bat cruise 原文】+ **explode_radius /
  shoot_sound 数据属性**【strict 非负 >0 须 shoot_range；ammo 规则
  豁免爆炸射手（火球撞击即消耗永不钉住）；shoot_sound 默认 "shoot"
  位级 + ghast "ghast_roar" 发射前咆哮 + init wav 交叉检
  ambient_sound 孪生】+ **火球 = Projectile 复用**【damage 0 /
  hurtPlayer / explodeSource="ghast" 尾参缺省 0 = 旧调用位级；爆炸
  三接线 = stuck→explodeAt+kill 钉住即爆 / 玩家接触→explodeAt /
  mob 扫描跳过爆炸弹（火球穿 mob MC 孪生）】+ **渲染 = 单色发光
  立方**【0.4，1.0/0.55/0.15 无旋转，drawArrows 分支 + 惰性 mesh】
  + 内容族【ghast 1.5×1.75 五盒 / ghast.png 16×16 atlas [9,10]
  165→166【物品图标 = 独立 32×32 RGBA 透明底 PNG 非 atlas tile】/
  声音 2 件 64→66【ghast 嘶鸣 0x6600 1.4s / ghast_roar 0x6E0B
  0.5s】/ 池 +ghast 2 总 33 尾追加位级 / membrane 物品 0-1 掉
  xp 5】；**门内捕获 = V3-14a 字体码点笔误修复**【「第二爆炸字」
  = 爽 U+723D 非 炸 U+70B8——charset/表/PNG/单测四一致地错，
  门 210 死因屏 crop 首渲染显「爆?杀死」；修复 = charset 换入 +
  九 glyph 重格 +1（0x70B9..0x7206）+ top-512 红线 131072 px 恒等
  + font.png FNV 重钉 + test_font cell 锚重钉 = 一致性 pin 抓不
  到语义错的实证 PITS 402④】；零 format 31 不变 / **sim 不
  bump**【canary 4 EXACT】/ 无击退 + 破碎无掉落 + 点火 100%（V3-
  14a 继承）+ 火球 = 箭弹道（近程弧线补偿朝上 MC 偏离）+ 射手自
  伤（MC 免疫自身火球偏离）记档；2833/2833（+12 MobRegistry 爆
  炸面 ×5 / MobFlyShoot ×4 / SoundGhast ×2 / GhastDataPinned）；
  结构免疫第 91 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——字体
  重格后复验，锚场景无 9 个移位 glyph 消费方 = 免重锚】；S2 下界
  决定性场景 v5【player nether 高原 (40.5,53,40.5) pitch 0 +
  obsidian 平台 4【br 3 负对照】+ 环清 y50-52 三带【floor top
  出 kSpawnFloorDelta 2 带 = 地面 pick 全败 cap 永不饱和】+
  obsidian 顶盖 y57-121【in-air 顶向下扫描压 feet 55 =
  kDetectVertical 2.0 恰线】+ netherrack 架 4 y56【br 0 正对照】：
  1800f ×2 截图 RAW + savenorm EXACT + 终档 9/9——架 4/4 破【玩
  家接触爆破，爆点 = 火球盒底位 PITS 402③】/ 平台 + 顶盖全存【br
  3 = r 3 恰界】/ 玩家 4 接触死 deathCount 1 / fires [] /
  nether_entities []【mob 不入档 = format 30 设计 PITS 402②】；
  决定性时间线 f240 生 + 4s 冷却发射 + 顶盖卡壳自爆自伤 5 +
  f870/f1110 同确定性 cell 新生 = 非重生【死亡反证法 = XP orb
  缺席】】；S2 视觉【492f 火球发光立方 + ghast 五盒同框 + 死因屏
  「被 ghast 的爆炸杀死」全行正确 = 字体修复 E2E 实证】；S6 点名
  【shoot_sound 缺 wav → init rc 1】；S2b canary 4 demo ×2 RAW +
  savenorm EXACT【headless 必 --frames > demo total，无 --frames
  = finished panel 观察循环永等 + timeout autosave 假 EXACT 陷阱
  PITS 402⑤】；性能 A/B 同档【旧 = e1320b4 v3.19.0 + g139：banner
  实证 v3.19.0/v3.20.0 + PERF-WIN 三窗 upd med 0.578/0.604/0.640
  vs 0.549/0.530/0.557 + render/swap 同档 + changesApplied 9/2-3/1
  【win2 = streaming 边界 real-dt 域】+ 终档 savenorm EXACT【零
  format = 最强形】】；PITS 402【下界决定性场
  景三件 + mob 不入档同位「重生」假说 + 爆点盒底位口径 + 字体
  码点笔误语义门 + canary --frames 纪律 + bob 恰线闪烁】；详见
  `docs/archive/phase-3.20.0.md`）；
  门计数 209（**V3-14a 爆炸机制 v3.19.0** 门 209 全 PASS【V3-14
  相位 A/4，ghast 前置机制族】：**纯核心 world/explosion**【scanExplosion
  = 包围盒确定 x/y/z 序扫描 + 格中心距离 + broken = destroyable ∧
  blast_resistance < 局域强度(radius−dist) + ignited = flamable 未破
  【break 优先】/ explosionDamage = ceil((1−dist/r)×12) 整读 界 0】+
  **块属性 blast_resistance**【strict 非负 absent 0.0；obsidian = 3
  数据存活 radius-3 blast 全域】+ **Game::explodeAt**【setBlock 收口 +
  碎片 spawnBreak 种子契约 + 中心 spawnFire + boom 音 64 + 玩家/mob
  衰减伤害【创造门自动免疫】】+ **--explode x y z [r] CLI seam**【帧 0
  应用 --weather 孪生 + 严格有限数 + r 缺省 3 + 管理旗/demo 三重互
  斥；游戏内暂无消费方 = V3-14b ghast 火球首个真触发】+ 死因「死于爆
  炸/被 <name> 的爆炸杀死」【字体 +2 爆/炸 428→430 扩展区 re-cell +2
  top-512 位级 test_font 全锚重钉】；无击退（项目无击退面 V3-15 先
  例）/ 破碎无掉落 / 流体不触碰 / 点火 100% 确定（creeper 25% roll
  偏离）记档；零 format 31 不变 / **sim 不 bump**（canary 4 demo EXACT
  实证——seam 无 demo 形）；2821/2821（+10 ExplosionTest ×5 /
  BlastResistance ×4 / SoundExplosionTest ×1 + 死因 2 断言）；结构免
  疫第 89 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——无 seam 无
  blast 全冷】；S2 密封石室双臂【场景 = fresh 60f 去火 + 14×14×7 石
  室封死火域 + 玩家口袋（PITS 401①③）：a1 = health 18【dist 2.5 →
  dmg 2 衰减整读】+ (1,56,0)/(2,56,0) 破【journal 回生成态条目消失
  PITS 401②】+ **(3,56,0) 恰界上存活**【local 0 硬界 pin】+ (5,56,5)
  房外对照 + entities=0 无掉落 / a2 = obsidian 柱 (2,56,0)/(2,57,0)
  双存活 vs 邻格破 = blast_resistance 数据对照；双臂截图 ×2 RAW +
  savenorm EXACT×2【fires[] 地下生成火簇 real-dt 域】】；S2b canary 4
  demo ×2 RAW + EXACT；S6 点名【init 负 blast_resistance → rc 1】；性
  能 A/B 同档【旧 = b71cb5e v3.18.0 + g139 banner 实证：PERF-WIN 三窗
  upd med 0.393/0.550/0.593 vs 0.571/0.549/0.588 + render/swap 同档 +
  changesApplied 9/3-2/1【win2 = streaming 边界 real-dt 域】+ **终档
  savenorm EXACT**【零 format = 最强形】】；PITS 401【爆炸门场景三纪
  律：石室封火域 + journal 回生成态消失断言法 + 地下火簇 savenorm
  域】；详见 `docs/archive/phase-3.19.0.md`）；
  门计数 208（**V3-11b 创造物品栏 v3.18.0** 门 208 全 PASS【矩阵 ⬜
  项小→大推进 4/6，**V3-11 创造模式全项完成**】：E 面板创造双
  tab【标题行「物品栏/背包」按钮，激活橙框；背包 tab = 既有 E 内容
  位级；生存 E 面板零变化 = a7 臂旧≡新二进制截图字节恒等】+
  **前缀搜索**【项目首个打字面：SDL_StartTextInput 仅创造 Inventory
  面板期开【SDL3 默认关 = 结构性免疫根】；Input 帧态 frameText/
  frameBackspace【wheelDelta 孪生 frameBegin 重置 256 raw 界】；
  词表 [a-z0-9_] 小写归一 + 24 上限 + 光标帧锁定闪烁；不入 tap 词
  表 = demo 录制 schema 8 型不变】+ 224 物品 name 升序过滤 + 6×4
  网格滚轮翻页【1 tick = 1 项双向夹紧；缩表重夹】+ **左键满组/右键
  单个授予选中热栏位**【同物合并清耐久附魔 / 异物替换 / 满同物
  no-op / pickup 音】+ **搜索焦点数字语义**【tab 开时数字 = 文本不
  切热栏 = 门内 xdotool 双投递实证定案】+ demo author 面扩展
  【timeline mouse_move 可选 x/y 绝对位【文件 schema 既有，4 canary
  位级恒等】+ 键词表 +g + Python 镜像同步】；零 format 31 不变 /
  零 config 键 / **sim 不 bump**【canary 4 demo EXACT 实证】；2811/
  2811（+10 CreativeLayout ×2 / CreativeTabUI ×6 / Input frame ×2）；
  结构免疫第 88 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——文本
  输入默认关 + 面板未开零键差零像素】；S2 面板臂【全 headless
  demo，捕获点击先行 = PITS 400①：a0 vs a1 = 55 360 px 面板在 / a1
  vs a3 = tab 视图 + tab 行 107 px / a2 授予臂 fresh + 退出写 =
  存档 slot0 = allium×64 + 背包视图 slot0 渲染 / a7 生存 E 旧≡新
  字节恒等 / 全臂截图 + 存档 ×2 RAW】；**S2c v3.17.0 补证臂**【门
  207 空转断言真 E2E：a4b = fresh + 捕获 + G + 低头 + 单帧 LMB →
  退出写档 creative=True / (0,53,0)=air 即挖 / entities=0 无掉落 /
  pos 飞行不坠 + a5 生存对照未破】；**S2d 搜索 xdotool 臂 ×2**【项
  目首个文本输入 E2E：捕获 + G + 2【开面板前选槽】+ E + 打字
  "app" + cell0 点击 → 退出写档 slot1 = apple×64 + 退出截图 cell0 =
  苹果图标 + 调试日志 frameText 逐字符送达】；S2b canary 4 demo ×2
  RAW + savenorm EXACT；S6 无【零新解析面】；性能 A/B 同档【旧 =
  57c417e v3.17.0 + g139 vs 新 + g139 banner 实证：PERF-WIN 三窗 upd
  med 0.635/0.624/0.753 vs 0.712/0.586/0.605 + render/swap 同档【新
  侧 win2 swap max 208 ms = 一次性 OS 停顿】+ fluid·fires·spread·
  ents·stream 同档 + changesApplied 9/3/1 逐位 + **终档 savenorm
  EXACT**【零 format = 最强形——面板/文本输入无输入全冷路径结构性零
  成本】】；PITS 400【demo 无初始指针锁定【门 207 G/即挖断言空转复
  盘】+ save-spec 红线禁 F5 + xdotool 数字双投递 + X grab 阻塞外来
  屏捕 + 门机窗口 679×744 口径】；详见
  `docs/archive/phase-3.18.0.md`）；
  门计数 207（**V3-11a 创造基础 v3.17.0** 门 207 全 PASS【矩阵 ⬜
  项小→大推进 3/6】：**G 切换**【GameKey::G 插 FlyToggle 前 kKeyCount
  尾钉不动；locked pointer + 仅存活 = C 键范式】+ **创造 = 存档
  flag**【format 31 player.creative strict bool 恒写【dimension 先例】
  pre-31 迁 false / 30 携键 = version lie 点名 / 非 bool 点名】+ **不
  死**【damage 单收口 creative 门 = 全源免疫】+ **饥饿静态满**【入场
  填饱 + 衰减/饥饿 tick if(!m_creative) 包裹非 early return 保护 lava
  段】+ **即挖**【duration 0 复用 data-driven instant path】+ **无
  掉落/无 XP/无挖掘耐久**【breakBlock gate 只包 4 段，粒子音效保留；
  声明前移重构】+ **放置 3 点免消耗** + **开关 = setFlying**；**sim
  不 bump**【canary 4 demo 回放 EXACT 实证 = 存档态增量功能 survival
  路径位级不变】；2801/2801（+6 SaveFormat31Test ×3 / PlayerTest.
  Creative ×3【对照臂 23 tick 裕量 = 1/60 双累加边界纪律】）；结构免
  疫第 87 次【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——creative 默认
  off 零键差零像素】；S2 creative demo 臂【场景 60f fresh 去火 +
  format 31 creative true；截图 ×2 RAW；⚠ 门 208 复盘：F5 被
  save-spec 红线禁用 + demo 无初始指针锁定 = 即挖/无掉落断言空转
  【PITS 400①②，补证 = 门 208 S2c a4b 臂】】；S2b canary 4 demo ×2 RAW + savenorm
  EXACT；S6 点名【实机 format-30 携 creative → rc 1 + 单元 ThreeWay
  三面】；性能 A/B 同档【旧 = 6b031c5 v3.16.0 + g139 banner 实证：
  PERF-WIN 三窗 upd med 0.578/0.667/0.650 vs 0.690/0.596/0.652 +
  render/swap 同档 + changesApplied 9/2-3/1【win2 = streaming 边界
  real-dt 域 v3.14.0 先例】+ **终档 = 恰白名单 2 键**【format 30→31 +
  player.creative missing-in-old，v3.4.0 format 跳变「恰白名单 4
  键」同款】——creative 无输入全冷路径结构性零成本】；PITS 399【format
  bump 测试面 = 4 类降级点盘点【首跑 61 失败全此族】+ demo save-spec
  结果捕获 F5】；详见 `docs/archive/phase-3.17.0.md`）；
  门计数 206（**V3-15 生存/表现小批 v3.16.0** 门 206 全 PASS【矩阵 ⬜
  项小→大推进 2/6】：**netherite 火·熔岩免疫**【物品 fire_resistant
  strict bool armor-only = parseArmor 守卫 shears 孪生 + 4 件护甲 true；
  setArmorSlot ANY 派生；三抑制 = 熔岩 tick 不落地 accumulator 重置 /
  接触不点燃 Phase-57 门控 / 火效果 tick 抑制视觉保留；**熔岩死因覆
  写**（fire 效果致死 ∧ inLava → 「死于熔岩」= MC 元素归因，门内实证
  首版报「死于燃烧」）；击退抗性弃做 = 无玩家击退面无消费方记档】+
  **portal UV 动画**【纹理水平 2 周期重写 x%8 左半 ≡ 右半其余 164 张
  位级不变；basic.frag tile-rect 分支 u_portalAnim 门控 + phase =
  m_frameCount % 120 纯函数，tiled/legacy 双路径统一 sampleUv，off =
  位级 pre-v3.16.0；init atlas 缺 tile fail-fast】+ **蒸汽粒子**
  【updatePortalSteam 每 6 帧 8×8×9 盒【y 非对称 feet-4..+5 = 门内首
  版 y+3 漏顶行场景捕获】1/10 确定性 roll fire 种子契约专属盐；
  spawnSteam = portal 纹理 0.12 半透立方体 blend pass -0.4 m/s² 慢升
  0.8-1.4 s；portal.steam 门控 off = 零发射】；2795/2795（+3
  FireResistantArmorField/SteamIsDeterministicAndRises/
  NetheriteArmorSuppressesLavaBurn）；结构免疫第 86 次【S1 ≡ base19
  逐字节 ×2 终态树复核】；S2a portal 场景【存档 ×2 RAW 恒等无火域 +
  动画关控制臂 ×2 RAW + 单跑目检漩涡流动/蒸汽 + f400/f460 diff 3560
  px；**动画内容截图 ×2 恒等非有效断言** = 退出帧 off-by-one × 高频
  纹理 PITS 398③】；S2b 熔岩池双臂【免疫 health 20 ×2 存档 RAW /
  对照 health 0 ×2 + 「死于熔岩」目检 = real-dt 场景域 health 断言权
  威】；性能 A/B 同档【旧 = 55da85d + g139：PERF-WIN 三窗同档 +
  changesApplied 9/3/1 逐位 + 终档 savenorm EXACT——三功能无输入全
  惰性结构性零成本】；PITS 398【场景 fresh 档自带 worldgen 地下火
  去火纪律 + 存档手术格式 fluids/armor/池格 + 动画内容门协议 + MC_LOG
  单参】；详见 `docs/archive/phase-3.16.0.md`）；
  门计数 205（**V3-13 死亡统计屏 v3.15.0** 门 205 全 PASS【矩阵 ⬜
  项小→大推进 1/5，用户 11-06 目标定案】：死亡屏 2 行 → 统计屏 =
  「你死了！」+ **致死原因行**【`Player::damage` DamageInfo 重载，2 参
  旧签名保留委托 None = pre-V3-13 位级；仅致死一击来源：Fall 死于高
  处掉落 / Void 死于世界之外 / Starve 饿死 / Lava 死于熔岩 / Drown 死
  于落水 / Arrow 被箭射死 / Mob「被 <name> 杀死」init-capture / Effect
  死于燃烧·毒·药水——9 措辞全字体表内 = 字体 418→428 +1 字形「于」
  【CJK_CHARSET 代码点序插入 + 重跑生成器：TTF pin / legacy 33 位级 /
  top-512 红线恒等 / 扩展区重排格 0x4E8E 后 +1 / atlas 256×2112 尺寸
  不变 / snippet 手贴 font.hpp；test_font 4 锚重钉含 vector 末元素无
  尾逗号正则必漏】+ 固定物理偏移行距【fScale 仅 font_scale 不含
  ui.scale = PITS 397，首版 8 行叠一行门内捕获】+ **会话统计 5 行**
  【采挖/放置/击杀 = Game::SessionStats（breakBlock 单收口 /
  placeBlock 双 setBlock 含 slab 合并 / 玩家致死近战 + 箭——与 SURV-7
  持久 kill.<name> 全部死因语义不同记档）+ 行走 = Player 水平位移
  累计传送阈值守卫 + 存活 = 帧锁定 m_frameCount/60；不存档 = 记档偏
  离 MC scoreboard 持久】；**Game Over 屏 = 死亡屏本身**（无 hardcore
  = 记档偏离）；2792/2792（+5）；结构免疫第 85 次【S1 ≡ base19 逐字
  节 + 字体重排格零像素复核——无输入无 CJK 渲染】；S2 场景手术 24 格
  竖井坠落死亡臂【无输入 ×2 RAW + savenorm EXACT（fall 帧锁定死亡帧
  确定）+ 死亡屏裁剪目检全行】；S6 无（零新解析面）；性能 A/B 同档
  【旧 = d61a757 v3.14.0 + g139：PERF-WIN 三窗同档 + changesApplied
  9/3/1 逐位 + 终档 savenorm EXACT】；详见
  `docs/archive/phase-3.15.0.md`）；
  门计数 204（**V3-8b 手柄习惯映射定案 v3.14.0** 门 204 全 PASS：
  用户 11-06 三项定案【调研先：SDL3 手柄数据库归一化 Xbox/PS/Switch
  三大布局到同一逻辑位 = 单表覆盖无需多表，山湾克隆 = 数据库已识别
  V3-8a 实机铁证】：**L3 = 蹲伏**【补键盘 LeftControl 手柄缺口 MC
  Java 惯例】/ **RT 扳机 = 疾跑**【0.2 死区数字化按住 = Shift 左摇
  杆同款，B 解放】/ **B = 飞行 toggle 专用钮**【新 GameKey::FlyToggle
  仅手柄键 = mapKey 永不返回 256 键黑盒钉，单击 setFlying(!) 无双击
  窗，键盘双击 Space 不变 A 双击冗余记偏离】；config 全自定义 = 定案
  不做；LT/R3/GUIDE/Dpad 上下左保持未映射记档；**首个实际 sim bump
  1 → 2**【录制的 gamepad 事件按当前解释重放 = v3.13.0「输入路由」
  条款首实例；4 canary 重新 author；门内捕获 Python 镜像常量 SIM_
  VERSION 漏 bump = 金丝雀先于用户发现 PITS 396①】；2787/2787（+3
  RTTriggerSprintDigitize/L3CrouchHold/FlyToggleKeyIsGamepadOnly +
  TriggersUnmapped re-pin）；结构免疫第 84 次【S1 ≡ base19 committed
  config 口径逐字节 + savenorm EXACT——手柄映射 = 无手柄冷路径】；S2
  sim-2 canary 4 demo ×2 RAW + savenorm EXACT；**S2b 首个 bump 实证**
  【sim-1 旧 canary rc 1 点名 + --demo-force rc 0 WARN + force 终档
  savenorm ≡ sim-2 canary = 纯键鼠录制行为恒等最强形】；性能 A/B 同
  档【900f 无输入 PERF-WIN 三窗同档 + changesApplied 9/2/1 vs 9/3/1
  real-dt 域 + 终档 savenorm EXACT】；**实机手感验收归用户**【门强度
  记档同款 V3-8】；详见 `docs/archive/phase-3.14.0.md`）；
  门计数 203（**demo 独立 sim 版本 v3.13.0** 门 203 全 PASS：用户
  报障 `--demo example.dmo` 被拒"recorded with v3.11.0, this is
  v3.12.0 - incompatible"【旧设计 = 构建版本串当模拟语义代理，纯渲染
  版误杀全部 demo】；**定案 = 独立 sim 版本** `kDemoSimVersion = 1`
  【bump 纪律：仅帧锁定模拟语义变更（update/物理/AI/流体/生成/输入
  路由/demo pacing）才 +1，纯渲染/UI/音频/内容/文档不 bump = v3.12.0
  阴影缩尺不 bump 实例】+ `recorded_with` 构建串仅信息展示永不拒播
  + **legacy `version` 指纹零迁移映射 sim 1**【用户手头 .dmo 全部继续
  可放】+ 混用 schema 点名拒 + sim 不匹配 rc 1 点名 + `--demo-force`
  逃生门【仅 bypass sim 门 schema 严格性保留 WARN 点名 / 无 --demo
  带 force rc 2】；**canary 纪律**【4 shipped demo = 模拟语义金丝雀
  每版门内回放 savenorm EXACT，破坏 = bump + 重新 author】；2784/
  2784（= 2777 + V3-8a 手柄 1【d5a5ea6 并行提交】+ 本版 6）；结构免疫
  第 83 次【S1 ≡ base19 committed config 口径逐
  字节 + 用户 ui.scale 2.0 口径 ×2 自洽——锚假失败 = 用户 config 调
  值 PITS 394】；S2 四 demo ×2 RAW + savenorm EXACT + **S2b 用户报
  障场景实证**【git 4531c42 旧 example.dmo legacy 回放成功 + 终档 ≡
  现行 schema 回放 = 映射行为恒等】；S2c sim-2 拒 + force 旁路；S6
  force 无 --demo rc 2；性能 A/B 同档【changesApplied 逐位 + 终档
  savenorm EXACT】；详见 `docs/archive/phase-3.13.0.md`）；
  门计数 201（**V3-9 demo 回放 v3.11.0** 门 201 全 PASS【**3.x
  矩阵收官**】：注入内核 = Input handle* 收口【与 xdotool 同款路径】
  + **双 Input 路由**【回放 = 观众 Input 为 SDL 单消费者 + 玩家通道
  零 pump 纯注入 = 改世界操作观众通道无消费者 结构性零遗漏】+ 编写
  /录制/回放三 CLI 旗互斥 + demo 文件 = V3-7 gzip 容器严格 schema
  【version 指纹拒播 / 乱序 / 未知 type 带 8 词表 / world 双键 /
  gzip 损坏】+ 0.5× = render-only 保持【帧锁定不双推】+ --frames 按
  模拟帧计【任意倍率同终态】+ 观众 orbit render-only【aimForward 不
  偷 demo 瞄准】+ save 引用槽永不回写观察红线 + shipped 示例 demo
  285 B 幂等；2777/2777（+43）；结构免疫第 81 次【S1 ≡ base19 +
  EXACT】；S2 六臂【回放 ×2 首放固化 / 录制→回放闭环 2810 B / 编写
  臂位移 4.121 / 观众路由姿态恒等 / 变速双向 EXACT+76px 角标 / 结束
  面板 lum 32.0】；S6 7 面点名；性能 A/B 同档【回放自成本 sub-ms】；
  **门内捕获修复 demo UI 绘制序缺陷**【块在主批 flush 后 = 顶点不
  消费 + 截图 = back buffer 落后一帧既有语义 → 独立批 + headless
  双渲染 + badge 移时钟下】；8 偏离；详见
  `docs/archive/phase-3.11.0.md`）；
  门计数 200（**V3-6 世界边缘雾 v3.10.0** 门 200 全 PASS【**base19
  重锚首重锚 225272 B**】：编排者实证定案【无输入 worker = 16 列后
  零扩展 PIPE-1 纪律 → 锚图"浮空截面" = 演示区边界 32-45 块 < 雾
  start 750 完全可见；fog off ≡ on 实证可见世界全 <750；现有雾按
  目标盘算对未填充盘缘无效】→ **fog end = min(静态 end, 实际填充盘
  cheb 半径 ×16)**【filledChebyshevRadius 填充 rim 语义下闭保守 +
  fillClampedEnd/fogFilledRange 纯函数 + render 雾块列快照共享零新
  树走查 + 下界同款 clamp【首传 3×3 半径 1 = end 16】；稳态盘满 64
  → min(1000,1024) = 1000 位级恒等【65 滞回变体钉】；零配置新键/零
  format/零新 worker 活动】；2734/2734（+10）；重锚论证【diff 20940
  px 全盘缘带 + 前景/HUD 0 diff + 单调趋天空 0 违例 + 单雾因子 100%
  ≤2LSB + 留证五图 + 存档 savenorm vs 旧侧 EXACT】；S1 ≡ base19 ×2；
  S2 fog-off ≡ base18 逐字节【三方闭环 = 唯一像素面】；性能 A/B 同
  档【16 列扫描零成本 + 逐位 + 终档 EXACT】；详见
  `docs/archive/phase-3.10.0.md`）；
  门计数 199（**V3-5 建筑内容族 v3.9.0** 门 199 全 PASS：**核心仅
  worldgen 4 文件**【Sediment 解析 absent=disabled 位级 legacy +
  [0,1] 严格 + pass 生成链最后读最终湖底 + 3 纯谓词 + fail-fast】：
  20 块【clay drops 4×clay_ball / mossy_cobblestone / bricks +smelt
  terracotta / stone_bricks / mossy_stone_bricks / **terracotta 15 色
  染色族**【项目染料词表含 white 无 lime = 设计 16 收敛偏离 1】】+ 2
  物品 + 17 配方 + 纹理 20【染色混合公式 (base×3+dye×2)//5 自建偏离
  4】+ atlas 145→165 空格零移位 + **水底 sediment pass**【worldgen.
  json 数据驱动 0.06/0.008，per-column 纯函数双盐，dry 列零触碰，
  熔岩湖双谓词拒，clay 优先顶 1 层；发行 census 1024² eligible 3683/
  clay 211/mossy 26 沙漠沙洋底 only】；2724/2724（+27 含发行世界
  census 样本列 ground truth）；结构免疫第 80 次【S1 ≡ 锚 +
  savenorm **EXACT** + 白名单 4 键】；S2 三臂【密封水室展示行 ×2 RAW
  【6 纹理水下可见 + 池格 32 精确】/ **破 clay → 4× clay_ball**【磁力
  + 编辑回退】/ 熔炼 → brick【fuel 4201 验算】】；S6 4/4【概率越界
  ×2 / 字符串 / 染色族缺一色点名】；性能 A/B 同档【upd 均值 0.554 vs
  0.598 噪声带 + 逐位 + 终档 EXACT】；详见
  `docs/archive/phase-3.9.0.md`）；
  门计数 198（**V3-3 下界 mob v3.8.0** 门 198 全 PASS：**5 mob 纯
  数据**【zombified_piglin hostile 孪生 zombie undead false / piglin·
  hoglin **neutral 新类**【damage() 单收口任意来源激怒 + 30 s real-dt
  脱激 + chase 谓词第二支 (neutral ? m_angry : true) 位级根】/ magma_
  cube 不分裂不跳 blob / **strider passive + walks_on_lava**【点火豁免
  【火块仍点燃】+ 熔岩面 feet-on-plane walk 档 snap 不动点 + 生成面
  熔岩源顶面】】+ **下界生成池零新基建**【m_nether.mobs 预留槽 +
  loadNetherSpawnPool fail-fast + 共享类型表 + 策略继承 + 只 tick 活跃
  维既有】+ **strider 光门 7→15**【熔岩自发光 15 机制缺口门内实证】
  + 食物链 hoglin_meat +4/熟 +8 + 地面 ambient 3 件【runAi 数据门控
  分支】+ 纹理 9【atlas 145 空格零移位】；2697/2697（+39）；结构免疫
  第 79 次【S1 ≡ 锚 + EXACT + 白名单 4 键】；S2 三臂【下界生成流 ×2
  恒等 + 截图 RAW / **strider f660 @ y=32.0 熔岩海顶面精确 ×2 + 540f
  零 burn**【环全清 cap 永不饱和 = 决定性臂】/ 池隔离 OW 4 + 下界 0】；
  S6 6/6 点名【互斥/双旗非 bool/池缺失/池未知 mob/ambient 单向对】；
  性能 A/B 同档【changesApplied 逐位 + 终档 savenorm EXACT】；7 偏离；
  详见 `docs/archive/phase-3.8.0.md`）；
  门计数 197（**V3-2 Part 2 下界深化 v3.7.0** 门 197 全 PASS：**全
  nether-scoped overworld 位级不变**【ambience 30 组输入面单测 + S1 锚
  双保险】零渲染/零 format/零 config：T1 水蒸发【下界泼水桶 = 桶变空
  桶 + evaporate 音 + 日志，零格触碰点击恒消耗；熔岩桶正常放置】+
  T2 音效族 11 件【nether/nylium/stem 3 族 9 + evaporate + ambience_
  nether，49→60 幂等；13 下界块 sound_material 重指派全钉】+ T3 magma
  light 3 + T4 ambience 第 4 上下文【nether > cave > day/night 默认参
  inNether=false 位级】+ T5 soul sand walk_speed_factor 0.4【严格
  (0,1] 解析 + 双 feet 格取最小 + 仅步行疾跑含 MC 同款 + ×1.0f 位级
  恒等】；2658/2658（+18）；结构免疫第 78 次【S1 ≡ 锚 + EXACT + 白名
  单 4 键】；S2 三臂【下界 shot ×2 RAW + ambience_nether 事件 a=2 b=2
  逐一对 / **步行比值 0.403**（3.0 s W-hold dx 4.321 vs 10.712）/ 桶
  蒸发 + OW 对照水正常放置】；S6 3/3 点名【1.5/0/"0.4" 字符串】；性能
  A/B 同档【均值 0.568 vs 0.557 + changesApplied 逐位 + 终档 savenorm
  EXACT】；详见 `docs/archive/phase-3.7.0.md`）；
  门计数 196（**V3-4 netherite 材料链 v3.6.0** 门 196 全 PASS：
  **零核心代码纯数据**【git status src/ = 空——"加材料 = 数据"第四档
  验证】：ancient_debris 块 +smeltable_to netherite_scrap（CRAFT-6 块
  物品继承）+ ingot = shapeless 4 scrap + 4 gold_ingot【偏离 1：MC 用
  nugget】+ 工具 3【dur **2031** MC 精确 / bp 4.0 项目铁刻度 / ad
  4·5·4.5 / ea 14 / **fast_against 2.0 × 21 块**【铁石族 10 + nether
  台地 11】/ repair ingot / 直接 crafting【偏离 2：项目无钻石工具】】+
  护甲 4【dur **545/792/743/644** = 钻石 ×1.5 MC 精确 / ap 3/8/6/3 /
  ea 10 / **shapeless 升级 ingot + diamond_<slot>**【偏离 3：无锻造
  台】/ model 克隆钻石系】+ icon 9【暗红铜色板偏离 4，既有 84 张 md5
  不变】；2640/2640（+23 含 SmeltThenCraftEndToEnd 全链）；结构免疫
  第 77 次【S1 ≡ 锚 + 白名单 4 键】；S2 熔炼臂【无输入 600f：smelt
  complete 恰 10 s + fuel_substeps 4201 验算 + **xp orb ×1 发放面实证**
  + 合成链单测】；S6 5/5 init 点名【durability 0 / enchant_class 未知
  词带 7 词表 / strict pair / 未知材料 / fast_against 未知块】+ 非拒收
  面 2 实证留证；性能 A/B 同档【终档 savenorm EXACT 完全恒等】；详见
  `docs/archive/phase-3.6.0.md`）；
  门计数 194（**V3-1 Phase B 下界世界 v3.4.0** 门 194 全 PASS：
  下界维度【D1 增量并行成员 overworld 零 diff + D2 惰性构造【首传前
  零一切】+ D3 仅 Player 换绑 Entity::setWorld【m_world 引用→指针
  17 点机械适配】+ 非活跃维时间胶囊】+ 下界 worldgen 专属纯函数
  【坐标纯函数零噪声：基岩 0-4/熔岩海 5-31 全源静态/netherrack
  32-122/基岩顶 123-127；派生 seed worldSeed^kSaltNether 不落盘】+
  netherrack 块【hardness 0.4 MC 精确，atlas [13,7] 零移位】+
  skyless 光照 2 参【shader 零改动】+ 真传送管线【60 帧倒计时 → 8:1
  floorDiv 双向映射 → ±16 搜活跃 portal【无 → 生成 4×5 含角 + 2×3
  基岩平台走咽喉点】→ hitbox 安全 + step-up → 3×3 列盘同步 catch-up
  → 换绑 + restorePosition → 流送 engage；900 帧冷却会话态；正交
  portal yaw −π/2 MC JE】+ portal_trigger/travel 2 音效【47→49】+
  渲染 5 点门控【暗红 clear/日月星云雨不画/雾 10→96/F3 dimension
  行】+ 下界死亡 = 主世界重生 + 8 条下界行为收口记档 + **format 30**
  【player.dimension 严格词表恒写 + nether 段 + nether_entities 恒写
  + 两维 portal rescan + 交叉检】；结构免疫第 75 次免重锚**最强形**
  【下界惰性未建：S1 ≡ 锚 + 终档恰增白名单 4 键】；S2 双跳 arm 全
  headless【传送：映射验算 + 下界截图 ×2 RAW 恒等 + 落点 [1,55,−0.5]
  精确 + nether 段 26 块 = 生成框 + 平台 / 返程：±16 搜中原 portal +
  nether 段持久化 + time 帧锁定验算】；S6 6/6 点名；性能 A/B 同档
  【终档白名单 4 键 + 下界稳态 ≤ OW：render 0.349 vs 0.510 med +
  fluid/fires = 0.000 熔岩海零 churn】；详见
  `docs/archive/phase-3.4.0.md`）；
  门计数 192（**V3-1 Phase A 门框 v3.2.0** 门 192 全 PASS：nether_portal
  块【full-cell 半透盒 + destroyable false + light 11 + state_count 2
  axis】+ core/portal 纯函数族【MC 框 4×5..23×23 角可选有界扫描 45×45
  前缀和 O(1) / 激活·去激活决策【非角破去激活·角破保留 MC 精确·fire
  熄灭自持】/ 8:1 floorDiv 映射 / 站入 60 帧 step / ambient roll】+
  咽喉点钩子 + 点火 (c)【面法向相邻内 air 格】+ GUI 互斥自动关 +
  载入 rescan 孤立置 air + **零 format 29**；结构免疫第 73 次免重锚
  【≡ 锚 224468：worldgen 零 obsidian + 打火石不在默认热栏】；S2 六臂
  （激活终档 6 portal state 0 + fires 0 + flint damage 1 / 去激活 /
  角保留 / 站入 "Nether ready at (0,55,−1)" 验算 + 零位移 + 每秒重计 /
  **GUI 互斥决定性六跑 d6 站入+E+重捕获 ≡ d3 对照 RAW 逐字节** +
  d4/d5 框外面板留开条件化 / 旧侧形状互斥）；S6 4/4 点名；性能 A/B
  同档【changesApplied 9/1/3 逐位 + 终档 savenorm EXACT 零 format 最
  强形 + portal 自成本无可测增量】；Phase A 传送 = kPortalTravelEnabled
  false 桩 + static_assert 构建门，Phase B 接真传送；详见
  `docs/archive/phase-3.2.0.md`）；
  门计数 193（**V3-8 手柄支持 v3.3.0** 门 193 全 PASS：SDL3
  GameController 输入边界层——**源位掩码 OR 语义**【m_keyDown 每源一
  bit：bit0 键鼠 / bit1 手柄，isDown = 任一源，wasReleased 弓语义仅
  本源 falling 且掩码清 0 = 键鼠手柄天然双连共存 + 无手柄位级恒等
  结构免疫根】+ v1 Xbox 映射 9 钮 2 摇杆【左摇杆移动死区 0.2 数字化 /
  右摇杆视角死区外重映射 ×100 px 合成 mouseDelta / A=Space 双击飞行
  继承 / B=疾跑 / X=背包 / Y=丢弃 / LB=挖掘 / RB=使用 / Back=相机 /
  Start=ESC / D-pad 右=Tab；LT/RT/L3/R3 等 v1 不映射记档】+ 单手柄
  模型 + 连断日志 + F3 末行连接态 18→19 行 + SDL_Init(VIDEO|GAMEPAD) +
  无 config 键；2555/2555（+19 含 legacy 位级恒等 fixture）；S1 ≡ 锚
  224468 + gamepad 日志行 = 0；性能 A/B 同档 + 终档 savenorm EXACT 零
  format；**门强度记档 = 本环境无物理手柄，全逻辑单测 + 实机验收归
  用户**；详见 `docs/archive/phase-3.3.0.md`）；
  门计数 191（**V3-7 存档 gzip v3.1.0** 门 191 全 PASS：确定性 gzip
  容器【10B 头全钉 + raw deflate 6 + CRC32/ISIZE 尾，同世界逐字节
  可复现】+ 加载魔数 1f 8b 探测【旧明文档零迁移直读；遗留零迁移最
  强形式 = savenorm 新旧 fresh-60f 恰 root.format 28→29 单键】+
  save.compression 严格读【absent = gzip 用户定案，none = 旧行为位
  级恒等】+ format 29【schema 零变更 = gzip 时代标记，解析侧代码零
  改动】+ 写侧单 choke point（writeSave exit/F5/autosave 共享，
  writeAtomicFile 原子性不变）/ 读侧两消费点（readSaveFile +
  --list-saves 摘要，corrupt 槽 problem 命名不隐藏）+ gate_compare.
  py savenorm 解压步 + save/load 日志 ms + 体积变换【real-dt 性能
  证据】；结构免疫第 72 次免重锚【≡ 锚 224468 逐字节 + savenorm
  EXACT + 头 10B 实证 + 门档 6.3×】；S2 四臂（roundtrip 6.25× /
  零迁移 / none / corrupt 点名 exit=1）；S6 3/3；2503/2503（+21）；
  详见 `docs/archive/phase-3.1.0.md`）；
  门计数 190（**V2-1 前视角 v2.1.0** 门 172：
  C 键三循环，前视 = 后视同款数学翻转角执行 + aimReach D+kReach——详见
  `docs/archive/v2x/phase-2.1.0.md`；**V2-2 Steve 皮肤 v2.2.0** 门 173：
  模型 per-face 纹理通用能力【既有 mob 位级不变】+ Steve 脸 8×8 像素
  + 7 tile 图集零移位——详见 `docs/archive/v2x/phase-2.2.0.md`；
  **V2-3 教程系列化 v2.3.0** 门 174：tutorials/ 三系列（beginner 零
  改动 + 农耕 + 厨房）+ format 25 + pickup./smelt. 事件 + T 跳当前
  系列——详见 `docs/archive/v2x/phase-2.3.0.md`；
  **V2-4 天气 v2.4.0** 门 175 全 PASS：core/weather 纯状态机（推进式流
  首段恒 CLEAR【60f 基线免疫根】+ 雷独立 salt）+ rain shader 静态 300
  滴 + 天空 dayFactor 单收口（RAIN ×0.85 + 雷 5 帧脉冲）+ 交互四件
  （雨灭火/营火/燃烧效果/润耕地 2×）+ render.weather strict 默认 true
  【off = v2.3.0 位级恒等，门内三方恒等实证】+ --weather CLI + 雷 wav
  + 零 format；性能 A/B changesApplied 9/3/1 逐位 + 终档集合恒等——
  详见 `docs/archive/v2x/phase-2.4.0.md`；**V2-5 猪 v2.5.0** 门 176：
  纯数据零核心代码 + V2-2 per-face 首个 mob 消费（pig_face 显式 8×8
  脸）+ pork 食物链（+3→+8）+ 图集 99 零移位 + 字体 423——详见
  `docs/archive/v2x/phase-2.5.0.md`；**V2-6 体验打磨批 v2.6.0** 门 177：
  四子项各消费一处记档偏离——① 第一人称手臂（arm_r box 数据驱动
  pa_arm 零新纹理 + 三态门控 + 静态无摆动记档）② mob blob 阴影
  （1.5×宽扁 quad alpha 0.3 贴底格顶面 + 玩家仅第三人称）③ 拾取磁力
  （帧锁定 f(距离,帧)：6×(1−d/2.5)² 加速趋近 + 24f swirl + hasRoomAt
  不穿墙 + 插墙箭同款拉出）④ 月相（8 相 = 累计天 index%8，食相 quad
  滑移零新纹理，**绘制序 = 食相先/月后**——r=800 平面 24-bit 深度
  ~160 块分辨，PITS 371）+ 零 format；基线 60f = base18 逐字节恒等
  ——详见 `docs/archive/v2x/phase-2.6.0.md`；**V2-7 材料补齐批 v2.7.0**
  门 178：四子项全数据驱动（核心唯一新增 = 块级 per-face faces 段，
  无 faces 位级不变红线）——① planks 块化（既有 tile 零新纹理 +
  fuel_time 7.5 迁块属性 + 8 配方按名零改动）② gold/diamond_block
  （iron_block 先例 + beacon_base 布尔入列）③ spider 掉
  string/spider_eye ×0-2（纯收集记档）④ furnace_front 单 tile
  （lit = 既有火焰粒子记档）+ 图集 102 零移位 + 零 format——详见
  `docs/archive/v2x/phase-2.7.0.md`；**V2-8 sky rescan clobber 修复 v2.8.0**
  门 179：光照核心定向修复（LGT-1 记档潜伏 bug：天花板骑跨 footprint
  边界 → 后一 region rescan 覆写边界格 sky 为纯垂直值 + sky BFS 种子
  无界外源 = stale-dark/stale-partial 暗带）——修复 = sky BFS 种子盒
  +1 块 shell 环（rescan/BFS 写界/no-op skip/propagateAll 零改动；
  shell 读 = 既有 1-chunk 边界读环 → 独立性判据/confluence 零改动）+
  TDD 双回归（pre-fix 2464/544 diff 格复现）+ CPU 探针 + E2E slab
  无输入流送判别 + 零 format——详见 `docs/archive/v2x/phase-2.8.0.md`；
  **V2-9 step-up quirk 修复 v2.9.0** 门 180：player/mob 共享
  Entity::stepUp 两 quirk 定向消除——① +X/+Z 恰 1 格宽台阶踏不过
  （front 格 snapped+1 不对称 → 查贴住格）② 非整数脚高巧合对齐
  凭空抬升 1.0（flush 位置判 → 无状态真阻挡验，严格不等式）；
  整数脚高位级不变红线 + TDD 3 新 + 1 反转 + 零 format——详见
  `docs/archive/v2x/phase-2.9.0.md`；**V2-10 网格修复批 v2.10.0** 门
  181：渲染节两记档修复——① 叶子-叶子内部面剔除（块属性
  cull_same_id 严格布尔，leaves 族 3 块首消费，mesher greedy pass
  同 id 剔面纹理选择前，glass/水/cross 族红线零触碰）② alpha
  cutout discard 零代码确认（PRE-5 桶无关 discard 普查全覆盖 +
  单测钉）；**base18 第 2 次重锚 178148 B**（diff 4185 px 全叶冠
  区零泄漏论证，Phase 18 先例）+ 零 format——详见
  `docs/archive/v2x/phase-2.10.0.md`）。
- 活跃已知问题：`docs/KNOWN_ISSUES.md`；**3.x「世界扩展」= 当前执行
  权威：`docs/BACKLOG.md`**（11-04 晋升 + 逐项定案 + 同日重编号
  【原 4.x 命名改 3.x】：6 项全保留 + 用户诉求 **V3-7 存档 gzip
  （默认开 = format 29）先执 = v3.1.0**（用户开新会话后执行），其后
  V3-1 下界维度锚【超大】拆分立项；v3.0.0 = 开启版已发布【纯文档 +
  banner】；11-05 UX 行 +2 = V3-8 手柄支持【v3.3.0 执行中】/ V3-9 demo
   回放【矩阵尾部默认，双入口 + 只观察交互模型定案，详见 BACKLOG】；逐版
   release 政策 11-05 用户定案撤销【v3.1.0–v3.10.0 只留 v3.9.0 代表，9
   release + 9 tag 删除，Latest = v3.9.0；v3.0.0 开启版保留；矩阵收官
   v3.11.0 发单一 release】）；2.x 全
  任务表 = `docs/archive/v2x/BACKLOG.md`；1.x = `docs/archive/v1x/`；
  会话交接：`opencode-sessions/SESSION_CONTEXT.md`。
## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告）
./build/bin/tokenmine_tests                                                 # 测试（2075）
./build/bin/tokenmine                                                       # 运行（ESC 退出）
./build/bin/tokenmine --frames N [--screenshot /abs.png] [--save slot] [--new] [--config path]
./build/bin/tokenmine --seed 42 --time 0.75 --log-level info                # 其它参数（--help 看全部）
```

游戏必须从仓库 cwd 启动（资源相对路径）；独立测试程序链接
`build/lib/libmc_core.a -lSDL3 -lGLEW -lGL -lz -lpthread`。

门协议工具（PITS 317 纪律：门资产可从仓库重建）：`tools/gate_compare.py`
（raw = PNG 字节比对 / norm = savenorm 比对，剥 real-dt 字段）、
`tools/perf_gate_config.json`（性能 A/B 用 config）、`tools/g139_instrument.py`
（性能插桩补丁器）、`tools/xenv.sh`（门机 X 环境）。

## 硬性规则 Top 10（违反即 bug；全文 docs/PROTOCOL.md）

1. **坐标**：world/chunk/local 严格分离，换算只在 `coordinates.hpp`；负坐标 `floorDiv`。
2. **渲染隔离**：游戏逻辑不碰 GL/SDL（renderer/ 与 window/input 边界除外）。
3. **数据驱动**：核心不 switch 方块 id（读 properties）；air 是代码哨兵（无 air.json）。
4. **错误与代码质量**：`Result<T>` + `MC_TRY` 显式传播；**无 TODO/伪代码/魔法数字/
   硬编码方块检查/删功能过构建/禁用测试**。
5. **相机**：yaw 0 = 朝 -Z、正 = 左转；pitch 正 = 上、clamp ±~89°；**鼠标转向仅在
   指针锁定时生效**（无输入运行字节复现的保证）。
6. **玩家物理**：position = AABB 底面中心；重叠判定**严格不等式**（无 epsilon）；
   防穿透 = 子步化（每子步 ≤0.4 块、≤64 子步）+ 终端速度 38。
7. **chunk streaming**：worker job 只在 **post→flush 窗口**运行（窗口内主线程只读
   World）；**光照 job：流送与流体追赶两类各至多 2 个/帧，同窗内两两位级独立**
   （99.10：判据
   `lightRegionsIndependent` = 写 reach ±1 列 + block 光 BFS 读环 +1 列双向不相交
   （gap≥4 任轴），`world/light_job.hpp` 纯函数；依赖对回退单 job 顺序 =
   pre-99.10 逐位行为；confluence 证明 =
   `docs/archive/v1x/phase-lgt1-flip.md`；99.11：流体 tick 的 changed 格 → 列粒度
   待集 `m_fluidLightPending`（`changedCellColumns` 合并同列），
   `planFluidLightJobs` 按 set 序取至多 `kFluidLightColumnsPerFrame = 2` 列/帧
   发 worker job（newTerrain=true 强制天空传播 = 99.9 跳过的流体洞修正），
   跨类同窗仅当对每个流送 job 列都独立（失败候选跳过留待下帧）；净变 chunk 走
   job-local `dirtyOut`，flush 后主线程统一 markChunkDirty → 流体列重烘
   （99.4 纪律）；卸载驱逐待集死列防无界增长）。
8. **确定性**：昼夜/Mob/箭/粒子 = 固定 1/60 帧步长；生存计时器（饥饿/进食/破坏/
   坠落/实体 age）= 真实 dt；截图只在帧边界字节一致。
9. **面表绕序**有回归测试守护（`FaceWindingIsCounterClockwiseFromOutside`），勿改
   顶点序；光照 = `max(sky − (8/15)×(1−df), block)/15 × faceShade`（顶 1.0/±X 0.8/±Z
   0.6/底 0.5；LGT-3 仿射撤退 + kNightSkyLevel 7 月光地板；per-draw 光覆盖 =
   PRE-6/LGT-4 掉落/光束，覆盖后强制复位）。
10. **出生点**：4×4 chunk 演示区列，按 `kSpawnCandidates` 自动找干燥陆地
    （h > seaLevel 且出生 AABB 内无 solid）。

## E2E 协议（摘要；全文 docs/PROTOCOL.md「E2E 协议」）

- **零并发 :0 活动**（用户手离开鼠标）+ 每拍截图前 `unmap_covers`。
- **无输入运行**：×2 全帧字节一致 + skygate（FRESH 模式色）+ base 门
  `/tmp/opencode/base18.png` 字节一致（新系统需论证"无输入下零活动"= 结构性免疫）。
- **输入驱动运行（xdotool 脚本）**：**存档状态（含 yaw/pitch）是确定性权威证据**——
  全帧字节一致受 SDL 指针锁定 grab delta 环境限制（一次性相机旋转 ±0.01 rad = 4 px；
  缓解 = warp 实际原点 + (632,352)、同批同 warp、手不动；存档 yaw/pitch = 漂移探测器）。
- 输入锚定窗口发现时刻，D ∈ [0.1, 0.5] s；tick 类断言需 ≥1.6 s 裕量（1/60 双累加
  边界：36 s 饥饿 tick = 帧 2161，20 s 饥饿伤害 = 帧 1200，1.6 s 进食通道完成 = 帧 97）。
- **HUD 红心检查**（心带 y[646,661] x[551,729]）：无输入运行 = 0（>50 = 污染废拍）；
  输入运行 >50（输入生效）。

## 阶段流程（每个 phase 按此顺序）

1. 用户确认方向 → 2. 实现（数据驱动、无 TODO）→ 3. 构建 + 全量测试（`-Werror`）→
4. E2E（base 门 + 回归对 + 场景专属验证，按上面协议）→ 5. 文档（CHANGELOG v2.x 条 / BACKLOG v2.x 表 + 变更记录、
KNOWN_ISSUES、opencode-sessions/SESSION_CONTEXT）→ 6. **显式路径列表** git commit（永不 `git add -A`；
永不提交 `opencode-sessions/` 下任何文件（本地）；**message 格式（11-06 用户定案）=
一句话标题 + 精简多行正文**——标题 = 版本 + 一件事；正文只留关键定案/门结论/测试数
（细节归阶段文档，不写长句））→
7. 中文阶段报告（标准结构：实现内容/变更文件/测试/构建与验证结果/已知问题/下一步建议）。

## 坑位 Top 8（全文 docs/PITS.md，406 条；pre-3.x 条目在 docs/archive/PITS_PRE3X.md）

1. **SDL grab delta**（P28）：输入 E2E 全帧字节一致不保证——存档 yaw/pitch 为漂移
   探测器；驱动 warp 用实际窗口原点（实测 (4,64)）+ (632,352)。
2. **物理光标**（P5）：指针未锁定时 SDL 仍投递 mouse motion → 无输入 E2E 必须零
   :0 活动，红心检查 = 污染哨兵。
3. **粒子视觉验证视角**（P29）：直下视角可视锥不含破坏粒子——必须斜视角（pitch
   −1.02 级）；帧差分析先排除已知 UI 变量（饥饿/HP 图标也在 diff 里）。
4. **splitmix64 是纯 finalizer**（P29）：不推进 state——推进式流必须
   `state += 0x9E3779B97F4A7C15ULL` 后再 mix（同 burst 同值缺陷根因）。
5. **`std::string::replace(pos,len,const char*,n)` 截断**（P28）：只取替换串前 n 字符。
6. **`--save slot` 第二次运行会加载第一次写的存档**（P15）：E2E 对跑每拍从源副本
   `cp` 新鲜存档。
7. **按住破坏会自动续破下一块**（P15）：输入脚本的释放时机 = 破坏完成 + 余量，
   余量内的按住对下一块产生确定性裂纹进度。
8. **像素验证**：`glReadPixels` 必须在 `swapBuffers()` 后（前缓冲）；
   `magick ... txt:` 输出 `(r,g,b,a)` 开头 `(` 破坏数值转换，awk 取 `$2`。

## 禁止事项

- `config/config.json` 是**已提交文件**（P32 起：用户调值 view_distance 80 /
  day_length 600 + `player` 默认节）——**不还原用户的调值**；内容改动仅在用户
  要求时做，提交用显式路径。
- **永不 `git add -A`**——提交用显式路径列表。
- `opencode-sessions/` 目录（SESSION_CONTEXT/kickoff_prompt 等）本地专用（`.git/info/exclude`），永不提交。
- 无 TODO/伪代码/硬编码检查/魔法数字；新内容一律走数据文件 + 注册表严格校验。

## 文档地图

| 文档 | 内容 | 何时读 |
|---|---|---|
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（现状 v2.0.0，16 子系统分节；2026-11-02 刷新，细节证据在 v1x 阶段存档；世界/渲染侧 = `docs/MECHANICS_WORLD.md`） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ worldgen 参数；配置/存档/资产纪律 = `docs/CONTENT_GUIDE_CONFIG.md` | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（426 条，都踩过；pre-3.x → `docs/archive/PITS_PRE3X.md`） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃；pre-2.0 已修复索引 → `docs/archive/PRE2.0-FIXED-INDEX.md`） | 阶段验收前后 |
| [docs/archive/v0x/ROADMAP.md](docs/archive/v0x/ROADMAP.md) | 0.x 时代 Phase 0–29 规划（已归档） | 了解历史 |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | 版本历史（3.x 时代逐版明细，v3.0.0 起；2.x 全两批 → `docs/archive/v2x/CHANGELOG.md`、v1.x → `docs/archive/v1x/CHANGELOG.md`、v0.x → `docs/archive/v0x/CHANGELOG.md`） | 查某版改了什么 |
| [docs/BACKLOG.md](docs/BACKLOG.md) | **3.x「世界扩展」任务表（执行权威）**（11-04 二批收官晋升 + 逐项定案 + 同日重编号【原 4.x 命名改 3.x】：6 项全保留 + 用户诉求 **V3-7 存档 gzip（默认开 = format 29）先执 = v3.1.0**，其后 V3-1 下界维度锚拆分立项；2.x 全任务表 → `docs/archive/v2x/BACKLOG.md`、v1.x 表 → `docs/archive/v1x/BACKLOG.md`） | 取下一个任务 |
| [docs/archive/v1x/](docs/archive/v1x/) | 1.x 阶段细节存档（181 份 phase，只读历史）+ HISTORY.md 全史 + v1x/CHANGELOG.md + v1x/BACKLOG.md | 深挖 1.x 某阶段 |
| [docs/archive/v0x/](docs/archive/v0x/) | 0.x 时代文档（ROADMAP 规划 / DISCUSSION 记录 / v0x/CHANGELOG.md） | 深挖 0.x |
| [docs/archive/v2x/](docs/archive/v2x/) | 2.x 阶段细节存档（20 份 phase，全两批，只读历史）+ v2x/HISTORY.md 全史 + v2x/CHANGELOG.md + v2x/BACKLOG.md | 深挖 2.x 某阶段 |
| `docs/archive/phase-3.x.0.md`（新阶段放 docs/archive/） | 3.x 阶段细节存档（收官后沉 `docs/archive/v3x/`） | 深挖某阶段 |
| `opencode-sessions/SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步（+ kickoff_prompt 启动 prompt；SESSION_CONTEXT_PRE2.0 / LEGACY_STATUS_P30 / SESSION_HISTORY / 门资产 → 同目录 `archive/`） | 任何"继续"会话 |
