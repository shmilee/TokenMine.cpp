# 内容添加指南（数据驱动）与存档使用

> Phase 30 文档整理：从 `README.md` 移入（原「新增一个生物群系/矿石」「调整初始
> hotbar」「新增方块/配方/Mob」「使用存档」小节组，未改写），并并入原
> `SESSION_CONTEXT.md`「Phase 4 关键参数」（worldgen.json 参考，文末）。文中
> `KNOWN_ISSUES` 均指 `docs/KNOWN_ISSUES.md`。

## 新增一个生物群系 / 矿石（无需改核心代码）

1. 生物群系：`assets/data/biomes/<name>.json` —— 权重/表层/次表层/深度/树密度/高度系数，
   可选 `decorations`（Phase 45 表面装饰，见下）：

   ```json
   {
     "id": "swamp",
     "weight": 3,
     "surface": "grass",
     "subsurface": "dirt",
     "subsurface_depth": 3,
     "tree_density": 0.08,
     "height_scale": 0.8,
     "decorations": [
       { "block": "tulip", "probability": 0.02 }
     ]
   }
   ```

   **表面装饰**（Phase 45）：`decorations` = 数组，每条 `{block, probability}`
   （严格校验：未知键拒绝、block 非空串、probability ∈ [0,1]；block 名在
   世界生成 setup 对 BlockRegistry 解析，**未知块名 = 启动报错**）。放置
   规则：树的同一格网（`trees.lattice`）、每 (格, 条目) 独立 roll、落表面
   格正上方一格、只在该 biome 的 surface 块上、高于海平面、树占格让位。
   装饰块建议用可穿过的十字植物（花）或低硬度实心块（仙人掌）——实心高
   硬度块会挡 mob 生成/路径。
2. 矿石：`assets/data/worldgen.json` 的 `ores.veins` 数组加一项（方块需已注册）：

   ```json
   { "block": "diamond_ore", "probability": 0.0101, "vein_size": 1, "max_y": 20 }
   ```

   - **`vein_size` = 椭球簇数 K ∈ [1, 8]**（`[GEN-3]`，`kMaxOreVeinClusters`）：
     一条脉 = K 个 **MC 3×2×1 旋转椭球**（各 21 格，半轴 (3,2,1) 随机取向；
     首球心 = 格网胞内均匀起点，后续球心在前球 ±2 内游走）。旧的"随机游走
     步数"语义已废。**加矿/调密度时注意**：K=1 单球已是旧 ~2-3 球游走的
     体量，密度主要由 `probability` 调（见下标定）。
   - **`probability` = 格网命中概率**（每个 8³ 世界格网胞独立 roll，
     `hash3 > probability` 才落脉）。**标定方法**（改 `probability`/`vein_size`
     后用）：固定 seed（123456789）+ 固定采样区（列 4..11 × 0..7，64 列
     全高，出生 clear 区外）跑 per-ore 格数探针（`WorldGenerator::create`
     直连，见 `tests/test_world_generator.cpp` 的 `Gen3ShippedOreVeinCellCountsPinned`
     钉死值 = 当前 shipped 标定终值），目标新/旧格数比 ∈ [0.8, 1.25]；
     `probability_new = probability_old / 比值`（3 位整洁值），单轮定稿，
     残留越带记档（当前残留：diamond 0.641×、lapis 0.732×，见
     `docs/KNOWN_ISSUES`）。
   - **`max_y`** 分层不变（脉只出现在 y ≤ max_y）。

   当前 shipped 6 条（`[GEN-3]` 单轮标定终值，全 K=1）：coal 0.154 / max_y
   80，iron 0.0855 / 56，gold 0.0376 / 36，diamond 0.0101 / 20，lapis
   0.0208 / 32，**gravel** 0.0445 / 80（gravel = 唯一非矿石"矿脉"，石替换脉）。
   矿脉只替换 deep stone（不破坏洞穴 = 洞壁矿暴露），跨 chunk 无缝（世界
   坐标格网 + 世界坐标起点）。

地形/洞穴/树木参数同样在 `assets/data/worldgen.json`（缺省=默认值，类型错误=启动报错）。
**注意：改 worldgen.json / biome JSON 会改变生成的世界 → 无输入基线门
（`/tmp/opencode/base18.png`）需要按 E2E 协议重锚或论证结构性免疫**
（P45 先例：lapis 矿脉 + 表面装饰进了数据，但出生点视野内无变化 →
第 19 次结构性免疫，免重锚）。

## 调整初始 hotbar / 新增工具与材料

`assets/data/hotbar.json` 的 `items` 数组（游戏内 `Tab` 循环切换 9 格，`Q` 从选中格丢弃）：

```json
{ "items": ["stone", "dirt", "grass", "sand", "wood", "leaves", "glass", "glowstone", "wooden_pickaxe"] }
```

物品名必须已注册（未知名=启动报错）；列表不能为空、至多 9 项；每格初始为满堆栈
（block 64 / 工具 1 / 材料 64）。

**`inventory` 段（GEN-2，可选）**——初始**存储槽**填充（隐藏 starter：
槽 [9, 26]，**非 hotbar = 无输入不渲染**）：

```json
{
  "items": ["stone", "..."],
  "inventory": [
    { "slot": 24, "item": "oak_sapling", "count": 3 },
    { "slot": 25, "item": "spruce_sapling", "count": 3 },
    { "slot": 26, "item": "birch_sapling", "count": 3 }
  ]
}
```

条目**严格三元组 {slot, item, count}**（缺键/未知键点名拒）：slot ∈
[9, 26]（hotbar 槽 = `items` 段的领地，越界点名）、item 已注册点名、
count ∈ [1, maxStack] 点名、重复 slot 拒。缺省段 = 无 starter；
**读档整体替换 inventory**（starter 只在新世界生效）。shipped =
3 树种 sapling 各 3 颗（槽 24/25/26）。

新增工具或材料 = `assets/data/items/<name>.json`：

```json
{
  "id": "wooden_pickaxe",
  "type": "tool",
  "tool": { "break_speed": 2.0 },   // 仅工具；>0，默认 1.0（破坏耗时 = 1.5s × hardness ÷ speed）
  "attack_damage": 2,               // 可选（Phase 20）：挥击伤害，(0, 100]，默认 1.0
  "shoot_speed": 15.0,              // 可选（Phase 21）：发射器速度，(0, 100]；>0 = 投射物发射器（弓）
  "shoot_damage": 5.0,              // 可选（Phase 21）：箭矢命中伤害，[0, 100]，默认 0
  "restore_hunger": 4,              // 可选（Phase 23）：食物恢复饥饿，整数 [1,20]；缺省=非食物
  "ammo": "arrow",                  // 可选（Phase 24）：发射器弹药物品名（须存在，init 交叉校验）；发射消耗 1 个
  "infinite": true,                 // 可选（Phase 77）：严格 bool，缺省 false；要求 'ammo' 非空——发射器的
                                    //   Infinity（MC 弓附魔的数据驱动版）：发射**不消耗**弹药（仍须持有
                                    //   至少 1 个 ammo 物品才能射；1 支箭永不被耗尽）；非 bool / 无 ammo
                                    //   点名拒绝（ammo 纪律）
  "smeltable_to": "cooked_x",       // 可选（Phase 40）：炉子产物物品名（须存在，init 交叉校验）；存在=可熔炼
  "smelt_time": 10.0,               // 可选（Phase 40）：单格熔炼秒数，(0,100]，默认 10（MC 炉子时长）；仅可熔炼时有意义
  "fuel_time": 80.0,                // 可选（Phase 40）：单格燃料燃烧秒数，(0,1000]；缺省=非燃料（显式 0 拒绝）
  "drink_time": 1.6,                // 可选（Phase 43）：饮用通道秒数，(0,10]；present=可饮用容器（与 restore_hunger 互斥）
  "returns_item": "bucket",         // 可选（Phase 43）：喝完返还的容器物品名（须存在，init 交叉校验）；要求 drink_time 存在
  "grants_effect": {                // 可选（Phase 49；SURV-4 放开到 drinkable）：完整吃下/喝下时施加的状态效果——
    "name": "regeneration",         //   食物（restore_hunger > 0）或可饮用（drink_time > 0）可给；两者皆非 = 解析拒绝。
    "duration": 10.0                //   name+duration 严格配对（on_hit_effect 纪律），name = 已注册效果名
  },                                //   （init 交叉校验）；duration > 0；drinkable 的 duration == 0 = 瞬时效果
                                    //   （效果须带 instant_hp，init 双向交叉校验，SURV-4 药水范式）
  "grants_effects": [               // 可选（Phase 56；SURV-4 同款放开）：grants_effect 的列表形式——同一严格配对的数组，
    { "name": "regeneration",       //   完整吃下/喝下时**按数据顺序**依次施加（金苹果 = 再生 + 吸收）。
      "duration": 10.0 },           //   与单字段 grants_effect **互斥**（同写 = 解析拒绝，P52 chance_drops 模式）；
    { "name": "absorption",         //   空数组/非对象元素/缺三元组/未知键逐条目点名 grants_effects[i] 拒绝；
      "duration": 5.0 }             //   食物或 drinkable + init 逐条目交叉校验效果名（ammo 模式）。
  ],
  "ignites_flamable": true          // 可选（Phase 60）：严格 bool，缺省 false。true = 点燃器——
                                    //   右键烧 `flamable` 块（setBlock fire、**无掉落**、**不消耗**）；
                                    //   与五个右键通道互斥（food/drink/launcher/dye/fluid bucket，
                                    //   通道所有权纪律）；非 bool 点名拒绝（P60）。
  "max_stack": 1,                   // 1..64，默认 64
  "icon": "wooden_pickaxe"          // UI 图标（Phase 15）：assets/textures/items/<icon>.png
}
```

```json
{ "id": "stick", "type": "material", "icon": "stick" }   // 材料：无 tool 字段，max_stack 默认 64
```

`icon` 指向 `assets/textures/items/<icon>.png`（16×16 RGBA，可多个物品共享同一
图标文件；文件缺失 = 启动报错；缺省=不画图标）。block item 没有独立 JSON——
每个数据方块隐式生成同名 item（方块定义是唯一来源，UI 里以带光照的 3D 立方体
呈现，不用 2D 图标）；显式物品只能是 `type: "tool"` 或 `type: "material"`
（材料带 `tool` 字段=启动报错，防止第二数据源冲突）。非 block 物品（工具/材料）
不可放置；它们的**掉落物在 world 中仍无实体表示**（UI/手持/背包图标已有，
见 docs/KNOWN_ISSUES.md）。

## 新增一个方块（无需改核心代码）

1. `assets/textures/blocks/<name>.png` —— 16×16 RGBA 纹理（可参考 `tools/generate_block_textures.py`）
2. `assets/data/blocks/<id>.json` —— 方块定义（属性/掉落/模型/纹理），例如：

   ```json
   {
     "id": "glowstone",
     "properties": { "hardness": 0.3, "light_emission": 12, "drops": "glowstone" },
     "model": { "textures": { "top": "glowstone", "side": "glowstone", "bottom": "glowstone" } }
   }
   ```

   属性缺省即默认值（solid/opaque/destroyable=true, hardness=1.0, light_emission=0, drops 为空=不掉落, drops_count=1）。
   `light_opacity`（`[LGT-2]`，0-15，越界点名拒绝；缺省**由 `opaque` 派生**：
   opaque→15 / 非 opaque→0 = 未设字段的块逐位复现旧行为）：光**进入**该格
   每格扣减（MC light_opacity 同款刻度）——15 = 全挡（= opaque 不穿透）、
   0 = air 同款（垂直天空免费 / 水平与格光 1/步）、中间 = 每层扣 N
   （shipped：leaves=2、glass=1）。**流体格不读本字段**（固定 3/格 = P51
   文档化简化，water/lava JSON 无需设置）。cross/glass_pane 等薄模型禁止
   15（init 点名，与 "must not be opaque" 同款矛盾类）。emission 源格自身
   持值不受影响（15 只挡进入、不挡自身发光）。
   可选 `interact`（Phase 17/40）：锁定时右键命中该方块打开对应面板而非
   放置——值必须来自已知交互集（`crafting_table` 工作台、`furnace` 炉子，
   其他值 = 启动报错）。
   校验严格：类型错误、越界值、重复 id、缺失纹理面都会报明确错误；未知字段警告但不拒绝。
   `drops` 是**物品名**（游戏启动时对 ItemRegistry 严格校验，未知名=启动报错）——
   block item 与方块同名，所以 `grass → "dirt"` 表示破坏 grass 掉 dirt；
   矿石掉材料物品（`iron_ore → "iron_ingot"`）。`drops_count`（1..64，默认 1）
   是单次破坏的掉落数量，须 ≤ 掉落物品的 max_stack（init 交叉校验）。
   **概率次级掉落**（Phase 49）：可选 `chance_drop` 对象，与 `drops` **独立**
   （一次破坏最多产生两个掉落物实体）：

   ```json
   "chance_drop": { "item": "apple", "count": 1, "chance": 0.0078125 }
   ```

   `item`+`count`+`chance` 是**严格三元组**（缺一 = 解析拒绝；`count` 1..64；
   `chance` ∈ (0,1]，1 = 保底掉落）；`item` 须是已注册物品且 `count` ≤ 其
   max_stack（init 交叉校验，drops 纪律）。是否掉出由**确定性哈希**决定——
   `rollChanceDrop(world_seed, x, y, z, chance)`（`src/block/chance_drop`，
   splitmix64 + 位置 hash3 + 专用 salt，纯函数）：**同一世界同一格每次破坏
   结果相同**（无 per-instance RNG、无墙钟），无输入运行字节可复现。
   **概率次级掉落列表**（Phase 52）：可选 `chance_drops` **数组**，元素是
   与上面**相同的严格三元组**；与单条目 `chance_drop` **互斥**（同时写 =
   解析拒绝，二选一）：

   ```json
   "chance_drops": [
     { "item": "wheat_1", "count": 1, "chance": 0.125 },
     { "item": "apple",   "count": 2, "chance": 0.5 }
   ]
   ```

   每个条目**独立 roll**（`rollChanceDropEntry(seed, x, y, z, entryIndex,
   chance)`，条目索引去相关 salt——同格各条目互不追踪、也不追踪单条目
   roll），每条目至多 1 个掉落实体；空数组 / 任一元素非对象 / 未知键 /
   缺三元组 / 越界均拒绝（逐条目点名 `chance_drops[i]`）；init 每条目
   复用 drops 纪律（物品存在 + `count` ≤ max_stack）。使用者（含
   `[Phase 55]` 内容扩展）：`grass`（`drops: dirt` +
   `chance_drops: [wheat_1 ×1 @ 0.125]` = MC 草掉泥土 + 偶尔种子）；
   `leaves`（`[stick ×1 @ 0.02, apple ×1 @ 1/128]` = MC 1.14+ 树叶掉
   木棍 2% + 苹果；无 sapling = 文档化偏离）；`lapis_ore` /
   `glowstone`（`drops_count: 2` + 两个 50% 条目 = 2..4 均匀，MC 同款
   数量分布）。同物品掉落会合并（`kMergeDistance`）——断言/统计掉落时
   数**实体数 × count**，不是实体个数。

   `hardness` 驱动破坏耗时（按住左键，`1.5s × hardness ÷ 选中物品 break speed`，
   hardness ≤0 且可破坏=即时破坏）。
   **十字植物形状**（Phase 45）：`model` 加 `"shape": "cross"`，`textures`
   只给单个 `"cross"` 键（cube 的 top/side/bottom 与 cross 形状互斥，给了 =
   启动报错）；注册时强制 `solid=false, opaque=false, transparent=true`
   （属性给错 = 启动报错）。纹理 = 透明背景 + 植物剪影（alpha cutout，
   16×16 RGBA，参考 `tools/generate_block_textures.py` 的 `make_flower`）。
   渲染 = 格内 2 条交叉竖直面片（混合 pass、双面、自身格光照），详见
   MECHANICS「十字植物渲染」。掉落走 `drops`（= 自身块名的 block item，
   UI/手持/掉落物图标 = 同款两面片 3D 体）。

    **per-block 状态**（`[ENT-2]`，Phase 94 楼梯朝向泛化）：`model` 可选
    **严格配对**字段 `state_count`（int [1,255]）+ `state_default`（int
    [0,state_count)）——缺一 = 点名拒绝（fast_against 范式）。带状态的块
    在 World 里每格一个 byte（chunk 的 `m_blockState` 数组，255 = 未设哨兵）：
    放置时收口写 `state_default`，潜行右键原地轮换 `(s+1)%state_count`
    （"place" 音、点击消费），存档 format 15 `states` 段全量持久化
    （每个 stateful 格恰一条，含默认值）。消费方（碰撞/mesh/save 推导）
    读 World byte、哨兵回落 `state_default`。**双门控**：`Shape::Stairs`
    自带**隐式**状态（count 4、default = `stair_facing`——shipped 楼梯
    内容**零字段**，行为逐位不变），楼梯形状显式写 `state_count` = 矛盾
    点名拒绝。约束：**状态值不得改变块的光属性**（opaque/light_opacity
    是 per-block 非 per-state——v1.0 全部消费者满足，per-state 光 = 未来
    扩展）；**worldgen 放置源不得用 stateful 块**（biome surface/
    subsurface/decorations、ores.veins、trees trunk/leaves——init 交叉
    校验点名拒绝：per-instance 状态没有 worldgen 写入方，存档 journal 从
    block 派生 states 依赖此不变量）。

## 新增一个配方（无需改核心代码）

`assets/data/recipes/<id>.json`（shapeless：原料袋，顺序无关；产物必须装进一个
堆栈）：

```json
{
  "id": "iron_pickaxe",
  "output": { "item": "iron_pickaxe", "count": 1 },
  "ingredients": [
    { "item": "iron_ingot", "count": 3 },
    { "item": "stick", "count": 2 }
  ]
}
```

校验严格（经 ItemRegistry 交叉检查，错误信息点名配方）：id 非空且唯一；
`output.item`/原料物品名必须已注册；`output.count` ∈ [1, 产物 max_stack]；
shapeless：原料非空、数量 ≥1、同一物品不得重复出现（合并为一条）；
shaped（Phase 17）：`pattern` 1..3 行、行长 ≤3、空格=空位、非空字母必须在
`key` 中定义且指向已注册物品；未知字段警告。
`craft()` 语义：原料按槽序升序消耗（hotbar 先于背包），先在背包副本上模拟
"消耗+入包"，成功才提交——失败时背包逐位不变（不会扣料却放不下产物丢物品，
产物是原料之一的配方也正确）。网格配方（Phase 17）另走 `CraftingGrid` 匹配：
pattern 偏移扫描（任意左上偏移放进 3×3、空格对应格必须为空），先 shaped 后
shapeless、配方按文件名字典序。当前配方链：wood→4 planks→(2 planks→4
stick）、2×2 planks→crafting_table、wooden_pickaxe（PPP/.S./.S.）、
iron_pickaxe（III/.S./.S.）、stone_pickaxe（CCC/.S./.S.，C =
cobblestone，Phase 62 = MC 配方、cobblestone 首个工具用途）、arrow
（flint+stick+feather→4，Phase 60 替换 P24 的 2 木棍竖排→2）、
flint_and_steel（flint+iron_ingot，Phase 60）、stone（4
cobblestone→1，Phase 61 = MC 材料循环的回程）。
游戏内触发：`E` 背包面板右侧的配方列表（仅 shapeless），点击配方行执行
`craft()`（Phase 15；可合成的配方白色高亮、不可合成的灰色）；或放置并右键
工作台打开 3×3 面板，按形状放料后取输出格（Phase 17）。

## 新增一个染料 / 染色方块（无需改核心代码，Phase 41；Phase 45 起颜色零代码）

颜色名是**纯数据词汇表**（Phase 45 起）：`dye_<color>` 物品注册即扩展
词汇表，**加新颜色名 = 零代码改动**。当前 15 色：`white`/`black`/`brown`/
`red`/`yellow`/`orange`/`pink`/`gray`/`silver`（P41）+ `blue`/`green`/
`purple`/`magenta`/`light_blue`/`cyan`（P45；项目 silver 位 = MC
light_gray）。加一个颜色 = 纯数据四步：

1. **染料物品** `assets/data/items/dye_<color>.json`：
   `{"id", "type": "material", "max_stack": 64, "icon": "dye_<color>",
   "dye_color": "<color>"}`；图标 `assets/textures/items/dye_<color>.png`
   （32×32，`tools/generate_item_icons.py` 的 `DYE_PALETTE` 同款确定性
   生成——调色板加一行即可）。显式 `"dye_color": ""` 拒绝。
2. **染色变体方块** `assets/data/blocks/<base>_<color>.json`（white 特例 =
   `<base>.json` 本身，名字不加后缀）：
   `properties` 加 `"dye_base": "<base>", "dye_color": "<color>"`（两者必须
   同时给；`dye_base` 与 `interact` 互斥）；纹理 tile 加进
   `assets/data/textures/block_atlas.json`（atlas = 16×8 网格，Phase 61 后
   已用 66 格；有空位直接加 `[col, row]` 条目。**网格扩容会移位所有既有
   UV（u = col/列数）→ 无输入基线门必须重锚**——Phase 59 的 gravel tile 是
   第 65 格，8×8 已满 → 扩 16×8 → base18 重锚（P41 同款先例），见 PITS）。
3. **合成配方** `assets/data/recipes/dye_<color>.json`（shapeless；当前 15 条
   见 GAMEPLAY「染色」；来源块 = 世界内容：lapis 矿脉 / 仙人掌 / 十字花，
   见「新增一个方块」+ biome `decorations`）。
4. **init 交叉校验**（启动即检查，报错点名）：每个 dyeable 块的 `dye_color`
   必须有同色染料物品；**每个 (dye_base, 已有染料颜色) 组合的变体块必须存在**
   ——所以加一个新颜色时，**所有** dye 家族都要补变体块（当前 = wool 家族
   15 个）；加一个新 dye 家族 = 15 个变体块。

交互规则见 MECHANICS「染色」：选中染料右键 dye 家族方块 = 换块 + 消耗 1；
同色重复不消耗（v1 规则）。

## 新增一个 Mob（无需改核心代码，Phase 20）

1. `assets/textures/mobs/<tile>.png` —— 16×16 RGBA 纹理（`tools/generate_mob_textures.py`
   同款：splitmix64 确定性噪声 + 简单五官）；并在 `data/textures/block_atlas.json`
   的 `tiles` 里给它一个空位 `[col, row]`（16×8 网格，Phase 61 后已用 66 tile，勿动已有条目）
2. `assets/data/mobs/<id>.json` —— mob 定义（行为参数 + box 模型，每箱一个
   atlas tile；box 必须在 `width × height` 的 AABB 内）：

   ```json
   {
     "id": "zombie",
     "properties": {
       "health": 20, "speed": 0.8,
       "detect_radius": 16, "attack_range": 1.6,
       "attack_damage": 3, "attack_cooldown": 1.0, "wander_interval": 2.5,
       "spawn_max_light": 0,
       "hostile": true,
       "drops": [ {"item": "rotten_flesh", "count": 1} ]
     },
     "model": {
       "width": 0.6, "height": 1.8,
       "boxes": [
         { "pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "zombie_head" },
         { "pos": [0.0, 0.5, 0.0],  "size": [0.5, 1.0, 0.3], "texture": "zombie_body" }
       ]
     }
   }
   ```

   校验严格（同方块/物品注册表纪律）：id 非空且唯一、数值字段 >0（唯三例外
   `detect_radius`/`attack_range`/`attack_damage` 允许 0 = 被动/无害，负数
   拒绝）、`spawn_max_light` 为 0..15 的整数（默认 0，光照门——见上）、
   `hostile` 为可选布尔（默认 false；难度系统的数据开关——保护环 + 伤害
   缩放，非布尔拒绝）、远攻 mob（Phase 27）可选字段：`shoot_range` /
   `retreat_distance`（≥0，`shoot_range > 0` = 远攻者）、`shoot_speed`
   ((0,100]，远攻必填)、`ammo`（Phase 42：**非空字符串** = 该 mob 射出的箭
   插墙后被玩家拾取时返还的弹药物品名；远攻必填 + init 对 ItemRegistry
   交叉校验物品存在，显式空串/非字符串拒绝；近战 mob 不给 = 无远攻）、
   挤奶（Phase 43）可选字段：`milk_with`（手持物品名）+ `milk_gives`
   （产出物品名）——**必须成对**（只给一边 = 报错，dye_base/dye_color
   纪律）、非空字符串、init 对 ItemRegistry 交叉校验两物品存在；
   手持 `milk_with` 右键该 mob = 消耗 1 个 + 产出 1 个 `milk_gives`
   （mob 不受影响、无冷却，见 KNOWN_ISSUES）；不给 = 不可挤、
   命中效果（Phase 46）可选字段：`on_hit_effect` =
   `{"name": <效果名>, "duration": <秒>}`——**两键必填成对**（缺一 /
   未知内键 / 类型错 = 报错）、duration ∈ (0,100]、name 非空字符串 +
   init 对 EffectRegistry 交叉校验（= ammo/挤奶模式）；该 mob 的
   **近战命中落定**时对玩家施加效果（远程箭不施加）；不给 = 无效果、
   `drops` 每条目：`count` ∈ [1,64] 或
    `count_min`/`count_max` ∈ [0,64] 且 min ≤ max（Phase 38 随机区间：两者
    必须成对、与 `count` 混用/只给一边 = 报错）、box 越界/缺纹理名
   = 明确报错；未知字段警告。
   `drops` 是**物品列表**（Phase 28：`[{"item": "leather", "count": 1}, ...]`；
   Phase 38：条目可用 `count_min`/`count_max` 随机区间（如
   `{"item": "bone", "count_min": 0, "count_max": 2}`——死亡时按列表顺序
   用该 mob 的种子流掷一次，0 = 该条目不掉）；每条目物品名 init 时对
   ItemRegistry 交叉校验；缺省/`[]` = 不掉落；旧的单字符串 + `drops_count`
   schema 被显式拒绝——迁移防护）。
3. 生成名单：`assets/data/mob_spawning.json` 的 `mobs` 数组加入条目
   `{"id": <id>, "weight": <1..10000>}`——weight = 相对生成概率（每次尝试
   抽中该类型的权重占比），缺省或 1 = 与其他 weight 1 条目等概率；调高某
   类型 weight 即可让它更常出现（Phase 39）。旧纯字符串条目被显式拒绝
   （迁移防护）；重复 id / 越界 / 非整数 weight = 加载失败。生成
   节奏/环半径/上限/离玩家消散距离都在这个文件里，全部数据驱动。

v1 数据有 6 种：zombie（敌对：绿色、只在黑暗生成）、skeleton（远攻：
`shoot_range` 14 + `ammo: "arrow"`，只在黑暗生成）、spider（敌对：
近战命中施加 `poison` 15 s = 首个 `on_hit_effect` mob，只在黑暗生成、
无掉落）与 cow / sheep / chicken（被动：攻击全 0、`spawn_max_light` 15
任何时间可生成；cow 掉 `leather` + `beef` 且可挤奶（`bucket` →
`milk_bucket`），sheep 掉 `wool` + `mutton`，chicken 掉 `feather` +
`chicken`）；全都不游泳不拆方块无动画——见 docs/KNOWN_ISSUES.md 的
Mob 章节。

## 新增一个状态效果（无需改核心代码，Phase 46）

1. 图标：`assets/textures/effects.png` 是 16px/tile 的透明底条带
   （当前 80×16 = 5 tile：poison / weakness / regeneration /
   absorption / fire）。加新效果 = 条带加宽一格（`tools/
   generate_effect_icons.py` 同款：确定性噪声 + 简单图元）；init 对
   每个效果的 `icon` 交叉校验 < tile 数，纹理宽度保持 16 的整数倍
2. `assets/data/effects/<id>.json`（**扁平 JSON**，无 properties 包裹）：

   ```json
   {
     "id": "slowness",
     "melee_penalty": 0,
     "negative": true,
     "icon": 5
   }
   ```

   字段（严格解析 = 注册表纪律，未知键拒绝）：`tick_type`（`"damage"`
   / `"heal"`；**缺省 = 无 tick** 的纯修饰效果，如上面的 slowness 骨架
   ——但注意 v1 核心只消费 `tick_*`/`min_health`/`melee_penalty`/
   `absorb_hp`/火系统标志 语义，纯修饰效果目前只影响 HUD/存档/牛奶清除）
   + `tick_amount`（每次 tick 的 HP，(0,10]，有 tick 必填）+
   `tick_interval`（秒，(0,100]，有 tick 必填）+ `min_health`（**仅
   damage**：0 = 无地板可致死 / 1 = 保持 ≥1 HP 永不致死 = MC 中毒地板）
   + `melee_penalty`（近战扣除攻击点，[0,100]）+ `absorb_hp`（Phase
   56：激活期间的**临时 max HP** 点数，整数 [1,64]——吸收层，见
   `docs/MECHANICS.md`「状态效果」）+ `negative`（bool，缺省 false；
   牛奶只清负面——fire 取 false = 牛奶不灭火）+
   `ignites_on_lava`（Phase 57：bool，缺省 false——玩家**在熔岩内**时
   每帧重施加本效果（duration 重置 = 熔岩内持续燃烧；离熔岩后继续
   countdown 至到期）。**至多一个**注册效果可带此标志（init 交叉校验，
   两个 = 启动失败；零个合法 = 无火系统的资产集））+
   `extinguished_by_water`（Phase 57：bool，缺省 false——玩家**在水内**
   时本效果被瞬时移除（在 tick 之前执行 = 水内零伤害）；水胜熔岩：
   两流体并存时先点燃后灭火 = 无火）+ `icon`（条带 tile 索引，必填）。
   **no-op 拒绝**：无 tick 且 `melee_penalty` = 0 且 `absorb_hp` = 0
   = 报错。id 非空且唯一
3. 来源（v1 五条）：
   - **mob 命中**：`assets/data/mobs/<id>.json` 加 `on_hit_effect`
     （见 Mob 章节）——近战命中施加
   - **进食施加**（Phase 49/56）：物品加 `grants_effect`（单条）或
     `grants_effects`（列表，按序施加；两字段互斥，见物品模板）——
     完整吃下时施加（金苹果 → regeneration 10 s + absorption 5 s）
   - **熔岩点燃**（Phase 57）：效果带 `ignites_on_lava` 即自动成为熔岩
     点燃源（当前 = `fire`；时长走 config `player.fire_duration_seconds`，
     默认 8 s = MC Normal，(0,100]）
   - **存档注入**：`player.effects: [{"name": "<id>", "remaining": <秒>}]`
     （format 8；E2E 场景用）
   - **吸收层读档**（Phase 56）：效果含 `absorb_hp` 时，存档 health 可
     高于 base max（上限 = base + 层，读档时层先于 health 生效）
4. 机制自动生效，无需改核心：真实 dt 倒计时 + 每跨 interval 结算一次
   （damage 走玩家受伤路径含 min_health 地板、heal 走 `Player::heal`
   clamp 有效上限【可填进吸收层】）、HUD 图标行（按施加顺序）+ 吸收
   红心带（正常心带左侧）、F3 `effects` 行、牛奶清负面（吸收 = 正面，
   保留）、死亡清空、存档 format 8 自动序列化/校验。语义细节见
   `docs/MECHANICS.md` 的「状态效果」。

当前 4 种：`poison`（damage 1 / 2.5 s、min_health 1、负面——spider
近战命中 15 s 毒源）、`weakness`（melee_penalty 4、负面——v1 无游戏内
来源）、`regeneration`（heal 1 / 4.5 s、正面——金苹果吃时给 10 s，
Phase 49）、`absorption`（absorb_hp 4、正面——金苹果吃时给 5 s，
Phase 56；HUD 红心带 = 心带左侧 2 颗红心）。

## 新增一个流体桶（无需改核心代码，Phase 47，Phase 51 双种类）

流体种类词汇表在代码中（`FluidKind`：`"water"` / `"lava"`，其他名解析
拒绝；新增种类 = 改代码的编码区间/词汇表）。桶 = 空桶 + 每种流体一个
满桶，字段：

1. **空桶侧**（舀 → 满桶）：`collects_fluid` = **流体名数组**（一个
   空桶可舀所有命名流体，MC 语义）：

   ```json
   { "collects_fluid": ["water", "lava"] }
   ```

2. **满桶侧**（倒 → 空桶），每种流体一个：

   ```json
   { "pours_fluid": "lava", "pours_becomes": "<空桶物品名>" }
   ```

   **满桶由空桶侧按流体推导**（`fullBucketFor(fluid, 空桶)`）——没有
   `collects_becomes` 字段（P51 废除）。

3. **init 硬校验**（两侧各自解析时本地一致 + 全部加载后交叉）：
   空桶命名的每种流体必须**恰有 1 个**满桶倒回该流体到本空桶（0 =
   dead field、2 = 舀的目标歧义，都拒绝，错误点名）；满桶的
   `pours_becomes` 伴侣必须存在且 `collects_fluid` 含本流体（对称，
   错配 = 启动失败点名双方）。一个物品**只能是一侧**（`collects_fluid`
   与 `pours_fluid` 互斥），且桶侧**不能同时是食物/可饮用**（右键
   通道归属，解析拒绝）
4. **图标**：满桶物品要 `icon`（`tools/generate_item_icons.py` 加一个
   函数 + ITEMS 表项，如 `lava_bucket` = 橙色熔岩面的桶）
5. 机制自动生效：舀 = 桶目标射线（首个「非 air 方块**或**带流体」的
   格，墙壁遮挡）命中 **level 8 源**格 → 移除源 + 换该流体的满桶 +
   "splash" 音；倒 = 放置目标格（受击面邻格）为 air 无流体且不重叠
   玩家 → 放该流体的 level 8 源 + 换空桶。倒下的源走正常 FluidSim
   （下落/扩散）。存档自动（物品按名 + 流体走 `fluids` 编辑——format 9
   起流体编辑带种类名 `[x,y,z,level,"water"|"lava"]`）
6. **舀的视线约束**（MC 行为，场景设计注意）：源格每 tick 向四向邻格
   补 level 7——**远处的玩家永远隔着自己源头的扩散膜**（L7 ≠ 8，舀不
   到）；要舀源，玩家必须站在源旁 1 格、从**上方**斜视源的顶面（射线
   从源的上方格直接进入源格）。E2E 场景见 PITS「流体桶 E2E 视线」

当前对：`bucket`（max_stack 16，`collects_fluid: ["water","lava"]`）
⇄ `water_bucket` + `lava_bucket`（各 max_stack 1）。

## 新增熔岩场景 / 反应（Phase 51）

熔岩 = 第二种流体种类，机制与水完全同构（同 level 1..8 编码区间
9..16、同 FluidSim 规则、同桶通道），差异全在数据：

1. **流体块**：`"fluid": "lava"`（词汇表内）+ `light_emission: 15`
   （熔岩发光，格光来源 = 该块的 `light_emission`）+ 可选
   `water_reaction: {"source": "<邻水含 L8 源时的产物>", "flowing":
   "<邻水为流动水时的产物>"}`（严格配对、需流体块、init 校验两个
   名字指向存在的方块）。**无 `water_reaction` = 惰性熔岩**（与水
   相邻不反应，异种流体格互相按"方块"处理 = 挡扩散不替换）
2. **桶对**：见上（满桶 `pours_fluid: "lava"`）
3. 放置/存档/E2E 场景：熔岩源 = 流体编辑 `[x,y,z,8,"lava"]`；
   反应产物（黑曜石/石头）= 普通方块编辑

## 新增一个可生长作物（无需改核心代码，Phase 48）

作物 = 一串 cross 形状方块，用 `grows_to` 链成生长序列；**生长状态就是
方块 id 本身**（`wheat_N` 的 N = 阶段），无 per-instance 状态、无存档
字段。核心生长引擎（`src/world/growth.{hpp,cpp}` + `Game::m_crops`）
是纯数据驱动——加一种新作物 = 加方块 JSON + 纹理 + 物品/配方，不改核心。

1. **生长阶段方块**（每阶一个 `assets/data/blocks/<name>_<n>.json`）：

   ```json
   {
     "id": "wheat_1",
     "properties": {
       "hardness": 0.0, "solid": false, "opaque": false, "transparent": true,
       "drops": "wheat_1",              // 未熟掉种子（可再种）
       "grows_to": "wheat_2",           // 指向下一阶（成熟阶留空 = 停止）
       "growth_ticks": 360,             // 平均多少 tick（1/60s 帧）进一阶
       "grows_on": "farmland",          // 下方须为该土壤（Phase 50；缺省 = 无要求）
       "plant_on": ["farmland"],        // 放置时下方须为之一（Phase 53；缺省 = 不限）
       "growth_light": 9,               // 作物格最低光照 max(sky,block)（9 = MC 值）
       "dry_slowdown": 2,               // 无水时 roll 模数倍数（2 = MC 干耕地）
       "flamable": true                 // 熔岩 6-邻接触即烧成 fire（Phase 58/64）
     },
     "model": { "shape": "cross", "textures": { "cross": "wheat_1" } }
   }
   ```

   `grows_to` 与 `growth_ticks` 是**严格配对**（缺一 = 解析拒绝；
   `growth_ticks` ∈ [1, 1'000'000]；显式空串 = 笔误拒绝；自引用拒绝）；
   **init 硬校验** `grows_to` 与 `grows_on` 都必须指向存在的方块
   （typo = 启动失败点名）。成熟阶（链尾）**不写** `grows_to`（不 roll、
   不再长），`drops` 指向收获物（如 `wheat_7` → `wheat` ×2）。cross 形状
   要求 `solid=false, opaque=false, transparent=true`（解析强制）。
   三个环境字段（Phase 50，均**可选**，缺省 = 无要求 = 旧行为）：
   `grows_on`（土壤名，显式空串 = 笔误拒绝）、`growth_light`（整数
   [0,15]，0 = 无要求，非整数拒绝）、`dry_slowdown`（整数 [1,64]，
   1 = 无减速，非整数拒绝）。
   放置字段（Phase 53，**可选**）：`plant_on`（方块名**数组**——放置该
   方块时，目标格**正下方**须为列表之一；缺省 = 不限 = 通用 placeBlock
   规则；空数组 = no-op 拒绝（ammo 纪律）；逐条目点名 `plant_on[i]`
   拒绝；init 每条目必须指向存在的方块，grows_on 同款纪律）。
   `plant_on`（放置门）与 `grows_on`（生长门）**相互独立**：可以放在
   土壤 X 上却只在土壤 Y 上生长（本项目两者同值 = farmland）。
   可燃字段（Phase 58，**可选**）：`flamable`（bool，非 bool 点名
   拒绝）——该方块的任意格被**熔岩 6-邻**接触时，FluidSim 燃烧 pass
   每帧把它烧成 **fire 块**（Phase 64：燃烧目标 = registry 有 `fire`
   块时 fire、否则 air；**无掉落**；水不烧）。内容 = wheat_1..7 全
   7 阶 + wood/leaves/grass（Phase 63 = MC 天然可燃三块，纯数据；
   点燃器 Phase 60 同款生效）。**init 交叉校验**（Phase 64）：registry
   有**任何** flamable 块则**必须**有 `fire` 块（删 `data/blocks/
   fire.json` = 启动报错点名）；零 flamable 的资产集合法。加可燃方块
   = 一行 `"flamable": true`，零代码。v1 不燃：羊毛/高草/火把（见
   KNOWN_ISSUES）。
2. **生长引擎**（自动）：`Game::m_crops`（`map<BlockPos,BlockID>`，位置序）
   追踪所有带 `grows_to` 的格——`setBlock` 收口维护（放作物=入集，
   破坏/成熟=出集，炉子同款模式），**读档后 `rescanCrops()` 全量扫描
   已加载 chunk 重建**（无存档字段，方块 id 即阶段）。每帧（固定 1/60
   步长，昼夜类——无输入运行字节可复现）先过三道**环境门**（Phase 50，
   全数据驱动，见下方「耕地与作物环境条件」），再 roll 确定性哈希
   `splitmix64(seed + frame×GOLDEN + hash3(x,y,z,salt)) % ticks == 0`
   → 进一阶（概率 1/ticks；`ticks` = `growth_ticks` × 干时减速）。
   位置哈希让两株作物不同步；世界 seed 让世界间去相关。
3. **种子 = 隐式 block 物品**：`wheat_1` 方块自动有同名 block 物品
   （右键 = 放置 = 种植），无需新物品。种子来源 = 某方块的 `drops`
   （本项目 `grass` → `wheat_1`，见 KNOWN_ISSUES）。
4. **收获物 + 食物**：`assets/data/items/<name>.json`（如 `wheat` 材料、
   `bread` 带 `restore_hunger`）；配方 `assets/data/recipes/<name>.json`
   （shapeless = 只写 `ingredients`，**同一物品合并成一条 count**——
   重复项解析拒绝）。
5. **纹理**：每阶一个 cross 纹理（`tools/generate_block_textures.py` 加
   函数 + TEXTURES 表项）+ `block_atlas.json` 空位加一行（不挪已有 UV）；
   物品图标（`tools/generate_item_icons.py`）。
6. **世界生成不放作物**（v1）：自然界的作物只来自玩家种植，`rescanCrops`
   对新生世界扫到空集。

当前唯一作物：`wheat_1`→…→`wheat_7`（growth_ticks 360，约 36 s 成熟），
收获 `wheat`（3 wheat → 1 bread，bread `restore_hunger` 5）。

## 概率掉落与"吃时给效果"的食物（Phase 49）

两个独立的小扩展，都是纯数据 + 注册表严格校验，不改核心玩法逻辑：

1. **概率次级掉落**（方块属性 `chance_drop`，见上方「新增一个方块」）：
   首个使用者 = `leaves` 的 `"chance_drop": { "item": "apple", "count": 1,
   "chance": 0.0078125 }`（1/128，Minecraft 橡树叶掉苹果 ~0.5% 同款量级）。
   加"某方块偶尔掉 X" = 在该方块 JSON 加一个 `chance_drop` 对象 + 物品
   JSON/图标，零核心代码。需要**多个**偶尔掉落（各自概率、独立 roll）=
   改用 `chance_drops` 数组（Phase 52，与 `chance_drop` 互斥；首个使用者
   = `grass` 的 `[wheat_1 ×1 @ 0.125]`）。
2. **吃时给效果的食物**（物品字段 `grants_effect`，见上方物品模板）：
   首个使用者 = `golden_apple`（`"grants_effect": { "name": "regeneration",
   "duration": 10.0 }`）。进食通道完整吃下（1.6 s）时在恢复饥饿的同一处
   施加效果（与 mob `on_hit_effect` 同一条 `StatusEffects::apply` 路径）——
   效果名/时长全数据驱动，效果本体在 `assets/data/effects/*.json`（Phase
   46 框架）。加"吃了给某效果"的食物 = 物品 JSON 加 `grants_effect`（须
   是食物），零核心代码。
3. **内容**：`apple`（材料，restore_hunger 4 = MC，max_stack 64）+
   `golden_apple`（材料，restore_hunger 4 + 吃给 regeneration 10 s）+
   shaped 配方 `GGG/GAG/GGG`（G=gold_ingot，A=apple，MC 同款 3×3 镶金）。
   纹理：2 个物品图标（`tools/generate_item_icons.py` 的 `apple` /
   `golden_apple`）。
4. **无 format 变更**（format 8：掉落物实体走既有 `entities`，效果走既有
   `player.effects`）。

## 耕地与作物环境条件（Phase 50）

消费 P48 遗留"作物无耕地/水/光照要求"：生长引擎在 roll 前过三道门，
全部**纯数据 + 纯函数**（`src/world/growth.{hpp,cpp}`），无 per-instance
状态、无存档字段、无输入运行零活动（结构性免疫，base18 免重锚）：

1. **土壤硬门**（`grows_on`，作物侧字段）：作物**下方一格**必须是指名
   方块，否则不 roll（`""` 缺省 = 无要求）。首个使用者 = 小麦
   `wheat_1..6` 的 `"grows_on": "farmland"`。土壤块本身是普通方块
   （`assets/data/blocks/farmland.json`：solid cube、hardness 0.5 = MC、
   `drops` 回 `dirt` = MC 语义）。
2. **光照门**（`growth_light`）：作物格 `max(天空光, 方块光)`（与渲染
   同款组合）≥ 阈值才 roll（0 = 无要求；小麦 = 9 = MC 值）。光照是
   世界状态（chunk 光照数组），读档/生成后稳定 → 确定性。
3. **干湿地**（`dry_slowdown`）：土壤格（作物下方）同 y、切比雪夫半径
   4（= MC 耕地湿润半径）内有**任一流体**（level ≥1）= 湿润 → roll
   模数 = `growth_ticks`；否则模数 × `dry_slowdown`（小麦 = 2 = MC 干
   耕地半速）。模数加倍不改变 roll 值 → **干时通过帧是湿时通过帧的
   严格子集**（恰好 1/dry_slowdown 频率，确定性可离线预计算）。
   水桶（Phase 47）在土壤层倒下的源就在同 y → 浇水工作流直接可用。

**内容**（全 JSON + 纹理）：`farmland` 方块（顶面 = 犁过的深色垄纹
`farmland_top`、侧面 `farmland_side`、底面复用 `dirt`；atlas 空位 (7,4)/
(7,5)，无 UV 移位）+ 配方 `farmland`（shapeless 1 dirt → 1 farmland）+
`wheat_1..6` 加三个环境字段。**无 format 变更**（format 8）。

**已知简化**（见 KNOWN_ISSUES）：`dry_slowdown` 参照格永远是作物下方
一格（即使 `grows_on` 缺省）；水 = 任一流体层级（不区分源/流动）。
（"作物可种在任意表面" = Phase 53 已解决：`plant_on` 放置门。）

## 作物放置限制（Phase 53）

消费 P48/P50 遗留"放置不受限"：`placeBlock` 在通用规则（受击面邻格
air + 不重叠玩家）之后加一道**数据驱动土壤门**：

1. **门**（纯函数 `canPlantAt`，`src/world/raycast.{hpp,cpp}`，
   `canPlaceAt` 同款可单测）：放置目标格**正下方**一格须持有
   `plant_on` 列表之一；列表空 = 不限（恒真）。下方越界（目标 y=0）
   读作 air → 拒绝。
2. **拒绝语义**：土壤不符 = **静默 no-op**（不消耗物品、无音效——与
   任何被拒放置一致，右键顺序在 eat/drink/bow/milk/interact/dye/
   bucket 之后，`placeBlock` 内 canPlaceAt 之后）。
3. **首个使用者**：`wheat_1..6` 的 `"plant_on": ["farmland"]`（与
   `grows_on` 同值；wheat_7 成熟阶不可放置，不写）。
   加"某方块只能种在某土壤上" = 该方块 JSON 加 `plant_on` 数组，
   零核心代码。
4. **无 format 变更**（format 9）。

## 新增一个树种 / sapling（GEN-2，v1.0 任务表 8/29）

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
       "drops": "oak_sapling",          // 破坏掉自己（可再种）
       "flamable": true,
       "plant_on": ["dirt", "grass"],   // 放置门 + 生长门（一体，P53 字段复用）
       "growth_light": 9,               // 光门（max(sky,block)，MC 值）
       "growth_ticks": 120,             // 每阶段 roll 节奏（2 s/阶 ≈ 16 s/棵）
       "tree": {                        // 严格 6 键（缺一/未知键/越域点名 tree.<key>）
         "trunk": "wood",               //   树干块名（目录 load 后交叉校验存在）
         "leaves": "leaves",            //   冠层块名（同上）
         "min_height": 4,               //   树干高 [1,16]
         "height_range": 3,             //   高差 [0,8]（roll ∈ [min, min+range]）
         "leaf_radius": 2,              //   冠半径 [1,2]（仅 oak 形状表读）
         "shape": "oak"                 //   ∈ {oak, spruce, birch}（形状表，见下）
       }
     },
     "model": {
       "shape": "cross",
       "state_count": 8,                // tree 存在 ⇒ state_count ≥ 2（注册时点名拒）
       "state_default": 0,              //   阶段 = 状态 byte 0..7（0 = 幼苗）
       "textures": { "cross": "oak_sapling" }
     }
   }
   ```

   `growth_ticks` 与 `tree` 严格配对（tree 无 ticks = 点名拒；ticks 无
   grows_to/tree = 无意义拒）。stage 视觉 = emitCross 按
   `stateCount>0` 门控 quad 缩放 (stage+1)/stateCount（绕格心，MC
   sapling 同款；无状态 cross 恒 scale 1 零变化）。
2. **trunk 方块**：cube 实心 opaque（`hardness 2.0` = MC 原木、`drops`
   自己、`flamable`）；**leaves 方块**：非 opaque 透明
   （`light_opacity 2` = LGT-2 叶衰减、`hardness 0.2`、`drops` 自己、
   `chance_drops` 含**对应 sapling** @ 0.05 + stick @ 0.02 = 种植闭环；
   apple 保留 oak 专属）。
3. **生长引擎**（自动，m_crops 四件套同款）：`Game::m_saplings`（
   `map<BlockPos,BlockID>` 位置序）经 setBlock 收口维护 + 读档后
   `rescanSaplings()` + 卸载保护。每帧（1/60 步长）纯函数
   `saplingGrowthSteps`（world/growth，专用盐）：土壤门（plant_on）→
   光门（growth_light）→ roll `% growth_ticks` → 阶段 < 7 =
   `Advance`（`setBlockState` +1，**零光照成本**——状态不改光，ENT-2
   约束）；阶段 = 7 = 高度 roll（`treeHeightRoll`，纯函数，**帧无关**
   ——被挡重试滚同一棵树）+ fits（planTree 全格 air 无流体 y 界内）→
   `Complete` = `placeGrownTree`（trunk 先冠后逐格 setBlock + **1 次
   updateRegion** over 树 bbox = 冠影 LGT-2 可见）。放不下 = 本 roll
   不完成等待重试（MC 天花板规则）。
4. **形状表**（`world/tree_shape.{hpp,cpp}` 纯函数 `planTree`，
   root = sapling 格 = trunk 底）：oak = (T-2,r),(T-1,r),(T,1)
   **与 worldgen placeTrees 逐格同构**（单测钉死）/ spruce =
   (T-2,2),(T-1,1),(T,1),(T+1,1) 下宽上尖锥 / birch =
   (T-1,1),(T,1),(T+1,1) 细高小冠；(0,0) offset ≤ T-1 跳过（trunk 占位）、
   半径 2 层圆角。**worldgen 零触碰**：野外森林保持单橡种（sapling
   stateful ⇒ worldgen 放置源 init 不变量天然拒，多树种野外 = 未来项
   需重锚）。
5. **atlas 空位**（GEN-2 分配的 9 格，免重锚）：oak_sapling (10,1) /
   spruce_sapling (11,1) / birch_sapling (12,1) / spruce_trunk_side
   (13,1) / spruce_trunk_top (14,1) / birch_trunk_side (10,2) /
   birch_trunk_top (11,2) / spruce_leaves (12,2) / birch_leaves (13,2)。
   纹理 = generate_block_textures.py 新函数（salt 935-956，既有零变化）；
   sapling 物品**无独立图标**（block item 以 3D cross 呈现，UI 惯例）。
6. **存档**：sapling 格 = blockEdits + 阶段 = format 15 `states`
   段（journal 派生直接覆盖：位置靠 block journal、值退出 live 读；
   阶段推进零 block 写）；落树 = 纯 blockEdits 无 states。

当前 shipped：oak（wood/leaves 复用）/ spruce / birch 三树种，starter
库存各 3 颗（hotbar.json `inventory` 段，槽 24/25/26）。

## 调节玩家参数（config/config.json `player.*`，Phase 32）

玩家运动/生存模型的全部数值在 `Player::Params`，可在 `config/config.json`
覆盖：**缺键 = 结构体默认值**；**已存在的键必须是 JSON 数字**且过范围检查
（类型错/越界 = 启动报错，不会静默回退默认）；未知 `player.*` 键容忍（与
config 其余部分的前向兼容一致）。种子配置（首次运行生成的 config.json）
默认带出 9 个生存节奏键，其余键写出即生效：

| 键 | 默认 | 说明 / 约束 |
|---|---|---|
| `max_health` | 20 | 最大生命；心带图标数 = ceil(max/2)（默认 10 心 × 2 HP）；>0 |
| `fall_damage_offset` | 3 | 免坠落伤害格数（Minecraft: 3）；≥0 |
| `regen_delay_seconds` | 10 | 脱离受伤后多久开始自然回复；>0 |
| `regen_interval_seconds` | 4 | 自然回复每 +1 HP 的间隔；>0 |
| `max_hunger` | 20 | 最大饥饿；饥饿带图标数 = ceil(max/2)；>0 |
| `hunger_decay_seconds` | 36 | 衰减 tick 间隔（Minecraft 被动节奏：1 点 = 720 tick）；>0 |
| `starve_damage_interval_seconds` | 20 | 饥饿 = 0 时每间隔受 1 HP 饥饿伤害（Minecraft normal：400 tick）；>0 |
| `regen_hunger_threshold` | 6 | 低于该饥饿值无自然回复（Minecraft 规则）；0 ≤ v ≤ `max_hunger` |
| `sprint_hunger_decay_factor` | 2 | 疾跑衰减加速因子（间隔 ÷ 因子）；>0 |
| `lava_damage_interval_seconds` | 1.0 | 站在熔岩格里每间隔受 `lava_damage` HP 灼烧（Minecraft：4 HP/s，真实 dt 生存计时器；离熔岩重置未到期 tick）；>0（Phase 54） |
| `lava_damage` | 4 | 每次灼烧的 HP（Minecraft：4）；0 < v ≤ `max_health`（Phase 54） |
| `fire_duration_seconds` | 8 | 熔岩点燃的 `ignites_on_lava` 效果时长（Minecraft Normal：8 s；熔岩内每帧重施加 = 持续燃烧，离熔岩后 countdown 至到期）；0 < v ≤ 100（Phase 57） |

运动/几何全套键（同样可覆盖，均 >0）：`width`（0.6）、`height`（1.8）、
`eye_height`（1.62，须 < `height`）、`walk_speed`（4.3）、`sprint_speed`（5.6）、
`jump_speed`（8.5）、`gravity`（32）、`terminal_velocity`（38）、`swim_speed`（3.1）、
`swim_up_speed`（4.5）、`water_gravity`（12）、`water_terminal_velocity`（4）、
`lava_swim_speed`（1.9）、`lava_swim_up_speed`（1.5）、`lava_gravity`（4）、
`lava_terminal_velocity`（2）（Phase 54：熔岩里的游泳节奏——比水更慢/更沉，
比值对齐 Minecraft 的运动系数比 0.014/0.0231）、
`mouse_sensitivity`（0.0025）、`crouch_height`（1.6，须 < `height`）、
`crouch_speed_scale`（0.3）。

## 调节 UI 缩放（config/config.json `ui.scale`，Phase 34）

高分屏（高 DPI、1× 缩放）上 UI 太小时的屏幕空间缩放键：

| 键 | 默认 | 说明 / 约束 |
|---|---|---|
| `ui.scale` | 1 | 1.0–4.0 的 JSON 数字（越界/非数值 = 启动报错）；缺键 = 1.0 |

全部屏幕空间 UI（字体、hotbar、心/饥饿带、背包+合成面板、F3、死亡屏、
手持图标、准星）布局在"UI 像素"空间 = 窗口逻辑尺寸 ÷ `ui.scale`，正交投影
拉伸到可绘制区——GPU 放大，NEAREST 纹理保持锐利。`ui.scale=2`：字体 16px、
面板格 104px、物品图标 32px 物理像素。世界渲染与截图尺寸不受影响。
窗口 1280×720 + `ui.scale=2` 时面板（宽 1000 UI px）会向两侧裁切
（既有窄窗行为，中间槽位可用）；`ui.scale=1` 时与旧版逐位一致。

## 使用存档（Phase 13）

```bash
./build/bin/tokenmine --save myworld      # 首次：新游戏；此后：读档
./build/bin/tokenmine --save myworld --new  # 推倒重来（退出时覆盖 myworld 槽）
```

- 存档槽名只允许字母/数字/`_`/`-`（防路径穿越）；文件在 `data/saves/<slot>.json`
  （git 忽略），JSON 格式化输出、可直接阅读
- **存什么**：seed、`time`（timeOfDay [0,1)）、玩家 pos/yaw/pitch、背包 27 格 +
  选中格、掉落物实体（item/count/pos/age/pickup_delay）、`blocks` 与 `fluids`
  编辑列表（仅与 `generateChunk(seed)` 不同的格子，按位置排序）。一次没动过世界
  的存档 edits 为空。**mob（Phase 20）与箭（Phase 21）是瞬态实体**：保存时跳过
  （读档后不出现），不是错误；未知实体种类才会保存失败
- **读档语义**：`--time` 只对全新世界生效（已加载存档时存档时间优先）；
  `--seed` 与存档 seed 不一致 = 启动报错（编辑会落到错误地形上）；存档里的
  玩家位置/朝向覆盖出生点与初始视角
- **何时写**：干净退出（ESC 二次/关窗/`--frames` 到点）自动写回；游戏内 **`F5`
  立即存档**（Phase 15，`--save` 槽激活时，日志记录成功/失败）；初始化失败或
  崩溃不写（保护旧档）。写失败只记 ERROR 日志、退出码仍为 0（本局已正常运行）
- 存档是机器生成的：字段集由 format 号锁定（当前 format 1），加载器对未知字段
  一律报错（与用户手写数据文件的"警告但容忍"策略相反）
## Phase 4 关键参数（assets/data/worldgen.json）

- 地形：base 40 / amplitude 24 / 海平面 34 / 高度噪声 0.02×4 octaves；y=0 bedrock。
- Biomes：desert(w2, sand, tree 0, hs 0.9) / forest(w4, grass, tree 0.12, hs 1.1) /
  plains(w4, grass, tree 0.03, hs 1.0)；biome 噪声 0.02。
- 洞穴（`[GEN-1]` 面条洞）：`caves` 段——`noise_scale 0.09` /
  `tube_threshold 0.06`（两 3D 噪声中位带相交 = 管状隧道；0 拒、≥0.5 拒，
  点名）/ `min_y 2` / `surface_guard 0`（0 = 允许洞口打穿表面 = shipped；
  ≥1 = 表面 + guard 层壳保护 = 旧语义）/ `spawn_clear_columns 2`（原点
  Chebyshev ≤N 列整列不挖 = 出生/演示区结构保护，[0,8] 点名）。每湖 lava
  带排除盒（区域 x/z ±1 × [floorY−1, floorY+fill]）构造期预计算——洞穴不挖
  湖带（保 P88/P90 熔岩湖静态被围契约）。
- 矿脉：lattice 8；coal .4/size9/maxY80, iron .25/8/56, gold .12/7/36, diamond .05/5/20。
- 树：lattice 4，树干 4+0..3，两层 r2 树冠 + r1 顶帽。
- 熔岩湖（Phase 86 单湖，Phase 88 列表）：`lava_lakes` 段是一个**湖列表**，
  每个湖 `{"enabled","center_x","center_z","radius","depth","fill"}`——固定位
  置 (center_x, center_z)、半径 `radius`（盆地 (2R+1)²）、`depth`（盆底 = 区
  域最低地形 − depth）、`fill`（盆底上满熔岩层数）。`enabled: false` 关单个
  湖。每个湖被盆地围住（四周地形更高）= 静态湖；放出生点 -Z 视野外 → 无输入
  基线门不受影响。shipped 2 湖（(40,40) R4 + (55,45) R3）。
- 验证基线：默认 seed 123456789 → 128 chunks、spawn (0,0) surface y=53 forest；
  同 seed 截图逐字节一致（像素级复现已验证）。
