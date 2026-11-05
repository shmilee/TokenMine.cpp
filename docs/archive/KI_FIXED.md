# 已知问题 · 已修复条目存档（v2.x 起）

> pre-2.0 已修复索引见 `docs/archive/PRE2.0-FIXED-INDEX.md`；条目原文含修复史，勿改写。

## 渲染（已修复条目）

- ~~**叶子挨叶子的内部面仍会生成**~~ — `[v2.10.0]` 已解决：块 JSON 可选布尔 `cull_same_id`（严格解析点名；leaves/birch_leaves/spruce_leaves 首消费，shipped 恰三 = 零影响数据钉）+ mesher 全方块 greedy pass 同 id 非 opaque 邻剔面（**纹理选择前 = greedy merge 不见该面**；对称性 = 共享 registry 条目构造保证）——MC fast-graphics 叶子语义（fancy 观感不在范围 = 记档）；红线：glass-glass 不剔 / 水-水流体规则（`[Phase 9]`）/ cross·slab·stairs·wall·fence·pane emitter 零触碰（单测几何钉：LeafLeaf 24v=6 quads vs 32v，跨 chunk 20v vs 24v）。像素影响 = 叶冠孔洞透视（门 181 S1 **base18 重锚**：diff 4185 px 全叶冠区，天空/UI 零泄漏，新锚 178148 B）。`[Phase 3]`（水部分 `[Phase 9]`、叶子部分 `[v2.10.0]` 已解决）
- **~~玻璃/叶子不衰减光~~** — `[Phase 7]` 已修复 `[LGT-2]`（v0.2.0，详见 docs/archive/v1x/phase-lgt2.md）：块属性可选字段 `light_opacity`（0-15，缺省由 opaque 派生 = 未设内容逐位复现旧行为）+ 统一进入格成本（流体 3 / 垂直天空 = O / 其余方向 max(1, O)）；shipped leaves 族 3 块 2 / glass 1 = MC 值。**残余偏离**：MC 叶子 fancy 选项（纹理 alpha 不均衰减）未实现 = 均匀衰减。水的每格 -3（`[Phase 9]`）为既有先例。
- **~~水面是 level/8 缩放的立方体顶面，无波纹动画~~** — `[Phase 9]` 已修复 `[PRE-3]`（v0.27.0，详见 docs/archive/v1x/phase-pre3.md）：静态水面 → 帧锁定 shimmer（水纹理 = 纯白噪声 GL_NEAREST，顶面 UV 采样窗逐帧平移即闪烁；幅度 0.06 tile ≈ 1 texel / 波数 0.8 rad/block / 周期 120 帧 = m_frameCount % 120 纯函数；`render.water_ripples` 代码默认 true，off = 对 v0.26.0 位级恒等）。**记档偏离**：熔岩不闪、侧/底静态、仅顶面波。
- ~~**按面采样光照，无逐顶点插值/环境光遮蔽**~~ — `[v2.19.0]` 已解决（**V2-18 AO 环境光遮蔽**）：经典 MC 顶点 AO bake 入既有 skyLight/blockLight 顶点 attribute（viewer 侧 C+n 基 3 邻格 = 与顶点共享边的前方三格 × opaque ∧ 高度带闭区间【满块/stairs/wall/fence [0,1]、slab 0/1/≥2 = solidRange 同款 kSlabHalf、越界不遮】× 4 档 MC 精确系数 1.0/0.75/0.5/**0.3 双邻特例**；smooth = GPU 双线性）+ MaskCell 合并键扩 ao[4]（AO 异断 run，MC greedy+AO 同款）+ slab/stairs 发射 y 判带（半块同款 MC「顶点只被同高度邻格遮」：中平面恒不遮）；**fluid/cross 免 AO（MC 精确：water surfaces 不受 smooth lighting 影响）+ wall/fence/glassPane 免 AO（偏离 4 薄几何范围外）**；**render.ao 严格 bool 默认 true，off = pre-V2-19 位级恒等**（fixer 自证 off 60f ≡ 旧二进制逐字节 + on-vs-off diff 单调变暗审计 99.83% 纯变暗）；4 条记档偏离：① MC 4abcd 中心修正项省略（attribute 模型不可重构 fragment 4 顶点原值；夜间 retreat 作用于 AO 后 sky 曲线微差同条）② stairs 斜面高度精化不做（高沿过遮）③ per-draw 光覆盖面（信标/掉落/XP 球）AO 消失（特效光覆盖优先，MC 光束亦不 AO）④ wall/fence/glassPane 免 AO。零 shader/零顶点格式（15 float）/零新 uniform/零 format（28 不变）。详见 docs/archive/v2x/phase-2.19.0.md。`[Phase 7]`
- ~~**无云、无昼夜音效**~~ — `[PRE-4]` 已解决（云层 + 昼夜环境音 3 上下文）；**`[UX-9a]`（v1.48.0）云方块化**：两级颗粒（region 16 格 50% ∧ sub 4 块 75%）+ 厚度 4 向边界带 MC 侧壁着色 + 独立 cloud program fragment 逐像素（iGPU buffer 140ms/MB 卡顿根因消除，跨格 render 142.9→2.5 ms）+ 云延伸地平线（384 块窗口硬边消失），详见 phase-ux9a.md。`[Phase 8]`（`[PRE-4]`/`[UX-9a]` 已解决）
- ~~**alpha cutout 无 discard（透明纹素仍写深度）**~~ — `[v2.10.0]` 确认划解：**PRE-5 起已全覆盖**（`render.alpha_test` 代码默认 true 时 `alpha < 0.5 → discard` **桶无关**作用于全部 basic-program draw——cutout pass = discard + 深度写开，blend pass = discard + 无深度写）；`[v2.10.0]` 纹理普查实证：含 0-alpha 像素的块纹理 21 个 = 20 纯 cutout（→ cutout pass）+ glass 唯一非纯（blend pass 已被桶无关 discard 覆盖），water/glass_pane/honey 无 0 孔（discard 不触发）——花/十字植物（`[Phase 45]`）/leaves/glass 孔遮挡全部消除；**普查单测钉**（test_alpha_cutout `EveryHoleTextureIsDiscardCovered`：含 0 孔纹理 ⊆ discard 覆盖路径，防未来漏检）。`[Phase 3]`（十字植物 `[Phase 45]`、全量 `[PRE-5]` 落地 + `[v2.10.0]` 普查确认）

## 世界 / 生成（已修复条目）

- **~~洞穴是"奶酪洞"~~** — `[Phase 4]` 拓扑升级 `[GEN-1]`（v0.2.0，详见 docs/archive/v1x/phase-gen1.md）：单噪声奶酪洞 → 两 3D value-noise 中位带相交管状隧道（MC spaghetti 同款）+ 洞口 surface_guard 0 + spawn_clear_columns 2 出生区结构保护 + 每湖 lava 带排除盒；参数再标定 `[GEN-4]`（v0.15.0，noise_scale 0.09→0.05 = 管粗洞大、口袋 −91%、中位洞体积 9-130×）；浮空树 `[GEN-5]`（v0.17.0）修、树放置半区 `[GEN-6]`（v0.18.0）修。
- ~~**矿脉是简化随机游走**~~ — `[GEN-3]` 已解决：**MC 3×2×1 旋转椭球脉**（`world/ore_vein` `planOreVein` 纯函数：整数谓词 `4u²+9v²+36w²≤36` = 21 格/球，6 轴置换取向，`vein_size` 语义演进为**椭球簇数 K ∈ [1,8]**；shipped 全 K=1 + 单轮密度标定）。格网/概率门/起点/盐流/跨 chunk 无缝/deep-only 契约逐位保留。
- ~~**基岩只有单层**~~ — **`[GEN-7]` 已解决（v1.6.0：`terrain.bedrock_layers` 3 层 MC 经典底：y0/y2 恒床岩 + y1 每格确定性 50% roll，`bedrockLayerRoll` 纯谓词；世界生成演进零 format）**。`[Phase 4]`
- ~~**水平速度无惯性**~~ — `[ENT-7]` 已解决：地面/空中指数趋近模型（起步 ramp + 松键滑行，MC 0.5448/tick 地面摩擦档 / 0.91/tick 空中拖曳档，fps 无关 exp(−k·dt)，stop-eps 恰好 0 = 静止态确定性）；FLUID-2 档（水/水面/熔岩）保持瞬置 = 流体模型独立调参（记档偏离）；mob 侧 AI 每帧设精确速度 = 项目设计（范围外，详见 phase-ent7.md）。`[Phase 5]`
- ~~**水量不守恒**~~ — `[Phase 9]` 问题，**`[FLUID-1]` 已修复**（路线 A，细节见 `archive/v1x/phase-fluid1.md`）：修复前流体体积有三条不记账路径——浅膜落入更深流动池被"吸收"（池保持原水位，**合法具名事件 α**）；下落链落入世界底部直接消失（**合法具名事件 δ**）；**落入池时按等级抬升池水位**（无源增益 = 唯一不守恒的体积增加路径，已删除）。修复 = 删抬升一分支的**统一池规则**三分支：下方满格（level 8）→ 本格拉满到 8（**κ**，source 托底补满）；下方流动池 0<B<8 且 L<B → 本格拉入池（**α**，池水位 B 不动）；L≥B → **静态沉积**（零写入：本格保留、池水位不动 = 池水位逐格单调不增）。守恒目标 = **逐 tick 会计恒等式**而非"总量 ±0"（后者是错误目标——MC 流前缘创生 level-L-1 邻居合法，记为具名事件 **σ**；下落传送带为成对移动净零）：封闭区域流动格数 F 与源数 S 满足 F' = F + σ_c − α − κ − δ、S' = S + κ，单测逐 tick 钉死（封闭盒 64 格精确终态表 + P90 湖精确终态计数）。**残余** = 站流水瀑布/崖膜的永久轮换（下条，接受现状）。`[Phase 9]`
- **~~只有一种流体（水）~~** — `[Phase 9]` 已解决：`[FLUID-5]`（v1.10.0）熔岩（9..16 编码、水/熔岩 6-邻反应 → obsidian/stone、熔岩桶、燃烧 pass）+ `[FLUID-6]`（v1.11.0，详见 docs/archive/v1x/phase-fluid6.md）蜂蜜（17..24、粘度 = 帧锁定 tick 周期 3、无 water_reaction 字段 = 反应 pass 数据驱动惰性）= 三种类 6-邻共存互不替换。`fluidBlock()` 取 `properties.fluid` 数据方块作纹理参考的机制保留；**残余**：种类词汇表在代码（FluidKind），加新种类仍需改编码区间（记档偏离）。
- ~~**草方块掉小麦种子（不再掉泥土）**~~ — `[Phase 52]` 已修复：grass `drops` 改回 `dirt`（MC 语义）+ `chance_drops: [{wheat_1, 1, 0.125}]`（偶尔种子，0.125 = MC 基础概率）。种子来源 = 草概率掉 + 未熟作物（`wheat_1..6`）掉落。`[Phase 48]`（概率系统 `[Phase 49]`、多掉落列表 `[Phase 52]` 已就位）
- ~~**作物可种在任何朝向的表面**~~ — `[Phase 53]` 已修复：新增 block 可选字段 `plant_on`（方块名数组）——放置时目标格**正下方**须为列表之一，不符 = 静默 no-op（不消耗物品）；纯函数 `canPlantAt`（canPlaceAt 同款可单测）+ init 每条目存在性校验（grows_on 纪律）。首个使用者 = `wheat_1..6` 的 `["farmland"]`（Minecraft 耕地约束落地）。`plant_on`（放置门）与 `grows_on`（生长门）相互独立。`[Phase 48]`（生长条件 `[Phase 50]`、放置限制 `[Phase 53]` 均已解决）
- ~~**卸载/未加载 chunk 的作物不持久**~~ — `[99.2]` 已解决：**含作物列永不卸载**。作物放置/生长/被烧/破坏全走 `setBlock` 收口 → 所在列入 `m_editedColumns`（**insert-only 无修剪**，"player-edited columns are never dropped"）且未熟作物另在 `m_crops`——`unloadFarColumns` 对编辑列 + 五类 per-block 状态列（炉子/营火/**作物**/信标/火）双重豁免（见「Chunk 流式」99.2 条）。作物 = 方块编辑随存档（无独立 crops 键），读档重放编辑 + `rescanCrops` 全量重建；生长每帧 tick 与玩家距离无关。唯一相关残余 = 仅流体模拟改动（无 tracked state）的远端列卸载后回生成态（99.2 残余①），**不适用于作物**（作物格恒为编辑）。`[Phase 48]`（`[99.2]` 已解决）
- ~~**概率次级掉落每方块至多一个条目**~~ — `[Phase 52]` 已修复：新增 `chance_drops`（`{item, count, chance}` 严格三元组数组，与单条目 `chance_drop` **互斥**【同时写拒绝】）。各条目**独立 roll**（`rollChanceDropEntry`，条目索引去相关 salt），breakBlock 循环条目、每条目至多 1 掉落实体；init 每条目复用 drops 纪律（物品存在 + max_stack）。单条目 `chance_drop` 保留（系统 + 单测覆盖；`[Phase 55]` leaves 迁入列表后一度**无现役内容使用者**，`[Phase 59]` gravel→flint（10%，MC 值）重新使用它）。`[Phase 49]`（多掉落列表 `[Phase 52]` 已就位；内容扩展 `[Phase 55]`、`[Phase 59]`）
- ~~**flint 无用途（v1 文档化偏离）**~~ — `[Phase 60]` 已解决：**箭头配方 = shapeless flint+stick+feather → 4 箭（MC 1.14+）**，**替换**了旧的 2 木棍竖排 → 2 箭配方（内容契约变更：早期箭现需 flint + feather = MC 同款闭环，gravel→flint、chicken→feather、树→stick）+ **flint_and_steel**（flint+iron_ingot 合成；`ignites_flamable` = 右键烧 `flamable` 块、无掉落、不消耗）。flint 的两个用途 = MC 的主用途。`[Phase 59]` → `[Phase 60]`

## 实体（Phase 10）（已修复条目）

- ~~**Mob 无行走摆腿/手臂动画**~~ — `[ENT-1]` 已解决：swing 相位 = per-mob 整数（移动帧 +2、静止帧 −4 衰减到 0、帧边界水平位移严格不等式判定、纯渲染层不入存档）+ constexpr 16.16 定点正弦表（编译期 Taylor、零运行期 libm）+ mob 模型 `swings` 数据段（box `name` + `part`/`amplitude` (0,1] 弧度/`group` 0/1 半周相位组，严格解析 + part 存在性交叉校验）+ CPU 逐帧顶点变换（pivot = box 顶面中心、绕局部 X 轴、法线同旋、phase 0 位级恒等 = 静态 fast path 结构性免疫）+ per-mob 动态 VBO（`Mesh::uploadDynamic`/`updateVertices`）+ config `mobs.limb_swing`（默认 true、false = 静态基线）。内容：chicken 2 腿 0.5 rad 半周相位、cow/sheep 4 腿 0.7 rad 对角步态、spider 8 腿 0.9 rad 偶奇分组。**残余（文档化偏离）**：zombie/skeleton 无独立 arm part（99.7 几何 = 头 + 躯干两 box，`[ENT-1]` 不造几何）；翅不摆（MC 只飞行时扇翅）；swing 相位 session-local（读档/生成 = 0，mob 本 ephemeral 不入档）。`[Phase 10]`（腿 `[99.7]` 已补；动画 `[ENT-1]` 已解决）

- ~~**实体与流体无交互**~~ — `[ENT-4]` 已解决（mob/掉落物）：**共享流体浸入模型** = 玩家 FLUID-2 的无输入孪生（`Entity::refreshFluidState` + `Entity::integrateFluid`，同常量 12/1.5/4/32 同名同公式，`isRestingOnSolid` 从 Player 上提共享）：浸没体被动上浮（vy≥0 门 = 沉水体继续下沉的 MC "dive" 语义）、水面 W' 几何漂浮（FLUID-3：W' = 真水面上扫【浸没体窗内最高 L8+1】+ 浮漂终态 = 游泳位 W'−h/2 半浸没【原脚面=水面干身终态 = 用户报障"站在水上"，已修】+ fps 无关不动点）、池底站立（fresh resting 门；FLUID-3 深度门：池深 ≥2 的 resting 体上浮出底，1 格浅池站底不变）；mob 水平速度门 ×0.6（命名常量、无 per-mob 数据字段 = 文档化 YAGNI 偏离）+ step-up 水下解禁（FLUID-3：ENT-4 的 `!m_inFluid` 门删除 = 玩家对齐，涉水 mob 可踏 1 格岸壁出池；游泳位 mob onGround=false 仍无 step-up = 无 swim-up 爬岸记档偏离）；下沉/浮力 inFluid 门控（FLUID-3：水面以上 0.4 干带 keep vy = 全速过带，原无门 = 悬崖坠水"降落伞"已修）；掉落物浮面静止（摩擦零残差 = 无漂移，MC 简化记档）；**熔岩对称同路**（玩家熔岩是弱浮力节奏，mob/物品两类流体同一水模型 = 文档化偏离；P58 燃烧系统零改动 = MobFireTest 熔岩池坐底同速率钉死）。**残余**：投射物/箭仍穿过水（任务条 = mob/掉落物，文档化边界）。`[Phase 10]`（mob/掉落物 `[ENT-4]` 已解决，投射物保留）

## 物品 / 背包（Phase 11）（已修复条目）

- ~~**拾取是固定 1.0 格半径，无磁力**~~ — 玩家底心距掉落物底心 <1.0 且过 0.5s 拾取延迟即整堆入包；Minecraft 会随时间逐渐把物品拉向玩家。`[Phase 11]`（**`[V2-6]` 已解决**（v2.6.0）：拾取延迟过后，水平距离 ≤ kMagnetRadius 2.5 的掉落物被**帧锁定磁力**吸向玩家——位移 = f(距离, 帧) 纯函数（entity/pickup_magnet.hpp，kMobStepPerFrame 同款固定步长纪律）：径向速度 = kMagnetMaxSpeed 6 ×(1−d/2.5)²（远慢近快 = 加速趋近，边缘 0）+ 侧向 swirl（周期 24 帧、幅值 = 径向 25% = 收紧螺旋，MC 拾取手感）；掉落物步进过 hasRoomAt（磁力不穿墙，MC 规则），**插墙箭同款消费**（stuck 箭被拉出方块——无 room 检查：卡住箭 AABB 本就在 solid 内）。**记档偏离**：① 磁力仅水平（y 分量恒 0，落差拾取仍需走近）；② swirl 是简化和弦摆动（MC 是三维螺旋）；③ 拾取半径 1.0 / 延迟 0.5s 不变（磁力只是把物品送进半径）。详见 docs/archive/v2x/phase-2.6.0.md）

## 存档（Phase 13）（已修复条目）

- ~~**写存档时在退出路径重跑生成器做 diff**~~ — `[Phase 13]` 引入（`[Phase 14]` 起规模放大；用户日志 ~100k-chunk 世界退出 13.6 s，`[Phase 33]` 诊断 ~0.14 ms/chunk），`[Phase 37]` 已修复（并行化）：`diffWorld` 在 >256 chunk 时改为**每 chunk 一个 job 跑在临时 4-worker 池**上（小世界保持单线程、线程生成零开销），每 job 写自己的 slice、flush 后合并；输出按位置排序是**全序**（一格至多一个编辑）→ 并行结果与串行**逐字节相同**（E2E A/B 对 P36 串行构建的存档字节一致是核心证据；~9600-chunk 实测退出 diff 1.3 s → 0.35 s，墙钟 Δ0.98 s）。增量编辑跟踪（O(编辑数)）**`[PIPE-2]` 已实现**：World 写收口位置集 journal（observer，含位移陷阱双上报）+ 退出按 journal 触及 chunk 重算 fresh、仅 live≠fresh 输出（**与 diffWorld 字节等价**，进程内 oracle = `--save-full-diff` 旗，legacy 路径保留）；楼梯 O(N) 世界扫描一并转 journal 派生（不变式 = worldgen 永不生成楼梯，真实 assets 单测钉死）；vd80 全环退出 6.2 s → **实测 17 ms**（journal 路径；3600f 场景 legacy 对照 3.28 s ≈ 190×；K = journal 触及 chunk 数，退出成本与环大小脱钩）。99.2 残余①（仅流体模拟改动的远端列卸载回生成态）在新语义下自动一致（卸载驱逐 journal + 退出过滤只查现存 chunk）。细节见 `archive/v1x/phase-37.md` + `archive/v1x/phase-pipe-2.md`。
- ~~**玩家速度/onGround 不存**~~ — 速度半 `[SAVE-2]` 已解决：format 22 `player.velocity` 3 数组恒写（oxygen/stats 键纪律：22 必键、≤21 携 = 版本谎报点名；区间 ±100 `kSavedVelocityMax` 损坏档点名）+ 读档 `restorePosition` 后 `setVelocity`（≤21 载入 = 零速 legacy 值保真免迁；门 141 S2c 孪生跨侧 player 段位级 + PNG 字节双证）；**onGround 半保留不存** = 位置派生（空中读档 onGround 自然 false / 静止读档首子步碰撞置位 = 值保真，记档偏离）。`[Phase 13]`（坠落伤害部分 `[Phase 36]` 已解决；速度 `[SAVE-2]` 已解决）
- ~~**物品严格解析白名单漏 `saturation` = 10 行 init WARN 噪音**~~ — `[v1.34.0]` 已解决：10 个带 `saturation` 的食物 JSON（SURV-9 内容）在 item_registry 的 unknown-field 白名单中缺名 → 每次 init 打 10 行 `Item '<food>': unknown field 'saturation' ignored`（WARN 非 FAIL；字段随后被 SURV-9 消费路径正确读取 = 值有效，纯日志噪音；新旧二进制两侧恒同 = 结构无害）→ 白名单加一行 `saturation`（零行为变更：解析路径不动，仅 WARN 消除；门 149 S2b 实证 = 旧 10 行 / 新 0 行）。`[SAVE-2 门内发现]` → `[v1.34.0]`
- ~~**存档无迁移机制**~~ — 当前 **format 30**（save.hpp `kSaveFormat=30` / `kSaveFormatMin=1`），**format 1..30 全部可加载**（各按其字段集 + 逐 format 白名单，规则式迁移——"未知字段=报错" rejectUnknownFields 保持）。format 演进（15 → 30）：15 `states`（ENT-2，14→15 值保真）/ 16 `player.armor`（SURV-1，恒写占位槽）/ 17 `player.oxygen`（SURV-2，恒写）/ 18 耐久 `damage`（SURV-3，>0 才写）/ 19 `tables`（CRAFT-1，恒写 + 18→19 sole 归属迁移）/ 20 效果 `tier`（SURV-6，omit-when-0 免迁）/ 21 `player.stats`（SURV-7，恒写空对象合法）/ 22 `player.velocity`（SAVE-2，恒写 ≤21 零速免迁）/ 23 `player.tutorial`（UX-7，恒写 {id,count}/{} ≤22 免迁）/ **24 inventory 36 槽（UX-8f，写恒 36；per-format 尺寸检查双向点名【24 须恰 36 / ≤23 须恰 27】+ 23→24 尾部补 9 空槽值保真迁移 `kPreUx8fInventorySlots`）/ 25 `player.tutorial` 加 `series`（V2-3 教程系列化，InProgress 形状 {series,id,count}；25 缺 series 拒 / 23-24 携 series = 版本谎报拒 / 23-24 载入缺省 0 = 值保真免迁；per-format 形状边界用引入 format 字面量 = PITS 361）**/ 26 `player.xp` + entities 第二 kind xp（V2-11 XP）/ 27 `player.xp_seed` + 物品 `enchants`（V2-12 附魔）/ 28 `player.respawn`（V2-13 床）/ **29 gzip 时代（V3-7：schema 与 28 完全一致，format 号仅标记默认 gzip 编码；编码与 format 号正交，魔数 `1f 8b` 探测唯一判据，旧明文档零迁移）**/ **30 下界维度（V3-1：`player.dimension` 严格词表恒写【29 缺 = overworld 迁移 / 29 携 = version lie 点名】+ `nether{blocks,fluids,states}` 恒写 + `nether_entities` 恒写 + 派生 seed 不落盘 + 两维 portal rescan + 交叉检）**。更早的规则式迁移：6→7 炉子（sole 归属/丢弃）、≤11→12 吸收（共享 health 拆分）、11/12→13 营火（单槽→槽 0）、13→14 stairs（迁移为空）。写入恒 format 30。`[Phase 13]`（`[Phase 16/17]` 起 format 2/3 可加载，其后逐 format 追加；UX-8f format 24；V2-11..V3-1 = 26..30）

## Chunk 流式（Phase 14）（已修复条目）

- ~~**只增不卸载** — 流式只加载、从不卸载 chunk：内存随探索面积线性增长（半径 8 列环 ≈ 2304 chunk ≈ 35 MB）。~~ — `[Phase 14]` 问题，**`[99.2]` 已修复**：`unloadFarColumns`（pool flush 后的主线程单写窗口）卸载距玩家 > view_distance+1（Chebyshev，+1 迟滞带防抖）的列；保留列 = 演示 4×4（重生安全）+ 玩家编辑列（m_editedColumns，存档 diff 所在）+ 五类 per-block 状态列（炉子/营火/作物/信标/火，存档交叉校验对象）+ 在途生成列。实测走路 85 s：RSS 平台 470-474 MB（修复前无限增长）。**残余**：① ~~仅流体模拟改动（远端熔岩流动无 fire 条目）的列卸载后重置为生成态~~ — **`[STRM-2]` 已修复（v1.1.0）**：驱逐时按 journal 格限域快照该列 live≠fresh diff（`retainColumnDiff` = `journalDiffChunk` 字节同语义）存入按列保留表（保留 diff 条目非 chunk = 内存有界）；列再流送时 integrate 回放（blocks 先 fluids 后 = applyWorldEdits 序，observer 自动再 journal 化 = 自相似循环，**不插 m_editedColumns** = fluid-only 列保持可卸载）；退出存档两条路径（journal + `--save-full-diff` legacy oracle）`mergeRetained` 合并（不变式：每列至多在 journal 或 retained 之一 = 无重复格）；读档路径零改动（既有顶满列 + m_editedColumns 保护）。覆盖范围 = 该列全部 journal diff（流体 + 燃烧/水熔岩反应的 block——后者经 world.setBlock 绕过 choke point 同样无保护资格）；② ~~环是**正方形**~~ — **`[STRM-1]` 已解决**：环 = 圆盘（dx²+dz² ≤ R²，纯整数谓词）+ **远平面帽** R_eff = min(view_distance, 64)（推导：kFar=1000 块——可绘制列全部 d ≤ 63.91，d≥65 的列最近角 ≥1002 块 = 原理上永不渲染，流送纯浪费）：vd80 = 12853 列 ≈ 103k chunk。实测 RSS A/B（同机同场景流送到 complete）：**方形 9.08 GB → 圆盘 4.59 GB（−49.5%）**（原文"~12 GB"为估算，方形实测 9.08）；原文"球 ~1/9 列数"把 MC 的形状与 MC 的渲染距离量级混为一谈（同 R=80 圆盘 = 9.3 GB；达 ~1/9 需 R_eff=30 = 480 块地平线，用户定案暂不采用、一个常量可切换）。base18 **第 81 次门**（无输入结构性免疫：流送永不 engage）。

## 性能 / 架构（已修复条目）

- ~~**sky rescan clobber（潜伏，`[LGT-1]` 设计时发现）**~~ — `[v2.8.0]`
  已修复：**sky BFS 加 shell seed**（记档两选一推荐形态；union 单遍
  = LGT-1 已砍除形态不复活）——`updateRegion` 天空种子盒从 footprint
  扩为 +1 块 shell 环（rescan 仍 footprint-only、BFS 写界 inRegion
  不变、no-op skip/propagateAll/块光零改动）：shell 格持收敛值 =
  恢复被 rescan 摧毁的边界条件，严格增 BFS 向内推进；无 clobber 时
  位级不变（旧不动点严格增不触发）。**独立性判据零改动**（shell 读
  列 c±2 = 99.10 文档化 1-chunk 边界读环，kLightParallelColumnGap
  与 confluence 证明原样成立，light_job.hpp 零变更）。TDD 双回归
  （stale-dark 2464 格 / stale-partial 544 格 pre-fix 复现，清点与
  设计预测精确吻合）+ CPU 探针（真实资产半板：旧库梯度 14..1 → 全
  0，新库完整保持）+ E2E slab 无输入流送场景（板 x25..63 西区
  x32..38 旧树暗带 vs 新树恢复）。`[LGT-1]` 发现，`[v2.8.0]` 修复。

## UI / 音效 / 粒子（Phase 15）（已修复条目）

- ~~**破坏 cross 块（作物/花/火把）崩溃：粒子纹理查找 `unordered_map::at`**~~ — `[Phase 65]` 发现并修复：`Game::drawParticles` 对所有块的粒子用 `cubeFaceTexture`（top/side/bottom 槽）查纹理——cross 模型（`[Phase 45]` 引入）没有这三项纹理（只有 `cross`）→ 空名 → `AtlasLayout::uvMin` 直接抛 `unordered_map::out_of_range`。`[Phase 45]` 起的潜在 bug（此前无任何 E2E 破坏过 cross 块），由 P65 火把破坏 E2E 首次暴露（gdb 回溯定位 `drawParticles → uvMin`）。修复 = `drawParticles` 加 cross 分支：粒子立方体六面全用 `cross` 纹理（与 item_mesh / chunk_mesher 既有的两个 cross 分支同款单一查找）；E2E S4（火把破坏 → 粒子帧渲染 → 无崩溃 + ×2 savenorm）为回归证据。`[Phase 65]`
- ~~**物品名 tooltip 只有名字**~~ — `[PRE-12]` 已解决：悬停格（炉子/E 面板槽与格）显示 `name`（count 1）/ `name xN`（count > 1）+ 工具第二行 `damage/max` 耐久读法（纯函数 `ui/item_tooltip`，campfire 范式 5 单测；**tooltip 渲染层修复**：旧文本与面板共享 kText batch = 绘制先于槽底/图标 + 偏移随面板 fit scale 收缩 = Phase-15 起悬停 tooltip 恒被覆盖不可见，E2E 首触发实证；修 = tooltip 独立 batch 移至 3D 块立方图标后绘制）。残余 = 无描述文本（item JSON 无 description 字段 = 记档偏离，MC 同款无描述）；~~附魔文本~~ **`[V2-12]` 已解决（v2.13.0）**：附魔物品的 tooltip 可展示附魔名（面板 offer 行同款字集）。`[Phase 15]` → `[PRE-12]` → `[V2-12]`
- ~~**"You died!" 死亡屏幕文字实际是白色（颜色参数被忽略）**~~ — `[Phase 33]` 发现，`[Phase 36]` 已修复：`ui.frag` textured 分支改 `texel.rgb × u_color.rgb`（所有 textured 绘制的 u_color.rgb 均为白，白字/图标/心带逐位不变，只死亡屏红字生效）+ E2E 重像素验证（红字 bbox A (488,320,282,24)/B 恰好 2×，hint 白字 bbox 不变）——细节见 `archive/v1x/phase-36.md`。

## 生存（Phase 16）（已修复条目）

- ~~**存档加载到空中位置时坠落伤害被绕过**~~ — `[Phase 33]` 发现（P33 E2E 探针实证：46–59 格自由落体满血落地），`[Phase 36]` 已修复：`Player::restorePosition`（= `teleportTo` + `fallStartY = pos.y` 重钉），`applySave` 改走该路径 + 单测 ×2（空中 10 格 → 精确 7 HP；地面读档零伤害）+ E2E（存档 pos y=62 加载 → 8 格坠落 → health 15 精确）——细节见 `archive/v1x/phase-36.md`。

## 饥饿 / 食物（Phase 23，Phase 28，Phase 31）（已修复条目）

- ~~**饥饿 HUD 在极窄窗口会被裁剪**~~ — `[PRE-11]` 已解决：**心行窄窗回退布局**（纯函数 `core/ui_layout.heartRowLayout`，campfire 范式 8 单测钉死）：整行超出右缘时把行左移直至**右排（饥饿/氧气 = 更紧急读数）右缘贴屏**——吸收带（行最左）最先被裁（记档 last resort），低于 hearts+gap+right 最小行宽时心本身也裁（无布局能救 sub-row 窗口，记档）；**宽窗返回 legacy 坐标位级精确**（无输入基线 + E2E 窗零触碰 = 结构免疫根）；右排宽度按本帧实际渲染的排预留（氧气替换饥饿时 10 图标，SURV-2 原位语义）；顺带 UI 度量常量（16/2/6）收口 ui_layout.hpp 单一真源（drawUI 别名）。残余 = sub-row（< 362 虚拟 px）窗口心也裁 = 记档极限。`[Phase 23]` → `[PRE-11]`

## 工作台 / 形状合成（Phase 17）（已修复条目）

- ~~**全工作台的网格是同一个容器**~~ — Phase 17 的 v1：`Game` 持有单个 `m_tableGrid`（`CraftingGrid`）：右键打开的**任何**工作台的 3×3 网格都是它（存档 format 3 `table_grid`）。`[Phase 17]`，**`[CRAFT-1]` 已修复**：每工作台方块独立容器（`Game` 持 `std::map<BlockPos, CraftingGrid> m_tables`，位置序 → 存档写序确定；`setBlock` 收口创建/销毁 + 读档 parse 时不变量保证完整性【炉子同款，无 rescan】）+ 右键面板绑定所点方块（`InventoryUI::bind`，`FurnaceUI::bind` 范式——面板 UI 零视觉变化，只换数据源）+ 存档 format 19 `tables`（每工作台块一条目 {pos, slots[9]}、位置序、与方块差集**双向**交叉校验、19 必需空数组合法 / 19 携 `table_grid` 拒 / ≤18 携 `tables` 拒；**18→19 迁移 = sole 归属**：旧全局 `table_grid` 非空且世界恰 1 个工作台 → 归属之，0 或多个 → 丢弃【P44 先例】；`inv_grid`（E 面板）不动）。**记档偏离：破坏工作台丢弃容器内物品**（setBlock 收口删 state，只掉工作台自身 `drops` = 炉子破坏同款语义 = MC 规则）；**格式 19 槽不携 `damage`**（合成网格属 SURV-3 三处范围外的 grid 纪律，受损物品入格读档回满 = 同款 quirk）。`[CRAFT-1]`

## 炉子 / 熔炼（Phase 40）（已修复条目）

- ~~**炉子无获取途径（内容缺口）**~~ — `[v1.34.0]` 已解决：shipped 全部
  65 条配方零 furnace 引用 + 初始库存（热栏 9 件 + 隐藏 starter 槽 16-26）
  无 furnace = 正常游玩拿不到炉子（10-30 GAMEPLAY.md 文档刷新时发现）→
  **shaped 配方 `8 cobblestone 围圈 → 1 furnace`**（MC 1.14+ 摆法，纯
  内容零核心代码；锻石 = Phase 61 挖石头掉落 = 石头→锻石→炉子闭环；
  内容钉死单测含真实匹配器正/负例）。隐藏 starter 槽不给（配方即获取
  途径，MC 同款进度）。`[Phase 40]` → `[v1.34.0]`
- ~~**炉子状态是单个全局容器**~~ — Phase 40 的 v1：`Game` 持有单个
  `FurnaceState`（input/fuel/output + 两个整数子步计数器），右键打开的**任何**
  炉子看到的是同一份熔炼状态（存档 format 6 `furnace`）。`[Phase 40]`，
  `[Phase 44]` 已修复：每炉子方块独立状态（`Game` 持
  `std::map<BlockPos, FurnaceState>`，位置序 → 逐帧 tick 确定；`setBlock`
  收口创建/销毁）+ 右键面板绑定所点方块（`FurnaceUI::bind`）+ 存档
  format 7 `furnaces`（每炉子块一条目、位置排序、与方块差集**双向**交叉
  校验）。format 6 加载**按规则迁移**：世界恰有 1 个炉子方块 → 全局状态
  归属它；0 个或多个 → 归属不可知，**丢弃**（所有炉子状态空载）——迁移在
  纯解析器（save.cpp）内完成，单测覆盖三分支。
- ~~**炉子外观四个侧面共用同一纹理**~~ — `[v2.7.0]` 已解决：**块级
  per-face 纹理通用能力**（`BlockModel.textures` 可选 `faces` 段，
  front/back/left/right 由**面外法线**定名——front=(0,0,−1) 共享
  yaw-0=−Z 约定；严格解析未知键/非串/空串/形状不符全点名；
  cross/fence/glass_pane 自持几何点名拒；**无 faces = 位级不变
  红线**（空串回退 side，除 furnace 外全块方向查找 = 旧查找单测
  证明））首个消费 = 炉子前脸单 tile `furnace_front`（宽炉口+余火）；
  **lit 态不加纹理 = 记档偏离**（lit 表现 = 既有炉口火焰粒子）。
  顶/底面仍石质（MC 同款）。`[Phase 40]` → `[v2.7.0]`

## 染色（Phase 41）（已修复条目）

- ~~**染料只有部分颜色有 MC 同款来源**~~ — Phase 41 的 v1：白名单只有
  9 色（白/黑/棕/红/黄/灰/橙/粉/银），无蓝/绿/紫；补色需扩白名单 + 全
  dye 家族变体块 + 纹理。`[Phase 41]`，`[Phase 45]` 已修复：补全缺的
  6 色（蓝/绿/紫/品红/浅蓝/青，项目 silver 位 = MC light_gray → 15 染料
  全），**颜色词汇表纯数据**（dye item 的 `dye_color` 命名，加颜色零代码
  改动）；MC 同款来源全部进世界：`lapis_ore` 矿脉（蓝←lapis）、`cactus`
  实心块（绿←cactus）、`allium`/`tulip`/`cornflower` 十字植物（紫/品红/
  浅蓝）、青←lapis+绿（MC 1.14+ 配方）；来源块走 biome `decorations`
  表面装饰（沙漠仙人掌、草原/森林花）+ `ores.veins` 矿脉（lapis）。
- ~~**sheep 只掉白 wool** — 无染色 sheep（MC 有 16 色羊）；彩色 wool 只能靠
  染色获得。~~ **`[V2-15]` 已解决（v2.16.0）**：sheep 出生毛色 per-mob
  流首抽（temperate 表 850:80:30:30:10 白主稀有色）+ shears 剪毛掉
  1-3 对应色 wool + RMB 染料改色（bare 拒染）+ 死亡彩毛钩子 + 7200f
  再生；词表 15 色无 lime = 记档偏离（MC 16 含 lime）。`[Phase 41]`
  → `[V2-15]`

## 牛奶桶 / 饮用（Phase 43）（已修复条目）

- ~~**状态效果系统不存在：喝奶效果集 = 清红闪一项**~~ — `[Phase 46]` 已修复：
  数据驱动状态效果系统（poison/weakness/regeneration，见下「状态效果」
  章节）；喝奶完成 = 清红闪 + **清除全部负面效果**（MC 语义，正面再生
  保留）。坠落伤害起点（`fallStartY`）仍**刻意不清**——MC 坠落伤害不是
  状态效果，空中喝奶不减坠落伤害。`[Phase 43]`

## Mob（Phase 20）（已修复条目）

- ~~**无 Peaceful 难度**~~ — `[ENT-8]` 已解决：`mobs.difficulty = "peaceful"` 四值词汇表（peaceful = Phase-25 保护环扩到全球 kSpawnProtectionPeaceful 1e6 → 敌对自然生成全拒，被动豁免照生，MC 和平核心语义）；"现有敌对消亡" 退化 = mobs ephemeral + difficulty 只 init 读取（无运行期切换）→ 读档后本无敌对（偏离记档，详见 phase-ent8.md）。`[Phase 25]`
- ~~**生成率缩放为全局节奏**~~ — 难度缩放的是尝试节奏（全部 mob 类型共享的 `check_every_frames`），不是 Minecraft 的按类型生成权重；easy/normal/hard = 30÷0.5/1.0/1.5 帧。`[Phase 25]` 发现，`[Phase 39]` 已修复：`mob_spawning.json` 的 `mobs` 条目改为 `{"id", "weight"?}` 对象（weight 1..10000，缺省 1；旧纯字符串条目显式拒绝 = 迁移防护；重复 id/越界/非整数 = 加载失败），类型抽取改为「每次 1 次流值映射进 [0, Σweight) 后按累计权重走名单序」——全 weight 1 与旧 `pick % 名单长度` 逐位相同（已发行数据零行为变化）。难度节奏缩放保留（与权重正交：节奏 = 多久尝试，权重 = 尝试时抽中哪种）。
- ~~**mob 无 step-up**~~ — 1 格台阶会挡住追逐（贴墙滑动、可能永远到不了玩家）；玩家有 P16 step-up，mob 没有。`[Phase 20]`（追逐卡台阶）**`[ENT-3]` 已修复**：step-up 纯几何上提为 `Entity::stepUp`（player 逐位不变 = 既有 stepUp 单测零改动全过），mob 在水平移动受阻的同一位置、同一判定（P99 TRUE-1-block：solid 区顶高 ≤ 1.0 → slab/满块/玻璃板可踏、墙/栅栏 1.5 帽不可踏）调用同款抬升 + 本 tick 水平位移重放；AI/速度/子步节奏零改动、零 format（mob ephemeral）。**共享判定顺带钉死 P16 两个潜伏 quirk**（player 同款、ENT-3 探针实证）——**`[v2.9.0]` 双双已修**（详见「生存」step-up 条：front 格对称化 + 真 clamp 无状态验；player/mob 共享生效）。
- ~~**mob 无游泳/水交互**~~ — `[ENT-4]` 已解决：走进水 = 共享流体模型（浮力 12 / 上浮帽 1.5 / 下沉帽 4 = 玩家 FLUID-2 同常量）——沉水被动上浮、水面 W' 几何漂浮（fps 无关终态）、池底站立、水平速度 ×0.6 门控、水下禁 step-up（**FLUID-3 修订**：`!m_inFluid` 门已删 = 涉水 mob 可踏 1 格岸壁出池；浮漂终态 = 游泳位 W'−h/2 半浸没；池深 ≥2 的 resting 体上浮；详见 phase-fluid3.md）；AI 状态机零改动（追逐/游走照常驱动，只是水下更慢）；熔岩同路（P58 燃烧照旧）。生成检查（地面格 + 头顶空余）不变。`[Phase 20]`
- ~~**mob 无坠落伤害**~~ — `[ENT-5]` 已解决：MobEntity 落地块镜像玩家 Phase-16 规则（净落差 > 3 且落地格 fresh 无流体 → damage，详见 phase-ent5.md）。`[Phase 20]`
- ~~**掉落数量是固定列表，无随机区间**~~ — Phase 28 起掉落数据 = `drops` 列表
  （每条目 `{item, count}`，count 1..64 固定）；`[Phase 38]` 已加随机区间：条目可用
  `count_min`/`count_max`（0..64、min ≤ max、必须成对、不得与 `count` 混用），死亡时按列表顺序
  用该 mob 的种子流掷一次（0 = 该条目不掉）；skeleton 现掉 `bone` 0–2 + `arrow`，cow 仍固定 1
  `leather` + 1 `beef`（数据选择，非 schema 缺口）；无掉落幸运/附魔影响；`leather` 目前无
  合成用途（纯收集/交换物）。`[Phase 22/28]`（随机区间 `[Phase 38]`）
- ~~**skeleton 无视线检查**~~ — `[Phase 38]` 已修复：射击回调先过 `hasLineOfSight`（纯 raycast 函数：眼→玩家眼被非空气格挡住 = blocked），blocked 则不发（箭反正只会插墙消失），冷却照常消耗、下个间隔重试（Minecraft 行为）+ debug log。E2E：存档手工 96 格石墙（两 skeleton与玩家之间）→ blocked=6 / shot=0 / health 20 / 两 skeleton 照常生成。
- ~~**skeleton 掉落固定 1 个 `arrow`**~~ — `[Phase 38]` 已修复：新 material 物品 `bone`（图标 = 白色对角骨，generate_item_icons.py 新 salt 63，既有 17 图标零变化）+ skeleton `drops` = bone 0–2（随机区间）+ `arrow` 1。掷落用该 mob 自己的种子流（`rollDropCount`；固定 count 不推进流 → P38 前的精确流保留）；E2E 输入击杀 → 存档 entities = arrow×1 + bone×2。（Minecraft skeleton 还掉 0–2 箭；本项目箭保持固定 1。）
- ~~**skeleton 箭提前量只补偿重力、不预判位移**~~ — `[Phase 38]` 已修复：`computeAimDirection`（projectile.hpp 自由函数）——“目标位置 + 速度×飞行时间”迭代 2 次 + 0.5·g·t² 弧线提前量；静止目标（速度 0）与 Phase 27 瞄法逐位相同（回归锚：E2E 射击场景 P37/P38 存档逐字节一致），移动目标被预判（单测：侧向/逃离目标最近接近距离严格小于无预判）——细节见 `archive/v1x/phase-38.md`。

- ~~**spider 无掉落**~~ — `[v2.7.0]` 已解决：新 material 物品
  `string`（螺旋绳）/ `spider_eye`（紫青眼球）+ spider `drops` =
  string 0–2 + spider_eye 0–2（既有 count_min/max 随机区间机制，
  MC 同款值；掷落走该 mob 自己的种子流 = Phase 38 纪律）。
  **两物品纯收集 = 记档偏离**（弓配方用 planks 不用 string，MC 弓
  要 string 的路线不演进 = 最小面定案）。spider 仍是首个
  `on_hit_effect` mob（poison 15 s，近战命中落定施加，见「状态
  效果」）。`[Phase 46]` → `[v2.7.0]`

## 弓 / 箭（Phase 21 + Phase 24 + Phase 42）（已修复条目）

- ~~**箭无箭头 / 无尾羽**~~ — `[PRE-9]` 已解决：**MC 式三段箭形**（棕色细杆盒 0.31 + 灰色四棱锥箭头 0.12 + 白色宽尾羽盒 0.09，三段总长 0.55 = 原轮廓尺寸不变；`arrow_mesh` 纯函数 + `drawArrows` 每箭三色三次纯色 draw，零 shader/atlas 改动）。残余 = **无 atlas 箭纹理**（三色纯几何近似 MC 箭色，纹理化需新 PNG + UV = 记档偏离）。`[Phase 21]` → `[PRE-9]`
- ~~**插墙箭寿命 = 30s**~~ — 与飞行箭共用 `kMaxAge`（Minecraft 插墙箭 100s）；插住的方块被破坏/替换时箭留在原处（不重新碰撞、不掉落）。`[Phase 24]`，`[Phase 42]` 已修复：插墙箭独立寿命 `kStuckMaxAge = 100s`（飞行箭仍 30s；age 累计、按**当前**状态取上限——插墙 >30s 后释放的箭立即消亡，MC 分歧已文档化）+ `Game::setBlock` 收口（破坏/放置/染色换块都走它）释放卡住格内的插墙箭（碰撞盒**触面**判定：盒触及面且中心在面外，非重叠判定——resolver 精确 clamp 到面上，盒不进入块内）。
- ~~**拉弓 = 2D 手持图标位移**~~ — 拉弓时右下角手持图标按蓄力威力向屏幕中心拉（满蓄力 64px）；无 3D 手部/弓弦模型、无拉弓进度条（Minecraft 有手部动画 + 弓弦后拉）。`[Phase 24]`，`[Phase 42]` 已修复：3D 手持手部（`drawHeldHand`）——指针锁定且无面板时选中的物品渲染为视口锚定的 3D 模型（块物品 = 掉落物同款 3D 方块体 + atlas 光照；图标物品 = 2D 图标在 3D 平面，恒亮），拉弓时手部随蓄力后拉 + 弓体后仰（拉弓指示），进食时随进度抬向面部（进食指示）；2D 手持图标改固定（P24/28 的 2D 拉移删除）。仅锁定时渲染 → 无输入基线结构性免疫（无重锚）。无弓弦模型/拉弓进度条（仍无）。

## 状态效果（Phase 46）（已修复条目）

- **~~weakness 无游戏内来源~~** — `[Phase 46]` 已修复 `[SURV-4]`（v0.16.0，详见 docs/archive/v1x/phase-surv4.md）：6 种单效果药水
  （weakness 90 / speed 180 / regeneration 45 / poison 45 / healing 0 / harming 0 =
  MC 时长；glass_bottle 桶对舀水 + `instant_hp` 瞬时原语；starter 隐藏槽 17-23
  发放）。regeneration = 金苹果（`[Phase 49]`）+ 药水双来源。mob `on_hit_effect`
  （spider = poison）与存档注入保留。**范围外（记档偏离）**：无酿造台/酿造线；
  药水档位无游戏内消费面（档位机制 = `[SURV-6]`）。`[Phase 46]`（`[SURV-4]` 已解决）
- ~~**中毒无独立紫色 tint**~~ — `[PRE-7]` 已解决：全屏效果 tint 通道
  （效果 JSON 可选 `tint` [r,g,b,a] 全 [0,1] 严格解析，缺省全零 = 无纱
  零绘制；正 alpha = no-op 规则的"表现层动作"）+ `poison.json`
  [0.63,0.24,0.86,0.35] = MC 中毒紫（HUD pass 红纱前每活跃 tinted 效果
  一个全屏 quad，`activeEffectTints` 纯函数收集）；毒 tick 红纱 + "hurt"
  音保留不变（纱在红纱之下 = 分层）；**恒定 alpha**（MC 中毒纱脉动 =
  记档偏离）；fire 效果无 tint（玩家燃烧表现 = 既有 0.2 s tick 红纱，
  mob 火焰视觉 = PRE-8 独立项）。`[Phase 46]`（`[PRE-7]` 已解决）
- ~~**吸收层与正常心共用一个 health 值**~~ — `[Phase 72]` 已解决：**两分量模型**——
  `m_health`（0..base max）+ `m_absorption`（0..64）**独立池**；`damage()`
  **先扣池**（池单独不能致死，死亡只来自 health）；`heal()`/`setHealth()`
  只钳制到 base max（heal 永不填池）；池 = 效果 `absorb_hp` 总和的纯函数
  **只在同步点**（进食施加 / mob 命中 / 到期 / 牛奶清 / 死亡清——刷新恢复
  满层 = MC 属性重算），同步点之间伤害扣减的**持久**（扣空后不再自动回满）；
  读档存值权威（不重同步——池可低于效果总和）；HUD 吸收红心从**池**派生
  （池 0 = 零绘制，无输入结构性免疫保持）；**format 12** `player.absorption`
  （键纪律 + [0,64] + ≤ 效果总和不变量 + format ≤11 共享 health 迁移）。
  剩余边界见下条。`[Phase 56]` → `[Phase 72]`
- ~~**效果图标行与吸收红心可视觉重叠**~~ — `[PRE-10]` 已解决：**MC 三行 HUD 堆栈**（心行 hy / 效果行 hy−22 / 护甲行 hy−44，行距 kIconPx+kStripGap 22，每行独占 = 任意图标数零碰撞）：效果图标从左锚改 = 心行最左缘（池在时吸收带左缘，MC 布局）+ 护甲行上移一行；纯布局常量零 format/config。门 146 像素断言实证：OLD 第 5 图标叠吸收带 45 px footprint vs NEW 带内 diff 0 + 带外世界 0 diff。残余（档位显示）= `[PRE-12]` 已解决：F3 效果行带档位罗马（`effectTierRoman` 纯函数，tier 0 = "I" MC amplifier 惯例）。**记档偏离**：效果图标 hover tooltip = 结构不可行（效果行仅 locked 时绘制，locked 下物理光标固定屏幕中心 = 悬停恒 false；MC 同款信息经 F3 通道呈现）。`[Phase 56]` → `[PRE-10]` → `[PRE-12]`

## 流体桶（Phase 47，Phase 51 双种类更新）（已修复条目）

- **~~只有水/熔岩两种流体~~** — `[Phase 47]`（Phase 51 起词汇表 = `water` +
  `lava`）已解决 `[FLUID-6]`（v1.11.0，详见 docs/archive/v1x/phase-fluid6.md）：第三种类 = **honey**（17..24 编码，粘度 = 帧锁定
  tick 周期 3，无 water_reaction 字段 = 反应 pass 数据驱动惰性，蜜/水/熔岩 6-邻
  共存互不替换，`bucket collects +honey` = 一桶全舀三种类）。**残余**：词汇表在
  代码（FluidKind），`slime` 等其它名仍被解析拒绝，加新种类需改编码区间；桶对与
  反应表本身是数据。

## 熔岩 / 流体双种类（Phase 51）（已修复条目）

- ~~**worldgen 不生成熔岩**~~ — `[Phase 86/88/90/93]` 已解决：worldgen 现生成熔岩湖——shipped `lava_lakes` 3 湖（(40,40) R4 深湖 / (55,45) R3 深湖 / (24,26) R1 表面湖）+ `lava_lakes_procedural` 5 个 seed 派生随机湖（region [60,90]²），详见「熔岩湖（Phase 86）」节。`[Phase 51]`
- ~~**熔岩无燃烧/减速**~~ — `[Phase 54]` 已修复：玩家 AABB 重叠熔岩格时
  进**同款游泳状态**（`isInLava`，水/熔岩并存时熔岩优先）+ 熔岩节奏
  （`lava_swim_speed` 1.9 / 上浮 1.5 / 重力 4 / 终端 2，config 可覆盖）+
  **灼烧伤害**（每 `lava_damage_interval_seconds` 1.0 s 真实 dt 受
  `lava_damage` 4 HP，Minecraft 4 HP/s，走正常伤害路径）+ 落入熔岩取消
  坠落伤害 + 熔岩内抑制疾跑/下蹲速度。Mob 在任一流体中不生成
  （P20 行为不变）。`[Phase 51]`（燃烧/减速 `[Phase 54]` 已解决）
- ~~**脱离熔岩无残留燃烧**~~ — `[Phase 57]` 已解决：`fire` 效果
  （1 HP / 0.2 s、可致死）——在熔岩内**每帧重施加**（duration 重置 =
  熔岩内持续燃烧、tick 相位保留；离熔岩后 countdown `fire_duration_seconds`
  8 s 继续烧至到期），水内**瞬时熄灭**（`extinguished_by_water`；水胜
  熔岩：两流体并存时无火），牛奶不灭火（`negative: false`，MC 语义）。
  表现层简化见"燃烧无独立音效/火焰 tint"条。`[Phase 54]`
  （`[Phase 57]` 已解决）
- ~~**熔岩不烧 Mob 与作物**~~ — `[Phase 58]` 已解决：**Mob 点燃**
  （生成时 `MobEntity::armFire` 从 ignites_on_lava 效果 + config
  `fire_duration_seconds` 取参武装；**整数帧燃烧格**：1 HP / 0.2 s、
  熔岩内每帧重置 duration【相位保留】、离熔岩烧 8 s、水瞬时熄灭且胜
  熔岩）+ **`flamable` 块属性**（wheat_1..7 全 7 阶）+ **FluidSim 燃烧
  pass**（熔岩格 6-邻 flammable 块烧成 air，无掉落，快照确定性）。
  Mob 无火焰视觉等 v1 限制见下三条新增条目。`[Phase 54]`
  （`[Phase 58]` 已解决）
- ~~**Mob 燃烧无火焰视觉**~~ — `[PRE-8]` 已解决：燃烧 mob 的余烬火焰
  （`Game::updateMobFlames` 每 `kMobFlameEveryNFrames` 36 帧、每燃烧
  mob 一批上升余烬 = P66 `spawnFire` 复用，专用盐 `kMobFlameRollSalt`；
  **自发光** = block 光 1.0 + 中心格天光环境项，深夜也亮 = MC 观感；
  中心格 = `entityCenterCell` 纯函数【AABB 中心 position+height/2 逐轴
  floor，从 PRE-6 `itemDropLight` 提取共享】；`render.mob_flames` getBool
  代码默认 true，off = 零发射 = 对 v1.28.0 位级恒等 = 结构免疫根）。
  与 CRAFT-5 炉口余烬的记档差异 = 炉口火探出（环境光）/ mob 火裹身
  （自发光）。**残余**：无红闪/无音效/无 HP 条（mob 燃烧仍无独立 HUD
  反馈，玩家燃烧红闪不变）；余烬 = 小型上升粒子（2 粒/批，与 P66 同款
  密度）。`[Phase 58]`（`[PRE-8]` 已解决）
- ~~**v1 flammable = 小麦 7 阶**~~ — `[Phase 63]` 已扩展：`flamable`
  集 = **wheat_1..7 + wood/leaves/grass**（MC 天然可燃三块加入，纯
  数据 = 两消费者——点燃器 `[Phase 60]` 与熔岩燃烧 pass `[Phase 58]`
  ——立即生效；fresh 世界零熔岩 = 无输入下仍惰性）。仍不燃的 MC 可燃
  物：**羊毛**（文档化偏离：染色产物保留不可燃，见
  Phase63FlamableContent 的精确集断言）、高草/火把（未实现）。
  `[Phase 58]` → `[Phase 63]`
- ~~**湿润判定把熔岩也算"水"**~~ — `[Phase 54]` 已修复：`cropIsWet` 改
  `getFluidOfKind(..., Water)`——只算**水**（熔岩旁的耕地 = 干节奏；
  MC 熔岩不湿润，且会烧掉作物——烧 `[Phase 58]` 已解决，见"熔岩 /
  流体双种类"节）。
  `[Phase 51]`（`[Phase 54]` 已解决）

## 台阶 slab（Phase 85）（已修复条目）

- **~~slab = 仅下半块~~** — `[Phase 85]` 已解决 `[V2-17]`（v2.18.0）：
  stone_slab state 化（state_count 3）= 0 底部（既有位级）/ 1 顶部
  （y 0.5..1 天花板贴放）/ 2 双 slab（满格，破坏掉 2 = MC 材质守恒，
  `drops_count_by_state` [1,1,2]）；放置 = 瞄准规则（顶面 → 0 / 底面 → 1 /
  侧面 local-y > 0.5 → 1 否则 0，`slabStateFromHit`）+ 同格同 id 互补 state
  合并 → 双 slab；详见 docs/archive/v2x/phase-2.18.0.md。**4 条记档偏离**：
  ① 双 slab = 单格 state 2 合并表示（一 cell 一 id 红线下的唯一表示——MC
  「同格上下两块共存」语义 = 状态合并，破坏掉 2 = 材质守恒）② 跨型堆叠
  合并不做（同 id 才合并，MC 1.14+ 跨 slab 型合并 = 范围外）③ 顶 slab 格内
  站立 = AABB 重叠全挡（项目无 sneak 缩高 = 既有 KNOWN_ISSUES，MC sneak
  1.5 高可钻顶 slab 下不可用）④ 蹲+右键旋转 Slab 形排除（state = 垂直位
  非朝向，环转 2→0 销毁半块材质不守恒；台阶朝向环不受影响）。
  `[Phase 85]` `[V2-17]`
- **~~slab 光照按满块不透明~~** — `[Phase 85]` 已修复 `[LGT-5]`（v1.5.0，详见 docs/archive/v1x/phase-lgt5.md）：`light_opacity: 0`
  （slab/stairs×4/wall/fence 7 块光如空气穿过 = MC 半块透光规则，统一进入格成本 +1
  零核心代码；`opaque` 保持 true = 剔面/渲染零变更）。

## 墙 wall（Phase 95）（已修复条目）

- **~~墙光照按满块不透明~~** — `[Phase 95]` 已修复 `[LGT-5]`（v1.5.0，详见 docs/archive/v1x/phase-lgt5.md）：墙（与 slab/stairs/栅栏
  同款）`light_opacity: 0` = 光如空气穿过，同 slab 条。

## 树生长（GEN-2）（已修复条目）

- ~~**cz ≥ cx+1 半区整块不种树/不放装饰（wz 循环上界 bug）**~~ —
  placeTrees/placeDecorations 内层 `for (wz = wz0; wz < limit; ...)` 的
  `limit` 误用 X 基（`ox + kChunkSize [+ margin]`）：chunk 的 oz ≥
  ox+19（树）/ ox+16（装饰）时本 chunk 的 z 列不被迭代 → trunk 单列只由
  本列 chunk 写入 = **约世界一半（cz ≥ cx+1 对角半区）永久无树无花**；
  装饰在 cz = cx+1 层同样整块缺失。Phase 4（ae19893）起潜伏，101 道门
  未发现 = 基线视图朝 -Z（该侧 chunk 恒在种树侧）+ 生成器单测只断言
  局部区域。GEN-5 shipped 扫描（320×320 树生长侧盒）首次量化：修复侧
  346 树列正常。**`[GEN-6]` 已修复（v0.20.0）**：wz 上界改 Z 基
  `zlimit = oz + kChunkSize [+ margin]`（2 处一行级，wx/margin/格网/
  roll/GEN-5 守卫零改动）+ 2 判别性单测（pre-fix FAIL/post-fix PASS）；
  世界生成演进量化（shipped seed 123456789 真实资产探针）：门场景盒树列
  13→33（2.54×）/装饰 5→20、演示盒 24→30/+2、前瞻半区锥内 215 列
  （游戏内视野，非 60f fresh 视图）；**base18 免重锚**（第 103 次结构性
  免疫：区内 gained 5 特征全在视锥外 = 探针论证 + 像素终裁互证）+ 性能
  A/B v0.19.0 成本可忽略（细节见 `archive/v1x/phase-gen6.md`）。`[GEN-5]`
  发现，`[GEN-6]` 解决。

## 数据 / 内容（已修复条目）

- **~~方块 per-instance 状态 = 仅楼梯朝向一种~~** — `[Phase 2]` BlockState 已泛化
  `[ENT-2]`（v0.3.0，详见 docs/archive/v1x/phase-ent2.md）：chunk `m_blockState`
  每格 byte（哨兵 255 = 未设置，缺省 = 无 stateful 块位级 legacy）+ 双门控
  （`Shape::Stairs` 隐式 count 4 ∪ 任意形状 `model.state_count` [1,255] +
  `state_default` 严格配对）+ `World::get/setBlockState` + setBlock 收口/潜行
  右键 (s+1)%count/placeBlock yaw 覆写三写入方 + 碰撞/mesh 哨兵回落三消费方同
  规则 + 存档 format 15 `states` 段（全量含默认值、双向交叉校验、14→15 值保真
  迁移 + journal 派生）+ init 不变量（worldgen 5 类放置源禁 stateful = 点名拒）；
  消费面 = 楼梯朝向（`[P94]`，详见「楼梯 stairs」节）+ **3 sapling 生长 stage
  0-7**（`[GEN-2]` v0.4.0，详见「树生长」节）。**仍未引入**：附属/流体等级等
  （需要时再设计 = YAGNI）；炉子/营火/工作台/火 4 个 per-position map（format
  7/13/19/10）+ m_beacons 位置集（功率等级 `world/beacon` 纯函数每帧从基座重
  算）未迁移 BlockState（`[ENT-2]` YAGNI 定案）。
