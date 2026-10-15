# Phase 88 细节存档

> 范围（用户"按顺序 把建议1 和 建议2 这两个 phase 完成" = 建议 2，代理定案
> 为最小完整版：**熔岩湖扩展 = 单湖 → 数据驱动湖列表 `lava_lakes`**）。

## 阶段报告

- **`[2026-10-15, Phase 88]`** 熔岩湖多湖 — 1144/1144（P86 湖测试演进为
  多湖，0 净增），`-Werror` 恒 0 警告；E2E 批跑全绿（无输入跑；S1 第 61 次
  结构性免疫【免重锚】+ S2 湖可见且静态【224154 橙色 px、×2 RAW】+ S6 init
  失败）。

### 实现内容

**单湖 → 湖列表（`src/world/worldgen_config.{hpp,cpp}`）**：
- `WorldgenParams`：P86 的 6 个单湖字段（`lavaLakeEnabled/CenterX/...`）→
  **`std::vector<LavaLake> lavaLakes`**（`LavaLake{enabled, centerX,
  centerZ, radius, depth, fill}`）。
- parse：`lava_lake`（单对象）→ **`lava_lakes`（列表）**，逐条读 6 字段 +
  逐条校验（radius [0,16] / depth [1,16] / fill [0,8]，点名 `lava_lakes[i]`）。
- `worldgen.json`：`lava_lake`（单）→ **`lava_lakes`（2 湖）**——(40,40) R4
  + (55,45) R3（皆出生点 -Z 视野外 → base18 免重锚）。

**世界生成器（`src/world/world_generator.{hpp,cpp}`）**：
- `placeLavaLake`（单）→ **`placeLavaLakes`**（循环所有 enabled 湖，各自独立
  region/floor；重叠湖 carve 同格 = 确定性）。每湖的盆底 = 该区域最低地形 −
  depth（纯 heightAt）。

**零 format**（熔岩在生成世界 = 非块/流体编辑，不入存档）。

**测试（1144，P86 湖测试演进）**：
- `Phase86LavaLakeIsContained` 改为多湖：2 湖（(20,20) R3 + (10,10) R2）——
  ① 湖 0 有熔岩 ② 湖 0 被围 ③ 确定性 ④ headspace ⑤ **湖 1 也有熔岩**（多
  湖列表放置每个湖）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p88_e2e.zsh`，**无输入跑**）

- **S1 第 61 次结构性免疫**：2 湖皆在出生点 -Z 视野外（z>0）→ base18 字节
  一致（免重锚）。
- **S2 湖可见 + 静态**（600f no-input ×2 RAW + 橙色像素检查）：玩家东堤
  (45,41,40.5) 朝湖 1 盆地 → 熔岩池充满下视野（**224154 橙色 px**）、×2 RAW
  字节一致（静态被围）。
- **S6 init 失败**：scratch worldgen.json `lava_lakes[0].radius` 17 → "must
  be in [0, 16]" 点名（含湖索引）。
- PITS 复用：600f（chunk 流送落定 P78）+ shot_pair WM flake 重试（P64）。

### 变更文件

- `src/world/worldgen_config.{hpp,cpp}`（单湖字段 → LavaLake 列表 + parse +
  校验）
- `src/world/world_generator.{hpp,cpp}`（placeLavaLake → placeLavaLakes）
- `assets/data/worldgen.json`（lava_lake → lava_lakes 2 湖）
- `tests/test_world_generator.cpp`（湖测试演进为多湖）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/CONTENT_GUIDE.md`、`AGENTS.md`、
  `docs/archive/phase-88.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 熔岩湖条「单个固定位置」→「固定位置列表」（shipped 2 湖，非
  随机分布/程序化多湖，每湖 `enabled` 可关）；静态满熔岩条不变。

### 建议 1 + 建议 2（P87 + P88）小结

- P87 楼梯 stairs（第二个新块模型类型，斜面高度感知碰撞）→ 1144，第 60 次
  免疫。
- P88 熔岩湖扩展（多湖列表）→ 1144，第 61 次免疫。
- 全程 `-Werror` 0 警告、base18 无重锚（2 湖放视野外）。

### 下一步

- 候选：楼梯 4 朝向（需 per-block 朝向状态）/ 熔岩湖燃烧周围（表面湖 + 可燃
  盆地壁）/ 附魔台（多效果同弓/同信标）。
