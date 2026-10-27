# 架构

> v1.x 整理（LGT-4 时点）。本文 = 目录结构 + 核心架构速览；逐阶段细节在
> `docs/archive/phase-*.md`，协议与硬规则在 `docs/PROTOCOL.md`，玩法机制在
> `docs/MECHANICS.md`，加内容在 `docs/CONTENT_GUIDE.md`。

## 目录结构

```
tokenmine/
├── assets/          # 游戏资源（textures/ models/ shaders/ sounds/ data/）
│                    # data/ = blocks/ items/ recipes/ biomes/ mobs/ effects/ +
│                    # worldgen.json（内容全数据驱动：加方块/物品/配方/Mob = JSON）
├── config/          # 用户配置（config.json，已提交文件：view_distance 80 /
│                    # day_length 600 用户调值，不还原；内容改动仅用户要求时做）
├── data/            # 存档目录（data/saves/<slot>.json，format 19）
├── logs/            # 运行日志（logs/tokenmine.log）
├── src/
│   ├── block/       # 数据驱动方块系统：Block 数据模型（properties + model +
│   │                # Shape：cube/slab/stairs/wall/fence/glass_pane/cross…）
│   │                # + BlockRegistry（JSON 加载/严格校验/未知键点名）、
│   │                # block_sound（CRAFT-3 材质音效引用校验）、chance_drop
│   │                # （加权多掉落确定性 roll）、cube_face_table（P10，绕序回归
│   │                # 测试守护）
│   ├── core/        # 核心框架：logger / error(Result+MC_TRY) / json / config
│   │                # （点号键 + 严格读取纪律）/ asset_manager（相对路径+缓存）/
│   │                # window / input（SDL 事件唯一消费者→GameKey）/ camera /
│   │                # png（自研）/ math（Vec2/3/4/Mat4 header-only）/
│   │                # save（SaveGame format 19 + diffWorld/applyWorldEdits +
│   │                # 严格双向交叉校验 + 原子写 writeAtomicFile + 增量 journal
│   │                # 支持，无 GL 依赖可单测）/ sound（SoundBank 立体声 +
│   │                # 16 块距离衰减/线性声像空间化 CRAFT-3 + 环境音钩子）/
│   │                # status_effect（状态效果双载体：玩家+Mob，纯数据驱动）/
│   │                # food / bow / difficulty（纯函数逻辑层）/
│   │                # thread_pool（固定 4 worker，post→flush 帧同步窗口）/
│   │                # game（主循环+场景+全部 per-block 状态宿主：炉子/营火/
│   │                # 工作台 map + 信标 + 作物 + 粒子 + 音频事件）/ main
│   ├── player/      # 玩家：第一人称（继承 Entity 统一物理）+ drown（氧气窒息
│   │                # 纯函数）+ 护甲减伤（item/armor 纯函数）
│   ├── entity/      # 实体系统：Entity 基类（底面中心/子步化轴分离碰撞/滑动/
│   │                # stepUp 踏阶/共享流体模型 refreshFluidState+integrateFluid）/
│   │                # MobEntity（数据驱动 AI 状态机 chase/wander/attack/远程）/
│   │                # mob_registry（mobs/*.json 严格解析）/ mob_spawning
│   │                # （夜间 + 光照门 spawn_max_light）/ mob_mesh /
│   │                # mob_swing（ENT-1 四肢摆动：16.16 定点正弦表+穿模谓词
│   │                # mob_swing_clip）/ ItemEntity（掉落物：摩擦停驻/同类合并/
│   │                # 拾取延迟/120s 消亡/流体浮沉/半浸没终态）/
│   │                # item_mesh（块立方掉落）/ item_model_mesh（CRAFT-2 盒模型
│   │                # 纯几何）/ item_light（PRE-6 掉落世界光采样）/
│   │                # Projectile（箭：重力抛物线/钉住/尾部效果 SURV-5）/
│   │                # arrow_mesh / particle（确定性爆发/逐粒重力/流体交互）
│   ├── item/        # 物品系统：Item/ItemStack（+damage float SURV-3）+
│   │                # 破坏耗时公式（effectiveBreakSpeed fast_against 分块倍率）/
│   │                # ItemRegistry（块隐式同名 item + 显式 JSON 严格校验：
│   │                # 食物/饮用/工具/护甲/发射器/染料/桶/模型 全数据字段）/
│   │                # Inventory（27 格确定性合并）/ Recipe/RecipeRegistry /
│   │                # durability（五类帧锁定磨损纯函数）/ armor（MC floor
│   │                # 减伤纯函数）/ furnace（per-block 熔炼状态 P40/P44）/
│   │                # campfire（per-block 4 槽烤制 + 浇灭/再点燃 P71-76）/
│   │                # crafting_grid（per-block 合成格 CRAFT-1）
│   ├── ui/          # 界面（纯逻辑无 GL 可单测）：font（ASCII + 32 CJK 字形
│   │                # 静态表 + 严格 UTF-8 解码 PRE-1）、debug_overlay（F3，
│   │                # 英文 = 开发者工具）、inventory_ui（面板布局 + 点击状态机
│   │                # + per-block 数据源 bind）、ui_scale（高分屏缩放）
│   ├── renderer/    # 渲染器（GL 封装）：Shader（location 缓存）/Texture/
│   │                # TextureAtlas/Mesh/Renderer（draw/drawSky/setCulling/
│   │                # setDepthMask）/ fog（PRE-2 距离雾纯函数）/ alpha_test
│   │                # （PRE-5 cutout 纹理普查纯函数）
│   └── world/       # 体素世界：coordinates（换算唯一入口）、Chunk（块+sky/
│                    # block 光 + fluid 双种类 + blockState per-instance byte
│                    # （P94/ENT-2）+ per-kind fluidCells（PIPE-3））、World
│                    # （journal 收口 PIPE-2 + 流体列保留 STRM-2 + 全部 get/set
│                    # 读接口）、chunk_streaming（**圆盘** dx²+dz²≤R_eff² 纯整数
│                    # 谓词 + 远平面帽 64，STRM-1）、chunk_mesher（三桶
│                    # opaque/cutout/blend + 全 Shape emit + 流体波纹 PRE-3 +
│                    # 按面烘焙光）、mesh_job（worker 快照 meshing PIPE-1）、
│                    # light_job（光照并行 job：流送/流体追赶各至多 2/帧，
│                    # 位级独立判据 99.10 + 流体光照异步 99.11）、lighting
│                    # （Sky/Block 0-15 传播：light_opacity 衰减 LGT-2 /
│                    # 源等级仿射撤退 + 月光地板 LGT-3 / 水 -3）、
│                    # fluid（FluidSim：反应 pass obsidian/stone + 逐列下落/
│                    # 水平扩散/守恒 FLUID-1 + per-cell 活动门控 99.8/PIPE-3）、
│                    # fire_spread（蔓延纯函数 P66）、growth（作物/树生长引擎
│                    # P48/GEN-2）、tree_shape、ore_vein（GEN-3 椭球脉）、
│                    # beacon（P83/91 基座等级纯函数）、beam_light（LGT-4 光束
│                    # 世界光采样纯函数）、cloud（PRE-4 云层几何/漂移纯函数）、
│                    # ambience（PRE-4 昼夜环境音判定/roll 纯函数）、noise
│                    # （splitmix64 value noise/fBm 无状态）、BiomeRegistry、
│                    # WorldgenParams/WorldGenerator（seed 可复现：地形→洞穴
│                    # （面条洞 GEN-1/4）→矿脉→湖→树→装饰，浮空树守卫 GEN-5、
│                    # 半区修复 GEN-6）、raycast（DDA 射线 + canPlaceAt/
│                    # canPlantAt/桶目标射线）
├── tools/           # 开发工具：generate_block_textures.py / generate_font.py
│                    # （ASCII + CJK 静态表）/ generate_cracks.py /
│                    # generate_item_icons.py / generate_sounds.py（53 WAV，
│                    # 固定种子确定性，重跑幂等）+ E2E 场景/探针脚本（.zsh/.py）
└── tests/           # GTest 单元测试（1710，-Werror 恒 0 警告）+ PNG 夹具
                     # （tests/data/，tests/generate_fixtures.py 再生）+
                     # 独立探针（cave_probe 等，真实资产 + 工厂函数）
```

## 核心架构

### 帧管线（每帧 1/60 帧步长，硬规则 8）

```
update（帧锁定段）:
  输入 → Player（Entity 物理/游泳/护甲/氧气/火）
  → Mob AI + 实体（子步物理/踏阶/流体/摆动相位/效果 tick）
  → 投射物 → 粒子
  → FluidSim::tick（反应/下落/扩散，per-cell 活动门控 O(活格)）
  → 流体光照追赶（列粒度待集，worker 每帧至多 2 列，99.11）
  → 生长（作物/树 roll）→ 火蔓延/寿命 → 信标 pass → 效果 pass
  → 合成/熔炼/烤制（per-block 状态，帧锁定子步）
  → 存档 journal 收口（World 单写窗口；F5/退出/自动三路，原子写）
render:
  相机 → 流送盘缘雾 + 天空（sky 程序）→ 世界三 pass
  （opaque → cutout【discard+深度写】→ blend【深度写关：水/玻璃/云/
  粒子/信标光束/掉落 model】）→ 实体（mob/掉落/箭）→ 手持 →
  UI（HUD/面板/F3）
worker 窗口（post→flush，硬规则 7）:
  mesh job（每帧 2 列，PIPE-1 快照）+ light job（流送/流体两类各至多 2，
  位级独立判据 lightRegionsIndependent）
```

### 确定性双轨（硬规则 8）

- **帧锁定（1/60 固定步长）**：世界/流体/光照/Mob/箭/粒子/云/环境音/
  合成/烤制/生长/摆动相位——无输入运行逐帧字节可复现（E2E 门 ×2 RAW）。
- **真实 dt（生存计时器）**：饥饿/进食/破坏进度/坠落计时/实体 age/火寿命/
  状态效果倒计时/自动存档计时——长墙钟跑触发 36s 饥饿 tick 类相位差，
  savenorm 按类剥离（PITS 244/286）。

### 存档（format 19，data/saves/<slot>.json）

seed + time + player{pos/yaw/pitch/health/absorption/hunger/saturation/
armor[4]/effects/oxygen} + inventory（27 槽，含 damage）+ tables[]（CRAFT-1
每工作台）+ furnaces[] + campfires[]（4 槽）+ fires[]（real-dt 剩余寿命）+
entities[]（item/mob 双 type）+ blocks[]（生成 diff）+ fluids[]（五元素，
format 9 起 water/lava 命名）+ states[]（ENT-2 per-instance byte；format 14
独立 stairs[] 段在 15 起值保真迁入 states，15+ 文件不含 stairs）。
严格双向交叉校验（段间引用点名拒绝）+ 全量历史 format 1..18 值保真迁移。
**增量 journal（PIPE-2）**：World 写收口记位置集，退出 O(会话编辑数) 按
journal chunk 重算 fresh diff（与全量 diffWorld 字节等价，`--save-full-diff`
进程内 oracle 守护）。**原子写**（.tmp + rename，SAVE-1 三路共用）。

### 流送与并行

- **圆盘流送（STRM-1）**：加载区 = 玩家列的 dx²+dz²≤R_eff²（纯整数谓词，
  R_eff = min(view_distance, 64) 远平面帽）；2 列/帧填充（99.8）；
  驱逐 = 最远优先（流体改动列保留 diff，STRM-2）。
- **mesh worker（PIPE-1）**：mesher 计算在 worker 快照 job，主线程只做
  列合并 + GL 上传。
- **光照并行（99.10/99.11）**：流送与流体追赶两类 light job 同窗各至多
  2 个，判据 = 写 reach ±1 列 + 读环 +1 列双向不相交（gap≥4），依赖对
  回退单 job 顺序 = 逐位行为；流体 tick 的 changed 格 → 列粒度待集异步
  追赶（排空 == propagateAll 幂等收敛）。

### 光照模型

- 双场 0-15：sky（列顶扫描 + 垂直 0/水平 1 BFS）+ block（emission 源 +
  全方向 1 BFS）；`light_opacity`（LGT-2）逐格进入成本（缺省 opaque 派生 =
  内容零变化；leaves 2 / glass 1 = MC 值）。
- **呈现层昼夜（LGT-3）**：天空光场对源等级 S = 7 + 8×dayFactor 仿射
  撤退 `max(stored − (15−S), 0)`——逐格精确（confluence 单测钉死）、零重
  传播；深夜露天月光地板 7/15（kNightSkyLevel）；敌对生成门
  spawn_max_light 7 配对。
- **per-draw 光覆盖（PRE-6/LGT-4）**：`u_useLightOverride` + sky/block
  三 uniform（basic.vert 顶点段门控），掉落物（itemDropLight）与信标光束
  （beamLightSample）经 CPU 采样世界光接入昼夜/萤石/雾管线；override draw
  后强制 bind+复位 0（UXFIX-1 泄漏纪律）。

### 流体（双种类 level 1..8）

反应 pass（水/熔岩 6-邻 → obsidian/stone，源水/流水分产物）+ 逐列下落
扫描（传送带语义）+ 水平扩散 L-1 + **守恒（FLUID-1）**（静态沉积稳压器
保留，落池抬升分支删除；ΔF 会计恒等式单测）+ 活动门控
（per-chunk per-kind 活格列表 + 活动 chunk 集，tick = O(活格)，PIPE-3
流体状态逐位不变 oracle 守护）+ 共享实体流体模型（Entity::
refreshFluidState/integrateFluid：浮漂 snap 游泳位/下沉/step-up 涉水修正，
FLUID-3；player/mob/item 三载体同一函数 = 行为对齐保证）。

### 实体与 Mob

- **Mob = 纯数据**（assets/data/mobs/*.json：几何 box 部件/纹理/AI 参数/
  生成条件/on_hit_effect/swings 摆腿段）；AI 状态机帧锁定
  （chase/wander/attack + 远程射击；夜间 + 光照门 spawn_max_light）。
- **摆动（ENT-1/1b）**：纯渲染层 per-part 动画（整数 swing 相位，帧边界
  位移判移动；constexpr 16.16 定点正弦表零运行期 libm；穿模谓词
  Sutherland-Hodgman 凸裁剪 ≤25% 体积单测守护）；存档零字段。
- **效果（SURV-4/5）**：状态效果双载体（玩家+Mob），全数据字段
  （tick/instant_hp/melee_penalty/reduction/speed/undead 免疫…）；
  箭尾部效果（skeleton wither / poison_bow）。

### 物品与生存

- **物品 = 纯数据**：food（restore_hunger/效果）/ drinkable
  （grants_effect(s)+instant_hp 药水 SURV-4）/ tool（break_speed/
  fast_against 分块倍率）/ armor（points 减伤 SURV-1）/ launcher
  （bow 族：shoot_speed/damage/infinite/ammo）/ 桶（collects/pours 对偶）/
  dye / model（CRAFT-2 盒模型）/ durability（MC 精确值 + 五类帧锁定磨损
  SURV-3）/ 护甲减伤（floor(amount×(100−4×pts))/100，溺水豁免 SURV-2）。
- **per-block 容器**：炉子/营火/工作台 = `std::map<BlockPos, State>` 位置
  序 + setBlock 收口创建/删除（破坏丢弃物品）+ 卸载保护 + format 段双向
  校验（P44/CRAFT-1 范式）。
- **护甲/吸收两分量**：health + absorption 池独立（池先扣，死亡只来自
  health）；池 = 效果总和纯函数，只在同步点重算（P72）。

### 渲染 pass 与着色器

- basic.vert/frag：三 pass 由 mesher 三桶驱动（opaque → cutout
  【`discard` alpha<0.5，深度写开，PRE-5】→ blend【深度写关】）；
  面表绕序回归守护；光照 = max(effSky, block)/15 × faceShade（顶 1.0/
  ±X 0.8/±Z 0.6/底 0.5）+ kAmbient 0.03 底；距离雾（PRE-2，gl_Position
  独立表达式树位级红线）；水面波纹（PRE-3，tiled 采样窗逐帧平移）；
  云（PRE-4，纯色平板 + 100s/格漂移 + 相机格缓存重建）；天空程序
  （日月星，不雾化 = 天空即雾色）。
- 音频：立体声 SoundBank（44100），世界事件空间化（16 块线性衰减 +
  恒定增益声像，CRAFT-3），环境音 3 上下文帧锁定 roll（PRE-4）；音频不入
  帧状态（截图/存档零影响）。

### 验证基础设施（详见 PROTOCOL.md）

- 单测 1710（GTest，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告）：
  纯函数优先（几何/物理/光照/流体/存档/内容钉死），GL 序列 = E2E 兜底。
- **无输入基线门**（base18.png 字节一致）：新系统须论证"无输入零活动"
  （结构性免疫）或重锚；过门次数与锚历史见 AGENTS.md 现状节。
- **E2E 场景门**：×2 RAW 字节一致（无输入）/ 存档 savenorm EXACT
  （状态级权威）+ 像素隔离带断言 + A/B 判别（old/new 互斥终态）。
- **性能 A/B（每版发布前必做）**：fall 900f 三窗指标同档 +
  fluidCells 逐位一致（位级 oracle 场景纪律：fire 出档 lifetime 9999s
  或 spread_chance 0）。
