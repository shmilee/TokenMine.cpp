# 机制详解（现状 v3.13.0）

> 2026-11-06 报障刷新：现状 = **v3.12.0**（3.x 矩阵收官于 v3.11.0；v3.12.0 = 实机报障阴影缩尺）（3.x「世界
> 扩展」期：v3.1.0 = V3-7 存档 gzip，门 191；v3.2.0 = V3-1 Phase A
> 下界门框，门 192；v3.3.0 = V3-8 手柄，门 193；v3.4.0 = V3-1 Phase B
> 下界世界 + 真传送，门 194【format 30】；v3.5.0 = V3-2/Phase C Part 1
> 下界内容族 + D9 空腔带修订，门 195；v3.6.0 = V3-4 netherite 材料链，门 196【零核心代码】；v3.7.0 = V3-2 Part 2 下界深化，门 197【全 nether-scoped】；v3.8.0 = V3-3 下界 mob，门 198【neutral/walks_on_lava 旗 + 下界池】；v3.9.0 = V3-5 建筑内容族，门 199【水底 sediment 生成】；v3.10.0 = V3-6 世界边缘雾，门 200【填充感知 fog end，base19 重锚】；v3.11.0 = V3-9 demo 回放，门 201【注入内核 + 双 Input 路由，3.x 矩阵收官】；其前 2.x 两批 20 版收官
> v2.0.0 → v2.19.0）、无输入基线门 **202** 次、单元测试
> **2777/2777**、`-Wall -Wextra -Wpedantic -Werror` 0 警告、存档
> **format 30**（下界维度：player.dimension + nether 段 +
> nether_entities；gzip 编码 V3-7 起；下界内容 = 世界生成演进零
> format）、base19 锚 **225272 B**（重锚谱系 179116 → 178148【V2-10
> 门 181】→ 178676【V2-19 门 182】→ 179027【V2-11 门 183】→ 224468
> 【V2-18 门 190，AO 世界光照】→ **225272【V3-6 门 200，填充感知雾 = 3.x
> 首重锚】**；门 184–189 = 结构免疫第 65–70 次免重锚；门 191–199 =
> 第 72–80 次；门 201–202 = 第 81–82 次免重锚（对 base19）【v3.0.0
> banner = 第 71 次】）、PITS **400**。上一轮全面刷新
> （2026-11-02）止于 v2.0.0、2026-11-03 增量止于 v2.3.0（下条），本次把
> v2.11.0 → v2.19.0 的机制（提前项 `[V2-19]` + 第二批 `[V2-11]`~`[V2-18]`，
> 逐版见 `docs/archive/v2x/CHANGELOG.md`，细节证据见 `docs/archive/v2x/`
> 20 份阶段存档）并入各节；3.x 十二版（v3.1.0–v3.12.0）
> 同样并入（逐版见 `docs/CHANGELOG.md`，细节见 `docs/archive/
> phase-3.x.0.md` 12 份）。
>
> **2026-11-03 增量（v2.1.0→v2.3.0）**：V2-1 前视角（门 172，`C` 三循环 +
> 翻转角 + aimReach）/ V2-2 Steve 皮肤（门 173，MobBox per-face 纹理）/
> **V2-3 教程系列化（门 174，tutorials/ 三系列 + format 25 + pickup./
> smelt. 事件）**；现状 = 测试 **2109/2109**、存档 **format 25**、门计数
> **174**（教程节/键位/存档 format 节/F3 行已同步；相机/皮肤细节见
> `docs/archive/v2x/phase-2.1.0.md` / `phase-2.2.0.md` / `phase-2.3.0.md`）。
>
> **来源约定**：本文所有具体数字/字段名/常量值以本文件各条的标注为准，细节
> （机制公式、单测、门证据、偏离记档全文）见 `docs/archive/v1x/phase-*.md`
> （181 份阶段存档）/ `docs/archive/v2x/phase-2.x.0.md`（20 份，2.x 两批）
> + 各代 `HISTORY.md` 全史（只读历史）；活跃已知问题与已修复索引见
> `docs/KNOWN_ISSUES.md`；逐版明细见 `docs/CHANGELOG.md`（v2.x 全两批 →
> `docs/archive/v2x/CHANGELOG.md`）；执行协议（命令/硬规则/E2E）见
> `docs/PROTOCOL.md` 与 `AGENTS.md`。
>
> **阶段标签约定**：`Phase NN`（或 `[PNN]`）= v0.x 时代编号阶段；`[XXX-N]` =
> v1.x 任务表任务 ID（如 `[ENT-7]`/`[SURV-3]`/`[CRAFT-1]`/`[PRE-2]`/`[STRM-1]`）；
> `[99.N]` = P99 后性能审计系列；`[V2-N]` = 2.x 任务表任务 ID（第一批
> V2-1~V2-10 + 提前项 V2-19 + 第二批 V2-11~V2-18；任务号与版本号解耦——
> `[V2-19]` = v2.11.0 / `[V2-11]` = v2.12.0 / `[V2-18]` = v2.19.0）。
>
> **config 约定**：下文「代码默认」= 缺键回落值；`config/config.json` 是**已提交
> 文件**，含用户调值（`world.view_distance` 80【代码默认 8】/
> `world.day_length_seconds` 600【代码默认 240】/ `ui.scale` 1.5【代码默认 1.0】/
> `world.seed` 123456789 + `player` 节全默认值）——文档与行为以代码默认为准，
> 实际运行以已提交 config 为准。
**本文 = 玩家/系统侧；世界/渲染侧（世界与流送/物理/光照/流体/火/下界维度/下界传送门/世界生成/渲染与表现/音频）= `docs/MECHANICS_WORLD.md`**

## 内容总览（现状）

- **技术栈**：C++20 / SDL3 / OpenGL 3.3 core；自研 `Result`/JSON/PNG；
  **内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，不改核心代码，注册表
  严格校验）；**确定性是一级要求**（无输入运行逐帧字节可复现）
- **方块**：**127** 个数据方块（`assets/data/blocks/`）+ 内置 air 哨兵（非数据，
  `BlockID{0}`，注册表从 id 1 分配）。模型形状分布：full cube（多数——87，其中
  47 无 shape 字段缺省 + 40 显式 `full_cube`，含 leaves/cactus 等透明或实心块
  + 3.x 下界族/建筑族）/
  cross 17（wheat_1..7、3 花、3 sapling、torch、fire、campfire 家族）/ stairs
  4 / slab 16（stone_slab `state_count 3` `[V2-17]` + 15 床块 `[V2-13]`）/
  wall 1 / fence 1 / glass_pane 1；3 个 sapling 另带 `tree` 对象（shape 词汇
  {oak, spruce, birch} = 落树形状）。3.x 新增 34 块：nether_portal `[V3-1]`
  （state_count 2 axis，full-cell 半透盒）/ netherrack `[V3-1]` / 下界内容族
  12 块 `[V3-2]`（nylium×2 / stem×2 / basalt / soul_sand / soul_soil /
  magma_block / nether_brick / 矿脉×3）/ 建筑内容族 20 块 `[V3-5]`（clay /
  mossy_cobblestone / bricks / stone_bricks / mossy_stone_bricks / terracotta
  15 色染色族）
- **物品**：**224** = 127 个隐式 block item（与方块同名，无独立 JSON——注册表拒
  显式同名物品文件，「block items are defined by their block」）+ **97 个显式
  物品**（`assets/data/items/`，`[ENT-9]` 起**全部**带 1..16 box 的 3D `model`
  段 = 三上下文渲染全覆盖；含 pork/cooked_pork `[V2-5]` / string·spider_eye
  `[V2-7]` / shears `[V2-15]` / quartz `[V3-2]` / hoglin_meat·cooked_
  hoglin_meat `[V3-3]` / netherite_scrap·ingot·工具×3·护甲×4 `[V3-4]` /
  clay_ball·brick `[V3-5]`）
- **状态效果**：**11**（`assets/data/effects/`，见「状态效果」）；**Mob**：**13**
  （主世界 8 = zombie/skeleton/spider + cow/sheep/chicken/pig `[V2-5]`/bat
  `[V2-14]` + 下界 5 `[V3-3]` = zombified_piglin/piglin/hoglin/magma_cube/
  strider，`assets/data/mobs/`，见「实体与 Mob」）；**配方**：**112**（shaped 58 /
  shapeless 54，`assets/data/recipes/`）；**生物群系**：9（主世界 4 = desert/
  forest/plains/snowy_taiga `[V2-16]` + 下界 5 `[V3-2]` = nether_wastes/
  crimson_forest/soul_sand_valley/basalt_deltas/warped_forest，
  `assets/data/biomes_nether/` 独立池）
- **食物**：14 种（`restore_hunger`，见「生存系统」，含 pork/cooked_pork
  `[V2-5]` / hoglin_meat·cooked_hoglin_meat `[V3-3]`）；**染料**：15 色；**护甲**：
  16 件（+netherite×4 `[V3-4]`）；**耐久**：36 件（12 工具 + shears + 6 弓 +
  燧石与钢 + 16 护甲，`[SURV-3]` 首批 27 件 + `[SURV-5]` poison_bow +
  `[V2-15]` shears 238 + `[V3-4]` netherite 7 件）
- **音效**：**63** 个确定性 WAV（`tools/generate_sounds.py` 固定种子，重跑字节
  幂等；17 基础事件 + 18 材质音 6×3 + 3 环境音 + 1 smelt + 1 雷 `[V2-4]` + 1
  sleep `[V2-13]` + 1 squeak `[V2-14]` + 1 shear `[V2-15]` + 3 snow 族
  `[V2-16]` + **3 portal 族 `[V3-1]`**（trigger/travel/ambient）+ **11 下界族
  `[V3-2 Part 2]`**（nether/nylium/stem 3 材质×3 + evaporate + ambience_
  nether）+ **3 下界 mob 氛围 `[V3-3]`**（piglin_whisper/hoglin_grunt/
  strider_hiss）；`[V2-16]` 音效缺陷族 13 件修复 = 字节变更件数不变）
- **纹理**：block atlas **16×16**（256 tile，**现状 165 tile 占用**——3.x 增长
  谱系 122→136 `[V3-2]`（16×8→16×16 幂二增长，**既有格零移位** = 像素行恒等
  锚免重锚实证）→145 `[V3-3]` →165 `[V3-5]`，
  `assets/data/textures/block_atlas.json` 数据驱动布局——加纹理 = 加 PNG + 加一行
  JSON，不改代码；**atlas 空位**纪律 = 不重锚基线）；物品图标 = 每物品独立
  32×32 PNG；字体 **256×2112**（ASCII 16×16 字形 + **427 个 CJK glyph**
  `[PRE-1]` 33 → `[CRAFT-4]` 34 → `[UX-6]` 392 → `[UX-8a]` 395 → `[UX-9b]` 401
  → `[V2-3]` 422 → `[V2-5]` 423 → `[V2-12]` 427（锋利限魔 等 4 字），顶 512
  行位级不变）；health/hunger 48×16 条带、oxygen 16×16、effects **176×16**
  （11 tile）、cracks 80×16、mob 图集（+ `[UX-3]` 玩家占位皮肤 64×64 + 4 派生
  16×16 tile 尾部追加）
- **初始库存**（`assets/data/hotbar.json`）：hotbar 9 格（stone/dirt/grass/
  sand/wood/leaves/glass/glowstone/wooden_pickaxe，每格满堆栈）+ **隐藏 starter
  存储槽 16–26**（honey_bucket ×1 `[FLUID-6]` / glass_bottle ×4 + 6 瓶单效果药水
  `[SURV-4]` / oak·spruce·birch sapling 各 ×3 `[GEN-2]`）——隐藏槽不渲染，
  无输入零活动
- **CLI**（`--help` 看全部）：`--frames N`（到点退出）/ `--screenshot /abs.png`
  （帧边界截图）/ `--save slot` / `--new`（忽略现有存档开新档）/ `--seed N` /
  `--time 0..1` / `--config path` / `--log-level` / `--save-full-diff`（`[PIPE-2]`
  oracle 旗）/ `--weather clear|rain`（`[V2-4]`，仅覆盖初始天气态）/
  `--list-saves` / `--copy-save S D` / `--rename-save S D` /
  `--delete-save S`（`[SAVE-3]` 管理旗，独占）/ `--demo <file>` /
  `--demo-author <t>` / `--demo-record [o]`（`[V3-9]` demo 回放三旗互斥，
  见「demo 回放」）

## 操作与交互

- **第一人称玩家**：`W/A/S/D` 行走（4.3 b/s），`LeftShift` 冲刺（5.6 b/s，
  非水中 + 移动；疾跑另见饥饿 ×2 与熔岩内抑制），`Space` 跳跃（仅贴地时；
  水中 = 上浮），`LeftControl` 潜行（身高 1.8→1.6 脚底不动、眼高 1.62→1.42、
  速度 ×0.3、1 格崖缘 guard、**禁用 step-up**、低顶陷阱：头顶 <1.8 松键不站起）；
  鼠标转向——**左键点击锁定指针；转向仅在指针锁定时生效**（未锁定光标不转
  视角 = 无输入运行截图逐字节复现的保证，硬规则 5）：yaw 0 = 朝 -Z、正 =
  左转；pitch 正 = 上、clamp ±~89°；灵敏度 0.0025 rad/px
- **手柄 `[V3-8]`（v3.3.0，输入边界层）+ `[V3-8b]`（v3.14.0 习惯定案
  映射）**：SDL3 GameController 事件 →
  游戏已消费的同一 `GameKey`/`MouseButton` 状态——**源位掩码 OR 语义**
  （`m_keyDown` 每源一 bit：bit0 键鼠 / bit1 手柄；`isDown` = 任一源按
  下；每源独立 rising edge；`wasReleased`（弓）仅本源 falling 且掩码清 0
  才置 = 另一源仍按着弓不松手；**无手柄 = bit1 永不置位 = 位级恒等**
  （结构免疫根 + legacy fixture 单测钉））。映射（**V3-8b 定案表**，
  Xbox 布局——SDL 手柄数据库把 PlayStation / Switch 布局归一化到同一
  逻辑位 = 单表覆盖三大布局）：左摇杆 = 移动（死区
  `kGamepadAxisDeadzone 0.2` 外数字化，每次 axis 事件四方向全量重算幂
  等）/ 右摇杆 = 视角（死区外重映射 × `kGamepadLookPixelsPerUnit 100`
  合成 mouseDelta：杆右 = xrel+，杆上 = yrel−）/ **RT 扳机 = LeftShift
  疾跑**（V3-8b：0.2 死区数字化按住 = Shift，左摇杆同款；原 B 位解放）/
  **A = Space**（跳跃 + 双击飞行继承，记档冗余 = B 已有专用钮）/ **B =
  `GameKey::FlyToggle` 飞行专用钮**（V3-8b：单击直接 `setFlying(!)`，无
  双击窗；**仅手柄键** = `mapKey` 永不返回【黑盒 256 键扫描单测钉】，键
  盘飞行 = Space 双击不变）/ X = E / Y = Q / LB = 左键 / RB = 右键 /
  Back = C / Start = Escape / D-pad 右 = Tab / **L3 = LeftControl 蹲伏**
  （V3-8b：补键盘 Ctrl 的手柄孪生缺口，MC Java 惯例，按住 = 蹲）；LT /
  R3 / Guide / D-pad 上下左 / paddles = 不映射（记档）。单手柄模型
  （第二手柄忽略）；连断日志 + F3 末行连接态；断连清全部手柄位**不置释
  放边**（弓 stale-up 分支取消蓄力而非悬挂）。无 config 键（映射 = 命
  名常量；全自定义 = V3-8b 用户定案不做）；`SDL_Init(VIDEO | GAMEPAD)`
  （无手柄环境惰性零成本）；**V3-8a**（用户 11-06 并行提交 d5a5ea6）=
  `SDL_OpenGamepad` 显式 open（SDL 3.x 事件须 open 后才投递——connected
  ≠ 可用，合成事件单测盲区，PITS 395）
- **第三人称 `[UX-4]`/`[UX-8g]`/v2.0.0**：`C` 切换第一/第三人称（MC 现代相机键；
  F5 已被手动存档占用 = 记档偏离；仅指针锁定时生效）——第三人称 = 相机**眼位后
  上方球面偏移 4.0 块**（`kThirdPersonDistance`，offset = −Camera::forward×D 与
  视图矩阵位级同源）+ **v2.0.0 准星不压头**：轨道点沿**相机 up 轴**抬高
  `kThirdPersonOrbitHeight` **0.4**（`thirdPersonOrbitCenter(eye, yaw, pitch)`
  纯函数 = 眼位 + `Camera::up()`×0.4，up = cross(right, forward) 与视图矩阵
  锁步单一真源——pitch 0 时头 top ≈**29 px** / 头底缘 ≈**96 px** 低于屏幕中心，
  任意 yaw/pitch 恒定 = 准星零重叠；crouch 随眼位降零额外代码；第一人称分支
  字节不变 = 结构免疫根）+ **遮挡收敛**（`kCameraOcclusionMargin` **0.1**：
  嵌墙 = 解析穿出留 0.1 间隙**玩家侧** / 远墙 = raycastBlock 面前 0.1 停 /
  穿流体 = 相机看水 MC 同款）+ **零状态**（相机每帧派生、无 Game 成员 = 零
  format / 不入档，读档回 First = MC 同款）；**瞄准射线两模式均自
  `eyePosition()` `[UX-8g]`**（5 处交互射线原点统一：targetBlock 挖掘/放置/
  交互/裂纹全链 / targetMob / fireBow 箭生成 / tryFluidBucket 双射线；方向
  `m_camera.forward()` 零动 = 两模式皆玩家朝向；**第一人称位级恒等零回归**；
  声音 listener 保持相机）→ 屏幕中心射线与眼位射线平行偏移 0.4 = **固定第三
  人称视差**（MC 同款观感，记档偏离——精确瞄准可切第一人称，见 KNOWN_ISSUES）；
  **玩家身体 `[UX-3]`**（第三人称可见）= **MC 标准 5 部件 box**（头 0.5³ / 躯干
  0.5×0.55×0.25 / 四肢 0.25×0.75×0.25，总高 1.8）+ **行走摆臂摆腿**（四部件
  对角步态 arm_l+leg_r / arm_r+leg_l，amplitude leg 0.35 / arm 0.3 = ENT-1
  `advanceSwingPhase` 相位机镜像进 `Player::update`，`checkSwingClip` 64 相位
  验收）+ **生成式占位皮肤**（64×64 经典布局官方 wiki 4px-arm 模板逐区解码 +
  `tools/generate_player_skin.py` 确定性幂等 + 4 派生 16×16 tile 尾部追加图集
  免重锚，真实皮肤同布局一键替换）+ `drawPlayerBody`（opaque 段 drawMobs 后，
  烘焙恒亮 = mob 同款记档偏离；模型 width 1.0 = box 验证界非碰撞，玩家 AABB
  0.6×1.8 零触碰）；drawHeldHand 第三人称隐藏（2D 手持角标 = HUD 层照画；
  **第一人称手臂 `[V2-6]`** = 臂 viewmodel，见 MECHANICS_WORLD.md「渲染与表现」）
- **滚轮缩放 `[V2-19]`（v2.11.0）**：SDL 滚轮 = session `m_thirdPersonDistance`
  调节（默认 `kThirdPersonDistance` 4.0 = `resolveThirdPersonCamera` **默认
  参数位级不变**【既有 16 单测零改动】；C 切模式不重置、不存档 = 相机 mode
  同款 session-state 先例）——**仅 ThirdBack/ThirdFront 生效**（First 滚轮
  no-op），clamp **2.0–8.0** 步进 **0.5**（MC 原版无滚轮缩放 = mod 惯例区
  间，记档偏离）；distance 默认参三处消费（offset 缩放/raycast reach/
  embedded clamp）+ **aimReach 联动**（ThirdFront = distance + `kReach` =
  拉近够近拉远够远，V2-1 前视约定泛化；First/ThirdBack 位级不变；**瞄准射
  线仍自眼位 UX-8g 零触碰**）+ F3 `cam <Mode> D x.x` 行
- **玩家飞行 `[V2-19]`（v2.11.0，"中国人会飞"）**：**双击 Space** toggle
  （`kFlyDoubleTapSeconds` **0.3 s** real-dt 窗 = CRAFT-7 双击窗同款纯函数
  `core/fly`，第二击吞跳跃；仅指针锁定 + 存活，面板开不响应；MC creative
  同款手势）：飞行中 = **重力跳过** + Space 按住升 / LeftShift 按住降
  `kFlyVerticalSpeed` **4.0 b/s**（潜行机制照常，Shift 双义 = MC creative
  同款）+ 水平 = walk × **1.6** = **6.88 b/s**（> 冲刺 5.6 = 快于走路）+
  碰撞照常（AABB 子步化不变）；**坠落累加器飞行中每帧归零**（悬停零累计、
  切出帧自切换高度计坠 = 无爆发伤害）+ 落地 `!flying` 免伤门（受控下降 =
  creative 同款）；**水/熔岩自动切出**（MC creative 水下可飞 = 记档偏
  离）+ **不存档**（MC 存 flying 标志 = 记档偏离，省 format）+ 死亡/重生
  复位；无输入双击永不触发 = 结构免疫；F3 `cam` 行携 `flying` 标记
- **24h HUD 时钟 `[V2-19]`（v2.11.0）**：`DayCycle::clockString` 纯函数
  `hour24 = fmod((t − kDawnStart 0.05)×24 + 6, 24)` **与 ramp 边精确对
  齐**（晨 06:00 @ t0.05 / 午 12:00 @ t0.3 / 昏 18:00 @ t0.55 / 夜 00:00 @
  t0.8；分钟 floor 两位补零；ulp 钉 `clockString(0.0)` = "04:47" = 记档偏
  离）——**renderUi 右上角恒可见**（每指针/面板/死亡态 = HUD 常显例外；MC
  无 HUD 时钟 = 记档新功能；绘制在 renderUi 而非 drawWorldOverlay = 记档偏
  离，screen-space 通道约束）；`:`（0x3A）字形 ASCII 行在列 = 零字体扩展；
  600 s 天 = 1 游戏分钟 25 帧（帧锁定推进 = 像素为帧数确定性函数）
- **左键**（锁定，**按住**）：准星命中 mob（射线先比方块距离——**方块遮挡**：
  方块更近则打方块）= 挥击，每 `kSwingInterval` **0.25s** 一击：伤害 = 选中
  物品 `attack_damage` 数据（徒手/方块/材料 = 1.0；shipped：wooden/stone/iron
  pickaxe 2/2.5/3、axe 3/3.5/4、shovel 2.5/3/3.5、bow 1）− 活跃效果
  `melee_penalty` 总和（clamp ≥0）+ 活跃效果 `melee_bonus` 总和（`[P84]`）；
  沿瞄准方向击退（水平 4 + 抬升 2.5，`kKnockbackTime` 0.3s 窗口内 AI 不接管
  速度）；**工具/弓/点火器近战命中 = 耐久磨损事件**（`[SURV-3]`，见下）。
  未命中 mob = **破坏方块**
- **破坏**（DDA 体素 raycast，`kReach` 5 格；raycast 把水体当 air 穿过 = 可隔水
  破坏湖底/水下方块，水被排水后流体模拟补回；**`[V2-17]` slab 半格感知**：
  射线从 slab 格的**空气半**进入 = 穿透（命中内表面 y=0.5【hit cell = slab
  格，法线 ±Y】或身后的块——MC 同款；**记档偏离：cell-DDA 旧行为变更**，
  旧「瞄准底 slab 空气半 = 破 slab」→ 新「破身后的块」；瞄准实心半 / 双
  slab 满格 = 整格命中不变；共享 dda 走块射线 + 流体射线一致，第三人称
  相机遮挡 / 箭视线同享）：耗时 = `kBreakBaseSeconds`
  **1.5s** × `properties.hardness` ÷ `effectiveBreakSpeed`（`properties.
  destroyable` 数据驱动，基岩不可破坏；准星移开/松键/换工具 = 进度清零）。
  **有效速度 = `effectiveBreakSpeed(item, block)` 纯函数**（`item.hpp`，
  硬规则 3 数据驱动）：徒手/非工具 = 1.0；tool = `tool.break_speed`（镐 wooden
  2.0 / stone 3.0 / iron 4.0；斧/铲缺省 1.0 = 手速）×（目标块在物品
  `fast_against` 列表内 ? `fast_against_multiplier` 2.0 : **`kWrongToolFactor`
  0.1** `[SURV-8]` = MC 无效率工具 10× 惩罚——**无列表工具对一切方块吃
  惩罚** = MC 剑情形）。shipped 清单：3 镐 ×2 对 **10 块**（stone/
  cobblestone/coal_ore/iron_ore/gold_ore/diamond_ore/lapis_ore/glowstone/
  sandstone/smooth_sandstone，`[P69]`→`[P80]`→`[P81]` 契约演进）、3 斧 ×2 对
  wood/leaves（`[P67]`；spruce/birch trunk 未入清单 = KNOWN_ISSUES）、3 铲 ×2
  对 dirt/grass/sand/gravel（`[P68]` = 项目土类全集）。经典示例：铁镐 × 石
  0.28125 s（on-list）/ × 木 7.5 s（off-list flat 0.75 ÷10）/ × obsidian
  37.5 s（obsidian 排除条款随规则演进 = MC 方向一致）。`fast_against` 与
  `fast_against_multiplier` 严格配对（缺一/空列表/非 tool 均解析拒绝，init
  逐条目交叉校验块名存在 = grows_on 同款）
- **右键**（锁定）完整通道链（**通道所有权纪律**：前者消费点击，落空继续；
  点火器之后 = 护甲之前 = 潜行旋转之前）：
  ① **营火**（准星命中营火家族块：放食物/空手取回/燧石再点燃——见「营火」，
  在全部物品通道之前）→ ② **食物**（选中食物 = 开始进食通道，点击被吞；满
  饥饿 no-op 仍吞点击）→ ③ **饮用**（`drink_time` 容器 = 开始饮用通道）→
  ④ **launcher**（`shoot_speed > 0` = 拉弓）→ ⑤ **挤奶**（准星命中 `milk_with`
  == 选中物品的 mob）→ ⑥ **interact**（块 `interact` 属性 = 打开工作台/炉子
  面板）→ ⑦ **染色**（染料 + dye 家族方块）→ ⑧ **流体桶**（舀/倒——在
  interact 之后：准星对着炉子/工作台时即使手持桶也开面板，MC）→ ⑨ **点火器**
  （`ignites_flamable`：`ignites_to` 再点燃 / flamable 点燃成 fire）→ ⑩ **护甲**
  （`[SURV-1]`：手持件 = 穿戴/交换，空手 = 按 head→chest→legs→feet 固定序
  脱下到选中槽）→ ⑪ **潜行 + 有状态块** = 原地旋转 `(s+1)%stateCount`（`[P94]`
  →`[ENT-2]`，"place" 音【`[CRAFT-3]` 材质音】、点击消费）→ ⑫ **放置**（命中
  面邻格放当前选中格的方块物品 + 消耗 1 个；`canPlaceAt` = 不放 solid/不重叠
  玩家 AABB；作物 `plant_on` 门：wheat_1..6 下方须 farmland、sapling 下方须
  dirt/grass；**楼梯放置自动朝向** `[P97]` = `stairFacingFromYaw` 纯函数，低端
  朝玩家视线方向，45° 平局取 X 轴）
- **按键**：`Tab` 循环切 hotbar；**数字键 1–9 直接选热栏槽**（`[CRAFT-7]`，
  面板开/关皆可用）；`Q` 丢弃选中格 1 个到脚下（0.5s 拾取延迟防立即回吸）；
  `E` 开/关背包面板（释放指针、暂停玩家物理与方块交互，世界/昼夜/流体/实体/
  流式/熔炼照常 tick——与 MC「暂停菜单」不同，接近「边看背包边看世界」）；
  `F3` 切换调试 overlay（任意指针状态，英文 = 开发者工具记档 `[PRE-1]`）；
  `F5` 立即存档（`--save` 槽激活时，日志记录结果）；`C` 切换第一/第三人称
  （`[UX-4]`，见上）；`T` 跳过**当前教程系列**（`[UX-7]`/`[V2-3]`，仅指针
  锁定且教程未结束时；末系列 = 效果等同完成 + toast）；`ESC` 释放指针 /
  未锁定时
  退出（干净退出自动写回）；`R` 死亡后重生
- **背包面板操作**（`[CRAFT-7]` = MC 操作面 3/4 项 + 数字键）：左键 = 取整堆/
  放入/交换/并入有空间同物堆（**合并门 = 同物品且同 damage** `[SURV-3]`）；
  **右键取半堆**（空手右键占位格取 floor(N/2) 最小 1：N=1 取自身 / N=2,3 取 1 /
  N=64 取 32；共享 cellRightClick 原语三面板同语义；耐久随堆）；**Shift 快捷
  移动**（hotbar⇄storage 互移索引序合并 + 容器格→主背包优先 + 堆跨格分裂 +
  手持堆 no-op；炉子面板 = 容器 GUI：炉槽→主背包、移 input 按不变量重置进度；
  左右 Shift 皆可）；**双击收集分拣**（**MC 分区语义：hotbar 0..8 与主背包
  9..35 独立分区** `[UX-8f]` 36 槽起；收集同物同耐久入点击格 + 溢出索引序回填；`kDoubleClick-
  Seconds` 0.3 真实 dt 窗 = 生存计时器类输入态）；**工作台材料修复
  `[UX-5]`**（合成网格 = 受损工具/护甲 + 1 材料 → 输出槽 = 修复后物品【1 材料
  +25% max 耐久 `kRepairFraction` 0.25】，点击输出取走；**配方恒优先、repair =
  无配方回退**；输出三重守卫 = 异 item/异 damage/maxStack 溢出拒；E 面板 2×2
  同款生效——详见「合成与容器 · 工作台」）；**无拖拽**（记档偏离：三
  点击 + 数字键覆盖核心操作面，拖拽 = 捕获下轨迹跟踪最高风险 E2E 类矩阵外）；
  点击面板外（左键）= 丢弃手持堆；关闭面板时手持堆保留在 UI 实例（MC 会退回
  背包 = 记档偏离）；**悬停 tooltip**（`[PRE-12]`，见 MECHANICS_WORLD.md「渲染与表现」）
- **新手指导 `[UX-7]`/`[UX-8a]`/`[UX-9b]`**（MC objective 式数据驱动逐步教程，
  内容 = `assets/data/tutorials/beginner.json` 严格解析【四键齐全/id 唯一/count
  ≥1 全点名】）：shipped **11 步链** = walk 按 W 走 → chop 打木头×3 → **planks
  合成 4 木板**【`[UX-8a]`，tip「按 E 打开面板，选木头点输出」】→ table 做工作
  台（4 木板排 2×2）→ wpick 木镐 → stone 挖石×5 → spick 石镐 → **coal 挖 1 煤
  矿石 / torch 合成 1 火把 / light 放 1 火把 = 三步点灯**【`[UX-9b]`，消费
  「掉在洞里黑乎乎怎么打怪」实机报障：光 14 > 敌对生成门槛 7】→ kill 打 1 只
  鸡；每步可选 **`tip` 辅助行**（`[UX-8a]` 严格解析，主行上方 kTextDim）；
  **`[V2-3]` 三系列**：内容 = `assets/data/tutorials/` 目录**多文件按名序 =
  系列序**（beginner 11 步字节零改动 + **farming 农耕 7 步**（2 种子→2
  耕地→2 种→2 收=4 小麦→面包→吃，×2 经济账修订）+ **kitchen 厨房 6 步**
  （3 锻石→炉子→放→开→熔炼熟鸡→吃））；跨系列步 id 全局唯一（init 点名）；
  **系列过渡无 toast**（目标行换步即提示，定案）；**封闭事件词表**
  （craft./break./place./kill./interact. + **pickup./smelt.** `[V2-3]`
  + walk/jump/eat 裸词，`emitTutorialEvent` 单点收口各一调用点——repair
  完成不发 craft = UX-5 边界；**recipe 行合成与 grid/output 路径同发
  craft 事件** = 门 174 内修复的 UX-7 潜伏漏发 PITS 360）+ **纯函数状态机**
  `core/tutorial`（campfire 范式帧锁定，进度 ≥ count 步进，越系列 → 下系列
  首步，越末系列 done）+ **format 25 `player.tutorial`**（恒写
  {series,id,count}/{}；三向键纪律：25 缺 series 拒 / 23-24 携 series =
  版本谎报拒 / ≤24 载入 series 缺省 0 = 值保真免迁；**series 越界或 id
  不在该系列 = 重置上报**不拒档/count 越界损坏点名；读档跨续接）+
  **UI**（下中部目标行 PRE-10 堆叠纪律 + 120 帧完成 toast【仅末系列】+
  **`T` 跳当前系列** + config `tutorial.enabled` 严格读代码默认 true
  config.json 零改动 + F3 `tutorial` 行）；字集扩容 33→392 `[UX-6]` →
  395 `[UX-8a]` → 401 `[UX-9b]` → **422 `[V2-3]`（+株 等 21 字）**，顶
  512 行位级不变免重锚
- **破坏反馈**（`[P15]`）：**渐进裂纹**——破坏进度映射 5 级累积裂纹纹理
  （cracks.png 80×16 五格，白 alpha 随机游走裂纹），破坏中的方块叠加半透明
  裂纹面（scale 1.006 免 Z-fight）；**破坏粒子**——方块碎成 16 个 0.12 格小
  立方体（初速弹散 + 重力，0.45~0.75s 消亡，无碰撞，spawn 时烘焙所在格光照——
  渲染是只读窗口不能读光数组），按桶路由（`[PRE-5]`：opaque 块粒子留 opaque /
  cutout 块（叶、十字植物、fire 余烬）进 cutout / 其余 blend），逐帧动态 mesh
  （全局粒子上限 256）

## 实体与 Mob

- **掉落物**（`ItemEntity` `[P10]`）：**block item = 0.25 立方体**（atlas 光照
  随昼夜变暗）/ **非 block item = 3D box 模型**（`[CRAFT-2]` 16 件 →
  **`[ENT-9]` 全 81 件**非 block 物品补齐 model——mob 掉落 rotten_flesh/bone/
  arrow/feather/beef/mutton/pork/leather + 所有可 Q 丢弃物皆可见，零核心代码纯
  内容；家族几何 1–3 box 剪影，纹理 = 既有 icon PNG）；统一 Entity 碰撞物理
  （重力/终端速度/水平摩擦）；**零初速生成**（静止位精确、截图可复现——对 MC
  随机初速的刻意偏离）；**破坏掉落确定性伪散落 `[UX-8d]`**（`block/
  drop_scatter` 纯函数 = seed⊕位置⊕dropIndex 专用 salt 与 4 既有 salt 零碰撞：
  水平 (dx,dz) ∈ **±0.2²**、**y 不偏移**；界论证 0.325 < 0.5 = 物品**恒留被破
  格内**；只改破坏掉落——Q 丢弃/mob 击杀/拾取/存档/粒子/掉落光零触碰；合并
  安全 0.566 < 0.75 同物仍合并 = MC 随机观感的确定性替身）；落地悬浮摆动
  （1.5 s 周期）+ 自转（4 s 周期）= **帧
  计数器驱动**（非墙钟）；同类 **0.75 格**内自动合并（**合并门 = 同物品且同
  damage** `[SURV-3]`，堆叠上限按物品数据：block 64 / 工具护甲弓 1）；**120 s**
  （MC 同款）或掉出世界消失（age = real-dt 累积）；**拾取** = 玩家底心距 ≤
  **1.0 格**且过 **0.5 s** 延迟（MC 10-tick，防立即回吸）整堆入包（先合并
  同物堆、再找空位，hotbar 优先）——**拾取磁力 `[V2-6]`（v2.6.0）**：0.5 s
  延迟过后水平距离 ≤ `kMagnetRadius` **2.5** 的掉落物被逐渐吸向玩家 =
  **帧锁定**（`updateEntities` 内固定 `kMobStepPerFrame` 步长应用，物品自身
  物理仍 real-dt 不变）：位移 = f(距离, 帧) 纯函数（`entity/pickup_magnet.hpp`）
  径向速度 = `kMagnetMaxSpeed` **6** × (1−d/2.5)²（远慢近快 = 加速趋近 + 距
  离衰减，边缘 0）+ 侧向 **swirl**（周期 **24 帧**、幅值 = 径向 25% = 收紧
  螺旋，frame 有界 modulo 纪律）；y 分量恒 0（磁力仅水平，落差仍靠走近 = 记
  档偏离）；步进过 `hasRoomAt`（**磁力不穿墙** = MC 规则，受阻 = 留原地下帧
  重试）；**插墙箭（Phase 42 同款半径 1.0）同步消费** = stuck 箭被同款磁力
  **拉出方块**（无 room 检查：卡住箭 AABB 本就在 solid 内，stuck 不做物理
  步）；无 ready 掉落物/插墙箭 = 零执行（60f 基线零实体 = 结构免疫）；**流体**
  （`[ENT-4]`）：预置静水浮面静止（摩擦保留无漂移）/ 带动量坠水沉底站立（互
  斥）；**光照**（`[PRE-6]`）：AABB 中心格世界光 per-draw 覆盖（洞穴石头立方
  不再恒白，MC 同款）
- **箭**（`Projectile` `[P10]`/`[P24]`/`[P42]`）：重力 **20 b/s²**（MC 箭
  0.05/格/tick²）、终端 38、初速 = `shoot_speed × power`（bow 满 = 15 = MC
  满威力）、碰撞盒 0.25；**固定帧步长** `kProjectileStepPerFrame` 1/60（运动
  实体截图可复现约定）；**每帧对每支飞行箭做扫掠 AABB 测试**（上一步 → 本步，
  `raycastAabb` 位移为方向 t ∈ [0,1]）对每个存活 mob——命中则 `shoot_damage ×
  power` 伤害 + **随蓄力击退**（`arrowKnockback` 纯函数：水平 4.0×power +
  抬升 2.5×power——满威力 = 旧近战常数逐位相同）+ 箭消耗（**箭击杀 mob 同帧
  不施加效果** `[SURV-5]` 记档）；箭**穿过**玩家与其他实体（`collidesWith-
  Entities = false`，无自伤 = 单人生存 v1，MC 同款）、**穿过水不动**（投射物
  边界，`[ENT-4]` 记档）；**撞 solid = 插住**（stuck：留在命中处、子步前捕获
  `stuckDir` 供渲染——碰撞解算器会清零碰撞轴速度）；**插墙箭寿命 100 s**
  （`kStuckMaxAge`，MC 值；飞行箭 30 s `kMaxAge`；age 累计、按**当前**状态取
  上限——插墙 >30 s 后释放的箭立即消亡，MC 分歧已文档化）；**卡住格被
  破坏/替换时释放**（`Game::setBlock` 收口：破坏/放置/染色换块都走它；判定 =
  箭盒触及该格面且中心在面外）→ 零速度落回飞行物理（可再命中 mob）；**插墙
  箭拾取**（`[P42]`）：距玩家底心 ≤1.0 块即拾取——返还 1 个发射器 `ammo`
  数据点名物品（mob 箭返还该 mob 数据的 `ammo`，ranged mob 必填 + init 校验；
  飞行箭不可拾取；背包满 = 箭留墙内每帧重试）；箭**不落地成掉落物**、**不入
  存档**（瞬态，同 mob）；**`[PRE-9]` 视觉 = MC 式三段箭形**（`makeArrow-
  Geometry` 纯函数：棕杆盒 0.31 + 灰四棱锥箭头 0.12【尖 @ 前端 −Z】+ 白宽
  尾羽盒 0.09 > 杆 0.05，总长 0.55 = 原轮廓尺寸不变；`drawArrows` 每箭三色
  三次纯色 draw，零 shader/纹理/atlas 改动；残余 = 无 atlas 箭纹理记档）；
  **`[SURV-5]` 箭效果**：`arrow_effect` **双源**（item 侧要求 shoot_speed +
  affects_mobs 效果 / mob 侧要求 shoot_range；init 双向点名；Projectile 尾部
  effectId/effectDuration 默认 −1 位级恒等）——skeleton 箭 = **wither 10 s**
  （玩家侧）/ poison_bow 箭 = **poison 60 s**（mob 侧可毒死）= MC 值
- **13 种 mob**（`assets/data/mobs/`，全数据驱动 `MobEntity`/`MobRegistry`；
  数值 = normal 基线；下界 5 种 = `[V3-3]` v3.8.0，主世界池/下界池隔离生成）：
  - **zombie**（敌对）：20 HP / speed 0.8 / 探测 16 / 近战 1.6 / 伤害 3 /
    冷却 1.0 / 掉 rotten_flesh ×1 / `undead: true`
  - **skeleton**（敌对，远程）：20 HP / 0.8 / 探测 16 / `shoot_range` 14 /
    `retreat_distance` 3 / `shoot_speed` 15 / `attack_range` 0 = 永不近战 /
    掉 bone **0–2**（随机区间 `[P38]`）+ arrow ×1 / `undead: true` /
    `arrow_effect` = wither 10 s
  - **spider**（敌对）：16 HP / speed 1.0（比 zombie 快）/ 探测 16 / 近战 1.6 /
    伤害 2 / 冷却 1.0 / 掉 **string ×0–2 + spider_eye ×0–2**（`[V2-7]` 既有
    随机区间机制；弓配方用 planks 不用 string = 两物品纯收集记档）/ **首个
    `on_hit_effect`** = poison 15 s（MC normal 值）
  - **cow**（被动）：10 HP / 0.5 / 掉 leather ×1 + beef ×1 / **可挤奶**
    （`milk_with` bucket → `milk_gives` milk_bucket）
  - **sheep**（被动）：8 HP / 0.5 / 掉 **wool 1–3**（`[V2-15]` MC 精确随机
    区间，死亡时 = 实例毛色变体，下条）+ mutton ×1；**`[V2-15]` 染色羊
    （v2.16.0）**：`wool_drops: true` + `wool_weights` **严格对**（shipped
    temperate 整数表 white **850** / black **80** / gray **30** / brown **30** /
    pink **10**，记档偏离：MC biome 表 → 项目单一 temperate 表；词表 = 项目
    15 色无 lime，MC 16 含 lime = 记档偏离）= **出生毛色 per-mob 流首抽**加权
    roll（非 wool mob 构造零消费 = 位级红线）+ **剪毛**（持 **shears** 右键：
    shears = 新物品 `shears` 严格 bool 旗 + durability **238** MC 精确 +
    break_speed 2 + `fast_against` 整字段省略 = 错工具 1/10 罚 + shapeless 2
    iron_ingot；剪毛磨损 +1）→ **1 个 ItemEntity 携 roll count 1–3 彩毛**于
    mob 位（"shear" 音空间化）+ bare 置位（bare 再剪 = no-op）+ **再生
    7200 f = 120 s 帧锁定**（记档偏离：MC 吃草门随机 ~12 min，项目无草交互）
    恢复当前染色色（染色保留语义）+ **染色**（持染料右键，**bare 拒染** = MC
    剪后不可染，"cloth" 音，耗 1）+ **死亡彩毛钩子** `woolDeathDropName` 纯
    函数（woolly → 实例色 wool 变体 / bare → 跳过 / 非 wool mob 位级不变）+
    **per-entity 变体网格**（swingMeshes 范式懒建/despawn 释放/变体变更重建，
    `withWoolVariantTile` 仅换 texture 几何位级不变；15 色复用 wool 族块
    tile，bare = 新 sheep_skin tile，atlas 120→121 零移位）
  - **chicken**（被动）：4 HP / 0.6 / 掉 feather ×1 + chicken ×1
  - **pig**（被动，`[V2-5]`）：10 HP / 0.6 / 掉 pork **1–2**（随机区间，首个
    min>0 区间 carrier）+ leather **0–1** / 4 腿 0.4 rad 对角步态 / head front
    = 首个 V2-2 per-face `faces` 消费 mob（显式 8×8 猪脸 2× tile）
  - **bat**（被动 ambient，`[V2-14]` v2.15.0，**项目首只飞行 mob**）：6 HP /
    speed 1.0（巡航速）/ 探测全 0 / `spawn_max_light` **3**（洞穴语义 = MC
    精确：夜底光 7 > 3 露天不可生）/ 无 drops / xp 0 / **三新严格字段**
    `flying` + `spawns_in_air` + `ambient_sound: "squeak"` + `ambient_interval:
    3.0`（唯一硬互配 flying⇒spawns_in_air，孤 interval 无音名 = 死数据点名
    拒，音名 init file-based 交叉检）/ model **0.9×0.5**（记档偏离：MC 盒
    0.5×0.9 竖薄容不下展翼 AABB）身/头/**双翼 4 box**（±0.275 = 0.9 极限）+
    swings 双翼 **0.9 rad 反相扇翅**（chicken 翅盒不摆先例的反例，巡航恒水平
    位移 → 相位恒动）；**悬停巡航 AI** = `runAi` 顶部 `def.flying` **纯新分支**
    （既有 7 mob 族零改动 = 位级红线）：航向 per-mob 流每 `wander_interval`
    **2 s** 重掷 + 数据 speed 巡航 / **垂直正弦悬停** `0.2×sin(2π·frame/60)`
    帧锁定（bob 初相 = 构造流首抽，flying 才消费）/ integrate 重力关 + 终端
    豁免 + 流体旁路【悬停穿水 = 实现级偏离】/ **撞壁 = 下帧航向重掷**（无寻
    路 = 记档偏离）/ 玩家碰撞保留（MC JE 不交互 = 记档偏离）/ knockback 0.3 s
    回巡航 / **ambient squeak** 每帧 p = 1/(interval×60) per-mob 流 roll 空间
    化（squeak.wav = 900→1400 Hz 滑升 + 30 Hz 微颤 0.12 s，无 ambient 的 mob
    流零消费）；**生成 in-air 分支**（`findMobSpawnCell` `spawnsInAir` 尾参，
    下条）；消散 40（MC 32 = 记档偏离）/ 倒挂不做（记档）/ takeoff 音不做
    （记档）；入被动池 8 词表（记档偏离：MC ambient 独立 cap）
  - **zombified_piglin**（敌对，`[V3-3]` v3.8.0）：20 HP / 0.8 / 探测 16 /
    近战 1.6 / 伤害 3 / 冷却 1.0 / 掉 rotten_flesh ×1 / **zombie 数据孪生但
    `undead: false`**（MC zombie piglin 非亡灵 = 中毒不免疫，记档）/ 下界
    池 only
  - **piglin**（neutral 新类，`[V3-3]`）：16 HP / 1.0 / 无掉落 / **neutral =
    生成即被动不追击，受击才激怒**——`damage()` **单收口钩子任意来源**
    （玩家/箭/火 tick/摔落/毒，MC piglin 偏离 7 同款）+ **30 s real-dt
    脱激**（再击刷新；玩家死亡即时脱激 = 重生不见 29 s 旧怨；致死不置
    态）/ ambient piglin_whisper 3.0 s / **无交易**（记档不做）
  - **hoglin**（neutral，`[V3-3]`）：40 HP / 1.0 / 掉 **hoglin_meat
    1–2 + leather 0–1**（食物链，见「生存系统」）/ ambient hoglin_grunt
    3.0 s
  - **magma_cube**（敌对，`[V3-3]`）：16 HP / 0.8 / 无掉落 / **不分裂不跳**
    blob 3 盒无脸（MC magma 跳跃/分裂 = 记档偏离）/ `spawn_max_light` 7
  - **strider**（passive + **`walks_on_lava`**，`[V3-3]`，**熔岩面行走
    者**）：20 HP / 1.0 / 掉 leather 0–1 / **点火豁免**（inLava 不点燃，
    **火块接触仍点燃** = MC strider 踩火也烧）/ **feet-on-plane snap
    不动点**（fps 无关；深潜浮回；落体免溅落）+ 水下 0.6× 速度门同旗
    豁免（strider 在水中仍泳档 = 下界水蒸发实践不可达，记档边界）/
    **生成面 = 熔岩源格顶面**（顶向下扫 + 头顶 air + 下方异种流体拒
    【海柱收形】）/ **`spawn_max_light` 15**（本引擎熔岩自发光 15，7 门
    会自毁 strider 永不生成 = 门 198 内实证；15 = 熔岩面独占位，其余 4
    种留 7 台地表 0–3 暗区不受影响）/ ambient strider_hiss 3.0 s /
    **不可骑乘**（记档不做）
- **生成**（`data/mob_spawning.json` + 帧节奏驱动，纯函数 = 同一运行必生成
  同一只）：每 `check_every_frames` **30** 帧尝试一次（难度节奏缩放：easy 60 /
  normal 30 / hard 20，见「难度」）、`first_spawn_frame` **60** 之前不生成、
  `max_mobs` **4** 上限、距玩家 > `despawn_distance` **40** 消失（**距离
  despawn 不算死** `[SURV-7]`）；生成格 = 玩家列周围 `[min_radius, max_radius]`
  = **[4,6]** 环（`[PITS 307]` 实证有效 5–6——旧文档「环 8–16」= 保护圈
  8/12/16 混淆误记）内按**固定扫描序**（dz/dx 嵌套）第一个合法格：① 保护环
  （敌对 only，见「难度」）② **floor band** = 玩家脚 ±**2** 格内最高 solid 格
  （远悬崖/山谷出局）③ **headroom** = 地板顶上方 ceil(height) 格（zombie 等 =
  2 格）全 air **且无流体**（水/熔岩里不生成）④ **光照门**（下条）⑤ 不与玩家
  AABB 重叠（严格不等式）⑥ 与其他 mob 水平间隔 ≥1；类型在每次尝试时按数据
  名单**确定性抽取**（种子 ⊕ 帧号，1 次流值映射 [0, Σweight) 后按累计权重走
  名单序——`[P39]` `mobs` 条目 = `{"id", "weight"?}`，weight 1..10000 缺省
  1，全 1 与旧 `pick % 长度` 逐位相同；shipped 主世界池 8 种全 weight 1【`[V2-14]`
  +bat】；**下界池 `[V3-3]` = `mob_spawning_nether.json` 独立文件**（
  zombified_piglin **10** / piglin **8** / hoglin **7** / magma_cube
  **5** / strider **1** = MC 近似权重；cadence/半径/despawn 参数与主
  世界同款表，会话策略（enabled/difficulty/limb_swing）继承主世界
  config = 记档偏离 5；类型表 = 游戏单一共享注册表 = 记档偏离 4；
  文件缺失/坏 schema/未知 mob 全 init 点名 fail-fast）；**池隔离**
  （`playerInNether() ? m_nether.mobs : m_mobs` 既有维度分派——主世界
  生成永不抽下界物种反之亦然，门 198 S2(b) 实证））；**抽中的类型
  过不了光门则该次尝试作废**（如白天抽中 zombie，v1 接受）；**`[V2-14]`
  in-air 分支**（`spawns_in_air` mob = bat，`findMobSpawnCell` 尾参默认
  false）：跳过 ②floor band/③headroom，**顶向扫描**（世界顶向下）box 全
  空气无流体 + 光 ≤ `spawn_max_light`（≤3 = 洞穴）**任意 Y**（MC 同款）+
  玩家重叠/间距既有 → 出生位 = **盒底格中心** = 洞穴悬停生成
- **光照门（昼夜生成规则，`[P20.5]`→`[LGT-3]`）**：生成格（脚位）的**当前
  有效光照** = max(天空光昼夜有效等级 `effectiveSkyLight(stored, df, L) = max(0,
  stored − (15−S))`，S = L + (15−L)×df【与渲染同款仿射撤退 + 月光地板：正午
  恒等，深夜开阔地 15 → **L 非 0**（`[UX-1]` 默认 L = 7）】，方块光【萤石，不
  受昼夜影响】)，必须 ≤ 该 mob 的
  `spawn_max_light`（数据字段 0..15；**shipped 敌对 trio zombie/skeleton/
  spider = 7** = MC 月光配对【`[LGT-3]` 起 0 → 7，与 `[UX-1]` 默认 L 7 配
  对】，被动 = 15 任何光照/时间可生成，**bat = 3**（`[V2-14]` 洞穴语义：
  夜底光 7 > 3 露天不可生））。效果：zombie 白天开阔地（有效 15 >
  7）不生成、深夜生成（有效 **L** ≤ 7 ⇔ L ≤ 7）、火把圈（块光 14 > 7）不生
  成 = 玩法闭环；**spawn 成对语义任意 L 成立**（夜露天等级 = L 本身：L ≤ 7
  敌对照生 / 火把圈 14 恒免疫 / L = 15 夜不生成 = 用户配置选择，`[UX-1]`）；
  萤石照亮处深夜也不生成敌对；
  **树冠/洞穴永久暗格（存储天空光 0）白天也可生成敌对**（MC 一致——洞内 24/7
  敌对生成，出生点保护圈不变）；**熔岩格 ~15 块内 hostile 永不生成**（块光 15
  对光门，`[P58]` 记档）
- **AI 状态机**（确定性：种子 = 世界种子 ⊕ 生成序号，splitmix64 流；帧步
  `kMobStepPerFrame` 1/60）：**Wander**（默认；每 `wander_interval` 秒一次种子
  随机决策：1/3 站定、其余半速随机方向）→ **Chase**（玩家进入 `detect_radius`
  （水平 16）且高度差 ≤2：全速追）→ **Attack**（水平 ≤`attack_range` 1.6 且
  高度差 ≤1：停下、面向玩家、每 `attack_cooldown` 秒打 `attack_damage` 点
  （难度缩放）——走玩家受伤路径：红闪/音效/死亡全由既有轮询驱动）；玩家死亡
  时不追不打。**RangedAttack**（`shoot_range > 0` = skeleton）：玩家进入
  `shoot_range` 14（水平）→ 面向玩家、每 `attack_cooldown` 1.5 s 经 shoot
  回调放箭——**先过 LOS 门**（`[P38]`：眼→玩家眼被非空气格挡住则不发【箭反正
  只会插墙消失】，冷却照常消耗、下个间隔重试）；瞄点 = `computeAimDirection`
  （`[P38]`：目标位置 + 速度×飞行时间**迭代 2 次预判** + 0.5·g·t² 弧线提前量
  ——静止玩家与 `[P27]` 瞄法逐位相同，移动玩家被预判）；玩家贴近
  `retreat_distance` 3 内 → 全速后撤（边退边射，MC kite）；超过 shoot_range
  但仍在 detect_radius 内 → 按 Chase 逼近。被动 mob = detect/attack 三字段
  0（校验 ≥0）→ 追/击条件永不成立，只有 Wander（纯数据表达，AI 代码无特判）。
  **`[V3-3]` neutral 类**（piglin/hoglin，mob JSON 严格 bool 旗，与 hostile
  互斥 init 点名拒）：chase 谓词**单式第二支** `(neutral ? m_angry : true)`
  （非 neutral = 常量 true 位级恒等，无物种 switch）；激怒 = 任意来源
  `damage()` 单收口钩子；脱激 = 30 s real-dt 倒计时（硬规则 8 生存计时
  器类）+ 再击刷新 + 玩家死亡即时脱激 + 致死不置态；激怒中行为 = 敌对
  同款（全速追 + 近战），脱激后 = Wander。**`[V3-3]` 地面 ambient**
  （piglin_whisper/hoglin_grunt/strider_hiss 3 件）：`runAi` 数据门控新
  分支（mob JSON `ambient_sound` + `ambient_interval` 严格配对——孤
  interval 无音名 = 死数据点名拒，`[V2-14]` 纪律）= 每帧 p =
  1/(interval×60) per-mob 流 roll 空间化（bat 悬停巡航 ambient 先例的
  地面推广；**无 ambient 既有 mob 零流抽取 = 位级根**）。
  **`[V2-14]` 悬停巡航（flying 纯新分支）**：`runAi` 顶部 `def.flying` 门
  （bat）——航向 per-mob 流每 2 s 重掷 + 数据 speed 巡航 + 垂直正弦悬停帧锁
  定 + 撞壁 = 下帧航向重掷（无寻路记档）+ knockback 0.3 s 后回巡航；详见
  上 bat 条目。**`[ENT-6]` 追逐寻路**（chase 态专属；wander/attack/ranged 转向不寻路 =
  记档偏离）：**卡住触发** = chase 窗 **0.5 s**（`kStuckWindowSeconds`）
  progress < 期望 **25%**（`kStuckProgressFactor`）→ `pathfindBFS` 纯函数
  （`cellStandable` = feet+头空 air + 下方 solid FullCube 整数地板；固定邻居序
  flat/step-up/fall(1..8) 三族边；target zone Chebyshev ≤1；展开帽
  `kPathMaxNodes` **512**；无随机无 float 帧锁定安全）跟随 + **0.5 s**
  重规划（`kRepathSeconds`）+ **1 s** lost 冷却（`kLostCooldownSeconds`）+
  **2 s 节点超时逃逸**（`kNodeTimeoutSeconds`：模型可错——共享 step-up 的
  +X/+Z 单格 quirk 使计划步不可行 → 弃路径回直线追逐，节流；quirk 几何行为
  与 ENT-6 前逐位一致）；未卡住 = 结构惰位级恒等。无 mob 间仇恨（只以玩家
  为目标）；被动 mob 无主动躲避（被打只有 0.3 s 击退窗口，无逃跑速度）
- **四肢摆动 `[ENT-1]`/`[ENT-1b]`**（**纯渲染层**，per-mob 整数 swing 相位
  **从不入档** = mobs ephemeral）：帧边界**水平位移严格不等式**判移动 → 移动
  帧相位 +`kSwingStep` **2**（整周期 32 帧 = 0.533 s）/ 静止帧
  −`kSwingDecay` **4**（16 帧收步到 0）；**constexpr 16.16 定点正弦表**（64
  相位，编译期 Taylor，零运行期 libm）；mob JSON `swings` 数据段（part box
  名 / amplitude (0,1] 弧度 / group 0/1 半周相位组——三级未知键拒绝 + part
  存在性解析期交叉校验）；CPU 逐帧部件顶点变换（pivot = box 顶面中心、局部 X
  轴、法线同旋、**phase 0 位级恒等 = 静态 fast path** 结构性免疫）+ per-mob
  动态 VBO；**`[ENT-1b]`**（用户实机反馈修复）：四足 body 长 > 高（cow
  0.75×0.55×1.05 = 1.9:1 / sheep 1.73:1 / chicken 1.43:1 = MC 比例带，纯
  JSON）+ **关节出体**（全部 swing 腿 pivot = 身体底缘）+ 摆幅下调（cow/sheep
  0.5 / chicken 0.4 / spider 0.45 rad）+ spider 腿 0.2 长 0.08 粗外移 + **穿模
  谓词 `checkSwingClip`**（旋转腿∩body Sutherland-Hodgman 凸裁剪相交体积 ≤
  腿体积 **25%** 且底部 2/3 段 ≤1e-6 + 脚角插地 sliver 0.02 = 运动学必然 dip
  容差，零运行期 libm）——4 mob × 14 部件 × 64 相位 = 896 次验收单测 + 3
  反例；内容 = chicken 2 腿 0.4 半周相位 / cow·sheep 4 腿 0.5 对角步态
  （FL+RR·FR+RL = MC 四足）/ spider 8 腿 0.45 偶奇分组、翅不摆（MC 只飞行时
  扇翅）；zombie/skeleton 人形 = 头+躯干两 box（`[99.7]` 几何）无独立 arm
  part = 不造几何（记档）；config `mobs.limb_swing`（代码默认 true；off =
  静态对照）
- **mob 渲染**：数据驱动 box 模型（per-part pos/size/texture + `swings`；
  `[99.7]` 四肢按原版几何重做；`[ENT-1b]` 比例修正）；与掉落物同 pass
  （`translate(位置) × rotateY(朝向)`，−Z 面是脸）；天空光全亮烘焙随昼夜变暗
  （LGT-3 仿射撤退 + 月光地板）；**燃烧视觉 `[PRE-8]`**：每
  `kMobFlameEveryNFrames` **36 帧**每燃烧 mob（`MobEntity::burning` =
  `[P57]`/`[P73]` 火系统状态）一批上升余烬（`[P66]` `spawnFire` 复用，专用盐
  `kMobFlameRollSalt`）+ **自发光**（block 光 1.0 深夜也亮 = MC 观感 + 中心格
  天光环境项；中心格 = `entityCenterCell` 纯函数——AABB 中心 position+
  height/2 逐轴 floor 负坐标安全，从 `[PRE-6]` `itemDropLight` 提取共享）；
  `render.mob_flames` 代码默认 true，off = 零发射位级恒等；**blob 阴影
  `[V2-6]`（v2.6.0）**：每 mob 脚下世界空间扁 quad（**玩家仅第三人称** =
  第一人称无自身阴影，MC 观感）——中心 = 实体底心正下方**底格顶面**
  （`blobShadowCenter` = floor(footY) + `kMobShadowLift` **0.01** 防 z-fight
  抬升，贴地面非悬浮脚边）+ 边长 = **1.0×模型宽**（`blobShadowSide`：猪
  0.6 / 玩家 1.0 / chicken 0.45——`[V2-6]` 原 1.5× MC blob 观感于 v3.12.0
  缩至 1.0 = 实机报障「阴影压边看似托住 mob」视觉误导；**物理经探针定案
  = AABB 质心规则**（失接地 = 身体中心恰过边缘 0.3000 = 半宽，MC 同
  款），阴影零参与判定）+ 双面半透明黑 **alpha
  0.3**（`kMobShadowAlpha`），UI shader 纯色路径 + blend pass（深度写关测
  开：山丘遮挡、地形承托）；几何 = 实体当前位置**纯函数、无新帧状态成
  员**（帧锁定零状态；mobs-off = 零 quad 结构免疫；方块扁影非 MC 径向渐变
  软影 = 记档偏离）；残余 = 无红闪/无
  音效/无 HP 条（记档）
- **挤奶 `[SURV-10]`**（MC 5 分钟挤奶冷却）：持 `milk_with` 物品（cow =
  bucket）右键准星命中的可挤 mob（近战目标：`kReach` 5 + 方块遮挡）= 消耗 1
  个手持 + 产出 1 个 `milk_gives`（milk_bucket）+ "milk" 音 + mob 不受任何
  影响（无伤害/死亡/掉落）；**per-mob 帧锁定冷却** `kMilkingCooldownSeconds`
  **300 s** × 60 = **18000 帧**（`[P58]` 整数帧纪律；update 每帧 −1）——冷却
  中再挤 = no-op 不耗桶不产奶不发声、**点击照吞**（failed-milk 同款）；背包满
  = 不产出但吞点击；mob ephemeral = **冷却从不入档**（MC 同款：读档后 mob
  重生）
- **死亡与掉落**：HP 归零 → 掉落 `drops` 数据定义的**物品列表**（`[P28]` 每
  条目 `{item, count}`；`[P38]` 或 `{item, count_min, count_max}` 随机区间——
  死亡时按列表顺序用该 mob 的种子流掷一次，0 = 该条目不掉；固定 count 不
  推进流）→ 按列表顺序各掉一件 ItemEntity 后移除；**`[SURV-7]` kill 统计**：
  despawn pass 每 dead() mob（含无掉落）`kill.<name>` +1（距离 despawn 不算
  死）；**燃烧 mob 死亡同款**（掉落照走，`[P73]` 火膜场景 5 mob【2 cow/1
  chicken/2 sheep】烧死 = 12 掉落实证）
- **mob 不入存档**（与 Minecraft 一致：生成物是瞬态的，其掉落物是物品、随
  存档保存）；`mobs.enabled` 代码默认 true（config 可关）

## 生存系统

- **生命值**：`max_health` 代码默认 **20**（10 颗心，每颗 2 HP，config 可覆盖）；
  受伤单收口 `Player::damage(bypassArmor=false)`（次序见「护甲」）；**受伤
  反馈** = 受击瞬间屏幕红色闪光（0.35×剩余/0.5 s 渐隐）+ "hurt" 音（游戏
  轮询 `m_prevHealth` 驱动，致死伤不重复播放）；死亡播 "death"
- **死亡与重生**：HP 归零 → 冻结玩家 + 显示**「你死了！/ 按 R 重生」**
  （`[PRE-1]` 中文化，红字）+ 流式加载暂停（`[P33]`，见 MECHANICS_WORLD.md「世界与流送」）；
  `R` 重生 = 回**重生点**（床验床回退世界出生点 `[V2-13]`，见下条）+ 满血 +
  **满饥饿 + 满饱和度**（= 一顿饱饭）+ 速度清零
  （**背包保留** = keep-inventory 记档，无死亡掉落）
- **床与重生点 `[V2-13]`（v2.14.0）**：**15 色染色族**（`dye_base: bed` +
  `dye_color` wool 同款 15 色词表无 lime、white = base；RMB 持染料染色 =
  既有路径免费）+ **Slab 半格形**（记档偏离 1：MC 1×2 两格无项目先例）+
  hardness **0.2**（MC 精确）+ cloth 音族 + 配方 15（shaped 3 同色 wool +
  3 planks = MC 精确）+ 确定性纹理 15 tile（atlas 104→119 零既有移位）+ 新
  sleep.wav。**睡眠三门**（`core/bed` 纯函数 + game 薄适配，RMB 链纯插入：
  面板分支后、染色前；命中块 `dye_base=="bed"` 族判 + 持染料落回染色路径）
  定案序：① **b 距离** ≤ **3.0**（玩家脚心 vs 床格中心，MC 精确，平方比较
  含边界）② **c hostile 盒** = 床格中心水平 **±8** / 垂直 **+5/−1** 闭盒内
  无 hostile（数据旗判据无物种 switch，被动/掉落物/箭豁免）→ ①②过 = **设
  重生点**（MC：RMB 恒设点，夜或日）③ **a 夜窗** t ∈ [kDuskEnd **0.55**,
  1)∪[0, kDawnStart **0.05**)（记档偏离 3：MC sky≤11 = [0.586, 0.045] 略
  窄）：**夜内 = 即时跳夜**（记档偏离 2：MC 101-tick 入睡动画 + 受击唤醒不
  做）——`setTimeOfDay(kDawnStart)` = 06:00 黎明 ramp 平台 + 雨/雷天气
  `weatherInitialState` 纯重置 CLEAR（MC"睡觉清雨"同款、晴天不动）+ "sleep"
  音空间化于床 + "sleep" 教程词入裸词表；**夜外 = 仅设点**（MC 白天 RMB
  语义）；**门 b/c 拒 = 全拒含设点**（记档偏离 6）；mob 炸床不做（记档偏离
  5：项目无爆炸机制，门 c 覆盖主语义）。**重生流**：`Player::spawnPos` 访问
  器 + **format 28** `player.respawn` int 三元组**恒写**（新档 = 世界出生点
  init 写入；27 携键拒 / 28 缺键拒 / 27 载 = 哨兵 → applySave 覆写世界出生
  点；分量非整数 / y∉[0,128) 拒）+ **R 键收口 `respawnPlayer` 验床**（纯函数
  `bedRespawnValid`：该位任意床族块 + 房间空间【床格支撑豁免】→ 床位，否则
  回退世界出生点 + 重新钉位 = 床破坏/堵塞永久回退，MC"仅 respawn 时查"同
  款）
- **自然回复**（`[P16]`/`[P23]`/`[SURV-9]`，real-dt 生存计时器）：脱离受伤
  状态 `regen_delay_seconds` **10 s** 后，每 `regen_interval_seconds` **4 s**
  回 1 HP（与 `[P46]` regeneration 状态效果**独立并存**，各自计时）；**饥饿
  门控**（`regen_hunger_threshold` **6** = MC 规则：饥饿 < 6 无自然回复）——
  门控关闭时累加器暂停（不重置），吃饱后继续；**`[SURV-9]` 回复成本**：每
  半心回复**消耗 1 点饥饿**（MC 规则）——经标准路径（隐藏饱和度 buffer 先扣、
  耗尽掉条 = 衰减 tick 同款原语 `drainOneHungerPoint`；**满血时区间免费
  流逝** = 无回复无成本）
- **饥饿**（`[P23]`/`[P31]`）：`max_hunger` **20**（10 只鸡腿每只 2 点）；每
  `hunger_decay_seconds` **36 s** 一个衰减 tick（MC 被动节奏：1 点 = 720 tick
  = 36 s）——**先扣 1 点隐藏饱和度**（`[P28]`，缓冲耗尽才动饥饿条；饱和度
  上限 = `max_hunger`），饱和度 = 0 时才衰减 1 点饥饿条；**疾跑**
  （`sprint_hunger_decay_factor` 2：LeftShift + 移动、非水中——与选
  sprintSpeed 判定同一条件）时 tick 速率 ×2（36 s → 18 s，MC 相对节奏）；
  **饥饿 = 0** 时每 `starve_damage_interval_seconds` **20 s** 受 1 点饥饿伤害
  （MC normal：1 HP = 400 tick = 20 s，走正常伤害路径 → 红闪/音效/死亡全套
  复用）；MC 的跳跃/攻击额外耗饥（exhaustion 子点）未建模 = 记档偏离
- **饱和度 `[SURV-9]`**：逐食物 `saturation` 数据字段（严格 number [0,20]、
  配对纪律 = 须有 `restore_hunger`；**缺省 = 旧 2× 规则位级复现**
  `foodSatiety` 纯函数）——shipped 14 食物 **MC 精确值全写**：rotten_flesh
  0.8 / beef 1.8 / mutton 1.2 / chicken 1.2 / pork 1.8（`[V2-5]`）/
  hoglin_meat 1.8（`[V3-3]`，生肉低饱和度即玩法点）/ cooked_beef 12.8 /
  cooked_mutton 7.2 / cooked_chicken 7.2 / cooked_pork 12.8（`[V2-5]`，熟牛
  肉同款刻度）/ cooked_hoglin_meat 12.8（`[V3-3]`）/ bread 6.0 /
  apple 2.4 / golden_apple 9.0
- **进食**（`[P28]` `core/food.hpp` 纯状态机，real-dt）：选中食物时**按住右键
  1.6 s**（`kEatDurationSeconds` = MC 32 tick）= 吃——完成时回复该物品
  `restore_hunger` 数据点饥饿 + **satiety**（`[SURV-9]`）+ `grants_effects`
  （金苹果 = 再生 10 s + 吸收 5 s `[P56]`）+ 扣 1 个库存 + 播放 "eat"；**提前
  松开右键 = 取消**（不消耗、无声音）；受击/死亡/解锁鼠标/开面板/切换选中格/
  左键挥击同样取消；进食中 3D 手部随进度抬向面部（`[P42]` 3D 手持）；进食中
  再按右键 = 忽略（进食继续）；不再放置/交互；**满饥饿时右键食物为 no-op 但仍
  吞掉该次右键**（MC：满条时不能放置）；浮点边界：完成帧落第 97 帧（1/60
  双累加约定，KNOWN_ISSUES）
- **食物数据**（`restore_hunger` 整数 [1,20]，缺省 = 非食物，显式 0 拒绝）：
  14 种 = 4 生肉/腐肉（rotten_flesh 4【zombie/zombified_piglin】/ beef 3
  【cow】/ mutton 2【sheep】/ chicken 2【chicken】）+ 3 熟肉（cooked_beef 8
  / cooked_mutton 6 / cooked_chicken 6，炉子熔炼对应生肉）+ **pork 3 /
  cooked_pork 8 `[V2-5]`**（杀猪→熔炼 MC 同款，无新配方）+ **hoglin_meat 4
  / cooked_hoglin_meat 8 `[V3-3]`**（hoglin 掉 1–2 → 熔炼，MC 同款）+ bread 5
  （shapeless 3 wheat `[P48]`）+ apple 4（leaves 1/128 概率掉 `[P49]`）+
  golden_apple 4（shaped 镶金 `[P49]` + grants_effects `[P56]`）；
  leather/wool/feather 刻意不是食物（MC 收集品定位）；**食物不直接回
  血**（HP 只走门控后的自然回复与饥饿 = 0 扣血）
- **XP 系统 `[V2-11]`（v2.12.0）**：玩家携**单个整数池** `player.xp`
  （format 26 恒写）= 累计经验总点数，**等级永不存储** = 由池派生（单源真
  值，存储级 = 第二份可漂移拷贝）。**等级曲线全整数**（`core/xp_curve`，MC
  现代公式核定：0→1 = 7、0→30 = 1395、段界连续）：`xpNeededForLevel(L)` =
  L≤15: **2L+7** / 16≤L≤30: **5L−38** / L≥31: **9L−158**（恒 ≥7 = HUD 除法
  不除零）；`totalXpForLevel(L)` = L≤16: **L²+6L** / 17≤L≤31: **(5L²−81L+
  720)/2** / L≥32: **(9L²−325L+4440)/2**；`levelFromTotalXp` = **整数二
  分**（往返构造性恒等，无浮点）。**经验球**（`entity/exp_orb`）：值
  **1..9**、**无合并无分裂**（记档偏离：MC 同值球合并）+ ItemEntity 同款物
  理纪律（帧锁定重力/碰撞 + pickup delay 0.5 s real-dt 双时钟）+ V2-6 磁力
  同款帧锁定拉取（半径 2.5）+ **1.0 内 pickup pass** 收 → 池 + "pickup" 音
  色。**三发放源**（数据驱动严格解析 [0,9] 越界点名拒，核心不 switch 内容
  id）：① **mob 击杀** = mob JSON `xp`（hostile zombie/skeleton/spider
  **5** / passive pig/cow/sheep/chicken **2** = MC 1–3 中值记档 / bat **0**；
  **下界 `[V3-3]`**：zombified_piglin 5 / hoglin 5 / piglin 3 / strider 3 /
  magma_cube 0）
  ② **熔炼完成** = item `smelt_xp`（**默认 1 统一** = 记档偏离：MC 分数
  表；0 = 无球数据门）③ **挖掘完成** = block `mine_xp`（coal_ore **1** /
  diamond_ore **5** / iron_ore **0** / gold_ore **0** = 现代 MC 1.17 raw 化
  值记档）；球生成 = 掉落点（破坏块中心 / mob 死亡位 / 炉位）。**HUD** = 热
  栏正下方 XP 条 + 等级数（填充 (total − T(L))/needed(L) 全整数数学、**恒可
  见** = 门 183 唯一像素源、零新字形）
- **护甲 `[SURV-1]`**：**16 件**（leather/iron/diamond × helmet/chestplate/
  leggings/boots + **netherite×4 `[V3-4]`**，max_stack 1，材质档四链；**MC
  精确值** = 1/3/2/1 总 **7** / 2/6/5/1 总 **14** / 3/8/6/3 总 **20** /
  **netherite 3/8/6/3 总 20**（= diamond 点值同，耐久 ×1.5）；chain 无材料 =
  记档偏离）+
  **MC 整数 floor 减伤** `floor(amount × (100 − 4×pts))/100`——`Player::
  damage` 单收口，**全伤害源生效**（坠落/饥饿/熔岩/虚空/mob/箭/火 tick），
  次序 **armor → resistance 效果 → 吸收池**（pts 0 位级恒等）；**右键穿戴/
  交换/脱下通道**（点火器之后、潜行旋转之前；手持件 = 穿戴或交换占位槽，空手
  = 按 head→chest→legs→feet 固定序脱下到选中槽——脱下到选中槽 → 下点 = 重穿，
  连点 = 穿/脱震荡，脱整套须每点之间换空热栏槽【热栏只有 Tab 循环 + 数字键】；
  逻辑上提纯函数 `item/armor`（campfire 范式）+ 9 单测含回归）；**豁免规则**
  （`bypassArmor=true` 源：溺水 `[SURV-2]` / 药水瞬时伤害 `[SURV-4]`——MC
  伤害调整表：Drowning/Potion = Armor No effect，Resistance Applies；
  resistance 与吸收池照常）；**format 16** `player.armor`（恒写、只含占位槽、
  键纪律 + 槽名-物品数据双向交叉校验点名）；**HUD 护甲行**（心行上方行 =
  hy−44 `[PRE-10]`，仅占位槽且指针捕获时绘制 = 无甲零绘制结构性免疫）；
  **附魔 `[V2-12]`** = 保护 +4L 护甲点（armor pts 步 `min(20, armorPts +
  4·min(20,ΣL))` 共享 cap，见「合成与容器 · 附魔台」）；**修复 = `[UX-5]` 工
  作台材料修复**（见「合成与容器
  · 工作台」）
- **氧气窒息 `[SURV-2]`**（帧锁定）：**150 空气**（`kOxygenMax`）；**眼位单格
  流体判定**（`headInFluid` 纯函数：眼格 = `eyePosition()` 含 crouch 缩减；
  水/熔岩同款 = MC 眼在水里规则——站 1 格浅池/浮漂面眼上水面 = 换气）头没入
  每 **6 帧**（`kOxygenDecayFrames`）−1（= 15.0 s 耗尽）；耗尽后每 **150 帧**
  （`kDrownDamageFrames`）= 2.5 s 受 **2.0** 伤（`kDrownDamage`，时间线 894
  耗尽 / 1044 首伤钉死）；出水即满；**溺水豁免护甲**（`damage(2.0,
  bypassArmor=true)`，resistance 与吸收池照常 = MC 同款）；面板暂停期间照常
  tick；死亡/重生复位满空气；**format 17** `player.oxygen`（恒写、键纪律、
  [0,150]；drownTicks 不存 = tick 相位惯例，MC 存 airSupply 相位 = 记档偏离）；
  **HUD 气泡排替换饥饿排原位**（`locked && !panelOpen && oxygen < 150`；10 泡
  = ceil(oxygen/15)、单 tile 无半态、出水同帧消失；oxygen.png）；F3 `oxygen
  N` 行；范围外（记档）：无气泡/喝水声（MC 气泡声随机 = 非确定性）/ 无 mob
  溺水 / 无呼吸附魔 / 无 config 键（MC 值 = `player/drown` 命名常量，YAGNI）
- **耐久 `[SURV-3]`**：`durability` 严格字段（允许集 = Tool ∪ armor_slot ∪
  ammo 发射器 ∪ ignites_flamable，其它物品带字段 = 解析拒绝点名）+ **36 件
  MC 精确值**（wood 59 / stone 131 / iron 250 / **netherite 2031 `[V3-4]`
  MC 精确** 镐斧锹三材质；弓族 6 件 384【bow/infinite/power/speed/
  master/poison】；燧石与钢 64；shears 238 `[V2-15]`；护甲 leather
  55/80/75/65 · iron 165/240/225/195 · diamond 363/528/495/429 =
  11:16:15:13 · **netherite 545/792/743/644 = diamond ×1.5 `[V3-4]`**——
  `[SURV-3]` 首批 27 件 + `[SURV-5]` poison_bow + `[V2-15]` shears +
  `[V3-4]` netherite 7 件）+ **`ItemStack.damage`
  float**（堆随移；**合并门 = 同物品且同 damage** = MC NBT 感知合并简化记档）
  + 纯函数 `item/durability`（campfire 范式）+ **五类帧锁定磨损事件**：① 破
  非瞬断块 +1（hardness == 0 = 0 扣）② 镐/斧/锹打 mob +2 ③ 弓射出 +1（含
  infinite）④ 点火成功 +1（含再点燃）⑤ **护甲 = 每次实际减伤事件每件在穿
  −max(1, incoming/4)**（incoming = 减伤前原始量，10 → 2.5 float 精确、1 → 1
  下限；破碎 = 槽清 + points 收口同步、不影响本次动作）——**豁免互斥**：
  bypassArmor 源（溺水）不磨损 = MC 同款；本项目饥饿/火 tick/虚空/效果 tick
  也磨损 = 记档偏离（MC 不磨损，"磨损 = 减伤生效"自洽规则）；**format 18**
  （damage > 0 才写：inventory slots / armor【占位槽翻 {item,damage} 对象形，
  满耐久保字符串形 = fmt17 逐字节同形】/ entities 三处；≤17 带键 = 版本谎报
  点名；交叉校验 damage < durability + 无 durability 物品带 damage 拒）+
  **HUD 2px 三色耐久条**（图标底部，填充 1−damage/durability，绿 ≥1/2 / 黄
  ≥1/4 / 红 <1/4，**满耐久零绘制** = 结构性免疫）+ F3 `held name d/max` +
  破碎音复用 break（玩家侧护甲破碎无声 = Player 无 sound 通道记档）；**修复
  `[UX-5]`** = 工作台材料修复（**MC 铁砧语义的 crafting-grid 形式**：受损
  工具/护甲 + 1 材料 → 输出槽 +25% max 耐久，`repair_with` 数据字段 35 件
  shipped 全配 MC 精确材料（`[V3-4]` netherite 7 件 → netherite_ingot；
  shears 无 repair 字段 = 剪毛工具定位），配方恒优先——详见「合成与容器 · 工作台」；E 面板
  2×2 同款；无 XP 成本/成本递增 = 记档偏离；磨刀石/同物合成/Mending = 矩阵外
  记档）；合成网格/炉子/营火
  槽不入 format 18 范围（受损物品入格读档 = 耐久回满 quirk 记档——现役内容
  无「可熔炼且带耐久」物品，仅 2×2/3×3 网格可触达）
- **死亡统计 `[SURV-7]`**（统计系统 v1 = 死亡相关计数器；**显示** = `[PRE-12]`
  F3 通道）：**封闭词表**（`deathCount` = 玩家 alive→dead 过渡任意死因 /
  `kill.<name>` = **mob 注册表合成**——加 mob JSON 自动获得统计，无新数据
  文件）+ **帧锁定计数**（死亡过渡块 + despawn pass 每 dead() mob 含无掉落；
  **距离 despawn 不算死**）+ m_stats map **只存非零**（omit-when-default）+
  **format 21** `player.stats`（恒写，空对象合法 = tables 键纪律；pre-21 带键
  = 版本谎报点名；非整数/负点名；**词表校验 = game 加载点 validateStats 单
  收口**【parseSave 不加 MobRegistry 参，rescan 范式孪生】未知 kill.<name>
  点名）+ **F3 `stats` 行**（`[PRE-12]`：键序 "name N | name N" 连接，空 =
  "stats (none)" 基线行）；死亡统计屏 = `[V3-13]` 已实现（下条）
- **死亡统计屏 `[V3-13]`（v3.15.0）**：HP 归零 → 画面冻结的死亡屏升级为
  **统计屏**——标题「你死了！」+ **致死原因行**（`Player::damage` 的
  `DamageInfo` 重载：仅**致死一击**的来源记录——摔落「死于高处掉落」/
  坠落虚空「死于世界之外」/ 饥饿「饿死」/ 熔岩「死于熔岩」/ 溺水
  「死于落水」/ 箭「被箭射死」/ mob「被 <name> 杀死」/ 效果
  「死于燃烧/死于毒/死于药水」；字符串全字体表内【+1 字形「于」入
  字体 428，扩展区重排格 +1，top-512 红线恒等】）+ **5 行会话统计**
  （采挖/放置/击杀/行走格/存活分钟——采挖·放置·击杀 = 会话级
  `SessionStats`【**不存档** = 记档偏离：MC scoreboard 持久，本项目
  随进程重置】；行走 = Player 水平位移累计【传送阈值守卫】；存活 =
  帧锁定 m_frameCount/60）+「按 R 重生」；固定物理偏移行距（fScale
  仅 font_scale 不含 ui.scale = PITS 397）。**Game Over 屏 = 死亡屏
  本身**（无 hardcore 模式，R 重生既有 = 记档偏离 MC 双屏）。
- **潜行**（`LeftControl`）
- **潜行**（`LeftControl`）：身高 1.8→1.6（脚底不动）、眼高 1.62→1.42、移动
  速度 ×0.3（水中除外）；潜行时**不**触发 1 格自动台阶；其它用途：营火坐姿
  （见「营火」）、楼梯/sapling 旋转键（潜行 + 右键，见「BlockState」）、
  低顶陷阱

## 状态效果

- **效果 = 数据**（`assets/data/effects/*.json` 每效果一个文件，严格解析 =
  注册表纪律：未知键拒绝、名字唯一、**no-op 拒绝**——必须有 tick 或正修饰
  字段或正 alpha tint）：
  - tick 形：`tick_type`（"damage"/"heal"）+ `tick_amount`（(0,10]）+
    `tick_interval`（(0,100]）成对必填；`min_health`（仅 damage：1 = 保持 ≥1
    HP 永不致死【MC 中毒地板】/ 0 = 可致死）；**`min_health_mob`**（`[SURV-5]`：
    0 = mob 可毒死，缺省跟随玩家地板——poison 设 0）
  - 修饰形：`melee_penalty`（[0,100] 近战扣除）/ `melee_bonus`（`[P84]` [0,100]
    加算）/ `damage_reduction`（`[P84]` [0,1) 受伤减免取最高单值）/
    `speed_multiplier`（`[P84]` (1,3] 移动速度乘算）/ `absorb_hp`（`[P56]`
    整数 [1,64] 临时 max HP = 吸收层）
  - 瞬时形：`instant_hp`（`[SURV-4]` 整数非 0 [−32,32]，与 tick 互斥、显式
    0 拒；**施加时结算一次**：正 = `heal` 钳 base max / 负 = `damage(
    bypassArmor=true)` 护甲豁免 = MC 药水伤害语义，resistance 照常）
  - 标志：`negative`（bool；牛奶只清负面）/ `ignites_on_lava`（`[P57]` bool，
    **至多一个**注册效果可带，init 交叉校验）/ `extinguished_by_water`（
    `[P57]`）/ `affects_mobs`（`[SURV-5]`：效果可作用于 mob）/
    `undead_exempt`（`[SURV-5]`：× mob `undead` 组合门 = 不死族免疫——poison
    设 true，zombie/skeleton 设 undead）/ `icon`（effects.png tile 索引，init
    对纹理宽度交叉校验）/ **`tint`**（`[PRE-7]` [r,g,b,a] 全 [0,1] 严格解析，
    缺省全零 = 无纱零绘制）
  - 档位（`[SURV-6]`）：`tier_max`（int [1,3] 缺省 1）+ `tier_steps`（**仅 5
    加性修饰字段可档位**：speed/absorption/strength/weakness/resistance——
    配对/死字段/no-op 三门纪律；**tick/instant 形单档** = 记档偏离）
- **shipped 11 效果**（字段精确值 = JSON）：
  - `poison`：damage 1/**2.5 s**，min_health 1，**min_health_mob 0**，negative，
    **affects_mobs + undead_exempt**，icon 0，**tint [0.63, 0.24, 0.86, 0.35]**
    = MC 中毒紫（RGB 160/60/220）
  - `weakness`：melee_penalty 4，negative，icon 1，tier_max 3（step 4 = 4/8/12
    MC 精确）
  - `regeneration`：heal 1/**4.5 s**，正面，icon 2
  - `absorption`：absorb_hp 4，正面，icon 3，tier_max 3（step 2 = 4/6/8）
  - `fire`：damage 1/**0.2 s**，min_health 0 可致死，**正面**（`negative:
    false` = 牛奶不灭火，MC 语义），`ignites_on_lava` + `extinguished_by_water`
    皆 true，icon 4
  - `speed`：speed_multiplier 1.2 = MC 速度 I，icon 5，tier_max 3（step 0.2 =
    1.2/1.4/1.6）
  - `strength`：melee_bonus 2，icon 6，tier_max 3（step 2 = 2/4/6 项目刻度）
  - `resistance`：damage_reduction 0.2 = MC 抗性 I，icon 7，tier_max 3（step
    0.2 = 0.2/0.4/0.6）
  - `wither`（`[SURV-5]`）：damage 1/**4.0 s**，negative，affects_mobs，**无
    地板双目标可杀** = MC wither I，icon 10
  - `healing`（`[SURV-4]`）：instant_hp **+4**，正面，icon 8
  - `harming`（`[SURV-4]`）：instant_hp **−6**，negative，icon 9
- **来源**：① **mob `on_hit_effect`**（`{"name", "duration"}` 成对必填，
  init 对 EffectRegistry 交叉校验——该 mob **近战命中落定**时施加，远程箭
  不施加 on_hit；shipped：spider = poison 15 s）② **`arrow_effect` 双源
  `[SURV-5]`**（skeleton 箭 = wither 10 s 玩家侧 / poison_bow 箭 = poison 60 s
  mob 侧）③ **熔岩/火块点燃**（`[P57]`/`[P74]`，见 MECHANICS_WORLD.md「火系统」）④ **饮用
  `[SURV-4]`**（`grants_effect(s)` **放开到 drinkable**（原 food-only；
  duration ≥ 0）：6 药水 weakness 90 / swiftness 180 / regeneration 45 /
  poison 45 / healing 0 / harming 0 = MC 时长；`Game::applyPlayerEffect`
  收口 = 替换吃×2/mob 命中 3 既有施加点 + 饮用侧；RESET 语义既有 = 再喝
  重置不叠加；**无 grants 的 drinkable = clear 原行为位级不变**；grants 条目
  duration == 0 ⇔ 效果 instantHp != 0 init 双向点名）⑤ **金苹果 `[P56]`**
  （grants_effects = regeneration 10 s + absorption 5 s，吃时按序施加）⑥
  **信标 `[P83]`/`[P91]`**（范围内 0.5 s 刷新，见「信标」）⑦ **存档注入**
  （E2E 场景范式——含 `[SURV-6]` 档位实例）；**tier grant `[SURV-6]`**：全
  grant 对（grants_effect(s)/item arrow/mob on_hit/mob arrow）可选 `tier`
  **0 基**（范围 init 交叉校验点名，on_hit_effect 同款纪律）——**无档位 in-
  game 消费面**（信标 = 纯名列表 / 药水无酿造 / **档位物品未 shipped**——
  `[UX-8f]` 36 槽后 storage 有 9 空槽但无档位内容）= 记档偏离
- **tick（real-dt，硬规则 8 生存计时器类）**：`Game::updateEffects`（每帧
  `updateSurvival` 后）：`remaining −= dt` ≤0 移除；tick 效果 `accum += dt`、
  **每跨过一个 interval 结算一次**（大 dt 可跨多次）；damage tick 的
  min_health 地板**在游戏侧执行**（`health − amount < min_health` 跳过——
  地板让中毒永不致死玩家；`[SURV-5]` min_health_mob 0 = mob 侧地板放开）；
  heal tick 走 `Player::heal`（clamp base max；**mob Heal = no-op 记档**）；
  毒伤走正常受伤路径（红闪/音效复用 + **紫色纱** `[PRE-7]` 独立通道——纱在
  红纱之下 = 分层）；**同一效果再次施加 = 重置 duration（无叠加，tick 相位
  保留）** = MC 语义
- **双载体 `[SURV-5]`**：MobEntity 持 StatusEffects（count == 0 fast-exit =
  无效果 mob 零成本）+ Player 同款；效果聚合 tier 化（下条）
- **档位聚合 `[SURV-6]`**：`effectTieredValue` 纯函数 = **base + tier × step**
  （**tier 0 = 位级旧值** = 结构免疫根）；5 修饰字段档位化（speed 乘算 /
  absorb_hp 加算 / resistance 取最高 / strength 加算 / weakness 扣算）；
  **format 20**
  （tier omit-when-0 = tier-0 条目与 f19 字节同形 ⇒ 迁移免迁；f19 带键 =
  版本谎报名单；非整数/越界点名）；**F3 档位罗马 `[PRE-12]`**（`effectTier-
  Roman` 纯函数：tier 0 = "I" MC amplifier 惯例 / 1 = "II" / 2 = "III"，越界
  钳制防御）
- **吸收池 `[P56]`→`[P72]` 两分量模型**：玩家 **health（0..base max）+
  absorption 池（0..64，`kAbsorptionMax`）两个独立池**（`[P56]` 时与 health
  共用一个值 = 已修复）；`damage()` **先扣池再扣 health**（池单独不能致死——
  死亡只来自 health）；`heal()`/`setHealth()` 只钳制到 **base max**（heal 永不
  填池——池只由效果设置）；池 = 效果 `absorb_hp` 总和的纯函数**只在同步点**
  （进食施加 / mob 命中 / `updateEffects` 内到期或水熄灭【效果总和变化才
  同步——每帧同步会与伤害扣减相抗】/ 牛奶清 / 死亡清；**读档不同步** = 存值
  权威【池 < 总和合法 = 伤害在同步点间持续过】）；刷新恢复满层（MC 属性重算
  语义）；层到期/清除 = 池随效果总和掉下（health 不动——池不是 HP，无红闪/
  音效/伤害计时器）；**format 12** `player.absorption`（12 必需 / ≤11 带键
  拒绝 / [0,64] / **≤ 存档效果 `absorb_hp` 总和**不变量；**format ≤11 迁移**：
  共享 health 按 base max 拆分【22 → 20+2，不损伤害】）；HUD 吸收红心从**池**
  派生（`ceil(池/2)` 颗、池 0 = 零绘制 = 无输入结构性免疫）
- **火系统（`[P57]` 残留燃烧）**：机制见 MECHANICS_WORLD.md「火系统」节；效果侧 = `fire` 在
  熔岩/火块内**每帧重施加**（duration 重置 + tick 相位保留 = 持续燃烧）、水
  **瞬时灭火先于 tick**（水内零伤害，水胜熔岩）、离源 **8 s 残留** countdown；
  死亡/重生照旧清空（fire 不残留于重生）；存档照旧（读档续烧）
- **死亡**：效果随玩家死亡清空（重生 = 干净状态，MC 一致）
- **存档（format 8 起）**：`player.effects = [{name, remaining, tier?}]`
  **名字序**（remaining = 真实 dt 秒数；**tick 相位不存**——重载后下一 tick 从
  整 interval 起算；tier omit-when-0 `[SURV-6]`；fmt 1–7 文件加载为空；fmt 8
  起**必填**该键 = furnaces 键纪律；名字对 EffectRegistry 交叉校验、重复/非
  注册拒绝）
- **HUD**：效果图标行 = **心行上方第二行 hy−22 `[PRE-10]`**（左锚 = 心行最左
  缘 = 池在时吸收带左缘，MC 布局）：每个活跃效果一个 effects.png 16px tile
  （176×16 = 11 tile：紫骷髅/蓝下箭/绿上箭/红心+白十字【吸收】/橙火焰+黄芯
  【火】/速度/力量/抗性/wither/healing/harming，`tools/generate_effect_icons.py`
  确定性生成；tile 数 = 宽度/16 UV 动态），按施加顺序排列；只在游玩态（锁定 +
  无面板）绘制——无输入运行结构性免疫；**吸收红心**（`[P56]`/`[P72]`）= 正常
  心带**左侧**多出 `ceil(池/2)` 颗红色心（正常心带位置不动 = 无层时逐位相同；
  层 0 = 零绘制）；**毒纱 `[PRE-7]`** = HUD pass 红纱前每**活跃玩家** tinted
  效果一个全屏 quad（`activeEffectTints` 纯函数收集，StatusEffects 确定性
  应用序；**恒定 alpha** = MC 脉动记档偏离；燃烧半 = 边界：fire 无 tint，玩家
  燃烧 = 既有 0.2 s tick 红纱不变，mob 火焰视觉 = `[PRE-8]` 独立项）；**F3**
  `effects poison 12.3s I | ...`（档位罗马 `[PRE-12]`；无效果 = `effects
  (none)`）；**效果图标 hover tooltip = 结构不可行记档**（效果行仅 locked 绘制，
  locked 物理光标固定屏幕中心 = 悬停恒 false；档位/时长呈现 = F3 通道 MC 同款
  信息）

## 弓与箭

- **6 弓**（material，max_stack 1，`ammo: arrow`，attack_damage 1，`[SURV-3]`
  耐久 384）：`bow`（`shoot_speed` **15** / `shoot_damage` **5** = MC 满威力，
  shaped 配方 3 stick + 1 planks C 形）/ `infinite_bow`（`[P77]` 同款 +
  **`infinite: true`**——严格 bool、**要求 `ammo` 非空**【无 ammo 发射器本就
  无限，flag = no-op 拒 = ammo 纪律】；shapeless bow + diamond → 1 = Infinity
  附魔的可合成替身【`[V2-12]` 起真附魔台已实现，变体弓保留替身定位】；图标
  = 弓 + 蓝色钻石微光）/ `power_bow`
  （`[P79]` **`shoot_damage` 8** = 弓的 5 +3；shapeless bow + 2 diamond；图标
  橙红迸裂）/ `speed_bow`（`[P79]` **`shoot_speed` 20** = 弓的 15 +5【弹道更
  远，E2E 像素判别钉死】；shapeless bow + gold_ingot；图标青色速度线）/
  `master_bow`（`[P82]` 力量+速度+无限**三叠加** = 20/8 + infinite——三把强化
  弓的合成顶点，shapeless power_bow + speed_bow + infinite_bow → 1 = MC
  附魔台合成替身【`[V2-12]` 起真附魔台已实现，保留三叠加顶点定位】；**无
  model** = bow 3 元素几何复用）/ `poison_bow`
  （`[SURV-5]` 15/5 + `arrow_effect {poison, 60}`）
- **发射（蓄力）**：选中 `shoot_speed > 0` 的物品（发射器）时**右键 = 拉弓**
  ——按下即开始蓄力（该次右键被吞：不交互工作台/不放置方块；**需有弹药才能起
  拉**【无弹药时该次右键被吞，不能起拉、不放置】；播放 "bow" 弦音 = 拉弓开始
  时响，取消也响过），按住累加拉弓时间（**真实 dt**，封顶 `kBowFullDraw-
  Seconds` **1.6 s**），**松键射出**：`power = clamp(拉弓/1.6, 0.1, 1)`
  （`core/bow.hpp` 纯函数，游戏与单测共享）——速度 = `shoot_speed × power`、
  伤害 = `shoot_damage × power`；满蓄力 = 物品数据满值（bow = 15/5，MC 满
  威力），快速点击 = 最低威力 0.1；箭从眼前 0.3 格沿瞄准方向出发，播放
  "shoot"；蓄力状态在解锁/开面板/换热栏格/重生时清零；**耐久**：射出 +1
  （`[SURV-3]`，含 infinite）；**左键 = 弓近战**：准星命中 mob 时按
  `attack_damage` 1 伤害 + 击退（0.25 s 挥击节奏），**永不挖掘**（弓是武器
  不是工具）
- **弹药**：发射消耗 1 个发射器 `ammo` 数据字段点名的物品（**槽序升序第一个
  持有它的槽位** = hotbar 先于背包，MC 同款全库存取箭）；无弹药不能起拉，拉弓
  中途耗尽则松键取消（无箭不射）；`ammo` 缺席的发射器 = 无限箭（`[P21]` 前
  语义）；字段名 init 时经 ItemRegistry 交叉校验；**Infinity `[P77]`**：
  发射**不消耗**弹药，但仍须持有**至少 1 个** ammo（1 支箭永不被耗尽 = MC
  Infinity 规则）
- **箭物理/插墙/释放/拾取/视觉/效果**：见「实体与 Mob · 箭」（重力 20 / 终端
  38 / 30 s / 插墙 100 s / 卡住格破坏释放 / 底心 1.0 拾取返 ammo / PRE-9 三段
  形 / SURV-5 wither·poison 箭）；**无弓弦模型/拉弓进度条/手部皮肤**（记档
  偏离）

## 流体桶与点燃

- **桶对 = 数据**（`[P47]` 双种类 `[P51]`，三种类 `[FLUID-6]`）：物品可选
  字段 `collects_fluid`（空桶侧：**流体名数组**，一个空桶舀所有命名流体 =
  MC）与 `pours_fluid` + `pours_becomes`（满桶侧：倒 → 伴侣；每种流体一个满
  桶）。**流体种类词汇表在代码中**（`FluidKind`：`"water"`/`"lava"`/`"honey"`
  `[FLUID-6]`，其他名解析拒绝）。满桶由空桶侧按流体推导（`fullBucketFor`，无
  `collects_becomes` 字段）。init 交叉校验：空桶命名的每种流体**恰有 1 个**
  满桶倒回（0 = dead field / 2 = 歧义，都拒绝）+ 满桶伴侣必须收集该流体
  （对称，错配 = init 失败点名）；两侧互斥（一个物品只是一对的一侧），且与
  食物/可饮用/launcher/dye/点火器互斥（右键通道归属纪律）。当前对：`bucket`
  （`["water","lava","honey"]`，max_stack 16 = MC 值）⇄ `water_bucket` /
  `lava_bucket` / `honey_bucket`（各 material max_stack 1；**water_bottle**
  是另一 `pours_fluid` 对：倒水回 glass_bottle = 纯容器不可饮 `[SURV-4]`）
- **舀（collect）**：选中空桶侧，**桶目标射线**（`raycastFluid`：DDA 首个「非
  air 方块 **或** 携带流体」的格——与 `raycastBlock` 共享遍历骨架，差异 =
  墙壁**遮挡**【MC：桶用方块目标，不能隔墙舀；普通方块射线穿水而过】）落在
  **level 8（源）** 格：移除该源 + 手持 1 个换成伴侣（满桶）+ "splash" 音效 +
  `Lighting::updateLocal`。**流动水（level < 8）不能舀**（MC 一致；源格每
  tick 向四向补 level 7——远处隔着源头自己的扩散永远舀不到，须站旁 1 格从上
  往下斜视源顶面）→ 落空继续后续（placeBlock 对 material 是 no-op）
- **倒（pour）`[FLUID-4]`/`[FLUID-5]`**（**纯函数 `bucketPourTarget`**
  （`world/raycast`），game 写路径零改动）：候选 = block 射线受击面邻格【
  `[P47]` 放置约定；block 射线穿流体 → 1 深水池地面受击使顶面水格成候选 =
  「对着池子倒」成立】或（block 射线无命中 = 超 block 射程开阔水面）fluid
  射线自身命中的格（**`[FLUID-4]` 倒向水面**，无 cross-fallback）；候选过
  `canPlaceAt` 且：① air 无流体 = 旧放置【位级不变】② **同种流体 level < 8 =
  倒向水面变源**（已源 level 8 = no-op 桶保留 = MC parity）③ **水桶对熔岩格
  = 反应倒 `[FLUID-5]`**（水源放受击法向一步的干空气格【顶视 = 正上方 / 侧
  向 = 开阔侧 / 无 block 命中 = +Y 约定】，既有 reaction pass 下 tick 转
  obsidian【倒出水 = L8 源 → 项目规则恒 obsidian = 单一规则源】；无空气位 =
  no-op 桶保留）④ **熔岩桶对水格 = 替换倒 `[FLUID-5]`**（setFluid 既有
  「替换他种」语义换熔岩源，下 tick 按残留邻水转 obsidian/stone；无邻水 =
  熔岩留存）；蜜桶对水格 = 同款替换倒 `[FLUID-6]`。成功：放 L8 源 + 手持换
  伴侣 + "splash" + `updateLocal`，倒下的源随后走正常 FluidSim；**吞点击
  语义**：舀/倒成功 = 吞点击（桶永不放方块）；背包满（伴侣无空位）= 无操作
  但吞点击（MC 一致）；落空（无源/目标无效）= 不吞继续后续；**范围外**：
  蒸汽（`[P51]` 偏离保留）/ 流动熔岩舀取维持 `[P47]` 不能舀
- **存档**：物品按名序列化（`water_bucket` 等直接可读）、流体格走 `fluids`
  编辑数组（format 9 起带种类名）
- **点燃器（燧石与钢，`[P60]`）**：物品可选字段 `ignites_flamable`（严格
  bool，缺省 false）= 点燃器；与五个右键通道**互斥**（`restore_hunger` /
  `drink_time` / `shoot_speed > 0` / `dye_color` / 桶对字段——通道所有权
  纪律，`[P47]` 范式）；非 bool 点名拒绝；init 白名单。语义：选中点燃器**右键**
  （`wasPressed` 单点击、无通道）：准星目标块（`targetBlock`，reach 5）① 带
  `ignites_to`（非空严格字符串，init 存在性交叉校验 = grows_to 纪律）→ **原位
  setBlock 目标块**（`[P76]` 再点燃：campfire_extinguished → campfire，光 0 →
  15 局部重光照、烤制状态经 setBlock 收口存活并**续烤**）② 否则 `flamable`
  = true → `setBlock` **FIRE**（`[P64]` 起烧成火焰块而非 air）——**无掉落**
  （`[P58]` 熔岩烧语义：烧掉的作物不掉落）、**不消耗**（MC：燧石与钢用不坏）+
  "ignite" 音 + 局部光照；非 flamable 目标/无目标 = 落空不吞（material 的
  placeBlock = no-op）；在 interact **之后**：准星对着炉子/工作台仍开面板
  （MC）。**耐久**：点火成功 +1（`[SURV-3]`）。内容：`flint_and_steel`
  （material 64 叠，图标 = 钢杆 + 燧石尖）+ 配方 shapeless flint + iron_ingot
  （MC 配方）；**确定性**：单点击边缘事件、无计时器、无随机——同存档同点击
  帧 = 同结果

## demo 回放（`[V3-9]`，v3.11.0）

- **定位**：录制游玩 / 编写动作时间线 / 回放只观察——回放 = demo 的输入事
  件流**确定性驱动玩家**，观众操作结构性零触达世界（白名单非黑名单）；
  3.x 矩阵收官项。
- **三 CLI 入口互斥**（同时给 >1 = rc 2 点名）：
  - `--demo <file>` = 回放（**开窗前** headless `loadDemoFile` 版本/schema
    拒收；**world spec = 起点权威**：文件 `world` 段定 fresh seed 或存档
    槽——`--demo` + `--new`/`--seed` 点名拒【记档偏离 4】；`--demo(fresh)`
    + `--save` 允许 = 退出写回放结果；**save-spec 引用槽 init 后 reset
    永不回写**【观察红线：槽 md5 回放前后逐字节不变，门 201 实证】）
  - `--demo-author <t>` = JSON 动作时间线展开为 `.dmo` 即退出（**standalone
    headless** = 存档管理旗孪生，游戏永不启动；输出 = 输入基名 + `.dmo`
    【PITS 391④】；dts = 1/60 定步无墙钟全帧锁定；hold = down@at +
    up@until 展开；**13 面拒收**：键/鼠标/按钮词汇 + 帧序 + part 字段
    全点名）
  - `--demo-record [o]` = 会话录制（输入事件 + 每帧 dts；默认
    `data/demos/rec_<ts>.dmo`，退出时保存【窗口关闭/ESC】；录制 = 现状
    pump + `Input::setEventTap` 边界**先记后注**——普通运行 tap 恒 null
    = 每事件一个判空分支位级根，tapped/plain 双 Input 同 10 事件流逐事件
    快照恒等单测钉；**quit 帧未模拟尾部排除** `discardPendingFrame`
    记档偏离 3）
- **demo 文件 = V3-7 确定性 gzip 容器**（10B 头全钉 + raw deflate 6 +
  CRC32/ISIZE 尾）+ **严格 schema（v3.13.0 双 schema = 独立 sim 版本）**：
  现行文件 = `sim`（录制时 `kDemoSimVersion` = **回放准入唯一判定轴**，
  独立于 TM_VERSION——**bump 纪律：仅帧锁定模拟语义变更**（update
  循环/物理/AI/流体/生成/输入路由/demo pacing）**才 +1**；纯渲染/UI/
  音频/内容数据/文档版本**不 bump** = demo 跨这些版本持续可放
  【v3.12.0 阴影缩尺 = 不 bump 实例】）+ `recorded_with`（录制时构建
  TM_VERSION 串，**仅信息展示**——回放日志行/结束面板，永不拒播）；
  legacy 文件（pre-v3.13.0 `version` 指纹）= **零迁移映射 sim := 1 +
  recorded_with := version**（旧严格门已证 record-build == replay-build
  = sim 同期系，用户手头全部 .dmo 继续可放）；混用 schema（`version`
  与 `sim`/`recorded_with` 同现）= 点名拒；**sim 不匹配 = rc 1 点名**
  「demo sim X != this build sim Y (recorded with Z) - the simulation
  semantics differ」+ `--demo-force` 逃生门（**仅 bypass sim 门**，
  schema 严格性保留，WARN 点名；无 `--demo` 带 force = rc 2 点名）；
  `world` 双键 fresh{seed}|save{slot}【双键·缺键·槽字符集拒】/ `dts`
  【负 / > `kDemoMaxDt` 0.25 拒】/ `events` **封闭 8 型词表**
  （key_down/key_up/mouse_down/mouse_up/mouse_move/wheel/gamepad_
  button/gamepad_axis——未知 type 带 8 词表点名）+ per-type payload 面
  严格（意外键拒）；乱序/悬空帧/gzip 损坏全点名 rc=1 带路径（门 201
  S6 七面实证）；**零 format** = 独立文件族，存档 schema 30 零触碰；
  **canary 纪律**（见 PROTOCOL.md E2E 协议）：4 个 shipped demo = 模拟
  语义金丝雀，每版门内回放 savenorm EXACT——核心改动破坏 canary =
  必须 bump sim + 重新 author（时间线 JSON 字节可复现）。**v3.14.0 =
  首个实际 bump（1 → 2）**：V3-8b 手柄输入语义变更（B 疾跑 → 飞行专用
  钮 / L3 → 蹲伏 / RT → 疾跑——录制的 gamepad 事件按**当前解释**重放
  =「输入路由」条款首个实例），v3.11–v3.13 时代 .dmo（sim 1）= 点名
  拒 + `--demo-force` 强制回放【纯键鼠录制的 force 回放行为恒等 = 门
  204 S2b 终档 savenorm ≡ 实证】。
- **注入内核**（`core/demo` 纯函数，三入口共享）：`DemoPlayer.
  injectForFrame` = 该帧全部 events **同序** → `m_input.handle*`（**Input
  公开事件入口 = pump 的 SDL 事件同款收口，零映射层** = E2E xdotool 同
  款输入路径，游戏逻辑零触碰）/ `dtForFrame` = **demo dts 唯一时间源**
  （墙钟只 pacing 渲染：target = 锚 + 已完成 demo 时间 / 倍率，int64 µs
  累计防漂移；**慢机掉渲染帧模拟不追** = 记档偏离 2）/ `rendersPer
  Frame`【0.5× → 2 渲染（第二帧 = **render-only 保持**：零 update、零
  ++m_frameCount = 帧锁定系统不双推）· 1×/2× → 1】/ `nextSpeed` 0.5→
  1→2 循环纯函数；**`--frames` 按模拟帧计** = 任意倍率同模拟终态（门 (e)
  臂变速双向 savenorm EXACT 实证）。
- **双 Input 路由 = 架构边界**（回放）：观众 Input 实例 = SDL 队列**单消
  费者**（pump 排空 + quitRequested 窗口关闭）+ 玩家 `m_input` **零 pump
  纯 demo 注入**——改世界操作（挖掘/放置/攻击/物品/背包/移动/交互）在观
  众通道**无消费者** = **白名单非黑名单结构性零遗漏**（非逐操作守卫列
  表）；gamepad 事件 = **合成连接**（demo 携 gamepad 事件才建
  `kDemoSyntheticGamepadWhich` 假设备——源位掩码语义与录制侧精确对
  齐，V3-8 输入边界层复用）。
- **观众只观察白名单**（永不触达模拟）：**ESC = 暂停菜单非退出**（暂停/
  结束共面板：title/name/speed/hint 4 行 + 全幅 0.55 暗帷；Q = 退出；
  放完 = 观察态 keep-watching 末帧）/ **C = 相机模式循环**（First/
  ThirdBack/ThirdFront，会话态）/ **mouse delta + 手柄摇杆 = 绕眼 orbit
  render-only**（`kAudienceOrbitSensitivity`，pitch clamp ~±89°；零
  orbit = 完全跳过；**瞄准射线读玩家基视角 `aimForward()`**——观众转镜头
  不偷 demo 玩家瞄准，7 处世界向射线机械替换 = 输入路由红线）/ **滚轮（或
  手柄 LB/RB）= 倍率 0.5→1→2**（切换落在 demo 帧边界之间 = 永不半帧，确
  定性；真实鼠标点击同触发 = 记档偏离 v1 局限，倍率仅 pacing）/ 手柄
  Start = 暂停、左摇杆 = 数字化步进 0.03 rad（记档偏离 7）。
- **shipped 示例**：`assets/demos/example.dmo` **294 B / 266 帧 / 14 事件**
  （W 走 50 帧 + LMB 指针锁定 + 俯角挖掘 ~3 s【2 块 dirt 被挖】+ E 背包
  开关 + C 三后视角 + 滚轮 zoom；`tools/make_example_demo.py` 确定性生成
  **重跑字节恒等**（手写 10B 钉头 PITS 385 纪律），`example_timeline.
  json` 人类可读源）+ 三个预设教程 demo（`tutorial_dig` /
  `tutorial_build` / `tutorial_fly`，11-06 新增，时间线 JSON 源 + 逐字节
  可复现，玩法见 GAMEPLAY.md「demo 回放」节速查表）——**4 件全 = canary
  家族**（每版门内回放 savenorm EXACT）。

## 合成与容器

- **配方系统**（`assets/data/recipes/*.json`，**112** = shaped 58 + shapeless
  54【含 `[V2-7]` 金/钻块 ×2 / `[V2-12]` 附魔台 + 书架 ×2 / `[V2-13]` 床 ×15 /
  `[V2-15]` shears ×1 / `[V3-2]` nether_brick 2×2 自压缩 ×1 / `[V3-4]`
  netherite ×8（ingot shapeless 4 scrap + 4 gold_ingot + 工具 3 直接
  crafted + 护甲 4 升级 shapeless）/ `[V3-5]` 建筑族 ×17（stone_bricks /
  bricks / mossy_stone_bricks + terracotta 染色 14 shapeless），
  `[P12]`/`[P17]` 起全数据驱动；v1.34.0 起含炉子配方
  `8 cobblestone 围圈 → furnace`）：shapeless = 原料袋（无 `type`，
  `ingredients`）/ shaped = `"type": "shaped"` + `pattern`（行字符串，1..3 行 ×
  1..3 列，空格 = 空位）+ `key`（字母 → 物品，shaped 禁用 ingredients）；经
  ItemRegistry 交叉严格校验（未知原料/产物点名）。**网格匹配 = pattern 偏移
  扫描**（pattern 的每个可能左上偏移都试一遍，空格对应的格必须为空）；**匹配
  顺序 shaped 先、shapeless 后，首个命中即成**（注册表加载序 = 文件名字典
  序）；`canCraft`/`craft` 确定性纯逻辑（原料按槽序升序消耗、副本模拟
  **全有或全无**——失败背包逐位不变，产出永远放得下）；无多配方消歧仍成立
  （shipped shapeless 原料集互不重复——将来重复 = 加载序首个命中，系统不猜）
- **E 背包面板**（`E` 开关；**36 格 = 9 hotbar + 27 背包（4 行）`[UX-8f]`**
  = MC 标准背包 + **2×2 合成格** =
  共享 3×3 网格模型的左上 4 格（放不进宽/高 3 的 shaped pattern——「需要
  工作台」= de facto 网格尺寸门，与 MC 一致）+ 右侧**配方列表只显示
  shapeless**（产物图标 + 名称 ×N，白 = 可合成/灰 = 不可 + `需要：` 原料
  清单；点击行 = `craft()`；**`[UX-8c]` 两列排布**【CRAFT-4 机制泛化：
  compute 三处去 kind 分支，左列 (n+1)/2 行；CraftingTable kind 位级不变红线】
  + **可合成排序置顶**（纯函数 `orderedRecipeHints` 两遍同序稳定分区
  canCraft 真在前、组内保注册表序，点击/渲染同一 seam；工作台 shaped 列表
  不排序 = display-only 记档偏离）+ 行内 fitLabel 约束列宽【E 面板 native
  宽 1000→1288 = 行高 ≈2×】）
- **工作台 `[P17]`→`[CRAFT-1]` 分块→`[CRAFT-4]` 提示**：`interact:
  "crafting_table"` 数据驱动块（破坏掉自身）；锁定时右键命中 = **打开该块的
  面板**（3×3 网格 + 输出格，不放置方块）；**每工作台块独立 `CraftingGrid`**
  （`[CRAFT-1]`：`std::map<BlockPos, CraftingGrid> m_tables` 位置序 = 炉子
  `[P44]`/营火 `[P71]` per-block 范式，**不消费 ENT-2 per-instance 状态**【
  工作台无朝向】）+ `openTablePanel(pos)` 绑定所点块（`InventoryUI::bind`
  零布局零视觉变化仅换数据源；E 面板 2×2 终生绑定零改动）+ setBlock 收口
  创建/删除（**破坏 = 容器内物品丢弃** = 炉子破坏同款语义，记档偏离 MC）+
  `protectStates(m_tables)` 卸载保护；**format 19 `tables` 段**（恒写可空数组
  + {pos, slots[9]} 位置序 + 槽 format-3 纪律【槽带 damage 拒 = `[SURV-3]`
  grid 范围】+ 双向交叉校验/重复 pos/未知字段全点名 + 键纪律三向【19 必
  tables 拒 table_grid / ≤18 必 table_grid 拒 tables】+ **parse 时 sole 归属
  迁移**【legacy 非空 ∧ 恰 1 表 → 归属，0/多 → 丢弃，`[P44]` 先例】+ 解析期
  不变量每表必有 entry = 无 rescan）；**`[CRAFT-4]` shaped 配方提示列**：面板
  网格旁 **52 个 shipped shaped 配方两列排布**【单列拟合因子 0.34 不可用；
  v1.34 炉子配方入列 = 33→34，`[V2-7]`/`[V2-12]`/`[V2-13]` 后随配方集增长】，
  每行 = mini 3×3 pattern（暗格 + 键位物品图标，按定义左上对齐 = 合法摆放）+
  输出图标 + "name xN"（**背包可供应则白/否则暗** = `canSupply(patternBag)`
  纯函数：每覆盖格 1 单位/空格除外/跨字符合并/ItemID 升序）+ **提示行点击 =
  消费性 no-op**（不合成不动背包，吞点击 ≠ 槽外点击丢 carry）；长 name 行内
  溢出 = 确定性 `fitLabel` 尾截（" xN" 恒保）；**`[UX-5]` 材料修复**（MC 铁砧
  语义的 crafting-grid 形式）：受损工具/护甲 + 材料入合成网格 → **输出槽 =
  修复后物品**（1 材料 +`kRepairFraction` **25%** max 耐久，damage 钳 0；
  `matchRepair` 格索引序配对 / `applyRepair` 原位**保格内 count**【flint_and_-
  steel maxStack 64 不丢件单测钉死】）；**配方恒优先、repair = 无配方回退**
  （shipped 实证碰撞：受损弓 + 钻石走 infinite_bow 配方胜 / + 棍走修复，
  Priority 专测）；`clickOutput` 修复分支（**三重守卫** = 异 item / 同 item
  异 damage / maxStack 溢出全拒 = SURV-3 合并纪律）+ 输出图标携 outputDamage
  （drawIcon 耐久条自动显示，零布局变化）+ "craft" 音既有路径自动；物品 JSON
  严格字段 **`repair_with`**（白名单演进 + 空串/非串/无 durability 携带/未知
  材料全点名；`checkRepairMaterials` init 交叉校验 CRAFT-3 范式）；**35 件
  shipped 全配 MC 精确材料**（wood 系 3 → planks / stone 系 3 → cobblestone /
  iron 系 3 → iron_ingot / **netherite 系 3 → netherite_ingot `[V3-4]`** /
  弓族 6 含 poison_bow → stick / 燧石与钢 → flint / 皮甲 4 → leather / 铁甲 4
  → iron_ingot / 钻甲 4 → diamond / **netherite 甲 4 → netherite_ingot
  `[V3-4]`**）；**E 面板 2×2
  同款生效**（MC 铁砧仅限工作台 = 记档偏离）+ 无 XP 成本/成本递增（`[V2-11]`
  XP 系统引入后仍零成本 = 记档偏离，MC 铁砧有 XP 成本）
- **炉子 `[P40]`→`[P44]`→`[CRAFT-6]`→`[CRAFT-5]`**：`interact: "furnace"`
  块（破坏掉自身）；锁定时右键命中 = **打开该炉子的面板**（**36 格背包
  `[UX-8f]` 4 行** + input/进度条/output + fuel 槽，MC 排布；无任何合成动作——output 自己填满，
  点击取走）；**每炉子块独立 `FurnaceState`**（`std::map<BlockPos,
  FurnaceState>` 位置序 → 逐帧 tick 迭代确定；放置时建空态、块被换/破时删态
  ——`Game::setBlock` 收口；面板打开时绑定所点方块，面板期间块交互暂停 →
  绑定态稳定；各炉独立点火/熔炼互不干扰——A 满 output 不会暂停 B）。
  **可熔炼** = 物品 `smeltable_to`（产物名，init 交叉校验）+ `smelt_time`
  （秒，(0,100]，**缺省 10 = MC 炉子时长**）：4 肉（beef/mutton/chicken/
  **hoglin_meat `[V3-3]`** → cooked）+ **sand → glass `[CRAFT-6]`**（MC 10 s
  缺省）+ **块/物品 smelt 链（CRAFT-6 block 属性继承 + 物品字段）**：
  **clay_ball → brick →（2×2 合成 bricks）→ bricks → terracotta `[V3-5]`** /
  **netherrack → nether_brick `[V3-2]`** / **ancient_debris →
  netherite_scrap `[V3-4]`**（见下条）。**燃料** = 物品
  `fuel_time`（秒，(0,1000]，显式 0 拒绝）：coal **80 s** / planks **7.5 s**
  （`[V2-7]` 起块属性迁入，值不变）/
  stick **5 s**（MC 同款数值）+ **`[CRAFT-6]` block 属性继承**（block 可选
  属性 `fuel_time`/`smeltable_to` → `buildBlockItems` 继承进合成 block item
  ——显式同名物品文件被注册表拒【「block items are defined by their block」=
  唯一数据驱动路线】）= **wood 20 s 燃料**（项目木族刻度：planks 7.5 / stick
  5 / coal 80 = MC 锚，20 s ≈ 2.7× 单 planks = MC log-vs-plank 比值方向）+
  **lava_bucket 1000 s**（MC 精确 = 区间上限）。**规则**：有燃料库存时炉火
  常亮（燃尽当 tick 立即点下一根，**整段燃烧时间授予该 tick**【点火帧授予不
  消耗 = 每根燃料实际多烧 1/60 s，刻意确定性】）；烧着 + 输入可熔炼 + output
  有空（空或同产物未满）才推进进度；进度满 `smelt_time` → 输入 −1、output
  +1；**output 满 = 暂停进度但火继续烧**；**输入物品变更（含取空/换非熔炼物）
  = 进度清零**（移 input 快捷移动同款不变量 `[CRAFT-7]`）；非燃料 burn 0 永不
  消耗。**计时**：每状态每帧固定 **1/60 子步 + 整数子步倒计时**（与昼夜同类：
  无输入运行逐字节可复现；`std::map` 位置序 → 多炉 tick 顺序确定；600 子步 =
  10 s 缺省熔炼）。**存档**：format 7 `furnaces` 数组——**每炉子块一条目**
  （`pos` + 三槽 + 整数 `progress_substeps` / `fuel_substeps`），位置排序，与
  方块差集**双向**交叉校验（entry 无块/块无 entry 均拒绝）；format 6（单个
  全局 `furnace`）仍可加载：恰 1 个炉子块 → 状态归属它，0/多个 → 丢弃。**
  表现 `[CRAFT-5]`**：① **炉口火焰粒子**（燃烧中炉子每 **36 帧**
  （`kFurnaceParticleEveryNFrames`，专用盐）自炉块**上方一格**发射 `[P66]`
  余烬批 + `(m_frameCount+1)%36` 门 = 截图帧恒携 age-0 批；**粒子光读上方
  空气格**【炉格 opaque 无 emission = 黑粒子已修】；`render.furnace_flames`
  代码默认 true，off = 位级恒等）② **熔炼完成 "smelt" ding**（新 wav =
  1320/660 Hz 双泛音钟 + LCG tick 种子 0x51DE，**空间化于炉块中心** + 帧
  锁定 info 日志）+ 复燃 0→燃烧帧复用 "ignite"；事件检测 = 纯函数
  `furnaceTickEvents(before, after)`（tick 无副作用，**音频不入帧状态** =
  硬规则 8）；**熔炼经验球 `[V2-11]`**（item `smelt_xp` 默认 1 = 每完成 1
  球，见「生存系统 · XP」）；**外观**：`[V2-7]` 起 **front 单 tile
  `furnace_front`**（块级 `faces` 段首消费，见「材料补齐批」；四侧面仍共用
  side 纹理，**无 lit 纹理态** = 记档偏离——lit 表现 = 既有炉口火焰粒子）
- **netherite 材料链 `[V3-4]`（v3.6.0，零核心代码纯数据）**：
  **ancient_debris**（下界台地矿脉 p 0.0026，hardness 10 = MC 50 的 /5 缩放
  先例，见 MECHANICS_WORLD.md「下界维度」）**熔炼 → netherite_scrap**（block `smeltable_to`
  CRAFT-6 继承）→ **shapeless 4 scrap + 4 gold_ingot = netherite_ingot**
  （记档偏离 1：MC 用 netherite nugget）；**工具 ×3**（pickaxe/axe/shovel，
  直接 crafted shaped 配方：durability **2031 MC 精确** / break_speed 4.0
  项目铁刻度 / `fast_against` 2.0 × **21 块** = 铁 10 块清单 + nether 台地
  11 块【netherrack/双 nylium/basalt/双 stem/soul sand·soil/ancient_debris/
  nether_brick/magma_block】/ repair netherite_ingot）/ **护甲 ×4**
  （durability **545/792/743/644 = 钻石 ×1.5 MC 精确** / ap 3/8/6/3 /
  **shapeless 升级配方 = netherite_ingot + diamond_<slot>** 记档偏离 2：项目
  无锻造台 / model 克隆钻石系 记档偏离 3）；icon 9 件暗红铜色板（记档偏离
  4，既有 84 张 md5 逐一不变）；附魔面：工具 pick/axe/shovel 类
  enchantability 14、护甲 10（可附魔 22 → **29 件**，见下条）；
  **创造模式 `[V3-11a]`（v3.17.0）**：`G` 键切换（locked pointer +
  仅存活，C 键范式）；**存档 flag**（format 31 `player.creative`，
  MC per-player /gamemode 语义）——进入即填饱饥饿 + 开飞行，关闭回
  生存（飞行关、饥饿自满条起衰减）。语义（全在 Player/Game 侧门控，
  数据零字段）：**全源伤害 no-op**（damage 单收口 `if (m_creative)
  return false`——lava/fire/fall/mob/arrow/starve/void 一律免疫 = 不
  死）+ **饥饿静态满**（衰减/饥饿 tick `if (!m_creative)` 包裹，非
  early return 保护 lava 段）+ **即挖**（creative 强制 break
  duration 0 = 复用 data-driven instant path）+ **无掉落/无 XP/无
  挖掘耐久**（breakBlock gate 只包 4 段 drops/chance/XP——粒子音效
  保留）+ **放置不消耗**（3 点）；sim 不 bump（canary 4 demo 回放
  EXACT 实证）。**创造物品栏 `[V3-11b]`（v3.18.0）**：创造下 E
  面板 = 双 tab（标题行「物品栏/背包」按钮，生存 E 面板位级不变 =
  门 208 旧≡新字节恒等实证）——物品栏 tab = **前缀搜索框**（SDL 文
  本输入新面：`SDL_StartTextInput` 仅创造 Inventory 面板期开启【SDL3
  文本输入默认关 = 无输入运行结构性免疫根】，Input 帧态 frameText +
  frameBackspace【wheel delta 孪生，frameBegin 重置；**不入 tap 词
  表** = demo 录制 schema 8 型不变，录制的 demo 永不打字，记档偏
  离】，词表 [a-z0-9_] 小写归一 + 24 上限 + 光标帧锁定闪烁）→
  **224 物品 name 升序前缀过滤** + **6×4 网格滚轮翻页**（1 tick =
  1 项，双向夹紧；搜索缩表重夹）+ **左键 = 满组授予 / 右键 = 单个**
  到**选中热栏位**（同物品有隙 = 合并并清 damage/enchants【创造
  授予不带耐久附魔 = 新栈规则】，异物品 = 替换；满同物品 = no-op；
  授予播放 pickup 音）；**搜索焦点数字语义**（物品栏 tab 开时数字
  键 = 文本入搜索，不切热栏 = MC 搜索焦点，槽位须在开面板前选）；
  背包 tab = 既有 E 面板内容位级（+标题行 tab）；面板 = 会话态零
  format（搜索串/滚动/激活 tab 均不落盘）；sim 不 bump（授予经既
  有 Inventory 公开操作，canary EXACT 实证）。**demo author 面扩展
  `[V3-11b]`**：timeline `mouse_move` 可选 `x`/`y` 绝对位【文件
  schema 既有 mouse_move x/y，author 面补齐——面板点击坐标；缺省
  0 = 既有 4 canary 位级恒等】+ 键词表 `+g`（V3-11a 的 G 键）。
  **火·熔岩免疫 `[V3-15]`（v3.16.0）**：物品可选属性 `fire_resistant`
  （strict bool，armor-only = parseArmor 守卫，shears tool-only 孪生）
  4 件 netherite 护甲 = true——**任一件穿在身上**（setArmorSlot 单收口
  同步 ANY 派生）：熔岩 burn tick 不落地（accumulator 重置 = 脱件无余
  伤）+ 火/熔岩接触**不再点燃**（Phase-57 重应用门控）+ 既有火效果
  的伤害 tick 抑制（视觉保留，MC fire-resistance 语义）；**熔岩中死亡
  的原因行覆写**（致死一击若为 fire 效果 tick 且 inLava → 「死于熔岩」
  = MC 元素归因）；**记档不做**：击退抗性（项目无玩家击退面 = 数据字段
  无消费方，弃字段）/ 悬浮熔岩（流体模型改造面）
- **建筑内容族 `[V3-5]`（v3.9.0）**：① **黏土链（MC 完整形态）**：水底
  sediment 生成（见 MECHANICS_WORLD.md「世界生成 · 水底 sediment」）→ 破 **clay 掉 clay_ball
  ×4**（hardness 0.6）→ **clay_ball 熔炼 → brick** → **2×2 brick shaped →
  bricks**（hardness 2.0）→ **bricks 熔炼 → terracotta**（块 smelt 先例 ×2，
  CRAFT-6 纪律）② **石砖族**：stone_bricks（2×2 cobblestone MC 精确，
  hardness 1.5）/ **mossy_cobblestone（水底稀有生成，记档偏离：MC 废墟替代
  来源）** / mossy_stone_bricks（shapeless 石砖 ×3 + 苔石 ×1，记档偏离：MC
  图案配方内容等价）③ **陶瓦染色族**（Phase 41 机制首个新族）：terracotta
  white 基块（dye_base + dye_color white = bed/wool 先例纪律）+ **14 染色
  变体**（**项目染料词表 15 色含 white 无 lime**——设计 16 色按项目词表收敛
  记档偏离；init 交叉检缺一色点名）+ RMB 持染料染色既有路径免费 + **14
  shapeless 染色 + terracotta 配方**（纹理 = 确定性混合公式 (base×3+dye×2)//5
  自建记档偏离：项目无 base×染料混合先例）④ 块 20 全 opaque `light_opacity`
  键省略 = 派生 15（记档偏离 2）/ 物品 2（clay_ball·brick 各 64 叠单盒
  model）/ 纹理 20 tile（atlas 145→165 空格零移位）
- **附魔台 `[V2-12]`（v2.13.0，真附魔台范式）**：消费三处「附魔台替身」
  记档（master_bow P82 / 弓强化 P77-79 / 信标 P91）。**7 附魔**
  （`assets/data/enchantments/` 数据驱动，严格解析点名）：锋利 sharpness
  （axe，近战 +（L+1）点，MC 精确区间）/ 保护 protection（armor，+4L 护甲
  点，**共享 20 点 cap** 记档偏离：MC 保护独立 + 总 80%）/ 效率 efficiency
  （pick·shovel·axe，正确工具 breakSpeed +（1+L²）= MC 精确）/ 耐久
  unbreaking（全 durability，每损点掷骰跳过：工具 **L/(L+1)** / 护甲
  **2L/5(L+1)** = MC 1.21 remove_binomial 精确，**无帧依赖确定**）/ 力量
  power（bow，shoot_damage +L）/ 速度 speed（bow，shoot_speed +L，记档偏
  离：MC Quick Charge = 蓄力时间）/ 无限 infinite（bow，发射不耗尽最后一
  支，∨ 字段）——max 5/4/5/3/5/3/1、weight 10/10/10/5/10/10/1、eCost 区
  间 = MC 精确（如锋利 1–11/12–22/23–33/34–44/45–65）。**物品侧**
  `enchant_class`（7 词汇表）+ `enchantability` 严格互配（**12 工具**：
  木 15 / 石 5 / 铁·netherite 14【`[V3-4]` +3 件】+ **16 护甲**：
  leather 4×15 / iron 4×9 / diamond·netherite 8×10【`[V3-4]` +4 件】+
  **bow 1** = **29 件**（v2.19.0 时 = 9 工具 + 12 护甲 + bow 1 = 22）；
  仅基础 bow 可附魔，
  变体弓保持替身定位）+ lapis `enchant_material: true`（青金石槽数据驱动
  门，tool 携带 = 点名拒）。**确定性 offer**（`core/enchanting` 纯函数，
  **全整式禁 double** = -O3 融合风险）：种子 = worldSeed ^ hash3(台位) ^
  enchantability ^ xpSeed 单推进流（PITS 30 纪律）；三槽 xpBase =
  1+randInt(7)+⌊B/2⌋+randInt(B) / top = ⌊max(1,xpBase/3)⌋ / mid =
  ⌊2xpBase/3⌋+1 / bottom = max(xpBase,2B)（MC 精确）+ lapisCost 1–3 均
  匀（记档偏离）+ eCost 含 randTri 整式；选择循环 = 类匹配 ∧ eCost 入区
  间取最高级 → 权重 pick → incompatible 移除 → 继续 v%(eCost+150)<150 →
  eCost/=2 回环（MC 精确）；bookshelfCount = **5×5 环×双层 cap 15**（记档
  偏离：无 MC 视线规则）；**仅面板打开时求值**（无面板零成本 = 结构免疫
  根）。**XP 扣减** = 整数精确：扣整级、条内余量保留（T(L0−c) + (pool −
  T(L0))）；xpSeed 成功附魔后推进。**新块 ×2**：附魔台（hardness 5 /
  light 7 / interact "enchant" / 恒掉自身【记档偏离：MC 镐门】/
  full_cube【记档偏离：3/4 盒触 mesher+碰撞红线，定案全盒】）+ 书架
  （hardness 1.5 / `enchant_bookshelf` 属性 / full_cube）；**配方 ×2**
  （台 = shapeless 4 obsidian + 2 diamond + 3 lapis / 书架 = shaped
  PPP/LLL/PPP，lapis 代 book = 记档偏离：项目无纸/书链）；tile atlas
  102→104 零移位。**面板**（furnace 范式全克隆）：RMB 开（interact 路
  由）+ pick-and-place carry 模型 + 物品槽左/青金石槽中上/3 offer 行右「名
  罗马 xpCost」+ E/关归还 + 复用 "craft" 音（记档偏离：无专用附魔音）；字
  体 +4 字锋利限魔（`kCjkGlyphs` 423→427，atlas 256×2112，顶 512 行逐字节
  不变）。**format 27**：`player.xp_seed` = **16 位小写 hex 串恒写**（记档
  偏离：JSON number = double 53 位，64-bit seed 须无损；26→27 迁移 =
  worldSeed ^ kXpSeedInit）+ 物品 `enchants` {id: level} 三处（slots/
  armor/entities，omit-when-empty）+ 四拒（26 携 xp_seed / 26 携 enchants
  / 27 缺 xp_seed / 交叉校验 level>max·类不兼容·未知 id）全点名
- **营火 `[P71]`→`[P75]`→`[P76]`→`[P78]`**：**家族** = `campfire`（
  `interact: "campfire"` 第三交互词汇表值、cross 模型、光 **15**、hardness
  0.5、破坏掉自身、非 flamable、shaped 配方 `SSS/LCL/LLL` = 3 stick + 5 wood +
  1 coal MC 配方）+ `campfire_extinguished`（`[P75]` 未点燃兄弟：同
  interact、光 0、掉自己、非 flamable、同纹理柴堆无火苗 = 水浇灭产物，**保留
  烤制状态**）。**四槽烤制 `[P75]`**：每家族块一个 `CampfireCookState`
  （`std::map<BlockPos, CampfireCookState>`，setBlock 收口创建/删除，位置序 =
  确定性 tick 序），**4 个独立槽**（`slots[4]` + `progressSubsteps[4]` 各自
  计时）；**每槽至多 1 个物品**——手持**可熔炼**物品（`smeltable_to` 非空 =
  数据驱动门）右键 = 放**第一个空槽** 1 个（消耗 1，"place" 音；四槽全满 =
  no-op——无面板 v1 记档）；**空手**右键 = 取回**第一个非空槽**（生熟均可，
  "pickup" 音，MC 空手规则；**未点燃的也能取回**）；**未点燃的不能放**（没火
  烤不了）；右键链中营火分支**先于进食通道**（MC 语义：手持食物右键营火 =
  放上火上不开吃——项目全部可熔炼物品都是食物，无此前置则烤槽永远填不上；
  炉子/工作台保留既有进食优先行为）。**计时**：每状态每帧 1/60 子步 + 每槽
  整数进度（与炉子同类：无输入字节可复现）；速度 = **炉子节奏**（复用物品
  `smelt_time`，`smeltSubstepsFor`——无独立 config）；**无燃料**（营火自燃 =
  与 MC 一致）；**只有点燃的营火烤**（未点燃 = 时钟暂停）；产物非可熔炼 →
  熟品在槽中静置。**水浇灭 `[P75]`**：点燃的营火 **6-邻任一流水体（任意
  等级）→ 即时变 `campfire_extinguished`**（光 15 → 0 局部重光照——fire 块
  浇灭的 `[P64]` 同款规则，水胜火）。**再点燃 `[P76]`**：`ignites_to:
  "campfire"` 块属性——燧石与钢（任意 `ignites_flamable` 物品）右键 = 原位
  setBlock（"ignite" 音、物品不消耗 = `[P60]` 语义），烤制状态经 setBlock
  收口存活并**续烤**；交互在**营火分支内**处理（先于通用点火器分支消费
  点击）；通用点火器分支的 `ignites_to` 路径对非营火家族块同样生效（数据驱动
  统一语义）。**烤制中渲染 `[P78]`**：**每个非空槽**的物品图标以 0.35 大小
  交叉 quad、**0.3 间距 1×4 横排**悬浮格上半部（槽 i 中心 = 格中心 +
  (i−1.5)×0.3——`[P71]` 的 0.5 单图标 = 4 槽排不下，缩小为行布局 = MC 展示
  全部烤位；cross 块惯例：视角无关、深度遮挡；+Y 假法线 = 顶面亮度档）。
  **座位 `[P71]`/`[P75]`**：crouch（LeftControl）+ 贴地 + 玩家 AABB 严格重叠
  **营火家族**格（点燃的或未点燃的——MC：柴堆没变，只是灭了）→ `isSitting`
  （每 tick 由按键 + 世界重算，**不入存档**）：锁定移动 + 跳跃（松 crouch 或
  脱离重叠 = 站起）；crouch 高度即坐姿（无独立坐姿 = v1 记档偏离）。**存档**：
  format 13 `campfires` 数组——**每家族块一条目**（`pos` + `slots`：**恰 4
  个**槽对象 `item`/`count`/`progress_substeps`），位置排序，与方块差集**双向**
  交叉校验（家族 = 点燃 + 未点燃）+ **每槽进度不变量**（progress > 0 ⇒ 该槽恰
  1 个可熔炼物）；format 11/12 旧单槽形状可加载（→ 槽 0 迁移；旧形状带
  `slots` 键 = 版本谎报拒绝）；format ≤10：无条目，手编旧文件的家族块编辑 →
  空状态
- **染色 `[P41]`/`[P45]`**（全数据驱动）：**染料物品** = 可选字段
  `dye_color`（颜色名 = 纯数据词汇表：任何 `dye_<color>` 物品注册即扩展
  词汇表，**加颜色零代码改动**；显式空串拒绝）——当前 **15 种**
  `dye_<color>`（material 64 叠 2D 图标）：white/black/brown/red/yellow/
  orange/pink/gray/silver（`[P41]`）+ blue/green/purple/magenta/light_blue/
  cyan（`[P45]`；项目 silver 位 = MC light_gray，MC 16 色位一一对应）。来源
  配方（MC 1.14+ 同款）：白←骨头、黑←煤炭、棕←腐肉、红←生牛肉、黄←金锭、
  灰/橙/粉/银←混色、**蓝←lapis、绿←cactus、紫←allium、品红←tulip、浅蓝←
  cornflower、青←lapis+绿**（来源块见 MECHANICS_WORLD.md「世界生成 · 表面装饰/矿脉」）。**可染
  方块** = `dye_base` + `dye_color`（必须同时给；`dye_base` 与 `interact`
  互斥）；变体名 = `dyeVariantName(base, color)` = `"<base>_<color>"`，**white
  特例 = base 名本身**（`wool_white` 不存在，白色就是 `wool`）；当前家族 =
  `wool`（白）+ **14 个染色变体** `wool_<color>`（hardness 0.8，掉自身，6 面
  同 tile；wool 物品图标 = 3D 方块体——与 material 同名冲突用隐式 block item
  解决，记档）。**交互**（右键链 ⑦）：选中染料右键命中 dye 家族方块 = 该方块
  立即转为目标颜色变体 + 消耗 1 个染料 + "cloth" 音效（`[CRAFT-3]` 材质音）+
  **点击被吞**（染料不是方块物品，不放置）；**同色重复 = no-op 不消耗不拦截**
  （点击落到 placeBlock 无操作——文档化 v1 规则，MC 会消耗）。**init 交叉
  校验**（ammo/smelt 同款）：dyeable 块必须有 `dye_color`；块的 `dye_color`
  必须是某个染料物品的颜色；**每个 (dye_base, 颜色) 组合的变体块必须存在**
  （缺 = 死染料，启动报错）。**染色即换块**：走 `setBlock` 脏标记管线（下一帧
  重网格）+ `Lighting::updateLocal`（染色前后都是 opaque，光不变）；无存档
  格式变更（diff 按块名存）
- **物品 3D 模型 `[CRAFT-2]`/`[ENT-9]`**：物品 JSON 可选 `model` 段（**1..16
  axis-aligned box** + 单纹理 + uv 可选；严格解析：未知键三层点名 / from<to /
  uv 退化矩形 / 空 / >16 全拒；**无旋转** = MC primitive rotation 记档不做）+
  纯函数 `makeItemModelGeometry`（`kCubeFaces` 角序/法线泛化到任意 from/to
  盒，faceShade 纪律顶 1.0/±X 0.8/±Z 0.6/底 0.5，居中单位空间 −0.5..0.5，
  skyLight 1.0 块立方掉落约定，双面关裁剪）+ init per-item 模型纹理（icon
  文件存在性交叉校验 = grows_to 纪律，texture == icon 双上传 ≈64 KB VRAM 一次性）
  + `itemModelMesh(ItemID)` **惰性缓存**（幂等纯几何 build-once）+ **init 期
  物品 icon alpha 普查**（`[PRE-5]`：isPureCutoutImage 纯函数全像素 α∈{0,255}
  ⇔ cutout，单次 Png::decode = 世界掉落路由，零 JSON）。**三上下文**：手持
  （`drawHeldHand` model 分支替代 icon 面片，相机系偏移 (0.42, −0.38, −0.85)，
  固定自旋 Ry 0.6 + Rx −0.35，**弓拉/进食举到嘴变换零额外代码继承**，block
  立方手路径位级不动）/ 世界掉落（`drawItemEntities` model 分支替代**原不可见
  非 block 掉落**，blending pass only【icon PNG 带 alpha】，帧锁定 bob+spin）/
  进食举到嘴。**shipped = 全 97 件非 block 物品**（79 件 1.x 基线 +
  `[V2-5]` pork 链 2 + `[V2-7]` string/spider_eye 2 + `[V2-15]` shears = 83
  + `[V3-2]` quartz 1 + `[V3-3]` hoglin 肉链 2 + `[V3-4]` netherite
  9 + `[V3-5]` clay_ball·brick 2 = 97；
  `[CRAFT-2]` 16 件【工具 9
  + 弓 5【上臂+下臂+弦，中部开口 C 弧近似，master_bow 无 model 共用】+ 苹果
  2【果体圆近似+柄】，纹理 = 既有图标 PNG，零新纹理免重锚】→ `[ENT-9]` 其余 63
  件全补齐：ingot/dye/wheat/stick/feather/bucket/armor 1 box、bone·arrow 3、
  bottle·potion·fands·rotten 2 家族剪影；共面 Z-fight 首轮套件揪出 4 家族 10
  件修正【相交合法共面非法，mesh 守护 modelCount 16→79 全量生效】）。无
  `model` 物品三上下文**位级不变**（全部新代码以 `it.model.has_value()`
  静态门控）；**HUD 2D 图标零变化**（热栏/面板/手持角标仍是 32×32 icon 或
  block 立方）
- **方块模型 7 形状 + per-block 状态**（机制细节见「操作与交互 · 放置」与
  MECHANICS_WORLD.md「物理与碰撞」）：
  - **cross**（17 块）：`model: {shape: "cross", textures: {cross: <tile>}}`
    严格（cross 形状只收单个 `cross` 纹理键，cube 的 top/side/bottom 拒绝；
    注册时强制 solid=false + opaque=false + transparent=true——十字植物可
    穿过、不遮光、走混合通道）；面循环跳过（不出 cube 面），固定格序 cross
    通道：每格 **2 条交叉竖直面片**（格的 2 条对角线，全纹理宽，纹理正立），
    **无邻居剔除**（面片在格内，被埋时深度测试自然遮挡），烘焙**自身格**的
    光，法线朝上 = 面 shade 1.0（MC 十字植物全亮）；透明 pass 关 culling
    （双面可见）；`[GEN-2]` stage 视觉：有状态 cross（sapling）quad 两对角线
    4 角落绕**格心**缩放 (stage+1)/stateCount（MC sapling 同款），**无状态
    cross 恒 scale 1 = 逐位零变化**
  - **slab `[P85]`→`[V2-17]`**（首个新块模型类型）：半块复用 cube 纹理；
    **有状态**（shipped stone_slab `state_count 3` / `state_default 0`）：
    0 = 底部（cell y 0..0.5 实心，P85 既有几何位级）/ 1 = 顶部（y 0.5..1，
    天花板贴放）/ 2 = 双 slab（满格——破坏掉 2，`drops_count_by_state`
    [1,1,2] = MC 材质守恒）；**无状态 slab（床族 15 块）恒底部**（门 =
    hasState 非 shape，位级不变）。mesh = 单位立方 6 面按 state 映射 Y
    （state 0：Y 缩 0.5 锚底，**顶面恒绘**【cell 顶半格是空气】、底/侧面邻
    opaque 剔除；state 1：Y 偏移 0.5 + 缩 0.5 镜像锚顶，**底面恒绘**对称、
    顶/侧面邻 opaque 剔除【通用规则】；state 2：满格标准邻剔除【满块规则，
    非常绘】；贪婪满块路径**跳过 slab** 不变）；碰撞 = `solidRange`
    {bottom, top} 区间（state 0 = {0, 0.5} 现值 / 1 = {0.5, 1} / 2 = {0, 1}；
    `moveAxis`/`hasRoomAt`/`stepUp` 对实心区 [y+bottom, y+top] 判重叠——
    pre-V2-17 形状全 bottom = 0 位级 backward-compat；顶 slab 顶面 y+1 恰
    1.0 可踏升 MC 同款，顶 slab 格内站立 = AABB 重叠全挡【无 sneak 缩高，
    偏离 3】）；**放置 = 瞄准规则**（`slabStateFromHit` 纯函数：顶面命中 →
    state 0 现行为位级 / 底面命中 → state 1 天花板贴放 / 侧面命中 → 命中点
    local-y > 0.5 → state 1 否则 0，MC 精确）+ **合并**（同格同 id 互补
    state → 双 slab state 2 消耗 1，`slabMergesToDouble`；state 2 目标拒 =
    canPlaceAt solid 常规；异 id 不合并 = 偏离 2）；**蹲+右键旋转 Slab 形
    排除**（偏离 4：state = 垂直位非朝向，环转 2→0 销毁半块材质不守恒；
    台阶朝向环不受影响）；mob 生成 floor-top = 格底 + 实心区顶
    （`spawnFloorTop`：顶 slab 顶面 y+1 可生，底部 slab 脚踩 slab 顶
    floorY+0.5 不再悬空）；slab 邻满块同层侧面 Z-fight/缺面 = 贪婪 mesh
    不能半面剔除（v1 局限，KNOWN_ISSUES——state 1 侧邻 state 0 slab 同款
    半面残差）；**`[LGT-5]`** 起 `light_opacity` 0 = 透光（opaque 保持
    true；**state 值不改光属性**——ENT-2 状态约束①，LGT-5 族三 state 位级
    同款）
    **射线半格感知**（`[V2-17]` 缺口补修，门 189 S2 实证）：共享 dda
    （块射线 + 流体射线）对 slab 格做子格判定（`slabSolidRangeAt`，读
    World per-cell state，unset → stateDefault 同款回落）——从**空气
    半**进入 = 穿透（命中内表面 y=0.5【hit cell = slab 格、法线指向
    来向空气半：底 slab +Y 顶面 / 顶 slab −Y 底面】或身后块；侧穿出
    先于内表面 = 继续 DDA）/ 从**实心半**进入 = 整格边界命中现行为
    （边界「入实心」判 = 严格内部 ∪ 顶边且下行 ∪ 底边且上行；水平恰
    贴面沿行 = 穿透）/ 双 slab 满格 = 整格不变。**合并场景由此可达**
    （空气半穿透 → 身后锚块命中面指向 slab 格 → target = slab 格 →
    `slabMerge`；placeBlock 代码零改动）；**记档偏离：cell-DDA 旧
    「瞄准底 slab 空气半 = 破 slab」→ 新「破身后块」**（MC 同款）；
    第三人称相机遮挡 / 箭视线 / 桶射线走共享 dda 自动一致（无特判）
  - **stairs `[P87]`/`[P89]`/`[P92]`/`[P94]`/`[P97]`→`[ENT-2]`**（第二个新块
    模型类型）：斜面（一端满高、另一端落地）；mesh = 楔形（斜顶恒绘 + 背墙 +
    两侧三角 + 底面，按朝向绕 Y 旋转 corner/normal/cull；侧/背/底邻 opaque
    剔除；贪婪跳过）；碰撞 = 斜面高度感知（`solidRange` 取 AABB 覆盖
    **最高**斜面点——玩家脚落在斜面高端）；朝向演进：`[P89]` 4 块变体
    （stone_stairs[_east/_south/_west]，`stair_facing` 0-3 数据）→ `[P92]`
    `stair_rotates_to` 块 id 环（保留解析不再用于旋转）→ **`[P94]` per-
    instance 朝向 = World 每格 byte**（项目首个 per-block 状态，突破「无
    per-instance 状态」哲学）→ `[P97]` 放置自动朝向（低端朝玩家视线，
    `stairFacingFromYaw` 纯函数）→ **`[ENT-2]` BlockState 泛化**（下条）。
    **`[LGT-5]`** 4 变体 `light_opacity` 0 透光
  - **wall `[P95]`/`[P96]`**（第三个新块模型类型）：1.5 格高柱（y 0..1.5 实心
    = 满块 + 顶 0.5 帽），复用 cube 纹理；mesh = 6 面 Y 缩 1.5（底面/顶帽邻
    opaque 剔除、贪婪跳过）+ **`[P96]` 侧面连接**（墙-墙共享面整面剔除 = 并墙
    连续；墙-满不透明块只绘暴露帽带 y 1..1.5；slab/楼梯/透明/空气邻居保守整
    面）；**barrier 碰撞** = 1.5 不能穿过不能跳过（帽伸进上方格 y0−1 扫描）；
    内容 `stone_wall`（hardness 1.5 = MC，自掉，复用 stone 纹理无新 atlas）+
    配方 shapeless 6 stone → 4（MC）；光照按满块不透明（帽是空气但挡光 = v1
    简化）→ **`[LGT-5]`** 改 `light_opacity` 0 透光（opaque 保持 true = 剔面
    零变更）
  - **fence `[P98]`**（第四个新块模型类型）：两根 1/8 厚交叉柱（y 0..1.5，同
    墙实心区）+ 朝**栅栏/满不透明块**邻居的连接横杆（1/8 厚全高开两端——边界
    端与邻居横杆在格边界相接成连续栏杆，柱端在柱面内部【无端帽共面双绘】；
    柱顶/底邻 opaque 剔除 = 栅栏叠栅栏）；**单 side 纹理**（top/bottom/cross
    键拒绝 = cross 纪律）；opaque 挡光 → **`[LGT-5]`** 改 `light_opacity` 0
    透光；**barrier 碰撞 = 墙同款 1.5**（细柱只是 mesh，整格 1.5 实心）；
    **贪婪剔面修正**：满块面**不因栅栏邻居被剔**（细柱不能遮挡——`[P85]`
    slab 同款「半面」问题的反向处理：slab opaque 剔面留上半洞、栅栏不剔保
    面）；内容 `fence`（hardness 2.0 = MC，自掉，salt 933/934 纹理，atlas
    空位免重锚）+ 配方 shaped **PSP/PSP**（4 planks + 2 stick → 3 = MC）；
    偏离：横杆只朝栅栏/满块（slab/楼梯/墙邻居无横杆）、无栅栏顶放置规则、
    木质单纹理
  - **glass_pane `[P99]`**（第五个新块模型类型）：z 居中 **1/16 厚 1×1 玻璃
    片**（双面透明 pass）+ 四条边细 rim；**强制非 opaque + 透明**（光透过，
    cross 纪律同款强制属性）；单 side 纹理；**连接（透明版 `[P96]`）**：rim
    朝玻璃板邻居剔除（共面 rim 双绘 = 透明 pass 暗缝）、朝 opaque 邻居顶/底
    rim 剔除；X 方向板排 = 连续单片；**碰撞 = 整格实心 1.0 可跳**（跳高
    1.129 > 1.0；墙/栅栏 1.5 不可跳）；内容 `glass_pane`（hardness 0.3 = MC
    玻璃，自掉，1px 框 + 半透明芯纹理，atlas 空位免重锚）+ 配方 shapeless
    **8 glass → 16**（MC）；偏离：无「玻璃板须放固体上」规则、rim 简化（全
    纹理压扁无独立 UV）
  - **BlockState `[ENT-2]`**（`[P94]` 楼梯朝向的泛化机制 = 通用 per-
    instance 状态）：**机制**：chunk `m_blockState`（每格 byte 数组，哨兵
    255 = `kBlockStateUnset`「未设置」）+ World `get/setBlockState`（set 只验
    <255 + y + chunk 存在 + 无变化——World 不持 registry；**[0, stateCount)
    值域由三个写入方强制**：收口写 default / 旋转 (s+1)%count / 存档 parse 按
    格块校验）。**双门控**（`Block::hasState/stateCount/stateDefault` 三纯
    访问器）：`Shape::Stairs` **隐式**状态（count 4、default = `stair_facing`
    ——shipped 楼梯内容零字段，行为逐位不变）∪ 任意形状显式 `model.state_count`
    [1,255] + `state_default` [0,count) **严格配对**（缺一点名拒，fast_against
    范式；楼梯显式写 state_count = 矛盾点名拒）。**写入点**（Game 三处）：①
    setBlock 收口——hasState → 写 `stateDefault()`，非 stateful → 哨兵写被
    World 拒（**零变化零脏**）；② 潜行 + 右键——门 `hasState()`（**`[V2-17]`
    Slab 形排除**——slab state = 垂直位非朝向，环转销毁半块材质，偏离 4），
    (s+1)%stateCount()（stair = (f+1)%4 同值），"place" 音【`[CRAFT-3]` 材质
    音】、点击消费；③ placeBlock 钩子——stair yaw 覆写（`[P97]` 语义不变）+
    **`[V2-17]` slab 瞄准态覆写**（`slabStateFromHit`：顶面 → 0 现行为 /
    底面 → 1 / 侧面 local-y > 0.5 → 1 否则 0）+ **同格同 id 互补 state 合并**
    → 双 slab state 2（`slabMergesToDouble`，消耗 1；state 2 目标拒 / 异 id
    不合并 = 偏离 2）；**破坏 = `drops_count_by_state`[state] 计数**（双 slab
    掉 2 = MC 材质守恒，物品名不变）。**消费方**：碰撞 `solidRange` + mesh
    `emitStairs`/`emitSlab`（形状门 + 哨兵回落 `stateDefault()`）+ mob 生成
    `spawnFloorTop` + 存档推导（collectSave 两路）——**哨兵回落各处同一规
    则**。**存档 format 15 `states` 段**（替 14 `stairs`）：`{pos, state}`
    严格字段、位置排序、**全量**（每个 stateful 格恰一条，**含默认值**——避免
    「非默认才存」手改静默回落陷阱）；域校验按 entry 位置对应块（blockEdits
    名 → registry `stateCount()`）越界点名；**双向交叉校验**（stateful cell
    ⟷ entry 逐格点名）；**键纪律**（版本谎报拒绝）：15 必需 `states`（空数组
    合法）+ 携 `stairs` 拒（限 14）/ 14 必需 `stairs` + 携 `states` 拒 / ≤13
    携两者皆拒；**14→15 值保真迁移**（{pos,facing} → {pos,state=facing}），
    ≤13 加载 = states 空（消费者回落 default）。**约束**：① 状态值**不得改变
    块的光属性**（opaque/light_opacity 是 per-block 非 per-state——`[LGT-5]`
    半块透光在块字段不在状态）；② **worldgen 放置源不得用 stateful 块**（biome
    surface/subsurface/decorations、ores.veins、trees trunk/leaves——
    `WorldGenerator::create()` init 交叉校验点名拒绝 = journal 派生前提
    【stateful 格 ⊆ block journal】的结构性保证；terrain bottom/deep fill 不在
    枚举内 = 边界注释 + 经验测试扫真实资产钉死）。**两类 per-block 状态存储
    分界**（YAGNI 定案不统一）：**byte 状态**（facing 0-3 / sapling stage 0-7
    量级）消费方 = 快照 mesher + 碰撞热路径——必须随 chunk 快照跨线程可读
    （`[PIPE-1]` 快照拷贝零特判）→ chunk 数组；**struct 状态**（furnace ~50-
    80 B / campfire 4 槽）消费方 = game tick / interact / overlay 面板——
    `Game` per-block map（`[P44]`/`[P71]` 范式，format 7/13 独立段）；两类
    消费方集合**不相交** = 消费驱动的自然分界
  - **树生长 `[GEN-2]`**（3 树种可种植 sapling = 消费 BlockState 的首个
    内容）：**模型**：oak（复用 wood/leaves）/ spruce / birch（新块
    spruce_trunk/spruce_leaves/birch_trunk/birch_leaves）各独立块 id =
    树种是内容
    维度非实例维度；cross + **`state_count` 8** + `tree` 对象（严格 6 键：
    trunk/leaves 块名 + min_height [1,16] + height_range [0,8] + leaf_radius
    [1,2] + shape 词汇 {oak, spruce, birch}，逐键点名拒；**tree ⇒ state_count
    ≥ 2** 注册时点名拒；trunk/leaves 存在 = 目录 load 后交叉校验点名）。
    **生长阶段 = 状态 byte 0-7**（与作物「块 id 即阶段」相反——8 阶纹理超
    atlas 预算，项目 = 1 纹理缩放）。**引擎节奏**（`saplingGrowthSteps` 纯
    函数 + `Game::m_saplings`，m_crops 四件套同款：收口注册/读档 rescan/卸载
    保护/每帧 tick）：每帧固定 1/60 步（昼夜类，无输入字节可复现）先过三道
    门——① **土壤门**（`plant_on` 复用：下方格 ∈ [dirt, grass]；**放置门 +
    生长门一体**，土壤被挖 = 生长暂停等待不自毁——文档化偏离 MC 掉种子）②
    **光门**（`growth_light` 9 = max(sky, block)，`[LGT-2]` 叶冠下会真挡——
    3 层冠下 = 9 临界仍过，叠冠/4 层+ = ≤7 停长，MC 语义收紧记档）③ **roll**
    （splitmix64(seed + frame×GOLDEN + hash3(pos, 专用盐)) % `growth_ticks`
    **120** = 0，≈2 s/阶 ≈16 s/棵 = 项目压缩先例；位置哈希去同步、专用盐不
    与作物/世界生成同流）。**落树**（roll 过 + 阶段 = 7）：`treeHeightRoll`
    纯函数滚高 ∈ [min, min+range]（**帧无关**——被挡重试滚同一棵）+
    `planTree` 形状表（root = sapling 格 = trunk 底：oak (T−2,r),(T−1,r),(T,1)
    **与 worldgen placeTrees 逐格同构**（单测钉死）/ spruce (T−2,2),(T−1,1),
    (T,1),(T+1,1) 下宽上尖锥 / birch (T−1,1),(T,1),(T+1,1) 细高小冠；(0,0)
    offset ≤ T−1 跳 trunk 占位、半径 2 层圆角）→ fits（全格 air 无流体 y ∈
    [0,128)，sapling 格除外）→ `placeGrownTree`（trunk 先冠后逐格 setBlock
    收口 + **1 次 `updateRegion`** over 树 bbox = 冠影 `[LGT-2]` 可见，玩家
    触发非每帧成本，~5–8 ms 记档）；**放不下 = 本 roll 不完成等待重试**（MC
    天花板规则）。**来源闭环**：leaves `chance_drops` 各 + 对应 sapling @
    **0.05**（oak leaves = `[P55]` 集演进 +1 条目，apple 保留 oak 专属；
    spruce/birch leaves 不掉苹果记档）+ 破坏 sapling 掉自己（可再种）+ 隐藏
    starter 槽 24-26 各 3 颗。**世界生成零触碰**：野外森林保持单橡种
    （worldgen trees 段/生成器代码一行不改）；sapling stateful ⇒ worldgen
    放置源 init 不变量天然拒（多树种野外 = 未来项，需重锚）。**`[GEN-2]` 记
    档**：旧存档无 spruce/birch 来源（starter 只新世界装载，读档整体替换
    inventory = 不向旧存档注入物品，有意为之）；旋转副作用 = 潜行右键 sapling
    → stage+1 手工催熟（stage 7 回 0，**接受为特性**，YAGNI 无逃逸字段）；
    斧对新木无效（spruce/birch trunk 未入 `fast_against` 清单 = 基础 break_
    speed，清单演进 = 独立未来项）
- **信标 `[P83]`→`[P91]`→`[LGT-4]`**：**数据**：`beacon` 块（self-drop、不
  发光、不生成——只经放置/合成）带 **`beacon_effects: ["absorption",
  "speed"]`**（`[P91]` 多效果列表 = MC 主 + 副效果 / 「附魔台」替身
  【`[V2-12]` 起真附魔台已实现】；指向
  **多个**效果名，init 交叉校验**每条目**效果存在——grows_to 纪律；
  `beacon_effect` 单值 = 向后兼容 1 元素列表，**两字段互斥**【都写 = 拒绝，
  grants_effect 纪律】）+ `iron_block` 块带 `beacon_base: true`（基座标记；
  **`[V2-7]` 基座集合 = {iron, gold, diamond}**——`beacon_base` 数据驱动布
  尔逐块入列，MC netherite 块无 = 记档）。**功率
  等级**（`world/beacon` 纯函数）：信标正下方**连续完整基座层数**——3×3
  （紧挨信标下）= 等级 1、5×5 = 2、7×7 = 3、9×9 = 4；**第一层缺角即停**
  （金字塔从上往下连续完整）；等级**每帧重算、无 per-instance 状态**（生长
  `[P48]` 的「块 id 即状态」同款）→ **零 format 变更**。**施加**：`Game::
  m_beacons`（setBlock 收口 + 读档 `rescanBeacons`，生长同款）每帧遍历；等级
  ≥1 且**玩家脚到信标格 Chebyshev 距离 ≤ `beacon.range_blocks`（代码默认
  50；MC 球状范围 = 项目立方记档）** → **循环 `beacon_effects` 列表**对每效果
  `apply(effect, 0.5s)` **刷新**（火源范式——站范围内**全部**效果常驻、离开
  0.5 s 到期）；**吸收池同步**（PITS）：信标的 apply 在 `updateEffects`
  **之前**，其「帧内总和变化才同步」检测看不到 → `updateBeacons` 捕获施加
  前后 `totalAbsorption`、变化即 `syncAbsorption`。**光束**：半透明薄柱
  （basic program 纯色 `u_alpha` 默认 1.0——纹理路径用 texel.a 不受影响；薄柱
  mesh 透明 pass 关裁剪、世界坐标恒等 model 单 draw；等级越高柱越长 =
  `kBeaconBeamBaseHeight` **64** + 等级 × `kBeaconBeamLevelHeight` **32** 格；
  `beacon.beam` 代码默认 true 可关）；**`[LGT-4]` 光束入光**（最后一个恒白
  unlit 消费者）：经 per-draw 光覆盖接收世界光——`beamLightSample` 采样**信
  标块上方空气格 (x, y+1, z)**（= beam 最低格；每 beam draw 后 bind + 复位 0
  = `[UXFIX-1]` 纪律）——深夜月光 7/15 / 近萤石亮化 / 白天恒白；**光束不提供
  方块光**（不照亮周围，`[P83]` 记档保持），信标块本身 `light_emission` 0
- **材料补齐批 `[V2-7]`（v2.7.0）**：四子项全数据驱动（核心唯一新增 =
  块级 per-face `faces` 通用能力，**无 faces = 位级不变红线**），零 format
  （25 不动）/ 零 config 新键：
  - ① **planks 块化**（material → block，MC parity 缺口）：新块
    `planks`（FullCube 六面复用**图集既有 planks tile @ [2,4]** = 零新纹
    理；hardness 2.0 / drops 自身 / sound wood / flamable / **fuel_time
    7.5** 自退役物品迁入值不变 = wood.json 块属性先例）；`items/
    planks.json` 删（块物品按名自动注册，同名显式文件 = init 点名拒
    「duplicate item id 'planks'」）；8 个引用配方（bow/crafting_table/
    planks/stick/wooden_axe/wooden_pickaxe/wooden_shovel/fence）全按名解
    析 = 零改动（单测钉）；图标观感变 3D 方块体（记档偏离，wool 同名先
    例）
  - ② **gold_block / diamond_block**：iron_block 先例复制（hardness
    3.0/5.0 + 9 锭→1 块 shaped 配方 ×2 + 图集新 tile row-7 纯 +12 行零移
    位）+ **`beacon_base: true`** 两块入信标配基座集合 {iron, gold,
    diamond}（等级仍是 `world/beacon` 纯函数 3×3/5×5 连续完整层，零核心
    代码）；金/钻物品既有用途（金苹果/染料/弓族）不变
  - ③ **spider 掉落**：`string ×0-2` + `spider_eye ×0-2`（既有
    count_min/max 随机区间机制，MC 同款值）+ 2 新 material 物品（螺旋
    绳/紫青眼球 icon 新 salt + 严格 3D model box）——弓配方用 planks 不
    用 string → 两物品纯收集 = 记档偏离（不加配方 = 最小面）
  - ④ **块级 per-face 纹理**：`BlockModel.textures` 扩**可选 `faces`
    段**（front/back/left/right 覆写 side，**面外法线定名**——front =
    (0,0,−1) 共享 yaw-0 = −Z 约定；top/bottom 归既有键，faces 内命名
    top/bottom = 点名拒）；严格解析：未知键/非串/空串/cross·fence·
    glass_pane 形状全点名拒（自持几何段 = 死数据，ammo 纪律同款）；
    `cubeFaceTexture` 方向感知重载 + mesher 10 调用点转向（greedy merge
    纹理键 = 方向感知结果 → **per-face 覆写自动断并**）；掉落物立方体/
    破坏粒子立方体同款；**无 faces = 位级不变**（空串回退 side；「除
    furnace 外全块方向查找 = 旧查找」单测证明）；首个消费 = **furnace
    front 单 tile `furnace_front`**（宽炉口+余火，salt 961-965）；**炉子
    无 lit 纹理态 = 记档偏离**（lit 表现 = 既有炉口火焰粒子）；图集
    102/128、字体 423 零新字、既有 184 纹理 PNG md5 零变化（5 新 1 删普
    查实证）
- **物品与背包**（`[P11]`/`[P12]`，`Item`/`ItemStack`/`Inventory`，全数据
  驱动）：物品 = 纯数据（`ItemRegistry`，176 种 = 93 隐式 block item + 83
  显式：`type: "tool"` 带 `tool.break_speed` / `type: "material"` 通用非方块
  物品——锭/木棍/染料/桶/药水/护甲/弓等，作为配方原料/产物和方块掉落）；
  **背包 36 格** = 9 hotbar + 27 背包（**4 行** `[UX-8f]`，`kTotalSize`
  27→36 双击/shift/取半/数字键全 size 驱动零代码改动；format 24 尾部补 9 空槽
  值保真迁移）（`hotbar.json` `items` 定义初始 hotbar
  满堆栈 + `inventory` 段隐藏 starter 槽 16–26）；拾取确定性入包（先合并同物
  同耐久堆、再找空位，hotbar 优先）；破坏方块按 `properties.drops`（物品名，
  init 严格校验）+ `drops_count`（1..64 默认 1，须 ≤ 掉落物品 max_stack）+
  `chance_drop`（`[P49]` 单条目 `{item,count,chance}` 严格三元组，独立 roll =
  一次破坏最多两个掉落实体；`rollChanceDrop` 确定性纯函数——位置哈希 + 专用
  salt，同世界同格每次破坏同果）+ `chance_drops`（`[P52]` 多条目独立 roll，与
  `chance_drop` 互斥【同时写 = 解析拒绝】，逐条目点名 `chance_drops[i]`——
  grass = dirt + wheat_1 @ 0.125【MC 基础概率】、leaves = stick @ 0.02 +
  apple @ 1/128【`[P55]`】+ sapling @ 0.05【`[GEN-2]`】）掉落 `ItemEntity`
  ——矿石掉材料（iron_ore → iron_ingot，MC 同款；矿方块物品不能再从采矿
  获得 = 刻意），wood 掉 wood 本身（木板配方原料，改掉则链断）；**材料链单向**
  （sand → sandstone → smooth_sandstone 与 stone → cobblestone 均不可逆 =
  记档偏离）；**部分材料用途**：coal/planks/stick = 炉子燃料 + coal 进火把
  配方；gold/diamond = 金苹果/弓强化链/染料；**leather 仍无合成用途**（纯
  收集物，记档）

## 存档

- **入口**：`--save <slot>` opt-in（不带 = 不读不写，默认行为不变 + 基线截图
  不受影响；没有「上次世界」隐式恢复）；`--new` = 忽略现有存档开新档（退出
  时覆盖）；干净退出（含 `--frames` 到点）**自动写回**（写失败只记错误日志，
  不影响退出码）；**F5** 游戏内立即存档（`[P15]`，`--save` 槽激活时，日志
  记录结果）；**`[SAVE-1]` 周期性自动存档**：`save.auto_save_seconds` **代码
  默认 120.0**（**config.json 零改动** = fire/mobs/beacon 节同款模式；显式 0 =
  关闭，负数/非有限/非数字点名拒 = player.cpp config-reader 严格读取纪律）+
  **真实 dt 自动存档计时器**（`--save` 槽门控 = 无槽结构惰性；`save.
  autoSaveStep` 纯函数 campfire-tick 范式 interval ≤0 恒 inert；**成功归零/
  失败不清零下帧重试**无退避 = 记档简化；`Game::updateAutoSave` 在 update()
  帧末 World 单写窗口内 = F5 先例）+ **原子写**（`save.writeAtomicFile` 同
  目录 `.tmp` + flush/close + `std::filesystem::rename` POSIX 原子，**F5/退出/
  自动三路共用**增强；任一步失败删 tmp + 原 MC_FAIL 语义——**失败写永不毁旧
  档**，陈旧 tmp 复用清理；kill -9 中途实证 = 零 `.tmp` 残留，半写档不可达）+
  日志行可区分（`World auto-saved to` vs `World saved to`）
- **文件**：`data/saves/<slot>.json` = **seed + 与生成态的 diff + 完整游戏
  状态**——不重复存储地形，只存「和这个 seed 生成的世界不同的格子」（
  `blocks`/`fluids` 编辑列表，破坏 = `"air"`）+ 玩家全状态 + 昼夜时间 + **36
  格背包 `[UX-8f]`** + 选中格 + 掉落物/经验球实体（含 120 s 寿命与拾取延迟
  计时器，经验球 `[V2-11]`）
- **diff = journal 增量 `[PIPE-2]`（O(会话编辑数)）**：World 写收口位置集
  **journal**（observer，含位移陷阱双上报）+ 退出**按 journal 触及 chunk
  重算 fresh**、仅 live≠fresh 输出（**与 diffWorld 字节等价**，进程内 oracle =
  `--save-full-diff` 旗，legacy 路径保留）+ 楼梯扫描转 journal 派生（不变式 =
  worldgen 永不生成楼梯）；vd80 全环退出 6.2 s → **17 ms**（legacy 对照 3.28 s
  ≈ 190×；K = journal 触及 chunk 数，退出成本与环大小脱钩）；`[STRM-2]`
  retained 列 `mergeRetained` 合并（不变式：每列至多在 journal 或 retained
  之一 = 无重复格，oracle 不失真）
- **legacy 并行 diff（`--save-full-diff` / 读档重建路径）**：写档时逐 chunk
  重跑生成器比对——**>256 chunk 时每 chunk 一个 job 跑在临时 4-worker 池**
  （`[P37]`；输出按位置排序是全序【一格至多一个编辑】→ 并行结果与串行**逐
  字节相同**；小世界走单线程零开销；~9600-chunk 实测退出 diff 1.3 s → 0.35 s）
- **`[SAVE-3]` 槽位管理 CLI**（headless 四操作，管理跑不启动游戏 = 无窗口无
  世界无资产）：`--list-saves`（名序确定性列表 = 摘要纯读文件内容【format/
  seed/昼时/位置/血量】**无 mtime 无墙钟** = 列表体 ×2 逐字节一致；损坏文件
  浮现 UNREADABLE + 点名而非隐藏）+ `--copy-save S D`（**字节保真**分支存档 =
  MC Save Copy；D 已存在点名拒；原子写 = `[SAVE-1]` 同款失败零半写）+
  `--rename-save S D`（POSIX 原子改名）+ `--delete-save S`（点名）；槽名 =
  `--save` 字符集（字母/数字/_/-，天然杀路径穿越）；**管理旗标独占**（每跑
  至多一个 + 与游戏旗标互斥 = rc 2 点名）；**记档偏离**：无游戏内选档面板/
  会话内换档（游戏无标题画面，会话内载档 = 世界重建级架构变更矩阵外；多槽
  切换 = CLI `--save` 既有面；「保存并退出」= 既有 ESC/退出存档路径）
- **format 31**（`kSaveFormat` 31 / `kSaveFormatMin` 1；**1..31 全部可加载**，
  各按字段集 + 逐 format 白名单，规则式迁移——「未知字段 = 报错」
  rejectUnknownFields 保持；**31 = `[V3-11a]` 创造时代**：`player.
  creative` strict bool **恒写**（format 30 载 → false 迁移 / format 30
  携键 = version lie 点名 / 非 bool 点名——dimension 恒写先例；创造
  语义全在游戏侧，见下「创造模式」）；schema 其余与 30 一致（**30 =
  `[V3-1]` 下界维度时代**：`player.dimension` 严格词表（`overworld`|`nether`）**恒写**（29 缺 = overworld
  迁移 / 29 携 = version lie 点名 / 未知词点名）+ `nether` 段
  `{blocks, fluids, states}` **恒写**（空合法 = 下界 journal；nether 内
  stateful 块须 nether.states 条目）+ `nether_entities`（item kind 掉落物）
  **恒写**（空合法）+ **派生 seed 不落盘**（worldSeed ^ kSaltNether 由
  parse 重建 = 单源真值）+ **两维 portal 载入 rescan**（孤立 → air，双向
  交叉检）；schema 其余与 29 一致（**29 = `[V3-7]` gzip 时代**：schema 与
  28 完全一致，零键增删改，format 号仅标记写入构建开始默认 gzip 编码）；
  on-disk = 确定性 gzip 容器（固定 10B 头 `1f 8b 08 00 00 00 00 00 00 03`
  【MTIME=0/FLG=0/XFL=0/OS=3】+ raw deflate level 6 + CRC32/ISIZE 尾，
  同世界逐字节可复现）；加载按魔数 `1f 8b` 自动探测 = **旧明文档零迁移
  直读**（唯一编码判据，扩展名恒 .json）；解码严格（头 10B 逐字节全钉 /
  截断 <20B / 半体 / CRC·ISIZE 不符全点名带路径，系统 gzip CLI 产物点名
  拒收 = 机器生成文件纪律）；`save.compression: "gzip"|"none"` 严格读
  （absent = gzip 代码默认 = 11-04 用户定案 / "none" = 明文 = 旧行为位级
  恒【同状态文件与 v3.0.0 差异 = 恰 root.format 单键】/ 非 string·空串·
  未知词 init 点名拒）；写侧单 choke point（`Game::writeSave` exit/F5/
  autosave 共享，`writeAtomicFile` 原子性对 gzip 字节同样成立）；读侧两
  消费点（`Game::readSaveFile` + `--list-saves` 摘要，corrupt 槽 problem
  命名不隐藏）；save copy/rename/delete = 纯字节操作（gzip 槽 copy =
  字节忠实 gzip 槽）；实测压缩率门档 6.3× / 大档 5.4×；**25 = `[V2-3]`
  教程系列**：`player.tutorial`
  InProgress 形状加 `series` 整数字段（25 必需 / 23-24 携 = 版本谎报拒 /
  23-24 载入缺省 0 = 值保真免迁）；**26 = `[V2-11]` XP**：`player.xp` 恒写
  （单个整数池，等级永不存储由池派生）+ entities 第二 kind `type: "xp"`
  经验球（三向键纪律：pre-26 载 = xp 迁 0 / 25 携 player.xp 点名拒 / 26 缺
  键点名拒）；**27 = `[V2-12]` 附魔**：`player.xp_seed` 16 位小写 hex 串恒
  写（26→27 迁移 = worldSeed ^ kXpSeedInit）+ 物品 `enchants` {id: level}
  三处（四拒：26 携 xp_seed / 26 携 enchants / 27 缺 xp_seed / 交叉校验
  level>max·类不兼容·未知 id 全点名）；**28 = `[V2-13]` 床**：`player.
  respawn` int 三元组恒写（27 携键拒 / 28 缺键拒 / 27 载 = 哨兵 →
  applySave 覆写世界出生点；分量非整数 / y∉[0,128) 拒））：
  - **player**：pos / yaw / pitch / `health` / `absorption`（12+，[0,64]，
    ≤ 效果总和不变量）/ `hunger` / `saturation`（5+）/ `oxygen`（17+，恒写，
    [0,150]）/ **`velocity` 3 数组（22+，恒写，±100 `kSavedVelocityMax` 损坏
    档点名——`[SAVE-2]` 玩家速度入档：写只在退出存档一次性、读只在载档
    `restorePosition` 后 `setVelocity`（≤21 = 零速 legacy 值保真免迁）=
    零运行期成本；onGround 不存 = 位置派生记档）** / `effects`（8+ 名字序，
    `{name, remaining, tier?}` tier omit-when-0（20+））/ `armor`（16+，恒
    写，只含占位槽）/ `stats`（21+，恒写，空对象合法——deathCount +
    kill.<name> 封闭词表）/ **`tutorial`（23+，恒写 {id,count}/{}（25+ =
    {series,id,count}）——`[UX-7]`/`[V2-3]` 教程进度：series 越界或 id
    不在该系列 = 重置上报、count 越界损坏点名、≤22 读入免迁、23-24 载入
    series 缺省 0 值保真）** / **`xp`（26+，恒写，单个整数 = 累计经验总点
    数，等级永不存储 `[V2-11]`）** / **`xp_seed`（27+，恒写，16 位小写 hex
    串，成功附魔后推进 `[V2-12]`）** / **`respawn`（28+，恒写，int 三元组
    = 重生点，验床回退世界出生点 `[V2-13]`）** / **`dimension`（30+，
    恒写，严格词表 `overworld`|`nether` = 当前维度 `[V3-1]`）**
  - **inventory**：**36 槽**（`[UX-8f]`，24+ 写恒 36；per-format 尺寸检查
    **双向点名**——24 须恰 36 / ≤23 须恰 27，错计数 = "exactly 36 entries"
    式版本谎报拒绝；**23→24 迁移 = 尾部补 9 空槽**原 27 槽逐槽值保真
    `kPreUx8fInventorySlots` 命名常量）
  - **world**：`blocks`（diff，破坏 = air；**同格不得同时有方块 + 流体编辑**）/
    `fluids`（diff，format 9+ 带种类名 `[x,y,z,level,"water"|"lava"|"honey"]`，
    format 8 的 4 元素编辑加载时全迁移 water）/ `states`（15+，全量含默认值，
    双向交叉
    校验，14→15 值保真）/ `furnaces`（7+，每炉子 1 条目，双向交叉校验，6→7
    sole 归属迁移）/ `fires`（10+，{pos, remaining}，双向交叉校验）/
    `campfires`（13+，每家族块 1 条目恰 4 槽，双向交叉校验 + 每槽进度不变量，
    11/12→13 单槽迁移）/ `tables`（19+，恒写可空数组，双向交叉校验，18→19
    sole 归属迁移）/ `entities`（**kind `type: "item"` 掉落物** + **`type:
    "xp"` 经验球** {pos, age, pickup_delay, xp}（26+ `[V2-11]`），含 120 s
    寿命与拾取延迟计时器；
    **箭不入存档** = 保存守卫对已知瞬态种类跳过，其他未知种类仍报错）/
    **`nether`（30+，`{blocks, fluids, states}` 恒写空合法——下界维度
    journal `[V3-1]`，states 全量含默认值 = states 段同款纪律）+
    `nether_entities`（30+，item kind 恒写空合法——下界掉落物；mob 恒
    不入存档）**
  - **校验严格**：format 号 / seed 与生成 seed 一致 / 物品方块名已注册 / 数量
    ≤ max_stack / y ∈ [0,128) / 位置不重复 / **键纪律**（每 format 新 section
    键必需【空数组/对象合法】，低版本携新键 = 版本谎报拒绝——如 22 必 velocity
    / ≤21 携拒；19 必 tables 拒 table_grid / ≤18 必 table_grid 拒 tables）/
    双向交叉校验（container 块 ⟷ 状态条目逐格点名）
- **读档**：正常从 seed 生成世界 → 逐格应用编辑（**先方块后流体**）→ 全量重
  传播光照（`propagateAll`）→ 恢复玩家/背包/实体/时间（`restorePosition` 含
  `fallStartY` 重钉 `[P36]` + `setVelocity` `[SAVE-2]`；inventory ≤23 → 24
  尾部补 9 空槽值保真 `[UX-8f]`）→ per-block 状态重建
  （furnaces/campfires/tables = 段内条目，fires = 段内条目，crops/saplings/
  beacons = **rescan** 范式——tables 例外 = parse 时不变量保证每表必有 entry）
  → 光传播。演示区外编辑会带上「生成的 chunk」（读档后世界含全新世界没有的
  chunk，一致，记档）
- **时间语义**：昼夜 `time` = **固定步长**（位精确；`setTimeOfDay` 至多 1 ulp
  偏差，呈现无影响记档）；实体 `age` = **真实 dt 累积**（墙钟——含实体存档
  跨运行 age 差 ±1-2 帧，渲染不依赖 age，截图确定性不受影响）；effects
  remaining = 真实 dt 秒数（tick 相位不存 = 重载后下一 tick 从整 interval 起
  算）；JSON 浮点序列化 = **最短可往返精度**（位精确 round-trip，位置/时间/
  朝向加载后与原值位级一致）
- **已知不对称（未修，矩阵外）**：抽干的天然熔岩入档不完整、读档后「复活」
  （diff 对被抽干格记 level-0 水编辑、被抽掉的 kind 不记录——`[PRE-3]` 上报，
  见 KNOWN_ISSUES）

## 难度

- **4 值词汇表**（`[P25]` → `[ENT-8]`）：config `mobs.difficulty` = `easy` /
  `normal` / `hard` / `peaceful`（代码默认 `normal`，严格校验，未知值 init
  报错；**只 init 读取**——无运行期切换）；mob 数据是 **normal 基线**，其他
  难度按 `core/difficulty.hpp` 纯函数缩放（游戏与单测共享，F3 显示当前
  难度）：
  - ① **出生点保护环**——敌对 mob（`hostile: true` = 主世界
    zombie/skeleton/spider + **下界 magma_cube `[V3-3]`**）
    不在**世界出生点**（新世界的出生点，读档不改变它）`spawnProtectionRadius`
    块内自然生成（下界池同款环——MC 下界无保护 = 记档偏离，门 198）（**水平圆、不跟随玩家**——无垂直分量；`[V2-13]` 床重生点不
    改本环锚 = 世界出生点（MC 保护环亦锚世界出生点，与床无关）；村庄保护环
    未实现记档；MC 值：easy **8** / normal **12** / hard **16**）；
    被动 mob 不受保护环限制（MC 约定）
  - ② **敌对伤害缩放**——玩家受伤 = `attack_damage × hostileDamageScale`
    （easy **0.5** / normal **1.0** / hard **4/3** / peaceful **0.0**——zombie
    3 → easy 1.5 / normal 3 / hard 4，镜像 Minecraft 2.5/3/4 的相对形状）
  - ③ **生成率缩放**——尝试节奏 = `check_every_frames ÷
    hostileSpawnRateScale`（easy 0.5 / normal 1.0 / hard 1.5 → 30 → easy
    **60** / normal **30** / hard **20**；节奏为全部 mob 类型共享——`[P39]`
    起按类型权重（weight 1..10000）是正交维度：节奏 = 多久尝试，权重 = 尝试
    时抽中哪种）
- **`[ENT-8]` peaceful 难度**（消费 Phase-25 记档「无 Peaceful 难度」=
  **零新代码路径复用 Phase-25 机制**）：`spawnProtectionRadius(Peaceful) =
  kSpawnProtectionPeaceful` **1e6** = 保护圈覆盖全球 → **敌对自然生成全拒** +
  **被动豁免照生**（MC 和平核心）；damageScale 0.0 / rateScale 1.0 = API
  一致性值（无宿主消费）；game.cpp 零改动。**偏离记档**：无「现有敌对消亡」
  （mobs ephemeral + difficulty 只 init 读取 = 读档后本无敌对）/ 无自动回血 /
  保护锚 = 世界出生点

## 确定性约定

- **硬规则 8：帧锁定 vs real-dt 分类**（无输入字节复现的核心；生存计时器纪律
  一致）：
  - **帧锁定（固定 1/60 帧步长）**：昼夜时间（`kDayStepPerFrame`）/ mob 模拟
    （`kMobStepPerFrame`）/ 箭（`kProjectileStepPerFrame`）/ 粒子发射 / 流体
    tick（3 帧节奏 + 蜂蜜 1/3 门）/ 炉子/营火/作物/树苗子步（整数子步倒计时）/
    火蔓延 roll（固定帧步纯函数）/ 氧气体积（帧计数 6/150）/ mob 整数帧燃烧
    格 / swing 相位 / 流送填充（2 列/帧）/ 光照 job 预算（2+2/帧）/ 环境音
    roll（300 帧）/ 云漂移（帧函数）/ 水波纹（帧 mod 120）/ 教程状态机
    （`[UX-7]` 纯函数步进，零 real-dt）/ 经验球磁力拉取 `[V2-11]` / 羊毛再生
    7200 f 倒计时 + 出生毛色流首抽 `[V2-15]` / 蝙蝠悬停巡航（正弦 bob +
    航向 2 s 重掷 + ambient roll）`[V2-14]` / 天气状态机（CLEAR/RAIN/雷 帧
    周期 + 推进式流）`[V2-4]` / 拾取磁力（固定 `kMobStepPerFrame` 步长）
    `[V2-6]` / 月相（累计天 index 帧函数）`[V2-6]` / 传送门站入倒计时
    60 帧 + 传送冷却 900 帧 `[V3-1]` / 下界 ambience roll（300 帧同节
    奏）`[V3-2 Part 2]` / demo 回放 dt 序列 + 倍率帧边界切换 + 0.5×
    render-only 保持 `[V3-9]`
  - **real-dt（真实墙钟，生存计时器类）**：饥饿衰减/星饥伤害/自然回复 / 进食
    饮用通道 / 破坏进度 / 弓蓄力 / 坠落伤害结算（fallStartY 记录）/ 熔岩灼烧 /
    火块燃尽（m_fires remaining）/ 效果 remaining 与 tick 累加 / 实体 age /
    物品拾取延迟 / 自动存档计时器 / 双击窗（0.3 s，含飞行 toggle `[V2-19]`）
    / 粒子运动
  - **音频不入帧状态**（事件驱动，截图/存档零影响）
  - **惰性求值与全整式纪律（二批）**：附魔 offer = **仅面板打开时求值**
    （无面板零成本 = 结构免疫根，`[V2-12]`）+ **全整式禁 double**（-O3 融合
    风险；种子单推进流 = PITS 30 纪律）；XP 等级 = 由整数池**派生永不存
    储**（`[V2-11]` 单源真值）；unbreaking 掷骰 = **伤害史确定**（state =
    worldSeed ^ damageBefore×golden ^ salt ^ itemRegistryId，无帧依赖，
    `[V2-12]`）；sheep 出生毛色 = per-mob 流**首抽**、bat bob 初相 = 构造
    流首抽（**flying/wool 才消费，其余 mob 零消费 = 位级红线**，
    `[V2-14]`/`[V2-15]`）
- **无输入运行逐字节可复现**：截图只在**帧边界**字节一致；**202 次无输入基线
  门**（新 ×2 RAW 字节一致 + vs `/tmp/opencode/base19.png` 字节一致，或论证
  「无输入下零活动」= **结构性免疫**免重锚【变更在无输入路径永不执行——如
  炉子永不生成 = m_furnaces 空双结构惰性 / 无掉落 = 零 item draw / 无效果 =
  count 0 fast-exit / 音频 = 无事件 / 3.x 门 191–199 = 第 72–80 次 + 门
  201–202 = 第 81–82 次免重锚（对 base19）】，或**重锚记档**【渲染基线本
  身变更——
  LGT-2 门 83 / PRE-4 门 111 云入基线 / PRE-5 门 112 cutout 入基线 / **UX-9a
  门 168 云 shader 化（168140 → 179116 B）** / 重锚谱系 179116 →
  178148【V2-10 门 181 叶冠剔除】→ 178676【V2-19 门 182 时钟】→ 179027
  【V2-11 门 183 XP 条】→ 224468【V2-18 门 190 AO 世界光照】→ **225272
  【V3-6 门 200 填充感知雾】**】；
  base19
  锚随环境态（WM 平铺/机器态）刷新有先例 = PITS 265/315 类，非回归）
- **指针锁定协议**（硬规则 5）：yaw 0 = -Z、正 = 左转；pitch 正 = 上、clamp
  ±~89°；**鼠标转向仅在指针锁定时生效**（无输入运行字节复现的保证；物理光标
  未锁定时 SDL 仍投递 mouse motion = 无输入 E2E 必须零 :0 活动，红心检查 =
  污染哨兵）
- **种子与盐纪律**（PITS #4）：splitmix64 是纯 finalizer——**推进式流必须
  `state += 0x9E3779B97F4A7C15ULL` 后再 mix**（不推进 = 同 burst 同值缺陷）；
  每消费者**专用 salt**（洞/树/装饰/矿/湖/生长/蔓延/余烬/环境音/床岩 roll
  各自去相关）+ **位置哈希**（同帧不同格去相关）+ **帧推进**（同格不同帧去
  相关）；同种子同帧同格 = 同结果
- **固定帧步长的代价**：非 60Hz 显示器上帧锁定系统（昼夜/mob/箭/流体/粒子
  发射）速度随帧钟成比例偏差（120 Hz = 2× 速，记档）；real-dt 系统 fps
  无关（墙钟稳定）
- **存档加载 = 确定性重放**：seed 生成 + 编辑应用 + 全量光照重传播；JSON 浮点
  最短可往返精度（位精确）；worldgen 演进 = 旧存档未编辑列按**当前**生成器
  重建（洞穴/矿/树/基岩/路径演进先例：P86/88/93、GEN-1..8——编辑权威、零
  format）
- **质量门**：**2777/2777 单元测试**（`./build/bin/tokenmine_tests`）+
  `-Wall -Wextra -Wpedantic -Werror` **0 警告**；机制逻辑一律上提**纯函数**
  （campfire 范式：`Result<T>` 显式传播 + 游戏与单测共享同一文件），像素/
  存档证据 = E2E 门（`docs/PROTOCOL.md` E2E 协议）；**禁止** TODO/伪代码/魔法
  数字/硬编码方块检查/删功能过构建/禁用测试（硬规则 4）
- **性能审计系列 `[99.x]` 要点**（v1.0 前完成，逐项过门）：99.2 流送内存 +
  帧预算（卸载 + RSS 平台）/ 99.4 lit pop-in / 99.5 水下覆盖层 / 99.6 日志 +
  标题 FPS / 99.7 mob 四肢按原版几何重做 / **99.8 流体活动门控 + 流送 2 列/
  帧**（chunk 粒度 → `[PIPE-3]` per-cell 演进）/ **99.9 视锥裁剪 + 列合并
  渲染 + 光照 chunk 级重构**（vd80 2-30 FPS 坍缩修复）/ **99.10 光照 job
  并行**（`lightRegionsIndependent` gap≥4 位级独立判据 + dirtyOut 解共享写
  竞态，流送填充期 flush 下限 16.7 → ~10 ms）/ **99.11 流体光照异步化**
  （深层卡顿根因 = 流体 tick 巨盒主线程同步 re-light：海岸线 churn 700-1400
  格/tick → 661 s/300f 窗；列粒度待集 + 每帧 2 列预算 worker 追赶 + 幂等
  收敛；vd16 填充 244.7 → 44.6 s【5.5×】、update p95 7.6 s → 80 ms）
