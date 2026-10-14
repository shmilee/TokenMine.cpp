# Phase 85 细节存档

> 范围（用户"按顺序将三个建议作为接下来的三个 phase" = 建议 2，代理定案
> 为最小完整版：**台阶 slab = 首个新块模型类型**（底部半块，高度感知碰撞
> + 半盒 mesh））。

## 阶段报告

- **`[2026-10-15, Phase 85]`** 台阶 slab — 1140/1140（+4），`-Werror` 恒
  0 警告；E2E 批跑全绿（无输入站立判别；S1 第 58 次结构性免疫 + S2 slab
  【pos.y 46.5】+ S2ctl 满块对照【pos.y 47】+ S6 init 失败）。

### 实现内容

**新块模型类型 `Shape::Slab`（底部半块，y 0..0.5 实心）**：
- `block.hpp`：`BlockModel::Shape::Slab`（FullCube/Cross 之外的第三形状）。
- `block_registry.cpp`：shape 解析加 `"slab"`（cube 纹理 top/side/bottom）。

**chunk mesher（`src/world/chunk_mesher.{hpp,cpp}`）**：
- `emitSlab`（新）：单位立方 6 面 **Y 缩 0.5**（emitFluid 的 h 范式）——
  **顶面恒绘**（cell 顶半格是空气，无遮挡）、底/侧面邻 opaque 剔除；进
  OPAQUE 子网格。
- `mesh()` 加 slab pass（Cross pass 之后、贪婪 pass 之前，固定 cell 序）。
- `meshFaceDirection`（贪婪满块路径）**跳过 slab**（slab 不是满格几何）。
- 局限：slab 邻满块同层侧面——贪婪 mesh 不能"半面剔除"→ v1 Z-fight/缺面
  （shipped 场景 slab 空邻居隔离，无此问题；文档化）。

**高度感知碰撞（`src/entity/entity.{hpp,cpp}`）**：
- `Entity::solidTopOffset(id)`（新）：返回实心区顶（air/非实心 = 0、slab =
  0.5、满块 = 1.0）。
- `moveAxis` + `hasRoomAt`：对**实心区 [y, y+top]**（非整格）判重叠 + Y
  面用 slab 顶/底（delta<0 → y+top 站立、delta>0 → y 撞头）——玩家站 slab
  顶（feet y+0.5）、不进 slab 体。**满块 top=1 复现旧的整格行为 =
  backward-compat**（无 slab 的世界碰撞零变化 = base18 免疫）。

**内容（1 新块 + 1 配方，复用 stone 纹理无新 atlas）**：
- `stone_slab.json`：shape slab、hardness 2.0、自掉、solid/opaque、**复用
  stone 纹理**（无新 atlas tile）。
- `stone_slab` 配方：shaped **SSS → 6 stone_slab**（MC 台阶同款）。

**测试（1136 → 1140，+4）**：
- `Phase85StoneSlabContent`（真实资产：shape Slab / hardness 2.0 / 自掉 /
  stone 纹理）+ `Phase85SlabShapeRequiresCubeTextures`（slab 用 cube 纹理，
  cross 纹理拒绝）。
- `Phase85ItemRestsOnSlabTop`（实体坠落停在 slab 顶 y=2.5）+
  `Phase85SlabHasRoom`（站 slab 顶有空间 / 进 slab 体无空间 / 满块对照）。
- `RejectsUnsupportedShapeValue` 契约演进（slab 现支持 → 改用 "stairs"）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p85_e2e.zsh`，**无输入站立判别**）

- **S1 第 58 次结构性免疫**：slab 永不生成 + 默认背包无 + fresh 无 slab =
  solidTopOffset 对满块恒 1.0（碰撞零变化）+ 无半盒 mesh → base18 字节一
  致。
- **S2 slab**（600f no-input ×2 RAW 截图对 + ×2 savenorm + EXACT）：
  stone_slab@(22,46,0)（空邻居隔离），玩家预置 feet **46.5**（slab 顶）→
  静止 → **pos.y = 46.5**（非满格顶 47、非穿落 45）= 高度感知碰撞证明。
- **S2ctl 满块对照**（同位 stone 满块，玩家 feet 47 → **pos.y = 47**）——
  **46.5 vs 47 互斥**（slab 顶低半格）。
- **S6 init 失败**：scratch stone_slab 缺 bottom 纹理 → "model.textures.
  bottom is required" 点名。
- PITS 复用：600f（chunk 流送落定 P78）+ shot_pair WM flake 重试（P64）。

### 变更文件

- `src/block/block.hpp`（Shape::Slab）、`src/block/block_registry.cpp`
- `src/world/chunk_mesher.{hpp,cpp}`（emitSlab + pass + 贪婪跳过）
- `src/entity/entity.{hpp,cpp}`（solidTopOffset + hasRoomAt + moveAxis）
- `assets/data/blocks/stone_slab.json`（新）、
  `assets/data/recipes/stone_slab.json`（新）
- `tests/test_block.cpp`（+2、1 改写）、`tests/test_entity.cpp`（+2 + slab
  进 makeBlocks）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`AGENTS.md`、`docs/archive/phase-85.md`

### 已知问题（本阶段）

- 新节「台阶 slab（Phase 85）」3 条：**仅底部 slab**（无顶 slab/叠合/斜向
  楼梯/墙）/ **slab 邻满块同层侧面 Z-fight/缺面局限**（贪婪 mesh 不能半面
  剔除）/ **slab 光照按满块不透明**（顶半格空气本应透光，v1 简化）。

### 下一步

- P86 = 熔岩湖 worldgen（三个建议的最后一个）。
