# Phase V3-1 Phase A — 下界传送门框（v3.2.0，门 192 全 PASS）

> 设计定案 = `/tmp/opencode/v320_design.md`（编排者收口：框 4×5..23×23
> 角可选保留 / "火须最后放" quirk 偏离 = 双向事件检查 / 站入延迟
> **60 帧（1s）帧锁定**（240 帧备选拒 = E2E 成本 ×4，MC 史值
> 10~80 ticks 区间内）/ 仅玩家穿门 / portal = full-cell 半透盒 +
> axis state（薄片化留 Phase C）/ 传送桩 = `kPortalTravelEnabled`
> 代码常量 + static_assert 构建门，禁 TODO）。立项调研 =
> `/tmp/opencode/v31_recon.md`（582 行：架构侦察 10 点 + MC wiki 三页
> 核定）。V3-1 三段拆分第一段；Phase B（下界世界 + 真传送，format 30）
> 与 Phase C（下界内容族）另立项。

## 实现

### nether_portal 块（纯数据 + 纹理 + 音效）
- `assets/data/blocks/nether_portal.json`：`model.shape full_cube`
  （lava 半透盒先例）/ solid·opaque false / transparent true /
  **destroyable false**（lava 先例旗）/ hardness 0 / **light_emission
  11**（MC 精确）/ 无 drops / sound_material glass / **state_count 2 +
  state_default 0**（ENT-2 严格成对；axis：0 = 长边沿 X（框在 z=const
  平面）/ 1 = 沿 Z）。块 id = 51（注册日志实证）。
- 纹理：`tools/generate_block_textures.py` 加确定性紫罗兰漩涡生成器
  （salts 1001..1008，alpha 200 = 水半透盒类）→ atlas 新格 **[13,6]**
  （16×8 网格零既有移位，census 122→123 钉）；重跑既有 96 张 md5 逐一
  不变。
- 音效：`tools/generate_sounds.py` 加 portal_ambient（种子 0xC4F1，
  110Hz 体 + 5.2Hz 涡旋颤音 + 220Hz 倍音 + 低通噪 grit，1.2 s，0.5
  音量类）→ SoundBank 46→47（PITS 382 纹理度量守卫钉）；重跑既有 46
  件 md5 逐一不变。
- **存档零 format**（29 不动）：portal = 普通 stateful 块，走既有
  blockEdits + format-15 states 段；worldgen 结构性不放 stateful 块
  （init 交叉检既有）= 结构免疫根之一。

### core/portal.{hpp,cpp} 纯函数族（单测拥有，campfire/bed 范式）
- **命名常量**：kPortalMin/MaxLongEdge 4/23、kPortalMin/MaxHeight 5/23
  （MC 4×5..23×23；高度恒 ≥5 = wiki 核定，5 宽×4 高"躺式"最小框拒）/
  kPortalMappingScale 8 / kPortalEntryFrames 60 / **kPortalTravelEnabled
  false**（Phase A 桩门）/ ambient 2.0 s + 32 块可视域 + salt 0x3D1A0C55。
- **有界扫描** `scanPortalFrames`：事件格两竖直平面各 45×45 窗口
  （±22 = 最大框延展算术根，偏离 1：设计 25×25 估计只覆盖事件格距远边
  ≤12 的情形）+ 4 张前缀和 O(1) 候选判定（~72k/平面，事件 <1ms 级）+
  确定性序（axis, lo, y0, plane）。CellClass 分类（air/obsidian/fire/
  portal/bad）；环完整 = 非角周边全 obsidian（角任意内容 = MC 精确）；
  内域全 {air, fire[, portal]}。
- `findFrameAt`（严格搜索：内无 portal——激活/点火几何）/
  `portalValidFrame`（rescan 搜索：内 portal 合法 + 单元严格内域）/
  环·内域·角·周边纯几何族 / `overworldToNether`（floorDiv 8:1，负坐标
  安全，7 组单测钉）/ `portalAmbientRoll`（PITS 4 推进纪律 + hash3
  去相关 + 专盐）。
- **transition 决策** `portalTransitionDecision`（纯）：去激活 = obsidian
  破坏 ∧ 非角 ∧ 事件前完整 ∧ 内 portal > 0（**角破坏保留 = MC 精确**；
  fire 熄灭 ≠ 去激活 = portal 自持）；激活 = 现完整 ∧ 内有火 ∧ 内无
  portal（双向事件检查，"火须最后放" quirk 偏离）。
- **站入 step** `portalEntryStep`（纯，campfire-tick 范式）：arm 帧计入
  60 之一（恰第 60 连续重叠帧完成）/ 走出即取消 / 无冷却重计（Phase A
  语义钉）。

### Game 薄适配
- **咽喉点钩子**（game_interaction.cpp setBlock 族）：oldId 预读（写前，
  brokenState 先例）+ `m_states.portals` 追踪集（beacons 孪生，per-cell
  id 谓词）+ obsidian/fire 事件门控 → `checkPortalTransition`（scan +
  决策 + 去激活先于激活应用——列表 per-frame 互斥）；激活 = 内
  air+fire → portal（setBlock 走咽喉点 = journal/追踪集/ENT-2 state
  自动随行，fire 格被替换 = MC "one of them replacing the fire"）+
  `setBlockState(axis)` + 框 bbox 一次 `Lighting::updateRegion`（光 11
  重烘）+ info 日志 "Portal activated at (x, y0, z) (WxH)"。
- **点火分支 (c)**（打火石 RMB 链尾，**编排者中途几何修正**：框内 air
  与环共面 = 射线不可直达 → slab 放置同款「命中面→相邻格」）：命中
  obsidian 环格 ∧ 命中面法向相邻格 = 完整框严格内 air ∧ 内无 portal →
  FIRE 入内格（咽喉点自动触发激活）+ 磨损 kIgniteWear + ignite 音
  （(a)/(b) 孪生）；环格无完整框拥有该邻格 = 无反应（位级不变）。
- **站入倒计时**（game_frame.cpp 帧锁定段）：`overlapsBlock(portalId)`
  AABB 重叠（fire 接触孪生）→ portalEntryStep；**GUI 互斥**：
  inPortal ∧ panelOpen → closePanel（MC 传送门规则——panel 不能留在
  portal 里；closePanel 既有语义 = 指针释放，重捕获 click 驱动 = 项目
  既有收口 UX，非本阶段行为）；完成 = kPortalTravelEnabled false →
  static_assert 构建门 + info 日志 "Nether ready at (mapped)"
  （overworldToNether(floor 玩家格)——Phase B 消费）+ 零位移 + 清零
  （再入再计）。
- **portal ambience**（updatePortalAmbience）：追踪集空 = 快退（结构
  免疫根）/ 32 块可视域 / 每帧 (seed, frame, cell) 纯 roll p = 1/120 /
  空间化播放 + 日志。
- **载入 rescan**（buildScene 尾，rescanBeacons 孪生）：world 扫描 portal
  格 → portalValidFrame 无效（孤立 = 手改档）→ 置 air（raw World API，
  applyWorldEdits 先例）+ 追踪集重建 + 框邻域一次性重光 + 日志
  "Portal rescan: N orphan portal block(s) set to air"。

## 门 192（结构免疫第 73 次；锚 224468 B 免重锚）

**结构免疫论证**：demo 区无输入 60f——worldgen 不放 obsidian 框（grep
实证零 obsidian 引用 + stateful 块 init 交叉检排除 portal）+ 打火石不
在默认热栏（hotbar.json 实证）→ 全部新路径零活动。

- **S1** 无输入 60f ×2：截图 RAW ×2 恒等 **且 ≡ 锚 224468 B 逐字节** +
  存档 savenorm EXACT（零 format）。
- **S2** 输入 E2E（场景 = 4×5 含角框，z=−2 平面 x=−1..2 y=54..58，内
  6 格，axis 0；PITS 376/379 协议；窗 (2,22) 679×744 → 点击点 (341,394)
  实测校准）：
  (a) **激活**：帧场景 + flint slot0，RMB 瞄准环格 (−1,55,−2) 的 **+X
  面**（点 (0,55.5,−1.5)，yaw 0.24498/pitch −0.05813 计算值；首跑瞄准
  外 −X 面 = 无反应——**面法向相邻格必须指向框内** = 编排者门内发现
  的几何纪律）→ 终档 6 portal 格 state 0 全 + fires 0（fire 被替换）+
  "Portal activated at (0, 54, -2) (4x5)" + flint damage 1（磨损孪生）；
  (b) **去激活**：激活场景，LMB 破非角 (0,54,−2)（obsidian 硬度 10 →
  17.5 s hold，1400f 预算）→ 6 portal 回 air + "Portal deactivated" +
  obsidian 掉落；
  (c) **角保留（MC 精确）**：破角 (−1,54,−2) → 6 portal **全保留** +
  零去激活行；
  (d) **站入**：玩家置 portal 格 (0,55,−2) 内（**脚 z 须在 (−2,−1) =
  −1.5，非 −2.5——cell 索引 vs 格中心差一坑，首跑实证**）→ 60 帧恰
  "Nether ready at (0, 55, -1)"（= overworldToNether(floor 玩家格)
  验算）+ 零位移 + 每秒一次 = 无冷却重计 Phase A 语义钉；
  (d-gui) **GUI 互斥决定性四跑**：d2 站入+E（mid-countdown）/ d3 站入
  对照 / d4 框外+E / d5 框外对照 + d6 站入+E+重捕获：d4 vs d5 diff
  bbox (1,123,679,744) = 面板渲染（E 送达证明，PITS 266 XTEST 协议）+
  d2 无面板残留（自动关）+ 倒计时不被面板阻塞（5 周期）+ **d6 ≡ d3
  RAW 逐字节**（自动关后重捕获 = 无输入态精确恢复）；
  (o) 旧侧 v3.1.0：flint 对框无反应 + 零 nether_portal 行 + 零
  activated/deactivated 日志 = 形状互斥。
- **S3** 全套 2536/2536（编排者独立复跑）。
- **S6** 点名 4/4（scratch assets 范式）：state_default 缺（"strict
  pair"）/ light_emission 16（"[0, 15]"）/ destroyable "yes"（"must be
  a boolean"）/ atlas 缺条目（"no entry in the atlas layout"）——全
  init 拒 rc≠0 带文件路径。
- **性能 A/B（用户硬要求）**：旧 = v3.1.0（tm_v320_perf = cc3dbb4 + g139，
  banner v3.1.0 实证），新 = v3.2.0（tm_v320_perf_new = worktree + g139，
  banner v3.2.0 实证）。主对 900f 无输入同 seed：PERF-WIN 三窗全指标
  同档（upd med 0.531/0.531/0.548 vs 0.569/0.609/0.575 ms / render
  med 0.588/0.503/0.500 vs 0.553/0.632/0.542 / swap 15.31/15.55/15.51
  vs 15.40/15.32/15.48 = vsync 主导 / fluid·fires·spread·ents·stream
  全同档）+ **changesApplied 9/1/3 逐位恒等** + **终档 savenorm
  EXACT**（零 format = 比 v3.1.0 门的单键口径更强的完全恒等——demo
  区无 portal = 特性结构惰性的性能同证）。**portal 自成本**（新侧
  600f：activated 场景【14 obsidian + 6 portal】vs frame-only 场景）：
  PERF-WIN 同档（upd med 0.512/0.567 vs 0.493/0.623 / render med
  0.498/0.572 vs 0.560/0.573）= **无可测 per-frame 增量**（ambience
  roll 6 格 + 追踪集迭代 = sub-µs 级；站入倒计时 = 一次 AABB 重叠
  查询/帧）。

## 实测数据

- 门档场景：激活一次 = 6 格转换 + 一次 bbox 重光；扫描 45×45×2 平面
  前缀和 O(1) 判定（事件 <1 ms 级，单测扫描域最远角可达钉）。
- 站入 60 帧 = 1.0 s（帧锁定，E2E 可断言）。
- 纹理 atlas [13,6] / SoundBank 47 / 块 id 51。

## 偏离/记档

1. 扫描域 ±22（45×45/平面）而非设计 25×25：大框远角事件的算术根
   （有界性保留，最远角单测钉）；事件一次性成本，无输入零成本。
2. PortalFrame 加 `plane` 字段（环的固定平面坐标，axis 0 = z / axis 1
   = x）。
3. 纯函数签名去 BlockRegistry 参（只消费解析后 id = 数据驱动强化；
   -Werror 拒悬空参数）。
4. "火须最后放" wiki quirk 偏离 = 双向事件检查（设计定案，header 记档）。
5. 点火 = 命中环格**面法向相邻内 air 格**（射线不可直达共面内 air——
   slab 放置同款「命中面→相邻格」，编排者 steer 修正，门内首跑实证：
   瞄准外侧面 = 无反应 = 机制正确性反证）。
6. 站入 step = arm 帧计入 60 之一（恰第 60 连续重叠帧完成）。
7. 激活/去激活 = 对**每个**满足条件的框逐框应用（共享 obsidian 退化
   几何下"最大框"歧义；决策纯函数双列表确定性有序）。
8. GUI 互斥的 closePanel 沿用既有语义（指针释放，click 重捕获——非
   本阶段行为；d6 决定性实证自动关后状态精确恢复）。
9. 5 宽×4 高"躺式"框拒（高度恒 ≥5 = MC 精确，wiki 核定）。

## 变更文件（显式路径）

src/core/portal.hpp（新，纯函数族 + 常量）/ src/core/portal.cpp（新）/
src/core/game.hpp（PerBlockStates + 方法族 + m_portalEntryFrames）/
src/core/game_interaction.cpp（咽喉点钩子 + 点火 (c) + 激活/去激活/
check 三实现）/ src/core/game_frame.cpp（站入 + ambience）/
src/core/game_init.cpp（id 解析 + rescanPortals）/
assets/data/blocks/nether_portal.json（新）/ assets/textures/blocks/
nether_portal.png（新生成）/ assets/data/textures/block_atlas.json
（[13,6]）/ assets/sounds/portal_ambient.wav（新生成）/
tools/generate_block_textures.py + tools/generate_sounds.py（生成器
+1）/ tests/test_portal.cpp（新 31 例）/ tests/test_block.cpp（内容钉
+1）/ tests/test_sound.cpp（47 计数 + ambient 守卫）/ tests/test_atlas.
cpp（123 census）/ tests/test_mob.cpp（atlas FNV 重钉）/
tests/test_debug_overlay.cpp（F3 v3.2.0）/ CMakeLists.txt（3.2.0 +
portal.cpp + test_portal.cpp）。

零改动红线核证：worldgen 零 obsidian 引用（grep 空）/ renderer 零改动
（full_cube shape 既有）/ save.{hpp,cpp}·save_slot 零改动（零 format）/
config.json 零触碰 / 既有 2503 测试零逻辑改动（5 处有意识计数/哈希/
版本钉更新）。
