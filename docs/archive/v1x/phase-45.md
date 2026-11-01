# Phase 45 细节存档

> 用户确认方向（"染色补全 + 十字植物渲染"，全 6 色方案 = P41/P12/P20/P23
> 遗留「无蓝/绿/紫等色 + 无花/仙人掌/lapis 来源」）。
> 范围 = ① 颜色词汇表纯数据化（加颜色零代码改动，旧文档"扩白名单（代码）"
> 作废）+ 补 6 色（blue/green/purple/magenta/light_blue/cyan，项目 silver 位
> = MC light_gray → 15 染料全）；② `BlockModel::Shape::Cross` 十字植物渲染
> （2 交叉竖直面片，透明 pass 双面）；③ biome `decorations` 表面装饰
> （仙人掌/花进世界）+ lapis 矿脉；④ 内容全 JSON + 11 atlas tile + 7 图标。

## 阶段报告

- **`[2026-10-12, Phase 45]` 染色补全 + 十字植物渲染**
  — 813/813（+25），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 19 次结构性
  免疫免重锚 + S2 存档×2 字节一致 + S3 新色游戏内染色精确）。

### 实现内容

**颜色词汇表纯数据化（核心零改动）**
- 染料颜色名 = 任何 `dye_<color>` 物品注册即扩展；init 交叉校验
  （每个 (dye_base, 已有染料颜色) 组合的变体块必须存在）天然覆盖新色——
  加 6 色 = 6 染料物品 + 6 wool 变体块 + 6 配方 + 纹理/图标，**零代码**。

**Cross 形状（`block/block.{hpp,cpp}`）**
- `BlockModel::Shape::{FullCube, Cross}`；`BlockTexture::cross` 单纹理字段。
- parseModel 严格：cross 只收单个 `cross` 键（top/side/bottom 互斥拒绝）；
  full_cube 带 `cross` 键 = typo 拒绝。
- registerBlock 强制 cross ⇒ solid=false + opaque=false + transparent=true
  （可穿过、不遮光、走混合通道——属性给错 = 启动报错）。
- `requiredTextures()` 含 cross 纹理（atlas 交叉校验覆盖）。

**十字植物渲染（`world/chunk_mesher.{cpp,hpp}` + `entity/item_mesh.cpp`）**
- 面循环（`meshFaceDirection`）跳过 Cross 块（不出 cube 面）。
- `mesh()` 固定格序 cross 通道：每格 2 条交叉竖直面片（格的 2 条对角线，
  全纹理宽、纹理正立【底边 = tile v1，与流体/立方体面约定一致】），
  **无邻居剔除**（面片在格内，被埋时深度测试自然遮挡 = MC 同款），烘焙
  **自身格**光，法线朝上 = 顶点着色器面 shade 表 1.0（MC 十字植物全亮）。
  进 transparent 子网格；透明 pass 关 culling → 双面可见。
- `makeItemCubeGeometry` 对 Cross 块输出同款中心对齐两面片（掉落物/
  hotbar/hand 3D 图标；UI/hand/掉落物路径 blend 均已开，alpha cutout
  安全）。alpha cutout 无 discard = 与 leaves/glass 同类既有简化
  （KNOWN_ISSUES 渲染新增条目）。

**biome 表面装饰（`world/biome.{hpp,cpp}` + `world/world_generator.{hpp,cpp}`）**
- biome JSON 可选 `decorations: [{block, probability}]`：严格校验
  （数组、未知键拒绝、block 非空串、probability ∈ [0,1]）；block 名在
  世界生成 setup（`WorldGenerator::create`）对 BlockRegistry 解析，
  未知块名 = 启动报错。
- `placeDecorations`：树的同一格网（`trees.lattice`，锚世界 0）、专用
  salt `kSaltDecoration`（0x...7A06）、每 (格, 条目) 对一次独立
  `hash3(wx, wz, entryIdx, seed ^ salt)` roll（装饰 roll 与树 roll 不相关）；
  落表面格正上方一格；前置 = 高于海平面 + 表面格仍是该 biome 的 surface
  块（洞穴啃掉的不放，防悬空）；`placeTrees` **之后**跑（树干/树冠占格时
  `placeIfAir` 让位，装饰丢弃——placeTrees 先写的赢）。

**内容（全数据 + 生成器）**
- 5 新方块：`lapis_ore`（hardness 3，掉 `lapis`）/ `cactus`（hardness 0.4，
  掉自身）/ `allium`·`tulip`·`cornflower`（cross，hardness 0，非 solid/
  opaque，transparent，掉自身）。
- 1 显式物品 `lapis`（material，max_stack 64）+ 6 染料物品 + 6 wool 变体
  块 + 6 配方（蓝←lapis / 绿←cactus / 紫←allium / 品红←tulip /
  浅蓝←cornflower / 青←lapis+绿，MC 1.14+ 同款）。
- worldgen：`lapis_ore` 矿脉（probability 0.08 / vein_size 6 / max_y 32）。
- biome 装饰：desert cactus 0.03；plains 3 花各 0.02；forest 3 花各 0.012。
- 纹理：11 新 block tile（`tools/generate_block_textures.py`：lapis_ore
  蓝斑点 ore / cactus 绿棱 / 3 花 = 透明底 + 茎 + 花头剪影，salt 61-65）+
  atlas 落空位 (4..5, 2..7)（**无 UV 移位**）+ 7 图标（6 染料色板 +
  lapis 宝石簇，`tools/generate_item_icons.py`）。
- **salt 回归修复**：wool/dye 纹理工具的 salt 原按 `sorted(palette).index`
  算——加色后既有色 salt 移位、**7 个 wool + 8 个 dye 既有 PNG 静默改变**
  （基线门没抓到 = 出生点视野无 wool/dye）。改显式 per-name salt 表
  （既有色保持原值、新色取后续空位）+ assert 覆盖；重生成后 git status
  零既有 PNG 修改，基线门复验通过（PITS 新坑）。

### 测试（+25 → 813）

- test_block（+9）：cross 解析 / 形状默认 / 未知 shape / cross 缺纹理 /
  cross 带 cube 键 / cube 带 cross 键 / 属性强制 3 种（opaque/solid/
  transparent）/ requiredTextures 含 cross。
- test_biome（+6）：decorations 解析 / 缺省空 / 非数组 / 未知键 /
  缺 block / 坏 probability。
- test_world_generator（+5）：花坐 biome 表面（含海平面/格网/lattice
  不变式）/ 仙人掌只在沙漠沙面（seed 28——seed 42 区域无沙漠列）/
  装饰恰高于表面 1 格 / lapis 矿脉出现且 ≤ maxY / 未知装饰块名拒绝。
- test_chunk_mesher（+5）：两面片 8 顶点 12 索引全进 transparent pass /
  对角角落点精确 / UV 全覆盖且正立 / 自身格光 / 不剔除邻居 cube 面。

### E2E（批跑，全绿；`/tmp/opencode/p45_e2e.zsh` + `p45_s3_only.zsh` 一次性）

- **S1 base18 门**：P45 ×2 + **P44 代码参考构建（worktree @c91676c，ref
  从自己 worktree 跑——P45 改资产，P44 二进制对 `shape: "cross"` 硬错误
  = PITS 新坑）** ×1 新世界 60f **三方字节一致 = 第 19 次结构性免疫
  （免重锚）**【lapis 矿脉在地下、花/仙人掌不在出生点相机视野内】+
  skygate + 红心 0。纹理 salt 修复后复验 ×2 依然字节一致。
- **S2 回归**（p27far 夜 360f seed 9）：P45 ×2 存档字节一致（确定性权威）
  + 截图字节一致 + health 8（同 P44——花未阻塞本场景 skeleton 生成环）+
  blocks [] / furnaces []。**无跨构建对比**：worldgen 变了（新装饰/矿脉），
  P44 构建的 seed 9 世界差异 = 真实内容非噪声（文档化）。
- **S3 染色输入运行**（正午 still mobs off、300f；源存档 = p27far 派生
  fmt7【**空 `furnaces` 数组必填** = PITS 新坑】+ 白 wool 块 (22,45,−2)
  + slot0 dye_blue×1 + 瞄准面中心 pitch −0.64136【P44 S4 几何复用】）：
  锁指针 → 右键：存档**精确**（(22,45,−2) = **wool_blue**【新色游戏内
  染色全链路：item → 交互 → setBlock 换块】、dye 消耗、yaw/pitch 零漂移、
  20/20）+ 红心 670>50【输入运行 HUD 渲染 = 输入生效正向信号；无输入
  对照 0 = 无 HUD，语义按运行类型分 = PITS 新坑】+ 世界区 87,414 px 差
  vs 对照（重染色 + HUD）+ 对照存档 wool 未动 + skygate。

### 已知问题（P45 新增/变更）

- KNOWN_ISSUES「染色」首条（无蓝/绿/紫来源）**已修复**（strikethrough +
  P45 说明）。
- KNOWN_ISSUES「渲染」新增：alpha cutout 无 discard（十字植物与
  leaves/glass 同类既有简化）。

### 提交

- 显式路径列表提交（70 文件：9 src + 4 tests + 2 tools + 29 data【5 改 +
  24 新 json】+ 18 纹理 + 7 docs + AGENTS.md；新文件先 `git add <path>`；
  `git commit -F`）。
- `SESSION_CONTEXT.md` 本地专用不入库。
