# Phase 94 细节存档

> 范围（用户"接下来的两个 phase 分别是 建议1 和 建议3。建议2...可丢掉" =
> 建议 1，代理定案为最小完整版：**楼梯 per-block 旋转 = 真突破"无
> per-instance 状态"**——朝向 = World 每格 byte，碰撞/mesh 直接读，存档
> format 14。P92 用块 id 环规避的正是此条）。

## 阶段报告

- **`[2026-10-16, Phase 94]`** 楼梯 per-block 旋转 — 1160/1160（+7），
  `-Werror` 恒 0 警告；E2E 批跑全绿（输入驱动；S1 第 67 次结构性免疫 +
  S2 旋转【per-instance 朝向 = 1 east、块 id 不变】+ S2b 石对照不转）。

### 实现内容

**per-instance 朝向（World 每格 byte）**（`src/world/chunk.{hpp,cpp}` +
`world.{hpp,cpp}`）：
- `Chunk::m_stairFacing`（每格 byte，哨兵 `kStairFacingUnset`=255=未设置）+
  `stairFacingAt/setStairFacing/clearStairFacing`（fill 时清哨兵）。
- `World::getStairFacing/setStairFacing`（越界/未创建返回哨兵；set 标脏 +
  邻 chunk 标脏 = mesh 重建）。

**碰撞 + mesh 读 World**（`src/entity/entity.cpp` + `src/world/
chunk_mesher.cpp`）：
- `solidTopHeight` / `emitStairs` 读 per-instance 朝向（哨兵时**回落块
  stair_facing 默认** = 单测无需改）。

**setBlock 收口**（`src/core/game.cpp`）：
- 楼梯放置 → 设 per-instance 朝向（块 stair_facing 默认）；移除 → 清哨兵。

**旋转**（`src/core/game.cpp` 右键链）：
- P92 块 id 环（stair_rotates_to）→ 改设 per-instance 朝向 `(f+1)%4`
  （stair 形状门控，块 id **不变**，"place" 音，点击消费）。

**存档 format 14**（`src/core/save.{hpp,cpp}` + `game.cpp`）：
- `SavedStair {pos, facing}` + `SaveGame::stairs` + `kSaveFormat=14`。
- 序列化（`stairs` 数组 {pos,facing}，恒写）+ 解析（format 14 要求 `stairs`
  键、format ≤13 迁移空、双向交叉校验：每个楼梯格有朝向 + 每个朝向是楼梯
  格）+ collectSave（扫 World 楼梯格）+ applySave（setStairFacing 到
  World）。

**内容**：
- P89 的 4 块变体保留为**默认朝向**（放置时 per-instance 朝向初始化为块
  stair_facing）；P92 的 `stair_rotates_to` 保留解析但**不再用于旋转**。

**测试（1153 → 1160，+7）**：
- save format 14（StairsRoundTrip / Format14RequiresStairsKey /
  Format13LoadsEmptyStairs / RejectsStairsFieldInFormat13 /
  RejectsStairEntryBlockMismatch）+ 碰撞 per-instance（
  Phase94PerInstanceFacingOverridesBlockDefault /
  Phase94UnsetFacingFallsBackToBlockDefault）。
- 既有存档测试适配（format 13→14 检查 + 各降级测试移除 `stairs` 键）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p94_e2e.zsh`，**输入驱动**）

- **S1 第 67 次结构性免疫**：楼梯永不生成 + fresh 世界 per-instance 朝向全
  哨兵 = 碰撞/mesh 回落块默认（无楼梯）→ base18 字节一致。
- **S2 旋转**（潜行+右键朝北楼梯）→ **per-instance 朝向 = 1 (east)**、块 id
  不变（仍 stone_stairs）、format 14 stairs 数组记录。×2 RAW 截图（P90 活火
  在出生点后方视图外；活火 real-dt 燃尽不可字节比 → 跳过 ×2 savenorm，单 run
  验证 stairs）。
- **S2b 石对照**（无 stair 形状）→ 不旋转、stairs 数组空。

### 变更文件

- `src/world/chunk.{hpp,cpp}`（m_stairFacing + 哨兵 + 3 方法）
- `src/world/world.{hpp,cpp}`（getStairFacing/setStairFacing）
- `src/entity/entity.cpp`（solidTopHeight 读 per-instance 朝向 + 回落）
- `src/world/chunk_mesher.cpp`（emitStairs 读 per-instance 朝向 + 回落）
- `src/core/game.cpp`（setBlock 收口 + 旋转改 per-instance + collectSave +
  applySave）
- `src/core/save.{hpp,cpp}`（SavedStair + stairs + format 14 + 交叉校验）
- `tests/test_save.cpp`（+5 + 既有适配）、`tests/test_entity.cpp`（+2）、
  `tests/test_campfire.cpp`（format 14 适配）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、`AGENTS.md`、
  `docs/archive/phase-94.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 楼梯条「4 块变体 + 旋转」→「per-instance 状态（真 per-block
  旋转）」（消费 P92"无 per-block 状态、无 format 变更"——P94 突破）。

### 下一步

- P95 = 墙（建议 3，第三个新块模型类型，1.5 高柱 + 高度感知碰撞）。
