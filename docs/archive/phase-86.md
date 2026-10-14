# Phase 86 细节存档

> 范围（用户"按顺序将三个建议作为接下来的三个 phase" = 建议 3，代理定案
> 为最小完整版：**熔岩湖 worldgen = 数据驱动固定位置静态满熔岩池**）。

## 阶段报告

- **`[2026-10-15, Phase 86]`** 熔岩湖 worldgen — 1141/1141（+1），
  `-Werror` 恒 0 警告；E2E 批跑全绿（无输入跑；S1 第 59 次结构性免疫
  【免重锚】+ S2 湖可见且静态【224154 橙色 px、×2 RAW】+ S6 init 失败）。

### 实现内容

**世界生成器 `placeLavaLake`（`src/world/world_generator.{hpp,cpp}`）**：
- 固定位置 (center_x, center_z)、半径 radius 的 (2R+1)² 盆地。
- **盆底 = 区域最低地形 − depth**（纯 `heightAt` 读取——确定性、跨 chunk
  一致：每个 chunk 生成时独立算同一 floorY）。
- 盆底以上（region 内）地形 carve 成 air；最低 `fill` 层注满熔岩（level 8）。
- **被围**：四周地形（region 外）≥ 区域最低 → 盆地墙高于熔岩 → 熔岩不流
  动、不蔓延（静态湖）。
- `generateChunk` 在 placeDecorations 后调用。

**WorldgenParams（`src/world/worldgen_config.{hpp,cpp}`）**：
- 6 字段：`lavaLakeEnabled` / `lavaLakeCenterX` / `lavaLakeCenterZ` /
  `lavaLakeRadius` / `lavaLakeDepth` / `lavaLakeFill`。
- `worldgen.json` 加 `lava_lake` 段（shipped：enabled true、center 40/40、
  radius 4、depth 4、fill 2）。
- 校验：radius [0,16] / depth [1,16] / fill [0,8]（越界点名）。

**位置选择**：shipped 湖 (40,40) 在出生点 (22.5,0.5) 的 **+X/+Z（后方
偏右）**，默认 -Z 朝向的 base18 视图**看不到** → **base18 免重锚**（第 59
次结构性免疫）。

**测试（1140 → 1141，+1）**：
- `Phase86LavaLakeIsContained`：① 区域有熔岩 ② 区域外无熔岩（被围）③ 二次
  生成熔岩位置相同（确定性）④ 熔岩顶上一格 = air（carve headspace）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p86_e2e.zsh`，**无输入跑**）

- **S1 第 59 次结构性免疫**：湖在出生点 -Z 视野外 → base18 字节一致（免
  重锚）。
- **S2 湖可见 + 静态**（600f no-input ×2 RAW 截图对 + 橙色像素检查）：玩
  家东堤 (45,41,40.5)、朝盆地（yaw 1.57 = -X、pitch −0.5 朝下）→ 熔岩池
  充满下视野（**224154 橙色 px**）；×2 RAW 字节一致 = **静态被围**（流体
  sim 不扰动）。
- **S6 init 失败**：scratch worldgen.json lava_lake.radius 17 → "must be in
  [0, 16]" 点名。
- PITS 复用：600f（chunk 流送落定 P78）+ shot_pair WM flake 重试（P64）。

### 变更文件

- `src/world/world_generator.{hpp,cpp}`（placeLavaLake + 调用 + <climits>）
- `src/world/worldgen_config.{hpp,cpp}`（6 字段 + parse + 校验）
- `assets/data/worldgen.json`（lava_lake 段）
- `tests/test_world_generator.cpp`（+1）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/CONTENT_GUIDE.md`、`AGENTS.md`、
  `docs/archive/phase-86.md`

### 已知问题（本阶段）

- 新节「熔岩湖（Phase 86）」2 条：**单固定湖**（非分布/多湖，`enabled` 可
  关）/ **静态满熔岩**（被围不流动不蔓延，MC 熔岩湖会流动/燃烧周围 = 无）。

### 三个 phase（P84/P85/P86）小结

- P84 信标更多效果（速度/力量/抗性）→ 1136，第 57 次免疫。
- P85 台阶 slab（首个新块模型类型，高度感知碰撞 + 半盒 mesh）→ 1140，
  第 58 次免疫。
- P86 熔岩湖 worldgen（数据驱动固定静态熔岩池）→ 1141，第 59 次免疫。
- 全程 `-Werror` 0 警告、base18 无重锚（P86 湖放视野外）。

### 下一步

- 候选：熔岩湖扩展（多湖/分布/熔岩燃烧周围，P58 已有燃烧机制）/ 楼梯·墙
  （P85 slab 之外的其余新模型类型）/ 信标更多效果等级。
