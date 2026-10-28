# 版本历史（CHANGELOG）

> 逐版细节（机制/证据/偏离/性能 A/B）见 `docs/archive/phase-*.md` 阶段存档与
> GitHub Releases。README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本）。

## v1.13.0（2026-10-28）

**ENT-6 AI 补强**（v1.x 任务表 13/33，ENT 行 2/6，门 128）——"AI
补强"原始定义已佚 = 代理定案 **mob 追逐寻路**（消费 KNOWN_ISSUES
记档缺口"每只 mob 只以玩家为对象……追墙会沿墙滑"；直线追逐被
墙/台阶/深坑挡住时永久卡死 = 最显著的 AI 缺口）：
- **新纯函数模块 `entity/pathfind.{hpp,cpp}`**（campfire 范式）：
  `cellStandable`（feet 格 + bodyCells 头空 = air + 下方 = solid
  FullCube = 整数地板；楼梯/半块 = 非整数 feet 出图 = 记档偏离）+
  `pathfindBFS`（统一代价 BFS，固定邻居序 = 确定性：flat 四邻 /
  step-up +1【物理 stepUp 可踏 1.0 同款模型】/ fall 1..kPathMaxFall(8)
  深走廊格全 air；target zone = Chebyshev ≤1【末段直线 chase 收】；
  展开帽 kPathMaxNodes(512)；无随机无 float = 帧锁定安全（硬规则 8）。
  空路径 = from 已在 zone / 错误 = 无路）。
- **MobEntity 卡住触发集成**：chase 窗 kStuckWindowSeconds(0.5 s)
  水平 progress < kStuckProgressFactor(0.25)×期望 → `tryPathfind` →
  `MobState::Pathfind`（新枚举值）跟随节点（数据 speed 指向下一节点
  中心）+ 每 kRepathSeconds(0.5 s) 重规划（目标移动跟进）+ 失败
  kLostCooldownSeconds(1.0 s) 节流 + **kNodeTimeoutSeconds(2.0 s) 节点
  超时逃逸**（A* 模型可能错——共享 step-up 的 +X/+Z 单格 quirk 使
  规划的 step 物理不可执行 → 弃路径回直线 chase = quirk 几何下行为
  与 ENT-6 前逐位一致，单测钉死）。**结构惰性**：未卡住 = 逐位恒等
  直线追逐；wander/attack/ranged 零触碰。
- **零 format（19）/config/内容/音效**。
- **范围外记档偏离**：寻路只覆盖 chase（wander 每 2.5 s 重掷自然
  逃逸 / ranged kite / 被动 mob 无逃跑 AI = 既有条）；无跳跃边（MC
  mob 亦不跳）；楼梯/半块地板出图（整数晶格纪律）；fall 走廊深帽 8
  （更深坠落直线 chase 亦坠）。

1798/1798（+19：PathfindStandableTest 5 + PathfindBFSTest 10【直线 /
绕墙 / 密封无路 / 1 深坑 fall+cross / 2 深坑绕行 / step-up 平台 /
确定性 / zone 空路径 / from 非 standable 拒 / 节点帽】+ MobPathfindTest
4【卡住找路到玩家攻击 / 平地永不寻路 = 结构惰性 / 密封 lost 恒夹零
命中 / 玩家离场弃路径】）、`-Werror` 0 警告。**4 契约演进重钉**
（MobStepUpTest，ENT-6 意图行为变更 = "恒夹墙" → "绕行不翻越"）：
DoesNotStepUpWall/Fence/TwoBlockStep → sawPathfind + **maxY 恒 1.0**
（原"不可踏"判定存活：1.5 barrier/2 步无 FullCube 顶节点 = A* 不规划
翻越）+ DoesNotStepSingleCellStepPlusX → 改名
SingleCellStepPlusXStaysClampedAtTheQuirk（走廊双侧封墙 = 唯一通路是
quirk 步 → A* 规划 → 物理拒绝 → 节点超时弃路径 → **x 9.7 恒夹 = quirk
pin 在 ENT-6 后存活**；quirk 本体修复仍 = player 行为变更单独立项）。
门 128：S1 = **第 128 次结构性免疫免重锚**（fresh 60f 四方 ×2 387726 B
+ base18 + savenorm new×2/old×2 EXACT；nomobs = BFS 结构惰性）+ **S2
harness A/B 矩阵 PASS**（gen128 单源双库【state 整数面：旧枚举无
Pathfind 值 = 旧侧 sawPathfind 构造性 false】2400f ×2/侧）：wall
NEW hits=19 sawPathfind=true maxY=1.0 pos=(12.188,1,7.409) Attack【绕
墙末端到达攻击】vs OLD hits=0 pos=(7.700,1,6.5) Chase 恒夹【8.0−0.3】
互斥终态 / flat **NEW≡OLD 逐值全同**（hits=29 同终位 = 未阻塞时结构
惰性的二进制级证明）/ sealed **NEW≡OLD 逐值全同**（hits=0 恒夹 = 无路
时 lost 回落旧行为）+ **S3 围栏竞技场零回归 A/B PASS**（s127_arena
900f 夜 mob on：new×2/old×2 RAW EXACT 235680 + **new≡old 字节一致**
【围栏内部无障 = 无 chase 卡住】+ savenorm EXACT）+ 性能 A/B
fluidCells win2/3 29803/29800 双侧逐位【重跑复现】+ PIPE3 逐位 + 全同
档【win3 upd med 首轮 3.44 vs 5.50 = 重跑 6.27 vs 4.96 双向摆动 =
环境噪声证伪】+ 终档流体 4 格差（PITS 283 类 churn 簇）= 成本可忽略。
组 1.11-1.15 进行中（第 3 版，组内不打 tag = 发布规则见 BACKLOG
「执行顺序」）。

## v1.12.0（2026-10-28）

**ENT-5 坠落伤害**（v1.x 任务表 12/33，ENT 行 1/6，门 127）——玩家侧
Phase 16 已完整（`fall > fall_damage_offset(3) && !isInWater() &&
!isInLava() → damage(fall − 3)`，P36 读档重钉、P54 任何流体取消），
"坠落伤害"定案 = **Mob 侧坠落伤害**（Minecraft 中 mob 同样受坠落
伤害；缺口在 MobEntity 无落差跟踪）：
- **Player 规则镜像到 `MobEntity::update`**（stepUp 之后的落地转换
  处）：`m_onGround && !m_wasOnGround` 时净落差 `m_fallStartY −
  position().y` **> kMobFallDamageOffset**（3.0f 命名常量 = MC 同值，
  玩家同值；不做 per-mob 字段——MC 无 mob 差异，YAGNI）且**落地格
  无流体**（fresh `overlapsFluid(Water) || overlapsFluid(Lava)` 扫描
  最终位置 = 玩家 P54 同款"任何流体取消坠落"；tick 初的 `m_inFluid`
  滞后一 tick 会漏 1 深池 = PITS 301）→ `damage(fall − 3)` 走既有
  伤害/死亡/掉落管线（致死掉落零新代码）。
- **钉点**：mobs 是 ephemeral（从不存档）→ 构造函数一次钉
  `m_fallStartY = pos.y`（无玩家 restorePosition 的读档重钉需求）；
  离地转换（`!m_onGround && m_wasOnGround`）重钉 = 走落悬崖的
  fall-start 正确（转换钉点晚 1 物理子步 = 确定性，单测紧公差）。
- **帧锁定**（硬规则 8）：m_onGround = 本 tick 物理结果；无 real dt、
  无随机。**零 format（19）/config/内容/音效**。
- **E2E 设计约束记档**：spawner floor band = 玩家脚高 ±kSpawnFloorDelta
  → 敌对 mob 只生成在玩家同层 ±2，**生成 mob 的致死坠落（需落差
  23+）无法自然生成构造**（chase 受 detect_radius 16 限制到不了；
  knockback 方向 = 远离玩家 = 推不向悬崖）→ S2 = CPU harness 探针
  （窗口免、逐值确定性，FLUID-3 夜探针先例）。

1779/1779（+5 MobFallDamageTest：9 块坠落恰 6 伤 / 3 块内 0 伤 / 1 深
水池落地 0 伤【流体取消，fresh 扫描】/ 23 块坠落致死【既有死亡管线】/
走落 5 格平台 ~2 伤【离地转换钉点 EXPECT_NEAR 0.02 紧公差】）、
-Werror 0 警告。门 127：S1 = **第 127 次结构性免疫免重锚**（fresh
60f 四方截图 ×2 387726 B 全字节一致 + base18 + savenorm new×2/old×2
EXACT；nomobs config 无 mob = 无坠落 = 结构惰性）+ **S2 harness 探针
矩阵 PASS**（2×1 高台 y47 + 清挖坑底 y22 = 25 落差，无玩家 wander-
only，种子 1/5/6/8 探针选出走落 seed）：new ×4 seed 全 **dead**
（deadAt 499/370/201/229，25−3=22 ≥ 20 hp 坠落致死）终位 y22.00 /
old ×4 seed 全 **hp 20.00 存活** 终位 y22.00（seed 6 两侧终位逐位同
(24.17,22.00,-6.55) = 物理恒等实证）——互斥终态 ×4 ×2 确定性（PITS
302：harness 须从所属树 cwd 运行，assets 相对路径）+ **S3 围栏竞技场
零回归 A/B PASS**（12×12 石墙 + 夜 0.75 + mobs on，900f 无输入：敌对
mob 自然生成+追逐+击杀 idle 玩家，围栏 = mob 永不坠落 → ENT-5 代码
结构惰性：new ×2 RAW EXACT 235680 B + old ×2 RAW EXACT + **new_a ≡
old_a 字节一致**（mob 生成/追逐/攻击/死亡全链零回归）+ savenorm EXACT
+ 玩家同帧死同位 hp 0）。+ 性能 A/B 同 seed 900f（fluidCells win2/3
**29803=29803 / 29800=29800 双侧逐位**【v112 首跑 29703 = 一次性
churn 相位波动，重跑复现 29803 = PITS 283 场景自分歧类非回归】+ win1
±0.004% 填充 real-dt 类 + PIPE3 changesApplied 1404=1404 逐位 + 全
指标同档 = **成本可忽略**，mob 代码仅 update 落地块 = 无坠落事件时
零执行）。PITS 301-302（m_inFluid tick 初滞后一 tick / harness 树
cwd）。组 1.11-1.15 进行中（v1.11.0 首版 → v1.12.0，组内不打 tag =
10-28 定案 tag 策略）。

## v1.11.0（2026-10-28）

**FLUID-6 流体种类扩展**（v1.x 任务表 11/33，FLUID 行 3/3 收尾，门
126）——第三流体种类 = **honey（Minecraft 1.14+ 蜂蜜）**，最小完整
版定案：1/3 速高粘度 + 不参与水-熔岩反应 + 可桶舀/倒 + 无波纹 + 无
worldgen（桶源 = 项目无蜂箱的记档偏离）：
- **编码三层化**（chunk.hpp）：`FluidKind{Water=1, Lava=2, Honey=3}`；
  编码带 0 / 水 1..8 / 熔岩 9..16 / **蜜 17..24**（水/熔岩带位级不
  变）；fluidCode/LevelOf/KindOf 纯算术三层泛化；词汇表 +honey（存档
  fluids 段种类名直接生效 = **零 format（19）**）。
- **粘度 = 帧锁定 tick 门控**（单门控记档简化：下落与扩散同 1/3
  速）：`FluidSim::tick(..., honeyActive)` 第 4 参；Game 门 =
  `(m_frameCount / kFluidTickEveryNFrames) % kHoneyTickPeriodFluidTicks
  (3) == 0`；水/熔岩模拟路径零改动（门只控第三段
  simulateType(Honey)）。
- **无反应 = 数据驱动惰性**：honey 块无 water_reaction 字段 →
  reaction pass（熔岩侧驱动）对蜜天然不触发；蜜/水/熔岩 6-邻共存
  互不替换（"他种类 = 阻挡"语义既有泛化，CellReader 4 元组 +
  kindLevel/otherLevel lambda 泛化）。
- **per-kind 容器 3→4**（PITS 299：kind 1 基下标 → 尺寸 = 种类数+1；
  3 尺寸时 Honey 静默 OOB 段错在下游测试）+ chunk_mesher 两种类三元
  派发 → 三种类（honey 顶面无波纹 = emitFluid 既有 kind==Water 门控
  天然排除）。
- **内容**：honey.json 块（非 solid/opaque、transparent、destroyable
  false、无 light_emission = 不发光、atlas 空位 [15,1] 免重锚）+ honey
  纹理（generate_block_textures.py honey() 琥珀光泽，既有 PNG 字节零
  变化）+ honey_bucket 物品（pours_fluid honey / pours_becomes
  bucket）+ bucket.json collects_fluid +honey（三种类一桶全舀）+
  **隐藏 starter 槽 16**（GEN-2 范式；PITS 300 = storage 范围
  [9,26]，槽 27 首版被点名拒绝）。
- **桶交互零改动继承**（FLUID-4/5）：蜜桶对空气 = 放置 / 对蜜流动 =
  变源 / 蜜桶对水格 = 替换倒（(c) 泛化 else 分支）/ 水桶对熔岩 = 反应
  倒（(c1) 特例不变）。

1774/1774（+8：编码带位级 / 门控冻结 / 每门控 tick 落一格 / 每门控
tick 扩一环 / 蜜水 6-邻共存互不替换 / 他种支撑不坠 / 门控不影响水 /
蜜顶面无波纹）、-Werror 0 警告。门 126：S1 = **第 126 次结构性免疫免
重锚**（fresh 60f 四方截图 ×2 387726 B 全字节一致 + savenorm new×2 /
old×2 EXACT；new vs old 存档差 = 恰好 2 项 honey_bucket 隐藏 starter
【存储格不渲染 = 截图免疫保持，GEN-2 先例】）+ **S2 六场景 PASS**
（CPU harness gen126 实证瞄准/落位 oracle）：A 蜜倒【new ×2 RAW
EXACT 407951 B：蜜源 (24,45,-8) L8 + 邻格 L7 封源池稳态 + slot0 =
bucket】/ W 水回归 A/B【s124 水场景 4 跑：new ×2 与 old ×2 各 RAW
EXACT 409647 B + **new_a ≡ old_a 字节一致 = 蜜代码结构惰性零回归**】/
B 落蜜【密封竖井 40f 无输入 ×2 RAW 708974 B：蜜 (24,39,-8) L8 井中
= 5 门控 tick oracle 逐位】/ C 落水对照【同井 40f：水 (24,37,-8) L8
已落底（21 帧落定）×2 RAW 708974 B —— **B vs C = 1/3 速证明**（同帧
蜜 y39 仍在落 vs 水 y37 已落底，互斥终态）】/ D 无反应行【蜜 24 / 水
25 / 封石 26 / 熔岩 27 全封 600f：三源恒 8/8/8 原位 + 行内零
obsidian/stone 转化 ×2 RAW 699617 B】/ S6【old v1.10.0 二进制 + 蜜
场景 = init 失败点名 unknown fluid 'honey'（新种类存档向前不兼容 =
记档）】。+ 性能 A/B 同 seed 900f（fluidCells win2/3 **逐位一致**
29803/29800 + win1 −0.02% 填充 real-dt 类 + PIPE3 changesApplied
1404=1404 / fluidCellsSeen 43994=43994 逐位 + 全指标同档 = **成本可
忽略**，无输入无蜜 = 门控与第三模拟段零执行结构惰性实证）。
PITS 299-300（per-kind 容器尺寸 = 种类数+1 哨兵槽纪律 / 隐藏 starter
槽范围 [9,26]）。**组 1.6-1.10 已于 v1.10.0 收官（tag + GitHub
release）；v1.11.0 = 新组 1.11-1.15 首版**（组内不打 tag = 10-28
定案 tag 策略）。

## v1.10.0（2026-10-28）

**FLUID-5 水熔岩反应**（v1.x 任务表 10/33，FLUID 行 2/3，门 125；消费
FLUID-4 范围外记档"水桶对熔岩面 = no-op（水-熔岩反应交互 = FLUID-5
行）"）——模拟侧水-熔岩反应 P51 已完整（FluidSim reaction pass：熔岩
6-邻水 → lava 块 water_reaction 数据驱动产物【邻水含 L8 源 = obsidian /
仅流动水 = stone】，水不消耗），**本项零新反应规则** = 桶×熔岩交互两
规则（MC parity，纯函数扩展 `bucketPourTarget` 候选接受，返回型不变，
game 写路径零改动）：
- **(c1) 水桶对熔岩格 = 反应倒**：水源放 candidate + 受击法向一步
  （顶视池格 = 正上方；侧向受击 = 开阔侧；无 block 命中 = +Y 约定），
  该格须 air 无流体 + canPlaceAt；熔岩由既有 reaction pass 下 tick 转
  产物（倒出的水是 L8 源 → 项目规则恒 obsidian = 单一规则源）；无空气
  位 = no-op 桶保留（MC：水放不下即什么都不发生）。
- **(c2) 熔岩桶对水格 = 替换倒**：setFluid 既有"替换他种流体"语义把水
  格换成熔岩源（桶消耗），reaction pass 下 tick 按残留邻水转
  obsidian/stone（无邻水 = 熔岩留存）——MC 同款（熔岩替换水随即反应）。
- **范围外记档**：蒸汽（P51 既有偏离保留）/ 流动熔岩舀取维持 P47 现状。

1766/1766（+10 改 1 RaycastPourTargetTest：反应倒目标 = 法向一步干空气
格【流动/源熔岩同目标 = 产物由 pass 决定与熔岩 level 无关】/ 无空气位
no-op / placement Y 越界 / placement 玩家重叠 / placement 持流体（防御
分支钉死）/ 水平法向开阔侧 / 无 block 命中 +Y 约定 ×2 / 替换倒源水与
流动水 = 水格本身；改 1 = P47 异种 no-op → FLUID-5 反应倒）、-Werror 0
警告。门 125：S1 = **第 125 次结构性免疫免重锚**（fresh 60f 四方 ×2
387726 B 全字节一致 new≡old v1.9.0≡base18 + savenorm EXACT——无输入 =
无右键 = (c) 分支零执行）+ **S2 三场景单次右键输入跑 PASS**（CPU
harness gen125_ray 实证瞄准几何；存档状态 = 权威断言；左点捕获协议
PITS 295）：A 反应倒【单封熔岩源 + 观察柱陡顶视】new ×2 熔岩 →
**obsidian** + 熔岩流体消失 + slot0 = bucket（平台浸水稳态）/ old ×2
熔岩恒 L8 + water_bucket【×2 RAW 对 EXACT 207329/205364 B】；B 替换倒
【FLUID-4 水 pool + 熔岩桶】new ×2 流动水格 → **obsidian** + 源水恒
L8 + slot0 = bucket / old ×2 水恒【L7+L8】+ lava_bucket【×2 RAW 对
EXACT 404382/408971 B】；C 守卫【2 格熔岩池，placement = 源熔岩格持
流体 → nullopt】new 熔岩恒【L7+L8】+ water_bucket 保留。**场景 A 双层
周界墙稳态化**（首版无墙 = 倒出的水漫出平台无限扩散 → ×2 RAW 帧差
发散 MISMATCH 实证；CPU 收敛探针 FluidSim::tick 循环至零 changed =
场景区 5 tick 稳态 → RAW 恢复 = **PITS 297**）。**PITS 298**（World::
getFluid 返回任一种类 level 0..8 非 chunk 编码 0/1..8/9..16——
fluidKindOf(getFluid()) 对熔岩格恒 Water，(c) 分支首轮 8 新测全败，
种类判定走 getFluidKind）。+ 性能 A/B 同 seed 900f（fluidCells win2/3
**逐位一致** 29803/29800 + win1 +0.006% 填充 real-dt 类 + PIPE3
changesApplied 1404=1404 + 全指标同档 = **成本可忽略**，无输入桶代码
结构惰性实证）。

**组边界 release**：v1.10.0 = 组 1.6–1.10 末版 → tag v1.10.0 + GitHub
release（覆盖 GEN-7 基岩多层 v1.6.0 / GEN-8 地形互感 v1.7.0 / GEN-9 耕地
湿润 v1.8.0 / FLUID-4 倒向水面 v1.9.0 / FLUID-5 水熔岩反应 v1.10.0；
10-28 定案 tag 策略 = 组内不打 tag、组边界 release 1:1）。

## v1.9.0（2026-10-28）

**FLUID-4 倒向水面**（v1.x 任务表 9/33，FLUID 行 1/3，门 124；消费
P47 流体桶记档偏离"倒水只能倒向方块受击面——对着水面/纯天空倒 =
no-op（MC 对水面的倒法未实现）"）——**纯函数 `bucketPourTarget`**
（world/raycast，canPlaceAt 同款落点）：倒水候选 = ① block 射线受击
面邻格（P47 放置约定——block 射线穿过流体，1 深水池的地面受击使池顶
水格成为候选 = "对着池子倒"成立）或 ② block 射线无命中（超 block
射程的开阔水面）时 fluid 射线自身命中的格；无 cross-fallback（block
命中恒优先 = MC 桶对块面瞄准）。候选过 canPlaceAt（Y 范围/实心/玩家
重叠，与旧路径共享）且满足：**(a) 空气无流体 = 旧放置（位级不变）**
或 **(b) 桶自身种类的流动格（level < 8）= 倒向水面，该格变 level-8
源**。已源格（level 8）/异种流体格 = no-op（桶保留，点击 fall
through，Minecraft 语义：源上倒源无变化）。tryFluidBucket pour 分支
重接，写入路径零改动（setFluid L8 + 列保护 + Lighting::updateLocal +
"splash" + 物品交换）。**零 format（19）/零 config/零内容/零音效**
（水/熔岩桶对既有 P47/P51；流体 = 世界编辑既有存档路径）。
**范围外记档**：① 流动水舀取维持 P47 现状（舀侧扩展另立项）② 水桶
对熔岩面 = no-op（kind 不匹配；水-熔岩反应交互 = FLUID-5 行）③ 倒出
的源走正常 FluidSim（既有）。1756/1756（+11 RaycastPourTargetTest：
旧放置位级 / 倒向流动 / 已源 no-op / 异种 no-op（水拒熔岩 + 熔岩桶收
熔岩 = 种类正确性）/ 实心 / 玩家重叠 / 开阔水（无 block 命中）/ 开阔
已源 no-op / 双无命中 / Y 越界 / block 命中优先无 cross-fallback）、
-Werror 0 警告。门 124：S1 = **第 124 次结构性免疫免重锚**（fresh
60f 四方 ×2 387726 B 全字节一致 new≡old v1.8.0≡base18 + savenorm
EXACT——无输入 = 无右键 = tryFluidBucket 零执行）+ **S2 单次右键输入
跑 PASS**（受控石台 + 封源池【源 L8 (25,45,-8) + 流动 L7 (24,45,-8)
+ 6 石封 = PITS 291 纪律】+ slot0 water_bucket；瞄准几何 = CPU
harness 实证：东玩家 (27.5,45,-7.5) 射线穿源格+流动格打西封石 +X 面
→ 邻格 = 流动格）：new ×2 流动格 → **L8 + slot0 = bucket（互斥终态）
+ "poured" 日志行** / old v1.8.0 ×2 流动恒 L7 + slot0 = water_bucket
（P47 旧 no-op 行为不变）/ S2b 西玩家经东封石瞄准已源格 → no-op 桶
保留（level-8 守卫）；×2 RAW/侧 + A/B 截图 diff 19,819 px = 手持图标
+ 热栏槽 0 + 池区水面（全为倒水直接后果）；输入协议 = 左点捕获（轮询
"Pointer captured" DEBUG 日志）+ 单次右键——**右键交互链只在指针锁定
时执行，裸右键静默丢弃（PITS 295）**；存档状态 = 权威断言（输入跑
协议定位）。+ 性能 A/B 同 seed 900f（fluidCells win2/3 **逐位一致**
= 零 worldgen 改动同世界 oracle + PIPE3 changesApplied 逐位 + 全指标
同档 = **成本可忽略**，无输入桶代码结构惰性实证）。PITS 295-296
（捕获协议 / 同槽双进程终档竞态 = 断言前验 clean exit）。只提交
（组内不打 tag = 10-28 定案 tag 策略）。

## v1.8.0（2026-10-28）

**GEN-9 耕地湿润**（v1.x 任务表 8/33，GEN 行 3/3 收尾，门 123；"耕地
湿润"原始定义已佚，代理定案 = farmland 湿/干视觉状态 = MC farmland
wet/dry）——块模型可选 `moisture_top`（FullCube-only 形状门控【其余 6
形状点名拒 = 死字段纪律】+ 严格非空串 + `requiredTextures` init 交叉
校验）；纯谓词 `soilIsWet`（Chebyshev 半径 4 同层水 level ≥1，熔岩不
湿 = P50 生长节奏同款规则——`cropIsWet` 改委托 = **生长节奏与视觉状态
一函数两消费者，永不打架**）；mesher FullCube 顶面 **mesh 时从活流体
场派生**（湿 tile/干 tile，湿干邻格永不贪婪并 quad）；Game 侧
`m_moistureBlocks` 追踪集（setBlock 收口数据驱动 + 读档 rescan）+
**每 10 帧周期 sweep 收敛**（worker 快照 mesh 可能在水流送前烘 dry，
且 flip 门对"tracked == live 但 mesh stale"死锁 → seed 点强制重烘 +
周期 sweep 双保险；无耕地世界 = 空 map fast-exit 结构零成本）。shipped
= farmland `moisture_top: "farmland_wet"` + atlas 空位 [9,4]（无 UV
移位免重锚）+ `farmland_wet.png`（同 salt 90/91/92 暗化 0.72×，均值
71.8/52.0/35.6 vs 干 101.5/72.4/48.6 = "同一片田、湿了"；生成器幂等
74 既有 PNG 字节零变化）。**零 format（19）/零 config/零新音效**——湿态
是世界派生量（block + fluid 编辑的函数），存档两侧 A/B 同形 = 强
oracle。1745/1745（+12：block 解析/内容钉 6 + growth 谓词 4 + mesher
3）、-Werror 0 警告。门 123：S1 = **第 123 次结构性免疫免重锚**
（fresh 60f 四方 ×2 387726 B 全字节一致 + savenorm EXACT）+ **S2 受控
平台场景双视角 PASS**（石平台 13×13 + 湿耕地 + 干耕地 + 4 石封源 L8
水：俯视 diff 108,542 px 全限耕地顶面带、100% new 更暗、old
(102,72,49)→new (72,52,36) = 纹理均值精确吻合 / 侧视 888 px 精确隔离 +
干耕地带零 diff + 场景区存档 EXACT【远端程序化熔岩湖 real-dt churn
5 格 = 同二进制 ×2 同位分歧，PITS 244/283 非缺陷类】）+ 性能 A/B 同
seed 900f（**fluidCells win2/3 逐位一致** + PIPE3 visitedChunks/
fluidCellsSeen/changesApplied 逐位 + 全指标同档 = 成本可忽略，零耕地
sweep 结构惰性实证）。**偏离**：二元湿/干（MC 是 0-3 四级 moisture
按水距衰减）/ 掉落物与手持立方保持干纹理（无世界上下文）/ 视觉收敛
延迟 ≤ 10 帧（0.17 s）/ 无 MC 水道机制。**记档**：大场景远景
（自然地形 8.5 块侧视 demo 列）湿耕地顶面新旧二进制同现不渲染 = 该
场景配置 GL 列 mesh 集成时机 quirk（CPU mesh 证湿 quad 已发射 + 受控
场景全正常 + old 侧同现 = 非 GEN-9 回归）→ 门改受控场景，KNOWN_ISSUES
候选。PITS 291-294（L8 源必须 4 石封【本项目源非 MC 静态源，240f
平台扩散 71 格实证】/ 大场景远景 GL quirk + 渲染不可见标准取证链
【红纹理判别帧 + CPU mesh 临时日志 + 世界视线盒 dump】/ zsh 无引号
`${var:-word}` 不分词 = gate 脚本 bash 化 / 场景 python 构建器静默未
写入 = 落盘后重读断言关键编辑）。只 tag（release 组 1.6-1.10 未满）。

## v1.7.0（2026-10-27）

**GEN-8 地形互感**（v1.x 任务表 7/33，门 122；"地形互感"原始定义已佚，
代理定案 = 山峰互感 mountain passes）——worldgen `terrain.passes`
新段（enabled/noise_scale (0,0.05]/threshold (0,1)/amplitude [0,64]
逐条点名，缺省段 = disabled 位级 legacy）；**coarse ridge mask**：
独立低频 fbm2（0.008 尺度 = 特征周期 ~125 块、3 octaves、kSaltPasses
专用 salt）> threshold 时 `bonus = floor(amplitude×(mask−threshold)/(1−threshold))`
只升不降，`heightAt = min(base+bonus, kWorldHeight−16)`，纯函数零
chunk 数据 = 跨 chunk 一致构造性成立；shipped {0.008, 0.55, 6} 探针
标定单轮定案（mask 实测 [0.07,0.88]：0.72 首选全世零 bonus 死参数，
0.55 = 全世 ~10% 列 bonus≥2 / max 4 / demo 区 242 列 +1）；**首版
局部峰设计废弃记档**（lround 平顶杀严格局部最大 = PITS 290）。1733/1733
（+11：config 7 + 生成 4）、-Werror 0 警告；4 契约演进重钉（GEN-3 矿
计数 3 值 + GEN-4 洞穴探针 3 测，方向 pin 逐条全保持）。门 122：S1 =
**第 122 次结构性免疫免重锚**（fresh 60f 三方 ×2 六跑 387726 B 全字节
一致：new≡base18≡old≡off；demo 区 242 bonus 列全在 z≥0 = 基线视锥
后方探针定量论证）+ **S2 山脊盆地排水判别 PASS**（玩家 (-304.5,35,560.5)
西望全世最强脊簇：off≡old 字节一致 = 代码级免疫 + ×2 RAW/侧 + diff
525,506 px 顶天带零 + old 侧 diff 区 30.5% 水域 → new 侧 0% 水域
100% 地形 = 抬升排水定案 + player 态 EXACT）+ 性能 A/B 同 seed 900f
（冻结起跑态流体格被抬升吞没 → 剥 fluids 重生 seed；render/swap/stream
同档或更优 + 每流体格成本 14.2→13.9µs + PIPE3 同档 = 代码成本可
忽略）；坑 = Firefox 同名窗平铺分半 + 黑窗双因（尺寸门 (1362,744)
定案）+ raw string 缺 `)"` 吞并下一个 TEST 宏（--gtest_list_tests
发现）。只 tag（release 组 1.6-1.10 未满）。

## v1.6.0（2026-10-27）

**GEN-7 基岩多层**（v1.x 任务表 6/33，门 121；消费 KNOWN_ISSUES Phase 4
"基岩只有单层"）——worldgen `terrain.bedrock_layers` 新字段（int [1,8]，
缺省 1 = 旧行为位级复现，越界/非整数点名拒）；shipped = **3 = MC 经典
基岩底**（y0/y2 恒床岩 + y1 每格确定性 50% roll `bedrockLayerRoll`
纯谓词，kSaltBedrock 专用 salt）；`fillTerrain` 单分支，layers=1 恒假
= 向后兼容；零 format（世界生成演进 = 旧存档新底先例）。1722/1722
（+8：config 5 + 生成 3）、-Werror 0 警告；GEN-3 矿计数契约演进重钉
4 值（y1/y2 变床岩 = 可放置深石集缩小，GEN-4 先例）。门 121：S1 第
121 次结构性免疫免重锚（基岩底 y0-2 在出生点视图外）+ S2 竖井壁带
判别（3×3 全深井 + 悬台内瞰）：diff 13172 px 93.2% 集中横带 + 99.0%
new 更暗（old 石亮 95.9 / new 床岩暗 42.1 = 解析值吻合）+ savenorm
EXACT + ×2 RAW/侧 + 性能 A/B 同起跑态 fall 900f fluidCells 三窗
229457/218800/221654 逐位一致 + 全指标同档 = 成本可忽略。PITS 289
（init 失败后旧产物残留 = 假 PASS：探针验证四件套 ERROR/clean exit/
真终档/截图 mtime）。只 tag。

## v1.5.0（2026-10-27）

**LGT-5 半块透光**（v1.x 任务表 5/33，门 120；**release 组 1.1-1.5 组末
= 本版本发 GitHub release**）——slab/stairs/wall/fence（7 块）加
`light_opacity: 0` = **MC 半块透光规则**（光如空气穿过，竖直天空免费 +
每格成本 1 = enterCost max(1,0) 与空气逐值同），**零核心代码**（LGT-2
统一进入格成本机制消费面 +1）；`opaque: true` 保持 = 剔面/网格渲染零
变更（P85 半块剔面局限保留 = 记档偏离）。1714/1714（+3：内容钉死 1 +
竖直天空透光 1 + 密封盒块光层透 1）、-Werror 0 警告；门 120：S1 第 120
次结构性免疫免重锚（半块永不生成 = 世界光场零变化）+ S2 密封石室判别
（**唯一开口 = slab 格 + 外壁 glowstone 15**）：old 室恒暗 (10.6) vs
new 光穿半块灌亮全室 (76.8)，diff 945782 px（93.3% 室内面）0 px 变暗 =
纯光增益 + savenorm EXACT（世界全同）+ ×2 RAW/侧 + 性能 A/B 同起跑态
fall 900f：fluidCells 三窗 229457/218800/221654 双侧逐位一致 + 全指标
同档 = 成本可忽略。PITS 288（场景构建覆盖 base 块编辑 = 孤儿 fires 交叉
校验 init 死）。**组 release 1.1-1.5**：notes 覆盖 v1.1.0-1.5.0（STRM-2
流体列保留 / PIPE-3 流体 per-cell / LGT-3 夜光重传播+月光地板 / LGT-4
光束入光 / LGT-5 半块透光）。

## v1.4.0（2026-10-27）

**LGT-4 光束入光**（v1.x 任务表 4/33，门 119/119b；信标光束 = 全项目最后
一个恒白 unlit 消费者）——复用 PRE-6 per-draw 光照覆盖范式：纯函数
`beamLightSample`（**信标块上方空气格 (x,y+1,z)** = beam 最低格的
sky/block ÷15；信标块自身格不透明恒 (0,0) = 门 119 E2E 揪出并修复的采样
缺陷，OwnCellOpaqueAlwaysIgnored 回归单测钉死）+ `u_drop*` 三 uniform
改名中性（u_useLightOverride/u_lightOverrideSky/u_lightOverrideBlock）+
每 beam draw 后 bind+复位 0（UXFIX-1 泄漏纪律）；beam 深夜 = LGT-3 月光
7/15（非恒白）、近萤石/火把亮化、白天露天 = 1.0 恒白（df=1 恒等根）；
零 format/config/内容。1711/1711（+7）、-Werror 0 警告；门 119：S1 第
119 次结构性免疫（fresh 60f 四方 + base18 连续，最终二进制复验）+ S2a
首版场景 beam 失明废弃（on/off 双二进制 diff=0 实证，PITS 287）+ S2b
门 119b（pitch 1.25 取景）：sanity day on/off 1646 px + day df=1 恒等
RAW（128226 B 字节一致，beam 在框内非平凡）+ night ×2 RAW/侧 + **night
beam 列 1646 px 全限竖直窄带仅暗化（带均 (66,105,116)→(33,53,63)，
比值 0.483 解析吻合，零亮化，带外 0 diff）** + savenorm EXACT + 性能
A/B 同起跑态 fall 900f：fluidCells 三窗 214627/194500/230306 双侧逐位
一致 + 全指标同档 = 成本可忽略（无信标结构惰性）。只 tag（release 组
1.1-1.5 末发）。

## v1.3.0（2026-10-27）

**LGT-3 夜光重传播 + MC 月光地板**（v1.x 任务表 3/33，门 118；深夜可夜视
= 10-27 夜用户实机定案）——呈现层昼夜缩放升级为**天空光场对源等级仿射
（无损）重演**：所有天空光源共享源等级 S 且逐格成本与 S 无关 ⇒
`有效天空光 = max(0, stored − (15−S))` 逐格精确（confluence 单测 = 源等级
S 直接重传播 vs 仿射逐格相等钉死），**零重传播**（shader 天空项一行 +
生成光门整数镜像）。**MC 月光地板**：S = 7 + 8×dayFactor（`kNightSkyLevel
= 7` = 午夜露天月光 7/15，深夜不再全黑 = 夜视能力恢复）+ 敌对
`spawn_max_light` 0→7（MC 配对：夜露天 7≤7 可生成保夜险 / 火把圈 14>7
免疫 / 白天 15>7 不生成 = 玩法闭环）。正午 df=1 恒等 = 免重锚。1704/1704
（+9）、-Werror 0 警告；门 118：S1 第 118 次结构性免疫免重锚 + S2 深夜
0.75 A/B 828,043 px 全仅亮化 + 黄昏 0.50 同数 diff + open-sky 锚盒零
diff + savenorm EXACT + 性能 A/B fall 900f 全指标同档 + fluidCells 三窗
逐位一致 = 零成本变更。只 tag（release 组 1.1-1.5 末发）。

## v1.2.0（2026-10-27）

**PIPE-3 流体 per-cell**（v1.x 任务表 2/33，门 117）——99.8 chunk 粒度
活动门控的 per-cell 化：tick 成本从 O(visited chunk × 32K 格 × 2 遍 +
全流体 chunk 集合遍历) 降到 **O(活动 chunk + 活流体格)**：per-chunk
per-kind 流体格列表（`setFluidAt` 成员变化 push / drain lazy stale /
walk 过滤+列序+去重+压缩，不变式 = 活格 ⇒ ≥1 条目）+ World per-kind
活动 chunk 集合（6 插入点穷尽 flag 置位点，walk 消费，flag 双检 belt）
+ falling 标志 per-column 化（colFlag[256] + 跨 chunk topFlags[256B]，
引用语义位级同旧）+ reaction/burn 活格扫 + propose map max 合并（序
无关）。流体状态**逐位不变**（门 117 S2 位级一致 oracle：fall/fill 跨
侧 savenorm EXACT + 截屏 RAW 字节同；cascade nospread 位稳 oracle 存档
RAW 854590 B 逐字节同；fire 区 1 格差 = 同二进制自分歧实证 = 既有
real-dt 类，非回归）。性能 A/B：fall 流体段 −50%（1261→631 ms/300f
窗）、活跃 tick 34.8→12.6 ms（2.8×）、fill churn 窗 −80%、沉降下界
−50%，fluidCells 逐位一致 + 帧指标全同档 = 纯收益。顺带修
`setFluidAt` 潜伏 debug assert（lava 编码 9..16；NDEBUG 零变化）+
test_mesh_job static_assert 契约演进（vector 成员）。只 tag（release
组 1.1-1.5 末发）。

## v1.1.0（2026-10-27）

**STRM-2 流体列保留**（v1.x 任务表 1/33，门 116）——仅流体模拟改动的列
（远端熔岩流/海岸膜，无 fire/状态条目 = 无卸载保护资格）卸载后重置为
生成态的修复：驱逐时 journal diff 快照按列保留（`retainColumnDiff` =
`journalDiffChunk` 字节同语义，保留 diff 条目非 chunk = 内存有界）+ 列
再流送时 integrate 回放（blocks 先 fluids 后、不插 m_editedColumns =
fluid-only 列保持可卸载）+ 退出存档双路径 `mergeRetained`（含
--save-full-diff oracle 不失真）。零 format（19）/config/内容。**随门
116 场景首触发修复 99.8 活动门流送边界活性缺口**：settled（冷）流体
前缘对 void 的 spread 被拒后转冷，目标列流送到达不唤醒 = 级联永久卡
在流送边界（插桩实证 395 tick 零推进）；修 = `insertChunk`/
`getOrCreateChunk` 的 `wakeFluidChunks`（自身 + 6 面邻持该 kind 的
chunk 各走一次，镜像 setBlock UNSTICK；唤醒仅在插入事件 = 性能面零
回退）。性能 A/B fall 900f：全指标同档或更优（flush −3..−10%、stream
−3..−6%）；fluidCells 123879 逐位一致 + 167781→167782 /
236573→236575（+1/+2 = 活性补洞行为语义：静态梯度提前一格越流送
边界，非 churn 放大）。只 tag（release 组 1.1-1.5 末发）。

## v1.0（2026-10-27）

**首个正式版**：v1.0 任务矩阵 34/34 全完成（十大系统 + 2 项实机报障）。覆盖
v0.31.0 / v0.32.0 两版 + 版本号收口（二进制内版本字符串自 v0.16.0 起滞后，
本次恢复诚实：启动日志/F3 首行 = `TokenMine.cpp v1.0`）。正式版 release
见 GitHub Releases "TokenMine.cpp v1.0.0"（原 tag v1.0，10-27 改名；
Latest = v1.5.0 组 release）。

## v0.32.0（2026-10-26）

**FLUID-3 mob 涉水修正**（任务表 34/34，实机报障修复）——牛羊鸡走入水中/
悬崖边的"悬空/站在水上"：浮漂终态从干身踩水皮改半浸没游泳位（身体中轴 =
水面，MC 游泳观感）+ 深水（≥2 格）池底 mob 可上浮出底 + 1 格浅池涉水不变
+ 涉水 mob 可踏 1 格岸壁出池 + 悬崖坠水不再"降落伞"减速（全速过水面带）。
玩家游泳行为位级不变。门 115。

## v0.31.0（2026-10-26）

**UXFIX-1 实机渲染修复**（实机报障修复）——两条：① 热栏/面板的方块物品
图标显示为半透明白盒 ② 挖掘时方块显示全黑（裂纹白线在、块面纯黑）。根因 =
PRE-4 云引入的 GL per-draw 状态泄漏（u_useTexture/u_color/u_alpha + blend
状态跨 draw 残留），锁定态专属、无输入门全盲；修复 = 云/光束 blend 所有权
上移 + 挖掘覆盖层自持混合 + 方块图标立方显式纹理配方。无输入位级恒等。
门 114。

## v0.30.0（2026-10-26）

**PRE-6 掉落光照**（任务表 32/32 全完成，PRE 行 6/6）——v1.0 候选。
掉落物受本地光照：洞穴里的掉落不再亮如白昼、火把旁的掉落被照亮（每帧采样
所在格光照，昼夜自动跟随）；`render.drop_light` 可关（代码默认开，
config.json 零改动），关 = 与 v0.29.0 位级恒等。

## v0.29.0（2026-10-26）

**PRE-5 透明 discard**（任务表 31/32，PRE 行 5/6）——修复"从树叶/玻璃孔
看不见后面"：alpha-0 像素不再写入深度（cutout 段 discard + 深度写，
半透明段水/玻璃/云改为不写深度）；物品掉落从叶孔可见。20 个 cutout 块
数据驱动路由（leaves/花草/麦/苗/火焰/火把/营火），`render.alpha_test`
可关（代码默认开，config.json 零改动），关 = 与 v0.28.0 位级恒等。

## v0.28.0（2026-10-26）

**PRE-4 云与环境音**（任务表 30/32，PRE 行 4/6）——天空云层：y=108
半透明平板随帧锁定漂移（MC 节奏 100 s/格），相机移动/漂移步进才重建
（零每帧开销）；昼夜环境音：洞穴/白天/夜晚三上下文各一环境底噪（新增 3
个确定性音效，音量低于全部 SFX），5 s 一掷的帧锁定调度。`render.clouds` /
`audio.ambience` 均可关（代码默认开，config.json 零改动），关 = 与
v0.27.0 位级恒等。

## v0.27.0（2026-10-26）

**PRE-3 水面波纹**（任务表 29/32，PRE 行 3/6）——静态水面帧锁定 shimmer：
水顶面 UV 采样窗逐帧平移（白噪声纹理 + NEAREST = 清晰 texel 台阶），走
既有 tiled 采样路径（fract 自动 tile 内 wrap，零 tile 识别）；
`render.water_ripples` 可关（代码默认开，config.json 零改动），关 = 与
v0.26.0 位级恒等。

## v0.26.0（2026-10-26）

**PRE-2 边缘雾**（任务表 28/32，PRE 行 2/6）——世界边缘距离雾：流送圆盘
边缘的原始 chunk 切面（"渲染条"）向同帧天空色线性溶解（雾程 =
min(远平面, 16×min(view_distance,64)) 块的 75%→100%），昼夜跟随；
`render.fog` 可关（代码默认开，config.json 零改动），关 = 与 v0.25.0
位级恒等。

## v0.25.0（2026-10-26）

**PRE-1 中文字体**（任务表 27/32，PRE 行 1/6）——UI 文本中文支持：UTF-8
解码渲染（坏字节回退 `?`）+ 32 个 CJK glyph（图集死格复用，尺寸零变化）
+ 8 处用户可见 UI 字符串中文化（死亡屏/面板标题；F3 调试屏保持英文）。

## v0.24.0（2026-10-26）

**SAVE-1 自动存档**（任务表 26/32，SAVE 行 1/1 收尾）——周期性自动存档
（`save.auto_save_seconds` 默认 120 s，真实 dt 节奏，无存档槽不启用）+
崩溃韧性原子写（temp + rename，手动/退出/自动三路共用——kill -9 不再
产出半写档）。

## v0.23.0（2026-10-26）

**CRAFT-3 材质音效**（任务表 25/32，CRAFT 行 3/3 收尾）——per-block
材质音色（`sound_material` 数据字段，6 材质 × 3 事件 = 18 个确定性新音效，
32 块赋值，无字段块位级不变）+ 3D 空间化（SoundBank stereo 化，距离衰减
cutoff 16 块 + 恒定增益声像，dig/破坏/放置/旋转/丢弃/拾取世界空间化，
UI/手持音效保持 2D）。

## v0.22.0（2026-10-26）

**CRAFT-2 物品 3D 模型**（任务表 24/32）——per-item 数据驱动 box 模型
（item JSON 可选 `model` 段 = 1..16 个 axis-aligned box + 单一纹理，严格
解析），三上下文渲染：手持（替代 icon 面片、继承弓拉/进食举到嘴变换）/
世界掉落（替代不可见掉落）/ 进食举到嘴（零额外代码）；16 内容模型（工具
9 + 弓 5 + 苹果 2，纹理 = 既有图标 PNG，零新纹理）；无 model 物品三上下文
位级不变。

## v0.21.0（2026-10-26）

**CRAFT-1 工作台分块**（任务表 23/32）——每个工作台独立 3×3 容器
（format 19 `tables` 段 + 面板绑定所点块，炉子/营火 per-block 范式；
破坏丢弃容器物品 = 炉子同款记档）。

## v0.20.0（2026-10-25）

**GEN-6 树放置半区修复**（任务表 32/32 执行序 22 项）——修复 Phase 4 起
潜伏的 wz 循环上界 bug：约世界一半（cz ≥ cx+1 对角半区）恢复种树/放装饰
（wz 上界 2 处一行级修复，零 format/config/content，跨 chunk 无缝契约
保持）。

## v0.19.0（2026-10-25）

**SURV-5 mob 效果**（任务表 21/32，SURV 行 5/5 全清）——状态效果双载体：
skeleton 箭带 wither（1HP/4s 无地板可杀）、毒弓 poison_bow（箭中 mob 中毒
60s 可毒死）、不死族免疫 poison（MC 语义）。

## v0.18.0（2026-10-25）

**GEN-5 浮空树**（任务表 20/31，用户报障修复）——洞穴挖掉的地面上不再长树
（caveCarvesCell 纯谓词守卫，carveCaves 行为恒等，零 format/config/
content）。

## v0.17.0（2026-10-25）

**GEN-4 洞穴破碎度**（任务表 19/31，用户报障修复）——洞穴重标定：洞体更
大更少，小洞窟 −91%，中位洞体积 9-130×（noise_scale 0.09→0.05 单参数，
零核心改动）。

## v0.16.0（2026-10-25）

**SURV-4 药水饮用**（任务表 18/29）——6 种单效果药水（虚弱 1:30 / 迅捷
3:00 / 再生 0:45 / 中毒 0:45 / 治疗 +4 瞬时 / 伤害 −6 瞬时护甲豁免）+
玻璃瓶舀水-倒水循环 + `instant_hp` 瞬时效果原语；喝 = 按住右键 1.6 s
返还玻璃瓶，再喝同效果重置不叠加。无酿造台（starter 发放，记档偏离）。
format 18 不变。

## v0.15.0（2026-10-25）

**SURV-3 耐久**（任务表 17/29）——27 件工具/护甲/弓 MC 精确耐久值 + 五类
磨损（破块/打 mob/射出/点燃/护甲减伤事件）+ format 18 + HUD 2px 耐久条；
无修复系统（记档偏离）。

## v0.14.0（2026-10-25）

**SURV-2 氧气窒息**（16/29）——水下氧气条 + 溺水伤害（帧锁定 15 s 空气 /
每 2.5 s 2 HP，溺水豁免护甲减伤 = MC 伤害表实证），format 17。

## v0.13.0（2026-10-25）

**SURV-1 护甲**（15/29）——12 件护甲 MC 精确值 + 整数 floor 减伤（全伤害
源含坠落）+ 右键穿戴/交换/脱下，format 16。

## v0.12.0（2026-10-25）

**ENT-4 水体交互**（14/29，ENT 行收尾）——mob/掉落物浮力（被动上浮/潜行
潜水/水面漂浮），陆上行为逐位不变。（FLUID-3 起共享模型演进：游泳位/
深度门/踏阶解禁，见 v0.32.0。）

## v0.11.0（2026-10-25）

**ENT-3 mob 踏阶**（13/29）+ **ENT-1b 模型修复**（实机反馈）——mob 可上
1 格台阶（与 player 同判定）；四足比例/关节/穿模修正。

## v0.10.0（2026-10-25）

**ENT-1 四肢摆动**（12/29）——mob 行走摆腿动画：纯渲染层，数据驱动
`swings` 段 + 编译期定点正弦表（零运行期 libm）。

## v0.9.0（2026-10-24）

**99.11 流体光照异步化**——"转摄像头卡"深层根因修复：列粒度 worker
追赶，海洋世界静置 update p95 秒级 → ~90 ms，vd16 冷填充 5.5×。

## v0.8.0（2026-10-24）

**99.10 光照 job 并行**——流送期光照下限 16.7 → ~10 ms；新旧二进制 300
帧字节一致（confluence 证明）。

## v0.7.0（2026-10-24）

**游泳划水**（FLUID-2）——被动上浮/潜行潜水/水面跳出，对齐 MC。

## v0.6.0（2026-10-23）

**水量守恒**（FLUID-1）——每格流体变化归入具名事件（会计恒等式单测钉
死），熔岩同款。

## v0.5.0（2026-10-22）

**矿脉形态 MC 化**（GEN-3，GEN 行收尾）——3×2×1 旋转椭球脉；旧存档可读。

## v0.4.0（2026-10-22）

**树生长**（GEN-2）——3 树种可种植树苗 + 数据驱动树形生长（format 15）。

## v0.3.0（2026-10-22）

**BlockState 通用 per-block 状态**（ENT-2）——楼梯朝向泛化为状态机制
（format 15，旧存档直接可读）。

## v0.2.0（2026-10-22）

**任务表前五项**（STRM-1/PIPE-1/PIPE-2/LGT-2/GEN-1）——流送环圆盘化
（vd80 内存 −49.5%）、网格化搬 worker（主线程 remesh −96%）、存档增量
（退出 6.2 s → 17 ms）、叶子光衰减、面条洞穴。

## v0.1.0（2026-10-17，首个公开版）

功能阶段 P0–P99 + 性能系列 99.x 全部完成：可玩的世界生成/生存/Mob/弓/
合成/流体/昼夜；1195/1195 测试、`-Werror` 0 警告。
