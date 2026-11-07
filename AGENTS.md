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
SDL 三大布局归一化单表 + 首个实际 sim bump 1→2）；**v3.15.0 = V3-13 死亡统计屏（门 205，11-06：致死原因行 9 措辞 + 会话统计 5 行 + 字体 +1 字形「于」428）；**v3.16.0 = V3-15 生存/表现小批（门 206，11-06：netherite 火·熔岩免疫【fire_resistant armor-only + 三抑制 + 熔岩死因覆写】+ portal 2 周期纹理 UV 滚动 + 蒸汽粒子 8×8×9 盒）；**v3.17.0 = V3-11a 创造基础（门 207，11-06：G 切换 + 存档 flag format 31 player.creative 恒写 + 全源伤害门 + 饥饿静态满 + 即挖 + 无掉落/无耐久 + 放置不消耗 + 入场飞行；sim 不 bump canary 4 demo EXACT）；**v3.18.0 = V3-11b 创造物品栏（门 208，11-06：E 面板双 tab 物品栏/背包 + 前缀搜索【SDL 文本输入首个打字面，仅创造面板期开】+ 6×4 网格滚轮翻页 + 左键满组/右键单个授予选中热栏位 + 搜索焦点数字语义 + demo author x/y 绝对位与 g 键）= 矩阵 ⬜ 项小→大推进 4/6，V3-11 创造模式全项完成；**v3.19.0 = V3-14a 爆炸机制（门 209，11-06：纯核心 scanExplosion/explosionDamage【局域强度 radius−dist 破块 + 线性衰减整读伤害 + flamable 点火】+ 块属性 blast_resistance【obsidian 3 数据存活】+ Game::explodeAt 接线 + --explode CLI seam【帧 0，--weather 孪生】+ 死因「死于爆炸」+ 字体 +2 爆/炸 430 + boom 音 64；零 format/sim 不 bump/无击退记档）= V3-14 相位 A/4**）；**v3.20.0 = V3-14b ghast（门 210，11-06：flyShoot 悬停射击分支【flying∧shootRange>0 数据门，bat cruise 位级红线】+ explode_radius/shoot_sound 数据属性【ammo 规则豁免爆炸射手 + init wav 交叉检】+ 火球 = Projectile 复用三爆炸接线【stuck 即爆/玩家接触爆/mob 扫描跳过】+ 发光立方渲染 + 内容族【声音 2 件 64→66/池 +2 总 33/atlas 166/膜物品】；**门内捕获 = V3-14a 字体码点笔误修复**【爽 U+723D→炸 U+70B8，九 glyph 重格 +1，top-512 红线 + S1 ≡ base19 字节恒等】；零 format/sim 不 bump canary 4 EXACT）= V3-14 相位 B/4**）；**v3.21.0 = V3-14c piglin 交易（门 211，11-06：barter_input 单输入 gold_ingot + barter_outputs 12 项加权表【MC 比例项目近似，netherite_scrap 1 头奖，数量 [1,64]】+ checkBarter 纯函数 init 交叉检 + tryBarter（tryMilk 范式）+ barterItem() 无状态 roll + **不激怒 = 结构性质**【零 damage 路径】+ barter.wav 67；零 format/sim 不 bump canary 4 EXACT）= V3-14 相位 C/4**）；**v3.22.0 = V3-14d strider 骑乘（门 212，11-06：rideable strict bool + Player m_carried 承载旁路【wish 锁 + move/stepUp 旁路 + integrate no-op + fall 复锚 = 落乘 1.05 落差永不坠伤】+ m_ridden + riderSteer【WASD 直控 + kRideDecel 0.9 漂停 + ambient 流契约 + collidesWithEntities opt-out = 分离 pass 不顶坐骑】+ ride pass 单收口【落乘 6 条件 + 承载 snap 背位 1.05 + steer 推送】+ tryRide（tryMilk 范式）+ Space/LeftControl 落乘【绕双击飞行窗】+ **承载 = 会话态零 format**【mob 不入档 = 唯一自洽解】+ 渲染零改动；MC 偏离 6 记档【无 saddle/WASD 直控/无水伤下沉/原地落乘/无脚步音/站姿背位】；零 format/sim 不 bump canary 4 EXACT）= V3-14 相位 D/4 全 4 相位收官**）；**v3.23.0 = V3-10a 末地维度框架 + 真传送（门 213，11-07：项目第三维度 = 下界维度基建第二次消费【end_gen 纯函数 + EndDim 12 成员镜像 + 33 分派点三值化】+ 入口 A-lite【双 stateless frame 块 + 数据谓词 end_frame/end_frame_eyed/end_portal + 5×5 水平环纯函数族 + ender_eye 物品 + 嵌眼交互 12 全 eyed 激活】+ 最小末地【120×120 台地 + 原点出口门常活 + 虚空 + 入场平台 5×5 obsidian 每次写入，偏离 MC (100,49,0)】+ 传送新函数【1:1 无 8:1 / 返程出生点床校验 / 60f 倒计时 + 900f 冷却复用 / travelPortal 原文零触碰】+ 渲染 6 点【暗紫黑 clear + 雾 8..48 + inOverworld 门族 + F3 +end + ambience 第 5 上下文 + **u_ambient/u_caveAmbient 双 floor 0.25 按维度**】+ **format 32**【dimension 词表按 format 分词 + end/end_entities 段恒写 + PITS 399 四类降级点同 commit 盘点】；块 127→131 / atlas 166→170 零移位 / item 98→99 / 声音 67→68；门内捕获 ×2【shader 无光分支只读 u_caveAmbient = 单设 u_ambient 全黑 + 场景手术丘陵埋没 = 挖深坑】；sim 不 bump canary 4 EXACT）= V3-10 相位 A/4**）；**v3.24.0 = V3-10b 末地内容族（门 214，11-07：基岩洋全维 y[0,4]【下界地板镜像，跌落死因 Void→Fall + 珍珠逃生面】+ 岛群 pass【lattice 16 p0.15 独立盐，r 5..12 顶 44..56，顶 2 层 + 下锥 2 层悬浮岛 + air-only 谓词 + 中央足迹护栏 + 跨 chunk 外扩 rMax】+ chorus 岛顶装饰【独立盐 p0.6，cross stateless + chance_drops ≈ MC，生长不做记偏离】+ biomes_end 单群系池【10d 缝】+ end_stone_bricks【2×2 配方 count 4 MC 精确】+ **末影珍珠全链 = 项目首个投掷物**【throws_teleport strict bool + 双音配对 + one-right-click 8 面互斥 + material-only + init 交叉检；挂点潜行旋转后 placeBlock 前；投掷者速度并入 + 30 b/s MC 绝对值 + 冷却 60f；Projectile 尾参 teleports + stuck 复用 → pearlLanding：catchUpDisk + 2.5 球面均匀偏移 counter 流 + hasRoomAt 上探 3 + fall 复锚消坠伤 + 落乘 + probe 全败吞没 + 无自伤【MC 5HP 偏离】+ 0.625× 青色发光立方渲染 + 2 wav 68→70】；format 32/sim 2 不变 canary 4 EXACT；门内捕获 = lattice 位切 int 截断负值【模先于转换】）= V3-10 相位 B/4**）；**v3.25.0 = V3-10c 末影人（门 215，11-07：End 首个 mob = neutral + 瞬移 + 携格【数据全旗核心零物种 switch】：注视激怒 eye_contact_range 48 = kEndFogEnd 定案【5 连续帧零流计数眨眼复位 + 全 AABB 瞄准射线 + 瞄准块 t 严格遮挡】→ 30s 窗复用 / 瞬移 interval 2-6 严格配对存在性 = 能力门【受击 100% damage 只置旗 + update 固定槽帧序钉 + 流契约 8 候选 × 3 抽 + 1 重掷 = 25 抽/尝试全败亦消耗 + 平静 ±16/±8 自位锚 / 激怒 ±8/±4 朝玩家 12 点锚项目缩放 + 候选须空旷 ∧ 16 格内有地面【下探穿固体虚空重写】+ fall 复锚 + 落点音 + 全败 swallowed 行】/ 携格 10c-lite【carries_blocks + 块 carriable 双严格旗缺省冷 + stateful carriable init 点名拒 + 集 {endstone, chorus_plant} + 摸每帧 4×3×4 dy 含脚下 x→y→z 定序 + 首格遮挡 + 放 6000 帧全局长拍 + 死亡 100% 掉所携 + 0.9× 头顶立方双 pass】/ End 池 mob_spawning_end.json 单种族 = 10a 两相位槽消费 + 掉落 pearl 0-1 + eye 0-1【生存唯一来源，10a/10b 记档闭合】+ xp 5 + 声音 70→71 仅 enderman_teleport【ambient/scream/stare/endermite/定身/跨维池后移 10d】；format 32/sim 2 不变 canary 4 EXACT / atlas 172→174 零移位 / 块 133 不变 / 门内捕获 7 坑全 PITS 407【demo 槽不回写假测量 / 火脚级重叠 / 拾格带含脚下 / 遮挡首格语义 / 下探虚空 / 截图 679×744 / 按住 LMB 挖掘】）= V3-10 相位 C/4**）；**v3.26.0 = V3-10d1 末影龙核心 + 首杀闭环（门 216，11-07：矩阵唯一真大项首相位 = 龙 boss【第 16 mob 纯数据：200 HP + 8 组数据旗 + 8 盒 8×4 双翼，核心零物种 switch】：dragonFlight 三态复用 MobState【runAi flying 分派最顶纯新分支，ghast/bat 位级红线；盘旋定心 30【门内裁决 40+pull→30】/侧移 3.5s 火球 2.5s/驻坐出口锚 咆哮→脚位吐息云 + 三起飞】+ 流契约每帧 ambient 1 抽 + 跨圈恰 2 抽【strafe 命中亦消费 u2 = 门内重构纯函数 rollDragonLapDecision】/ 水晶 = 新块非实体【6 固定槽 4 塔顶 + 2 出口 D3 非 MC 10 + 破块爆半径 3 无连锁结构性质 + 箭扫掠碎 + 治疗源晶碎反伤 10 + 晶上永久火 D9 + 普查卸载无关【门内捕获 raw getBlock 0→6 漂】】/ 龙息云 = 零物理新实体【leavesCloud 尾参缺省 false 位级红线 + dragon_breath 3HP/s 玩家-only 效果 11→12】/ **首杀闭环 = format 32 零 bump 最强形**【击败态 = 蛋块世界态 (0,54,0) + 升天 60f 飞 + 180f 悬停 + 23 orb = 22×9+2 = 200 整 + wail + 240f 延迟门；重载 = 蛋在无复生；末地 orb/云存档静默跳过偏离记档 format 33 后移 d2】/ 血条 renderUi 段【186×8 十段 + 「末影龙」字体 +3 = 433 扩展区 528/528 满 + margin 8→24 门内裁名修 + 2s 淡出】/ **--dragon-damage CLI seam**【--explode 孪生——龙战斗 demo 不可手编 = 耦合系统定案】/ 生成门 playerInEnd∧mobs∧无龙∧非蛋→60f→(0.5,58,0.5) kSaltDragon；块 133→135 / mob 15→16 / atlas 174→179 零移位 / 声音 71→75 / 效果 11→12 / 配方 113→114；门内捕获 ×8【init 交叉检顺序 / 数值累加器泄漏 / demo 存档红线 / 流送依赖锚点 / -1 哨兵 delta / 同帧 spawn+kill 死循环 / 血条 margin / CJK 边界】= PITS 408；sim 2 不 bump canary 4 EXACT）= V3-10 相位 D1/2**）；**v3.27.0 = V3-10d2 召唤与复杀（门 217，11-07：crystal_base 块旗 4 块 + requires_crystal_base = placeBlock 数据门【end_crystal 生存可放置】/ 4 出口面晶 ∧ 无蛋 ∧ 无龙 → 60f 倒计时【面晶 ±X = arena 晶算数 + ±Z worldgen 缺席；任一损失取消】→ 4 晶引爆【相邻爆波及 = MC 同爆读】+ 蛋计时清除 + 缺位塔晶回写 + 显式 60f 复生 / **败亡旗**【首杀置位重载复位——无蛋 ≠ 首访，门内捕获破蛋 60f 无召唤复生】/ 蛋 60s 会话重生【玩家破蛋 → 3600f 回写，重载复位 D7】+ 蛋 blast_resistance 3【晶爆 r3 存活】/ **复杀 XP = 8**【单 8 值 orb，D5 1/24×200；首杀 200 burst 不变】/ --dragon-damage 每生成施加 / F3 第 21 行 dragon【全英文零新字形，528/528 满区不碰】；format 32 不变全会话态 / sim 2 不 bump canary 4 EXACT；门内捕获 5 坑全 PITS 409【负 cell 中心 -N+0.5【10d1 词表负侧偏格静默修】/ 无蛋 ≠ 首访 / demo 同帧死点击 + hold 续取目标 / 旋转 dy 累计 / /tmp 路径重写】）= V3-10 相位 D2/2 全 4 相位收官**）**）——**3.x 矩阵
18 项 = 原 11 项全 ✅（v3.1.0–v3.11.0 门 191–201）+ 矩阵后 v3.12.0
（门 202）/ v3.13.0（门 203）/ v3.14.0（门 204）/ v3.15.0（门 205）
/ v3.16.0（门 206）/ v3.17.0（门 207）/ v3.18.0（门 208）/ v3.19.0
（门 209）/ v3.20.0（门 210）/ v3.21.0（门 211）/ v3.22.0（门 212）
/ v3.23.0（门 213）/ v3.24.0（门 214）/ v3.25.0（门 215）
+ 11-06 扩容 7 项（V3-8b ✅ v3.14.0 + V3-13 ✅ v3.15.0 + V3-15 ✅
v3.16.0 + V3-11 ✅ v3.17.0/v3.18.0 + V3-14a ✅ v3.19.0 + V3-14b ✅
v3.20.0 + V3-14c ✅ v3.21.0 + V3-14d ✅ v3.22.0【V3-14 全 4 相位完成】+
V3-10a ✅ v3.23.0 + V3-10b ✅ v3.24.0 + V3-10c ✅ v3.25.0 + V3-10d1 ✅ v3.26.0 + V3-10d2 ✅ v3.27.0【V3-10 全 4 相位完成】+ V3-12a~c ⬜【大项已
相位拆分 V3-10a~d / V3-11a~b / V3-12a~c / V3-14a~d，BACKLOG 待执行
项明细】）**

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
- 测试 **2964/2964**（2777 基线 + V3-8a 手柄 open 修复 1【用户 11-06 并行提交 d5a5ea6】+ v3.13.0 demo sim 族 6 + v3.14.0 V3-8b 手柄映射族 3 + v3.15.0 死亡统计屏族 5 + v3.16.0 V3-15 小批族 3 + v3.17.0 V3-11a 创造基础族 6 + v3.18.0 V3-11b 创造物品栏族 10 + v3.19.0 V3-14a 爆炸机制族 10 + v3.20.0 V3-14b ghast 族 12 + v3.21.0 V3-14c piglin 交易族 14 + v3.22.0 V3-14d strider 骑乘族 9 + v3.23.0 V3-10a 末地族 29 + v3.24.0 V3-10b 末地内容族 14 + v3.25.0 V3-10c 末影人族 31 + v3.26.0 V3-10d1 末影龙族 29 + v3.27.0 V3-10d2 召唤复杀族 5），`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告；
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
| [docs/PITS.md](docs/PITS.md) | 坑位合集（409 条，都踩过；pre-3.x → `docs/archive/PITS_PRE3X.md`） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃；pre-2.0 已修复索引 → `docs/archive/PRE2.0-FIXED-INDEX.md`） | 阶段验收前后 |
| [docs/archive/v0x/ROADMAP.md](docs/archive/v0x/ROADMAP.md) | 0.x 时代 Phase 0–29 规划（已归档） | 了解历史 |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | 版本历史（3.x 时代逐版明细，v3.0.0 起；2.x 全两批 → `docs/archive/v2x/CHANGELOG.md`、v1.x → `docs/archive/v1x/CHANGELOG.md`、v0.x → `docs/archive/v0x/CHANGELOG.md`） | 查某版改了什么 |
| [docs/BACKLOG.md](docs/BACKLOG.md) | **3.x「世界扩展」任务表（执行权威）**（11-04 二批收官晋升 + 逐项定案 + 同日重编号【原 4.x 命名改 3.x】：6 项全保留 + 用户诉求 **V3-7 存档 gzip（默认开 = format 29）先执 = v3.1.0**，其后 V3-1 下界维度锚拆分立项；2.x 全任务表 → `docs/archive/v2x/BACKLOG.md`、v1.x 表 → `docs/archive/v1x/BACKLOG.md`） | 取下一个任务 |
| [docs/archive/v1x/](docs/archive/v1x/) | 1.x 阶段细节存档（181 份 phase，只读历史）+ HISTORY.md 全史 + v1x/CHANGELOG.md + v1x/BACKLOG.md | 深挖 1.x 某阶段 |
| [docs/archive/v0x/](docs/archive/v0x/) | 0.x 时代文档（ROADMAP 规划 / DISCUSSION 记录 / v0x/CHANGELOG.md） | 深挖 0.x |
| [docs/archive/v2x/](docs/archive/v2x/) | 2.x 阶段细节存档（20 份 phase，全两批，只读历史）+ v2x/HISTORY.md 全史 + v2x/CHANGELOG.md + v2x/BACKLOG.md | 深挖 2.x 某阶段 |
| `docs/archive/phase-3.x.0.md`（新阶段放 docs/archive/） | 3.x 阶段细节存档（收官后沉 `docs/archive/v3x/`） | 深挖某阶段 |
| `opencode-sessions/SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步（+ kickoff_prompt 启动 prompt；SESSION_CONTEXT_PRE2.0 / LEGACY_STATUS_P30 / SESSION_HISTORY / 门资产 → 同目录 `archive/`） | 任何"继续"会话 |
