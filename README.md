# MineCraft

一个以 Minecraft 为设计参考的 3D 体素沙盒游戏，从零构建，目标是**可运行、可扩展、可维护**的长期项目。

## 技术栈

| 领域 | 选择 | 说明 |
|---|---|---|
| 语言 | C++20 | |
| 构建 | CMake + Ninja | |
| 窗口/输入 | SDL3 | |
| 图形 | OpenGL 3.3 Core (GLEW) | |
| 音频 | SDL3 Audio（`SoundBank`，SDL 内置音频后端） | Phase 15 引入，8 个程序化 WAV |
| 图像 | 内置 PNG 编解码器（`src/core/png.hpp`，zlib） | 8/16 位 RGB/RGBA/灰度/调色板，全 filter，CRC 校验 |
| 测试 | GoogleTest | |
| JSON | 内置实现（`src/core/json.hpp`） | 无外部依赖 |

## 构建与运行

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行（打开窗口，ESC 或关闭窗口退出）
./build/bin/minecraft

# 运行 120 帧后自动退出（可用于自动化验证）
./build/bin/minecraft --frames 120

# 运行 300 帧并保存最后一帧截图
./build/bin/minecraft --frames 300 --screenshot shot.png

# 其他参数
./build/bin/minecraft --config config/config.json --seed 42 --log-level debug

# 从指定时刻开始（0 = 黎明前, 0.25 = 正午, 0.5 = 日落, 0.75 = 午夜）
./build/bin/minecraft --time 0.75

# 存档槽（Phase 13）：启动时加载 data/saves/<slot>.json（若存在），干净退出时写回
./build/bin/minecraft --save myworld
# 忽略现有存档、从全新世界开始（退出时覆盖该槽）
./build/bin/minecraft --save myworld --new
```

**窗口**：大小由 `config/config.json` 的 `window.width`/`window.height` 决定（默认
1280×720）。窗口模式下窗口可被用户缩放（`window.resizable`，默认 `true`）——
视口、投影纵横比与屏幕 UI 布局每帧读取窗口当前像素尺寸，缩放后场景立即跟随
（最小尺寸 320×200）；设为 `false` 则锁定在配置尺寸。

## 当前可玩内容（Phase 21）

- **第一人称玩家**：`W/A/S/D` 行走，`Space` 跳跃（仅贴地时）/游泳时随时上浮，
  `LeftShift` 冲刺，鼠标转向（**左键点击锁定指针；转向仅在指针锁定时生效**，
  未锁定的光标不会转动视角——保证无输入运行下截图可逐字节复现）
- **生存系统**（Phase 16，`Player` 扩展）：
  - **生命值 / 受伤 / 死亡**：`maxHealth=20`（10 颗心，每颗 2 HP）；
    **坠落伤害** = `max(0, 净下落格数 − 3)` HP（从离地帧记录 `fallStartY`，
    落地时结算；落入水中的落地取消伤害）；掉出世界（y < −32）= 直接死亡
    （`damage(maxHealth×1000)`）。死亡后冻结玩家、显示 "You died! / Press R"，
    `R` 重生 = 回出生点 + 满血 + 速度清零（**背包保留**）
  - **自然回复**：脱离受伤状态 `regenDelay=10s` 后，每 `regenInterval=4s` 回 1 HP
  - **受伤反馈**：受击瞬间屏幕红色闪光（`0.35×剩余/0.5s` 渐隐）+ 播放 "hurt"；
    死亡播放 "death"。游戏轮询生命值（`m_prevHealth`）驱动，致死伤不重复播放
  - **潜行**（`LeftControl`）：身高 1.8→1.6（脚底不动）、眼高 1.62→1.42、
    移动速度 ×0.3（水中除外）；潜行时**不**触发 1 格自动台阶
  - **1 格自动台阶（step-up）**：非潜行 + 贴地 + 身体贴住整数块面 + 面前脚位
    为 solid + 其上方 2 格为空 → 自动抬升 1 格并继续本 tick 的水平移动
    （无需跳跃即可爬上 1 格台阶，2 格仍不行）
  - **玩家 ↔ 掉落物分离**：玩家与物品实体不再互相穿过——沿最小穿透轴把
    物品推开（远离玩家中心，面到面跨度）；玩家自身不被动位移
  - **生命 HUD**：hotbar 上方 10 颗心（16px、2px 间隔），三态贴图（满 / 半 / 空）
    取自 48×16 条带（`assets/textures/health.png`，`tools/generate_health_icons.py`）；
    填充 = `clamp(hp − 2i, 0, 2)`（半心按 0.5 HP 粒度渲染）。只在游玩态（锁定 +
    背包未开）显示，无输入运行零新增绘制
- **Mob 与 AI**（Phase 20，`MobEntity`/`MobRegistry`，全数据驱动；数据含
  zombie（敌对）+ cow（被动，Phase 22））：
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
     15 > 0）不生成、夜晚生成；cow（`spawn_max_light` 15）任何光照/时间
     都可生成（被动 mob 约定）；萤石照亮处即使夜晚也不生成 zombie；树冠/
     洞穴的永久暗格（存储天空光 0）白天也可生成（与 Minecraft 一致）。
     类型在每次尝试时按数据名单确定性抽取（种子 ⊕ 帧号），抽中的类型过不
     了光门则该次尝试作废（如白天抽中 zombie）
  - **AI 状态机**（确定性：种子 = 世界种子 ⊕ 生成序号，splitmix64 流）：
    **Wander**（默认；每 `wander_interval` 秒一次种子随机决策：1/3 站定、
    其余半速随机方向走）→ **Chase**（玩家进入 `detect_radius`（水平）且高度差
    ≤2：全速追）→ **Attack**（水平距离 ≤`attack_range` 且高度差 ≤1：停下、
    面向玩家、每 `attack_cooldown` 秒打 `attack_damage` 点——走 P16 的玩家
    受伤路径：红闪/音效/死亡全由既有轮询驱动）。玩家死亡时不追不打。
    **被动 mob**（Phase 22）：`detect_radius`/`attack_range`/
    `attack_damage` 三个字段允许 0（校验 `>= 0`，负数仍拒绝）——0 值使
    追/击条件永不成立，cow 只有 Wander 状态，纯数据表达，AI 代码无特判
  - **玩家攻击**：左键命中 mob（射线先比方块距离——**方块遮挡**：方块更近则打
    方块）= 挥击，按住时每 `0.25s` 一击：伤害取**选中物品的 `attack_damage`**
    数据（徒手/方块/材料=1.0，工具见 `data/items/*.json`，木镐 2 / 铁镐 3）+
    沿瞄准方向击退（水平 4 + 抬升 2.5，0.3s 击退窗口内 AI 不接管速度）
  - **死亡**：HP 归零 → 掉落 `drops` 数据定义的物品（zombie 掉
    `rotten_flesh`，cow 掉 `leather`——均为 material 物品 + 图标）后移除；
    击退/坠落无伤害（v1）
  - **渲染**：box 模型（zombie = 头 0.5³ + 身 0.5×1.0×0.3；cow = 头
    0.45³ + 身 0.8×0.7×1.1、AABB 1.2×1.5 比 zombie 宽，各 6 面同一 atlas
    tile，`tools/generate_mob_textures.py` 生成 zombie 绿皮肤+深色眼/青色
    上衣、cow 棕底白斑毛+深色眼），
    与掉落物同 pass（`translate(位置) × rotateY(朝向)`，朝向 = 相机 yaw 约定，
    箱体的 −Z 面是脸）；天空光全亮烘焙（与掉落物同一简化，无局部光）
  - **开关**：config `mobs.enabled`（默认 true）；**mob 不入存档**（与 Minecraft
    一致：生成物是瞬态的，其掉落物是物品、随存档保存）
- **弓与箭**（Phase 21，物品数据驱动发射 + P10 `Projectile`）：
  - **获取**：shaped 配方 `bow`（工作台：3 stick + 1 planks 的 C 形，
    `data/recipes/bow.json`）；material 物品 `bow`（`data/items/bow.json`，
    `max_stack 1`，图标 `bow.png`）
  - **发射**：选中 `shoot_speed > 0` 的物品（发射器）时**左键 = 射箭**——按住
    每 `0.25s`（挥击同款节奏）射一支，**不再**挖方块/挥击（Minecraft：弓的
    左键是射击）；箭从眼前 0.3 格沿瞄准方向以物品 `shoot_speed` 数据（bow = 15，
    MC 满威力）出发，播放 "shoot"
  - **箭**（P10 `Projectile` 实体）：重力 20 b/s²（MC 箭 0.05/格/tick²）、
    终端 38、撞 solid 即亡（v1 无"插在墙上"）、30s 寿命、世界下方杀界；
    **固定帧步长** `kProjectileStepPerFrame`（运动实体截图可复现约定）
  - **箭矢命中**：每帧对每支箭做**扫掠 AABB 测试**（上一步位置→本步位置，
    `raycastAabb` 位移为方向、t∈[0,1]）对每个存活 mob——命中则按箭携带的
    `shoot_damage` 数据（bow = 5，zombie 4 箭毙）伤害 + 沿箭水平速度的
    近战式击退，箭被消耗；箭**穿过**玩家与其他实体（`collidesWithEntities
    = false`），不伤玩家（单人生存 v1）
  - **渲染**：细盒体（长 0.55 × 粗 0.05，本地 −Z = 飞行方向）纯色
    （基本着色器 `u_useTexture=0` 路径，无箭纹理），朝向 = `rotateY(yaw) ×
    rotateX(pitch)` 把本地 −Z 转到速度（相机同款约定，`arrowOrientation`
    纯函数）；天空光全亮烘焙（随昼夜变暗）
  - **v1 简化**（KNOWN_ISSUES.md）：无蓄力（永远满威力）、无箭物品/弹药消耗
    （无限箭）、箭不入存档（瞬态，同 mob）、无插墙残箭、不伤玩家
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
- **游泳**（Phase 9）：玩家包围盒与任何水体格相交即进入水中状态——水中重力减半
  （12 vs 32）、终端速度 4、水平速度降为 3.1（冲刺无效）、`Space` 随时上浮（4.5）；
  浅水（头在水面上）可以"蹚水走"，深水可以游
- **方块交互**（DDA 体素 raycast，reach 5 格）：
  - `左键`（锁定时，**按住**）：破坏准星指向的方块，耗时数据驱动
    （`1.5s × properties.hardness ÷ 当前选中物品的 break speed`，徒手=1.0；
    `properties.destroyable` 数据驱动，基岩不可破坏）。准星移开/松键/换工具
    进度清零。raycast 把水体当 air 穿过——可以隔水破坏湖底/水下的方块
    （水会被破坏排水、随后流体模拟把它补回）
  - `右键`（锁定时）：先试 interact——命中格有 `interact` 属性（当前仅工作台）
    则打开对应面板、不放置；否则在命中面的邻格放置**当前选中格**的方块物品，
    并**消耗 1 个**（方块不再无限——背包是方块的来源；工具不可放置）；不能
    放进 solid 方块或玩家自身包围盒内
  - `Tab`：切换选中的 hotbar 格（9 格）
  - `Q`（锁定时）：丢弃选中格 1 个到脚下（生成掉落物，0.5s 拾取延迟防立即回吸）
  - `ESC`（锁定时）：释放指针；未锁定时 ESC 退出游戏
  - 破坏/放置复用 `setBlock` 脏标记管线，下一帧自动重建受影响 chunk 的 mesh
- **游戏内 UI**（Phase 15，专用无光照 UI 着色器 + 批量化绘制；
  所有 UI 元素只在按键/指针锁定时渲染，无输入运行渲染零新增 → 基线截图逐字节不变）：
  - **准星**（指针锁定时屏幕中央白色十字）与**目标方块描边**（24 顶点线框立方体，
    描出 raycast 命中格）——只在游玩态（锁定 + 背包未开）显示
  - **hotbar**（锁定且背包关闭时）：底部 9 格，选中格橙色边框，堆栈数 < max 时
    显示数量；**手持物品**右下角大图标（76px）
  - **物品图标**：block item = 带光照的倾斜 3D 立方体（复用掉落物 cube mesh +
    基本着色器，白天亮夜晚暗，与掉落物同观感）；工具/材料 = 16×16 图标纹理
    （`Item.icon` 数据字段 → `assets/textures/items/<icon>.png`，
    `tools/generate_item_icons.py` 程序化生成）
  - **背包 + 合成面板**（`E` 开关；打开时释放指针、暂停玩家物理与方块交互，
    世界/昼夜/流体/实体/流式照常 tick）：27 格（hotbar 行 + 18 背包格）+
    2×2 合成格（共享 3×3 网格模型的左上 4 格）+ 右侧配方列表（**仅 shapeless
    配方**；产物图标 + 名称 ×N，白色=可合成/灰色=不可 + 原料清单）；
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
  背包开/关、合成成功、受伤/死亡（Phase 16）、射箭（Phase 21）。11 个程序化
  WAV（`tools/generate_sounds.py`，44.1kHz 16-bit 单声道），音量
  `config audio.volume`（默认 0.7）
- **物品与背包**（Phase 11/12，`Item`/`ItemStack`/`Inventory`，全数据驱动）：
  - 物品 = 纯数据（`ItemRegistry`，当前 24 种 = 14 block + 2 工具 + 8 材料）：
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
  全有或全无——失败背包逐位不变，产出永远放得下）。当前 5 配方：wood→4
  planks、2 planks→4 stick（shapeless）、2×2 planks→crafting_table、
  wooden_pickaxe（PPP/.S./.S.）、iron_pickaxe（III/.S./.S.）（均 shaped，
  Phase 17）。游戏内触发：背包面板（`E`）右侧配方列表点击行 = `craft()`
  （仅 shapeless，Phase 15）；或放置并右键工作台打开 3×3 面板合成（Phase 17）
- **存档/读档**（Phase 13，`--save <slot>`，`src/core/save.{hpp,cpp}`）：
  存档文件 `data/saves/<slot>.json` = **seed + 与生成态的 diff + 完整游戏状态**——
  不重复存储地形，只存"和这个 seed 生成的世界不同的格子"（`blocks`/`fluids`
  编辑列表，破坏=`"air"`）+ 玩家位置/朝向 + 昼夜时间 + 27 格背包 + 选中格 +
  掉落物实体（含 120s 寿命与拾取延迟计时器）。读档 = 正常从 seed 生成世界后
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
  `KNOWN_ISSUES.md`）。静止玩家（距边缘 ≥2 列）永不触发 → 无输入运行零 job，
  基线截图逐字节复现
- **WorldGenerator**（无状态、纯函数 `generateChunk`，worker 线程并行调用）：
  - **地形**：fBm 2D 噪声高度场（±24 格，基线 40），y=0 bedrock 层，深处 stone
  - **生物群系**：低频噪声 + 权重选择（`assets/data/biomes/*.json`：desert/forest/plains），
    各 biome 有自己的表层/次表层/树密度/高度系数
  - **水**：低于海平面（y=34）的柱体填充流体水（fluid level 8）到海平面；
    生成即处于平衡态（游戏内 20Hz 流体 tick 不会扰动湖面）
  - **洞穴**：3D 噪声阈值雕刻，保留表层 4 格与底层 2 格的壳
  - **矿脉**：世界空间格网 + 随机游走（coal/iron/gold/diamond，`max_y` 分层限制），
    跨 chunk 无缝；只替换 stone（不破坏洞穴）
  - **树木**：世界空间格网候选 + 数据驱动的树密度；树干 4~7 格 + 两层树冠 + 顶帽；
    跨 chunk 边界由邻接 chunk 各自补齐（边际格网机制），只写入 air（不破坏地形）
- **演示区 + 流式扩展**：启动时同步生成 4×4 chunk 列（64×64 方块）；玩家按
  seed 自动出生在干燥陆地上（候选列需满足：高于海平面 + 出生包围盒内无方块，
  避开树干/树冠）。走出演示区后世界按流式加载（上一项）继续扩展——X/Z 真正
  无限，走到哪加载到哪（半径 `world.view_distance` 列）
- **流体模拟（水，Phase 9）**：每个 chunk 一个 16³ 的 0-8 等级数组
  （8=满/源，1-7 流动；同一 cell 里水与方块互斥）。`FluidSim::tick`（游戏线程，
  每 3 帧一次 ≈20Hz，快照式确定性）：下落（下方空/流动则沿列传送带下移）、
  满格失撑也下落（对 MC 的刻意偏离）、静止在方块上水平扩散 L-1（1 级终端）、
  坐在**满水**上补满到 8（source 托底的柱是满的——破坏湖底后整列补满、湖面恢复）、
  坐在流动池上按等级吸收/抬升（水量不守恒，见 `KNOWN_ISSUES.md`）。
  `World::setFluid` 只写已存在 chunk 的 air 格；`setBlock` 会把所在格的水排掉
- **水的光照衰减**：天空光每穿一格水 -3（列顶扫描 + BFS 与不透明方块共用传播器）；
  深水湖底随深度变暗（截图可见浅水亮、深水暗的渐变）
- **半透明水体渲染**：`ChunkMesher::emitFluid` 输出 level/8 缩放的立方体（流动水
  顶面半高），水-水内部面剔除，流体面烘焙**自身格**的光；shader 使用纹理 alpha
  （水 α≈0.78 / 玻璃 α≈0.21 / 叶子 α≈0.92 + 透明孔），湖面可透视湖底
- **Face Culling 网格化**：只渲染暴露给 air/非 opaque 邻居的面；跨 chunk 边界通过
  `World` 查询邻居剔除；每个 chunk 输出 opaque + transparent 两个 mesh，
  渲染顺序不透明 → alpha 混合（leaves/glass/water）
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

### 新增一个生物群系 / 矿石（无需改核心代码）

1. 生物群系：`assets/data/biomes/<name>.json` —— 权重/表层/次表层/深度/树密度/高度系数：

   ```json
   {
     "id": "swamp",
     "weight": 3,
     "surface": "grass",
     "subsurface": "dirt",
     "subsurface_depth": 3,
     "tree_density": 0.08,
     "height_scale": 0.8
   }
   ```

2. 矿石：`assets/data/worldgen.json` 的 `ores.veins` 数组加一项（方块需已注册）：

   ```json
   { "block": "diamond_ore", "probability": 0.05, "vein_size": 5, "max_y": 20 }
   ```

地形/洞穴/树木参数同样在 `assets/data/worldgen.json`（缺省=默认值，类型错误=启动报错）。

### 调整初始 hotbar / 新增工具与材料

`assets/data/hotbar.json` 的 `items` 数组（游戏内 `Tab` 循环切换 9 格，`Q` 从选中格丢弃）：

```json
{ "items": ["stone", "dirt", "grass", "sand", "wood", "leaves", "glass", "glowstone", "wooden_pickaxe"] }
```

物品名必须已注册（未知名=启动报错）；列表不能为空、至多 9 项；每格初始为满堆栈
（block 64 / 工具 1 / 材料 64）。新增工具或材料 = `assets/data/items/<name>.json`：

```json
{
  "id": "wooden_pickaxe",
  "type": "tool",
  "tool": { "break_speed": 2.0 },   // 仅工具；>0，默认 1.0（破坏耗时 = 1.5s × hardness ÷ speed）
  "attack_damage": 2,               // 可选（Phase 20）：挥击伤害，(0, 100]，默认 1.0
  "shoot_speed": 15.0,              // 可选（Phase 21）：发射器速度，(0, 100]；>0 = 投射物发射器（弓）
  "shoot_damage": 5.0,              // 可选（Phase 21）：箭矢命中伤害，[0, 100]，默认 0
  "max_stack": 1,                   // 1..64，默认 64
  "icon": "wooden_pickaxe"          // UI 图标（Phase 15）：assets/textures/items/<icon>.png
}
```

```json
{ "id": "stick", "type": "material", "icon": "stick" }   // 材料：无 tool 字段，max_stack 默认 64
```

`icon` 指向 `assets/textures/items/<icon>.png`（16×16 RGBA，可多个物品共享同一
图标文件；文件缺失 = 启动报错；缺省=不画图标）。block item 没有独立 JSON——
每个数据方块隐式生成同名 item（方块定义是唯一来源，UI 里以带光照的 3D 立方体
呈现，不用 2D 图标）；显式物品只能是 `type: "tool"` 或 `type: "material"`
（材料带 `tool` 字段=启动报错，防止第二数据源冲突）。非 block 物品（工具/材料）
不可放置；它们的**掉落物在 world 中仍无实体表示**（UI/手持/背包图标已有，
见 KNOWN_ISSUES.md）。

### 新增一个方块（无需改核心代码）

1. `assets/textures/blocks/<name>.png` —— 16×16 RGBA 纹理（可参考 `tools/generate_block_textures.py`）
2. `assets/data/blocks/<id>.json` —— 方块定义（属性/掉落/模型/纹理），例如：

   ```json
   {
     "id": "glowstone",
     "properties": { "hardness": 0.3, "light_emission": 12, "drops": "glowstone" },
     "model": { "textures": { "top": "glowstone", "side": "glowstone", "bottom": "glowstone" } }
   }
   ```

   属性缺省即默认值（solid/opaque/destroyable=true, hardness=1.0, light_emission=0, drops 为空=不掉落, drops_count=1）。
   可选 `interact: "crafting_table"`（Phase 17）：锁定时右键命中该方块打开
   工作台面板而非放置——值必须来自已知交互集（当前仅 `crafting_table`，
   其他值 = 启动报错）。
   校验严格：类型错误、越界值、重复 id、缺失纹理面都会报明确错误；未知字段警告但不拒绝。
   `drops` 是**物品名**（游戏启动时对 ItemRegistry 严格校验，未知名=启动报错）——
   block item 与方块同名，所以 `grass → "dirt"` 表示破坏 grass 掉 dirt；
   矿石掉材料物品（`iron_ore → "iron_ingot"`）。`drops_count`（1..64，默认 1）
   是单次破坏的掉落数量，须 ≤ 掉落物品的 max_stack（init 交叉校验）。
   `hardness` 驱动破坏耗时（按住左键，`1.5s × hardness ÷ 选中物品 break speed`，
   hardness ≤0 且可破坏=即时破坏）。

### 新增一个配方（无需改核心代码）

`assets/data/recipes/<id>.json`（shapeless：原料袋，顺序无关；产物必须装进一个
堆栈）：

```json
{
  "id": "iron_pickaxe",
  "output": { "item": "iron_pickaxe", "count": 1 },
  "ingredients": [
    { "item": "iron_ingot", "count": 3 },
    { "item": "stick", "count": 2 }
  ]
}
```

校验严格（经 ItemRegistry 交叉检查，错误信息点名配方）：id 非空且唯一；
`output.item`/原料物品名必须已注册；`output.count` ∈ [1, 产物 max_stack]；
shapeless：原料非空、数量 ≥1、同一物品不得重复出现（合并为一条）；
shaped（Phase 17）：`pattern` 1..3 行、行长 ≤3、空格=空位、非空字母必须在
`key` 中定义且指向已注册物品；未知字段警告。
`craft()` 语义：原料按槽序升序消耗（hotbar 先于背包），先在背包副本上模拟
"消耗+入包"，成功才提交——失败时背包逐位不变（不会扣料却放不下产物丢物品，
产物是原料之一的配方也正确）。网格配方（Phase 17）另走 `CraftingGrid` 匹配：
pattern 偏移扫描（任意左上偏移放进 3×3、空格对应格必须为空），先 shaped 后
shapeless、配方按文件名字典序。当前配方链：wood→4 planks→(2 planks→4
stick)、2×2 planks→crafting_table、wooden_pickaxe（PPP/.S./.S.）、
iron_pickaxe（III/.S./.S.）。
游戏内触发：`E` 背包面板右侧的配方列表（仅 shapeless），点击配方行执行
`craft()`（Phase 15；可合成的配方白色高亮、不可合成的灰色）；或放置并右键
工作台打开 3×3 面板，按形状放料后取输出格（Phase 17）。

### 新增一个 Mob（无需改核心代码，Phase 20）

1. `assets/textures/mobs/<tile>.png` —— 16×16 RGBA 纹理（`tools/generate_mob_textures.py`
   同款：splitmix64 确定性噪声 + 简单五官）；并在 `data/textures/block_atlas.json`
   的 `tiles` 里给它一个空位 `[col, row]`（4×8 网格，现有 21 tile，勿动已有条目）
2. `assets/data/mobs/<id>.json` —— mob 定义（行为参数 + box 模型，每箱一个
   atlas tile；box 必须在 `width × height` 的 AABB 内）：

   ```json
   {
     "id": "zombie",
     "properties": {
       "health": 20, "speed": 0.8,
       "detect_radius": 16, "attack_range": 1.6,
       "attack_damage": 3, "attack_cooldown": 1.0, "wander_interval": 2.5,
       "spawn_max_light": 0,
       "drops": "rotten_flesh", "drops_count": 1
     },
     "model": {
       "width": 0.6, "height": 1.8,
       "boxes": [
         { "pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "zombie_head" },
         { "pos": [0.0, 0.5, 0.0],  "size": [0.5, 1.0, 0.3], "texture": "zombie_body" }
       ]
     }
   }
   ```

   校验严格（同方块/物品注册表纪律）：id 非空且唯一、数值字段 >0（唯三例外
   `detect_radius`/`attack_range`/`attack_damage` 允许 0 = 被动/无害，负数
   拒绝）、`spawn_max_light` 为 0..15 的整数（默认 0，光照门——见上）、
   `drops_count` ∈ [1,64]、box 越界/缺纹理名 = 明确报错；未知字段警告。
   `drops` 是**物品名**（init 时对 ItemRegistry 交叉校验，`""` = 不掉落）。
3. 生成名单：`assets/data/mob_spawning.json` 的 `mobs` 数组加入该 id
   （生成节奏/环半径/上限/离玩家消散距离都在这个文件里，全部数据驱动）。

v1 数据有 zombie（敌对：绿色、只在黑暗生成（`spawn_max_light` 0））与
cow（被动：棕白斑毛、`detect_radius`/`attack_range`/`attack_damage` 全 0、
`spawn_max_light` 15 任何时间可生成、掉 `leather`）；两者都不游泳不拆方块
无动画——见 KNOWN_ISSUES.md 的 Mob 章节。

### 使用存档（Phase 13）

```bash
./build/bin/minecraft --save myworld      # 首次：新游戏；此后：读档
./build/bin/minecraft --save myworld --new  # 推倒重来（退出时覆盖 myworld 槽）
```

- 存档槽名只允许字母/数字/`_`/`-`（防路径穿越）；文件在 `data/saves/<slot>.json`
  （git 忽略），JSON 格式化输出、可直接阅读
- **存什么**：seed、`time`（timeOfDay [0,1)）、玩家 pos/yaw/pitch、背包 27 格 +
  选中格、掉落物实体（item/count/pos/age/pickup_delay）、`blocks` 与 `fluids`
  编辑列表（仅与 `generateChunk(seed)` 不同的格子，按位置排序）。一次没动过世界
  的存档 edits 为空。**mob（Phase 20）与箭（Phase 21）是瞬态实体**：保存时跳过
  （读档后不出现），不是错误；未知实体种类才会保存失败
- **读档语义**：`--time` 只对全新世界生效（已加载存档时存档时间优先）；
  `--seed` 与存档 seed 不一致 = 启动报错（编辑会落到错误地形上）；存档里的
  玩家位置/朝向覆盖出生点与初始视角
- **何时写**：干净退出（ESC 二次/关窗/`--frames` 到点）自动写回；游戏内 **`F5`
  立即存档**（Phase 15，`--save` 槽激活时，日志记录成功/失败）；初始化失败或
  崩溃不写（保护旧档）。写失败只记 ERROR 日志、退出码仍为 0（本局已正常运行）
- 存档是机器生成的：字段集由 format 号锁定（当前 format 1），加载器对未知字段
  一律报错（与用户手写数据文件的"警告但容忍"策略相反）

## 测试

```bash
ctest --test-dir build --output-on-failure
# 或直接运行
./build/bin/minecraft_tests
```

## 目录结构

```
mine-craft/
├── assets/          # 游戏资源（textures/ models/ shaders/ sounds/ data/）
├── config/          # 用户配置（config.json，不存在时自动生成默认值）
├── data/            # 存档目录（data/saves/<slot>.json，Phase 13）
├── logs/            # 运行日志（logs/mine-craft.log）
├── src/
│   ├── block/       # 数据驱动方块系统：Block 数据模型 + BlockRegistry（JSON 加载/校验）
│   ├── core/        # 核心框架：日志、错误处理、JSON、配置、资源、窗口、输入、相机、PNG、
│   │                # 存档（save：SaveGame + diffWorld/applyWorldEdits + 严格序列化/解析，
│   │                # 无 GL 依赖可单测）、sound（SoundBank：SDL3 音频流队列 + WAV 解析，
│   │                # 初始化失败优雅降级静音）、主循环
│   │   └── math/    # 数学库（header-only）：Vec2/Vec3/Vec4/Mat4
│   ├── player/      # 玩家：第一人称状态 + 输入驱动移动（继承 Entity 的统一碰撞物理）
│   ├── entity/      # 实体系统：Entity 基类（底面中心位置/子步化轴分离碰撞/滑动/kill plane）、
│   │                # ItemEntity（掉落物：摩擦停驻/同类合并/120s 消亡）、Projectile（投射物：
│   │                # 受重力抛物线/撞 solid 即钉住）、particle（破坏粒子：确定性爆发/
│   │                # 半隐式 Euler/无碰撞/spawn 烘焙光照）、item_mesh（掉落物完整立方体几何，
│   │                # 与 chunk mesher 共享 cube_face_table）
│   ├── item/        # 物品系统（Phase 11/12）：Item/ItemStack 数据模型（含 icon 字段）+ 破坏耗时公式、
│   │                # ItemRegistry（方块隐式同名 block item + 显式工具/材料 JSON 严格校验）、
│   │                # Inventory（27 格 = 9 hotbar + 18 背包，确定性合并/入包/take/setSlot）、
│   │                # Recipe/RecipeRegistry（JSON 配方 + 经 ItemRegistry 交叉校验）、
│   │                # canCraft/craft（确定性、全有或全无的合成纯逻辑）
│   ├── ui/          # 界面（Phase 15，纯逻辑无 GL 可单测）：font（8x8 字形 atlas UV/文本
│   │                # 顶点生成）、debug_overlay（F3 行格式化）、inventory_ui（面板布局 +
│   │                # 左右键点击状态机，纯函数）
│   ├── renderer/    # 渲染器（GL 封装）：Shader/Texture/Mesh（Triangles/Lines）/Renderer
│   │                # （setDepthTest/setCulling）/TextureAtlas
│   └── world/       # 体素世界：coordinates（world/chunk/local 转换）、Chunk（块+光照数组）、
│                    # World、ChunkMesher（按面烘焙光照）、day_cycle（昼夜循环：时间/
│                    # dayFactor/天空色/日月方位/确定性星空）、raycast（DDA 体素射线+放置校验）、
│                    # lighting（Sky/Block Light 0-15 传播与局部更新，水每格衰减 3）、
│                    # fluid（FluidSim：0-8 等级水模拟——下落/扩散/吸收/补满）、
│                    # noise（确定性
│                    # value noise/fBm）、BiomeRegistry、WorldgenParams（worldgen.json）、
│                    # WorldGenerator（Seed 可复现生成）
├── tools/           # 开发工具：generate_block_textures.py（16x16 占位纹理）、
│                    # generate_font.py（8x8 字形 atlas）、generate_cracks.py（5 级裂纹条）、
│                    # generate_item_icons.py（16x16 物品图标）、generate_sounds.py（8 个 WAV）
└── tests/           # 单元测试（GTest）+ PNG 测试夹具（tests/data/，可用 tests/generate_fixtures.py 重新生成）
```

## 架构规划（分阶段）

- [x] **Phase 0** — 项目基础：构建系统、主循环、日志、配置、资源系统、错误处理、测试框架
- [x] **Phase 1** — 基础 3D 引擎：数学库、Input、Camera、Renderer（Shader/Texture/Mesh/VAO）、PNG 编解码、demo 场景
- [x] **Phase 2** — 方块系统：Block 数据模型、BlockRegistry（JSON 数据驱动 + 严格校验）、16×16 TextureAtlas、透明 pass
- [x] **Phase 3** — Chunk 系统：World/Chunk/坐标转换（负坐标安全）/Chunk Mesh（Face Culling；Greedy Meshing 留待性能需要时）
- [x] **Phase 4** — 世界生成：WorldGenerator（地形/生物群系/洞穴/矿脉/树木），Seed 可复现（无状态生成器，数据驱动 biome/ore/参数）
- [x] **Phase 5** — 玩家系统：第一人称移动、AABB 碰撞、滑动处理
- [x] **Phase 6** — Raycast：方块破坏/放置
- [x] **Phase 7** — 光照：Sky/Block Light（0-15），传播与更新
- [x] **Phase 8** — 昼夜循环：太阳/月亮/星星/天空色（dayFactor 缩放天空光）
- [x] **Phase 9** — 流体：水（0-8 等级/下落/水平扩散/水光衰减/游泳/半透明水面）
- [x] **Phase 10** — 实体系统：Entity 基类（统一 AABB 物理）/Player 并入/ItemEntity 掉落物/Projectile 投射物
- [x] **Phase 11** — 物品与背包：Item/ItemStack/Inventory、掉落物拾取、按住破坏（hardness/工具数据驱动）、Q 丢弃
- [x] **Phase 12** — 合成：数据驱动 Recipe（Recipe/Registry 严格校验 + canCraft/craft 纯逻辑 + `drops_count` + 材料物品；游戏内合成 UI 在 Phase 15 实现）
- [x] **Phase 13** — 存档：Seed + 修改数据（`--save <slot>`/`--new`，diff 式存档 + 严格校验 + 背包/实体/时间持久化）
- [x] **Phase 14** — Chunk Streaming + 多线程：ThreadPool（post/flush）+ worker 线程按列并行生成（view_distance 半径环，迟滞激活）+ insertChunk 边界标脏 + 增量光照 worker job（替代全量重传播）；基线逐字节不变
- [x] **Phase 15** — UI / 音效 / 粒子 / 性能统计：游戏内 UI（准星/目标描边/hotbar/手持/背包+合成面板/F3 overlay/F5 存档）、破坏裂纹+粒子、SDL3 音效、物品图标；UI 全按键/指针门控，无输入基线逐字节不变
- [x] **Phase 16** — 生存系统：生命值/坠落伤害/死亡与重生（R，背包保留）/自然回复/受伤红闪、潜行（LeftControl）、1 格自动台阶（step-up）、玩家↔掉落物分离、生命 HUD（10 心三态）；存档格式 2（player.health）；基线逐字节不变
- [x] **Phase 17** — 工作台 + 形状合成：shaped 配方 JSON（`pattern`/`key`，网格偏移扫描匹配，先 shaped 后 shapeless）/`CraftingGrid` 9 格网格（`matchesShaped`/`matchesShapeless`/`matchGridRecipe` 纯逻辑）/`BlockProperties.interact` 数据属性 + crafting_table 方块（右键开 3×3 面板、破坏掉自身）/工作台面板（独立容器网格，跨开关保留、入存档）/E 面板 2×2 = 共享 3×3 模型左上 4 格、配方列表仅 shapeless/存档格式 3（`inv_grid`+`table_grid`）/atlas 布局数据驱动（`block_atlas.json` 4×8，row 4 = 工作台顶·侧 + planks）；无输入基线逐字节不变
- [x] **Phase 18** — Greedy Meshing：逐面片按（纹理、sub-mesh、面光）合并为单 quad（光保守合并：面光不同不合并，光照零近似）/每平面双 slice（相邻非 opaque 方块间双面保留）/tiled UV 约定（合并 quad：`uv`=tile 索引 + `uvLocal`=tile 单位位置，`basic.frag` fract 回卷；单面片保持 legacy atlas UV 逐位精确）/`MeshVertex` +`uvLocal`+`tiled`（13 float）；200-chunk 实测总三角形 3.3× 下降（109,648 → 33,558）；无输入基线重拍（`base18`，3593 px 亚纹素重采样/剪影边缘差，几何覆盖集逐 chunk 100% 相等）
- [x] **Phase 19** — 通用实体间碰撞：Phase 16 玩家↔掉落物分离推广为通用分离 pass（固定序 body 列表：玩家先、实体按索引；重叠对沿最小穿透轴推离——两可推动体 50/50、对不可推动体全量面到面跨度；`hasRoomAt` 世界感知：被挡份额转由另一侧全量接管、双挡跳过；重复 sweep 至 <1e-6 块亚视觉残差或 32 次上限）/数据位 `collidesWithEntities`（投射物不参与，箭穿过实体）+ `pushableByEntities`（玩家不可推动）/中心精确重合保持 P16 方向（可推动体走 −axis，Q 丢脚底 → 玩家 −x 侧）/`Entity::hasRoomAt` 严格格扫描/`separateFrom` 上移 Entity 基类（P16 三测原样保留为回归锚）/F3 overlay +实体计数；无输入基线逐字节不变（`base18` 门通过），items 场景 ×2 字节一致 + 像素判据（stone 灰/dirt 棕分离位）
- [x] **Phase 20** — Mob 与 AI：数据驱动 mob 类型（`data/mobs/*.json` 严格校验：health/speed/detect_radius/attack_range/attack_damage/attack_cooldown/wander_interval/drops(+count)/box 模型（每箱一 atlas tile，须在模型 AABB 内）+ init 交叉校验（纹理/掉落/spawning 列表））/`MobEntity` AI 状态机（Wander 种子随机决策 → Chase 全速 → Attack 冷却打击，经 hit 回调走 P16 受伤路径；击退窗口抑制 AI；死亡掉落入 despawn pass）/确定性生成（帧节奏 + 固定扫描序首合法格：地板/头顶/高度带/玩家重叠/mob 间距；种子 = 世界种子 ⊕ 生成序号）/玩家攻击（左键 mob 优先于方块——方块遮挡；0.25s 挥击节奏；物品 `attack_damage` 数据 + 击退）/box 模型 mesh（共享六面表，`textures/mobs` 并入 atlas：zombie 头/身 2 tile，atlas 21 tile）/mob 固定帧步长（`kMobStepPerFrame`，运动实体截图逐字节可复现的日循环同款约定）/zombie 掉 `rotten_flesh`（新 material 物品+图标）/config `mobs.enabled`（默认 true；`first_spawn_frame=60` 保证 60 帧基线无 mob）；无输入基线逐字节不变（`base18` 门通过），mobs 240f ×2 字节一致 + 像素判据（vs 无 mob 对照：世界区 54k px 差、8.4k 绿皮肤 px、UI 零差、无伤害）
- [x] **Phase 20.5** — 修复两项用户反馈：① 昼夜生成规则——mob 数据字段 `spawn_max_light`（0..15 默认 0，Minecraft 敌对 mob 约定）+ `findMobSpawnCell` 光照门（生成格脚位的当前有效光照 = floor(天空光×dayFactor) 与方块光取大，≤ 阈值才生成；zombie 白天不生成、夜晚/暗格生成）；② 窗口缩放——`window.resizable`（默认 true）+ `Window::width/height` 每帧实时读像素尺寸（SDL3 `SDL_GetWindowSizeInPixels`），视口/投影纵横比/UI 布局/截图全部跟随窗口，最小 320×200。588/588 单测；E2E：base18 门 ×2 字节不变、白天 mobs-on == mobs-off（字节级证明白天不生成）、夜晚（day_length 1200s + start 0.536 黄昏段）mobs ×2 字节一致 vs 对照 54k px 差（bbox 与 P20 相同）、resize 运行 截图 == 窗口实际尺寸（场景跟随缩放）
- [x] **Phase 21** — 弓 + Projectile 游戏内发射器 + 箭渲染：物品数据字段 `shoot_speed`/`shoot_damage`（`(0,100]`/`[0,100]`，缺席=非发射器；严格校验）+ bow 物品（material，max_stack 1，图标）+ shaped 配方（3 stick + 1 planks C 形）/发射器语义（选中发射器时左键=射箭，不再挖掘/挥击；按住 0.25s 节奏；箭 = P10 `Projectile`：数据速度/伤害、眼前 0.3 格沿瞄准、固定帧步长 `kProjectileStepPerFrame`）/`Projectile` +`damage()`/`kill()`（非方块来源消耗，同 despawn 路径）/箭矢↔mob 每帧扫掠 AABB 命中（`raycastAabb` 位移方向 t∈[0,1]，首命中伤 `shoot_damage` + 近战式击退，箭消耗；穿过玩家/其他实体、不伤玩家）/箭渲染（0.55×0.05 细盒体纯色 `u_useTexture=0` 路径，`arrowOrientation` 纯函数把本地 −Z 转到速度，相机同款 rotateY×rotateX 约定）/`shoot` 音效 + 弓图标/存档守卫修正（mob/箭为瞬态实体，保存时跳过而非报错——修复 P20 遗留的"存活 mob 时 --save 退出保存失败"）；600/600 单测（+12：shoot 字段解析/默认/拒绝 ×5、箭几何 4、朝向 3、`kill()` 1）；E2E：base18 门 ×2 字节不变、bow 存档运行（热栏 0 = bow）×3 字节一致（运动实体确定性）、vs 无射击对照 616 px 差（2 支飞行箭、屏幕中央竖直带 x[632,647] y[338,489]、93% 箭棕色像素、UI/心带零差）

- [x] **Phase 22** — 更多 mob（纯数据）：被动 mob 落地——`detect_radius`/`attack_range`/`attack_damage` 校验放宽为 `>= 0`（0 = 永不追/永不攻击/无害，负数与其余必填字段 0 仍拒绝；AI 状态机对 0 值天然不触发 Chase/Attack，零特判代码）/cow（`data/mobs/cow.json`：HP 10、speed 0.5、全 0 攻击字段、`wander_interval` 3.0、`spawn_max_light` 15 = 任何光照/时间可生成、掉 `leather`；box 模型 头 0.45³ + 身 0.8×0.7×1.1、AABB 1.2×1.5——首个比 zombie 宽的模型）/`leather` material 物品 + 图标/cow 纹理（棕底白斑毛 + 深色眼，2 tile，atlas 21→23）/`mob_spawning.json` 名单 [zombie, cow]——类型按名单确定性抽取（种子 ⊕ 帧号），过不了光门的尝试作废（白天抽中 zombie）；E2E：base18 门 ×2 字节不变（atlas +2 tile 对既有渲染零影响）、白天 240f mobs-on ×2 与 mobs-off ×2 各自逐字节一致、vs 对照 7578 px 差（bbox x[631,743] y[244,326] 单一 cow 轮廓，77% cow 棕 + 1% 白斑 + 22% 边缘/暗部，**zombie 绿 0 px** = 白天敌对压制保持，UI/心带零差）；603/603 单测（+3：passive 解析、负数/零拒绝 ×7 用例、passive AI 300 帧不追不打）

## 开发约定

- 游戏逻辑不直接调用 GL/SDL API —— 图形走 Renderer（Phase 1），窗口走 `Window`。
- 数据驱动：方块/物品/配方通过注册表（Registry）管理，新增内容 = 资源 + 数据，不改核心代码。
- 世界坐标 / Chunk 坐标 / Local 坐标严格区分，转换函数集中定义。
- 错误必须显式传播（`Result<T>` + `MC_TRY`），不允许静默失败。
- 每个阶段完成后：编译 → 运行 → 测试 → 修复 → 总结，再进入下一阶段。
