# Phase V3-1 Phase B — 下界世界 + 真传送（v3.4.0，门 194 全 PASS）

> 设计定案 = `/tmp/opencode/v340_design.md`（D1-D9 收口）。立项调研 =
> `/tmp/opencode/v31_recon.md`（§三 Phase B 交付 8 件 + 风险面 3 点）。
> MC 机制 wiki 核定（recon §二）：8:1 floorDiv 映射（Y 不变）/ 落点
> ±16 格搜活跃 portal（下界现代值）无则生成 4×5 含角 / 下界 = 无昼夜
> 无天气 sky light 恒 0 / 熔岩海 y≤31 / 基岩顶 5 层 / 高度 128 项目
> 直接用 / 下界死亡回主世界重生。

## 实现

### 下界维度（D1 增量并行成员 / D2 惰性 / D3 仅 Player 换绑）
- `NetherDim` 聚合成员（World optional + generator + StreamingState +
  网格 map 族 + 集族 + MobSystem 空池 + entities + portals 追踪集）；
  **overworld 代码路径零 diff**（结构免疫论证根）；`DimCtx` 访问器
  统一两维流送/光照/网格操作。
- **惰性**：buildScene 不建下界（首传前零 chunk/mesh/journal/流送/
  内存）；`ensureNether()` = 首传 / 载入下界段非空 / 玩家在下界时构造
  （日志 "Nether engaged (derived seed ...)"）。
- **Entity `m_world` const 引用 → 指针**（D3 单咽喉点 `setWorld`；
  17 调用点机械适配 `world()` 解引用，无语义变化；旧世界存活 = Game
  拥有双维世界，无悬垂）；**仅 Player 跨维**（per-dimension 实体
  list，inactive 维冻结不 tick；掉落物不跨维 = 记档偏离 2）。
- **非活跃维 = 时间胶囊**（记档偏离 4）：玩家在下界时主世界
  world-pass（流体/炉子/作物/火）冻结；DayCycle/Weather 全局恒推进
  （MC 时间连续语义）。

### 下界 worldgen（D4：专属纯函数，零 biome/矿/洞/树机制）
- `fillNetherChunk`（**坐标的纯函数 = 零噪声零随机，构造性确定**）
  层带：基岩底 y[0,4] / **熔岩海 y[5,31] 全源 level 8**（静态源海 =
  流体模拟零 churn，recon 风险面 ③ 的消解）/ netherrack y[32,122] /
  基岩顶 y[123,127]（kWorldHeight 128 直接用）；命名常量带边界全
  单测钉（层带 5 组 / 熔岩海顶边 / chunk 间连续 / 负坐标 / 纯净性）。
- 派生 seed `worldSeed ^ kSaltNether`（0x5E73E74E92B1D3A1，独立盐
  纪律；**不落盘** = parse 从主世界 seed 重建，xp_seed 单源真值
  同款）。
- **netherrack 块**（纯数据）：full / opaque / hardness 0.4（MC 精确）/
  drops 自身 / light 11 透传（portal 照明）/ 音族复用 stone（记档偏离
  3：netherrack 音族 = Phase C）/ 纹理 atlas [13,7] 零既有移位
  （123→124 census 钉）。

### skyless 光照（D5：2 处参数，shader/顶点格式零改动）
- `World` 构造加 `bool skyless = false`（默认 = overworld 位级不变）：
  ① void/顶 sky 读 0（getSkyLight 的 15 → skyless ? 0）② sky rescan
  顶源 0（lighting.cpp kMaxLight → 0）。sky=0 烘焙 = dayFactor 结构
  性失效；ambient 保持 u_ambient 0.03（记档偏离 4：MC 0.1，Phase C
  可切）。

### 传送管线（D6：单帧帧锁定，无 loading screen）
- `kPortalTravelEnabled` → true（static_assert 翻转解锁 Phase A 桩）+
  **`kPortalCooldownFrames = 900`**（15 s 帧锁定，防落点在框内立即
  反向振荡 = 记档偏离 3；**会话态不入存档**——重载即冷却清零，门 S2
  返程臂实证）。
- 纯函数族（core/portal，单测拥有）：`netherToOverworld`（×8 逆映射，
  往返 7 组含负坐标）/ `portalTravelDestination`（**±16 格搜活跃
  portal**【valid frame 判据 = 孤儿框不作落点】含 Y 全高盒，最近 =
  框底中距离；无 → **生成 4×5 含角框 + 2×3 基岩平台**（y0−1，框底
  触地则无平台））/ `portalPlatformPlan` / **正交 portal yaw −π/2**
  （MC JE 顺时针 90° = 项目正 = 左转约定下的 −π/2；同轴保留 yaw；
  wiki 双转 180° quirk 自然继承）。
- 落点安全：hitbox 探测 + step-up 上探（单测钉）；restorePosition
  （fall pin 防载档空中免伤同款）。
- Game 侧（travelPortal 单帧）：ensureNether（按需）→ 目标维同步
  catch-up（3×3 列盘 first-touch 放大）→ setWorld 换绑 + 落点 + 相机
  → m_playerDimension 翻转（流送中心 = 玩家位，下帧自动 engage）→
  音效 portal_trigger（进入）+ portal_travel（抵达）2 件新 wav
  （确定性 LCG，既有 47 件 md5 不变，SoundBank → 49）→ 日志
  "Traveled to <dim> at (...) from (...)"。

### 渲染维度门控（D7：5 点，零 shader 改动，主世界路径默认 off）
- clear 色 = kNetherSkyColor（0.16, 0.04, 0.04 暗红）/ 日月星不画 /
  云不画 / 雨不画 / 雾 10→96 块（kNetherFogRange 命名常量）+ F3
  dimension 行（19→20 行钉）。

### 下界行为收口（D8 + 记档偏离族）
- 下界死亡 → **主世界重生**（respawnPlayer 玩家在下界时先 rebind OW +
  catch-up；MC 精确）。
- 7 个 per-block state 族主世界 only（furnace/campfire/table/crop/
  sapling/moisture/beacon/fire）；portal 追踪 per-dimension；容器面板
  主世界 only；**床睡眠下界 no-op**（MC 床爆炸无 TNT 机制，跳过）；
  **下界火恒定不烧尽**（偏离 7：下界 = 火的世界，burn-out 计时是
  主世界行为）；**下界水桶泼水 = no-op**（偏离 8：下界流体仅岩浆
  全源，水无下界语义，维度感知 FluidSim = Phase C）。
- 流送：nether StreamingState 复用既有流送函数（DimCtx 参数化，共享
  线程池 = 主世界者）；inactive 维零 post（稳态零争用）；
  editedColumns/retained/journal 全 per-dimension。

### format 30（D9）
- `player.dimension`：严格词表 "overworld"|"nether"，**恒写**（29 缺 =
  overworld 迁移 velocity-zeroed twin / **29 携 = version lie 点名** /
  30 缺 = 点名 / 未知词 = 点名带词表）；玩家 pos/vel/yaw/pitch =
  **当前维度坐标**（y ∈ [0,128) 校验既有）。
- `nether` 段：闭集 {blocks, fluids, states}，**恒写**（空数组合法；
  **29 携 = version lie**）；`nether_entities`：item kind 形状恒写空
  合法（xp kind = Phase C+）。
- 交叉检：nether.blocks 块名注册（点名）/ **nether 内 stateful 块须
  nether.states 条目**（PITS 383① 同款双向）/ 两维 portal 载入
  rescan（孤立 → air）。
- 载入时序：OW 生成 →（下界段非空 ∨ 玩家在下界）→ ensureNether →
  nether edits 重放（applyWorldEdits 第二调用）→ 两维 rescan → 玩家
  rebind + catch-up。**gzip 编码零改动**（V3-7 容器自动覆盖 format 30）。

## 门 194（结构免疫第 75 次；锚 224468 B 免重锚）

**结构免疫论证 = 最强形**：下界惰性未建（首传前零 chunk/mesh/journal/
流送 job/内存；drain 的 `if (m_nether.world)` 快退 = 无 nether 会话
逐位改前序列）。

- **S1** 无输入 60f ×2：截图 ≡ 锚 224468 逐字节 + savenorm EXACT +
  **终档恰增白名单 4 键**（format 30 / player.dimension="overworld" /
  nether={blocks:[],fluids:[],states:[]} / nether_entities=[]）。
- **S2 双跳臂（全 headless——站入姿态在场景内，帧锁定倒计时零输入
  协议）**：
  (a) **传送**：activated 场景 + stand 姿态 120f → frame 60 "Traveled
  to nether at (1, 55, -1) from (0, 55, -2)"（8:1 映射验算）→ 下界
  截图 ×2 RAW 恒等（netherrack 岩洞 + portal 光 11 紫晕 + 暗红雾 =
  首访观感实证）+ 终档：dimension=nether / pos = **[1, 55, -0.5]
  精确**（落点纯函数输出）/ nether 段 26 块 = 生成框 14 obsidian +
  6 portal + 6 bedrock 平台 + states 6×state 0 + nether_entities [] /
  OW 段 26 块原样。
  (b) **返程**：载 (a) 终档（玩家在生成 portal 内）120f → "Traveled
  to overworld at (1, 55, -2) from (1, 55, -1)"（±16 搜中原 portal）
  → dimension=overworld / pos = [1.0, 55.0, −1.5]（**原 portal 内域底
  中心**，cell 中心验算——门内修正一处断言：cell z=−2 中心 = −1.5 非
  −2.5）/ nether 段 26 块持久化 / OW 6 portal 保留 / time 恰 +0.0033333
  （120f 帧锁定验算）。
- **S3** 全套 2582/2582（编排者独立复跑）。
- **S6 点名 6/6**（save 手术 headless）：29 携 dimension（"unknown
  field" version lie）/ 29 携 nether 段（同族）/ 30 缺 dimension（"format
  30 requires"）/ 词表 "void"（"unknown word ... vocabulary: overworld,
  nether"）/ nether.blocks 未知块名 "netherite_block"（点名）/ nether
  portal 无 states 条目（"stateful block ... has no nether.states
  entry" PITS 383① 孪生）——全 rc=1 带文件路径。
- **性能 A/B（用户硬要求）**：旧 = v3.3.0（tm_v340_perf = 0824f6f +
  g139，banner 实证），新 = v3.4.0（tm_v340_perf_new + g139，banner
  实证）：主对 900f 无输入同 seed——PERF-WIN 三窗全指标同档（upd med
  0.564/0.532/0.616 vs 0.629/0.625/0.565 / render 0.622/0.558/0.585
  vs 0.660/0.629/0.510 / swap vsync 同档 / stream 同档）+
  **changesApplied 9/1/3 逐位** + **终档 savenorm = 恰白名单 4 键**
  （format 29→30 + 3 新键 missing-in-A，零其它漂移）+ **下界稳态自
  成本**（新侧传送场景 900f = 60f 倒计时 + 传送 + ~840f 下界）：upd
  med 0.578/0.535/0.475（≤ OW 基线）/ **render med 0.395/0.346/0.349
  （< OW——实心岩板几何 + 无天空云雨）/ fluid·fires·spread = 0.000
  （全源静态熔岩海零 churn = recon 风险面 ③ 按设计消解实证）**。
  **门内工具坑（PITS 387）**：Phase B 的 DimCtx 参数化改了 drain 调用
  点形态 → g139 插桩器 STREAMING 锚点 loudly-fail（PITS 317 纪律按
  预期触发）→ 插桩器加双形态锚（新形态优先，旧形态保留 = 旧侧树可
  重建），tools/g139_instrument.py 随版提交。

## 实测数据

- 首传落点 = 映射坐标 (0,55,−1) → 生成框锚定 + 落点 (1, 55, −0.5)
  （内域底中心）；下界首访观感 = netherrack 岩洞 + portal 照明（布局
  裁决：Y 映射落 netherrack 带 32..122 内 = MC 同款"岩中 portal 房"，
  熔岩海 y≤31 = 向下 24 格可达，记档）。
- 冷却 900 帧（15 s，会话态）；下界稳态流送/渲染 ≤ 主世界基线。
- atlas 124 / SoundBank 49 / 块 id：netherrack 注册（id 日志实证）。

## 偏离/记档（fixer 10 条，编排者全验收）

1. 层带按契约字面（bedrock 0-4 / lava 5-31 / netherrack 32-122 /
   roof 123-127）——Y 映射落 netherrack 带内（门 S2 视觉 = 岩洞，
   熔岩海 = 深度内容）。
2. Entity m_world 引用 → 指针（setWorld 机制必需，17 点机械适配）。
3. +kPortalCooldownFrames 900（防振荡；会话态不入档）。
4. 非活跃维 = 时间胶囊（world-pass 冻结；DayCycle/Weather 恒推进）。
5. 生成框锚点 + 2×3 bedrock 平台（y0−1，触地无平台）。
6. 7 per-block state 族主世界 only（D9 闭集）。
7. 下界火恒定不烧尽。
8. 下界水桶泼水 no-op（水无下界语义，维度 FluidSim = Phase C）。
9. 下界床睡眠 no-op；容器面板主世界 only。
10. 落点格 = 内侧边界（"底中"固有属性，存档 pos 精确断言已钉）。

## 变更文件（显式路径，42 + 插桩器 = 43）

src/world/nether_gen.{hpp,cpp}（新）/ src/world/world.{hpp,cpp}
（skyless 2 读点）/ src/world/lighting.cpp（rescan 顶源）/
src/core/portal.{hpp,cpp}（传送纯函数族 + 冷却 + yaw 转）/
src/core/game.hpp（NetherDim/DimCtx/成员族）/ src/core/game_frame.cpp
（门控 + travelPortal + 双 pass 封装）/ src/core/game_streaming.cpp
（DimCtx 参数化）/ src/core/game_interaction.cpp（咽喉点维度解析）/
src/core/game_render.cpp（5 点门控 + F3）/ src/core/game_init.cpp
（ensureNether + 载入时序 + 双维 rescan）/ src/core/game_save.cpp +
src/core/save.{hpp,cpp}（format 30 三件 + 交叉检 + 双维 collect/apply）
/ src/core/game_entities.cpp（下界死亡回 OW）/ src/core/game_ui.cpp
（面板/床门控）/ src/entity/entity.{hpp,cpp} + mob_entity.cpp +
player.cpp（setWorld + 指针适配）/ src/ui/debug_overlay.{hpp,cpp}（F3
dimension 行）/ assets：netherrack.json + netherrack.png + atlas [13,7]
+ portal_trigger.wav + portal_travel.wav + 2 生成器 / tests：
test_nether_gen.cpp（新 14 例）+ test_portal（+4）+ test_save（30 族
7 + 降级 helper）+ test_save_gzip（白名单例替换）+ test_entity /
test_debug_overlay / test_block / test_atlas / test_mob / test_sound /
test_bed / test_campfire / test_drown / test_durability /
test_enchant_save / test_retained_column（有意识更新）/ CMakeLists
（3.4.0 + 新 TU）/ **tools/g139_instrument.py（双形态 STREAMING 锚，
PITS 387）**。

零改动红线核证：worldgen（主世界）零触碰 / config 零键 / gzip 容器
零改动 / overworld 位级不变（skyless 默认 false + 渲染门 default-off +
savenorm 白名单 4 键 + 截图 ≡ 锚逐字节 = 端到端实证）。
