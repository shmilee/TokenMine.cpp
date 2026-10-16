# Phase 96 细节存档

> 范围（用户"下一阶段选建议1" = P95 下一步建议 1：**墙连接（贴块延伸/栅栏
> 顶，MC 墙语义扩展，消费 P95 偏离）**，代理定案为最小完整版：**合并墙
> 几何无缝**——emitWall 侧面连接剔除。MC 式延伸板对全柱墙不适用（柱已贴
> 邻块，延伸板在柱体内不可见）= 文档化偏离）。

## 阶段报告

- **`[2026-10-16, Phase 96]`** 墙连接 — 1166/1166（+3），`-Werror` 恒 0
  警告；E2E 批跑全绿（无输入；S1 第 69 次结构性免疫 + S2 墙线【3 并墙 +
  端贴石 ×2 RAW】+ S2b 单墙对照【像素 diff 210808 px】）。

### 实现内容

**emitWall 侧面连接剔除**（`src/world/chunk_mesher.cpp`，纯 mesh 正确性，
零 format/零内容/零 parse 面）：
- **墙-墙**：共享侧面整面剔除（两柱皆 y 0..1.5 实心 = 完全内部面）→ 并墙
  线连续、无共面双绘。
- **墙-满不透明块**（opaque + FullCube）：块面朝墙早被贪婪路径剔除（墙
  opaque）→ 墙的 y 0..1 段与该被剔面共面 = 内部面，**只绘暴露帽带
  y 1..1.5**（全侧面纹理压入帽带 = emitSlab 的 h 范式）。
- **墙-半块（slab/stairs）/透明（玻璃）/空气**：保守整面 y 0..1.5（slab
  只盖 y 0..0.5 → 下段有暴露区；玻璃走透明 pass 混合 → 无同 pass 共面
  争）。
- 实现 = 每面局部垂直区间 [yBase, yBase+yScale]（默认 0..1.5；帽带
  1.0..1.5；底/顶帽面 yScale 无关）。

**修正 P95 文档误判**：P95 KNOWN_ISSUES 记录的"墙邻满块同层侧面 Z-fight"
不存在——贪婪满块路径对 opaque 邻居（含墙）已剔满块面，无共面双绘；真正
的 P95 残留 = 墙侧面内部段双绘（隐藏面），P96 修复。

**测试（1163 → 1166，+3，`tests/test_chunk_mesher.cpp`）**：
- `Phase96WallAirKeepsFullSideFaces`：单墙 6 面 24 顶点 + 空气邻居侧面全
  高（y 4..5.5）基线。
- `Phase96WallWallCullsSharedFace`：2 并墙 = 40 顶点（24+24−8）+ 仅剩 -X
  在 x=8 / +X 在 x=10（共享面双剔）。
- `Phase96WallFullBlockEmitsCapBandOnly`：墙+石 = 44 顶点（24+20，石 -X
  剔除）+ 石 -X 面消失 + 墙朝石侧面 = 帽带 4 顶点 y∈[5.0,5.5] + 墙空气侧
  仍全高。
- 测试注册表加 `wall` 块。

### E2E（全绿；批跑脚本 `/tmp/opencode/p96_e2e.zsh`，**无输入**）

- **S1 第 69 次结构性免疫**：纯墙 mesh 变更 + 墙永不生成 = fresh 世界
  emitWall 零次运行 → base18 字节一致。
- **S2 墙线**：3 并墙 (22/23/24,45,-2) + 端贴石 (25,45,-2)，玩家
  (22.5,45,0.5) yaw 0 pitch −0.25 正前方俯看——玩家侧（+Z，空气邻居）必
  绘、内部共享面必剔：600f ×2 RAW 字节一致 = 合并线渲染连续无过度剔除。
- **S2b 单墙对照**：单墙 (22,45,-2) ×2 RAW + S2 vs S2b 像素 diff =
  **210808 px**（∈(20000,400000)）= 连接线的额外几何可见（并墙未被剔没）。
- **无 S6**【零 parse 面】。

### 变更文件

- `src/world/chunk_mesher.cpp`（emitWall 侧面连接剔除 + yBase/yScale）
- `tests/test_chunk_mesher.cpp`（+3 + 注册表 wall 块）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`（墙段修正 + 消费）、
  `docs/MECHANICS.md`、`AGENTS.md`、`docs/archive/phase-96.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 墙段第 2 条改写："墙邻满块 Z-fight"（P95 误判）→ "墙侧面
  vs 半块/透明邻居的保守整面"（真实残留 + 正确暴露说明）。

### 下一步

- 候选（SESSION_CONTEXT「下一步」）：① 楼梯放置自动朝向（按玩家 yaw 设
  per-instance 朝向，消费 P94"无放置自动朝向"偏离，零 format）；② fence
  （连接型第四模型：横向交叉面片 + 墙/块连接，MC fence 语义）；③ 玻璃板
  （透明连接变体，透明 pass 版墙连接）。
