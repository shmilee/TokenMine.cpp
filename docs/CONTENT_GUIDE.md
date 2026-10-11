# 内容添加指南（数据驱动）与存档使用

> Phase 30 文档整理：从 `README.md` 移入（原「新增一个生物群系/矿石」「调整初始
> hotbar」「新增方块/配方/Mob」「使用存档」小节组，未改写），并并入原
> `SESSION_CONTEXT.md`「Phase 4 关键参数」（worldgen.json 参考，文末）。文中
> `KNOWN_ISSUES` 均指 `docs/KNOWN_ISSUES.md`。

## 新增一个生物群系 / 矿石（无需改核心代码）

1. 生物群系：`assets/data/biomes/<name>.json` —— 权重/表层/次表层/深度/树密度/高度系数：

   ```json
   {
     "id": "swamp",
     "weight": 3,
     "surface": "grass",
     "subsurface": "dirt",
     "subsurface_depth": 3,
     "tree_density": 0.08,
     "height_scale": 0.8
   }
   ```

2. 矿石：`assets/data/worldgen.json` 的 `ores.veins` 数组加一项（方块需已注册）：

   ```json
   { "block": "diamond_ore", "probability": 0.05, "vein_size": 5, "max_y": 20 }
   ```

地形/洞穴/树木参数同样在 `assets/data/worldgen.json`（缺省=默认值，类型错误=启动报错）。

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
  "fuel_time": 80.0                 // 可选（Phase 40）：单格燃料燃烧秒数，(0,1000]；缺省=非燃料（显式 0 拒绝）
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
   缩放，非布尔拒绝）、`drops` 每条目：`count` ∈ [1,64] 或
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

v1 数据有 zombie（敌对：绿色、只在黑暗生成（`spawn_max_light` 0））与
cow（被动：棕白斑毛、`detect_radius`/`attack_range`/`attack_damage` 全 0、
`spawn_max_light` 15 任何时间可生成、掉 `leather`）；两者都不游泳不拆方块
无动画——见 docs/KNOWN_ISSUES.md 的 Mob 章节。

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
