# Phase 56 细节存档

> 范围（用户"金苹果后续，为下一阶段。"，代理定案为最小完整版：**金苹果
> 后续 = 吸收层（新 HP 层概念）+ `grants_effects` 列表字段 + 金苹果
> 再生+吸收**，消费 P49 遗留"金苹果只有再生（无吸收）"）：
> ① 效果字段 `absorb_hp`（激活期间的临时 max HP，整数 [1,64]）；
> ② 玩家有效 max health（base + Σ层）+ 施加提升/到期无声钳制 +
>    读档顺序（层先于 health）+ `Game::syncAbsorption` 六收口点；
> ③ 物品字段 `grants_effects`（严格配对数组，与 `grants_effect` 互斥，
>    P52 chance_drops 模式，按数据顺序施加）；
> ④ 内容：`absorption` 效果（4 HP = MC 吸收 II）+ 金苹果改列表
>    （regeneration 10 s + absorption 5 s）+ effects.png 第 4 图标；
> ⑤ HUD 吸收红心（正常心带左侧，无层零绘制 = 结构性免疫）。

## 阶段报告

- **`[2026-10-13, Phase 56]`** 金苹果后续：吸收层
  — 982/982（+15），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 30 次
  结构性免疫免重锚 + S2 金苹果吃【fps 无关终态：slot0 空 + 饥饿 16 +
  health 20 + 效果 []】+ S3 层激活【health 24 合法读档 + 红心带像素
  存在】+ S4 到期钳制 24→20 + S5 到期不过扣 18→18 + S6 init 失败 ×3）。

### 实现内容

**效果系统**（`core/status_effect.{hpp,cpp}`）：
- `Effect::absorbHp`（float，默认 0）：解析 `absorb_hp`，**整数**
  [1,64]（层 = 整 HP，小数会与 2 HP/心 的心带打架）；独立于
  tick_type / melee_penalty。no-op 拒绝规则更新 = 三者皆无才拒
  （纯层效果合法）。
- `StatusEffects::totalAbsorption(const EffectRegistry&)`：活跃实例的
  `absorbHp` 之和（纯函数，单测可测；无层 = 0）。

**玩家**（`player.{hpp,cpp}`）：
- `maxHealth()` = `m_params.maxHealth + m_absorptionBonus`（**有效**
  上限；`setHealth`/`heal` 对它钳制——再生可填进层）。
- `setAbsorptionBonus(bonus, healOnIncrease)`：
  - 层**增大**且 `healOnIncrease` = health 提升增量（clamp 新上限）
    ——层显示为**满**（满血吃金苹果 = 24/24，与 MC 逐位一致；受伤
    状态施加时层只显示 (新上限−当前) = **文档化偏离**，MC 是叠满层）；
  - 层**缩小/消失** = 超出 base max 的 health **无声掉下**（无红闪/
    音效/伤害计时器重置——层不是真 HP）；低于 base 的部分不受影响
    （不过扣）；
  - `healOnIncrease = false` = **读档模式**（存档 health 已含层，
    读档不得再抬——否则 22 被抬到 24 = 读档白送血）。
- `respawn()` 清层（bonus 0 + health 回 base 满）。

**Game 收口**（`game.{hpp,cpp}`）：`syncAbsorption(bool)` =
`m_player->setAbsorptionBonus(m_playerEffects.totalAbsorption(m_effects),
flag)`，六个调用点：进食施加后 / mob `on_hit_effect` 施加后 /
`updateEffects` 尾部（每帧到期）/ 牛奶 `clearStatusEffects` / 死亡
`clearAll` / `applySave` 4e。**漏一处 = "bonus ≠ Σ层" 静默漂移**。
`applySave` 重排：health + m_prevHealth 从 3) 移到 4d 效果之后
（4e）——层先于 health 生效，health 24（base+4 层）读档合法。

**物品**（`item.{hpp,cpp}` + `item_registry.cpp`）：
- `Item::GrantEffect {name, duration}` + `grantsEffects` 列表；
  解析 `grants_effects`：数组、非空、与 `grants_effect` **互斥**
  （同写拒 = P52 模式）、仅食物（restore_hunger > 0）、逐条目严格
  配对（name 非空 / duration > 0 / 未知键拒，点名 `grants_effects[i]`）。
- 进食施加：单字段与列表并存解析（互斥保证至多一个非空），列表**按
  数据顺序** apply，之后一次 `syncAbsorption(true)`。
- init 交叉校验：列表逐条目对 EffectRegistry（ammo 模式，点名
  `grants_effects[i]`）。

**内容**（纯数据）：
- `assets/data/effects/absorption.json`（新）：
  `{"id":"absorption","absorb_hp":4,"negative":false,"icon":3}`
  ——4 HP = MC 金苹果吸收 II（2 颗心）；正面 = 牛奶保留（项目规则：
  牛奶只清负面）。
- `assets/data/items/golden_apple.json`：`grants_effect`（regeneration
  10 s）→ `grants_effects: [{regeneration,10.0},{absorption,5.0}]`
  ——再生 10 s 保留项目值；吸收 5 s = MC 附魔金苹果的吸收时长。
- `tools/generate_effect_icons.py`：W 48→64，第 4 图标 = 红心（9×7
  剪影）+ 白十字（MC 吸收标志），确定性噪声同前 → `effects.png`
  64×16（init 的 icon 范围交叉校验按宽度自动适配）。

**HUD**（`game.cpp` drawStrip 区）：
- 正常心带**位置不动**（`heartIcons = ceil(base max / 2)`，默认 10 心
  = Phase 16 布局逐位相同）；层激活时其**左侧**画 `ceil(层/2)` 颗
  **红心**（tint (1, 0.35, 0.35)），值 = `health − base`（层显示超出
  base 的部分——与单 health 值模型一致，见 KNOWN_ISSUES 偏离条目）。
  层 = 0 → 零绘制（无输入基线结构性免疫）。效果图标行仍在 hotbar
  左缘（默认几何下与红心带不重叠；≥3 效果可重叠 = 新增
  KNOWN_ISSUES，纯视觉）。

### 变更文件

- `src/core/status_effect.{hpp,cpp}`（absorb_hp + totalAbsorption +
  no-op 规则）
- `src/player/player.{hpp,cpp}`（有效 max + setAbsorptionBonus +
  respawn 清层）
- `src/item/item.hpp`、`src/item/item_registry.cpp`（grants_effects）
- `src/core/game.{hpp,cpp}`（syncAbsorption 六收口 + applySave 4d/4e
  重排 + init 逐条目校验 + HUD 红心带）
- `assets/data/effects/absorption.json`（新）、`assets/data/items/
  golden_apple.json`（列表化）
- `tools/generate_effect_icons.py`（48→64 + 第 4 图标）、
  `assets/textures/effects.png`（重新生成 64×16）
- 测试：`tests/test_status_effect.cpp`（+4，makeRegistry 加 absorption）、
  `tests/test_player.cpp`（+6）、`tests/test_item.cpp`（+4）、
  `tests/test_save.cpp`（+1，makeEffects 加 absorption）
- 文档：`AGENTS.md`（现状 P56 / 982 / 第 30 次免疫）、
  `docs/ROADMAP.md`（P56 条目 + 索引）、`docs/KNOWN_ISSUES.md`（关
  P49 两条、no-op 条更新、+2 新遗留）、`docs/PITS.md`（+2）、
  `docs/CONTENT_GUIDE.md`（物品模板 + 效果节 4 tile/absorb_hp/
  grants_effects + 4 种效果）、`docs/GAMEPLAY.md`（金苹果吸收）、
  `docs/MECHANICS.md`（状态效果节：absorb_hp/吸收层/4 种/HUD 红心）、
  本文件

### 构建与验证结果

- `-Wall -Wextra -Wpedantic -Werror` 恒 **0 警告**。
- 测试 **982/982**（+15：效果 4 + 玩家 6 + 物品 4 + 存档 1）。
- **E2E 批跑全绿**（`/tmp/opencode/p56e2e/p56_e2e.log`，EXIT=0）：
  - **S1 base18 门**：P56 ×2 新世界 60f 无输入字节一致 + == base18 →
    **第 30 次结构性免疫（免重锚）**【无输入零效果 = 红心带零绘制 +
    无 worldgen 变更】+ 红心 0。
  - **S2 金苹果吃**（1200f 输入 ×2 + 240f 无输入对照 ×2）：对照 ×2
    字节一致（苹果未吃）。1200f @ 观测 fps 26..62 = **19.4..46 s >
    再生到期 14.5 s**（吃完成于 ~4.5 s 真实 dt：吸收 5 s + 再生
    10 s）→ 终态在**一切观测 fps 下相同**（fps 无关设计）：slot0 空
    + 饥饿 16（12+4；饱度缓冲 10+ 吸掉 36 s 衰减 tick）+ health 20
    （层到期后超出 base 的 4 无声掉下；后续再生 tick 被有效上限
    钳回 20）+ 效果 []。×2 savenorm（剥 age/remaining/**saturation**
    = 真实 dt 字段）+ 精确断言。
  - **S3 层激活**（240f 锁定单输入 ×2 + 对照 ×2）：场景 health 24 +
    absorption 100 s。存档：health **24 合法**（层先于 health 生效的
    读档路径）+ 效果 [absorption]（remaining ∈ (90,100)）×2 savenorm
    （对照 ×2 也归一化——场景自带效果，remaining 真实 dt 递减）。
    截图：红心带 x[511,545] y[646,662] 红像素 > 50（2 颗满红心）+
    正常心带 heartcheck 通过（10 颗满心）。
  - **S4 到期钳制**（240f 无输入 ×2）：health 24 + absorption 3 s
    （3 s < 3.9 s 最短跑长 = 一切 fps 下跑内到期）→ health **20** +
    效果 []，×2 **字节**一致（终态无真实 dt 字段）。
  - **S5 到期不过扣**（240f 无输入 ×2）：health 18 + absorption 3 s
    → health **18**（层消失不伤真 HP）+ 效果 []，×2 字节一致。
  - **S6 init 失败 ×3**（scratch 资产）：`grants_effect`+
    `grants_effects` 同写 → "mutually exclusive"；
    `grants_effects[0].name = "nope"` → "grants_effects[0] references
    the unregistered effect"；`absorb_hp: 0` → "[1, 64]"。

### 关键决策

- **单 health 值模型（Option B：施加时提升）**：层 = 临时 max HP +
  施加时 health 提升增量（层显示为满）。满血吃金苹果与 MC **逐位
  一致**（24/24，受伤先掉红心）；受伤状态施加时偏离（MC 叠满层在
  正常心上，本项目层只显示空缺部分）= 文档化偏离入 KNOWN_ISSUES。
  双分量模型（normal + absorption 两个值）是精确方案但动存档格式 +
  伤害路径 + HUD，超出最小完整版。
- **到期钳制走静默 clamp 不走 damage()**：层不是真 HP——掉下无红闪/
  无音效/不重置 `m_timeSinceDamage`（否则吃金苹果 5 秒后必然红闪 =
  假伤害事件，且会重置饥饿再生计时）。
- **读档顺序 = 层先于 health**：save.cpp 对 health 无上限校验
  （"游戏加载时钳制"的既有设计），钳制上限必须已是**有效**上限
  （24），否则 24 被钳 20 = 层白读。`applySave` 4d/4e 重排 + 单测
  钉死（health 24 + 层往返）。
- **金苹果吸收 5 s（非 MC 附魔金苹果的 2 分钟）**：v1 节奏与项目
  再生 10 s（MC 是 5 s）一致——项目效果时长自 P49 起用项目值；
  5 s 也是 E2E 可观测窗口（S2 终态设计依赖它在跑内到期）。
- **S2 用 1200f 终态而非中途断言**：效果到期是真实 dt 事件，中途
  状态（"效果在不在"）随 fps 漂移；1200f 保证一切观测 fps 下两效果
  均已到期 → 终态 fps 无关（P54 范式）。
- **savenorm 剥 saturation**：饱度缓冲 36 s 真实 dt 衰减 → 场景 ×2
  的 tick 数可不同 = 字节 ×2 假失败；判定标准 = 字段是否随真实 dt
  累积（age/remaining/saturation）。PITS +1。

### 已知问题（本阶段关闭 2 / 新增 2 / 备注更新 1）

- 关闭：**金苹果只有再生（无吸收）**（P49）→ 吸收层实现（带文档化
  偏离）；**吃时给效果仅支持单一固定时长**（P49）→ `grants_effects`
  列表（每条目仍固定时长、无 amplifier = 与效果系统整体一致）。
- 备注更新：**no-op 效果被注册表拒绝**（P46）→ 规则 = 无 tick 且
  melee_penalty = 0 且 absorb_hp = 0。
- 新增：**吸收层与正常心共用一个 health 值**（受伤状态先扣正常心 +
  施加时层不满）；**效果图标行与吸收红心可视觉重叠**（≥3 效果）。

### 下一步建议

- 脱离熔岩残留火（火系统，较大）/ 多掉落内容再扩展（如 gravel→flint
  需新物品）/ 吸收层双分量模型（精确 MC 层语义，动存档格式 = 大改）。
- **worldgen 熔岩湖 = 用户明示"留到很久的未来"**（排在火系统之后）。
