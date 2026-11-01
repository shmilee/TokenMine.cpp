# Phase 7 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 7 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-03, Phase 7]` 占位方向光** — 替换为正式光照系统：每 chunk 两个 16³ 字节数组（sky/block light，0-15）；天空光=列顶扫描（最高 opaque 上方=15）+ BFS（垂直无衰减、水平 -1）；方块光=light_emission 源 BFS（全方向 -1）；setBlock 后局部重传播（天空光按整列、方块光按 ±16 盒，区域外读旧值作边界）；渲染改为顶点烘焙按面光值 + MC 经典面着色（顶 1.0/侧 0.8,0.6/底 0.5）+ 3% 环境光下限；shader 移除 u_lightDir 方向光。关键不变量有测试保证：`updateLocal` 与 `propagateAll` 结果逐格一致。

## 会话存档（原 SESSION_CONTEXT）

- `src/world/lighting.{hpp,cpp}`：`kMaxLight=15`、`kLightUpdateRadius=16`。
  - 传播模型（MC 简化）：二值 opacity（opaque 挡光，水/玻璃/叶子透光）；
    sky=列顶扫描（最高 opaque 上方连续非 opaque 段=15，下方=0 待 BFS）+
    BFS（垂直 0/水平 -1）；block=emission 源（源自身保留 emission 值即使 opaque）
    + BFS（全方向 -1）；void=sky15/block0 边界源。
  - `propagateAll(world, blocks)`：全量（buildScene 一次 + 每出现新 chunk 重跑；
    全部标 dirty）。`updateLocal(world, blocks, changed)`：sky 整列（±16 足迹）
    + block 盒（±16），保存-比较净变化标 dirty；每块操作 ~几 ms（游戏线程）。
- Chunk 加 skyLight/blockLight 数组 + 访问器 + raw data()；World 加
  getSkyLight/getBlockLight/findChunkMut。
- 渲染：MeshVertex `light`→`skyLight+blockLight`（2 float，40 字节；mesh.cpp
  VAO location 3/4）；mesher 每面采样"面后方一格"（blockPos+face normal）烘焙
  到 4 顶点；shader 移除 u_lightDir，`max(sky,block)×faceShade(顶1.0/±X0.8/±Z0.6/
  底0.5)+3% 环境光`（faceShade 在顶点着色器按法线算）。
- Game：buildScene 生成后 propagateAll+记录 `m_lastLightedChunks`；update() 每帧
  查 chunkCount 变化→重传播（新 chunk 零光，必须在 mesh 前）；breakBlock/
  placeBlock 的 setBlock 后 updateLocal。
- 数据：glowstone（emission 15，黄亮纹理）入 hotbar；14 数据方块/16 图集瓦片。
- 17 个 LightingTest：虚空查询 / 天空光（顶扫/地面柱/水平衰减+垂直无衰减精确值/
  水透射）/ 方块光（Manhattan 衰减/全高遮挡+矮墙绕行精确值/弱源范围 5）/ 跨
  chunk×2（block 边界格、sky 洞穿两 chunk）/ propagateAll 幂等 /
  **updateLocal==propagateAll 逐格一致×4**（拆 glowstone/放石/放 glowstone/
  远处源+近处变化）/ dirty 追踪（变化标脏/幂等不脏/密封腔内拆 air 不脏）/
  mesher 烘焙（密封暗腔内面=0、开阔地面=1.0）。
- E2E（xdotool）：锁定+俯视+两连击破坏出 2 格深洞（泥土壁可见、洞底正常受光
  非黑屏）+ Tab×7 右键放 glowstone（亮黄 25k 像素，纹理色 (255,228,113) 全亮）。
  注意：pitch -1.0 时准星在前方地面，glowstone 落在洞旁平地（瞄准问题，非 bug）。
- 验证基线：seed 123456789 → 15 blocks/16 tiles/128 chunks、spawn (0,0) y=53；
  同 seed 截图逐字节一致（含 clean rebuild）；260/260 测试。
