# Phase 98 细节存档

> 范围（用户"下面三个phase 是建议1 、 建议2 和建议3" = 建议 2，代理定案
> 为最小完整版：**栅栏 fence = 第四个新块模型类型：交叉细柱 + 连接横杆，
> 1.5 barrier**（碰撞同墙，mesh 为 MC 栅栏的细柱+栏杆；横杆只朝栅栏/满
> 块，无 MC 栅栏顶放置规则/材质族 = 文档化偏离））。

## 阶段报告

- **`[2026-10-16, Phase 98]`** 栅栏 — 1176/1176（+9），`-Werror` 恒 0
  警告；E2E 批跑全绿（无输入+输入驱动；S1 第 71 次结构性免疫 + S2 栅栏线
  【像素 diff 223100 px】+ S3 barrier【精确停 z=-0.7】+ S3b 对照 + S6
  纹理）。

### 实现内容

**新块模型类型（`src/block/block.hpp` + `block_registry.cpp`）**：
- `BlockModel::Shape::Fence` + "fence" 形状解析（错误信息 6 形状词汇表）。
- **单 side 纹理纪律**（cross 同款 ammo 纪律）：`model.textures.side`
  必需非空；top/bottom/cross 键拒绝。

**内容（`assets/`）**：
- `blocks/fence.json`：shape fence、hardness 2.0（MC 木栅栏）、自掉、
  opaque（挡光如满块）。
- `recipes/fence.json`：shaped **PSP/PSP**（4 planks + 2 stick → 3 = MC
  配方）。
- 纹理：`generate_block_textures.py` 加 `fence(x,y)`（木板底 salt 933 +
  对角 X 撑盐 934；既有纹理确定性再生字节零漂移——git status 仅新增
  fence.png）；atlas 空位 **[8,2]**（75 tiles，无 UV 移位免重锚）。

**mesh（`src/world/chunk_mesher.{hpp,cpp}`）**：
- `emitFence`：两根 1/8 厚交叉柱（x 柱 z∈[0,1] + z 柱 x∈[0,1]，皆 y
  0..1.5，6 面 box——柱顶/底邻 opaque 剔除 = 栅栏叠栅栏）+ **连接横杆**
  （朝每个水平**栅栏/满不透明块**邻居：1/8 厚、全高、从柱外侧面到格
  边界，**4 面开两端**——边界端与邻居横杆在格边界相接成连续栏杆、柱端
  与柱面共面 = 端帽会共面双绘故省）；自身格光（cross 惯例——细柱读邻居
  光会在线缝闪烁）；kCubeFaces 绕序/UV 约定逐面复制（面表回归测试守护
  的通用约定）。
- 形状分发 + 贪婪路径跳过 Fence。
- **贪婪剔面修正**：满块面**不因栅栏邻居被剔**（`opaque && shape !=
  Fence`）——细柱不能遮挡整面（P85 slab 同款"半面"问题的反向处理：slab
  opaque 剔面留上半洞、栅栏不剔保面）。

**barrier 碰撞（`src/entity/entity.cpp`）**：
- `solidTopHeight` 对 Fence 返回 **1.5**——与墙同款（细柱只是 mesh，整格
  1.5 实心：不能穿过也不能跳过）；P95 的 y0-1 扫描扩帽逻辑 generic（top>1
  即生效）零改动。

**测试（1167 → 1176，+9）**：
- test_block：`Phase98FenceShapeParsed` / `Phase98FenceRequiresSideTexture`
  / `Phase98FenceRejectsCubeTextureKeys` / `Phase98FenceContent`（真实资产
  钉死）；既有形状拒绝测试适配（"fence" 已合法 → 改拒 "pane"）。
- test_entity：`Phase98FenceIsABarrier`（栅栏顶有房、墙体/帽内无房）。
- test_chunk_mesher：`Phase98LoneFenceEmitsPosts`（48 顶点 = 12 quads）/
  `Phase98FencePairEmitsBars`（128 = 16 quads × 2）/
  `Phase98FenceBesideBlockKeepsBlockFace`（88 = 栅栏 64 + 石 24，石 -X 面
  保留——被剔则 84）/ `Phase98FenceNoBarTowardWall`（72 = 无横杆 + 墙 24）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p98_e2e.zsh`）

- **S1 第 71 次结构性免疫**：栅栏永不生成 + 默认背包无 + atlas 空位 [8,2]
  无 UV 移位 → base18 字节一致。
- **S2 栅栏线（无输入）**：3 并栅栏 (22/23/24,45,-2) + 端贴石 (25,45,-2)
  （横杆 hook 到石块 + 石块面保留可见）600f ×2 RAW 字节一致 + vs 单栅栏
  像素 diff = **223100 px**（连接线的额外几何可见）。
- **S3 barrier（输入驱动）**：hold w 1.5 s 朝栅栏 (22,45,-2) → 精确停
  **z=-0.7**（墙同款整格 1.5 实心），×2 run 一致。
- **S3b 无栅栏对照（输入驱动）**：同 hold → 走到 z≈-6.4（stepUp 踏
  z=-6 台阶）∈(-6.5,-4.5) 与 -0.7 互斥。
- **S6 init 失败**：scratch fence.json 缺 side 纹理 → "model.textures.side
  is required" 点名。

### 变更文件

- `src/block/block.hpp`（Shape::Fence）、`src/block/block_registry.cpp`
  （"fence" 解析 + 单 side 纹理纪律）
- `src/entity/entity.cpp`（solidTopHeight 1.5）
- `src/world/chunk_mesher.{hpp,cpp}`（emitFence + 分发 + 贪婪跳过 + 栅栏
  不剔面修正）
- `tools/generate_block_textures.py`（fence 纹理 salt 933/934）
- `assets/textures/blocks/fence.png`、`assets/data/textures/block_atlas.json`
  （[8,2]）、`assets/data/blocks/fence.json`、`assets/data/recipes/fence.json`
- `tests/test_block.cpp`（+4 + 2 既有适配）、`tests/test_entity.cpp`（+1 +
  fixture）、`tests/test_chunk_mesher.cpp`（+4 + fixture）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `AGENTS.md`、`docs/archive/phase-98.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 新增「栅栏 fence（Phase 98）」段：交叉细柱 + 横杆 / 1.5
  barrier（同墙）/ 横杆连接偏离（只朝栅栏/满块、无栅栏顶放置规则、木质
  单纹理）。

### 下一步

- P99 = 玻璃板（建议 3，透明连接变体：1/16 薄片 + 透明 pass + 同行无缝
  连接【P96 版连接剔除的透明版】+ solid 1.0 可跳过）。
