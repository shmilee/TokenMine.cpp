# Phase 99 细节存档

> 范围（用户"下面三个phase 是建议1 、 建议2 和建议3" = 建议 3，代理定案
> 为最小完整版：**玻璃板 glass_pane = 第五个新块模型类型：1/16 薄片 +
> 透明 pass 连接（Phase 96 连接剔除的透明版）+ 可跳实心 1.0**。E2E 跳跃
> 对照场景首触发并修复两处 P95/P98 潜伏物理缺陷 = 本阶段附带加固）。

## 阶段报告

- **`[2026-10-16, Phase 99]`** 玻璃板 — 1184/1184（+8），`-Werror` 恒 0
  警告；E2E 批跑全绿（S1 第 72 次结构性免疫 + S2 板排【像素 diff
  103024 px】+ S3 跳过板 + S3b 墙对照【1.0 可跳 vs 1.5 不可跳互斥】+
  S6 纹理）。

### 实现内容

**新块模型类型（`src/block/block.hpp` + `block_registry.cpp`）**：
- `BlockModel::Shape::GlassPane` + "glass_pane" 形状解析（错误信息 7
  形状词汇表）。
- 单 side 纹理纪律（fence/cross 同款）：`model.textures.side` 必需，
  top/bottom/cross 键拒绝。
- **强制属性**（cross 纪律同款）：`opaque: true` 拒绝（"light passes
  through the pane"）；`transparent` 必需（blending pass）。solid 允许
  （玻璃板挡移动，与 cross 不同）。

**内容（`assets/`）**：
- `blocks/glass_pane.json`：shape glass_pane、hardness 0.3（MC 玻璃）、
  自掉、non-opaque + transparent。
- `recipes/glass_pane.json`：shapeless **8 glass → 16**（MC 配方）。
- 纹理：`generate_block_textures.py` 加 `glass_pane(x,y)`（1px 白框 +
  半透明芯 alpha 90 + 两条高光 streak；扁平构造无需 salt；既有纹理确定
  性再生字节零漂移）；atlas 空位 **[8,3]**（76 tiles，无 UV 移位免重锚）。

**mesh（`src/world/chunk_mesher.{hpp,cpp}`）**：
- `emitGlassPane`：z 居中 **1/16 厚 1×1 玻璃片**（z 0.46875..0.53125，
  y 0..1）——两面大面恒绘（双面透明 pass，cross 惯例：自身格光、无邻居
  剔除）+ **四条边细 rim**：
  - **rim 朝玻璃板邻居剔除**（X 方向板排：两片共面 rim 双绘 = 透明 pass
    暗缝——Phase 96 连接剔除的透明版）；
  - 顶/底 rim 朝 opaque 邻居剔除（块上/块下遮没）；
  - 侧 rim 对 opaque 邻居保留（不同 pass 无共面争）。
  - X 方向板排每片全宽共面 = 连续单片（每片自己的全宽 sheet 与邻居共面
    相接）。
- 形状分发（slab pass）+ 贪婪路径跳过 GlassPane。

**碰撞（`src/entity/entity.cpp`）**：
- `solidTopHeight` 对 GlassPane 返回 **1.0**——整格实心、**可跳**（跳高
  ~1.129 > 1.0；墙/栅栏 1.5 不可跳）。1/16 薄片只是 mesh。

**物理加固 ×2（P95/P98 潜伏、本阶段 E2E 首触发；`src/player/player.cpp`
+ `src/entity/entity.hpp`）**：
1. **stepUp "真 1 格台阶"门**：前方格判定从 `isSolidCell`（格 solid
   属性）改为 `solidTopHeight ≤ 1.0`——原判定对墙/栅栏（实心区 1.5、上方
   格空气）放行：脚被抬 +1.0 钻进帽内（46 < 46.5），随后跳跃从脚 46 起
   跳（+1.125 > 46.5−46）翻越 1.5 barrier（S3b 首轮 50% 翻越率 = 跳时
   抖动决定起跳脚位）。楼梯/台阶（top<1.0）仍可踏步（行为不变）；楼梯
   低端斜面接触（top≈0）不再误踏步（顺带修正）。`solidTopHeight` 从
   private 改 protected（stepUp 需同款实心区高度）。
2. **跳跃冲量计入子步 maxSpeed**：子步数的 `maxSpeed` 估计原用**跳跃
   前** vy（=0）→ 长帧（dt≈0.086 s，流送卡顿）只切 1 子步，整帧按
   8.5 b/s 跳速位移 0.73 块（> 0.4 子步上限，违反硬规则 #6）→ 跳跃顶
   点从 1.129 膨胀到实测 **1.649**，翻过 1.5 墙（帧率依赖物理：60fps
   稳态翻不过、卡帧翻得过——同场景两 run 一个停 -0.7 一个到 -6.4 即
   此）。修复：pending 跳跃（onGround && !sitting && 非流体 && space）
   时 `maxSpeed = max(maxSpeed, jumpSpeed)` → 长帧切 2 子步，顶点 ≈1.16
   稳定 < 1.5。

**测试（1176 → 1184，+8）**：
- test_block：`Phase99GlassPaneShapeParsed` / `Phase99GlassPaneRequires
  SideTexture` / `Phase99GlassPaneForcesNonOpaque` / `Phase99GlassPane
  ForcesTransparent` / `Phase99GlassPaneContent`（真实资产钉死）。
- test_entity：`Phase99GlassPaneIsJumpableSolid`（顶 y+1.0 有房、体内/
  半高无房——顶位 3.0 比墙/栅栏的 3.5 低 0.5 = 可跳高度）。
- test_chunk_mesher：`Phase99LoneGlassPaneEmitsSheetAndRims`（6 quads
  = 24 透明顶点，sheet 平面 z 精确 8.53125 钉死）/ `Phase99GlassPane
  PairCullsSharedRim`（10 quads = 40 顶点，共享边界 rim 剔除——±X rim
  只剩 8 顶点 = 两个外端）。

**PITS +2**：stepUp 格 solid 判定踏墙帽 / 长帧跳跃顶点膨胀（口诀见
`docs/PITS.md`）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p99_e2e.zsh`）

- **S1 第 72 次结构性免疫**：玻璃板永不生成 + 默认背包无 + atlas 空位
  [8,3] 无 UV 移位 → base18 字节一致。
- **S2 板排（无输入）**：3 并板 (22/23/24,45,-2) 600f ×2 RAW 字节一致 +
  vs 单板像素 diff = **103024 px**（板排的额外几何可见 = 连续片未被剔
  没）。
- **S3 跳过板（输入驱动，walk + 单跳）**：越过 1.0 实心（跳上板顶/越过
  → 落板后），终态 z≈-6.3（踏上 y46 台阶），×2 run——**1.0 可跳证明**。
- **S3b 墙对照（输入驱动，同一跳输入 vs 1.5 墙）**：精确停 **z=-0.7**
  ×2 run——**1.5 不可跳证明**；与 S3 互斥 = 可跳高度 + 两处物理加固的
  集成验证（加固前该场景 50-100% 翻墙）。
- **S6 init 失败**：scratch glass_pane.json 缺 side 纹理 → "model.
  textures.side is required" 点名。

**调试轨迹（本阶段最大收获）**：S3b 首轮"墙被跳过"违反物理（跳高
1.129 < 1.5）→ 无窗口 Player 探针（真实资产 + 模拟输入）证明 60fps 稳
态正确 → 临时子步级插桩（stderr）实测定长帧 86ms 跳跃顶点 1.649 →
两处物理缺陷定位（stepUp 帽 + 长帧跳跃）→ 修复 + 插桩全撤 + 全量回归。

### 变更文件

- `src/block/block.hpp`（Shape::GlassPane）、`src/block/block_registry.cpp`
  （"glass_pane" 解析 + 单 side 纹理 + 强制属性）
- `src/entity/entity.{hpp,cpp}`（solidTopHeight protected + GlassPane
  1.0）
- `src/world/chunk_mesher.{hpp,cpp}`（emitGlassPane + 分发 + 贪婪跳过）
- `src/player/player.cpp`（stepUp 真 1 格台阶门 + 跳跃冲量计 maxSpeed）
- `tools/generate_block_textures.py`（glass_pane 纹理）
- `assets/textures/blocks/glass_pane.png`、`assets/data/textures/
  block_atlas.json`（[8,3]）、`assets/data/blocks/glass_pane.json`、
  `assets/data/recipes/glass_pane.json`
- `tests/test_block.cpp`（+5 + 2 既有适配）、`tests/test_entity.cpp`
  （+1 + fixture）、`tests/test_chunk_mesher.cpp`（+2 + fixture）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/PITS.md`（+2）、`AGENTS.md`、`docs/archive/phase-99.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 新增「玻璃板 glass_pane（Phase 99）」段：1/16 薄片可跳
  实心 1.0 / rim 连接剔除（透明版 P96）/ 放置自由 + rim 简化偏离；
  「跳跃/踏步物理加固（P99 修）」条目（两处修复说明）。

### 下一步

- 候选（SESSION_CONTEXT「下一步」）：① 苔石/黑曜石/链栅栏族（数据驱动
  fence 变体，纯内容）；② 玻璃板/栅栏族纹理材质扩展；③ 楼梯 4 变体退役
  评估（P94 per-instance 朝向已使变体冗余——内容契约变更，需调研审计）；
  ④ 世界生成引入自然结构（如石英簇）打破"出生点视野内全草地"基线。
