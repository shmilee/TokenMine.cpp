# Phase 93 细节存档

> 范围（用户"下面两个阶段分别是：建议1 和 建议2" = 建议 2，代理定案为最小
> 完整版：**熔岩湖随机分布 = 程序化多湖**——`lava_lakes_procedural` 段，
> `count` 个湖在区域内确定性随机分布，= MC 熔岩湖散布世界。消费 P88"固定位
> 置列表（非随机分布/程序化多湖）"偏离）。

## 阶段报告

- **`[2026-10-16, Phase 93]`** 熔岩湖随机分布 — 1153/1153（+1），`-Werror`
  恒 0 警告；E2E 批跑全绿（S1 第 66 次结构性免疫 + S2 程序化湖确定性生成 +
  S3 跨 seed 不同 + S6 init 失败）。

### 实现内容

**`lava_lakes_procedural` 配置**（`src/world/worldgen_config.{hpp,cpp}`）：
- `LavaLakeProcedural`（enabled/count [0,64]/min_max_radius [0,16]/
  min_max_depth [1,16]/min_max_fill [0,8]/region 边界 + 4096 上限）。
- 解析 + 逐条校验点名（`lava_lakes_procedural.count` 等）。

**程序化湖生成**（`src/world/world_generator.{hpp,cpp}`）：
- `WorldGenerator::computeProceduralLakes()`（构造时从 seed 派生）：
  splitmix64 + kSaltLavaLake + 金步推进（PITS #4 纪律——state += GOLDEN 再
  mix），每湖 (x, z, radius, depth, fill) 各取范围内确定性随机值 = MC 熔岩
  湖散布世界。跨 run/chunk 一致（纯 seed 函数）。
- `placeLavaLakes` 改为 `placeOne` lambda + 循环 **fixed 湖（m_params.
  lavaLakes）+ procedural 湖（m_proceduralLakes）**（同款 carve/填熔岩）。

**shipped 配置**（`assets/data/worldgen.json`）：
- `lava_lakes_procedural`：count 5、radius [1,3]、depth [2,4]、fill [1,2]、
  region [60,90]×[60,90]（出生点 -Z 视野外 +Z 后方 + 视图距离内 → 流送但不
  入基线视图，免重锚）。

**测试（1152 → 1153，+1）**：
- `Phase93ProceduralLakesAreDeterministic`：程序化湖生成（≥3 熔岩列）/ 确定
  性（同 seed 两次生成同位置）/ 跨 seed 不同（seed 派生非固定）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p93_e2e.zsh`）

- **S1 第 66 次结构性免疫**：程序化湖在 [60,90]，seed 123456789 出生点
  (0.5,*,0.5) 朝 -Z 的**后方**（+Z）→ base18 字节一致。
- **S2 程序化湖确定性生成**（p93lakes 探针，纯 WorldGenerator 跑）：×2 同
  seed（9）字节一致 + 128 熔岩格 101 列非空 = 湖生成 + 确定性。
- **S3 跨 seed**：seed 9 vs 123456789 湖位置**不同** = seed 派生非固定。
- **S6 init 失败**：scratch worldgen `lava_lakes_procedural.count` 65 →
  "count' must be in [0, 64]" 点名。

### 变更文件

- `src/world/worldgen_config.{hpp,cpp}`（LavaLakeProcedural + 解析 + 校验）
- `src/world/world_generator.{hpp,cpp}`（computeProceduralLakes + placeOne
  lambda + kSaltLavaLake）
- `assets/data/worldgen.json`（lava_lakes_procedural 段）
- `tests/test_world_generator.cpp`（+1）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、`AGENTS.md`、
  `docs/archive/phase-93.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 熔岩湖条「固定位置列表（非随机分布）」→「固定位置列表 + 程序
  化分布」（消费 P88"非随机分布/程序化多湖"偏离）。

### 下一步

- 两个 phase（建议 1/2 = P92/P93）全部完成。候选：楼梯 per-block 旋转（真
  正突破无 per-instance 状态，需 format 变更）/ 附魔台给物品加效果 / 墙（第
  三新块模型）。
