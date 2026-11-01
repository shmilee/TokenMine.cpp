# GEN-9 耕地湿润（v1.8.0 候选，门 123）

v1.x 任务矩阵第八项（docs/BACKLOG.md，GEN 行 3/3 收尾）。原始定义已佚
（v1.0 砍掉清单只剩名称"耕地湿润"），代理定案最小完整版 = **耕地湿/干
视觉状态（MC farmland wet/dry）**：土壤格（耕地自身格）同层 Chebyshev
半径 4 内有水（level ≥1）时，耕地顶面渲染更暗的"湿"纹理，否则渲染原
犁纹（干）纹理。

## 定案

- **语义 = MC 远mland 湿/干**，规则**复用 P50 生长节奏的同一谓词**：
  水（仅水，熔岩不湿 = P54）在土壤格同 y、Chebyshev 距离 4 内 = 湿。
  `soilIsWet(world, soilPos)` 上提 growth.hpp 公开（kWetWaterRadius 4
  同款），`cropIsWet(cropPos)` = `soilIsWet(cropPos − 1y)` 委托——**生长
  节奏与视觉状态一函数两消费者，永不打架**。
- **实现路径 = 块模型字段 `moisture_top` + mesh 时从流体场派生 + 游戏侧
  周期 sweep 收敛**（三件配套，缺一不闭环）：
  1. **模型字段** `model.moisture_top`（严格非空字符串；**FullCube-only
     形状门控**——slab/stairs/cross/wall/fence/glass_pane 点名拒 = 死字段
     ammo 纪律；tile 存在性走 `requiredTextures` init 交叉校验 =
     sound_material 同款）。
  2. **mesher 钩子**（meshFaceDirection，FullCube 顶面 slot 0）：
     `moistureTop` 非空且 `soilIsWet(m_world, cell)` → 顶面用湿 tile，
     否则干 tile。湿/干邻格**永不贪婪并 quad**（sameAs 键含 tile = 结构
     性保证）。掉落物立方/手持立方保持干纹理（无世界上下文，MC 物品栏
     图标同款静态，记档偏离）。
  3. **游戏侧收敛 sweep**（必要性的实证发现）：wetness 在 **mesh 时**
     读**活流体场**，而 mesh 计算跑在 **worker 快照 job** 上——耕地 chunk
     可能在邻水 chunk 流送到位前被烘（跨 chunk 流体读 = void = 烘干），
     且"tracked 湿态 == 活湿态"时 flip 门**永不触发**（tracked 存的是
     活流体场的值，不是 mesh 已烘的值）→ 纯 flip 门对"预烘 stale mesh"
     死锁。修复三件套：**① 每 kMoistureRefreshEveryNFrames = 10 帧周期
     sweep**（重评全部 tracked 格湿态、flip 才 markChunkDirty——流送到位
     /倒水/抽水全收敛，延迟 ≤0.17 s）+ **② setBlock 收口 seed 点强制
     markChunkDirty**（新放置格可能顶着 stale mesh）+ **③ rescan
     seed**（读档后首评）。sweep 成本 = |tracked| × 81 次流体查询/10
     帧，**无耕地世界（fresh/性能场景）空 map fast-exit = 结构零成本**。
- **追踪集** `m_moistureBlocks`（map<BlockPos,bool>，位置序确定性迭代，
  m_crops/m_saplings 同款范式）：setBlock 收口按 `model.moistureTop` 非空
  数据驱动进出（核心零方块 id 切换）；读档尾 rescanMoistureBlocks 重建。
- **shipped 内容**：`farmland.json` 加 `model.moisture_top:
  "farmland_wet"`；atlas 空位 **[9,4]**（farmland_top 在 [7,4]，空位 = 无
  UV 移位 = 免重锚）；`farmland_wet.png` = 同 salt（90/91/92）暗化 0.72×
  （均值 71.8/52.0/35.6 vs 干 101.5/72.4/48.6）= "同一片田、湿了"，
  犁纹行（y%4==0 ×0.72）结构保留；生成器幂等（74 既有 PNG md5 全同）。
- **零 format（19）/零 config/零新音效**：湿态是世界派生量（block +
  fluid 编辑的函数），存档只含两侧 A/B 相同的 block/fluid 编辑 →
  **savenorm EXACT = 强 oracle**。

## 实现

10 文件：`block/block.hpp`（BlockModel.moistureTop）+
`block/block_registry.cpp`（parse 严格化 + 形状门控 + requiredTextures）+
`world/growth.{hpp,cpp}`（soilIsWet 公开 + kWetWaterRadius 上提 +
cropIsWet 委托）+ `world/chunk_mesher.{cpp}`（FullCube 顶面钩子）+
`core/game.{hpp,cpp}`（m_moistureBlocks + 收口 + rescan + sweep 常量 +
load 钩子）+ `assets/data/blocks/farmland.json` +
`assets/data/textures/block_atlas.json`（[9,4]）+
`assets/textures/blocks/farmland_wet.png`（新）+
`tools/generate_block_textures.py`（farmland_wet 函数 + 注册）+
CMakeLists.txt / test_debug_overlay.cpp（版本 1.8.0）。

## 测试（1745/1745 = 1733 + 12，-Werror 0 警告）

- block 解析/内容 6：moistureTop 缺省空 / 解析成功 / **6 形状点名拒**
  （slab·stairs·cross·wall·fence·glass_pane，cross 用正确纹理单独复测 =
  拒因确为 moisture 字段）/ 空串 + 非字符串拒 / **shipped farmland 内容
  钉死**（textures.top=farmland_top + moisture_top=farmland_wet + atlas
  [9,4] + PNG 存在）。
- growth 谓词 4：半径 4 含边界（角点 (dx4,dz4) 湿 / 距离 5 干 / 抽干复
  干）/ 仅同 y（上下一格水均不湿）/ 熔岩不湿 / **cropIsWet ≡
  soilIsWet(−1y) 等价**（两消费者一致性钉死）。
- mesher 3：湿 → 顶面 4 顶点 UV 全落湿 tile 矩形（且不在干 tile）/ 干 →
  反之 / **湿干邻格不并 quad**（2 相邻耕地 + 水距 4/5：顶面 8 顶点 =
  4 湿 + 4 干两 homogenous quad）。
- 无 Game 级单测（Game 构造依赖窗口 = 项目现状）——sweep/收口逻辑由 E2E
  + 谓词/几何单测合围覆盖。

## E2E 门 123

**S1 = 第 123 次结构性免疫，免重锚**（final binary 复跑）：fresh 60f
四方 ×2 全 387726 B 字节一致（new ≡ old v1.7.0 ≡ base18）+ savenorm
EXACT。论证：farmland 永不 worldgen（ENT-2 init 不变量外，生成器零
farmland 引用）+ 默认热栏/隐藏 starter 槽无 farmland + 无输入零放置 →
fresh 世界零 moisture 块 → m_moistureBlocks 恒空 sweep 惰性 + mesher
钩子零执行 + 新 atlas tile 无人采样 → 位级恒等。

**S2 = 受控平台场景**（石平台 y44 13×13 + 湿耕地 (22,45,-8) + 干耕地
(18,45,-8) + **封源** L8 水 (25,45,-8)【4 石封 P64 先例——L8 源在本项目
会流动，240f 实测平台扩散 71 格，不封则流到距干耕地 3 格 = 对照失效】
+ time 0.25）：

- **S2a 俯视**（玩家 (22.5,46,-7.5) 站湿耕地上 pitch −1.5 直俯，600f
  无输入）：new×2 + old×2 RAW + **A/B diff 108,542 px 全限中央耕地
  顶面带**x[513,848] y[248,577]+ old
  侧 = 干 tilled（102,72,49）/ new 侧 = 湿纹理（72,52,36 = 纹理均值精确吻合）+ 100% diff px new 更暗。
  存档 = **场景区 EXACT**（blocks 175=175 + fluids 1=1 封源水 + player 位/yaw/pitch/hp 逐值同）+ 远端
  程序化熔岩湖（x81-84 z73-77）real-dt churn 5 格【同二进制 ×2 同位分歧
  实证 = PITS 244/283 非缺陷类，帧外零影响：×2 RAW 字节一致】。
- **S2b 侧视**（玩家 (28.5,45,-7.5) 平台东 6 块 yaw +π/2 pitch −0.10，
  600f 无输入）：new×2 + old×2 RAW + **A/B diff 888 px 限湿耕地顶面** x[633,728] y[369,378]
  （红纹理判别帧 1,618 px x[623,738] y[343,380] 同区；old = (99,71,48)
  干 tilled / new = (70,51,35) 湿）+ **干耕地带零 diff（隔离：diff 全在
  y≥369，干耕地顶面 y~349 带外）** + 100% diff px new 更暗 + 存档
  场景区 EXACT 同款（远端熔岩湖 churn 非缺陷类）。
- 红纹理判别实验（临时 farmland_wet.png = 纯红，测毕还原 md5 验）：
  俯视 new 中心纯红 (255,0,0) 108,542 px / old 中心干 tilled——功能 E2E
  首证；侧视红斑 1,618 px 精确隔离——两视角均证。

**大场景 quirk（非缺陷，记档）**：初版 S2 用自然地形场景（湿耕地
(22,37,-8) + 封源水 (26,37,-4) + 相机 8.5 块侧视 + 走廊挖通），四版
场景迭代（v3-v6）中湿耕地顶面**新旧两侧均不渲染**（帧字节一致 = 零
GEN-9 回归证据，old 二进制同样受影响）：CPU mesh 日志实证湿 quad 发射
（tile (9,4) ×2，QUAD-DBG 临时探针，测毕删）+ 世界 dump 实证视线盒全
空 + 受控场景全正常 → GL 列 mesh 更新时机 quirk（该场景配置特异的
demo 列 mesh 集成滞后），与 GEN-9 变更无关（old 侧同现）→ 门改用受控
平台场景，quirk 记 PITS。

## 性能 A/B（发布前必做）

tm_v170_perf（GEN-8 代码 = v1.7.0 行为参照）vs tm_v180_perf（GEN-9
最终代码插桩），同 seed（perf_ab17 剥 fluids 重生 seed）fall 900f 三窗：
**fluidCells 应逐位一致**（GEN-9 零 worldgen 改动 = 同世界，比 GEN-8
的 +1.1% 世界演进更强的 oracle）+ 全指标同档（场景零耕地 →
m_moistureBlocks 空 sweep 结构惰性 = 零成本）+ 每流体格成本同档。
**PASS = 成本可忽略**（tm_v170_perf GEN-8 参照 vs tm_v180_perf 最终代码，
同 seed（perf_ab17 剥 fluids 重生）fall 900f 三窗，**PITS 15 修正：每次跑前
重 cp 干净 seed**【首轮未修正 = v180 侧加载 v170 终档 = 起跑态不同，
win2/3 +100 漂移即该污染，重跑后消除】）：
- **fluidCells 51922→51926（win1 +4 = 流送填充期 real-dt 类 PITS 283）/
  29803 = 29803 / 29800 = 29800（win2/3 逐位一致）**——GEN-9 零 worldgen
  改动 = 同世界，比 GEN-8 的 +1.1% 世界演进更强的 oracle 兑现；
- **PIPE3-S0 逐位一致**：visitedChunks 268=268 / fluidCellsSeen
  43994=43994 / changesApplied 1404=1404（tick med 5.5/5.7 ms 同档）；
- upd/render/swap 三窗全同档（med ±15% 内，max = 填充噪声）；
- **零耕地场景 = m_moistureBlocks 恒空，sweep 结构惰性实证**（fluid 段
  无新增成本）；
- 终档对照：blocks 450 vs 452（远端熔岩湖 real-dt churn 2 格，PITS 244
  类）/ fluids 10623=10623 差 1 格 = 非回归。

## 偏离/记档

- **二元湿/干**（MC farmland 是 0-3 四级 moisture 按水距衰减）：本项目
  复用 P50 生长节奏的二元半径 4 规则 = 视觉与生长同规则，简化记档。
- 掉落物/手持立方 = 干纹理（无世界上下文；MC 物品栏图标同款静态）。
- 视觉收敛延迟 ≤ 10 帧（0.17 s）= sweep 周期（MC 状态切换即时；本项目
  帧锁定纪律下 mesh 派生量的固有节奏，倒水/抽水/流送全路径覆盖）。
- 无 MC"水道（channel water）"机制（水渠 + 0.25/块衰减湿润半径 =
  矩阵外）。
- 熔岩不湿（P54 生长规则同款）。

## 坑位（PITS 追加）

- **PITS 291：场景 L8 水源必须 4 石封**（第 3 次实证）——本项目 L8 源
  非 MC 静态源，会向邻格流动（level 7,6,5…）；耕地湿润场景不封源：
  v3 水淹相机走廊（A/B diff=0 假象）+ 平台场景 240f 扩散 71 格、2 格
  到干耕地（对照失效）。封 = 4 水平邻石（P64 高架水 / P75 浇灭场景
  同款）。
- **PITS 292：大场景远景湿耕地顶面 GL 不更新（新旧同现 quirk）**——
  自然地形场景（相机 8.5 块侧视 + demo 列）中湿耕地顶面恒不渲染：CPU
  mesh 日志证湿 quad 已发射（tile (9,4)）、世界 dump 证视线盒全空、
  红纹理判别证帧内零湿采样、old 二进制帧字节一致（非 GEN-9 回归）；
  受控平台场景（俯视/侧视近景）全正常。= 该场景配置的 GL 列 mesh
  集成时机 quirk；门改受控场景，quirk 留 KNOWN_ISSUES 候选（非 v1.8
  阻塞：实机玩家视角 = 近景站立/行走，受控场景覆盖的行为面）。
- **PITS 293：zsh 无引号 `${var:-word}` 不分词**——batch 脚本 inline 跑
  在 zsh（shell 工具默认）：`${scene:---new --save tag}` 展开为**单个**
  词 `--new --save tag` → 游戏 CLI "unknown argument" 打 help（日志
  特征 = 整段 help 文本 + sz=0 非黑窗 22106）。gate 脚本一律写 .sh 文件
  `bash` 跑 + 显式 if/else（gate123_s1s2.sh 范式）。
- **PITS 294：场景 python 构建器"静默未写入"**——v5 走廊 air 编辑
  12 条构建输出丢失（heredoc 与 cmake 命令链混排），场景文件实为 v4
  内容 → 后续两轮 A/B diff=0 误判（v4/v5 同尺寸帧 432583 B 即线索）。
  纪律：场景文件构建后**必须从磁盘重读断言关键编辑在档**
  （`assert (23,37,-8) in ...` 范式，v6 落实）。
