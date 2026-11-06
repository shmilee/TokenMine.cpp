# Phase v3.16.0 — V3-15 生存/表现小批（门 206 全 PASS，2026-11-06）

> 用户 11-06 目标定案「完成 3.x 任务矩阵，从小到大推进」第 2 项。
> 三小项打包 1 版：netherite 火·熔岩免疫 + portal UV 动画 + 蒸汽粒子。
> 零 format / 2 个新 config 键（严格 bool 默认 true，off = 位级）。

## 范围与定案

### ① netherite 火·熔岩免疫（V3-4 记档后续兑现）
- 物品可选属性 **`fire_resistant`**（strict bool，默认 false；**armor-only**
  = parseArmor 守卫——非护甲携带点名拒，shears tool-only 孪生；物品
  字段白名单 +1）；4 件 netherite 护甲 JSON = true。
- **任一件穿上**（`setArmorSlot` 单收口 ANY 派生同步，points 同款模式）：
  - 熔岩 burn tick 不落地（`m_lavaAccum` 重置 = 脱件无余伤）
  - 火/熔岩接触**不再点燃**（Phase-57 火效果重应用门控）
  - 既有火效果伤害 tick 抑制（视觉保留 = MC fire-resistance 语义）
- **熔岩死因覆写**（门内实证捕获）：首版对照臂死因 = 「死于燃烧」
  （fire 效果 tick 先落地）→ damage 致死分支：Effect(fire) ∧ inLava
  → 记 Lava = MC 元素归因（「死于熔岩」）。
- **击退抗性 = 弃做**（记档偏离）：项目无玩家击退面（箭只伤不推、
  mob 近战只伤不推）——数据字段无消费方 = 死数据，违反数据驱动纪律。

### ② portal UV 动画
- 纹理 **水平 2 周期**重写（`generate_block_textures.py` nether_portal
  生成器全部列函数取 `x % 8`——左半 ≡ 右半字节级；全量重生成 = 其余
  164 张位级不变，git status 单文件实证）。
- `basic.frag` **tile-rect 分支**：`u_portalAnim`（config `portal.anim`
  严格 bool 默认 true）门控下，sampleUv（tiled / legacy 双路径收敛的
  atlas 空间坐标）落在 portal tile rect 内 → `x += phase`（fract 回绕，
  2 周期 = 无缝）；**off = 分支不执行 = pre-v3.16.0 位级**（ripple
  同款门控纪律）。
- `u_portalPhase = m_frameCount % 120 / 120` 纯函数（frame-locked，
  周期 = 命名常量 kPortalAnimPeriodFrames 120f = 2 s/整 tile 滚动）。
- init fail-fast：atlas 缺 `nether_portal` tile 点名（`u_portalTile`
  per-run 常量 init 期设定）。
- 非 MC 独立动画纹理 = 单 tile 滚动（记档偏离）。

### ③ 蒸汽粒子
- `updatePortalSteam`：每 6 帧（kPortalSteamEveryNFrames）一次，玩家
  脚下 **8×8×9 盒**（y 非对称 feet-4..feet+5——门内首版盒 y+3 漏
  portal 顶框行 = 场景实证捕获，player 站位差 1 格即漏顶行）扫描
  portal 格，每格 **1/10 确定性 roll**（fire 种子契约 + 专属盐
  kPortalSteamRollSalt）发射 1 粒。
- 粒 = `spawnSteam`：portal 自身纹理的 0.12 半透立方体（translucent
  块粒子走 blend pass = 天然蒸汽形态），-0.4 m/s² 慢升，0.8-1.4 s
  寿命，初速小漂移（linger 观感）。
- config `portal.steam` 严格 bool 默认 true（fire.particles 先例，
  off = 零发射）。
- 营火 = 既有火焰粒子覆盖（不重复造）；距离 >4 块的 portal 不发射
  （盒限 = 性能纪律，记档）。

## 实现（代码 11 文件 + 数据 6）

- `src/item/item.hpp`（fireResistant 字段）/ `item_registry.cpp`
  （parseArmor 守卫 + 解析 + 白名单）
- `src/player/player.hpp/cpp`（setArmorSlot +1 参默认 false 位级保
  3 参调用点 / m_armorFireResistant ANY 派生 / hasFireResistantArmor /
  lava tick 抑制 / 熔岩死因覆写）
- `src/item/armor.cpp`（装备 2 点传参）/ `src/core/game_save.cpp`
  （装载传参）
- `src/core/game_ui.cpp`（点燃重应用门控 + 火 tick 抑制）
- `src/core/game_frame.cpp`（updatePortalSteam + 常量 + 调用点）
- `src/entity/particle.hpp/cpp`（kSteam* 常量 + spawnSteam）
- `src/core/game_init.cpp`（portal.anim init uniform + tile fail-fast
  + portal.steam config 读）/ `src/core/game_render.cpp`（per-frame
  phase uniform + 周期常量）
- `assets/shaders/basic.frag`（tile-rect scroll 分支）
- `tools/generate_block_textures.py`（portal 2 周期）+
  `assets/textures/blocks/nether_portal.png`（重生成）
- 数据：netherite 护甲 ×4 JSON `fire_resistant: true`

## 门 206（全 PASS）

- **S1 结构免疫第 86 次**：无输入 60f ×2 ≡ base19 逐字节 + savenorm
  EXACT（shader 分支 / 蒸汽 / 免疫全为死亡态·场景态面——终态树复核）。
- **S2a portal 场景臂**（场景手术：4×5 obsidian 框 + 2×3 内域 +
  states 6 条目【stateful 必带 = PITS 383】+ 地板平台 + **去火**
  【种子 fresh 档自带 3 团地下火 = real-dt 污染，PITS 398①】）：
  - 存档 ×2 **RAW 恒等**（无火域恢复）
  - **动画关控制臂**（portal.anim/steam = false）×2 RAW 恒等（管线
    确定性 = 位级）
  - 动画开单跑目检：漩涡流动 + 蒸汽点（f400/f460 diff 3560 px =
    scroll + steam 在动）
  - 截图 ×2 恒等对**动画内容非有效断言**（退出渲染帧 off-by-one 抖动
    × 高频漩涡纹理 = ~1300 px 相位差，全在 portal tile 区——PITS 398③；
    权威 = 存档 ×2 + 动画关控制臂 + 单跑目检三件套）
- **S2b 熔岩池双臂**（挖掘池 + 石环 + 流体日志 `[x,y,z,7,'lava']`
  【PITS 398②：fluids 数组 level 前置于 name；armor = 命名 dict 非
  数组；池格先挖 air 再填 = cell occupied 拒载规避】）：
  - 免疫臂（chest = netherite_chestplate）：health **20 ×2** + 存档
    ×2 RAW 恒等（免疫 = 无 real-dt tick = 确定性域）
  - 对照臂：health **0 ×2** + 死亡屏「死于熔岩」目检（real-dt 场景域
    = health 断言权威，协议场景例外同款）
- **S3 全套 2795/2795**（+3：FireResistantArmorField【4 面：穿件/缺省/
  非护甲拒/非 bool 拒】/ SteamIsDeterministicAndRises /
  NetheriteArmorSuppressesLavaBurn【免疫 4 s 满血 + 脱件恢复伤害】），
  `-Werror` 0 警告。
- **S6**：无新解析面（fire_resistant = 既有 strict 模式；2 config 键 =
  既有 getBool 严格纪律）。
- **性能 A/B（用户硬要求）**：旧 = 55da85d v3.15.0 + g139 vs 新 +
  g139（banner 实证）：900f 无输入——PERF-WIN 三窗 upd med
  0.541/0.648/0.648 vs 0.650/0.657/0.583（窗口间机器噪声带）+ render
  0.585/0.739/0.738 vs 0.694/0.688/0.657 + swap med
  15.302/15.064/15.063 vs 15.085/15.107/15.180（新侧 win1 max 31 ms =
  一次性 OS 停顿 med 正常）+ fluid·fires·spread·ents·stream 同档 +
  **changesApplied 9/3/1 逐位恒等** + **终档 savenorm EXACT**——蒸汽/
  免疫/scroll 在无输入场景全惰性（无 portal 无火无护甲）= 结构性零
  成本。

## 记档偏离

1. **击退抗性弃做**（无玩家击退面 = 字段无消费方）。
2. **portal 动画 = 单 tile 2 周期滚动**（非 MC 独立动画纹理）。
3. **蒸汽 = 玩家 8×8×9 盒限**（>4 块 portal 不发射）。
4. **熔岩死因覆写**（fire 效果致死一击 ∧ inLava → 「死于熔岩」= MC
   元素归因，非严格 last-damage 字面）。

## PITS

- **PITS 398**（门 206 四捕获）：① 场景 fresh 档自带 worldgen 地下火
  = real-dt 污染（去火 = 移除 fire 日志条目回落生成值，**勿加 air
  覆盖** = duplicate block edits 拒载）② 存档手术格式（fluids =
  `[x,y,z,level,name]` level 前置 / armor = 命名 dict / 池格先挖后填）
  ③ 动画内容门协议（截图 ×2 恒等非有效断言；权威 = 存档 ×2 + 动画关
  控制臂 ×2 + 单跑动画目检/双帧 diff）④ MC_LOG_* = 单参消息串（无
  printf 格式，插桩用 to_string 拼接）。

## 变更文件（显式路径，30）

src/item/item.hpp / src/item/item_registry.cpp / src/item/armor.cpp /
src/player/player.hpp / src/player/player.cpp / src/core/game.hpp /
src/core/game_init.cpp / src/core/game_frame.cpp / src/core/game_render.cpp
/ src/core/game_ui.cpp / src/core/game_save.cpp / src/entity/particle.hpp
/ src/entity/particle.cpp / assets/shaders/basic.frag /
assets/textures/blocks/nether_portal.png /
assets/data/items/netherite_{helmet,chestplate,leggings,boots}.json /
tests/test_item.cpp / tests/test_particle.cpp / tests/test_player.cpp /
tools/generate_block_textures.py / CMakeLists.txt /
tests/test_debug_overlay.cpp / AGENTS.md / README.md / docs/CHANGELOG.md
/ docs/BACKLOG.md / docs/GAMEPLAY.md / docs/MECHANICS.md /
docs/MECHANICS_WORLD.md / docs/PITS.md / docs/KNOWN_ISSUES.md /
docs/archive/phase-3.16.0.md（本文）。

Release 政策 = 延续：v3.16.0 只留 **tag + 提交**，无新 release。
