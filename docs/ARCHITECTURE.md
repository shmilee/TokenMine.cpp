# 架构

> v1.x 整理（LGT-4 时点），11-20 刷新到 v3.51.0 状态（目录结构 = 3.x
> 时点：下界 / 末地维度、多人 net、主菜单、教学 demo 族 + scene 资产、
> 全景资产；game.cpp 拆分文件组 + net/ 入树）。本文
> = 目录结构 + 核心架构速览；逐阶段细节在
> `docs/archive/v1x/phase-*.md`（181 份）/ `docs/archive/v2x/`（20 份）/
> `docs/archive/phase-3.x.0.md`（51 份），协议与硬规则在
> `docs/PROTOCOL.md`，玩法机制在 `docs/MECHANICS.md`，加内容在
> `docs/CONTENT_GUIDE.md`。

## 目录结构

```
tokenmine/
├── assets/          # 游戏资源（textures/ models/ shaders/ sounds/ data/）
│                    # data/ = blocks/ items/ recipes/ biomes/ biomes_nether/
│                    # biomes_end/ mobs/ effects/ enchantments/ tutorials/ +
│                    # worldgen.json（V3-5 起含 sediment 节）+ mob_spawning.
│                    # json + mob_spawning_nether.json（V3-3）+
│                    # mob_spawning_end.json（V3-10c）+ hotbar.json（内容
│                    # 全数据驱动：加方块/物品/配方/Mob/附魔/群系 = JSON）；
│                    # textures/panorama.png = 主菜单全景背景 committed
│                    # 资产（4800×360 闭环环带，--panorama 确定性再生）；
│                    # demos/ = shipped .dmo（4 个教程/示例 + 10 个教学
│                    # guide_* V3-16）+ index.json（四轴 schema {category,
│                    # seq,mode,tags,file,length_frames} + title 可选；5 章
│                    # 封闭词表 day1/build/dimension/survival/coop，init
│                    # 每次 boot 全量交叉检查）+ scenes/（committed 确定性
│                    # 场景存档 .json.gz V3-16a，含 panorama.json.gz）
├── config/          # 用户配置（config.json，已提交文件：view_distance 80 /
│                    # day_length 600 用户调值，不还原；内容改动仅用户要求时做）
├── data/            # 存档目录（data/saves/<slot>.json.gz，format 33 =
│                    # players[] per-player V3-12c2；gzip 容器 V3-7；
│                    # data/demos/ 录制 demo V3-9，gitignore）
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
│   │                # window / input（SDL 事件唯一消费者→GameKey；**源位掩码
│   │                # 多输入 V3-8** + **setEventTap 录制钩子 V3-9**）/ camera /
│   │                # png（自研）/ math（Vec2/3/4/Mat4 header-only）/
│   │                # save（SaveGame format 33【players[] per-player
│   │                # V3-12c2；下界段 format 30 / 末地段 format 32 /
│   │                # end_xp_entities format 33】+ diffWorld/
│   │                # applyWorldEdits + 严格双向交叉校验 + **确定性 gzip 容器
│   │                # V3-7** + 原子写 writeAtomicFile + 增量 journal
│   │                # 支持，无 GL 依赖可单测）/ save_slot（SAVE-3 管理旗）/
│   │                # sound（SoundBank 立体声 75 件 + 16 块距离衰减/线性声像
│   │                # 空间化 CRAFT-3 + 环境音钩子 5 上下文）/
│   │                # status_effect（状态效果双载体：玩家+Mob，纯数据驱动）/
│   │                # food / bow / difficulty / xp_curve（V2-11 全整数曲线）/
│   │                # bed（V2-13 睡眠三门 + 验床纯函数）/ fly（V2-19 双击窗）/
│   │                # tutorial（V2-3 系列状态机）/ weather（V2-4 纯状态机）/
│   │                # enchanting（V2-12 确定性 offer 全整式）/ portal
│   │                #（V3-1 门框纯函数族）/ end_portal（V3-10a 嵌眼
│   │                # 5×5 环纯函数族）/ demo（V3-9 注入内核/录制器/
│   │                # 文件 schema + V3-16a scene 世界 spec/panel_click
│   │                # 事件/index.json 交叉检查）（纯函数逻辑层）/
│   │                # thread_pool（固定 4 worker，post→flush 帧同步窗口）/
│   │                # game（主循环+场景+全部 per-block 状态宿主：炉子/营火/
│   │                # 工作台 map + 信标 + 作物 + 粒子 + 音频事件 + **NetherDim
│   │                # /EndDim 聚合 = 下界/末地维度宿主（V3-1 Phase B / V3-10a）**）/ main；game.cpp
│   │                # 拆分文件组（game_init/frame/render/ui/streaming/
│   │                # entities/interaction/save/menu/net + game_local.hpp；
│   │                # menu_layout.hpp + ui_layout = 菜单/面板纯布局 test 孪生）
│   ├── net/         # 多人（V3-12 主机权威 lockstep）：POSIX TCP 无 net
│   │                # 线程（主循环内 poll/wait 同步）；net_proto（HELLO
│   │                # 四指纹握手 / WELCOME【REJOIN 快照形 + hostName/
│   │                # MOTD V3-17d】/ FrameState 40B【mouse raw f32 零量化】/
│   │                # CHAT / CTRL【join/renumber】/ 流重组 R16；
│   │                # kNetMaxPlayers 8）/ net_session（lockstep 屏障
│   │                # D=2 无预测 / 30 帧状态哈希双向交换 / 心跳 min
│   │                # 水印点名移除 / 断线移除）
│   ├── player/      # 玩家：第一人称（继承 Entity 统一物理）+ drown（氧气窒息
│   │                # 纯函数）+ 护甲减伤（item/armor 纯函数）
│   ├── entity/      # 实体系统：Entity 基类（底面中心/子步化轴分离碰撞/滑动/
│   │                # stepUp 踏阶/共享流体模型 refreshFluidState+integrateFluid
│   │                # / **setWorld 维度换绑 V3-1**）/
│   │                # MobEntity（数据驱动 AI 状态机 chase/wander/attack/远程
│   │                # + **neutral 激怒 V3-3** + **walks_on_lava 熔岩面行走
│   │                # V3-3**）/
│   │                # mob_registry（mobs/*.json 严格解析，16 种）/ mob_spawning
│   │                # （夜间 + 光照门 spawn_max_light）/ nether_spawn_pool
│   │                #（V3-3 下界池独立文件 fail-fast）/ mob_mesh /
│   │                # mob_swing（ENT-1 四肢摆动：16.16 定点正弦表+穿模谓词
│   │                # mob_swing_clip）/ pathfind（ENT-6 BFS）/ ItemEntity
│   │                #（掉落物：摩擦停驻/同类合并/拾取延迟/120s 消亡/流体浮
│   │                # 沉/半浸没终态）/ exp_orb（V2-11 经验球实体）/
│   │                # pickup_magnet（V2-6 磁力纯函数）/ blob_shadow（V2-6
│   │                # 扁阴影纯几何，v3.12.0 = 1.0× 身体宽）/
│   │                # wool_drops（V2-15 彩毛死亡钩子纯函数）/
│   │                # item_mesh（块立方掉落）/ item_model_mesh（CRAFT-2 盒模型
│   │                # 纯几何）/ item_light（PRE-6 掉落世界光采样）/
│   │                # Projectile（箭/ghast 火球 V3-14b/末影珍珠 V3-10b：重力
│   │                # 抛物线/钉住/尾部效果 SURV-5；珍珠 stuck → 瞬移）/
│   │                # arrow_mesh / particle（确定性爆发/逐粒重力/流体交互）
│   ├── item/        # 物品系统：Item/ItemStack（+damage float SURV-3）+
│   │                # 破坏耗时公式（effectiveBreakSpeed fast_against 分块倍率）/
│   │                # ItemRegistry（块隐式同名 item + 显式 JSON 严格校验：
│   │                # 食物/饮用/工具/护甲/发射器/染料/桶/模型 全数据字段）/
│   │                # Inventory（27 格确定性合并）/ Recipe/RecipeRegistry /
│   │                # durability（五类帧锁定磨损纯函数）/ armor（MC floor
│   │                # 减伤纯函数）/ furnace（per-block 熔炼状态 P40/P44）/
│   │                # campfire（per-block 4 槽烤制 + 浇灭/再点燃 P71-76）/
│   │                # crafting_grid（per-block 合成格 CRAFT-1）/ repair
│   │                #（UX-5 工作台材料修复纯函数）
│   ├── ui/          # 界面（纯逻辑无 GL 可单测）：font（ASCII + 469 CJK 字形
│   │                # 静态表 + 严格 UTF-8 解码 PRE-1）、debug_overlay（F3，
│   │                # 英文 = 开发者工具）、inventory_ui（面板布局 + 点击状态机
│   │                # + per-block 数据源 bind）、ui_scale（高分屏缩放）
│   ├── renderer/    # 渲染器（GL 封装）：Shader（location 缓存）/Texture/
│   │                # TextureAtlas（**16×16 V3-2 幂二增长**）/Mesh/Renderer
│   │                # （draw/drawSky/setCulling/setDepthMask）/ fog（PRE-2
│   │                # 距离雾 + **V3-6 填充感知 fogFilledRange** 纯函数）/
│   │                # alpha_test（PRE-5 cutout 纹理普查纯函数）
│   └── world/       # 体素世界：coordinates（换算唯一入口）、Chunk（块+sky/
│                    # block 光 + fluid 三种类（honey FLUID-6）+ blockState
│                    # per-instance byte（P94/ENT-2）+ per-kind fluidCells
│                    # （PIPE-3））、World
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
│                    # （splitmix64 value noise/fBm 无状态）、BiomeRegistry
│                    #（主世界 + **下界独立池 V3-2** + **末地池 V3-10b**）、
│                    # WorldgenParams/WorldGenerator（seed 可复现：地形→洞穴
│                    # （面条洞 GEN-1/4）→矿脉→湖→树→装饰→**水底 sediment
│                    # pass V3-5**，浮空树守卫 GEN-5、半区修复 GEN-6）、
│                    # **nether_gen（V3-1B 下界专属 worldgen 纯函数：坐标纯
│                    # 函数零噪声 + D9 带 + 5 群系 + 3 矿脉 + 装饰）**、
│                    # **end_gen（V3-10a 末地专属 worldgen 纯函数：台地 + 出口
│                    # 门 + 基岩洋 + 岛群 pass）**、
│                    # ao（V2-18 顶点 AO 纯几何）、day_cycle、raycast（DDA 射
│                    # 线 + canPlaceAt/canPlantAt/桶目标射线 + **slab partial-
│                    # aware V2-17**）
├── tools/           # 开发工具：generate_block_textures.py /
│                    # generate_mob_textures.py / generate_player_skin.py /
│                    # generate_font.py（ASCII + CJK 静态表）/
│                    # generate_cracks.py / generate_item_icons.py /
│                    # generate_effect_icons.py + health/hunger/oxygen 图标 /
│                    # generate_sounds.py（75 WAV，固定种子确定性，重跑幂等）
│                    # / make_example_demo.py（V3-9 示例 demo 确定性生成）/
│                    # 场景工具 mk_scene.py（场景手术幂等）+
│                    # demo_author.py（教学 demo Guide DSL）+
│                    # demo_fly_patch.py（dmo 飞行后贴，字节 parity）+
│                    # tools/scenes/（beat 脚本）/ 门协议工具
│                    # （gate_compare.py / perf_gate_config.json /
│                    # g139_instrument.py / xenv.sh）+ SVG 工具组 + E2E
│                    # 场景/探针脚本（.zsh/.py）
└── tests/           # GTest 单元测试（3028，-Werror 恒 0 警告）+ PNG 夹具
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
  相机 → 流送盘缘雾（V3-6 填充感知 end）+ 天空（sky 程序；
  下界/末地维度门控：暗红/暗紫黑 clear、日月星云雨不画、雾 96/8..48）→ 世界三 pass
  （opaque → cutout【discard+深度写】→ blend【深度写关：水/玻璃/云/
  粒子/信标光束/掉落 model】）→ 实体（mob/掉落/箭/XP 球）→ 手持 →
  UI（HUD/面板/F3/demo 角标+面板【V3-9 独立批】）
worker 窗口（post→flush，硬规则 7）:
  mesh job（每帧 2 列，PIPE-1 快照）+ light job（流送/流体两类各至多 2，
  位级独立判据 lightRegionsIndependent）
```

**维度分派（V3-1 Phase B）**：Player 是唯一可跨维实体（`Entity::setWorld`
单咽喉点换绑）；每维各持 World + 流送 + MobSystem + per-block 状态集 +
journal（NetherDim/EndDim 聚合，惰性构造 = 首传前零 chunk/零内存）。每帧只 tick
**活跃维**（非活跃维 = 时间胶囊冻结；DayCycle/Weather 全局恒推进 = MC 时
间连续）。**demo 回放循环（V3-9）**：`runDemoReplay` 替换 gameplay 循环——
demo dts 唯一时间源 + 事件同序注入 `Input.handle*`（与 SDL pump 同款收
口）+ 观众 Input 单消费者（orbit/倍率/暂停 = render-only 白名单）+ 0.5×
render-only 保持。
**主菜单循环（V3-17）**：`runMenuLoop` = 第 4 循环（window + renderer +
buildUiAssets 后、buildScene 前；menuEnabled 五形绕开 = --demo/--record/
--demo-author/--listen/--connect/--script/--new/管理旗 = 结构免疫根）——
renderMenu = clear + UiBatch only（世界未建，禁 render/renderUi；原语 =
renderUi 拷贝，游戏面位级不变）；六面 = 标题/登陆/设置/教学/多人 + Join
子页（键盘统一 m_menuSel + 文本输入生命周期 m_textInputOn 页入开/页出关）；
单人 = fresh world-start（--new 位级孪生）；教学两级 = index.json 5 章
（菜单驱动 demo 走 applyDemoArgs 共享单源 = CLI --demo 位级孪生）；多人 =
net 主机/客机 init 块原文提取共用（WELCOME + hostName/MOTD）；背景 =
committed 全景 png cover-fit UV 帧锁定慢速平移（V3-20~23）。
**全景生成器（V3-20，进程一次性）**：`--panorama <out>`（+ `--panorama-
scene <name>` 场景环）= config 种子 fresh 世界 + 40f fixed-dt 预热（地形
网格在流送集成）→ 8 向 48° overlap 捕获 640×360 取中央 600 px →
strip 4800×360 闭环环带（45° tile 精确 + 24 列边界混合带
kPanoramaBlendHalf = 周期性结构解，零调参）→ 帧 0 程序布景（Steve = 第
二玩家 + 动物群 4 只 + 末影龙无回调静默飞行；--panorama-scene 同步列
烘焙 + ring 面强制生成）→ committed assets/textures/panorama.png（×2
字节恒等 = 确定性生成器）。

### 确定性双轨（硬规则 8）

- **帧锁定（1/60 固定步长）**：世界/流体/光照/Mob/箭/粒子/云/环境音/
  合成/烤制/生长/摆动相位——无输入运行逐帧字节可复现（E2E 门 ×2 RAW）。
- **真实 dt（生存计时器）**：饥饿/进食/破坏进度/坠落计时/实体 age/火寿命/
  状态效果倒计时/自动存档计时——长墙钟跑触发 36s 饥饿 tick 类相位差，
  savenorm 按类剥离（PITS 244/286）。

### 存档（format 33，data/saves/<slot>.json.gz【V3-18 起；legacy .json 同名探测零迁移】）

seed + time + **players[] per-player 数组（format 33+ 恒写，≥1 条；
entry 0 = primary 镜像，entries 1.. = 远程玩家；entry = format-32 形
player 对象 + inventory/inv_grid 嵌套迁入 V3-12c2，kSavePlayerCap 8）**
{pos/velocity/yaw/pitch/health/absorption/hunger/saturation/armor[4]/
effects/oxygen/xp/xp_seed/respawn【精确 float V3-12c2】/stats/tutorial/
creative【format 31+ strict bool V3-11a】/**dimension（format 30+ 严格
词表，32+ 起三词 overworld|nether|end——V3-1/V3-10a）*/
inventory【36 槽，含 damage/enchants】+ inv_grid} +
**nether{blocks,fluids,states}（format 30+ 恒写空合法）+ nether_entities[]
（30+）** + **end{blocks,fluids,states} + end_entities[]（format 32+
V3-10a）+ end_xp_entities[]（format 33+ 末地 XP orb 持久化 V3-12c2）** +
tables[]（CRAFT-1 每工作台）+
furnaces[] + campfires[]（4 槽）+ fires[]（real-dt 剩余寿命）+ entities[]
（item/xp 双 type）+ blocks[]（生成 diff）+ fluids[]（五元素，format 9 起
water/lava/honey 命名）+ states[]（ENT-2 per-instance byte；format 14 独立
stairs[] 段在 15 起值保真迁入 states，15+ 文件不含 stairs）。
严格双向交叉校验（段间引用点名拒绝）+ 全量历史 format 1..32 值保真加载。
**下界/末地派生 seed 不落盘**（worldSeed ^ kSaltNether / kSaltEnd parse
重建 = 单源真值）+ 两维 portal 载入 rescan（孤立 → air，双向交叉检）。
**format 32（V3-10a，v3.23.0）**：dimension 词表按 format 分词（30/31
携 "end" = version lie 点名）+ end/end_entities 段恒写 + 末地内容 = 世界
生成演进零 format。**format 33（V3-12c2，v3.31.0）**：players[] per-
player 数组 + end_xp_entities 恒写 + respawn 精确 float；REJOIN 快照 =
save 形 WELCOME（磁盘档 + 会话计数族 + 活粒子 + per-player 会话计时器，
全 presence-gated on-disk 不变）；net 快照段 {particles/weather/
spawn_counters/particle_break_counter/net_players} 仅 WELCOME 文档携、
on-disk 省略。
**V3-7 gzip 编码（format 29 时代）**：on-disk = 确定性 gzip 容器（固定
10B 头 MTIME=0/FLG=0 + raw deflate level 6 + CRC32/ISIZE 尾，同世界逐字节
可复现；`save.compression: "gzip"|"none"` 严格读默认 gzip）；加载按魔数
`1f 8b` 自动探测——旧明文档零迁移直读；解码侧头 10B 逐字节全钉（截断/
半体/CRC·ISIZE 不符/外源 gzip 全点名）。JSON 内容与 format 28 schema
完全一致（编码与 format 号正交）。
**增量 journal（PIPE-2）**：World 写收口记位置集，退出 O(会话编辑数) 按
journal chunk 重算 fresh diff（与全量 diffWorld 字节等价，`--save-full-diff`
进程内 oracle 守护）。**原子写**（.tmp + rename，SAVE-1 三路共用——gzip
字节同样 tmp+rename 崩溃安全）。

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

### 流体（三种类 level 1..8，honey FLUID-6）

反应 pass（水/熔岩 6-邻 → obsidian/stone，源水/流水分产物）+ 逐列下落
扫描（传送带语义）+ 水平扩散 L-1 + **守恒（FLUID-1）**（静态沉积稳压器
保留，落池抬升分支删除；ΔF 会计恒等式单测）+ 活动门控
（per-chunk per-kind 活格列表 + 活动 chunk 集，tick = O(活格)，PIPE-3
流体状态逐位不变 oracle 守护）+ 共享实体流体模型（Entity::
refreshFluidState/integrateFluid：浮漂 snap 游泳位/下沉/step-up 涉水修正，
FLUID-3；player/mob/item 三载体同一函数 = 行为对齐保证）。

### 实体与 Mob

- **Mob = 纯数据**（assets/data/mobs/*.json：几何 box 部件/纹理/AI 参数/
  生成条件/on_hit_effect/swings 摆腿段/**neutral·walks_on_lava 旗
  V3-3**/**rideable 骑乘旗 V3-14d**/携格·投掷·爆炸·交易数据旗
  V3-10b~d/V3-14b~c）；16 种（主世界 8 + 下界 6 + 末地 2）；AI 状态机帧
  锁定（chase/wander/attack + 远程射击 + neutral 激怒脱激 + flying：bat
  巡航/ghast 悬停射击 V3-14b/末影龙三态 V3-10d + strider riderSteer
  V3-14d + 末影人注视激怒/瞬移/携格 V3-10c + piglin 交易 V3-14c；夜间 +
  光照门 spawn_max_light）；**下界/末地生成池独立文件**
  （mob_spawning_nether.json / mob_spawning_end.json，
  loadNetherSpawnPool / loadEndSpawnPool fail-fast；池隔离 = 维度分派既
  有，零新基建）。
- **摆动（ENT-1/1b）**：纯渲染层 per-part 动画（整数 swing 相位，帧边界
  位移判移动；constexpr 16.16 定点正弦表零运行期 libm；穿模谓词
  Sutherland-Hodgman 凸裁剪 ≤25% 体积单测守护）；存档零字段。
- **效果（SURV-4/5）**：状态效果双载体（玩家+Mob），全数据字段
  （tick/instant_hp/melee_penalty/reduction/speed/undead 免疫…）；
  箭尾部效果（skeleton wither / poison_bow）+ dragon_breath 玩家-only
  吐息 V3-10d。

### 物品与生存

- **物品 = 纯数据**：food（restore_hunger/效果）/ drinkable
  （grants_effect(s)+instant_hp 药水 SURV-4）/ tool（break_speed/
  fast_against 分块倍率）/ armor（points 减伤 SURV-1，16 件含
  netherite V3-4）/ launcher
  （bow 族：shoot_speed/damage/infinite/ammo）/ 桶（collects/pours 对偶）/
  dye / model（CRAFT-2 盒模型，101 件全覆盖）/ durability（MC 精确值 36
  件 + 五类帧锁定磨损 SURV-3）/ **shears 剪毛旗 V2-15** /
  **enchant_class/enchantability V2-12（29 件）** /
  护甲减伤（floor(amount×(100−4×pts))/100，溺水豁免 SURV-2）/
  **repair 材料修复 UX-5**。
- **per-block 容器**：炉子/营火/工作台 = `std::map<BlockPos, State>` 位置
  序 + setBlock 收口创建/删除（破坏丢弃物品）+ 卸载保护 + format 段双向
  校验（P44/CRAFT-1 范式）。
- **护甲/吸收两分量**：health + absorption 池独立（池先扣，死亡只来自
  health）；池 = 效果总和纯函数，只在同步点重算（P72）。

### 多人（V3-12，主机权威 lockstep）

- **net/ = POSIX TCP，无 net 线程**：主机 poll / 客户端 waitPacket 同步于
  主循环（流重组 R16：分裂/粘包）；HELLO 四指纹握手（TM_VERSION/sim/
  assetFNV/simConfigHash——seed 豁免 WELCOME 权威 + render.* 豁免 dt 广播
  归一；混版先拒 = schema 增长兼容性免费）；WELCOME fresh = 仅 seed 零
  字节世界传输（纯函数 worldgen 同 seed 位级）/ REJOIN = save 形快照
  （磁盘档 + 会话计数族 + 活粒子 + per-player 会话计时器）+ hostName/
  MOTD（V3-17d）；kNetMaxPlayers 8，kNetDefaultPort 5555（菜单主机默
  认，CLI --listen 显式面不变）。
- **lockstep = 每机模拟每玩家**：主机权威（输入中继 + 世界权威）；
  FrameState 40B（keys u64 / mouse raw f32 零量化 / Ax·Ay 面板落格 /
  locked / wheel）D=2 无预测（38–42 ms）；屏障 = 主机等客户端 INPUT
  水印（门内捕获 = 无头主机快跑吃冷输入的 desync 根）；每 30 帧状态哈希
  字段清单双向交换 + 不符点名退出；心跳 = 屏障 stall 10 s **min 水印**点名
  移除最滞后（solo 续跑 / 客户端见主机 EOF 全退 D2）；断线 = 库存 100% 掉落 +
  落乘 + 效果清（R19）。
- **player 多人化 = 双别名最小方案**：m_players + m_localSlot +
  m_player = primaryPlayer() raw pointer 别名（354 处零改动）+
  PlayerSession 吸收成员族 + pass 归属二分（primary 锚不动 / ×N 循环化
  全 sim 覆盖：物理/生存/效果/骑乘/珍珠/交互/拾取磁吸/眼接触/箭扫
  掠/面板）+ per-player aim（solo 相机路径位级）；持久面 = format 33
  players[]（见存档节）；聊天/名牌/打字抑制 = 表现层不入哈希（D21）。

### 渲染 pass 与着色器

- basic.vert/frag：三 pass 由 mesher 三桶驱动（opaque → cutout
  【`discard` alpha<0.5，深度写开，PRE-5】→ blend【深度写关】）；
  面表绕序回归守护；光照 = max(effSky, block)/15 × faceShade（顶 1.0/
  ±X 0.8/±Z 0.6/底 0.5）+ kAmbient 0.03 底；距离雾（PRE-2，gl_Position
  独立表达式树位级红线）；水面波纹（PRE-3，tiled 采样窗逐帧平移）；
  云（**UX-9a 独立 cloud program 逐像素**：两级颗粒 37.5% 覆盖 + 4 向
  MC 侧壁厚度 + 100 s/格漂移 + 16384² quad 单次上传 + 延伸地平线）；天空
  程序（日月星 + 8 月相 V2-6，不雾化 = 天空即雾色）。
- 音频：立体声 SoundBank（44100，75 件），世界事件空间化（16 块线性衰减
  + 恒定增益声像，CRAFT-3），环境音 **5 上下文**（end V3-10a >
  nether V3-2 Part 2 > cave/day/night）帧锁定 roll（PRE-4）；音频不入帧
  状态（截图/存档零影响）。

### 验证基础设施（详见 PROTOCOL.md）

- 单测 3028（GTest，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告）：
  纯函数优先（几何/物理/光照/流体/存档/内容/菜单布局/网络 codec 钉死），
  GL 序列 = E2E 兜底。
- **无输入基线门**（base19.png 字节一致 = 游戏面锚；base20 = 主菜
  单标题锚【菜单期逐版重锚 PITS 426②】；锚谱系 base18 →
  base19【V3-6 填充感知雾重锚】）：新系统须论证"无输入零活动"（结构性
  免疫）或重锚；过门次数与锚历史见 AGENTS.md 现状节。
- **E2E 场景门**：×2 RAW 字节一致（无输入）/ 存档 savenorm EXACT
  （状态级权威）+ 像素隔离带断言 + A/B 判别（old/new 互斥终态）。
- **性能 A/B（每版发布前必做）**：fall 900f 三窗指标同档 +
  fluidCells 逐位一致（位级 oracle 场景纪律：fire 出档 lifetime 9999s
  或 spread_chance 0）。
