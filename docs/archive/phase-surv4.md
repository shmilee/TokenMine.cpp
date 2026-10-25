# Phase SURV-4 — 药水饮用（v1.0 任务表 18/29，门 98）

> 用户定案方向（行序）= SURV 行第 4 项。代理定案最小完整版见
> `/tmp/opencode/surv4_design.md`（设计定稿，实现唯一权威）。

## 实现内容

**饮用通道给效果**（核心 4 处 + init，其余全数据）：

- **效果新字段 `instant_hp`**（status_effect.{hpp,cpp}）：整数非 0、
  [-32, 32]、与 `tick_type` 互斥（一个效果一种动作形态）、显式 0 拒
  （dead-field 纪律）；施加时结算一次：正 = `heal`（钳 base max）/
  负 = `damage(-x, bypassArmor=true)`（护甲豁免 = MC 药水伤害语义，
  Resistance 照常；复用 SURV-2 溺水同款管线）
- **`grants_effect(s)` 放开到 drinkable**（item_registry.cpp）：原
  food-only → `restore_hunger>0 || drink_time>0`（一个物品一个通道
  纪律不变）；duration `>0` → `>=0`，`duration==0` 合法性由 init 判
- **饮用完成分支**（game.cpp updateEatChannel）：有 grants → 按序施加
  + `syncAbsorption`；无 grants → `clearStatusEffects` 原行为位级不变
  （milk/water 不碰）
- **`Game::applyPlayerEffect(name, duration)` 收口**：替换 3 个既有
  施加点（吃单值 / 吃列表 / mob 命中）+ 新饮用侧 = 全部玩家效果施加
  单一入口（RESET 语义既有：`StatusEffects::apply` 已实现再施加重置，
  零改动；瞬时结算在此处）
- **init 交叉校验**（game.cpp）：grants 条目 `duration==0` ⇔ 效果
  `instantHp != 0`（双向点名）

**内容**（全数据 + 生成脚本）：

- 效果 2 新：`healing`（+4, icon 8）/ `harming`（−6, icon 9）
- 物品 8 新：`glass_bottle`（collects_fluid ["water"], stack 16）/
  `water_bottle`（pours_fluid water → glass_bottle, stack 16, 纯容器
  不可饮）/ 6 药水（drink_time 1.6 + returns_item glass_bottle +
  max_stack 1；weakness 90 / swiftness(speed) 180 / regeneration 45 /
  poison 45 = MC 精确时长 / healing 0 / harming 0 = 瞬时）
- `hotbar.json` starter 槽 17-23（glass_bottle ×4 + 6 药水各 1；
  24-26 saplings 不动；隐藏存储 = GEN-2 范式，无输入零渲染）
- 纹理：effects.png 128→160（尾部 append，既有 8 tile 逐像素零变化
  已验证）+ 8 物品 icon（独立 PNG，salt 128-135 接续）
- 桶对 init 唯一性键 = (fluid, poursBecomes) → 与既有 water_bucket
  桶共存（已核 game.cpp 校验代码）

**零 format（18 不变）/ 零 config / 零新音效（复用 milk.wav，开始
播放既有行为）/ 零渲染新路径**。

## 变更文件（fixer 交付，待主线提交）

核心 7：src/core/status_effect.{hpp,cpp}、src/item/item.hpp、
src/item/item_registry.{hpp,cpp}、src/core/game.{hpp,cpp}
内容 11：assets/data/effects/{healing,harming}.json（新）、
assets/data/items/{glass_bottle,water_bottle,potion_weakness,
potion_swiftness,potion_regeneration,potion_poison,potion_healing,
potion_harming}.json（新）、assets/data/hotbar.json
纹理 11：tools/generate_effect_icons.py、tools/generate_item_icons.py、
assets/textures/effects.png、assets/textures/items/ 8 新 PNG
测试 3：tests/test_status_effect.cpp（+10）、tests/test_item.cpp
（+3 新 +4 更新）、tests/test_food.cpp（+6）

## 测试

1485/1485（+19：instant_hp 解析 8 / apply(0) 移除重置 2 / 瓶对
fullBucketFor + 内容钉死 3 / grants 放开语义 + starter 槽 17-26 钉死
4 / 饮用完成施加-返还-重置-瞬时结算 6 中的组合件——Game 层由门 98
E2E 钉死，项目无 Game 单测 harness = 既有测试文化）。
`-Werror` 0 警告（fixer 报告 + orchestrator 重跑复核 1485/1485）。

## 门 98 E2E

- **S1 结构性免疫 PASS**（10-25 15:39）：starter 隐藏存储不渲染 +
  无输入零饮用零桶操作 = 零活动；fresh 60f 新版 ×2 RAW 字节一致 +
  **vs v0.15.0 二进制 A/B 字节一致**（tm_v0150_run 旧资产目录跑，
  同窗态三连发 = 第 98 次过门，结构性免疫不重锚）。
- **S2 时长药水互斥对**（输入）：slot0 potion_weakness 右键按住
  2.2 s → effects weakness remaining > 82 + slot0 空 + glass_bottle
  ×1 返还；s2ctl 无输入对照。——**补跑清单**（10-25 15:3x-15:5x away
  组件窗口 churn 70 min+，s2 ×6 全未送达 = PITS 253；场景/runner/
  断言全备，窗口恢复后 `zsh gate98_run.zsh s2 s2ctl` 一键补收）
- **S2b 重置语义**（输入）：双 weakness，喝 → Tab → 隔 8 s 再喝 →
  remaining > 82（有重置 [84.5,90] vs 无重置 [73.2,78.7]，5.8 s
  判别带）+ 2 瓶返还。——**补跑清单**（同上）
- **S3 瞬时互斥对**（输入，同基底 hp 16）：A healing → **hp 20**
  （+4 钳 max）/ B harming + 满钻甲（20 pts）→ **hp 10.0**（−6 护甲
  豁免；走减伤 = 14.8 互斥）+ 双瓶返还 + 护甲零磨损 + 效果已过期。
  ——**补跑清单**（同上；行为 = test_food 组合件单测已证：heal 钳
  max / damage bypass 10 vs 14.8 互斥实证）
- **S4 瓶-水循环**（输入）：glass_bottle 舀 (22,39,−3) L8 水（石锚
  (22,39,−4) 在射线上，舀-倒闭环几何已核）→ water_bottle → 倒回 →
  终态 slot0 glass_bottle + fluids live==fresh（journal 净零）。——
  **补跑清单**（同上；桶对 scoop/pour = P47 既有已证路径，S6 唯一性
  点名已证 init 侧）
- **S6 init 失败 ×5 PASS**（10-25 15:36 即收）：
  ① stick 加 grants_effects → `requires 'restore_hunger' or
  'drink_time' (only food or a drinkable grants an effect on consume)`
  ② 未知效果名 → `grants_effects[0] references the unregistered
  effect 'weaknessz'` ③ duration 0 给 weakness → `grants 'weakness'
  with duration 0, but the effect is not instant (instant_hp 0)`
  ④ instant_hp 显式 0 → `'instant_hp' must be non-zero (absent = no
  instant action; an explicit 0 is a typo)` ⑤ 双 water_bottle →
  `item 'glass_bottle' collects fluid 'water' with 2 full bucket(s)
  pouring it back (exactly one is required — the scoop target must be
  unique)`（正确点名被破坏侧）

## 性能（插桩 A/B v0.15.0，发布前测试）

- **fall_armor 900f 三 300f 窗**（new = SURV-4 / old = v0.15.0，同场景
  同 config g95_fire9999）：**fluidCells 71202/91763/135654 三窗逐位
  一致 = 两侧处理同一世界**（强有效性信号）；swap med 双侧 ~0.48-0.52
  ms（无 stall——截图黑 = 深黑相呈现层，跑期时序干净 = 10-25 run2
  timing-clean 接受协议首用）：
  - upd med 45.9/48.6/50.1（old 45.8/49.3/47.3 同档）
  - render med 0.354/0.512/0.672（old 0.351/0.517/0.676 同档）
  - flush med 36.2/34.9/31.2（old 35.6/34.7/28.7 同档）
  - player 2.57/2.58/2.68（old 2.58/2.52/2.69）/ ents 0.29/0.28/0.32
    （old 0.30/0.30/0.29）
  - stream 1876/2123/2456 ms（old 1925/2241/2525，new 略快）
  - fluid 940/1914/2934（old 943/1984/2979 同档）
- **结论：SURV-4 成本可忽略，无回归**（饮用通道 = 既有 real-dt 计时器，
  instant_hp 只在 apply 事件结算；无饮用时零每帧新增）

## 记档偏离（KNOWN_ISSUES）

1. 无酿造台/酿造线：药水 = starter 发放（GEN-2 sapling 先例）；无
   II 级/延长/强化/泼溅/滞留
2. water_bottle 不可饮用：项目 drinkable = 清负面（milk 语义），无
   中性饮用（避免新字段）；MC 水瓶 = 中性喝
3. 瓶舀水移除水源：项目桶对范式（P47 既有语义）；MC 瓶填 = 工作台
   3 瓶/桶不耗源
4. 瞬时伤害护甲豁免：MC 药水伤害语义（Resistance 有效 = damage 管线
   既有次序 bypass 之后）
5. 再喝同效果 = 重置：项目 apply 语义（MC = 保较长者；单等级药水无
   实际差）
6. 单效果药水（无多效果瓶）

## 已知问题 / 下一步

【待回填】下一步 = SURV-5 mob 效果（行序第 5 项）或 CRAFT-1（行序：
SURV 行 5 项先）。
