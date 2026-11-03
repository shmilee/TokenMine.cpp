# Phase V2-7 — 材料补齐批（v2.7.0，门 178 全 PASS）

> 2.x 第七项（V2-7 = planks 可放置 / gold_block·diamond_block /
> spider 掉落 / 块级 per-face 纹理）。四子项均数据驱动（核心唯一新增
> = 「块级 faces 通用能力」一处，无 faces = 位级不变红线）。设计定案
> = /tmp/opencode/v27_design.md。**零 format（25 不动）/ 零 config 新键**。

## 实现

### ① planks 可放置化（material → block，MC parity 缺口）
- 新块 `blocks/planks.json`（FullCube，六面复用**图集既有 planks tile
  @ [2,4]** = 零新纹理）：hardness 2.0 / drops 自身 / sound_material
  wood / flamable / **fuel_time 7.5**（自退役物品迁入，值不变 =
  wood.json 块属性先例）
- **删 `items/planks.json`**：块物品按名自动注册（buildBlockItems
  先例）；同名显式物品文件 = init 点名拒（「duplicate item id
  'planks'」——门 178 S6 实证）
- 8 个引用配方（bow/crafting_table/planks/stick/wooden_axe/
  wooden_pickaxe/wooden_shovel/fence）全按名解析 = 零改动（单测钉）
- 图标观感变 3D 方块体 = 记档偏离（wool 同名冲突同款先例，Phase 41）

### ② gold_block / diamond_block（「信标基座 = 仅 iron_block」记档消费）
- iron_block 先例复制：块 JSON（hardness 3.0/5.0、drops 自身）+
  9 锭→1 块 shaped 配方 ×2 + 纹理生成器新 tile 新 salt（957-960）
- **beacon_base: true**（数据驱动布尔，block_registry 既有
  checkField）：信标配基座集合 = {iron, gold, diamond}；等级仍是
  world/beacon 纯函数 3×3/5×5 连续完整层（零核心代码）
- 金/钻物品既有用途（金苹果/染料/弓族）不变，块 = 建筑 + 信标配
  新增用途

### ③ spider 掉落 string / spider_eye（「spider 无掉落」记档消费）
- mobs/spider.json drops：string ×0-2 + spider_eye ×0-2（既有
  count_min/max 随机区间机制，MC 蜘蛛同款值；spider 其余字段零
  改动 + 六 mob FNV 钉）
- 新 material 物品 ×2（icon 生成器新 salt 146-151 + 严格 3D model
  box；螺旋绳 / 紫青眼球）
- **弓配方用 planks 不用 string → 两物品纯收集 = 记档偏离**
  （不加配方 = 最小面定案）

### ④ 块级 per-face 纹理（「炉子四侧面共用同一纹理」记档消费；通用能力）
- `BlockModel.textures` 扩**可选 `faces` 段**（front/back/left/right
  覆写 side；top/bottom 归既有键——faces 内命名 top/bottom = 数据
  加载点名拒）
- 面名由**面外法线**定（共享 yaw-0 = −Z 约定：front=(0,0,−1) /
  back=(0,0,+1) / left=(−1,0,0) / right=(+1,0,0)）；
  `cubeFaceTexture` 加方向感知重载（normal 版）
- **严格解析**：未知键 / 非串 / 空串均点名 block+面；cross/fence/
  glass_pane 形状点名拒（自持几何，段 = 死数据——ammo/cross 纪律
  同款）
- mesher 10 处调用点转方向感知（greedy merge 纹理键 = 方向感知结果
  → per-face 覆写**自动断并**）；掉落物立方体（item_mesh）/
  破坏粒子立方体（game_render）同款
- **无 faces = 位级不变红线**：空串 = 无覆写（回退 side）；既有全
  块 JSON 零改动 + 「除 furnace 外全块方向查找 = 旧查找」单测证明
- 首个内容 = **furnace front 单 tile**（faces.front =
  furnace_front，宽炉口+余火，salt 961-965）；**炉子无 lit 纹理态
  = 记档偏离**（lit 表现 = 既有炉口火焰粒子，不加 furnace_front_lit）

## 变更文件
- `assets/data/blocks/planks.json`（新）/ `gold_block.json`（新）/
  `diamond_block.json`（新）/ `furnace.json`（+faces.front）
- `assets/data/items/planks.json`（删）/ `string.json`（新）/
  `spider_eye.json`（新）
- `assets/data/recipes/gold_block.json`（新）/ `diamond_block.json`
  （新）；`assets/data/mobs/spider.json`（+drops 两项）
- `assets/data/textures/block_atlas.json`（+3 tile，纯 +12 行零移位）
- `assets/textures/blocks/{gold_block,diamond_block,furnace_front}.png`
  （新）/ `items/{string,spider_eye}.png`（新）/ `items/planks.png`
  （删）；其余 184 个 PNG md5 零变化（普查实证）
- `src/block/block.hpp`（BlockTexture +front/back/left/right 可选覆写
  + 约定注释）；`src/block/cube_face_table.hpp`（方向感知重载）；
  `src/block/block_registry.cpp`（faces 严格解析 + requiredTextures
  收录覆写）；`src/world/chunk_mesher.cpp`（10 调用点）；
  `src/entity/item_mesh.cpp` / `src/core/game_render.cpp`（同款立方）；
  `src/item/item.hpp`（material 注释示例修正）
- `tools/generate_block_textures.py`（+3 tile 函数）/
  `tools/generate_item_icons.py`（+string/spider_eye，planks 退役）
- `CMakeLists.txt`：2.6.0→2.7.0（单点）
- 测试：`test_block.cpp`（+5 改 2）/ `test_item.cpp`（+3）/
  `test_mob.cpp`（+1 改 1）/ `test_beacon.cpp`（+1）/
  `test_atlas.cpp`（改 1 census 99→102）/ `test_chunk_mesher.cpp`
  （+2）/ `test_item_model_mesh.cpp`（改 1 计数 81→82）/
  `test_debug_overlay.cpp`（改 1 F3 钉 v2.7.0）

## 实现侧自证
- 构建 `-Wall -Wextra -Wpedantic -Werror` 0 警告
- **2169/2169**（2157 既有 + 12 新）
- **base18 恒等**：60f 无输入 `--screenshot` 与 179116 B 锚
  **IDENTICAL ×3**（含最终 relink 后）；结构免疫根 = worldgen 零新块
  + 热栏无 planks + 无 faces 块位级不变
- 纹理普查：187 PNG = 5 新 1 删，既有 184 零变化（md5 diff 实证）
- 图集 102/128（余 26），row-7 [10..12,7]（V2-5 pig 续行先例）；
  字体 423 零新字（扩展区余 2）
- 冒烟 banner v2.7.0 + format 25

## 门 178（编排者执行；old 侧 = v2.6.0 = cd4c62f 树）— 全 PASS

### S1 = 第 178 次结构性免疫 + base18 锚连续 179116 B 免重锚
- fresh 60f 四方（new v2.7.0 n1/n2 vs old v2.6.0 o1/o2）全 179116 B
  **RAW 全字节一致** + n1 ≡ base18 锚 + 跨版 savenorm **EXACT**
  （零 format——planks/金钻块/炉前脸/spider 掉落无输入全惰性：
  worldgen 零新块 + 无 faces 位级不变 + 块物品注册不改像素路径）

### S2 四子项判别全 PASS
- **planks**（鼠标-only 协议，PITS 368）：场景 = slot0 planks×1
  预置 + 瞄准 (0,54,-3) 顶面；**新侧 RMB 放置 = planks 块入 world
  @ (0,54,-3)（终档实证）/ 旧侧 material 物品 RMB 无动作（同格
  air）= 互斥终态**（放置本身首试命中，无 CALIB 调整）
- **炉子前脸**（块级 per-face 首个内容）：场景 = furnace @
  (0,54,-3) + 玩家 (0.5,54,−6.5) **yaw 3.0 朝 +Z 看 front 面**
  （front 法线 = (0,0,−1)——首跑从 +Z 侧看 = back 面，三图恒等
  零 diff = 面名约定实证，记 PITS 374）；new ×2 RAW 恒等 +
  **new vs old diff bbox (211,313)-(323,420) = 112×107 px @ 画面
  中心 = 前脸纹理区**（old = 四侧同 side 纹理）
- **beacon**（金块信标配）：场景 = 3×3 gold_block 基座 @ y54 +
  beacon (0,55,-3) + 5×5×4 空腔清理（自然地形干扰排除）；
  **gold vs stone 对照（同 new 侧）diff = 细竖列 x[316..355]
  y0..328（光束本体）+ 基座金色区 y483-741（块纹理）**，中心
  (340,370) = 画面上半竖直 = 光束形态；stone new-vs-old diff =
  None（基座纹理族不变）；**旧侧载 gold 场景 init 点名拒（gold_block
  未知）= 可载性互斥**；CPU 探针（probe178_beacon，真实资产+世界）
  实证 beaconLevel(gold 3×3) = 1 / (stone) = 0
- **spider**（掉落 E2E）：scratch 资产树（mob_spawning.json =
  spider only + max_mobs 1——夜间被动 mob 同生 + cap 4 会饿死
  敌对 pick，PITS 373）+ 夜 0.70 + tanky 配置（max_health 60，
  玩家扛 spider 回追）+ pose (0,52,−16)（离世界出生点 >12 =
  敌对生成保护外，PITS 373）；spider f60 生 @ (−1.5,50,−20.5)
  → 爬升 → f300 恒停 (−0.667,52,−17.445) 攻击态；**hold-swing
  击杀（铁斧 4 伤 ×4 击 = 16 HP）：单次运行 kill 2 只（首杀后
  respawn 游荡回 aim 内被余挥击杀），掉落两组 = string×2+eye×2
  @ (−0.8,−19.1) / string×1+eye×2 @ (−1.1,−20.6)——每组独立
  在 0-2 区间（count_max 2 实证）**；**复跑两组逐位相同**
  （跨运行掉落集恒等，capture 时延同款）；玩家 hp 32 > 0；
  旧侧 = 结构互斥（v2.6.0 spider.json **无 drops 字段** +
  string/spider_eye 物品不存在——单测 PreV25 FNV 钉证）

### S3 套件
- **2169/2169**（编排者独立复跑，插桩删除后干净树）

### S6 精确点名（真实二进制，scratch 资产触发）
- stone.json + `faces: {weirdface: stone}` → init 拒：**「block
  'stone': unknown face 'weirdface' in model.textures.faces
  (allowed: front, back, left, right — top/bottom are the existing
  model.textures keys)」**
- items/planks.json（v2.6.0 版 249 B 复原）→ init 拒：**「item
  file 'data/items/planks.json': duplicate item id 'planks'」**

### 性能 A/B PASS 同世界同成本（用户硬要求，发布前）
- v2.6.0（tm_v270_perf = cd4c62f + g139 27 标记）vs v2.7.0
  （tm_v270_perf_new = 插桩删除后终版工作树 + g139 双 27 标记，
  banner 双侧实证）：fall 900f 无输入 seed 123456789：
  - PERF-WIN 三窗全指标同档（upd med 0.62/0.66/0.69 vs
    0.56/0.60/0.68 ms；p95 窗间摆动无系统偏移；render med
    0.58/0.65/0.60 vs 0.51/0.59/0.60；swap 15.3/15.2/15.1 vs
    15.4/15.3/15.2 = vsync 主导同值；fluid 0.045/0.050/0.047 vs
    0.042/0.050/0.053、fires/spread/ents/stream 全同档）
  - **PIPE3-S0 changesApplied 三窗 max 9/3/1 双侧逐位**
  - **终档 fluids 18/18 + blocks 55/55 集合恒等**（同世界最强形
    ——planks/金钻块/炉前脸/spider 掉落无输入全惰性）
  - **FURN 炉子场景自成本**（furnace @ (0,54,-3) + 5×5×4 空腔，
    双侧 900f = per-face 覆写非空分支实测）：v2.7.0 upd med
    0.63/0.66/0.60 vs v2.6.0 0.57/0.66/0.68 ms；render med
    0.62/0.65/0.53 vs 0.57/0.65/0.66；ents 0.0024/0.0027/0.0023
    vs 0.0027/0.0025/0.0024 = **同档，覆写分支零可测增量**

### 门协议发现（新 PITS 373/374）
- **PITS 373 敌对生成双门**：生成保护（normal = 12 块，世界出生点
  半径）+ 夜间被动 mob 同生（夜底光 7 ≤ 15）挤占 max_mobs 4 cap
  → 敌对 pick 恒 "no valid cell / cap reached"；确定性敌对门场景
  = scratch 资产（spawn list spider only + max_mobs 1）+ 玩家
  >12 块站位
- **PITS 374 输入 E2E 三坑**：① hold-swing 窗须落在帧预算内——
  X 退化环境 capture 时延 ~11s（f~660），sleep 8 + 4s hold 把
  mousedown 推到 f~866-900 = 游戏已退（G178DBG2 零行签名 =
  targetMob 从未被调 = LMB 未达 isDown 分支，插桩判别法）；②
  块级 faces 门 = front 法线 (0,0,−1)，玩家须站 **−Z 侧朝 +Z**
  （yaw 3.0 级）——首跑反侧 = 三图恒等零 diff；③ 游戏必须用**真
  实二进制路径**启动（symlink `./tm` 令 comm = "tm"，
  pgrep -x tokenmine 失配 → NO-WINDOW → kill 漏发 → 游戏跑满
  帧 + spider 杀玩家 hp 0）

## 偏离（记档）
1. **planks 图标 = 3D 方块体**（块物品惯例，wool 同款先例 Phase 41）
2. **string / spider_eye 纯收集**（弓配方用 planks 不用 string，
   不加配方 = 最小面定案）
3. **炉子无 lit 纹理态**（lit 表现 = 既有炉口火焰粒子，不加
   furnace_front_lit = 底稿两 tile 收敛单 tile）
4. **campfire 顶面未做**（底稿"可选，定案时定" = 不做，不在定案
   清单）
5. planks 块 sound_material = wood + flamable = true（wood.json 参考
   推论；flammable 集合 17→18、sound 家族 +1 两个既有钉同步更新）
6. 图集落位 row-7 [10..12,7]（V2-5 pig 续行先例）
