# 已知问题清单（Known Issues）

每个阶段完成后把新发现的限制/简化记到这里；修复后移入「已修复（归档）」。
条目格式：**问题** — 影响与现状。`[阶段]` 标注发现/引入阶段，`计划` 标注预计修复阶段。

## 渲染

- **叶子挨叶子的内部面仍会生成** — 剔除规则 v1 只判断"邻居是否 opaque"，结果正确但多发了面。水-水的内部面已在 Phase 9 由流体网格化剔除（相邻水位相同/更高的水面不出面）；叶子-叶子待细化为"邻居是否为同种方块"。`[Phase 3]`（水部分 `[Phase 9]` 已解决）
- **玻璃/叶子不衰减光** — 光的遮挡对非水方块仍是二值的（opaque=15，其余=0）：Minecraft 中玻璃/叶子有衰减（叶子有 fancy 选项）。水的每格衰减（天空光 -3）已在 Phase 9 实现；玻璃/叶子要支持需给 `BlockProperties` 加 0-15 的 opacity 字段并改传播算法（衰减系数）。`[Phase 7]`（水部分 `[Phase 9]` 已解决）
- **水面是 level/8 缩放的立方体顶面，无波纹动画** — 流动水（level 1-7）的顶面高度按等级线性缩放，静态观感；Minecraft 有水面波纹动画。`[Phase 9]`
- **按面采样光照，无逐顶点插值/环境光遮蔽** — 每个面采样面后方一格的光值，4 个顶点同值（块状光照感，Minecraft 经典观感）；无 AO 阴影。`[Phase 7]`
- **昼夜是呈现层缩放，不是夜间重算光照** — 天空光恒按正午存储（0-15），渲染时 sky × dayFactor（夜晚=0）。Minecraft 会在夜间真正重传播光照（例如日落时光线从洞口退出的动态变化）；本实现夜晚的天空光区域是"均匀变暗"，无动态传播变化。方块光（glowstone）不受昼夜影响（与 MC 一致）。`[Phase 8]`
- **昼夜时间按每帧固定 1/60s 推进（非墙钟）** — 保证无输入运行（截图验证）逐字节可复现；代价是非 60Hz 显示器上昼夜的真实时间速度按比例偏差（120Hz = 2× 速）。`[Phase 8]`
- **太阳/月亮/星星是 billboard 圆形 sprite** — 太阳 40px / 月亮 28px / 星星 2px 的像素级圆形 quad（着色器 view-space 膨胀；不用点精灵——Mesa ANV 等驱动会把 shader `gl_PointSize` 钳到 1）。无纹理、无月相、无星星闪烁；星星固定在以世界原点为心的 r=800 上半球（走路时有极小视差），无银河。`[Phase 8]`
- **无云、无昼夜音效** — 云层与昼夜环境音未实现。`[Phase 8]`
- **光不传播进未生成区域** — 演示区之外的缺失 chunk 读作"开放虚空"（sky=15 / block=0）作为传播边界源；生成新区后触发全量重传播，边缘光缝在生成前可见。`[Phase 7]` 随 chunk streaming（Phase 14）处理。
- **光照更新全在游戏线程** — 首次全量传播（~几十 ms）在新 chunk 生成时也会重跑；每次破坏/放置做局部重传播（±16 整列天空光 + ±16 盒方块光，~几 ms）。演示区规模下无感，世界变大后随 Phase 14 移入 job queue。`[Phase 7]`
- **顶面/底面纹理朝向约定** — Phase 3 修正 Y 面绕序后，顶面纹理的 u 轴指向 +Z、v 轴指向 -X（与 Minecraft"顶面纹顶部朝向 -Z"不同）。占位纹理下无视觉影响；正式纹理阶段可统一调整。`[Phase 3]`
- **世界边缘即"虚空"** — 缺失 chunk 读作 air，没有雾效/边缘遮挡，站在世界边缘能看到悬空截面。`[Phase 3]` 计划随 chunk streaming（Phase 14）处理。

## 世界 / 生成

- **洞穴是"奶酪洞"** — 单个 3D 噪声阈值雕刻（`caves.threshold`），生成的是团块状空腔，不是 Minecraft 式的 spaghetti 隧道（两条 3D 噪声相交）。形状可用配置调，但拓扑类型要等以后升级。`[Phase 4]`
- **矿脉是简化随机游走** — 从世界空间格网单元出发、每步 27 邻域随机走的短链（`vein_size` 个方块）；不是 Minecraft 的 3×2×1 旋转椭球 vein。跨 chunk 无缝（格网+边际机制），分布由 `ores.veins` 数据驱动。`[Phase 4]`
- **树是固定模板** — 树干 1×N（4~7）+ 两层半径 2 树冠 + 顶部半径 1 帽，无生长阶段、无 BlockState、无树种差异（不同树只有高度随机）。`[Phase 4]`
- **基岩只有单层** — y=0 是 `terrain.bottom_block`（数据里为 bedrock），只有一层厚；y≥1 的石头理论上可被挖穿（Phase 6 前没有挖掘）。`[Phase 4]`
- **地形高度与洞穴/矿/树相互独立** — 洞穴只保留表层 4 格（`surface_guard`）与底层 2 格（`min_y`）的壳，不感知矿脉/树洞；可能出现洞口正对矿脉等巧合。`[Phase 4]`
- **无准星/目标高亮** — 破坏/放置靠 raycast 命中，但屏幕上没有准星，也没有 MC 式的目标方块描边；命中判定纯几何（reach 5）。`[Phase 6]` 随 UI（Phase 15）加准星+描边。
- **无裂纹动画** — 破坏已是数据驱动耗时（Phase 11：按住左键，`1.5s × hardness ÷ 选中物品 break speed`），但进度无任何视觉反馈——方块要么完整、要么消失，没有 MC 式渐进裂纹纹理/描边。`[Phase 6]`（破坏耗时/工具系统 `[Phase 11]` 已实现）裂纹动画留待 Phase 15（UI/粒子）。
- **放置到未生成区域会同步生成 chunk** — 演示区边缘之外放置方块会触发 `getOrCreateChunk`（生成器在游戏线程同步跑）。`[Phase 6]` Phase 14（异步 chunk streaming）。
- **无潜行/无 1 格自动台阶** — 上 1 格台阶必须主动跳跃；无 crouch（卡 1.8 门框）、无自动 step-up。`[Phase 5]`
- **水平速度无惯性** — 期望速度每 tick 直接设定（无加速/摩擦），松键即停。手感可用但非 Minecraft 式平滑。`[Phase 5]`
- **虚空坠落 = 传送回出生点** — 世界边缘是虚空（见渲染节），掉到 y < -32 直接回出生点，无死亡/掉落伤害/护甲。`[Phase 5]` 随 chunk streaming（Phase 14）+ 死亡机制处理。
- **无合成/存档** — 实体（Phase 10）与物品/背包（Phase 11）已落地；合成（数据驱动 Recipe）与存档仍缺。Phase 12/13。
- **水量不守恒** — 简化规则下流体体积会凭空增减：浅膜落入更深流动池被"吸收"（池保持原水位）；下落链落入世界底部直接消失；落入池时按等级抬升池水位而非按体积换算。Minecraft 用源/流动状态机近似守恒。`[Phase 9]`
- **满水格（source）失去支撑会下落** — 对 Minecraft 的刻意偏离：MC 中 source 水不随支撑消失而下落（只会停止扩散）；本实现中任何满格下方为空都会沿列下落（传送带式补位），行为更"物理"但与 MC 不符。`[Phase 9]`
- **流体不会流进未加载的 chunk** — `World::setFluid` 只写已存在 chunk（不创建）；FluidSim 只 tick 已存在 chunk。湖流到演示区边缘会"暂停"，chunk 加载后恢复扩散。`[Phase 9]` 随 chunk streaming（Phase 14）自然缓解。
- **只有一种流体（水）** — 无岩浆/蜂蜜等；`fluidBlock()` 取第一个 `properties.fluid` 数据方块作纹理参考，加第二种流体需扩展存储（level 数组按流体类型分）。`[Phase 9]`
- **流体不在存档里** — 保存/加载（Phase 13）目前不含 per-chunk 流体数组；存档前需把流体数据纳入修改数据快照。`[Phase 9]` Phase 13。

## 实体（Phase 10）

- **掉落物不受本地光照、也不遮挡光** — 掉落物顶点烘焙满天空光（sky=1.0、block=0），shader 只乘 dayFactor，不采样所在位置的方块光：洞穴/树荫下的掉落物和空地上一样亮（夜间同步变暗）；反过来掉落物也不参与光传播/网格化（不挡光）。需要按物品位置读世界光数组、并让掉落物参与遮挡。`[Phase 10]`
- **掉落物动画由帧计数器驱动（非墙钟）** — 上下浮动（1.5s 周期）/自转（4s 周期）取自 `m_frameCount`，保证无输入/自动化运行逐字节可复现；代价是非 60Hz 显示器上动画速度随帧率变化（与昼夜时间同一权衡）。`[Phase 10]`
- **透明掉落物不做深度排序** — 掉落物按其方块属于哪个 pass（opaque/transparent）与 chunk 同 pass 绘制，但"掉落物之间 + 掉落物与 chunk 之间"无全局排序，复杂透明场景可能出现混合顺序伪影（当前透明方块只有水/玻璃/叶子，演示区规模下不易察觉）。`[Phase 10]`
- **实体与流体无交互** — 掉落物/投射物直接穿过水（无浮力/游泳/阻力）。`[Phase 10]`
- **无实体间碰撞** — 实体之间（含玩家）互相穿过，可以走进掉落物。`[Phase 10]`
- **Projectile 无游戏内发射器、也不渲染** — `Projectile` 实体（抛物线/撞 solid 钉住/30s 消亡）已实现并有单测，但游戏内没有弓等发射器，也没有投射物的渲染几何。`[Phase 10]`（弓留待后续阶段）
- **实体不持久化** — 存档（Phase 13）必须包含实体列表（类型/方块/位置/数量/已存时长）；目前重启后掉落物全部消失。`[Phase 10]` Phase 13。
- **掉落物零初速生成（刻意权衡）** — Minecraft 的掉落物带随机初速散开；本实现在方块中心零速生成，静止位精确、无随机性，headless 截图可逐字节复现。`[Phase 10]`

## 物品 / 背包（Phase 11）

- **工具无分块类型效率** — 工具的 `break_speed` 是 flat 乘数（对任何方块生效）；Minecraft 有镐/铲/斧分类 × 方块类别（石类对镐、土类对铲）+ 无效率工具慢 10× 的规则。v1 只有 flat 乘数，加分类需给方块数据加类别字段并改破坏耗时公式。`[Phase 11]`
- **无库存 UI** — hotbar/背包内容无任何屏幕显示（看不到选中格与数量）；背包格 9-26（storage）可被拾取填入但不可见、不可选（Tab 只循环 9 个 hotbar 格）；无手持物品视图。`[Phase 11]` Phase 15。
- **非 block 物品（工具/材料）无 in-world/手持表现** — 工具与材料都没有纹理/图标，Q 丢出或挖出的工具/材料是**不可见实体**（`drawItemEntities` 跳过非 block 物品——物理/拾取/合并/消亡照常工作）；手持视图同样缺失。`[Phase 11]`（材料 `[Phase 12]` 起同样适用）Phase 15。
- **拾取是固定 1.0 格半径，无磁力** — 玩家底心距掉落物底心 <1.0 且过 0.5s 拾取延迟即整堆入包；Minecraft 会随时间逐渐把物品拉向玩家。`[Phase 11]`
- **放置消耗库存（行为变化，非缺陷）** — Phase 11 起方块不再是无限的：初始 hotbar 每格满堆栈（block 64/工具 1，`hotbar.json` 定义），放置消耗 1 个；hotbar 与背包都空时右键无效。`[Phase 11]`
- **工具数据部分不可获得** — `iron_pickaxe`（break_speed 4.0）已有配方数据（3 iron_ingot + 2 stick，Phase 12），但游戏内无合成 UI/触发（Phase 15），仍然无法在游戏内获得；初始 hotbar 只有 `wooden_pickaxe`。`[Phase 11]`（配方途径 `[Phase 12]`）
- **背包与实体不持久化** — 存档（Phase 13）必须同时包含 Inventory（27 格 ItemStack）与实体列表（类型/物品/位置/数量/已存时长）；目前重启后库存回初始 hotbar、掉落物全部消失。`[Phase 11]` Phase 13（实体部分 `[Phase 10]`）。

## 合成（Phase 12）

- **无游戏内合成 UI/触发** — 合成系统（`Recipe`/`RecipeRegistry` 经 ItemRegistry 交叉严格校验 + `canCraft`/`craft` 确定性纯逻辑 + 4 个配方数据 + 6 种材料物品）已完整并由单测覆盖，游戏启动时严格加载校验；但游戏内**没有任何操作能触发合成**（无 UI、无配方书、无热键）。系统验证目前只靠单测（与 Phase 10 Projectile 无发射器同一模式）。`[Phase 12]` Phase 15（UI）。
- **配方只有 shapeless** — 无 shaped（3×3 网格按位置）配方、无催化剂/"需要工作台"类别、无多配方消歧（同原料不同产物不可能出现——`craft` 按 id 精确执行一个配方，未来 UI 负责让玩家选配方，系统不负责猜）。`[Phase 12]`
- **部分材料暂无用途** — `coal`/`gold_ingot`/`diamond` 可从对应矿石挖得，但没有配方消费它们（无炉子/火把/饰品）；`planks`/`stick`/`iron_ingot` 已在配方链中。`[Phase 12]`
- **矿石掉落改为材料（行为变化，刻意）** — 4 种矿石现在掉材料物品（coal_ore→coal、iron_ore→iron_ingot、gold_ore→gold_ingot、diamond_ore→diamond，MC 同款），**矿方块物品不能再从采矿获得**（数据仍在、可放置，只是无产出途径）；wood 仍掉 wood 本身（它是木板配方的原料，若改掉 planks 则配方链断）。`[Phase 12]`

## 性能 / 架构

- **Chunk 网格化在单线程游戏线程** — 演示区所有 chunk 在第一帧同步 mesh；chunk 数量增长后需要 job queue。`[Phase 3]` Phase 14（worker threads）。`WorldGenerator::generateChunk` 已按无状态纯函数设计（Phase 4），可直接并行。
- **`world.view_distance` 配置存在但未使用** — 演示区大小是编译期常量，没有按视距流式加载。`[Phase 3]` Phase 14。
- **`Chunk&` 引用会被 unordered_map rehash 失效** — `getOrCreateChunk` 返回的引用在后续可能 rehash 的 World 变更（其他 chunk 的 setBlock/getOrCreateChunk）后失效；已在 world.hpp 头部文档化，调用方须在一个逻辑操作内使用。`[Phase 3]`
- **Greedy Meshing 未实现** — 只做 Face Culling，顶点数按暴露面线性增长。计划：性能成为瓶颈时再引入（README 路线图已注明）。

## 数据 / 内容

- **方块无 per-instance 状态** — BlockState 概念未引入（朝向、附属、流体等级等需要时再设计）。`[Phase 2]`
- **音频系统未引入** — OpenAL 依赖尚未接入。

## 已修复（归档）

- **`[2026-10-04, Phase 11]` 破坏不掉落物品 / 无拾取 / 无背包** — Phase 11 实现完整物品闭环：`Item`/`ItemStack`/`ItemRegistry`（每个数据方块隐式同名 block item + `assets/data/items/*.json` 显式工具，严格校验）/`Inventory`（27 格 = 9 hotbar + 18 背包，确定性合并入包）；破坏按 `properties.drops`（物品名，init 严格校验）生成 `ItemEntity`（现持物品名 + 数据 maxStack，合并上限尊重物品数据），玩家 1.0 格内自动拾取（0.5s 延迟防 Q 丢出即回吸）；hotbar 改为背包前 9 格（`hotbar.json` 的 `items` 列表定义初始内容，满堆栈），放置消耗选中格，`Q` 丢弃 1 个。
- **`[2026-10-04, Phase 11]` 破坏瞬时 / 选择方块临时方案** — 破坏改为按住驱动：`breakDurationSeconds(hardness, breakSpeed) = 1.5s × hardness ÷ speed`（工具 = 数据 `break_speed`，徒手/方块物品 = 1.0；hardness ≤0 且可破坏 = 即时），准星移开/松键/换工具（Tab）进度清零；`properties.hardness` 数据首次投入使用。hotbar 从"临时 BlockID 列表"换成背包物品格（Tab 循环 9 格）。已知简化（flat 工具乘数/无裂纹动画/无 UI）记入「物品 / 背包」节。
- **`[2026-10-04, Phase 9]` 透明 pass 实际渲染为不透明** — basic.frag 硬编码 `alpha=1.0`、只取纹理 `.rgb`，导致"透明 pass"里的水/玻璃/叶子全部不透明（树叶纹理里的 20 个透明孔被渲染成纯黑斑点；水看不见湖底）。改为取纹理 `texel.a`（图集本身是 RGBA，alpha 一直保留在 GPU 纹理里）。副作用：截图 PNG 的 alpha 通道曾写入 framebuffer 混合残留值（水面 α≈0.831），`takeScreenshot` 现在强制 α=255（截图是不透明画面记录）。`[Phase 2]` 引入、`[Phase 9]` 发现并修复。
- **`[2026-10-04, Phase 9]` 破坏 1 格深湖底后流体列永久振荡** — 落水满格上方的邻居 7 级膜"坐在满水上被吸收"（7→0），下一 tick 邻居再补 7，7↔0 每 tick 振荡、永不平衡（E2E 探针 330 个 tick 后仍振荡）。修复：`FluidSim` 坐在**满水**上的格子补满到 8（有 source 托底的柱是满的——MC 行为），仅"坐在流动（非满）池上"才走吸收/抬升规则。回归测试 `FilmAboveFullWaterRefillsToFull` + `BrokenLakeFloorRefillsAndSettles`。
- **`[2026-10-04, Phase 9]` 水是静态方块/无游泳** — Phase 9 实现完整流体：per-chunk 0-8 等级数组（8=满/源，1-7 流动；cell 里水与方块互斥），`World::setFluid`（只写已有 chunk、只写 air 格）/`setBlock` 排水；`FluidSim::tick`（快照式、下落传送带、水平扩散 L-1、1 级终端、满水托底补满、max-merge 冲突合并、确定性）游戏线程 20Hz（每 3 帧一次）；玩家游泳（水中重力 12/终端 4/水平 3.1/Space 上浮 4.5，AABB 与流体格相交判定）；光照水衰减（天空光每格 -3，列扫描+BFS）；网格化 `emitFluid`（level/8 缩放立方体、水-水剔面、流体面烘焙自身格光）；世界生成湖改填 fluid 8（`worldgen.json` 移除 `water_block` 键，water 数据方块仅作纹理参考经 `BlockRegistry::fluidBlock()`）。
- **`[2026-10-03, Phase 8]` 静态天空色/无昼夜** — Phase 8 实现昼夜循环：`DayCycle`（`src/world/day_cycle.{hpp,cpp}`，确定性游戏内时间）；shader 天空光 × `u_dayFactor`（黎明/黄昏 smoothstep 渐变，夜晚天空光=0，方块光不受影响）；天空背景色关键帧插值（夜蓝→黎明橙→昼蓝→黄昏橙→夜蓝，正午段与 Phase 1-7 天空色逐位一致——保证基线截图不变）；太阳/月亮/300 颗星为像素级圆形 billboard quad（`assets/shaders/sky.{vert,frag}`，Renderer::drawSky，无 u_model）。新增 CLI `--time <0..1>` 与配置 `world.day_length_seconds`（240）/`world.start_time_of_day`（0.25）。
- **`[2026-10-03, Phase 7]` 占位方向光** — 替换为正式光照系统：每 chunk 两个 16³ 字节数组（sky/block light，0-15）；天空光=列顶扫描（最高 opaque 上方=15）+ BFS（垂直无衰减、水平 -1）；方块光=light_emission 源 BFS（全方向 -1）；setBlock 后局部重传播（天空光按整列、方块光按 ±16 盒，区域外读旧值作边界）；渲染改为顶点烘焙按面光值 + MC 经典面着色（顶 1.0/侧 0.8,0.6/底 0.5）+ 3% 环境光下限；shader 移除 u_lightDir 方向光。关键不变量有测试保证：`updateLocal` 与 `propagateAll` 结果逐格一致。
- **`[2026-10-03, Phase 3]` face 表 +Y/-Y 面绕序反了** — 顶面从上方看是顺时针（被 GL 背面剔除，永远看不见），底面从上方可见。Phase 1/2 的截图验证只采样了侧面纹理，掩盖了该 bug；Phase 3 排查渲染异常时发现并修正，新增绕序回归测试（`FaceWindingIsCounterClockwiseFromOutside`）防复发。
