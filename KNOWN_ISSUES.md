# 已知问题清单（Known Issues）

每个阶段完成后把新发现的限制/简化记到这里；修复后移入「已修复（归档）」。
条目格式：**问题** — 影响与现状。`[阶段]` 标注发现/引入阶段，`计划` 标注预计修复阶段。

## 渲染

- **同类非 opaque 方块之间的内部面仍会生成**（水挨水、叶子挨叶子）— 剔除规则 v1 只判断"邻居是否 opaque"，结果正确但多发了面。`[Phase 3]` 计划随流体（Phase 9）细化为"邻居是否为同种流体"。
- **光照为占位方向光**（每顶点 light=1.0，shader 内 diffuse+ambient 0.3）— 无天空光/方块光（0-15）、无阴影。`[Phase 3]` Phase 7 替换为正式光照系统。
- **顶面/底面纹理朝向约定** — Phase 3 修正 Y 面绕序后，顶面纹理的 u 轴指向 +Z、v 轴指向 -X（与 Minecraft"顶面纹顶部朝向 -Z"不同）。占位纹理下无视觉影响；正式纹理阶段可统一调整。`[Phase 3]`
- **世界边缘即"虚空"** — 缺失 chunk 读作 air，没有雾效/边缘遮挡，站在世界边缘能看到悬空截面。`[Phase 3]` 计划随 chunk streaming（Phase 14）处理。

## 世界 / 生成

- **水是静态方块** — 无水平扩散/下落/FluidState。`[Phase 3]` Phase 9 实现流体模拟。
- **洞穴是"奶酪洞"** — 单个 3D 噪声阈值雕刻（`caves.threshold`），生成的是团块状空腔，不是 Minecraft 式的 spaghetti 隧道（两条 3D 噪声相交）。形状可用配置调，但拓扑类型要等以后升级。`[Phase 4]`
- **矿脉是简化随机游走** — 从世界空间格网单元出发、每步 27 邻域随机走的短链（`vein_size` 个方块）；不是 Minecraft 的 3×2×1 旋转椭球 vein。跨 chunk 无缝（格网+边际机制），分布由 `ores.veins` 数据驱动。`[Phase 4]`
- **树是固定模板** — 树干 1×N（4~7）+ 两层半径 2 树冠 + 顶部半径 1 帽，无生长阶段、无 BlockState、无树种差异（不同树只有高度随机）。`[Phase 4]`
- **基岩只有单层** — y=0 是 `terrain.bottom_block`（数据里为 bedrock），只有一层厚；y≥1 的石头理论上可被挖穿（Phase 6 前没有挖掘）。`[Phase 4]`
- **地形高度与洞穴/矿/树相互独立** — 洞穴只保留表层 4 格（`surface_guard`）与底层 2 格（`min_y`）的壳，不感知矿脉/树洞；可能出现洞口正对矿脉等巧合。`[Phase 4]`
- **无方块交互** — 玩家已能行走/跳跃（Phase 5），但不能破坏/放置方块（无 raycast）。Phase 6。
- **无游泳/水体物理** — 水是 `solid:false`，玩家直接穿过去，无浮力/游泳速度/水下视角。`[Phase 5]` 随流体（Phase 9）一并处理。
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

- **`[2026-10-03, Phase 3]` face 表 +Y/-Y 面绕序反了** — 顶面从上方看是顺时针（被 GL 背面剔除，永远看不见），底面从上方可见。Phase 1/2 的截图验证只采样了侧面纹理，掩盖了该 bug；Phase 3 排查渲染异常时发现并修正，新增绕序回归测试（`FaceWindingIsCounterClockwiseFromOutside`）防复发。
