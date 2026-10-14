# Phase 74 细节存档

> 范围（用户"如果下一步建议中，建议1是最简单的，则下一phase 完成
> 建议1" = P73 定案建议 1，代理定案为最小完整版：**fire 块点燃玩家
> = 熔岩同款**，全量消费 P70"直接接触伤害"的非 MC 语义，消掉 P73
> 记录的"玩家/mob 火接触不对称"偏离）：`Game::updateEffects` 点火门
> `inLava() || isInFire()` + P70 直伤机制整体退役（`m_fireAccum` +
> `player.fire_damage*` config 键）。

## 阶段报告

- **`[2026-10-14, Phase 74]`** 火 v4：玩家火块点燃 — 1095/1095
  （P70 直伤 8 测试删除 + AABB 谓词改写 + 退役 config 容忍 1 新），
  `-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 47 次结构性免疫【无重
  锚】+ S2 站火烧死【×2 savenorm + EXACT】+ S2b 残留判别【×2
  savenorm + EXACT】+ S2ctl 无重叠对照【×2 RAW 存档+截图字节一
  致】；无 S6 = 零 parse 面）。

### 实现内容

**核心（fire 块 = 熔岩点燃的块版，玩家侧）**：
- `src/core/game.cpp`：`Game::updateEffects` 点火门
  `if (m_player->inLava())` → **`if (m_player->inLava() || m_player->
  isInFire())`**——fire 块与熔岩共用同一点火路径（P57 既有）：重施
  加 duration 重置 + tick 相位保留（火源内持续燃烧）、离开火源残
  留 `fire_duration_seconds` 8 s、水瞬时熄灭且先于 tick（P57 水胜规
  则）。节奏 = fire 效果数据（1 HP/0.2 s = 5 HP/s、min_health 0 可
  致死）——**玩家/mob/熔岩三源同节奏**（mob 侧 = P73 同款门）。
- `src/player/player.{hpp,cpp}`（P70 直伤机制退役）：
  - 删 `m_fireAccum` real-dt 计时器 + `updateSurvival` 的直伤块。
  - 删 Params `fireDamage` / `fireDamageIntervalSeconds` + fromConfig
    读取 + `fire_damage ≤ max_health` 交叉校验。**旧 config 仍启动**
    （未知 `player.*` 键容忍 = 既有 forward-compat 契约，单测钉死）。
  - `isInFire()` 改**公开**（P70 时 private，经 `inFire()` 读 tick
    缓存暴露；缓存 `m_inFire` + accessor 一并删除——消费方只剩
    updateEffects 门，直接调谓词）。
  - **零 format、零内容、零新 config**。
- 语义变化（P70 → P74）：站在 fire 块里 = **被点燃**（离开后继续烧
  8 s，MC 语义）而非"站着掉血、离开即停"；`player.fire_damage: 0`
  的关闭开关消失（关闭 = 资产集无 fire 效果，P57 结构条款同款）。

**测试（1102 → 1095）**：
- 删除 P70 直伤 8 测试（FireBurnsAtTheConfiguredRate / FireBurnKills /
  FireBurnResetsOnExit / FireDamageZeroDisables + config 4 测试）。
- 改写 `FireFaceTouchIsNotOverlap`（直调公开的 `isInFire()` 谓词，
  严格面约定不变）。
- 新增 `RetiredFireDamageKeysAreTolerated`（旧两键在 config 里 =
  fromConfig 仍 ok——用户旧 config 不炸）。
- 点燃语义（速率/残留/水灭）= P57 效果路径既有覆盖 + 本阶段 E2E
  S2/S2b（Game 级路径无单测夹具）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p74_e2e.zsh`）

- **S1 第 47 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节一
  致（fire 块永不生成 → fresh 零 fire 格 → `isInFire` 恒 false → 点
  火门惰性；无 config/内容/format 变更）。
- **S2 站火烧死**（no-input 600f ×2 savenorm + EXACT）：feet 格
  (22,45,0) 放 fire + remaining 8.0 + `spread_chance 0` + mobs off。
  玩家无输入站在火里：帧 ~1 点燃、5 HP/s → **~4 s 死**（real dt，
  600f = 10..26 s 任何 fps 都死透）；终态 health 0 + effects []（死
  亡清效果）+ fires []（8 s 燃尽）+ blocks []（fire→air 与生成态相
  消）+ time 帧确定。
- **S2b 残留判别**（no-input 900f ×2 savenorm + EXACT）：**同格
  fire remaining 1.0**——退役的 P70 直伤会在火灭（1 s）后停 =
  health 15 存活；P74 点燃后**离开火源残留 8 s** 继续烧 → 仍
  **health 0 死**。终态互斥 = 机制判别证明。
- **S2ctl 无重叠对照**（no-input 600f **×2 RAW**）：fire 在
  (22,45,-5)（4.5 格外，AABB 不重叠）→ 永不点燃：health 20、无
  effects；火自己燃尽。零 real-dt 活动 → **存档 + 截图 RAW 字节一
  致**（最强形式）。
- **无 S6**：零新 parse 面（场景存档 = P72 format-12 解析器既有文
  件；config 变更是删键，未知键容忍）。
- **PITS +1**：E2E 场景生成的存在性守卫（`[ -f 第一个场景 ] ||
  python3`）被冒烟测试的残留文件整块跳过 → 其余场景缺失；修 = 守
  卫覆盖全部文件 / 无条件幂等生成。

### 变更文件

- `src/player/player.hpp`、`src/player/player.cpp`
- `src/core/game.cpp`（点火门一行 + 注释）
- `tests/test_player.cpp`（−8 +2）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+1）、`AGENTS.md`、
  `docs/archive/phase-74.md`

### 已知问题（本阶段）

- P70 非 MC 语义"火接触 = 直伤、离开即停"**已退役**（改点燃，
  KNOWN_ISSUES 火 v2 条 + 伤害来源条同步）。
- P73 记录的"玩家火接触仍无残留点燃 = 两实现不对称"偏离**已消
  除**（玩家/mob/熔岩三源同点燃路径）。
- 退役键 `player.fire_damage` / `player.fire_damage_interval_seconds`
  从解析与校验中消失（旧 config 容忍 = 既有契约）。

### 下一步建议

1. **campfire v2**（4 槽 / 燃料 / 水浇灭，P71 遗留偏离条）。
2. **弓 v3 / 箭强化**（P42 遗留"无箭矢强化"）。
3. 信标（吸收第二来源，需方块实体系统 = 高成本，可拆 v1）。
4. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
5. 火 v5 候选（低优先）：fire 块的"点燃"目前无火焰视觉附着在玩家
   身上（燃烧 tick 只有红闪——MC 有角色身上的火焰粒子；可考虑
   玩家燃烧时的粒子表现 = 纯渲染层）。
