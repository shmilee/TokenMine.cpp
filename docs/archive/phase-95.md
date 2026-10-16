# Phase 95 细节存档

> 范围（用户"接下来的两个 phase 分别是 建议1 和 建议3" = 建议 3，代理定案
> 为最小完整版：**墙 = 第三个新块模型类型，1.5 格高柱 + 高度感知碰撞
> barrier**）。

## 阶段报告

- **`[2026-10-16, Phase 95]`** 墙 — 1163/1163（+3），`-Werror` 恒 0
  警告；E2E 批跑全绿（输入驱动+无输入；S1 第 68 次结构性免疫 + S2 barrier
  【精确停 z=-0.7】+ S2b 无墙对照【走到 z≈-6.4】+ S2r 渲染【×2 RAW + 像素
  diff 135556 px】+ S6 纹理 init 失败）。

### 实现内容

**新块模型类型（`src/block/block.hpp` + `block_registry.cpp`）**：
- `BlockModel::Shape::Wall`（第三个新块模型类型，slab P85 / stairs P87
  之后）+ "wall" 形状解析（cube 纹理 top/side/bottom 校验同 slab 路径）。

**内容（`assets/data/`）**：
- `blocks/stone_wall.json`：shape wall、hardness 1.5（MC）、自掉、复用
  stone 纹理（无新 atlas tile → base18 免重锚）。
- `recipes/stone_wall.json`：shapeless 6 stone → 4（MC 墙配方）。

**barrier 碰撞（`src/entity/entity.cpp`）**：
- `solidTopHeight` 对 Wall 返回 **1.5**（实心区 [y, y+1.5] = 满块 + 顶 0.5
  帽）——玩家不能穿过（AABB 重叠实心区）也不能跳过（跳跃 ~1.25 < 1.5）。
- **帽伸进上方格**：`hasRoomAt` / `moveAxis` 的 y 扫描从 y0 扩到 **y0-1**
  （clamp ≥0）——AABB 在 y+1 格时须查 y 格的墙帽；非墙格 top≤1 时实心区
  [y, y+1] 到不了 AABB（严格不等式）= no-op backward-compat（既有 slab/
  满块行为零变化）。

**mesh（`src/world/chunk_mesher.{hpp,cpp}`）**：
- `emitWall`：单位立方 6 面 Y 缩 1.5（emitFluid 的 h 范式）——底面（dy<0）/
  顶帽（dy>0）邻 opaque 剔除、**侧面（dy=0）恒绘**（帽伸出格外无法半面
  剔除，邻满块 y 0..1 段 Z-fight = 同 slab v1 保守取舍）；opaque 子 mesh。
- 贪婪满块路径跳过 Wall（同 slab/stairs）。

**测试（1160 → 1163，+3）**：
- `Phase95WallShapeParsed`（"wall" 形状解析）+ `Phase95StoneWallContent`
  （真实资产内容钉死）+ `Phase95WallIsABarrier`（1.5 实心区：墙顶有房、
  墙体内/帽内无房）。
- 既有形状拒绝测试适配（"wall" 已合法 → 改拒 "fence"）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p95_e2e.zsh`）

- **S1 第 68 次结构性免疫**：stone_wall 永不生成 + 默认背包无 + 复用 stone
  纹理无新 atlas tile → base18 字节一致。
- **S2 barrier（输入驱动）**：石墙 (22,45,-2) + 玩家 (22.5,45,0.5) 朝 -Z，
  hold w 1.5 s → **精确停在 z=-0.7**（AABB -Z 面 zc-0.3 贴墙格 +Z 面
  z=-1）；×2 run 位置一致 + yaw/pitch 钉死（wall 钳位精确 → 输入边沿抖动
  免疫）。走道地形探针预证（x=22, z∈[-8,1]：-5..1 平 surface y45，-6..-8
  升 y46 台阶）。
- **S2b 无墙对照（输入驱动）**：同 hold → 走到 z≈-6.4（stepUp 踏上 z=-6
  台阶，y 45→46）∈(-6.5,-4.5)——与 S2 的 -0.7 **互斥 = barrier 证明**。
- **S2r 渲染（无输入）**：墙可见（1.5 高石柱在玩家正前方，pitch -0.25）；
  600f ×2 RAW 截图字节一致（静态；P90 湖火在 +Z 后方视图外）+ S2r vs
  S2r2（无墙）像素 diff = **135556 px**（隔离墙几何）。
- **S6 init 失败**：scratch 资产 stone_wall 缺 bottom 纹理 →
  "model.textures.bottom is required" 点名。

### 变更文件

- `src/block/block.hpp`（Shape::Wall）、`src/block/block_registry.cpp`
  （"wall" 解析 + 错误信息）
- `src/entity/entity.cpp`（solidTopHeight 1.5 + hasRoomAt/moveAxis y 扫描
  扩 y0-1）
- `src/world/chunk_mesher.{hpp,cpp}`（emitWall + 形状分发 + 贪婪路径跳过）
- `assets/data/blocks/stone_wall.json`、`assets/data/recipes/stone_wall.json`
- `tests/test_block.cpp`（+2 + 2 既有适配）、`tests/test_entity.cpp`（+1 +
  fixture wall 块）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `AGENTS.md`、`docs/archive/phase-95.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 新增「墙 wall（Phase 95）」段：1.5 高柱 barrier（无 MC 墙
  连接）/ 侧面 Z-fight（同 slab v1）/ 光照按满块不透明。

### 下一步

- 候选（SESSION_CONTEXT「下一步」）：墙连接（贴块延伸/栅栏顶，MC 墙语义
  扩展）；fence（连接型第四模型，横向面片）；铁栏杆/玻璃板（透明连接变体）；
  楼梯放置自动朝向（按玩家 yaw 设 per-instance 朝向，消费 P94"无放置自动
  朝向"偏离）。
