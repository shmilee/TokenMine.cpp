# 已知问题清单（Known Issues）

每个阶段完成后把新发现的限制/简化记到这里；修复后移入「已修复（归档）」。
条目格式：**问题** — 影响与现状。`[阶段]` 标注发现/引入阶段，`计划` 标注预计修复阶段。

## 渲染

- **同类非 opaque 方块之间的内部面仍会生成**（水挨水、叶子挨叶子）— 剔除规则 v1 只判断"邻居是否 opaque"，结果正确但多发了面。`[Phase 3]` 计划随流体（Phase 9）细化为"邻居是否为同种流体"。
- **光的遮挡是二值的**（opaque=15，其余=0）— 水/玻璃/叶子不衰减光（Minecraft 中水每格衰减天空光 2、叶子有 fancy 选项）。光照数据里只有 `opaque` 布尔，没有 0-15 的 opacity 字段；要支持需改 `BlockProperties` + 数据 + 传播算法（衰减系数）。`[Phase 7]`
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

- **水是静态方块** — 无水平扩散/下落/FluidState。`[Phase 3]` Phase 9 实现流体模拟。
- **洞穴是"奶酪洞"** — 单个 3D 噪声阈值雕刻（`caves.threshold`），生成的是团块状空腔，不是 Minecraft 式的 spaghetti 隧道（两条 3D 噪声相交）。形状可用配置调，但拓扑类型要等以后升级。`[Phase 4]`
- **矿脉是简化随机游走** — 从世界空间格网单元出发、每步 27 邻域随机走的短链（`vein_size` 个方块）；不是 Minecraft 的 3×2×1 旋转椭球 vein。跨 chunk 无缝（格网+边际机制），分布由 `ores.veins` 数据驱动。`[Phase 4]`
- **树是固定模板** — 树干 1×N（4~7）+ 两层半径 2 树冠 + 顶部半径 1 帽，无生长阶段、无 BlockState、无树种差异（不同树只有高度随机）。`[Phase 4]`
- **基岩只有单层** — y=0 是 `terrain.bottom_block`（数据里为 bedrock），只有一层厚；y≥1 的石头理论上可被挖穿（Phase 6 前没有挖掘）。`[Phase 4]`
- **地形高度与洞穴/矿/树相互独立** — 洞穴只保留表层 4 格（`surface_guard`）与底层 2 格（`min_y`）的壳，不感知矿脉/树洞；可能出现洞口正对矿脉等巧合。`[Phase 4]`
- **破坏方块不掉落物品** — 破坏只把方块变 air，`properties.drops` 数据存在但不收集（无物品实体/背包）。`[Phase 6]` Phase 11（物品与背包）。
- **选择方块是临时方案** — `assets/data/hotbar.json` 列表 + `Tab` 循环，无 UI 显示（看不到当前选中什么）、无物品概念。`[Phase 6]` Phase 11 换成物品/背包/hotbar UI。
- **无准星/目标高亮** — 破坏/放置靠 raycast 命中，但屏幕上没有准星，也没有 MC 式的目标方块描边；命中判定纯几何（reach 5）。`[Phase 6]` 随 UI（Phase 15）加准星+描边。
- **破坏是瞬时的** — 无破坏耗时/裂纹动画/工具系统；`properties.hardness` 数据已存在但未使用。`[Phase 6]` 随物品/工具（Phase 11）实现。
- **放置到未生成区域会同步生成 chunk** — 演示区边缘之外放置方块会触发 `getOrCreateChunk`（生成器在游戏线程同步跑）。`[Phase 6]` Phase 14（异步 chunk streaming）。
- **无游泳/水体物理** — 水是 `solid:false`，玩家直接穿过去，无浮力/游泳速度/水下视角；水可被方块替换（非 solid 格可放置）但不能破坏（`destroyable:false`）。`[Phase 5]` 随流体（Phase 9）一并处理。
- **无潜行/无 1 格自动台阶** — 上 1 格台阶必须主动跳跃；无 crouch（卡 1.8 门框）、无自动 step-up。`[Phase 5]`
- **水平速度无惯性** — 期望速度每 tick 直接设定（无加速/摩擦），松键即停。手感可用但非 Minecraft 式平滑。`[Phase 5]`
- **虚空坠落 = 传送回出生点** — 世界边缘是虚空（见渲染节），掉到 y < -32 直接回出生点，无死亡/掉落伤害/护甲。`[Phase 5]` 随 chunk streaming（Phase 14）+ 死亡机制处理。
- **无实体/物品/合成/存档** — Phase 10/11/12/13（Phase 10 会把 Player 并入统一 Entity 体系）。

## 性能 / 架构

- **Chunk 网格化在单线程游戏线程** — 演示区所有 chunk 在第一帧同步 mesh；chunk 数量增长后需要 job queue。`[Phase 3]` Phase 14（worker threads）。`WorldGenerator::generateChunk` 已按无状态纯函数设计（Phase 4），可直接并行。
- **`world.view_distance` 配置存在但未使用** — 演示区大小是编译期常量，没有按视距流式加载。`[Phase 3]` Phase 14。
- **`Chunk&` 引用会被 unordered_map rehash 失效** — `getOrCreateChunk` 返回的引用在后续可能 rehash 的 World 变更（其他 chunk 的 setBlock/getOrCreateChunk）后失效；已在 world.hpp 头部文档化，调用方须在一个逻辑操作内使用。`[Phase 3]`
- **Greedy Meshing 未实现** — 只做 Face Culling，顶点数按暴露面线性增长。计划：性能成为瓶颈时再引入（README 路线图已注明）。

## 数据 / 内容

- **方块无 per-instance 状态** — BlockState 概念未引入（朝向、附属、流体等级等需要时再设计）。`[Phase 2]`
- **音频系统未引入** — OpenAL 依赖尚未接入。

## 已修复（归档）

- **`[2026-10-03, Phase 8]` 静态天空色/无昼夜** — Phase 8 实现昼夜循环：`DayCycle`（`src/world/day_cycle.{hpp,cpp}`，确定性游戏内时间）；shader 天空光 × `u_dayFactor`（黎明/黄昏 smoothstep 渐变，夜晚天空光=0，方块光不受影响）；天空背景色关键帧插值（夜蓝→黎明橙→昼蓝→黄昏橙→夜蓝，正午段与 Phase 1-7 天空色逐位一致——保证基线截图不变）；太阳/月亮/300 颗星为像素级圆形 billboard quad（`assets/shaders/sky.{vert,frag}`，Renderer::drawSky，无 u_model）。新增 CLI `--time <0..1>` 与配置 `world.day_length_seconds`（240）/`world.start_time_of_day`（0.25）。
- **`[2026-10-03, Phase 7]` 占位方向光** — 替换为正式光照系统：每 chunk 两个 16³ 字节数组（sky/block light，0-15）；天空光=列顶扫描（最高 opaque 上方=15）+ BFS（垂直无衰减、水平 -1）；方块光=light_emission 源 BFS（全方向 -1）；setBlock 后局部重传播（天空光按整列、方块光按 ±16 盒，区域外读旧值作边界）；渲染改为顶点烘焙按面光值 + MC 经典面着色（顶 1.0/侧 0.8,0.6/底 0.5）+ 3% 环境光下限；shader 移除 u_lightDir 方向光。关键不变量有测试保证：`updateLocal` 与 `propagateAll` 结果逐格一致。
- **`[2026-10-03, Phase 3]` face 表 +Y/-Y 面绕序反了** — 顶面从上方看是顺时针（被 GL 背面剔除，永远看不见），底面从上方可见。Phase 1/2 的截图验证只采样了侧面纹理，掩盖了该 bug；Phase 3 排查渲染异常时发现并修正，新增绕序回归测试（`FaceWindingIsCounterClockwiseFromOutside`）防复发。
