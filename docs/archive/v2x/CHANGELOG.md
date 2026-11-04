# v2.x 版本历史（已归档，全两批）

> 2.x「上手与体验补全」期**收官（11-04）**归档：第二批（V2-11~V2-18 = 原
> 3.x 八项并入，11-03 收官日用户定案）+ 第一批（V2-1~V2-10）。逐版细节
> （机制/证据/偏离/性能 A/B）= 本目录 `phase-2.x.0.md` 阶段存档（20 份）。
> 3.x 时代（下，原命名 v4.x = 11-04 同日用户重编号定案改 3.x）=
> `docs/CHANGELOG.md`；v1.x → `docs/archive/v1x/CHANGELOG.md`；
> v0.x → `docs/archive/v0x/CHANGELOG.md`。

## v2.19.0（2026-11-04）— V2-18 AO 环境光遮蔽（经典 MC 顶点 AO bake 入既有光 attribute；**二批收官**）

二批 PRE 行收官项（消费「按面采样光照，无 AO」记档 [Phase 7]；MC 机制
11-04 wiki 核定：smooth lighting 默认 ON Beta 1.3 引入，AO = 按面前方
对角格暗角，water surfaces 不受影响）：**纯函数族 world/ao**（viewer
侧 C+n 基 3 邻格查询 = 与顶点共享边的前方三格【e 向 = 背离网格单元于
顶点——MC 经典观感三具名场景钉：台阶竖面顶沿暗/凹槽角 0.3/抬升地板边
暗】× opaque ∧ 高度带【剔面同 opaque 旗 = 项目自洽规则：满块/stairs/
wall/fence [0,1]（偏离 2：斜面不精化，高沿过遮记档）、slab 0→[0,0.5]/
1→[0.5,1]/≥2→[0,1] = solidRange 同款 kSlabHalf 单常量、越界不遮】闭区
间含端点【半块同款 MC「顶点只被同高度邻格遮」：底 slab 顶面中平面恒
不遮、顶 slab 顶面行上正常、接缝 0.5 双遮】× **4 档 MC 精确系数**
kAoFull 1.0/kAoOne 0.75/kAoTwo 0.5/kAoCorner 0.3【side1∧side2 双邻特
例 = 0.3 非计数 2】+ smooth = GPU 双线性【偏离 1：4abcd 中心修正项省
略——attribute 模型不可重构；夜间 retreat 作用于 AO 后 sky 曲线微差同
条记档】）；**编码 = bake 入既有 skyLight/blockLight 顶点 attribute**
（零 shader 变更/零顶点格式变更 15 float/零新 uniform/per-draw 覆盖代
码零触碰【偏离 3：信标/掉落/XP 球覆盖面 AO 消失 = 特效光覆盖优先，MC
光束亦不 AO】）；**生效面** = greedy 满块（**MaskCell 合并键扩 ao[4]**
uint8 档位精确比较 = 共享边角 AO 异断 run，MC greedy+AO 同款；merged
顶点 i 取端 cell corner i = seed.ao[i]【键强制 run 内全等；ao[4] 序 =
面表 c0..c3 = 1×1 run 发射序单测钉】）+ slab 三态（发射 y 判带）+
stairs（顶点实际 y；斜面 = +Y 面规则 fixer 定案：高沿可遮/低沿恒不遮
= 台阶观感）；**fluid/cross 免 AO（MC 精确：water surfaces 不受 smooth
lighting 影响）+ wall/fence/glassPane 免 AO（偏离 4 薄几何范围外）= 豁
免 6 emitter 顶点写点零触碰**；快照零新依赖（AO 查询 ⊆ 3×3 列快照）；
**render.ao 严格 bool 默认 true**（weatherEnabled 范式：非 bool 点名拒
= S6 臂；config.json 不动）= off 系数恒 1.0 查询不跑 **pre-V2-19 位级
恒等**（fixer 自证：off 60f 截图 ≡ 旧 v2.18.0 二进制同环境逐字节
【本环境锚不复现 = 云/XP 条环境态漂移与 AO 无关，相对口径为位级根】）+
on ×2 恒等 + on-vs-off diff 36462 px 单调变暗审计【99.83% 逐通道
new≤old+1.5；61 px = run 断裂重排 z-fight 换面确定性类别；HUD/准星/
天空 0 diff；世界区 bbox 论证】）。2482/2482（+27：ao 纯函数族 16 +
mesher V218 节 11）、banner v2.19.0、format 28 不变（save smoke 顶层
键闭集钉）。门 190（S1 重锚论证范式 = V2-10 先例 + off 位级证明 +
savenorm EXACT / S2 corner 场景 journal / S6 点名 / 性能 A/B 900f
mesh 期口径 + 密角 600f）= **二批收官**（tag + GitHub release + 4.x 晋升
定案 = 编排者收官动作）。详见 docs/archive/v2x/phase-2.19.0.md。

## v2.18.0（2026-11-04）— V2-17 顶部 slab/双 slab（state 化 + 放置瞄准规则 + 同格合并 + partial-aware 射线）（门 189 全 PASS）

二批 GEN 行（消费「slab = 仅下半块」记档 [Phase 85]）；MC 机制
11-04 wiki 核定（Slab 页：面上半/底面 → 顶 slab、下半 → 底 slab；
同型上下同格 = 双 slab；**双 slab 破坏 = 整块掉 2**）；**零 format
bump**（states = format 15 机制，stone_slab 无 state → 有 state 同
数组新条目）：**stone_slab state 化**（state_count 3：0 底部既有位
级 / 1 顶部 y 0.5..1 / 2 双 slab 满格）+ **新严格字段 drops_count_
by_state [1,1,2]**（长度==state_count/零/非整/无 state 块携带全点名
拒 = 双 slab 破掉 2 MC 材质守恒）；**solidRange 物理扩展**（单 top
锚格底 → {bottom,top} 区间：0→{0,0.5} 位级 / 1→{0.5,1} / ≥2→{0,1}；
床/stairs/满块现值逐位不变红线，消费点 hasRoomAt/moveAxis/stepUp
【顶 slab 顶面恰 1.0 可踏升 MC 同款】/mob 生成 spawnFloorTop 纯函数/
pathfind）；**emitSlab 三态**（state 0 逐语句未动 / emitSlabTop 镜像
底面恒绘对称 / emitSlabFull 满格标准剔，slab pass 内绘制 greedy 不
动；零新剔除规则 = 拼合面无共面双绘）；**放置 slab 规则**（:1173
自称扩展点消费：命中点 = eye+forward×distance，顶面→0 位级根 / 底面
→1 天花板贴放 / 侧面 local-y>0.5→1 否则 0 = slabStateFromHit 纯函数
kSlabHalf 单常量）+ **同格同 id 互补 state 合并 → 双 slab**（仅绕
canPlaceAt solid 拒保 Y+玩家重叠，消耗 1）+ **破坏 per-state count**
+ **蹲旋转 Slab 形排除**（偏离 4：垂直位非朝向，环转不守恒）；
**partial-aware slab raycast**（门机范围缺口补口 = 合并 E2E 可达根：
cell DDA 全遮挡 → 共享 dda walk 子格判定 slabSolidRangeAt：空气半入
= 穿透【内表面 y=0.5 交点命中 ±Y 法线 + t 位值 / 否则继续】实心半入
= 整格现行为 / 双 slab = 整格；块射线 5 消费点 + 桶 ×2 + 骨架 LOS +
第三人称遮挡共享一致零特判；非 slab 格位级不变；**记档行为变更** =
瞄准底 slab 空气半破身后块 MC 同款）；床 15 块 hasState false 位级
不变。2455/2455（+23 V2-17 + 12 raycast）、banner v2.18.0、format
28 不变。门 189：S1 **结构免疫第 70 次免重锚**（≡ 锚 179027 零
diff + savenorm EXACT；fixer 终版二进制自证第 71 次）/ S2 三臂×双侧
（**顶 slab** 石底面瞄准 → state 1【旧侧 legacy 底 slab 互斥】/
**合并** 锚块 +Z 面上半【partial-aware 射线穿 slab 空气半命中锚面 →
target = slab 格】→ state 2 物品耗尽【旧侧 cell DDA 邻格叠第二块无
state = 互斥最强形】/ **双 slab 破 2** hold 3.3s 断点时序【旧侧底
slab 破 1】+ 掉落 2.83>磁力 2.5 留地断言）/ S3 / S6 6/6 点名 / 性能
同档（9/1/3 逐位 + fluids/blocks/**states 全集合恒等**【V2-17 不触
worldgen 强口径】+ 3 slab 结构自成本 0.62/0.50 同档）。PITS 383
（stateful 块场景 journal 须 states 条目【init 拒 = 输入静默丢失】/
cell DDA 全遮挡 = 合并不可达【probe 三组对照定位】/ 临时插桩定界后
移除重建）。详见 docs/archive/v2x/phase-2.18.0.md。

## v2.17.0（2026-11-04）— V2-16 雪原/针叶林 biome（snowy_taiga + snow 满块 + 针叶树 + 演示区结构保护）+ 音效缺陷族 13 件修复（门 188 全 PASS）

二批 GEN 行（「中」；消费「无 clay/sandstone/snow」土类缺失声明）；MC
机制 11-04 wiki 核定（Snowy_taiga 页 temperature −0.5/trees_taiga 密
林 + freeze_top_layer 雪层/creature 池；Snow 页 雪层 1-8 层薄覆盖
hardness 0.1 透明 shovel 每层 snowball 光≥12 融）：**snowy_taiga
biome**（weight 2 = desert 稀有带先例偏离 4 / surface **snow** /
tree_density 0.12 = forest 偏离 3 / height_scale 1.1 / **新严格字段
`tree` 6 键对象** = sapling tree 段同款面 {spruce_trunk,
spruce_leaves, 6, 3, 2, spruce} 缺/多/类型/范围/shape 词汇全点名 +
create 交叉检 trunk/leaves 存在 + ENT-2 拒 stateful / **新严格字段
`exclude_spawn_area: true`** = 演示区结构保护【GEN-1 spawn_clear 结
构排除先例：inSpawnClearZone 纯谓词 = carveCaves 同款列对齐，区内
累计走查跳旗 → totalWeight 恒 10 = 现有 3 群系映射**逐位不变**
双单测钉】/ 无 decorations = 偏离 2）+ **snow 满块**（hardness 0.1
MC 精确 / 新 "snow" 三件音族确定性合成 / drops 自身 / opaque / 单
tile atlas 121→122 空格零移位；**偏离 1**：MC 1-8 层雪层机制 =
V2-17 相邻范围 + 无 snowball + 无降雪天气 → 满块替代无配方）。
**placeTrees 针叶路径**：同 kSaltTree 格网 + 同 h 密度门/高度位复
用 + 冠层 = **planTree(height, TreeSpec)** 逐 cell placeIfAir（Oak
表已钉 cell-for-cell 同构；root no-op = 树干自 hgt+1 起既有同款）+
**无 tree biome 原硬编码路径零触碰**（既有 3 群系位级红线）。
**音效缺陷族 13 件修复**（记档子项，gate 183 PITS 361 先例）：
generate_sounds.py `_noise()`/`dig()`/`break_sfx()` LCG **逐采样重
建 = 常值流** → 4 件静音（sand_break/gravel_break/dig/break peak
0）+ 5 件 DC 退化 + 2 件部分受损，跨多版本潜伏【截图/存档不录音频
= 门盲区】；修复 = 单 LCG 实例推进，md5 变更恰 13 件（其余 33 件
head-identical 幂等），修后峰值全非零 + abs-rise-frac 纹理度量全落
正常带。2415/2415（+23 biome/worldgen + 1 音效 13 件钉族 + 矿脉/
洞穴探针区外演进重钉）、banner v2.17.0、**零 format**（biome 不入
档 = 每次 seed 重生成；旧档加载 = 区外雪带世界演进 GEN-1/3/5 先
例，演示区结构不变）。门 188：S1 **结构免疫第 69 次免重锚**（设计
预判「远区雪带入 60f 视野 = 重锚」被证据推翻——60f 流送半径内可见
地形全落受保护演示区：新侧 ×2 ≡ 现锚 179027 **零 diff** + savenorm
EXACT + 区内 5×5 chunk 全列映射位级钉）/ S2 雪块破/放**单准星双动
作** E2E【射线破 (2,54,2) 后穿 air 续行至 (3,53,3) 地顶 = 同准星自
然转放置 (3,54,3)；hold 0.5s 断点时序（雪 ~0.2s 破 + 地 0.3s 裂释
放重置）；掉落 2.83 > 磁力 2.5 留地断言；旧侧 snow 词表不存在形状
互斥】/ S3 / S6 6/6 点名 / 性能 A/B 同世界同成本【主对 9/1/3 逐位
+ 终档 journal 0/0 恒等【**口径发现**：世界演进 = 参考世界变化非
journal 编辑，雪带不入 journal】+ 雪带自成本传送场景 1800f 稳态
upd med 111.9 vs 112.5 ms（0.5%）同档】。PITS 382（确定性音效 LCG
逐采样重建静音缺陷族 + 音频门盲区审计口径 peak/sign-chg/abs-rise-
frac）。详见 docs/archive/v2x/phase-2.17.0.md。

## v2.16.0（2026-11-04）— V2-15 16 色染色羊（shears 剪毛 + 出生毛色 + 染色改色 + 彩色羊毛掉落）（门 187 全 PASS）

二批 ENT 行（消费「sheep 只掉白 wool」记档 [Phase 41]）；MC 机制
11-04 wiki 核定（Sheep 页）；**零 format**（mob 不持久化 = 实例态
session-only，savenorm EXACT 最强形）；词表 = 项目 15 色（wool 族无
lime，MC 16 含 lime = 偏离 1）：**新物品 shears**（durability 238
MC 精确 / break_speed 2 / `shears:true` 严格 bool 旗【非 tool 携带
点名拒】/ shapeless 2 iron_ingot / fast_against 整字段省略 = 不加速
任何块 / 5 盒 model + icon tile）+ 剪毛磨损 kShearWear 1。**
sheep.json**：drops 1-3 wool（MC 精确）+ mutton / **wool_drops +
wool_weights 严格对**（白 850 黑 80 灰 30 棕 30 粉 10 = temperate
整数表，biome 表偏离 2；词表/零权重/空列表/孤字段全点名拒，词表拒
放 init 交叉检 checkWoolDrops）+ body box name "wool"。**实例态**
（milkCooldown 帧纪律同款）：出生毛色 = **per-mob 流首抽**（非
wool mob 构造零消费位级钉）/ bare + **再生 7200f = 120 s 帧锁定**
（偏离 3：MC 吃草门随机 ~12 min，项目无草交互）/ 染色保留语义。**
渲染**：15 色复用 wool 族块 tile（同 atlas）+ bare = 新 sheep_skin
tile（atlas 120→121 零移位）+ **per-entity 变体网格**（swingMeshes
范式：懒建/despawn 释放/变体变更重建；withWoolVariantTile 纯函数
几何位级不变；非 sheep 静态网格零触碰）。**RMB 链** tryMilk →
**tryShear**（shears 旗 × wool_drops 旗，无物种 switch：1 个
ItemEntity 携 roll count 活体掉落 + shear.wav【双咔嚓确定性合成，
42→43】+ 磨损 1 + bare 再剪 no-op）→ **tryDyeMob**（手持 dye
× wool_drops，**bare 拒染** = MC 剪后不可染，"cloth" 音）。**死亡
彩毛钩子**：纯函数 woolDeathDropName（woolly → 实例色 wool 变体 /
bare → 跳过 / 非 wool 位级不变）。2391/2391（+35）、banner
v2.16.0、format 28 不变。门 187：S1 **结构免疫第 68 次免重锚 +
savenorm EXACT**（≡ 锚 179027 B 逐字节）/ S2 剪毛染色输入 E2E
（两 pass 确定性瞄准【scratch sheep-only f900 单生 + yaw/pitch 算
准】+ **弹幕范式**【羊 1.2>1×1 围盒弃 / 命中窗 = 出生→首次
wander ~3s / key→RMB ≥0.15s】，**断言 = DEBUG 日志行权威**：a
剪毛 (1 x wool)+damage 1 / b 染红→剪 wool_red+染料耗尽 / c 剪后拒
染染料保留 / o 旧侧零行形状互斥）/ S3 / S6 6/6 点名 / 性能 A/B 同
世界同成本（主对 9/1/3 逐位 + 16/16 36/36 + 羊群自成本 2 羊双树同
位确定性出生 ents 0.0606/0.0600 同档 = 变体网格不贵）。PITS 381
（热栏 key→RMB ≥0.15s / 活体掉落物品不可作终档断言【磁力→首空槽→
弹幕误放置成块】/ 羊静止窗 = 出生→首次 wander）。详见
docs/archive/v2x/phase-2.16.0.md。

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
色相 + HUD 区审计非固定带）。详见 docs/archive/v2x/phase-2.15.0.md。

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
"v1 无床" 注释记档。详见 docs/archive/v2x/phase-2.14.0.md。

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
docs/archive/v2x/phase-2.13.0.md。

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
docs/archive/v2x/phase-2.12.0.md。

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
PASS 源）。详见 docs/archive/v2x/phase-2.11.0.md。

## v2.10.0（2026-11-03）— V2-10 网格修复批（叶子-叶子内部面剔除 + cutout discard 普查确认）（门 181 全 PASS）

渲染节两记档修复（**收官前最后一项**）：
**① 叶子-叶子内部面剔除**（KNOWN_ISSUES 18 条）——剔除 v1 只判
邻居 opaque，叶子（非 opaque）内部面全生成（~12% 透明孔后看到
邻叶背面）：修 = 块 JSON 新可选布尔 `cull_same_id`（严格解析，
leaves/birch_leaves/spruce_leaves 首消费，shipped 恰三 = 零影响
数据钉）+ mesher greedy pass 同 id 非 opaque 邻剔面（纹理选择
前 = merge 不见该面；对称性 = 共享 registry 构造保证；
glass-glass 不剔 / 水规则 / cross·slab·stairs·wall·fence·pane
红线零触碰；单测几何钉 LeafLeaf 24v=6 quads vs 32v）。
**② alpha cutout discard = 零代码确认**（KNOWN_ISSUES 29 条划解）
——侦察实证 PRE-5 已全覆盖（`render.alpha_test` 代码默认 true，
`alpha<0.5 → discard` **桶无关**含 blend section；普查：含 0 孔
纹理 21 个 = 20 纯 cutout + glass 唯一非纯已被覆盖，
water/glass_pane/honey 无 0 孔）+ 普查单测钉（防未来漏检）。
2181/2181（+7）、零 format（25 不动）/ 零 config 新键 / 图集·
纹理·字体零变化。门 181：**S1 = base18 重锚论证**（Phase 18
先例——旧侧 60f ×2 ≡ 现锚 179116 B【环境干净纯代码归因】/
新侧 ×2 恒等 **178148 B** / 新旧 diff 4185 px @ bbox (0,0)-
(679,380) 全叶冠区，**天空蓝泄漏 0 + UI 区零 diff**；像素归因
含 2681 px 旧「邻叶背面暗块」→ 新「天空」= 透明孔透视修复
直接实证；**新锚 178148 B 采用**）+ S2 叶冠判别（= S1 diff
本体 + 普查钉）+ S3 2181 + S6 点名（`cull_same_id` 非布尔 =
「must be a boolean」）+ **性能 A/B 同世界同成本**（changesApplied 9/1/3 三窗逐位 + 终档 16/16 36/36 集合恒等 + PERF-WIN 全指标同档【叶冠面数减无可测回归信号】）。详见
docs/archive/v2x/phase-2.10.0.md。

## v2.9.0（2026-11-03）— V2-9 step-up quirk 修复（player/mob 共享 Entity::stepUp 两 quirk 定向消除）（门 180 全 PASS）

KNOWN_ISSUES 203/382 条「单独立项」两 quirk（ENT-3 探针实证，
player/mob 共享）：**① +X/+Z 恰 1 格宽台阶踏不过**（front 格
`snapped+1` 不对称 = +dir 查贴住格之后那格）→ 修 = `front =
snapped + (w > 0 ? 0 : -1)`（+dir 查贴住格本身，−dir 不变）；
**② 非整数脚高巧合对齐凭空抬升 1.0 再落回**（stepUp 调用点无条件，
「贴住整数块面」位置判非因果判——slab 顶 1.5 行走 face 巧合过整数
+ 前方低半板/同高 slab → 抬升再落回，mob 追逐 frame 606 实证）→
修 = **无状态真阻挡验**（front 列在身体 y 跨内 ∃ solid 格且顶 >
feet 严格不等式——moveAxis 同款行；不加 per-tick 旗标/不改调用
点/零新帧状态）。**整数脚高位级不变**（face 对齐 ⟹ 真 clamp ⟹
新校验恒过；既有 step-up 单测族零改动全过 = 红线）。TDD：3 新
测试（+X/+Z 单格踏过 + slab 顶行走 y 轨迹恒 1.5【相位 8.17 =
pre-fix 逐 0.01 扫相钉】）先在 v2.8.0 树证 FAIL（x/z 9.7 clamp /
maxY 2.5 抬升）后修复 PASS；既有 mob 单格 +X 钉**反转**（
DoesNotStep → StepsUp，ENT-6 封死走廊保留）。2174/2174（+3）、
零 format（25 不动）/ 零 config 新键。门 180：S1 **第 180 次结构
免疫**（四方 RAW 179116 B + 锚 + savenorm EXACT——无输入零位移
stepUp 永不触发）+ S2 **1 格台阶 E2E**（自建平台场景：旧树
**x = 15.700 ×2 精确 clamp**【quirk ① 实机命名复现】vs 新树
踏过 x 25.1/25.1）+ S3 2174 + S6 命名回归（pre-fix TDD 四证）+
**性能 A/B 同世界同成本**（changesApplied 9/1/3 三窗逐位 + 终档 16/16 36/36 集合恒等 + PERF-WIN 全指标同档 + 台阶场景自成本双侧同款 churn 同档）。**键盘协议回归**（PITS 368 已随 X 会话重起
根治——门 177 起鼠标-only 绕过退役）。详见
docs/archive/v2x/phase-2.9.0.md。

## v2.8.0（2026-11-03）— V2-8 sky rescan clobber 修复（LGT-1 记档潜伏 bug 定向消除）（门 179 全 PASS）

**光照核心修复**（KNOWN_ISSUES「光照」节划解）：天花板板骑跨光照
footprint 边界时，后一 region 的 sky rescan 把边界格 sky 重写为纯
垂直值（opaque 下 0 / LGT-2 衰减冠下部分值 < 真值），sky BFS 种子
只取 footprint 内 lit 格（开敞源列在境外）→ 水平光流无法恢复 =
**stale-dark/stale-partial 暗带**（1-14 格宽、天花板下，流送 drain
序触发，确定性"错得确定"）。修复 = 记档两选一中的 **sky BFS 加
shell seed**（union 单遍 = LGT-1 已砍除形态不复活）：
`updateRegion` 天空种子盒从 footprint 扩为 **+1 块 shell 环**
（rescan 仍 footprint-only、BFS 写界 inRegion 不变、no-op skip /
propagateAll / 块光段零改动；shell 格持收敛值 = 恢复边界条件，
严格增 BFS 向内推进；**无 clobber 时位级不变**——旧不动点上严格增
不触发）。**独立性判据零改动**（算术实证：标准列盒的 shell 读列
c±2 = 99.10 文档化 1-chunk 边界读环，kLightParallelColumnGap=4 与
confluence 证明原样成立；world/light_job.hpp 零变更）。零 format /
零 config 新键 / 零新数据。

**TDD**：2 回归测试先在 v2.7.0 树上证 FAIL（stale-dark 2464 diff
格 x8..21 全 0 / stale-partial 544 diff 格恰 = 冠下 x8..10 的
11<14/13/12——清点与设计预测精确吻合），修复后 PASS。2171/2171
（+2）、banner v2.8.0、base18 恒等（无输入启动窗 clobber 几何未
触发 = 结构免疫）。CPU 探针（真实资产半板几何）：旧库 = 梯度
14..1 被 updateRegion 毁成全 0（bug 命名复现）/ 新库 = 完整保持
（恢复实证）。

门 179 全 PASS：S1 第 179 次结构免疫（**锚重启后重锚 179116 B**
= 679×744 WM tile 态——/tmp 丢失后从 v2.7.0 净树重导 ×2 恒等，
字节数与原锚吻合 = 连续性；PITS 375 tile 几何协议 + dummy 暖机）
+ S2 clobber 判别（CPU 探针
旧/新互斥 + E2E slab 场景无输入流送：板 x25..63 西区 x32..38 旧树
stale-dark vs 新树梯度恢复，diff bbox @ 板下地台）+ S3 2171 + S6
命名回归 + **性能 A/B 同世界同成本**（changesApplied 9/1/3 三窗逐位 + 终档 16/16 36/36 集合恒等 + PERF-WIN 全指标同档 + slab 自成本同档）。**环境事件**：机器重启后 /tmp 重建 +
**XTEST 键盘恢复**（PITS 368 根治实证 = X 会话重启，W 键位移实证
z 0.5→−3.0，V2-9 起完整输入协议可用）。详见
docs/archive/v2x/phase-2.8.0.md。

## v2.7.0（2026-11-03）— V2-7 材料补齐批（planks 块化 / 金钻块 / spider 掉落 / 块级 per-face）

四子项均数据驱动（核心唯一新增 = 块级 `faces` 通用能力，
**无 faces = 位级不变红线**），**零 format（25 不动）/ 零 config
新键**：

- **planks 块化**：新块 `planks`（FullCube 复用图集既有 planks
  tile @ [2,4] **零新纹理**；hardness 2.0 / drops 自身 / sound wood
  / flamable / **fuel_time 7.5** 自物品迁入值不变 = wood.json 块
  属性先例）；`items/planks.json` 删（块物品按名自动注册；同名
  显式文件 = init 点名拒「duplicate item id 'planks'」S6 实证）；
  8 配方（bow/crafting_table/planks/stick/wooden_axe/pickaxe/
  shovel/fence）全按名解析 = 零改动（单测钉）。图标观感 = 3D
  方块体（记档偏离，wool 同名先例 Phase 41）。
- **gold_block / diamond_block**：iron_block 先例复制（hardness
  3.0/5.0 + 9 锭 shaped 配方 ×2 + 图集新 tile）；**`beacon_base:
  true`** 两块入信标配基座集合 {iron, gold, diamond}——等级仍是
  world/beacon 纯函数（3×3/5×5 连续完整层），零核心代码。
- **spider 掉落**：`string ×0-2` + `spider_eye ×0-2`（既有
  count_min/max 随机区间机制，MC 同款值；spider 其余字段零改动
  + 六 mob FNV 钉）+ 2 新 material 物品（螺旋绳 / 紫青眼球 icon
  新 salt）。**弓配方用 planks 不用 string → 两物品纯收集 = 记档
  偏离**（不加配方 = 最小面）。
- **块级 per-face 纹理**：`BlockModel.textures` 扩可选 `faces` 段
  （front/back/left/right 覆写 side，**面外法线定名**——
  front=(0,0,−1) 共享 yaw-0=−Z 约定；top/bottom 归既有键，faces
  内命名 = 点名拒；严格解析：未知键/非串/空串/cross·fence·
  glass_pane 形状全点名拒；`cubeFaceTexture` 方向感知重载 +
  mesher 10 调用点转向 = **per-face 覆写自动断 greedy merge**）。
  **无 faces = 位级不变**（空串回退 side；单测证明除 furnace 外
  全块方向查找 = 旧查找）。首个消费 = **furnace front 单 tile
  `furnace_front`**（宽炉口+余火）；**炉子无 lit 纹理态 = 记档
  偏离**（lit 表现 = 既有炉口火焰粒子）。

图集 102/128（3 新 tile row-7 [10..12,7]，纯 +12 行零移位）、
字体 423 零新字、既有 184 纹理 PNG md5 零变化（5 新 1 删普查
实证）；`-Werror` 0、banner v2.7.0。

门 178 全 PASS：S1 **第 178 次结构免疫**（new/old 60f 四方 RAW
全字节一致 + base18 锚 179116 B + 跨版 savenorm EXACT 零 format）
+ S2 四子项判别全 PASS（planks RMB 放置互斥终态 = 新侧块入 world
@ (0,54,-3) / 旧侧 material 无动作 / 首试命中零 CALIB；炉子前脸
new ×2 恒等 + 新旧 diff bbox 112×107 @ 画面中心 = 前脸纹理区【pose
站 −Z 侧朝 +Z yaw 3.0——首跑从 +Z 侧看 back 面三图恒等零 diff =
面名约定实证，PITS 374②】；beacon gold 3×3 基座光束 = 细竖列
x[316..355] + 基座金色 diff vs stone 对照 None + **旧侧载 gold
场景 init 点名拒 = 可载性互斥** + CPU 探针（真实资产+世界）
beaconLevel(gold)=1/(stone)=0；spider 夜间 scratch spider-only
场景击杀 E2E【PITS 373 敌对生成双门 = 生成保护 12 块 + 夜间被动
cap 挤占】：单跑 hold-swing kill 2 只，掉落两组
string×2+eye×2 / string×1+eye×2 **每组独立在 0-2 区间** +
**复跑两组逐位恒等**，旧侧 v2.6.0 spider.json 无 drops 字段 =
结构互斥）+ S3 2169/2169（+12）+ S6 双点名（`unknown face
'weirdface' in model.textures.faces` / `duplicate item id
'planks'`）+ **性能 A/B 同世界同成本**（900f 无输入：changesApplied
9/3/1 三窗双侧逐位 + 终档 fluids 18/18、blocks 55/55 集合恒等 +
PERF-WIN 全指标同档；FURN 炉子场景双侧 900f = per-face 覆写非空
分支零可测增量）。门协议
发现记 PITS 373/374（敌对生成双门 + hold-swing 帧预算/capture
时延锚/face 面名 pose/symlink 进程名脱管）。详见
docs/archive/v2x/phase-2.7.0.md。

## v2.6.0（2026-11-03）— V2-6 体验打磨批（第一人称手臂 / mob 阴影 / 拾取磁力 / 月相）（门 177 全 PASS）

UX-8/9 同款多格批：四子项各消费一处记档偏离，全为表现层/手感层，
**零 format（25 不动）/ 零 config 新键**：

1. **第一人称手臂**（「无手部皮肤——只渲染手持物品」消费）：
   `drawHeldHand` 加臂 viewmodel = 玩家数据模型 **`arm_r` box**
   （data/player_body.json，init 校验存在）re-center 后走既有
   box-model 管线（makeMobModelGeometry：chunk-mesher-verified UVs）
   ——Steve 皮肤 pa_arm tile 六面全贴（**零新纹理**，V2-2 tile；tile
   下 10 行 = 肤、上 6 行 = 袖，π 翻转姿态使肤手朝持物端）；
   **仅第一人称**（门控 = 指针锁定 ∧ 无面板 ∧ First = 既有手持门
   三态矩阵化，`firstPersonArmVisible` 纯函数单测钉）；**无手持也画
   空手（肤手）**；静态（无行走摆动 = 记档偏离）但继承手持 offset
   → 弓拉/进食通道携臂微动；懒构建（无输入运行永不建 mesh =
   结构性免疫）。
2. **mob blob 阴影**（「无 blob 阴影 = 仍活跃」消费）：每 mob 脚下
   （+ 玩家**仅第三人称**，第一人称无自身阴影 = MC 观感）世界空间扁
   quad：双面、半透明黑 **alpha 0.3** 命名常量、边长 = **1.5×模型
   宽**（blobShadowSide 纯函数钉）、中心 = 底心正下方**底格顶面**
   （blobShadowCenter = floor(footY) + 0.01 防 z-fight 抬升）——
   贴地面而非悬浮脚边（MC blob 观感）；UI shader 纯色路径（outline
   范式）+ blend pass（深度写关测开：山丘遮挡、地形承托）；纯渲染
   零帧状态（无新帧状态成员；mesh 懒构建，无阴影可画永不建）。
3. **物品拾取磁力**（「固定 1.0 格半径无磁力」消费）：0.5s 延迟过后
   水平距离 ≤ **kMagnetRadius 2.5** 的掉落物被逐渐吸向玩家——
   **帧锁定**（kMobStepPerFrame 同款固定步长纪律）：位移 =
   f(距离, 帧) 纯函数（entity/pickup_magnet.hpp）：径向速度 =
   kMagnetMaxSpeed 6 ×(1−d/2.5)²（远慢近快 = **加速趋近 + 距离
   衰减**，边缘 0）+ 侧向 **swirl**（24 帧周期、幅值 = 径向 25% =
   收紧螺旋 MC 手感，frame 有界 modulo 纪律）；掉落物步进过
   hasRoomAt（**磁力不穿墙** = MC 规则）；**箭头拾取（Phase 42 同款
   半径 1.0）同步消费** = 插墙箭被同款磁力拉出方块（无 room 检查：
   卡住箭 AABB 本就在 solid 内，stuck 箭不做物理步）；拾取半径 1.0 /
   延迟 0.5s / 入包规则零动（磁力只是把物品送进半径）；无 ready
   掉落物/插墙箭 = 零执行（60f 基线零实体 = 结构性免疫）。
4. **月相**（billboard「无月相」消费）：**8 相 = day cycle 累计天
   index（帧锁定派生，`DayCycle::dayIndex` = floor(start + total/len)）
   % 8**（纯函数 `moonPhaseIndex/moonPhaseOffset` 单测钉：新月同心 →
   满月相切，25/50/75% 象限刻度，盈凸 1..3 滑 +X / 亏凸 5..7 滑 −X）
   ；呈现 = 既有月 quad 上叠暗色「食相」quad（**零新纹理**：sky
   shader 纯色路径同款 28px 圆盘滑移，颜色 = 深夜天色 kNightSky，
   覆盖区读作天空 = 娥眉幻觉）；**绘制序 = 食相先/月后**——
   kSkyDistance 平面 24-bit 深度只分辨 ~160 块，同平面第二绘制只能
   靠严格 LESS 的绘制序取胜（亚块 nudge 被量化吞掉 = 实证废案，
   **PITS 371**）；正午基线月在地平线下 = 整分支不执行（结构性
   免疫）；**星星不动**（无星闪保持记档）。

2157/2157（+22：月相 5【8 相序列/偏移表/单调/天 index/基线钉】/
磁力 9【速度曲线/单调加速/到达零向量/swirl 峰/周期/越界】/ 阴影
3【贴地几何/边长/常量】/ 手臂 5【门控三态矩阵/姿态钉/静态确定/
offset 继承/相机随行】）、`-Werror` 0、banner v2.6.0。基线自证：
60f 无输入 `--screenshot` = base18 锚 179116 B **逐字节恒等**（四子项
全惰性）；功能实证：月相 1/3 两相像素判别（0.25r 娥眉 / 0.75r 凸月
宽度精确）、臂 viewmodel 锁屏态像素带（袖蓝 800px + 肤手 + 石方块
手持）、mob 阴影像素判别（猪/牛脚下草地精确 ×0.7 变暗 + 玩家脚区
100% 无阴影）。门 177 全 PASS：S1 **第 177 次结构性免疫**（60f 四方
RAW 全字节一致 + 锚 179116 B + 跨版 savenorm EXACT 零 format）+ S2
四子项判别全 PASS（手臂 new-locked ×2 恒等 + new vs old 臂区 diff
349×191 右下 / 阴影 mobs-off new≡old 零 quad 免疫 + mobs-on 地面
diff / 磁力 new ×2 恒等 + pork 吸入拾取 vs old 原地不动互斥终态 /
月相 new ×2 恒等 + **diff bbox 恰 28×28 px 月盘 @ 预测位**【存档
time 0.519 + 朝东 pose——PITS 372 载档 time 压过 --time】）+ S3
2157 + S6 点名（arm_r 缺失 init 拒）+ **性能 A/B 同世界同成本**
（changesApplied 9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + mobs-on
阴影 ents 同档 + 磁力场景同档）。门内环境事故 3 条记 PITS
368-370/372（XTEST 键盘服务器级失效 + drawin 窗盖中央点击 + mob 不
持久化 + 载档 time 优先）。详见 docs/archive/v2x/phase-2.6.0.md。

## v2.5.0（2026-11-03）— V2-5 新 mob 猪（门 176 全 PASS）

「加 mob = JSON + 纹理」数据驱动承诺的验证 = **零核心代码**：
**pig.json**（passive 10 HP / 0.6 speed / spawn_max_light 15 / drops
pork ×1-2 + leather ×0-1 随机区间既有机制）+ **宽体 6-box 模型**
（body + 4 粗短腿 + head 0.45³ 嵌入 body 前方 cow 范式；4 腿 0.4 rad
对角 swing；64 相位 clip 验收）+ **head `faces.front` = pig_face**
（**V2-2 per-face 纹理的首个 mob 消费**——此前仅玩家身体用过；显式
8×8 猪脸粉/深粉鼻口/2×2 眼 Steve 范式零 hash）+ **pig_body 纹理**
（MC 猪粉 (228,170,160)+noise，新 salt 0x5011；**既有 6 mob tile 176
文件全 cmp 零变化**）+ **pork/cooked_pork 食物链**（+3/1.8 sat →
smelt → +8/12.8 sat = beef 熟同款刻度；icon 生成器新 salt；无新配方
杀猪→熔炼 MC 同款）+ **生成权重** {"id":"pig","weight":1}（6→7 加权
抽取零代码）+ **图集 97→99**（pig_body [8,7]/pig_face [9,7] 进 row-7
空位，97 既有条目**零移位**，99 条全 census 逐格钉）+ **字体
422→423**（猪 U+732A 码点序，顶 512 行逐字节不变免重锚，424 预算剩
1）。2135/2135（+7 新 + 10 既有扩展）、-Werror 0、banner v2.5.0。
门 176 全 PASS：S1 **第 176 次结构性免疫**（60f 六跑 n1≡o1≡o2 跨版
RAW + n1≡m1【mobs-on ≡ off = first_spawn_frame 60 零 spawn 扫描
render-inert】+ m1≡m2 + 锚 179116 B + savenorm EXACT 零 format）+
S2 猪判别（a 生成序列 ×2 恒等 118 行【pig f60+f300 (0.5,-5.5) 外环
优先复现】+ 旧侧零 pig 互斥；b 杀猪链全存档权威：离线 f210 截图粉簇
反投影 + 走位自洽迭代解 KILL pose → 实跑零修正 3+ 命中 iron_axe →
**pork ×1 掉落区间钉** + leather ×0 → 两段接力【mobs-off save-posed
传送到掉落格】拾取/开炉/喂料/熔炼/取排/吃【selected 预置免数字键】
→ hunger 14→20）+ S3 2135 + S6 点名 2/2（unknown mob 'dragon' /
pig_face 图集缺条目）+ **性能 A/B 同世界同成本**（changesApplied
9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + mobs-on 猪自成本 ents
0.04-0.10 ms ≈ 既有 mob 同档无回归）。门内环境事故 3 条记 PITS
368-370（XTEST 键盘服务器级失效 + drawin 窗盖中央 + mob 不持久化）。
详见 docs/archive/v2x/phase-2.5.0.md。

## v2.4.0（2026-11-03）— V2-4 天气（雨/雷）（门 175 全 PASS）

帧锁定天气系统（设计定案 11-03 夜间自主推进）：**状态机 = core/weather 纯函数**（campfire 范式，splitmix64 推进式流 PITS 4——主流 seed = 世界 seed【同世界同序列】，雷 = 独立 salt 流 0x7E42B1D9；CLEAR [9000,18000)f / RAIN [3000,6000)f / 雷 [600,1800)f 命名常量；**首段恒 CLEAR** = 无输入 60f 基线永不进雨的结构免疫根）+ **呈现 = rain shader program**（cloud 第四先例：静态 VBO 300 滴出生偏移+相位【0xD93A5E17 专用 salt 流】init 一次上传 + 顶点 shader 逐滴纯函数位置【有界 fallOffset + 玩家 16³ 盒 boxOrigin，per-frame 零上传】+ 0.02×0.5 细竖条 alpha 0.35 白蓝 (200,210,235) 无纹理）+ **天空 dayFactor 消费点单收口**（RAIN ×0.85 + 雷 5 帧线性衰减 +0.15 脉冲——u_dayFactor/clear/雾同源，shader 表达式树零改动）+ **交互四件全帧锁定**（雨灭火块/灭营火 = 新纯谓词 extinguishedByWaterOrRain【6-邻水 ∨ raining，Phase 64/75 路径收口】/ 雨灭燃烧效果【玩家+mob inWater ∨ raining，re-ignite 先行保水胜火序，无屋檐检查 = 记档偏离】/ 雨润耕地【soilIsWet/cropIsWet/cropGrowthSteps 扩参默认 false = 旧语义位级不变，mesher 湿润视觉不传 = 视觉零变，雨中生长 2× = 111/200→200/200 钉】）+ **render.weather strict read 默认 true**（off = 状态机不跑+零雨滴+零交互 = v2.3.0 位级恒等）+ **--weather clear|rain CLI**（仅覆盖初始 state——rain = kRainMin 3000f 固定可复现，主流零消费其后走流；词校验点名）+ F3 `weather clear|rain` 行 + **雷新 wav**（generate_sounds 固定种子 0x70DE 60→40Hz 滚雷 3.5s，既有 39 wav md5 零变化+幂等实证；2D 中心不空间化；**无雨声循环** = 记档）+ **零 format**（25 不动，天气 = session 派生云漂移先例类）。2128/2128（+19：weather 17【设计 §6 八组 + 脉冲/wav 资产】+ F3 WeatherLine + mob ExtinguishedInRain）、-Werror 0、banner v2.4.0。门 175 全 PASS：S1 **三方恒等最强形**（60f 六跑 new-on ≡ old v2.3.0 ≡ new-off ≡ base18 锚逐字节 + savenorm EXACT 零 format）+ S2 雨判别全 PASS（rain ×2 恒等 / 雨可见 / 120f savenorm 精确 churn 判别【rain 侧恒无火 + 非世界键 EXACT】/ 营火+火块互斥终态 / 雷+停雨 4 事件 ×2 间隔 ±0.08s 帧锁定 + 雨窗 50.5 s ≈ kRainMin）+ S3 2128 + S6 点名 2/2 + **性能 A/B 同世界同成本**（changesApplied 9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + 全指标同档；rain 自成本 = 300 滴静态 VBO 在噪声带内）。详见 docs/archive/v2x/phase-2.4.0.md。

## v2.3.0（2026-11-03）— V2-3 教程系列化（农耕 + 厨房）（门 174 全 PASS）

用户 11-02 实机反馈「之前的新手指导比较单调，有没有多个任务系列可以添加」：**A 机制系列化**（`tutorials/` 多文件按名序 = 系列序 + `AssetManager::listDir` + 步 id 跨系列全局唯一 init 点名；`TutorialState` +seriesIndex 纯函数推广；**存档 format 24→25** `player.tutorial` 加 `series`——三向键纪律【25 缺 series 拒 / 23-24 携 = 版本谎报拒 / ≤24 载入缺省 0 = 值保真免迁】+ series 越界/id 不在该系列 = 重置上报不拒档；事件词表 +**pickup./smelt.** 两前缀均帧锁定【拾取收口 + 炉子 smelt-complete 消费点】；**T = 跳当前系列**；系列过渡无 toast【目标行换步即提示定案】；F3 行 `<s>/<total> <id> p/c`）+ **B 两个新系列**：② 农耕 7 步（2 种子→2 耕地→2 种→2 收 = 4 小麦→面包→吃，全链数据核实在位零新机制；**×2 修订** = wheat_7 掉 2 < bread 配方 3 的经济账死局修复）+ ③ 厨房 6 步（3 锻石→炉子→放→开→熔炼熟鸡→吃）；**beginner.json 字节零改动**；字体 401→422（+株 等，顶 512 行逐字节不变免重锚）。**门 174 内抓到并修复 UX-7 潜伏 bug**：E 面板 recipe 行左键合成成功漏发 tutorial craft 事件（玩家点配方行合成 = 永久卡教程；1.x 链从未经该路径 = 潜伏两版，PITS 360）。2109/2109（+25）、-Werror 0、banner v2.3.0。门 174：S1 **第 174 次结构性免疫免重锚**（四方 ×2 RAW 全字节一致 + 锚 179116 B 连续 + 跨版 savenorm 恰 2 项【format + series:0 值保真】）+ S2 输入链全 PASS（A 农耕 9 段【FAR 先放 = 累积态遮挡定案 + 生长轮询 + 走捡 + 逐 checkpoint 断言 ×8】/ B 厨房 2 段【开炉重捕获重试 + 燃料点火即耗物品判别】/ C T 三态 + 日志逐字 / D format 24 迁移值保真）+ S3 2109 + S6 精确点名 4/4 + 性能 A/B 同世界同成本【changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55 集合恒等 + 全指标同档】。PITS 359→367（8 条：360 recipe 行漏 emit / 361 format 边界字面量 / 362 教程经济账 / 363 锁定态禁预 move / 364 面板 uiSize 空间 / 365 漂移须重捕获 / 366 燃料点火即耗 / 367 饥饿满 no-op）——详见 docs/archive/v2x/phase-2.3.0.md。

## v2.2.0（2026-11-02）— V2-2 玩家皮肤 Steve 化（门 173 全 PASS）

用户实机反馈「看到正脸了，没有眼睛，没有头发，皮肤也丑，请仿照史蒂夫更新」：根因 = 每盒六面全贴同一 tile + head tile 是四侧脸 2×2 拼格 → 五官亚像素。两部分修复：**A 模型 per-face 纹理**（MobBox 可选 `faces` 六面覆写，严格解析 + init 存在性校验点名 box+面；**无 faces = 位级不变**——既有 6 mob 零改动 + 全族测试零语义改动 = 红线证明；角序/绕序零动，摆动 UV 随行零改动）+ **B Steve 风格皮肤**（生成器确定性显式像素：8×8 脸 = 棕发双色调 + 2×2 眼 ×2【鼻侧白 + 外侧蓝虹膜】+ 鼻/嘴阴影行；青蓝衫/蓝裤/棕靴；7 个 per-face 独立 16×16 派生 tile，头 8×8→16×16 2× 精确放大 = 五官像素级；图集 94→97 row-5 空位，**90 遗留条目零移位免重锚**，旧 4 拼格退役）。2084/2084（+5）、-Werror 0、banner v2.2.0。门 173：S1 **第 173 次结构性免疫免重锚**（四方 ×2 + 锚 179116 B + savenorm EXACT）+ S2 Steve 脸 A/B（三模式 ×2 + savenorm 两两 EXACT + 头带 0.6 阴影换算色分类：**旧前视零五官（2840 旧肤）/ 新前视眼白 306 + 虹膜蓝 324 + 发 1421 / 新后视全发 5280**）+ S6 ×2 精确点名 + 性能同世界同成本【首轮 render 偏置重跑证伪 + changesApplied 9/3/1 逐位 + 终档 18/18 55/55 diff 0】——详见 docs/archive/v2x/phase-2.2.0.md。

## v2.1.0（2026-11-02）— V2-1 第三人称前视角（2.x 首个功能项，门 172 全 PASS）

用户 11-02 实机诉求「第三视角下能看到自己的前面」= MC F5 前视角。**核心 = 前视是后视同款数学在翻转视角角下执行**（yaw+π/−pitch ⇒ −forward 翻转 = 玩家前方；up_cam 翻转不变 ⇒ v2.0.0 的 0.4 轨道抬升同款继承 = 前视头也在准星下方 29-96px；遮挡收敛/穿流体/embedded 自动对称）——`resolveThirdPersonCamera` 等纯函数零改动。C 键三循环 First→后视→前视→First + 新纯函数 `thirdPersonFrontAngles`；**瞄准 UX-8g 眼位射线 origin/方向零动**（前视方向 = 相机向 = 屏幕中心向自动翻转）+ 新 `aimReach()`（前视 = D+kReach = 玩家身后 5 块有效带 MC 前视惯例，First/后视位级不变）4 消费点 + drawHeldHand 前视隐藏（`!= First`，First/后视逐位等价）+ 身体前视自动渲染（既有门控）。2079/2079（+4：翻转角钉/up_cam 不变/空世界恒等 bit-exact+后视镜像/墙钳止镜像）、-Werror 0、banner v2.1.0。门 172：S1 **第 172 次结构性免疫免重锚**（四方 ×2 + 锚连续 179116 B + savenorm EXACT）+ S2 三模式矩阵（三模式 savenorm 两两 EXACT 不入档 + 皮肤判别 First 0 / 后视 10,151 / **前视 10,125 全 0.6 阴影调 = 脸朝向相机** + 世界朝向 B≠C 10,674 diff 互斥 + 六跑零漂移）+ S2b 前视挖矿互斥（前视破身后 4.5 块靶 vs 后视破面前墙，pitch 翻转场景坑首轮实证）+ 性能 A/B 同世界同成本【首轮 win2 churn 偏置重跑证伪 9/3/1 逐位 + 终档 18/18 55/55 diff 0】——详见 docs/archive/v2x/phase-2.1.0.md。

## v2.0.0（2026-11-02）— 1.x 收官里程碑发布（扫尾三件事，门 171 全 PASS）

1.x 任务表 40/40 + UX-8g/UX-9 实机报障 4 项全完成后的里程碑发布（无新玩法），用户 11-02 定案三件扫尾：

1. **1.x 阶段文档归档**：`docs/archive/` 181 份 phase 文档 `git mv` → `docs/archive/v1x/`（保历史）+ 全仓 556 处引用零残留 + BACKLOG v1.x 表标记归档、顶部开 v2.x 任务表占位（待用户定范围）+ AGENTS「现状」节瘦身（1.x 巨段编年史 149k + 逐门记录抽出 → `docs/archive/v1x/HISTORY.md` 只读全史）+ 活文档刷新到 2.0 状态（MECHANICS 12+ 处 / KNOWN_ISSUES 8+12 处 / GAMEPLAY 8 处，全数字源码实证）+ 门协议工具入仓（`tools/gate_compare.py` / `perf_gate_config.json` / `xenv.sh` = PITS 317 重建纪律）
2. **第三人称准星不压头**（用户报障，B 方案 = MC 同款抬相机）：轨道点沿相机 up 轴抬高 `kThirdPersonOrbitHeight` 0.4 块（`Camera::up()` = cross(right, forward) 单一三角真源，头 top 降 29px / 底缘降 96px 低于屏幕中心 = 准星零重叠，任意俯仰角恒定）；瞄准射线两模式仍自眼位（UX-8g 红线零动）= 固定 TP 视差 MC 同款记偏离；First 分支字节不变
3. **发布**：tag v2.0.0 + GitHub release（2.x 迭代起点）

2075/2075（扫尾② +3）、`-Werror` 0 警告、banner v2.0.0。门 171：S1 **第 171 次结构性免疫 + 默认 First 位级恒等 + base18 锚连续 179116 B 免重锚**（四方 ×2 全字节一致 + 跨版 savenorm EXACT）+ S2 TP 准星 A/B 判别 PASS（OLD 中央带 160 皮肤 px = 压头实证 / NEW 中央带 0 + 下移带 720 = 头降 29-96px 解析吻合 + 中心像素 OLD 面部 vs NEW 草逐像素 + 遮挡回归 2390 皮肤 = 相机出墙保持 + 跨侧 savenorm EXACT 同世界最强形）+ S3 2075 + 性能 A/B v1.50.0 vs v2.0.0 同世界同成本（changesApplied 9/3/1 逐位 + 终档 18/18 55/55 diff 0）——详见 docs/archive/v2x/phase-2.0.0.md。
