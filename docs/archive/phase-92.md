# Phase 92 细节存档

> 范围（用户"下面两个阶段分别是：建议1 和 建议2" = 建议 1，代理定案为最小
> 完整版：**楼梯旋转 = `stair_rotates_to` 数据属性 + 潜行右键旋转**，4 楼梯块
> 变体成环 N→E→S→W→N，朝向即块 id = "状态=块 id"哲学，**无 per-block 状态、
> 无 format 变更**（比建议里"需 format 变更"更简洁，不突破"无 per-instance
> 状态"哲学））。

## 阶段报告

- **`[2026-10-16, Phase 92]`** 楼梯旋转 — 1152/1152（+2），`-Werror` 恒
  0 警告；E2E 批跑全绿（输入驱动；S1 第 65 次结构性免疫 + S2 旋转【north→
  east】+ S2b 石对照不转 + S6 init 失败）。

### 实现内容

**`stair_rotates_to` 块属性**（`src/block/block.{hpp,block_registry.cpp}`）：
- `BlockProperties.stairRotatesTo`（可选字符串，指向下一朝向变体）。
- 解析 + 显式空串拒绝（ammo 纪律，grows_to/ignites_to 同款）。

**4 楼梯块变体成环**（`assets/data/blocks/stone_stairs*.json`）：
- stone_stairs（north 0）→ stone_stairs_east（1）→ stone_stairs_south（2）→
  stone_stairs_west（3）→ stone_stairs（0）。各块 `stair_rotates_to` 指向
  下一朝向。

**旋转交互**（`src/core/game.cpp` 右键链 placeBlock 前）：
- 潜行（crouch）+ 右键一个 `stair_rotates_to` 非空的块 → 原地 setBlock 到
  下一朝向变体 + "place" 音 + 点击消费（不放置）。
- 朝向即块 id（"状态=块 id"哲学）→ **无 per-block 状态、无 format 变更**。
- 碰撞（solidTopHeight）+ mesh（emitStairs）自动跟随块 id（P89 既有）。

**init 交叉校验**（`src/core/game.cpp`）：
- `stair_rotates_to` 指向存在的块（grows_to/ignites_to 纪律，点名）。

**测试（1150 → 1152，+2）**：
- `Phase92StairRotatesToParsed`（解析 / 显式空串拒）+
  `Phase92StairRotationCycle`（4 变体成环，各指向下一朝向 + 环闭合）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p92_e2e.zsh`，**输入驱动**）

- **S1 第 65 次结构性免疫**：楼梯永不生成 + 默认背包无 + stair_rotates_to
  无视觉只经潜行右键 → base18 字节一致。
- **S2 旋转**（石块托底 (22,45,0) + 朝北楼梯 (22,46,0) 到眼高，玩家
  (22.5,45,1.5) yaw0 pitch-0.08 瞄准，潜行+右键）→ 楼梯变 **stone_stairs_
  east**、原 north 消失、石块托底不变、无掉落。×2 savenorm 字节一致。
  **PITS**：地面 (22,0)=y44（草，表面 y45）——stair 悬空 1 块时玩家头钻入
  → 碰撞推玩家上移 1.8 块 → 瞄准全偏；改石块托底 + 玩家后退 z1.5 + 浅俯角
  -0.08 解决。
- **S2b 石对照**（同位石块无 stair_rotates_to）→ 不旋转（点击 fall through
  到 placeBlock，空手 no-op）。
- **S6 init 失败**：scratch stone_stairs stair_rotates_to "stone_stairsz" →
  "stair_rotates_to names unknown block 'stone_stairsz'" 点名。

### 变更文件

- `src/block/block.hpp`（stairRotatesTo）、`src/block/block_registry.cpp`
  （解析 + 显式空串拒）
- `src/core/game.cpp`（旋转交互 + init 交叉校验）
- `assets/data/blocks/stone_stairs{,_east,_south,_west}.json`（stair_rotates_to
  成环）
- `tests/test_block.cpp`（+2）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、`AGENTS.md`、
  `docs/archive/phase-92.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 楼梯条「4 块变体（无自动旋转）」→「+ 旋转（stair_rotates_to
  成环 + 潜行右键）」（消费 P89"放置哪个朝向块 = 该朝向，无自动旋转"偏离）。

### 下一步

- P93 = 熔岩湖随机分布（建议 2）。
