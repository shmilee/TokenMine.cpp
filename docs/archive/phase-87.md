# Phase 87 细节存档

> 范围（用户"按顺序 把建议1 和 建议2 这两个 phase 完成" = 建议 1，代理定案
> 为最小完整版：**楼梯 stairs = 第二个新块模型类型**——朝北斜面 + 斜面高度
> 感知碰撞 + 楔形 mesh）。

## 阶段报告

- **`[2026-10-15, Phase 87]`** 楼梯 stairs — 1144/1144（+3），`-Werror` 恒
  0 警告；E2E 批跑全绿（无输入站立判别；S1 第 60 次结构性免疫 + S2 楼梯斜面
  【pos.y 46.2】+ S2ctl 满块对照【pos.y 47】+ S6 init 失败）。

### 实现内容

**新块模型类型 `Shape::Stairs`（朝北斜面，-Z 高 +Z 矮）**：
- `block.hpp`：`BlockModel::Shape::Stairs`（FullCube/Slab/Cross 之外的第 4
  形状）。
- `block_registry.cpp`：shape 解析加 `"stairs"`（cube 纹理 top/side/bottom；
  "wall" 等仍拒绝）。

**chunk mesher（`src/world/chunk_mesher.{hpp,cpp}`）**：
- `emitStairs`（新）：楔形——**斜顶面**（恒绘，法线 (0,1,1) 顶面亮度）、-Z
  背墙（满高，kCubeFaces -Z 绕序）、±X 侧三角（楔形）、底面（kCubeFaces
  -Y 绕序）；侧/背/底邻 opaque 剔除；进 OPAQUE 子网格。
- `mesh()` 的 slab pass 加 Stairs 分支；贪婪满块路径**跳过 stairs**。

**斜面碰撞（`src/entity/entity.{hpp,cpp}`）**：
- `solidTopOffset(BlockID)` → **`solidTopHeight(BlockID, float mxLocalZ)`**：
  楼梯实心高度随 z 变（`1 - mxLocalZ`，mxLocalZ = AABB 的 +Z 边在格内 local
  位置）；slab = 0.5、满块 = 1.0（忽略 mxLocalZ = P85 backward-compat）。
- `moveAxis` + `hasRoomAt` 算 mxLocalZ（clamp AABB +Z 边到格内）+ 调
  solidTopHeight——玩家脚落在其 +Z 边处的斜面高度。

**内容（1 新块 + 1 配方，复用 stone 纹理无新 atlas）**：
- `stone_stairs.json`：shape stairs、hardness 1.5、自掉、solid/opaque、**复用
  stone 纹理**（无新 atlas tile）。
- `stone_stairs` 配方：shaped **S/SS/SSS → 4 stone_stairs**（MC 台阶同款）。

**测试（1141 → 1144，+3）**：
- `Phase87StoneStairsContent`（真实资产：shape Stairs / hardness 1.5 / 自掉 /
  stone 纹理）+ `Phase87StairsShapeRequiresCubeTextures`（stairs 用 cube 纹
  理，cross 纹理拒绝）。
- `Phase87StairSlopeRestsByZ`（实体在楼梯后侧【-Z 高】停得比前侧【+Z 矮】高
  = 斜面碰撞证明）。
- P85 的两个 shape 测试契约演进（"stairs" 现支持 → 改用 "wall"）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p87_e2e.zsh`，**无输入站立判别**）

- **S1 第 60 次结构性免疫**：楼梯永不生成 + 默认背包无 + fresh 无 stairs =
  solidTopHeight 对满块恒 1.0（碰撞零变化）+ 无楔形 mesh → base18 字节一致。
- **S2 楼梯斜面**（300f no-input ×2 RAW 截图对 + ×2 savenorm + EXACT）：
  stone_stairs@(22,46,0)，玩家居中（AABB +Z 边 local 0.8 → 高 0.2）→ 静止
  → **pos.y ≈ 46.2**（非满块 47、非穿落 46）= 斜面高度感知碰撞证明。
- **S2ctl 满块对照**（同位 stone 满块，玩家 feet 47 → **pos.y ≈ 47**）——
  46.2 vs 47 互斥（斜面低于满块）。
- **S6 init 失败**：scratch stone_stairs + cross 纹理 → "model.textures.cross
  is only valid for shape" 点名。
- PITS 复用：300f + shot_pair WM flake 重试（P64）。

### 变更文件

- `src/block/block.hpp`（Shape::Stairs）、`src/block/block_registry.cpp`
- `src/world/chunk_mesher.{hpp,cpp}`（emitStairs + pass + 贪婪跳过）
- `src/entity/entity.{hpp,cpp}`（solidTopOffset→solidTopHeight + 2 调用点）
- `assets/data/blocks/stone_stairs.json`（新）、
  `assets/data/recipes/stone_stairs.json`（新）
- `tests/test_block.cpp`（+2、2 契约演进）、`tests/test_entity.cpp`（+1 +
  stairs 进 makeBlocks）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`AGENTS.md`、`docs/archive/phase-87.md`

### 已知问题（本阶段）

- 新节「楼梯 stairs（Phase 87）」2 条：**固定朝北朝向**（无 4 旋转，项目方
  块无朝向状态）/ **斜面碰撞按 AABB +Z 边**（非亚像素斜面）。

### 下一步

- P88 = 熔岩湖扩展（建议 2：多湖/分布/燃烧周围）。
