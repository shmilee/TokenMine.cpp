# Phase v3.17.0 — V3-11a 创造基础（门 207 全 PASS，2026-11-06）

矩阵 ⬜ 项小→大推进 3/6。V3-11 创造模式 = 2 相位（11-06 定案拆分）；
本版 = **Phase A**（模式切换 + 不死 + 飞行 + 即挖），Phase B（创造物品
栏）= v3.18.0。

## 范围与定案

- **创造 = 存档 flag（非会话态）**——`player.creative` strict bool
  **恒写**（format 31；dimension 恒写先例；pre-31 载迁 false = 模式不
  存在；format-30 文件携键 = version lie 点名）。MC per-player /gamemode
  语义；相机模式（会话态）先例的对偶。
- **切换键 = `G`**（GameKey::G 插在 FlyToggle 前，kKeyCount 尾钉不动）：
  locked pointer + 仅存活 = C 键范式（指针自由时按 G 无动作）。
- **不死 = 全源伤害门**：`Player::damage` 单收口 `if (m_creative)
  return false;`——lava/fire/fall/mob/arrow/starve/void 全部走这个门，
  一处门控即全源免疫（无 per-source 分支 = 零数据字段）。
- **饥饿静态满**：`setCreative(true)` 入场填饱（hunger/saturation 满 +
  accumulator 清 0）；衰减 + 饥饿 tick 用 `if (!m_creative)` **包裹**
  ——非 early return（保护下方 lava/kill-plane 段继续跑，其伤害被
  damage 门覆盖 = 双重保险）。
- **即挖**：creative 强制 break duration 0 = 复用既有 data-driven
  instant-break 路径（hardness 0 同款），首帧落地。
- **无掉落 / 无 XP / 无挖掘耐久**：breakBlock 的 4 段（primary
  drops / chance_drop / chance_drops / XP orb）gate 包裹——**声明
  前移重构**（block/dropPos/dropSeed/dropAt 提到 gate 前，尾段粒子/
  音效/耐久可见）；破碎粒子与音效保留 = 创造挖掘手感（MC 同款）；
  挖掘耐久（tool wear）creative 跳过。
- **放置不消耗**：3 点（cloth 染色放置 / 顶部 slab 同格合并 / 常规
  放置）`if (!m_player->isCreative())` 门控 take。
- **开关 = setFlying(true/false)**：模式拥有飞行过渡（进入即飞，关闭
  落地）；双击 Space 保留作模式内 toggle。
- **sim 不 bump（保持 2）**：创造 = 存档态增量功能，survival 路径位级
  不变（damage 门/hunger 包裹/creative 冷路径）；canary 4 demo 回放
  savenorm EXACT = 门内实证（bump 纪律见 v3.13.0）。
- **创造物品栏 = V3-11b 不做**（搜索/给予面板 = UI 面，下版）。

## 实现（代码 10 文件 + 测试 4）

- `src/player/player.hpp/cpp`：`m_creative` + `isCreative()` /
  `setCreative(bool)`（入场填饱）+ damage 门 + hunger 包裹。
- `src/core/input.hpp/cpp`：GameKey::G（FlyToggle 前）+ mapKey
  SDLK_G；kKeyCount 尾钉不变（PITS 396② 纪律）。
- `src/core/game_ui.cpp`：G 切换处理器（C 键后；locked pointer + 仅
  存活；MC_LOG_INFO 单参）。
- `src/core/game_interaction.cpp`：即挖 duration 0 + breakBlock gate
  重构（声明前移）+ 耐久 creative 跳过 + 放置 3 点免消耗。
- `src/core/save.hpp`：kSaveFormat 31 + format-31 文档块 +
  `playerCreative` 字段。
- `src/core/save.cpp`：player 白名单（≥31 加 creative）+ 三段 parse
  （31 必需 strict bool / pre-31 迁 false / 30 携键 = 白名单点名）+
  恒写。
- `src/core/game_save.cpp`：collectSave 恒写 + load 侧 setCreative +
  creative → setFlying(true)。
- 版本三件套：CMakeLists 3.17.0 + test_debug_overlay F3 钉 3.17.0。
- 测试：`tests/test_save.cpp` SaveFormat31Test ×3 +
  `tests/test_player.cpp` PlayerTest.Creative ×3。

## 门 207（全 PASS）

- **S1 ≡ base19 逐字节 ×2 + savenorm EXACT**（**结构免疫第 87
  次**——creative 默认 off = 零存档键差【format 31 键恒写但 S1 口径
  = 双跑互比 + PNG 锚】零像素）。
- **S2 creative demo 臂**（全 headless）：
  - 场景 = 种子 123456789 60f fresh 档**去火**（3 团地下火移除 +
    fires:[]，PITS 398① 纪律）+ `player.creative = true`（format 31）。
  - 时间线：f10 低头 dy 628（准星对地面）/ f20 LMB down / f30 up
    （**即挖** (0,53,0)）/ **f50 F5 down / f55 up**（结果捕获——
    demo save-spec 回放 `--save` 须匹配槽 + 退出永不回写槽 → F5 =
    唯一结果机制，PITS 399②）。
  - 断言（×2 臂）：`creative=True` / `health 20` / `pos (0.5,54,0.5)`
    【飞行不坠】/ `(0,53,0) = air`【已挖】/ `entities=0`【无掉落】+
    截图 ×2 RAW 恒等 + 结果存档 ×2 RAW 恒等。
- **S2b canary 4 demo**（sim 不 bump 实证）：example / tutorial_dig /
  tutorial_build / tutorial_fly（sim 2，fresh world spec，0 动作）×2
  RAW 恒等 + savenorm EXACT（300f）。
- **S6 点名**：实机 format-30 文件携 creative → **rc 1**「save
  'player' has unknown field 'creative'」；单元 ThreeWay 三面（30
  携键 / 31 缺键 / 非 bool "on" 串）全点名。
- **S3 全套 2801/2801**（+6：SaveFormat31Test
  RoundTripCreativeTrueAndFalse / Pre31LoadMigratesToFalse /
  ThreeWayKeyDiscipline + PlayerTest.CreativeBlocksAllDamageSources
  【8 源全免疫】/ CreativeHungerIsStatic【对照臂 23 tick 裕量 =
  1/60 双累加边界纪律，PITS 族】/ CreativeEntryFillsAStarvingBar），
  `-Wall -Wextra -Wpedantic -Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = 6b031c5 v3.16.0（git archive）+
  g139 vs 新 = 工作树 + g139（banner 实证 v3.16.0 / v3.17.0），900f
  无输入独占——PERF-WIN 三窗 upd med 0.578/0.667/0.650 vs
  0.690/0.596/0.652 + render 0.567/0.734/0.742 vs 0.725/0.665/0.776 +
  swap med 15.297/15.016/14.980 vs 15.042/15.179/14.993（新旧各一窗
  swap max ~32 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·spread·
  ents·stream 同档 + **changesApplied 9/2-3/1**【win2 ±1 = streaming
  边界 real-dt 域，v3.14.0 先例】+ **终档 = 恰白名单 2 键**（format
  30→31 + player.creative missing-in-old，零其它漂移——format 跳变版
  v3.4.0「恰白名单 4 键」同款论证）——creative 无输入全冷路径 =
  结构性零成本。

## 门内捕获（PITS 399）

- **format bump 测试面 = 4 类降级点**（首跑全套 **61 失败**全此族，
  非实现 bug）：(a) strip 助手本地复制 ×5 文件（test_save/bed/
  campfire/drown/enchant_save 各持一份 stripFormat30Keys）升
  stripFormat31Keys + creative strip；(b) 手动 withoutKey 链
  （SaveFormat8Test player rebuild）加 creative 一环；(c) inline
  rebuild（test_durability player30 块 `k != "dimension"`）加 `&& k
  != "creative"`；(d) gzip identity 测试 29→30 rebase 29→31（模拟旧
  档 strip creative + format 断言 + 往返断言 + 测试名同步）。**纪律
  = format bump commit 同时盘点 tests/ 全部降级点**（grep：strip
  助手 / `"format\": 30` / `asInt(), 30` / withoutKey player 链）。
- **demo save-spec 回放结果捕获 = demo 内 F5**：world = save slot
  spec 时 `--save` 不匹配槽 = rc 2 点名，匹配后退出也**永不回写槽**
  （观察红线）→ 常规退出存档不存在；结果捕获 = 时间线显式 F5（F5
  handler 无条件 writeSave 当活跃槽）。fresh world spec（4 canary）
  无此问题。

## 记档偏离

- 创造物品栏未做（V3-11b 下版）——现创造玩家只能用既有背包物品。
- 创造免疫含饥饿/虚空 = 死亡统计屏创造下不可达（无死因行 = 模式
  语义）。
- 创造即挖零耐久（MC 规则）；无掉落 = 无 XP 球（经济零参与）。
- sim 不 bump（存档态增量，survival 位级不变——canary EXACT 实证）。

## 变更文件（显式路径，28）

src/player/player.hpp src/player/player.cpp src/core/input.hpp
src/core/input.cpp src/core/game_ui.cpp src/core/game_interaction.cpp
src/core/save.hpp src/core/save.cpp src/core/game_save.cpp
CMakeLists.txt tests/test_save.cpp tests/test_player.cpp
tests/test_debug_overlay.cpp tests/test_bed.cpp tests/test_campfire.cpp
tests/test_drown.cpp tests/test_durability.cpp
tests/test_enchant_save.cpp tests/test_save_gzip.cpp AGENTS.md
README.md docs/CHANGELOG.md docs/BACKLOG.md docs/GAMEPLAY.md
docs/MECHANICS.md docs/PITS.md docs/KNOWN_ISSUES.md
docs/archive/phase-3.17.0.md
