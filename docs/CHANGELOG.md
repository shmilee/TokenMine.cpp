# 版本历史（CHANGELOG）

> 本文件只留 **v2.11.0 起** 逐版明细（2.x 第二批 = 原 3.x 八项并入，
> 11-03 用户定案；V2-11~V2-18，原 4.x「世界扩展」= 下一大版本 v4.x）。
> v2.x 第一批（v2.10.0→v2.0.0）→ `docs/archive/v2x/CHANGELOG.md`；
> v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x → `docs/archive/v0x/CHANGELOG.md`。逐版细节（机制/证据/
> 偏离/性能 A/B）见对应时代 `docs/archive/*/phase-*.md` 阶段存档。
> README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本 /
> v2.10.0 第一批收官 tag / v2.18.0 二批收官 tag）。

## v2.15.0（2026-11-04）— V2-14 蝙蝠（项目首只飞行 mob：悬停巡航 AI + 洞穴生成 + squeak 氛围音）（门 186 全 PASS）

二批 ENT 行（「中」）；MC 机制 11-04 wiki 核定（Bat 页）；**零
format**（mob 不持久化 PITS 375③ = savenorm EXACT 最强形）：
**数据**（bat.json 全数据驱动）——HP 6 / 被动全 0 /
spawn_max_light 3（洞穴语义 MC 精确：夜底光 7>3 露天不可生）/
无 drops / xp 0 / **三新严格字段** flying + spawns_in_air +
ambient_sound/ambient_interval（唯一硬互配 flying⇒spawns_in_air；
孤 interval 死数据点名拒；音名 init file-based 交叉检）+ model
**0.9×0.5**【偏离 1：MC 盒 0.5×0.9 竖薄容不下展翼 AABB】身/头/
**双翼 4 box**（±0.275 = 0.9 极限）+ swings 双翼 0.9 rad 反相扇翅
（chicken 翅盒不摆先例的反例）+ mob_spawning 8 词表【偏离 2：入
被动池 max_mobs 4，MC ambient 独立 cap】+ bat.png（深褐黑 + 翼膜，
既有 24 mob tile md5 全不变）+ atlas 119→120 零移位 +
**squeak.wav**（900→1400Hz 滑升 + 30Hz 微颤 0.12s vol 0.1 类 = MC
ambient 0.1，既有 41 wav md5 全不变）。
**飞行 AI**（runAi 顶部 def.flying 纯新分支，既有 7 mob 族零改动
全过 = 位级红线）——航向 = per-mob 流每 wander_interval 2s 重掷 +
数据 speed 1.0 巡航 / **垂直正弦悬停**（0.2×sin(2π·frame/60) 帧锁
定，bob 初相构造流首抽【flying 才消费】）/ integrate 重力关 + 终
端豁免 + 流体旁路【悬停穿水实现级偏离】/ **撞壁 = 下帧航向重掷**
【偏离 3 无寻路】/ 玩家碰撞保留【偏离 4：MC JE 不交互】/
knockback 0.3s 回巡航 / **ambient squeak** 每帧 p=1/(3×60) 流 roll
空间化（无 ambient mob 流零消费）。
**生成 in-air 分支**（findMobSpawnCell spawnsInAir 尾参默认
false）：顶向扫描 + box 全空气无流体 + 光 ≤3 + **跳过 floor 带
（任何 Y = MC）** + 出生位盒底格中心。消散 40【偏离 5：MC 32】/
倒挂不做【偏离 6】/ takeoff 音不做【偏离 7】。教程 "kill.bat"
入闭合格言。2356/2356（+26：解析 5/内容钉 2/生成格判 8/
flyCruise 帧钉 8【含 900f 密封舱双跑位等】）、banner v2.15.0。门
186：S1 **结构免疫第 67 次免重锚 + savenorm EXACT**（≡ 锚 179027
逐字节；**门协议再证 PITS 379①**：编排者首跑又误用 v2.13.0 树当
旧侧 3 键假象，cc05d53 重建 banner 实证后 EXACT）/ S2 洞穴无输入
E2E【氛围 mob 范式：证据 = log 层 + 像素孪生；密封暗厅 seed 3355
（f60/90/120/150 四 pick 全 bat）：new ×2 log 4 蝙蝠 spawn 位置逐
位同 + 截图逐字节恒等；对照 mobs-off ×2 恒等零 spawn；new vs 对照
diff 43425 px **全褐色蝙蝠纹理**（4 只 bob 不同屏高 y9-220，近景
每只 ~10k px）+ 泄漏审计 HUD 区 0/时钟 bbox 0/非褐色 0；old
v2.14.0 同场景（format 28 双向可载）7 词表零 'bat' 行 = 形状互斥
最强形】/ S3 / S6 6/6 点名 / 性能 A/B 同世界同成本【主对 9/1/3 逐
位 + 终档 16/16 36/36 恒等【mob 不入档】+ 洞穴自成本 ents 0.093/
0.089 同档 = 飞行 AI 不比走地贵】。PITS 380（spawn 成功行无
'pick' 正则陷阱 / 门场景洞穴须密封【亮洞口 = 流送重光照收敛时序
依赖】/ CAVE 自成本须场景专属 config seed / 飞行 diff 带随 bob =
色相 + HUD 区审计非固定带）。详见 docs/archive/phase-2.15.0.md。

## v2.14.0（2026-11-04）— V2-13 床（15 色染色族 + 睡觉跳夜 + 重生点 + format 28）（门 185 全 PASS）

二批 SURV 行（BACKLOG「中」）；MC 机制 11-04 wiki 核定（Bed 页）：
**15 色床**（dye_base/dye_color Phase 41 机制，wool 15 色词表同款
无 lime，white = base；RMB 持染料染色 = 既有路径免费）+ 确定性纹理
15 tile（彩色毯 + 头端白枕，atlas 104→119 零既有移位 md5 实证）+
配方 15（shaped 3 同色 wool + 3 planks = MC 精确）+ 新 sleep.wav
（确定性合成）；**Slab 半格形**（MC 1×2 两格无项目先例 = 记档偏离
1）/ hardness 0.2 MC 精确 / cloth 音族。**睡眠三门定案序**
（core/bed 纯函数 + game 薄适配，RMB 链纯插入面板分支后染色前）：
b 距离 ≤3.0（MC 精确，脚心 vs 床格中心）→ c hostile 盒 水平 ±8/
垂直 +5/−1（数据旗判据无物种 switch，被动/掉落物/箭豁免）→ 设重生
点（MC：RMB 恒设点夜或日）→ a 夜窗 [kDuskEnd 0.55,1)∪[0,kDawnStart
0.05)（MC sky≤11 略窄 = 偏离 3）：夜内 = **即时跳夜**（偏离 2：
setTimeOfDay(kDawnStart) 06:00 黎明 + 雨/雷 weatherInitialState 纯
重置 CLEAR + sleep 音空间化 + "sleep" 教程词入裸词表）/ 夜外 = 仅
设点；门 b/c 拒 = 全拒含设点（偏离 6）。**重生点**：Player 新
spawnPos 访问器 + **format 28** player.respawn int 三元组恒写（27
携键拒 / 28 缺键拒 / 27 载 = 哨兵 → applySave 覆写世界出生点；分量
非整数 / y∉[0,128) 拒）+ **respawn 验床**（R 键收口 respawnPlayer：
纯函数 bedRespawnValid = 该位任意床族块 + 房间空间【床格支撑豁免】
→ 床位，否则世界出生点回退 + 重新钉位 = 床破坏永久回退，MC"仅
respawn 时查"同款）。mob 炸床不做（偏离 5：项目无爆炸机制；门 c
覆盖主语义）。2330/2330（+22 TEST：块 15 内容钉/配方 15/三门边界/
验床 4/save 28 全形/sleep.wav 装载/教程词）、banner v2.14.0。门
185：S1 **结构免疫第 66 次免重锚**（≡ 锚 179027 B 逐字节——床不入
worldgen + RMB 门；savenorm 恰 {format 28, respawn}）/ S2 三臂输入
E2E（**夜跳**：t0.70 RMB → time 0.0796 + respawn = 床格 / **日设
点**：t0.30 不跳 + 设点 / **mob 拒**：scratch spider 恒停格置床 +
aim 重几何【slab 半高更陡 pitch】→ 不跳 + respawn = 世界出生点 +
hp 22/60 交战实证；旧侧全 format 27 无 respawn 形状互斥）/ S3 / S6
5/5 点名（缺染色变体/dye+interact 互斥/hardness −1/27 携 respawn/
28 缺）/ 性能 A/B 同世界同成本（9/1/3 逐位 + 16/16 36/36 恒等 +
respawn 唯一预期 diff + 预置床场景零成本）。PITS 379（旧侧参照树
= 前一版本 banner 实证【首跑误用 v2.12.0 树 = 3 键假象】/ 单行
local 展开序 / slab 瞄准 + capture 单击不破 0.2 床）。消费
"v1 无床" 注释记档。详见 docs/archive/phase-2.14.0.md。

## v2.13.0（2026-11-04）— V2-12 附魔系统（7 附魔 + 附魔台/书架 + 确定性 offer + format 27）（门 184 全 PASS）

二批 CRAFT 行（立项定案 = **真附魔台范式**，消费三处「附魔台替身」
记档：master_bow P82 / 弓强化 P77-79 / 信标 P91）；MC 现代数值
11-04 wiki 核定入稿：
**7 附魔**（assets/data/enchantments 数据驱动，严格解析点名）——
锋利 axe（近战 +(L+1) 点，MC 精确区间）/ 保护 armor（+4L 护甲点，
共享 20 点 cap 偏离）/ 效率 pick·shovel·axe（正确工具 +（1+L²）挖
速，MC 精确）/ 耐久 全 durability（每损点掷骰跳过：工具 L/(L+1) /
护甲 2L/5(L+1) = MC 1.21 remove_binomial 精确，无帧依赖确定）/ 力量
bow（shoot_damage +L）/ 速度 bow（shoot_speed +L，Quick Charge 蓄
力 = 弹速范式偏离）/ 无限 bow（发射不耗尽最后一支，∨ 字段）。
物品侧 enchant_class + enchantability 严格互配（12 工具 15/5/14 +
9 护甲 15/9/10 + bow 1；仅基础 bow，变体弓保持替身）+ lapis
enchant_material 门。**确定性 offer**（core/enchanting 纯函数全整
式禁 double）：种子 = worldSeed^hash3(台位)^enchantability^xpSeed；
xpBase/三槽成本/lapis 1-3/eCost 含 randTri 整式/选择循环（权重 +
区间取最高级 + incompatible 移除 + v%(eCost+150) 继续判定）= MC
精确；bookshelfCount 15 环×双层 cap 15（无视线偏离），**仅面板开
时求值**。**新块 ×2**（附魔台 hardness 5/light 7/interact
"enchant"/恒掉自身偏离/full_cube 偏离 + 书架）+ **配方 ×2**（lapis
代 book 偏离）+ tile 102→104 零移位。**format 27**：player.xp_seed
16-hex 串恒写（64-bit 无损偏离；26 迁移 = worldSeed^init）+ 物品
enchants 三处（omit-when-empty）+ 四拒（26 携 xp_seed / 26 携
enchants / 27 缺 xp_seed / 交叉校验 level>max·类不兼容·未知 id）。
**面板**（furnace 范式全克隆）：RMB 开 + pick-and-place carry + 3
offer 行「名 罗马 成本」+ 青金石槽门 + 关归还 + "craft" 音（偏离）
+ 字体 +4 字 锋利限**魔**（首交漏标题字「魔」门内补；atlas
256×2080→2112，顶 512 行逐字节不变；kCjkGlyphs 423→427 码点序
re-cell）。2308/2308（+59）、banner v2.13.0。门 184：S1 **结构免
疫第 65 次免重锚**（新侧 ×2 ≡ 现锚 179027 B 逐字节——atlas 高度
v-UV 亚 LSB 位移零像素差实证；fixer 中间态"需重锚"= 环境伪影复证
作废 + savenorm 恰 {format, xp_seed}）/ S2 面板输入 E2E（场景 B=8
+ xp T(30)：RMB 开 → 拾斧放槽【offer 效率 I 6/III 9/IIII 17 种子
确定】→ 青金石 → 点 offer → 取回 → 终档 **斧 {效率:1} + xp
1395→828 = T(24) 恰扣 6 整级余量保留 + seed 推进 + lapis 10→9**；
旧侧 format 26 无 enchants/xp_seed 形状互斥；面板开态 ×2 逐字节恒
等）/ S3 / S6 12/12 点名 / 性能 A/B 同档（9/1/3 逐位 + 16/16 36/36
恒等 + 预置台场景零成本）。PITS 378（场景块 AABB 重叠顶高 / 面板
carry 模型 / xdotool --pid / savenorm 官方工具）。详见
docs/archive/phase-2.13.0.md。

## v2.12.0（2026-11-04）— V2-11 XP 系统（经验球 + 等级曲线 + 三发放源 + format 26）（门 183 全 PASS）

第二批 SURV 行首项（实现源 = fixer 43 文件半成品 11-03 中断取消 →
git stash → 11-04 pop 零冲突恢复至 v2.11.0 之上 + 编排者修 3 处 WIP
编译破损 + 1 处 save.cpp 潜在 bug）：
**XP 曲线**（core/xp_curve 全整数，MC 现代公式 11-03 wiki 核定）——
needed 2L+7/5L−38/9L−158 + total L²+6L/(5L²−81L+720)/2/
(9L²−325L+4440)/2 + 逆 = 整数二分（往返构造性恒等，无浮点）；玩家
单整数 pool（player.xp 恒写），**等级永不存储** = 派生单源真值。
**经验球**（entity/exp_orb）——值 1..9 无合并无分裂【记档偏离】+
ItemEntity 同款物理纪律 + V2-6 磁力同款帧锁定拉取（2.5）+ 1.0 内
pickup pass 收 → pool + "pickup" 音色。
**三发放源**（数据驱动严格解析 [0,9] 点名拒）——mob 击杀
（hostile 5 / passive 2 = MC 1-3 中值）/ 熔炼完成（item smelt_xp
**默认 1 统一**【MC 分数表 = 记档偏离】）/ 挖掘完成（block mine_xp：
coal_ore 1 / diamond_ore 5 / iron_ore 0 / gold_ore 0 = 现代 MC 1.17
raw 化值）。
**format 26**——player.xp 恒写 + entities 第二 kind type∈{item,xp}
三向键纪律（V2-3 同款：pre-26 载迁 0 / 25 携 xp 点名拒 / 26 缺键
点名拒）。**save.cpp 潜在 bug 修复（PITS 361 违例第二实例）**：
tutorial 白名单 isV25 = fmt >= kSaveFormat → 字面量 >= 25（bump 后
format-25 文件误拒 series；UX-8f 1667 行同款先例注释佐证）。
**HUD**——热栏下 XP 条 + 等级数（全整数数学，恒可见 = 门 183 唯一
像素源；零新字形）+ F3 xp 行。2249/2249（+42：xp_curve / exp_orb /
SaveFormat26Test 族）、banner v2.12.0。门 183：S1 **base18 第 4 次
重锚论证**（旧侧 ≡ 现锚 178676 B 环境干净 / 新侧 ×2 恒等 / diff
2031 px 全落底部 XP 条带 bbox (30,714)-(647,740) 零泄漏 / savenorm
仅 {format, player.xp} 预期键 → **新锚 179027 B**）+ S2 三发放源
（熔炼无输入 xp=1 / 击杀 scratch spider 短按住 + W 步行拾取 xp=5
【orb 距静止玩家 2.54 > 磁力 2.5 新坑】/ 挖掘 xp=1；旧侧全 format 25
无 xp 键形状互斥【场景 py25 降级 = PITS 377】）+ S3 + S6 四项点名
+ **性能 A/B 同世界同成本**（changesApplied 9/1/3 逐位 + 终档
16/16 36/36 恒等 + 熔炼场景球全生命周期同档不可测）。PITS 377（旧侧
新格式场景 init 拒 / orb 磁力 3D 边界）。详见
docs/archive/phase-2.12.0.md。

## v2.11.0（2026-11-03）— V2-19 视角/移动/时间显示批（滚轮缩放 + 玩家飞行 + 24h HUD 时钟）（门 182 全 PASS）

用户 11-03 实机诉求三项（UX 行续编号 V2-19，**提前执行 = 版本
v2.11.0**——任务号与版本号解耦，V2-1/V2-2 同款先例）。全会话态
= **零 format / 零 config 新键 / 零新数据**：
**① 第三视角滚轮缩放**——SDL 滚轮事件入 Input 帧态（无输入恒 0
免疫根）+ session distance clamp 2.0-8.0 step 0.5（默认
kThirdPersonDistance 4.0 = **resolveThirdPersonCamera 默认参数
位级不变**，既有 16 单测零改动）+ 仅 Third 模式 + ThirdFront
aimReach = distance + kReach 联动（MC 前视约定泛化；**瞄准射线仍
自眼位 UX-8g 零触碰**）+ F3 `cam <Mode> D x.x` 读法；MC 原版无
滚轮缩放 = mod 惯例记档。
**② 玩家飞行（"中国人会飞"）**——双击 Space 0.3 s real-dt 窗
（CRAFT-7 先例纯函数 core/fly，第二击吞跳跃，仅 locked + 存活）；
飞行 = 重力跳过 + Space 升 / Shift 降 4.0 b/s（潜行机制照常）+
水平 ×1.6（6.88 b/s > 冲刺 5.6 = 快于走路诉求）+ 碰撞照常 +
**坠落累加器飞行中每帧归零**（切出帧自切换高度计坠无爆发伤害）
+ 水/熔岩自动切出（记档）+ 不存档（记档，省 format）+ 死亡重生
复位。
**③ 24 小时 HUD 时钟**——纯函数 `DayCycle::clockString`：
`hour24 = fmod((t − kDawnStart 0.05)×24 + 6, 24)` **与 ramp 边精确
对齐**（晨 06:00 / 午 12:00 @ t0.3 / 昏 18:00 / 夜 00:00 @ t0.8；
ulp 钉 clockString(0.0) = "04:47"）+ renderUi 右上角恒可见
（MC 无 HUD 时钟 = 记档新功能）+ ':' 字形 ASCII 行在列实证（零
字体扩展）。2207/2207（+26）、banner v2.11.0。门 182：S1
**base18 第 3 次重锚论证**（旧侧 ≡ 现锚 178148 B 环境干净 / 新侧
×2 恒等 / 新旧 diff 609 px 全落右上角时钟区 bbox (544,12)-
(657,29) 零泄漏 / savenorm EXACT → **新锚 178676 B 采用**）+ S2
三判别（zoom：new 视差全框 diff vs old wheel 无动作 None = 互斥 /
flight：new 终档 y 62.13 升空 vs old y 54.00 跳跃落地 = 互斥 /
clock = S1 区）+ S3 + S6（命名单测钉族）+ **性能 A/B 同世界同
成本**（changesApplied 9/1/3 逐位 + 终档 16/16 36/36 集合恒等 +
PERF-WIN 全指标同档——时钟 +1 文本 draw 成本不可测）。门协议
发现 PITS 376（输入 E2E：capture 重试环【DEBUG 级 captured 假象】
/ 帧限收尾禁 SIGTERM【截图 + kill 超时壳双坑】/ pgrep 轮询假
PASS 源）。详见 docs/archive/phase-2.11.0.md。

## v2.x 第二批（V2-11~V2-18，进行中）
（V2-15 染色羊起，逐版条目入此。）

