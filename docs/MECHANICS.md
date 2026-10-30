# 机制详解（现状 v1.33.0）

> 2026-10-30 全面刷新：现状 = **v1.33.0**（tag = PRE-11+12 合并）、无输入基线门
> **148** 次、单元测试 **1912/1912**、`-Wall -Wextra -Wpedantic -Werror` 0 警告、
> 存档 **format 22**、v1.x 任务矩阵 **33/33 全完成**（v1.0 矩阵 34 项 + v1.x 33 项
> 均已归档）。旧版本文档是 Phase 28 快照 + 逐点补丁（尾部 LGT-2/LGT-3），本次把
> 全部子系统更新到现状。
>
> **来源约定**：本文所有具体数字/字段名/常量值以本文件各条的标注为准，细节
> （机制公式、单测、门证据、偏离记档全文）见 `docs/archive/phase-*.md`（164 个
> 阶段存档）；活跃已知问题与已修复索引见 `docs/KNOWN_ISSUES.md`；逐版明细见
> `docs/CHANGELOG.md`；执行协议（命令/硬规则/E2E）见 `docs/PROTOCOL.md` 与
> `AGENTS.md`。
>
> **阶段标签约定**：`Phase NN`（或 `[PNN]`）= v0.x 时代编号阶段；`[XXX-N]` =
> v1.x 任务表任务 ID（如 `[ENT-7]`/`[SURV-3]`/`[CRAFT-1]`/`[PRE-2]`/`[STRM-1]`）；
> `[99.N]` = P99 后性能审计系列。
>
> **config 约定**：下文「代码默认」= 缺键回落值；`config/config.json` 是**已提交
> 文件**，含用户调值（`world.view_distance` 80【代码默认 8】/
> `world.day_length_seconds` 600【代码默认 240】/ `ui.scale` 1.5【代码默认 1.0】/
> `world.seed` 123456789 + `player` 节全默认值）——文档与行为以代码默认为准，
> 实际运行以已提交 config 为准。

## 内容总览（现状）

- **技术栈**：C++20 / SDL3 / OpenGL 3.3 core；自研 `Result`/JSON/PNG；
  **内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，不改核心代码，注册表
  严格校验）；**确定性是一级要求**（无输入运行逐帧字节可复现）
- **方块**：**72** 个数据方块（`assets/data/blocks/`）+ 内置 air 哨兵（非数据，
  `BlockID{0}`，注册表从 id 1 分配）。模型形状分布：full cube（多数，缺省——
  含 leaves/cactus 等透明或实心块）/ cross 17（wheat_1..7、3 花、3 sapling、
  torch、fire、campfire 家族）/ stairs 4 / slab 1 / wall 1 / fence 1 /
  glass_pane 1；3 个 sapling 另带 `tree` 对象（shape 词汇 {oak, spruce,
  birch} = 落树形状）
- **物品**：**151** = 72 个隐式 block item（与方块同名，无独立 JSON——注册表拒
  显式同名物品文件，「block items are defined by their block」）+ **79 个显式
  物品**（`assets/data/items/`，`[ENT-9]` 起**全部**带 1..16 box 的 3D `model`
  段 = 三上下文渲染全覆盖）
- **状态效果**：**11**（`assets/data/effects/`，见「状态效果」）；**Mob**：**6**
  （`assets/data/mobs/`，见「实体与 Mob」）；**配方**：**66**（shaped 34 /
  shapeless 32，`assets/data/recipes/`）；**生物群系**：3（desert/forest/plains）
- **食物**：10 种（`restore_hunger`，见「生存系统」）；**染料**：15 色；**护甲**：
  12 件；**耐久**：28 件（9 工具 + 6 弓 + 燧石与钢 + 12 护甲，`[SURV-3]` 首批 27
  件 + `[SURV-5]` poison_bow）
- **音效**：**39** 个确定性 WAV（`tools/generate_sounds.py` 固定种子，重跑字节
  幂等；17 基础事件 + 18 材质音 6×3 + 3 环境音 + 1 smelt）
- **纹理**：block atlas **16×8**（128 tile，`assets/data/textures/block_atlas.json`
  数据驱动布局——加纹理 = 加 PNG + 加一行 JSON，不改代码；**atlas 空位**纪律 =
  不重锚基线）；物品图标 = 每物品独立 32×32 PNG；字体 **256×512**（ASCII 16×16
  字形 + **33 个 CJK glyph** `[PRE-1]`/`[CRAFT-4]`）；health/hunger 48×16 条带、
  oxygen 16×16、effects **176×16**（11 tile）、cracks 80×16、mob 图集
- **初始库存**（`assets/data/hotbar.json`）：hotbar 9 格（stone/dirt/grass/
  sand/wood/leaves/glass/glowstone/wooden_pickaxe，每格满堆栈）+ **隐藏 starter
  存储槽 16–26**（honey_bucket ×1 `[FLUID-6]` / glass_bottle ×4 + 6 瓶单效果药水
  `[SURV-4]` / oak·spruce·birch sapling 各 ×3 `[GEN-2]`）——隐藏槽不渲染，
  无输入零活动
- **CLI**（`--help` 看全部）：`--frames N`（到点退出）/ `--screenshot /abs.png`
  （帧边界截图）/ `--save slot` / `--new`（忽略现有存档开新档）/ `--seed N` /
  `--time 0..1` / `--config path` / `--log-level` / `--save-full-diff`（`[PIPE-2]`
  oracle 旗）/ `--list-saves` / `--copy-save S D` / `--rename-save S D` /
  `--delete-save S`（`[SAVE-3]` 管理旗，独占）

## 操作与交互

- **第一人称玩家**：`W/A/S/D` 行走（4.3 b/s），`LeftShift` 冲刺（5.6 b/s，
  非水中 + 移动；疾跑另见饥饿 ×2 与熔岩内抑制），`Space` 跳跃（仅贴地时；
  水中 = 上浮），`LeftControl` 潜行（身高 1.8→1.6 脚底不动、眼高 1.62→1.42、
  速度 ×0.3、1 格崖缘 guard、**禁用 step-up**、低顶陷阱：头顶 <1.8 松键不站起）；
  鼠标转向——**左键点击锁定指针；转向仅在指针锁定时生效**（未锁定光标不转
  视角 = 无输入运行截图逐字节复现的保证，硬规则 5）：yaw 0 = 朝 -Z、正 =
  左转；pitch 正 = 上、clamp ±~89°；灵敏度 0.0025 rad/px
- **左键**（锁定，**按住**）：准星命中 mob（射线先比方块距离——**方块遮挡**：
  方块更近则打方块）= 挥击，每 `kSwingInterval` **0.25s** 一击：伤害 = 选中
  物品 `attack_damage` 数据（徒手/方块/材料 = 1.0；shipped：wooden/stone/iron
  pickaxe 2/2.5/3、axe 3/3.5/4、shovel 2.5/3/3.5、bow 1）− 活跃效果
  `melee_penalty` 总和（clamp ≥0）+ 活跃效果 `melee_bonus` 总和（`[P84]`）；
  沿瞄准方向击退（水平 4 + 抬升 2.5，`kKnockbackTime` 0.3s 窗口内 AI 不接管
  速度）；**工具/弓/点火器近战命中 = 耐久磨损事件**（`[SURV-3]`，见下）。
  未命中 mob = **破坏方块**
- **破坏**（DDA 体素 raycast，`kReach` 5 格；raycast 把水体当 air 穿过 = 可隔水
  破坏湖底/水下方块，水被排水后流体模拟补回）：耗时 = `kBreakBaseSeconds`
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
  `F5` 立即存档（`--save` 槽激活时，日志记录结果）；`ESC` 释放指针 / 未锁定时
  退出（干净退出自动写回）；`R` 死亡后重生
- **背包面板操作**（`[CRAFT-7]` = MC 操作面 3/4 项 + 数字键）：左键 = 取整堆/
  放入/交换/并入有空间同物堆（**合并门 = 同物品且同 damage** `[SURV-3]`）；
  **右键取半堆**（空手右键占位格取 floor(N/2) 最小 1：N=1 取自身 / N=2,3 取 1 /
  N=64 取 32；共享 cellRightClick 原语三面板同语义；耐久随堆）；**Shift 快捷
  移动**（hotbar⇄storage 互移索引序合并 + 容器格→主背包优先 + 堆跨格分裂 +
  手持堆 no-op；炉子面板 = 容器 GUI：炉槽→主背包、移 input 按不变量重置进度；
  左右 Shift 皆可）；**双击收集分拣**（**MC 分区语义：hotbar 0..8 与主背包
  9..26 独立分区**；收集同物同耐久入点击格 + 溢出索引序回填；`kDoubleClick-
  Seconds` 0.3 真实 dt 窗 = 生存计时器类输入态）；**无拖拽**（记档偏离：三
  点击 + 数字键覆盖核心操作面，拖拽 = 捕获下轨迹跟踪最高风险 E2E 类矩阵外）；
  点击面板外（左键）= 丢弃手持堆；关闭面板时手持堆保留在 UI 实例（MC 会退回
  背包 = 记档偏离）；**悬停 tooltip**（`[PRE-12]`，见「渲染与表现」）
- **破坏反馈**（`[P15]`）：**渐进裂纹**——破坏进度映射 5 级累积裂纹纹理
  （cracks.png 80×16 五格，白 alpha 随机游走裂纹），破坏中的方块叠加半透明
  裂纹面（scale 1.006 免 Z-fight）；**破坏粒子**——方块碎成 16 个 0.12 格小
  立方体（初速弹散 + 重力，0.45~0.75s 消亡，无碰撞，spawn 时烘焙所在格光照——
  渲染是只读窗口不能读光数组），按桶路由（`[PRE-5]`：opaque 块粒子留 opaque /
  cutout 块（叶、十字植物、fire 余烬）进 cutout / 其余 blend），逐帧动态 mesh
  （全局粒子上限 256）

## 世界与流送

- **世界**：16×16×16 chunk，世界高度 128（y ∈ [0,128)），X/Z 无限；同一 seed
  逐块复现（`WorldGenerator` 无状态纯函数 `generateChunk`，worker 并行调用 +
  单元测试 + 像素级截图比对保证）；**演示区 = 4×4 chunk 列**（world x,z ∈
  [−32,32)，启动同步生成）
- **流送圆盘 `[STRM-1]`**（v1.0 任务表首项）：流送环 = **圆盘**（纯整数谓词
  `dx²+dz² ≤ R²`，方形 (2R+1)² 环砍除）+ **远平面帽** `R_eff = min(view_-
  distance, 64)`（`kStreamCapRadius`；推导 = 远平面 `kFar` 1000 块——可绘制列
  全部 d ≤ 63.91，d ≥ 65 的列最近角 ≥ 1002 块原理上永不渲染，流送纯浪费；
  用户定案选项 b「只装可绘制列，地平线视觉零下行」）——vd80 实测 RSS **9.08 →
  4.59 GB（−49.5%）**；R64 圆盘 = **12853 列** ≈ 103k chunk
- **触发迟滞**：玩家距缺失列 ≤1（平方欧氏；整数偏移下 = 玩家列 + 4 轴邻列，
  对角 (1,1) 不触发）激活，半径圆盘充满前不解除；静止玩家（距边缘 ≥2 列）
  永不触发 = **无输入运行零 job、基线截图逐字节复现**
- **填充预算**：每帧 **2 列**（`kStreamColumnsPerFrame` 2，`[99.8]`），距离
  优先（最近先）
- **卸载 `[99.2]`**（旧版「只增不卸载」已推翻）：距玩家 Chebyshev >
  view_distance+1（`kUnloadHysteresis` 1 防抖带）的列卸载（pool flush 后主
  线程单写窗口）；保留 = 演示 4×4（重生安全）+ 玩家编辑列（m_editedColumns，
  存档 diff 所在）+ 五类 per-block 状态列（炉子/营火/作物/信标/火，存档交叉
  校验对象）+ 在途生成列；实测走路 85 s RSS 平台 **470–474 MB**
- **流体列保留 `[STRM-2]`**（仅流体模拟改动的列卸载后不再重置生成态）：
  驱逐时按 journal 格限域快照该列 live≠fresh diff（`retainColumnDiff` =
  `journalDiffChunk` 字节同语义，**存 diff 条目非 chunk = 内存有界**）→ 再
  集成回放（blocks 先 fluids 后 = applyWorldEdits 序、observer 再 journal 化
  = 自相似循环、**不插 m_editedColumns** = fluid-only 列保持可卸载）→ 退出
  存档双路径 `mergeRetained`（不变式：每列至多在 journal 或 retained 之一 =
  无重复格，`--save-full-diff` oracle 不失真）；**随场景首触发修复 99.8
  活动门流送边界活性缺口**（冷流前对 void 被拒后目标列到达不唤醒 = 级联永久
  卡边；修 = insertChunk/getOrCreateChunk 的 wakeFluidChunks 镜像 setBlock
  UNSTICK，唤醒仅在插入事件 = 性能面零回退）
- **worker 网格化 `[PIPE-1]`**：mesher 计算搬 **4 worker** 快照 job（每帧至多
  2 个列快照 job：目标列 **3×3 邻域 9 列 × 8 chunk** memcpy 进 job 私有临时
  World，worker 用**零改动的 ChunkMesher** 出 8 chunk 几何；次帧 flush 后主
  线程 move 进 m_chunkMeshData + 清脏 + **列合并 GL 上传**【GL 上下文只在主
  线程，~1–3 ms/列 = 不可搬走部分】）；选择规则 = 脏列投影 ∩ 非 unlit ∩ 距
  当帧 light-pending 列 Chebyshev ≥2（光 footprint 会改本帧快照 = 保持脏下帧
  入选）∩ 非在途，最近优先；帧时序 pop-in 晚 1 帧（帧锁定确定）；动机 =
  vd80 step-0 实测主线程 remesh 填充期 7–11.3 ms/帧 + 2.8 min 拖尾
- **lit pop-in `[99.4]`**：新列集成后进 `m_unlitColumns`，光照 worker job 完成
  前**不网格化**（chunk 无 mesh = 不可见，等同 void），job 完成后一次性网格为
  正确光照——消灭「首帧光 0 + 二次网格」黑面（每新列只 mesh 一次，工作量减半），
  帧锁定确定性 pop-in
- **渲染预算 `[99.9]`**（vd80 旧管线 2–30 FPS 坍缩三根因修复）：**视锥裁剪**
  （列级保守 AABB）+ **列合并 VBO**（draw ÷8）+ **光照 chunk 级重构**（每 16³
  块一次查找 + BFS last-chunk 缓存 + 既有地形重扫零写入跳过天空 BFS——
  新地形必须强制传播，`newTerrain` 参数）；实测 vd80 流送中 render 37 →
  1.5–2.5 ms/帧（不随环增长）、flush 25–30 → 13–17 ms、主线程 CPU 100% →
  40%（4 worker 各 16–23%）
- **流体 per-cell 活动 `[PIPE-3]`**（见「流体」节）：tick = O(活跃 chunk +
  活流体格)，vd80 流体段 −50%、活跃 tick 2.8×
- **死亡暂停 `[P33]`**：`streamChunks()` 在 `isDead()` 时提前返回（在迟滞逻辑
  之前，激活状态保留）——不暂停的话死亡画面背后会按 view_distance 继续填满
  半径盘（vd80 ≈ 12853 列 / 数 GB / 数小时）；在飞的 ≤1 列经 post→flush 窗口
  自然排空，`postLightJobs` 照常照亮（否则该列停在光 0）；`R` 重生后从保留的
  迟滞状态继续
- **放置到未生成区域**：同步生成该列（放置当帧生效，一列 ≈1.6 ms 生成尖峰），
  流式管线随后按正常路径补光照
- **读档点在演示区外**：触发器对「玩家自身列缺失」= 距离 0 立即激活，~2–4 帧
  补上，期间玩家从存档位置先下落几帧（实测 ~0.03 格，逐位精确落定；无死亡
  风险，远小于 −32 杀线）

## 物理与碰撞

- **玩家 AABB**：0.6×1.8（**position = 底面中心**，硬规则 6；重叠判定**严格
  不等式**无 epsilon）；**轴分离解析 X→Z→Y**（撞墙该轴停住、另一水平轴继续
  = 滑动；贴面精确解析无抖动）；**子步化防穿透**（每子步 ≤0.4 块、≤64
  子步，硬规则 6）；重力 32、终端速度 38、跳跃初速 8.5 → **跳高 ≈1.129 块**
  （可跳上 1 格台阶，跳不上 2 格）
- **跳跃冲量计入子步 maxSpeed `[P99]`**：pending 跳跃（onGround + Space +
  非流体）时子步 maxSpeed ≥ jumpSpeed——原长帧（流送卡顿 86 ms）单子步整帧
  跳速位移使顶点膨胀到 ~1.65 翻过 1.5 barrier = 帧率依赖物理（P99 物理加固
  ×2 之一）
- **移动惯性 `[ENT-7]`**（消费 Phase-5 记档「松键即停」）：地面/空中**指数
  趋近模型**（fps 无关 `exp(−k·dt)` 真实 dt）：有 wish `v += (dir×S − v) ×
  (1 − e^{−k·dt})`（地面 `kGroundAccelRate` 8.0 敏捷起步 / 空中
  `kAirSteerRate` 2.0 弱转向）；无 wish `v ×= e^{−k·dt}`（地面
  `kGroundDecelRate` 12.0 = MC 0.5448/tick 摩擦档，4.3 walk 松键滑 ~0.36 块 /
  空中 `kAirDecelRate` 1.9 = MC 0.91/tick 拖曳档，松键滑翔 ~2.3 块）；
  `kVelocityStopEps` 0.001 **恰好 0** = 静止态确定性红线；**FLUID-2 档保持
  瞬置**（流体模型独立调参 = 记档偏离）；mob 侧范围外（AI 每帧设精确速度 =
  既有设计）
- **1 格自动台阶（step-up）`[ENT-3]`**：非潜行 + 贴地 + 身体贴住整数块面
  （1e-4）+ 面前脚位 **`solidTopHeight ≤ 1.0`**（`[P99]` 加固 ×2 之另一：原格
  solid 属性判定会踏上墙/栅栏 1.5 实心区的帽 = 翻越隐患）+ 其上方 2 格为空 →
  自动抬升 1 格并继续本 tick 水平移动（slab 0.5/满块 1.0/glass_pane 可踏、
  墙/栅栏 1.5 不可踏、楼梯脚位点采样）；判定**纯几何上提 `Entity::stepUp`，
  player 与 mob 同一函数**（player 行为逐位不变 = test_player 零改动红线）；
  2 格仍须跳跃。**P16 两个潜伏 quirk 记档不修**（`[ENT-3]` 探针实证，
  player/mob 共享）：① +X/+Z 方向恰 1 格宽台阶踏不过（front 格查 snapped+1，
  ≥2 格宽或 −X/−Z 正常）② 非整数脚高（slab 顶/楼梯斜面）face 巧合对齐整数可
  触发凭空抬升 1.0 再落回；修复 = player 行为变更单独立项
- **barrier 块**：wall/fence `solidTopHeight` **1.5** = 不能穿过（AABB 与实心
  区 [y, y+1.5] 重叠）也不能跳过（1.129 < 1.5）；**帽伸进上方格**：
  `hasRoomAt`/`moveAxis` 的 y 扫描扩到 y0−1（AABB 在 y+1 格时查 y 格的帽；
  非墙格 top ≤1 为 no-op backward-compat）；glass_pane = 整格实心 **1.0 可跳**
- **斜面碰撞**（楼梯 `[P87]`/`[P89]`）：`solidTopHeight(id, mn, mx, cell)` 按
  facing 沿对应轴取 **AABB 覆盖的最高斜面点**（north = 1−mnLocalZ / south =
  mxLocalZ / east = mxLocalX / west = 1−mnLocalX——`[P89]` 修正低端边切入高端
  bug）；slab = 固定 0.5、满块 = 1.0（忽略 AABB，backward-compat）
- **坠落伤害**（`[P16]`/`[P36]`）：`max(0, 净下落格数 − fall_damage_offset)`
  （offset 代码默认 **3** = MC「3 格安全」，config 可覆盖），从离地帧记录
  `fallStartY` 到落地结算；**落入流体的落地取消伤害**（落地格 fresh 流体检查）；
  **读档经 `restorePosition` 重钉 `fallStartY`**（`[P36]`：空中读档没有「离地」
  转换，不重钉则保存的坠落满血落地）；掉出世界 **y < −32 = 直接死亡**
  （`damage(maxHealth×1000)`，走死亡/重生流程，背包保留 = keep-inventory
  记档）
- **Mob 坠落伤害 `[ENT-5]`**：落地块镜像玩家规则（净落差 > 3 且落地格 fresh
  无流体 → `damage()` 既有死亡/掉落管线）；mobs ephemeral = 构造一次钉
  fallStartY + 离地转换重钉（1 子步滞后 = 确定性紧公差）
- **流体交互 `[ENT-4]`/`[FLUID-2]`/`[FLUID-3]`**（玩家/ mob / 掉落物共享
  `Entity::refreshFluidState` + `integrateFluid`，`isRestingOnSolid` 纯上提）：
  inFluid = AABB **严格重叠**水或熔岩格（玩家 `inWater()/inLava()` 同款并集，
  水/熔岩并存时熔岩优先）；`W'` = 脚中心列最高 L8+1 真水面上扫（`[FLUID-3]`
  浸没体改真水面上扫，干身读逐位恒等）；**atSurface** = 脚面离 W' 0.4 带
  （kind 记忆：漂浮终态脚面离水由该带维持——否则熔岩漂浮终态塌）；
  优先级 = **浮漂 snap 不动点**（fps 无关；`[FLUID-3]` 浮漂终态 = **游泳位
  W'−h/2** 半浸没 = MC 游泳观感，原「脚面 = 水面干身踩水皮」= 用户报障
  「站在水上」已修）> **被动上浮**（buoyancy 12、帽 1.5、vy ≥ 0 门【带动量
  入水沉底 = MC dive 语义】+ 池底站立 fresh resting 门 + 流膜无面门【纯流动
  膜无 L8 支撑 = 无浮面无上浮，Phase-9 下沉】）> **下沉帽** 4（**流体中零
  重力**）；`[FLUID-3]` 下沉/上浮 **inFluid 门**（水面以上 0.4 干带 keep vy =
  全速过带，悬崖坠水「降落伞」修）+ **深度门**（池深 ≥2 的 resting 体上浮出
  底，1 格浅池站底不变 = E4 主线）。玩家数值（`player.*` config 可覆盖，代码
  默认 = 下表）：水 swim 3.1 / Space 上浮 4.5 / buoyancy 12 / 被动上浮帽 1.5 /
  潜行下潜帽 1.5 / 终端 4；**熔岩**（`[P54]`，比值镜像 MC 运动系数
  0.014/0.0231 ~0.6 + 弱浮力）swim **1.9** / 上浮 1.5 / gravity 4 / 终端 2 +
  **灼烧 4 HP/1.0s real-dt**（`lava_damage`/`lava_damage_interval_seconds`，
  走正常伤害路径；离开熔岩重置未到期直伤 tick——残留燃烧是 `[P57]` fire 效果，
  另一系统）+ 落入熔岩**取消坠落伤害** + 熔岩内抑制疾跑/下蹲；浅水（眼在
  水面上）= 蹚水走/换气（`[SURV-2]`），深水游。**Mob**：AI 状态机零改动 +
  涉水/水下水平 ×`kUnderwaterSpeedFactor` **0.6**（命名常量，AI 与击退同门，
  无 per-mob 字段 = YAGNI 记档）+ **`[FLUID-3]` 踏阶 `!m_inFluid` 门删除**
  （涉水 mob 可踏 1 格岸壁出池 = 玩家对齐；游泳位 mob onGround=false 仍无
  step-up = 无 swim-up 爬岸记档偏离）+ 站立 mob 不上浮（同 MC）；**掉落物**：
  同款浮力（预置静水浮面静止无漂移【摩擦保留】+ 带动量坠水沉底站立互斥）+
  **熔岩对称同浮力**（记档偏离）；**箭穿过水不动**（投射物边界，记档）
- **实体分离 `[P19]`**：每帧分离 pass（固定顺序：玩家先、再实体列表按索引；
  中心精确重合时确定性 tie-break）——重叠对沿**最小穿透轴**分离（两个可推动
  体 50/50、对不可推动体走全量面到面跨度；体只移动到其 AABB 可占据的位置
  `hasRoomAt`，被 solid 挡住的一侧份额由另一侧全量接管，两侧都挡则跳过该
  对）；重复 sweep 直到最大位移 <1e-6 块（亚视觉残差）或 32 次硬上限；
  参与/可推动为**数据位**（`collidesWithEntities`/`pushableByEntities`，不按
  类型硬编码）：投射物不参与（箭穿过实体）、**玩家不可推动**（mob 在玩家脸前
  停下）；玩家 ↔ 掉落物分离（`[P16]` 起源）= 沿最小穿透轴把物品推开（远离
  玩家中心），玩家自身不被动位移
- **出生点**（硬规则 10）：演示区 4×4 chunk 列内按 `kSpawnCandidates` **10 个
  候选列**（原点偏移 0/±4/±8 块）顺序找**干燥陆地**（h > seaLevel 且出生
  AABB 内无 solid = `hasRoomAt`，避开树干/树冠/流体）；**世界出生点**在读档前
  捕获 = 敌对保护环锚（读档不改变它，MC 语义；v1 无床）

## 光照

- **双光场 0–15**：每 chunk 两个 16³ 字节数组（sky/block）。**天空光** = 列顶
  扫描（最高不穿透格上方 = 15，其下每层扣该格 `light_opacity`）+ BFS 传播；
  **方块光** = `properties.light_emission` 源格 BFS（同款进入格成本）；破坏/
  放置后只重传播受影响区域（天空光 = 变化点 ±16 **整列**——垂直天空光无距离
  限制；方块光 = ±16 盒），光值实际变化的 chunk 才重建 mesh
- **`light_opacity` `[LGT-2]`**（0–15 字段；**缺省由 opaque 派生** = opaque 15
  不穿透 / 非 opaque 0——未设内容逐位复现旧行为；统一**进入格成本**：流体
  **3**（各方向），否则垂直天空 = O、其余方向 = max(1, O)，O = 15 不穿透；
  emission 源格自身持值不受影响）。shipped 赋值：**leaves 族 2 / glass 1 /
  流体 3（固定，不读块字段）/ slab·stairs×4·wall·fence = 0**（`[LGT-5]` 半块
  透光 = MC 半块规则：光如空气穿过，竖直天空免费 + 每格成本 1 = 与空气逐值
  同，**零核心代码**；opaque 保持 true = 剔面/渲染零变更）
- **昼夜 = 天空光场仿射撤退 `[LGT-3]`（零重传播）**：天空光场**恒按正午
  （源等级 15）存储**；有效源等级 **`S = 7 + 8×dayFactor`**（深夜 =
  `kNightSkyLevel` **7** = MC 午夜露天月光——10-27 用户实机定案「深夜须可
  夜视」，初版全黑被推翻）。所有天空光源共享源等级 S 且逐格成本与 S 无关 ⇒
  **`field_S(x) = max(0, stored(x) − (15−S))` 逐格精确**（confluence 单测 =
  源等级 S 直接重传播 vs 仿射逐格相等）——日落时亮区沿 `stored = 15−S`
  等值线向洞口/冠隙收缩（MC 同款撤退观感），零重传播、分数 Δ 平滑（MC 量化
  超集）。**生成光门 `currentSpawnLight` 与渲染求值同一公式**（所见即所生）
- **面烘焙光公式**（basic.frag，归一化 0..1，硬规则 9）：`light = max(
  v_skyLight − (8/15)×(1 − dayFactor), v_blockLight)`，`rgb = base × (0.03 +
  0.97 × light × faceShade)`；**faceShade** = 顶 1.0 / ±X 0.8 / ±Z 0.6 / 底
  0.5；3% 环境光底（全暗区与虚空背景可区分）；每面采样**面后方一格**的光值、
  4 顶点同值（块状光照，无 AO = KNOWN_ISSUES）；**方块光不受昼夜影响**
  （glowstone 夜间也亮，MC 一致）
- **per-draw 光覆盖 `[PRE-6]`/`[LGT-4]`**（vert 端唯一门控分支，frag 零改动 =
  昼夜缩放/环境底/雾/波纹自动继承；**draw 后强制 rebind 复位** = per-draw 状态
  泄漏纪律 `[UXFIX-1]`）：**掉落物** = `itemDropLight` 纯函数采样 **AABB
  中心格**（position 底面 + height/2，逐轴 floor 负坐标安全）sky/block ÷ 15，
  **每帧采样无缓存**（光照变化自然跟随，2 world 读/掉落/帧可忽略）；**信标
  光束** = `beamLightSample` 采样**信标块上方空气格 (x, y+1, z)**（= beam
  最低格；首版「块自身格 opaque 恒 (0,0)」缺陷被门 119 E2E 揪出修复，
  OwnCellOpaqueAlwaysIgnored 回归单测钉死）——深夜月光 7/15（LGT-3 仿射
  继承）/ 近萤石亮化 / 白天恒白（df = 1 恒等根）
- **并行与异步 `[99.10]`/`[99.11]`**：流送光照 job 与流体追赶光照 job **各至多
  2 个/帧**（同窗内两两位级独立判据 `lightRegionsIndependent` = 写 reach ±1
  列 + 块光 BFS 读环 +1 列双向不相交【gap ≥4 任轴】纯函数，依赖对回退单 job
  顺序 = pre-99.10 逐位行为）；净变 chunk 走 job-local `dirtyOut`（解共享写
  竞态），flush 后主线程统一 markChunkDirty → 列重烘；**流体光异步化
  `[99.11]`**：流体 tick changed 格 → **列粒度**待集 `m_fluidLightPending`
  （同列合并）+ 每帧 ≤`kFluidLightColumnsPerFrame` **2 列** worker 追赶
  （`newTerrain=true` 强制天空传播 = 流体洞修正）+ **幂等收敛**（排空 ==
  propagateAll，跨类同窗位级独立排除）；vd16 填充 244.7 → 44.6 s（5.5×）、
  update p95 7.6 s → 80 ms
- **潜伏未修（记档）**：sky rescan clobber——天花板板骑跨光照 footprint 边界
  时顺序两次 region 传播后一 rescan 把边界格 sky 覆写，1–2 格宽 stale-dark
  暗带（`[LGT-1]` 设计时发现；`[LGT-2]` 下后果扩到 stale-partial；暴露面窄
  暂留未修，见 KNOWN_ISSUES）
- **光源**（`light_emission`）：glowstone 15（hotbar 内）/ lava（流体参考块）
  15 / fire 15 / campfire 15 / torch **14**（MC 值）/ campfire_extinguished 0 /
  beacon 0（光束不提供方块光 = 不照亮周围，`[P83]` 记档；`[LGT-4]` 后光束
  自身按上方格受光）

## 流体

- **编码**：每 chunk 每种类 0–8 level 字节数组（8 = 源/满，1–7 = 流动；同一
  cell 水与方块互斥）：干 0 / **水 1..8 / 熔岩 9..16 / 蜂蜜 17..24**（`[FLUID-6]`
  三层化，水/熔岩带位级不变）；`setFluid` 正 level **替换异种**、0 只清本种
- **节奏**：`FluidSim::tick` 游戏线程**每 3 帧**（`kFluidTickEveryNFrames` 3，
  ≈20 Hz）快照式确定性（无输入运行逐字节可复现）
- **模拟规则**（水/熔岩/蜂蜜各自快照、按种过滤；**异种流体格按「方块」处理**
  = 支撑 + 挡扩散，防惰性配置跨种替换振荡）：下落（下方空/流动则沿列传送带下
  移 = 成对移动净零；链达世界底部 y<0 排出 = **δ** 记账；满格失撑也下落 = 对
  MC 刻意偏离）/ 静止水平扩散 L−1（1 级终端；扩散前缘创生/抬升浅流 = **σ**
  记账，冲突目标取 max）/ **统一池规则 `[FLUID-1]` 三分支**（静止在同种流体
  level B 上：① B==8 → 本格拉满到 8（**κ**：source 托底的柱是满的——破坏湖底
  后整列补满）② 0<B<8 且 L<B → 本格拉入池（**α**：池水位不动）③ L≥B →
  **静态沉积**（本格保留、零写入——`[FLUID-1]` 前「把池抬升到 L」分支删除 =
  唯一无源体积增益路径））。**守恒 = 逐 tick 会计恒等式** ΔF = σ−α−κ−δ
  （「总量 ±0」是错误目标——MC 流前缘创生邻居合法；封闭盒 64 格精确终态表 +
  P90 表面湖精确终态计数单测逐 tick 钉死；`[FLUID-1]` 路线 A = 只删抬升分支，
  静态沉积稳压器保留 = churn 回旧水位）
- **反应 pass `[P51]`**（tick 第一步）：快照扫所有熔岩格：6-邻域任有**水** →
  该熔岩格 `setBlock` 为熔岩块 `water_reaction` 的产物（`{source, flowing}`
  严格配对在**方块数据**上：邻水含 level 8 源 → **obsidian**，否则 →
  **stone**；**水不消耗、无蒸汽** = v1 记档偏离；无 `water_reaction` 的惰性
  熔岩不反应可邻水共存）；稳态不变式 = 无 6-邻接的水-熔岩对（反应必收敛出
  石头/黑曜石墙）。正面争夺同一空格 = apply 顺序（水先熔岩后 = 熔岩胜出）是
  确定性实现细节非数据规则
- **燃烧 pass `[P58]`**（tick 第二步）：快照扫所有熔岩格 6-邻 **BLOCK** 格
  `flamable: true` → 收集排序后 `setBlock`（`[P64]` 起 = **fire 块**，**无
  掉落**；注册表无 fire 块回落 air = 单测 fixture 语义）；无 flamable 块 =
  fast-exit
- **蜂蜜 `[FLUID-6]`**：第三流体种类 = **粘度帧锁定 tick 门控**（每 3 个流体
  tick 模拟一次 = `kHoneyTickPeriodFluidTicks` 3，水/熔岩零改动）；**无波纹**
  （emitFluid 波纹标记仅 kind==Water）/ **无 worldgen**（桶源 = honey_bucket，
  无蜂箱记档）/ 无 `water_reaction` 字段 = 反应 pass 数据驱动惰性；蜜/水/熔岩
  6-邻共存互不替换
- **活动门控 `[PIPE-3]`**（99.8 chunk 粒度 → per-cell）：per-chunk per-kind
  流体格列表（setFluidAt 成员变化 push / drain lazy stale / walk 过滤 + 列序 +
  去重 + 压缩，不变式 = 活格 ⇒ ≥1 条目）+ World per-kind 活跃 chunk 集合（6
  插入点穷尽 flag 置位 + eraseChunk purge）+ falling 标志 per-column 化
  （跨 chunk topFlags 引用语义位级同旧）+ reaction/burn 活格扫 + propose map
  max 合并（序无关 = 位级同果）；**tick = O(活跃 chunk + 活流体格)**；流体
  状态**逐位不变**（门 117 位级一致 oracle）；vd80 流体段 −50%（1261 → 631
  ms/300f 窗）、活跃 tick 34.8 → 12.6 ms（2.8×）
- **光照**：流体格光 = 该种类参考块的 `light_emission`（熔岩 15 = 发光池）；
  天空光**每穿一格水 −3**（`[LGT-2]` 进入成本 3，列顶扫描 + BFS 与不透明方块
  共用传播器）——深水湖底随深度变暗（截图可见浅水亮/深水暗渐变）
- **稳态与残余**：生成世界的水（海平面以下填充）处于平衡态（20 Hz tick 不
  扰动湖面）；**残余 churn** = 站流水瀑布/崖膜永久轮换（24 格类：视图外、
  ×2 RAW 确定、`[99.8]` 活动门只保对应 chunk 热——接受现状，见
  KNOWN_ISSUES 流体节）；表面熔岩湖溢出/燃烧 = 确定性但非静止（见「世界
  生成 · 熔岩湖」）
- **`World::setFluid`** 只写已存在 chunk 的 air 格；`setBlock` 会把所在格的水
  排掉

## 火系统

- **fire 块 `[P64]`**：cross + 火焰纹理（atlas 空位免重锚）、`light_emission`
  **15**（MC 值）、hardness 0（瞬破）、**无 drops**（破坏/燃尽都不掉）、
  非 solid/opaque/**非 flamable**（火是燃烧终态）；**来源**：① 点燃器右键
  flamable 块 ② 熔岩燃烧 pass ③ **玩家放置**（fire 有隐式 block item）④
  蔓延
- **蔓延 `[P66]`**（`world/fire_spread` 纯函数，镜像生长引擎范式）：每
  `fire.spread_interval_frames` 帧（**代码默认 6** = 0.1 s MC tick 节奏，固定
  帧步 `m_frameCount % interval` 门控）每活体火对 **5 邻**（水平 4 + **上**；
  **不向下**——MC 地火不烧地板）的 `flamable` 格独立 roll（splitmix64(世界
  种子 + 帧×GOLDEN + 位置哈希 + 邻序×GOLDEN2) < `fire.spread_chance`（**代码
  默认 0.05** = MC normal））→ `setBlock(fire)`（收口注册满寿命 = 新火继续
  蔓延）+ 局部光照；一格邻多火 = 每火各 roll、**只点燃一次**（输出位置序
  去重）；无火/无 fire 块/无 flamable/chance 0 全 fast-exit；**无音效**
  （防刷屏，记档）
- **余烬粒子 `[P66]`**（`fire.particles` 代码默认开）：每 **6 帧**（
  `kFireParticleEveryNFrames`）每活体火发 **2 粒**（0.12 立方体、火焰纹理、
  **负重力 −2 上升**、寿命 0.3–0.5 s、种子 = 世界种子 ⊕ 位置哈希 ⊕ 帧
  推进）；**燃尽/浇灭**死亡爆发 16 粒（复用 spawnBreak）；发射 = 固定帧步
  （确定性）、运动 = real dt（活余烬截图不可字节比 = PITS 条款）
- **燃尽**：`Game::m_fires` map（位置 → 剩余秒，setBlock 收口创建/删除 =
  炉子范式）；**real-dt** 倒计时（生存计时器类，硬规则 8）；`fire.
  lifetime_seconds` **代码默认 4.0**（>0 硬校验）归零 → `setBlock(air)`
  （无炭无掉落）
- **浇灭**：fire 的 6-邻任一格有**水**（任一级）→ 当帧瞬灭；**水胜火**
  （`[P57]` 规则）；**熔岩不浇灭**（池边的火是熔岩的产物，继续烧——MC 同款
  共存）
- **点燃（玩家 / mob 同节奏，`[P57]`/`[P74]`/`[P73]`/`[P58]`）**：`fire` 效果
  （1 HP/0.2 s = 5 HP/s 可致死，min_health 0，`negative: false` = **牛奶不
  灭火** MC 语义）+ 两数据标志（`ignites_on_lava` 至多一个注册效果可带 +
  `extinguished_by_water`，init 交叉校验）。**玩家**：`updateEffects` 每帧
  （count==0 早退前）**inLava() || isInFire()**（fire 块 AABB 严格重叠 =
  `[P74]`，`[P70]` 直伤机制整体退役——点燃走 P57 既有路径）→ **重施加**
  （duration = `player.fire_duration_seconds` 代码默认 **8 s** = MC Normal；
  重施加 = duration 重置 + tick 相位保留 → 火源内持续燃烧）；**inWater() 瞬时
  灭火且先于 tick**（水内零伤害、水胜熔岩）；离火源后 **8 s 残留** countdown
  烧至到期（MC 核心行为）。**Mob `[P58]`/`[P73]`**：生成时 `armFire`（参数
  从点火效果 + config 同源取）；燃烧 = **整数帧燃烧格**（`m_fireFrames` int
  计数，`llround(interval×60)`——double 累加相位 off-by-one 的修正）：
  火源内每帧重置（相位不清零）、离源 8 s 残留、水瞬时熄灭先于 tick、**可致
  死**（到期帧不 tick = `[P46]` 语义）；init 交叉校验：点火效果
  `tick_interval × 60` 必须整帧数（容差 1e-4 帧）
- **`flamable` 集 `[P63]`**（可选 bool 属性，非 bool 点名拒）：**wheat_1..7
  全 7 阶 + wood/leaves/grass**（MC 天然可燃面）——**羊毛刻意不燃**（染色
  产物保留不可燃，精确集单测钉死，记档偏离）；高草/火把（torch 非 flamable）
  未实现
- **存档**：format 10 `fires` 数组（{pos, remaining} 位置序、remaining > 0、
  与块编辑**双向**交叉校验 = 炉子键纪律；format 9 读档 = 空 + 手编旧文件
  fire 块补满寿命条目）
- **init 交叉校验**：registry 有**任何** flamable 块则**必须**有 `fire` 块
  （燃烧目标缺失 = 点名拒绝）；零 flamable 资产集合法（燃烧路径全惰性，fire
  块可选）

## 实体与 Mob

- **掉落物**（`ItemEntity` `[P10]`）：**block item = 0.25 立方体**（atlas 光照
  随昼夜变暗）/ **非 block item = 3D box 模型**（`[CRAFT-2]` 16 件 →
  **`[ENT-9]` 全 79 件**非 block 物品补齐 model——mob 掉落 rotten_flesh/bone/
  arrow/feather/beef/mutton/leather + 所有可 Q 丢弃物皆可见，零核心代码纯
  内容；家族几何 1–3 box 剪影，纹理 = 既有 icon PNG）；统一 Entity 碰撞物理
  （重力/终端速度/水平摩擦）；**零初速生成**（静止位精确、截图可复现——对 MC
  随机初速的刻意偏离）；落地悬浮摆动（1.5 s 周期）+ 自转（4 s 周期）= **帧
  计数器驱动**（非墙钟）；同类 **0.75 格**内自动合并（**合并门 = 同物品且同
  damage** `[SURV-3]`，堆叠上限按物品数据：block 64 / 工具护甲弓 1）；**120 s**
  （MC 同款）或掉出世界消失（age = real-dt 累积）；**拾取** = 玩家底心距 ≤
  **1.0 格**且过 **0.5 s** 延迟（MC 10-tick，防立即回吸）整堆入包（先合并
  同物堆、再找空位，hotbar 优先）——无磁力（MC 会拉，记档）；**流体**
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
- **6 种 mob**（`assets/data/mobs/`，全数据驱动 `MobEntity`/`MobRegistry`；
  数值 = normal 基线）：
  - **zombie**（敌对）：20 HP / speed 0.8 / 探测 16 / 近战 1.6 / 伤害 3 /
    冷却 1.0 / 掉 rotten_flesh ×1 / `undead: true`
  - **skeleton**（敌对，远程）：20 HP / 0.8 / 探测 16 / `shoot_range` 14 /
    `retreat_distance` 3 / `shoot_speed` 15 / `attack_range` 0 = 永不近战 /
    掉 bone **0–2**（随机区间 `[P38]`）+ arrow ×1 / `undead: true` /
    `arrow_effect` = wither 10 s
  - **spider**（敌对）：16 HP / speed 1.0（比 zombie 快）/ 探测 16 / 近战 1.6 /
    伤害 2 / 冷却 1.0 / **无掉落**（string/spider_eye 未做，记档）/ **首个
    `on_hit_effect`** = poison 15 s（MC normal 值）
  - **cow**（被动）：10 HP / 0.5 / 掉 leather ×1 + beef ×1 / **可挤奶**
    （`milk_with` bucket → `milk_gives` milk_bucket）
  - **sheep**（被动）：8 HP / 0.5 / 掉 wool ×1 + mutton ×1
  - **chicken**（被动）：4 HP / 0.6 / 掉 feather ×1 + chicken ×1
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
  1，全 1 与旧 `pick % 长度` 逐位相同；shipped 6 种全 weight 1）；**抽中的类型
  过不了光门则该次尝试作废**（如白天抽中 zombie，v1 接受）
- **光照门（昼夜生成规则，`[P20.5]`→`[LGT-3]`）**：生成格（脚位）的**当前
  有效光照** = max(天空光昼夜有效等级 `effectiveSkyLight(stored, df) = max(0,
  stored − (15−S))`【与渲染同款仿射撤退 + 月光地板：正午恒等，深夜开阔地 15
  → **7 非 0**】，方块光【萤石，不受昼夜影响】)，必须 ≤ 该 mob 的
  `spawn_max_light`（数据字段 0..15；**shipped 敌对 trio zombie/skeleton/
  spider = 7** = MC 月光配对【`[LGT-3]` 起 0 → 7】，被动 = 15 任何光照/时间
  可生成）。效果：zombie 白天开阔地（有效 15 > 7）不生成、深夜生成（有效 7
  ≤ 7）、火把圈（块光 14 > 7）不生成 = 玩法闭环；萤石照亮处深夜也不生成敌对；
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
  **`[ENT-6]` 追逐寻路**（chase 态专属；wander/attack/ranged 转向不寻路 =
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
  `render.mob_flames` 代码默认 true，off = 零发射位级恒等；残余 = 无红闪/无
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
  （`[PRE-1]` 中文化，红字）+ 流式加载暂停（`[P33]`，见「世界与流送」）；
  `R` 重生 = 回出生点 + 满血 + **满饥饿 + 满饱和度**（= 一顿饱饭）+ 速度清零
  （**背包保留** = keep-inventory 记档，无死亡掉落）
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
  `foodSatiety` 纯函数）——shipped 10 食物 **MC 精确值全写**：rotten_flesh
  0.8 / beef 1.8 / mutton 1.2 / chicken 1.2（**生肉低饱和度即玩法点**）/
  cooked_beef 12.8 / cooked_mutton 7.2 / cooked_chicken 7.2 / bread 6.0 /
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
  10 种 = 4 生肉/腐肉（rotten_flesh 4【zombie】/ beef 3【cow】/ mutton 2
  【sheep】/ chicken 2【chicken】）+ 3 熟肉（cooked_beef 8 / cooked_mutton 6 /
  cooked_chicken 6，炉子熔炼对应生肉）+ bread 5（shapeless 3 wheat `[P48]`）+
  apple 4（leaves 1/128 概率掉 `[P49]`）+ golden_apple 4（shaped 镶金
  `[P49]` + grants_effects `[P56]`）；leather/wool/feather 刻意不是食物（MC
  收集品定位）；**食物不直接回血**（HP 只走门控后的自然回复与饥饿 = 0 扣血）
- **护甲 `[SURV-1]`**：**12 件**（leather/iron/diamond × helmet/chestplate/
  leggings/boots，max_stack 1，材质档三链；**MC 精确值** = 1/3/2/1 总 **7** /
  2/6/5/1 总 **14** / 3/8/6/3 总 **20**；chain 无材料 = 记档偏离）+
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
  **无附魔、无修复系统**（矩阵外记档）
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
  ammo 发射器 ∪ ignites_flamable，其它物品带字段 = 解析拒绝点名）+ **28 件
  MC 精确值**（wood 59 / stone 131 / iron 250 镐斧锹三材质；弓族 6 件 384
  【bow/infinite/power/speed/master/poison】；燧石与钢 64；护甲 leather
  55/80/75/65 · iron 165/240/225/195 · diamond 363/528/495/429 = 11:16:15:13
  ——`[SURV-3]` 首批 27 件 + `[SURV-5]` poison_bow）+ **`ItemStack.damage`
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
  破碎音复用 break（玩家侧护甲破碎无声 = Player 无 sound 通道记档）；**无
  修复系统**（铁砧/磨刀石/同物合成/Mending = 矩阵外记档）；合成网格/炉子/营火
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
  "stats (none)" 基线行）；独立统计屏/Game Over 屏 = 范围外记档
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
  mob 侧）③ **熔岩/火块点燃**（`[P57]`/`[P74]`，见「火系统」）④ **饮用
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
  game 消费面**（信标 = 纯名列表 / 药水无酿造 / starter 27 槽满）= 记档偏离
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
- **火系统（`[P57]` 残留燃烧）**：机制见「火系统」节；效果侧 = `fire` 在
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
  附魔的可合成替身【项目无附魔系统】；图标 = 弓 + 蓝色钻石微光）/ `power_bow`
  （`[P79]` **`shoot_damage` 8** = 弓的 5 +3；shapeless bow + 2 diamond；图标
  橙红迸裂）/ `speed_bow`（`[P79]` **`shoot_speed` 20** = 弓的 15 +5【弹道更
  远，E2E 像素判别钉死】；shapeless bow + gold_ingot；图标青色速度线）/
  `master_bow`（`[P82]` 力量+速度+无限**三叠加** = 20/8 + infinite——三把强化
  弓的合成顶点，shapeless power_bow + speed_bow + infinite_bow → 1 = MC
  附魔台合成替身；**无 model** = bow 3 元素几何复用）/ `poison_bow`
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

## 合成与容器

- **配方系统**（`assets/data/recipes/*.json`，**66** = shaped 34 + shapeless
  32，`[P12]`/`[P17]` 起全数据驱动；v1.34.0 起含炉子配方
  `8 cobblestone 围圈 → furnace`）：shapeless = 原料袋（无 `type`，
  `ingredients`）/ shaped = `"type": "shaped"` + `pattern`（行字符串，1..3 行 ×
  1..3 列，空格 = 空位）+ `key`（字母 → 物品，shaped 禁用 ingredients）；经
  ItemRegistry 交叉严格校验（未知原料/产物点名）。**网格匹配 = pattern 偏移
  扫描**（pattern 的每个可能左上偏移都试一遍，空格对应的格必须为空）；**匹配
  顺序 shaped 先、shapeless 后，首个命中即成**（注册表加载序 = 文件名字典
  序）；`canCraft`/`craft` 确定性纯逻辑（原料按槽序升序消耗、副本模拟
  **全有或全无**——失败背包逐位不变，产出永远放得下）；无多配方消歧仍成立
  （shipped shapeless 原料集互不重复——将来重复 = 加载序首个命中，系统不猜）
- **E 背包面板**（`E` 开关；27 格 = 9 hotbar + 18 背包 + **2×2 合成格** =
  共享 3×3 网格模型的左上 4 格（放不进宽/高 3 的 shaped pattern——「需要
  工作台」= de facto 网格尺寸门，与 MC 一致）+ 右侧**配方列表只显示
  shapeless**（产物图标 + 名称 ×N，白 = 可合成/灰 = 不可 + `需要：` 原料
  清单；点击行 = `craft()`）
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
  网格旁 **33 个 shipped shaped 配方两列排布**【单列拟合因子 0.34 不可用】，
  每行 = mini 3×3 pattern（暗格 + 键位物品图标，按定义左上对齐 = 合法摆放）+
  输出图标 + "name xN"（**背包可供应则白/否则暗** = `canSupply(patternBag)`
  纯函数：每覆盖格 1 单位/空格除外/跨字符合并/ItemID 升序）+ **提示行点击 =
  消费性 no-op**（不合成不动背包，吞点击 ≠ 槽外点击丢 carry）；长 name 行内
  溢出 = 确定性 `fitLabel` 尾截（" xN" 恒保）
- **炉子 `[P40]`→`[P44]`→`[CRAFT-6]`→`[CRAFT-5]`**：`interact: "furnace"`
  块（破坏掉自身）；锁定时右键命中 = **打开该炉子的面板**（27 格背包 +
  input/进度条/output + fuel 槽，MC 排布；无任何合成动作——output 自己填满，
  点击取走）；**每炉子块独立 `FurnaceState`**（`std::map<BlockPos,
  FurnaceState>` 位置序 → 逐帧 tick 迭代确定；放置时建空态、块被换/破时删态
  ——`Game::setBlock` 收口；面板打开时绑定所点方块，面板期间块交互暂停 →
  绑定态稳定；各炉独立点火/熔炼互不干扰——A 满 output 不会暂停 B）。
  **可熔炼** = 物品 `smeltable_to`（产物名，init 交叉校验）+ `smelt_time`
  （秒，(0,100]，**缺省 10 = MC 炉子时长**）：3 肉（beef/mutton/chicken →
  cooked）+ **sand → glass `[CRAFT-6]`**（MC 10 s 缺省）。**燃料** = 物品
  `fuel_time`（秒，(0,1000]，显式 0 拒绝）：coal **80 s** / planks **7.5 s** /
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
  硬规则 8）；**无熔炼经验球**（无 XP 系统，记档）；**外观**：四个侧面共用
  同一纹理（BlockModel 只有 top/side/bottom 三面组，记档）
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
  cornflower、青←lapis+绿**（来源块见「世界生成 · 表面装饰/矿脉」）。**可染
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
  进食举到嘴。**shipped = 全 79 件非 block 物品**（`[CRAFT-2]` 16 件【工具 9
  + 弓 5【上臂+下臂+弦，中部开口 C 弧近似，master_bow 无 model 共用】+ 苹果
  2【果体圆近似+柄】，纹理 = 既有图标 PNG，零新纹理免重锚】→ `[ENT-9]` 其余 63
  件全补齐：ingot/dye/wheat/stick/feather/bucket/armor 1 box、bone·arrow 3、
  bottle·potion·fands·rotten 2 家族剪影；共面 Z-fight 首轮套件揪出 4 家族 10
  件修正【相交合法共面非法，mesh 守护 modelCount 16→79 全量生效】）。无
  `model` 物品三上下文**位级不变**（全部新代码以 `it.model.has_value()`
  静态门控）；**HUD 2D 图标零变化**（热栏/面板/手持角标仍是 32×32 icon 或
  block 立方）
- **方块模型 7 形状 + per-block 状态**（机制细节见「操作与交互 · 放置」与
  「物理与碰撞」）：
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
  - **slab `[P85]`**（首个新块模型类型）：底部半块（cell y 0..0.5 实心），复用
    cube 纹理；mesh = 单位立方 6 面 Y 缩 0.5（**顶面恒绘**【cell 顶半格是
    空气，无遮挡】、底/侧面邻 opaque 剔除；贪婪满块路径**跳过 slab**）；
    碰撞 = `solidTopOffset` 0.5（`moveAxis`/`hasRoomAt` 对实心区 [y, y+top]
    判重叠——满块 top = 1 复现旧整格行为 backward-compat）；**v1 只有底部
    slab**（无顶/叠合/斜向）；slab 邻满块同层侧面 Z-fight/缺面 = 贪婪 mesh
    不能半面剔除（v1 局限，KNOWN_ISSUES）；**`[LGT-5]`** 起 `light_opacity`
    0 = 透光（opaque 保持 true 挡光规则演进——光照如空气穿过，剔面/渲染零
    变更）
  - **stairs `[P87]`/`[P89]`/`[P92]`/`[P94]`/`[P97]`→`[ENT-2]`**（第二个新块
    模型类型）：斜面（一端满高、另一端落地）；mesh = 楔形（斜顶恒绘 + 背墙 +
    两侧三角 + 底面，按朝向绕 Y 旋转 corner/normal/cull；侧/背/底邻 opaque
    剔除；贪婪跳过）；碰撞 = 斜面高度感知（`solidTopHeight` 取 AABB 覆盖
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
    World 拒（**零变化零脏**）；② 潜行 + 右键——门 `hasState()`，
    (s+1)%stateCount()（stair = (f+1)%4 同值），"place" 音【`[CRAFT-3]` 材质
    音】、点击消费；③ placeBlock 钩子——stair yaw 覆写（`[P97]` 语义不变）。
    **消费方**：碰撞 `solidTopHeight` + mesh `emitStairs`（形状门 + 哨兵回落
    `stateDefault()`）+ 存档推导（collectSave 两路）——**哨兵回落三处同一
    规则**。**存档 format 15 `states` 段**（替 14 `stairs`）：`{pos, state}`
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
  "speed"]`**（`[P91]` 多效果列表 = MC 主 + 副效果 / 「附魔台」替身；指向
  **多个**效果名，init 交叉校验**每条目**效果存在——grows_to 纪律；
  `beacon_effect` 单值 = 向后兼容 1 元素列表，**两字段互斥**【都写 = 拒绝，
  grants_effect 纪律】）+ `iron_block` 块带 `beacon_base: true`（基座标记；
  **基座 = 仅 iron_block**——MC gold/diamond/netherite 块无 = 记档）。**功率
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
- **物品与背包**（`[P11]`/`[P12]`，`Item`/`ItemStack`/`Inventory`，全数据
  驱动）：物品 = 纯数据（`ItemRegistry`，151 种 = 72 隐式 block item + 79
  显式：`type: "tool"` 带 `tool.break_speed` / `type: "material"` 通用非方块
  物品——锭/木棍/染料/桶/药水/护甲/弓等，作为配方原料/产物和方块掉落）；
  **背包 27 格** = 9 hotbar + 18 背包（`hotbar.json` `items` 定义初始 hotbar
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
  `blocks`/`fluids` 编辑列表，破坏 = `"air"`）+ 玩家全状态 + 昼夜时间 + 27 格
  背包 + 选中格 + 掉落物实体（含 120 s 寿命与拾取延迟计时器）
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
- **format 22**（`kSaveFormat` 22 / `kSaveFormatMin` 1；**1..22 全部可加载**，
  各按字段集 + 逐 format 白名单，规则式迁移——「未知字段 = 报错」
  rejectUnknownFields 保持）：
  - **player**：pos / yaw / pitch / `health` / `absorption`（12+，[0,64]，
    ≤ 效果总和不变量）/ `hunger` / `saturation`（5+）/ `oxygen`（17+，恒写，
    [0,150]）/ **`velocity` 3 数组（22+，恒写，±100 `kSavedVelocityMax` 损坏
    档点名——`[SAVE-2]` 玩家速度入档：写只在退出存档一次性、读只在载档
    `restorePosition` 后 `setVelocity`（≤21 = 零速 legacy 值保真免迁）=
    零运行期成本；onGround 不存 = 位置派生记档）** / `effects`（8+ 名字序，
    `{name, remaining, tier?}` tier omit-when-0（20+））/ `armor`（16+，恒
    写，只含占位槽）/ `stats`（21+，恒写，空对象合法——deathCount +
    kill.<name> 封闭词表）
  - **world**：`blocks`（diff，破坏 = air；**同格不得同时有方块 + 流体编辑**）/
    `fluids`（diff，format 9+ 带种类名 `[x,y,z,level,"water"|"lava"|"honey"]`，
    format 8 的 4 元素编辑加载时全迁移 water）/ `states`（15+，全量含默认值，
    双向交叉
    校验，14→15 值保真）/ `furnaces`（7+，每炉子 1 条目，双向交叉校验，6→7
    sole 归属迁移）/ `fires`（10+，{pos, remaining}，双向交叉校验）/
    `campfires`（13+，每家族块 1 条目恰 4 槽，双向交叉校验 + 每槽进度不变量，
    11/12→13 单槽迁移）/ `tables`（19+，恒写可空数组，双向交叉校验，18→19
    sole 归属迁移）/ `entities`（掉落物，含 120 s 寿命与拾取延迟计时器；
    **箭不入存档** = 保存守卫对已知瞬态种类跳过，其他未知种类仍报错）
  - **校验严格**：format 号 / seed 与生成 seed 一致 / 物品方块名已注册 / 数量
    ≤ max_stack / y ∈ [0,128) / 位置不重复 / **键纪律**（每 format 新 section
    键必需【空数组/对象合法】，低版本携新键 = 版本谎报拒绝——如 22 必 velocity
    / ≤21 携拒；19 必 tables 拒 table_grid / ≤18 必 table_grid 拒 tables）/
    双向交叉校验（container 块 ⟷ 状态条目逐格点名）
- **读档**：正常从 seed 生成世界 → 逐格应用编辑（**先方块后流体**）→ 全量重
  传播光照（`propagateAll`）→ 恢复玩家/背包/实体/时间（`restorePosition` 含
  `fallStartY` 重钉 `[P36]` + `setVelocity` `[SAVE-2]`）→ per-block 状态重建
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

## 世界生成

- **地形**：fBm 2D 噪声高度场（`height_octaves` **4**、`height_noise_scale`
  **0.02**、**base 40 ± 24 格**）+ **`[GEN-8]` 山峰互感 `terrain.passes`**
  （「地形互感」定案 = mountain passes；独立低频 **fbm2**（scale 0.008、3
  octaves、专用 salt kSaltPasses）> threshold 0.55 时 `bonus = floor(
  amplitude × (mask−threshold)/(1−threshold))` 0..amplitude **只升不降**，
  `heightAt = min(base + bonus, 128 − 16)` 纯函数零 chunk 数据 = 跨 chunk
  一致构造性成立；shipped {noise_scale 0.008, threshold 0.55, amplitude 6}
  探针单轮定案【mask 实测 [0.07,0.88]：0.72 首选全世零 bonus 死参数 → 0.55
  = 全世 ~10% 列 bonus ≥2 / max 4 / demo 区 242 列 +1】；**首版局部峰设计
  废弃**（lround 平顶杀严格局部最大，`[PITS 290]`）；缺省段 = disabled 位级
  legacy；逐条点名 enabled/noise_scale (0,0.05]/threshold (0,1)/amplitude
  [0,64]）；y = 0 `bottom_block` bedrock（`[GEN-7]` 多层）+ 深处 `deep_block`
  stone
- **`[GEN-7]` 基岩多层**：`terrain.bedrock_layers`（int [1,8]，缺省 1 = 旧
  行为位级复现，越界/非整数点名）；shipped **3 = MC 经典底**（y0/y2 恒床岩 +
  y1 每格确定性 50% roll `bedrockLayerRoll` 纯谓词，kSaltBedrock）；fillTerrain
  单分支；零 format（世界生成演进）
- **生物群系**：低频噪声（0.02）+ 权重选择（`assets/data/biomes/`：**desert**
  = sand 表层 / **forest** / **plains** = grass 表层），各 biome 有自己的表层/
  次表层/树密度/高度系数
- **水**：低于海平面（**y = 34**）的柱体填充流体水（level 8）到海平面；生成
  即处于平衡态（游戏内 20 Hz 流体 tick 不扰动湖面）
- **洞穴 `[GEN-1]` 面条洞穴 → `[GEN-4]` 破碎度重标定**：单噪声奶酪洞 → **两
  独立 3D value-noise 中位带相交**管状隧道（`|n1−0.5|<T ∧ |n2−0.5|<T`，n2
  仅 n1 入带时求值——MC spaghetti 同款）；**`[GEN-4]`（用户报障「洞穴太多太
  碎」）= `noise_scale` 0.09 → 0.05 单值**（`tube_threshold` 0.06 保持，
  worldgen.json 唯一内容变更，src/ 零变更）——特征周期 1.8× = 管粗洞大、口袋
  并入主管；效果（96×96 四 seed 聚合）：挖除 5.82%→5.12% / 域数 35→8 /
  **pocket 110→10 格 −91%** / 中位洞体积 9-130×；`min_y` 2（底壳保留）/
  **`surface_guard` 0** = 洞口可达表面（shipped = MC 同款）/ **
  `spawn_clear_columns` 2** = 出生/演示区列（原点 ±2 列）整列不挖（结构保护，
  任意 seed 出生点恒安全）/ 每湖 lava 带排除盒不挖（保熔岩湖静态被围）；矿脉
  只替换 stone（洞壁矿暴露 = 自然成立、不填 air）；树/装饰在洞之后跑（
  `[GEN-5]` 守卫）；**旧存档加载得新洞穴**（世界生成演进先例：存档不重生成，
  只重演编辑，已编辑列不受影响）；海床洞穴一次性渗水（生成平衡态后静态，
  记档）
- **`[GEN-5]` 浮空树守卫**（用户报障「洞穴生成后树长半空」）：根因 =
  placeTrees 用 heightAt（与洞无关）选位 + surface_guard 0 洞口可达表面格 =
  悬空树（placeDecorations 有同款守卫，树漏网）；修复 = carveCaves 逐格判定
  提纯函数 **`caveCarvesCell(wx, wy, wz, height)`**（逐位搬移，carveCaves 改
  调 = 行为恒等）+ placeTrees 守卫【表面格被洞吃 = **跳过整棵树**；跨 chunk
  纯函数重算不读 chunk 数据 = 整树同生同灭无半截冠，worker 并行安全】；shipped
  seed 实证 346 树列 14 列（4.05%）被跳
- **`[GEN-6]` 树放置半区修复**（交付上报 bug 立项）：placeTrees/
  placeDecorations 内层 wz 循环上界误用 X 基 limit = **cz ≥ cx+1 对角半区整块
  不种树/不放装饰**（Phase 4 起潜伏、102 道门未发现——基线视图朝 -Z 恒在种
  树侧）；一行级修复（`zlimit = oz + kChunkSize [+ margin]` 2 处；wx/margin/
  格网/roll/GEN-5 守卫零改动）；shipped 实证：门场景盒树列 13→33（2.54×）/
  装饰 5→20、前瞻半区锥内 215 列（游戏内视野）；零 format（世界生成演进）
- **矿脉 `[GEN-3]`**：世界空间格网（`lattice` **8**）+ **MC 3×2×1 旋转椭球
  脉**（`world/ore_vein` 纯函数 `planOreVein`：整数椭球谓词 `4u²+9v²+36w²
  ≤36` = **恰 21 格**，半轴 (3,2,1) 按 6 轴置换定向，专用 salt + PITS #4
  推进流；概率门/格内均匀起点/盐流逐位沿用旧机制）+ `vein_size` = **椭球簇数
  K ∈ [1,8]**（首簇中心 = 胞内均匀起点，后续簇中心在前簇 ±2 内游走 = 21..168
  格/脉；shipped 全 K=1）；**单轮密度标定**（探针 per-ore 新旧格数比 →
  probability re-tune：定稿 **coal 0.154（max_y 80）/ iron 0.0855（56）/
  gold 0.0376（36）/ diamond 0.0101（20）/ lapis 0.0208（32）/ gravel 0.0445
  （80）**；**gravel 同改椭球**（石替换是替换目标属性非形状属性）；矿感 = 脉
  更大更稀（21 格/脉 vs 旧 9 格游走、概率 ~÷2-5 = MC 1.18+ chunky sparse，
  diamond 单次收益大）；残留越带：diamond 0.641× / lapis 0.732×（单轮纪律
  记档）；跨 chunk 无缝（世界坐标格网 + 世界坐标起点）；只替换 deep stone
  （不破坏洞穴 = 洞壁矿暴露契约不变）；脉的 block 名生成 setup 对 BlockRegistry
  解析，未知 = 启动报错；**旧存档未编辑 deep 石按新脉形生成**（worldgen 演进
  先例，记档）
- **树木**：世界空间格网（`lattice` **4**）候选 + 数据驱动密度；**树干 4~7
  格**（min 4 + range 3）+ 两层半径 2 树冠 + 顶帽半径 1；跨 chunk 边界由邻接
  chunk 各自补齐（margin 格网机制），**只写入 air**（不破坏地形）；**野外单
  橡种**（spruce/birch 只经 `[GEN-2]` 种植落地 = 树种差异是内容维度非生成
  维度；多树种野外 = 未来项需重锚）
- **表面装饰 `[P45]`**（biome 数据驱动）：biome JSON 可选 `decorations:
  [{block, probability}]`（严格校验：未知键拒绝、probability ∈ [0,1]、block
  名生成 setup 对 BlockRegistry 解析，未知 = 启动报错）；放置走**树的同一格
  网**（`trees.lattice`，专用 salt kSaltDecoration，每 (格, 条目) 对一次独立
  hash3 roll——装饰 roll 与树 roll 不相关）；落在表面格**正上方一格**，前置
  条件：高于海平面 + 表面格仍是该 biome 的 surface 块（**洞穴啃掉的表面不
  放**，防悬空 = 既有检查直接生效）；在 placeTrees **之后**跑（树干/树冠占格
  时 placeIfAir 让位，装饰丢弃）。当前内容：**沙漠 cactus（0.03）**、**草原
  3 花（allium/tulip/cornflower 各 0.02）**、**森林 3 花（各 0.012）**
- **熔岩湖 `[P86]` 单湖 → `[P88]` 列表 → `[P90]` 表面湖 → `[P93]` 程序化**
  （`worldgen.json` 数据驱动）：`lava_lakes` = **湖列表**，每个湖 = 固定位置
  (center_x, center_z) 的满熔岩池——盆地 (2R+1)²（radius R），**盆底 = 区域
  最低地形 − depth**（纯 `heightAt`，确定性、跨 chunk 一致），盆底以上地形
  carve air、最低 `fill` 层注满熔岩（level 8）；`placeLavaLakes` 循环所有
  enabled 湖（各自独立 region/floor，重叠湖 carve 同格 = 确定性）。shipped：
  - **深湖（depth > fill）**：(40,40) R4 D4 F2 + (55,45) R3 D3 F2——熔岩埋于
    盆底、四周地形 ≥ 区域最低 → **被围、静态、不流动不蔓延不燃烧**（熔岩顶
    低于 rim，与 rim 顶不 6-邻）
  - **表面湖（depth == fill）**：(24,26) R1 D2 F2——熔岩顶 = 区域最低高度（与
    草 rim 顶**同高**）→ 运行时 **`[P58]` 燃烧 pass** 把 rim 点燃 = 草→火→
    燃尽**焦土环**；同时地表熔岩**沿下坡溢出盆地**（FluidSim 流动，流缘再
    点燃相邻可燃物）= MC 熔岩湖「流动 + 燃烧周围」落地。**偏离**：表面湖非
    静态（溢出 + 燃烧改世界，确定性但非静止）、溢出形态取决于地形坡度、深湖
    无此行为（埋没被围）
  - **程序化分布 `[P93]`**（`lava_lakes_procedural` 段）：`count` **5** 个湖在
    区域 **[60,90]²** 内**确定性随机**分布（`computeProceduralLakes` 构造时从
    seed 派生：splitmix64 + kSaltLavaLake + 金步推进 = PITS #4 纪律；每湖
    radius 1-3 / depth 2-4 / fill 1-2 各取范围内随机值 = MC 熔岩湖散布世界）；
    跨 run/chunk 一致（纯 seed 函数）
  - 全部湖放出生点默认 -Z 视野外（+Z 后方）→ 无输入基线门不受影响（免重锚）；
    每湖 `enabled: false` 可关；熔岩会烧（`[P58]` 接触伤害/点燃）
- **出生点**（硬规则 10）：4×4 chunk 演示区列内按 `kSpawnCandidates` 10 候选
  自动找干燥陆地（h > seaLevel 且出生 AABB 内无 solid = 避开树干/树冠/流体），
  见「物理与碰撞 · 出生点」

## 渲染与表现

- **管线与 pass**：OpenGL 3.3 core；每列 chunk mesh **三桶**（`[PRE-5]`：
  opaque / cutout / blend——mesher 第三桶 + MaskCell cutout 字段，
  **cutout/blend 面永不并 quad**，每列 mesh 2→3）；**贪婪剔面 `[P18]`**（逐面
  片合并为同（纹理、sub-mesh、面光）run 的单 quad；200-chunk 实测三角形
  109,648 → 33,558 = 3.3×；**合并不跨 chunk 边界**（标准做法，MC 同款逐
  chunk）；**流体表面仍逐面片**（level 缩放几何，刻意不合并）；**两个相邻非
  opaque 方块之间仍双向出面**（每平面两独立 slice 分别合并，叶片-叶片/玻璃-
  玻璃双面保留，正确略浪费）；只渲染暴露给 air/非 opaque 邻居的面（跨 chunk
  边界经 `World` 查询剔除）；**`[PRE-5]` 透明 discard**（根因 = 全项目零
  depthMask → blend pass 的 alpha-0 像素（leaves/cross/glass 框）写深度 = 透明
  孔后几何被不可见深度遮挡）：**块 property `cutout` 数据驱动路由**（严格 bool
  点名 + cutout ⇒ transparent 配对拒 + shape 门控仅 full-cube/cross【其余 shape
  = 死字段 init 点名拒 = ammo 纪律】；shipped **20 块** = 3 叶 + 3 花 + 7 麦 +
  3 苗 + fire/torch + 2 营火【纹理 16×16 alpha 普查为据】；**glass/
  glass_pane/water/lava 保持 blend 桶** = 半透/流体不写深度 MC 纪律）+ **frag
  唯一新行 `if (u_discardEnabled != 0 && alpha < 0.5) discard`**（桶无关：
  glass 框/leaves 孔/fire 缝全修，水 0.78/glass 芯 0.86/云 0.55/光束 0.5 全不
  discard；雾/波纹/gl_Position/v_viewDepth 零触碰）+ **渲染序 cutout 段
  （discard + 深度写 GL 默认）先于 blend 段（`setDepthMask(false)` 新 API——
  云/水/玻璃/粒子透明段/信标/overlay 不写深度）**+ `render.alpha_test`
  getBool 代码默认 true（config.json 零改动），**off = v0.28.0 位级恒等**
  （discard 关 + depthMask 恒默认；含半透混合场景的 off 分段序偏离 = 记档）；
  MeshVertex 尾字段追加纪律（water 标记 location 7 → cutout location 8，默认
  0 既有生产方零改动）
- **`[UXFIX-1]` 用户实机两渲染 bug 修复**（10-26 报障，`[PRE-4]` 云 per-draw
  状态泄漏家族 v0.28.0 起潜伏）：① **白盒**（热栏/面板块立方图标 = 半透明白
  盒）= drawClouds 遗留 `u_useTexture=0` + `u_color` 白 + `u_alpha` 0.55，
  立方（唯一不设自身该三 uniform 的 basic draw）继承；② **黑块**（挖掘块面
  纯黑）= 云/光束尾部 disableBlending 泄漏进 drawWorldOverlay，裂纹立方
  (0,0,0,0) 透明底不透明写入——locked + 无输入门全盲（113 次免疫从未见过）；
  修复 = 云/光束删自持 blend（混合段所有权上移）/ overlay 自持混合 / 立方显式
  bind + u_useTexture=1 + u_color(1,1,1) + u_alpha=1.0（受害者侧防御）；无输入
  位级恒等
- **脏标记增量重网格**：`setBlock` 标记本 chunk（及共享面的邻居 chunk）dirty，
  下一帧只重建 dirty chunk 的 GPU mesh（mesher 计算走 worker 快照 = 「世界与
  流送 · `[PIPE-1]`」）
- **雾 `[PRE-2]`**：流送盘缘原始切面（「渲染条」）距离雾溶解——basic shader
  雾 4 uniform（`u_fogEnabled` per-draw 分支无分歧 / `u_fogStart` / `u_fogEnd`
  / **`u_fogColor` = 同帧 `m_dayCycle.skyColor()`** 昼夜跟随）+ **雾深度独立
  表达式**（`v_viewDepth = −(u_view×u_model×pos).z`，**gl_Position 表达式树一
  字未动** = 位级红线：重排浮点乘序改 ULP 可翻转共面/深度竞争像素）+ 纯函数
  `renderer/fog`：**end = min(kFar 1000, 16×min(view_distance, kStreamCapRadius
  64))**、**start = `kFogStartScale` 0.75×end**（shipped vd80 = 750/1000 块）；
  `render.fog` getBool 代码默认 true（config.json 零改动）+ **off = v0.25.0
  全管线位级恒等**（结构免疫）；**偏离**：太阳/月亮/星星（sky program
  kSkyDistance 800）不雾化 = **天空即雾色本体 MC 同款**；仅线性距离雾（无高度
  雾/per-material 雾/fogStart 比例非 config）
- **水面波纹 `[PRE-3]`**：静态水面 → 帧锁定 shimmer——水纹理 = 纯白噪声
  （per-pixel hash，GL_NEAREST）故**顶面 UV 采样窗逐帧平移即闪烁**（幅度
  **0.06 tile** ≈ 1 texel / 波数 **0.8 rad/block** / 周期 **120 帧** =
  `m_frameCount % 120` 纯函数，硬规则 8，命名常量零 config）；**水顶面走既有
  tiled 采样路径编码**（emitFluid：uv = tile 原点 + uvLocal = 角内坐标 + tiled
  = 1 + **location 7 water 标记**；frag tiled 分支 `(v_uv + fract(v_uvLocal +
  v_rippleOff)) × u_tileSize` = fract 自动 tile 内 wrap 零 tile 识别；vert 算
  v_rippleOff = (sin 0.8x+φ, cos 0.8z−φ)×0.06 仅水顶面非零，(0,0) 时
  fract(x+0) = x 位级恒等）；`render.water_ripples` getBool 代码默认 true +
  off = v0.26.0 位级恒等；**偏离**：lava 不闪 / 侧底静 / 非顶点波；
  water.png 周期化重跑字节零变化（16 格网内恒等）
- **云 `[PRE-4]`**：basic program **纯色半透明平板层**（y = `kCloudHeight`
  **108** / **16 块格** / **50% 密度** splitmix64 顶位 / 漂移 t = frame/
  `kCloudFramesPerCell` **6000** = **100 s/格** MC 节奏 / alpha **0.55**，全
  命名常量零 config；u_useTexture=0 + u_alpha + culling off + depth on = 信标
  光束 `[P83]` 同款范式，世界坐标恒等 model 单 draw；**缓存 mesh 仅相机云格/
  t 变化时重建** = 无输入恒静态 ×2 RAW 位级、走路每 16 块一次 = 99.x 纪律；
  `render.clouds` getBool 代码默认 true，off = 零 mesh 零 draw 位级恒等）；
  **偏离**：云 = 纯色平板无云团形状 / 云窗口 **±384 块**窗外 pop-in / 云不随
  昼夜变色（恒白）
- **掉落光照 `[PRE-6]`**：块立方/model 物品掉落顶点 skyLight = 1.0 硬编码恒白
  （洞穴石头立方亮如白昼）→ **vert 端 per-draw uniform 覆盖**（basic.vert
  唯一改动：u_useLightOverride 3 uniform + 1 行门控分支；**frag 零改动** =
  昼夜缩放/环境底/雾/波纹全自动继承）+ 纯函数 `entity/item_light`
  itemDropLight（AABB 中心格 → getSkyLight/getBlockLight ÷ 15；每帧采样无
  缓存 = 光照变化自然跟随）+ `render.drop_light` getBool 代码默认 true，
  **off = 3 uniform 从不 set = v0.29.0 位级恒等**【几何文件零改动 = 不重烘】；
  **参与遮挡半 = `[PRE-5]` 结构性闭合**（opaque/cutout 掉落写深度、blend 不写
  = MC 纪律）；**箭头/手持恒白** = 记档偏离（矩阵外）
- **毒纱 `[PRE-7]`**（全屏效果 tint 通道数据驱动）：效果 JSON 可选 `tint`
  [r,g,b,a] + **no-op 规则演进**（正 alpha tint = 表现层动作，纯装饰纱合法；
  alpha 0 仍 no-op 拒）+ 内容 `poison.json` [0.63, 0.24, 0.86, 0.35]（其余 10
  效果无 tint）+ 渲染 = HUD pass 红纱前每活跃玩家 tinted 效果一个全屏 quad
  （`activeEffectTints` 纯函数收集）；详见「状态效果」
- **mob 火焰 `[PRE-8]`**：`Game::updateMobFlames` 每 **36 帧**（
  `kMobFlameEveryNFrames`，`[CRAFT-5]` 周期）每燃烧 mob 一批上升余烬（
  `[P66]` `spawnFire` 复用，专用盐）+ **自发光**（block 光 1.0 深夜也亮 = MC
  观感 + 中心格天光环境项）+ `render.mob_flames` 代码默认 true，off = 零发射
  位级恒等；详见「实体与 Mob」
- **箭三段 `[PRE-9]`**（MC 式箭形 = 棕杆 0.31 + 灰箭头 0.12 + 白尾羽 0.09，
  总长 0.55 原轮廓不变）：详见「实体与 Mob · 箭」
- **昼夜天空**（`[P8]`/`[LGT-3]`）：游戏内时间按每帧**固定 1/60 s** 推进
  （不用墙钟 = 无输入逐字节可复现；非 60Hz 显示器昼夜真实速度按比例偏差，
  记档）；`world.day_length_seconds` 代码默认 240 s 一天（**已提交 config =
  600 s 用户调值**），`world.start_time_of_day` 代码默认 0.25 = 正午（已提交
  同），`--time <0..1>` 可覆盖起点；**dayFactor**：黎明 **[0.05, 0.15)** /
  黄昏 **[0.45, 0.55)** smoothstep 渐变，白天 = 1 恒等、深夜 = 0；天空背景色
  按时间关键帧插值（t = 0.00 夜深蓝 (0.02,0.03,0.08) → 0.10 黎明橙
  (0.85,0.45,0.25) → 0.15 昼蓝 (0.47,0.71,0.95) → 0.45 昼蓝 → 0.50 黄昏橙
  (0.85,0.40,0.20) → 0.60 夜深蓝 → 1.00 夜深蓝）；太阳/月亮 = 像素级圆形
  billboard（**40/28 px**，越靠近地平线越淡；太阳轨迹：t = 0 正东地平线 →
  正午天顶 → t = 0.5 正西地平线 → 午夜天底，月亮恒在太阳对面；无纹理无月相）；
  夜空 **300 颗确定性星星**（世界空间 r = 800 上半球，splitmix64 哈希生成，
  2 px，以世界原点为心 = 走路极小视差；无闪烁无银河）；**天空不雾化**
  （`[PRE-2]` 偏离：天空即雾色本体）；**光照呈现** = `[LGT-3]` 仿射撤退
  （见「光照」）
- **流体覆盖层 `[99.5]`**：相机格含流体时**全屏半透明染色**（水蓝
  (0.20, 0.42, 0.85, 0.45) / 熔岩橙 (0.95, 0.42, 0.05, 0.60)，HUD 之下、世界
  之上，MC 同款 fluid overlay）；缓解「完全浸没时同种流体面互剔 = 看到空气」
  （`[P9]` 既有行为）；无波纹/无深度渐变/无气泡（v1 记档）
- **HUD**（`[P15]` 起，**只在指针锁定 + 背包未开时渲染** = 无输入运行零新增
  绘制 → 基线逐字节不变）：**MC 三行 HUD 堆栈 `[PRE-10]`**（行距 `kIconPx` +
  `kStripGap` **22**，每行独占 = 任意图标数零碰撞——消费「效果图标行与吸收红
  心可视觉重叠」Phase-56 条）：**心行 hy**（10 颗心 16 px 2 px 间隔，三态
  贴图满/半/空取自 health.png 48×16 条带，填充 = `clamp(hp − 2i, 0, 2)` 半心
  0.5 HP 粒度；**吸收红心** `[P56]`/`[P72]` = 正常心带左侧 `ceil(池/2)` 颗，
  池 0 零绘制）| **效果行 hy−22**（`[P46]`，左锚 = 心行最左缘 = 池在时吸收带
  左缘，MC 布局）| **护甲行 hy−44**（`[SURV-1]`，仅占位槽且指针捕获时绘制）；
  **心行右侧同行**（带间 6 px）= 饥饿 10 鸡腿（hunger.png 48×16）或**氧气泡
  排替换原位**（`[SURV-2]`：oxygen < 150 时 10 泡 = ceil(oxygen/15)，单 tile
  无半态，出水同帧消失）；**窄窗回退 `[PRE-11]`**：纯函数 `core/ui_
  layout.heartRowLayout`——整行超出右缘时把行左移直至**右排（更紧急读数）右缘
  贴屏**，吸收带（行最左）最先被裁（记档 last resort），低于最小行宽心也裁
  （记档极限：无布局能救 sub-row 窗）；**宽窗返回 legacy 坐标位级精确**（无
  输入基线 + 所有 E2E 窗零触碰 = 结构免疫根）；右排宽度按本帧实渲染排预留
  （氧气 < 150 替换饥饿 = `[SURV-2]` 原位语义 10 图标）；UI 度量常量
  kUiIconPx 16/kUiIconGap 2/kUiStripGap 6 收口 ui_layout.hpp 单一真源；**耐久
  条 `[SURV-3]`**：图标底部 2 px 横条（满耐久零绘制）；**hotbar**：底部 9 格
  （选中格橙色边框，堆栈数 < max 时显示数量）+ **手持物品**右下角大图标
  （76 px）；**物品图标**：block item = 带光照的倾斜 3D 立方体（复用掉落物
  cube mesh + 基本着色器，白天亮夜晚暗）/ 工具材料 = 32×32 图标纹理（
  `Item.icon` 数据字段 → `assets/textures/items/<icon>.png`，
  `tools/generate_item_icons.py` 程序化生成，`[P35]` 起 32×32）；**准星**（
  锁定屏幕中央白色十字）+ **目标方块描边**（24 顶点线框立方体，描出 raycast
  命中格）；**HUD 红心检查**（E2E 协议哨兵：心带 y[646,661] x[551,729] 无
  输入运行 = 0，>50 = 污染废拍）
- **`[PRE-12]` tooltip**：悬停格（炉子面板槽 / E 面板槽与格）显示 `name`
  （count 1）/ `name xN`（count > 1，配方行 " xN" 同款）+ 工具第二行
  `damage/max`（F3 held 行同款 d/max 读法，**满耐久 0/max 文本显示**【区别
  于 HUD 条满耐久零绘制】）——纯函数 `ui/item_tooltip.itemTooltip(item,
  stack)`（campfire 范式）；**渲染层修复 = 门内发现 Phase-15 潜伏缺陷**（旧
  文本与面板共享 kText batch【batch 首用于面板标题 = 绘制先于槽底 kSlotBg/
  边框/图标】+ 偏移 L(12) 随面板 fit scale 收缩【679×744 窗 s ≈ 0.286 文本
  4.6 px 紧贴光标全落槽内】双杀 = **悬停 tooltip Phase-15 起恒被覆盖不可
  见**）→ **tooltip 独立 batch**（title kText / 耐久行 kTextDim）**移至 2D
  batch 与 3D 块立方图标之后绘制**（UI 最顶层、`[PRE-7]` 纱前；无悬停 = 零
  batch = 无输入基线不变）；**记档偏离**：无描述/附魔文本（项目无附魔系统、
  item JSON 无 description = MC 同款无描述）+ 效果图标 hover tooltip 结构不可
  行（见「状态效果 · HUD」）
- **F3 调试 overlay**（`[P15]`，任意指针状态可切换，**英文保持** `[PRE-1]`
  记档）：15 行 = `TokenMine.cpp v1.33.0`（banner）/ `%d FPS | frame %d`
  （FPS = 真实 dt 平滑取整）/ `chunks %d | draws %u | verts %llu` / `pos %.2f,
  %.2f, %.2f yaw %.2f pitch %.2f` / `col (x, z) seed N` / `time %.3f day %.2f
  locked|free` / `streaming engaged|idle pending N radius N` / `inventory
  open|closed particles N entities N` / `health X / X absorption X armor N`
  （`[P72]`）/ `oxygen N`（`[SURV-2]`）/ `hunger X / X` / `held name d/max`
  （`[SURV-3]`，无 = "(none)"）/ `difficulty %s`（`[P25]`）/ `effects name
  Ns 罗马 | ...`（无 = "(none)"；`[PRE-12]` 档位罗马）/ `stats deathCount N |
  kill.<name> N | ...`（无 = "(none)"；`[PRE-12]` stats 行）
- **死亡屏**（`[P16]`/`[PRE-1]`）：「**你死了！** / **按 R 重生**」中文化（红
  字——`[P36]` 文字着色修复：ui.frag textured 分支 = `texel.rgb × u_color.rgb`
  【此前 u_color 只调制 alpha，死亡屏红字被静默忽略渲成白字】，图标/心带/白字
  u_color.rgb 均为白 = 逐位不变）
- **中文 UI `[PRE-1]`**：**UTF-8 解码渲染**（纯函数 `utf8NextCodepoint` 严格
  1..4 字节——拒 overlong/surrogate/>U+10FFFF/5-6 字节/孤立续字节/截断，非法
  字节 nullopt + pos+1 = 每坏字节一个 `?` 旧逐字节回退语义保持）+ **CJK glyph
  静态表 33**（19 UI 字【你死了按重生熔炉工作台背包合成需要关闭——PRE-1 表仅
  18 漏 `作`，`[CRAFT-4]` 补】+ 10 中文标点（，。！？：；“”（））+ 4 余量
  【物品方块】；离线 Noto Sans CJK SC Regular em 16 提取 rows 5..20 窗口阈值
  ≥128，.py 静态表范式 = ASCII 同款，TTF 仅工具期）+ `glyphUv(uint32_t)` 泛化
  （ASCII 32..126 原 cell 公式零改动 / CJK lower_bound 二分 / 未知 `?` 旧格）+
  textWidth/makeTextVertices 按 codepoint 迭代（CJK 16 px 等宽 advance，ASCII
  宽度逐位不变）；**图集 = 既有死格复用**（ASCII 控制字符 cell rows 0..3 恒
  透明从未被采样，32 格恰装 32 字形；`作` 取 ASCII DEL 死格 127——
  font.png **256×512 尺寸零变化** + 既有 32 格逐位复现自校验 + 全图仅 cell 127
  一格变 56 ink px）；**8 处 UI 字符串中文化**（死亡屏 You died! → 你死了！/
  Press R to respawn → 按 R 重生 + 面板 Furnace → 熔炉 / Crafting Table →
  工作台 / Inventory → 背包 / E to close ×2 → 按 E 关闭 / Crafting → 合成 /
  needs: → 需要：）；**F3 调试屏保持英文**（开发者工具记档偏离）+ 悬停/配方
  行物品名 = JSON 数据驱动英文 id 不译（翻译 = 内容变更矩阵外）
- **UI 坐标空间与缩放**（`[P34]`/`[P35]`）：全部 UI（hotbar/心·饥饿带/面板/
  F3/死亡屏/手持/准星）布局在「UI 像素」空间 = 窗口**逻辑**尺寸 ÷ `ui.scale`
  （config [1,4]，代码默认 1.0，**已提交 config = 1.5 用户调值**），正交投影
  拉伸到可绘制区——GPU 放大（NEAREST 纹理保持锐利），SDL 逻辑鼠标坐标 ÷
  ui.scale 做命中测试；**高分 UI 素材 `[P35]`**：字体 16×16 字形（32 px cell
  8 px 偏移）、平面物品图标 32×32、文字缩放 double scale（分数缩放支持）；
  **面板自适应 `[P35]`**：面板 native 设计，UI 空间更小时整个面板（槽/间距/
  内边距/标题/配方行 + 按同一系数绘制的字体 scale 与图标尺寸）等比缩小（长度
  全程 double 运算、仅 rect 边界 lround），恒贴合 UI 空间（每边 ≥1 px 余量）；
  **世界渲染不受 ui.scale 影响**（始终全可绘制区）
- **纹理 atlas**：16×16 tile × **16×8** 网格（128 tile，`assets/data/
  textures/block_atlas.json` **布局数据驱动**——新增纹理 = 加 PNG + 加一行
  JSON，不改代码；atlas 扩容（4×8 → 8×8 `[P41]` → 16×8 `[P59]`）会移位全部
  UV = 基线重锚，空位追加免重锚 = 纪律）
- **半透明水体渲染**：`ChunkMesher::emitFluid` 输出 level/8 缩放的立方体
  （流动水顶面半高），水-水内部面剔除（相邻水位相同/更高不出面），流体面
  烘焙**自身格**的光；shader 用纹理 alpha（水 α ≈ 0.78 / glass 芯 α ≈ 0.86 /
  leaves ≈ 0.92 + 透明孔），湖面可透视湖底；**波纹** = `[PRE-3]`（见上）；
  叶子挨叶子内部面仍会生成（剔除规则 v1 只判 opaque，结果正确但多发明，
  KNOWN_ISSUES 渲染节）
- **窗口标题**实时显示 FPS 与 draw call 数（`[99.6]`）

## 音频

- **SoundBank**（`[P15]`）：SDL3 `SDL_OpenAudioDeviceStream` 队列式播放（无
  回调）；初始化失败**优雅降级为静音 + WARN**（headless 音频降级 play 静默但
  日志照常发——环境音 E2E 证据走日志）；音量 `audio.volume`（代码默认 0.7，
  已提交 config 同）；**`[CRAFT-3]` mono → stereo**（44100 Hz 不变；旧
  `play(name)` = mono 复制双声道 = 居中听感位级等价）
- **39 个确定性 WAV**（`tools/generate_sounds.py` 固定种子 LCG/sine，**既有
  wav 字节零变化 + 重跑幂等**纪律逐版 md5 实证）：17 基础事件（dig/place/
  break/pickup/step/ui_open/ui_close/craft/eat/hurt/death/shoot/bow/splash/
  milk/cloth【`[P41]` 染色布料声】+ ignite【`[P60]`】）+ **18 材质音
  `[CRAFT-3]`**（6 材质 × 3 事件：stone 硬脆/raw noise/2200 Hz ping、wood 中
  频腔音、gravel 双 noise 层干颗粒、sand double lowpass 低 shush、cloth 最静
  rustle、glass 1900-3200 Hz 铃音）+ **3 环境音 `[PRE-4]`**（cave 低频嗡鸣 +
  4 水滴 / day 鸟鸣 + 双风床 / night 蟋蟀 + 弱风，amb_env 慢淡入淡出无咔哒，
  音量 = SFX 的 1/4~1/2 环境底噪定位）+ 1 **smelt** ding `[CRAFT-5]`（1320/
  660 Hz 双泛音钟 + LCG tick 种子 0x51DE）
- **事件集**（数据驱动）：破坏（按住时每 `kDigSoundInterval` **0.3 s** 一个 dig
  tick + 完成时 break）、放置、拾取（含 Q 丢弃）、脚步（移动 + 贴地 + 锁定 +
  背包关闭时每 `kStepSoundInterval` **0.4 s**）、背包开/关、合成成功、受伤/
  死亡（`[P16]`）、射箭（`[P21]`）、拉弓弦音（`[P24]`，拉弓开始时响）、
  进食（`[P23]`）、染色布料声（`[P41]`）、牛奶咕嘟声（`[P43]`，饮用开始 +
  挤奶共用，取消也响过 = MC 一致）、点火（`[P60]`）、smelt 完成 + 复燃
  ignite（`[CRAFT-5]`）、破碎（`[SURV-3]` 复用 break，玩家侧护甲破碎无声
  记档）
- **空间化 `[CRAFT-3]`**：块 JSON 可选 `properties.sound_material`（严格
  字符串 + 显式空串拒 = ammo 纪律）+ init 交叉校验（`block_sound.{hpp,cpp}`
  纯函数 `checkSoundMaterials`：引用材质 ⇒ `<m>_dig/_place/_break.wav` 3 件
  **文件级**存在【headless 音频降级时 init 照样严格】，点名 = registry 序首
  败块 + 事件序首缺件）+ **纯函数空间化**（`distanceGain` 线性衰减全 3D 距离
  cutoff = `kSoundCutoffBlocks` **16.0** 命名常量零 config / `panGains` 恒定
  增益线性声像 (1±t)/2 高度不参与前后居中 / `toStereo` 16-bit 钳制）+
  `play(name, std::optional<Vec3> origin)` 新 API + `setListener(相机)` 帧始
  同步【1 帧陈旧零成本】+ **6 调用点位世界空间化**（dig tick/破坏完成/放置/
  潜行旋转/丢弃 @脚位/物品拾取 @物品位，事件名 = `blockSoundName` 纯函数：无
  字段 = 通用名位级不变）；**内容 = 6 材质 × 3 事件 = 18 新 wav + 32 块赋值**
  （stone 8【含矿脉族 = MC 语义】/ wood 3 / gravel 1 / sand 3【sandstone 系 =
  MC】/ cloth 15【wool 全染变】/ glass 2；未赋值块 = 通用回退，obsidian/
  furnace/铁块/楼梯族等 = 下轮候选记档）；手持物品破碎/箭拾取/营火放取/
  step/shoot/eat/hurt/ui_* 保持 2D 位级不变
- **昼夜环境音 `[PRE-4]`**：3 上下文纯函数 `ambienceContext`（**cave = 玩家
  脚格 raw skyLight 0 优先**【MC 无天空规则，洞口边缘抖动记档】/ day =
  dayFactor ≥ 0.5 / night）+ 每 **300 帧**（`kAmbienceIntervalFrames` = 5 s
  at 60 fps）`ambienceRoll(seed, frame, ctx)` 纯函数 < **0.5** → 2D play
  （**全局不空间化**）+ `Ambience: <name>` info 日志（帧锁定事件序列 = E2E
  证据）；`audio.ambience` getBool 代码默认 true；**偏离**：MC 不规则多音池
  = 3 上下文单音 + 5 s 帧锁定 roll 简化；kAmbienceSalt 选定使 shipped seed S2
  事件帧全 pass（E2E 可观测性设计选择，1000-roll 无偏证）
- **硬规则 8：音频不入帧状态**——截图/存档零影响；事件检测纯函数（tick 无
  副作用，`[CRAFT-5]` furnaceTickEvents 范式）；实际听感 = 用户实机（无头不
  可听记档）

## 难度

- **4 值词汇表**（`[P25]` → `[ENT-8]`）：config `mobs.difficulty` = `easy` /
  `normal` / `hard` / `peaceful`（代码默认 `normal`，严格校验，未知值 init
  报错；**只 init 读取**——无运行期切换）；mob 数据是 **normal 基线**，其他
  难度按 `core/difficulty.hpp` 纯函数缩放（游戏与单测共享，F3 显示当前
  难度）：
  - ① **出生点保护环**——敌对 mob（`hostile: true` = zombie/skeleton/spider）
    不在**世界出生点**（新世界的出生点，读档不改变它）`spawnProtectionRadius`
    块内自然生成（**水平圆、不跟随玩家**——无垂直分量，v1 无床/村庄，MC 村庄
    保护环未实现记档；MC 值：easy **8** / normal **12** / hard **16**）；
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
    roll（300 帧）/ 云漂移（帧函数）/ 水波纹（帧 mod 120）
  - **real-dt（真实墙钟，生存计时器类）**：饥饿衰减/星饥伤害/自然回复 / 进食
    饮用通道 / 破坏进度 / 弓蓄力 / 坠落伤害结算（fallStartY 记录）/ 熔岩灼烧 /
    火块燃尽（m_fires remaining）/ 效果 remaining 与 tick 累加 / 实体 age /
    物品拾取延迟 / 自动存档计时器 / 双击窗（0.3 s）/ 粒子运动
  - **音频不入帧状态**（事件驱动，截图/存档零影响）
- **无输入运行逐字节可复现**：截图只在**帧边界**字节一致；**148 次无输入基线
  门**（新 ×2 RAW 字节一致 + vs `/tmp/opencode/base18.png` 字节一致，或论证
  「无输入下零活动」= **结构性免疫**免重锚【变更在无输入路径永不执行——如
  炉子永不生成 = m_furnaces 空双结构惰性 / 无掉落 = 零 item draw / 无效果 =
  count 0 fast-exit / 音频 = 无事件】，或**重锚记档**【渲染基线本身变更——
  LGT-2 门 83 / PRE-4 门 111 云入基线 / PRE-5 门 112 cutout 入基线】；base18
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
- **质量门**：**1912/1912 单元测试**（`./build/bin/tokenmine_tests`）+
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
