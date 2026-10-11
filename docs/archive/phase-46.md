# Phase 46 细节存档

> 代理自定案范围（用户"继续任务"未反对，可叫停）：状态效果系统 =
> ① poison/weakness/regeneration 数据驱动效果（纯 JSON + 注册表严格校验，
> 真实 dt 倒计时 + 跨 interval tick）；② **spider mob**（毒源，首个
> `on_hit_effect` mob，推荐项①）；③ 效果图标 HUD 行（心行左对齐，
> effects.png 3-tile 条带）；④ 牛奶清负面效果补全（P43 遗留"效果集 =
> 清红闪一项"）；⑤ 存档 format 8（`player.effects`）。

## 阶段报告

- **`[2026-10-12, Phase 46]`** 状态效果系统
  — 835/835（+22），`-Werror` 恒 0 警告；E2E 批跑全绿（第 5 次全批；
  S1 第 20 次结构性免疫免重锚 + S3/S4 tick 精确 + S5 牛奶清负面全链路
  + S6 spider 游戏内）。

### 实现内容

**核心模块 `src/core/status_effect.{hpp,cpp}`**
- `EffectRegistry`：`assets/data/effects/*.json` 严格解析（注册表纪律）——
  `tick_type` "damage"/"heal"（**缺省 = 无 tick** 纯修饰）、`tick_amount`
  (0,10]、`tick_interval` (0,100]、`min_health` [0,1] **仅 damage**
  （0 = 可致死 / 1 = MC 中毒地板保持 ≥1 HP）、`melee_penalty` [0,100]、
  `negative` bool 缺省 false（牛奶只清负面）、`icon` ≥0 必填；**no-op
  拒绝**（无 tick 且 penalty 0）、未知字段拒绝、id 非空唯一。
- `StatusEffects`：`apply`（重应用重置 duration、**保留 tick 相位**）/
  `clearNegative`（牛奶）/ `clearAll`（死亡）/ `update(dt,reg,onTick)`
  （真实 dt；跨 interval `while` 逐个结算、**到期帧不结算**）/
  `totalMeleePenalty`。floor/max 检查在 Game 回调侧。
- `Player::heal(amount)` 新 API（clamp 上限）。

**Game 集成（`game.{cpp,hpp}`）**
- `m_playerEffects` + `updateEffects(dt)`（`updateSurvival` 之后——
  同帧 tick 的伤害在下一帧 survival 轮询触发红闪/音效）。
- 死亡 `clearAll`；`clearStatusEffects()`（饮用完成）= 重置 `m_hurtFlash`
  + `clearNegative`（**正面 regen 保留** = MC 语义；fallStartY 刻意不清）。
- `selectedAttackDamage()` = max(0, base − totalMeleePenalty)（虚弱的
  徒手 = 0 伤害）。
- HUD 效果图标行：心行 y[646,661]、左对齐 x0=434、16px 间隔 2px、仅
  `locked && !panelOpen` 绘制；**无效果 = 不绘制**（无输入基线结构性
  免疫）。F3 `effects` 行（12 行）。
- 纹理 `effects.png`（48×16，3 tile）init 交叉校验 `icon < tile 数`。

**Mob `on_hit_effect`（`mob_registry.{cpp,hpp}` + game init）**
- `{"name","duration"}` **成对必填**严格校验（缺一/未知内键/类型错 =
  报错）、duration (0,100]、name 非空 + init 对 EffectRegistry 交叉校验
  （= ammo/挤奶模式）。**仅近战命中落定施加**（远程箭不施加）。

**spider（`assets/data/mobs/spider.json`）**
- health 16 / speed 1.0 / detect 16 / attack_range 1.6 / attack_damage 2 /
  cooldown 1.0 / wander 2.5 / `spawn_max_light` 0（只黑暗）/ hostile /
  `on_hit_effect {poison, 15.0}` / **无掉落**（string/spider_eye 未做）。
- AABB 1.4×0.9；atlas 空位 `spider_body [5,7]` / `spider_head [6,7]`
  （无 UV 移位）；`mob_spawning.json` 加 weight 1（第 6 项）。

**存档 format 8（`save.{cpp,hpp}`）**
- `kSaveFormat = 8`；`player.effects = [{name, remaining}]` **名字序**、
  必写必载（furnaces 键纪律）、名字对 EffectRegistry 校验、fmt 1–7
  缺键 = 空、**tick 相位不保存**（重载从整 interval 起算）。
- `parseSave` 新签名加 `const EffectRegistry&`（第 4 参）——
  test_save.cpp 72 处调用全量更新。

**内容（全 JSON + 纹理生成器）**
- 3 效果：`poison`（damage 1/2.5s + min_health 1 + negative + icon 0）、
  `weakness`（melee_penalty 4 + negative + icon 1，v1 无游戏内来源）、
  `regeneration`（heal 1/4.5s + icon 2，v1 无游戏内来源）。
- 新 `tools/generate_effect_icons.py` → `effects.png`（0 紫骷髅 / 1 深蓝
  下箭 / 2 绿上箭；确定性噪声 + 简单图元）。
- `tools/generate_mob_textures.py` 加 spider_tile（salt 0x5D11/0x5D12；
  既有 PNG 零修改 = salt 纪律）。

### 测试（+22 → 835）

- 新 `tests/test_status_effect.cpp`（+12）：EffectParse 4（poison/
  weakness 无 tick/regeneration/拒绝集）+ EffectRegistry 2（重复 id/
  查找）+ StatusEffects 6（重应用重置 duration 保留相位 / damage 跨
  interval 结算 / **min_health 地板不在此层**（Game 回调侧）/ heal tick /
  清负面保正面 / totalMeleePenalty）。
- test_mob（+3）：on_hit_effect 缺省 None / 解析 / 坏值拒绝。
- test_save（+5）：SaveFormat8Test（名字序 round-trip / 空数组合法 /
  fmt7 无键可载 / **fmt8 缺 effects 键拒绝** / 坏条目拒绝）+ 断言
  format 7→8。
- test_player（+1）：heal 累加 + clamp 上限。
- test_debug_overlay（+1）：effects 12 行。

### E2E（批跑，全绿；`/tmp/opencode/p46_e2e.zsh`，第 5 次全批）

- **S1 base18 门**：P46 ×2 + **P45 代码参考构建（own worktree
  `/tmp/opencode/p45ref` @dc958c3）** ×1 新世界 60f **三方字节一致 =
  第 20 次结构性免疫（免重锚）** + skygate + 红心 0。效果图标行在无输入
  下不绘制 = 结构性免疫（与 P41 起各 HUD 行同纪律）。**无跨构建 A/B**：
  spawn 表加第 6 项改变 `pickIndex` 权重映射（S2 同因）。
- **S2 回归**（p27far 夜 360f seed 9，mobs on）：P46 ×2 存档一致（剥
  real-dt remaining = **cmpf_norm**——S3 首拍栽过：带活跃效果的场景
  直接 cmp 必败，PITS 新坑）。
- **S3 毒 tick**（正午 still mobs off 180f，场景注入 poison 30s）：
  health **19（恰 1 tick @2.5s）** + remaining 25.60/25.64 ∈
  [25.1,25.9] + ×2 存档剥离后一致。
- **S4 再生**（240f，注入 regen 30s、起始 health 10）：health **11
  （恰 1 tick @4.5s，10→11）** + remaining 23.96/23.78 ∈ [23.4,24.6]。
- **S5 牛奶**（240f，注入 poison+regen 30s、slot0 milk_bucket）：
  - s5ctl（无输入）：双效果存续 + milk 未动（save-only 控制）。
  - s5lock（仅锁）：双图标（icon0 紫 45px / icon1 绿 20px）+ health 19
    （2 毒 tick −2 + 1 再生 +1）+ 红心 640>50。
  - **s5 饮用**（重试环 3 次，成功门 = slot0 == bucket）：毒 tick 间隙
    喝——mousedown **锚定 `Entering main loop` 日志时间戳** @ 帧时 2.9s
    （0.8s 窗口 2.55..3.35；固定 sleep 不可行：launch→帧 0 偏移 ~0.6s
    不确定度 + **受击中断饮用通道** P28 规则使 ~0.9s 开喝必撞 2.5s 首毒
    tick = 3/3 确定性失败，PITS 新坑）：health **精确 = 20**
    （20−1@2.5 tick1 +1@4.5 regen，通道期 2.9–4.5s 无伤害）、
    effects = [regen]（**poison 清除、regen 保留** = MC 语义）、
    slot0 = bucket、icon0 紫 ≤5 + 绿 ≥20、红心 670。
  - 像素检查：天空锚点 (100,100) = (120,181,242)±3（过期帧/红纱检测器）
    + 图标严格 + **世界 diff 上界 600px（非零）**——实测 467px
    （**WM 运行中途重 tile 窗口 → 前缓冲冻结 → 截图 = 过期帧**：远景
    剪影 27px 条带 / 全帧红纱 tint α≈0.22；存档循环后按真实状态写出
    不受影响 = 确定性权威 = PITS 新坑）。
- **S6 spider 游戏内**（夜 720f seed 9，mobs on，×2）：存档一致（新
  spawn 条目下确定性）+ **spider spawn 1**（咬中概率性——18s 内生成 +
  贴近；机制权威 = S3/S5 注入路径，S6 = 游戏内 spawn 集成证据）。

### 已知问题（P46 新增/变更）

- KNOWN_ISSUES「牛奶桶」P43 首条（状态效果系统不存在）**已修复**
  （strikethrough + P46 说明）。
- KNOWN_ISSUES 新「状态效果（Phase 46）」节：weakness/regeneration 无
  游戏内来源（药水/金苹果未做）、中毒无独立紫色 tint（复用红纱）、
  tick 相位不保存、效果只作用于玩家 v1（mob 不持、箭不施加、无
  amplifier）、no-op 效果拒绝。
- KNOWN_ISSUES「Mob」新增：spider 无掉落（string/spider_eye 物品未做）。
- PITS +4：WM 过期帧截图（天空锚点 + 世界 diff 上界 + 存档权威）/
  带活跃效果场景存档 ×2 须剥 real-dt remaining（cmpf_norm）/ 受击中断
  饮用 → 毒 tick 间隙喝（锚定 main-loop 日志时间戳）/ Read 行号前缀含
  空格 → 文档缩进比显示少 1。

### 提交

- 显式路径列表提交（36 文件：11 src【含新 status_effect.{hpp,cpp}】+
  5 tests【含新 test_status_effect.cpp】+ 2 tools【含新
  generate_effect_icons.py】+ 6 data【3 新 effects json + spider.json +
  mob_spawning + block_atlas】+ 3 纹理【effects.png + spider ×2】+
  8 docs【6 既有 + archive/phase-46.md + AGENTS.md】+ CMakeLists.txt；
  新文件先 `git add <path>`；`git commit -F`）。
- `SESSION_CONTEXT.md` 本地专用不入库。
