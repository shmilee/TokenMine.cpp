# Phase V3-4 — netherite 材料链（v3.6.0，门 196 全 PASS）

> 设计定案 = `/tmp/opencode/v360_design.md`（M1-M4 + 实测锚点收口：
> 铁工具 dur 250/pick bp 4.0/ad 3·4·3.5/ea 14/fast 2.0 + 石族 10 块/
> 形状 "III/S S/S S"；钻石护甲 dur 363/528/495/429/ap 3/8/6/3/ea 10）。
> 依赖 v3.5.0（ancient_debris 块 + nether_gold_ore → gold_ingot）。
> **零核心代码交付 = "加材料 = 数据"承诺的第四档验证**。

## 实现（全数据）

### 材料链（M1：MC 精确部分）
- **ancient_debris 块** `+smeltable_to: netherite_scrap`（CRAFT-6 块
  物品 smelt 继承机制，netherrack → nether_brick 先例）→ **scrap**
  material 物品（1:1，无燃料则不熔）。
- **netherite_ingot** = **shapeless 4× scrap + 4× gold_ingot → 1**
  （记档偏离 1：MC 用 4 gold nugget，项目无 nugget 物品）。

### 工具 3 件（M2）
netherite_pickaxe / netherite_axe / netherite_shovel：
- **项目无钻石工具**（既有偏离）→ netherite 工具**直接 shaped
  crafting**（克隆铁形状 "III/S S/S S" 族，key = ingot + stick；记档
  偏离 2 = MC 须钻石升级）。
- 数值：**durability 2031**（MC 精确）/ break_speed 4.0（pick，项目铁
  刻度——记档：MC 9 vs 项目刻度自成体系）/ attack_damage 4/5/4.5（铁
  +1 升档）/ enchant_class pick/axe/shovel + enchantability 14（铁同款
  项目刻度）/ **fast_against_multiplier 2.0 × 21 块**（铁石族 10 块 +
  nether 台地 11 块：netherrack/双 nylium/basalt/双 stem/soul 族/
  ancient_debris/nether_brick/magma_block = 正确工具语义）/
  repair_with netherite_ingot / model 克隆铁系换纹理。

### 护甲 4 件（M3）
netherite_helmet/chestplate/leggings/boots：
- **shapeless 升级配方** = netherite_ingot + diamond_<slot> → 1（项目
  无锻造台，记档偏离 3 = MC 锻造升级）。
- 数值：**durability 545/792/743/644**（MC 精确 = 钻石 ×1.5 验算：
  363×1.5=544.5→545 等）/ armor_points 3/8/6/3（钻石同款 MC 精确）/
  enchantability 10（钻石同款项目刻度）/ repair_with ingot / model
  克隆钻石系换纹理。

### 资产
- icon 9 件（generate_item_icons 确定性：scrap 暗红渣块 / ingot 暗红
  锭 / 工具克隆铁系 / 护甲 ARMOR 族 salt 172；**色板 = 暗红铜**
  （122,84,74 / 164,122,108）源自 ancient_debris 铜块 + netherrack
  暗红——下界世界观一致，非 MC 灰褐 netherite = 记档偏离 4）；既有
  84 张 md5 逐一不变（幂等实证）。
- model 9 件（JSON 内 model 字段；ENT-9 census 84→93）。
- repair census 28→35（+3 工具 +4 护甲；scrap/ingot 无字段）。

### 零核心代码核证
`git status --short src/` = 空（改动面 = assets/data 17 + textures/
items 9 + tools 生成脚本 + tests 4 + CMakeLists 版本）。smeltable_to /
shapeless / shaped / repair_with / fast_against / model 全既有机制，
**机制缺口 = 无**（fixer 如实报告：三链全通，块物品 smelt 继承端到端
冒烟实证）。

## 门 196（结构免疫第 77 次；锚 224468 B 免重锚）

**结构免疫论证**：纯内容（9 物品 + 8 配方 + icon/model）——新物品不
在默认热栏（hotbar.json 零改）+ 无世界生成消费 + 熔炼/合成需输入 =
无输入运行零参与。

- **S1** 无输入 60f ×2 ≡ 锚 224468 逐字节 + savenorm EXACT + 白名单 4
  键（format 30 不变）+ banner v3.6.0。
- **S2 熔炼臂**（fixer 冒烟 = 门级证据，无输入 600f）：场景炉
  (0,54,−3) 预载 debris ×1 + coal → "Furnace lit" → 恰 10 s "Furnace
  smelt complete: netherite_scrap" → 终档炉内 output = scrap ×1 /
  input 空 / fuel_substeps **4201**（4800−599，CRAFT-6 点火免算纪律
  精确验算）/ 库存 debris 原样 / **xp orb ×1**（smelt_xp 默认发放面
  实证 V2-11）。合成链（scrap+gold → ingot → 工具/升级）= 单测
  SmeltThenCraftEndToEnd + 3 族合成行为钉（取物步需面板输入，降级形
  记档）。
- **S3** 全套 2640/2640（编排者独立复跑）。
- **S6 点名 5/5**（scratch 资产手术，全 init 带路径 rc=1）：
  durability 0（"must be a number > 0"）/ enchant_class 未知词
  "sword2"（带 7 词表）/ enchant_class·enchantability strict pair 半对
  （"both are required when either is present"）/ 配方未知材料
  "netherite_ingot_v2"（"unknown ingredient item"）/ fast_against 未知
  块引用（"references the unknown block"）。**非拒收面实证**（机制
  理解留证）：enchant_class = "sword"（合法词表成员）不拒 / 升级配方
  缺 ingot（= 内容错误非 schema 错误，行为面单测覆盖）。
- **性能 A/B（用户硬要求）**：旧 = v3.5.0（tm_v360_perf = 27f374e +
  g139，banner v3.5.0 实证），新 = v3.6.0（tm_v360_perf_new，banner
  v3.6.0 实证）：主对 900f 无输入同 seed——PERF-WIN 三窗全指标同档
  （upd med 0.580/0.543/0.587 vs 0.609/0.571/0.639 / render
  0.606/0.572/0.564 vs 0.585/0.543/0.542 / swap vsync 同档 /
  fluid·fires·spread·ents·stream 同档）+ **changesApplied 9/1/3 逐位
  恒等** + **终档 savenorm EXACT**（纯内容零 format = 完全恒等最强
  形）。

## 单测族（2617 → 2640，+23，tests/test_netherite.cpp 新）

ScrapAndIngotContentPinned / AncientDebrisSmeltDataPinned / Tools-
ContentPinned / FastAgainstExtendsTheIronList / FastAgainstBlocks-
Resolve（21 名全真实 BlockRegistry 解析 + 铁前缀逐字恒等）/
FastAgainstBreakSpeedSemantics / ArmorContentPinned / ArmorDurability-
IsTheDiamondScaleTimes15 / RepairMaterialsCheckPinned / IconCensus-
Pinned / IngotRecipePinned / ToolRecipesPinned / UpgradeRecipesPinned /
RecipeCensusPinned（87→95）/ IngotCraftBehavior / UpgradeCraftBehavior
/ ToolCraftGridBehavior / SmeltTickDebrisToScrap / **SmeltThenCraft-
EndToEnd**（debris → scrap → ingot → 工具/护甲全链）/ 拒收 4 面
（durability 0 / enchant_class 错配 / 升级配方未知 ingot / fast_
against 坏条目）。

## 实测数据

- 2640/2640；icon 93 / model 93 / recipe 95 / repair 35。
- 熔炼 10 s（MC 锚）；fuel 4201 验算；xp orb ×1。
- 门档 60f 无输入 ≡ 锚逐字节（第 77 次）。

## 偏离/记档（4 条）

1. gold nugget → gold_ingot（项目无 nugget）。
2. netherite 工具直接 crafting（项目无钻石工具 = MC 升级链断裂点）。
3. 护甲升级 = shapeless ingot + diamond（无锻造台）。
4. icon 色板 = 暗红铜（下界世界观一致，非 MC 灰褐）。
另：不做击退抗性/火免疫/悬浮熔岩（M4，Part 2 候选——伤害管线改造
面）/ break_speed = 项目铁刻度（非 MC 9）/ 工具攻击力铁 +1（命名
注释）。

## 变更文件（显式路径，34）

assets/data/blocks/ancient_debris.json（+smeltable_to）/ assets/data/
items/netherite_* ×9（新）/ assets/data/recipes/netherite_* ×8（新）/
assets/textures/items/netherite_* ×9（新生成）/ tools/generate_item_
icons.py（+5 图标函数 + ARMOR 族 + ITEMS 9）/ tests/test_netherite.
cpp（新 23 例）/ tests/{test_item_model_mesh 93 / test_repair 35 /
test_debug_overlay F3 v3.6.0} / CMakeLists.txt（3.6.0 + 测试注册）。

零改动红线核证：src/ 零触碰（git status 空）/ 默认热栏零改 /
worldgen/渲染/存档/config 零触碰 / 既有 icon 84 张 md5 不变。
