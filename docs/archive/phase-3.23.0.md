# Phase v3.23.0 — V3-10a 末地维度框架 + 真传送（门 213 全 PASS，2026-11-07）

## 范围与定案

**项目第三维度**（V3-1 下界维度基建的**第二次消费**：worldgen 纯函数 /
流送 / 光照 / 传送管线 / 存档 format 末地段；规模 ≈ V3-1 Phase B
门 194）。末地内容（原野/基岩洋/岛群/末影珍珠）= V3-10b，末影人 =
V3-10c，末影龙 = V3-10d——本相位 = 框架 + 传送 + 最小可站立末地 +
入口门机制（A-lite）。

- **入口 = A-lite**：`end_portal_frame`（空）/ `end_portal_frame_eyed`
  （嵌眼）= **两个 stateless 块**（项目无 per-state 纹理机制——frame
  单块 state_count 2 否决）+ 数据谓词 `end_frame`/`end_frame_eyed`/
  `end_portal`（strict bool，配对检：eyed 须配 frame）+ `end_portal`
  块（nether_portal.json 逐字镜像：transparent/solid false/light 11/
  destroyable false）+ `ender_eye` 物品（32×32 独立 icon，ghast 膜先
  例）+ **5×5 水平环纯函数族**（ring/center cells + scan + activation
  plan + 8 候选中心反解 + 孤儿谓词）。
- **环几何**：frame/portal 全在 **body 层**（玩家 feet 层）；frame 环
  = 1 格墙可跳过进入 3×3 中心；激活 = 12 格全 eyed + 中心 3×3 全 air
  （嵌眼交互触发扫描；**frame 不可破坏 = 无去激活路，偏离记档**）。
- **最小末地**：120×120 末地石台地 x/z∈[−60,60] y∈[48,52]（feet 53）
  + **原点出口门**（worldgen：y53 12 格 frame_eyed 环 + 中心 3×3
  end_portal，y52 endstone = 站立面）+ 岛外虚空无地板（kill plane
  −32 + 「死于世界之外」既有 + 死亡回 OW 重生三值化）；入场平台 =
  5×5 obsidian @y52 中心 (40,0) **管线每次写入** + 上方 3 层清 air +
  落点 (40.5,53.0,0.5) 朝西 yaw +π/2（**偏离 MC (100,49,0) 记档**——
  保留 MC x 须扩 200×200）；倒计时复用 60f + 冷却 900f（偏离 MC 即
  时记档）；返程 = 主世界出生点（床校验先例）。
- **format 32**：dimension 词表 +end（**按 format 分词**：30/31 携
  "end" = version lie；≥32 = 3 词）+ end 段 {blocks,fluids,states} +
  end_entities 恒写 + endSeed = worldSeed^kSaltEnd 不落盘；PITS 399
  四类降级点与实现同 commit 盘点（strip 助手 ×5 扩 end 键 / 伪造
  format-30 真形只剥 31/32 键 / gzip identity 29→32 rebase）。
- **渲染 6 点**：clear kEndSkyColor 0.05/0.03/0.06 / 雾 8..48（fill
  clamp 复用）/ 日月星云雨门 inOverworld / F3 行 +end / ambience 第 5
  上下文（AmbienceContext::End=4，默认参 +ambience_end.wav 67→68）/
  **u_ambient + u_caveAmbient 双 floor 按维度**（kAmbientEnd 0.25；
  set 点 = init + 翻维帧；OW/nether 位级不变——**门内捕获**：shader
  无光分支只读 u_caveAmbient，单设 u_ambient = 末地全黑）。
- **生成池 = 空槽零代码**（10c 两段式，Phase B 先例）；**sim 不
  bump**（canary 4 全 OW 无末地路径，EXACT 实证）；块 127→131 /
  atlas 166→170（[10..13,10] 零移位）/ item census 98→99（ender_eye）。

## 实现（代码 14 + 数据 5 + 工具 3 + 测试 5 + CMakeLists 1）

- `src/world/end_gen.{hpp,cpp}`：kSaltEnd + endSeed + EndGen::create
  fail-fast hasState 检 + fillEndChunk 120×120 台地 + 原点出口门。
- `src/core/end_portal.{hpp,cpp}`：ring/center cells + 8 候选中心反解
  + scanEndPortalRing + endPortalCellValid（rescan 孤儿谓词）+
  activationPlan + endEntryPlatformPlan + endEntryPosition/Yaw。
- `src/block/block.{hpp,cpp}`：endPortal/endFrame/endFrameEyed strict
  bool + 配对检。
- `src/core/game.hpp`：Dimension +End / playerInEnd()/inOverworld()/
  dimensionOfWorld() / EndDim 12 成员镜像 / m_end / m_states.
  endPortals + endPortalId/endFrameEyedId / m_enderEyeId /
  m_endPortalEntryFrames / kAmbientEnd / applyDimensionAmbient。
- `src/core/game_init.cpp`：访问器三值化 + catchUpDisk 三值 + ensureEnd
  / endWorld / makeEndChunkGenerator + rescanEndPortals（双世界模板
  助手）+ EndGen::create init fail-fast + id 解析 + 载入 rescan 三维 +
  loadedColumns 镜像。
- `src/core/game_frame.cpp`：drain 三值 + 时间胶囊三值 + 流体三值 +
  mesh 三值 + updateEndPortalEntry + travelEnd（1:1 无 8:1）+
  updateEndFluidTick + ambience 第 5 参。
- `src/core/game_interaction.cpp`：setBlockInWorld 指针判别重做
  （dimensionOfWorld，风险②）+ 状态族/editedColumns 三值 + 营火/容器
  /床 inOverworld 门 + tryEyePortal（嵌眼交互）。
- `src/core/game_render.cpp`：sky/雾/columns/columnMeshes/门控族六
  点 + End 常量 + applyDimensionAmbient（双 floor）。
- `src/core/game_ui.cpp`：F3 +end / respawn D8 三值 + 翻维 ambient。
- `src/core/game_save.cpp`：dimension 三值收集 + end 段收集/载入 +
  player rebind 三值。
- `src/core/save.{hpp,cpp}`：kSaveFormat 32 + SaveGame end 族 + 按
  format 词表 + end/end_entities 序列化/解析（appendEndSections 独立
  函数 = GCC-16 假阳性结构隔离）。
- `src/world/ambience.{hpp,cpp}`：End=4 + ambience_end。
- assets：4 块 JSON + ender_eye（含 model）+ 5 纹理 + atlas 170 +
  ambience_end.wav。
- tools：generate_sounds.py（ambience_end）+ g139_instrument.py（三形
  态锚 STREAMING_OLD3/NEW3，PITS 387 第三演进）。
- tests：test_end_gen.cpp（10）+ test_end_portal.cpp（11）+
  test_save.cpp（SaveFormat32Test ×6 + 夹具 end 块族 + strip 助手扩
  end 键 + format pin kSaveFormat 化）+ 4 文件 strip 助手 + census
  re-pin（atlas 170 / 块 131 / item 99 / 声音 68 / FNV 3 处 / 版本
  行）+ test_v390 / test_nether_gen / test_netherite / test_mob /
  test_item_model_mesh / test_debug_overlay。

## 门 213（全 PASS）

- **S1 结构免疫第 94 次**：60f 无输入 ×2 ≡ **base19 字节恒等** +
  savenorm EXACT（末地惰性 = ensureEnd 未 engage，drain 快退）；终档
  **恰白名单 3 键** {format 31→32, end={[],[],[]}, end_entities=[]}
  （diff 键集实证）。u_caveAmbient 修复后复验恒等。
- **S2 去程 (a) = authored demo**（7×7 挖深坑 y54..70 平顶 + y53 石
  地板 + 12 frame @y54 + 玩家站 3×3 中心 + 热栏 13 眼；timeline = LMB
  捕获 f30 + 12×【瞄准 mouse_move f40+5i + RMB f45+5i】+ f900 延展）：
  12 嵌眼（4 次「no complete ring」= 环未全时的正确语义）→ f100
  **`End portal activated at (3, 54, 3)`** → 60f 倒计时 → f175
  **`Traveled to end at (40, 53, 0) from (3, 54, 3)`**；×2 截图
  **RAW 恒等** + 动作行逐字恒等 + 末地场景视觉（暗紫黑天 + 雾收虚
  空 + 岛面 0.25 ambient）+ 热栏 **13→1 眼**消费 crop 实证。**门内
  捕获 ×2**：① 首探场景选在丘陵 y54 手术层被地形埋没（黑屏）→ 7×7
  挖深坑平顶（PITS 405②）；② 末地全黑 = shader 无光分支只读
  u_caveAmbient（u_ambient 0.25 单设无效）→ 双 floor 修复（PITS
  405③）。终态权威 = 截图 + DEBUG 行（demo save 红线不回写
  PITS 403①）。
- **S2 返程 (b) 零输入臂**（场景手术：dimension end + 玩家 (0.5,53,
  0.5) 站 worldgen 出口 3×3 中心，end 段空）：180f ×2 RAW + savenorm
  EXACT + **`Traveled to overworld (end exit) at (0, 54, 0) from (0,
  53, 0)`** ×2 逐字 + 终档 dimension=overworld / pos=(0.5,54,0.5)
  出生点精确 / end 段空（出口 = worldgen 无 diff）/ hp 20。
- **S2 末地持久化 (c) 臂**（场景手术：dimension end + 玩家平台角
  (42.5,53,2.5) + end 段 25 obsidian 平台编辑 = travelEnd 首访写入
  形）：180f ×2 RAW + savenorm EXACT + 终档 **end 段 = 恰 25 obsidian
  平台格集合精确**（风险⑦ journal diff 验算）+ dimension=end / hp
  20 平台零伤。
- **S6 五面点名**（temp 资产法，mv + 还原 + grep 回验 / python 存档
  手术）：① endstone.json 缺失 → init rc 1 `end_gen.cpp: missing
  block 'endstone'`；② format-31 携 dimension "end" → `unknown word
  'end' (vocabulary: "overworld", "nether")`；③ format-31 携 end 段
  → `save 'end' requires format 32 or newer (a version lie)`；④
  format-32 缺 end 段 → `save format 32 requires an 'end' object`；
  ⑤ end 段 stateful 块缺 states 条目 → `the end's stateful block at
  (4, 55, 0) has no 'end.states' entry`（PITS 383 双向检）。
- **S2b canary 4 demo** ×2 RAW + savenorm EXACT（**sim 不 bump 实
  证**——kDemoSimVersion 2 不变）。
- **S3 全套 2885/2885**（+29：EndGen ×10 / EndPortal ×11 /
  SaveFormat32 ×6 + census re-pin 8 文件），`-Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = bc26bed v3.22.0（git archive）+
  g139【三形态锚 STREAMING_OLD3/NEW3，PITS 387 第三演进随版更新】vs
  新 = 工作树 rsync + g139（单次插桩 + 300f 冒烟验 PERF-WIN），900f
  无输入 tools/perf_gate_config.json 独占——banner 实证 v3.22.0/
  v3.23.0 + PERF-WIN 三窗 upd med 旧 0.327/0.323/0.324 vs 新
  0.533/0.573/0.575 + render med 旧 0.368/0.369/0.383 vs 新
  0.556/0.655/0.699 + swap 15.8/15.8/15.8 vs 15.4/15.2/15.2（vsync
  地板同档）+ fluid·fires·spread·ents·stream 同档 +
  **changesApplied 9/3/1 vs 9/3/1 全逐位** + **终档 savenorm = 恰白
  名单 3 键 {format 31→32, end, end_entities}**（format bump 最强形
  ——门 207「恰白名单 2 键」先例的 32 版）。**同档裁决依据**：旧树
  同二进制复跑 upd med 0.641/0.719/0.724（首跑 0.327 的 2× spread
  = 环境漂移带实证，历史谱系同版本 run 间带宽 0.32–0.76），新树
  0.533–0.575 在带内 = 无回归。

## 门内捕获（PITS 405）

1. **场景手术选位 = 先探地形**：fresh 世界出生点邻域是丘陵（feet
   54 起爬升）——y54 环 + y53 地板手术被高处地形埋没（黑屏 + RMB
   全落空假象极似「没瞄准」）。纪律 = 7×7 挖深坑（y54..70 air +
   y53 石地板）平顶化，与绝对高度无关。
2. **shader 无光分支只读 u_caveAmbient**：`light <= 0.001 → amb =
   u_caveAmbient`——无光源维度（10a 末地全岛 light 0）的 ambient
   floor 须**双 uniform 同设**（u_ambient + u_caveAmbient），单设
   u_ambient = 岛面仍走 0.03 cave floor = 全黑截图。
3. **GCC-16 -O3 maybe-uninitialized 假阳性族**：format 32 序列化增
   量改变了 -O3 内联决策，既有 serializeSlotEntry 的 Json variant
   假阳性复发（noinline 属性无效；**结构隔离有效** = end 段序列化抽
   独立静态函数 + 测试 TU 的 grid cell 构造抽助手）——新编译器下的
   旧代码假阳性 ≠ 行为 bug，先验 HEAD 树同编译器干净再动结构。
4. **存档伪造真形 = 只剥对应后键**：format-30 伪造若用 pre-30 strip
   （剥 nether/dimension）= 缺 30 必携键假失败；正确 = 只剥 31/32
   键（creative/end/end_entities）保留 30 键（dimension/nether）。
5. **场景派生须同步 fluids**：从带 fluids 编辑的 base 派生新场景清
   blocks 不清 fluids → 「cell occupied」init 拒（fluid 落生成固体
   格）。

## 记档偏离（MC 语义）

- **A-lite 双 stateless frame 块**（项目无 per-state 纹理机制，MC
  单块 16 态）。
- **入场平台 (40,52,0) 每次重写**（MC (100,49,0)；保留 MC x 须扩
  200×200 岛——决策弃）。
- **60f 站入倒计时 + 900f 冷却复用**（MC 即时传送）。
- **返程 = 主世界出生点（床校验）+ 出口常活**（MC 出口龙死后激活
  = 10d 前偏离）。
- **无去激活路**（frame destroyable false，嵌眼单调；MC 破 frame
  去激活）。
- **声音复用 portal_trigger/portal_travel**（零新 wav；+1
  ambience_end）。
- **水蒸发谓词 nether-scoped = 末地水不蒸发**（MC 末地蒸发——10b
  复议）。
- **虚空无地板**（kill plane −32 + 「死于世界之外」+ 死亡回 OW
  重生 D8 三值化 = MC「末地只能死回去/出口回去」）。
- **生成池空槽**（10c 消费）/ **frame 生存不可得**（creative 授予 +
  门手术；10b/10c 复议）。
- **End 流体 pass = 静态零 churn 纪律**（10a worldgen 无流体；桶
  正常流动，honey gate 同拍签名平价）。

## 变更文件（显式路径，29）

见 git commit。
