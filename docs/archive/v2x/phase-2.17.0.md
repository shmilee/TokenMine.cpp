# Phase V2-16 — 雪原/针叶林 biome（snowy_taiga）+ 音效缺陷族修复（v2.17.0，门 188 全 PASS）

> 二批 GEN 行（BACKLOG:「中」：biome JSON + 雪面 + 装饰；旧存档 =
> 世界生成演进先例 GEN-1/3/5；新 biome 避开演示区 = 结构性免疫设计
> 【熔岩湖先例】）。MC 机制 11-04 编排者 wiki 核定（Snowy_taiga 页：
> temperature −0.5 / downfall 0.4 / trees_taiga 密林 + freeze_top_layer
> 雪层 / creature 池；Snow 页：雪层 1-8 层薄覆盖 hardness 0.1 透明
> shovel 每层 1 snowball 光 ≥12 融）入设计稿 /tmp/opencode/v216_
> design.md。fixer 一次交付 + 两轮音效缺陷修复（ses_e5b32b49effe6KeF2
> J2UGy0MCv）。**零 format**（biome 布局不入档 = 每次从 seed 重生成，
> GEN-1/3/5 同款；旧档加载 = 区外雪带世界演进，演示区结构不变）。

## 实现

### 新 biome：snowy_taiga（assets/data/biomes/snowy_taiga.json）
- weight **2**（= desert 稀有带先例；总权重 10→12；偏离 4）
- surface **snow** / subsurface dirt / depth 3 / tree_density
  **0.12**（= forest 项目密林值，偏离 3）/ height_scale 1.1（=
  forest）
- **新严格字段 `tree`**（6 键对象 = sapling tree 段同款面：
  trunk spruce_trunk / leaves spruce_leaves / min_height 6 /
  height_range 3 / leaf_radius 2 / shape spruce——值 = 既有
  spruce_sapling tree 段 = MC 高瘦 5-7 针叶）：缺/多/类型/范围/
  shape 词汇（oak/spruce/birch）全点名 `tree.<key>` 拒；create
  交叉检 trunk/leaves 块存在 + ENT-2 拒 stateful（全点名）
- **新严格字段 `exclude_spawn_area: true`**（严格 bool）=
  **演示区结构保护**（GEN-1 spawn_clear 结构排除先例）：
  `inSpawnClearZone` 纯谓词（|chunk| ≤ caves.spawn_clear_columns
  = carveCaves 早退同款列对齐判定）内 biomeIndexAt 累计走查跳过
  该旗 → **区内 totalWeight 恒 12−2=10 = 现有 desert(2)/forest(4)/
  plains(4) 映射逐位不变**（单测 5×5 chunk 全列 + 4×4 演示区
  chunk 3/4 群系 blocks+fluids 逐字节恒等双钉）
- decorations 无（偏离 2：MC 雪针叶林 = fern/berry，项目无此类块）

### 新块：snow（满块，MC snow_block 范式）
- hardness **0.1**（MC 精确）/ sound_material **新 "snow" 族**
  （snow_dig 0.11s / snow_place 0.12s / snow_break 0.18s 确定性
  合成，sand 族范式一阶更软，seed 0x75001/0x75111/0x75222；既有
  43 wav 重跑 md5 全不变）/ drops 自身 / opaque（light 15 派生）/
  full_cube 单 tile（白 246/248/252 微噪 salt 1000；atlas 121→122
  [12,6] 空格零既有移位）
- **偏离 1**：MC 雪 = 1-8 层薄覆盖（每层 2px / 1 层空碰撞 / shovel
  每层 1 snowball / 光 ≥12 融 / 放块替换单层）——项目无层机制（=
  V2-17 顶部 slab 相邻范围）/ 无 snowball 物品 / 无降雪天气（weather
  仅 CLEAR/RAIN）→ 满块替代；无配方（MC 4 snowball 合雪块）
- spruce 树苗 plant_on [dirt,grass] 不动（雪面不生长 = MC 同款）

### 核心：placeTrees 针叶路径
- 同 kSaltTree 格网 + 同 h（密度门 h>>40 / 高度位 h>>24 复用 =
  确定性）+ 高度参数换 tree 对象 + 冠层 = **planTree(height,
  TreeSpec)** 逐 cell placeIfAir（既有纯函数；Oak 表单测已钉
  cell-for-cell 同构 placeTrees 硬编码冠层；root dy=0 = 表面格非
  air 自然 no-op → 树干自 hgt+1 起 = 既有行为同款）+ 海平面/洞穴
  浮空守卫不变
- **无 tree 的 biome = 原硬编码路径零触碰**（位级红线：既有 3
  biome 逐位不变，oak isomorphism 既有钉零改动全过）

### 音效缺陷族修复（本 phase 记档子项，gate 183 save.cpp PITS 361
随版修复先例）
- **缺陷**：generate_sounds.py 的 `_noise()` 及 `dig()`/`break_sfx()`
  内联 LCG **逐采样重建** = 常值流；负常值经 lowpass `max(0.0,…)`
  钳位 → **静音 wav**（sand_break/gravel_break/dig/break peak 0
  实证）；另有 5 件 DC 退化（单符号纯指数斜坡零纹理：stone_dig/
  wood_dig/wood_break/gravel_dig/sand_dig/cloth_dig/cloth_break）+
  2 件部分受损（常值 DC 偏置 + 真噪声层：stone_break/glass_break）
  = **共 13 件跨多版本潜伏**（截图/存档不录音频 = 门盲区）
- **修复**：单 LCG 实例推进范式（snow 族正确写法）；重跑 md5 变更
  = 恰好 13 件（其余 33 件逐一 head-identical + snow 3 新件不变，
  二次重跑幂等）；修后峰值全非零 + abs-rise-frac 纹理度量全落正常
  带（0.30-0.50，健康件对照 0.251-0.504）
- **审计口径（PITS 382）**：peak（0 = 静音）+ sign-chg +
  abs-rise-frac（纹理 = 逐采样幅度变化占比，常值流 ≈0）

## 变更文件（37：13 wav 修复 + 24 V2-16，其中 9 新）
新：snowy_taiga.json / snow.json / snow.png / snow 三 wav /
biome tree+旗解析（biome.{hpp,cpp}）/ world_generator 区内保护 +
针叶路径（world_generator.{hpp,cpp}）/ 生成器 2（snow tile + snow
音族 + 13 件修复）/ test_biome +13 / test_world_generator +8 /
内容钉 5（atlas 122/sound 46/区块 FNV/材质集/F3 v2.17.0）/
重锚钉（矿脉 4 件 + 洞穴探针 3×2 尺寸——区外世界演进 GEN-4/7/8
先例）/ CONTENT_GUIDE biome 节 +4 群系 / KNOWN_ISSUES 铲列表注

## 门 188（old = v2.16.0 tm_v2160_base/perf = 930dcc1；编排者执行）— 全 PASS

### S1 = 结构免疫第 69 次（**免重锚**——设计预判被证据推翻）
- 设计预判「远区雪带边界漂移入 60f 视野 = 重锚」；实证 = **60f
  流送半径内可见地形全落受保护演示区**（远区雪带已入世界但未入
  流送可见范围）：新侧 60f ×2 EXACT 且 **≡ 现锚 179027 B 零 diff**
  （旧侧 ×2 同恒等）
- savenorm（新侧双跑）= EXACT（零 format）
- 区内位级双钉（单测）：5×5 chunk 全列 resolvedBiomeAt = 改前 10
  权重公式同值 + 4×4 演示区 chunk 3/4 群系 blocks+fluids 逐字节恒等

### S2 = 雪块破/放单准星双动作输入 E2E（PITS 376/379 协议）
- 场景：雪块 (2,54,2) + 手持 snow×1 slot0 + 固定 pose（yaw
  −2.3460 / pitch −0.3642）：射线自 (2,54,2) −Z 面入（(2.04,
  54.79,2)，破目标）→ 雪破后穿 air 格续行至 (3,53,3) 地顶面
  ((3.56,54,3.48)) → **同一准星破后自然转为放置目标 (3,54,3)**
  （无重瞄）
- 序列：LMB hold 0.5s（雪 0.1 硬度 ~0.2s 破 + 续裂地 0.3s < 0.75s
  破时释放重置 = PITS 7/379 断点时序）→ RMB 放
- **终档全断言 PASS**：(2,54,2) 无雪（破）+ (3,54,3) 有雪（放）+
  slot0 snow 0（手持消耗）+ 掉落 1 件雪物品留地 pos (2.57,54,2.58)
  ∈ 破格盒（**距玩家 2.83 > 磁力 2.5 = PITS 377 不拾取**，断言
  稳定化）；旧侧 v2.16.0（snow 词表不存在，场景无雪同键形输入）
  = 零 snow 块/物品 + format 28 形状互斥

### S3 套件
- **2415/2415**（2391 + 23 biome/worldgen + 1 音效 13 件钉族；
  编排者独立复跑）

### S6 = 6/6 点名
tree 缺 shape（严格 6 键）/ shape 'pine' 未知词汇 / trunk
'acacia_trunk' 未知块（create 交叉检）/ exclude_spawn_area 非
bool / snow hardness −1 / snow sound_material 缺 snow_dig.wav
（文件级交叉检）

### 性能 A/B PASS 同世界同成本（用户硬要求）
- 主对 fall 900f 无输入 seed 123456789（mobs off）：PERF-WIN 三窗
  同档（upd med 0.59/0.51/0.50 vs 0.59/0.53/0.64 ms——f900 新侧
  0.64 = 流送尖峰同量级）+ **changesApplied 9/1/3 逐位** + 终档
  journal **0/0 恒等**（**口径发现**：世界生成演进 = 参考世界变化
  非 journal 编辑——blocks journal 仅记录生成后活动【营火/流体/
  玩家】，故区外雪带不入 journal，断言比演示区子集口径更强 = 全
  集恒等）
- **雪带自成本**（玩家传送 (48.5,36,263.5) 雪列 + 600f/1800f 无
  输入，双侧同场景同 config）：600f 窗双侧同档（upd med 95.8/
  102.9）但受 263 块瞬移 80-chunk 流送爆发主导 → **1800f 稳态窗**
  收口：upd med **111.9 vs 112.5 ms（0.5%）** + render/swap/
  fluid/stream 全同档（场景区水网 = fluid 17-34ms 双侧同款主导）
  = 雪地形（snow 表面 + spruce 针叶）稳态无可测增量

## 偏离（记档 4 条）
1. snow = 满块替代 MC 1-8 层雪层（无层机制/snowball/降雪天气；
   无 snowball 配方）
2. taiga 无 decorations（项目无 fern/berry 块）
3. tree_density 0.12 = forest 项目密林值（非 MC 0.24 尺度）
4. weight 2 = desert 稀有带先例（非 MC 频率）

## 门协议发现（PITS 382）
**确定性音效 LCG 逐采样重建 = 静音/退化 wav 缺陷族**：`[Lcg(seed).
next() for _ in range(n)]` 式逐采样重建状态 = 常值流（同 seed 同
值，PITS 4 纯 finalizer 坑的生成器侧同族）；负常值 + lowpass 钳位
max(0,·) = 静音；常值 + 正弦/真噪声混合 = DC 偏置音色偏离。13 件
跨多版本潜伏（sand/gravel/stone/wood/cloth/glass break+dig 族 +
generic dig/break 兜底）——**截图/存档门不录音频 = 门盲区**，
审计口径 = peak（0 = 静音）+ sign-chg + abs-rise-frac 纹理度量
（常值流 ≈0，健康带 0.25-0.50）。修复纪律 = md5 变更集须与受损
清单精确一致 + 二次重跑幂等 + 长度钉不变（时长未动）。
