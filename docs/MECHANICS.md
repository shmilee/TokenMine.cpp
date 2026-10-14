# 当前可玩内容（Phase 28）——机制详解

> Phase 30 文档整理：从 `README.md` 移入（未改写）。Phase 29 只改粒子外观（同 burst
> 16 个分散小立方体，见 `docs/archive/phase-29.md`），其余机制描述不变。文中
> `KNOWN_ISSUES` 均指 `docs/KNOWN_ISSUES.md`。

- **第一人称玩家**：`W/A/S/D` 行走，`Space` 跳跃（仅贴地时）/游泳时随时上浮，
  `LeftShift` 冲刺，鼠标转向（**左键点击锁定指针；转向仅在指针锁定时生效**，
  未锁定的光标不会转动视角——保证无输入运行下截图可逐字节复现）
- **生存系统**（Phase 16 + 23，Phase 28 饱和度/进食动画，Phase 32 参数可配置，`Player` 扩展）：
  下文所有数值默认值均可在 `config/config.json` 的 `player.*` 节覆盖（缺键 =
  默认值，类型错/越界 = 启动报错；全键表见 `docs/CONTENT_GUIDE.md`）：
  - **生命值 / 受伤 / 死亡**：`maxHealth=20`（10 颗心，每颗 2 HP）；
    **坠落伤害** = `max(0, 净下落格数 − 3)` HP（从离地帧记录 `fallStartY`，
    落地时结算；落入水中的落地取消伤害；**读档经 `restorePosition` 重钉
     `fallStartY` 到恢复的 Y**（Phase 36——空中读档没有"离地"转换，不重钉则
     保存的坠落满血落地））；掉出世界（y < −32）= 直接死亡
    （`damage(maxHealth×1000)`）。死亡后冻结玩家、显示 "You died! / Press R"，
    流式加载同时暂停（Phase 33：见世界生成节——不暂停的话死亡画面背后会按
    `view_distance` 继续填满半径方，大半径下 = 数小时 CPU + 数 GB 内存），
    `R` 重生 = 回出生点 + 满血 + **满饥饿** + **满饱和度**（= 一顿饱饭）
    + 速度清零（**背包保留**）
  - **自然回复**：脱离受伤状态 `regenDelay=10s` 后，每 `regenInterval=4s` 回 1 HP（与 Phase 46 的 `regeneration` 状态效果
     **独立并存**，各自计时）；
    **饥饿门控**（Phase 23，Minecraft 规则：饥饿 < 6 无自然回复）——门控关闭时
    累加器暂停（不重置），吃饱后继续
  - **饥饿**（Phase 23，Phase 28，Phase 31，`maxHunger=20`，10 只鸡腿每只 2 点）：
    每 `hungerDecay=36s` 一个衰减 tick（Minecraft 被动节奏：1 点 = 720 tick
    = 36s，Phase 31）——**先扣 1 点隐藏饱和度**（Phase 28，
    缓冲耗尽才动饥饿条；饱和度上限 = `maxHunger`），饱和度 = 0 时才衰减
    1 点饥饿条；**疾跑**（`Shift` + 移动、非水中——与选 `sprintSpeed` 的判定
    同一条件）时 tick 速率 ×2（`sprintHungerDecayFactor` = 18s/点，Minecraft
    36s→18s 的相对节奏）；**饥饿 = 0** 时每 `starveInterval=20s` 受 1 点
    饥饿伤害（Minecraft normal：1 HP = 400 tick = 20s，Phase 31）
    （走正常伤害路径 → 红闪/音效/死亡全套复用）；**吃食物**（Phase 28 进食
    通道，`core/food.hpp` 纯状态机）：选中食物时**按住右键 1.6s**（Minecraft
    进食时长 `kEatDurationSeconds`）= 吃——完成时回复该物品 `restore_hunger`
    数据点饥饿 + **2× 饱和度**（`kSaturationFactor`，Minecraft 熟食 ≈2× 经验
    法则）、扣 1 个库存、播放 "eat"；**提前松开右键 = 取消**（不消耗、无声
    音）；受击/死亡/解锁鼠标/开面板/切换选中格/左键挥击同样取消；进食中
    3D 手部随进度抬向面部（Phase 42 3D 手持手部；2D 手持图标固定不拉）；
    进食中再按右键 = 忽略（进食继续）；不再放置/交互；
    **满饥饿时右键食物为 no-op 但仍吞掉该次右键**（Minecraft：满条时不能
    放置）
  - **食物数据**（Phase 23，Phase 28 扩充，Phase 40 熟肉）：物品可选字段
    `restore_hunger`（整数 [1, 20]；缺省 = 非食物，显式 0 拒绝）——当前
    7 种食物：生 `rotten_flesh`（=4，zombie 掉落）、`beef`（=3，cow 掉落）、
    `mutton`（=2，sheep 掉落）、`chicken`（=2，chicken 掉落）；熟
    `cooked_beef`（=8）、`cooked_mutton`（=6）、`cooked_chicken`（=6）
    （炉子熔炼对应生肉得到，见「熔炼」）
  - **受伤反馈**：受击瞬间屏幕红色闪光（`0.35×剩余/0.5s` 渐隐）+ 播放 "hurt"；
    死亡播放 "death"。游戏轮询生命值（`m_prevHealth`）驱动，致死伤不重复播放
  - **潜行**（`LeftControl`）：身高 1.8→1.6（脚底不动）、眼高 1.62→1.42、
    移动速度 ×0.3（水中除外）；潜行时**不**触发 1 格自动台阶
  - **1 格自动台阶（step-up）**：非潜行 + 贴地 + 身体贴住整数块面 + 面前脚位
    为 solid + 其上方 2 格为空 → 自动抬升 1 格并继续本 tick 的水平移动
    （无需跳跃即可爬上 1 格台阶，2 格仍不行）
  - **玩家 ↔ 掉落物分离**：玩家与物品实体不再互相穿过——沿最小穿透轴把
    物品推开（远离玩家中心，面到面跨度）；玩家自身不被动位移
  - **生命 / 饥饿 HUD**：hotbar 上方 10 颗心（16px、2px 间隔）+ 心带右侧同行
    10 只鸡腿（带间 6px 间距），各三态贴图（满 / 半 / 空）取自 48×16 条带
    （`assets/textures/health.png` / `hunger.png`，
    `tools/generate_health_icons.py` / `generate_hunger_icons.py`）；
    填充 = `clamp(value − 2i, 0, 2)`（半图标按 0.5 粒度渲染）。只在游玩态
    （锁定 + 背包未开）显示，无输入运行零新增绘制
- **Mob 与 AI**（Phase 20，Phase 22/26 被动 mob，Phase 25 难度系统，
  Phase 27 skeleton 远程敌对，`MobEntity`/`MobRegistry`，全数据驱动；
  数据含 zombie/skeleton（敌对）+ cow/sheep/chicken（被动））：
  - **生成**：帧节奏驱动（`data/mob_spawning.json`：每 `check_every_frames` 帧尝试
    一次、`first_spawn_frame` 之前不生成、`max_mobs` 上限）；生成格 = 玩家列周围
    `[min_radius, max_radius]` 环内按固定扫描序的第一个合法格（固体地板 + 头顶
    空余 + 地板高度差不超过 2 格 + 不压玩家 + 与其他 mob 水平间隔 ≥1 块 +
     **光照门**）——
    纯函数，同一运行必生成同一只
   - **光照门（昼夜生成规则，Phase 20.5）**：生成格（脚位）的**当前有效光照**
     = `floor(天空光 × dayFactor)` 与方块光（萤石，不受昼夜影响）取较大者，
     必须 ≤ 该 mob 的 `spawn_max_light`（mob 数据字段，0..15，默认 0 = 只在
     黑暗生成——Minecraft 敌对 mob 约定）。dayFactor 是渲染层昼夜系数（正午
     1、深夜 0、晨昏平滑过渡），所以 zombie 白天（开阔地天空光 15 → 有效
     15 > 0）不生成、夜晚生成；cow/sheep/chicken（`spawn_max_light` 15）
     任何光照/时间都可生成（被动 mob 约定）；萤石照亮处即使夜晚也不生成
     zombie；树冠/
     洞穴的永久暗格（存储天空光 0）白天也可生成（与 Minecraft 一致）。
     类型在每次尝试时按数据名单确定性抽取（种子 ⊕ 帧号）：1 次流值映射进
     [0, Σweight) 后按累计权重走名单序（Phase 39：`mobs` 条目 = `{"id",
     "weight"?}`，weight 1..10000 缺省 1 = 相对生成概率；全 weight 1 与
     旧 `pick % 名单长度` 逐位相同），抽中的类型过不了光门则该次尝试作废
     （如白天抽中 zombie）
  - **AI 状态机**（确定性：种子 = 世界种子 ⊕ 生成序号，splitmix64 流）：
    **Wander**（默认；每 `wander_interval` 秒一次种子随机决策：1/3 站定、
    其余半速随机方向走）→ **Chase**（玩家进入 `detect_radius`（水平）且高度差
    ≤2：全速追）→ **Attack**（水平距离 ≤`attack_range` 且高度差 ≤1：停下、
    面向玩家、每 `attack_cooldown` 秒打 `attack_damage` 点——走 P16 的玩家
    受伤路径：红闪/音效/死亡全由既有轮询驱动）。玩家死亡时不追不打。
    **RangedAttack**（Phase 27，数据 `shoot_range > 0` 的 mob = skeleton）：
    玩家进入 `shoot_range`（水平）→ 面向玩家、每 `attackCooldown` 秒经
    shoot 回调放箭（Phase 38：先过 LOS 门——眼→玩家眼被非空气格挡住则
    不发（箭反正只会插墙消失），冷却照常消耗、下个间隔重试；游戏在 mob
    头部生成带 `hurtPlayer` 标记的箭，瞄点 = `computeAimDirection` = 玩家
    位置 + 速度×飞行时间迭代 2 次预判 + 0.5·g·t² 弧线提前量——静止玩家
    （速度 0）与 Phase 27 瞄法逐位相同，移动玩家被预判；箭被扫掠
    命中玩家 AABB 时走 P16 受伤路径）；玩家贴近 `retreat_distance` 内 →
    全速后撤（边退边射，Minecraft kite）；超过 `shoot_range` 但仍在
    `detect_radius` 内 → 按 Chase 逼近。远程 mob 的 `attack_range` 是
    （不使用的）近战距离，skeleton 设 0 = 永不近战。
    **被动 mob**（Phase 22 落地、Phase 26 扩到 sheep/chicken）：`detect_radius`/
    `attack_range`/`attack_damage` 三个字段允许 0（校验 `>= 0`，负数仍拒绝）——0 值
    使追/击条件永不成立，被动 mob 只有 Wander 状态，纯数据表达，AI 代码无特判
  - **玩家攻击**：左键命中 mob（射线先比方块距离——**方块遮挡**：方块更近则打
    方块）= 挥击，按住时每 `0.25s` 一击：伤害取**选中物品的 `attack_damage`**
    数据（徒手/方块/材料=1.0，工具见 `data/items/*.json`，木镐 2 / 铁镐 3）− 活跃虚弱效果 `melee_penalty` 总和（Phase 46，
    clamp ≥0）+
    沿瞄准方向击退（水平 4 + 抬升 2.5，0.3s 击退窗口内 AI 不接管速度）
  - **死亡**：HP 归零 → 掉落 `drops` 数据定义的**物品列表**（Phase 28：
    每条目 `{item, count}`；Phase 38：或 `{item, count_min, count_max}`
    随机区间——死亡时按列表顺序用该 mob 的种子流掷一次，0 = 该条目不掉），
    按列表顺序各掉一件 ItemEntity（zombie 掉 `rotten_flesh`，skeleton 掉
    `bone` 0–2 + `arrow`，cow 掉 `leather`+`beef`，sheep 掉
    `wool`+`mutton`，chicken 掉 `feather`+`chicken`——均为 material 物品
    + 图标）后移除；击退/坠落无伤害（v1）
  - **渲染**：box 模型（zombie = 头 0.5³ + 身 0.5×1.0×0.3；skeleton 同
    zombie 尺寸（头 0.5³ + 身 0.5×1.0×0.3、AABB 0.6×1.8）；cow = 头
    0.45³ + 身 0.8×0.7×1.1、AABB 1.2×1.5 比 zombie 宽；sheep = 头 0.4³ + 身
    0.8×0.8×1.2、AABB 同 cow；chicken = 头 0.25³ + 身 0.3×0.4×0.5、AABB
    0.5×0.8 比 zombie 小，各 6 面同一 atlas tile，
    `tools/generate_mob_textures.py` 生成 zombie 绿皮肤+深色眼/青色上衣、
    skeleton 骨白+深色眼窝/鼻腔、cow 棕底白斑毛+深色眼、sheep 白绒毛+
    深色眼、chicken 米白羽+深色眼+红冠+橙喙；atlas 29 tile），
    与掉落物同 pass（`translate(位置) × rotateY(朝向)`，朝向 = 相机 yaw 约定，
    箱体的 −Z 面是脸）；天空光全亮烘焙（与掉落物同一简化，无局部光）
  - **开关**：config `mobs.enabled`（默认 true）；**mob 不入存档**（与 Minecraft
    一致：生成物是瞬态的，其掉落物是物品、随存档保存）
  - **难度（Phase 25）**：config `mobs.difficulty` = `easy` / `normal` /
    `hard`（默认 `normal`，严格校验，未知值 init 报错）。mob 数据是
    **normal 基线**，其他难度按 `core/difficulty.hpp` 纯函数缩放（游戏与
    单测共享，F3 显示当前难度）：
    ① **出生点保护环**——敌对 mob（数据字段 `hostile: true`，当前仅
    zombie）不在**世界出生点**（新世界的出生点，读档不改变它）
    `spawnProtectionRadius` 块内自然生成（Minecraft 值：easy 8 /
    normal 12 / hard 16，水平距离）；被动 mob 不受保护环限制
    （Minecraft 约定）；
    ② **敌对伤害缩放**——玩家受伤 = `attack_damage × hostileDamageScale`
    （zombie 3 → easy 1.5 / normal 3 / hard 4，镜像 Minecraft 2.5/3/4 的
    相对形状）；
    ③ **生成率缩放**——尝试节奏 = `check_every_frames ÷
    hostileSpawnRateScale`（30 → easy 60 / normal 30 / hard 20；节奏为全部
    mob 类型共享，v1 非按类型权重）。`normal` = 三个缩放全 1.0 =
    基线行为不变
- **弓与箭**（Phase 21 发射，Phase 24 蓄力/箭物品/插墙残箭/近战，
  Phase 42 3D 手部/插墙箭 100s/释放/拾取/击退随蓄力；
  物品数据驱动 + P10 `Projectile`）：
  - **获取**：shaped 配方 `bow`（工作台：3 stick + 1 planks 的 C 形，
    `data/recipes/bow.json`）；material 物品 `bow`（`data/items/bow.json`，
    `max_stack 1`，图标 `bow.png`，`ammo: arrow`，`attack_damage: 1`）；
    **箭 = material 物品 `arrow`**（`max_stack 64`，图标 `arrow.png`）+
    shapeless 配方 **flint + stick + feather → 4 arrow**（`data/recipes/
    arrow.json`，Phase 60，MC 1.14+；**替换** P24 的 2 stick 竖排 → 2 =
    内容契约变更：早期箭现需燧石 + 羽毛，MC 同款闭环）
  - **发射（蓄力）**：选中 `shoot_speed > 0` 的物品（发射器）时**右键 =
    拉弓**——按下即开始蓄力（该次右键被吞：不交互工作台/不放置方块；
    需有弹药才能起拉；播放 "bow" 弦音），按住累加拉弓时间（真实 dt，封顶
    `kBowFullDrawSeconds = 1.6s`），**松键射出**：`power =
    clamp(拉弓/1.6, 0.1, 1)`（`core/bow.hpp` 纯函数，游戏与单测共享）——
    速度 = `shoot_speed × power`、伤害 = `shoot_damage × power`；满蓄力 =
    物品数据满值（bow = 15 / 5，MC 满威力），快速点击 = 最低威力 0.1；
    箭从眼前 0.3 格沿瞄准方向出发，播放 "shoot"。蓄力状态在解锁/开面板/
    换热栏格/重生时清零。**左键 = 弓近战**：准星命中 mob 时按物品
    `attack_damage` 数据（bow = 1）伤害 + 击退（0.25s 挥击节奏），
    **永不挖掘**（弓是武器不是工具）
  - **弹药**：发射消耗 1 个发射器 `ammo` 数据字段点名的物品（**槽序升序
    第一个持有它的槽位**）；无弹药不能起拉，拉弓中途耗尽则松键取消
    （无箭不射）；`ammo` 缺席的发射器 = 无限箭（Phase 21 前语义），
    字段名在 init 时经 ItemRegistry 交叉校验
  - **箭**（P10 `Projectile` 实体）：重力 20 b/s²（MC 箭 0.05/格/tick²）、
    终端 38、**撞 solid = 插住**（stuck：留在命中处、保留撞击方向
    `stuckDir` 供渲染——碰撞解算器会清零速度的碰撞轴，`stuckDir` 在
    子步前捕获）、**插墙箭寿命 100s**（`kStuckMaxAge`，MC 值；飞行箭
    仍 30s `kMaxAge`；age 累计、按当前状态取上限——插墙 >30s 后释放的
    箭立即消亡）、世界下方杀界；**固定帧步长** `kProjectileStepPerFrame`
    （运动实体截图可复现约定）；插住后无物理、不参与 mob 命中测试；
    **卡住格被破坏/替换时释放**（`Game::setBlock` 收口：破坏/放置/染色
    换块都走它；判定 = 箭盒触及该格面且中心在面外——resolver 精确
    clamp 到面上，盒不进入块内）：箭以零速度落回飞行物理（重力 + 碰撞
    + 可再命中 mob）
  - **箭矢命中**：每帧对每支**飞行中**的箭做**扫掠 AABB 测试**（上一步
    位置→本步位置，`raycastAabb` 位移为方向、t∈[0,1]）对每个存活 mob——
    命中则按箭携带的 `shoot_damage × power`（满威力 bow = 5，zombie 4 箭毙）
    伤害 + **随蓄力的击退**（`core/bow.hpp arrowKnockback` 纯函数：沿箭
    水平速度方向 `4.0 × power` 水平冲量 + `2.5 × power` 抬升——满威力
    = 旧近战常数逐位相同，半蓄力减半），箭被消耗；箭**穿过**玩家与其他实体
    （`collidesWithEntities = false`），不伤玩家（单人生存 v1）
  - **插墙箭拾取**（Phase 42）：插墙箭距玩家**底心** ≤ 1.0 块即拾取——
    返还 1 个发射器 `ammo` 数据点名的物品（飞行箭不可拾取；背包满 = 箭
    留墙内、每帧重试）；mob 箭返还该 mob 数据的 `ammo` 字段（ranged mob
    必填、init 校验物品存在——skeleton = `arrow`）；箭不落地成掉落物
  - **渲染**：细盒体（长 0.55 × 粗 0.05，本地 −Z = 飞行方向）纯色
    （基本着色器 `u_useTexture=0` 路径，无箭纹理），朝向 = `rotateY(yaw) ×
    rotateX(pitch)` 把本地 −Z 转到速度（插墙箭用 `stuckDir`；相机同款约定，
    `arrowOrientation` 纯函数）；天空光全亮烘焙（随昼夜变暗）
  - **3D 手持手部**（Phase 42，`drawHeldHand`）：指针锁定且无面板时，选中
    物品渲染为视口锚定的 3D 模型——相机系偏移 (0.42, −0.38, −0.85)（右下方
    前方）、固定自旋（Ry 0.6 + Rx −0.35 露出顶面/侧面）；**块物品** = 掉落物
    同款 3D 方块体（atlas 光照，随昼夜变暗）、**图标物品** = 2D 图标贴 3D
    平面（共享单位 quad，`blockLight = 1` 恒亮——顶点法线取 +Y 保持
    faceShade = 1.0 桶，几何 +Z 法线会落入 0.6 桶变暗 61%）；**拉弓**时
    手部随蓄力后拉 0.3 格 + 弓体后仰 0.9 rad（拉弓指示，替代 Phase 24 的
    2D 图标拉移——2D 手持图标改固定）、**进食**时随进度抬向面部 0.35 格
    （进食指示，替代 Phase 28 的 2D 拉移）；图标透明像素需开 blending
    （不透明段默认关，否则渲成不透明黑方）；无弓弦模型/拉弓进度条/手部
    皮肤；仅锁定时渲染 → 无输入基线结构性免疫
  - **v1 简化**（docs/KNOWN_ISSUES.md）：箭不入存档（瞬态，同 mob）、不伤
    玩家/无自伤、无箭纹理、非块手部 = 平面图标 billboard（无 per-item 3D
    模型）、释放箭共享累计寿命（上限按当前状态切换）
- **合成**（Phase 17，`CraftingGrid`/shaped 配方/工作台，全数据驱动）：
  - **形状配方**：配方 JSON 加 `"type": "shaped"` + `pattern`（行字符串，空格=空位）
    + `key`（字母→物品）；无 `type` 字段 = shapeless 原料袋（Phase 12 语义）。
    3×3 网格匹配 = pattern 偏移扫描（pattern 的每个可能左上偏移都试一遍，空格
    对应的格必须为空）；匹配顺序先 shaped 后 shapeless，配方按文件名字典序
  - **工作台**：数据驱动方块（`"interact": "crafting_table"` 属性，破坏掉自身）；
    **锁定时右键命中它 = 打开工作台面板**（3×3 网格 + 输出格，无配方列表），
    不放置方块；网格内容容器语义（关面板保留、入存档）
  - **E 背包面板**：2×2 合成格 = 共享 3×3 网格模型的左上 4 格（放不进 shaped
    配方）；右侧配方列表**只显示 shapeless 配方**（shaped 必须用工作台）
  - **stone→cobblestone 材料链**（Phase 61，全数据）：`stone.json`
    `drops: "cobblestone"`（挖天然石得锻石，MC 材料循环）；配方
    `stone`（shapeless 4 cobblestone → 1 stone = MC 1.14+ 的回程）。
    **cobblestone 不能熔炼回 stone**——`smeltable_to` 是物品字段而
    block item 无独立 JSON（文档化偏离，见 KNOWN_ISSUES）
- **熔炼 / 炉子**（Phase 40，全数据驱动；per-block 状态 Phase 44）：
  - **炉子方块**：数据驱动方块（`"interact": "furnace"` 属性，破坏掉自身）；
    **锁定时右键命中它 = 打开该炉子的面板**（27 格背包 + input/进度条/output
    + fuel 槽，Minecraft 排布；无任何合成动作——output 自己填满，点击取走）。
    **每个炉子方块有独立的熔炼状态**（Phase 44：`Game` 持
    `std::map<BlockPos, FurnaceState>`，位置序 → 逐帧 tick 迭代确定；放置时
    建空态、方块被换/破时删态——`Game::setBlock` 收口；面板 UI 打开时绑定到
    所点方块的状态，面板期间块交互暂停 → 绑定态稳定）。各炉独立点火/熔炼，
    互不干扰（A 满 output 不会暂停 B）
  - **可熔炼**：物品可选字段 `smeltable_to`（产物物品名，init 时交叉校验）
    标记可熔炼 + `smelt_time`（秒，(0,100]，缺省 10 = MC 炉子时长）；
    当前：beef→cooked_beef、mutton→cooked_mutton、chicken→cooked_chicken
  - **燃料**：物品可选字段 `fuel_time`（秒，(0,1000]；显式 0 拒绝）；
    当前：coal 80 s、planks 7.5 s、stick 5 s（MC 同款数值）
  - **规则**：有燃料库存时炉火常亮（燃尽当 tick 立即点下一根，整段燃烧
    时间授予该 tick）；烧着 + 输入可熔炼 + output 有空（空或同产物未满）
    才推进进度；进度满 `smelt_time` → 输入 −1、output +1；**output 满 =
    暂停进度但火继续烧**；**输入物品变更（含取空/换非熔炼物）= 进度清零**
  - **计时**：每状态每帧固定 1/60 子步 + **整数子步倒计时**（与昼夜同类：
    无输入运行逐字节可复现；`std::map` 位置序 → 多炉 tick 顺序确定）
  - **存档**：format 7 `furnaces` 数组——**每炉子块一条目**（`pos` + 三槽 +
    整数 `progress_substeps` / `fuel_substeps`），位置排序，与方块差集
    **双向**交叉校验（entry 无块 / 块无 entry 均拒绝）。format 6（单个全局
    `furnace`）仍可加载：恰 1 个炉子块 → 状态归属它，0/多个 → 丢弃
    （见 KNOWN_ISSUES）
 - **营火 / 烤食物**（Phase 71，数据驱动 + per-block 状态，炉子 P44 范式）：
   - **营火方块**：`"interact": "campfire"` 属性（第三交互词汇表值）、cross
     模型、光 **15**、hardness 0.5、**破坏掉自己**（可复用）、非 flamable
     （是火源外观，不是可燃物）；配方 shaped `SSS/LCL/LLL`（3 木棍 +
     5 wood 块 + 1 coal = MC 营火配方）
   - **单槽烤制**：每营火块一个 `CampfireCookState`（`std::map<BlockPos,
     CampfireCookState>`，setBlock 收口创建/删除，位置序 = 确定性 tick 序）；
     **槽至多 1 个物品**——手持**可熔炼**物品（`smeltable_to` 非空 =
     数据驱动门）右键空槽 = 放上 1 个（消耗 1，"place" 音）；**空手**右键 =
     取回（生熟均可，"pickup" 音，MC 空手规则）；其余组合 no-op。右键链中
     营火分支**先于进食通道**（MC 语义：手持食物右键营火 = 放上火上，不开
     吃——项目全部可熔炼物品都是食物，无此前置则烤槽永远填不上；炉子/工作台
     保留既有进食优先行为）
   - **计时**：每状态每帧固定 1/60 子步 + 整数 `progressSubsteps`（与炉子
     同类：无输入运行逐字节可复现）；速度 = **炉子节奏**（复用物品
     `smelt_time` 数据、`smeltSubstepsFor`——无独立 config）；**无燃料**
     （营火自燃 = v1 文档化偏离）；产物非可熔炼 → 熟品在槽中静置
   - **烤制中渲染**：槽非空时物品图标以 0.5 大小交叉 quad 悬浮在格上半部
     （cross 块惯例：视角无关、opaque pass、深度遮挡；+Y 假法线 = 顶面亮度
     档）
   - **座位**：crouch（LeftControl）+ 贴地 + 玩家 AABB 严格重叠 campfire 格
     → `isSitting`（每 tick 由按键 + 世界重算，**不入存档**）：锁定移动 +
     跳跃（松 crouch 或脱离重叠 = 站起）；crouch 高度即坐姿（无独立坐姿 =
     v1 文档化偏离）
   - **存档**：format 11 `campfires` 数组——**每 campfire 块一条目**（`pos`
     + `item`/`count` + 整数 `progress_substeps`），位置排序，与方块差集
     **双向**交叉校验 + **进度不变量**（progress > 0 ⇒ 槽恰 1 个可熔炼物）。
     format 10 及以下可加载：无条目，手编旧文件的 campfire 块编辑 → 空状态
- **染色**（Phase 41，Phase 45 补全颜色，全数据驱动）：
  - **染料物品**：物品可选字段 `dye_color`（颜色名 = 纯数据词汇表：任何
    `dye_<color>` 物品注册即扩展词汇表，**加颜色零代码改动**；显式空串
    拒绝）；当前 15 种 `dye_<color>`（material，max_stack 64，2D 图标）：
    white/black/brown/red/yellow/orange/pink/gray/silver（P41）+
    blue/green/purple/magenta/light_blue/cyan（P45；项目 silver 位 = MC
    light_gray，MC 的 16 色位一一对应）。来源配方（MC 1.14+ 同款）：
    白←骨头、黑←煤炭、棕←腐肉、红←生牛肉、黄←金锭、灰/橙/粉/银←混色、
    **蓝←lapis、绿←cactus、紫←allium、品红←tulip、浅蓝←cornflower、
    青←lapis+绿**（来源块见「世界生成」的 biome 装饰/矿脉）
  - **可染方块**：方块可选属性 `dye_base` + `dye_color`（必须同时给；
    `dye_base` 与 `interact` 互斥）。变体名 = `dyeVariantName(base, color)` =
    `"<base>_<color>"`，**white 特例 = base 名本身**（`wool_white` 不存在，
    白色就是 `wool`）。当前家族：`wool`（白）+ 14 个染色变体
    `wool_<color>`（hardness 0.8，掉自身，6 面同 tile）
  - **交互**（右键顺序：食物 → launcher → interact → **染色** → 放置）：
    选中染料右键命中 dye 家族方块 = 该方块立即转为目标颜色变体 + 消耗 1 个
    染料 + "cloth" 音效 + **点击被吞**（染料不是方块物品，不放置）。**同色
    重复 = no-op 不消耗不拦截**（点击落到 placeBlock 无操作——文档化的 v1
    规则，Minecraft 会消耗）
  - **init 交叉校验**（ammo/smelt 同款模式）：dyeable 块必须有 `dye_color`；
    块的 `dye_color` 必须是某个染料物品的颜色；**每个 (dye_base, 颜色) 组合
    的变体块必须存在**（缺 = 死染料，启动报错）
  - **染色即换块**：走 `setBlock` 脏标记管线（下一帧重网格）+
    `Lighting::updateLocal`（染色前后都是 opaque，光不变）；无存档格式变更
    （diff 按块名存，format 6 不变）
- **饮用 / 挤奶**（Phase 43，全数据驱动）：
  - **可饮用容器**：物品可选字段 `drink_time`（秒，(0,10]；present = 可饮用，
    **与 `restore_hunger` 互斥**——一个物品不能既是食物又是可饮用）；
    当前唯一：`milk_bucket`（material，max_stack 1，`drink_time 1.6` = MC
    牛奶时长，`returns_item: "bucket"`）
  - **返还容器**：物品可选字段 `returns_item`（非空物品名，**要求
    `drink_time` 存在**；init 对 ItemRegistry 交叉校验 = ammo 模式）——
    饮用完成时返还 1 个该容器到背包（开始饮用时已探明有空位/可并入堆，
    见下）
  - **饮用通道**：复用进食状态机（`core/food.hpp` `EatChannel::start(
    duration)` 泛化——通道时长按物品 `drink_time` 数据驱动，progress()
    按通道自身 total 归一）；**开始**时播 "milk" 音效（MC 一致：咕嘟声在
    前，取消也响过）；**完成**时消耗 1 个 + 返还容器 +
    `clearStatusEffects()`（v1 效果集 = 清红闪 `m_hurtFlash`；坠落起点
    刻意不清——MC 坠落伤害非状态效果）。中断/取消语义与进食完全相同
    （提前松开/受击/死亡/解锁/开面板/换格）
  - **清状态效果（Phase 46 补全）**：饮用完成 `clearStatusEffects()` =
    清红闪 `m_hurtFlash` + **移除全部负面（`negative: true`）状态效果**
    （MC 牛奶语义：正面的再生保留；**火 `negative: false` = 牛奶不灭火**，
    Phase 57，MC 一致——灭火只有水）。注意**受击会中断饮用通道**（P28
    规则）——中毒状态下开喝，2.5 s 毒 tick 会打断它（MC 一致）
  - **满背包 no-op**：开始饮用前探测 `returns_item` 的 `isFull`（count-1
    可加探针）= 无空位且无未满同物堆 → 不开始（no-op）但**吞点击**
    （MC 一致：空桶没处放时喝不了）
  - **挤奶**（mob 字段，成对校验 = dye_base/dye_color 纪律）：mob 可选
    字段 `milk_with`（手持物品名）+ `milk_gives`（产出物品名），必须同时
    给、非空字符串，init 交叉校验两物品存在。当前：cow（`bucket` →
    `milk_bucket`）
  - **右键顺序**（Phase 60 更新）：食物 → **饮用** → launcher → **挤奶**
    → interact → 染色 → **流体桶** → **点燃器** → 放置。挤奶 = 准星命中（近战目标：
    kReach 5 + 方块遮挡，复用 `targetMob`）`milk_with` == 选中物品名的
    mob：消耗 1 个手持 + 产出 1 个 `milk_gives` + "milk" 音效 + **mob
    不受任何影响**（无伤害/死亡/掉落/冷却——v1 规则，MC 有 5 分钟挤奶
    冷却，见 KNOWN_ISSUES）；背包满 = 不产出但吞点击。选中非
    `milk_with` 物品或准星无 mob = 落空继续后续（不吞）。流体桶在
    interact **之后**：准星对着炉子/工作台时即使手持水桶也开面板（MC）
  - **物品**：`bucket`（material，max_stack 16 = MC 值，2D 图标）；配方
    `bucket`（shaped `I I`/` I `，I = iron_ingot，3 铁锭 = MC 配方）；
    新内容均不进默认 hotbar / 世界生成
- **流体桶**（Phase 47，Phase 51 双种类，全数据驱动）：
  - **桶对 = 数据**：物品可选字段 `collects_fluid`（空桶侧：**流体名
    数组**，一个空桶舀所有命名流体 = MC）与 `pours_fluid` +
    `pours_becomes`（满桶侧：倒 → 空桶；每种流体一个满桶）。流体种类
    词汇表在代码中（`FluidKind`：`"water"`/`"lava"`，其他名解析拒绝）。
    **满桶由空桶侧按流体推导**（`fullBucketFor`，无 `collects_becomes`
    字段）。init 交叉校验：空桶命名的每种流体**恰有 1 个**满桶倒回
    （0 = dead field / 2 = 歧义，都拒绝）+ 满桶伴侣必须收集该流体
    （对称，错配 = init 失败点名）。两侧互斥（一个物品只是一对的一
    侧），且与食物/可饮用互斥（右键通道归属）。当前对：`bucket`
    （`["water","lava"]`）⇄ `water_bucket` + `lava_bucket`（各
    material，max_stack 1）
  - **舀（collect）**：选中空桶侧，**桶目标射线**（`raycastFluid`：DDA
    首个「非 air 方块 **或** 携带流体」的格——与 `raycastBlock` 共享
    遍历骨架，差异 = 墙壁**遮挡**（MC：桶用方块目标，不能隔墙舀；普通
    方块射线穿水而过））落在 **level 8（源）** 格：移除该源 + 手持 1
    个换成伴侣（满桶）+ "splash" 音效 + `Lighting::updateLocal`（水格
    光照衰减变化）。流动水（level < 8）**不能舀**（MC 一致）→ 落空
    继续后续（placeBlock 对 material 是 no-op = 什么都不发生）
  - **倒（pour）**：选中满桶侧，**放置目标格**（首个非 air 方块的受击
    面邻格 = `placeBlock` 的同一约定）必须为 air、无流体、不重叠玩家
    AABB（`canPlaceAt` 的测试）：放一个 level 8 源 + 手持 1 个换成伴侣
    （空桶）+ "splash" + `Lighting::updateLocal`。倒在水面上/实心格上
    = 落空 no-op。倒下的源随后走正常的 **FluidSim**（20 Hz 帧确定性：
    悬空下落、落地四向扩散 level 递减——放出的水与生成湖同一套模拟）
  - **吞点击语义**：舀/倒成功 = 吞点击（桶永不放方块）；背包满（伴侣
    无空位）= 无操作但吞点击（MC 一致）；落空（无源/目标无效）= 不吞
    继续后续（实际效果 = no-op，material 不可放置）
  - **存档**：物品按名序列化（`water_bucket`/`lava_bucket` 直接可读）、
    流体格走 `fluids` 编辑数组——format 9 起编辑带种类名
    `[x, y, z, level, "water"|"lava"]`（format 8 的 4 元素编辑加载时
    全部迁移为 water）
- **点燃 / 燧石与钢**（Phase 60，数据驱动）：
  - **字段**：物品可选字段 `ignites_flamable`（严格 bool，缺省 false）；
    true = 点燃器。与五个右键通道**互斥**（`restore_hunger` /
    `drink_time` / `shoot_speed > 0` / `dye_color` / 桶对字段——通道
    所有权纪律，P47 范式）；非 bool 点名拒绝；init 白名单（P58 教训）
  - **语义**：选中点燃器**右键**（`wasPressed` 单点击、无通道）：准星
    目标块（`targetBlock`，reach 5）**`flamable`** = true → `setBlock`
    **FIRE**（Phase 64 起：烧成火焰块而非 air）+ `Lighting::updateLocal`
    + "ignite" 音——**无掉落**（Phase 58 熔岩烧语义：烧掉的作物不掉落）、
    **不消耗**（MC：燧石与钢用不坏）。非 flamable 目标 / 无目标 =
    落空不吞（material 的 placeBlock = no-op）；在 interact **之后**：
    准星对着炉子/工作台仍开面板（MC）
  - **内容**：`flint_and_steel`（material，64 叠，图标 = 钢杆 + 燧石
    尖）+ 配方 `flint_and_steel`（shapeless flint + iron_ingot，MC 配方）。
    v1 范围（文档化偏离）：只作用于块（Mob 点燃仍只来自熔岩 Phase 58）
  - **确定性**：单点击边缘事件、无计时器、无随机——同存档同点击帧 =
    同结果
- **火 / fire 块**（Phase 64 火视觉 v1；Phase 66 粒子 + 蔓延）：
  - **块**：`fire` = cross 模型 + 火焰纹理（atlas [10,0]）、
    `light_emission 15`（MC 值，夜间可见的光源）、`hardness 0`
    （瞬破）、**无 drops**（破坏/燃尽都不掉东西）、非 solid/opaque、
    **非 flamable**（火是燃烧的终态，不会被再烧）
  - **来源**：① 点燃器右键 flamable 块（上）；② 熔岩燃烧 pass
    （Phase 58：熔岩 6-邻 flamable 块 → fire，pass 的燃烧目标 =
    registry 有 `fire` 块时 fire、否则 air【单测 fixture 语义】）；
    ③ **玩家放置**（fire 有隐式 block item：放一个 = 临时营火）；
    ④ **蔓延**（Phase 66，下）
  - **蔓延**（Phase 66，`world/fire_spread` 纯函数，镜像生长引擎）：
    每 `fire.spread_interval_frames` 帧（默认 6 = 0.1 s，MC tick 节奏；
    固定帧步、`m_frameCount % interval` 门控），每活体火对 **5 邻**
    （水平 4 + **上**；**不向下**——MC：地火不烧地板）的 `flamable`
    格独立 roll：`u = splitmix64(世界种子 + 帧×GOLDEN + 位置哈希 +
    邻序×GOLDEN2) < fire.spread_chance`（默认 0.05 = MC normal 值）→
    过则 `setBlock(fire)`（收口注册满寿命 = 新火继续蔓延）+ 局部光照。
    一格邻多火 = 每火各 roll、**只点燃一次**（输出位置序去重）；
    无火/无 fire 块/无 flamable/chance 0 全 fast-exit；**无音效**
    （文档化偏离，防刷屏）
  - **余烬粒子**（Phase 66，`fire.particles` 默认开）：每
    `kFireParticleEveryNFrames = 6` 帧每活体火发 **2 粒**（0.12 立方
    体、火焰纹理、**负重力 −2 上升**、寿命 0.3–0.5 s、种子 = 世界种子
    ⊕ 位置哈希 ⊕ 帧推进）；**燃尽/浇灭**再发一次 16 粒死亡爆发
    （复用 `spawnBreak`）。发射 = 固定帧步（确定性）、运动 = real dt
    （P15 约定 → 活余烬截图不可字节比，见 PITS）
  - **燃尽**：`Game::m_fires` map（位置 → 剩余秒，setBlock 收口创建/
    删除 = 炉子范式）；**real-dt** 倒计时（生存计时器类），
    `config fire.lifetime_seconds`（默认 4.0，>0 硬校验）归零 →
    `setBlock(air)`（无炭无掉落）
  - **浇灭**：fire 的 6-邻任一格有**水**（任一级）→ 当帧瞬灭；
    水胜火（Phase 57 规则）；**熔岩不浇灭**（池边的火是熔岩的产物，
    继续烧——MC 同款共存）
  - **接触伤害**（Phase 70）：玩家 AABB **严格重叠**任一 fire 块格时，
    每 `player.fire_damage_interval_seconds`（默认 0.2 = MC 火节奏
    5 HP/s）受 `player.fire_damage` HP（默认 1；**0 = 关**；
    `≤ player.max_health` 交叉校验）——**real-dt 生存计时器**
    （P54 熔岩灼烧的块版：在火里累积、离开重置未到期 tick、走正常
    伤害路径 = 红闪/音效/死亡处理全触发）。fire 块 id 在 Player 构造
    时解析一次（registry 无 fire 块 = 永不，单测 fixture 语义，
    fluid.cpp 回落同款）；重叠判定 = `Entity::overlapsBlock`
    （与 `overlapsFluid` 同款 AABB 严格不等式扫描，caller 传 id =
    数据驱动）。**Mob 站火里不受伤害**（v3 范围）
  - **存档**：format 10 的 `fires` 数组（{pos, remaining}，位置序；
    remaining > 0；与块编辑**双向**交叉校验 = 炉子键纪律；format 9
    读档 = 空 + 手编的旧文件 fire 块补满寿命条目）
  - **init 交叉校验**：registry 有**任何** flamable 块则**必须**有
    `fire` 块（燃烧目标缺失 = 点名拒绝）；零 flamable 的资产集合法
    （燃烧路径全惰性、fire 块可选）
  - **v2 范围（文档化偏离）**：余烬 = v1 视觉（2 粒/0.1 s、单材质
    上升火星）、蔓延引燃无声、蔓延无难度/风/湿度调制、光强度恒定 15
    无闪烁、无接触伤害（见 KNOWN_ISSUES 火 v2 条）
  - **确定性**：倒计时 real-dt（同帧率跑字节可复现、燃尽时刻墙钟
    稳定）；浇灭/燃尽的 setBlock 走主线程写窗口、map 迭代位置序；
    蔓延 roll = 固定帧步纯函数（同种子同帧同格 = 同结果，跨世界/帧
    去相关）；余烬发射 = 固定帧步（种子纯函数）、运动 real dt
- **熔岩 / 流体双种类**（Phase 51，数据驱动 + 反应表在方块上）：
  - **编码**：chunk 流体字节 0=干、1..8=水 level、9..16=熔岩 level
    （一格至多一种，`setFluid` 正 level 替换异种、0 只清本种）
  - **模拟**：`FluidSim::tick` = ①**反应 pass**（快照扫所有熔岩格：
    6-邻域任有水 → 该熔岩格 `setBlock` 为熔岩块 `water_reaction` 的
    产物——邻水含 level 8 源 → `source`（黑曜石），否则 `flowing`
    （石头）；水不消耗、无蒸汽 = v1 文档化偏离；无 `water_reaction`
    的惰性熔岩不反应）→ ②**燃烧 pass**（Phase 58：快照扫所有熔岩格
    6-邻 **BLOCK** 格，`flamable: true`（可选 bool 属性；内容 =
    wheat_1..7 + wood/leaves/grass，Phase 63 扩到 MC 天然可燃面）
    → 收集排序后 `setBlock(air)`，**无掉落**；注册表无 flamable 块 =
    fast-exit）→ ③水模拟 → ④熔岩模拟（各自快照规则与 P9 水模拟完全
    相同，按种过滤；**异种流体格按"方块"处理** = 支撑 + 挡扩散，防
    惰性配置下跨种替换振荡）。稳态不变量：无 6-邻接的水-熔岩对
    （反应必收敛出石头/黑曜石墙）、熔岩 6-邻无 flamable 块（烧 pass
    收敛）
  - **光照**：流体格光 = 该种类参考块的 `light_emission`（熔岩 15 =
    发光池）；衰减（每格 −3）两种相同
  - **玩家**：`isInWater` 仅水；熔岩 = 同款游泳状态 + 灼烧（Phase 54）
    + 点燃火效果（Phase 57，见"火系统"）；**mob** 燃烧 = Phase 58
    （见"火系统 v2"）；mob 在任一流体中不生成（不变）
  - **内容**：`lava`（fluid、发光 15、`water_reaction {obsidian,
    stone}`、不可破坏）/ `obsidian`（hardness 10、掉自身）/
    `lava_bucket`（倒 → `bucket`）/ `bucket`（`["water","lava"]`）
- **状态效果**（Phase 46，数据驱动，`core/status_effect.{hpp,cpp}` +
  `assets/data/effects/*.json`；效果只作用于玩家，v1 mob 不持效果）：
  - **效果 = 数据**：`data/effects/` 每效果一个 JSON（严格解析 =
    方块/物品/mob 注册表纪律：未知键拒绝、名字唯一、**no-op 拒绝**
    ——必须有 tick 或正 `melee_penalty`）：`tick_type`（`"damage"` /
    `"heal"`；缺省 = 无 tick 的纯修饰效果）+ `tick_amount`（每次 tick 的
    HP，(0,10]，tick 必填）+ `tick_interval`（秒，(0,100]，tick 必填）+
    `min_health`（仅 damage：0 = 无地板可致死 / 1 = 保持 ≥1 HP 永不致死
    = MC 中毒地板）+ `melee_penalty`（近战扣除攻击点，[0,100]）+
    `absorb_hp`（Phase 56：激活期间的**临时 max HP**，整数 [1,64]——
    吸收层；0/缺省 = 无层）+ `negative`（bool；牛奶只清负面）+
    `ignites_on_lava`（Phase 57：bool，缺省 false——在熔岩内每帧重施加
    本效果；**至多一个**注册效果可带此标志，init 交叉校验）+
    `extinguished_by_water`（Phase 57：bool，缺省 false——玩家在水内时
    瞬时移除本效果）+ `icon`（effects.png 条带 tile 索引，init 对纹理
    宽度交叉校验）。当前 5 种：
    `poison`（damage 1 / 2.5 s，min_health 1，负面，icon 0）、
    `weakness`（melee_penalty 4，负面，icon 1）、`regeneration`
    （heal 1 / 4.5 s，正面，icon 2）、`absorption`（absorb_hp 4，正面，
    icon 3）、`fire`（damage 1 / 0.2 s，min_health 0 可致死，**正面**
    【`negative: false` = 牛奶不灭火，MC 语义】，两数据标志皆 true，
    icon 4）
  - **来源（v1）**：① **mob `on_hit_effect`**（`{"name", "duration"}`，
    成对必填严格解析，init 对 EffectRegistry 交叉校验 = ammo/挤奶
    模式）——该 mob 的**近战命中落定**时施加（远程箭**不**施加）；
    当前：spider（`poison` 15 s = MC normal 难度值）；② **熔岩点燃**
    （Phase 57，见下"火系统"）；③ 存档注入（E2E 场景）。weakness
    v1 无游戏内来源（MC 来源 = 药水，未做）
  - **tick（真实 dt，生存计时器类，硬规则 8）**：`Game::updateEffects`
    （每帧 `updateSurvival` 后）：`remaining -= dt`、≤0 移除；tick 效果
    `accum += dt`、**每跨过一个 interval 结算一次**（大 dt 可跨多次）。
    damage tick 的 min_health 地板**在游戏侧执行**（`health − amount <
    min_health` 跳过——地板让中毒永不致死）；heal tick 走
    `Player::heal`（clamp maxHealth）。毒伤走正常受伤路径（红闪/音效
    复用；**无独立紫闪**，见 KNOWN_ISSUES）。同一效果再次施加 = 重置
    duration（MC 语义，无 amplifier；tick 相位保留）
  - **虚弱**：`selectedAttackDamage()` 减去活跃效果 `melee_penalty` 总和
    （clamp ≥0——虚弱的空手打不出伤害）；只影响玩家→mob 近战
  - **吸收池（Phase 56 引入，Phase 72 改两分量模型）**：玩家
    **health（0..base max）+ absorption 池（0..64，`kAbsorptionMax`）
    两个独立池**（`Player::absorption` / `setAbsorption`；
    `Game::syncAbsorption` 纯 SET 到 `totalAbsorption`）。**同步点** =
    效果集合真正变化处：进食施加 / mob 命中 / `updateEffects` 内到期
    或水熄灭（效果总和变化才同步——每帧同步会与伤害扣减相抗）/ 牛奶清
    / 死亡清；**读档不同步**（存值权威）。池是效果总和的纯函数**只在
    同步点**——刷新恢复满层（MC 属性重算语义）；**同步点之间伤害把池
    扣减的部分持久保留**（池 < 总和合法）。`damage()` **先扣池再扣
    health**（池单独不能致死——死亡只来自 health）；`heal()`/
    `setHealth()` 只钳制到 **base max**（heal 永不填池——池只由效果
    设置）。层到期/清除 = 池随效果总和掉下（health 不动——池不是 HP，
    无红闪/音效/伤害计时器）。**format 12** `player.absorption`
    （parse 校验：格式 12 必需 / ≤11 带键拒绝 / [0,64] / **≤ 存档效果
    `absorb_hp` 总和**）；**format ≤11 迁移**（`applySave` 4e）：共享
    health 按 base max 拆分（超出 → 池，health 取 base 部分；22 →
    20+2）。HUD 吸收红心从**池**派生（`ceil(池/2)` 颗、strip 值 = 池，
    池 0 = 零绘制 = 无输入结构性免疫）；F3 调试层显示 `absorption`。
    金苹果 = 首个来源（吸收 4 / 5 s，正面——牛奶保留）
  - **火系统（Phase 57，残留燃烧）**：`fire` 效果（1 HP / 0.2 s = MC
    燃烧节奏，min_health 0 可致死）+ 两个数据标志：
    ① **熔岩点燃**（`ignites_on_lava`，init 解析出唯一点火效果 id，
    `Game::m_fireEffectId`）：`Game::updateEffects` **每帧**（即使无活跃
    效果）检查玩家流体状态——`inLava()` 为真时**重施加** fire（duration
    = `player.fire_duration_seconds`，默认 8 s = MC Normal；重施加 =
    duration 重置 + tick 相位保留 → 站在熔岩里 = 持续燃烧，MC 语义）；
    ② **水灭火**（`extinguished_by_water`，`StatusEffects::clearExtinguishable`）：
    `inWater()` 为真时瞬时移除（在 tick 之前执行 → 水内零燃烧伤害）。
    **水胜熔岩**（两流体并存时先点燃后灭火 = 无火，P54 熔岩优先的
    游泳状态不受影响）。离熔岩后 fire **继续 countdown** 8 s 烧至到期
    = 残留燃烧（MC 核心行为）。燃烧 tick 走正常受伤路径（红闪/音效
    复用，**无独立火焰表现**，见 KNOWN_ISSUES；每秒 5 次）。牛奶**不**
    灭火（`negative: false`，MC 语义——只清负面）。熔岩内还有 P54 的
    直接灼烧（4 HP/s）与火 tick（5 HP/s）**叠加**（MC 同款：熔岩内
    ~9 HP/s）。死亡/重生照旧清空（fire 不残留于重生）；存档照旧
    （`player.effects` 含 fire，读档续烧）
  - **火系统 v2（Phase 58，熔岩烧 Mob 与作物；Phase 73 火 v3：火块
    同样点燃 Mob——`inLava || inFire` 同一点火路径）**：
    ① **Mob 点燃**：`Game::attemptMobSpawn` 在 `make_unique` 后对每只
    mob `MobEntity::armFire(interval, amount, duration)`（参数 = 点火
    效果的 `tick_interval`/`tick_amount` + config
    `fire_duration_seconds`——与玩家同源）；无 fire 效果的资产集 =
    全部 mob 结构不燃。燃烧 = **整数帧格**（`m_fireFrames` int 计数，
    `m_fireIntervalFrames = llround(interval×60)`）：`update()` 在
    `moveInSubsteps` 后——`inLava`（`Entity::overlapsFluid(Lava)`，
    AABB 重叠熔岩格）**或 `inFire`（Phase 73 火 v3：
    `Entity::overlapsBlock`，AABB 严格重叠 fire 块格——块的 id 构造
    时从注册表解析，无 fire 块 = invalid = 结构惰性，P70 玩家同款
    容错）**→ remaining 重置 = duration（**相位不清零** =
    火源内持续燃烧）；`inWater` → remaining = 0 + 相位重置（**先于
    tick 执行 = 水内零伤害、水胜熔岩**）；remaining −= dt 后 `> 0`
    才 ++frames，每跨一个 interval 帧结算 1 次 damage tick（走
    `MobEntity::damage` 正常路径，**可致死**；**到期帧不 tick**，
    P46 语义）。fire 0.2 s / 1 HP → 5 HP/s，cow 10 HP = 2 s 烧死；
    熔岩/火块内 remaining 每帧重置 = 离开火源才 countdown 8 s
    （残留）。**无火焰视觉**（纯 HP 通道，见 KNOWN_ISSUES）。
    ② **作物/方块燃烧**：FluidSim 燃烧 pass（见上"熔岩 / 流体双
    种类·模拟"②）——`flamable` 块被熔岩 6-邻接触即烧成 air（无
    掉落；水不烧）。内容 = wheat_1..7。
    ③ **init 交叉校验**：点火效果的 `tick_interval × 60` 必须是
    **整帧数**（容差 1e-4 帧——float 0.2f×60 = 12.00000018 过；
    0.33 s = 19.8 帧点名拒绝"not a whole number of 1/60 s frames"；
    `flamable` 非 bool 点名拒绝）。
  - **死亡**：效果随玩家死亡清空（重生 = 干净状态，MC 一致）
  - **存档（format 8）**：`player.effects = [{name, remaining}]` 名字序
    （`remaining` = 真实 dt 秒数；**tick 相位不存**——重载后下一 tick
    从整 interval 起算；fmt 1-7 文件加载为空；fmt 8 文件**必填**该键 =
    furnaces 键纪律；名字对 EffectRegistry 交叉校验、重复/非注册拒绝）
  - **HUD**：效果图标条与心带**同行**、hotbar 左缘左对齐（MC 布局）：
    每个活跃效果一个 `assets/textures/effects.png` 的 16px tile
    （80×16，5 tile：紫骷髅 / 蓝下箭 / 绿上箭 / 红心+白十字【吸收】/
    橙火焰+黄芯【火】，`tools/generate_effect_icons.py` 确定性生成；
    tile 数 = 纹理宽度 / 16，UV 动态），按施加顺序排列；
    只在游玩态（锁定 + 无面板）绘制——无输入运行结构性免疫。**吸收红
    心**（Phase 56）：层激活时正常心带**左侧**多出 `ceil(层/2)` 颗
    红色心（正常心带位置不动 = 无层时逐位相同；红心显示 health 超出
    base max 的部分，`health − base`）；层 = 0 时零绘制。F3 调试层
    显示 `effects poison 12.3s; ...`（无效果 = `effects (none)`）
  - **spider**（Phase 46，MC 标准毒源）：16 HP、speed 1.0（比 zombie
    快）、攻击 2（normal 基线，走难度缩放）、近战 1.6 / 冷却 1.0、
    探测 16、只在黑暗生成（`spawn_max_light` 0）、**无掉落**（MC 的
    string/spider_eye 物品未做）、模型 AABB 1.4×0.9（身 1.3×0.7×0.9 +
    头 0.5³，宽扁低趴）、纹理深灰褐几丁质 + 头部前双面红眼、生成表
    weight 1（与其他 5 种等概率）
- **游泳**（Phase 9）：玩家包围盒与任何水体格相交即进入水中状态——水中重力减半
  （12 vs 32）、终端速度 4、水平速度降为 3.1（冲刺无效）、`Space` 随时上浮（4.5）；
  浅水（头在水面上）可以"蹚水走"，深水可以游
- **方块交互**（DDA 体素 raycast，reach 5 格）：
  - `左键`（锁定时，**按住**）：破坏准星指向的方块，耗时数据驱动
    （`1.5s × properties.hardness ÷ 当前选中物品对目标块的 effective
    break speed`，徒手/方块/材料=1.0；`properties.destroyable` 数据驱动，
    基岩不可破坏）。**有效速度 = `effectiveBreakSpeed(item, block)`
    纯函数**（Phase 67）：tool = 其 `tool.break_speed` ×（目标块在物品
    的 `fast_against` 块名列表内 ? `fast_against_multiplier` : 1）——
    分块工具类别 = 三把斧对 wood/leaves ×2（MC 斧对木 2×）+ Phase 68
    三把铲对 dirt/grass/sand/gravel ×2（MC 铲对土 2×，项目土类全集）+
    Phase 69 三把镐对 stone/cobblestone/5 矿/glowstone ×2（MC 镐对石
    类/矿类 2×，项目石矿类全集；**obsidian 排除**——MC 铁镐不能破黑
    曜石、项目无钻石镐，黑曜石保持平镐速）；清单外块 = 工具自身
    break_speed（斧/铲 = 手速 1× = MC 斧/铲对石 1×；镐 = 其 2/3/4
    flat 档）；
    `fast_against` 与 `fast_against_multiplier` 是
    严格配对字段（缺一/空列表/非 tool 均解析拒绝，init 逐条目交叉
    校验块名存在 = grows_on 同款）。准星移开/松键/换工具
    进度清零。raycast 把水体当 air 穿过——可以隔水破坏湖底/水下的方块
    （水会被破坏排水、随后流体模拟把它补回）
  - `右键`（锁定时）：**先试食物**（选中食物 = 开始进食通道，点击被吞）
    再试**饮用**（选中可饮用容器 = 开始饮用通道，点击被吞，Phase 43）
    再试 launcher（弓蓄力）再试**挤奶**（准星命中可挤 mob + 手持其
    `milk_with` 物品，Phase 43）再试 interact——命中格有 `interact` 属性
    （工作台/炉子）则打开对应面板、不放置——再试**染色**（选中染料 +
    命中 dye 家族方块，见「染色」）；否则在命中面的邻格放置
    **当前选中格**的方块物品，
    并**消耗 1 个**（方块不再无限——背包是方块的来源；工具不可放置）；不能
    放进 solid 方块或玩家自身包围盒内
  - `Tab`：切换选中的 hotbar 格（9 格）
  - `Q`（锁定时）：丢弃选中格 1 个到脚下（生成掉落物，0.5s 拾取延迟防立即回吸）
  - `ESC`（锁定时）：释放指针；未锁定时 ESC 退出游戏
  - 破坏/放置复用 `setBlock` 脏标记管线，下一帧自动重建受影响 chunk 的 mesh
- **游戏内 UI**（Phase 15，专用无光照 UI 着色器 + 批量化绘制；
  所有 UI 元素只在按键/指针锁定时渲染，无输入运行渲染零新增 → 基线截图逐字节不变；
  **UI 坐标空间**（Phase 34）：全部 UI（hotbar/心·饥饿带/面板/F3/死亡屏/手持/准星）
  布局在"UI 像素"空间 = 窗口**逻辑**尺寸 ÷ `ui.scale`（config，默认 1.0，
  范围 [1,4]），正交投影拉伸到可绘制区——GPU 放大（NEAREST 纹理保持锐利），
  SDL 逻辑鼠标坐标 ÷ `ui.scale` 做命中测试。**高分 UI 素材**（Phase 35）：
  字体 16×16 字形（scale 1 = 16px、scale 2 = 32px）、平面物品图标 32×32、
  文字缩放为 double scale（分数缩放支持）；**面板自适应**（Phase 35）：
  面板 native 设计 1000×228 UI px，UI 空间更小时整个面板（槽/间距/内边距/
  标题/配方行 + 按同一系数 `InventoryLayout::scale` 绘制的字体 scale 与图标尺寸）
  等比缩小（长度全程 double 运算、仅 rect 边界 lround），恒贴合 UI 空间
  （每边 ≥1px 余量）——1280×720 窗口 + `ui.scale=2`（UI 640×360）下面板
  缩到 638×145 恰好放下（P34 时代会向两侧裁切）。
  **文字着色**（Phase 36）：UI 着色器 textured 分支 = `texel.rgb ×
  u_color.rgb`（此前 u_color 只调制 alpha——死亡屏传入的红色被静默忽略、
  实际渲染白字，P33 发现）；图标/心带/白字的 u_color.rgb 均为白 → 逐位不变，
  仅彩色文字（死亡屏红字）生效。
  世界渲染不受 `ui.scale` 影响（始终全可绘制区））：
  - **准星**（指针锁定时屏幕中央白色十字）与**目标方块描边**（24 顶点线框立方体，
    描出 raycast 命中格）——只在游玩态（锁定 + 背包未开）显示
  - **hotbar**（锁定且背包关闭时）：底部 9 格，选中格橙色边框，堆栈数 < max 时
    显示数量；**手持物品**右下角大图标（76px）
  - **物品图标**：block item = 带光照的倾斜 3D 立方体（复用掉落物 cube mesh +
    基本着色器，白天亮夜晚暗，与掉落物同观感）；工具/材料 = 32×32 图标纹理
    （`Item.icon` 数据字段 → `assets/textures/items/<icon>.png`，
    `tools/generate_item_icons.py` 程序化生成，Phase 35 起 32×32）
  - **背包 + 合成面板**（`E` 开关；打开时释放指针、暂停玩家物理与方块交互，
    世界/昼夜/流体/实体/流式/熔炼照常 tick）：27 格（hotbar 行 + 18 背包格）+
    2×2 合成格（共享 3×3 网格模型的左上 4 格）+ 右侧配方列表（**仅 shapeless
    配方**；产物图标 + 名称 ×N，白色=可合成/灰色=不可 + 原料清单）；
    炉子面板（Phase 40，右键炉子打开）同 27 格 + input/进度条/output/fuel
    四槽，点击语义同背包格（无合成动作）；
    **左键**取整堆/放入/交换（取出被占格的堆）/并入有剩余空间的同物堆，
    **右键**移 1 个（同物有空间或空格；不同物格=无效操作），点击面板外（左键）
    丢弃手持堆；悬停格显示物品名；点击配方行 = `craft()`（全有或全无）
  - **F3 性能/调试 overlay**（任意指针状态可切换）：标题/FPS/帧号、chunk 数/
    draw call/顶点数、玩家坐标/朝向、列数/seed、昼夜时刻/指针状态、流式激活/
    待加载列/半径、背包内容/粒子数
  - **F5 立即存档**（`--save` 槽激活时，游戏内随时保存，日志记录结果）
- **破坏反馈**（Phase 15）：**渐进裂纹**——破坏进度映射到 5 级累积裂纹纹理
  （`assets/textures/cracks.png`，80×16 五格，白 alpha 随机游走裂纹），破坏中的
  方块上叠加半透明裂纹面（进度 = `1.5s × hardness ÷ speed` 的完成比）；**破坏
  粒子**——方块碎成 16 个 0.12 格小立方体（带初速弹散 + 重力，0.45~0.75s 消亡，
  无碰撞，spawn 时烘焙所在格光照——渲染是只读窗口不能读光数组），按透明性
  分 opaque/transparent 两 pass，逐帧动态 mesh
- **音效**（Phase 15，`SoundBank` = SDL3 `SDL_OpenAudioDeviceStream` 队列式播放，
  无回调；初始化失败优雅降级为静音 + WARN）：固定事件集 v1（数据驱动，无
  per-block 差异）——破坏（按住时每 0.3s 一个 dig tick + 完成时的 break）、
  放置、拾取（含 Q 丢弃）、脚步（移动 + 贴地 + 锁定 + 背包关闭时每 0.4s）、
  背包开/关、合成成功、受伤/死亡（Phase 16）、射箭（Phase 21）、拉弓弦音
  （Phase 24）、进食（Phase 23）、染色布料声（Phase 41）、牛奶咕嘟声
  （Phase 43，饮用开始 + 挤奶共用）。15 个程序化 WAV
  （`tools/generate_sounds.py`，44.1kHz 16-bit
  单声道），音量
  `config audio.volume`（默认 0.7）
- **物品与背包**（Phase 11/12，`Item`/`ItemStack`/`Inventory`，全数据驱动）：
  - 物品 = 纯数据（`ItemRegistry`，当前 25 种 = 14 block + 2 工具 + 9 材料）：
    **每个数据方块隐式生成同名 block item**（无独立 JSON，方块定义是唯一来源）；
    **工具/材料**是显式数据（`assets/data/items/*.json`，`type: "tool"` 带
    `break_speed`，`type: "material"` 是通用非方块物品——锭/木棍/木板等，
    作为配方原料/产物和方块掉落）
  - 背包 27 格 = 9 hotbar + 18 背包（`assets/data/hotbar.json` 的 `items` 列表
    定义初始 hotbar，每格满堆栈）；拾取确定性入包（先合并同物堆栈、再找空位，
    hotbar 优先于背包）
  - 破坏方块按 `properties.drops`（物品名，init 严格校验）+ `drops_count`
    （1..64 默认 1，须 ≤ 掉落物品 max_stack）掉落 `ItemEntity`——矿石掉材料
    （iron_ore → iron_ingot），wood 掉 wood 本身（木板配方的原料）；
    玩家走近（1.0 格内）自动拾取进背包（MC 同款 0.5s 拾取延迟 + 120s 消亡）
- **合成**（Phase 12 shapeless 基础 + Phase 17 shaped/工作台，全数据驱动）：
  配方 = `assets/data/recipes/*.json`（shapeless 原料袋 → 产物，或
  `"type": "shaped"` + `pattern`/`key` 形状配方，经 ItemRegistry 交叉严格
  校验）；`canCraft`/`craft` 是确定性纯逻辑（原料按槽序升序消耗、副本模拟
  全有或全无——失败背包逐位不变，产出永远放得下）。当前 7 配方：wood→4
  planks、2 planks→4 stick（shapeless）、2×2 planks→crafting_table、
  wooden_pickaxe（PPP/.S./.S.）、iron_pickaxe（III/.S./.S.）、bow
  （.S./SP/.S.，Phase 21）、2 stick 竖排→2 arrow（Phase 24）（后 5 个为
  shaped）。游戏内触发：背包面板（`E`）右侧配方列表点击行 = `craft()`
  （仅 shapeless，Phase 15）；或放置并右键工作台打开 3×3 面板合成（Phase 17）
- **存档/读档**（Phase 13，`--save <slot>`，`src/core/save.{hpp,cpp}`）：
  存档文件 `data/saves/<slot>.json` = **seed + 与生成态的 diff + 完整游戏状态**——
  不重复存储地形，只存"和这个 seed 生成的世界不同的格子"（`blocks`/`fluids`
  编辑列表，破坏=`"air"`；写档时 `diffWorld` 逐 chunk 重跑生成器比对——
  **>256 chunk 时每 chunk 一个 job 跑在临时 4-worker 池上**（Phase 37；
  输出按位置排序是全序，并行结果与串行逐字节相同，小世界走单线程零开销）
  ）+ 玩家位置/朝向/生命/饥饿/饱和度（格式 5）
  + 昼夜时间 + 27 格背包 +
  选中格 + 掉落物实体（含 120s 寿命与拾取延迟计时器）。读档 = 正常从 seed 生成世界后
  逐格应用编辑（先方块后流体）→ 全量重传播光照 → 恢复玩家/背包/实体/时间。
  加载校验**严格**（机器生成的文件出现未知字段 = 损坏/版本不符，直接报错：
  format 号、seed 与生成 seed 一致、物品/方块名已注册、数量 ≤ max_stack、
  y ∈ [0,128)、同一格不得同时有方块+流体编辑、位置不重复）。
  无 `--save` = 不读不写（默认行为不变）；`--new` = 忽略现有存档开新档
  （退出时覆盖）；干净退出（含 `--frames` 到点）自动写回，写失败只记错误日志
  不影响退出码。JSON 浮点序列化是**最短可往返精度**（位精确 round-trip），
  位置/时间/朝向加载后与原值位级一致
- **掉落物**（Phase 10，`ItemEntity`）：0.25 的小立方体，走统一的 Entity 碰撞
  物理（重力/终端速度/水平摩擦），落地后原地悬浮摆动 + 自转（动画由帧计数器
  驱动，非墙钟），同类 0.75 格内自动合并（堆叠上限按物品数据，block 64/工具 1），
  120 秒（MC 同款）或掉出世界后消失。零初速生成——静止位精确、截图可复现
  （对 MC 随机初速的刻意偏离）
- **AABB 体素碰撞 + 滑动**：玩家 0.6×1.8 包围盒（底面中心为位置），轴分离解析
  （X→Z→Y）——撞墙时该轴停住、另一水平轴继续走（滑动）；贴面精确解析（无 epsilon
  抖动）；子步化防高速穿透（每子步 ≤0.4 块）；重力 + 终端速度 + 跳跃高度 1.13 块
  （可跳上 1 格台阶，跳不上 2 格）；掉出世界（y < -32）回出生点
- **程序化体素世界（Seed 可复现）**：16×16×16 chunk，世界高度 128（y ∈ [0,128)），X/Z 无限。
  同一 seed 逐块复现（单元测试 + 像素级截图比对保证）；不同 seed 生成不同世界
- **Chunk 流式加载 + 多线程**（Phase 14，`ThreadPool` + `chunk_streaming`）：
  演示区之外的世界**按"列"（整列 8 个垂直 chunk）在 worker 线程上生成**——
  玩家走近已加载区边缘（距缺失列 ≤1 列）时流式激活（迟滞：半径环充满前不解除），
  每帧预算 1 列，按距离优先（最近先）填充 `world.view_distance`（默认 8 列，
  配置首次生效）的半径环。新列集成（`World::insertChunk`，面邻居标脏重网格化
  防边界 Z-fighting）与光照（`updateRegion` 柱 box 的 worker job，替代旧的
  "新 chunk→全量重传播"）都走同一帧管线：flush → 主线程写 World → 网格化/
  渲染（只读）→ post jobs（worker 运行窗口）。新列首帧以光 0 渲染、下一帧
  补光重网格（帧锁定下的确定性 pop-in）。**只增不卸载**（v1，见
  `docs/KNOWN_ISSUES.md`）。静止玩家（距边缘 ≥2 列）永不触发 → 无输入运行零 job，
  基线截图逐字节复现。**死亡时暂停**（Phase 33）：`streamChunks()` 在
   `isDead()` 时提前返回（在迟滞逻辑**之前**，激活状态保留）——死亡画面冻结
   玩家，但流式若继续会围绕尸体把半径方填满（`view_distance` 80 ≈ 20.7 万
   chunk / 数 GB / 数小时）；在飞的 ≤1 列经 post→flush 窗口自然排空，
   `postLightJobs` 照常照亮（integrate/光照不门控，否则该列会停在光 0）；
   `R` 重生后从保留的迟滞状态继续填充。昼夜/流体/mob 照常 tick（与背包打开
   同规则）
- **WorldGenerator**（无状态、纯函数 `generateChunk`，worker 线程并行调用）：
  - **地形**：fBm 2D 噪声高度场（±24 格，基线 40），y=0 bedrock 层，深处 stone
  - **生物群系**：低频噪声 + 权重选择（`assets/data/biomes/*.json`：desert/forest/plains），
    各 biome 有自己的表层/次表层/树密度/高度系数
  - **水**：低于海平面（y=34）的柱体填充流体水（fluid level 8）到海平面；
    生成即处于平衡态（游戏内 20Hz 流体 tick 不会扰动湖面）
  - **洞穴**：3D 噪声阈值雕刻，保留表层 4 格与底层 2 格的壳
  - **矿脉**：世界空间格网 + 随机游走（coal/iron/gold/diamond/lapis/
    **gravel**，`max_y` 分层限制；lapis：probability 0.08 / vein 6 /
    max_y 32；gravel：probability 0.25 / vein 4 / max_y 80，Phase 59 的
    常见石替换脉 = 唯一的非矿石"矿脉"），跨 chunk 无缝；只替换 stone
    （不破坏洞穴）；脉的 block 名在生成 setup 对 BlockRegistry 解析，
    未知 = 启动报错
  - **树木**：世界空间格网候选 + 数据驱动的树密度；树干 4~7 格 + 两层树冠 + 顶帽；
    跨 chunk 边界由邻接 chunk 各自补齐（边际格网机制），只写入 air（不破坏地形）
  - **表面装饰**（Phase 45，biome 数据驱动）：biome JSON 可选
    `decorations: [{block, probability}]`（严格校验：未知键拒绝、probability
    ∈ [0,1]、block 名在生成 setup 对 BlockRegistry 解析，未知 = 启动报错）；
    放置走**树的同一格网**（`trees.lattice`，专用 salt `kSaltDecoration`，
    每 (格, 条目) 对一次独立 hash3 roll——装饰 roll 与树 roll 不相关）；
    落在表面格**正上方一格**，前置条件：高于海平面 + 表面格仍是该 biome
    的 surface 块（洞穴啃掉的表面不放，防悬空）；在 `placeTrees` **之后**
    跑（树干/树冠占格时 `placeIfAir` 让位，装饰丢弃）。当前内容：沙漠
    cactus（probability 0.03）、草原 3 花（各 0.02）、森林 3 花（各 0.012）
- **演示区 + 流式扩展**：启动时同步生成 4×4 chunk 列（64×64 方块）；玩家按
  seed 自动出生在干燥陆地上（候选列需满足：高于海平面 + 出生包围盒内无方块，
  避开树干/树冠）。走出演示区后世界按流式加载（上一项）继续扩展——X/Z 真正
  无限，走到哪加载到哪（半径 `world.view_distance` 列）
- **流体模拟（水，Phase 9）**：每个 chunk 一个 16³ 的 0-8 等级数组
  （8=满/源，1-7 流动；同一 cell 里水与方块互斥）。`FluidSim::tick`（游戏线程，
  每 3 帧一次 ≈20Hz，快照式确定性）：下落（下方空/流动则沿列传送带下移）、
  满格失撑也下落（对 MC 的刻意偏离）、静止在方块上水平扩散 L-1（1 级终端）、
  坐在**满水**上补满到 8（source 托底的柱是满的——破坏湖底后整列补满、湖面恢复）、
  坐在流动池上按等级吸收/抬升（水量不守恒，见 `docs/KNOWN_ISSUES.md`）。
  `World::setFluid` 只写已存在 chunk 的 air 格；`setBlock` 会把所在格的水排掉
- **水的光照衰减**：天空光每穿一格水 -3（列顶扫描 + BFS 与不透明方块共用传播器）；
  深水湖底随深度变暗（截图可见浅水亮、深水暗的渐变）
- **半透明水体渲染**：`ChunkMesher::emitFluid` 输出 level/8 缩放的立方体（流动水
  顶面半高），水-水内部面剔除，流体面烘焙**自身格**的光；shader 使用纹理 alpha
  （水 α≈0.78 / 玻璃 α≈0.21 / 叶子 α≈0.92 + 透明孔），湖面可透视湖底
- **Face Culling 网格化**：只渲染暴露给 air/非 opaque 邻居的面；跨 chunk 边界通过
  `World` 查询邻居剔除；每个 chunk 输出 opaque + transparent 两个 mesh，
  渲染顺序不透明 → alpha 混合（leaves/glass/water）
- **十字植物渲染**（Phase 45，`BlockModel::Shape::Cross`）：方块 JSON
  `model: {shape: "cross", textures: {cross: <tile>}}`（严格：cross 形状只收
  单个 `cross` 纹理键，cube 的 top/side/bottom 拒绝；注册时强制
  solid=false + opaque=false + transparent=true——十字植物可穿过、不遮光、
  走混合通道）。`ChunkMesher` 面循环跳过 Cross 块（不出 cube 面），另跑一个
  固定格序的 cross 通道：每格 2 条交叉竖直面片（格的 2 条对角线，全纹理宽，
  纹理正立），**无邻居剔除**（面片在格内，被埋时深度测试自然遮挡），烘焙
  **自身格**的光，法线朝上 = 面 shade 1.0（MC 十字植物全亮）；进
  transparent 子网格，透明 pass 关 culling（双面可见）。物品 3D 图标
  （`makeItemCubeGeometry`）对 Cross 块输出同款中心对齐的两面片（花/
  仙人掌掉落物 + hotbar 图标；UI/hand 路径 blend 已开）。alpha cutout 无
  discard：透明纹素混合到透明（视觉正确）但仍写深度——与 leaves/glass
  同类既有简化（见 KNOWN_ISSUES 渲染）
- **脏标记增量重网格**：`setBlock` 标记本 chunk（及共享面的邻居 chunk）dirty，
  下一帧只重建 dirty chunk 的 GPU mesh
- **光照（Sky/Block Light 0-15）**：每个 chunk 两个 16³ 字节数组；天空光由列顶扫描
  （最高 opaque 方块上方=15）+ BFS 传播（垂直无衰减、水平每格 -1）；方块光由
  `properties.light_emission` 源 BFS（全方向每格 -1）。破坏/放置后只重传播受影响区域
  （天空光按变化点 ±16 的整列——垂直天空光无距离限制；方块光按 ±16 盒），光值实际
  变化的 chunk 才重建 mesh。渲染：每个面烘焙"面后方一格"的光值到顶点（块状光照），
  shader 取 `max(sky, block)` × MC 经典面着色（顶 1.0 / 侧 0.8、0.6 / 底 0.5）+ 3%
  环境光下限。glowstone（emission 15）在 hotbar 里，放进暗处会发光（**夜间也亮**——
  方块光不受昼夜影响）
- **昼夜循环**：游戏内时间（`world.day_length_seconds` 默认 240s 一天，
  `world.start_time_of_day` 默认 0.25=正午；`--time <0..1>` 可覆盖起点）。天空光按
  `dayFactor` 缩放（黎明 [0.05,0.15) / 黄昏 [0.45,0.55) smoothstep 渐变，白天=1、
  夜晚=0——MC 式呈现层近似，光值本身仍按正午存储）；天空背景色按时间关键帧插值
  （夜深蓝 → 黎明橙 → 昼蓝 → 黄昏橙 → 夜深蓝）；太阳/月亮是像素级圆形 billboard
  （40/28px，越靠近地平线越淡），太阳轨迹：t=0 正东地平线 → 正午天顶 → t=0.5 正西
  地平线 → 午夜天底（月亮恒在太阳对面）；夜空有 300 颗确定性星星（世界空间
  r=800 上半球，`splitmix64` 哈希生成）。时间按每帧固定 1/60s 推进（不用墙钟），
  保证无输入运行逐字节可复现
- **数据驱动方块**：15 个方块（stone/dirt/grass/sand/wood/leaves/glass/water/bedrock/glowstone/crafting_table +
  4 种矿石）+ 内置 air 哨兵（`BlockID{0}`，非数据方块，注册表从 id 1 分配）
- 16×16 瓦片 TextureAtlas（Minecraft 同款规格），**布局数据驱动**
  （`assets/data/textures/block_atlas.json`，4×8；row 4 = 工作台顶/侧 + planks；
  新增纹理 = 加 PNG + 加一行 JSON，不改代码）
- 光照贴图（lightmap 顶点烘焙，Phase 7）+ 昼夜循环（Phase 8）
- 窗口标题实时显示 FPS 与 draw call 数
