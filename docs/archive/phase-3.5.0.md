# Phase V3-2/Phase C Part 1 — 下界内容族（v3.5.0，门 195 全 PASS）

> 设计定案 = `/tmp/opencode/v350_design.md`（D1-D9，含 **D9 带修订**）。
> V3-2 下界内容族与 V3-1 Phase C 合并执行第一段：群系族 + nether-
> rack 族块 + 矿脉 + 装饰 = "下界变成 MC 的下界"。矿脉机制 = GEN-3
> lattice 先例克隆；biome 选择 = biomeIndexAt 机制克隆（valueNoise2 +
> 权重累计行走）。

## 实现

### D9 带修订（编排者 11-05 布局裁决——实现后实证下界不可航行的修正）
v3.4.0 层带（netherrack 32..122 实心板）把典型传送落点（Y=55）埋在
岩层：stem/群系表面全不可见，"表面"贴基岩顶零头空间 = 下界不可探索。
修订为 MC 下界废土形态（世界生成演进先例 GEN-1/3/5：未编辑列按当
前生成器重建，既有 portal 框 = 编辑持久）：

| 层 | y 带 | 内容 |
|---|---|---|
| 基岩底 | 0-4 | 不变 |
| 熔岩海 | 5-31 | 全源 level 8 静态（不变，岩板下 = 挖穿可见 MC 同款） |
| netherrack 台地 | 32-52 | 21 层（原 32..122）；群系表面 = 台地顶向下 depth（soul_sand 50-52 / nylium 52）；soul_soil·magma 带 48-52；**矿脉带 32-52**（debris 不再全板） |
| **空气空间** | **53-122** | 70 格可航行空腔（新增） |
| 天花板 glowstone | 122 | 空腔顶行贴基岩顶底面（MC 精确位置） |
| 基岩顶 | 123-127 | 不变 |

stem·basalt 柱自 y=53 生长（台地顶之上 = 可见）；传送落点 Y=55 →
空腔内（portal 房间悬浮，平台自足；落点安全核对 = 严格优于修订前：
room 探针旧板内静默失败保位，新空腔首试即 roomy）。

### 群系族（D1：独立池独立 registry，主世界池零触碰）
`assets/data/biomes_nether/` 5 文件（共享 BiomeRegistry schema 子集）：
nether_wastes 36 / crimson_forest 22 / soul_sand_valley 17 / basalt_
deltas 16 / warped_forest 9（权重和 100 = MC 比例；registry load 序
确定性）。`netherBiomeAt` = biomeIndexAt 克隆（valueNoise2(x×0.02,
z×0.02, 派生 seed ^ kSaltNetherBiome) → 权重累计行走）。depth 0 群
系省略 subsurface_depth（共享 schema [1,16] 严格域拒 0，surface =
netherrack 构造性 no-op）；装饰门控 = 数据派生旗（forest↔nylium 兄弟
stem / netherrack+概率↔basalt / soul_sand↔soulFamily——fill 只读旗不
读名，零 name switch）。

### 四个生成 pass（全 = 派生 seed + 坐标纯函数，chunk 间顺序无关）
pass 序 = 表面 → 矿脉 → 装饰（最后写者覆写，确定性顺序钉）：
1. biome 表面（per-column 台地顶向下）
2. 矿脉（per-cell lattice 8，netherrack 台地 only——熔岩海/基岩/覆写
   格不放）：quartz_ore p 0.0376 / nether_gold_ore p 0.0208（drops
   gold_ingot 1 MC 精确）/ ancient_debris p 0.0026
3. 装饰：stem 柱（crimson/warped，列对齐 lattice 4，高 4..9，自 y53）
   / basalt 柱（basalt_deltas，lattice 8，自 y53 高 5..30）/ soul_soil
   per-cell p 0.05 带 48-52 / magma_block per-cell p 0.01 带 48-52 /
   天花板 glowstone per-cell p 0.04 y=122（**复用既有 glowstone 块**）
7 个专盐（kSaltNetherBiome/Ore/Stem/Basalt/SoulSoil/Magma/Glowstone，
kSaltNether 族续）。

### 块族 12 件 + 物品 + 配方（纯数据）
nether_brick（2.0）/ magma_block（1.5）/ quartz_ore（3.0 drops quartz）
/ nether_gold_ore（3.0）/ ancient_debris（**10** = MC 50 的 obsidian
/5 缩放先例）/ soul_sand（0.5）/ soul_soil（0.5）/ crimson_nylium
（0.4，grass 先例 = sound_material 缺省）/ warped_nylium（0.4）/
basalt（1.2）/ crimson_stem（0.4 drops 自身 = 记档偏离 MC 不掉落）/
warped_stem（0.4）。全 stateless（worldgen 永不写 state 纪律构造性
满足）。物品：quartz（手工 JSON + icon + model）；ancient_debris =
块物品（名字占用物品空间，手工文件重名拒收——偏离 1）。配方：nether
brick 熔炼（netherrack → nether_brick 1:1，netherrack.json +smeltable_
to）+ 2×2 shape（项目单物品空间坍缩为块物品自压缩 = 偏离 7，part 2
材料链细化）。纹理 12 件（salts 1017..1064 确定性生成，既有 98 张
md5 逐一不变）+ **atlas 16×8 → 16×16 幂二增长**（128 < 136 必须；
既有 124 格 [col,row] 零移位 + NEAREST 采样像素行恒等 = base 锚免重
锚实证）+ quartz icon 1 件（83→84，ENT-9 83→84 钉）。

### init 时序 fail-fast（门内 S6 实证发现，fixer 续修）
nether 内容解析从 ensureNether（惰性 = 首传送才跑）前移到 **buildScene
init**（NetherGen::create + MC_TRY，缓存 m_nether.gen，与主世界
WorldGenerator::create 同序）——懒的是 nether World（chunks/meshes/
journal/streaming），不是内容解析。单测 InitTimeFailFastNaming 双形态
消息钉。

### 版本/零改动
3.4.0→3.5.0 三件套；**overworld 零触碰**（主世界 biome 池/World-
Generator/worldgen.json/流送/渲染零 diff；唯一共享面 = 图集幂二增长
的像素行恒等）；存档零 format（30 不变——nether 内容 = 世界生成 =
参考演进，不入 nether 段 diff【fixer 旧树对照实证 census 差 ∅】）。

## 门 195（结构免疫第 76 次；锚 224468 B 免重锚）

- **S1** 无输入 60f ×2：截图 ≡ 锚 224468 逐字节（**图集 16×16 扩容的
  像素行恒等端到端实证**）+ savenorm EXACT + 终档白名单 4 键（format
  30 不变）。
- **S2 内容双臂**：(a) 传送 + 空腔：activated 场景 200f → "Traveled
  to nether at (1, 55, -1)" + **下界截图 ×2 RAW 恒等**（观感实证：
  ~60% 暗红空腔天穹 + 台地顶远地平线雾吞剪影【几何验算：eye 56.62
  pitch 0 台地低 4.62 → 7.4° 低于中心 ✓】+ 基岩平台近景 + portal 光
  11 紫晕）+ 终档 dimension/pos 精确/26 块生成框 census；(b) **下界
  原生采矿**（scene dimension=nether 手编辑 + 玩家台地顶 feet 53.0 +
  相邻 quartz_ore 编辑）：LMB 5.5 s hold → quartz_ore 编辑回退（终档
  无 diff）+ **quartz ×1 入库存**（磁力 1.08 < 2.5 吸取）+ 按住续破
  相邻 soul_sand（PITS 7 实证）其掉落距玩家中心 2.5 边缘留地（PITS
  377 实证）+ OW 段 26 块原样（下界挖掘不碰主世界）。
- **S3** 全套 2617/2617（编排者独立复跑）。
- **S6 5/5 init 点名**（scratch 资产手术）：weight 0（"must be >= 1"）
  / 重复 id（"duplicate biome id"）/ 未知 surface（"unknown surface
  block 'crimson_wart_block'"）/ 空池（"requires at least one biome"）
  / stateful surface（"per-instance state — worldgen never writes
  state"）——全 rc=1 带文件路径；**门内发现并修复校验时序缺陷**（见
  init 时序节）。
- **性能 A/B（用户硬要求）**：旧 = v3.4.0（tm_v350_perf = 951628d +
  g139，banner v3.4.0 实证），新 = v3.5.0（tm_v350_perf_new + g139，
  banner v3.5.0 实证）。主对 900f 无输入同 seed：PERF-WIN 三窗全指标
  同档（upd med 0.578/0.615/0.634 vs 0.556/0.563/0.584 ms / render
  0.597/0.651/0.596 vs 0.600/0.598/0.544 / swap vsync 同档 /
  fluid·fires·spread·ents·stream 同档）+ **changesApplied 9/1/3
  逐位恒等** + **终档 savenorm EXACT**（v3.5.0 零 format + 下界惰性
  未建 = 完全恒等，比 4 键白名单更强的口径）。**下界含内容稳态自
  成本**（传送场景 900f = 60f 倒计时 + 传送 + ~840f 下界）：render
  med 0.365/0.361/0.362（**≤ OW 基线 0.60**——实心岩板 + 空腔几何 <
  主世界树冠）+ upd 同档 + fluid/fires/spread = 0.000（全源静态熔岩
  海零 churn 再证）+ stream 0.024-0.027 ms（nether 流送含熔岩流体
  列，sub-ms 级同档）。

## 单测族（2582 → 2617，+35）

nether_gen 34【NetherBiomeAt 4（确定性 + 100 列分布带 16/22/36/17/9 ±
10% + load 序敏感 + 盐独立 73/100 列不同）/ NetherGenCreate 6（全内容
解析 + 未知块点名 + stateful 拒收 + schema 拒收族 + 空池退化 +
**InitTimeFailFastNaming**）/ 表面 2 / 矿脉 5（lattice roll 精确流钉 +
netherrack-only 谓词 + 台地带边界 + debris 全板钉 + 族在场）/ stem 2 /
basalt 1 / soul_soil 1 / magma 1 / glowstone 1（122 行 + 空腔 112..121
air）/ pass 序 1 / fill 含内容 4（**2×2×2 chunk 双序逐字节** + 纯净性
+ 负坐标 + 熔岩海零触碰）/ 内容 6（真实资产 12 块 + 2 物品 + 2 配方 +
biome 目录交叉检 + RealContentGenCensus 12 族 hunt 在场 + atlas 136）】
+ 带修订重钉 ~12 例（层带 5 组 / 表面顶锚方向 / 柱基面 53 / ore 带缩
台地 / glowstone = 空腔顶 / pass 覆写语义重写 / census 重钉）。

## 实测数据

- 群系分布（100 列确定性采样）：wastes 36 / crimson 22 / soul 17 /
  basalt 16 / warped 9 ± 带内（单测钉）。
- 首访观感：空腔开阔（非岩壁）+ 远地平线 + 平台 + portal 紫晕（截图
  实证）。
- atlas 136 / 纹理 110（98 既有 + 12 新）/ 物品 icon 84 / SoundBank
  49（不变）/ 块注册 83+12。

## 偏离/记档（fixer 9 条 + 门内 1 条，全验收）

1. ancient_debris = 块物品（手工同名文件构建性拒收；icon +1 非 +2）。
2. depth 0 群系 = 省略 subsurface_depth（共享 schema [1,16] 拒 0，
   主世界池零触碰红线不许改共享 schema）。
3. biome 文件 +subsurface（=netherrack 非空必填）+ tree_density 复用
   = 柱状特征 per-lattice 概率（数据驱动门控，fill 零 name switch）。
4. nylium sound_material 缺省 = grass 先例（assets 无 grass 族 wav，
   显式 "grass" init 硬失败——按既有 grass 块形态落实）。
5. magma_block 无 light_emission（MC 3 光 = part 2 ambience）。
6. atlas 16×8→16×16 幂二增长（128 < 136 必须；既有格零移位 + 像素行
   恒等 = 锚免重锚实证）。
7. 2×2 shape = 4× 块物品 → 1（项目单物品空间无独立 brick 材料；part 2
   细化）。
8. kNetherBiomeNoiseScale 0.02 = overworld 发货值（双维同量级群系区
   简化，记档）。
9. 下界内容 census 落点 = 世界层单测（RealContentGenCensus 确定性
   hunt）——nether 段 = 对演进参考的 diff，生成内容构造性不入档
   （旧树对照 census 差 ∅ 实证）。
10.（门内）**校验时序 fail-fast 前移**：surface 存在性/stateful 拒收
    从首传惰性 → buildScene init（主世界同款；门 S6 实证发现）。

## 变更文件（显式路径，44 + 1 = 45）

assets/data/biomes_nether/*.json（5 新）/ assets/data/blocks/{12 新 +
netherrack.json smeltable_to} / assets/data/items/quartz.json（新）/
assets/data/recipes/nether_brick.json（新）/ assets/textures/blocks/
{12 新 png} + items/quartz.png / block_atlas.json（16×16 + 12 格）/
src/world/nether_gen.{hpp,cpp}（带修订 + 4 pass + create）/
src/core/game.{hpp}（m_netherBiomes + m_nether.gen 缓存）/
src/core/game_init.cpp（第二 loadDirectory + init fail-fast create +
ensureNether 消费）/ CMakeLists（3.5.0）/ tests/{test_nether_gen +35
重钉 / test_atlas 136 / test_block 音族 / test_item_model_mesh 84 /
test_mob FNV / test_debug_overlay F3} / tools/{generate_block_
textures +12 件 / generate_item_icons +1 件}。

零改动红线核证：overworld worldgen/biome 池/渲染/流送零 diff（锚逐字
节 + 白名单 4 键端到端实证）/ config 零键 / 存档零 format / 主世界
块数据仅 netherrack +1 smeltable 字段（worldgen 不引用 netherrack）。
