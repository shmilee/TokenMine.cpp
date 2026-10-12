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
   { "block": "diamond_ore", "probability": 0.05, "vein_size": 5, "max_y": 20 }
   ```

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
（block 64 / 工具 1 / 材料 64）。新增工具或材料 = `assets/data/items/<name>.json`：

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
  "smeltable_to": "cooked_x",       // 可选（Phase 40）：炉子产物物品名（须存在，init 交叉校验）；存在=可熔炼
  "smelt_time": 10.0,               // 可选（Phase 40）：单格熔炼秒数，(0,100]，默认 10（MC 炉子时长）；仅可熔炼时有意义
  "fuel_time": 80.0,                // 可选（Phase 40）：单格燃料燃烧秒数，(0,1000]；缺省=非燃料（显式 0 拒绝）
  "drink_time": 1.6,                // 可选（Phase 43）：饮用通道秒数，(0,10]；present=可饮用容器（与 restore_hunger 互斥）
  "returns_item": "bucket"          // 可选（Phase 43）：喝完返还的容器物品名（须存在，init 交叉校验）；要求 drink_time 存在
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
   可选 `interact`（Phase 17/40）：锁定时右键命中该方块打开对应面板而非
   放置——值必须来自已知交互集（`crafting_table` 工作台、`furnace` 炉子，
   其他值 = 启动报错）。
   校验严格：类型错误、越界值、重复 id、缺失纹理面都会报明确错误；未知字段警告但不拒绝。
   `drops` 是**物品名**（游戏启动时对 ItemRegistry 严格校验，未知名=启动报错）——
   block item 与方块同名，所以 `grass → "dirt"` 表示破坏 grass 掉 dirt；
   矿石掉材料物品（`iron_ore → "iron_ingot"`）。`drops_count`（1..64，默认 1）
   是单次破坏的掉落数量，须 ≤ 掉落物品的 max_stack（init 交叉校验）。
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
stick)、2×2 planks→crafting_table、wooden_pickaxe（PPP/.S./.S.）、
iron_pickaxe（III/.S./.S.）。
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
   `assets/data/textures/block_atlas.json`（atlas 满 8×8 格前直接加行；
   **网格扩容会移位所有既有 UV → 无输入基线门必须重锚**，见 PITS）。
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
   的 `tiles` 里给它一个空位 `[col, row]`（4×8 网格，现有 21 tile，勿动已有条目）
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
   （当前 48×16 = 3 tile）。加新效果 = 条带加宽一格（`tools/
   generate_effect_icons.py` 同款：确定性噪声 + 简单图元）；init 对
   每个效果的 `icon` 交叉校验 < tile 数，纹理宽度保持 16 的整数倍
2. `assets/data/effects/<id>.json`：

   ```json
   {
     "id": "slowness",
     "properties": {
       "melee_penalty": 0,
       "negative": true,
       "icon": 3
     }
   }
   ```

   字段（严格解析 = 注册表纪律，未知键拒绝）：`tick_type`（`"damage"`
   / `"heal"`；**缺省 = 无 tick** 的纯修饰效果，如上面的 slowness 骨架
   ——但注意 v1 核心只消费 `tick_*`/`min_health`/`melee_penalty` 语义，
   纯修饰效果目前只影响 HUD/存档/牛奶清除）+ `tick_amount`（每次 tick
   的 HP，(0,10]，有 tick 必填）+ `tick_interval`（秒，(0,100]，有 tick
   必填）+ `min_health`（**仅 damage**：0 = 无地板可致死 / 1 = 保持
   ≥1 HP 永不致死 = MC 中毒地板）+ `melee_penalty`（近战扣除攻击点，
   [0,100]）+ `negative`（bool，缺省 false；牛奶只清负面）+ `icon`
   （条带 tile 索引，必填）。**no-op 拒绝**：无 tick 且
   `melee_penalty` = 0 = 报错。id 非空且唯一
3. 来源（v1 两条）：
   - **mob 命中**：`assets/data/mobs/<id>.json` 加 `on_hit_effect`
     （见 Mob 章节）——近战命中施加
   - **存档注入**：`player.effects: [{"name": "<id>", "remaining": <秒>}]`
     （format 8；E2E 场景用）
4. 机制自动生效，无需改核心：真实 dt 倒计时 + 每跨 interval 结算一次
   （damage 走玩家受伤路径含 min_health 地板、heal 走 `Player::heal`
   clamp 上限）、HUD 图标行（按施加顺序）、F3 `effects` 行、牛奶清
   负面、死亡清空、存档 format 8 自动序列化/校验。语义细节见
   `docs/MECHANICS.md` 的「状态效果」。

当前 3 种：`poison`（damage 1 / 2.5 s、min_health 1、负面——spider
近战命中 15 s 毒源）、`weakness`（melee_penalty 4、负面——v1 无游戏内
来源）、`regeneration`（heal 1 / 4.5 s、正面——v1 无游戏内来源）。

## 新增一个流体桶（无需改核心代码，Phase 47）

v1 只有一种流体（**水** = 项目唯一模拟流体，流体名词汇表 = `"water"`，
其他名解析拒绝）。桶 = 一对物品（空桶 ⇄ 满桶），字段成对出现：

1. **空桶侧**（舀水 → 满桶）：

   ```json
   { "collects_fluid": "water", "collects_becomes": "<满桶物品名>" }
   ```

2. **满桶侧**（倒水 → 空桶）：

   ```json
   { "pours_fluid": "water", "pours_becomes": "<空桶物品名>" }
   ```

3. **对偶对称 = init 硬校验**：伴侣必须存在，且反向指回——同流体名、
   `*_becomes` == 本物品名（单边/错配 = 游戏启动失败，错误信息点名
   双方）。一个物品**只能是一侧**（`collects_fluid` 与 `pours_fluid`
   互斥），且桶侧**不能同时是食物/可饮用**（右键通道归属，解析拒绝）。
   `*_becomes` 缺 `*_fluid` = dead field，拒绝（dye 配对纪律）
4. **图标**：满桶物品要 `icon`（`tools/generate_item_icons.py` 加一个
   函数 + ITEMS 表项，如 `water_bucket` = 蓝色水面的桶）
5. 机制自动生效：舀 = 桶目标射线（首个「非 air 方块**或**带流体」的
   格，墙壁遮挡）命中 **level 8 源**格 → 移除源 + 换物品 + "splash"
   音；倒 = 放置目标格（受击面邻格）为 air 无流体且不重叠玩家 → 放
   level 8 源 + 换物品。倒下的源走正常 FluidSim（下落/扩散）。存档
   自动（物品按名 + 流体走 `fluids` 编辑，无 format 变更）
6. **舀的视线约束**（MC 行为，场景设计注意）：源格每 tick 向四向邻格
   补 level 7 水——**远处的玩家永远隔着自己源头的扩散水**（L7 ≠ 8，
   舀不到）；要舀源，玩家必须站在源旁 1 格、从**上方**斜视源的顶面
   （射线从源的上方格直接进入源格）。E2E 场景见 PITS「流体桶 E2E
   视线」

当前唯一对：`bucket`（max_stack 16）⇄ `water_bucket`（max_stack 1）。

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

运动/几何全套键（同样可覆盖，均 >0）：`width`（0.6）、`height`（1.8）、
`eye_height`（1.62，须 < `height`）、`walk_speed`（4.3）、`sprint_speed`（5.6）、
`jump_speed`（8.5）、`gravity`（32）、`terminal_velocity`（38）、`swim_speed`（3.1）、
`swim_up_speed`（4.5）、`water_gravity`（12）、`water_terminal_velocity`（4）、
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
./build/bin/minecraft --save myworld      # 首次：新游戏；此后：读档
./build/bin/minecraft --save myworld --new  # 推倒重来（退出时覆盖 myworld 槽）
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
- 洞穴：阈值 0.68 / scale 0.06 / minY 2 / 表面保护 4。
- 矿脉：lattice 8；coal .4/size9/maxY80, iron .25/8/56, gold .12/7/36, diamond .05/5/20。
- 树：lattice 4，树干 4+0..3，两层 r2 树冠 + r1 顶帽。
- 验证基线：默认 seed 123456789 → 128 chunks、spawn (0,0) surface y=53 forest；
  同 seed 截图逐字节一致（像素级复现已验证）。
