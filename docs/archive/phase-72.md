# Phase 72 细节存档

> 范围（用户"请继续未完成的 phase" = 延续 P71 定案建议 2，代理定案为最
> 小完整版：**吸收两分量模型** = health + absorption 独立池，全量消费
> P56 遗留"吸收层与正常心共用一个 health 值"）：`m_health`（0..base
> max）+ `m_absorption`（0..64）独立池、`damage()` 先扣池、heal 只填
> health、池 = 效果总和纯函数只在同步点、format 12 `player.absorption`
> + format ≤11 迁移、HUD 从池派生。

## 阶段报告

- **`[2026-10-14, Phase 72]`** 吸收两分量模型 — 1097/1097
  （player 5→7 语义改写 + save 套件 5 新 + 15 处旧 format 降级剥键适
  配），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 45 次结构性免疫【无
  重锚】+ S2 坠落 D=4 全吸收【×2 savenorm + EXACT】+ S2b D=8 部分
  【×2 savenorm】+ S2c 对照【×2 savenorm】+ S3 迁移【×2 savenorm】
  + S4/S4d HUD 像素互斥 + S6 存档加载失败 ×2）。

### 实现内容

**核心（两分量模型，消费 P56 遗留）**：
- `src/player/player.{hpp,cpp}`：
  - `m_health`（0..base max）+ **`m_absorption`**（0..64，
    **`kAbsorptionMax = 64.0f`**）独立池；`absorption()` /
    `setAbsorption()`（inline clamp [0, 64]）。删除
    `setAbsorptionBonus`（P56 单 health + 抬 max 语义）。
  - `damage()` **先扣池**：`absorbed = min(池, amount)` 先出池，
    余量才扣 health——**池单独不能致死**（死亡只来自 health）。
  - `heal()` / `setHealth()` 只钳制到 **base max**（`maxHealth()` =
    `m_params.maxHealth`，不再含层）——**heal 永不填池**（池只由效果
    设置 = MC 属性语义）。
  - `respawn()` 清池 + health 回 base。
- `src/core/game.{hpp,cpp}`：
  - `Game::syncAbsorption()` **去掉 `healOnIncrease` 参数** = 纯 SET
    到 `totalAbsorption`（刷新/再施加恢复满层 = MC 属性重算）。五收口
    点（进食施加 / mob 命中 / 到期或水熄灭 / 牛奶清 / 死亡清）改无参
    调用；**读档不再调 sync**（存值权威）。
  - **关键语义修复（E2E 实测暴露）**：`updateEffects` 末尾的 sync 原
    为**每帧无条件**（P56 时代幂等无害）——两分量模型下每帧把伤害扣
    减重置回效果总和。改为**效果总和变化才同步**（`absorptionBefore`
    前后比较；熔岩点燃的每帧 `apply` 刷新 fire 不改变吸收总和 → 不
    触发）。PITS 已收录。
  - `applySave` 4e 重写（双池直设）：format 12 = 存值权威（池可低于
    效果总和 = 伤害在同步点间持续；health > base max 点名拒绝）；
    format ≤11 = **共享 health 迁移**（`h = 存 health`，-1 = base max；
    池 = `max(0, h − baseMax)`、health = `min(h, baseMax)`——22 →
    20+2，不损伤害；手编超 `base + 总和` 点名拒绝）。
  - HUD 心带：吸收红心改从**池**派生（`absIcons = ceil(池/2)`、strip
    value = 池；池 0 = 零绘制 = 无输入结构性免疫保持）。
  - `collectSave` 写 `m_player->absorption()`；F3 调试行加
    `absorption`。
- `src/core/save.{hpp,cpp}`：**format 12**：
  - `kSaveFormat = 12` + 注释块；`SaveGame.playerAbsorption = -1.0f`
    哨兵（旧档）。
  - 写入：`player.absorption` 恒写（≥0 时；游戏恒有真实值，0 = 无
    层）。
  - parse 键纪律（campfires 键纪律同款）：格式 12 文件**必需**该键；
    格式 ≤11 **带键拒绝**；范围 `[0, 64]`；**≤ 存档效果 `absorb_hp`
    总和**不变量（parse 已有 EffectRegistry 第 4 参，点名拒绝）；
    player 字段白名单加 `"absorption"`。
- `src/ui/debug_overlay.{hpp,cpp}`：`info.absorption` + 显示行
  （F3 不入基线）。

**测试（1092 → 1097）**：
- `tests/test_player.cpp`：5 个 P56 吸收测试 → 7 个两分量语义测试
  （独立池 / 钳 64 / damage 先扣池 / 池不能致死 / heal 不填池 /
  setHealth 钳 base / respawn 清池）。
- `tests/test_save.cpp`：`SaveFormat12Test` 套件 5 新（往返【16+2 池
  低于总和合法】/ 12 缺键拒 / 11 带键拒 + 去键可载【-1 哨兵】/ 超总
  和拒 / 超 64 拒）；旧 `HealthAboveBaseWithAbsorptionLayerRoundTrips`
  改写；`makeSave` 夹具补 `playerAbsorption = 0`；**15 处旧 format
  降级测试**各剥 `player.absorption` 键（新键纪律下旧档必无该键）。
- `tests/test_campfire.cpp`：makeSave 补池字段、format 断言 11→12、
  3 处 format 10 降级剥键。
- `tests/test_debug_overlay.cpp`：调试行断言加 absorption。

### E2E（全绿；批跑脚本 `/tmp/opencode/p72_e2e.zsh`）

- **S1 第 45 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节一致
  （无效果/无伤害/无 heal = 池 0 零绘制，零世界状态）。
- **S2 坠落 D=4 全吸收**（no-input 120f ×2 savenorm + EXACT）：存档
  预置玩家**空中 (22.5,52,0.5)**（restorePosition 重锚 fallStartY =
  P36）+ absorption 4 + absorption 效果 30s → 直落 7 格（D = 7−3 =
  4）→ 终态 pos EXACT (22.5,45,0.5) + **池 0 + health 20** + 效果仍
  活。
- **S2b D=8 部分吸收**：空中 y=56（D = 11−3 = 8）→ 池 4 吸收、余 4
  扣血 → **池 0 + health 16**。
- **S2c 对照**：同空位无池 → **health 12**（与 S2b 的 4-HP 差 = 池先
  吸收的证明）。
- **S3 format 11 迁移**：手编 format 11 档（共享 health 22 + 效果
  30s、无 absorption 键）→ 终态 **health 20 + 池 2**，且池全程保 2
  （无每帧重同步——若同步在每帧发生池会回到 4）。
- **S4/S4d HUD**（locked run 600f，关系像素检查）：吸收盒 x[505..550]
  y[642..665]（盒背景 = 场景编辑 6 格 stone，灰底排除世界红色假
  阳）：S4 池 4 → **138 px** 红心（>100，恰 = 2 满心 tile）；S4d 池
  0 + 效果活 → **0 px**（<20——旧模型画 2 空红心 ≈138 px，互斥）。
- **S6 存档加载失败 ×2**：format 12 缺 `absorption` 键 → "save format
  12 requires 'player.absorption'"；absorption 100 → "must be a number
  in [0, 64]"。

**帧数无关论证**：坠落终态 = 落地后静态（物理 ~40 帧内落地，120f 有
80 帧余量）；**120f 上界 = 5.2 s @23fps < regen_delay 10 s**（无自然
回血 tick——600f 首轮实测 health 18 ≠ 12 = 回血污染，PITS 已收录）；
效果 30s 保证 run 内不到期（23fps 下余 24.8s）；savenorm 剥 effect
remaining。

### 变更文件

- `src/player/player.hpp`、`src/player/player.cpp`
- `src/core/game.hpp`、`src/core/game.cpp`
- `src/core/save.hpp`、`src/core/save.cpp`
- `src/ui/debug_overlay.hpp`、`src/ui/debug_overlay.cpp`
- `tests/test_player.cpp`、`tests/test_save.cpp`、
  `tests/test_campfire.cpp`、`tests/test_debug_overlay.cpp`
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+2）、`AGENTS.md`、
  `docs/archive/phase-72.md`

### 已知问题（本阶段）

- P56 遗留"吸收层与正常心共用一个 health 值"**已解决**（划删，本阶段
  条目）。
- 新文档化偏离条：池硬上限 64 / 同步点语义（刷新恢复满层）/ 读档存
  值权威 / format ≤11 迁移拆分规则（见 KNOWN_ISSUES）。
- PITS +2：每帧纯 SET 与帧间状态相抗；自然回血污染长跑 E2E 伤害终
  态（run 时长须 < regen_delay 窗）。

### 下一步建议

1. **熔岩湖 worldgen**（用户明示"留到很久的未来"——候选保留，优先级
   最低）。
2. **火 v3：Mob 火接触伤害**（P70 遗留"Mob 站火里不受伤害"；机制 =
   P70 玩家同款 real-dt 计时器下沉到 Mob + `overlapsBlock`，mob 侧无
   存档字段）。
3. **吸收内容扩展**：信标（beacon）= MC 吸收的第二个来源（高成本，
   需方块实体系统——可拆"信标块 + 无效果"v1 再补效果）。
4. **campfire v2**（4 槽 / 燃料 / 水浇灭，P71 遗留偏离条）。
5. **弓 v3 / 箭强化**（P42 遗留"无箭矢强化/无限"）。
