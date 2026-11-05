# Phase V3-5 — 建筑内容族（v3.9.0，门 199 全 PASS）

> 设计定案 = `/tmp/opencode/v390_design.md`（7 基础块 + 15 染色 / 2
> 物品 / 19 配方 / 纹理 21 + 2 / 水底 sediment pass / 锚图零水域 =
> 结构免疫第 80 次口径）。**交付按项目实际词表收敛**（见偏离 1/6）。
> 前置：无硬依赖（主世界内容）。

## 实现

### 块（20：6 单 + 14 染色变体）
1. **clay**（黏土）：drops = **clay_ball ×4**（MC 精确）/ sound stone /
   hardness 0.6 / **light_opacity 键省略 = 派生 opaque 15**（偏离 2：
   设计"1"与同不透明度论证自相矛盾——被替换 4 块全 opaque 派生 15，
   省略 = 最强同不透明度形式，测试钉 lightOpacity==15）
2. **mossy_cobblestone**（苔石）：drops 自身 / hardness 2.0
   （偏离 1 设计口径：水底生成 = 项目替代 MC 废墟来源）
3. **bricks**（砖块）：hardness 2.0 + **smeltable_to terracotta**
   （块 smelt 先例；MC 精确：砖块熔炼 = 陶瓦）
4. **stone_bricks**（石砖）：hardness 1.5
5. **mossy_stone_bricks**（苔石砖）：hardness 1.5
6. **terracotta**（陶瓦 = white 基块，Phase 41：携
   dye_base "terracotta" + dye_color "white"——偏离 5：基块按
   bed/wool 先例携白，命名纪律 dyeVariantName("terracotta",
   "white")=="terracotta" 双钉）+ **terracotta_<color> ×14**
   （**项目染料词表 15 色含 white 无 lime**——偏离 1：MC 16 色按
   项目 wool/bed 同款 15 色收敛；init 交叉检 = 既有染料颜色遍历，
   lime 变体 = 孤儿）

### 物品（2）+ 配方（17）
- **clay_ball**（stack 64 + smeltable_to brick + 单盒 model + icon）
- **brick**（stack 64 + 单盒 model + icon）
- 配方 17：bricks（2×2 brick×4）/ stone_bricks（2×2
  cobblestone×4）/ mossy_stone_bricks（**shapeless stone_bricks×3 +
  mossy_cobblestone×1**，偏离 2 设计口径）/ 染色 ×14（shapeless
  dye_<color> + terracotta → 1）。两熔炼 = 数据字段非 recipe 文件
  （CRAFT-6 纪律）。偏离 6：设计"19 配方"内部计数笔误（4+15 列实为
  3+15）。

### 纹理（20 tile + 2 icon）
- generate_block_textures +20（salts 1065..1076；14 染色 = **新确定
  性混合公式 `dyed = (base×3 + dye×2)//5`**——偏离 4：项目无 base×
  染料混合先例（bed 染色 = 羊毛色板原样复用）；60% 染向整数无浮
  点，dye 色板 = WOOL_PALETTE 同款 15 色源，零新盐，重跑恒等）
- generate_item_icons +2（salt 块 180..183）
- atlas **145→165**（行 9 [5..15] + 行 10 [0..8]，< 256 无幂二增长，
  既有 145 格零移位——锚免重锚实证）

### 水底 sediment 生成 pass（worldgen.json 新节，数据驱动）
- `"sediment": {"enabled": true, "clay_probability": 0.06,
  "mossy_cobblestone_probability": 0.008}`——解析严格（[0,1] 越界/
  非数字 init 点名；**absent = disabled 位级 legacy**；双 0 = 数据形
  no-op）；world_generator create() fail-fast 解析 2 沉积块 + 拒
  stateful + 4 词表块（dirt/gravel/sand/stone 代码常量 = 偏离 4 设计
  口径）
- **时机 = 生成链最后**（placeLavaLakes 之后——偏离 3：代码既有顺序
  装饰先于湖，"湖后装饰前"不可满足；读最终湖底 = 设计意图）
- 判定（per-column 纯函数，新盐 kSaltSediment/kSaltSedimentMossy，
  hash2 零噪声）：dry 列（h ≥ seaLevel）先跳零触碰 / 表面格在本
  chunk y 层 / **表面格终数据 ∈ 词表**（洞穴/矿脉/湖挖过的拒）/
  两个熔岩湖谓词（列顶流体 = water【熔岩带 ≥ seaLevel 拒】+ 水面
  接触地板【地板被熔岩带占 = 拒】）/ 双独立 roll **clay 优先**
  （同格双中 = clay，pass 序确定性）/ **顶 1 层**（MC 3-4 层简化 =
  偏离 5 设计口径）
- 发行世界 census（seed 123456789，±512²，真实 biome 词表面 =
  **desert 沙洋底 only**）：eligible 3683 / **clay 211 / mossy
  26** + 样本列 chunk 生成 ground truth 交叉验证（oracle 与生成器
  一致）

## 门 199（结构免疫第 80 次；锚 224468 B 免重锚）

**结构免疫论证**：sediment pass 只换水底 solid（锚图零水域目检 =
不可见）+ 替换块 opaque 派生 15 同不透明度（光零变化）+ 生成内容 =
参考不入存档 diff + 纯块/物品内容不在默认热栏。
- **S1** 无输入 60f ×2 ≡ 锚逐字节 + savenorm **EXACT** + 白名单 4
  键 + banner v3.9.0。
- **S2 场景臂**（全手工水室——生成证明 = census 单测，v3.5.0 先例；
  场景几何教训入 PITS 390：首跑 y=50/51 落 demo 区参考石内
  【表面 y=53】流体编辑 "cell occupied" 拒载 → 整体上移 y=55/56 参考
  air 区；开放水源漫溢 402 格 → 围堰 48 格密封 = 32 池格恒定）：
  - (a) **展示行 ×2**：clay 3 / mossy cobblestone / bricks /
    terracotta / terracotta_red / terracotta_blue 水下陈列 + 32 密封
    池格 + 32 格 cobble 底 + 24 格围墙——截图 RAW 恒等（6 件新纹理
    水下可见实证）+ 终档行编辑全保留 + 池格 32 精确（发货世界程序
    湖 ~30 块外 x25-30 y34 的 9 格 fluid-settle 确定性编辑 = 断言
    须自格 scoped，PITS 390③）。
  - (b) **破 clay 臂**（输入）：玩家池内直下瞄准 LMB 1.3 s hold
    （hardness 0.6 ~0.96 s 破 + 余量不可达下方参考 stone 2.4 s）→
    **4× clay_ball 入库存**（磁力拾取，终档零掉落实体）+ clay 编辑
    回退参考（终档 clay 3→2，v3.5.0 quartz_ore 先例）。
  - (c) **熔炼臂**（无输入 600f，g196 炉子先例）：clay_ball ×1 +
    coal ×1 预载 → "Furnace smelt complete: brick" + 终档 output =
    brick ×1 / input 空 / **fuel_substeps 4201**（4800−599 精确验算）
    ——clay_ball → brick 熔炼 E2E（bricks → terracotta = 单测 smelt
    tick ×2 钉）。
- **S3** 全套 2724/2724（编排者独立复跑）。
- **S6 点名 4/4**（scratch 资产手术全 init 带路径 rc=1）：
  sediment.clay_probability 1.5 / −0.01（"must be in [0, 1]"）/
  "0.06" 字符串（"must be a number"）/ **terraccotta_red.json 删除**
  （game_init "dye family 'terracotta' is missing the variant block
  'terracotta_red' (dye color 'red')"——15 色交叉检实证）。
- **性能 A/B（用户硬要求）**：旧 = v3.8.0（tm_v390_perf = ae1fd92 +
  g139，banner v3.8.0 实证），新 = v3.9.0（tm_v390_perf_new + g139，
  banner 实证）：主对 900f 无输入同 seed：PERF-WIN 三窗同档（upd med 0.550/0.539/
  0.574 vs 0.607/0.574/0.612 ms——窗口间机器噪声带，均值 0.554 vs
  0.598；sediment pass = 生成链一次性列成本，零每帧开销）+ render
  均值 0.573 vs 0.579 同档 + swap vsync 同档 + fluid·fires·spread·
  ents·stream 同档 + **changesApplied 9/1/3 逐位恒等** + **终档
  savenorm EXACT**（生成内容不入 diff 零 format 完全恒等最强形）

## 单测族（2697 → 2724，+27）

test_v390.cpp 新 13（块/物品/配方内容钉 + dye 族 15 色全 + 熔炼 tick
×2 + 合成行为【2×2 grid 匹配 + shapeless bag + 缺料拒收】+ census +
24 新资产 FNV 钉）/ test_world_generator +8（disabled 位级 no-op /
仅表面格 diff / per-column roll 纯谓词逐位 / clay 优先 / 词表拒收
grass / 熔岩湖拒收双面 / 跨生成器确定性 / **发行世界 census 1024²**
样本列 ground truth）/ test_worldgen_config +6（absent 默认 / 解析 /
类型拒收 / 越界拒收 / 发行值钉）；有意重钉 6 处（atlas 165 + 坐标表
/ mob atlas FNV / nether atlas 165 / netherite icon 97 + recipe 112 /
block sound 集合 +20 / model 97 / F3 v3.9.0）。

## 偏离/记档（6 条）

1. **染色族 15 色非 16**（项目染料词表含 white 无 lime；terracotta
   white 基块 + 14 变体 = 族 15；块文件 20 / tile 20 / recipe 17）。
2. light_opacity 键省略（派生 opaque 15，同不透明度最强形）。
3. pass 时机 = 生成链最后（装饰先于湖既有顺序，"湖后装饰前"不可
   满足；读最终湖底 = 设计意图）。
4. 染色纹理混合公式 (base×3+dye×2)//5 自建（无既有先例，如实报告）。
5. 顶 1 层（MC 3-4 层简化）+ 词表 4 块代码常量（数据化收益低）。
6. "19 配方"设计计数笔误（实 17：3 块配方 + 14 染色 + 2 熔炼走
   数据字段）。

## 变更文件（显式路径，80 + config 除外）

核心 4（worldgen_config.{hpp,cpp} Sediment 节 / world_generator.
{hpp,cpp} pass + 3 纯谓词 + fail-fast）+ 数据 23（20 块 / 2 物品 /
17 配方 / block_atlas.json 165 / worldgen.json +sediment）+ 资产
22（20 tile / 2 icon）+ 工具 2（block 纹理 + item icon 生成器）+
测试 7（1 新 6 改）+ CMakeLists（3.9.0 + 测试注册）。

零改动红线核证：renderer/ 零 / save 零 format（30 不变）/ config 零
/ biomes_nether 零 / hotbar 零 / 主世界无输入 ≡ 锚（S1 实证）。
