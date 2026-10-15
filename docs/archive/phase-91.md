# Phase 91 细节存档

> 范围（用户"按顺序把 建议1 、建议2、建议3 这三个 phase 完成" = 建议 3，
> 代理定案为最小完整版：**附魔台 = 多效果同信标**——信标 `beacon_effect`
> 单值 → `beacon_effects` 列表，= MC 主+副效果 / 项目"叠加多效果于同一对象"
> 范式。"多效果同弓" = master_bow（P82）已做；本 phase 做"多效果同信标"。
> 消费 P83"信标只施加 `beacon_effect` 那一个"偏离）。

## 阶段报告

- **`[2026-10-16, Phase 91]`** 附魔台：多效果同信标 — 1150/1150（+2），
  `-Werror` 恒 0 警告；E2E 批跑全绿（无输入；S1 第 64 次结构性免疫 +
  S2 多效果【absorption + speed，池 4】+ S2b 无基座对照 + S6 init 失败）。

### 实现内容

**`beacon_effects` 块属性（列表）**（`src/block/block.{hpp,block_registry.cpp}`）：
- `BlockProperties.beaconEffects`（std::vector<std::string>，权威字段）+
  保留 `beaconEffect`（单值，向后兼容）。
- 解析 `beacon_effects`（非空数组，逐条目非空字符串，点名 `beacon_effects
  [i]`）+ 与 `beacon_effect` **互斥**（都写 = 拒绝，grants_effect 纪律）+
  合并（`beacon_effect` 单值 → 1 元素列表）。

**shipped beacon 内容**（`assets/data/blocks/beacon.json`）：
- `beacon_effect: "absorption"` → **`beacon_effects: ["absorption", "speed"]`**
  （吸收主 + 速度副 = MC 信标主 + 副效果）。

**施加（`src/core/game.cpp` `updateBeacons`）**：
- 原单值 `apply(beaconEffect)` → **循环 `beaconEffects` 列表**，对每效果
  `apply(eff, kBeaconEffectDurationSeconds)`（既有火源刷新范式——范围内全部
  效果常驻、离开 0.5 s 到期）。
- **init 交叉校验**：逐条目 `beaconEffects` 效果存在（grows_to 纪律，点名）。
- **信标追踪**（rescanBeacons + setBlock）：`beaconEffect.empty()` →
  `beaconEffects.empty()`。

**测试（1148 → 1150，+2）**：
- `Phase91BeaconEffectsListParsed`（列表解析 / `beacon_effect` 向后兼容 1
  元素 / 空数组拒 / 空条目点名）+ `Phase91BeaconEffectMutuallyExclusive`
  （单值 + 列表互斥拒绝）+ P83 `Phase83BeaconBlockContent` 演进（shipped
  beacon = [absorption, speed]）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p91_e2e.zsh`，**无输入**）

- **S1 第 64 次结构性免疫**：信标/铁块永不生成 + 默认背包无 + fresh 无信标
  = m_beacons 空 fast-exit + 列表惰性 → base18 字节一致。
- **S2 多效果**（信标 (22,47,−5) + 3×3 铁基座 y46，玩家 in-range）→
  **effects = [absorption, speed] 双效果 + 吸收池 = 4**（300f ×2 RAW 截图
  【P90 表面湖活火在 z26-34 出生点 -Z 视图外，×2 savenorm 因活火 real-dt
  燃尽非字节可比而跳过】+ 单 run EXACT）。
- **S2b 无基座对照**（同信标无铁基座 = 等级 0）→ **池 0 + effects []**——
  2 vs 0 互斥 = 多效果施加证明。
- **S6 init 失败**：scratch beacon `beacon_effects ["absorption","speedz"]`
  → "beacon_effects names unknown effect 'speedz'" 点名。

### 变更文件

- `src/block/block.hpp`（beaconEffects 列表）、`src/block/block_registry.cpp`
  （列表解析 + 互斥 + 合并）
- `src/core/game.cpp`（updateBeacons 循环施加 + init 逐条目交叉校验 + 信标
  追踪 2 处）
- `assets/data/blocks/beacon.json`（beacon_effect → beacon_effects 列表）
- `tests/test_block.cpp`（+2 + P83 内容钉死演进）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、`AGENTS.md`、
  `docs/archive/phase-91.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 信标条「单一效果」→「效果列表（多效果同信标，附魔台替身）」
  （消费 P83"MC 同时双效果 = 无"偏离）。

### 下一步

- 三个 phase（建议 1/2/3 = P89/P90/P91）全部完成。候选：楼梯 per-block
  旋转（突破无 per-instance 状态）、熔岩湖随机分布、附魔台给物品加效果
  （需新交互 + 物品附魔系统，超出"数据驱动叠加"范式）。
