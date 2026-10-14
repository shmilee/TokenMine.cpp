# 架构

> Phase 30 文档整理：合并 `README.md`「目录结构」与 `SESSION_CONTEXT.md`「核心架构」
> （均未改写）。

## 目录结构

```
tokenmine/
├── assets/          # 游戏资源（textures/ models/ shaders/ sounds/ data/）
├── config/          # 用户配置（config.json，不存在时自动生成默认值）
├── data/            # 存档目录（data/saves/<slot>.json，Phase 13）
├── logs/            # 运行日志（logs/tokenmine.log）
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
│                    # fluid（FluidSim：水/熔岩双种类 level 1..8 模拟——反应 pass + 下落/扩散/吸收/补满）、
│                    # noise（确定性
│                    # value noise/fBm）、BiomeRegistry、WorldgenParams（worldgen.json）、
│                    # WorldGenerator（Seed 可复现生成）
├── tools/           # 开发工具：generate_block_textures.py（16x16 占位纹理）、
│                    # generate_font.py（16x16 字形 atlas 256×512）、generate_cracks.py（5 级裂纹条）、
│                    # generate_item_icons.py（32x32 物品图标）、generate_sounds.py（13 个 WAV）
└── tests/           # 单元测试（GTest）+ PNG 测试夹具（tests/data/，可用 tests/generate_fixtures.py 重新生成）
```
## 核心架构

```
src/core/    logger / error(Result) / json / config(点号键) / asset_manager(相对路径+缓存)
             window / input(SDL 事件唯一消费者→GameKey) / camera / png / math(Vec3/Mat4)
             save(P13)：SaveGame 数据模型 + diffWorld/applyWorldEdits（World+生成器回调+BlockRegistry，
             无 GL 依赖）+ serializeSave/parseSave（严格 JSON 层）——存档=seed+diff+状态，文件
             data/saves/<slot>.json（format 9；fluids 编辑 =
             [x,y,z,level,"water"|"lava"] 五元素，format<9 的 4 元素全迁 water）
             thread_pool(P14)：固定 N worker（游戏里 4）、post(job)/flush()（帧同步点：等所有
             已 post 的 job 完成）、job 异常首个存 exception_ptr、在 flush 重抛（池不坏）、
             析构 stopped+先跑完 pending job 再 join；worker 只在 post→flush 窗口运行（帧管线
             保证此窗口内主线程只读 World）
             game(主循环+场景) / main
src/block/   Block 纯数据(properties/model) + BlockRegistry
             （JSON 数据驱动、严格校验、air 哨兵 BlockID{0}、数据块 id 从 1）
             + BlockIDHash（P10）+ cube_face_table.hpp（P10）：kCubeFaces[6]/cubeFaceTexture，
             chunk mesher 与掉落物 item mesh 共享（绕序有回归测试）
src/world/   coordinates.hpp：kChunkSize=16, kWorldHeight=128, BlockPos/ChunkPos,
             floorDiv(负坐标正确), worldToChunk/chunkOrigin/chunkLocalToWorld/worldToChunkLocal
             Chunk：16^3 BlockID 数组 + skyLight/blockLight 两个 16^3 字节数组(0-15)
             + fluid 16^3 字节数组(0=干 1-8=水 level 9-16=熔岩 level【P51 双种类，
               kMaxFluidLevel=8 为界；一格至多一种】；8=满/源 1-7 流动；cell 里流体与方块互斥)
             + dirty（新 chunk 即 dirty）；raw 访问 data()/skyLightData()/blockLightData()/
             fluidData()；skyLightAt/blockLightAt/fluidAt 读接口
             World：chunk map；getBlock 越界/缺失→air；getSkyLight 缺失/世界上方→15、
             y<0→0；getBlockLight 缺失→0；setBlock 返回 bool、边界(local 0/15)标记邻居 dirty
             （并排掉所在格的水）；setFluid（只写已存在 chunk 的 air 格，越界/非 air/不变
             →false，标 self+面邻居 dirty）/getFluid（缺失→0）；getOrCreateChunk 跑
             ChunkGenerator；findChunk const / findChunkMut 非const 不创建；
             generator 不得回调 World
             + insertChunk(P14)：预建 chunk 插入（流式集成用）；已存在→false 不改动；新 chunk
             （构造即 dirty）+ 6 个面邻居（存在者）标脏——边界剔除决策变了，不标会 Z-fighting
             chunk_streaming(P14)：ColumnPos{x,z}（字典序 operator<，供 set/map）+
             columnDistance（Chebyshev）+ missingColumns(player,loaded,radius)（环内缺失列，
             (dist,x,z) 排序确定性最近优先）+ nearestMissingDistance（环内最近缺失列距离，
             全载返回 radius+1 哨兵）+ streamingActive（= d≤radius 且 d≤trigger，全载永不激活）
             纯函数无 World/线程，Game 把结果接到线程池与 World
             ChunkMesher：face culling v1（邻居非 opaque 才出面）；opaque+transparent 双子网格；
             每面烘焙"面后方一格"的 sky/block 光（/15 归一 float）；
             emitFluid（P9）：level/8 缩放立方体（满格=整块）、水-水剔面（邻居同级/更高
             不出面）、流体面烘焙**自身格**的光；
             顶点为 chunk-local(0..16)，绘制 model=translate(chunkOrigin)
             lighting：kMaxLight=15、kLightUpdateRadius=16（world/lighting.{hpp,cpp}）；
             propagateAll（全量：清 block 光→列顶扫 sky→sky BFS→emission 源→block BFS→全标
             dirty）；updateLocal（委托 updateRegion(min,max)）；两阶段都含 shell 种子
             （区域外紧邻的旧光值作只读边界源）；BFS 规则：opaque 不可入、sky 垂直 0/
             水平 1、block 全方向 1、严格增才写+重入队；**水衰减（P9）**：列顶扫描遇水
             每格 -3（enterCost），sky BFS 入水格额外 -3——深水湖底随深度变暗
             fluid：FluidSim::tick（world/fluid.{hpp,cpp}，P9，P51 双种类）——
             tick = ①反应 pass（快照扫熔岩格：6-邻有水 → setBlock 为该块
             water_reaction 产物【源水→source/流动水→flowing】；水不消耗）→
             ②水模拟 → ③熔岩模拟；每种类的快照式确定性模拟：
             逐列下落扫描（下方空/下方流动→falling；falling=传送带：下方吃本级、本级吃上方）；
             静止在方块上：水平扩散 L-1 到 4 邻居（L=1 终端；邻居是方块/同级/更高则挡）；
             静止在流体上：满格稳定；**坐在满水上→补满到 8**（source 托底的柱是满的，
             破坏湖底后整列补满、湖面恢复；缺这条规则 7↔吸收 永久振荡——P9 E2E 探针发现）；
             坐在流动池上：L<L2 吸收（池保级）/ L≥L2 保持并抬升下方到 L；
             冲突 max-merge（propose 同格取 max）、按格排序应用、返回排序净变化格（空=平衡）；
             registry-free（只读 block/fluid 数组）
             day_cycle（world/day_cycle.{hpp,cpp}）：DayCycleParams 在**命名空间作用域**
             （DayCycle::Params 是 using 别名）；timeOfDay∈[0,1)（0=黎明前, 0.25=正午,
             0.5=日落, 0.75=午夜）；dayFactor：夜 0/昼 1，[0.05,0.15)/[0.45,0.55) smoothstep
             渐变（kDawn*/kDusk* 公开常量）；starAlpha=1-dayFactor；skyColor 关键帧插值
             （夜(0.02,0.03,0.08)→黎明橙(0.85,0.45,0.25)@0.10→昼(0.47,0.71,0.95)@0.15-0.45
             →黄昏橙(0.85,0.40,0.20)@0.50→夜@0.60-1.0；**常值段直接返回关键帧色**——
             lerp 同端点有 1ulp 漂移，会破坏逐字节截图）；sunDirection：azimuth=2πt
             （东+X→南-Z→西-X）、elevation=sin(2πt)·90°（t=0.25 天顶）；moon=-sun；
             makeStarPositions(seed,count,radius)：splitmix64 上半球（y≥0.05）确定性星位
             noise：splitmix64 哈希 value noise 2D/3D + fBm，纯 (坐标, seed)，无状态线程安全
             BiomeRegistry：assets/data/biomes/*.json（权重+低频噪声 0.02 选择）
             WorldgenParams：assets/data/worldgen.json（缺省=默认，类型错误=报错）
             WorldGenerator：无状态 generateChunk=纯(seed,pos)；顺序 地形→洞穴→矿脉→树；
             树/矿脉=世界空间格网+边际（跨 chunk 无缝）；surfaceHeight(x,z) 供出生点
             raycast：DDA 体素射线（Amanatides & Woo），起点 cell 不测试；
             BlockHit{block,normal,distance}；canPlaceAt（Y 范围+非 solid+玩家
             AABB 不重叠，严格不等式与碰撞同约定）
src/renderer/ Shader/Texture/Mesh/Renderer/TextureAtlas（16x16 瓦片，文件名排序入图集）
              ShaderProgram：setUniform1i/1f/2f/3f/4f/Mat4（location 缓存，缺失只告警一次）
              Renderer::draw（索引+u_model）/ drawSky（索引+无 u_model，天空 billboard）
src/player/  Player：第一人称状态（yaw/pitch 公开成员）+ 输入驱动移动（继承 Entity 物理）
             + 游泳（P9）：isInWater()=AABB 与任何 fluid>0 格相交（每 update 判一次）；
             水中：重力 12/终端 4/水平 3.1（冲刺无效）/Space 随时上浮 4.5
src/entity/  （P10）Entity 基类：底面中心 position + AABB(width×height) + velocity + onGround；
             moveInSubsteps(dt,maxSpeed)：子步 ≤0.4 块/子步、≤64 步，每子步 integrate(h) →
             moveAxis X,Z,Y → afterSubstep(blocked)；被挡轴精确贴块面 + 该轴速度清零（滑动），
             Y 向下被挡 = onGround；重叠严格 `min < b+1 && max > b`（整数边界 float 精确，
             无 epsilon 抖动）；solid 只读 properties.solid（水可穿过）；teleportTo/setVelocity
             （测试入口）；shouldDespawn()/belowKillPlane()（y < -32）；实体不自删（game 延迟移除）
             Player 并入：integrate()=输入驱动移动+游泳/跳跃；m_spawnPos 虚空重生
             ItemEntity（P11 改）：持**物品名**（game 经 ItemRegistry 解析）+ 数据 maxStack
             （合并上限=min(item.maxStack,64)：工具 maxStack 1 永不合并）/0.25 立方/120s 消亡
             （MC 同款）/**0.5s 拾取延迟**（kPickupDelaySeconds，MC 10 tick，防 Q 丢出即回吸；
             pickupReady() 供 game 拾取 pass）/同类 0.75 底心距合并（canMergeInto/absorb，
             game 做合并 pass）/重力 32 终端 38/水平指数摩擦 2.5（0.005 硬清零）/零初速生成
             （静止位精确、截图可复现，刻意偏离 MC 随机初速）
             Projectile：0.25/发射 15（MC 满力箭）/重力 20（MC 0.05/tick²）/终端 38/30s 消亡/
             afterSubstep(blocked)→m_dead（撞 solid 钉住，MC 箭行为）；无游戏内发射器
             （弓=后续阶段）、无渲染几何
             item_mesh：makeItemCubeGeometry(block,layout)：24 顶点/36 索引/全 6 面（不剔除）/
             中心 ±0.125/烘焙 sky=1.0 block=0（shader dayFactor → 夜间变暗）
             src/item/    （P11）item.hpp（header-only）：ItemID（invalid=空槽）/Item（name/type/
             block/breakSpeed/maxStack）/ItemStack（item+count，empty()）/ItemType{Block,Tool}/
             breakDurationSeconds(hardness,speed)=1.5×hardness÷speed（hardness≤0=即时；kBreakBaseSeconds）
             ItemRegistry：buildBlockItems(blocks)（每个数据方块隐式同名 block item，air 跳过；
             必须先于 loadDirectory 调用）+ loadDirectory("data/items")（显式物品**只能是**
             type:tool——block item 以方块定义唯一来源，防第二数据源；break_speed>0、
             max_stack 1..64、重复 id（含撞方块名）=报错；未知字段警告）
             Inventory：27 格（kHotbarSize 9 + 18 storage）；add(item,count,registry) 确定性
             （先合并同物堆栈再首空格，槽序升序=hotbar 优先）→slot 或 -1（满）；take(slot,n)；
             isFull；add 前置 count≤maxStack（game 保证：实体合并受 item 上限约束+Q 丢 1 个）
             game 集成：m_items/m_inventory/m_selectedSlot + 破坏状态（m_breakTarget 格 +
             m_breakProgress 秒）；init 顺序 blocks→items(build+load)→**drops 严格校验**
             （未知名=启动报错）→hotbar.json {"items":[≤9 物品名]} 填满初始 hotbar（满堆栈）；
             handleInteraction(dt)：左键**按住**=updateBreaking（目标变/松键/解锁/Tab→
             resetBreaking；duration≤0 即时破坏）、右键=placeBlock（选中格须 block item，
             放置后 take 1）、Tab=cycleHotbar（mod 9 + resetBreaking）、Q=dropSelectedItem
             （选中格 take 1 → 玩家底心零初速生成 ItemEntity）；updateEntities 合并 pass 后
             拾取 pass（索引序：pickupReady && 底心距≤kPickupDistance=1.0 → inventory.add，
             满则留地重试）；drawItemEntities 经 item 解析 block（非 block 物品=工具跳过
             不渲染）
```
