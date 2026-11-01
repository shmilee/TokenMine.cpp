# Phase GEN-2 — 树生长（v1.0 任务表 8/29）

> 设计案 = ora-2（oracle）定稿 + 代理开放问题拍板（用户夜间授权"按建议的来"）。
> 依赖 ENT-2（BlockState 通用 per-cell byte 状态，format 15）。

## 一句话定案

**3 树种可种植 sapling**（oak 复用 wood/leaves + spruce/birch 新块；每树种
独立块 id，皆非 worldgen 放置 → 与 ENT-2 init 不变量天然兼容）+ **m_crops
范式平行生长引擎**（`m_saplings` 位置集 + 纯函数 `saplingGrowthSteps`，stage
= ENT-2 状态 byte 0-7，`tree` 对象 6 字段数据驱动，stage 满 + roll 过 + 空间
够 → 纯函数 `planTree` 多格落树，放不下 = 等待重试【MC 同款】）+ stage 视觉
= emitCross 按 `stateCount>0` 门控 quad 缩放 (stage+1)/stateCount（既有 cross
零变化）+ **世界生成森林保持现状单橡种零触碰**（免疫铁证）+ sapling 来源 =
初始背包隐藏槽 3 种各 3 颗（hotbar.json 新 `inventory` 段，非 hotbar = 不渲染
= 免疫）+ leaves 掉落闭环；atlas 9 新 tile 走 45 空位（免重锚）；**门 86 =
结构性免疫不重锚**；新增单测 ~21 条，E2E 今晚 10 min 无输入四场景，晨间补
种植输入项。

## 开放问题拍板（设计 §10，按推荐通过）

- **Q1 野外多树种 = 今晚不做**（免疫铁证 + 10 min 预算；形状纯函数已抽好，
  未来项 = worldgen trees 段 species 数据 + 选择 roll + 演示区 clear 列强制
  橡 + 重锚门）。文档化偏离："野外森林单橡种（MC 混生）；树种差异经种植
  落地"。
- **Q2 starter 库存 = 3 种各 3 颗，存储槽 24/25/26（隐藏 = 免疫）**。
- **Q3 growth_ticks = 120**（2 s/阶段 ≈ 16 s/棵树，项目压缩先例）。

## 架构 in/out 清单

### In

1. **3 树种内容**：oak（trunk=wood / leaves=leaves 既有块复用）、spruce、
   birch（各新 trunk/leaves/sapling 3 块 = 6 新块 + 3 sapling = 9 新块）。
   树种 = 独立块 id 路线（P89/P48 先例）——不用状态区分树种（stateful 块进
   不了 worldgen；树种是内容维度不是实例维度）。
2. **sapling 块 ×3**：Cross + `state_count: 8, state_default: 0` + `tree`
   对象 6 字段 + `plant_on: [dirt, grass]` + `growth_light: 9` +
   `growth_ticks: 120` + drops self + flamable。
3. **生长引擎**：`m_saplings`（map<BlockPos,BlockID>，m_crops 平行）+
   `saplingGrowthSteps` 纯函数（growth.cpp 第二函数，新盐）+ Game tick
   （作物 tick 后）+ 收口注册/卸载保护/读档 rescan（m_crops 四件套同款）。
4. **落树**：`planTree(height, spec)` 纯几何（3 形状表）+
   `treeHeightRoll(seed, pos)` 纯高度 roll + `placeGrownTree`（fits 检查 →
   逐格 Game::setBlock → 1 次 updateRegion over bbox）。放不下 = 本 roll 不
   完成等待重试。
5. **stage 视觉**：emitCross 缩放门控 `stateCount>0` → quad 角落绕格心缩放
   (state+1)/stateCount（wheat/torch 等无状态 cross 恒 scale 1 = 零变化）。
6. **内容闭环**：3 种 leaves 的 chance_drops += 对应 sapling @ 0.05（oak
   leaves = P55 集演进 +1 条目）+ **hotbar.json `inventory` 段**（3 sapling
   各 3 颗入槽 24/25/26；解析校验：slot [9,26] / 项存在 / count
   [1,maxStack] / 重复槽拒）。
7. **纹理/atlas**：9 新 tile（3 sapling cross + 4 trunk side/top + 2 leaves）
   入空位（免重锚）+ generate_block_textures.py 新 salt（既有零变化）+ per-
   block 物品图标 PNG。**空位分配（已核 49 空位）**：oak_sapling (10,1) /
   spruce_sapling (11,1) / birch_sapling (12,1) / spruce_trunk_side (13,1) /
   spruce_trunk_top (14,1) / birch_trunk_side (10,2) / birch_trunk_top (11,2) /
   spruce_leaves (12,2) / birch_leaves (13,2)。
8. **文档**：CONTENT_GUIDE（tree 对象 + sapling 生长）、MECHANICS（树生长节）、
   KNOWN_ISSUES（偏离条）。

### Out（不做 + 理由）

- **worldgen 野外多树种**：生成森林视觉变化 → 必然重锚 + 预算内不可验证 →
  未来项（Q1）。
- **m_crops 引擎泛化合一**：步类型/数据字段/注册谓词皆不同，无第三消费 →
  YAGNI；平行引擎共享 roll 纪律。
- **树种配方**（MC 无 sapling 配方 = 同款）。
- **新树种进斧 fast_against**（P67 契约演进 = 独立未来项；偏离：斧对新木无效）。
- **sapling 土壤被挖后自毁**（MC 掉；项目 = 生长暂停等待，文档化）。
- **8 张 stage 纹理**（项目 = 1 纹理缩放，atlas 预算 + 视觉足够）。
- **spruce/birch leaves 掉苹果**（apple = oak 专属保留）。

## 内容定义

### tree 对象（properties 级嵌套对象，chance_drops entry 先例）

`{ trunk, leaves, min_height [1,16], height_range [0,8], leaf_radius [1,2],
shape ∈ {oak, spruce, birch} }`——严格 6 键逐键点名。init 交叉校验（beacon_
effects 范式）：trunk/leaves 名存在；**tree 存在 ⇒ state_count ≥ 2**；
stateful 块进 worldgen 放置源 ⇒ ENT-2 init 不变量点名拒（sapling 首验）。
生长土壤门**复用 `plant_on`**（放置门 + 生长门一体）。

### 9 新块要点

| 块 | 关键数据 |
|---|---|
| oak_sapling | cross, state_count 8, plant_on [dirt,grass], growth_light 9, ticks 120, drops self, flamable, tree{wood, leaves, 4, 3, 2, oak} |
| spruce_sapling | 同型, tree{spruce_trunk, spruce_leaves, 6, 3, 2, spruce} |
| birch_sapling | 同型, tree{birch_trunk, birch_leaves, 5, 2, 1, birch} |
| spruce/birch_trunk | cube 实心 opaque, hardness 2.0, drops self, flamable |
| spruce/birch_leaves | transparent, light_opacity 2, hardness 0.2, drops self, chance_drops [sapling 1 @ 0.05, stick 1 @ 0.02], flamable |
| leaves（唯一既有编辑） | chance_drops += oak_sapling 1 @ 0.05（stick/apple 不动 = P55 演进） |
| hotbar.json（唯一既有编辑） | + inventory 段（槽 24/25/26） |

可燃集钉死演进：10 → **17**（P63 精确集断言更新）。

### 树形状表（planTree 纯函数，root = sapling 格 = trunk 底）

trunk = (0,i,0) i ∈ [0,T-1]；冠层 (offset, radius)：oak = (T-2,r),(T-1,r),
(T,1)【与 placeTrees 逐格同构】/ spruce = (T-2,2),(T-1,1),(T,1),(T+1,1)
【下宽上尖锥】/ birch = (T-1,1),(T,1),(T+1,1)【细高小冠】。(0,0) 跳过 =
offset ≤ T-1 时；角圆化 = radius 2 且 |dx|==r 且 |dz|==r。r 仅 oak 生效
（shape 决定表，radius 上限校验；解析仍要求 [1,2]）。

## API 面

| 层 | API |
|---|---|
| world/tree_shape.{hpp,cpp}（新） | TreeSpec / TreeShape{Oak,Spruce,Birch} / planTree(int h, TreeSpec) → vector<TreeCell{offset, trunk?}> / treeHeightRoll(seed, pos)（纯，新盐） |
| BlockProperties | + TreeGrow 嵌套对象解析 + hasTree()；tree 存在 ⇒ state_count ≥ 2 |
| growth.{hpp,cpp} | + SaplingStep{pos, Action{Advance,Complete}, stage, height} + saplingGrowthSteps(world, blocks, saplingCells, seed, frame)（纯：plant_on 土壤门 + growth_light 门 + roll%ticks；Complete 附 fits【全 air 无流体 y 界内】+ heightRoll） |
| Game | + m_saplings + rescanSaplings + tick（Advance → setBlockState 无 updateLocal【状态不改光】；Complete → placeGrownTree）+ placeGrownTree（trunk 先冠后逐格 setBlock + 1 次 updateRegion bbox） |
| chunk_mesher emitCross | stateCount>0 → scale = (state+1)/stateCount（哨兵 → stateDefault） |
| hotbar 装载 | + inventory 段解析（严格 {slot,item,count}） |

## 与 ENT-2 的交互

- 依赖：sapling = stateful 块（state_count 8）→ GEN-2 后于 ENT-2 落地（行序 7→8）。
- 存档：sapling 格 = blockEdits + stage = format 15 states（journal 派生直接
  覆盖：位置靠放置入 block journal、值退出 live 读——stage 推进无 block 写 ✓）。
- 收口：sapling 放置 → ENT-2 设默认 0 + GEN-2 注册 m_saplings（并列）；落树 →
  两清。
- **旋转副作用（定）**：潜行右键 sapling = ENT-2 旋转泛化 → stage +1（手工
  催熟，确定性）——**接受为特性**（E2E/调试加速器），文档化；不加
  state_rotates 逃逸字段（YAGNI）。
- 光照：stage 推进零光照成本；落树 = 1 次 updateRegion（冠影 = LGT-2 可见）。

## 测试计划（~21 新）

- 解析/内容（test_block）：tree 6 键合法+访问器 / 逐键缺失·未知·shape 越词汇
  ·越界点名 / tree 无 state_count 拒 / trunk 名不存在拒 / 真实资产钉死（3
  sapling spec / 2 trunk / 2 leaves light_opacity 2 + chance_drops / oak
  leaves 掉落集 / 可燃 17 块精确 / inventory 段 5 例）。
- 纯函数：planTree 3 形状精确格集（oak 与 placeTrees 换算对照）/ heightRoll
  确定+值域+分布 / saplingGrowthSteps 推进（ticks 1 恒过 + 帧精确 roll 预
  计算）/ 三道门（土壤 stone 无步 / light 8 无步 / **light 11 冠下值有步**）
  / Complete fits（应用后格集精确 + states 无残留 + m_saplings 清）/ 天花板
  阻挡（等待→移除后续 roll）/ 水阻挡 / 跨 chunk 落树（双 chunk 格 + 脏）。
- mesh/存档：emitCross 缩放（stage 3/8 顶点钉死 + wheat scale 1 回归）/
  PIPE-1 快照带 stage（顶点 == live）/ sapling 存档往返（stage 4 保持 +
  rescan 注册 + 续生长）/ 落树存档（纯 blockEdits 无 states）/ 收口
  （注册/破坏清+掉 self）。

## E2E 计划（今晚 10 min 无输入 + 晨间输入）
- **S1 门 86（免疫）**（~2 min）：fresh 60f ×2 + **与门 85 锚字节一致** +
  红心 0 + skygate。论证 = 门 86 节。
- **S2 落树场景**（~4 min）：探针选 grass 格（light 15 / plant_on 过 / 上方
  T+1 全 air / 演示区外）+ sapling 预置 **stage 7**（states 条目）+ 预计算
  completion roll 通过帧 ≤ 600 → 600f no-input ×2 RAW + savenorm ×2 + EXACT
  （trunk/leaves 格在、sapling 格 = trunk、states 无该格）+ 截图树 bbox
  trunk 棕 + canopy 绿像素计数。
- **S3 推进 A/B**（~3 min）：同场景 stage 0 vs stage 7 双存档 600f：stage 0
  → 终 stage N ∈ [0,7) 无树 / stage 7 → 树；A/B diff = 树区互斥判别。
- **S6 init 失败**（~1 min）：scratch worldgen biome surface = oak_sapling →
  启动点名拒（ENT-2 不变量 + 首真实 stateful 块双背书）。
- **晨间（输入）**：① 右键 grass 放 sapling（消耗 1 + stage 0 + 生长启动）
  ② 潜行右键 sapling → stage +1（旋转副作用背书）③ 破坏掉 self。×2 savenorm
  + EXACT。

## 门 86 论证（结构性免疫，不重锚）

无输入 fresh 世界：① worldgen 零触碰（placeTrees / trees 段 / 生成器代码一
行不改 = 生成世界逐格同门 85，铁证级）；② sapling 永不生成（init 不变量 +
S6 实证）→ m_saplings 恒空 fast-exit；③ starter 库存 = 存储槽 24-26 非
hotbar = 不渲染；④ atlas 9 tile 空位无 UV 移位；⑤ oak leaves 掉落变更只在
破坏时生效（零破坏）；⑥ 可燃 +7 块 fresh 零存在 → 燃烧 pass 不变；⑦
emitCross 缩放门控：fresh 零 stateful → 所有既有 cross scale 1 逐位不变；
⑧ 新图标独立 PNG 无 atlas。→ **帧 60 终图像 = 门 85 锚逐字节一致**。

## 风险清单

1. starter 库存与存档交互：旧存档 load = slots 全量替换 → 旧世界无
   spruce/birch 来源（oak 经 leaves 掉落可达）= 文档化偏离。
2. 落树 ensureColumn：跨入未加载列收口同步补列（~1.6 ms/列既有行为）= 文档化。
3. placeGrownTree 一次性 updateRegion（bbox ~5-8 ms 主线程）：玩家触发非每帧
   = 文档化。
4. oak 形状表与 placeTrees 换算漂移：单测 placeTrees 格集对照钉死。
5. completion roll 600f 不通过尾（0.7%）：S2 预计算通过帧规避。
6. 旋转副作用（潜旋 sapling 催熟）：报告披露 + 晨间背书；用户反对 → 未来加
   state_rotates 字段（不影响本相）。
7. -Werror：tree 对象 6 字段嵌套 unknown-field 拒绝。
8. 跨实现并行：引用 ENT-2 定稿名（getBlockState 等），联调 = 换名完成后首
   次构建。

## 构建与验证结果（E2E）

- **实测**：1285/1285（存量 1260 零回归 + 25 新 + 2 处内容演进断言【P55
  leaves 掉落集 2→3 条、P63 可燃集 10→17 精确】），干净重建 0 警告。
  纹理确定性 = 重跑生成器既有 PNG 零变化 + 9 新 block tile（salt 935 起
  顺延）+ atlas 双向交叉校验。**世界生成零触碰**（world_generator.cpp /
  worldgen.json trees 段 diff 空）。3 处实现偏离（任务文本层面）：无 sapling
  物品图标 PNG（block item 无 2D 图标 = 代码库约定，item_mesh 3D cross 渲染
  覆盖 UI/手持双界面，图标会是死数据）/ trunk-leaves 存在校验在
  loadDirectory 后置 pass（可单测 seam）/ 25 测试 vs 设计 ~21。
- **S1 base18 门 86 = 结构性免疫（不重锚）**：fresh 60f ×2 全帧字节一致 +
  **与门 84 锚（s1_l2c_r1.png）字节一致**（门 85/86 连锁）+ 红心 0 + skygate
  75,348 严格。论证兑现：worldgen 零触碰 + sapling 永不生成（init 不变量）
  + starter 隐藏槽不渲染 + atlas 空位无 UV 移位 + emitCross 门控惰性。
- **选点纪律（PITS #208 执行）**：gen2_scan 探针（真实 assets + 生成器 +
  propagateAll + saplingGrowthSteps 逐帧复刻，tick 相位 = m_frameCount
  0..599）预计算候选——选定 **(-40,53,-27)**（grass 顶 52 上、light 15、
  H=6、平列玩家位 (-39.5,52,-23.5)）：stage-7 预置 **f=7 落树** / stage-0
  预置 **600f 终 stage = 4**（无落树）。
- **S2 落树场景**（stage 7 预置，600f 无输入）：新 ×2 **RAW 字节一致** +
  全存档 savenorm **EXACT**（含 3 万+ 环边水编辑 = 全环确定性）；存档 =
  root 格 wood + **states []**（落树清状态）+ trunk 6 格 + leaves **49 格**
  （= planTree(H=6, oak) 精确：r2 层 2×20 + r1 顶帽 9——纯函数与 E2E 世界
  逐格一致）；截图 trunk 棕 39,241 px + 冠绿 64,566 px（树可见）。
- **S3 推进对照**（stage 0 预置，600f）：sapling 保留 + **states = [stage
  4] 与探针预计算逐帧一致**（帧无关 roll 模型 E2E 验证）+ 零树格；S2 vs
  S3 全帧 diff **324,447 px**（树 vs 中阶 sapling 互斥判别）。
- **S6 init 失败**：scratch worldgen biome surface = oak_sapling → **rc=1 +
  点名**（ENT-2 init 不变量 + GEN-2 首真实 stateful 块双背书）。
- **晨间（输入，推迟）**：① 右键 grass 放 sapling（消耗 1 + stage 0 + 生长
  启动，plgt2_s4.zsh 右键范式）② 潜旋 sapling → stage +1（旋转催熟特性
  背书，pent2_rot.zsh 同款改右键对象）③ 破坏 sapling 掉 self。
