# Phase V2-15 — 16 色染色羊（剪毛/染色/彩色羊毛）（v2.16.0，门 187 全 PASS）

> 二批 ENT 行（BACKLOG:「中」：mob 持色【per-instance 状态】+ 16 色
> wool 掉落；消费「sheep 只掉白 wool」记档 KNOWN_ISSUES [Phase 41]）。
> MC 机制 11-04 编排者 wiki 核定（Sheep 页：shearing 1-3 wool 无伤 /
> 剪后须吃草再生 / 剪后不可染 / 死亡 1 羊肉 + 未剪 1-3 彩毛 / 出生
> 色 = biome 表）。**零 format**（mob 不持久化 = 实例态 session-only，
> 蝙蝠同款，savenorm EXACT 实证）。词表 = 项目 15 色（wool 族无
> lime；MC 16 含 lime = 记档偏离 1）。fixer 一次交付（ses_e5bfd9133
> ffevFklg85jtTwjYO），7 处自主定案全接受。

## 实现

### 新物品：shears（项目无剪毛工具 = 必加）
- `shears.json`：type tool / **durability 238（MC 精确）** /
  break_speed 2（MC）/ **`shears: true` 严格 bool 旗**（非 tool 携带
  点名拒 = armor_slot 双生纪律）/ max_stack 1 / 5 盒 item model
  （双铁环 + 刃 + 枢轴，z-fight 守卫逼环/刃 y 面分离 0.28/0.30）
  + icon tile（PIL 确定性新 salt）
- **`fast_against` 整字段省略**（fixer 定案 1：空 `[]` 非法——既有
  钉 FastAgainstRejectsEmptyList；absent = "不加速任何块"默认，比
  "最小合法集"更极小——点名任一块都会真实加速挖掘 = 行为副作用；
  后果 = SURV-8 无列表工具 1/10 错工具罚）
- 配方 shapeless 2 iron_ingot（MC 精确）
- 剪毛磨损 = 每次成功剪 +1（`kShearWear` 命名常量，kIgniteWear 双生）

### sheep.json 更新（V2-15 数据面）
- drops = [{wool, count_min 1, count_max 3}（MC 精确 1-3）,
  {mutton, 1}]
- **新严格字段 ×2**：`wool_drops: true`（死亡 pass 钩子判据 = 毛色
  随实例）+ `wool_weights`（[{color, weight}] 列表：color ∈ 15 染料
  词表 / weight 整数 1..10000 / 重名拒 / 空列表拒 / **孤字段拒 =
  严格对**（fixer 定案 2，milk pair 纪律））。shipped 值 = white 850
  / black 80 / gray 30 / brown 30 / pink 10（**偏离 2**：MC biome
  表 → 项目单一 temperate 表整数近似，demo 生态 = 温带）
  - 词表点名拒放 **init 交叉检**（`checkWoolDrops`，fixer 定案 3——
    解析期无 ItemRegistry 可见，与 ammo/milkWith/drops/model tile
    全部 init 交叉检同款纪律）
- model：body box 加 `name: "wool"`（渲染层 per-instance 变体挂点名；
  head/legs 位级不变）

### 纹理（+2 tile，atlas 120→121 + item icon）
- `sheep_skin`（bare 羊身 = 肉粉 (226,178,164) + 噪，salt 0x5EE4；
  block_atlas (11,6) 首空格，零既有移位 md5 实证）
- 毛色 15 tile = **复用 wool 族块 tile**（wool/wool_red/… 已在同一
  atlas；mob mesh 同款 AtlasLayout 解析；init 交叉检覆盖全 15 色
  wool 变体存在性）
- `shears.png` item icon（salt 152-154）

### 实例状态（MobEntity 新增，milkCooldown 帧纪律同款）
- `m_woolColor`（**生成时 per-mob 流首抽**加权 roll——非 wool_drops
  mob 构造零消费 = 位级红线，单测钉：非 wool mob draw1 = 游走决策
  vs wool mob draw1 = 色 roll，恰差一个流值）
- `m_bare`（bool）+ `m_woolRegrowFrames`（int 帧倒计时）：剪毛置
  kSheepRegrowFrames = **7200 = 120 s 帧锁定**（**偏离 3**：MC 吃
  草门随机 1/500 tick 期望 ~12 min，项目无草交互 → 固定帧 =
  确定性 E2E 可断言）；update 每帧 −1 至 0 → bare=false（恢复当前
  染色色 = MC「剪后保留染色」语义 ✓）
- accessor：woolColor()/isBare()/shearWool()（rollDropCount 于自家
  wool 条目范围 + 置 bare + 满窗）/dyeWool()（数据入参零流消费；
  bare 拒）

### 渲染（per-entity 变体网格，swingMeshes 范式）
- sheep 绘制：body box 纹理 = f(woolColor, bare) → **variant 键入
  per-entity 动态网格**（懒建 + despawn 释放 + 剪毛/染色/再生触发
  重建；`withWoolVariantTile` 纯函数 = 仅换 texture，几何位级不变）
- 非 wool_drops mob 走原静态网格零触碰（位级红线）

### RMB 交互（tryMilk 范式，链插入 tryMilk 后）
- **tryShear**：targetMob 命中 ∧ 手持 `shears` 旗 ∧ mob wool_drops
  ∧ !bare → **1 个 ItemEntity 携 roll count**（fixer 定案 5，死亡
  pass 双生）于 mob 位零速度 + pickup delay + "shear" 音空间化（新
  shear.wav：亮噪声 catch + 低通 parting 双「咔嚓」，seed 0x5B4A，
  既有 42 wav 重跑字节不变，SoundBank 43）+ 磨损 1（破碎走既有
  "break" 音）+ 吞点击；bare 再剪 = no-op 吞
- **tryDyeMob**：targetMob ∧ 手持 dye（dyeColor 非空 = 数据旗）∧
  wool_drops ∧ **!bare**（MC 剪后拒染，点击吞）→ 变色 + 耗 1 +
  "cloth" 音 + 吞
- 全数据旗判据，无物种 switch（shears 旗 × wool_drops 旗）

### 死亡彩色羊毛（game 死亡 pass 钩子）
- 纯函数 `woolDeathDropName`（fixer 定案 4，armor/bed 模式 → CPU
  帧钉可单测）：woolly → dyeVariantName("wool", 实例色)；bare →
  nullopt（跳过于 roll 之前）；非 wool mob 位级不变
- despawn pass drop 收集换用该函数；mutton 条目不动

## 变更文件（73：49 新 + 24 改）
新：shears.json + 配方 + shears.png / sheep_skin.png + shear.wav /
wool_drops.{hpp,cpp} / test_wool_drops.cpp（11）；改：sheep.json /
mob_registry（WoolWeight + rollWoolColor + 严格对）/ mob_entity
（实例态 + shearWool/dyeWool + 再生倒计时）/ mob_mesh
（withWoolVariantTile）/ item 族（shearsTool 严格 bool）/
durability（kShearWear）/ game.hpp + game_interaction（tryShear/
tryDyeMob + RMB 链）/ game_entities（变体网格构建 + 死亡钩子 +
despawn 清 2 新 map）/ game_init（checkWoolDrops 交叉检）/
block.hpp（kWoolDyeFamily）/ 3 生成器 / block_atlas.json（121）/
CMakeLists（2.16.0）/ 9 测试文件（FNV/计数/F3 钉）

## 门 187（old = v2.15.0 tm_v2150_base/perf = 248773b；编排者执行）— 全 PASS

### S1 = 结构免疫第 68 次（**免重锚 + 零 format 最强形**）
- 新侧 60f ×2 恒等 且 ≡ 现锚 179027 B 逐字节（sheep 出生色 = 流首
  抽但 60f 基线无 mob【first_spawn_frame 60 免疫根】+ 剪/染需 RMB +
  shears 不在默认库存 + 新 tile 惰性）
- **savenorm = EXACT**（零 format：与旧侧完全同形）

### S2 = 剪毛/染色输入 E2E（两 pass 确定性瞄准 + 弹幕范式）
- 场景：scratch 池 sheep-only / max_mobs 1 / min_radius 1.5 /
  max_radius 2.5 / **first_spawn_frame 900**（羊 f900 单生于首有效
  ring 格 = 确定性，pass 1 无输入读 `mob spawn f900: 'sheep' at
  (-0.5, 54, -1.5)`）；pass 2 算 yaw 0.321751 / pitch −0.493263 瞄
  准 + 库存 slot0=shears / slot1=dye_red + 玩家 (0.5,54,0.5)
- **弹幕**（羊 1.2 宽 > 1×1 围盒出生重叠挤出 = 弃围盒；羊游走 0.25
  b/s = 单发必失；命中窗 = f900 出生 → f1080 首次 wander 的 ~3s
  静止窗）：臂 a = 80×RMB@0.3s；臂 b/c = 40 密对（key 0.15s → RMB，
  对内 dye→shear 0.2s，key→RMB 0.05s 实证选择未生效 = RMB 用旧
  selected）；3000f 预算
- **断言 = DEBUG 日志行权威**（终档 wool 物品受磁力拾取 → 首空槽 →
  弹幕 RMB 误放置成块干扰，不作断言口径——PITS 381②）：
  - 臂 a ✓：`sheared 'sheep' (1 x wool)` + shears **damage 1**
  - 臂 b ✓：`dyed 'sheep' to red` + `sheared 'sheep' (1 x wool_red)`
    + dye 耗尽 + damage 1（染色保留语义实证）
  - 臂 c ✓：`sheared 'sheep' (1 x wool)` + **dye 1 保留 = bare 拒
    染**（无 dyed 行，两分支之一）
  - 臂 o ✓（旧侧 v2.15.0 同场景无 shears 物品）：**零 dyed/sheared
    行** + 无 wool 物品 + 无磨损 = 形状互斥

### S3 套件
- **2391/2391**（2356 + 35：wool_drops 11 / mob 15 / item 6 /
  durability 1 / sound 1 / atlas 1 / player_body 1；编排者独立复跑）

### S6 = 6/6 点名（真二进制 scratch 资产）
wool_weights 未知色 'violet'（init 交叉检点名）/ 零权重 / 空列表 /
孤 wool_drops 无 weights（严格对）/ shears 旗挂非 tool（pork）/
durability 0——全点名

### 性能 A/B PASS 同世界同成本（用户硬要求）
- 主对 fall 900f 无输入 seed 123456789（mobs off）：PERF-WIN 三窗
  全指标同档（upd med 0.39/0.39/0.40 vs 0.37/0.45/0.39 ms；swap
  vsync 同值）+ **changesApplied 9/1/3 逐位** + 终档 fluids 16/16 +
  blocks 36/36 集合恒等
- **羊群场景自成本**（双树池 patch sheep-only max 2 + mobs on +
  场景 t0.25 600f 无输入）：2 羊 **双树同 f60/f90 同位置**确定性
  出生【(0.5,54,-5.5) / (−2.5,54,−4.5)】+ ents **0.0606 vs 0.0600
  同档** = 变体网格路径（v2.16.0）与静态白网格（v2.15.0）同成本

## 偏离（记档 3 条 + fixer 自主定案 7 处全接受）
1. 词表 15 色无 lime（对齐 dye/wool 既有值，MC 16 含 lime）
2. MC biome 表 → 项目单一 temperate 表 850:80:30:30:10
3. 再生 = 固定 7200f 帧锁定（项目无草交互，MC 随机吃草门 ~12 min）
定案：① shears fast_against 整字段省略 ② wool_drops+wool_weights
严格对 ③ weight 词表拒放 init 交叉检 ④ 死亡 pass 纯函数化
⑤ 剪毛 = 1 ItemEntity 携 roll count ⑥ shears 5 盒 model + z-fight
守卫 ⑦ attack_damage 留数据默认 1.0（MC 2.5 = 范围外）

## 门协议发现（PITS 381）
① **热栏 key → RMB 须 ≥0.15s**（0.05s = 同帧选择未处理，RMB 用旧
selected——b 臂 60 密对染色全失 = 剪毛先中白毛）② **活体掉落物品
不可作终档断言**（磁力拾取入首空槽【染料已耗 = slot 1】→ 后续弹幕
key 2 选中 wool 块物品 RMB → placeBlock 误放置成块 = 终档块编辑噪
声；功能链实际正常，断言口径 = DEBUG 日志行 dyed/sheared）③ **羊
静止命中窗 = 出生 → 首次 wander（wander_interval 3.0s）**（1.2 宽
> 1×1 围盒出生重叠挤出弃围盒；0.25 b/s 游走单发必失；弹幕密对周期
≤0.8s 全覆盖）
