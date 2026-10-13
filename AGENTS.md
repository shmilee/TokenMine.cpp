# AGENTS.md — 代理快速接收

> Phase 30 文档整理产物。本文件是代理接手本项目的**第一入口**：读完本页即可开工，
> 细节按「文档地图」按需加载。所有规则原文见 `docs/PROTOCOL.md`。

## 项目一句话

可运行、可扩展的 Minecraft 风格 3D 体素沙盒：C++20 / SDL3 / OpenGL 3.3 core，
自研 `Result`/JSON/PNG，**内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，
不改核心代码），**确定性是一级要求**（无输入运行逐帧字节可复现）。

## 现状（Phase 55 完成时）

- 功能阶段 P0–P56 全部完成并提交；P56 = 金苹果后续：吸收层（用户
   "金苹果后续，为下一阶段。"，代理定案最小完整版 = 吸收层 HP 概念 +
   `grants_effects` 列表 + 金苹果再生+吸收，消费 P49 遗留"金苹果只有
   再生（无吸收）"）：**效果 `absorb_hp`**（整数 [1,64]，独立于
   tick/melee_penalty，no-op 规则 = 三者皆无才拒）+ **有效 max health**
   （`base + Σ层`；施加层 = health 提升增量【层显示为满】，层到期 =
   超出 base 的 health **无声掉下**【无红闪/伤害计时】；heal 可填进层；
   读档 = 层先于 health 生效【applySave 4d/4e 重排】+ 不重复提升）+
   **`Game::syncAbsorption` 六收口点**（进食/mob 命中/每帧到期/牛奶/
   死亡/读档）+ **物品 `grants_effects`**（严格配对数组，与
   `grants_effect` 互斥【P52 模式】，仅食物，init 逐条目校验）+ 内容：
   `absorption`（4 HP = MC 吸收 II、正面、icon 3）+ 金苹果改
   `[regeneration 10 s, absorption 5 s]` + effects.png 48→64（红心+
   白十字）+ **HUD 吸收红心**（正常心带左侧 `ceil(层/2)` 颗红心，无层
   零绘制 = 结构性免疫）；**无 format 变更**（format 9；health 24 + 层
   读档合法）；982/982（+15）、`-Werror` 0 警告、base18 **第 30 次结构
   性免疫**【无输入零效果 = 红心带零绘制】；E2E 全绿（S2 金苹果吃
   1200f = fps 无关终态【19.4..46 s > 再生到期 14.5 s】slot0 空 + 饥饿
   16 + health 20 + 效果 []；S3 层激活 health 24 合法读档 + 红心带像素
   存在；S4 到期钳制 24→20 字节 ×2；S5 不过扣 18→18 字节 ×2；S6 init
   失败 ×3；savenorm 新增剥 saturation = 真实 dt 字段）；
   P55 = 多掉落内容扩展（用户"熔岩湖
   不着急，先记录下来，留到很久的未来。下一步先进行建议2的扩展。" =
   建议 2，代理定案最小完整版 = 3 方块 MC 语义内容扩展、**零代码**，
   消费 P52 遗留"多掉落内容扩展"；worldgen 熔岩湖 = 用户明示"留到很久
   的未来"）：**`leaves`**（单条目 `chance_drop` →
   `chance_drops: [stick×1 @ 0.02, apple×1 @ 1/128]` = MC 1.14+ 树叶掉
   木棍 2% + 苹果；无 sapling = 文档化偏离，苹果保留 P49 项目值）+
   **`lapis_ore` / `glowstone`**（`drops_count: 2` + 两个 50% 条目 =
   2..4 均匀 = MC 同款数量分布）+ **3 个内容钉死单测**（加载真实
   `assets/data/blocks` 断言字段精确值——防概率笔误，如 0.2 vs 0.02）；
   **无 format 变更**（format 9）；967/967（+3）、`-Werror` 0 警告、
   base18 **第 29 次结构性免疫**【掉落只作用于破坏，fresh 世界无输入
   零破坏 = 无输入零活动】；E2E 复用 P52 已验证瞄准几何（p55scan 真实
   raycast 复验 3 目标 HIT + 真实 roll 预计算：lapis×3 / glowstone×3 /
   leaves×1+stick×1；铁镐按住破坏；归一化存档 ×2 + 精确掉落断言 +
   无输入对照 ×2 全过）；P54 = 熔岩后续（用户"好的.按建议
   继续。" = 建议 1，代理定案最小完整版 = 熔岩游泳状态 + 灼烧伤害 +
   下沉减速 + 熔岩不湿润耕地，消费 P51 遗留"熔岩无燃烧/减速" + "湿润
   判定把熔岩也算水"，worldgen 熔岩湖/脱离残留火/熔岩烧 Mob 与作物
   留后续）：**熔岩 = 同款游泳状态 + 熔岩节奏 + 灼烧**（`isInLava`
   AABB 扫描，水/熔岩并存时熔岩优先；`lava_swim_speed` 1.9 / 上浮 1.5 /
   重力 4 / 终端 2，config 可覆盖，比值对齐 MC 运动系数比
   0.014/0.0231）+ **灼烧伤害**（真实 dt 生存计时器：每
   `lava_damage_interval_seconds` 1.0 s 受 `lava_damage` 4 HP = MC
   4 HP/s，走正常伤害路径；离熔岩重置未到期 tick = v1 无残留火）+ 落入
   熔岩取消坠落伤害 + 熔岩内抑制疾跑/下蹲 + **`cropIsWet` 只算水**
   （熔岩旁耕地 = 干节奏）；6 个新 `player.*` config 键（默认 = MC 值，
   `lava_damage` ≤ `max_health` 交叉校验）；**无 format 变更**（format
   9）；964/964（+10：物理 7 + config 2 + growth 1）、`-Werror` 0 警告、
   base18 **第 28 次结构性免疫**【熔岩只存在于场景（桶/放置），fresh
   世界零熔岩零作物 = 无输入零活动】；E2E 场景池 = seed-9 天然 2×1 空穴
   + 3 石块编辑围墙【存档禁止同格 block+fluid 编辑，PITS 已收录】；
   P53 = 作物放置限制（用户"按你建议
   继续：候选1"，代理定案最小完整版 = `plant_on` 数据驱动放置土壤门，
   消费 P48/P50 遗留"放置不受限"，熔岩后续/多掉落内容扩展留后续）：
   **`plant_on` 纯数据**（block 可选**数组**（方块名）——放置时目标格
   **正下方**须为列表之一；缺省 = 不限 = 通用 placeBlock 规则；空数组
   no-op 拒绝【ammo 纪律】、逐条目点名 `plant_on[i]` 拒绝）+ **门 = 纯
   函数 `canPlantAt`**（`world/raycast`，`canPlaceAt` 同款可单测；下方
   越界读 air → 拒）+ `placeBlock` 在 canPlaceAt 后加门（土壤不符 =
   静默 no-op，不消耗物品）+ **init 每条目存在性校验**（grows_on 纪律）
   + **`plant_on`（放置门）与 `grows_on`（生长门）相互独立**（本项目同值
   farmland = Minecraft 耕地约束落地）+ 内容：`wheat_1..6` 加
   `plant_on: ["farmland"]`。**无 format 变更**（format 9）；954/954
   （+12：block 解析 6 + canPlantAt 6）、`-Werror` 0 警告、base18 **第
   27 次结构性免疫**【plant_on 只影响放置，fresh 世界零放置 = 无输入零
   活动】；E2E 场景选位 = p53scan 探针调真实 `cropGrowthSteps` 预计算
   【选 [0,240) 无生长通过帧的格 = fps 无关断言，见 PITS P53 条】；
   P52 = 加权多掉落列表（用户"按候选顺序
   继续后面两个 phase" = 候选 2，代理定案最小完整版 = `chance_drops`
   多条目独立 roll + grass 改 MC 语义，放置限制/熔岩后续留后续）：
   **`chance_drops` 纯数据**（block 可选数组，元素 = 与 P49 单条目
   `chance_drop` **相同的严格三元组** `{item,count,chance}`；与
   `chance_drop` **互斥**【同时写 = 解析拒绝】；空数组/非对象元素/未知键/
   缺三元组/越界逐条目点名 `chance_drops[i]` 拒绝）+ **每条目独立
   roll**（`rollChanceDropEntry(seed,x,y,z,entryIndex,chance)` 纯函数：
   列表专用 base salt + 条目索引 golden-ratio 推进 + splitmix64 finalize，
   同格各条目互不追踪、也不追踪单条目 roll；per-cell 确定性 = P49 同款
   刻意代价）+ breakBlock 循环条目、每条目至多 1 掉落实体 + init 每条目
   复用 drops 纪律（物品存在 + count ≤ max_stack，点名 `chance_drops[i]`）
   + 内容：`grass` 改 **MC 语义**（`drops: wheat_1` → `drops: dirt` +
   `chance_drops: [wheat_1 ×1 @ 0.125]` = 草掉泥土 + 偶尔种子，0.125 =
   MC 基础概率；种子来源 = 草概率掉 + 未熟作物掉落）；`leaves` 保留单条目
   `chance_drop` = 两字段并存示范。**无 format 变更**（format 9，掉落 =
   实体走既有 `entities`）；942/942（+21：block 解析 13 + entry roll 8）、
   `-Werror` 0 警告、base18 **第 26 次结构性免疫**【grass drops 变数组不改
   worldgen/atlas = 无输入零活动】；
   P51 = 熔岩流体系统（用户"按候选顺序继续后面两个 phase" = 候选 1，
   代理定案最小完整版 = 熔岩第二流体种类 + 水/熔岩反应 + 熔岩桶 + format
   9，加权多掉落留 P52）：chunk 流体字节双种类（0=干 / 1..8=水 / 9..16=
   熔岩）+ FluidSim 反应 pass（水/熔岩 6-邻 → obsidian/stone）+ 桶双种类
   化 + format 9 + lava/obsidian/lava_bucket 内容；921/921（+12）、第 25 次
   结构性免疫；
   P50 = 耕地·水·光照生长条件（用户"按建议继续下两个阶段" = 建议 2，
   代理定案最小完整版 = `farmland` 方块【dirt 合成】+ 生长引擎三道数据
   驱动环境门，加权多掉落/熔岩/放置限制留后续）：**作物侧三个可选环境
   字段**（缺省 = 无要求 = 旧行为）
   ——`grows_on`（作物**下方一格**须为该土壤，空 = 无要求，init 硬校验
   指向存在方块 = grows_to 纪律）/ `growth_light`（作物格
   max(天空光,方块光) 最低值 [0,15]，0 = 无要求）/ `dry_slowdown`
   （无水时 roll 模数倍数 [1,64]，1 = 无减速）+ 湿润判定（土壤格同 y
   切比雪夫半径 4 内任一流体 level ≥1；模数 = `growth_ticks` × (湿 1 /
   干 `dry_slowdown`)，干时通过帧 = 湿时严格子集）+ 内容：`farmland`
   （solid cube、hardness 0.5、drops 回 `dirt`、顶面犁地垄纹）+ 配方
   `farmland`（shapeless 1 dirt）+ `wheat_1..6` 加 farmland/9/2（MC 值）；
   **无 format 变更**（format 8）；
   P49 = 金苹果（概率掉落 + 吃时给效果；
   用户"按建议继续下两个阶段" = 建议 1，代理定案最小完整版 = 概率次级
   掉落系统 + 苹果 + 金苹果，耕地/多掉落留后续）：**概率次级掉落**
   （block `chance_drop` `{item,count,chance}` 严格三元组，与 `drops`
   独立【一次破坏最多两个掉落实体】；是否掉出 = `rollChanceDrop(seed,
   x,y,z,chance)` 确定性纯函数 `block/chance_drop`——位置哈希 + 专用 salt，
   同世界同格每次破坏同果）+ **吃时给效果**（item `grants_effect`
   `{name,duration}` 严格配对，**仅食物**【可饮用清效果、通道互斥】；
   进食通道完整吃下时经 P46 `StatusEffects::apply` 施加，与 mob
   `on_hit_effect` 同路径）+ init 双硬校验（chance_drop 物品存在 +
   max_stack / grants_effect 效果名注册）+ 内容：`leaves` 1/128 掉
   `apple` + `apple`（`restore_hunger` 4 = MC）+ `golden_apple`
   （`restore_hunger` 4 + 吃给 regeneration 10 s）+ 配方
   `golden_apple`（shaped GGG/GAG/GGG，MC 镶金）+ 2 图标；**无 format
   变更**（format 8：实体走既有 `entities`、效果走既有 `player.effects`）；
   P48 = 面包（小麦种植系统 v1；用户
   "继续建议1" = 第一候选，代理定案最小完整版 = 作物 + 生长引擎 + 收获 +
   面包配方/食物，金苹果/熔岩留后续）：**生长纯数据**（block `grows_to`
   下一阶 + `growth_ticks` 平均 tick 进一阶，严格配对 + init 硬校验指向
   存在方块；**生长状态 = 方块 id 本身**，无 per-instance 状态/存档字段）
   + 生长引擎（`world/growth.{hpp,cpp}` 纯函数 + `Game::m_crops` map
   位置序，setBlock 收口维护 + 读档后 `rescanCrops` 全量重建；每帧 1/60
   固定步长确定性哈希 roll）+ 内容：7 方块 `wheat_1`→…→`wheat_7`
   （cross，未熟掉种子可再种 / 成熟掉 `wheat`×2）+ `wheat`/`bread`
   （`restore_hunger` 5）+ 配方 `bread`（shapeless 3 wheat，同物品合并
   count）+ 7 atlas tile（空位无 UV 移位）+ 2 图标 + 7 cross 纹理 +
   **grass drops 改 `wheat_1`**（v1 种子来源）；**无 format 变更**
   （format 8，作物 = 方块编辑）；P47 = 水桶（流体桶 v1；用户"按排序
  继续"= 第一候选，代理定案最小完整版 = 水，熔岩/面包/金苹果留后续）：
  **桶对纯数据**（item `collects_fluid`+`collects_becomes` 空桶侧 /
  `pours_fluid`+`pours_becomes` 满桶侧；流体名 v1 词汇表 = 仅 "water"；
  两侧互斥 + 与食物/可饮用互斥；**init 对偶对称硬校验** = 伴侣存在且
  反向指回）+ `raycastFluid`（桶目标射线：首个「非 air 方块或带流体」
  格，**墙壁遮挡** = MC；与 raycastBlock 共享 DDA 骨架）+
  `tryFluidBucket`（舀 = 命中 **level 8 源** → 移除源 + 换物品 +
  "splash"；倒 = 放置目标格 air 无流体不重叠玩家 → 放 L8 源 + 换物品；
  右键顺序在 interact 之后 = 手持桶对炉子仍开面板；倒下的源走正常
  FluidSim）+ 内容：`water_bucket` + 蓝水面桶图标 + `splash.wav`；
  **无 format 变更**（format 8）；P46 = 状态效果系统（代理自定案范围：
  poison/weakness/regeneration 数据驱动效果 + **spider** 毒源 + 效果图标
  HUD + 牛奶清负面补全 + 存档 format 8）：**效果纯数据**（`assets/data/
  effects/*.json` 严格解析 + no-op 拒绝；真实 dt 倒计时 + 跨 interval
  tick、min_health 地板、melee_penalty、negative 标志）+ **spider**
  （首个 `on_hit_effect` mob：近战命中施加 poison 15 s、只在黑暗、
  无掉落）+ 效果图标行（心行左对齐 x0=434、仅 locked && !panelOpen、
  无效果不绘制 = 结构性免疫）+ 牛奶 = 清红闪 + **清全部负面效果**
  （正面 regen 保留 = MC 语义）+ format 8（`player.effects` 名字序、
  tick 相位不保存、`parseSave` 加 `EffectRegistry` 第 4 参）；P45 =
  染色补全 + 十字植物渲染（P41/P12/
  P20/P23 遗留"无蓝/绿/紫来源"）：**颜色词汇表纯数据化**（dye item
  `dye_color` 命名，加颜色**零代码**改动）+ 补 6 色 blue/green/purple/
  magenta/light_blue/cyan（项目 silver 位 = MC light_gray → **15 染料全**）
  + `BlockModel::Shape::Cross`（2 交叉竖直面片，透明 pass 双面、无邻居
  剔除、自身格光、面 shade 1.0）+ biome `decorations` 表面装饰（树的格网
  + 专用 salt + 每(格,条目)独立 roll，落表面格正上方、洞穴啃掉不放、
  树干让位）+ 内容全 JSON：5 方块（`lapis_ore`/`cactus`/`allium`/`tulip`/
  `cornflower`）+ `lapis` 物品 + 6 染料 + 6 wool 变体 + 6 配方（青←lapis+绿
  MC 1.14+）+ 11 atlas tile（空位，无 UV 移位）+ lapis 矿脉；P44 =
  per-block 炉子状态（P40 遗留"单个全局 FurnaceState"）：`Game` 每炉子块
  独立 `FurnaceState`（`std::map<BlockPos, FurnaceState>` 位置序）+
  setBlock 收口创建/删除 + 面板绑定所点方块 + 存档 format 7（`furnaces`
  双向交叉校验 + format 6 迁移：sole 归属/其余丢弃）；P40 = 炉子/熔炼
  （P28 遗留"熟肉"）、P41 = wool 染色框架（P12/P20 遗留"wool 无用途/无
  染色"；atlas 4×8→8×8 → base18 于 P41 重锚）、P42 = 弓后续 2、P43 =
  牛奶桶最小项（饮用通道 + 挤奶）。
- 测试 **982/982**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- 无输入基线门：`/tmp/opencode/base18.png`（字节一致；P41 重锚【atlas
  8×8】，P42/P43/P44/P45/P46/P47/P48/P49/P50/P51/P52/P53/P54/P55/P56
  过门 = 第 16/17/18/19/20/21/22/23/24/25/26/27/28/29/30 次结构性免疫、
  无重锚）。
- 活跃已知问题：`docs/KNOWN_ISSUES.md`；下一阶段候选：`SESSION_CONTEXT.md`「下一步」。

## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告）
./build/bin/minecraft_tests                                                 # 测试（964）
./build/bin/minecraft                                                       # 运行（ESC 退出）
./build/bin/minecraft --frames N [--screenshot /abs.png] [--save slot] [--new] [--config path]
```

游戏必须从仓库 cwd 启动（资源相对路径）；独立测试程序链接
`build/lib/libmc_core.a -lSDL3 -lGLEW -lGL -lz -lpthread`。

## 硬性规则 Top 10（违反即 bug；全文 docs/PROTOCOL.md）

1. **坐标**：world/chunk/local 严格分离，换算只在 `coordinates.hpp`；负坐标 `floorDiv`。
2. **渲染隔离**：游戏逻辑不碰 GL/SDL（renderer/ 与 window/input 边界除外）。
3. **数据驱动**：核心不 switch 方块 id（读 properties）；air 是代码哨兵（无 air.json）。
4. **错误与代码质量**：`Result<T>` + `MC_TRY` 显式传播；**无 TODO/伪代码/魔法数字/
   硬编码方块检查/删功能过构建/禁用测试**。
5. **相机**：yaw 0 = 朝 -Z、正 = 左转；pitch 正 = 上、clamp ±~89°；**鼠标转向仅在
   指针锁定时生效**（无输入运行字节复现的保证）。
6. **玩家物理**：position = AABB 底面中心；重叠判定**严格不等式**（无 epsilon）；
   防穿透 = 子步化（每子步 ≤0.4 块、≤64 子步）+ 终端速度 38。
7. **chunk streaming**：worker job 只在 **post→flush 窗口**运行（窗口内主线程只读
   World）；**光照 job 每帧至多 1 个**（相邻列区域重叠 ±16）。
8. **确定性**：昼夜/Mob/箭/粒子 = 固定 1/60 帧步长；生存计时器（饥饿/进食/破坏/
   坠落/实体 age）= 真实 dt；截图只在帧边界字节一致。
9. **面表绕序**有回归测试守护（`FaceWindingIsCounterClockwiseFromOutside`），勿改
   顶点序；光照 = `max(sky,block)/15 × faceShade`（顶 1.0/±X 0.8/±Z 0.6/底 0.5）。
10. **出生点**：4×4 chunk 演示区列，按 `kSpawnCandidates` 自动找干燥陆地
    （h > seaLevel 且出生 AABB 内无 solid）。

## E2E 协议（摘要；全文 docs/PROTOCOL.md「E2E 协议」）

- **零并发 :0 活动**（用户手离开鼠标）+ 每拍截图前 `unmap_covers`。
- **无输入运行**：×2 全帧字节一致 + skygate（FRESH 模式色）+ base 门
  `/tmp/opencode/base18.png` 字节一致（新系统需论证"无输入下零活动"= 结构性免疫）。
- **输入驱动运行（xdotool 脚本）**：**存档状态（含 yaw/pitch）是确定性权威证据**——
  全帧字节一致受 SDL 指针锁定 grab delta 环境限制（一次性相机旋转 ±0.01 rad = 4 px；
  缓解 = warp 实际原点 + (632,352)、同批同 warp、手不动；存档 yaw/pitch = 漂移探测器）。
- 输入锚定窗口发现时刻，D ∈ [0.1, 0.5] s；tick 类断言需 ≥1.6 s 裕量（1/60 双累加
  边界：36 s 饥饿 tick = 帧 2161，20 s 饥饿伤害 = 帧 1200，1.6 s 进食通道完成 = 帧 97）。
- **HUD 红心检查**（心带 y[646,661] x[551,729]）：无输入运行 = 0（>50 = 污染废拍）；
  输入运行 >50（输入生效）。

## 阶段流程（每个 phase 按此顺序）

1. 用户确认方向 → 2. 实现（数据驱动、无 TODO）→ 3. 构建 + 全量测试（`-Werror`）→
4. E2E（base 门 + 回归对 + 场景专属验证，按上面协议）→ 5. 文档（ROADMAP 行、
KNOWN_ISSUES、SESSION_CONTEXT）→ 6. **显式路径列表** git commit（永不 `git add -A`；
永不提交 `SESSION_CONTEXT.md`（本地））→
7. 中文阶段报告（标准结构：实现内容/变更文件/测试/构建与验证结果/已知问题/下一步建议）。

## 坑位 Top 8（全文 docs/PITS.md，~98 条）

1. **SDL grab delta**（P28）：输入 E2E 全帧字节一致不保证——存档 yaw/pitch 为漂移
   探测器；驱动 warp 用实际窗口原点（实测 (4,64)）+ (632,352)。
2. **物理光标**（P5）：指针未锁定时 SDL 仍投递 mouse motion → 无输入 E2E 必须零
   :0 活动，红心检查 = 污染哨兵。
3. **粒子视觉验证视角**（P29）：直下视角可视锥不含破坏粒子——必须斜视角（pitch
   −1.02 级）；帧差分析先排除已知 UI 变量（饥饿/HP 图标也在 diff 里）。
4. **splitmix64 是纯 finalizer**（P29）：不推进 state——推进式流必须
   `state += 0x9E3779B97F4A7C15ULL` 后再 mix（同 burst 同值缺陷根因）。
5. **`std::string::replace(pos,len,const char*,n)` 截断**（P28）：只取替换串前 n 字符。
6. **`--save slot` 第二次运行会加载第一次写的存档**（P15）：E2E 对跑每拍从源副本
   `cp` 新鲜存档。
7. **按住破坏会自动续破下一块**（P15）：输入脚本的释放时机 = 破坏完成 + 余量，
   余量内的按住对下一块产生确定性裂纹进度。
8. **像素验证**：`glReadPixels` 必须在 `swapBuffers()` 后（前缓冲）；
   `magick ... txt:` 输出 `(r,g,b,a)` 开头 `(` 破坏数值转换，awk 取 `$2`。

## 禁止事项

- `config/config.json` 是**已提交文件**（P32 起：用户调值 view_distance 80 /
  day_length 600 + `player` 默认节）——**不还原用户的调值**；内容改动仅在用户
  要求时做，提交用显式路径。
- **永不 `git add -A`**——提交用显式路径列表。
- `SESSION_CONTEXT.md` 本地专用（`.git/info/exclude`），永不提交。
- 无 TODO/伪代码/硬编码检查/魔法数字；新内容一律走数据文件 + 注册表严格校验。

## 文档地图

| 文档 | 内容 | 何时读 |
|---|---|---|
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（截至 P28：生存/Mob/弓/合成/UI/世界生成） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ 存档使用 + worldgen 参数 | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（~98 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃）+ 已修复存档索引 | 阶段验收前后 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phase 0–29 规划 + 细节存档索引 | 了解历史 |
| [docs/archive/phase-NN.md](docs/archive/) | 单阶段细节存档（阶段报告 + 会话存档） | 深挖某阶段 |
| `SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步 | 任何"继续"会话 |
