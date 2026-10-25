# Phase SURV-3 存档（耐久系统）

## 定案

工具/护甲/弓/点火器耐久（v1.0 任务表 SURV 行第 3 项，17/29）。MC 语义
查证（minecraft.wiki Durability 页）：工具材质值 wood 59 / stone 131 /
iron 250（镐斧锹同材质同值）、弓 384、燧石与钢 64；护甲 leather
55/80/75/65、iron 165/240/225/195、diamond 363/528/495/429（11:16:15:13
比例）；磨损 = 破非瞬断块 +1（hardness 0 瞬断 = +0）/ 镐斧锹打 mob +2 /
射出 +1（含 infinite）/ 点燃 +1 / **护甲每次"实际减伤"事件每件在穿
−max(1, incoming/4)**（incoming = 减伤前原始量，float 精确）；溺水
bypass = 不磨损（MC 同款，与 SURV-2 豁免规则天然对齐）；破碎 = 本次
动作完成 + 物品移除（现代 MC 行为，无掉落）。**修复系统（铁砧/磨刀石/
Mending）= 范围外记档偏离**（矩阵无对应项）。

## 实现

- **`durability` 物品字段**（严格 number > 0；允许集 = Tool ∪ armor_slot
  ∪ ammo 发射器 ∪ ignites_flamable，其它物品带字段 = 解析拒绝点名；
  27 件内容全 MC 精确值）
- **`ItemStack.damage` float**（缺省 0 = 满；operator== 加 damage；堆
  移动随堆 = 拷贝语义；**Inventory::add 合并门 = 同物品且同 damage**
  （不等开新槽 = 记档简化）；ItemEntity.canMergeInto 同款门）
- 纯函数 `item/durability.{hpp,cpp}`（campfire/drown 范式）：
  `applyDurability(stack, wear, items) -> bool broken`（破碎 = 清空）+
  `armorWearPerPiece(incoming) = max(1, incoming/4)` + 命名常量
  kToolBlockWear 1 / kToolMeleeWear 2 / kLauncherShotWear 1 /
  kIgniteWear 1 / kArmorWearPerIncoming 4 / kArmorWearMin 1
- **磨损事件钩子（全帧锁定事件驱动）**：breakBlock（hardness≠0 且 Tool
  且 durability>0 → +1）/ updateAttacking 命中（Tool +2）/ fireBow
  射出（+1 含 infinite）/ 点燃分支 ×2（+1）/ **`Player::damage` 内
  护甲磨损**（!bypass && pts>0 && 减伤后仍 >0 → 每件在穿 +
  armorWearPerPiece(incoming)；破碎 = 槽清 + points 收口同步）
- **format 17→18**：damage 键 >0 才写（slots/armor/entities item 三处；
  grid/炉子/营火槽不入档 = 记档 quirk）；armor 满耐久保持字符串形
  （与 format 17 逐字节同形 = 免疫最强）、damage>0 翻严格对象
  {item, damage}（≤17 带对象形 = 版本谎报点名）；交叉校验 damage <
  durability + 无 durability 物品带 damage 点名
- **HUD 耐久条**：drawIcon 共享点（hotbar/手持/护甲排/inventory 面板）
  damage>0 时图标底部 2px 横条（填充 1−damage/durability；三色档
  绿≥1/2 / 黄≥1/4 / 红<1/4 = MC 渐变简化记档）；满耐久零绘制（结构性
  免疫）
- F3 手持行 `held <name> d/max`；破碎音复用既有 "break" wav（护甲破碎
  无声 = 记档）

## 变更文件

（fixer 交付，待主线提交：48 改 + 3 新——item.hpp/registry/durability
新/inventory/entity/player/armor/save/game/debug_overlay/inventory_ui +
27 件物品 JSON + CMakeLists + test_durability.cpp 新 17 条 +
test_save/test_campfire/test_debug_overlay 适配 + KNOWN_ISSUES）

## 测试

1466/1466（+18：test_durability 17 + debug overlay 手持行 1）、
`-Werror` 0 警告、存量 1448 零回归（ItemStack 加字段对合并/存档
路径零回归背书）。
- applyDurability 5：扣减/恰界破碎/界下不碎/0-wear+无耐久 no-op/
  护甲槽清+points 同步
- 解析 4：非正拒/错误物品点名（apple）/四集合法/27 件真实资产值钉死
- damage 语义 2：operator==/堆移动随 damage（穿戴-脱下-Q 丢-拾取
  合并门）
- format 18 4：三处中段 RoundTrip + 形状钉死 / 17 带键谎报点名 /
  damage≥durability 拒 / 无耐久物品带 damage 拒
- 护甲公式 1：10 → 2.5 exact / 1 → 1 下限 / 双件同事件
- 溺水 bypass 1：恒 0 ×4 击

## E2E（门 97）

- **S3 坠落护甲磨损（no-input）**：预置 iron helmet（damage 0）+
  13 格坠落 10 伤 → **hp 10.8**（20 − floor(10×92)/100 = 9.2——
  护甲减伤生效实证，对照无甲 10.0）+ **helmet damage 2.5 exact**
  （max(1, 10/4)——护甲磨损进 E2E）；×2 savenorm3 EXACT
- **S3b 溺水豁免（no-input，SURV-2 深池）**：满铁甲 + 1500f 溺水 →
  **hp 0 死**（bypass = 无减伤，对照若误走全源减伤会 hp 11.2 活）+
  **4 件 armor damage 恒 0**（bypass 不磨损）；×2 savenorm3 EXACT
- **S6 init 失败 ×3**：`slot #0 holds damage 5.0 but the item 'apple'
  has no durability`（无耐久物品带 damage 点名）/ `damage 250.0 is
  at/above the item 'iron_pickaxe's durability 250.0 (a broken item is
  removed, never saved)`（破碎态非法点名）/ fmt17 带 damage 键 =
  `save slot #0 has unknown field 'damage'`（版本谎报点名）
- **S2 工具磨损/破碎（输入）**：预置 iron pickaxe damage 249 + 单石，
  破 1 块 → 碎（槽空 + cobble 掉落）vs damage 248 → 249 存活（互斥）
  ——**窗口相位未开（away 组件 13:00 起持续隐藏游戏窗）= 待补**
- **S4 弓磨损（输入）**：bow + arrow×3，L R R → bow damage 2 + arrow
  剩 1 ——**待补**（同上）
- **S5 耐久条像素隔离（捕获）**：预置 damage 100 pickaxe vs 满耐久
  ——**待补**（同上）
- **环境注**：门 97 场景首轮 6/7 缺 format 17 必需键 player.oxygen
  （场景升级 format 须补齐该版全部必需键——游戏点名拒 = 正确行为，
  场景生成器纪律补记）；输入窗口相位 13:00 起关闭（no-input 跑不受
  影响 = 存档级证据照常收）。

## 性能（插桩 A/B v0.14.0，发布前测试）

- **fall_armor 900f 三 300f 窗**（坠落 = 护甲磨损事件 + format 18
  存档路径；场景含大流送填充）：render med 0.348/0.505/0.684（old
  0.340/0.503/0.655 同档）、upd med 46.3-48.3 ms（old 46.1-48.8
  同档）、swap med 0.49-0.52（old 同款）、player 2.48-2.67 ms（old
  2.54-2.71）、ents 0.29-0.31（old 0.26-0.27）、stream 1.87/2.14/
  2.46 s（old 1.82/2.09/2.51 s）——**全指标同档**
- vd16 fresh 填充：黑相期未得帧（fall 窗已含 stream 填充段指标 =
  等效覆盖）
- **结论：SURV-3 成本可忽略，无回归**（磨损 = 稀有事件一次 float 加
  法；耐久条仅 damage>0 且指针捕获时绘制 = 无输入零活动；format 18
  满耐久 = 键缺席 = 存档体积零增量）

## 偏离

- 无修复系统（铁砧/磨刀石/同物合成/Mending = 矩阵外，记档）
- 堆合并 = 同物品**且同 damage**（MC 合并保留各自 NBT——项目无 NBT，
  同 damage 门 = 确定性的简化，记档）
- 耐久条三色档（MC 连续渐变）
- 瞬断判定 = hardness 0 近似（MC "naturally breaks instantly" 含工具
  高速特例，记录）
- **护甲磨损源 = "护甲实际减伤"事件**（设计门自洽推论）：本项目
  饥饿/火 tick/虚空/poison 不走 bypass → 也磨损护甲（MC 中这些源不
  磨损；影响限死亡/长跑边缘，KNOWN_ISSUES 记档）
- grid/炉子/营火槽不入档（受损物放网格读档回满耐久 quirk，仅网格
  可实际触达，记档）
- 护甲破碎无声（Player 无 sound 通道，记档）
