# Phase V2-11 — XP 系统（v2.12.0，门 183 全 PASS）

> 2.x 第二批 SURV 行首项（原 3.x 八项并入之第一项）。**版本 = 执行序
> v2.12.0**（V2-19 用户诉求提前项先占 v2.11.0，任务号与版本号解耦 =
> V2-1/V2-2 同款先例）。实现源 = fixer 半成品（43 文件，11-03 被用户
> 中断取消 → git stash「V2-11 XP partial」）11-04 stash pop 零冲突恢复
> 至 v2.11.0 之上 + 编排者修复 3 处 WIP 编译破损 + 1 处 save.cpp 潜在
> bug（PITS 361 违例）。设计定案 = /tmp/opencode/v211_design.md（MC
> 公式 11-03 编排者 web 核定入稿，恢复时无需再核）。

## 实现

### XP 曲线（core/xp_curve.hpp，全整数）
- 玩家携**单个整数** `Player::m_xpTotal`（存档 player.xp，format 26）=
  累计经验总点数；**等级永不存储** = 由池派生（单源真值，存储级 =
  第二份可漂移拷贝）。
- 现代 MC 公式（wiki "Experience: Useful numbers" 核定：0→1 = 7、
  0→30 = 1395、27→30 = 306，段界连续，三式互验）：
  - `xpNeededForLevel(L)`（L→L+1 条成本）：L≤15: 2L+7 / 16≤L≤30:
    5L−38 / L≥31: 9L−158（恒 ≥7 = HUD 除法不除零）
  - `totalXpForLevel(L)`（达 L 累计）：L≤16: L²+6L / 17≤L≤31:
    (5L²−81L+720)/2 / L≥32: (9L²−325L+4440)/2
  - `levelFromTotalXp(total)` = **整数二分**（上界倍增；严格递增 →
    往返 totalXpForLevel(levelFromTotalXp(x)) 构造性恒等，无浮点
    逆函数）
- 段界全钉单测（tests/test_xp_curve.cpp：T(0)=0/T(1)=7/T(16)=352/
  T(17)=394/T(30)=1395/T(31)=1507/T(32)=1628 连续性 + 往返）。

### 经验球（entity/exp_orb.{hpp,cpp}）
- 值 1..9（**无合并无分裂** = 记档偏离，MC 同值球合并；项目 orb 是
  独立实体，ItemEntity 同款物理纪律：帧锁定重力/碰撞/pickup delay
  0.5 s real-dt 双时钟）
- 磁力 = V2-6 拾取磁力同款帧锁定拉取（READY 球，半径 2.5）；拾取 =
  游戏 pickup pass 收 1.0 内 READY 球 → `addXpTotal(值)` + 球移除 +
  "pickup" 音色（复用）
- 渲染：game_render `drawExpOrbs`（view/projection 既有通道）。

### 三发放源（数据驱动，核心不 switch 内容 id）
- **mob 击杀**：mob JSON 严格字段 `xp`（[0,9]，>9 点名拒）——hostile
  zombie/skeleton/spider 5 / passive pig/cow/sheep/chicken 2（MC 1-3
  中值 = 记档定案值）
- **熔炼完成**：item JSON 严格字段 `smelt_xp`（[0,9]；**默认 1 统一**
  = 记档偏离，MC 分数表 iron 1/other 0 类；0 = 无球数据门）
- **挖掘完成**：block JSON 严格字段 `mine_xp`（[0,9]，默认 0）——
  coal_ore 1 / diamond_ore 5 / iron_ore 0 / gold_ore 0（现代 MC
  1.17 raw 化后铁金矿挖掘无 XP = 记档定案值）
- 球生成 = 掉落点（破坏块中心 / mob 死亡位 / 炉位），既有 dropPos
  范式。

### format 26（存档三向键纪律，V2-3 tutorial 同款）
- `player.xp` **恒写**（缺键 = 版本谎报）；entities 数组第二 kind
  `type: "xp"`（{pos, age, pickup_delay, xp}；type ∈ {item, xp}）
- pre-26 文件（25）载 = xp 迁 0 + 无 xp 实体（velocity 零化迁移同款）
- 26 文件带 25 形状 / 25 文件带 player.xp = 点名拒
- **save.cpp 潜在 bug 修复（PITS 361 违例第二实例）**：tutorial
  白名单 `isV25 = fmt >= kSaveFormat` —— kSaveFormat bump 到 26 后
  format-25 文件被误判非-25 → series 键点名拒（V2-3 引入时
  kSaveFormat=25 恰好没炸）。修为引入格式字面量 `>= 25`（1667 行
  UX-8f 同款先例注释佐证：「kSaveFormat moved on ... the old >=
  kSaveFormat form would misclassify」）。

### HUD（恒可见 XP 条 + 等级数）
- 热栏正下方（kXpBarGap 4 / kXpBarHeight 2，既有坐标范式）：条填充 =
  (total − T(L)) / needed(L) 全整数数学 + 等级数居上。字体 = 既有
  ASCII 数字（零新字形）。无输入也画 = **门 183 唯一像素变更源**。
- F3 调试行 +xp 读法（既有 overlay 扩字段）。

## 变更文件（stash 恢复 43 + 编排者修复）
- 新：src/core/xp_curve.hpp / src/entity/exp_orb.{hpp,cpp} /
  tests/test_xp_curve.cpp / tests/test_exp_orb.cpp
- 数据：blocks/{coal_ore,diamond_ore,gold_ore,iron_ore}.json（mine_xp）/
  mobs/7 个（xp）/ items 族（smelt_xp 默认 1，显式值按需）
- 核心：save.{hpp,cpp}（format 26 三向 + isV25 修复）/
  block_registry / mob_registry / item_registry（三字段严格解析）/
  game_frame（熔炼发放）/ game_interaction（挖掘发放 + 击杀发放）/
  game_entities（orb 实体 pass）/ game_render（drawExpOrbs）/
  game_ui（XP 条 + F3）/ player.hpp（m_xpTotal）
- 测试：test_save（SaveFormat26Test 族）/ test_block / test_item /
  test_mob / test_campfire / test_drown / test_durability /
  test_retained_column / test_debug_overlay（版本钉）
- CMakeLists.txt：2.11.0→2.12.0（单点）+ exp_orb.cpp + 2 新测试
- 编排者 WIP 修复：tests/test_save.cpp 3 处（EXPECT_EQ 4 参宏
  Vec3 字面量 → 临时变量 / `ents.at(0)` const 无整数下标 → 拷贝重构 /
  lambda 内 ASSERT_TRUE → if+ADD_FAILURE 显式返回）

## 门 183（编排者执行；old 侧 = tm_v2120_base/perf = 28d107e v2.11.0）— 全 PASS

### S1 = base18 第 4 次重锚论证（Phase 18 先例）
- 旧侧（v2.11.0）60f ×2 恒等且 o1 ≡ 现锚 178676 B 逐字节（环境干净）
- 新侧 ×2 恒等（179027 B）
- **新 vs 旧 diff = 2031 px，bbox (30,714)-(647,740) = 仅底部 XP 条带**
  （y<700 零像素）；savenorm diff = 恰好 {format 26 vs 25,
  player.xp 新增} 两处预期键（世界恒等）
- **新锚 = 179027 B 采用**（锚谱系：179116 → 178148 → 178676 →
  179027）

### S2 = 三发放源 E2E（输入协议 PITS 376 retry capture + 帧限收尾）
- **熔炼（无输入）**：furnace (0,54,-3) pork+coal，玩家 (0.5,54,-4)
  = 1.0 格（磁力半径内；V2-7 的 -6.5 pose 3.5 格够不着 = 换 pose）。
  900f：new xp=1（恰 1 次 10 s 熔炼）/ old format 25 无 xp 键
- **击杀（输入）**：scratch spider-only 树（mob_spawning spider
  max_mobs 1，s7 范式）+ 夜 0.70 + tanky 60hp（**mobs.enabled 必须
  true**——quiet config 默认 false = 首跑零生成假象）+ pose (0,52,-16)
  yaw 0.3 pitch −0.6 + 2 s 短按住 = 恰 1 杀（spider f300 恒停，掉落
  string×2 + spider_eye×2 实证）。**新坑**：orb 落点 (−1.0,51,−18.1)
  距静止玩家 3D 2.54 > 磁力半径 2.5（差 0.04 永不吸）→ kill 后
  W 步行 1.5 s 拾取（确定性：spider 恒停 = orb 位每跑一致）→ new
  xp=5 / old format 25 无 xp 键
- **挖掘（输入）**：coal_ore (0,54,-3) + LMB 5 s → new xp=1 / old
  format 25 无 xp 键
- 旧侧三跑全 format 25 无 xp 键 = **形状互斥**（旧解析器不载
  format-26 场景 → 场景 py25 降级：format=25 + 剥 player.xp + 剥 xp
  实体 = PITS 377）

### S3 套件
- **2249/2249**（2207 + 42 XP 族；编排者独立复跑）

### S6 点名（真二进制 + scratch 资产 + 存档手术）
- mob xp=10 → init 点名拒（'xp' must be in [0,9]）
- item smelt_xp=10 → init 点名拒
- block mine_xp=10 → init 点名拒
- format-25 存档携 player.xp=5 → 载时点名拒（unknown field 'xp'）

### 性能 A/B PASS 同世界同成本（用户硬要求）
- v2.11.0（tm_v2120_perf）vs v2.12.0（tm_v2120_perf_new，banner 双侧
  实证）：fall 900f 无输入 seed 123456789：
  - PERF-WIN 三窗全指标同档（upd med 0.40/0.36/0.38 vs
    0.57/0.58/0.56 ms；swap 15.5-15.8 = vsync 主导同值；fluid/
    fires/spread/ents/stream 全同档）
  - **PIPE3-S0 changesApplied 三窗 max 9/1/3 双侧逐位**
  - 终档 fluids 16/16 + blocks 36/36 集合恒等 + xp 键 old=False/
    new=True（唯一预期 diff）
  - **熔炼场景自成本**（同场景 900f：new = 球生成+磁力+拾取全程 vs
    old 无 XP）：全指标同档（upd med ~0.60 vs ~0.58）= 不可测

## 偏离（记档）
1. smelt_xp 默认 1 统一（MC 分数表 = 偏离）
2. mine_xp = 现代 MC 1.17+ raw 化值（coal 1 / diamond 5 / iron 0 /
   gold 0）
3. passive mob xp = 2（MC 1-3 中值定案）
4. orb 值 1..9 无合并无分裂（MC 同值合并 = 偏离）
5. 等级不存储（由池派生 = 设计纪律，非偏离）
6. save.cpp isV25 潜在 bug 修复（PITS 361 第二实例；UX-8f 先例同款）

## 门协议发现（PITS 377）
1. **旧侧跑新格式场景 = init 拒**（未知字段点名）：场景存档须降级到
   旧格式形状（format 回拨 + 剥该版新增键 + 剥新 kind 实体）
2. **orb 磁力 2.5 = 3D 边界**：击杀点距静止玩家 2.54 → 永不拾取 =
   xp 假零；静止玩家场景须击杀位 ≤2.5 或加 W 步行（spider f300 恒停
   = orb 位确定性）
