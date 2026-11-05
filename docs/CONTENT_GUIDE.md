# 内容添加指南（数据驱动）与存档使用

> Phase 30 文档整理：从 `README.md` 移入（原「新增一个生物群系/矿石」「调整初始
> hotbar」「新增方块/配方/Mob」「使用存档」小节组），并并入原 `SESSION_CONTEXT.md`
> 「Phase 4 关键参数」（worldgen.json 参考）。10-30 全量刷新（v1.33.0 / 存档
> format 22 / 测试 1912）：各节字段表逐一对照真实资产（`assets/data/`）与解析
> 代码核实（`src/block/block_registry.cpp`、`src/item/item_registry.cpp`、
> `src/core/status_effect.cpp`、`src/entity/mob_registry.cpp`、
> `src/item/recipe.cpp`、`src/world/worldgen_config.cpp`、`src/world/biome.cpp`、
> `src/core/save.cpp`、`src/core/game.cpp` init 交叉校验段）；新增 worldgen.json
> 完整参考、config 参考、块音效材质、纹理/atlas 纪律节。
> 11-04 收官刷新（v2.19.0 / 存档 format 28 / 测试 2482）+ V3-7 刷新
> （v3.1.0 / 存档 format 29 = gzip 时代 / 测试 2503）+ V3-1 Phase A
> 刷新（v3.2.0 / nether_portal 块 / 测试 2536）+ V3-1 Phase B 刷新
> （v3.4.0 / netherrack 块 + 下界维度 / 存档 format 30 / 测试 2582）
> + V3-2 Part 1 刷新（v3.5.0 / 下界内容族 12 块 + 5 群系 + 3 矿脉 +
> 2 配方 / 测试 2617）+ V3-4 刷新（v3.6.0 / netherite 材料链 9 物品 +
> 8 配方，零核心代码 / 测试 2640）：二批新数据驱动机制
> 并入（biome tree 6 键 + exclude_spawn_area / slab 三态 state_count +
> drops_count_by_state / XP 三发放源 / 附魔 7 项 / 床 15 色染色族 / 染色羊变体
> / render.ao），各节数量（93 块 / 83 物品 / 86 配方 / 8 mob / 7 附魔 /
> 图集 122 用 6 空 / 46 wav / 427 字形）与 config、存档、配方表逐一对照真实
> 资产与解析代码刷新（新增 `src/core/enchanting.cpp` 交叉核）。文中
> `KNOWN_ISSUES` 均指 `docs/KNOWN_ISSUES.md`。每节标题的 phase 标签（PXX /
> ABC-N）是引入该数据面的阶段，v1.x 细节存档见 `docs/archive/v1x/phase-*.md`、
> v2.x 见 `docs/archive/v2x/phase-2.x.0.md`。

## 数据布局与校验策略总览

| 目录 / 文件 | 内容 | 数量（shipped） |
|---|---|---|
| `assets/data/biomes/*.json` | 生物群系（含 4 群系：V2-16 snowy_taiga 入列） | 4 |
| `assets/data/blocks/*.json` | 方块（含 3 个流体参考块 water/lava/honey；V2-7 planks/gold·diamond_block + V2-12 附魔台/书架 + V2-13 床 15 + V2-16 snow） | 93 |
| `assets/data/items/*.json` | 显式物品（工具/材料；block 物品无独立 JSON；V2-5 pork/cooked_pork/string + V2-15 shears） | 83 |
| `assets/data/recipes/*.json` | 配方（shaped 52 / shapeless 34） | 86 |
| `assets/data/mobs/*.json` | Mob（V2-5 pig + V2-14 bat 入列） | 8 |
| `assets/data/effects/*.json` | 状态效果 | 11 |
| `assets/data/enchantments/*.json` | 附魔【V2-12，数据驱动新面】 | 7 |
| `assets/data/worldgen.json` | 世界生成参数 | 1 |
| `assets/data/mob_spawning.json` | 生成节奏/环/上限/名单 | 1 |
| `assets/data/hotbar.json` | 初始热栏 + 隐藏 starter 槽 | 1 |
| `assets/data/textures/block_atlas.json` | 图集布局（16×8 网格） | 1 |

校验策略（三条纪律，与代码一致）：

1. **用户数据文件**：顶层未知键**警告但不拒**（前向兼容，block/item/mob/
   recipe/biome/附魔【V2-12】同款——附魔文件的**字段值纪律**严格：未知
   effect/class、倒置/重叠 eCost 区间、零权重、未知 incompatible 全点名
   拒）；但**严格子对象**（`chance_drop`/`tree`（biome 段与 sapling 段
   同款）/`water_reaction`/`model`（物品）/`swings`/`arrow_effect`/
   `on_hit_effect`/效果字段白名单/`tier_steps`/`tint`/`lava_lakes[i]`/
   `wool_weights[i]`【V2-15】等）未知键**点名拒绝**（ammo 纪律：显式空串/
   显式 0/缺配对键 = no-op 笔误，点名拒）。
2. **init 交叉校验**（游戏启动，`src/core/game.cpp`）：跨文件引用逐条点名
   （grows_to/ammo/drops/beacon_effects/声音材质 3 件/model 纹理文件/
   挤奶双物品/染色全家族 等）。
3. **存档**是机器生成的：**任何**未知字段一律报错（与用户数据文件的
   "警告但容忍"相反），见「使用存档」。

加内容 = JSON + 纹理，不改核心代码。**新增/修改任何数据面都须过 E2E 基线门
论证**（无输入下零活动 = 结构性免疫免重锚，或按协议重锚 `/tmp/opencode/
base18.png`）——P45 先例：lapis 矿脉 + 表面装饰进了数据，但出生点视野内
无变化 → 免重锚。

## 新增一个生物群系（无需改核心代码）

`assets/data/biomes/<name>.json`：

| 字段 | 类型 / 范围 | 说明 |
|---|---|---|
| `id` | 非空字符串，唯一 | 群系名 |
| `weight` | 整数 ≥ 1 | 选择相对权重 |
| `surface` / `subsurface` | 非空块名（缺省 grass/dirt） | 表层 / 次表层（世界生成 setup 对 BlockRegistry 解析，**未知 = 启动报错**） |
| `subsurface_depth` | 整数 [1, 16] | 次表层厚度 |
| `tree_density` | [0, 1] | 野生树密度（`trees.lattice` 格网 roll） |
| `height_scale` | (0, 5] | 高度乘数 |
| `tree` | 严格 6 键对象，可选【V2-16】 | 本群系自己的野生树（下）；**缺省 = 全局 `worldgen.json` `trees` 段 + 硬编码 oak 冠层（既有 3 群系逐位不变）** |
| `exclude_spawn_area` | 严格 bool，缺省 false【V2-16】 | 演示区（`caves.spawn_clear_columns` 半径内）群系选择**跳过**该群系（演示区结构保护：区内总权重保持既有值，区内映射逐位不变） |
| `decorations` | 数组，可选 | 表面装饰（下） |

**`tree` 对象**（V2-16，与树苗块 `tree` 段同款严格面）：`{"trunk",
"leaves", "min_height", "height_range", "leaf_radius", "shape"}` 六键
缺一/多一/类型错 = 点名拒；`min_height` ∈ [1,16]、`height_range` ∈
[0,8]、`leaf_radius` ∈ [1,2]（整数）、`shape` ∈ {oak, spruce, birch}
（treeShapeValid 词汇）；trunk/leaves 块名在世界生成 setup 对
BlockRegistry 交叉检（未知 = 启动报错点名）+ ENT-2 无状态块纪律（有
state 的块拒）。有 `tree` 的群系：同一 `trees.lattice` 格网 + 同一
确定性 roll（密度门 h>>40 / 高度位 h>>24），高度 = `min_height +
(h>>24) % (height_range+1)`，冠层 = `planTree(height, spec)` 纯函数
逐格 placeIfAir（root 格 = 表面格非 air 自然 no-op → 树干自表面 +1 起，
与既有 oak 路径同款）。

```json
{
  "id": "forest",
  "weight": 4,
  "surface": "grass",
  "subsurface": "dirt",
  "subsurface_depth": 3,
  "tree_density": 0.12,
  "height_scale": 1.1,
  "decorations": [
    { "block": "allium", "probability": 0.012 },
    { "block": "tulip", "probability": 0.012 },
    { "block": "cornflower", "probability": 0.012 }
  ]
}
```

**表面装饰**（P45）：`decorations` 每条目是**严格二元组 {block, probability}**
（未知键点名拒、block 非空串、probability ∈ [0,1]，逐条点名
`decorations[i]`）。放置规则：与野生树同一格网（`trees.lattice`）、每
(格, 条目) 独立 roll、落表面格正上方一格、只在该 biome 的 surface 块上、
高于海平面、树占格让位、**表面格被洞穴啃掉 = 跳过**（GEN-5 同款守卫，
P45 已有）。装饰块建议用可穿过的十字植物（花）或低硬度实心块（仙人掌）
——实心高硬度块会挡 mob 生成/路径。

shipped 4 群系：desert（w2, sand/sand depth 4, tree 0, hs 0.9, cactus
0.03）/ forest（w4, grass/dirt 3, tree 0.12, hs 1.1, 三花 0.012）/
plains（w4, grass/dirt 3, tree 0.03, hs 1.0, 三花 0.02）/ snowy_taiga
（w2【V2-16】, surface = **snow 满块**（hardness 0.1 MC 精确 / 新
"snow" 三件音族 / drops 自身 / opaque；MC 1-8 层雪层机制 = 满块替代
记档偏离）/ dirt 3, tree_density 0.12, hs 1.1, 无装饰,
**tree = spruce 6-8 高锥冠** {trunk spruce_trunk / leaves
spruce_leaves / min_height 6 / height_range 3 / leaf_radius 2 / shape
spruce}, **exclude_spawn_area true**）。群系选择噪声 =
`biomes.noise_scale`（见下 worldgen 参考）。

**演示区保护**（V2-16）：群系选择是逐列纯函数（噪声值 → 累计权重走
查，总权重 12）。`exclude_spawn_area: true` 的群系在 spawn clear 区
（复用 `caves.spawn_clear_columns` 既有判定 = |chunk| ≤ 2 双轴，
shipped 2 → chunk [−2,2]² = world [−32,48)²，即 4×4 出生候选区
[−32,32)² 的同款结构排除超集）的走查中被跳过，区内总权重
= 12 − 2 = **10 = 既有 desert(2)/forest(4)/plains(4) 映射逐位不变**
（单测钉：区内全部列 resolvedBiomeAt = 改前公式同值 + 区内 chunk
3-群系 vs 4-群系生成逐字节恒等）。区外 = 全 4 群系（雪带/针叶林入
世 = 预期的世界演进，旧存档加载得新区外地形，GEN-1/3/5 先例）。

## 新增一个矿石 / 世界生成参数（worldgen.json 完整参考）

`assets/data/worldgen.json`——缺省 = 结构化默认（`src/world/worldgen_config.hpp`
`WorldgenParams`），类型错误 = 启动报错，各段未知键警告。逐段参考：

### terrain（地形）

| 字段 | 范围 | shipped | 说明 |
|---|---|---|---|
| `base_height` | [1, 120] | 40 | 高度带中心 |
| `height_amplitude` | [0, 60] | 24 | ± 振幅 |
| `sea_level` | [1, 120] | 34 | 水填满到此 Y |
| `height_noise_scale` | > 0 | 0.02 | 高度噪声频率 |
| `height_octaves` | [1, 8] | 4 | fbm 八度 |
| `bottom_block` | 非空块名 | "bedrock" | 底层（GEN-7 后 = 基岩层块） |
| `deep_block` | 非空块名 | "stone" | 深层填充 |
| `bedrock_layers` | 整数 [1, 8] | **3** | `[GEN-7]` 基岩多层：y < N 为底——偶层（y 0,2,4…）恒 bottom_block，奇层每格确定性 50% roll（`bedrockLayerRoll` 纯谓词，MC 经典底规则）；1 = 旧单层 y0 位级复现 |
| `passes` | 对象，可选 | 见下 | `[GEN-8]` 山峰互感 |

`terrain.passes`（GEN-8）：`{enabled, noise_scale, threshold, amplitude}`——
coarse ridge mask（独立低频 fbm2 3 octaves，专用 salt）> threshold 时基
高度只升不降（`heightAt = min(base+bonus, 世界高−16)`，纯函数零 chunk
数据 = 跨 chunk 一致构造性成立）。逐条校验：`noise_scale` ∈ (0, 0.05] /
`threshold` ∈ (0, 1) / `amplitude` ∈ [0, 64]；**缺省段 = disabled = legacy
高度场位级**。shipped `{true, 0.008, 0.55, 6}`（探针单轮定案：mask 实测
[0.07, 0.88]，0.55 ≈ 全世 ~10% 列 bonus≥2）。

### biomes

`noise_scale` > 0（shipped 0.02）——群系选择噪声频率。

### caves（洞穴，`[GEN-1]` 面条洞 / `[GEN-4]` 破碎度重标定）

| 字段 | 范围 | shipped | 说明 |
|---|---|---|---|
| `enabled` | bool | true | |
| `noise_scale` | > 0 | **0.05** | GEN-4 重标定（0.09→0.05：管粗洞大、pocket −91%） |
| `tube_threshold` | [0, 0.5] | **0.06** | 两独立 3D value-noise 中位带相交 = 管状隧道（MC spaghetti 同款）；\|n−0.5\| < 该值于**两**噪声场 |
| `min_y` | [0, 120] | 2 | 低于此 Y 不挖 |
| `surface_guard` | [0, 15] | **0** | 0 = 允许洞口打穿表面（shipped，MC 洞口）；≥1 = 表面 + guard 层壳保护（旧语义） |
| `spawn_clear_columns` | [0, 32] | **2** | 原点 Chebyshev ≤ N 列整列不挖 = 出生/演示区结构保护 |

（旧 `threshold` 键 = GEN-1 前单噪声规则，已退役：仍解析 + 范围校验但生成器
不读，段内警告 = Phase 74 retired-config 先例。）

**每湖 lava 带排除盒**：`lava_lakes` 各湖的区域 x/z ±1 × [floorY−1,
floorY+fill] 构造期预计算——洞穴不挖湖带（保 P88/P90 熔岩湖静态被围契约）。

### ores（矿脉，`[GEN-3]` 椭球化）

`lattice` ∈ [1, 32]（shipped 8）+ `veins` 数组：

```json
{ "block": "diamond_ore", "probability": 0.0101, "vein_size": 1, "max_y": 20 }
```

- `block`：非空块名（init 对 BlockRegistry 解析，未知 = 启动报错）。
- `probability` ∈ [0, 1]：格网命中概率（每个 8³ 世界格网胞独立 roll，
  `hash3 > probability` 才落脉）。
- **`vein_size` = 椭球簇数 K ∈ [1, 8]**（`kMaxOreVeinClusters`）：一条脉 =
  K 个 **MC 3×2×1 旋转椭球**（各 21 格，半轴 (3,2,1) 随机取向；首球心 =
  格网胞内均匀起点，后续球心在前球 ±2 内游走）。旧的"随机游走步数"语义
  已废。K=1 单球已是旧 ~2-3 球游走的体量，密度主要由 `probability` 调。
- `max_y` ∈ [1, 127]：脉只出现在 y ≤ max_y。

**标定方法**（改 probability/vein_size 后用）：固定 seed（123456789）+
固定采样区（列 4..11 × 0..7，64 列全高，出生 clear 区外）跑 per-ore 格数
探针（`WorldGenerator::create` 直连，`tests/test_world_generator.cpp` 的
`Gen3ShippedOreVeinCellCountsPinned` 钉死值 = 当前 shipped 标定终值），
目标新/旧格数比 ∈ [0.8, 1.25]，`probability_new = probability_old / 比值`
（3 位整洁值），单轮定稿，残留越带记档（当前残留：diamond 0.641×、
lapis 0.732×，见 KNOWN_ISSUES）。

shipped 6 条（GEN-3 单轮标定终值，全 K=1）：coal 0.154/max_y 80、iron
0.0855/56、gold 0.0376/36、diamond 0.0101/20、lapis 0.0208/32、
**gravel** 0.0445/80（唯一非矿石"矿脉"，石替换脉）。矿脉只替换 deep
stone（不破坏洞穴 = 洞壁矿暴露），跨 chunk 无缝（世界坐标格网 + 世界
坐标起点）。

### trees（野生树）

`lattice` ∈ [1, 16]（4）、`trunk`/`leaves`（"wood"/"leaves"，init 解析）、
`min_height` ∈ [1, 16]（4）、`height_range` ∈ [0, 16]（3）。野生树 = 默认
单橡种（本段全局值）；**带 `tree` 字段的 biome 走 planTree 针叶路径**
（V2-16：同一 `trees.lattice` 格网 + 同 h 密度门/高度位，冠层 =
`planTree(height, spec)` 逐格 placeIfAir，shipped snowy_taiga = spruce；
**无 `tree` 的 biome = 原硬编码 oak 路径零触碰**，既有群系位级红线，
见 biome 节）；sapling stateful ⇒ worldgen 放置源 init 不变量天然拒
stateful 块（见方块节 ENT-2 约束）；`[GEN-5]` 表面格被洞吃 = 整棵树
跳过（守卫在 placeTrees）；`[GEN-6]` cz≥cx+1 对角半区缺失已修（修复后
两半区都种，内容无需动作）。

### lava_lakes（固定熔岩湖，P86 单湖 → P88 列表）

数组，每湖 `{enabled, center_x, center_z, radius, depth, fill}`：固定
位置 (center_x, center_z)、盆地 (2R+1)²、盆底 = 区域最低地形 − `depth`、
盆底上满熔岩 `fill` 层。逐湖校验：`radius` ∈ [0, 16] / `depth` ∈ [1, 16] /
`fill` ∈ [0, 8]（点名 `lava_lakes[i].<key>`）。`enabled: false` 关单个
湖。每个湖被盆地围住（四周地形更高）= 静态湖；放出生点默认 -Z 视野外 →
无输入基线门不受影响。

shipped 3 湖：(40,40) R4 D4 F2 / (55,45) R3 D3 F2（均埋没被围）+
**(24,26) R1 D2 F2 = 表面湖（P90）**：`depth == fill` → 熔岩顶与草 rim
同高 → P58 燃烧 pass 把 rim 点燃 = 焦土环 + 地表熔岩沿下坡溢出盆地
（确定性但非静止，见 KNOWN_ISSUES「熔岩湖」）。

### lava_lakes_procedural（程序化湖，P93）

`{enabled, count, min_radius, max_radius, min_depth, max_depth, min_fill,
max_fill, region_min_x, region_min_z, region_max_x, region_max_z}`——
enabled 时 `count` 个湖以 seed 派生的确定性伪随机位置落在 region 内，每湖
radius/depth/fill 各从区间随机取值。交叉校验（enabled 时）：`count` ∈
[0, 64]、0 ≤ min_radius ≤ max_radius ≤ 16、1 ≤ min_depth ≤ max_depth ≤ 16、
0 ≤ min_fill ≤ max_fill ≤ 8、region min ≤ max、每轴宽 ≤ 4096（逐条点名）。

shipped：`{true, 5, 1..3, 2..4, 1..2, region [60,90]×[60,90]}`（出生点
-Z 视野外 + 流送视图距离内；与固定湖并存）。

### 世界生成改动与基线门

改 worldgen.json / biome JSON 会改变生成的世界 → 无输入基线门需要重锚
或论证结构性免疫（P45 先例：出生点视野内无变化 → 免重锚）。验证基线：
默认 seed 123456789 的 (0,0) 列表面高 = 53 由单测契约钉死
（`test_world_generator.cpp`，GEN-8 重锚值）；同 seed 截图逐字节一致。
**零 format 变更纪律**：世界生成演进（旧存档按新地形读）= GEN-4/5/6/7/8/9
同款先例，不升存档版本。

## 调整初始 hotbar / 新增工具与材料

`assets/data/hotbar.json` 的 `items` 数组（游戏内 `Tab` 循环切换 9 格，
`1-9` 数字键直选，`Q` 从选中格丢弃）：

```json
{ "items": ["stone", "dirt", "grass", "sand", "wood", "leaves", "glass", "glowstone", "wooden_pickaxe"] }
```

`items` 必填非空、至多 9 项（越界/未知物品点名）；每格初始为满堆栈
（= 该物品 `max_stack`，放置会消耗——块不再是无限的）。

**`inventory` 段（GEN-2 起，可选）**——初始**存储槽**填充（隐藏 starter：
槽 [9, 26]，**非 hotbar = 无输入不渲染**）：

```json
{
  "items": ["stone", "dirt", "grass", "sand", "wood", "leaves", "glass", "glowstone", "wooden_pickaxe"],
  "inventory": [
    { "slot": 16, "item": "honey_bucket", "count": 1 },
    { "slot": 17, "item": "glass_bottle", "count": 4 },
    { "slot": 18, "item": "potion_healing", "count": 1 },
    { "slot": 24, "item": "oak_sapling", "count": 3 },
    { "slot": 25, "item": "spruce_sapling", "count": 3 },
    { "slot": 26, "item": "birch_sapling", "count": 3 }
  ]
}
```

条目**严格三元组 {slot, item, count}**（缺键/未知键点名拒）：slot ∈
[9, 26] 存储区（hotbar 槽 = `items` 段的领地，越界点名）、item 已注册点名、
count ∈ [1, max_stack] 点名、重复 slot 拒。缺省段 = 无 starter；
**读档整体替换 inventory**（starter 只在新世界生效）。shipped = 11 槽：
16 honey_bucket（FLUID-6）/ 17 glass_bottle ×4 / 18-23 六种药水
（SURV-4）/ 24-26 三种 sapling（GEN-2）——**隐藏 starter 惯例 = storage
区槽位，不渲染**。

### 物品 JSON 字段全集

新增工具或材料 = `assets/data/items/<name>.json`。显式物品只能是
`"type": "tool"` 或 `"type": "material"`——**"block items are defined by
their block"（CRAFT-6）**：每个数据方块隐式生成同名 block item（方块定义
是唯一来源，UI 以带光照 3D 立方/shape 模型呈现），**显式同名物品文件 =
注册表拒绝**（duplicate item id 点名）。block item 继承方块的
`fuel_time`/`smeltable_to`（= 块属性数据驱动路线，见方块节）。

```json
{
  "id": "wooden_pickaxe",
  "type": "tool",
  "tool": { "break_speed": 2.0 },
  "fast_against": ["stone", "cobblestone", "coal_ore", "iron_ore", "gold_ore", "diamond_ore", "lapis_ore", "glowstone", "sandstone", "smooth_sandstone"],
  "fast_against_multiplier": 2.0,
  "attack_damage": 2,
  "max_stack": 1,
  "icon": "wooden_pickaxe",
  "durability": 59,
  "model": { "texture": "wooden_pickaxe", "elements": [ { "from": [0.05, 0.62, 0.34], "to": [0.95, 0.8, 0.66], "uv": [0.05, 0.2, 0.95, 0.38] } ] }
}
```

字段表（严格解析 = 注册表纪律；顶层未知键**警告**，`model` 内部未知键
**点名拒**）：

| 字段 | 类型 / 范围 | 缺省 | 说明 |
|---|---|---|---|
| `type` | "tool" / "material" | 必填 | material 带 `tool` 字段 = 拒（防第二数据源） |
| `tool.break_speed` | 数字 > 0 | 1.0 | 仅工具；破坏耗时 = 1.5 s × hardness ÷ 有效速度（SURV-8：工具且目标 ∉ fast_against = ×0.1 惩罚 = MC 1/10；on-list = ×multiplier 位级不变） |
| `fast_against` + `fast_against_multiplier` | 非空块名数组 + 数字 > 0，**严格配对**（缺一拒、空数组拒）；仅 tool | 无 | P67 分块效率；init 逐条目块存在（grows_on 纪律） |
| `attack_damage` | 数字 (0, 100] | 1.0 | P20 挥击伤害 |
| `shoot_speed` / `shoot_damage` | (0, 100] / [0, 100] | 0 / 0 | P21/P79：shoot_speed > 0 = 投射物发射器（弓）；拉弓释放按蓄力缩放速度/伤害（core/bow.hpp） |
| `ammo` | 非空物品名 | 无 | P24：发射消耗 1（init 存在性）；显式空串拒 |
| `infinite` | 严格 bool | false | P77：要求 `ammo` 非空（无 ammo 发射器本就无限 = no-op 拒）；发射**不消耗**但须持有 ≥1 ammo（MC Infinity）；∨ 附魔 `infinite`（V2-12：附魔在则同款免末支） |
| `shears` | 严格 bool | false | V2-15：剪毛旗（**仅 tool 可携**，非 tool 点名拒 = armor_slot 双生纪律）；手持右键 `wool_drops` mob = 剪毛（每次成功磨损 1，bare no-op，见 Mob 节染色羊） |
| `arrow_effect` | `{name, duration, tier?}` 严格（未知键点名；duration > 0；tier 整数 ≥0 可选） | 无 | SURV-5：**发射器**箭矢命中施加的效果（poison_bow `{poison, 60}`）；要求 shoot_speed > 0；init：效果存在 **且 `affects_mobs`**（弓箭只命中 mob）+ tier < tier_max |
| `restore_hunger` | 整数 [1, 20] | 0 = 非食物 | P23：存在 = 食物（右键按住 eat 通道，1.6 s real-dt）；显式 0 拒 |
| `saturation` | 数字 [0, 20]，**须配 restore_hunger**（非食物带 = 死字段点名拒） | 旧 2× 规则（`foodSatiety` 纯函数位级复现） | SURV-9 食物饱和度（shipped 10 食物 MC 精确值：rotten 0.8 / 生肉 1.2-1.8 / 熟肉 7.2-12.8 / bread 6.0 / apple 2.4 / golden 9.0） |
| `smeltable_to` | 非空物品名 | 无 | P40/CRAFT-6：炉子产物（init 存在性）；存在 = 可熔炼 |
| `smelt_time` | 数字 (0, 100] | 10 | 单格熔炼秒数 |
| `smelt_xp` | 整数 [0, 9] | **1** | V2-11 XP：熔炼完成该物品发该值经验球（默认 1 统一 = MC 分数表记档偏离；**显式 0 = 无球**是合法数据声明——与 fuel_time 显式 0 拒相反）；>9 点名拒（球值上限，无分裂机制） |
| `fuel_time` | 数字 (0, 1000]，显式 0 拒 | 0 = 非燃料 | 单格燃料燃烧秒数（shipped：coal 80 / planks 7.5 / stick 5 / lava_bucket 1000） |
| `drink_time` | 数字 (0, 10] | 0 = 不可饮 | P43/SURV-4：饮用通道秒数；**与食物互斥**（同写 = 拒，通道所有权） |
| `returns_item` | 非空物品名，**须配 drink_time** | 无 | 喝完返还的容器（init 存在性） |
| `grants_effect` | `{name, duration, tier?}` 严格 | 无 | P49/SURV-4/SURV-6：完整吃下/喝下施加；**食物或可饮**才可带（否则死字段拒）；duration ≥ 0，**0 = 瞬时效果**（须配效果 `instant_hp`，init 双向交叉）；与 `grants_effects` 互斥 |
| `grants_effects` | 同上二元组数组（可带 tier），非空 | 无 | P56：按数据顺序依次施加（金苹果 = 再生 10 s + 吸收 5 s；6 药水各 1 条，瞬时 = duration 0）；init 逐条目效果名 + 0⇔instant 纪律 |
| `collects_fluid` | 非空流体名数组（词汇表 water/lava/honey），不重复 | 无 | P47/FLUID-6：**空桶侧**（见流体桶节）；与 pours 互斥、与食物/可饮互斥 |
| `pours_fluid` + `pours_becomes` | 流体名 + 非空物品名，严格配对 | 无 | **满桶侧**；init 双向对偶交叉校验 |
| `ignites_flamable` | 严格 bool | false | P60：点燃器——右键烧 `flamable` 块（**无掉落、不消耗**；成功点燃磨损 +1 = SURV-3）；与五个右键通道互斥（food/drink/launcher/dye/bucket，逐对点名）；**营火分支先于点火器**（P76：燧石与钢对未点燃营火 = 再点燃而非烧掉） |
| `dye_color` | 非空颜色名 | 无 | P41/P45：颜色词汇表 = 纯数据（见染料节） |
| `armor_slot` + `armor_points` | "head"/"chest"/"legs"/"feet" + 整数 [0, 20]，**严格配对**（缺一拒；tool 带 = 拒） | 无 | SURV-1：护甲件（右键穿戴/交换/脱下通道，点火器之后、潜行旋转之前）；减伤 = `floor(amount × (100 − 4 × 总点)) / 100`（次序 armor → resistance → 吸收池，pts 0 位级恒等） |
| `enchant_class` + `enchantability` | 词汇名（sword/axe/pick/shovel/armor/bow/hoe）+ 整数 ≥ 0，**严格配对**（缺一拒、未知类点名） | 无 | V2-12：可附魔（类须与附魔 `classes` 匹配，见附魔节）；shipped 22 件 = 工具 9（wood 15 / stone 5 / iron 14）+ 护甲 12（leather 15 / iron 9 / diamond 10）+ bow 1（仅基础 bow，变体弓保持替身定位） |
| `enchant_material` | 严格 bool | false | V2-12：附魔台青金石槽数据驱动门（**仅 material 可携**，tool 点名拒）。shipped = lapis true |
| `durability` | 数字 > 0，显式 0 拒 | 0 = 无耐久 | SURV-3：**允许集 = 工具 / 护甲 / ammo 发射器 / 点火器**（其余物品带 = no-op 点名拒）。shipped MC 精确值：wood 59 / stone 131 / iron 250（镐斧锹）、弓族 384、燧石 64、护甲 leather 55/80/75/65 / iron 165/240/225/195 / diamond 363/528/495/429 / **netherite 2031（镐斧锹）/ 545/792/743/644（护甲，[V3-4] 第四材料档）**。五类帧锁定磨损：破非瞬断块 +1 / 镐斧锹打 mob +2 / 射出 +1（含 infinite）/ 点燃 +1 / 护甲每次实际减伤事件 −max(1, incoming/4)（溺水 bypass 不磨损）。**无修复系统**（KNOWN_ISSUES） |
| `model` | `{texture, elements[1..16]}` 严格（内部未知键点名拒） | 无 | CRAFT-2：物品 3D 模型——单纹理（`assets/textures/items/<texture>.png`，init 文件存在性，**惯例 = 复用既有 icon PNG**）+ 轴对齐盒 1..16：每盒 `{from[3], to[3], uv[4]?}`，from/to ∈ [0,1]³ 且逐轴 from < to，uv ∈ [0,1]⁴ 且 u0<u1, v0<v1（退化矩形拒）。单位空间居中 −0.5..0.5；无 model = 三上下文（手持/掉落/进食举到嘴）位级不变。ENT-9 后 shipped 全部 83 件非 block 物品**都有** model（= 掉落可见，纯内容） |
| `max_stack` | 整数 [1, 64] | 64 | |
| `icon` | 非空字符串 | 不画图标 | `assets/textures/items/<icon>.png`（32×32 RGBA；文件缺失 = init 报错；可多物品共享同一文件） |

**右键通道顺序**（锁定时命中方块，自上而下，先消费者独占点击）：
① 营火（interact = campfire，烤槽放/取 = MC 先于进食）→ ② eat →
③ drink → ④ milk（挤奶）→ ⑤ interact（crafting_table/furnace/
**enchant** 面板）→ **⑤b 床睡眠**（V2-13：插入面板分支后、染色前——
命中 dye_base bed 族且非持染料；持染料落回染色路径，见床节）→
⑥ 染色 → ⑦ 流体桶 → ⑧ 点火器 → ⑨ 护甲（穿戴/交换/脱下）→
⑩ 潜行旋转 stateful 块（ENT-2；Slab 形排除 = V2-17）→
⑪ placeBlock（含 plant_on 门；slab 三态放置瞄准 + 同格合并，见方块节）。
**命中 mob 路径**（targetMob，全数据旗判据无物种 switch）：tryMilk →
**tryShear**（V2-15：shears 旗 × wool_drops 旗 × !bare）→ **tryDyeMob**
（V2-15：dyeColor × wool_drops × !bare）。
**一个物品只拥有一个通道** = 解析期逐对互斥纪律的根源。

物品图标 = 每物品独立 PNG（无 atlas）；block 物品 UI 以 3D 立方/shape
呈现不用 2D 图标。非 block 物品不可放置；掉落物 world 中以模型/立方
实体可见（ENT-9/CRAFT-2，PRE-6 掉落光照 = 洞穴掉落随世界光）。

## 新增一个方块（无需改核心代码）

1. `assets/textures/blocks/<name>.png` —— 16×16 RGBA（`tools/generate_block_textures.py`
   同款确定性生成：per-pixel hash (x, y, salt)；**重跑须保持既有文件字节
   不变**，见「纹理/atlas 纪律」）+ `assets/data/textures/block_atlas.json`
   空位 `[col, row]`（**有空位 = 免重锚**；网格扩容 = 重锚）。
2. `assets/data/blocks/<id>.json` —— 方块定义（`properties` + `model`）。

> **系统块先例（`nether_portal`，`[V3-1]` Phase A）**：无物品形态（不
> 入 `buildBlockItems` 物品词表、不可放置/不可拾取）+ `destroyable:
> false` + 仅由游戏系统写入（传送门激活/去激活的咽喉点转换 + 载入
> rescan 孤立置 air）；`state_count 2` = 框 axis（ENT-2 机制）。玩家
> 接触面 = 打火石点火框内格激活 + 站入倒计时（见 MECHANICS 传送门
> 节）+ worldgen 结构性不放（init 交叉检 = 结构免疫根）。
>
> **下界内容块族（`[V3-2]` Part 1，v3.5.0，12 件纯数据）**：
> nether_brick（熔炼 netherrack 1:1 + 2×2 配方成块）/ magma_block /
> quartz_ore（drops quartz）/ nether_gold_ore（drops gold_ingot）/
> ancient_debris（hardness 10 = MC /5 缩放先例）/ soul_sand /
> soul_soil / crimson_nylium / warped_nylium（grass 音先例 =
> sound_material 缺省）/ basalt / crimson_stem / warped_stem（drops
> 自身 = 记档偏离）——全 stateless（worldgen 永不写 state 纪律）。
> **下界群系文件 = `assets/data/biomes_nether/`**（独立目录独立
> registry，主世界 `data/biomes/` 零触碰）：{id, weight, surface,
> subsurface_depth?, subsurface, tree_density?}——tree_density 复用 =
> 柱状装饰 per-lattice 概率（数据驱动门控）；weight 0 / 重复 id /
> 未知 surface / stateful surface / 空池 = init 点名拒收（fail-fast，
> 主世界同款时序）。下界生成带 = nether_gen 命名常量（熔岩海 5-31 /
> 台地 32-52 / 空腔 53-122 / 天花板 122 / 基岩顶 123-127），改带 =
> 代码常量（非数据，记档）。

### properties 字段全集

| 字段 | 类型 / 范围 | 缺省 | 说明 |
|---|---|---|---|
| `solid` / `opaque` / `transparent` | bool | true / true / false | opaque+transparent 同写 = 拒 |
| `cutout` | 严格 bool | false | PRE-5：alpha cutout 渲染段（discard + 写深度）——叶冠孔/fire 缝/glass 框透见修复。**cutout=true 须 transparent=true**（缺 transparent = 矛盾拒）；**shape 门控：仅 full-cube / cross 可带**（其余 shape = 死字段 init 点名拒；流体同拒）。shipped 20 块 = 3 叶 + 3 花 + 7 麦 + 3 苗 + fire + torch + 2 营火 |
| `cull_same_id` | 严格 bool | false | V2-10：叶子-叶子内部面剔除——mesher greedy pass 在同 id 相邻格之间剔面（纹理选择前），叶冠内部双绘止；无 init 交叉检（mesher 读属性 + 邻居 block id，两者皆注册表数据）。shipped = leaves 族 3 块（leaves/spruce_leaves/birch_leaves），glass/水/cross 族红线零触碰 |
| `destroyable` | bool | true | false = 不可破坏（bedrock/流体） |
| `flamable` | 严格 bool | false | P58/P63：熔岩 6-邻燃烧 pass + 点火器目标（烧成 fire 块、**无掉落**）；**registry 有任何 flamable 块 ⇒ 必须有 `fire` 块**（init 点名） |
| `beacon_base` | bool | false | P83：信标金字塔基座（连续完整层 3×3/5×5/7×7/9×9 = 等级 1..4，每帧重算） |
| `enchant_bookshelf` | bool | false | V2-12：附魔台书架环计数数据旗（纯数据旗，beacon_base 双生——核心读属性不读块 id，硬规则 3）；**仅面板打开时求值**（无面板 = 零成本）。shipped = bookshelf true |
| `fluid` | 字符串 | 无 | 流体参考块：词汇表 **water / lava / honey**（FLUID-6，其他名拒）。其纹理点亮流体面、`light_emission` = 该种类格光（lava 15；honey 无）。shipped 3 参考块均 solid=false/opaque=false/transparent=true/destroyable=false |
| `hardness` | 数字 ≥ 0 | 1.0 | 破坏耗时 = 1.5 s × hardness ÷ 选中物品有效速度；≤0 且可破坏 = 即时 |
| `light_emission` | 整数 [0, 15] | 0 | 发光（torch 14 / lava·glowstone·fire·campfire 15） |
| `light_opacity` | 整数 [0, 15] | **由 opaque 派生**：opaque→15 / 非 opaque→0（LGT-2：缺省 = 未设字段块逐位复现旧行为；显式值覆盖派生） | 光**进入**该格每格扣减（MC 同款刻度）：15 = 全挡、0 = air 同款、中间 = 每层扣 N。shipped：leaves 族 2 / glass 1 / slab·stairs×4·wall·fence 0（LGT-5 半块透光）。**流体格不读本字段**（固定 3/格 = P51 简化）；cross/glass_pane 禁 15（init 点名） |
| `drops` | 物品名（`""` = 不掉落，如 glass） | 空 = 不掉落 | init 对 ItemRegistry 存在性（点名） |
| `drops_count` | 整数 [1, 64] | 1 | 单次破坏数量；须 ≤ 掉落物 max_stack（init）；无 drops 却给 = 拒 |
| `drops_count_by_state` | 整数数组（元素 [1, 64]），**须配 `model.state_count` 且长度 == state_count** | 无 | V2-17：per-state 破坏主掉落数（双 slab 破坏掉 2 = MC 材质守恒）；无显式 state_count 的块携带（含 stairs 隐式状态）= 死数据点名拒；零数组/非整/长度不配全点名；game_init per-state 数 ≤ 掉落物 max_stack 交叉检。shipped = stone_slab [1,1,2] |
| `mine_xp` | 整数 [0, 9] | 0 | V2-11 XP：挖掘完成发该值经验球（0 = 无球）；非整/负/>9 点名拒（球值上限，无分裂机制）。shipped = coal_ore 1 / diamond_ore 5 / iron_ore 0 / gold_ore 0（现代 MC 1.17 raw 化值 = 定案：铁金矿挖掘无 XP） |
| `chance_drop` | `{item, count, chance}` **严格三元组**（未知键点名） | 无 | P49：概率次级掉落，与 `drops` 独立（一次破坏最多两个掉落实体）；count [1,64]、chance (0,1]（1 = 保底）；init item 存在 + count ≤ max_stack；**确定性 roll** `rollChanceDrop(world_seed, x, y, z, chance)`（splitmix64 + 位置 hash3 + 专用 salt 纯函数 = 同世界同格每次破坏同果，无 per-instance RNG） |
| `chance_drops` | 同款三元组**数组**（非空、逐条目点名） | 无 | P52：每条目**独立 roll**（条目索引去相关 salt——同格各条目互不追踪、也不追踪单条目 roll）；**与 chance_drop 互斥**（同写拒）。shipped：grass [wheat_1×1@0.125] / leaves [stick×1@0.02, apple×1@1/128, **oak_sapling×1@0.05**（GEN-2 种植闭环）] / spruce·birch_leaves [对应 sapling@0.05, stick@0.02] / lapis_ore·glowstone [drops_count 2 + 两个 50% 条目 = 2..4 均匀] |
| `interact` | 字符串 | 无 | **词汇表 = crafting_table / furnace / campfire / enchant**（其他值 = 启动报错点名）：右键命中打开对应面板/交互而非放置（enchant = V2-12 附魔台，开 EnchantUI 面板，见附魔节） |
| `dye_base` + `dye_color` | 非空字符串，dye_color 须配 dye_base | 无 | P41 染色家族（见染料节）；dye_base 与 interact 互斥 |
| `beacon_effect` / `beacon_effects` | 非空效果名 / 非空效果名数组（逐条目点名） | 无 | P83/P91：信标施加的效果——**两字段互斥**（同写拒）；单值 = 1 元素列表向后兼容；init 逐条目存在性（grows_to 纪律）；每帧在范围内刷新（0.5 s 周期，火源范式）。shipped beacon = `["absorption", "speed"]` |
| `fuel_time` / `smeltable_to` | 数字 (0,1000]（显式 0 拒）/ 非空物品名（显式空串拒） | 无 | CRAFT-6：block item 继承（木 20 s 燃料 / sand→glass / lava_bucket 1000 s 走物品侧） |
| `ignites_to` | 非空块名（显式空串拒） | 无 | P76：点火器右键该块 = 原位变体（**烤制/状态保留续烤**、不消耗物品、"ignite" 音）；init 存在性。shipped = campfire_extinguished → campfire |
| `stair_rotates_to` | 非空块名（显式空串拒） | 无 | P92：4 楼梯变体成环的指向字段；**ENT-2 后旋转改 per-instance 朝向（(f+1)%4，块 id 不变），本字段保留解析 + init 存在性但不再驱动旋转**（shipped 4 变体各指下一） |
| `grows_to` + `growth_ticks` | 非空块名（自引用拒）+ 整数 [1, 1'000'000]，**严格配对** | 无 | P48 作物生长链（见作物节）；`tree` 块（sapling）以 growth_ticks 为**每阶段**节奏，与 tree 配对 |
| `grows_on` | 非空块名（显式空串拒） | 无 = 无要求 | P50 生长土壤门（下方一格须为该块）；init 存在性 |
| `growth_light` | 整数 [0, 15] | 0 = 无要求 | P50：作物格 max(天空光, 方块光) 最低值（wheat = 9 = MC） |
| `dry_slowdown` | 整数 [1, 64] | 1 = 无减速 | P50：无水时 roll 模数倍数（wheat = 2 = MC 干耕地半速） |
| `plant_on` | 非空块名**数组**（逐条目点名） | 无 = 不限 | P53 放置土壤门（目标格正下方须为之一）；init 逐条目存在性；与 grows_on 相互独立 |
| `tree` | **严格六键** `{trunk, leaves, min_height, height_range, leaf_radius, shape}`（缺一/未知键点名 `tree.<key>`） | 无 | GEN-2 树生长（见树种节）；trunk/leaves 名在目录 load 后交叉校验（可指后续文件） |
| `water_reaction` | `{source, flowing}` 严格配对（均非空块名）、**须流体块** | 无 = 惰性流体 | P51：6-邻水时的反应产物（init 两名存在性）。shipped lava = obsidian/stone |

顶层未知键 = **警告**（不是拒）；属性类型错 = 点名拒（`checkField`：
"block property '<key>' must be a <type>"）。

### model 字段全集

`shape`（可选，缺省 = full_cube）词汇表 7 种：**full_cube / slab / stairs /
cross / wall / fence / glass_pane**（其他值点名拒）。纹理键纪律按 shape：

| shape | 纹理键 | 属性强制 | 说明 |
|---|---|---|---|
| full_cube（缺省） | `top`+`side`+`bottom` 三键必填（给 `cross` 键 = 拒） | — | 整格实心（高度感知碰撞自 P85 起对满块恒 1.0 位级不变） |
| slab | 同上三键 | — | P85 底部半盒（实心区 [y, y+0.5]）；V2-17 三态（stone_slab state_count 3，见下状态段）；床 15 块 = Slab 形无 state【V2-13，单格适配 MC 1×2 记档偏离】 |
| stairs | 同上三键 + 可选 `stair_facing` 整数 [0,3]（0 北/1 东/2 南/3 西） | — | P87/P89 楔形；**隐式 per-instance 状态（count 4，default = stair_facing）**；P97 放置自动朝向（低端朝玩家视线，`stairFacingFromYaw` 纯函数）；斜面碰撞按 AABB 覆盖最高斜面点 |
| cross | **仅 `cross` 单键**（top/side/bottom = 拒） | 强制 solid=false, opaque=false, transparent=true, light_opacity≠15（属性给错 = 拒） | P45 两条交叉竖直面片（混合 pass、双面、自身格光） |
| wall | 同上三键 | — | P95 1.5 格高柱 barrier（不能穿过也不能跳过）；P96 墙-墙共享侧面剔除 + 墙-满块帽带 |
| fence | **仅 `side` 单键**（top/bottom/cross = 拒） | — | P98 交叉细柱 + 连接横杆，1.5 barrier（横杆只朝栅栏/满不透明邻居） |
| glass_pane | **仅 `side` 单键**（top/bottom/cross = 拒） | 强制 opaque=false, transparent=true, light_opacity≠15（拒）；solid = true（缺省，不另强制——薄片挡移动，1.0 踏阶门可跳） | P99 1/16 薄片 + 透明 pass 连接剔除 |

`state_count` + `state_default`（ENT-2，**严格配对**：缺一拒）：整数
[1, 255] / [0, state_count)；**stairs shape 显式写 = 矛盾点名拒**（自带
隐式状态）。带状态块 = World 每格 byte（255 = 未设哨兵）：放置收口写
default、潜行右键原地轮换 `(s+1)%state_count`（"place" 音、点击消费）、
存档 format 15 `states` 段全量持久化。**约束**：状态值不得改变块光属性
（opaque/light_opacity 是 per-block 非 per-state）；**worldgen 放置源禁
stateful 块**（biome surface/subsurface/decorations、ores.veins、trees
trunk/leaves——init 交叉校验点名：per-instance 状态没有 worldgen 写入方，
存档 journal 从 block 派生 states 依赖此不变量；sapling 因此天然不野生）。

`moisture_top`（GEN-9）：**FullCube-only**（其余 shape 点名拒）、严格非空
串；湿态顶面纹理 tile（存在性 = requiredTextures init 交叉校验）——顶面
按"Chebyshev 4 同层有水"逐帧派生（`soilIsWet` 纯谓词 = 生长门同款
半径），湿/干邻格永不贪婪并 quad。shipped = farmland `farmland_wet`
（atlas 空位 [9,4] 免重锚 + 同 salt 暗化 0.72×）。**零 format**（湿态 =
世界派生量）。

**slab 三态**（V2-17，stone_slab 首消费，**零 format bump**——states =
format 15 机制，无 state → 有 state = 同数组新条目）：`state_count 3` +
`state_default 0`（0 = 底部 slab [y, y+0.5] 既有位级 / 1 = 顶部 slab
[y+0.5, y+1] / 2 = 双 slab 满格）+ properties `drops_count_by_state
[1,1,2]`。放置瞄准规则 = `slabStateFromHit` 纯函数（手持 slab 物品 =
shape Slab ∧ hasState 数据旗：命中点 = eye + forward × distance——
**顶面 → 0** = 现行为位级根 / **底面 → 1** = 天花板贴放 / **侧面
local-y > 0.5 → 1 否则 0**，0.5 严格不等式 kSlabHalf 单常量，MC 精确）；
**同格同 id 互补 state 放置 = 合并双 slab**（消耗 1 = 材质守恒；仅绕过
canPlaceAt solid 拒、保 Y 范围 + 玩家重叠检查、跳 canPlantAt）；**破坏
per-state count**（双 slab → 2 件）；**蹲旋转 Slab 形排除**（state = 垂直
位非朝向，环转 2→0 销毁半块材质不守恒 = 记档偏离；台阶朝向环不受影响）。
solidRange 物理扩展 0→{0,0.5} / 1→{0.5,1} / ≥2→{0,1}（顶 slab 顶面恰
1.0 可踏升 = MC 同款；床/stairs/wall/fence/玻璃板/满块现值逐位不变
红线；mob 生成 spawnFloorTop 纯函数同款消费）。raycast **partial-aware**
（slab 格自空气半入格 = 穿透、内表面 y=0.5 交点命中 ±Y 法线；块射线 5
消费点 + 桶 ×2 + 骨架 LOS + 第三人称遮挡共享一致零特判；非 slab 格位级
不变；**记档行为变更** = 瞄准底 slab 空气半破身后块，MC 同款）。床 15
块 Slab 形无 state_count = hasState false 位级不变（门 = hasState() 非
shape）。**stateful 块存档场景须配 `states` journal 条目**（init 双向
交叉检点名拒 = 输入静默丢失，PITS 383①）。

### init 交叉校验清单（启动即查，报错点名）

drops / chance_drop(s) 每条目 / grows_to / grows_on / plant_on 每条目 /
ignites_to / stair_rotates_to / water_reaction 两名 / tree.trunk+leaves /
beacon_effects 每条目 / drops_count_by_state（长度 == state_count + 每
state ≤ 掉落物 max_stack）/ sound_material 3 件 wav / 流体块 fluid 名
词汇表 / dye 全家族（每色染料存在 + 每 (base, 色) 变体存在——现 = wool
15 + bed 15【V2-13 第二染色族】）/ 有 flamable ⇒ 有 fire 块 / 图集 tile
存在（含 moisture_top）。

## 新增一个配方（无需改核心代码）

`assets/data/recipes/<id>.json`。解析纪律（`src/item/recipe.cpp`，经
ItemRegistry 交叉检查，错误信息点名配方）：

- `id` 非空且唯一；顶层未知键警告。
- `type` 可选："shapeless"（缺省）/ "shaped"，其他值拒。
- `output` = `{item, count}`：item 已注册点名、count ∈ [1, 产物 max_stack]。
- **shapeless**：`ingredients` = 非空数组 `[{item, count}]`（count ≥ 1、
  同一物品不得重复出现 = 合并为一条，点名）；**不得**带 pattern/key。
- **shaped**：`pattern` = 1..3 行、每行 1..3 字符（空格 = 空位；至少 1 个
  物品格）、`key` = 单字符 → 物品名（未知物品点名）；**双向交叉**：每个
  pattern 字符必须有 key 条目、每个 key 条目必须被 pattern 使用（未用 =
  静默笔误，拒）；**不得**带 ingredients。
- block item 可作原料/产物（继承 = 同一物品注册表，如 sand→glass 配方
  产物是 block item）。

```json
{ "id": "iron_pickaxe", "output": { "item": "iron_pickaxe", "count": 1 },
  "ingredients": [ { "item": "iron_ingot", "count": 3 }, { "item": "stick", "count": 2 } ] }
```
```json
{ "id": "iron_pickaxe", "type": "shaped", "output": { "item": "iron_pickaxe", "count": 1 },
  "pattern": ["III", " S ", " S "], "key": { "I": "iron_ingot", "S": "stick" } }
```

`craft()` 语义（E 面板 shapeless 行）：原料按槽序升序消耗（hotbar 先于
背包），先在背包**副本**上模拟"消耗+入包"，成功才提交——失败时背包逐位
不变（产物是原料之一的配方也正确）。网格匹配（工作台 3×3 / E 面板 2×2）：
pattern 偏移扫描（任意左上偏移放进网格、空格对应格必须为空），**先 shaped
后 shapeless**、配方按文件名字典序。

**游戏内呈现**：`E` 背包面板右侧 = **shapeless** 配方列（可合成白色、
不可合成灰色，点击行执行 craft）；放置并右键工作台 = 3×3 面板，右侧
**shaped 配方提示列（CRAFT-4，两列排布**——CRAFT-4 时 34 个 shipped shaped
配方单列拟合因子 0.34 不可用），每行 = mini 3×3 pattern（暗格 + 键位物品图标，
按定义左上对齐 = 合法摆放）+ 输出图标 + "name xN"（`canSupply(patternBag)`
纯函数着色）；**提示行点击 = 消费性 no-op**（不合成不动背包，吞点击 ≠
槽外点击丢 carry）。当前 shipped 86 条（shaped 52 / shapeless 34）：
方块/材料/杂项 44（planks/stick/stone/sandstone/smooth_sandstone/
iron_block/gold_block/diamond_block【V2-7：9 锭方块，iron_block
先例】/torch/farmland/bucket/glass_pane/fence/stone_slab/stone_wall/
楼梯 4/crafting_table/**furnace【v1.34.0：8 cobblestone 围圈 = MC 摆法，
补齐 Phase 40 起炉子无获取途径的内容缺口】*/arrow/bread/campfire/
flint_and_steel/golden_apple/**bed 15【V2-13：shaped 3 同色 wool + 3
planks = MC 精确】/**enchanting_table（shapeless 4 obsidian + 2 diamond
+ 3 lapis）+ bookshelf（shaped PPP/LLL/PPP）【V2-12：均 lapis 代 book
偏离】/**shears【V2-15：shapeless 2 iron_ingot】）+ 工具 9 + 弓 6 +
护甲 12 + 染料 15。

## 新增一个附魔（无需改核心代码，V2-12）

`assets/data/enchantments/<id>.json`——数据驱动新面（目录按名排序装载，
`EnchantmentRegistry` init 注册；**字段值纪律严格**：未知 effect/class、
倒置/重叠 eCost 区间、零权重、未知 incompatible 全点名拒）：

| 字段 | 类型 / 范围 | 说明 |
|---|---|---|
| `id` | 非空串，唯一 | 注册表键（存档 `enchants` 交叉校验词表） |
| `name` | 非空串 | 显示名（面板 offer 行「名 罗马 成本」） |
| `effect` | 字符串，词汇表 {sharpness, protection, efficiency, unbreaking, power, speed, infinite} | 代码枚举（grants_effects 先例：数据点名、核心读枚举，不读内容 id） |
| `classes` | 非空数组，词汇表 {sword, axe, pick, shovel, armor, bow, hoe} | 适用物品类（对物品 `enchant_class` 匹配） |
| `weight` | 整数 ≥ 1 | offer 选择权重（零/负 = pick 破坏，拒） |
| `ecost` | `[min, max]` 整数对数组，非空 | **每级一条区间**（max = 区间数）；min ≥ 1 / min ≤ max / 区间须连续升序（后段 min > 前段 max，levelForCost 最高段走查定义所系）——逐条点名 `ecost[i]` |
| `incompatible` | 可选，已注册 id 数组 | 互斥对（双向查）；shipped 7 项无对；typo = 点名拒（drops 纪律） |

**shipped 7 附魔**（区间 + 权重 MC 精确，11-04 wiki 核定入档）：

| id | 名 | classes | weight | max | eCost 区间 | 效果 |
|---|---|---|---|---|---|---|
| sharpness | 锋利 | axe | 10 | 5 | 1–11/12–22/23–33/34–44/45–65 | 近战 +（L+1）点 |
| protection | 保护 | armor | 10 | 4 | 5–12/13–20/21–28/29–37 | +4L 护甲点（共享 20 点 cap 偏离：MC 保护独立 + 总 80%） |
| efficiency | 效率 | pick/shovel/axe | 10 | 5 | 1–10/11–20/21–30/31–40/41–91 | 正确工具（block ∈ fast_against 门）breakSpeed +（1+L²） |
| unbreaking | 耐久 | axe/pick/shovel/armor/bow | 5 | 3 | 5–12/13–20/21–71 | 每损点掷骰跳过（工具 L/(L+1) / 护甲 2L/5(L+1) = MC 1.21 remove_binomial 精确，状态 = 伤害史确定性，无帧依赖） |
| power | 力量 | bow | 10 | 5 | 1–10/11–20/21–30/31–40/41–56 | shoot_damage +L |
| speed | 速度 | bow | 10 | 3 | 12–31/32–50/51–91 | shoot_speed +L（记档偏离：MC Quick Charge = 蓄力时间，项目 = 弹速范式） |
| infinite | 无限 | bow | 1 | 1 | 20–50 | 发射不耗尽最后一支（∨ 物品 `infinite` 字段） |

**附魔台 / 书架（2 块 2 配方）**：
- `enchanting_table`：hardness 5 / light_emission 7 / `interact: "enchant"`
  （RMB 开 EnchantUI 面板）/ drops 自身【记档偏离：MC 镐门】/ **full_cube**
  【记档偏离：设计 3/4 盒触 mesher + 碰撞红线，定案全盒】/ stone 音族。
- `bookshelf`：hardness 1.5 / `enchant_bookshelf: true` 数据旗 / full_cube /
  wood 音族。
- 配方：台 = shapeless 4 obsidian + 2 diamond + 3 lapis；书架 = shaped
  PPP/LLL/PPP（**均 lapis 代 book = 记档偏离**，项目无纸/书链）。

**确定性 offer**（core/enchanting 纯函数，**全整式禁 double** = -O3 融合
风险，PITS 30 单推进流纪律）：种子 = worldSeed ^ hash3(台位) ^
enchantability ^ xp_seed（player 存档键，成功附魔后推进）；xpBase/三槽
成本/lapis 1-3（均匀 = 记档偏离）/eCost 含 randTri 整式/选择循环（候选 =
类匹配 ∧ eCost 入区间取最高级 → 权重 pick → incompatible 移除 → 继续 =
v%(eCost+150)<150 → eCost/=2 回环）= MC 精确；bookshelfCount = 5×5 环 ×2
层 cap 15（**无 MC 视线规则** = 记档偏离），**仅面板打开时求值**（无面板
零成本 = 结构免疫根）。XP 扣减 = 扣整级、条内余量保留（T(L−c) +
pool−T(L) 整数精确）。

**物品侧**（见物品字段表 `enchant_class`/`enchantability`/
`enchant_material`）：shipped 22 件可附魔 = 工具 9（wood 15 / stone 5 /
iron 14）+ 护甲 12（leather 15 / iron 9 / diamond 10）+ bow 1（**仅基础
bow**，变体弓保持替身定位）；lapis `enchant_material: true` = 青金石槽
数据驱动门。

**存档**（format 27）：物品 `enchants {id: level}` 三处（inventory 槽 /
armor 占位槽 / item 实体，omit-when-empty = 无附魔堆栈逐字节旧形）+
`player.xp_seed` 恒写；载时交叉校验（level > max / 类不兼容 / 未知 id /
incompatible 对全点名，validateEnchants 与 validateStats 同款）。详见存档
节。加新附魔 = 1 个 JSON +（如需新效果行为 = 代码枚举扩展，非纯数据）。

## 新增一个染料 / 染色方块（P41 框架；P45 起颜色零代码）

颜色名是**纯数据词汇表**（P45）：`dye_<color>` 物品注册即扩展词汇表，
**加新颜色名 = 零代码改动**。当前 15 色：`white`/`black`/`brown`/`red`/
`yellow`/`orange`/`pink`/`gray`/`silver`（P41）+ `blue`/`green`/`purple`/
`magenta`/`light_blue`/`cyan`（P45；项目 silver 位 = MC light_gray）。

1. **染料物品** `assets/data/items/dye_<color>.json`：
   `{"id", "type": "material", "max_stack": 64, "icon": "dye_<color>",
   "dye_color": "<color>"}`（`dye_color` 显式空串拒）；图标
   `assets/textures/items/dye_<color>.png`（32×32，`tools/generate_item_icons.py`
   的 `DYE_PALETTE` 同款确定性生成——调色板加一行即可）+ model（ENT-9 后
   全物品惯例）。
2. **染色变体方块** `assets/data/blocks/<base>_<color>.json`（white 特例 =
   `<base>.json` 本身）：`properties` 加 `"dye_base": "<base>",
   "dye_color": "<color>"`（两者必须同时给——dye_color 须配 dye_base；
   dye_base 与 interact 互斥）；纹理 tile 加进 `block_atlas.json` 空位
   （16×8 网格现 6 空位 = (13,6)-(15,7) 区段；**网格扩容 = 移位所有既有 UV → base18 重锚**，
   见纹理纪律节）。
3. **合成配方** `assets/data/recipes/dye_<color>.json`（shapeless；当前 15 条
   见 GAMEPLAY「染色」；来源块 = 世界内容：lapis 矿脉 / 仙人掌 / 十字花 /
   熟肉 / 骨粉 / 金锭）。
4. **init 交叉校验**（启动即查，报错点名）：每个 dyeable 块的 `dye_color`
   必须有同色染料物品；**每个 (dye_base, 已有染料颜色) 组合的变体块必须
   存在**——加新颜色时**所有** dye 家族都要补变体块（当前 = wool 家族
   15 个 + **bed 家族 15 个**【V2-13 第二染色族首消费，见床节】）；加新
   dye 家族 = 15 个变体块。

交互（MECHANICS「染色」）：选中染料右键 dye 家族方块 = 换块 + 消耗 1；
同色重复不消耗（v1 规则）；染色音效 = cloth.wav。

## 新增一个床（V2-13，15 色染色族先例 + 睡眠三门）

床 = **第二个染色族**（wool 15 色 = 第一族；dye 机制零新代码）：
- **块 15 件**：`dye_base: bed` + `dye_color` 15 色（wool 同款词表无
  lime，white = base `bed.json` 无后缀）/ **Slab 半格形**（MC 1×2 两格
  无项目先例，单格适配 = 记档偏离 1；无 state_count = 恒 state 0 位级
  不变，见方块节 slab 三态）/ hardness 0.2 MC 精确 / cloth 音族 /
  drops 自身 / 无 interact（dye_base 互斥，init 点名）。
- **纹理 15 tile**：`generate_block_textures.py` `make_bed_tile`（彩色
  毯 = WOOL_PALETTE 同款色 + 头端白枕带，新 salt 985-999，既有零变化
  md5 实证），atlas 104→119 零移位。
- **配方 15**：shaped `["   ","WWW","PPP"]`（3 同色 wool + 3 planks =
  MC 精确）。染色 = Phase 41 机制免费（RMB 持染料于床 = 重染色，落回
  既有路径；init 染色家族交叉校验自动覆盖 15 变体）。
- **音**：新 `sleep.wav`（确定性合成，既有 wav 重跑字节不变）。

**睡眠三门**（core/bed 纯函数 + game 薄适配；RMB 链插入面板分支后、
染色前——命中 `dye_base == bed` 族且非持染料；持染料落回染色路径），
定案序：
1. **门 b 距离**：玩家脚心 vs 床格中心 3D ≤ 3.0（MC 精确，平方比较
   含边界）。
2. **门 c hostile 盒**：床格中心水平 ±8 / 垂直 +5/−1 闭盒内无
   hostile（数据旗判据，无物种 switch，spawn 保护同款；被动/掉落物/
   箭豁免）。
3. b+c 过 → **设重生点**（床格中心；MC：RMB 恒设点，夜或日）→
   **门 a 夜窗**：t ∈ [0.55, 1) ∪ [0, 0.05)（kDuskEnd/kDawnStart；
   MC sky≤11 略窄 = 记档偏离 3）：夜内 = **即时跳夜**
   （`setTimeOfDay(0.05)` = 06:00 黎明 + 雨/雷 → CLEAR 纯重置 +
   "sleep" 音空间化于床 + "sleep" 教程词；MC 101-tick 入睡动画不做
   = 记档偏离 2）/ 夜外 = 仅设点（MC 白天 RMB 语义）。
   **门 b/c 拒 = 全拒含设点**（记档偏离 6）。

**重生点**（format 28，见存档节）：`player.respawn` int 三元组恒写
（新档 = 世界出生点 init 写入；format 28 = 本阶段唯一存档变更）。R 键
重生收口 `Game::respawnPlayer` **验床**（纯函数 `bedRespawnValid`：该位
任意床族块 + 房间空间【床格支撑豁免，hasRoomAt 先例】→ 床位；否则回退
世界出生点 + 重新钉位 = 床破坏/堵塞永久回退，MC"仅 respawn 时查"同款）。
mob 炸床不做（记档偏离 5：项目无爆炸机制，门 c 覆盖主语义）。床不入
worldgen = 无输入基线结构免疫。

## 新增一个 Mob（无需改核心代码，Phase 20 起全数据驱动）

1. `assets/textures/mobs/<tile>.png` —— 16×16 RGBA（`tools/generate_mob_textures.py`
   同款：splitmix64 确定性噪声 + 简单五官；box 六面同采一个 tile，-Z 面 =
   正面）。**mob tile 与块 tile 共用同一图集**：在
   `assets/data/textures/block_atlas.json` 的 `tiles` 里给它空位 `[col, row]`
   （16×8 网格现 6 空位 = (13,6)-(15,7) 区段；勿动已有条目）。
2. `assets/data/mobs/<id>.json` —— mob 定义。`properties`（必填 + 可选）：

   | 字段 | 类型 / 范围 | 缺省 | 说明 |
   |---|---|---|---|
   | `health` / `speed` | 数字 > 0 | 必填 | |
   | `detect_radius` / `attack_range` / `attack_damage` | 数字 ≥ 0 | 必填 | 0 = 被动/无害（AI 视为"从不"） |
   | `attack_cooldown` / `wander_interval` | 数字 > 0 | 必填 | |
   | `spawn_max_light` | 整数 [0, 15] | 0 = 只在黑暗 | 光照门；shipped hostile = **7**（LGT-3 MC 月光配对：夜露天 7≤7 保夜险 / 火把 14>7 免疫 / 白天 15>7 不生成）、passive = 15 |
   | `hostile` | bool | false | Phase 25 难度系统数据开关（保护环 + 伤害/生成速率缩放，见 config `mobs.difficulty`） |
   | `undead` | 严格 bool | false | SURV-5：不死族——`undead_exempt` 效果（poison）免疫（wither 照中 = MC 独一份性） |
   | `shoot_range` / `retreat_distance` | 数字 ≥ 0 | 0 = 近战 | Phase 27 远攻对；shoot_range > 0 = 远攻者 |
   | `shoot_speed` | 数字 (0, 100] | 0 | 远攻必填（配对点名） |
   | `ammo` | 非空物品名 | 无 | Phase 42：射出的箭插墙后被玩家拾取时返还的弹药物品；远攻必填 + init ItemRegistry 存在性；显式空串拒 |
   | `arrow_effect` | `{name, duration, tier?}` 严格（未知键点名；duration > 0） | 无 | SURV-5：箭矢命中施加（skeleton `{wither, 10}`）；**须 shoot_range > 0**；init：效果存在 + tier < tier_max（**不要求 affects_mobs**——skeleton 箭命中玩家，与 bow 侧纪律的差异） |
   | `on_hit_effect` | `{name, duration, tier?}` 严格（duration > 0） | 无 | Phase 46：**近战命中落定**时施加（spider `{poison, 15}`）；init 对 EffectRegistry 存在性 |
   | `milk_with` + `milk_gives` | 非空物品名，**严格成对**（只给一边拒） | 无 | Phase 43/SURV-10：挤奶（cow `bucket → milk_bucket`）——手持 milk_with 右键 = 消耗 1 + 产出 1 + "milk" 音；init 两物品存在；**SURV-10 = MC 5 分钟冷却**（per-mob 帧锁定 18000 帧，冷却中点击照吞但不耗桶不产奶；mob 瞬态 = 冷却从不入档） |
   | `drops` | 数组 `[{item, count}]` 或 `[{item, count_min, count_max}]` | [] = 不掉落 | Phase 28/38：count 整数 [1,64]，或 min/max 整数 [0,64] 成对（0 = 可能不掉）；两形混用/只给一边拒；`drops_count` 旧字段显式拒（迁移防护）；init 每条目 ItemRegistry 存在 + max ≤ max_stack |
   | `xp` | 整数 [0, 9] | 0 = 无球 | V2-11 XP：击杀掉该值经验球（球值上限，无分裂机制，>9/非整点名拒）。shipped：hostile zombie/skeleton/spider 5 / passive pig/cow/sheep/chicken 2（MC 1-3 中值 = 定案）/ bat 0 |
   | `flying` | 严格 bool | false | V2-14 飞行（悬停巡航 AI，见下 bat 条；**唯一硬互配：flying ⇒ spawns_in_air**——地面生成的飞行者无意义，点名拒）；absent = pre-V2-14 语义位级不变 |
   | `spawns_in_air` | 严格 bool | false | V2-14：in-air 生成分支（顶向扫描 + box 全空气无流体 + 光 ≤ spawn_max_light + **跳过 floor 带**（任何 Y = MC）+ 出生位盒底格中心） |
   | `ambient_sound` + `ambient_interval` | 非空音名 + 数字 > 0，**interval 须配 sound 名**（孤 interval = 死数据点名拒） | 无 | V2-14：氛围音（音名 init file-based 交叉检；每帧 p = 1/(interval×60) 流 roll 空间化，无 ambient mob 流零消费）。shipped = bat squeak / 3.0 |
   | `wool_drops` + `wool_weights` | 严格 bool + `[{color, weight}]` 数组（color 非空串、weight 整数 [1, 10000]、重名拒、空列表拒，逐条目点名），**严格对**（孤字段点名拒 = milk pair 纪律） | 无 / 无 | V2-15 染色羊（见下）：毛色随实例（出生加权 roll / 剪毛 / 染色 / 死亡彩毛）；color 词表点名拒放 **init 交叉检 `checkWoolDrops`**（解析期无 ItemRegistry 可见 = ammo/milkWith/drops 同款纪律） |

   `model`（**严格对象**：未知键点名拒——top-level def 仍警告容忍，
   model 内不容忍，ENT-1 起）：`width`/`height`（>0）+ `boxes` 非空数组
   每盒 `{pos[3], size[3] > 0, texture（atlas tile 名，init 存在性）,
   name?（部件名，非空、模型内唯一）, faces?（per-face 纹理覆盖，V2-2）}`，
   盒须在 `width × height` AABB 内（bottom-center 原点：x/z ∈ [−w/2, w/2]，
   y ∈ [0, h]，越界点名）+ `swings?`（下）。init 对每个 box texture（含
   faces 各 tile）交叉校验图集存在。**`faces`（V2-2 per-face 纹理通用
   能力）**：`{front/back/left/right/top/bottom: atlas tile 名}` 严格对象
   （front = −Z 面）——未知 face 键 = typo 点名拒、空串/非串拒、**部分键
   合法**（仅命名面覆盖，其余回落 `texture`）、absent/{} = 单 tile 位级
   不变。shipped = Steve 玩家模型（ph_face/ph_hair/ph_side/pa_arm/
   pl_pants/pl_boot）+ pig 头盒 front = pig_face 8×8 脸【首个 mob 消费】。

   **`swings`（ENT-1 四肢摆动，纯渲染层）**：数组每条目**严格三元组
   {part, amplitude, group}**（未知键点名拒三级）：`part` = 本模型 box 的
   `name`（解析期交叉校验存在性，点名 `swings[i] references unknown
   part`）+ 一个 part 不得被两条 swing 认领；`amplitude` ∈ (0, 1] 弧度
   （越界点名）；`group` ∈ {0, 1} 整数（半周相位组 = 对角步态）。动画
   机制（帧锁定、编译期定点正弦表、零运行期 libm）：移动相位 +2/帧
   （整周期 0.533 s）、静止 −4/帧 16 帧收步、相位 0 = 静态位级恒等
   fast path；pivot = **部件盒顶面中心**（局部 X 轴旋转，法线同旋）——
   **ENT-1b 内容惯例：摆腿盒顶缘对齐身体底缘**（pivot = 髋关节，出体
   穿模谓词 `checkSwingClip` 单测验收：相交体积 ≤ 腿 25%）。shipped：
   chicken 2 腿（0.4，group 0/1 对角）/ cow·sheep 4 腿（0.5，FL+RR = 0、
   FR+RL = 1 = MC 四足对角）/ spider 8 腿（0.45，偶奇行分组，翅不摆）/
   zombie·skeleton 人形无独立 arm part = 不造几何（记档，99.7 几何重做）/
   bat 双翼（0.9 rad 反相扇翅 = chicken 翅盒不摆先例的反例，V2-14）。
   `config mobs.limb_swing` = false → 全静态（v0.9.0 对照）。
3. 生成名单：`assets/data/mob_spawning.json`（全数据驱动，严格解析）：
   `check_every_frames` [1, 100000]（shipped 30）/ `first_spawn_frame`
   [0, 1000000]（60）/ `max_mobs` [1, 1000]（4）/ `min_radius` <
   `max_radius`（4 < 6）/ `despawn_distance` > max_radius（40，离玩家
   消散）/ `mobs` 非空数组 `{"id", "weight"?}`（weight 整数 [1, 10000]，
   缺省 1 = 等概率；旧纯字符串条目显式拒；重复 id 拒）。**生成环 = 整数
   格偏移环，半径带 [min_radius, max_radius]（shipped 4..6）**，锚
   **世界出生点**（floor band ±2 + headroom 2 格含流体 + 自然地形 =
   潜在生成平台 + ±0.5 jitter = spawner 精确语义，PITS 307：ENT-8 文档
   曾误记 8-16 = Phase-25 保护圈混淆）；敌对生成保护圈（Phase 25）
   easy 8 / normal 12 / hard 16 / peaceful 全球（ENT-8：
   spawnProtectionRadius = 1e6，敌对自然生成全拒 + 被动豁免照生 =
   MC 和平核心）。

**shipped 8 mob**（NORMAL 难度基线数据）：zombie（敌对 undead：hp 20 /
speed 0.8 / detect 16 / range 1.6 / dmg 3 / cd 1.0 / light 7 / xp 5；掉
rotten_flesh ×1）/ skeleton（敌对 undead 远攻：hp 20 / shoot_range 14 /
retreat 3 / shoot_speed 15 / ammo arrow / arrow_effect {wither, 10} /
light 7 / xp 5；掉 bone 0-2 + arrow ×1）/ spider（敌对：hp 16 / speed 1.0 /
range 1.6 / dmg 2 / light 7 / on_hit_effect {poison, 15} / xp 5；掉
string 0-2 + spider_eye 0-2【V2-7 纯收集记档】）/ pig（被动【V2-5】：
hp 10 / speed 0.6 / light 15 / xp 2；掉 pork 1-2 + leather 0-1；头盒
faces.front = pig_face 8×8 脸 = V2-2 per-face 首个 mob 消费）/ cow（被动：
hp 10 / light 15 / xp 2 / 挤奶；掉 leather + beef）/ sheep（被动：
hp 8 / light 15 / xp 2；掉 wool 1-3 + mutton 1【V2-15，染色机制见下】）
/ chicken（被动：hp 4 / light 15 / xp 2；掉 feather + chicken）/
bat（飞行【V2-14，项目首只飞行 mob】：hp 6 / speed 1.0 /
**light 3**（洞穴语义 MC 精确：夜底光 7 > 3 露天不可生）/ xp 0 / 无掉落
/ flying + spawns_in_air + ambient squeak 3.0；model **0.9×0.5** 4 box
身/头/双翼（±0.275 = 0.9 极限）【MC 盒 0.5×0.9 竖薄容不下展翼 AABB =
记档偏离】+ swings 双翼 0.9 rad 反相扇翅（chicken 翅盒不摆先例的
反例）；悬停巡航 AI：航向 per-mob 流每 wander_interval 2s 重掷 + 数据
speed 巡航 / 垂直正弦悬停 0.2×sin(2π·frame/60) 帧锁定（bob 初相构造流
首抽，flying 才消费）/ 重力关 + 终端豁免 + 流体旁路 / 撞壁 = 下帧航向
重掷（无寻路 = 记档偏离）/ knockback 0.3s 回巡航；in-air 生成 = 顶向
扫描 + 跳过 floor 带（任何 Y = MC）；消散 40【MC 32 记档偏离】；
ambient squeak 每帧 p = 1/(3×60) 流 roll 空间化）。

**染色羊（V2-15，per-entity 变体机制）**：新物品 **shears**（type tool /
durability 238 MC 精确 / break_speed 2 / `shears: true` 严格 bool 旗，
**仅 tool 可携**非 tool 点名拒 = armor_slot 双生 / **fast_against 整字段
省略** = 不加速任何块（空 `[]` 非法既有钉；后果 = SURV-8 错工具 1/10 罚）
/ 配方 shapeless 2 iron_ingot）。出生毛色 = **per-mob 流首抽加权
roll**（wool_weights shipped：white 850 / black 80 / gray 30 / brown 30 /
pink 10——MC biome 表 → 项目单一 temperate 表整数近似 = 记档偏离；非
wool_drops mob 构造零消费位级红线）；**剪毛**（RMB 链 tryMilk →
tryShear：shears 旗 × wool_drops 旗 × !bare，全数据旗判据无物种
switch）= 1-3 彩毛（1 个 ItemEntity 携 roll count）+ bare + **再生
7200 帧 = 120 s 帧锁定**（MC 吃草门随机 ~12 min，项目无草交互 = 记档
偏离）+ 磨损 1 + shear 音；bare 再剪 = no-op。**染色**（tryDyeMob：
手持 dye × wool_drops × **!bare = MC 剪后拒染**）= 变色 + 耗 1 + cloth
音。**死亡彩毛钩子**（纯函数 `woolDeathDropName`：woolly → 实例色 wool
变体 / bare → 跳过 / 非 wool 位级不变）。渲染 = **per-entity 变体网格**
（swingMeshes 范式：body box 纹理 = f(woolColor, bare) 变体键入 per-
entity 动态网格，懒建 / despawn 释放 / 剪毛染色再生触发重建；
`withWoolVariantTile` 纯函数仅换 texture 几何位级不变；非 wool mob 静态
网格零触碰）——15 色毛 **复用 wool 族块 tile**（同 atlas，init 交叉检
覆盖全 15 色存在性）+ bare = 新 sheep_skin tile【(226,178,164) 肉粉，
atlas (11,6) 零移位】。零 format（mob 不持久化 = 实例态 session-only）。

**行为面（代码驱动，无数据字段）**：AI 状态机（wander/chase/retreat，
ENT-6 卡住 0.5 s 进度 <25% → BFS 寻路跟随 + 0.5 s 重规划 + 1 s lost 冷却
+ 2 s 节点超时逃逸）/ 踏阶 ENT-3（与玩家同一 `Entity::stepUp` 判定：
slab 0.5 / 满块 1.0 / 玻璃板可踏、墙/栅栏 1.5 不可踏）/ 涉水 ENT-4 /
FLUID-3（流体内水平 ×0.6 命名常量 kUnderwaterSpeedFactor、游泳位半浸没、
踏 1 格岸壁出池）/ 坠落伤害 ENT-5（净落差 > 3.0 且落地无流体，MC 同值）/
燃烧（P58/P73：熔岩/火块格内每帧重点燃 + 离源 8 s 残留、1 HP/0.2 s 可
致死、水瞬时熄灭；PRE-8 余烬火焰视觉）/ 状态效果载体 SURV-5（count==0
fast-exit）/ 瞬态实体（从不入档，读档后不出现）。

## 新增一个状态效果（无需改核心代码，Phase 46 框架 + 后续字段）

1. 图标：`assets/textures/effects.png` 是 16 px/tile 的透明底**条带**
   （当前 176×16 = **11 tile**：0 poison / 1 weakness / 2 regeneration /
   3 absorption / 4 fire / 5 speed / 6 strength / 7 resistance / 8 healing /
   9 harming / 10 wither）。加新效果 = **条带尾部 append 一格**
   （`tools/generate_effect_icons.py` 同款确定性生成；**既有 tile 逐像素
   零变化验证** = SURV-4 128→160 / SURV-5 160→176 先例）；init 对每个
   效果的 `icon` 交叉校验 < tile 数，纹理宽度保持 16 的整数倍。
2. `assets/data/effects/<id>.json`（**扁平 JSON**，无 properties 包裹；
   **字段白名单严格**——未知键点名拒绝）：

   ```json
   {
     "id": "speed",
     "speed_multiplier": 1.2,
     "negative": false,
     "icon": 5,
     "tier_max": 3,
     "tier_steps": { "speed_multiplier": 0.2 }
   }
   ```

   | 字段 | 类型 / 范围 | 说明 |
   |---|---|---|
   | `id` / `icon` | 非空串 / 整数 ≥ 0 | icon = 条带 tile 索引（必填） |
   | `tick_type` | "damage" / "heal" | **缺省 = 无 tick 的纯修饰**；tick_amount/tick_interval 无 tick_type = 拒（配对） |
   | `tick_amount` + `tick_interval` | (0, 10] + (0, 100] 秒，有 tick 必填 | 每跨 interval 结算一次（damage 走玩家受伤路径 / heal 走 `Player::heal` clamp 有效上限【可填吸收层】） |
   | `min_health` | [0, 1]，**仅 damage** | 0 = 无地板可致死 / 1 = 保持 ≥1 HP 永不致死（MC 中毒地板）；heal 带 = 拒 |
   | `min_health_mob` | 整数 0 / 1，**仅 damage** | SURV-5：mob 侧地板（缺省 = 跟随玩家 min_health；poison = 0 可毒死 mob，玩家地板 1 不动） |
   | `melee_penalty` | [0, 100] | 近战扣除攻击点（weakness 4） |
   | `melee_bonus` | [0, 100] | P84 加性攻击点（strength 2，项目刻度） |
   | `absorb_hp` | 整数 [1, 64] | P56 临时 max HP 点数（吸收层；有效 max = base + Σ层；施加 = health 提升增量、到期超出 base 无声掉下） |
   | `speed_multiplier` | (1.0, 3.0] | P84 移动速度乘算（多效果相乘） |
   | `damage_reduction` | [0.0, 1.0) | P84 取**最高**单值（不叠）；减伤次序 armor → resistance → 吸收池 |
   | `instant_hp` | 整数**非 0** [-32, 32]（显式 0 拒） | SURV-4：施加时结算——正 = heal 钳 max / 负 = **damage(bypassArmor=true) 护甲豁免**（MC 药水伤害语义 = SURV-2 同款管线）；**与 tick_type 互斥**（一效果一动作形） |
   | `negative` | bool | 缺省 false；牛奶只清负面（fire = false 不灭火） |
   | `ignites_on_lava` | bool | P57：玩家**在熔岩内**每帧重施加（duration 重置 = 持续燃烧，离熔岩 countdown 至到期）。**至多一个**注册效果可带（init 交叉校验，两个 = 启动失败；零个合法）；**init 另查 `tick_interval × 60` 必须整帧数**（浮点相位容差 1e-4 帧，燃烧格计数纪律） |
   | `extinguished_by_water` | bool | P57：玩家**在水内**瞬时移除（tick 之前执行 = 水内零伤害；水胜熔岩） |
   | `affects_mobs` | 严格 bool | SURV-5：mob 可携带（wither/poison 双目标）——bow `arrow_effect` init 要求本字段 |
   | `undead_exempt` | 严格 bool | SURV-5：`undead` mob 免疫（poison；wither 不带 = 照杀） |
   | `tier_max` | 整数 [1, 3] | SURV-6：授予方可用的档位（0 基，tier < tier_max）；缺省 1 = 单档 |
   | `tier_steps` | 对象 | SURV-6：每档增量，**仅 5 个加性修饰字段可设**（speed_multiplier / absorb_hp / melee_bonus / melee_penalty / damage_reduction——tick/instant 形单档 = 记档偏离）；step ≥ 0；**非零 step 须配基础字段**（死字段拒）；tier_max > 1 须有非零 step（no-op 拒）；tier_max 1 带 step 拒。值 = base + tier × step（`effectTieredValue` 纯函数，**tier 0 = 位级旧值** = 结构免疫根） |
   | `tint` | 4 数组 [r,g,b,a] 全 [0, 1]（非数组/长度≠4/非数字/越界逐条点名） | PRE-7：全屏表现纱（HUD pass 红纱前，每活跃 tinted 效果一个全屏 quad；恒定 alpha = MC 脉动记档偏离）。shipped = poison [0.63, 0.24, 0.86, 0.35]（MC 中毒紫 RGB 160/60/220） |

   **no-op 拒绝**（演进后）：无 tick 且 melee_penalty = 0 且 absorb_hp = 0
   且 speed_multiplier ≤ 1 且 melee_bonus = 0 且 damage_reduction = 0 且
   instant_hp = 0 且 tint alpha = 0 = 报错（PRE-7：正 alpha tint = 表现层
   动作合法）。
3. **施加来源**（全数据驱动）：mob `on_hit_effect`（近战）/ mob
   `arrow_effect`（箭，skeleton {wither,10}）/ 物品 `grants_effect(s)`
   （吃/喝：金苹果 + 6 药水）/ 弓 `arrow_effect`（item 侧：poison_bow
   {poison,60}）/ 信标 `beacon_effects`（P83/P91：范围内每帧 0.5 s 刷新，
   MC 主+副效果范式）/ 熔岩点燃（ignites_on_lava 自动）/ 存档注入（见
   存档节 player.effects，可带 tier = format 20+）。所有授予方均可带
   0 基 `tier`（整数 ≥ 0；范围 = 被引用效果 tier_max，init 点名）。
4. 机制自动生效，无需改核心：真实 dt 倒计时 + 每跨 interval 结算一次 +
   RESET 语义（再喝重置不叠加，档位不叠加）+ HUD 图标行（按施加顺序）+
   吸收红心带（正常心带左侧）+ F3 `effects` 行（PRE-12 档位罗马 I/II/III
   + 时长）+ 牛奶清负面（吸收 = 正面保留）+ 死亡清空 + 存档自动
   序列化/校验。语义细节见 `docs/MECHANICS.md`「状态效果」。

**shipped 11 效果**：poison（damage 1/2.5 s、min_health 1 / min_health_mob 0 /
affects_mobs / undead_exempt、负面、tint 紫）/ weakness（melee_penalty 4、
负面、tier 3）/ regeneration（heal 1/4.5 s）/ absorption（absorb_hp 4、
tier 3 step 2）/ fire（damage 1/0.2 s、min_health 0、ignites_on_lava +
extinguished_by_water、**negative false = 牛奶不灭火**）/ speed（×1.2、
tier 3 step 0.2）/ strength（+2、tier 3 step 2）/ resistance（−0.2、
tier 3 step 0.2）/ healing（instant +4）/ harming（instant −6、负面）/
wither（damage 1/4 s、负面、affects_mobs）。

## 新增一个流体桶（P47 水 / P51 双种类 / FLUID-6 三种类）

**流体种类词汇表 = water / lava / honey**（FLUID-6：编码三层化
0 / 1..8 / 9..16 / **17..24**，水/熔岩带位级不变）。honey 特性：粘度 =
**每 3 个流体 tick 模拟一次**（`kHoneyTickPeriodFluidTicks = 3` 帧锁定
门控，水/熔岩零改动）、无波纹（PRE-3 水顶面专属）、无 worldgen（桶源 =
无蜂箱，记档偏离）、无 `water_reaction`（反应 pass 惰性）、item/mob 流体
交互同款浮力（ENT-4 记档偏离）。

桶 = 空桶 + 每种流体一个满桶，字段纪律（`src/item/item_registry.cpp`
parseFluidBucket，init 硬校验）：

1. **空桶侧**（舀 → 满桶）：`collects_fluid` = **流体名数组**（非空、
   不重复；一个空桶可舀所有命名流体，MC 语义）：

   ```json
   { "collects_fluid": ["water", "lava", "honey"] }
   ```

2. **满桶侧**（倒 → 空桶），每种流体一个：

   ```json
   { "pours_fluid": "honey", "pours_becomes": "bucket" }
   ```

   `pours_becomes` 须配 `pours_fluid`（配对纪律）。**满桶由空桶侧按流体
   推导**（`fullBucketFor(fluid, 空桶)`）——无 `collects_becomes` 字段
   （P51 废除）。

3. **init 硬校验**（两侧各自解析时本地一致 + 全部加载后交叉）：空桶命名
   的每种流体必须**恰有 1 个**满桶倒回该流体到本空桶（0 = dead field、
   2 = 舀的目标歧义，都拒绝点名）；满桶的 `pours_becomes` 伴侣必须存在
   且 `collects_fluid` 含本流体（对称，错配 = 启动失败点名双方）。一个
   物品**只能是一侧**（collects/pours 互斥），桶侧**不能同时是食物/可饮**
   （右键通道归属，解析拒绝）。
4. **图标 + model**：满桶物品要 `icon`（32×32，`tools/generate_item_icons.py`
   同款）；ENT-9 后加 model（桶家族 1 盒剪影）。
5. 机制自动生效：舀 = 桶目标射线（首个「非 air 方块**或**带流体」的格，
   **墙壁遮挡**）命中 **level 8 源**格 → 移除源 + 换该流体的满桶 +
   "splash" 音；倒 = 放置目标格（受击面邻格）air 无流体且不重叠玩家 →
   放 L8 源 + 换空桶。倒下的源走正常 FluidSim（下落/扩散/反应）。存档
   自动（物品按名 + 流体走 `fluids` 编辑 = format 9 起五元组带种类名）。
6. **舀的视线约束**（MC 行为，场景设计注意）：源格每 tick 向四向邻格补
   level 7——远处的玩家永远隔着自己源头的扩散膜（L7 ≠ 8，舀不到）；要舀
   源，玩家须站在源旁 1 格、从**上方**斜视源顶面（射线从源上方格直接进入
   源格）。E2E 场景见 PITS「流体桶 E2E 视线」。
7. **玩家面倒桶规则**（P47 放置 + FLUID-4 倒向水面 + FLUID-5 反应）：
   - 普通倒：block 射线受击面邻格 = air 无流体 → 放 L8 源（位级旧行为）。
   - FLUID-4：block 射线**无命中**（超 block 射程的开阔水面）→ 取 fluid
     射线自身命中的格；该格 = 桶自身种类的**流动格 level < 8** → 倒向水面
     = 变源（L8）（"对着池子倒"成立：block 射线穿流体 → 1 深池地面受击
     使顶面水格成候选）。
   - FLUID-5：**水桶对熔岩格 = 反应倒**——水源放在受击格 + 受击法向一步
     的干空气格（顶视 = 正上方 / 侧向 = 开阔侧 / 无 block 命中 = +Y 约定），
     无空气位 = no-op 桶保留（MC parity）；下 tick 既有 reaction pass 转
     obsidian（倒出水 = L8 源 → 项目规则恒 obsidian = 单一规则源）。
     **熔岩桶对水格 = 替换倒**（setFluid 既有"替换他种"语义换熔岩源；
     下 tick 按残留邻水转 obsidian/stone，无邻水 = 熔岩留存）。
   - 已源 level 8 / 异种流体格 = no-op 桶保留（MC parity）。

**shipped 对**：`bucket`（max_stack 16，`collects_fluid: [water, lava,
honey]`——一桶全舀三种类）⇄ `water_bucket` + `lava_bucket` + `honey_bucket`
（各 max_stack 1）；**玻璃瓶对**（SURV-4 药水链）：`glass_bottle`
（max_stack 16，`collects_fluid: [water]`——桶对舀水）⇄ `water_bottle`
（`pours_fluid: water`, `pours_becomes: glass_bottle`，**纯容器不可饮**）。

## 新增熔岩场景 / 反应（P51）

熔岩 = 流体种类，机制与水完全同构（同 level 1..8、同 FluidSim 规则、同
桶通道），差异全在数据：

1. **流体块**：`"fluid": "lava"`（词汇表内）+ `light_emission: 15`（格光
   来源 = 参考块的 emission）+ 可选 `water_reaction: {"source":
   "<邻水含 L8 源时的产物>", "flowing": "<邻水为流动水时的产物>"}`
   （严格配对、须流体块、init 校验两个名字存在）。**无 `water_reaction`
   = 惰性熔岩**（与水相邻不反应，异种流体格互相按"方块"处理 = 挡扩散不
   替换）。shipped lava = obsidian/stone。
2. **桶对**：见上（满桶 `pours_fluid: "lava"`）。
3. 放置/存档/E2E 场景：熔岩源 = 流体编辑 `[x, y, z, 8, "lava"]`；反应
   产物 = 普通方块编辑。
4. **世界生成湖**：固定湖（P88 列表）+ 程序化湖（P93，见 worldgen
   参考）；**表面湖（P90）= depth == fill**：熔岩顶与草 rim 同高 →
   燃烧 pass 点燃 rim（12 草 rim → fire → 焦土环）+ 沿下坡溢出盆地
   （确定性但非静止）。洞穴对每湖带排除盒（不挖湖）。场景坑：熔岩
   real-dt churn（PITS 244/283 类）、L8 源必 4 石封（PITS 291）。

## 新增一个可生长作物（P48 生长引擎 / P49 概率掉落 / P50 环境条件 / P53 放置门）

作物 = 一串 cross 形状方块，用 `grows_to` 链成生长序列；**生长状态就是
方块 id 本身**（`wheat_N` 的 N = 阶段），无 per-instance 状态、无存档
字段（与 sapling 相反）。核心生长引擎（`src/world/growth.{hpp,cpp}` +
`Game::m_crops`）纯数据驱动——加一种新作物 = 加方块 JSON + 纹理 +
物品/配方，不改核心。

1. **生长阶段方块**（每阶一个 `assets/data/blocks/<name>_<n>.json`）：

   ```json
   {
     "id": "wheat_1",
     "properties": {
       "hardness": 0.0, "solid": false, "opaque": false, "transparent": true,
       "cutout": true,
       "drops": "wheat_1",
       "grows_to": "wheat_2",
       "growth_ticks": 360,
       "grows_on": "farmland",
       "plant_on": ["farmland"],
       "growth_light": 9,
       "dry_slowdown": 2,
       "flamable": true
     },
     "model": { "shape": "cross", "textures": { "cross": "wheat_1" } }
   }
   ```

   - `grows_to` + `growth_ticks` 是**严格配对**（缺一 = 解析拒绝；
     `growth_ticks` ∈ [1, 1'000'000]；显式空串 = 笔误拒；自引用拒）；
     **init 硬校验** grows_to 与 grows_on 都必须指向存在的方块
     （typo = 启动失败点名）。成熟阶（链尾）**不写** grows_to（不 roll、
     不再长），`drops` 指向收获物（wheat_7 → wheat ×2）。
   - 三个环境字段（P50，均**可选**，缺省 = 无要求 = 旧行为）：
     `grows_on`（土壤名，下方一格须为该块）、`growth_light`（整数
     [0,15]，作物格 max(sky, block) 最低值；0 = 无要求）、`dry_slowdown`
     （整数 [1,64]，无水时 roll 模数倍数；1 = 无减速）。**湿润判定**：
     土壤格同 y、**切比雪夫半径 4**（= MC 耕地湿润半径）内任一流体
     level ≥ 1。模数加倍不改变 roll 值 → **干时通过帧是湿时通过帧的
     严格子集**（确定性可离线预计算）。
   - 放置字段（P53，**可选**）：`plant_on`（方块名**数组**——放置目标格
     正下方须为之一；缺省 = 不限；空数组 = no-op 拒（ammo 纪律）；逐条目
     `plant_on[i]` 点名；init 每条目存在性）。拒绝语义 = **静默 no-op**
     （不消耗物品、无音效）。`plant_on`（放置门）与 `grows_on`（生长门）
     **相互独立**。
   - 可燃（P58，**可选**）：`flamable`（严格 bool）——任意格被**熔岩
     6-邻**接触时燃烧 pass 烧成 **fire 块**（P64：有 fire 块 = fire、否则
     air；**无掉落**；水不烧）。**init 交叉校验**：registry 有任何
     flamable 块则必须有 fire 块。shipped = wheat 全 7 阶 + wood/leaves/
     grass + 3 sapling（P63 = MC 天然可燃三块）。
   - cross 形状要求 `solid=false, opaque=false, transparent=true`（注册
     强制，给错 = 拒）；shipped 作物均带 `cutout`（PRE-5）。
2. **生长引擎**（自动）：`Game::m_crops`（`map<BlockPos,BlockID>` 位置序）
   追踪所有带 grows_to 的格——`setBlock` 收口维护（放作物 = 入集，破坏/
   成熟 = 出集，炉子同款模式），**读档后 `rescanCrops()` 全量扫描已加载
   chunk 重建**（无存档字段，方块 id 即阶段）。每帧（固定 1/60 步长，
   昼夜类——无输入运行字节可复现）先过三道环境门，再 roll 确定性哈希
   `splitmix64(seed + frame×GOLDEN + hash3(x,y,z,salt)) % ticks == 0`
   → 进一阶（概率 1/ticks；ticks = growth_ticks × 干时减速）。位置哈希
   让两株不同步；世界 seed 让大世界间去相关。
3. **种子 = 隐式 block 物品**：`wheat_1` 方块自动有同名 block 物品（右键
   = 放置 = 种植），无需新物品。种子来源 = 某方块的 `chance_drops`
   （shipped `grass` → [wheat_1 ×1 @ 0.125] = MC 草掉泥土 + 偶尔种子）。
4. **收获物 + 食物**：`assets/data/items/<name>.json`（wheat 材料、bread
   带 `restore_hunger` + `saturation`）；配方（shapeless = 同一物品合并
   成一条 count——重复项解析拒绝）。
5. **概率掉落与"吃时给效果"（P49）**：`chance_drop` 首用户 = 当时 leaves
   苹果 1/128（现 = GEN-2 后的三条目列表）；`grants_effect` 首用户 =
   golden_apple（现为 `grants_effects` 列表：regeneration 10 s +
   absorption 5 s）。加"某方块偶尔掉 X" = 方块 JSON 加 chance_drop(s)
   条目 + 物品 JSON/图标，零核心。
6. **耕地（P50 内容）**：`farmland` 方块（solid cube、hardness 0.5 = MC、
   `drops` 回 dirt、atlas 空位）+ 配方（shapeless 1 dirt → 1 farmland）+
   **GEN-9 湿视觉**：`model.moisture_top: "farmland_wet"`（atlas 空位
   [9,4]；顶面按 wet/dry 逐帧派生，零 format）。
7. **纹理**：每阶一个 cross 纹理（`tools/generate_block_textures.py` 加
   函数 + TEXTURES 表项）+ `block_atlas.json` 空位加一行（不挪已有 UV）；
   物品图标（`tools/generate_item_icons.py`）。
8. **世界生成不放作物**（v1）：自然界的作物只来自玩家种植。

**已知简化**（KNOWN_ISSUES）：`dry_slowdown` 参照格永远是作物下方一格
（即使 grows_on 缺省）；水 = 任一流体层级（不区分源/流动）；**v1 无修复
系统**在耐久节。

当前唯一作物：`wheat_1`→…→`wheat_7`（growth_ticks 360，约 36 s 成熟），
收获 `wheat`（3 wheat → 1 bread，restore_hunger 5 / saturation 6.0）。

## 新增一个树种 / sapling（GEN-2）

树生长 = **可种植的 stateful sapling**（阶段 = ENT-2 的 per-instance
状态 byte，非方块 id——与作物相反）+ `tree` 对象数据驱动落树。加一个
树种 = 3 个方块 JSON（sapling + trunk + leaves）+ 纹理/atlas，不改核心
（生长引擎/形状表/收口/rescan 全数据驱动）。

1. **sapling 方块**（cross + 8 状态 + `tree` 对象）：

   ```json
   {
     "id": "oak_sapling",
     "properties": {
       "hardness": 0.0, "solid": false, "opaque": false, "transparent": true,
       "cutout": true,
       "drops": "oak_sapling",
       "flamable": true,
       "plant_on": ["dirt", "grass"],
       "growth_light": 9,
       "growth_ticks": 120,
       "tree": {
         "trunk": "wood",
         "leaves": "leaves",
         "min_height": 4,
         "height_range": 3,
         "leaf_radius": 2,
         "shape": "oak"
       }
     },
     "model": {
       "shape": "cross",
       "state_count": 8,
       "state_default": 0,
       "textures": { "cross": "oak_sapling" }
     }
   }
   ```

   - `tree` = **严格六键**（缺一/未知键/越域点名 `tree.<key>`）：trunk /
     leaves（非空块名，目录 load 后交叉校验存在——可指字母序后续文件）/
     min_height [1,16] / height_range [0,8]（roll ∈ [min, min+range]）/
     leaf_radius [1,2]（仅 oak 形状表读）/ shape ∈ {oak, spruce, birch}
     （形状表词汇表，其他值点名）。
   - `growth_ticks` 与 `tree` 严格配对（tree 无 ticks 拒；ticks 无
     grows_to/tree = 无意义拒）。**tree 存在 ⇒ state_count ≥ 2**（注册时
     点名：阶段 = 状态 byte，< 2 = 无阶段可推进的矛盾）。
   - stage 视觉 = emitCross 按 `stateCount > 0` 门控 quad 缩放
     (stage+1)/stateCount（绕格心，MC sapling 同款；无状态 cross 恒
     scale 1 零变化）。
2. **trunk 方块**：cube 实心 opaque（hardness 2.0 = MC 原木、drops 自己、
   flamable、sound_material wood 惯例）；**leaves 方块**：非 opaque 透明
   cutout（`light_opacity 2` = LGT-2 叶衰减、hardness 0.2、drops 自己、
   `chance_drops` 含**对应 sapling @ 0.05** + stick @ 0.02 = 种植闭环；
   apple 保留 oak 专属）。
3. **生长引擎**（自动，m_crops 四件套同款）：`Game::m_saplings`
   （`map<BlockPos,BlockID>` 位置序）经 setBlock 收口维护 + 读档后
   `rescanSaplings()` + 卸载保护。每帧（1/60 步长）纯函数
   `saplingGrowthSteps`（world/growth，专用盐）：土壤门（plant_on）→
   光门（growth_light）→ roll `% growth_ticks` → 阶段 < 7 = `Advance`
   （setBlockState +1，**零光照成本**——状态不改光 = ENT-2 约束）；阶段
   = 7 = 高度 roll（`treeHeightRoll` 纯函数，**帧无关**——被挡重试滚同一
   棵树）+ fits（planTree 全格 air 无流体 y 界内）→ `Complete` =
   `placeGrownTree`（trunk 先冠后逐格 setBlock + **1 次 updateRegion**
   over 树 bbox = 冠影 LGT-2 可见）。放不下 = 本 roll 不完成等待重试
   （MC 天花板规则）。
4. **形状表**（`world/tree_shape.{hpp,cpp}` 纯函数 `planTree`，root =
   sapling 格 = trunk 底）：oak = (T-2,r),(T-1,r),(T,1) **与 worldgen
   placeTrees 逐格同构**（单测钉死）/ spruce = (T-2,2),(T-1,1),(T,1),
   (T+1,1) 下宽上尖锥 / birch = (T-1,1),(T,1),(T+1,1) 细高小冠；(0,0)
   offset ≤ T-1 跳过（trunk 占位）、半径 2 层圆角。
5. **worldgen**：野外森林 = 全局单橡种（无 `tree` 字段的 biome 位级不变）；
   **V2-16 起 biome `tree` 字段实现按群系野生树种**（snowy_taiga =
   spruce，planTree 纯函数路径，本节生长机制零改动，见 biome 节）。
   sapling stateful ⇒ worldgen 放置源 init 不变量天然拒 stateful 块。**
   [GEN-5]** 表面格被洞吃 = 整棵树跳过（悬空树修复，守卫在 placeTrees，
   跨 chunk 纯函数重算 = 整树同生同灭无半截冠）。
6. **atlas 空位**（GEN-2 分配的 9 格，免重锚）：oak_sapling (10,1) /
   spruce_sapling (11,1) / birch_sapling (12,1) / spruce_trunk_side
   (13,1) / spruce_trunk_top (14,1) / birch_trunk_side (10,2) /
   birch_trunk_top (11,2) / spruce_leaves (12,2) / birch_leaves (13,2)。
   纹理 = generate_block_textures.py 新函数（专用 salt，既有零变化）；
   sapling 物品**无独立图标**（block item 以 3D cross 呈现，UI 惯例）。
7. **存档**：sapling 格 = blocks 编辑 + 阶段 = format 15 `states` 段
   （journal 派生直接覆盖：位置靠 block journal、值退出 live 读；阶段
   推进零 block 写）；落树 = 纯 blocks 编辑无 states。**零 format**。

当前 shipped：oak（wood/leaves 复用）/ spruce / birch 三树种，starter
库存各 3 颗（hotbar.json `inventory` 段，槽 24/25/26 = 隐藏 starter 惯例）。

## 新增一个块音效材质（CRAFT-3，无需改核心代码）

**数据面 = 块属性 `sound_material` + 3 件 wav 文件**（无新解析机制）：

1. 块 JSON 加 `"properties": { "sound_material": "<m>" }`（严格字符串，
   **显式空串拒** = ammo 纪律；缺省 = 通用事件名位级不变）。
2. `assets/sounds/` 补 3 件：`<m>_dig.wav` / `<m>_place.wav` /
   `<m>_break.wav`（16-bit mono 44.1 kHz；`tools/generate_sounds.py`
   确定性扩展——固定种子 LCG/sine；**重跑须保持既有 wav 字节零变化**，
   md5 实证 + 幂等）。
3. **init 交叉校验**（`checkSoundMaterials` 纯函数，**文件级**存在检查——
   headless 音频降级时照样严格）：引用材质的块 ⇒ 3 件全存在，缺件点名
   = registry 序首败块 + 事件序首缺件。
4. 机制自动生效：`blockSoundName(block, event)` 纯函数选事件名（有材质 =
   `<m>_<event>`、无 = 通用裸事件名 = 播放既有 dig/place/break.wav 位级
   不变）；6 个调用点位**世界空间化**（dig tick/破坏完成/放置/潜行旋转/
   丢弃@脚位/物品拾取@物品位）——`distanceGain` 线性衰减（cutoff =
   `kSoundCutoffBlocks` 16.0 命名常量零 config）+ `panGains` 恒定增益
   线性声像（高度不参与前后居中）；SoundBank = stereo（旧 mono 复制双
   声道居中听感位级等价）。手持破碎/箭拾取/营火放取/step/shoot/eat/
   hurt/ui_* 保持 2D 位级不变。**音频不入帧状态**（硬规则 8：截图/存档
   零影响）。

shipped 7 材质 × 3 事件 = 21 wav：stone（硬脆/raw noise/2200 Hz ping）/
wood（中频腔音）/ gravel（双 noise 层干颗粒）/ sand（double lowpass 低
shush）/ cloth（最静 rustle）/ glass（1900-3200 Hz 铃音）/
**snow**【V2-16，sand 族范式一阶更软，确定性合成】。51 块赋值：stone
9【stone/cobblestone/5 矿脉（coal·iron·gold·diamond·lapis_ore）/
stone_wall = 矿脉族 MC 语义/**enchanting_table**】/ wood 5【wood +
2 trunk/**planks**/**bookshelf**】/ gravel 1 / sand 3【sand +
sandstone 族】/ cloth 30【wool 全染变 15 + **bed 族 15**】/ glass 2 /
snow 1【snow】；未赋值块 = 通用回退（obsidian/furnace/铁·金·钻石块/
楼梯族等 = 下轮候选记档）。

## 调节玩家参数（config/config.json `player.*`）

玩家运动/生存模型的全部数值在 `Player::Params`，可在 `config/config.json`
覆盖：**缺键 = 结构体默认值**；**已存在的键必须是有限 JSON 数字**且过
范围检查（非数字/非正 = 启动报错点名，**不会静默回退默认**——player.cpp
严格读取纪律，Config getters 类型失配静默回落的已知缺陷在此绕过）；未知
`player.*` 键容忍（前向兼容：退役键 `fire_damage` /
`fire_damage_interval_seconds`（P74）与 `water_gravity`（FLUID-2，角色翻转
为 `water_buoyancy`）仍带着的旧 config 照常启动）。

| 键 | 默认 | 说明 / 约束 |
|---|---|---|
| `max_health` | 20 | 最大生命；心带 = ceil(max/2) 心；>0 |
| `fall_damage_offset` | 3 | 免坠落伤害格数（MC: 3）；≥0（允许 0 的三键之一） |
| `regen_delay_seconds` | 10 | 脱离受伤后多久开始自然回复；>0 |
| `regen_interval_seconds` | 4 | 每 +1 HP 间隔（回复门 bar ≥ `regen_hunger_threshold`；SURV-9：满血区间免费流逝，回复耗 1 点饥饿）；>0 |
| `max_hunger` | 20 | 最大饥饿（10 图标 × 2）；>0 |
| `hunger_decay_seconds` | 36 | 衰减 tick 间隔（MC 被动节奏 1 点 = 720 tick）；>0 |
| `starve_damage_interval_seconds` | 20 | 饥饿 = 0 时每间隔 1 HP 饥饿伤害（MC normal: 400 tick）；>0 |
| `regen_hunger_threshold` | 6 | 低于此饥饿无自然回复（MC 规则）；0 ≤ v ≤ max_hunger |
| `sprint_hunger_decay_factor` | 2 | 疾跑衰减加速因子（间隔 ÷ 因子）；>0 |
| `lava_damage_interval_seconds` | 1.0 | 熔岩格灼烧节奏（MC 4 HP/s，real-dt 生存计时器；离熔岩重置未到期 tick）；>0（P54） |
| `lava_damage` | 4 | 每次灼烧 HP（MC: 4）；0 < v ≤ max_health（P54） |
| `fire_duration_seconds` | 8 | 点燃的 `ignites_on_lava` 效果时长（MC Normal 8 s；熔岩/火块内每帧重施加 = 持续燃烧，离源后 countdown）；0 < v ≤ 100（P57） |

运动/几何全套键（同样可覆盖，均 >0）：`width`（0.6）、`height`（1.8）、
`eye_height`（1.62，须 < height）、`walk_speed`（4.3）、`sprint_speed`
（5.6）、`jump_speed`（8.5）、`gravity`（32）、`terminal_velocity`（38）、
`swim_speed`（3.1）、`swim_up_speed`（4.5）、`water_buoyancy`（12，
FLUID-2：上浮/下沉加速度）、`water_rise_speed`（1.5，被动上浮帽）、
`water_sink_speed`（1.5，潜行下潜帽）、`water_terminal_velocity`（4）、
`lava_swim_speed`（1.9）、`lava_swim_up_speed`（1.5）、`lava_gravity`（4）、
`lava_terminal_velocity`（2）（P54：熔岩游泳节奏——比水更慢/更沉，比值
对齐 MC 运动系数比 0.014/0.0231）、`mouse_sensitivity`（0.0025 rad/px）、
`crouch_height`（1.6，须 < height）、`crouch_speed_scale`（0.3）。

交叉字段检查（启动报错点名）：eye_height < height、crouch_height <
height、regen_hunger_threshold ≤ max_hunger、lava_damage ≤ max_health、
fire_duration_seconds ≤ 100。已提交 `config/config.json` 的 player 节 =
9 个生存节奏键（值均 = 默认；**用户调值，不还原**）。

## 其它游戏设置（config/config.json 全键参考）

**纪律**：`getBool/getString/getDouble/getInt` 读键——缺键 = 代码默认，
类型失配 = **静默回退默认**（player./save. 两例外 + render.weather /
render.night_sky_level / render.cave_ambient / render.ao 四个严格读键
= 现严格读取点名，见上、存档节与下表）。已提交 `config/config.json`
（**用户调值，不还原**）现含：
window{title/1280×720/fullscreen false}、**ui.scale 1.5**、render{vsync
true, fps_cap 0}、world{**seed 123456789, view_distance 80,
day_length_seconds 600**, start_time_of_day 0.25}、player{9 键}、
audio{volume 0.7}——**render 各特性键 / mobs / fire / beacon / save 均
不写也开 = 代码默认**（fire.particles 先例，config.json 零改动纪律）。

| 键 | 类型 | 代码默认 | 说明（引入 phase） |
|---|---|---|---|
| `window.title` / `window.width` / `window.height` / `window.fullscreen` / `window.resizable` | str/int/int/bool/bool | "TokenMine.cpp"/1280/720/false/true | 窗口（种子 config 带出 title/width/height/fullscreen） |
| `render.vsync` / `render.fps_cap` | bool/int | true/0 | 0 = 不封顶 |
| `render.fog` | bool | **true** | PRE-2 边缘距离雾（流送盘缘溶解，同帧天空色跟随昼夜；off = 对前版位级恒等） |
| `render.water_ripples` | bool | **true** | PRE-3 水面 shimmer（帧锁定 120 帧周期；lava 不闪） |
| `render.clouds` | bool | **true** | PRE-4 云（y=108 平板层、100 s/格漂移、alpha 0.55；相机云格/t 变化才重建 mesh） |
| `render.alpha_test` | bool | **true** | PRE-5 透明 discard（cutout 段写深度 + alpha<0.5 丢弃；off = 混合段序偏离记档） |
| `render.drop_light` | bool | **true** | PRE-6 掉落光照（掉落物随世界光，每帧采样中心格） |
| `render.mob_flames` | bool | **true** | PRE-8 燃烧 mob 余烬（每 36 帧一批，自发光） |
| `render.furnace_flames` | bool | **true** | CRAFT-5 炉口火焰粒子（燃烧中每 36 帧自炉上格发射） |
| `render.weather` | bool | **true** | V2-4 雨/雷天气（严格读，非 bool 点名拒；off = 状态机永不运行 + 零雨滴 + 零交互 = v2.3.0 管线位级恒等；推进式流首段恒 CLEAR ≥ 9000 帧 = 150 s → 60 帧无输入基线结构免疫根；流种子 = world seed，雷独立 salt） |
| `render.night_sky_level` | int | **7**（kNightSkyLevel） | UX-1 深夜天空地板（严格读：absent = 7 位级 v1.34.0；present = 整数 [0, 15]——0 = 全黑夜（ambient 地板随之归零），15 = 无 retreat；非整/越界/非数点名拒） |
| `render.cave_ambient` | number | **0.03**（kAmbientFloor） | UX-9c 全遮挡 ambient 地板（严格读：absent = 0.03 = pre-UX-9c 管线逐位；present = 有限数 [0, 1]——0 = 洞穴/深水/密封室无残光，1 = 全亮；shader 单消费者） |
| `render.ao` | bool | **true** | V2-18 AO 环境光遮蔽（严格读 weatherEnabled 范式：非 bool 点名拒；**off = 查询不跑 + greedy 合并键 ao[4] 恒零 + 顶点系数恒 1.0 = pre-V2-19 位级恒等**【门内 off ≡ 旧锚实证】；on = 经典 MC 顶点 AO 4 档 1.0/0.75/0.5/0.3（side1∧side2 双邻特例 = 0.3）× opaque ∧ 高度带闭区间，bake 入既有 sky/block 光 attribute（零 shader / 零顶点格式 15 float / 零新 uniform）；生效面 = greedy 满块（MaskCell 合并键扩 ao[4]，共享边角 AO 异断 run）+ slab 三态 + stairs；**fluid/cross/wall/fence/glass_pane 免 AO**（MC 精确：水面不受 smooth lighting 影响 + 薄几何范围外）；per-draw 光覆盖面（信标光束/掉落/XP 球）AO 消失 = 特效光覆盖优先记档偏离；门 190 重锚 base18 ← 224468 B） |
| `audio.volume` | number | 0.7 | 种子 config 带出 |
| `audio.ambience` | bool | **true** | PRE-4 昼夜环境音（cave/day/night 三上下文，每 300 帧纯函数 roll，2D 全局） |
| `world.seed` | int64 | getter 回落 0（种子 config 生成值 = 123456789） | 已提交 = 123456789；`--seed` 覆盖并写回 |
| `world.view_distance` | int | 8 | **已提交 = 80（用户调值）**；流送半径 = min(vd, 64)（STRM-1 远平面帽 R_eff） |
| `world.day_length_seconds` | number | 240 | **已提交 = 600（用户调值）** |
| `world.start_time_of_day` | number | 0.25 | `--time` 覆盖（仅新世界） |
| `mobs.enabled` | bool | true | 生成总开关 |
| `mobs.limb_swing` | bool | true | ENT-1 四肢摆动（off = v0.9.0 静态对照） |
| `mobs.difficulty` | string | "normal" | Phase 25/ENT-8：**"easy" / "normal" / "hard" / "peaceful"**（大小写敏感，其他值点名拒）——敌对保护环 8/12/16/**全球(peaceful)**、伤害 ×0.5/1/4÷3/0、生成速率 ÷0.5/1/1.5（check_every_frames 30 → 60/30/20）；peaceful = 敌对自然生成全拒 + 被动豁免（无"现有敌对消亡"= mob 瞬态，记档偏离）；只 init 读取（无运行时切换） |
| `fire.lifetime_seconds` | number | 4.0 | P64 火块寿命（real-dt 倒计时；>0 点名） |
| `fire.spread_interval_frames` | int | 6 | P66 蔓延 roll 节奏（0.1 s MC tick；≥1 点名） |
| `fire.spread_chance` | number | 0.05 | P66 每 5-邻（水平 4 + 上，不向下）独立 roll 概率（[0,1] 点名） |
| `fire.particles` | bool | true | P66 火焰余烬粒子（E2E 对照组） |
| `beacon.range_blocks` | int | 50 | P83 信标效果范围（玩家脚到信标格 Chebyshev；>0 点名） |
| `beacon.beam` | bool | true | P83 半透明光束（E2E 对照） |
| `save.auto_save_seconds` | number | **120.0**（代码默认，SAVE-1） | 自动存档节奏（**真实 dt**，`--save` 槽门控）；**严格读取**：缺 = 默认、present 须有限数 ≥ 0（**0 = 关闭**、负数/非有限/非数字点名拒）；成功归零、失败不清零下帧重试；日志 "World auto-saved to" 区别于 F5/退出 "World saved to" |
| `ui.scale` | number | 1.0 | P34 屏幕空间 UI 缩放，**[1.0, 4.0]** 越界/非数值点名；**已提交 = 1.5（用户调值）**。全部屏幕空间 UI（字体/hotbar/心饥饿带/面板/F3/死亡屏/手持/准星）在 "UI 像素" 空间（窗口逻辑 ÷ scale）正交投影拉伸——世界渲染与截图尺寸不受影响 |

## 使用存档（Phase 13 框架 → format 29）

```bash
./build/bin/tokenmine --save myworld        # 首次：新游戏；此后：读档
./build/bin/tokenmine --save myworld --new  # 推倒重来（退出时覆盖 myworld 槽）
./build/bin/tokenmine --list-saves          # SAVE-3：槽位列表（摘要纯读，确定性名序）
./build/bin/tokenmine --copy-save S D       # SAVE-3：字节保真分支（D 已存在拒；原子写）
./build/bin/tokenmine --rename-save S D     # SAVE-3：POSIX 原子改名
./build/bin/tokenmine --delete-save S       # SAVE-3：删除
```

管理旗标**独占**（每跑至多一个 + 与游戏旗标互斥 = rc 2 点名；管理跑不
启动游戏）。

- 存档槽名只允许字母/数字/`_`/`-`（防路径穿越）；文件在 `data/saves/
  <slot>.json`（git 忽略）。**V3-7 起 on-disk = 确定性 gzip(JSON) 容器
  （默认开）**：JSON 格式化输出、`gunzip` 后可直接阅读；加载按魔数
  `1f 8b` 自动探测编码——**旧明文档（format 28 及更早）零迁移直读**；
  `config` 键 `save.compression: "gzip"|"none"` 严格读（absent = gzip
  代码默认；"none" = 明文 = 旧行为位级恒【同状态文件与 v3.0.0 差异
  = 恰 root.format 单键】；非 string / 未知词 init 点名拒）。gzip 容器
  每字节可复现（固定 10B 头 MTIME=0/无文件名 + raw deflate level 6 +
  CRC32/ISIZE 尾）——同世界存档逐字节可复现（确定性一级纪律）；解
  码侧同 10B 头逐字节全钉（系统 gzip CLI 产物点名拒收）
- **读档语义**：`--time` 只对全新世界生效（存档时间优先）；`--seed` 与
  存档 seed 不一致 = 启动报错（编辑会落到错误地形上）；存档里的玩家
  位置/朝向/速度覆盖出生点与初始视角
- **何时写**：干净退出（ESC 二次/关窗/`--frames` 到点）自动写回；游戏内
  **F5 立即存档**（`--save` 槽激活时）；**SAVE-1 自动存档**（每
  `save.auto_save_seconds` 真实秒，槽门控）；初始化失败或崩溃不写（保护
  旧档）。写失败只记 ERROR 日志、退出码仍 0。**三路共用原子写**（同目录
  `.tmp` + flush/close + `rename` POSIX 原子——kill 中途最多留陈旧 tmp，
  失败写永不毁旧档）
- 存档是机器生成的：字段集由 format 号锁定（**当前 format 29**），加载器
  对未知字段一律报错（与用户手写数据文件的"警告但容忍"策略相反）。
  **25→29 谱系**（一句话）：25 = 教程（V2-3）/ 26 = XP 三向键（player.xp
  + entities 第二 kind xp）/ 27 = 附魔（player.xp_seed + 物品 enchants）/
  28 = 床（player.respawn 恒写）/ **29 = gzip 时代（V3-7：schema 与 28
  完全一致，format 号仅标记写入构建开始默认 gzip 编码；编码与 format
  号正交，魔数探测唯一判据）**；完整 layout 与逐版证据见
  `docs/archive/v2x/CHANGELOG.md` 各版条目 + `phase-2.x.0.md` 档（29 →
  `docs/archive/phase-3.1.0.md`），不在此重抄全文
- **PITS**：`--save slot` 第二次运行会加载第一次写的存档（E2E 对跑每拍
  从源副本 `cp` 新鲜存档）

### format 29 完整布局（on-disk = gzip 容器，JSON 内容 = 明文 payload）

```json
{
  "format": 29,
  "seed": 123456789,
  "time": 0.2541666,
  "player": {
    "pos": [22.5, 53.0, 0.5],
    "velocity": [0, 0, 0],
    "yaw": 0.0, "pitch": -0.25,
    "health": 20.0, "absorption": 0.0,
    "hunger": 20.0, "saturation": 6.0,
    "effects": [ { "name": "speed", "remaining": 12.5, "tier": 1 } ],
    "stats": { "deathCount": 1, "kill.chicken": 3 },
    "armor": { "head": "iron_helmet", "chest": { "item": "leather_chestplate", "damage": 12.5 } },
    "oxygen": 150,
    "xp": 1395,
    "xp_seed": "5eed1a2b3c4d5e6f",
    "respawn": [0, 54, -3]
  },
  "inventory": { "selected": 0,
                 "slots": [ { "item": "stone", "count": 64 },
                            { "item": "iron_axe", "count": 1, "damage": 2.0,
                              "enchants": { "efficiency": 1 } },
                            { "item": "", "count": 0 } ] },
  "inv_grid": [ { "item": "", "count": 0 } ],
  "furnaces": [ { "pos": [0, 53, 0], "input": { "item": "", "count": 0 },
                 "fuel": { "item": "", "count": 0 }, "output": { "item": "", "count": 0 },
                 "progress_substeps": 0, "fuel_substeps": 0 } ],
  "fires": [ { "pos": [0, 53, 0], "remaining": 3.2 } ],
  "campfires": [ { "pos": [0, 53, 0], "slots": [ { "item": "", "count": 0, "progress_substeps": 0 } ] } ],
  "states": [ { "pos": [0, 54, 0], "state": 2 } ],
  "tables": [ { "pos": [0, 53, 0], "slots": [ { "item": "", "count": 0 } ] } ],
  "entities": [ { "type": "item", "item": "dirt", "count": 1, "damage": 2.0,
                  "pos": [0.5, 52.5, 0.5], "age": 12.5, "pickup_delay": 0.25 },
                { "type": "xp", "xp": 5, "pos": [1.5, 52.5, 0.5],
                  "age": 12.5, "pickup_delay": 0.25 } ],
  "blocks": [ [0, 53, 0, "air"] ],
  "fluids": [ [4, 50, 4, 5, "water"] ]
}
```

（上方 campfires/inv_grid/tables 数组为示意——实际各恰 4 / 9 / 9 个槽
条目。顶层各段**恒写**（空 = 空数组/空对象，含 `tables`/`states`/`fires`/
`campfires`/`furnaces`/`inv_grid`——新格式必键纪律）；player 的
health/hunger/saturation/absorption/oxygen/xp/xp_seed/respawn = 游戏恒有
值故实际恒写（−1"未指定" = 省略字段，仅老 format 读入语义）。）

**player 段**（键纪律：format 29 必键 = pos/**velocity**/yaw/pitch/health/
absorption/hunger/saturation/effects/armor/**stats**/**oxygen**/**xp**
/**xp_seed**/**respawn**；低版本带新键 = 版本谎报点名）：

| 键 | 引入 | 纪律 |
|---|---|---|
| `pos` / `yaw` / `pitch` | 1 | AABB 底面中心；[0, 128) 世界 y |
| `velocity` | **22（SAVE-2）** | 3 数组**恒写**（静止 = [0,0,0]——ENT-7 停止 epsilon 使静止恰 0）；分量区间 ±100（`kSavedVelocityMax`，越界点名）；≤21 读入 = 零速 legacy（值保真免迁）；onGround 不存（位置派生，记档偏离） |
| `health` | 2+ | base 池（≤ base max；absorption 分池后不含层）；format 1 = 缺省满 |
| `absorption` | 12 | [0, 64]，**恒写**（无层 = 0）；须 ≤ 存档效果 absorb_hp 总和（不变量点名）；≤11 读入 = 共享 health 迁移（超 base 部分入池） |
| `hunger` | 4+ | format 1-3 读入 = 满 |
| `saturation` | 5+ | [0, max_hunger]；format 1-4 读入 = 0 |
| `effects` | 8+ | **必键**（空数组合法）；名序；`{name, remaining, tier?}`——remaining > 0 秒（real-dt 倒计时；**tick 相位不存** = 读档重启满 interval）；**tier 0 基，仅 >0 时写**（omit-when-default，20+；范围 = 效果 tier_max，点名） |
| `stats` | 21 | **必键**（空对象合法）；词表**封闭**：`deathCount` + 每注册 mob 一条 `kill.<name>`（非零才存，名序）；未知 `kill.<name>` = game 加载点 validateStats 点名（parseSave 不加 MobRegistry 参 = 184 调用点涟漪，rescan 范式孪生） |
| `armor` | 16 | **必键**（空对象 = 无甲）；**只含占位槽**、固定槽序 head/chest/legs/feet；值 = 物品名字符串（满耐久保字符串形 = fmt17 逐字节同形）**或** `{item, damage}` 对象（damage > 0，18+；可携 enchants 27+ 同款）；解析期对 ItemRegistry 校验（物品存在 + 其 armor_slot 匹配键名） |
| `oxygen` | 17 | **必键**，整数 [0, 150]（kOxygenMax）；drownTicks 不存（读档重启 0——MC 同款记档偏离） |
| `xp` | 26（V2-11） | **必键**，整数 ≥ 0 **恒写**（累计经验池；HUD 条/等级 = 池派生，**等级永不存储** = 单源真值，core/xp_curve 全整数 MC 现代公式）；pre-26 载 = 迁 0（velocity 零化迁移同款）；≤25 文件携 = 版本谎报点名 |
| `xp_seed` | 27（V2-12） | **必键**，**16 位小写 hex 串恒写**（附魔 offer 去相关种子；存档 JSON number = double 53 位容不下 64-bit = hex 串记档偏离）；26 载 = worldSeed ^ kXpSeedInit 迁移；pre-27 文件携 = 版本谎报点名 |
| `respawn` | 28（V2-13） | **必键**，int 三元组 **恒写**（重生点 = 记忆位置的格分量 floor；新档 = 世界出生点 init 写入）；分量非整数 / y ∉ [0,128) 点名拒（x/z 无界）；27 载 = 哨兵 -1 → `applySave` 覆写世界出生点（parse 无法知世界出生点 = game 侧迁移）；pre-28 文件携 = 版本谎报点名 |

**世界段**：`inventory`（27 格 `slots`：{item, count, **damage? >0**,
**enchants? 27+（omit-when-empty，{id: level} 对象，载时交叉校验：未知
id / level > max / 类不兼容 / incompatible 对全点名**）} 严格白名单——
grid 槽不携 damage = 18 范围记档偏离；`selected` 0..8）+
`inv_grid`（E 面板 2×2 的 9 格，恒 9 条目）+ `furnaces`（7+，**每炉子块
一条目**位置序，空态含；双向交叉校验 = 块编辑 ⇔ 条目）+ `fires`（10+，
每火块一条目，remaining > 0 = **real-dt** 燃尽倒计时）+ `campfires`（13+，
每营火**家族**块（点燃 + 未点燃）一条目，**恰 4 槽**，每槽进度不变量
progress > 0 ⇔ 槽恰 1 可熔炼物品）+ `states`（15+，每个 stateful 块恰一条
{pos, state}，state 对块 stateCount() 范围校验 + 双向交叉）+ `tables`
（19+，每工作台块一条目 {pos, slots[9]}，槽 format-3 纪律【槽带 damage
拒 = SURV-3 grid 范围】）+ `entities`（`type` ∈ {item, xp} 两种：item
条目 item/count [1,64]/pos/age/pickup_delay/**damage? >0**/**enchants?
27+（omit-when-empty）**；**xp 条目（26+）= 严格形 {type, xp 整数 1..9,
pos, age, pickup_delay}**（球不携堆叠，无 item/count/damage；item 条目
携 xp 字段 = 版本谎报点名）；**mob 与箭 = 瞬态实体不存**（读档后不
出现，不是错误）+ `blocks`（位置序 [x,y,z,块名]，"air" = 移除）
+ `fluids`（位置序 [x,y,z,level,种类名]，种类 = water/lava/honey；level 0 =
抽干）。

**键纪律三向**（与用户数据文件相反）：① 新版本**必键**（effects/fires/
campfires/armor/oxygen/stats/tables/states/velocity/**xp/xp_seed/respawn**
等，缺 = 拒）；② 低版本**带新键 = 版本谎报点名**（如 fmt14 带 states、
pre-21 带 stats、pre-22 带 velocity、pre-26 带 player.xp 或 xp 实体、
pre-27 带 xp_seed 或堆栈 enchants、pre-28 带 respawn）；③ 未知字段一律
报错。**迁移先例**：值保真（14 stairs
→ 15 states facing→state）/ 免迁（tier 0 omit-when-default ⇒ f19 文件 f20
直接读）/ sole 归属（f18 table_grid 非空 ∧ 恰 1 表 → 归属，0/多 → 丢弃，
P44 furnace 先例）/ 系统不存在 = 默认（f11 无吸收 → 共享 health 拆分、
f16 无甲 → 空、f17 无氧 → 满、f20 无 stats → 空、pre-22 无 velocity → 0、
pre-26 无 xp → 0 且无 xp 实体、pre-27 无 xp_seed → worldSeed ^
kXpSeedInit、pre-28 无 respawn → 哨兵覆写世界出生点）。

## 纹理 / 图集 / 字体 / 声音资产纪律（重锚红线）

**块图集**（单纹理多 tile）：tile 源 = `assets/textures/blocks/*.png`
（块，96 件）+ `assets/textures/mobs/*.png`（mob/玩家，26 件），均
16×16 RGBA；
布局 = `assets/data/textures/block_atlas.json` `{columns, rows, tiles:
{name: [col, row]}}`——**显式格子，非计算排布**。解析纪律（纯函数，
init 双向交叉）：columns/rows 必须**2 的幂**（UV 角 = 精确二进分数，浮点
管线永不跨界采样）；tile 格须在网格内；**两 tile 不得共格**；PNG 无布局
条目 / 条目无 PNG = 数据错误点名。

- **shipped = 16 × 8 = 128 格，已用 122，空 6**（余空 = (13,6)-(15,7)
  区段；honey = (15,1)、farmland_wet = (9,4)、sheep_skin = (11,6)、
  snow = (12,6) 先例）。空位仅剩 6——新 tile 须精确规划，耗尽 = 网格
  扩容（重锚）。
- **空位添加 = 无既有 UV 移位 = base18 免重锚**（绝大多数内容相走的路）。
- **网格扩容 = u = col/列数 全部既有 UV 移位 = 无输入基线门必须重锚**
  （P41 4×8→8×8、P59 8×8→16×8 先例）。

**物品图标**：每物品独立 PNG（`assets/textures/items/`，32×32 RGBA，**无
atlas**；可多物品共享同一文件——block item 不用 2D 图标，以 3D 模型/
立方呈现）。物品 `model.texture` 惯例 = 复用 icon 文件（CRAFT-2 零新纹理
免重锚）。

**effects.png**：16 px/tile 条带（当前 176×16 = 11 tile）；新效果 =
**尾部 append**（既有 tile 逐像素零变化验证——SURV-4/SURV-5 先例）。

**font.png**（256×2112，32 px 格 = 8 列 × 66 行）：rows 4..15 = ASCII
32..126；rows 0..3 = **32 个 ASCII 控制字符死格复用** = PRE-1 的 32 个
CJK 字形（18 UI 字 + 10 中文标点 + 4 余量）；cell 127（ASCII DEL 死格）
= CRAFT-4 补的「作」（U+4F5C——PRE-1"18 UI 字"实为 19 字误计，工作台
标题 v0.25.0 起渲染"工?台"）= legacy **CJK 33 字形**。**UX-6 扩展 +
V2-12 附魔 4 字（锋利限魔，魔 re-cell 鸡麦黄默 +1）= 现
`kCjkGlyphs[427]`**（码点序）：legacy 33 保 PRE-1/CRAFT-4 格 0..31/127
位级不变；394 扩展字形占 cells 128..521 = rows 16..65（row 65 = 部分
余量，cells 522..527）；未知 CJK 落 `?` 格。**加 CJK 字形 = 重新生成
脚本**：glyph 静态表（font.hpp / generate_font.py 双份）加条目 +
font.png 重跑——**顶 512 px（rows 0..15 = ASCII + legacy CJK 33 格）
逐字节不变红线**（测试钉），新字形追加尾部 cell；rows 16..65 余量耗尽
= 字体布局再扩展（按重锚纪律处理）。ASCII 区位级不变红线 + 既有 CJK
格逐位复现自校验 + FNV-1a 64 字节钉在测试里；TTF 仅工具期输入，游戏
构建不依赖。

**声音**：`assets/sounds/` 46 件 16-bit mono 44.1 kHz WAV（SoundBank 载入
时转 stereo 居中，CRAFT-3；2.x 增 thunder【V2-4】/ sleep【V2-13】/
squeak【V2-14】/ shear【V2-15】/ snow×3【V2-16】）。
`tools/generate_sounds.py` 确定性生成（固定
种子 LCG/sine；**重跑后既有文件字节零变化 = md5 实证 + 幂等**）；材质 3
件见「块音效材质」节。

**确定性生成工具总则**（全部 `tools/*.py`，无运行时依赖，纯标准库写 PNG）：
generate_block_textures.py（块 16×16，per-pixel hash(x, y, salt)）/
generate_mob_textures.py（mob 16×16，splitmix64）/ generate_item_icons.py
（物品 32×32，splitmix64，DYE_PALETTE 等）/ generate_effect_icons.py
（条带）/ generate_font.py（字体表）/ generate_sounds.py（wav）。加纹理 =
**新 salt**（既有纹理零变化）+ 重跑须 md5 验证幂等。

---

*（完）——各 phase 细节存档：v1.x 见 `docs/archive/v1x/phase-*.md`、
v2.x 见 `docs/archive/v2x/phase-2.x.0.md`；活跃已知问题与记档偏离见
`docs/KNOWN_ISSUES.md`；玩法语义见 `docs/GAMEPLAY.md` /
`docs/MECHANICS.md`。*
