# Phase 9 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 9 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-04, Phase 9]` 透明 pass 实际渲染为不透明** — basic.frag 硬编码 `alpha=1.0`、只取纹理 `.rgb`，导致"透明 pass"里的水/玻璃/叶子全部不透明（树叶纹理里的 20 个透明孔被渲染成纯黑斑点；水看不见湖底）。改为取纹理 `texel.a`（图集本身是 RGBA，alpha 一直保留在 GPU 纹理里）。副作用：截图 PNG 的 alpha 通道曾写入 framebuffer 混合残留值（水面 α≈0.831），`takeScreenshot` 现在强制 α=255（截图是不透明画面记录）。`[Phase 2]` 引入、`[Phase 9]` 发现并修复。
- **`[2026-10-04, Phase 9]` 破坏 1 格深湖底后流体列永久振荡** — 落水满格上方的邻居 7 级膜"坐在满水上被吸收"（7→0），下一 tick 邻居再补 7，7↔0 每 tick 振荡、永不平衡（E2E 探针 330 个 tick 后仍振荡）。修复：`FluidSim` 坐在**满水**上的格子补满到 8（有 source 托底的柱是满的——MC 行为），仅"坐在流动（非满）池上"才走吸收/抬升规则。回归测试 `FilmAboveFullWaterRefillsToFull` + `BrokenLakeFloorRefillsAndSettles`。
- **`[2026-10-04, Phase 9]` 水是静态方块/无游泳** — Phase 9 实现完整流体：per-chunk 0-8 等级数组（8=满/源，1-7 流动；cell 里水与方块互斥），`World::setFluid`（只写已有 chunk、只写 air 格）/`setBlock` 排水；`FluidSim::tick`（快照式、下落传送带、水平扩散 L-1、1 级终端、满水托底补满、max-merge 冲突合并、确定性）游戏线程 20Hz（每 3 帧一次）；玩家游泳（水中重力 12/终端 4/水平 3.1/Space 上浮 4.5，AABB 与流体格相交判定）；光照水衰减（天空光每格 -3，列扫描+BFS）；网格化 `emitFluid`（level/8 缩放立方体、水-水剔面、流体面烘焙自身格光）；世界生成湖改填 fluid 8（`worldgen.json` 移除 `water_block` 键，water 数据方块仅作纹理参考经 `BlockRegistry::fluidBlock()`）。

## 会话存档（原 SESSION_CONTEXT）

- `src/world/fluid.{hpp,cpp}`：FluidSim::tick（规则见核心架构节）。性能：per-chunk raw
  数组 + 6 个缓存邻居指针，演示区规模单 tick <1ms（20Hz 游戏线程跑）。
- 存储：`Chunk::m_fluid`（16^3 字节）+ setFluidAt/fluidAt/clearFluid/fluidData；
  `World::setFluid`（**不创建 chunk**；只写 air 格；越界/非 air/值不变→false；标
  self+面邻居 dirty）/`getFluid`；`World::setBlock` 把目标格 fluid 清 0（排水）。
- 光照：`Lighting::updateRegion(min,max)`（updateLocal 委托之）；水衰减
  kFluidLightAttenuation=3：列顶扫描 enterCost（遇水每格 -3）+ sky BFS 入水 -3。
  开水面列：水面格恒 12（15-3）；深度 d 的湖底光 = max(0, 15-3×(d+1))。
- 网格化：`ChunkMesher::emitFluid`（transparent pass）；`BlockRegistry::fluidBlock()`
  （第一个 properties.fluid 的数据方块，作纹理参考；air 格+fluid>0 → 用它的纹理）。
- 世界生成：湖=fluid 8（block 留 air）；`worldgen.json` 移除 `water_block` 键
  （WorldgenParams 同步删字段）；生成即平衡（tick 不动湖面——基线不变的原因之一）。
- Game：`kFluidTickEveryNFrames=3`（game.cpp 匿名命名空间，挨着 kDayStepPerFrame）；
  update() 里 handleInteraction 之后：`m_frameCount % 3 == 0` → FluidSim::tick →
  净变化非空 → min/max 盒 → Lighting::updateRegion（在 chunkCount/propagateAll 检查前）。
  m_frameCount 在 update 末尾自增 → tick 发生在第 0,3,6... 帧。
- 游泳：Params 加 swimSpeed 3.1 / swimUpSpeed 4.5 / waterGravity 12 /
  waterTerminalVelocity 4；`Player::isInWater()`（AABB 与 fluid>0 格相交，每 update
  判一次）；step(h,input,inWater) 水中分支（Space 随时上浮含贴地；冲刺无效）。
- **shader 修复**（basic.frag）：`fragColor = vec4(base*light, texel.a)`（此前 alpha
  恒 1.0）。修复了 P2 以来的潜在 bug：水/玻璃/叶子全不透明 + 树叶 20 个透明孔渲染成
  纯黑斑点（默认视角基线因此变化——**P9 新基线**，不再与 P8 shot7_base 一致）。
- **流体 bug 修复**（同阶段发现）：坐在满水上的流动格被"吸收"→破坏 1 格深湖底后
  该列 7↔0 永久振荡（探针 330 tick 后仍在振荡）。修复：坐在满水上→补满到 8。
- takeScreenshot：强制 PNG alpha=255（水面像素曾带 framebuffer 混合残留 α≈0.831）。
- 验证：
  - 310/310 测试；60 帧默认种子基线两次运行+clean rebuild 逐字节一致（新基线）。
  - 水渲染 E2E：seed 29（湖在默认 -Z 视角：130 柱、水面平面 y=35）60 帧截图，
    34k 水像素；半透明（湖底透出水色）、深度渐变（浅 (55,117,167)→深 (24,79,117)）。
  - 水下破坏 E2E（break_e2e2.zsh：锁→W 走 2.2s→dy=520 直下→点击，准星浅水色验收
    重试环）：A/B 两 run 逐像素一致除 1×1 破坏列（~280×312px 更暗矩形，准星处
    Δ≈(-9,-9,-6) ≈ 预测湖底光 12→9 的 (-9.2,-8.6,-6.8)）；玩家未掉坑（AABB 跨
    两 cell、另一 cell 仍支撑——物理正确）。
- E2E 配方（seed 29 浅水边缘）：spawn (0.5,36,0.5) h=35；平路到 z=-4.5（1 格落台
  h=34 干滩）→ z=-6.5 入水（h=33，1 格深，平到 z=-10.5）→ 2 格悬崖（h=31）以下渐深。
  W 走 2.2s 终点 z≈-8.2（平带内）；直下 pitch=-1.55（dy=520，kMaxPitch=1.5533）；
  准星=脚下地板（dist 1.62<reach 5）。
- 水色事实：水面（顶面）光 12 → 水 (47,119,189)×0.806×0.784 + 湖底×0.216；
  深区湖底光 0 → 纯水色×0.806×0.784 ≈ (30,75,116)。
