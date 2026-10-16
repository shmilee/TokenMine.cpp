# Phase 97 细节存档

> 范围（用户"下面三个phase 是建议1、建议2 和建议3" = 建议 1，代理定案为
> 最小完整版：**楼梯放置自动朝向 = Minecraft 放置规则**——低端朝玩家视线
> 方向，消费 P94"无放置自动朝向"偏离）。

## 阶段报告

- **`[2026-10-16, Phase 97]`** 楼梯放置自动朝向 — 1167/1167（+1），
  `-Werror` 恒 0 警告；E2E 批跑全绿（输入驱动；S1 第 70 次结构性免疫 +
  S2 yaw0→south + S2b yaw+90°→east【互斥】+ S2c 石对照）。

### 实现内容

**纯函数**（`src/block/block.hpp`）：
- `stairFacingFromYaw(float yaw)`：玩家放置楼梯时的 per-instance 朝向。
  相机约定 yaw 0 = 朝 -Z、正 = 左转 → 前向 = (−sin yaw, −cos yaw)。楼梯
  **低端朝视线方向**（facing = 高端侧）：yaw 0 朝 -Z → 朝南(2)（高端 +Z）/
  yaw +π/2 朝 -X → 朝东(1) / yaw π 朝 +Z → 朝北(0) / yaw 3π/2 朝 +X →
  朝西(3)。**45° 平局取 X 轴**（`|fx| ≥ |fz|` 走 X 分支）= 确定性。

**placeBlock 钩子**（`src/core/game.cpp`）：
- `setBlock` 收口（P94）先设块 stair_facing 默认朝向；随后 stair 形状
  放置 → `m_world.setStairFacing(target, stairFacingFromYaw(m_player->yaw))`
  覆写。**任意 stair 形状块**（基础 stone_stairs + P89 4 变体）同一规则——
  变体的 baked stair_facing 对玩家放置失效（朝向以 yaw 为准，变体 id 仅
  存档/非游戏上下文保留）。非 stair 放置零影响（stair 门控）。

**零 format/零内容/零 parse 面**（per-instance 朝向 = P94 既有机制）。

**测试（1166 → 1167，+1）**：
- `Phase97StairFacingFromYaw`：4 正交 yaw 钉死（0→2 / π/2→1 / π→0 /
  3π/2→3）+ 8 象限稳健（±0.3 rad 窗）+ 2 个 45° 平局（π/4→1 / 5π/4→3）。

**E2E 放置几何**（p97scan 探针，真实 `raycastBlock` 预验）：
- 场景 A：eye (22.5,46.62,0.5) yaw 0 pitch −0.22 → 命中托底 (22,45,-3)
  顶面 → 目标 (22,46,-3)，facing 2。
- 场景 B：yaw +π/2 → 需清生成块 (19,46,0)/(20,46,0)（地形在 x=19 升到
  y47，首轮被 (20,46,0) 侧面截获 = 探针暴露）→ 命中托底 (19,45,0) 顶面
  → 目标 (19,46,0)，facing 1。

### E2E（全绿；批跑脚本 `/tmp/opencode/p97_e2e.zsh`，**输入驱动**）

- **S1 第 70 次结构性免疫**：自动朝向只在玩家放置时运行，fresh 无输入
  零放置 → base18 字节一致。
- **S2 yaw0 放置**（单次右键，×2 run save EXACT）：楼梯落 (22,46,-3)、
  **facing = 2 (south)**（stairs 数组精确钉死）、物品消耗、托底完好、无
  掉落、玩家 pos/yaw 钉死。（P90 活火在 +Z 后方视图外 → 跳 savenorm，
  同 P94。）
- **S2b yaw+90° 放置**（×2 run save EXACT）：楼梯落 (19,46,0)、**facing
  = 1 (east)**——与 S2 的 2 **互斥 = yaw 驱动朝向**。
- **S2c 石对照**（单次右键）：同 yaw 放 stone → 块落位、**stairs 空**
  （自动朝向 stair 门控）。
- **无 S6**【零 parse 面】。

### 变更文件

- `src/block/block.hpp`（stairFacingFromYaw 纯函数 + <cmath>）
- `src/core/game.cpp`（placeBlock 钩子）
- `tests/test_block.cpp`（+1）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `AGENTS.md`、`docs/archive/phase-97.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 楼梯条：P94"无自动旋转"句 → P97"放置自动朝向"（Minecraft
  规则）；偏离：P89 变体 baked 朝向对玩家放置失效。

### 下一步

- P98 = fence（建议 2，连接型第四块模型：横向交叉面片 + 墙/块连接）。
- P99 = 玻璃板（建议 3，透明连接变体，透明 pass 版 P96 连接剔除）。
