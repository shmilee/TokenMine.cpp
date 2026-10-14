# Phase 84 细节存档

> 范围（用户"按顺序将三个建议作为接下来的三个 phase" = 建议 1，代理定案
> 为最小完整版：**状态效果系统 v2 = 信标的其它主效果**——加 speed /
> strength / resistance 三个修饰效果，数据驱动；信标机制 P83 本身通用
> 【beacon_effect 指向任意效果】，效果词汇表即信标能力）。

## 阶段报告

- **`[2026-10-15, Phase 84]`** 状态效果系统 v2 — 1136/1136（+4），
  `-Werror` 恒 0 警告；E2E 批跑全绿（无输入坠落判别；S1 第 57 次结构性
  免疫 + S2 抗性坠落【health 16】+ S2ctl 无抗性对照【health 15】+ S6 init
  失败）。

### 实现内容

**效果数据 3 修饰字段（`src/core/status_effect.{hpp,cpp}`）**：
- `speed_multiplier`（float，(1.0, 3.0]，乘算）——移动速度倍数（MC 速度
  I = 1.2）。
- `melee_bonus`（float，[0, 100]，加算）——近战伤害加成（melee_penalty
  的相反项；MC 力量）。
- `damage_reduction`（float，[0.0, 1.0)，取最高单值）——受伤减免（MC 抗
  性 I = 0.2）。
- 解析（严格范围、逐字段点名）+ **no-op 规则扩到 3 字段**（有任一即合法）
  + **3 total 访问器**（`totalSpeedMultiplier` 乘积 / `totalMeleeBonus` 求
  和 / `totalDamageReduction` 取最高）+ `kFields` 白名单加 3 键。

**整合（3 处）**：
- **speed**：`Player` 加 `m_speedMultiplier`（`setSpeedMultiplier`），移动
  （player.cpp）`speed *= m_speedMultiplier`。
- **strength**：`Game::selectedAttackDamage` 加 `+ totalMeleeBonus`（与
  weakness 的 `− totalMeleePenalty` 相抵，net clamp ≥0）。
- **resistance**：`Player` 加 `m_damageReduction`（`setDamageReduction`），
  `Player::damage` 在**吸收池之前** `amount *= (1 - m_damageReduction)`。
- `Game::updateEffects` 每帧把 `totalSpeedMultiplier` / `totalDamageReduction`
  推到玩家（恒 1.0 / 0.0 当无此效果）。

**内容（3 新效果 + effects.png 扩 80→128）**：
- `speed.json`（speed_multiplier 1.2、icon 5）、`strength.json`（melee_bonus
  2、icon 6）、`resistance.json`（damage_reduction 0.2、icon 7）。
- `tools/generate_effect_icons.py`：W 80→128 + 3 图标（speed 青色菱 /
  strength 红方块 / resistance 蓝盾）；**既有 5 tile 确定性再生零变化**
  （E2E 前置校验确认字节一致）。

**信标连接**：信标机制（P83 `updateBeacons` 施加 `beacon_effect` 指向的
效果）**本身通用**——shipped 信标 = 吸收；改 `beacon_effect` 数据即换效
果。本 phase 不改进信标代码，只扩它的效果词汇表。**无附魔台 = 信标只施
加单一效果**（MC 主 + 副双效果无，文档化偏离）。

**测试（1132 → 1136，+4）**：
- `Phase84ModifierFields`（3 字段解析 + 越界拒绝）/ `Phase84NoOpRule`
  （speed-only 合法 / speed 1.0 仍 no-op）/ `Phase84Totals`（乘积/求和/取
  最高）/ `Phase83BeaconEffectsPinned`（speed.json 内容钉死）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p84_e2e.zsh`，**无输入坠落判别**）

- **S1 第 57 次结构性免疫**：3 新效果在注册表但 fresh 无输入世界无活跃效
  果 = 无 HUD 图标；effects.png 仅未绘制的 3 新 tile 变化（既有 5 字节
  一致）→ base18 不变。
- **S2 抗性坠落**（600f no-input ×2 RAW 截图对 + ×2 savenorm + EXACT）：玩
  家预置 y=53（地面 y=45 → 8 块坠落 = 5 伤），save 注入 `resistance`
  （damage_reduction 0.2）→ 5 × 0.8 = 4 → **health 16**。
- **S2ctl 无抗性对照**（同坠落无效果 → 5 伤 → **health 15**）——16 vs 15
  互斥（抗性减了 1 HP 伤害）。
- **S6 init 失败**：scratch `resistance.json` damage_reduction 1.0 → "must
  be in [0.0, 1.0)" 点名。
- **PITS 复用**：600f（chunk 流送落定，P78）+ scene config 禁回血
  （`regen_delay_seconds` 100000，P72 run-length 规则）+ shot_pair WM
  flake 重试（P64）。

### 变更文件

- `src/core/status_effect.{hpp,cpp}`（3 字段 + 解析 + no-op + 3 访问器）
- `src/player/player.{hpp,cpp}`（2 字段 + 移动 + 伤害）
- `src/core/game.cpp`（updateEffects 推送 + selectedAttackDamage bonus）
- `assets/data/effects/{speed,strength,resistance}.json`（新）、
  `assets/textures/effects.png`、`tools/generate_effect_icons.py`
- `tests/test_status_effect.cpp`（+4）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`AGENTS.md`、`docs/archive/phase-84.md`

### 已知问题（本阶段）

- KNOWN_ISSUES 信标条「信标效果 = 仅吸收」改「信标效果 = 数据驱动单一效
  果」（词汇表扩到速度/力量/抗性/吸收；MC 主 + 副双效果 = 无）。
- resistance 减免在 `Player::damage`（**所有**伤害源，含毒/火 tick）——
  MC 抗性不免毒/凋灵，项目简化（文档化偏离）。

### 下一步

- P85 = 楼梯·台阶·墙（新块模型类型 = 中成本）；P86 = 熔岩湖 worldgen。
