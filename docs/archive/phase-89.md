# Phase 89 细节存档

> 范围（用户"按顺序把 建议1 、建议2、建议3 这三个 phase 完成" = 建议 1，
> 代理定案为最小完整版：**楼梯 4 朝向 = 数据驱动 4 块变体**，无 per-block
> 朝向状态，符合项目"方块无 per-instance 状态"哲学）。

## 阶段报告

- **`[2026-10-15, Phase 89]`** 楼梯 4 朝向 — 1147/1147（+3），`-Werror` 恒
  0 警告；E2E 批跑全绿（无输入站立判别；S1 第 62 次结构性免疫 + S2 朝北
  【46.4】+ S2b 朝南【47.0】同位互斥 + S6 init 失败）。

### 实现内容

**`stair_facing` 块属性（0-3）**（`src/block/block.{hpp,block_registry.cpp}`）：
- `BlockModel.stairFacing`（0=north / 1=east / 2=south / 3=west，缺省 0）。
- 解析 + [0,3] 校验（越界点名）。

**4 楼梯块变体（内容，复用 stone 纹理无新 atlas）**：
- `stone_stairs`（朝北，stair_facing 0，P87 已有加显式 0）/
  `stone_stairs_east`（1）/ `stone_stairs_south`（2）/ `stone_stairs_west`
  （3），各 1 配方（shaped S/SS/SSS → 4）。

**斜面碰撞 4 向（`src/entity/entity.{hpp,cpp}`）**：
- `solidTopHeight(BlockID, float mxLocalZ)` → **`solidTopHeight(BlockID,
  mn, mx, cell)`**——按 facing 沿对应轴取 **AABB 覆盖的最高斜面点**（north =
  1−mnLocalZ、south = mxLocalZ、east = mxLocalX、west = 1−mnLocalX）；slab/
  满块忽略 AABB（固定高度 = backward-compat）。
- **修正 P87 bug**：P87 用低端边（mxLocalZ）→ 玩家会切入楼梯高端；改为高端
  边（AABB 覆盖最高点）→ 脚越过整个斜面 footprint。
- `moveAxis` + `hasRoomAt` 传 mn/mx/cell。

**楔形 mesh 4 向（`src/world/chunk_mesher.cpp`）**：
- `emitStairs` 按 `stairFacing` 把 north 楔形的 5 面（斜顶 + 高端墙 + 两侧
  三角 + 底面）**绕 Y 旋转 facing×90°**（corner 仿射、normal/cull 方向线
  性），复用 P87 的绕序。

**测试（1144 → 1147，+3）**：
- `Phase89StairFacingParsed`（0-3 解析 / 缺省 0 / 越界拒）+
  `Phase89StairVariantsContent`（4 变体 stair_facing 0/1/2/3 + stone 纹理 +
  自掉）+ `Phase89SouthStairSlopeIsOpposite`（南楼梯 +Z 端高，朝北相反）。
- P87 的 `Phase87StairSlopeRestsByZ` 阈值随 max-edge 修正更新。

### E2E（全绿；批跑脚本 `/tmp/opencode/p89_e2e.zsh`，**无输入站立判别**）

- **S1 第 62 次结构性免疫**：楼梯永不生成 + 默认背包无 + fresh 无 stair =
  solidTopHeight 对满块恒 1.0（碰撞零变化）+ 无楔形 mesh → base18 字节一致。
- **S2 朝北**（stone_stairs@(22,46,0)，玩家 +Z 端 z=0.9）→ **pos.y ≈ 46.4**
  （+Z 端低）。
- **S2b 朝南**（stone_stairs_south 同位，同玩家位置 z=0.9）→ **pos.y ≈
  47.0**（+Z 端高）。**同位 46.4 vs 47.0 互斥 = 朝向翻转斜面**。
- **S6 init 失败**：scratch stone_stairs stair_facing 4 → "[0, 3]" 越界点名。

### 变更文件

- `src/block/block.hpp`（stairFacing）、`src/block/block_registry.cpp`
- `src/entity/entity.{hpp,cpp}`（solidTopHeight 签名 + 4 向 + 2 调用点 +
  P87 max-edge 修正）
- `src/world/chunk_mesher.cpp`（emitStairs 4 向旋转）
- `assets/data/blocks/stone_stairs.json`（+stair_facing 0）、
  `stone_stairs_{east,south,west}.json`（新）、
  `assets/data/recipes/stone_stairs_{east,south,west}.json`（新）
- `tests/test_block.cpp`（+2）、`tests/test_entity.cpp`（+1 + 阈值更新 +
  stairs_south 进 makeBlocks）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`AGENTS.md`、`docs/archive/phase-89.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 楼梯条「固定朝向」→「4 块变体（非 per-block 状态）」+
  碰撞条「AABB +Z 边」→「AABB 高端边（Phase 89 修正低端边 bug）」。

### 下一步

- P90 = 熔岩湖燃烧周围（建议 2）；P91 = 附魔台/信标多效果（建议 3）。
