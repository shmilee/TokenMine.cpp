# Phase 51 细节存档

> 范围（用户"按候选顺序继续后面两个 phase" = 候选 1，代理定案为最小
> 完整版：**熔岩流体系统** = 熔岩第二流体种类 + 水/熔岩反应 + 熔岩桶 +
> 存档 format 9；加权多掉落列表留 P52）：
> ① chunk 流体字节双种类编码（0=干 / 1..8=水 / 9..16=熔岩）+
>    `FluidKind{Water,Lava}`；
> ② FluidSim tick = 反应 pass（熔岩 6-邻有水 → `water_reaction` 产物，
>    邻水含 L8 源 = 黑曜石 / 流动水 = 石头）→ 水模拟 → 熔岩模拟（每
>    种类自含快照）；
> ③ 桶双种类化（`collects_fluid` 名称数组、`collects_becomes` 废除 =
>    满桶侧推导、init 对偶恰 1 校验）+ 流体块 light_emission（lava 15
>    发光池）+ 存档 format 9（流体编辑带种类名）；
> ④ 内容：`lava` / `obsidian` / `lava_bucket` + atlas 空位 + 纹理。

## 阶段报告

- **`[2026-10-13, Phase 51]`** 熔岩流体系统
  — 921/921（+12），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 25 次
  结构性免疫免重锚 + S2 三室场景 2400f 无输入【A 反应=黑曜石+水膜 /
  B 竞速=水挡熔岩 / C 纯熔岩池，全部 = C++ 探针稳态 ground truth】+
  S3a 舀熔岩 / S3b 倒熔岩 输入运行存档精确 ×2 字节一致 + S4 init 失败
  ×3 干净报错）。

### 实现内容

**chunk 流体编码（`src/world/chunk.hpp`）**
- `m_fluid` 字节：0=干、1..8=水 level、**9..16=熔岩 level**
  （`kMaxFluidLevel`=8 为界）。`FluidKind{Water=1, Lava=2}` + 编码
  helper（`fluidLevelOfKind` / `encodeFluidLevel` / `fluidKindOfLevel`）
  内联。一格至多一种流体：`setFluid(kind, 正 level)` 替换异种；
  `setFluid(kind, 0)` 只清本种（本种不在 = no-op，不误清异种）。
- `getFluid` 返回 0..16 编码；`getFluidOfKind(x,y,z,kind)` 返回本种
  level（0..8）——光照/湿润/桶判定按种类取。

**流体模拟（`src/world/fluid.{hpp,cpp}`）**
- `FluidSim::tick(world, blocks)` 三段：
  1. **反应 pass**：快照扫全部熔岩格，6-邻（同 y 四向 + 上下）任一格
     带水 → `world.setBlock` 为 lava 块的 `water_reaction` 产物：邻水
     含 **L8 源 → `.source`（obsidian）** / 仅流动水 → `.flowing`
     （stone）；**水不消耗、无蒸汽**（v1 文档化偏离，MC 有蒸汽）。
  2. 水模拟（`simulateType(Water)`）→ 3. 熔岩模拟
     （`simulateType(Lava)`）。
- **每个 simulateType 自含快照**：扩散读的是本种类开始前的快照，异种
  流体格**当方块**（不扩散进）→ 先到的种类挡后到的；水先 apply → 熔岩
  快照已含水格 → 稳态不变量 = **无 6-邻接水-熔岩对**（反应 pass 收敛
  保证；26-邻对角接触允许）。
- 熔岩扩散规则与水同款（源 L8、每层 -1、平流、下落优先），参数纯数据
  （无独立 lava 速度字段 = v1）。

**反应表 + 光照（数据驱动）**
- `BlockProperties::waterReaction {source, flowing}` 严格配对（缺一
  拒绝 / 显式空串拒绝 / 未知键拒绝），两侧指向的块须存在（init 交叉
  校验点名）。lava 块写 `{source: obsidian, flowing: stone}`（MC 语义）；
  无 water_reaction 的流体 = 惰性（不反应）。
- **流体块 light_emission**：`lava` = 15（发光池）。光照 step4 对
  "方块或带流体" 格按参考块 emission 播种（流体格参考 = 该种类的流体
  块）；`Lighting::updateLocal/updateRegion` 重播种路径同款双种类化。

**桶（`src/core/game.cpp` `tryFluidBucket` 双种类化）**
- item 数据：`collects_fluid` = **流体名数组**（v1 词汇表
  `"water"`/`"lava"`，init 对未知名拒绝）、`collects_becomes` **废除**
  （旧字段出现 = 拒绝）；满桶侧 `pours_fluid`/`pours_becomes` 不变。
  **init 对偶校验**（双种类化）：每个点名流体**恰 1 个**满桶
  （`pours_fluid` = 该名 且 `pours_becomes` = 空桶名）+ 反向指回对称。
  `ItemRegistry::fullBucketFor(fluid, empty)` = 由数据推导满桶
  （舀时换入的物品）。
- 舀：raycastFluid（首个「非 air 方块或带流体」格，**墙壁遮挡**）命中
  **level 8 源** 且该格流体种类 ∈ 桶点名集合 → 移除源 + 手持 1 换
  `fullBucketFor(该种类, 本物品名)` + "splash"。
- 倒：raycastBlock 首个非 air 方块受击面邻格 = air、无流体、不重叠玩家
  → 放 L8 源（物品 `pours_fluid` 种类）+ 换 `pours_becomes`。
- 日志：`"scooped <kind> at (x, y, z) (a -> b)"` /
  `"poured <kind> at (x, y, z) (a -> b)"`。

**存档 format 9（`src/core/save.{hpp,cpp}`）**
- `kSaveFormat = 9`。`fluids` 编辑 5 元素 `[x, y, z, level, kindName]`
  （kindName ∈ `"water"`/`"lava"`，名字对词汇表校验）。
- **format<9 迁移**：4 元素编辑（P47 及以前全是水）全迁 water。
- 写档/读档双向；顶层键白名单不变（`fluids` 既有）。

**其他**
- `Player::isInWater()` 仅水（熔岩不触发游泳 = v1，无燃烧/下沉）。
- 生长引擎湿润判定（P50）任一流体 level ≥1 = 含熔岩（熔岩池旁的耕地
  算湿 = v1 语义，KNOWN_ISSUES 记录）。
- Mesher 按 kind 剔除流体面（水/熔岩互不为"空气邻居"）。

**内容（全 JSON + 纹理）**
- `lava`（fluid/lava、`light_emission` 15、`water_reaction`
  {obsidian, stone}、unbreakable【hardness 极大 + drops 空】）、
  `obsidian`（solid cube、hardness 10 = MC、drops 自身）、
  `lava_bucket`（material、max_stack 1、`pours_fluid` lava、
  `pours_becomes` bucket）。
- `bucket.json`：`collects_fluid ["water","lava"]`（删
  `collects_becomes`）。
- atlas 空位：lava [7,6]、obsidian [7,7]（无 UV 移位）。**曾与
  wool_blue 撞 [4,7]**（P50 版 atlas 空位对算错）→ E2E 截图兜出
  （池面颜色污染）→ 移空位（PITS：atlas 落位先对现有 tile 表核）。
- 图标：`lava_bucket.png`（橙水面桶，generate_item_icons 新增）；
  方块纹理：`lava.png`（橙红波纹）/ `obsidian.png`（近黑紫晶点）。

### 变更文件

- `src/world/chunk.hpp`（编码 + FluidKind + helper）
- `src/world/fluid.{hpp,cpp}`（反应 pass + 双种类 simulate + 参数）
- `src/world/world.{hpp,cpp}`（setFluid/getFluid 双种类 API）
- `src/world/lighting.cpp`（流体格 emission 播种）
- `src/world/chunk_mesher.{hpp,cpp}`（按 kind 剔除）
- `src/world/world_generator.cpp`（无 worldgen 熔岩；湿润引用适配）
- `src/block/block.{hpp,cpp 注册}` / `block_registry.{hpp,cpp}`
  （water_reaction 解析 + init 校验）
- `src/item/item.{hpp}` / `item_registry.{hpp,cpp}`（collects_fluid
  数组 + fullBucketFor + 对偶校验）
- `src/core/game.{cpp}`（tryFluidBucket 双种类 + init 交叉校验 +
  日志）、`src/core/save.{hpp,cpp}`（format 9）
- `src/player/player.cpp`（isInWater 仅水）
- `assets/data/blocks/{lava,obsidian}.json`（新）、
  `assets/data/blocks/water.json`（water_reaction 无 = 惰性标注）、
  `assets/data/items/{lava_bucket}.json`（新）、
  `assets/data/items/bucket.json`（collects_fluid 数组）
- `assets/data/textures/block_atlas.json`（lava [7,6] / obsidian [7,7]）
- `assets/textures/blocks/{lava,obsidian}.png`（新）、
  `assets/textures/items/lava_bucket.png`（新）
- `tools/generate_block_textures.py` / `tools/generate_item_icons.py`
- `tests/test_{block,chunk_mesher,fluid,growth,item,lighting,mob,
  player,raycast,save,world}.cpp`（+12 测试 + 既有适配）
- 文档：ARCHITECTURE / CONTENT_GUIDE / GAMEPLAY / KNOWN_ISSUES /
  MECHANICS / PITS / ROADMAP / AGENTS / 本文件

### 测试

921/921（+12）：
- fluid 4：双种类编码往返 / setFluid 替换异种 / 清本种 no-op 不误清
  异种 / 反应 pass 邻水含源 → obsidian（流动水 → stone，水不消耗）。
- block 2：water_reaction 严格配对解析（缺一拒 / 显式空拒）。
- item 2：collects_fluid 数组解析 + fullBucketFor 推导。
- save 2：format 9 流体编辑双向 + format<9 迁移 water。
- lighting 1：lava 流体格 emission 15 播种。
- chunk_mesher 1：异种流体面不互剔。
（+ 既有 fluid/growth/world 测试的编码适配。）

### 构建与验证结果

- `cmake --build build`（Release）：`-Wall -Wextra -Wpedantic -Werror`
  **0 警告**。
- **E2E（批跑 `/tmp/opencode/p51_e2e.zsh`，全绿）**：
  - **S1 base18 门**：P51 ×2 新世界 60f 无输入字节一致 + **== base18
    → 第 25 次结构性免疫（免重锚）**【worldgen 未动、atlas 落空位、
    熔岩仅存档编辑可见】+ skygate + 红心 0。
  - **S2 三室场景**（2400f 正午 still mobs off 无输入；房间 x 21..25：
    A z −5..−1【lava (23,46,−3) + water (24,46,−3)】/ B z −12..−8【lava
    (22,46,−10) + water (24,46,−10)】/ C z −20..−16【lava (23/24,46,−18)】
    + 塔 (27,45..47,−22) 相机 [27.5,48,−21.5] yaw 2.2143 pitch −0.5536）：
    存档 ×2 **全字节一致（确定性权威）** + 分室断言 = 探针
    `p51scenesim` 稳态 ground truth——A = obsidian (23,46,−3) + 8 格水
    【(24,−3)=8 (24,−4)=7 (24,−2)=7 (23,−4)=6 (23,−2)=6 (22,−4)=5
    (22,−3)=4 (22,−2)=5】/ B = 3 石 (22,−10/−9/−11) + 6 格水
    【(23,46,−9) **留水 6**：水先按种快照落位挡熔岩】/ C = 9 格熔岩池
    【(23/24,−18)=8 四邻 7 角 6】+ hunger ≥18（本机 ~33fps → 2400f
    墙钟 91s > 72s = 2 次 36s 衰减，×2 同桶）+ 截图橙像素 20,862 +
    skygate + 红心 0。
  - **S3a 舀熔岩**（240f 输入；玩家 (26.5,46,−17.9) 站支撑石
    (26,45,−18)，yaw 1.5708 / pitch −0.38 → 射线在墙列 x=25 之前下穿
    y=47 → 命中源 (24,46,−18) L8；slot0 bucket×1）：ctl ×2 无输入
    存档一致（源存续 L8 + slot0 未动）；输入 ×2 **字节一致** + 存档
    精确（slot0 = **lava_bucket**、(24,46,−18) = 7【邻源回填】）+
    日志 "scooped lava at (24, 46, -18) (bucket -> lava_bucket)"。
  - **S3b 倒熔岩**（同范式；**瞄支撑块 (28,46,−18) 西面面心**
    (28,46.5,−17.5)：yaw −1.831399 / pitch −0.624981；slot0
    lava_bucket×1）：ctl ×2 无倒；输入 ×2 **字节一致** + 存档精确
    （slot0 = **bucket**、**13 格 y=43 池** = 探针 ground truth 逐格
    level：(27,−18)=8 (27,−19)=7 (26,−18)=7 (27,−20)=6 (26,−19)=6
    (28,−19)=6 (25,−18)=6 (26,−20)=5 (28,−20)=5 (25,−19)=5 (24,−19)=4
    (25,−20)=4 (24,−20)=3）+ 日志 "poured lava at (27, 46, -18)
    (lava_bucket -> bucket)"。
  - **S4 init 失败 ×3**（scratch 资产副本，仓库未动）：
    `lava.json water_reaction.source` = "nope" → rc≠0 + "water_reaction
    names unknown block"；`bucket.json collects_fluid` = ["water"]
    【lava_bucket 无对偶】→ rc≠0 + 对偶校验点名；未知流体名 → rc≠0 +
    "unknown fluid"。
- **E2E 期间抓到的真 bug**：atlas [4,7] 与 wool_blue 冲突（obsidian
  误占）→ 截图池面颜色污染兜出 → obsidian 移 [7,7]、lava 留 [7,6]
  （干净 8 行 diff）。

### 已知问题（本阶段新增/更新）

KNOWN_ISSUES 桶节双种类化 + 新"熔岩 / 流体双种类（Phase 51）"节：
1. worldgen 无熔岩（仅存档/桶可放置）。
2. 无燃烧/下沉/熔岩伤害（isInWater 仅水；玩家进熔岩 = 无游泳无伤害
   = v1 边界）。
3. 无蒸汽、水不消耗（反应只产石/黑曜石，水存续 = 文档化 MC 偏离）。
4. 湿润判定含熔岩（熔岩池旁耕地算湿 = P50 语义延续，非 MC）。
5. obsidian 仅反应可得（无其他用途/配方）。
6. 流动熔岩与流动水同速（无独立熔岩速度数据字段）。

### 坑位（PITS +5）

1. **raw string `R"(...)"` 终止符 `)"` 拼 JSON 尾部连踩 3 次**（测试
   拼接 lava/water_reaction JSON：`...)"` 提前终止 raw string）。
2. **两种流体同 tick 抢同一空格 = apply 顺序决定**（反应 pass → 水 →
   熔岩；测试断言**稳态不变量**，不 assert 瞬态赢家）。
3. **`git checkout -- <file>` 连本阶段未提交工作一起还原**（atlas JSON
   修复误还原丢 lava/obsidian tile；数据文件修 JSON 用定点文本替换）。
4. **桶射线墙遮挡 + 倒射线只认方块受击面**（1 格高墙挡同层舀射线 →
   墙列前下穿 y=47；孤立空格不可倒 → 旁放支撑块瞄面）。
5. **瞄方块面中心用错半格 = 射线擦角边漏过**（格 (x,y,z) 西面面心 =
   (x, y+0.5, **z+0.5**)；S3b 瞄成 (28,46.5,−18.5)【西北棱中点】→
   射线从石头北侧 0.5 格穿过、reach 5 内无命中 → pour 静默 fall
   through。排查时渲染/相机/世界全对（截图轮廓反解 fit 5px/点），
   真实 raycastBlock 探针（p51ray）一锤定音。对策：场景瞄准先探针
   验证 hit + 受击面再跑输入 E2E）。

### 下一步

P52 = **加权多掉落列表**（候选 2）：`chance_drop`（保留）+ 新增
`chance_drops`（同三元组 `{item,count,chance}` 数组，与 chance_drop
互斥【二选一，同时写拒绝】；各条目**独立 roll**、salt 按条目索引去
相关；breakBlock 循环条目、每条目至多 1 个掉落实体）；内容 = grass
改 `chance_drops: [{item: wheat_1, count: 1, chance: …}]`（+ 可能的
dirt 条目 = MC 草掉 1-2 种子语义 v1 简化）；init 互斥/存在/stack 校验
（chance_drop 纪律复用）；测试（离线预计算 roll 范式 + 多条目独立性
）；E2E（base 门 = 第 26 次免疫候选【grass drops 变数组不改 worldgen /
atlas】+ 破坏格 ×2 归一化一致【实体 age 剥真实 dt】+ init 失败）；
提交 + 报告。
