# Phase UX-3 — 玩家身体模型（v1.x 任务表 36/40，门 157，UX 行 3/7）

> 玩家的世界空间身体（MC 标准 5 部件 box 模型 + 行走摆臂摆腿）。**全管线
> 复用 mob box 模型系统**（parseModel / makeMobModelGeometry / ENT-1
> swing 相位机 / checkSwingClip），零新渲染机制。第一人称下身体不可见
> = `CameraMode` 门恒 First → **无输入跑结构性免疫**（第三人称 = UX-4
> 消费本相的门）。手持 viewmodel（drawHeldHand）零改动。
>
> tag 政策（10-30 用户定案）：UX-3 本版不打 tag；**v1.38.0 = UX-3+4
> 合并 tag**（UX-4 收官时打）。

## 定案（10-30 用户确认）

- **皮肤 = 生成式占位**（用户三选一：生成式占位 / 纯色 / 用户提供）。
  64×64 经典布局（官方 wiki 4px-arm 模板逐区解码验证）+ 4 色块占位
  （肤色 / 绿衣 / 蓝裤 / 深灰靴）；确定性幂等脚本，后续可一键换真实
  皮肤（同布局同脚本）。
- **数据 = 独立文件路线 B**（勘察两路评估：路线 A = player.json 入
  mobs/ 须填整套死 AI 字段 = 假数据，拒；路线 B = `player_body.json`
  只含 model 段 + 薄 loader 复用 `MobRegistry::parseModel`，schema
  干净，选）。
- **几何 = MC 5 部件**：头 0.5³ / 躯干 0.5×**0.55**×0.25 / 双臂双腿
  各 0.25×0.75×0.25，总高 1.8 与玩家 AABB 一致。**躯干高 0.75→0.55
  修正**（编排者 spec 堆叠 bug：腿 0.75 + 体 0.75 + 头 0.5 = 2.0 >
  1.8 → 腿嵌体 0.2 = checkSwingClip 静息相位重叠 26.7% 必挂 25% 界；
  修正 = 体 [0.75, 1.3] = 腿 pivot（体底缘）..头底（体顶缘），ENT-1b
  关节出体规则同款）。双臂共用 1 tile / 双腿共用 1 tile（MC 惯例）。
- **步态 = 四部件对角**（臂腿各一对，MC 人形步行）：arm_l + leg_r
  group 0 / arm_r + leg_l group 1；头/体不摆。amplitude 初值 leg 0.35
  / arm 0.3 = 64 相位 clip 全过（腿最大重叠 = 腿体积 1.47%【界 25%，
  @phase 48】；最坏脚角 −0.0103 ∈ 容差 −0.02【解析 dip 下限
  −w²/2L = −0.0104，即脚角插地 = 运动学必然 dip 的 2× 裕量内】，
  ENT-1b 同款校准）。
- **相位 = 玩家既有移动判定镜像 mob**：`Player::update` 物理前捕获
  prevX/prevZ、stepUp 后**严格不等式**推进 `advanceSwingPhase`（ENT-1
  纯函数复用）；ENT-7 惯性滑行帧也推进 = MC 同款观感。相位
  **render-only 不入档**（mobs ephemeral 先例）= **零 format（22）**。

## 实现

1. **皮肤生成器** `tools/generate_player_skin.py`（新）：splitmix64 /
   每区 salt / 手写 PNG 写盘纪律 = generate_mob_textures.py 同款。产出
   5 文件：`assets/textures/player/player_skin.png`（64×64 经典布局，
   1632 可用 px 逐区程序化断言：零重叠零多余）+ 4 个 **16×16 派生
   部件 tile**（从同一画布 crop = 与源永不分叉）写入
   `assets/textures/mobs/`（loadDirectory 自动进图集）。重跑 ×3 md5
   恒定；**既有 15 个 mob tile 字节零变化**（FNV 钉入单测）。
2. **图集 +4 tile 尾部追加免重锚**：`block_atlas.json` 90→94 条目
   player_head/body/arm/leg → [10..13, 4]（farmland_wet [9,4] GEN-9
   同款空位纪律；既有 90 条目标位零变化 = 单测 Python 核）。
3. **模型数据** `assets/data/player_body.json`（新）：width 1.0（box
   验证界，**非碰撞**——玩家 AABB 恒 Player::Params 0.6×1.8 零触碰）/
   height 1.8 / 6 box / 4 swings。
4. **loader + init 交叉校验**（game_init.cpp buildScene，mob 注册段
   后）：`player_body.json` 顶层恰 1 个 `model` 键点名拒 →
   `MobRegistry::parseModel(…, "player body model")` → 每 box texture
   图集存在性 5 行范式。`parseModel` 加第 3 参 `const char* label =
   "mob model"`（7 处措辞点改 label，默认参 = **既有 mob 路径消息位级
   零变化**，负例单测钉死）。
5. **玩家相位机**（player.{hpp,cpp}）：`int m_swingPhase = 0`（
   render-only）+ `swingPhase()`；`update()` 物理前捕获 prevX/prevZ、
   stepUp 后推进（无条件 int 运算，零 config 门——玩家身体可见性只归
   相机模式）。
6. **渲染**（game.hpp `PlayerBody` 域 struct【UX-2 纪律，6 成员：
   model/modelLoaded/mesh/swingModel/swingMesh/swingFrameVerts】+
   `CameraMode { First, ThirdBack }` + `m_cameraMode = First`【UX-4
   加 F5 切换】；game_entities.cpp `drawPlayerBody` + 3 个 lazy
   builder【mobMesh/swingMobMesh/swingModel 单实例镜像】；game_render.
   cpp render() 在 drawMobs 之后插入 = opaque 段，atlas 已绑定上下文，
   **零 per-draw uniform**【紧随 drawMobs 同 pass 同前态】+ 模型矩阵
   = translate(position)·rotateY(yaw)【yaw 约定与 mob facingYaw 同款】
   + 烘焙 skyLight 1.0 / blockLight 0 = mob 恒亮同款【世界光覆盖 =
   记档偏离，与 mob 一致】。门 `m_cameraMode == First ||
   !modelLoaded` → return = **UX-3 恒 First → 全跑零 draw + mesh 永不
   lazy build**（结构免疫根）。

**零 format（22）/ 零 config / 零音效 / 零新渲染机制 / 玩家碰撞零触碰。**

## 变更文件

- `tools/generate_player_skin.py`（新）
- `assets/textures/player/player_skin.png`（新）
- `assets/textures/mobs/player_{head,body,arm,leg}.png`（新 ×4）
- `assets/data/textures/block_atlas.json`（+4 尾部追加，90→94）
- `assets/data/player_body.json`（新）
- `src/entity/mob_registry.{hpp,cpp}`（parseModel +label 默认参）
- `src/player/player.{hpp,cpp}`（相位机 +2 成员 +1 公开方法）
- `src/core/game.hpp`（PlayerBody struct + CameraMode + m_cameraMode）
- `src/core/game_init.cpp`（loader + 交叉校验）
- `src/core/game_entities.cpp`（drawPlayerBody + 3 builder）
- `src/core/game_render.cpp`（render() 插 1 行）
- `CMakeLists.txt`（1.36.0→1.37.0 + tests +1）
- `tests/test_player_body.cpp`（新，6 测）
- `tests/test_player.cpp`（+3 相位机测）
- `tests/test_debug_overlay.cpp`（F3 banner 钉 → v1.37.0）

## 测试

1938/1938（+9）、`-Werror` 0 警告、banner v1.37.0 实证：

- **PlayerBodyTest 6**（新文件）：ShippedModelPinned（6 box 精确
  pos/size/texture + 关节几何：腿 pivot = 体底 0.75、臂 pivot = 体顶
  1.3、头底 = 体顶 + 4 swings 精确 amplitude/group + 总高 1.8）/
  ShippedTilesInAtlasLayout（4 tile 落 [10..13,4]）/
  ShippedAllPhasesNoClip（4 摆部件 × 64 相位 checkSwingClip 全过，
  渲染器精确 sinCos = test_mob_model 范式）/
  ShippedMaxSwingJointKissNonVacuous（eff 16 非真空交叉钉 + 腿 overlap
  >0 且 <25% + 脚角离地 + 臂 overlap 恒 0）/ ParseLabelsAndRejects
  （box 出界 / 未知 part 'wing' / amplitude 0 / 未知 box 键 / 未知
  model 键 全 "player body model" 点名 + 默认 label 仍 "mob model"）/
  SkinPngBytesPinned（5 新 PNG FNV 钉 + **既有 15 mob tile FNV 钉 =
  零变化实证**）。
- **PlayerSwingPhaseTest 3**（test_player.cpp）：AdvancesWhileWalking
  （W 10 帧 phase 恰 10×kSwingStep）/ KeepsSwingingWhileSlidingThen-
  Decays（松键后 ENT-7 滑行帧 phase 继续 +2/帧【slide ≥5 非空断言】
  → 速度归零帧起 kSwingDecay 精确衰减到 0）/ InertWithoutInput（无
  输入 20 帧 phase 恒 0 = 结构免疫根）。

## 门 157（编排者执行，old 侧 = v1.36.0 = 58fe9a7 = tm_v137_base）

- **S1 = 第 157 次结构性免疫 + base18 锚连续 168140 B 免重锚**：fresh
  60f 四方（new ×2 + tm_v137_base ×2，quiet config）全字节一致 +
  跨版 savenorm EXACT。三重惰性论证：① 门恒 First = drawPlayerBody
  零 draw + 3 mesh 永不 lazy build；② 无输入 = 相位恒 0 = 静态 fast
  path 位级恒等（ENT-1 先例）；③ 图集 +4 尾部追加 = 既有 UV 零移位
  + 皮肤 tile 无采样者。
- **S3** = 1938/1938（我方构建，0 警告）。
- **S6 = init 点名 ×3 PASS**（scratch 资产变体，"player body model"
  精确点名）：a = head box size y 0.6 → 顶 1.9 > height 1.8 "box 0
  lies outside the 1.000000x1.800000 model AABB" / b = swing part
  'wing' → "swings[0] references unknown part 'wing'" / c = amplitude
  0 → "swings[1]: 'amplitude' must be in (0, 1] radians (the
  limb-swing range), got 0.000000"。

## 性能 A/B（UX-3 收官必做，发布前硬门槛）PASS

fall 900f 无输入，tm_v137_perf_old（v1.36.0 + 27 标记 split 布局）vs
tm_v137_perf_new（v1.37.0 + 同款补丁器，banner 双侧实证）：PERF-WIN
三窗 upd med 0.366/0.367/0.396 vs 0.401/0.410/0.468、render med
0.400/0.443/0.429 vs 0.427/0.441/0.471、swap med 15.78/15.71/15.76
vs 15.66/15.69/15.58 = **全指标同档**；PIPE3-S0 changesApplied
**9/3/1 三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff
0 = 同世界同成本（结构惰性最强形：门恒关 = 零身体 draw + 零 mesh
build）**。win3 fluid med 0.045→0.061 ms = 300 窗小值中位数环境
噪声（tickMs 双侧同幅同窗，世界状态逐位恒等排除行为差）。

## 偏离/记档

- **占位皮肤非人形细节**（纯色分区，脸/眼/发 = 色块不画五官——真实
  皮肤同布局一键替换，脚本接口已备）。
- **头部 tile 只表征 4 侧面**（8×8×4 面恰好铺满 16×16，顶/底 cap 不
  表征——头部 cap 不朝向玩家 = 视觉影响零；占位约束记档）。
- **躯干 tile 缺 4 侧面**（front/back 8×12 + top/bottom 8×4 = 256 px
  恰满，侧 4×12 不表征——侧面在占位色下与 front 同色域，影响可忽略；
  真实皮肤替换时派生逻辑同脚本可扩）。
- **模型烘焙恒亮**（skyLight 1.0 / blockLight 0，mob 同款）：不做
  PRE-6/LGT-4 世界光覆盖 = 夜间玩家身体不暗（与 mob 一致，记档）。
- **crouch 时身体不缩放**（crouchHeight 1.6 只改碰撞 AABB，模型恒
  1.8——蹲下穿地视觉 = 记档偏离，MC 同款有蹲缩动画矩阵外）。
- **相位无条件推进**（零 config 门）：无输入恒 0 = 惰性，输入跑有
  移动即推进 = 预期（mob 侧 mobs.limb_swing 关不影响玩家）。
- 玩家身体不入存档（相位 render-only；模型/皮肤 = init 期纯资产）=
  零 format（22）。

## PITS

- 无新增（门 157 四方一次过；S6 首轮日志路径笔误 = 编排脚本子壳 cd
  后相对路径，非项目缺陷，未入档）。
