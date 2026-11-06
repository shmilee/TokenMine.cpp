# Phase v3.25.0 — V3-10c 末影人（门 215 全 PASS，2026-11-07）

## 范围与定案

**End 首个 mob = 末影人**（neutral + 瞬移 + 携格；≈ V3-3 单 mob 规模，
纯数据 + 数据旗，核心零物种 switch——硬规则 3 的第四档验证）。侦察
权威 = `/tmp/opencode/v310c_recon.md`（全采纳）。**format 32 不变**
（mob 不入档 + 携块走 end 段 journal 既有面）/ **sim 2 不 bump**
（canary 4 全 OW 无末影人，EXACT 实证）/ 零 config / 块 133 不变 /
atlas 172→174（[2..3,11] 零移位）/ item 101 不变 / 声音 70→71。

- **数据旗 = 3 组，全部「数值/布尔门控 + 缺省冷」**（core 读 flag，
  无 name switch；pre-10c mob 全默认 = 位级根）：
  - `eye_contact_range`（strict number ≥ 0，缺省 0 = 关；数值门范式 =
    shoot_range）——末影人 48 = **kEndFogEnd「看见它 = 激怒它」定案**
    （末地雾 8..48 视界 = 激怒射程，编排者裁决）。
  - `teleport_interval_min/max`（strict 配对 (0,60] min≤max，**必须成
    对出现**——milk 配对纪律）——末影人 2/6；**配对存在性 = 瞬移能力
    的结构门**（0/0 = 无瞬移）；受击 100% 与随机间隔同属一种能力
    （无新旗）。
  - `carries_blocks`（mob 侧 strict bool 缺省 false）+ 块侧
    `carriable`（strict bool 缺省 false，cull_same_id 范式）——**两端
    齐发才活**（两相位资产纪律）；stateful carriable = **init 点名
    拒**（ENT-2 孪生：携格 journal 无法写 per-instance 状态）。
- **末影人 census**：40 HP / speed 0.8 / detect 16 / attack 6（1.6
  范围 / 1.5 冷却）/ neutral / **spawn_max_light 0**（末地暗处读 +
  出口门光 11 圈排除——门 213 出口常活的生成压制位）/ 掉 **ender_
  pearl 0-1 + ender_eye 0-1**（**生存唯一来源**——10a 入口「eye 生存
  不可得」+ 10b 珍珠「creative/存档手术可得」两条记档闭合）/ xp 5 /
  model **0.6×2.9 六盒**（head + torso + 2 长臂 + 2 长腿，四 swing
  长臂特征 = MC 瘦高剪影）/ 纹理 2 张（近黑虚空体 + 紫发光眼行，
  新盐 0x10C1/0x10C2，atlas [2,11]/[3,11] 幂等零移位）。

### 注视激怒（eye contact）

game pass（**mob 循环前固定位**，setPlayer feed 的位级孪生——几何读
帧新鲜玩家瞄准，下方 mob update 带着激怒现状跑）：

- 逐 mob 数据门 `eye_contact_range > 0`（range-0 = 位级根，循环一次
  比较即过）。
- 几何 = 玩家瞄准射线（**眼位出发 UX-8g**）`raycastAabb` 命中 mob
  **全 AABB**（MC 读头 = 放宽记档——2.9 盒的头是其顶部 ~0.5；全盒读
  近距瞄腿也激怒）且 `t_mob < min(range, t_block)`（**瞄准块 t 严格
  遮挡**——透明块遮挡 MC 读，ray 自身语义）。
- mob 纯函数 `eyeContactFeed(looking)`：**5 连续帧**（kEyeContact-
  Frames 帧锁定整数计数 = kNeutralAngryDurationSeconds 代码常量先
  例）→ m_angry + **30 s 窗复用**（damage 激怒的 state 孪生）；
  **单帧未看 = 计数复位**（眨眼不持续激怒，MC 读）；已怒帧不重复
  激怒。**零流**（纯几何 + 帧计数——确定性的干净侧，ambient roll
  的对照）。
- 激怒帧 DEBUG 行（f 号）= E2E 证据。

### 瞬移（teleport）

**动作非状态**（MobState 零新增——瞬移是位置事件，不是 AI 态）：

- 来源 2，**update 固定槽帧序钉死**（愤怒窗倒计时后、AI+物理前）：
  **受击 100%**（`damage()` 单收口只置 m_wantTeleport 旗——调用点
  帧序不可钉【近战/火 tick/效果/摔落任意帧点】，旗由槽消费 = 流
  顺序唯一消费者）→ 随机间隔到期（m_teleportTimer real-dt 倒计时，
  lerp over 数据对）。
- **流契约 = 25 抽/尝试固定**（PITS 389 纪律：变量次数 = 流被世界
  答案步进）：构造首掷 1 抽（canTeleport mob 唯一新增出生抽，bat
  bobPhase 先例）+ 每次尝试 = `rollTeleportCandidates` 纯函数 **8
  候选 × 3 抽**（u1 x 偏移 / u2 z 偏移 / nextMix y 切片——**模先于
  int 转换** PITS 406① 纪律）+ 尝试后 **1 重掷**（成功失败都跑）。
- 盒（MC 64³ / 激怒 16h×17v 的**项目缩放**，偏离记档）：**平静 ±16
  x/z ±8 y 自位锚 / 激怒 ±8 x/z ±4 y 朝玩家 min(12, dist) 点锚**
  （玩家更近 = 锚即玩家——瞬移落玩家邻域，追击瞬移读）。
- 执行 = **game 缝** `setTeleportCallback`（spawn 时接线，setShoot-
  Callback 范式；mob 用自己的流 roll，game 探世界）→
  `executeMobTeleport`（update 主线程写者窗，flush barrier 已跑）：
  逐候选 ① catchUpDisk（探针前提，pearlLanding 逐字孪生）② **候选
  位须空旷（hasRoomAt 严格不等式 ∧ 无流体 aabbHasFluid）+ 16 格内
  footprint 有地面**（任一 solid 支撑 = 站立约定；kTeleportProbe-
  Down 16）③ teleportTo + `finishTeleport`（**fall 复锚** = 候选空
  气落差非伤害，珍珠消坠伤孪生 + ENT-6 stuck 窗/path 丢弃——瞬移后
  追击者从落点重测）+ 落点音空间化 + DEBUG 行。**全败 = swallow**
  （mob 原地不动 + 永久 DEBUG swallowed 行——MC 恒瞬移偏离，封闭洞
  罕见；门内捕获①见 PITS 407⑤）。
- 声音 = **enderman_teleport**（70→71，固定 LCG 0x1E40 专属种子，
  1100→220 Hz 坠落 shimmer + crackle burst，0.28 s，**仅落点播**）；
  init 交叉检 = 一切瞬移能力 mob 要求该 wav 存在（shoot_sound 的
  mob 孪生，S6 面）。

### 携格（block carry，10c-lite）

- **集 = {endstone, chorus_plant}**（项目 holdable ∩ 末地相关读；
  主世界/下界 17+ 块无消费方不扩 = 偏离记档）；两块均有 item 形
  （endstone drops 自身；chorus_plant chance_drops 含株）——死亡掉
  落/放置零新 item 面。
- game pass（**mob 循环后固定位**——mob 本帧位置 = 扫描真值，
  setBlock 走 journal 收口 = V3-14a explodeAt 写先例）：
  - **摸取**（未携，**每帧**——MC 1/20 tick 读帧化）：4×3×4 区
    （x/z ±2，**y feet−1..+1 含脚下**——站立体下方地块 = 偷取主对象，
    PITS 407③）；**x→y→z 定序**（scanExplosion 定序先例——未钉定
    序 = 不可复现的格）；首个 非 air ∧ carriable ∧ stateless（belt，
    init 已拒）∧ **首格遮挡**（`raycastBlock`(eye→格心) 首命中 ==
    目标格——hasLineOfSight 拒目标格自身 = 箭撞击语义误用，
    PITS 407④）→ setBlock(air) + setCarried + DEBUG 行。
  - **放置**（已携，每 **kCarryPlaceFrames 6000**——MC 1/2000 tick
    读 @ 60 fps ≈ 100 s；帧 mod 零流纪律，MC 每 mob 随机 = 偏离
    记档）：2×2×2 首个 air + 下方 solid 地板 + **不叠玩家盒**（严格
    cell-AABB）。
  - **MC 平地板豁免移除**（末地全平 endstone，该规则让功能惰性——
    掏空即体验目标，偏离记档）。
  - **死亡 = 100% 掉所携**（实例态，wool 死亡掉落孪生位：骑死亡
    pass 不走数据 drops 列表）。
- 渲染 = **0.9× 头顶携块立方**（kCarryBlockScale，模型头顶 + 0.5
  空隙，facing 旋转，`itemMesh` 惰性复用零新几何；按块自身渲染类
  路由——endstone Opaque / chorus Cutout，双 pass 调用，零新 pipeline）。
- **承载 = 会话态零 format**（mob 不入档 = 骑乘/portal 冷却先例同
  款唯一自洽解）。

### 生成与掉落

- **End 池** = `mob_spawning_end.json` 单种族（enderman 10；cadence
  30 / first 60 / cap 4 / 环 4-6 / despawn 40 = 下界文件孪生）——
  **10a 两相位槽的消费方**（10a 发货空 m_end.mobs = 结构免疫根，10c
  填文件）；`loadEndSpawnPool` = loadNetherSpawnPool **第 3 次消费**
  （纯函数 init 侧，headless 单测；文件缺失/坏 schema/未知 mob 名
  全 fail-fast 点名 = 门 213 S6 时点定案孪生）；会话策略继承主世界
  config；**类型表 = 游戏单一共享注册表**（维度无关内容定案）；
  主世界/下界池**无** enderman 条目（End-only 偏离，10d 龙时代复
  议）。
- **掉落**：pearl 0-1（MC 50% 读）+ eye 0-1（**项目加项**——10a
  嵌眼交互的生存闭合）+ xp 5；roll = 既有 Phase-38 纯函数（0-1 非
  退化 = 每次死亡 2 抽，帧锁定可复现）。
- **声音边界**：仅 enderman_teleport（落点）；**ambient vwoop /
  scream / stare（激怒瞬间）后移 10d**（记偏离）；**endermite 后移**
  （MC 珍珠 5% + 瞬移生成；R11 承接 10d 复议）；**定身（sprint 看
  注视定住）后移**（无击退面 + 无 sprint 门缝，10d 与龙批复议）。

## 变更文件

- 数据：`assets/data/mobs/enderman.json`（新）/ `assets/data/
  mob_spawning_end.json`（新）/ `assets/data/blocks/{endstone,
  chorus_plant}.json`（+carriable）/ `assets/data/textures/
  block_atlas.json`（+2 tile [2..3,11]）/ `assets/textures/mobs/
  {enderman_body,enderman_face}.png`（新，生成器幂等）/ `assets/
  sounds/enderman_teleport.wav`（新，生成器幂等）
- 生成器：`tools/generate_mob_textures.py`（+2 tile 盐 0x10C1/0x10C2）/
  `tools/generate_sounds.py`（+enderman_teleport 盐 0x1E40）
- 代码：`src/block/block.hpp` + `block_registry.cpp`（carriable 旗 +
  stateful 拒）/ `src/entity/mob_registry.{hpp,cpp}`（3 组旗解析 +
  严格域）/ `src/entity/mob_entity.{hpp,cpp}`（rollTeleportCandidates
  纯函数 + eyeContactFeed + finishTeleport + m_wantTeleport/
  m_teleportTimer/m_carried + update 槽 + damage 旗）/ `src/entity/
  end_spawn_pool.{hpp,cpp}`（新，nether 孪生）/ `src/core/game_init.
  cpp`（end 池消费 + 瞬移 wav 交叉检）/ `src/core/game_entities.cpp`
  （注视 pass + 携块 pass + 回调接线 + 死亡携块掉落 + 死亡/掉落
  DEBUG 行 + drawCarriedBlocks）/ `src/core/game_frame.cpp`
  （executeMobTeleport + aabbHasFluid）/ `src/core/game_render.cpp`
  （携块双 pass 调用）/ `src/core/game.{hpp,local.hpp}`（声明 +
  kCarryPlaceFrames/kCarryBlockScale）/ `CMakeLists.txt`（VERSION
  3.25.0 + 新源/测试）
- 测试：`tests/test_enderman.cpp`（26：解析族 7 / carriable 族 2 /
  roll 纯函数 4 / 注视 4 / 瞬移槽 3 / 携格 2 / 节拍 1 / 发货 census 3）/
  `tests/test_end_spawn_pool.cpp`（5：有效解析/缺文件点名/未知 mob
  点名/schema 拒流/发货池钉）/ re-pin 族（test_atlas 174 + v310c tile
  位 / test_nether_gen GrewTo174 / test_mob atlas FNV 9bb55b0e10445d4e /
  test_sound 71 ×2 / test_debug_overlay v3.25.0 banner）

## 门 215 结果

- **构建**：0 警告（`-Wall -Wextra -Wpedantic -Werror`）。
- **测试 2930/2930**（2899 基线 + 31：enderman 族 26 + end 池 5；
  re-pin 6 处）。
- **S1 结构免疫第 96 次**：base19 **逐字节恒等 ×2** + savenorm
  EXACT + 终档恰白名单 3 键 {format 32, end, end_entities}——末影人
  全链（池/瞬移/携格/注视）活在 playerInEnd 门 + 数据旗缺省冷 +
  零流零构造下，OW 无输入位级根。
- **S2 无输入末地臂**（scene_end：player end (40.5,53,0.5) yaw 0
  creative，600f）：×2 截图 RAW 恒等 + savenorm EXACT + DEBUG 逐字
  ×2：spawn f60/90/120/150（cap 4）+ **provoked ×2** + **teleported
  ×8** + **carried ×4** + 终档 end 段**恰 4 air 格**（携块拾取 cell
  集合精确：(35,52,-7)/(36,52,-7)/(38,52,-8)/(38,52,-7)）+ hp 20。
- **S2 火杀 demo 臂**（scene_fire3：chorus 2 格 @ y53 脚级 +
  flint_and_steel 热栏 + creative + config_fire 60 s 火寿命——门口
  径资产非提交 config；probeJ.dmo：LMB 按住 f30 捕获 → f100 瞄准
  攻击位 → f300 点火 A → f350-895 按住（燃烧链）→ f900 点火 B →
  f1150 松 LMB【PITS 407⑦：观察段无按下键】→ f1200 回头）：
  - **death f1016 (37.695915, 53.306664, 3.944854)** 逐字 ×2
  - **mob drop 'endstone' x1 同位 f1016** 逐字 ×2（**所携 100% 掉落
    实证**；pearl/eye 0-1 roll = 0-0【合法结果，无行】）
  - teleported ×51 + provoked ×2 + carried ×4 逐字 ×2
  - 截图 ×2 RAW 恒等 + 掉落物 crop 目检（准星处 0.12 立方 + F3 日志
    行同框；endstone-on-endstone 低对比 = 日志行为权威）
  - 燃烧链验证：火块 y53 脚级严格 AABB 重叠点燃【PITS 407②：y52
    火不点燃站立 mob】→ fire 效果 8.0 s = 39 伤 < 40 HP → **双点火**
    覆盖整段战斗（fire.lifetime 4 s 默认不够——config_fire 拉到 60 s）
- **S6 七面点名**（run 目录资产变体，PITS 404① 纪律；全 rc 1 + 点名）：
  ① 池缺文件「cannot read asset file: 'data/mob_spawning_end.json'」
  ② 池未知 mob「unknown mob 'ender_dragon'（ghast = 已注册种，首踩
  误选）」③ interval min>max「'teleport_interval_min' must be <=
  'teleport_interval_max' (got 7.0 > 6.0)」④ eye 非数「'eye_contact_
  range' must be a number」⑤ wav 缺失「mob 'enderman' carries a
  teleport interval — the landing cue is missing 'sounds/
  enderman_teleport.wav'」⑥ carriable stateful「block 'endstone' is
  'carriable' but has a per-instance state（state_count 在 model 段——
  properties 段误放 = 首踩静默通过）」⑦ drops 未知 item「mob
  'enderman' drops unknown item 'nonexistent_ore'」
- **canary 4 demo ×2 RAW + savenorm EXACT**（example/tutorial_dig/
  build/fly，fresh world sim 2，**无 --demo-force** = sim 不 bump
  实证）。
- **性能 A/B 同档**（旧 = e2e2071 v3.24.0 + g139 双树）：banner 实证
  v3.24.0/v3.25.0 + PERF-WIN 三窗 upd med 0.307/0.352/0.315 vs 0.366/0.349/0.334 + render 0.387/0.407/0.371 vs 0.455/0.433/0.379 + swap 15.8 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形（300f 冒烟新树 upd med 0.527 在带内）
- **门内捕获**（全 PITS 407）：① demo 引用槽永不回写——读 slot
  文件 = 输入场景副本假测量（误判 mouse_move 无效绕 2 h；g213 复跑
  12 格不同嵌入 = 反证法）② 火脚级重叠 ③ 拾格带含脚下 dy −1 ④
  遮挡首格语义 ⑤ 下探穿固体掉虚空（#1 f436 后静默消失 → 候选空旷 ∧
  16 格内有地面重写 + swallowed 行）⑥ 无头截图 = 679×744 渲染目标
  非窗口 1280×720（crop 数学 4 连踩）⑦ 按住 LMB 挖准星所指（创造
  即挖——f1203 掉落物地板被挖 item 下坠 1 块视觉谜团）。

## MC 偏离记档（8）

1. 注视激怒 = 全 AABB 命中（MC 读头）——放宽记档。
2. 瞬移盒 = 项目缩放（MC 平静 64³ / 激怒 16h×17v → ±16×±8 / ±8×±4
   朝玩家 12 点锚）。
3. 摸取 = 每帧扫（MC 1/20 tick 概率读帧化）+ 放置 = 全局 6000 帧长
   拍（MC 每 mob ~1/2000 tick 随机读帧化）。
4. **平地板豁免移除**（末地全平，MC 该规则让功能惰性）。
5. 携格集 = 2 块（项目 holdable ∩ 末地相关）。
6. eye 48 = kEndFogEnd（看见 = 激怒定案）。
7. 声音仅瞬移落点（ambient/scream/stare 后移 10d）。
8. 掉落 +eye 0-1（项目加项，10a 生存闭合；MC 无末影人掉眼）；
   无瞬移自伤（MC 亦无，一致）；End-only 池（跨维条目 10d 复议）。
