# Phase V2-12 — 附魔系统（v2.13.0，门 184 全 PASS）

> 二批 CRAFT 行（原 3.x 第二项；BACKLOG 立项定案 = **真附魔台范式**，
> 消费三处「附魔台替身」记档：master_bow P82 / 弓强化 P77-79 / 信标
> P91）。MC 现代数值 11-04 编排者 web 核定（minecraft.wiki：
> Enchanting_table_mechanics / Sharpness / Unbreaking / Efficiency /
> Protection / Enchanting_table）入设计稿 /tmp/opencode/v212_design.md，
> fixer 实现（ses_e5d93cbdbffewcIYLNQ0xj7g4e，一次交付 + 一次补
> 「魔」字形）。

## 实现

### 7 附魔（数据驱动 assets/data/enchantments/*.json）
| id | 名 | classes | weight | max | eCost 区间 | 效果 |
|---|---|---|---|---|---|---|
| sharpness | 锋利 | axe | 10 | 5 | MC 精确 1–11/12–22/23–33/34–44/45–65 | 近战 +（L+1）点 |
| protection | 保护 | armor | 10 | 4 | 5–12/13–20/21–28/29–37 | +4L 护甲点（共享 20 点 cap） |
| efficiency | 效率 | pick/shovel/axe | 10 | 5 | 1–10/11–20/21–30/31–40/41–91 | 正确工具 breakSpeed +（1+L²） |
| unbreaking | 耐久 | 全 durability | 5 | 3 | 5–12/13–20/21–71 | 每损点掷骰跳过（工具 L/(L+1) / 护甲 2L/5(L+1)） |
| power | 力量 | bow | 10 | 5 | 1–10/11–20/21–30/31–40/41–56 | shoot_damage +L |
| speed | 速度 | bow | 10 | 3 | 12–31/32–50/51–91 | shoot_speed +L（偏离：MC Quick Charge = 蓄力时间） |
| infinite | 无限 | bow | 1 | 1 | 20–50 | 发射不耗尽最后一支（∨ 字段） |

- 物品侧：`enchant_class`（7 词汇表）+ `enchantability` 严格互配
  （12 工具 15/5/14 + 9 护甲 15/9/10 + bow 1 = 22 件；仅基础 bow
  可附魔，变体弓保持替身定位）；lapis `enchant_material: true`
  （青金石槽数据驱动门，tool 携带 = 点名拒）
- 附魔 JSON 严格解析：未知 effect/class、倒置区间、零权重、未知
  incompatible 全点名；EnchantmentRegistry init 装载。

### 确定性 offer 生成（core/enchanting.{hpp,cpp} 纯函数）
- 种子 = worldSeed ^ hash3(tablePos, salt) ^ enchantability ^ xpSeed；
  单推进流（PITS 30 纪律，**全整式禁 double** = -O3 融合风险）
- 三槽：xpBase = 1+randInt(7)+⌊B/2⌋+randInt(B)；top =
  ⌊max(1,xpBase/3)⌋ / mid = ⌊2xpBase/3⌋+1 / bottom = max(xpBase,2B)
  （MC 精确；B=0 表 top 1-2/mid 1-6/bottom 1-8 pin）；lapisCost =
  1+randInt(2)（均匀 1-3，偏离）；eCost 含 randTri(0.15) 整式
- 选择循环（MC 精确）：候选 = 类匹配 ∧ eCost 入区间取最高级 →
  权重 pick → incompatible 移除 → 继续 = v%(eCost+150)<150 →
  eCost/=2 回环
- bookshelfCount = 5×5 环 ×2 层 cap 15（**偏离**：无 MC 视线规则）；
  仅面板打开时求值（无面板零成本 = 结构免疫根）
- **XP 扣减**（整数精确）：扣整级、条内余量保留 = T(L0−c) +
  (pool − T(L0))；xpSeed = player 新键，成功附魔后推进

### 新块 ×2 + 配方 ×2 + 纹理 ×2
- enchanting_table（hardness 5 / light 7 / interact "enchant" /
  drops 自身【偏离：MC 镐门】/ **full_cube**【偏离：设计 3/4 盒触
  mesher+碰撞红线，定案全盒】）
- bookshelf（hardness 1.5 / `enchant_bookshelf` 属性 / full_cube）
- 配方：台 = shapeless 4 obsidian + 2 diamond + 3 lapis（偏离：MC
  用 book，项目无纸/书链）；书架 = shaped PPP/LLL/PPP（偏离同款）
- tile：atlas 102→104（空位 (9,2)/(14,2)，既有零移位）

### format 27（四拒 + 迁移）
- `player.xp_seed` = **16 位小写 hex 串**（偏离：存档 JSON number =
  double 53 位，64-bit seed 须无损）恒写；26→27 迁移 = worldSeed ^
  kXpSeedInit
- 物品 `enchants` {id: level} 三处（slots/armor/entities，omit-when-
  empty，damage 先例）；ItemStack operator==/add 合并含 enchants
- 四拒：26 携 xp_seed / 26 携 enchants / 27 缺 xp_seed / 交叉校验
  （level>max、类不兼容、未知 id、incompatible 对）全点名
- 载时 validateEnchants（validateStats 同款）

### 效果消费（6 点位级不变红线 = 无附魔逐位，既有全绿实证）
- melee：selectedAttackDamage +（L+1）（sharpness accessor）
- protection：Player::damage armor 步 pts = min(20, armorPts +
  4·min(20,ΣL))（共享 cap 偏离：MC 保护独立 + 总 80%）
- efficiency：effectiveBreakSpeed +（1+L²），门 = block ∈ fast_against
- unbreaking：6 磨损调用点统一 unbreakingWear（state = worldSeed ^
  damageBefore×golden ^ salt ^ itemRegistryId，**无帧依赖** = 伤害史
  确定 → 掷骰确定）
- power/speed：fireBow 参数 +L；infinite：字段 ∨ 附魔

### 面板（EnchantUI，furnace 范式全克隆）
- OpenPanel::Enchant + RMB 开（interact 路由）+ EnchantLayout
  （物品槽左/青金石槽中上【enchant_material 门】/3 offer 行右
  「名 罗马 xpCost」）+ pick-and-place carry 模型 + E/关归还
- 标题「附魔台」+ 字体 +4 字：锋利限（fixer 首交）+ **魔**（门内
  补：首交漏标题字 = 渲染「附?台」）；atlas 256×2080→2112（66 行，
  cell ≤521 ≤ 527 余量内）；顶 512 行逐字节不变红线 pin；
  kCjkGlyphs 423→427（鸡麦黄默 re-cell +1，码点序）
- 音 = 复用 "craft"（记档：无专用附魔音）

## 变更文件（70 + 魔 4）
新：enchanting.{hpp,cpp} / enchantments 7 json / 块 2 json / 配方 2
json / 纹理 2 png / test_enchanting(28) / test_enchant_content(15) /
test_enchant_save(16)；改：item 模型+注册表+inventory / save+game_save
（27）/ player（protection/unbreaking/xpSeed）/ item_entity（enchants
携带）/ game_interaction（6 消费点 + interact 路由）/ game_ui（面板
全流）/ inventory_ui（EnchantLayout/UI + carry 三处含 enchants）/
block 族（interact 白名单 + enchantBookshelf）/ font 族（427 +
2112）/ block_atlas（104）/ 23 物品 json / CMakeLists（2.13.0 + 4 新
源）/ 8 测试文件（format bump 剥离 xp_seed+enchants / 字体 / atlas /
block / F3 钉）

## 门 184（old = v2.12.0 tm_v2130_base/perf；编排者执行）— 全 PASS

### S1 = 结构免疫第 65 次（**免重锚**）
- 新侧 60f ×2 恒等 且 **≡ 现锚 179027 B 逐字节**（无输入：面板 RMB
  门永不开 + 新块不入 worldgen + 新字/tile 不绘制；atlas 高度变化
  的 v-UV 亚 LSB 位移实证零像素差——fixer 中间态"需重锚"报告 = 环境
  伪影，终态复证作废）
- savenorm（官方 gate_compare.py）= 恰 {format 27 vs 26, player.xp_seed}

### S2 = 附魔面板输入 E2E（PITS 376/378 协议）
- 场景：台 (0,54,-3) + 8 书架环（B=8，**勿置 (0,54,-5)**——与玩家
  AABB 重叠 0.3 → 载时顶高 → 瞄准升空 RMB 失手 = PITS 378①）+ 热栏
  0 铁斧 + 热栏 1 青金石×10 + player.xp 1395（= T(30) 整级，B=8 下
  恒可付：top≤6/mid≤14/bottom≤20）
- 序列（pick-and-place carry 模型，PITS 378②）：RMB 开面板 → 拾斧
  （热栏 0 屏 (34,477)）→ 放物品槽 (454,410)【offer 生成 = 效率 I 6 /
  效率 III 9 / 效率 IIII 17，种子确定】→ 拾青金石（热栏 1 屏
  (80,477)）→ 放青金石槽 (500,410) → 点 offer 行 0 (604,374)【应用】
  → 拾附魔斧 → 放回热栏 0 → E 关
- **断言全 PASS**：slot0 铁斧 enchants = {效率:1} 非空 ∧ xp 1395→
  **828 = T(24) 恰扣 6 整级**（余量保留）∧ xp_seed 5eed…→ffda…
  推进 ∧ 青金石 10→9（lapisCost 1）；**旧侧**（无台同库存同 xp）：
  斧无 enchants 键 ∧ xp 1395 不动 ∧ format 26 无 xp_seed = 形状互斥
- S2b：面板开态截图 ×2 **逐字节恒等**（offer = 种子确定性，与打开
  时刻无关）

### S3 套件
- **2308/2308**（2249 + 59 附魔族 + 魔 字形并入；编排者独立复跑 ×2）

### S6 = 12/12 点名（真二进制 scratch 资产 + 存档手术）
- 数据 7：未知 effect / 倒置 ecost 区间 / 未知 class / ability 无
  class（iron_ingot）/ class 无 ability（iron_axe 剥 class）/
  enchant_material 挂 tool / 未知 interact
- 存档 5：26 携 xp_seed / 26 携 enchants / 27 缺 xp_seed / sharpness
  9 > max 5 / efficiency 挂 leather_boots（类不兼容）

### 性能 A/B PASS 同世界同成本（用户硬要求）
- v2.12.0 vs v2.13.0 fall 900f 无输入 seed 123456789：PERF-WIN 三窗
  全指标同档（upd med 0.60/0.63/0.61 vs 0.60/0.58/0.56 ms；swap
  15.2-15.4 vsync 同值）+ **changesApplied 9/1/3 逐位** + 终档
  fluids 16/16 + blocks 36/36 集合恒等 + xp_seed old=F/new=T 唯一
  预期 diff
- **预置台场景自成本**（台 + 8 书架 + 斧 + 青金石，无输入 900f）：
  全指标同档（面板不开 = bookshelfCount 不求值 = 零成本实证）

## 偏离（记档 10 条）
1. 台/书架恒掉自身（无镐门）2. 无视线书架（满环计数）3. 均匀
lapis 1-3 4. speed = 弹速范式（无蓄力模型）5. 配方 lapis 代 book ×2
6. 复用 "craft" 音 7. full_cube 台/书架（3/4 盒触红线）8. xp_seed =
hex 串（64-bit 无损）9. 保护共享 20 点 cap（MC 独立 80%）10. 剑类无
物品（sharpness = axe 主项，MC secondary 需铁砧）

## 门协议发现（PITS 378）
① 场景块置玩家 AABB 邻格（0.3 重叠）= 载时碰撞顶高 → 瞄准静默失手
② 面板 = pick-and-place carry（空 carry 点空槽 no-op；shift 仅拉出）
③ xdotool 窗口名子串匹配命中用户浏览器（须 --pid）④ savenorm 必须
官方 gate_compare.py（手写 norm 漏剥 fires[*].remaining）
