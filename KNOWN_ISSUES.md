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
- **世界边缘即"虚空"（无雾）** — 未加载区域读作 air，没有雾效/边缘遮挡，站在流式加载的边缘能看到悬空截面（Phase 14 起"边缘"随玩家移动，不再是固定的演示区边界）。`[Phase 3]`（流式部分 `[Phase 14]` 已缓解）
- **顶面/底面纹理朝向约定** — Phase 3 修正 Y 面绕序后，顶面纹理的 u 轴指向 +Z、v 轴指向 -X（与 Minecraft"顶面纹顶部朝向 -Z"不同）。占位纹理下无视觉影响；正式纹理阶段可统一调整。`[Phase 3]`

## 世界 / 生成

- **洞穴是"奶酪洞"** — 单个 3D 噪声阈值雕刻（`caves.threshold`），生成的是团块状空腔，不是 Minecraft 式的 spaghetti 隧道（两条 3D 噪声相交）。形状可用配置调，但拓扑类型要等以后升级。`[Phase 4]`
- **矿脉是简化随机游走** — 从世界空间格网单元出发、每步 27 邻域随机走的短链（`vein_size` 个方块）；不是 Minecraft 的 3×2×1 旋转椭球 vein。跨 chunk 无缝（格网+边际机制），分布由 `ores.veins` 数据驱动。`[Phase 4]`
- **树是固定模板** — 树干 1×N（4~7）+ 两层半径 2 树冠 + 顶部半径 1 帽，无生长阶段、无 BlockState、无树种差异（不同树只有高度随机）。`[Phase 4]`
- **基岩只有单层** — y=0 是 `terrain.bottom_block`（数据里为 bedrock），只有一层厚；y≥1 的石头理论上可被挖穿（Phase 6 前没有挖掘）。`[Phase 4]`
- **地形高度与洞穴/矿/树相互独立** — 洞穴只保留表层 4 格（`surface_guard`）与底层 2 格（`min_y`）的壳，不感知矿脉/树洞；可能出现洞口正对矿脉等巧合。`[Phase 4]`
- **放置到未生成区域会同步生成 chunk** — 演示区边缘之外放置方块仍触发 `getOrCreateChunk`（生成器在游戏线程同步跑）。Phase 14 实现了流式加载，但放置需要**立即**拿到目标 chunk（放置当帧生效），所以该路径刻意保持同步（一列 8 chunk ≈1.6 ms 的生成尖峰）；流式管线随后按正常路径为它补光照。`[Phase 6]`
- **水平速度无惯性** — 期望速度每 tick 直接设定（无加速/摩擦），松键即停。手感可用但非 Minecraft 式平滑。`[Phase 5]`
- **虚空坠落 = 死亡（非传送）** — 掉到 y < -32 直接死亡（`damage(maxHealth×1000)`），走死亡/重生流程（Phase 16，`R` 回出生点满血，背包保留）。Phase 14 起世界随玩家流式扩展，走路时地面始终在前方加载（预算 1 列/帧 ≫ 步行速度），虚空坠落大幅减少——但仍可能发生（读档点位于未加载区域时先下落几帧再由流式补上地面；快于流式速度离开时）。`[Phase 5]`（虚空范围 `[Phase 14]` 缓解；`[Phase 16]` 死亡机制已实现）
- **水量不守恒** — 简化规则下流体体积会凭空增减：浅膜落入更深流动池被"吸收"（池保持原水位）；下落链落入世界底部直接消失；落入池时按等级抬升池水位而非按体积换算。Minecraft 用源/流动状态机近似守恒。`[Phase 9]`
- **满水格（source）失去支撑会下落** — 对 Minecraft 的刻意偏离：MC 中 source 水不随支撑消失而下落（只会停止扩散）；本实现中任何满格下方为空都会沿列下落（传送带式补位），行为更"物理"但与 MC 不符。`[Phase 9]`
- **流体不会流进未加载的 chunk** — `World::setFluid` 只写已存在 chunk（不创建）；FluidSim 只 tick 已存在 chunk。Phase 14 起"未加载"只存在于流式半径环之外/玩家身后：水在加载区边界暂停，玩家走近加载后恢复扩散（生成地形本身的水是平衡态，tick 无变化）。`[Phase 9]`（范围 `[Phase 14]` 已收窄）
- **只有一种流体（水）** — 无岩浆/蜂蜜等；`fluidBlock()` 取第一个 `properties.fluid` 数据方块作纹理参考，加第二种流体需扩展存储（level 数组按流体类型分）。`[Phase 9]`

## 实体（Phase 10）

- **掉落物不受本地光照、也不遮挡光** — 掉落物顶点烘焙满天空光（sky=1.0、block=0），shader 只乘 dayFactor，不采样所在位置的方块光：洞穴/树荫下的掉落物和空地上一样亮（夜间同步变暗）；反过来掉落物也不参与光传播/网格化（不挡光）。需要按物品位置读世界光数组、并让掉落物参与遮挡。`[Phase 10]`
- **掉落物动画由帧计数器驱动（非墙钟）** — 上下浮动（1.5s 周期）/自转（4s 周期）取自 `m_frameCount`，保证无输入/自动化运行逐字节可复现；代价是非 60Hz 显示器上动画速度随帧率变化（与昼夜时间同一权衡）。`[Phase 10]`
- **透明掉落物不做深度排序** — 掉落物按其方块属于哪个 pass（opaque/transparent）与 chunk 同 pass 绘制，但"掉落物之间 + 掉落物与 chunk 之间"无全局排序，复杂透明场景可能出现混合顺序伪影（当前透明方块只有水/玻璃/叶子，演示区规模下不易察觉）。`[Phase 10]`
- **实体与流体无交互** — 掉落物/投射物直接穿过水（无浮力/游泳/阻力）。`[Phase 10]`
- **实体间碰撞只做玩家↔掉落物分离** — Phase 16 起玩家会把掉落物沿最小穿透轴推开（面到面跨度，远离玩家中心），不再"走进"掉落物；但**掉落物之间**、以及未来的实体（投射物/mob）之间仍互相穿过（无通用实体-实体碰撞求解）。`[Phase 10]`（玩家↔掉落物 `[Phase 16]` 已解决）
- **Projectile 无游戏内发射器、也不渲染** — `Projectile` 实体（抛物线/撞 solid 钉住/30s 消亡）已实现并有单测，但游戏内没有弓等发射器，也没有投射物的渲染几何。`[Phase 10]`（弓留待后续阶段）
- **掉落物零初速生成（刻意权衡）** — Minecraft 的掉落物带随机初速散开；本实现在方块中心零速生成，静止位精确、无随机性，headless 截图可逐字节复现。`[Phase 10]`

## 物品 / 背包（Phase 11）

- **工具无分块类型效率** — 工具的 `break_speed` 是 flat 乘数（对任何方块生效）；Minecraft 有镐/铲/斧分类 × 方块类别（石类对镐、土类对铲）+ 无效率工具慢 10× 的规则。v1 只有 flat 乘数，加分类需给方块数据加类别字段并改破坏耗时公式。`[Phase 11]`
- **非 block 物品的掉落物在 world 中不可见** — 工具/材料的**掉落物**（Q 丢出或挖出）没有 in-world 实体表示（`drawItemEntities` 跳过非 block 物品——物理/拾取/合并/消亡照常工作）；UI 侧（背包/hotbar/手持/合成产物）Phase 15 起已有图标（`Item.icon` 16×16 纹理）。`[Phase 11]`（UI 部分 `[Phase 15]` 已解决）
- **拾取是固定 1.0 格半径，无磁力** — 玩家底心距掉落物底心 <1.0 且过 0.5s 拾取延迟即整堆入包；Minecraft 会随时间逐渐把物品拉向玩家。`[Phase 11]`
- **放置消耗库存（行为变化，非缺陷）** — Phase 11 起方块不再是无限的：初始 hotbar 每格满堆栈（block 64/工具 1，`hotbar.json` 定义），放置消耗 1 个；hotbar 与背包都空时右键无效。`[Phase 11]`

## 合成（Phase 12）

- **配方只有 shapeless** — 无 shaped（3×3 网格按位置）配方、无催化剂/"需要工作台"类别、无多配方消歧（同原料不同产物不可能出现——`craft` 按 id 精确执行一个配方，未来 UI 负责让玩家选配方，系统不负责猜）。`[Phase 12]`
- **部分材料暂无用途** — `coal`/`gold_ingot`/`diamond` 可从对应矿石挖得，但没有配方消费它们（无炉子/火把/饰品）；`planks`/`stick`/`iron_ingot` 已在配方链中。`[Phase 12]`
- **矿石掉落改为材料（行为变化，刻意）** — 4 种矿石现在掉材料物品（coal_ore→coal、iron_ore→iron_ingot、gold_ore→gold_ingot、diamond_ore→diamond，MC 同款），**矿方块物品不能再从采矿获得**（数据仍在、可放置，只是无产出途径）；wood 仍掉 wood 本身（它是木板配方的原料，若改掉 planks 则配方链断）。`[Phase 12]`

## 存档（Phase 13）

- **存档是 opt-in（`--save <slot>`），无默认世界** — 不带 `--save` 时不读不写（保持既有运行行为与基线截图不变）；没有"上次世界"的隐式恢复，也没有 MC 式多存档槽管理/选择 UI（多个槽只是多个文件，手动切换）。`[Phase 13]`
- **写存档时在退出路径重跑生成器做 diff** — `diffWorld` 对每个已有 chunk 用 scratch chunk 重跑纯函数 `generateChunk` 逐格比对（仅发生在干净退出时）；Phase 14 流式加载后世界可达 2300+ chunk，尖峰相应变长（实测 2312 chunk ≈ 0.5 s，一次性）；需要改增量编辑跟踪或并行 diff。`[Phase 13]`（`[Phase 14]` 起规模放大）
- **无周期性自动存档** — 崩溃/断电丢失自上次写入以来的全部修改；`F5` 立即存档（Phase 15，`--save` 槽激活时游戏内随时手动保存）已补上"会话中手动保存"，但仍无周期性 autosave/无"保存并退出"流程。`[Phase 13]`（手动保存热键 `[Phase 15]` 已解决）
- **玩家速度/onGround 不存** — 读档用 `teleportTo`（速度清零、onGround=false）：存档时玩家静止则一帧重力回落到整数平面后逐位精确；存档时玩家在空中/移动则位置保留但速度丢失（读档后从该位置重新下落）。`[Phase 13]`
- **实体 `age` 按真实 dt 累积（墙钟）** — 与玩家/物理一致（非固定步长），所以含实体的存档文件在 age 字段上跨运行差 ±1-2 帧；渲染不依赖 age（掉落物动画由帧计数器驱动），截图确定性不受影响。昼夜 `time` 是固定步长（位精确）。`[Phase 13]`
- **`DayCycle::setTimeOfDay` 至多 1 ulp 偏差** — 存 timeOfDay [0,1)，恢复时解出 totalSeconds；对远离配置 start_time 的值，`startTime + delta` 的浮点往返可能差 1 ulp（~1e-16）。昼夜渐变段是常量/平滑的，呈现无影响。`[Phase 13]`
- **演示区外的编辑会带上"生成的 chunk"** — 在演示区外放置方块会同步生成该 chunk（既有行为），存档 diff 覆盖它；读档时 `setBlock` 重建同样生成该 chunk——一致，但读档后的世界含全新世界没有的 chunk（读档后 `propagateAll` 全量重传播，之后新出现的 chunk 由 Phase 14 的 worker 光照 job 处理）。`[Phase 13]`
- **存档无迁移机制** — format 1 之外一律拒绝（无 format 2 读取器）；字段集由 format 号锁定，未知字段=报错（机器生成文件的损坏/版本检测，与用户数据文件的"警告容忍"相反）。`[Phase 13]`

## Chunk 流式（Phase 14）

- **只增不卸载** — 流式只加载、从不卸载 chunk：内存随探索面积线性增长（半径 8 列环 ≈ 2304 chunk ≈ 35 MB）。卸载需要先把"卸载前该区域的编辑"持久化（与 Phase 13 存档格式交互：diff 是相对生成器的，卸载本身不入档，但卸载后再编辑会重新生成 chunk，diff 语义仍成立——真正的问题是卸载时玩家/实体可能站在将被卸载的 chunk 上、流体边界状态丢失），v1 刻意不做。`[Phase 14]`
- **新列首帧以光 0 渲染（1 帧暗）** — 流式列集成后先网格化（光数组还是 0）、光照 worker job 在网格化之后才 post，该列与受影响的边界 chunk 有 1 帧暗色，job 完成后（下帧）重网格为正确光照。帧锁定下的确定性 pop-in（与 MC 的 chunk pop-in 观感一致）；若未来要消除需把光照提前到集成同帧（~19 ms 主线程尖峰）。`[Phase 14]`
- **增量光照不能"消光"（封洞伪影）** — 新列的光照走 `updateRegion`（天空光列重扫可减可增；BFS 只严格增大）。若某处旧高光的唯一通路穿过当时未加载的虚空（如演示区边缘的洞穴开向虚空），新列生成后通路被封，区域外的旧高值不会被降低（BFS 不跨区减光）。旧代码的"新 chunk→全量重传播"无此伪影（代价是每次 50 ms+ 游戏线程尖峰）。窄场景，演示区边缘的洞穴开向虚空才触发。`[Phase 14]`
- **流式触发是"贴近缺失列"迟滞，静止玩家永不流式** — 激活 = 玩家距缺失列 ≤1（Chebyshev）；激活后直到玩家周围半径环充满才解除。设计目的：无输入运行（基线截图）零 job、逐字节复现。副作用：站在加载区正中（距边缘 2 列）的玩家即使半径环未充满也不会流式（他从未"贴近过"边缘）。`[Phase 14]`
- **存档点位于未加载区域时，读档玩家先下落几帧** — 读档世界 = 演示区 + 编辑重建的 chunk；存档玩家在演示区外时，他的列由流式在 ~2-4 帧内补上（触发器对"玩家自身列缺失"= 距离 0 立即激活），期间玩家从存档位置下落（实测 ~0.03 格，落在整数地面平面后逐位精确）。无死亡风险（远小于 -32 杀线）。`[Phase 14]`

## 性能 / 架构

- **Chunk 网格化在游戏线程（有界）** — 启动时演示区所有 chunk 在第一帧同步 mesh；Phase 14 后增量 meshing 的每帧成本由流式预算（1 列/帧）限界（实测 ~0.01 ms/chunk，8 chunk/帧 ≈ 0.08 ms）。worker 网格化需要"快照式"设计（worker 读 World 的边界邻块），留待性能成为瓶颈时。`[Phase 3]`（生成部分 `[Phase 14]` 已并行化）
- **`Chunk&` 引用会被 unordered_map rehash 失效** — `getOrCreateChunk` 返回的引用在后续可能 rehash 的 World 变更（其他 chunk 的 setBlock/getOrCreateChunk）后失效；已在 world.hpp 头部文档化，调用方须在一个逻辑操作内使用。`[Phase 3]`
- **Greedy Meshing 未实现** — 只做 Face Culling，顶点数按暴露面线性增长。计划：性能成为瓶颈时再引入（README 路线图已注明）。

## UI / 音效 / 粒子（Phase 15）

- **音效是固定事件集，无 per-block/per-item 差异** — v1 只有 8 个数据驱动事件（dig/place/break/pickup/step/ui_open/ui_close/craft），所有方块共用同一 break/place/dig 音色，无 MC 式的材质/方块音色差异、无 3D 空间化（单声道 2D 播放）。加 per-block 音色需扩事件集 + 音色映射数据。`[Phase 15]`
- **物品无 3D 模型，图标是程序化 16×16 贴图** — block item 的 UI 表示是带光照的倾斜 3D 立方体（复用掉落物 cube mesh），工具/材料是 `tools/generate_item_icons.py` 程序化生成的 16×16 平面图标（非 MC 的 3D 旋转手持模型）；无手持动画/旋转。`[Phase 15]`
- **背包 UI 右键语义极简** — 左键取整堆/放入/交换（取出被占堆）/并入有空间同物堆；右键只"移 1 个"（同物有空间或空格，不同物格无效）；无 MC 的"右键取半堆"、无拖拽、无 shift 快捷移动、无双击分拣。点击面板外（左键）丢弃手持堆。`[Phase 15]`
- **背包打开不暂停世界模拟** — 打开背包只暂停**玩家物理与方块交互**；世界/昼夜/流体/实体/流式加载照常 tick（与 MC 的"暂停菜单"不同，更接近"边看背包边看世界"）。`[Phase 15]`
- **F3 FPS 显示为整数** — `m_fpsSmoothed`（真实 dt 平滑）取整显示，vsync 稳定时恒 60；无小数帧率、无帧时间直方图/分位数（Minecraft F3 有 1% / 0.1% lows）。`[Phase 15]`
- **粒子是逐帧动态 mesh、无碰撞** — 每帧重建一个动态 indexed mesh（0.12 格小立方体），粒子之间/粒子与世界无碰撞（穿模），无风/阻力，半隐式 Euler（先速后位）；上限 256、每爆发 16、寿命 0.45~0.75s。spawn 时烘焙光照（渲染是只读窗口，不能读光数组）。`[Phase 15]`
- **字体是程序化 8×8 像素字** — `tools/generate_font.py` 生成 128×256 字形 atlas（16px cell、glyph 4px 偏移），仅 ASCII 可打印（非 ASCII → `?`）；无等宽以外的字重/字号渐变（缩放是整数像素复制）、无中文字形。`[Phase 15]`
- **物品名 tooltip 只有名字** — 悬停格显示物品名（无数量/描述/工具耐久/附魔文本）。`[Phase 15]`

## 生存（Phase 16）

- **伤害来源只有坠落与虚空** — 无 mob 攻击、无岩浆/火/窒息伤害、无摔落以外的接触伤害。`fallDamage = max(0, 净下落格数 − fallDamageOffset)`（offset 默认 3），从离地帧的 `fallStartY` 到落地点的净竖直距离结算；落入水中的落地取消伤害。无护甲/摔落缓冲机制。`[Phase 16]`
- **生命 HUD 是 10 心三态，半心按 0.5 HP 粒度渲染** — 每颗心 2 HP，填充 = `clamp(hp − 2i, 0, 2)`（2=满/1=半/0=空），贴图是 3 个固定状态（满/半/空），无更细粒度（如 0.5 HP 的四分之心）；MC 的心是 10 颗每颗 1 HP 且支持 0.5 增量。`[Phase 16]`
- **无死亡分数/死亡统计** — 死亡只显示 "You died! / Press R"，无得分、无死亡次数、无 Game Over 统计屏；重生保留背包与全部物品（无死亡掉落）。`[Phase 16]`
- **自然回复是无条件定时** — 脱离受伤 `regenDelay=10s` 后每 `regenInterval=4s` 回 1 HP；无饥饿条、无食物、回复不依赖任何资源。`[Phase 16]`
- **潜行缩高在纯整数方块世界无通行效果** — 潜行把身高 1.8→1.6（脚底不动、眼高 1.62→1.42）。整数方块世界的头顶空间恒为 ≥2.0（站立可过）或 ≤1.0（潜行也不可过），1.6 不改变可通行集合；潜行当前只影响速度（×0.3）与禁用 step-up，"钻 1.8 门框" 的效果要等出现非整数高度（半块/实体头顶）才有意义。`[Phase 16]`
- **step-up 只抬升 1 格、每 tick 至多一次** — 条件：非潜行 + 贴地 + 身体贴住整数块面（`kFaceEps=1e-4`）+ 面前脚位 solid + 其上方 2 格空；X 轴优先于 Z 轴（固定顺序），一次 update 只抬一次。2 格及以上仍须跳跃。`[Phase 16]`

## 工作台 / 形状合成（Phase 17）

- **全工作台的网格是同一个容器** — `Game` 持有单个 `m_tableGrid`（`CraftingGrid`）：右键打开的**任何**工作台的 3×3 网格都是它——内容跨关/重开保留、并写入存档（format 3 `table_grid`），但放第二个工作台再打开看到的是同一份内容。MC 里每个工作台有独立容器；per-block 容器需要 per-instance 状态（见「数据 / 内容」）。`[Phase 17]`
- **工作台面板无配方列表** — 表格面板只有 3×3 网格 + 输出格，无任何配方提示；配方提示只存在于 E 面板列表（且该列表仅 shapeless），shaped 配方的摆法没有游戏内指引。`[Phase 17]`
- **E 面板 2×2 固定在共享 3×3 模型的左上** — E 面板合成格与表格面板共用同一个 `CraftingGrid` 3×3 模型，只暴露槽 {0,1,3,4}（左上 2×2）；宽度或高度为 3 的 pattern（如 pickaxe `PPP/.S./.S.`）无法摆入，E 里的 shaped 匹配只可能命中落在左上角的 pattern（2×2 工作台配方可在 E 摆出合成，与 MC 一致）。`[Phase 17]`
- **E 配方列表只显示 shapeless 配方** — shaped 配方不出现在列表里，唯一入口是工作台面板。`[Phase 17]`
- **E2E 合成输入的环境约束（本机 :0）** — xdotool 每次调用 ~150 ms（长序列追不上帧帽截图）；XTest 伪事件**每个动作后必须 `XFlush`**（libX11 会缓冲无应答请求成 ~1s 一批投递，同帧事件被 `wasPressed` 边沿逻辑折叠）；合成输入要求点击位置处游戏窗口在最上层（运行时临时 unmap UI 窗口），但 unmap 期间用户的真实鼠标会污染运行（指针锁定期间的真实 motion = 相机旋转），所以运行前须等光标静止 ~400 ms；地面命中格瞄准容差在 pitch −0.80 时仅 ~±10 px，驱动用 pitch −0.68（~±49 px）。Xephyr :99 仅作逻辑回退（:0 光标扫过 Xephyr 窗口会被转发进 :99；llvmpipe 渲染不能用于 base 字节基线）。`[Phase 17]`

## 数据 / 内容

- **方块无 per-instance 状态** — BlockState 概念未引入（朝向、附属、流体等级等需要时再设计）。`[Phase 2]`

## 已修复（归档）

- **`[2026-10-04, Phase 16]` 无潜行/无 1 格自动台阶（P5）/ 虚空坠落=传送回出生点·无死亡机制（P5）/ 实体间互相穿过（P10，玩家↔掉落物部分）/ 无生命值·受伤·死亡 UI** — Phase 16 实现生存系统：`Player` 扩展 `maxHealth=20`（10 心）+ **坠落伤害**（`max(0, 净下落格数 − 3)`，离地帧记 `fallStartY`，落地结算，落水中取消）+ **虚空（y < −32）= 死亡** + 死亡冻结 + "You died! / Press R" + `R` 重生（回出生点满血、速度清零、**背包保留**）+ **自然回复**（脱离受伤 10s 后每 4s 回 1 HP）+ **受伤红闪**（`0.35×剩余/0.5s` 渐隐，`m_prevHealth` 轮询驱动，致死伤不重复播）+ **潜行**（`LeftControl`：身高 1.8→1.6 脚底不动、眼高 1.62→1.42、速度 ×0.3 水中除外、潜行不 step-up）+ **1 格自动台阶**（非潜行+贴地+贴整数块面+面前脚位 solid+上方 2 格空 → 抬 1 格继续本 tick 移动，X 轴优先、每 tick 至多一次）+ **玩家↔掉落物分离**（沿最小穿透轴把物品推开、远离玩家中心、面到面跨度，玩家不被动位移）+ **生命 HUD**（hotbar 上方 10 心 16px/2px，三态满/半/空取自 48×16 条带，填充 `clamp(hp−2i,0,2)`，仅游玩态渲染）。存档格式 2（`player.health`，省略=满；解析接受 1–2；`applySave` 映射 −1→maxHealth、0→死亡）。音效 +2（hurt/death → 10 样本）。**stepUp Z 轴回归 bug**（`cz` 误用 moveAxis 编号 `axis==2` 恒假 → Z 向永不 step，X 向正常）由新增 `ClimbsOneBlockAlongZ` 单测捕获并在同阶段修复（`axis==1`）。`addQuad` 拆分 `addQuadUv`（P15 遗留：`addQuad` 曾把 UV 留 (0,0)）。新增 15 个单测（PlayerTest 生存套件[坠落伤害精确/落水中取消/虚空死亡/重生满血清速留包/回复计时/潜行缩高+不 step/step-up X·Z 轴+2 格不抬+低顶不抬]、ItemEntityTest 玩家↔物品分离 ×3、SaveTest health 往返、InputTest R 键、DebugOverlayTest Phase 16 行）。E2E：base 逐字节不变；fall（4 格坑→hp 19.008842、心 10 半[左红(222,38,34)/右暗(55,57,70)]、心 1 满）×2 字节一致；step（Tab 泥土 B (0,54,-3)+Tab×8 石头 C (0,55,-3) 叠 B 顶+石头 A (0,54,-1)，W 1.5s 自动 step-up 上 A、跨越、1 格落下、被 B+C 墙钳 (0.5,54.0,−1.7) 精确）×2 字节一致。**驱动规则**：xdotool 鼠标按钮必须 `mousedown 1`/`mouseup 1`（`keydown 1` 是数字键"1"）；fall 瞄准 pitch −1.37（448px 下移，浅角度 −1.2 会削坑远壁）。新简化记入「生存（Phase 16）」节。
- **`[2026-10-04, Phase 15]` 无准星/目标高亮（P6）/ 无裂纹动画（P6）/ 无库存 UI（P11）/ 工具数据部分不可获得（P11）/ 无游戏内合成 UI·触发（P12）/ 音频系统未引入** — Phase 15 实现 UI/音效/粒子/性能统计：专用无光照 UI 着色器（`assets/shaders/ui.{vert,frag}`，u_useTexture/u_color，ortho 屏幕空间与 perspective 世界空间通用）+ 批量化绘制（`UiBatch`，同色同纹理合并为一个 mesh+draw）；准星/目标方块线框描边（`MeshMode::Lines`）/hotbar（选中橙框+数量）/手持物品；`E` 背包+合成面板（纯逻辑 `InventoryLayout`+点击状态机 `InventoryUI`：左键取/放/交换/并入、右键移 1、点击外丢弃、悬停物品名、点击配方行 `craft()`）；`F3` 调试 overlay（8 行：FPS 整数/chunk·draw·顶点/坐标/seed/昼夜/流式/背包·粒子）；`F5` 立即存档（`--save` 激活时）。破坏反馈：5 级累积裂纹条（`tools/generate_cracks.py`，进度→stage 钳制）+ 破坏粒子（`ParticleSystem`，确定性 splitmix64 爆发、半隐式 Euler、上限 256、spawn 烘焙光照——渲染只读窗口不读光数组）。音效：`SoundBank`（SDL3 `SDL_OpenAudioDeviceStream` 队列式，NULL 回调，`SDL_PutAudioStreamData` 排队；init 失败优雅降级静音+WARN）+ 8 个程序化 WAV（`tools/generate_sounds.py`，44.1kHz 16-bit 单声道），固定事件集（dig 每 0.3s tick/break/place/pickup/step 每 0.4s/ui_open·close/craft），音量 `audio.volume`（默认 0.7）。物品图标：block item = 带光照倾斜 3D cube（复用掉落物 mesh，3D 图标在 2D 批之后绘制防被槽背景盖住），工具/材料 = `Item.icon` 数据字段 → `assets/textures/items/<icon>.png`（`tools/generate_item_icons.py`，init 交叉校验文件存在）。字体：程序化 8×8 字形（`tools/generate_font.py` → `font.png` 128×256，`src/ui/font` UV/顶点纯逻辑）。**门控规则**：所有 UI 只在按键（F3/E）或指针锁定或事件（裂纹/粒子/音效）时渲染 → 无输入 60 帧运行零新增绘制，`base3.png` 基线逐字节不变（E2E 验证）。新增 40 个单测（font UV/文本宽度/顶点；debug overlay 行格式；InventoryUI 布局/左键全流程/右键/点击外丢弃/合成行；Particle 确定性爆发/更新/上限/死亡；Sound parseWav/applyGain）+ E2E（无输入基线逐字节；锁定态准星+hotbar+手持+描边；F3 overlay；背包面板+悬停；背包左键取整堆移格；点击 planks 配方行合成 wood 64→63+planks×4；按住破坏 0.45s 裂纹 stage 2；破坏后粒子消亡定格——各用例两次运行截图逐字节一致；8 次运行日志 "Audio ready: 8 samples, volume 0.700000"）。新简化记入「UI / 音效 / 粒子（Phase 15）」节。
- **`[2026-10-04, Phase 14]` `world.view_distance` 未使用 / 世界边缘固定 / 新 chunk 触发全量重传播 / 光照更新全在游戏线程** — Phase 14 实现 chunk 流式 + 多线程：`ThreadPool`（`src/core/thread_pool.{hpp,cpp}`，固定 4 worker、`post`/`flush` 帧同步点、首个 job 异常在 flush 重抛、析构先跑完 pending job 再 join）+ 纯决策逻辑 `chunk_streaming`（`missingColumns` 距离优先确定性排序 / `nearestMissingDistance` 迟滞输入 / 玩家列 Chebyshev 几何）+ `World::insertChunk`（预建 chunk 插入 + 面邻居标脏防边界 Z-fighting）+ 帧管线（flush → 主线程写 World → 网格化/渲染只读 → post jobs，worker 只运行在 post→flush 窗口）。按"列"（整列 8 垂直 chunk）流式：worker 线程并行调纯函数 `generateChunk` 填 scratch chunk（零 World 接触），主线程集成 + 网格化；`world.view_distance`（默认 8 列）首次生效为流式半径；迟滞激活（≤1 列贴近缺失列 → 激活，半径环充满 → 解除），静止玩家零 job。新列光照 = 单 worker job 顺序 `updateRegion`（相邻区域重叠，光照 job 禁止并发），替代旧的"chunkCount 变化 → 游戏线程 `propagateAll`"（每新 chunk 50 ms+ 尖峰）。新增 21 个单测（ThreadPool 执行/flush 语义/worker 线程/异常传递/析构 join/5000 job 压力；流式几何排序/半径边界/迟滞；insertChunk 去重/邻居标脏/自标脏；增量光照 == 全量传播逐格等价 ×2 场景）+ E2E（无输入基线逐字节不变；seed 57 走 36 格跨边缘停在 1 格台阶、半径 8 环 273 列流满、两次运行截图逐字节一致、2312 chunk 14 s 静止；save 标志运行中性；读档玩家落在未加载区、流式 4 ms 内补地面、两次运行逐字节一致）。新简化记入「Chunk 流式（Phase 14）」节。
- **`[2026-10-04, Phase 13]` 无存档 / 背包与实体不持久化 / 流体改动不入档** — Phase 13 实现完整存档（`--save <slot>`/`--new`，`src/core/save.{hpp,cpp}`）：`data/saves/<slot>.json` = seed + 与 `generateChunk(seed)` 的 diff（`blocks`/`fluids` 编辑列表，只存不同的格子）+ 玩家 pos/yaw/pitch + 昼夜 timeOfDay + 27 格背包 + 选中格 + 掉落物实体（item/count/pos/age/pickup_delay）。读档 = 生成世界后逐格应用（先方块后流体）→ `propagateAll` → 恢复玩家/背包/实体（`setAge`/`setPickupDelay`）/`DayCycle::setTimeOfDay`。严格加载校验（format 号/seed 一致/注册表交叉/位置不重复/方块+流体不同格/未知字段=错误）。JSON 浮点序列化本就最短可往返精度（位精确）。新增 17 个单测（round-trip 位精确/严格校验全路径/diff 精确检出/apply 恢复与失败/diff→fresh→apply 全格等价）+ E2E（无 flag 基线字节不变；存档文件跨运行字节一致；加载路径截图字节一致；含实体存档 age 恢复精确）。
- **`[2026-10-04, Phase 11]` 破坏不掉落物品 / 无拾取 / 无背包** — Phase 11 实现完整物品闭环：`Item`/`ItemStack`/`ItemRegistry`（每个数据方块隐式同名 block item + `assets/data/items/*.json` 显式工具，严格校验）/`Inventory`（27 格 = 9 hotbar + 18 背包，确定性合并入包）；破坏按 `properties.drops`（物品名，init 严格校验）生成 `ItemEntity`（现持物品名 + 数据 maxStack，合并上限尊重物品数据），玩家 1.0 格内自动拾取（0.5s 延迟防 Q 丢出即回吸）；hotbar 改为背包前 9 格（`hotbar.json` 的 `items` 列表定义初始内容，满堆栈），放置消耗选中格，`Q` 丢弃 1 个。
- **`[2026-10-04, Phase 11]` 破坏瞬时 / 选择方块临时方案** — 破坏改为按住驱动：`breakDurationSeconds(hardness, breakSpeed) = 1.5s × hardness ÷ speed`（工具 = 数据 `break_speed`，徒手/方块物品 = 1.0；hardness ≤0 且可破坏 = 即时），准星移开/松键/换工具（Tab）进度清零；`properties.hardness` 数据首次投入使用。hotbar 从"临时 BlockID 列表"换成背包物品格（Tab 循环 9 格）。已知简化（flat 工具乘数/无裂纹动画/无 UI）记入「物品 / 背包」节。
- **`[2026-10-04, Phase 9]` 透明 pass 实际渲染为不透明** — basic.frag 硬编码 `alpha=1.0`、只取纹理 `.rgb`，导致"透明 pass"里的水/玻璃/叶子全部不透明（树叶纹理里的 20 个透明孔被渲染成纯黑斑点；水看不见湖底）。改为取纹理 `texel.a`（图集本身是 RGBA，alpha 一直保留在 GPU 纹理里）。副作用：截图 PNG 的 alpha 通道曾写入 framebuffer 混合残留值（水面 α≈0.831），`takeScreenshot` 现在强制 α=255（截图是不透明画面记录）。`[Phase 2]` 引入、`[Phase 9]` 发现并修复。
- **`[2026-10-04, Phase 9]` 破坏 1 格深湖底后流体列永久振荡** — 落水满格上方的邻居 7 级膜"坐在满水上被吸收"（7→0），下一 tick 邻居再补 7，7↔0 每 tick 振荡、永不平衡（E2E 探针 330 个 tick 后仍振荡）。修复：`FluidSim` 坐在**满水**上的格子补满到 8（有 source 托底的柱是满的——MC 行为），仅"坐在流动（非满）池上"才走吸收/抬升规则。回归测试 `FilmAboveFullWaterRefillsToFull` + `BrokenLakeFloorRefillsAndSettles`。
- **`[2026-10-04, Phase 9]` 水是静态方块/无游泳** — Phase 9 实现完整流体：per-chunk 0-8 等级数组（8=满/源，1-7 流动；cell 里水与方块互斥），`World::setFluid`（只写已有 chunk、只写 air 格）/`setBlock` 排水；`FluidSim::tick`（快照式、下落传送带、水平扩散 L-1、1 级终端、满水托底补满、max-merge 冲突合并、确定性）游戏线程 20Hz（每 3 帧一次）；玩家游泳（水中重力 12/终端 4/水平 3.1/Space 上浮 4.5，AABB 与流体格相交判定）；光照水衰减（天空光每格 -3，列扫描+BFS）；网格化 `emitFluid`（level/8 缩放立方体、水-水剔面、流体面烘焙自身格光）；世界生成湖改填 fluid 8（`worldgen.json` 移除 `water_block` 键，water 数据方块仅作纹理参考经 `BlockRegistry::fluidBlock()`）。
- **`[2026-10-03, Phase 8]` 静态天空色/无昼夜** — Phase 8 实现昼夜循环：`DayCycle`（`src/world/day_cycle.{hpp,cpp}`，确定性游戏内时间）；shader 天空光 × `u_dayFactor`（黎明/黄昏 smoothstep 渐变，夜晚天空光=0，方块光不受影响）；天空背景色关键帧插值（夜蓝→黎明橙→昼蓝→黄昏橙→夜蓝，正午段与 Phase 1-7 天空色逐位一致——保证基线截图不变）；太阳/月亮/300 颗星为像素级圆形 billboard quad（`assets/shaders/sky.{vert,frag}`，Renderer::drawSky，无 u_model）。新增 CLI `--time <0..1>` 与配置 `world.day_length_seconds`（240）/`world.start_time_of_day`（0.25）。
- **`[2026-10-03, Phase 7]` 占位方向光** — 替换为正式光照系统：每 chunk 两个 16³ 字节数组（sky/block light，0-15）；天空光=列顶扫描（最高 opaque 上方=15）+ BFS（垂直无衰减、水平 -1）；方块光=light_emission 源 BFS（全方向 -1）；setBlock 后局部重传播（天空光按整列、方块光按 ±16 盒，区域外读旧值作边界）；渲染改为顶点烘焙按面光值 + MC 经典面着色（顶 1.0/侧 0.8,0.6/底 0.5）+ 3% 环境光下限；shader 移除 u_lightDir 方向光。关键不变量有测试保证：`updateLocal` 与 `propagateAll` 结果逐格一致。
- **`[2026-10-03, Phase 3]` face 表 +Y/-Y 面绕序反了** — 顶面从上方看是顺时针（被 GL 背面剔除，永远看不见），底面从上方可见。Phase 1/2 的截图验证只采样了侧面纹理，掩盖了该 bug；Phase 3 排查渲染异常时发现并修正，新增绕序回归测试（`FaceWindingIsCounterClockwiseFromOutside`）防复发。
