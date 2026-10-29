# 已知问题清单（Known Issues）

每个阶段完成后把新发现的限制/简化记到这里；修复后移入 `docs/archive/phase-NN.md`
（本文件末尾有已修复存档索引）。
条目格式：**问题** — 影响与现状。`[阶段]` 标注发现/引入阶段，`计划` 标注预计修复阶段。
排序约定：**章节按主题（子系统）组织**——查"某系统有什么问题"跳到对应 `##` 节；章节顺序 = 主题首次立项的顺序（非 phase 排序，`[Phase]` 标签只是引入/发现阶段的标记，不是排序键）；章节内条目 = 发现/追加时间序（append-only），修复历史写进条目正文（`[99.x] 已修复` 式标注），不按修复阶段重排。

## 渲染

- **叶子挨叶子的内部面仍会生成** — 剔除规则 v1 只判断"邻居是否 opaque"，结果正确但多发了面。水-水的内部面已在 Phase 9 由流体网格化剔除（相邻水位相同/更高的水面不出面）；叶子-叶子待细化为"邻居是否为同种方块"。`[Phase 3]`（水部分 `[Phase 9]` 已解决）
- **玻璃/叶子不衰减光** — 光的遮挡对非水方块仍是二值的（opaque=15，其余=0）：Minecraft 中玻璃/叶子有衰减（叶子有 fancy 选项）。水的每格衰减（天空光 -3）已在 Phase 9 实现；玻璃/叶子要支持需给 `BlockProperties` 加 0-15 的 opacity 字段并改传播算法（衰减系数）。`[Phase 7]`（水部分 `[Phase 9]` 已解决）
- **水面是 level/8 缩放的立方体顶面，无波纹动画** — 流动水（level 1-7）的顶面高度按等级线性缩放，静态观感；Minecraft 有水面波纹动画。`[Phase 9]`
- **完全浸没时水体几何自剔除、"看到空气"** — 同种流体面互剔（level 相等/更高的邻居不出面）：深水中一格满水有 6 个同级邻居 = 零面，相机在水内时周边水体几何全部不可见（99.x 用户报告"走进深水后周边像空气一样透明"）。`[Phase 9]` 观感由 **`[99.5]` 流体覆盖层**缓解：相机格含流体时全屏半透明染色（水蓝 (0.20,0.42,0.85,0.45) / 熔岩橙 (0.95,0.42,0.05,0.60)，HUD 之下、世界之上，MC 同款 fluid overlay）；无波纹/无深度渐变/无气泡（v1 注记）。
- **按面采样光照，无逐顶点插值/环境光遮蔽** — 每个面采样面后方一格的光值，4 个顶点同值（块状光照感，Minecraft 经典观感）；无 AO 阴影。`[Phase 7]`
- **昼夜是呈现层缩放，不是夜间重算光照** — 天空光恒按正午存储（0-15），渲染时 sky × dayFactor（夜晚=0）。Minecraft 会在夜间真正重传播光照（例如日落时光线从洞口退出的动态变化）；本实现夜晚的天空光区域是"均匀变暗"，无动态传播变化。方块光（glowstone）不受昼夜影响（与 MC 一致）。`[Phase 8]`（**`[LGT-3]` 已解决**：缩放 → **天空光场对源等级仿射（无损）重演**——源等级 S = 7 + 8×dayFactor（正午 15 / 深夜 = 月光 7）的收敛场 = `max(0, stored − (15−S))` 逐格精确（所有天空光源共享同一等级 + 逐格成本与 S 无关；confluence 单测 = 直接改源等级重传播 vs 仿射变换逐格相等钉死），shader 天空项与生成光门（`currentSpawnLight`）求值同一公式 = **零重传播**的 MC 同款日落撤退/黎明回涌（亮区沿 `stored = 15−S` 等值线向洞口收缩）。**记档**：分数 Δ 平滑撤退（MC 量化超集）。**月光地板 = 10-27 用户实机定案落实**（深夜全黑无法夜视不可玩 → 推翻立项时"矩阵外"记档）：`kNightSkyLevel` 7（MC 午夜露天月光），源等级 S = 7 + 8×dayFactor，敌对 trio spawn_max_light 0→7 配对【深夜露天 7 可生成保夜险 / 火把圈 14 免疫 / 白天露天 15 不生成 = 玩法闭环不变】）
- **昼夜时间按每帧固定 1/60s 推进（非墙钟）** — 保证无输入运行（截图验证）逐字节可复现；代价是非 60Hz 显示器上昼夜的真实时间速度按比例偏差（120Hz = 2× 速）。`[Phase 8]`
- **太阳/月亮/星星是 billboard 圆形 sprite** — 太阳 40px / 月亮 28px / 星星 2px 的像素级圆形 quad（着色器 view-space 膨胀；不用点精灵——Mesa ANV 等驱动会把 shader `gl_PointSize` 钳到 1）。无纹理、无月相、无星星闪烁；星星固定在以世界原点为心的 r=800 上半球（走路时有极小视差），无银河。`[Phase 8]`
- **无云、无昼夜音效** — 云层与昼夜环境音未实现。`[Phase 8]`
- **世界边缘即"虚空"（无雾）** — 未加载区域读作 air，没有雾效/边缘遮挡，站在流式加载的边缘能看到悬空截面（Phase 14 起"边缘"随玩家移动，不再是固定的演示区边界）。`[Phase 3]`（流式部分 `[Phase 14]` 已缓解）
- **顶面/底面纹理朝向约定** — Phase 3 修正 Y 面绕序后，顶面纹理的 u 轴指向 +Z、v 轴指向 -X（与 Minecraft"顶面纹顶部朝向 -Z"不同）。占位纹理下无视觉影响；正式纹理阶段可统一调整。`[Phase 3]`
- **alpha cutout 无 discard（透明纹素仍写深度）** — 混合 pass 用 src-alpha 混合、无 `discard`：alpha=0 的纹素视觉透明（贡献 0），但深度照常写入——花/十字植物面片"透明孔"后面的几何会被孔遮挡（孔内看到深度而非透过去）。leaves（~12% 透明孔）/glass 既有同类行为；Phase 45 的十字植物花（allium/tulip/cornflower）同类。修复 = shader 加 `if (alpha < 0.5) discard;`（需先确认无其他依赖深度写入的混合几何）。`[Phase 3]`（十字植物部分 `[Phase 45]`）
- **特定自然地形场景配置下 GL 列 mesh 更新滞后（远景顶面不渲染，候选）** — 10-28 GEN-9 门 123 发现：自然地形场景（相机 8.5 块侧视 demo 列 + 走廊挖通）中，湿耕地顶面恒不渲染——CPU mesh 日志证 quad 已发射（tile 正确）、世界 dump 证视线盒全空、受控平台场景同二进制全正常、**old v1.7.0 二进制帧字节一致（非 GEN-9 引入）**。= 该场景配置的 GL 列 mesh 集成时机 quirk（demo 列 + 场景编辑 + 流送交互）；实机玩家视角 = 近景站立/行走（受控场景覆盖）不受影响。复现场景与取证链见 `docs/archive/phase-gen9.md` E2E 节 + PITS 292。`[GEN-9 门 123 候选]`

## 世界 / 生成

- **洞穴是"奶酪洞"** — 单个 3D 噪声阈值雕刻（`caves.threshold`），生成的是团块状空腔，不是 Minecraft 式的 spaghetti 隧道（两条 3D 噪声相交）。形状可用配置调，但拓扑类型要等以后升级。`[Phase 4]`
- ~~**矿脉是简化随机游走**~~ — `[GEN-3]` 已解决：**MC 3×2×1 旋转椭球脉**（`world/ore_vein` `planOreVein` 纯函数：整数谓词 `4u²+9v²+36w²≤36` = 21 格/球，6 轴置换取向，`vein_size` 语义演进为**椭球簇数 K ∈ [1,8]**；shipped 全 K=1 + 单轮密度标定）。格网/概率门/起点/盐流/跨 chunk 无缝/deep-only 契约逐位保留。
- **旧存档未编辑的 deep 石按新脉形生成** — 存档只存块编辑（diff vs 生成）：读档后**未被玩家编辑**的 deep 石列仍由世界生成器按当前（GEN-3 椭球）脉形生成——旧存档里的矿脉位置/形状 ≠ 当时的随机游走（与地形同源的 worldgen 演进，`[GEN-1]` 面条洞穴同款先例：存档不重生成，只重演编辑）。已编辑列不受影响（编辑权威）。`[GEN-3]`
- **矿感 = 脉更大更稀（MC 1.18+ chunky sparse）** — 单脉 21 格（vs 旧 9 格游走）+ 概率降档（标定后 ~÷2-5：coal 0.4→0.154、iron 0.25→0.0855、gold 0.12→0.0376、diamond 0.05→0.0101、lapis 0.08→0.0208、gravel 0.25→0.0445）：挖矿"走更远才见脉、见脉收益更大"（diamond 单次可达 21 格 vs 旧 5 格）。**标定残留（单轮纪律记档）**：探针新/旧格数比 diamond 0.641×、lapis 0.732×（低于 [0.8,1.25] 带——低概率下格网通过集换血 + 截断脉占比变化的非线性，单轮后接受；软带测试按钉死值防漂移）。`[GEN-3]`
- **K>1 多簇机制已实现 + 单测，shipped 全 K=1** — `vein_size` = 椭球簇数（[1,8]，`kMaxOreVeinClusters`）：后续簇中心在前簇 ±2 内游走（21..168 格/脉）。shipped 6 条全 K=1（Q2 拍板：单 re-tune 轴；21 格脉已比旧 9 格游走 chunky 得多）——未来给 coal/iron 调大簇 = **零核心改动的内容调档**（改 worldgen.json + 重标定）。`[GEN-3]`
- **树是固定模板** — 树干 1×N（4~7）+ 两层半径 2 树冠 + 顶部半径 1 帽，无生长阶段、无 BlockState、无树种差异（不同树只有高度随机）。`[Phase 4]`
- ~~**基岩只有单层**~~ — **`[GEN-7]` 已解决（v1.6.0：`terrain.bedrock_layers` 3 层 MC 经典底：y0/y2 恒床岩 + y1 每格确定性 50% roll，`bedrockLayerRoll` 纯谓词；世界生成演进零 format）**。`[Phase 4]`
- **地形高度与洞穴/矿/树相互独立** — 洞穴只保留表层 4 格（`surface_guard`）与底层 2 格（`min_y`）的壳，不感知矿脉/树洞；可能出现洞口正对矿脉等巧合。`[Phase 4]`
- **放置到未生成区域会同步生成 chunk** — 演示区边缘之外放置方块仍触发 `getOrCreateChunk`（生成器在游戏线程同步跑）。Phase 14 实现了流式加载，但放置需要**立即**拿到目标 chunk（放置当帧生效），所以该路径刻意保持同步（一列 8 chunk ≈1.6 ms 的生成尖峰）；流式管线随后按正常路径为它补光照。`[Phase 6]`
- ~~**水平速度无惯性**~~ — `[ENT-7]` 已解决：地面/空中指数趋近模型（起步 ramp + 松键滑行，MC 0.5448/tick 地面摩擦档 / 0.91/tick 空中拖曳档，fps 无关 exp(−k·dt)，stop-eps 恰好 0 = 静止态确定性）；FLUID-2 档（水/水面/熔岩）保持瞬置 = 流体模型独立调参（记档偏离）；mob 侧 AI 每帧设精确速度 = 项目设计（范围外，详见 phase-ent7.md）。`[Phase 5]`
- **虚空坠落 = 死亡（非传送）** — 掉到 y < -32 直接死亡（`damage(maxHealth×1000)`），走死亡/重生流程（Phase 16，`R` 回出生点满血，背包保留）。Phase 14 起世界随玩家流式扩展，走路时地面始终在前方加载（预算 2 列/帧 ≫ 步行速度），虚空坠落大幅减少——但仍可能发生（读档点位于未加载区域时先下落几帧再由流式补上地面；快于流式速度离开时）。`[Phase 5]`（虚空范围 `[Phase 14]` 缓解；`[Phase 16]` 死亡机制已实现）
- **材料链单向** — `sand → sandstone → smooth_sandstone`（Phase 80/81，
  3 沙合成砂岩、2 砂岩合成光滑砂岩）与 `stone → cobblestone`（Phase 61，
  挖掘掉落）都**不可逆**（无锻回配方/熔炼——P61 既定范式的同款偏离；MC
  的砂岩→沙、光滑砂岩→砂岩亦无，一致）。`[Phase 80]` → `[Phase 81]`
- ~~**水量不守恒**~~ — `[Phase 9]` 问题，**`[FLUID-1]` 已修复**（路线 A，细节见 `archive/phase-fluid1.md`）：修复前流体体积有三条不记账路径——浅膜落入更深流动池被"吸收"（池保持原水位，**合法具名事件 α**）；下落链落入世界底部直接消失（**合法具名事件 δ**）；**落入池时按等级抬升池水位**（无源增益 = 唯一不守恒的体积增加路径，已删除）。修复 = 删抬升一分支的**统一池规则**三分支：下方满格（level 8）→ 本格拉满到 8（**κ**，source 托底补满）；下方流动池 0<B<8 且 L<B → 本格拉入池（**α**，池水位 B 不动）；L≥B → **静态沉积**（零写入：本格保留、池水位不动 = 池水位逐格单调不增）。守恒目标 = **逐 tick 会计恒等式**而非"总量 ±0"（后者是错误目标——MC 流前缘创生 level-L-1 邻居合法，记为具名事件 **σ**；下落传送带为成对移动净零）：封闭区域流动格数 F 与源数 S 满足 F' = F + σ_c − α − κ − δ、S' = S + κ，单测逐 tick 钉死（封闭盒 64 格精确终态表 + P90 湖精确终态计数）。**残余** = 站流水瀑布/崖膜的永久轮换（下条，接受现状）。`[Phase 9]`
- **站流水瀑布/崖膜永久轮换（接受现状）** — FLUID-1 路线 A 下，海崖/盘边供给的薄膜级联以**站流水瀑布**形态运行：有界的边缘格集合每 tick 重新 churn（薄膜落下 → 沉积成新薄膜 → 再落下，period-2）。定量（路线 A step-0 probe）：流送盘 r5 场景 = **24 格**（与旧规则逐格同一集合、无新增振荡格）、稳态 **24 编辑/tick**、总编辑 ~18.6k/600 tick（旧规则 ~18.6k，同量级）；P90 表面湖溢出 = **1 churn 格**、稳态 ~3.4 编辑/tick（**`[GEN-4]` 后 = 3 churn 格**：洞穴重排新开排水沟把溢流路由出 disk-3 普查区，overflow 157→72 / total 473→707，湖契约不变——世界生成演进同款先例）**接受现状 = 非缺陷**：24 格类**视图外**（无输入基线 gate 原样通过）、**×2 RAW 字节确定**、99.8 活动门只把对应 chunk 保热（流体成本永久项有界）。pre-FLUID-1 的池抬升分支本可打断该循环——但那是唯一不守恒的体积增益路径，删抬升保稳压器 = 路线 A 定案。**`[99.11]` 成本修正**：该接受的 churn 在 shipped 海洋（seed 123456789）海岸线尺度 = 713→918→1357 格/tick（非 24 格量级），其 re-light 原为主线程同步巨盒重扫 = 每 300 帧窗口 130s→396s→661s = 用户深层卡顿根因；99.11 后光的传导走 worker 追赶流水线（≤2 列/帧、幂等收敛、滞后有界、film 光值近乎不变 = 视觉零差），churn 的主线程成本降为 set 插入（~10 ms/tick 量级、实测 update p95 7.6 s→80 ms）——churn 本体仍是接受现状（FluidSim 逐位未动，门 91 状态 vs v0.8.0 逐位相同钉死）。若未来做"斜向 film 静态梯度沉降"（MC 流时间链）= 流体模型语义变更，单独立项。`[FLUID-1]`
- **满水格（source）失去支撑会下落** — 对 Minecraft 的刻意偏离：MC 中 source 水不随支撑消失而下落（只会停止扩散）；本实现中任何满格下方为空都会沿列下落（传送带式补位），行为更"物理"但与 MC 不符。`[Phase 9]`
- **流体不会流进未加载的 chunk** — `World::setFluid` 只写已存在 chunk（不创建）；FluidSim 只 tick 已存在 chunk。Phase 14 起"未加载"只存在于流式半径环之外/玩家身后：水在加载区边界暂停，玩家走近加载后恢复扩散（生成地形本身的水是平衡态，tick 无变化）。`[Phase 9]`（范围 `[Phase 14]` 已收窄）
- **流体模拟 = chunk 粒度活动门控（非 MC 的 cell 队列）** — `[99.8]` 起每 tick 只走"热"chunk（上一 tick 有流体变化 / 被块变更扰动 / 新生成），沉降流体（静态池、**静态流动梯度**）零成本——实测 P90 熔岩 228 格峡谷流沉降后流体成本 40 ms/tick → ~2 ms/tick。**`[PIPE-3]` 已 per-cell 化（v1.2.0）**：tick 成本 = O(活动 chunk + 活
流体格)——per-chunk per-kind 流体格列表（`Chunk::setFluidAt` 成员变化时
push、drain lazy stale、walk 过滤/列序/去重 + >50% stale 或重复即压缩，
不变式 = 活格 ⇒ ≥1 条目）+ World per-kind **活动 chunk 集合**（6 插入点
穷尽全部 flag 置位点：setFluid 双 kind / setBlock 排水 / setBlock UNSTICK /
wakeFluidChunks / scanFluidChunks / eraseChunk purge，walk 点消费，flag
双检 = stale belt）——原每 visited chunk 两遍 32K 全扫 + 32KB falling
数组堆分配、每 tick 全流体 chunk 集合的 flag 遍历全部消除；falling 标志 =
per-column colFlag[256]（列内自底向上 = 收集序）+ 跨 chunk 边界
topFlags[256B]（引用语义位级同旧）；reaction/burn 同款活格扫；propose
O(n) 线性合并 → map max 合并（序无关 = 位级同果）。流体状态逐位不变
（门 117 S2 = fall/fill 跨侧 savenorm EXACT + 截屏 RAW 字节同 + cascade
nospread 位稳 oracle 存档 RAW 逐字节同 + 同 OLD 二进制自分歧实证 fire
区 1 格 = 既有 real-dt 类 PITS 283 非回归）；性能 = fall 流体段 −50%
（1261→631 ms/300f 窗）、fill churn 窗 tick 1.5-2.3→0.3-0.4 ms（−80%）、
沉降下界 0.19-0.30→0.10-0.16 ms（−50%，全集合遍历消除）。块变更（挖掉
静置水片下方的支撑）会强制受影响 chunk 走一次（walk-pending，单测钉死）。**流送边界活性补洞 `[STRM-2]`**：新 chunk 出现（`insertChunk`/`getOrCreateChunk`）同款强制——自身 + 6 面邻中持有该 kind 流体的 chunk 各走一次（`wakeFluidChunks`，setBlock UNSTICK 镜像）：pre-fix 一个 settled 冷流前对 void 的 spread 被 `setFluid` 拒（"the void stays dry"）后 chunk 转冷，目标列流送到达**不唤醒**它 = 级联永久卡在流送边界（门 116 场景插桩实证：前沿 tick 4 被拒转冷，目标列 frame ~15 已集成，其后 395 tick 零推进；CellReader 缺失 chunk 读 air = 表面条件全满足而写入静默失败，排障须探针 setFluid 拒绝分支 PITS 271）；反向同病 = 带生成海的新 chunk 插入后自身冷、不与邻接 settled 流体互扰。唤醒仅在插入事件（streaming 节奏）= 99.8 性能面零回退（`SettledWorldStaysColdWithoutInsertions` 钉死）；性能 A/B 的 fluidCells 窗差 +1/+2 = 该修复的行为语义（静态梯度提前一格越界，非 churn 放大）。
- **只有一种流体（水）** — 无岩浆/蜂蜜等；`fluidBlock()` 取第一个 `properties.fluid` 数据方块作纹理参考，加第二种流体需扩展存储（level 数组按流体类型分）。`[Phase 9]`
- ~~**草方块掉小麦种子（不再掉泥土）**~~ — `[Phase 52]` 已修复：grass `drops` 改回 `dirt`（MC 语义）+ `chance_drops: [{wheat_1, 1, 0.125}]`（偶尔种子，0.125 = MC 基础概率）。种子来源 = 草概率掉 + 未熟作物（`wheat_1..6`）掉落。`[Phase 48]`（概率系统 `[Phase 49]`、多掉落列表 `[Phase 52]` 已就位）
- ~~**作物可种在任何朝向的表面**~~ — `[Phase 53]` 已修复：新增 block 可选字段 `plant_on`（方块名数组）——放置时目标格**正下方**须为列表之一，不符 = 静默 no-op（不消耗物品）；纯函数 `canPlantAt`（canPlaceAt 同款可单测）+ init 每条目存在性校验（grows_on 纪律）。首个使用者 = `wheat_1..6` 的 `["farmland"]`（Minecraft 耕地约束落地）。`plant_on`（放置门）与 `grows_on`（生长门）相互独立。`[Phase 48]`（生长条件 `[Phase 50]`、放置限制 `[Phase 53]` 均已解决）
- **耕地无湿润视觉状态、不会变回泥土** — 湿润是生长引擎**逐作物计算**的环境量（土壤格 4 格内同 y 有无水），不是方块状态：同一片耕地浇不浇水长得一样（视觉无变化，Minecraft 的湿润耕地变深色）；干耕地也不会随时间退化回泥土（MC 有 revert）。无 per-instance 状态（Phase 2 遗留同一性质）。`[Phase 50]`
- **湿润判定是简化几何** — "土壤格（作物下方）同 y、切比雪夫半径 4 内有任一**水**（level ≥1；`[Phase 54]` 起只算水，熔岩不湿润）"：不区分源/流动水（流动水膜也算湿润）、不检查水是否可达（4 格外隔墙的水也算）、半径/层级规则与 Minecraft 的"4 格内水源"不完全一致。`[Phase 50]`
- **未熟作物掉种子（非 MC 的"不掉"）** — 破坏 `wheat_1..wheat_6` 掉 1 个种子（可再种，玩家友好）；Minecraft 破坏未熟作物不掉任何东西。`[Phase 48]`
- **`wheat_2..wheat_7` 有隐式 block 物品但不可获得** — 每个数据方块自动有同名 block 物品（可放置）；`wheat_2..wheat_7` 的物品没有掉落/合成来源，实际拿不到（`wheat_1` 是种子来源）。无害，但物品表里有 7 个小麦 block 物品。`[Phase 48]`
- ~~**卸载/未加载 chunk 的作物不持久**~~ — `[99.2]` 已解决：**含作物列永不卸载**。作物放置/生长/被烧/破坏全走 `setBlock` 收口 → 所在列入 `m_editedColumns`（**insert-only 无修剪**，"player-edited columns are never dropped"）且未熟作物另在 `m_crops`——`unloadFarColumns` 对编辑列 + 五类 per-block 状态列（炉子/营火/**作物**/信标/火）双重豁免（见「Chunk 流式」99.2 条）。作物 = 方块编辑随存档（无独立 crops 键），读档重放编辑 + `rescanCrops` 全量重建；生长每帧 tick 与玩家距离无关。唯一相关残余 = 仅流体模拟改动（无 tracked state）的远端列卸载后回生成态（99.2 残余①），**不适用于作物**（作物格恒为编辑）。`[Phase 48]`（`[99.2]` 已解决）
- ~~**概率次级掉落每方块至多一个条目**~~ — `[Phase 52]` 已修复：新增 `chance_drops`（`{item, count, chance}` 严格三元组数组，与单条目 `chance_drop` **互斥**【同时写拒绝】）。各条目**独立 roll**（`rollChanceDropEntry`，条目索引去相关 salt），breakBlock 循环条目、每条目至多 1 掉落实体；init 每条目复用 drops 纪律（物品存在 + max_stack）。单条目 `chance_drop` 保留（系统 + 单测覆盖；`[Phase 55]` leaves 迁入列表后一度**无现役内容使用者**，`[Phase 59]` gravel→flint（10%，MC 值）重新使用它）。`[Phase 49]`（多掉落列表 `[Phase 52]` 已就位；内容扩展 `[Phase 55]`、`[Phase 59]`）
- **概率掉落是 per-cell 确定性（无重 roll）** — 同一世界同一格每次破坏结果相同（splitmix64(seed + 位置哈希)）：破坏-重种同一格树叶，苹果结果不变；这是无输入字节可复现的刻意代价（MC 是真随机）。位置哈希 + 专用 salt 保证相邻格去相关（单测有实证率断言）。`[Phase 49]`
- ~~**flint 无用途（v1 文档化偏离）**~~ — `[Phase 60]` 已解决：**箭头配方 = shapeless flint+stick+feather → 4 箭（MC 1.14+）**，**替换**了旧的 2 木棍竖排 → 2 箭配方（内容契约变更：早期箭现需 flint + feather = MC 同款闭环，gravel→flint、chicken→feather、树→stick）+ **flint_and_steel**（flint+iron_ingot 合成；`ignites_flamable` = 右键烧 `flamable` 块、无掉落、不消耗）。flint 的两个用途 = MC 的主用途。`[Phase 59]` → `[Phase 60]`
- **燧石与钢 v1 范围（文档化偏离）** — 点燃器**只作用于块**（`flamable` 集 = wheat_1..7 + wood/leaves/grass，`[Phase 63]` 扩到 MC 天然可燃）：Mob 点燃仍只来自熔岩（`[Phase 58]`），玩家/Mob 不可被点燃；不引燃流体。**火视觉**（烧 = 生成 `fire` 块）`[Phase 64]` 已解决。`[Phase 60]` → `[Phase 63]` → `[Phase 64]`
- **火 v2 范围（文档化偏离）** — `fire` 块（`[Phase 64]`）+ 粒子/蔓延（`[Phase 66]`）的边界：**余烬粒子 = v1 视觉**（2 粒/0.1 s/火格、0.12 立方体、寿命 0.3–0.5 s——MC 的火粒子更密且分火焰/烟两层，本项目单材质上升火星；`fire.particles` 可关）；**蔓延引燃无声**（蔓延走 `setBlock(fire)` 但不播 `ignite` 音效——一片蔓延火会刷屏一次性音效；点燃器单次引燃仍有声）；**蔓延无难度/风/湿度调制**（chance 固定 config 值，无 MC 的 difficulty 缩放/风场/湿度）；fire 块**可放置**（block item 自动存在：放一个 = 临时营火，燃尽回 air、无掉落）；光的**强度恒定 15 无闪烁**（纹理静态、light 不随时间变）；**玩家/Mob 火接触均已落地**：~~**Mob 站火里不受伤害**~~（`[Phase 73]` 解决：火块点燃 Mob = P58 熔岩点燃的块版）；玩家接触 = `[Phase 70]` 直伤版 → **`[Phase 74]` 改点燃**：玩家 AABB 重叠 fire 块格 = fire 效果点燃（`updateEffects` 门 `inLava || isInFire`，与熔岩/与 Mob 同一路径——1 HP/0.2 s、离开火源残留 8 s、水瞬时熄灭）；**`player.fire_damage` / `player.fire_damage_interval_seconds` 退役**（旧 config 仍启动 = 未知键容忍，单测钉死）。`[Phase 64]` → `[Phase 66]` → `[Phase 70]` → `[Phase 73]` → `[Phase 74]`
- **火把 v1 范围（文档化偏离）** — `torch` 块（`[Phase 65]`）的边界：**无点燃/蔓延**（火把是光源不是火源——不引燃相邻 flamable，火源语义仍归 `[Phase 64]` fire 块与点燃器/熔岩 pass）；**无支撑要求**（通用 placeBlock 规则：任意实心面可贴，含方块底面——MC 火把需要支撑，本项目无支撑语义）；光**恒定 14 无闪烁**（静态纹理）；hardness 0 **瞬破且掉自己**（与 fire 相反：fire 无掉落）；配方 shapeless coal+stick → 4（MC，coal 首个合成用途）。~~**无 campfire**~~（`[Phase 71]` 已解决：campfire 块 = 单槽烤食物 + 座位，见下条）。`[Phase 65]` → `[Phase 71]`
- **营火 v2 范围（文档化偏离）** — `campfire` 块（`[Phase 71]` 单槽 + 座位；`[Phase 75]` 四槽 + 水浇灭）的边界：**无燃料**（营火自燃 = 与 MC 一致，非偏离）；**速度 = 炉子节奏**（复用 `smelt_time`，MC 同款）；**只烤可熔炼物品**（`smeltable_to` 非空，项目 = 生牛肉/羊肉/鸡肉——MC 营火也只烤食物）；**烤制中无噼啪音效**（放置/取回复用 place/pickup）；**Mob 不可坐**（只有玩家）；**坐姿 = crouch 高度**（无独立坐姿动画/相机高度）；**营火不是 fire 块**（无剩余寿命条目、不蔓延/不被蔓延、**站里面不烧**——光 15 外观 ≠ 火源语义，火源仍归 fire 块）；**右键链中营火分支先于进食通道**（MC 语义，烤槽可被食物填满——项目全部可熔炼物品都是食物；炉子/工作台保留既有"手持食物先开吃"行为 = 既有偏离不变）；`[Phase 75]` 新增边界：**四槽无面板**（放 = 第一个空槽、取 = 第一个非空槽，索引序确定性——MC 营火可开面板逐槽操作）；~~**只渲染第一个非空槽的图标**~~（`[Phase 78]` 已解决：**4 槽图标全渲染**——每槽 0.35 图标、0.3 间距 1×4 横排悬浮格上半部，P71 的 0.5 单图标改为缩小排布）；**未点燃的不能放食物**（没火烤不了 = MC 语义）；~~**水浇灭后无再点燃**~~（`[Phase 76]` 已解决：燧石与钢右键 `campfire_extinguished` = **原位再点燃**——新块属性 `ignites_to`【严格字符串、空串拒、init 存在性交叉校验】，目标块 = `campfire`；物品不消耗、"ignite" 音、烤制状态保留续烤；MC 同款）；~~**无水浇灭**~~（`[Phase 75]` 已解决：6-邻水 → 瞬时变 `campfire_extinguished`【光 0、状态保留】，P64 fire 块同款规则）；~~**单槽**~~（`[Phase 75]` 已解决：4 个独立烤位，各自计时，format 13 四槽形状 + 单槽迁移）。`[Phase 71]` → `[Phase 75]` → `[Phase 76]`
- **stone→cobblestone 材料链（Phase 61 内容契约变更 + 熔炼偏离）** — 挖天然 stone 现掉 **cobblestone**（MC 材料循环）：stone 只能经 **4 cobblestone → 1 stone 合成**（MC 1.14+）回来。文档化偏离：**cobblestone 不能熔炼回 stone**——`smeltable_to` 是物品字段而 block item 无独立 JSON（隐式生成、无 item 数据），给 block item 加数据 = 核心改动，超出本 phase 最小范围；合成路线已覆盖转换。`[Phase 61]`
- **斧 v1 范围（文档化偏离）** — 三把斧（`wooden/stone/iron_axe`，`[Phase 67]`）的边界：`fast_against` 只覆盖 **wood/leaves ×2**（MC 斧对木 2×；对其它块 = 基础手速 1× = MC 斧对石 1×）；**无"肉/战斗"倍率**（对 mob 就是平值 attack_damage 3/3.5/4 = 各比同档镐 +1 的项目压缩刻度，MC 斧对生物无特殊倍率）；~~无耐久~~（当时项目无耐久系统——**`[SURV-3]` 已解决：三斧各得 wood 59 / stone 131 / iron 250 = MC 精确值**）。引入斧同时消费了 `[Phase 62]` 的"无斧类工具"注记（新工具型 = 数据驱动 `fast_against` 列表 + `effectiveBreakSpeed` 纯函数，不加新 ItemType）。`[Phase 62]` → `[Phase 67]`

## 实体（Phase 10）

- ~~**Mob 无行走摆腿/手臂动画**~~ — `[ENT-1]` 已解决：swing 相位 = per-mob 整数（移动帧 +2、静止帧 −4 衰减到 0、帧边界水平位移严格不等式判定、纯渲染层不入存档）+ constexpr 16.16 定点正弦表（编译期 Taylor、零运行期 libm）+ mob 模型 `swings` 数据段（box `name` + `part`/`amplitude` (0,1] 弧度/`group` 0/1 半周相位组，严格解析 + part 存在性交叉校验）+ CPU 逐帧顶点变换（pivot = box 顶面中心、绕局部 X 轴、法线同旋、phase 0 位级恒等 = 静态 fast path 结构性免疫）+ per-mob 动态 VBO（`Mesh::uploadDynamic`/`updateVertices`）+ config `mobs.limb_swing`（默认 true、false = 静态基线）。内容：chicken 2 腿 0.5 rad 半周相位、cow/sheep 4 腿 0.7 rad 对角步态、spider 8 腿 0.9 rad 偶奇分组。**残余（文档化偏离）**：zombie/skeleton 无独立 arm part（99.7 几何 = 头 + 躯干两 box，`[ENT-1]` 不造几何）；翅不摆（MC 只飞行时扇翅）；swing 相位 session-local（读档/生成 = 0，mob 本 ephemeral 不入档）。`[Phase 10]`（腿 `[99.7]` 已补；动画 `[ENT-1]` 已解决）

- **掉落物不受本地光照、也不遮挡光** — 掉落物顶点烘焙满天空光（sky=1.0、block=0），shader 随世界同款昼夜（LGT-3 仿射撤退 + 月光地板；sky=1.0 ⇒ 有效 = 7/15 + (8/15)×dayFactor——深夜月光 7/15 非全黑，特性本体），不采样所在位置的方块光：洞穴/树荫下的掉落物和空地上一样亮（夜间同步变暗到月光）；反过来掉落物也不参与光传播/网格化（不挡光）。需要按物品位置读世界光数组、并让掉落物参与遮挡。`[Phase 10]`
- **掉落物动画由帧计数器驱动（非墙钟）** — 上下浮动（1.5s 周期）/自转（4s 周期）取自 `m_frameCount`，保证无输入/自动化运行逐字节可复现；代价是非 60Hz 显示器上动画速度随帧率变化（与昼夜时间同一权衡）。`[Phase 10]`
- **透明掉落物不做深度排序** — 掉落物按其方块属于哪个 pass（opaque/transparent）与 chunk 同 pass 绘制，但"掉落物之间 + 掉落物与 chunk 之间"无全局排序，复杂透明场景可能出现混合顺序伪影（当前透明方块只有水/玻璃/叶子，演示区规模下不易察觉）。`[Phase 10]`
- ~~**实体与流体无交互**~~ — `[ENT-4]` 已解决（mob/掉落物）：**共享流体浸入模型** = 玩家 FLUID-2 的无输入孪生（`Entity::refreshFluidState` + `Entity::integrateFluid`，同常量 12/1.5/4/32 同名同公式，`isRestingOnSolid` 从 Player 上提共享）：浸没体被动上浮（vy≥0 门 = 沉水体继续下沉的 MC "dive" 语义）、水面 W' 几何漂浮（FLUID-3：W' = 真水面上扫【浸没体窗内最高 L8+1】+ 浮漂终态 = 游泳位 W'−h/2 半浸没【原脚面=水面干身终态 = 用户报障"站在水上"，已修】+ fps 无关不动点）、池底站立（fresh resting 门；FLUID-3 深度门：池深 ≥2 的 resting 体上浮出底，1 格浅池站底不变）；mob 水平速度门 ×0.6（命名常量、无 per-mob 数据字段 = 文档化 YAGNI 偏离）+ step-up 水下解禁（FLUID-3：ENT-4 的 `!m_inFluid` 门删除 = 玩家对齐，涉水 mob 可踏 1 格岸壁出池；游泳位 mob onGround=false 仍无 step-up = 无 swim-up 爬岸记档偏离）；下沉/浮力 inFluid 门控（FLUID-3：水面以上 0.4 干带 keep vy = 全速过带，原无门 = 悬崖坠水"降落伞"已修）；掉落物浮面静止（摩擦零残差 = 无漂移，MC 简化记档）；**熔岩对称同路**（玩家熔岩是弱浮力节奏，mob/物品两类流体同一水模型 = 文档化偏离；P58 燃烧系统零改动 = MobFireTest 熔岩池坐底同速率钉死）。**残余**：投射物/箭仍穿过水（任务条 = mob/掉落物，文档化边界）。`[Phase 10]`（mob/掉落物 `[ENT-4]` 已解决，投射物保留）
- **实体间碰撞是每帧分离 pass，不是多体约束求解器** — Phase 19 把 Phase 16 的玩家↔掉落物分离推广为通用实体 pass：固定顺序 body 列表（玩家先、再实体列表按索引；中心精确重合时：可推动体被推向 −axis 侧、两个可推动体则列表中靠后者走 − 侧——正好丢在玩家脚底的物品去玩家 −x 侧，保持 P16 方向），重叠对沿最小穿透轴分离——两个可推动体 50/50、对不可推动体走全量面到面跨度（玩家不可推动，Phase 16 规则）；体只移动到其 AABB 可占据的位置（`hasRoomAt`，与碰撞解析同一严格格扫描），被 solid 挡住的一侧份额由另一侧全量接管、两侧都挡则跳过该对；重复 sweep 直到某 sweep 的最大位移 <1e-6 块（亚视觉残差）或 32 次硬上限。参与/可推动均为数据位（`collidesWithEntities`/`pushableByEntities`，非按类型硬编码）：投射物不参与（箭穿过实体）、玩家不可推动。剩余简化：(a) 成对分离，病态簇可留 ≤~1e-6 块的亚视觉残留穿透；(b) 只沿最小穿透轴推、不尝试次轴——该轴两侧都被墙挡死时该对保持原样（无切向滑动）；(c) 玩家↔掉落物分离从"无条件推"变为"有世界感知"：唯一可推方向被 solid 挡死时物品保持重叠而不再被推进墙里（P16 旧行为是嵌入墙内）。`[Phase 10]`（玩家↔掉落物 `[Phase 16]`、掉落物↔掉落物与通用 pass `[Phase 19]` 已解决）
- **掉落物零初速生成（刻意权衡）** — Minecraft 的掉落物带随机初速散开；本实现在方块中心零速生成，静止位精确、无随机性，headless 截图可逐字节复现。`[Phase 10]`

## 物品 / 背包（Phase 11）

- **工具无完整分块类型效率** — 工具效率 = flat `break_speed` + Phase 67 起的**物品侧 `fast_against` 块名列表 × 倍率**（Phase 67 = 斧对 wood/leaves 2×，Phase 68 = 铲对 dirt/grass/sand/gravel 2×【项目土类全集——无 clay/sandstone/snow】，Phase 69 = 镐对 stone/cobblestone/5 矿/glowstone 2×【项目石矿类全集；**obsidian 例外** = MC 铁镐不能破黑曜石、项目无钻石镐，保留黑曜石 = flat 镐速 3.75 s（铁）】，物品数据驱动、init 逐条目交叉校验）。MC 的"无效率工具慢 10×"规则 **`[SURV-8]` 已解决**（`kWrongToolFactor` 0.1：工具对 off-list 方块 = 自身速度 ÷ 10，无列表工具对一切方块吃惩罚 = MC 剑情形，手/非工具位级不变；obsidian 排除条款随规则演进 = 铁镐挖 obsidian 变慢档）；仍缺：方块侧无类别字段（分类知识在物品侧的块名列表里）。`[Phase 11]` → `[Phase 67]` → `[Phase 68]` → `[Phase 69]` → `[SURV-8]`，详见 phase-surv8.md
- **非 block 物品的掉落物在 world 中不可见** — 工具/材料的**掉落物**（Q 丢出或挖出）没有 in-world 实体表示（`drawItemEntities` 跳过非 block 物品——物理/拾取/合并/消亡照常工作）；UI 侧（背包/hotbar/手持/合成产物）Phase 15 起已有图标（`Item.icon` 32×32 纹理，Phase 35）。`[Phase 11]`（UI 部分 `[Phase 15]` 已解决；**16 个 model 物品 `[CRAFT-2]` 已解决**：带 `model` 段的物品掉落物改绘双面板 box mesh（blending pass、关裁剪、per-item 模型纹理、kSize 足迹）；其余非 model 非 block 物品仍不可见 = `drawItemEntities` 的跳过路径位级保留；**`[ENT-9]` 全量解决**：全部 63 件非 block 无 model 物品补齐 `model` 段（texture = 既有 icon，零新纹理零核心代码）= 全 79 件非 block 物品掉落/丢弃皆可见，跳过路径退役（详见 phase-ent9.md））
- **拾取是固定 1.0 格半径，无磁力** — 玩家底心距掉落物底心 <1.0 且过 0.5s 拾取延迟即整堆入包；Minecraft 会随时间逐渐把物品拉向玩家。`[Phase 11]`
- **放置消耗库存（行为变化，非缺陷）** — Phase 11 起方块不再是无限的：初始 hotbar 每格满堆栈（block 64/工具 1，`hotbar.json` 定义），放置消耗 1 个；hotbar 与背包都空时右键无效。`[Phase 11]`
- **堆合并 = 同物品且同耐久（`[SURV-3]`）** — `Inventory::add` / 地面 `ItemEntity` 合并 / 拾取 的合并门 = 同物品 **且 damage 相等**（不等 = 不合并、开新槽；UI pick-and-place 同款）：MC 的 NBT 感知合并（修复合并/部分合并）的简化——不同磨损度的同物品堆永不互并（记档简化，设计定案）。`[SURV-3]`

## 合成（Phase 12）

- **配方 = shapeless + shaped（`[Phase 17]` 起）** — shaped（`pattern` + `key`，1..3 行 × 1..3 列；shaped 禁用 `ingredients`）`[Phase 17]` 实装（shipped 21 个：工具 / 营火 SSS/LCL/LLL / 楼梯 ×4 / 台阶 / 墙 / 栅栏 / 金苹果 / 铁块 / 工作台 / 弓 / 桶）；网格匹配 **shaped 先、shapeless 后，首个命中即成**（注册表加载序）。**"需要工作台" = de facto 网格尺寸门**：E 面板只暴露 2×2（共享 3×3 左上，见「工作台 / 形状合成」节 E 面板条），宽/高 3 的 pattern 只能在工作台摆出（与 MC 一致）。**无多配方消歧仍成立**：`craft` 按 id 精确执行；shipped 31 个 shapeless 原料集互不重复——若将来重复 = 加载序首个命中，系统不猜。`[Phase 12]`（shaped + 工作台门 `[Phase 17]` 已解决）
- **部分材料暂无用途** — **`gold_ingot`/`diamond` 用途已解决**：gold_ingot = 金苹果（shaped 镶金，`[Phase 49]`）/ 黄色染料（`[Phase 45]`）/ 速度弓（`[Phase 79]`）；diamond = 无限弓（`[Phase 77]`）/ 力量弓（`[Phase 79]` ×2），大师弓（`[Phase 82]`）间接。**仍无配方消费的材料 = 仅 `leather`**（见「Mob」节）。无饰品、火把用 coal（金/钻石不用于火把 = 偏离不变）；`coal`/`planks`/`stick` 在 Phase 40 起是炉子燃料（`fuel_time`）；`coal` 另进火把配方（Phase 65）；`planks`/`stick`/`iron_ingot` 已在配方链中。`[Phase 12]`（coal `[Phase 40/65]`、gold/diamond `[Phase 45]`→`[Phase 49]`→`[Phase 77]`→`[Phase 79]` 已解决）
- **矿石掉落改为材料（行为变化，刻意）** — 4 种矿石现在掉材料物品（coal_ore→coal、iron_ore→iron_ingot、gold_ore→gold_ingot、diamond_ore→diamond，MC 同款），**矿方块物品不能再从采矿获得**（数据仍在、可放置，只是无产出途径）；wood 仍掉 wood 本身（它是木板配方的原料，若改掉 planks 则配方链断）。`[Phase 12]`

## 存档（Phase 13）

- **存档是 opt-in（`--save <slot>`），无默认世界** — 不带 `--save` 时不读不写（保持既有运行行为与基线截图不变）；没有"上次世界"的隐式恢复。**槽管理面 `[SAVE-3]` 已解决**：CLI headless 四操作 `--list-saves`（名序确定性，摘要纯读文件内容无 mtime；损坏文件浮现 UNREADABLE 点名）/ `--copy-save`（字节保真分支，原子写）/ `--rename-save`（原子）/ `--delete-save`（槽名 = `--save` 字符集 = 路径穿越死）；**游戏内选择 UI/会话内换档保留不立项** = 游戏无标题画面，会话内载档 = 世界重建级架构变更（矩阵外记档偏离；多槽切换 = CLI `--save <slot>` 既有面）。`[Phase 13]`（管理面 `[SAVE-3]` 已解决）
- ~~**写存档时在退出路径重跑生成器做 diff**~~ — `[Phase 13]` 引入（`[Phase 14]` 起规模放大；用户日志 ~100k-chunk 世界退出 13.6 s，`[Phase 33]` 诊断 ~0.14 ms/chunk），`[Phase 37]` 已修复（并行化）：`diffWorld` 在 >256 chunk 时改为**每 chunk 一个 job 跑在临时 4-worker 池**上（小世界保持单线程、线程生成零开销），每 job 写自己的 slice、flush 后合并；输出按位置排序是**全序**（一格至多一个编辑）→ 并行结果与串行**逐字节相同**（E2E A/B 对 P36 串行构建的存档字节一致是核心证据；~9600-chunk 实测退出 diff 1.3 s → 0.35 s，墙钟 Δ0.98 s）。增量编辑跟踪（O(编辑数)）**`[PIPE-2]` 已实现**：World 写收口位置集 journal（observer，含位移陷阱双上报）+ 退出按 journal 触及 chunk 重算 fresh、仅 live≠fresh 输出（**与 diffWorld 字节等价**，进程内 oracle = `--save-full-diff` 旗，legacy 路径保留）；楼梯 O(N) 世界扫描一并转 journal 派生（不变式 = worldgen 永不生成楼梯，真实 assets 单测钉死）；vd80 全环退出 6.2 s → **实测 17 ms**（journal 路径；3600f 场景 legacy 对照 3.28 s ≈ 190×；K = journal 触及 chunk 数，退出成本与环大小脱钩）。99.2 残余①（仅流体模拟改动的远端列卸载回生成态）在新语义下自动一致（卸载驱逐 journal + 退出过滤只查现存 chunk）。细节见 `archive/phase-37.md` + `archive/phase-pipe-2.md`。
- **无周期性自动存档** — 崩溃/断电丢失自上次写入以来的全部修改；`F5` 立即存档（Phase 15，`--save` 槽激活时游戏内随时手动保存）已补上"会话中手动保存"，但仍无周期性 autosave/无"保存并退出"流程。`[Phase 13]`（手动保存热键 `[Phase 15]` 已解决；**`[SAVE-1]` 周期性 autosave + 崩溃韧性已解决**：`save.auto_save_seconds` 代码默认 120 s（0 = 显式关闭，config.json 零改动）真实 dt 计时器（`--save` 槽门控 = 无槽结构惰性，失败下帧重试）+ **原子写**（同目录 `.tmp` + rename，F5/退出/自动三路共用 = 崩溃时目标文件恒为完整旧档或完整新档，半写档不可达，kill -9 实证）；残余**`[SAVE-3]` 已解决**：多槽轮转 = `--copy-save` 分支 + `--rename-save` + `--delete-save`（CLI 管理面，headless 确定性）；"保存并退出" = 既有 ESC/退出存档路径（本已存在，无菜单流 = 无标题画面记档偏离））
- ~~**玩家速度/onGround 不存**~~ — 速度半 `[SAVE-2]` 已解决：format 22 `player.velocity` 3 数组恒写（oxygen/stats 键纪律：22 必键、≤21 携 = 版本谎报点名；区间 ±100 `kSavedVelocityMax` 损坏档点名）+ 读档 `restorePosition` 后 `setVelocity`（≤21 载入 = 零速 legacy 值保真免迁；门 141 S2c 孪生跨侧 player 段位级 + PNG 字节双证）；**onGround 半保留不存** = 位置派生（空中读档 onGround 自然 false / 静止读档首子步碰撞置位 = 值保真，记档偏离）。`[Phase 13]`（坠落伤害部分 `[Phase 36]` 已解决；速度 `[SAVE-2]` 已解决）
- **物品严格解析白名单漏 `saturation` = 10 行 init WARN 噪音** — 10 个带 `saturation` 的食物 JSON（SURV-9 内容）在 item_registry 的 unknown-field 白名单（~L960）中缺名 → 每次 init 打 10 行 `Item '<food>': unknown field 'saturation' ignored`（WARN 非 FAIL；字段随后被 SURV-9 消费路径正确读取 = 值有效，纯日志噪音；新旧二进制两侧恒同 = 结构无害）。修复 = 白名单加一行 `saturation`（基础设施轮，随插桩器入 tools/ 同类积压）。`[SAVE-2 门内发现]`
- **实体 `age` 按真实 dt 累积（墙钟）** — 与玩家/物理一致（非固定步长），所以含实体的存档文件在 age 字段上跨运行差 ±1-2 帧；渲染不依赖 age（掉落物动画由帧计数器驱动），截图确定性不受影响。昼夜 `time` 是固定步长（位精确）。`[Phase 13]`
- **`DayCycle::setTimeOfDay` 至多 1 ulp 偏差** — 存 timeOfDay [0,1)，恢复时解出 totalSeconds；对远离配置 start_time 的值，`startTime + delta` 的浮点往返可能差 1 ulp（~1e-16）。昼夜渐变段是常量/平滑的，呈现无影响。`[Phase 13]`
- **演示区外的编辑会带上"生成的 chunk"** — 在演示区外放置方块会同步生成该 chunk（既有行为），存档 diff 覆盖它；读档时 `setBlock` 重建同样生成该 chunk——一致，但读档后的世界含全新世界没有的 chunk（读档后 `propagateAll` 全量重传播，之后新出现的 chunk 由 Phase 14 的 worker 光照 job 处理）。`[Phase 13]`
- **抽干的天然熔岩入档不完整、读档后"复活"** — `diffWorld`/journal 流体 diff 不对称：对「生成态有熔岩、会话抽干（level 0）」的格，`fluidKindOf(0)` 特例按 Water 记账 → diff 记为 **level-0 水**编辑（被抽掉的 kind 不记录）；回载 `World::setFluid(level 0)` 只清同 kind 水、不清熔岩，且 `applyWorldEdits` 对「level-0 且该 kind 已空」的编辑跳过 → 读档后生成熔岩复活。触发路径：玩家用熔岩桶抽干天然熔岩湖后存档重开。修复方向（二选一）：diff 对被抽干的格记 drained kind / level-0 编辑清任意 kind。`[PRE-3 2026-10-26 上报，未修，矩阵外]`
- ~~**存档无迁移机制**~~ — 当前 **format 15**（save.hpp `kSaveFormat=15` / `kSaveFormatMin=1`），**format 1..15 全部可加载**（各按其字段集 + 逐 format 白名单），含 5 处规则式迁移：6→7 炉子（sole 归属/丢弃，见「炉子 / 熔炼」节）、≤11→12 吸收（共享 health 按 base max 拆分）、11/12→13 营火（单槽→槽 0）、13→14 stairs（迁移为空；旧 format 带新键 = 版本谎报拒绝）、**14→15 states**（`stairs {pos,facing}` → `states {pos,state}` 值保真；ENT-2）。写入恒 format 15。`states` 键纪律：15 必需（空数组合法）/ ≤14 携带拒 / 15 携 `stairs` 拒；双向交叉校验 = stateful 格（hasState：Stairs 形状 ∨ `model.state_count`）↔ entry 逐格点名 + 域按块 stateCount。"未知字段=报错"（rejectUnknownFields）保持。`[Phase 13]`（`[Phase 16/17]` 起 format 2/3 可加载，其后逐 format 追加；ENT-2 format 15）

## Chunk 流式（Phase 14）

- ~~**只增不卸载** — 流式只加载、从不卸载 chunk：内存随探索面积线性增长（半径 8 列环 ≈ 2304 chunk ≈ 35 MB）。~~ — `[Phase 14]` 问题，**`[99.2]` 已修复**：`unloadFarColumns`（pool flush 后的主线程单写窗口）卸载距玩家 > view_distance+1（Chebyshev，+1 迟滞带防抖）的列；保留列 = 演示 4×4（重生安全）+ 玩家编辑列（m_editedColumns，存档 diff 所在）+ 五类 per-block 状态列（炉子/营火/作物/信标/火，存档交叉校验对象）+ 在途生成列。实测走路 85 s：RSS 平台 470-474 MB（修复前无限增长）。**残余**：① ~~仅流体模拟改动（远端熔岩流动无 fire 条目）的列卸载后重置为生成态~~ — **`[STRM-2]` 已修复（v1.1.0）**：驱逐时按 journal 格限域快照该列 live≠fresh diff（`retainColumnDiff` = `journalDiffChunk` 字节同语义）存入按列保留表（保留 diff 条目非 chunk = 内存有界）；列再流送时 integrate 回放（blocks 先 fluids 后 = applyWorldEdits 序，observer 自动再 journal 化 = 自相似循环，**不插 m_editedColumns** = fluid-only 列保持可卸载）；退出存档两条路径（journal + `--save-full-diff` legacy oracle）`mergeRetained` 合并（不变式：每列至多在 journal 或 retained 之一 = 无重复格）；读档路径零改动（既有顶满列 + m_editedColumns 保护）。覆盖范围 = 该列全部 journal diff（流体 + 燃烧/水熔岩反应的 block——后者经 world.setBlock 绕过 choke point 同样无保护资格）；② ~~环是**正方形**~~ — **`[STRM-1]` 已解决**：环 = 圆盘（dx²+dz² ≤ R²，纯整数谓词）+ **远平面帽** R_eff = min(view_distance, 64)（推导：kFar=1000 块——可绘制列全部 d ≤ 63.91，d≥65 的列最近角 ≥1002 块 = 原理上永不渲染，流送纯浪费）：vd80 = 12853 列 ≈ 103k chunk。实测 RSS A/B（同机同场景流送到 complete）：**方形 9.08 GB → 圆盘 4.59 GB（−49.5%）**（原文"~12 GB"为估算，方形实测 9.08）；原文"球 ~1/9 列数"把 MC 的形状与 MC 的渲染距离量级混为一谈（同 R=80 圆盘 = 9.3 GB；达 ~1/9 需 R_eff=30 = 480 块地平线，用户定案暂不采用、一个常量可切换）。base18 **第 81 次门**（无输入结构性免疫：流送永不 engage）。
- **新列"光照后再网格化"（lit pop-in，非首帧暗）** — 流式列/首触列集成后进入 `m_unlitColumns`：光照 worker job 完成前**不网格化**（chunk 无 mesh = 不可见，等同 void），job 完成后（下帧 flush 收集）才一次性网格为正确光照。`[Phase 14]` 原设计是"首帧光 0 网格化 + job 完成后重网格"——99.2 的重 mesh 帧预算把重网格推迟了整整一个脏块积压（秒级），新流送地形持续黑面（用户走路跑报告）；lit pop-in 同时消灭黑面与二次网格（每新列只 mesh 一次，网格工作量减半）。确定性 pop-in（帧锁定）。`[99.4]`
- **增量光照不能"消光"（封洞伪影）** — 新列的光照走 `updateRegion`（天空光列重扫可减可增；BFS 只严格增大）。若某处旧高光的唯一通路穿过当时未加载的虚空（如演示区边缘的洞穴开向虚空），新列生成后通路被封，区域外的旧高值不会被降低（BFS 不跨区减光）。旧代码的"新 chunk→全量重传播"无此伪影（代价是每次 50 ms+ 游戏线程尖峰）。窄场景，演示区边缘的洞穴开向虚空才触发。`[Phase 14]`
- **流式触发是"贴近缺失列"迟滞，静止玩家永不流式** — 激活 = 玩家距缺失列 ≤1（平方欧氏，`[STRM-1]`；整数偏移下 = 玩家列 + 4 轴邻列，对角 (1,1) sq=2 不触发）；激活后直到玩家周围半径圆盘充满才解除。设计目的：无输入运行（基线截图）零 job、逐字节复现。副作用：站在加载区正中（距边缘 2 列）的玩家即使半径圆盘未充满也不会流式（他从未"贴近过"边缘）。`[Phase 14]`
- **存档点位于未加载区域时，读档玩家先下落几帧** — 读档世界 = 演示区 + 编辑重建的 chunk；存档玩家在演示区外时，他的列由流式在 ~2-4 帧内补上（触发器对"玩家自身列缺失"= 距离 0 立即激活），期间玩家从存档位置下落（实测 ~0.03 格，落在整数地面平面后逐位精确）。无死亡风险（远小于 -32 杀线）。`[Phase 14]`

## 性能 / 架构

- **Chunk 网格化 = worker 快照 job（`[PIPE-1]` 起）+ 主线程列合并 GL 上传** — `[99.2]` 的 `kRemeshBudgetPerFrame=8`（主线程预算 mesher）**已删除** → `[PIPE-1]`：**mesher 计算搬 4 worker**（每帧至多 `kMaxMeshBacklog=2` 个列快照 job：目标列 3×3 邻域 9 列 × 8 chunk memcpy 进 job 私有临时 World，worker 用**零改动的 ChunkMesher** 出 8 chunk 几何，次帧 flush 后主线程 move 进 `m_chunkMeshData` + 清脏 + 列合并 GL 上传【GL 上下文只在主线程，~1-3 ms/列 = 不可搬走部分】）。选择规则 = 脏列投影 ∩ 非 unlit（99.4 lit pop-in 承接）∩ 距当帧 light-pending 列 Chebyshev ≥2（光 footprint E±1 列会改 C 的光 = 本帧快照必陈旧；被排除列保持脏、下帧自动入选）∩ 非在途，`columnDistanceSq` 最近优先。**帧时序**：pop-in 晚 1 帧（render N+1→N+2，帧锁定确定）；已 mesh 列的邻列 stream 进来 = 最坏 +2 帧收敛（边界面临时对 void 烘焙，dirty 自愈，PITS 有录）。**启动演示区网格化**也走 worker（2 列/帧，帧 ≤10 完成——base18 论证措辞 = "无输入 worker 活动有界于启动 16 mesh job，其后零活动"；帧 60 终图像位级不变 = A/B 四对字节实证）。**实测**：vd80 step-0（插桩）主线程 remesh 填充期 7-11.3 ms/帧 + complete 后 **2.8 min 拖尾**（脏集峰值 45,895）→ `[PIPE-1]` 后主线程只留 GL 上传（step-0b 实测）；填充墙钟不变（gen/post-rate bound：vd16 A/B 8.1 s vs 8.0 s，存档 time/age 剥离后字节一致）。`[Phase 3]`（生成部分 `[Phase 14]` 已并行化；重 mesh 预算 `[99.2]`；列合并+裁剪 `[99.9]`；**worker 快照 mesh `[PIPE-1]`**）
- **vd80 渲染管线（`[99.9]` 前是 2-30 FPS 坍缩，现已修）** — 三个实测根因：① 无视锥裁剪：207k 加载 chunk 全部提交 draw call（iGPU 每 draw ~10-30 µs + 小 VBO 页表开销，随环增长无上限）；② 光照 job 每格一次 `unordered_map` 查找（天空重扫+种子扫描+BFS ≈ 12 ms/box，流送 2 box/帧 = flush 恒等 25-30 ms）；③ 每 chunk 一个小 VBO。`[99.9]` 修复：视锥裁剪（列级保守 AABB）+ 列合并（draw ÷8）+ 光照 chunk 级重构（每 16³ 块一次查找 + BFS last-chunk 缓存 + 既有地形重扫零写入时跳过天空 BFS——**新地形必须强制传播**，`newTerrain` 参数：整列天生暗（天花板下）时重扫零写入，但侧邻列的光仍要流进来）。实测 vd80 流送中：render 37→1.5-2.5 ms/帧（不随环增长）、flush 25-30→13-17 ms、主线程 CPU 100%→40%（4 worker 各 16-23%，负载分布见 99.9 报告）。**残余**：~~方形环 12 GB~~（`[STRM-1]` 已解决：圆盘 + 远平面帽，实测 9.08→4.59 GB，见「Chunk 流式」99.2 条）、~~光照 union-region~~（`[LGT-1]` 砍除记档：step-0 实测同帧选列大多远隔（(distSq,x,z) 平局 → 位置序配对同半径圆远隔对称点），可合并 6/6418 = 0.1%、预期节省 0.002 ms/帧 ≪ 1 ms 砍除线；光照 job 下限 ~16.7 ms/帧保留 = 流送 fps 主下限，潜在方向 = 选列 tie-break 相邻化 + union（STRM 相邻变更）表外——见 `archive/phase-lgt1-flip.md`）、worker 网格化（`[PIPE-1]` 已完成：mesher 计算 → worker 快照 job，主线程只留 GL 上传，见「性能 / 架构」首条）。
- **sky rescan clobber（潜伏，`[LGT-1]` 设计时发现，未修）**：天花板板骑跨
  光照 footprint 边界（边界一侧开敞列、另一侧 opaque 天花板延伸）时，顺序
  两次 region 传播中**后一 region 的 rescan** 把边界格 sky 重写为纯地形值
  （0），sky BFS 无边界 seed、开敞列不在后一 region 内 → 前一 region 的
  水平光流无法恢复 = **stale-dark 暗带**（1-2 格宽、天花板下）。block 光
  不受影响（shell 注入自愈）；确定性不受影响（每次 run 同结果）。修复形态 =
  单遍 union/整 footprint 单 region（LGT-1 形态，已砍除）或 sky BFS 加
  shell seed（改动面大，留 100+）；暴露面窄（仅骑跨边界的天花板地形），
  暂留未修。（`[LGT-2]` 交互：衰减语义下 clobber 后果从「stale-dark（0）」
  扩到「stale-partial（垂直值 < 真值）」——部分衰减冠骑跨 footprint 边时，
  前一 region 的水平流（穿冠 2/格）被后一 region rescan 覆写为纯垂直值且
  sky BFS 无边界 seed 不自愈；暴露面量级不变（需同 job 双 box footprint
  相交 = LGT-1 step-0 实测 0.1% 帧 + 冠骑跨边），修复形态同前。）
- **叶冠下作物生长（`[LGT-2]` 行为变化，MC 一致，非缺陷）**：冠下光按层
  衰减（leaves −2/层）后，wheat `growth_light=9`：3 层冠下 = 9 临界仍过
  （可长）、叠冠/4 层+ = ≤7 停长。「树下种麦」玩法按 MC 语义收紧；wheat
  growth_light 保持 9（MC 值，不偏离）。
- **面条洞穴行为注记（`[GEN-1]`，均 MC 一致/文档化偏离，非缺陷）**：
  ① ~~洞口悬树~~ **`[GEN-5]` 已修复**（caveCarvesCell 纯谓词守卫跳过整树；
  旧存档未编辑列读档即消失 = 世界生成演进先例）② 海床洞穴一次性渗水（生成平衡态后静态，99.8 冷 chunk 零成本）
  ③ 洞内 24/7 敌对生成（封闭袋 sky 0 = 敌对光门 0，昼夜无关 = MC 洞穴
  语义；出生点 spawn protection circle 不变）④ 旧存档加载得新洞穴
  （世界生成演进 = P86/88/93 先例；编辑保留、零 format）。
- **`Chunk&` 引用会被 unordered_map rehash 失效** — `getOrCreateChunk` 返回的引用在后续可能 rehash 的 World 变更（其他 chunk 的 setBlock/getOrCreateChunk）后失效；已在 world.hpp 头部文档化，调用方须在一个逻辑操作内使用。`[Phase 3]`
- **Greedy Meshing 已实现（Phase 18）** — 逐面片合并为同（纹理、sub-mesh、面光）run 的单 quad；200-chunk 实测总三角形 109,648 → 33,558（3.3×，不透明 3.5×）。剩余简化见「渲染 / 网格化（Phase 18）」。

## 渲染 / 网格化（Phase 18）

- **无输入基线重拍（base3 → base18）** — Greedy Meshing 有意改变了无输入渲染：合并 quad 的纹理 UV 插值与逐面片在浮点层不同（数学上同一映射），GL_NEAREST 下少数纹素边界翻转；合并 quad 的剪影边缘线（远世界边界山脊）亚像素偏移 1-2 px。实测与 base3 差 3593 px（0.39%），全部为剪影边缘/纹理重采样（几何覆盖集经逐 chunk 对比 100% 相等）。`/tmp/opencode/base18.png` 为新基线，后续阶段对它字节门控。单面片（1×1）quad 保持 Phase 17 的 legacy atlas UV + `tiled=0` 路径，逐位精确不变。`[Phase 18]`
- **合并不跨 chunk 边界** — 每个 chunk 独立合并：相邻 chunk 的同质地形产生相邻的两个 quad 而非一个大 quad（边界处少合并）。标准做法（Minecraft 同样逐 chunk 合并）；跨 chunk 合并需要边界交换，留待需要时。`[Phase 18]`
- **流体表面仍逐面片** — 水面是 level 缩放的几何（顶面在 level/8 高度、侧壁随 level），合并规则与整格面片不同，Phase 18 刻意不合并（保持逐面片 + 原有 culling）。`[Phase 18]`
- **两个相邻非 opaque 方块之间仍双向出面** — 每平面两个独立 slice（+normal 来自下方格、−normal 来自上方格）分别合并，叶片-叶片/玻璃-玻璃之间的双面保留（正确、略浪费；相机只看到前向一面，背面被 cull）。`[Phase 2]`（Phase 18 按设计保留）

## UI / 音效 / 粒子（Phase 15）

- **音效是固定事件集，无 per-block/per-item 差异** — v1 只有 8 个数据驱动事件（dig/place/break/pickup/step/ui_open/ui_close/craft），所有方块共用同一 break/place/dig 音色，无 MC 式的材质/方块音色差异、无 3D 空间化（单声道 2D 播放）。加 per-block 音色需扩事件集 + 音色映射数据。`[Phase 15]`（**`[CRAFT-3]` 已解决**：块 JSON 可选 `sound_material` = 6 材质（stone/wood/gravel/sand/cloth/glass）× 3 事件（dig/place/break）= 18 个确定性新 wav，32 块赋值（stone 8 含矿脉族/wood 3/gravel 1/sand 3/cloth 15/glass 2）；无字段块回退通用事件名位级不变。**空间化** = SoundBank stereo 化 + `play(name, origin)` 距离衰减（线性 cutoff 16 块命名常量）+ 恒定增益线性声像（listener = 相机），dig/破坏/放置/潜行旋转/丢弃/拾取 6 点位世界空间化，手持破碎/step/UI 等保持 2D；实际听感 = 用户实机（无头不可听））
- **物品无 3D 模型，图标是程序化 32×32 贴图** — block item 的 UI 表示是带光照的倾斜 3D 立方体（复用掉落物 cube mesh），工具/材料是 `tools/generate_item_icons.py` 程序化生成的 32×32 平面图标（Phase 35：16×16 → 32×32，形状谓词按 2× 分辨率重采样——弧线/对角线获得真 2× 边缘细节，非 2×2 upscale；非 MC 的 3D 旋转手持模型）；无手持动画/旋转。`[Phase 15]`（32×32 `[Phase 35]`；**16 个物品 `[CRAFT-2]` 已有数据驱动 3D box 模型**：item JSON 可选 `model` 段 = 1..16 个 axis-aligned box + 单一纹理（严格解析、无旋转 = MC primitive rotation 记档不做、v1 简化体素造型）——手持（替代 icon 面片、继承弓拉/进食举到嘴变换）/世界掉落（替代不可见掉落）/进食举到嘴三上下文渲染；**`[ENT-9]` 全量解决**：其余 63 件全部补齐数据驱动 3D box 模型（家族几何 1-3 box 剪影、texture = 既有 icon PNG、零核心代码）= 全 79 件非 block 物品三上下文 3D 模型，2D icon 手路径退役；block item 3D 立方不变（详见 phase-ent9.md））
- **背包 UI 右键语义极简** — 左键取整堆/放入/交换（取出被占堆）/并入有空间同物堆；v1 右键只"移 1 个"（同物有空间或空格，不同物格无效）。**`[CRAFT-7]` 已解决（3/4）：**① 右键取半堆（MC：空手右键占位格取 floor(N/2) 最小 1——N=1 取自身、N=2/3 取 1 = v1 行为边界、N=64 取 32；耐久随堆 SURV-3 纪律）② shift 快捷移动（MC E 屏语义：hotbar⇄storage 互移、容器格→主背包优先、同物同耐久合并、堆可跨格分裂、手持堆 no-op；炉子面板 = 容器 GUI 语义：炉槽→主背包、玩家背包侧 no-op、移 input 按 furnace.hpp 不变量重置进度）③ 双击收集分拣（MC 分区语义：hotbar 0..8 与主背包 9..26 为两个独立分区，点击分区内收集同物同耐久堆、溢出按索引序回填、游戏层 0.3 s 真实 dt 双击窗【生存计时器类 = 输入态不入帧状态，无输入跑永不触碰】）。另：顶排数字键 1-9 直接选热栏槽（MC；Tab 循环保留，面板开/关皆可用；右 Shift 并入 LeftShift 映射）。**残余 = 无拖拽**（记录偏离：三点击操作 + 数字键覆盖核心操作面，拖拽需指针捕获下的鼠标轨迹跟踪 = 最高风险 E2E 类，矩阵外）。点击面板外（左键）丢弃手持堆。关闭面板时手持堆保留在 UI 实例（v1 既有语义，MC 会退回背包 = 记档偏离不变）。`[Phase 15]`（→ `[CRAFT-7]`）
- **背包打开不暂停世界模拟** — 打开背包只暂停**玩家物理与方块交互**；世界/昼夜/流体/实体/流式加载照常 tick（与 MC 的"暂停菜单"不同，更接近"边看背包边看世界"）。`[Phase 15]`
- **F3 FPS 显示为整数** — `m_fpsSmoothed`（真实 dt 平滑）取整显示，vsync 稳定时恒 60；无小数帧率、无帧时间直方图/分位数（Minecraft F3 有 1% / 0.1% lows）。`[Phase 15]`
- **粒子是逐帧动态 mesh、无碰撞** — 每帧重建一个动态 indexed mesh（0.12 格小立方体），粒子之间/粒子与世界无碰撞（穿模），无风/阻力，半隐式 Euler（先速后位）；上限 256、每爆发 16、寿命 0.45~0.75s。spawn 时烘焙光照（渲染是只读窗口，不能读光数组）。`[Phase 15]`
- ~~**破坏 cross 块（作物/花/火把）崩溃：粒子纹理查找 `unordered_map::at`**~~ — `[Phase 65]` 发现并修复：`Game::drawParticles` 对所有块的粒子用 `cubeFaceTexture`（top/side/bottom 槽）查纹理——cross 模型（`[Phase 45]` 引入）没有这三项纹理（只有 `cross`）→ 空名 → `AtlasLayout::uvMin` 直接抛 `unordered_map::out_of_range`。`[Phase 45]` 起的潜在 bug（此前无任何 E2E 破坏过 cross 块），由 P65 火把破坏 E2E 首次暴露（gdb 回溯定位 `drawParticles → uvMin`）。修复 = `drawParticles` 加 cross 分支：粒子立方体六面全用 `cross` 纹理（与 item_mesh / chunk_mesher 既有的两个 cross 分支同款单一查找）；E2E S4（火把破坏 → 粒子帧渲染 → 无崩溃 + ×2 savenorm）为回归证据。`[Phase 65]`
- **字体无中文字形与字重渐变** — `tools/generate_font.py` 生成 256×512 字形 atlas（32px cell、16×16 glyph 8px 偏移；Phase 35：8×8 → 16×16，字形表一次性从 DejaVu Sans Bold em16 提取（基线 row 12、阈值 128）后静态嵌入生成器——纯 stdlib、确定性输出），仅 ASCII 可打印（非 ASCII → `?`）；无字重渐变（缩放支持 double scale——Phase 35 起面板自适应分数缩放，`textWidth/makeTextVertices` 均 double）；无中文字形。`[Phase 15]`（8×8 低细节 `[Phase 35]` 已解决：16×16 字形 + 32×32 物品图标）
- **物品名 tooltip 只有名字** — 悬停格显示物品名（无数量/描述/工具耐久/附魔文本）。`[Phase 15]`
- ~~**"You died!" 死亡屏幕文字实际是白色（颜色参数被忽略）**~~ — `[Phase 33]` 发现，`[Phase 36]` 已修复：`ui.frag` textured 分支改 `texel.rgb × u_color.rgb`（所有 textured 绘制的 u_color.rgb 均为白，白字/图标/心带逐位不变，只死亡屏红字生效）+ E2E 重像素验证（红字 bbox A (488,320,282,24)/B 恰好 2×，hint 白字 bbox 不变）——细节见 `archive/phase-36.md`。

## 生存（Phase 16）

- **伤害来源：坠落、虚空、mob 攻击、熔岩灼烧、燃烧效果与火接触** — 无 mob 攻击与火接触以外的接触伤害（`fire` 块非 solid，玩家站火里被**点燃** = Phase 74【P70 直伤版退役】：fire 效果 1 HP/0.2 s、离开残留 8 s、水灭；Mob 站火里被点燃 = Phase 73，P58 熔岩点燃的块版）。`fallDamage = max(0, 净下落格数 − fallDamageOffset)`（offset 默认 3），从离地帧的 `fallStartY` 到落地点的净竖直距离结算；落入水中的落地取消伤害。~~无护甲/摔落缓冲机制~~ — **`[SURV-1]` 已解决（护甲 = 摔落缓冲：全源减伤含坠落）**：12 件护甲（leather/iron/diamond × helmet/chestplate/leggings/boots = MC 精确值 1/3/2/1（总 7）、2/6/5/1（总 14）、3/8/6/3（总 20），max_stack 1，材质档 = 三链；chain 无材料 = 记档偏离）+ MC 整数 floor 减伤（`Player::damage` 单收口 = **全伤害源生效**【坠落/饥饿/熔岩/虚空/mob/箭/火 tick 全走它】，次序 armor → resistance → 吸收池，pts=0 位级恒等）+ 右键穿戴/交换/脱下通道（点火器之后、潜行旋转之前；空手按 head→chest→legs→feet 固定序脱下到选中槽——10-25 修复脱下 dangling reference【物品消失，PITS 246】，逻辑上提纯函数 `item/armor.hpp` + 9 单测；脱下后手非空，连点 = 穿上/脱下震荡，脱整套须每点之间换空热栏槽）+ 存档 format 16 `player.armor`（恒写、只含占位槽、键纪律 + 槽名-物品数据交叉校验）+ HUD 心行上方护甲排（仅占位槽绘制 = 无输入结构性免疫）；~~窒息伤害未实现~~ — **`[SURV-2]` 已解决（氧气窒息：水下/熔岩扣空气 + 耗尽后每 150 帧 2 HP 豁免护甲减伤）**。`[Phase 16]`（mob 攻击 `[Phase 20]`、熔岩灼烧 `[Phase 54]`、燃烧效果 `[Phase 57]`、玩家火接触点燃 `[Phase 70]`→`[Phase 74]`、Mob 火接触点燃 `[Phase 73]`、护甲 `[SURV-1]`、氧气窒息 `[SURV-2]` 已解决）
- **`[SURV-2]` 氧气窒息（水下/熔岩扣空气 + 耗尽每 150 帧 2 HP）** — 纯函数 `player/drown.{hpp,cpp}`（`drownStep` 帧锁定节奏 + `headInFluid` 单格眼位谓词【眼在水/熔岩里；站 1 格浅池/漂浮面 = 换气；熔岩同款溺水 = MC】，campfire 范式单测拥有）+ `Player::damage` 加 `bypassArmor=false` 默认参（**溺水 = damage(2.0f, true) 豁免护甲 floor 减伤【维基伤害表 Drowning: Armor: No effect】，resistance 与吸收池照常【Resistance: Applies】；默认参 = 既有调用点位级不变，存量伤害测试零改动**）+ Game 每帧 pass（player update 之后、传 m_frameCount、面板暂停期间照常 tick）+ 死亡/respawn 复位满空气 + **format 17 `player.oxygen` int [0,150] 恒写**（17 必需 / ≤16 带键 = 版本谎报点名 / 越界与非整数点名 / 15/16→17 迁移满空气 150）+ **HUD 氧气泡排替换饥饿排原位**（`locked && !panelOpen() && oxygen<150`；10 图标 = ceil(oxygen/15)、单 tile 无半态、出水同帧消失；`oxygen.png` 16×16 = `tools/generate_oxygen_icons.py` salt 105 起【既有 PNG 重跑零变化】）+ F3 `oxygen N` 行。**范围外（记档偏离）**：① 无气泡/喝水声（MC 气泡声本身随机 = 非确定性，项目无新 wav）② 无 mob 溺水（MC mob 不溺水；任务条 = 玩家氧气条）③ 无呼吸附魔/水肺/和平豁免（项目无这些系统）④ 无 config 键（MC 值 = `player/drown.hpp` 命名常量 kOxygenMax=150 / kOxygenDecayFrames=6 / kDrownDamageFrames=150 / kDrownDamage=2.0f，YAGNI）⑤ **drownTicks 不入档**（读档归零 = tick 相位惯例，effects 同款；MC 存 airSupply 相位 = 偏离）⑥ 气泡排空位不绘暗态（单 tile 纹理设计定案，填充数 = 绘制数）。`[SURV-2]`
- ~~**存档加载到空中位置时坠落伤害被绕过**~~ — `[Phase 33]` 发现（P33 E2E 探针实证：46–59 格自由落体满血落地），`[Phase 36]` 已修复：`Player::restorePosition`（= `teleportTo` + `fallStartY = pos.y` 重钉），`applySave` 改走该路径 + 单测 ×2（空中 10 格 → 精确 7 HP；地面读档零伤害）+ E2E（存档 pos y=62 加载 → 8 格坠落 → health 15 精确）——细节见 `archive/phase-36.md`。
- **生命 HUD 是 10 心三态，半心按 0.5 HP 粒度渲染** — 每颗心 2 HP，填充 = `clamp(hp − 2i, 0, 2)`（2=满/1=半/0=空），贴图是 3 个固定状态（满/半/空），无更细粒度（如 0.5 HP 的四分之心）；MC 的心是 10 颗每颗 1 HP 且支持 0.5 增量。`[Phase 16]`
- **无死亡分数/死亡统计** — 死亡只显示 "You died! / Press R"，无得分、无死亡次数、无 Game Over 统计屏；重生保留背包与全部物品（无死亡掉落）。`[Phase 16]`（**死亡次数 `[SURV-7]` 已解决**：`player.stats.deathCount` + 每 mob `kill.<name>` 计数器（帧锁定事件、format 21 存档、封闭词表校验）——统计的**显示**（统计屏/F3 行/Game Over 屏）= `[PRE-12]` 呈现框架；得分/死亡掉落 = 范围外记档，详见 phase-surv7.md）
- **自然回复是定时 + 饥饿门控（Phase 23 起）** — 脱离受伤 `regenDelay=10s` 后每 `regenInterval=4s` 回 1 HP，且要求饥饿 ≥ 6（Minecraft 规则：低于 6 无自然回复）；门控关闭时累加器暂停（不重置），吃饱后继续累计。**`[SURV-9]` 已解决：自然回复的每半心消耗 1 点饥饿（MC 规则）**——经标准路径（隐藏饱和度 buffer 先扣、耗尽掉条 = Phase 28 衰减 tick 同款原语 `drainOneHungerPoint`；**满血时区间免费流逝** = 无回复无成本），回复门（bar ≥ 6）不变（MC 18 = 既有偏离条款）。`[Phase 16]`（饥饿门控 `[Phase 23]`，回复成本 `[SURV-9]`）
- **潜行缩高：钻 1 格缝隙仍未实现，楼梯几何下 1.6/1.8 有通行差异** — 潜行把身高 1.8→1.6（脚底不动、眼高降 0.2），速度 ×0.3、禁用 step-up、1 格崖缘 guard（`[Phase 16]` 原状）。**`[Phase 16]` 时"纯整数世界 1.6 无通行差异"的前提已被消费**：`[Phase 85/87]` 半块/楼梯带来非整数头顶空间——但**最大缝隙 = slab 上方 1.5 < 潜行 1.6**，"钻 1 格高缝隙"仍未实现；唯一 1.6/1.8 改变可通行集合的几何 = 楼梯斜面（如楼梯排上方 2 格高走廊：站立需 AABB 覆盖最高斜点 ≤0.2、潜行 ≤0.4，窄边缘情形）。**潜行其它用途**：营火坐姿（`[Phase 71/75]`：crouch + 贴地 + AABB 重叠 campfire 格 = 锁移动/跳）、楼梯旋转键（`[Phase 94]`：潜行 + 右键）、低顶陷阱（`updateCrouchHeight`：头顶空间 <1.8 时松开键不站起）。`[Phase 16]`（半块/楼梯 `[Phase 85/87]` 已就位；缝隙通行仍未实现）
- **step-up 只抬升 1 格、每 tick 至多一次** — 条件：非潜行 + 贴地 + 身体贴住整数块面（`kFaceEps=1e-4`）+ 面前脚位 solid + 其上方 2 格空；X 轴优先于 Z 轴（固定顺序），一次 update 只抬一次。2 格及以上仍须跳跃。`[Phase 16]`（**`[ENT-3]`** 起纯几何 = `Entity::stepUp`，mob 追逐/游走同款共享——player 逐位不变、既有单测零改动；mob 侧无潜行门。**潜伏 quirk（ENT-3 探针实证，player/mob 同款共享）**：① +X/+Z 方向 front 格查 `snapped+1`（-X/-Z 查 `snapped-1` = 贴住的格）→ **恰 1 格宽的台阶在 +X/+Z 方向踏不过**（≥2 格宽或 -X/-Z 方向正常，单测 `MobStepUpTest.DoesNotStepSingleCellStepPlusX` / `.StepsUpSingleCellStepMinusX` 钉死）；自然地形台阶通常多格宽，影响限于 1 格窄沿。② **非整数脚高行走**（slab 顶 1.5 / 楼梯斜面）时"贴住整数块面"判定只看 face 位置落在整数 ±`kFaceEps`、不校验本 tick 是否真被 clamp：face 巧合对齐 + 脚位行（`floor(footY)`）前方 solid（= 脚下的 slab 自身）→ **凭空抬升 1.0 再落回**（mob slab 追逐测试 frame 606 实证，落回后轨迹恢复）；整数脚高（平地/满块顶）下 face 对齐 ⟹ 真被 clamp，无此问题。修复（真 clamp 校验 / +X front 格）= player step-up 行为变更，单独立项。）
- **`[SURV-3]` 耐久系统（工具/护甲/弓/点火器磨损 + 破碎 + format 18）** — 物品字段 `durability`（严格 number > 0，允许集 = Tool ∪ armor_slot ∪ ammo 发射器 ∪ ignites_flamable，其它物品带字段 = 解析拒绝点名）+ 27 件 MC 精确值内容（wood 59 / stone 131 / iron 250 三工具 ×3 材质；bow 族 5 件 384；燧石与钢 64；护甲 leather 55/80/75/65、iron 165/240/225/195、diamond 363/528/495/429）+ `ItemStack.damage` float（operator== 含 damage；堆移动随堆）+ 纯函数 `item/durability.{hpp,cpp}`（`applyDurability` 扣减/破碎 + `armorWearPerPiece`，campfire 范式单测拥有）+ 全帧锁定磨损事件（破非瞬断块 +1【hardness==0 = 0 扣】/ 近战命中 mob 工具 +2 / 弓射出 +1 含 infinite / 点火成功 +1 / **护甲 = Player::damage 内 bypassArmor==false && pts>0 && 减伤后仍造成 >0 时每件在穿 +max(1, incoming/4)**【incoming = 减伤前原始量，10 → 2.5 float 精确、1 → 1 下限；破碎 = 槽清 + points 收口同步、不影响本次动作】）+ **format 18**（damage 键 >0 才写：inventory slots / armor【占位槽翻 {item,damage} 对象形，满耐久 = 字符串形】/ entities item 三处；≤17 带键 = 版本谎报点名；17→18 迁移恒等；交叉校验 damage < item.durability + 无 durability 物品带 damage 拒）+ HUD 耐久条（drawIcon 共享点：图标底部 2px 横条、填充 1−damage/durability、三色档绿≥1/2/黄≥1/4/红<1/4、满耐久零绘制 = 结构性免疫）+ F3 `held name d/max` 行 + 破碎音复用既有 "break" wav（sound 表无专用 item-break 资产，设计定案选项）+ 1466/1466（+18）。**范围外（记档偏离）**：① 无修复（铁砧/磨刀石/同物合成/Mending——矩阵无对应项）② 无 Unbreaking 类随机（项目无附魔系统）③ 耐久条三色简化（MC 连续渐变）④ 瞬断判定 = hardness 0 近似（MC "naturally breaks instantly" 含工具高速特例）⑤ **护甲磨损源 = 本项目"护甲实际减伤"的伤害事件**（设计定案门 = bypassArmor 豁免规则天然对齐：溺水 bypass = 不磨损【MC 同款，单测钉死】；坠落被护甲挡 = 磨损【自洽】；**MC 不磨损但本项目经护甲减伤的源——饥饿/火 tick/虚空/效果 tick（如 poison）——在本项目中磨损护甲**（它们不走 bypassArmor，护甲 floor 对它们生效，"磨损 = 减伤生效"自洽规则下即磨损；影响 = 死亡/长跑边缘情形，无 gameplay 回路差异）⑥ **合成网格/炉子/营火槽不入 format 18 的三处范围**（设计定案 = slots/armor/entities 三处）：受损物品放进 crafting grid 或炉子槽后读档 = 耐久回满（数据丢失 quirk；现役内容无"可熔炼且带耐久"物品，炉子/营火实际不可达，仅 2×2/3×3 网格可触达）⑦ 护甲破碎无声（Player 无 sound 通道；四个 game 侧破碎点有 "break" 音）⑧ mob 穿戴物不涉（项目 mob 不穿装备 = SURV-1 同款边界）⑨ 死亡保留背包（项目 keep-inventory 语义，无死亡掉落——Q 丢/破碎路径 damage 随堆已覆盖）。`[SURV-3]`
- **`[SURV-4]` 药水饮用（6 单效果药水 + 玻璃瓶/水瓶循环 + instant_hp 瞬时效果原语）** — 物品侧：`drink_time 1.6`（既有 P43 通道）+ `returns_item glass_bottle`（既有 P43 返还）+ `grants_effects`（**SURV-4 从 food-only 放开到 drinkable**：喝完按序施加 = `applyPlayerEffect` 收口【RESET 语义既有，再喝同效果重置不叠加】；**无 grants 的 drinkable 保持清负面原行为位级不变**）；`glass_bottle`（`collects_fluid ["water"]`，桶对 P47 范式，init 唯一性键 = (fluid, poursBecomes) 与既有水桶共存）/ `water_bottle`（`pours_fluid` 回 glass_bottle，**纯容器不可饮**）；6 药水（weakness 90 / speed 180 / regeneration 45 / poison 45 / healing 0 / harming 0，max_stack 1）；**效果侧新字段 `instant_hp`**（整数非 0、[-32,32]、与 `tick_type` 互斥、显式 0 拒；施加时结算一次：正 = `heal` 钳 base max / 负 = `damage(bypassArmor=true)` = 护甲豁免、Resistance 照常）；grants 条目 `duration==0` ⇔ 效果 `instantHp!=0`（init 双向点名）；内容发放 = hotbar.json `inventory` 隐藏 starter 槽 17-23（GEN-2 范式，无输入零渲染）；texture：effects.png 128→160 尾部 append（既有 8 tile 逐像素零变化验证）+ 8 物品 icon 独立 PNG。**范围外（记档偏离）**：① **无酿造台/酿造线**——药水 = starter 发放（GEN-2 树苗先例）；无 II 级/延长/强化/泼溅/滞留/多效果瓶 ② **water_bottle 不可饮用**——项目 drinkable = 清负面（milk 语义），无"中性饮用"（避免新字段）；MC 水瓶 = 中性喝 ③ **瓶舀水移除水源**——项目桶对范式（P47 既有语义：舀 = 删 L8 源）；MC 瓶填 = 工作台 3 瓶/桶不耗源 ④ **瞬时伤害护甲豁免**——MC 药水伤害语义（伤害调整表 Potion = Armor No effect / Resistance Applies；管线 = SURV-2 溺水同款 bypassArmor）⑤ **再喝同效果 = 重置**——项目 `StatusEffects::apply` 语义（MC = 保较长者；单等级药水下无实际差）⑥ 瞬时效果 icon 闪现 ≤1 帧（duration 0 下一帧 update 移除，截图不可观测，非缺陷）。`[SURV-4]`

## 饥饿 / 食物（Phase 23，Phase 28，Phase 31）

- **饱和度是简化模型，非 MC 的 exhaustion 机制** — Phase 28 实现了隐藏
  饱和度缓冲（上限 = `maxHunger`，存档格式 5 `player.saturation`，format 1-4
  读档 = 0）：每 36s 衰减 tick（Phase 31 对齐 MC 被动节奏）**先扣 1 点饱和度、
  耗尽才掉饥饿条**；食物饱食度 **`[SURV-9]` 已解决：逐食物 `saturation`
  数据字段**（严格 number [0,20]、配对纪律 = 须有 restore_hunger、缺省
  = 旧 2× 规则位级复现，`foodSatiety` 纯函数）——MC 精确值 10 食物全
  写（rotten 0.8 / 生 beef 1.8 / mutton 1.2 / chicken 1.2 = **生肉低饱
  和度即玩法点** / 熟 beef 12.8 / mutton 7.2 / chicken 7.2 / bread 6.0 /
  apple 2.4 / golden 9.0）+ **`[SURV-9]` 已解决：回复 HP 消耗饱食度**
  （每半心 1 点，标准路径）。仍未建模：MC 的 exhaustion 子点（衰减节奏
  为单一固定速度——`player.hunger_decay_seconds` 可配置，Phase 32——饱和
  只推迟掉条；跳跃/攻击额外耗饥同款条款）。`[Phase 28]`（逐食物饱和 +
  回复成本 `[SURV-9]`）
- **进食通道无进度条、无手部动画** — Phase 28 进食 = 按住右键 1.6s（MC 32 tick
  时长）的通道（`core/food.hpp` 纯状态机，real dt）：唯一视觉提示是**手持物品
  图标向屏幕中心抬起**（进度 0→1、满进度 64px，复用弓拉取几何）；无 MC 的 3D
  手部举到嘴边动画 / 进食进度条。中断 = 取消且不消耗食物：提前松开右键 /
  受击 / 死亡 / 解锁鼠标 / 开面板 / 切换选中格（Tab）/ 左键挥击；进食中再按
  右键 = 忽略（进食继续）。浮点边界：96×(1/60) 累加略短于 double 1.6，完成帧
  落在第 97 帧（1/60 累加约定，见本节末条）。`[Phase 28]`
- **饥饿消耗：只有冲刺 ×2，走动/跳跃/攻击不额外耗** — Phase 28 起**疾跑**
  （`Shift`+移动、非水中——与选 `sprintSpeed` 同一输入条件，含贴墙冲刺）把
  36s 衰减节奏变 18s（MC 36s→18s 的相对节奏，Phase 31）；MC 的跳跃（+0.05/tick 级）与
  攻击（+0.1 级）额外耗饥未建模，纯走动与 MC 一致不额外耗。`[Phase 23/28]`
- **满饥饿时吃 = no-op 但吞掉右键** — 选中食物且饥饿已满时，右键既不进食也不
  放置/交互（对齐 MC "满条时不能放置"）；该次点击被静默消耗。`[Phase 23]`
- **食物不直接回血** — `restore_hunger` 只加饥饿点；HP 恢复只走（门控后的）
  自然回复与饥饿=0 的扣血，无"吃牛排直接 +4 HP"类食物。`[Phase 23]`
- **食物共 10 种：4 生肉/腐肉 + 3 熟肉 + 面包/苹果/金苹果** — Phase 28 起：`rotten_flesh`（+4，zombie）、
  `beef`（+3，cow）、`mutton`（+2，sheep）、`chicken`（+2，chicken）；
  Phase 40 起：`cooked_beef`（+8）、`cooked_mutton`（+6）、`cooked_chicken`（+6）
  —— 由炉子熔炼对应生肉得到（MC 数值：牛排 8 / 熟羊肉 6 / 熟鸡肉 6）；
  **`bread`（+5，shapeless 3 wheat，`[Phase 48]`）**、**`apple`（+4，leaves 1/128 概率掉，`[Phase 49]`）**、
  **`golden_apple`（+4，shaped 镶金，`[Phase 49]`；`[Phase 56]` 起 grants_effects = regeneration 10 s + absorption 5 s）**。
  `leather`/`wool`/`feather` 刻意**不是**食物（对齐 MC 的收集品定位）。
  `[Phase 23/28]`（熟肉 `[Phase 40]`、面包 `[Phase 48]`、苹果/金苹果 `[Phase 49]`、金苹果效果 `[Phase 56]`、牛奶桶 `[Phase 43]` 已解决）
- **饥饿 HUD 在极窄窗口会被裁剪** — 心带居中（178px）+ 饥饿带在心带右侧
  （+6px 间距、178px），合计约 362px；窗口宽度 < ~380px 时饥饿带右端超出
  屏幕被裁剪（UI 是像素正交、越界即裁，无回退布局）。E2E/基线窗口 1280×720
  不受影响。`[Phase 23]`
- **衰减/饥饿计时边界在浮点累加下落在第 2161/1200/1080 帧** — 累加器每帧加
  double `1/60`：2160 帧之和略小于 36.0，首个 36s 衰减边界落在第 2161 帧
  （t≈36.017s），其后每 2160 帧一个边界；1200 帧之和略大于 20.0，20s 饥饿
  伤害边界落在第 1200 帧（t=20.000s）；疾跑 18s 边界落在第 1080 帧；单测与
  E2E 的断言均按此留整帧余量（见 `test_player.cpp` 注释）。`[Phase 23]`
  （Phase 31 节奏对齐 MC 后由 4s/帧 241 移至此）

## 工作台 / 形状合成（Phase 17）

- ~~**全工作台的网格是同一个容器**~~ — Phase 17 的 v1：`Game` 持有单个 `m_tableGrid`（`CraftingGrid`）：右键打开的**任何**工作台的 3×3 网格都是它（存档 format 3 `table_grid`）。`[Phase 17]`，**`[CRAFT-1]` 已修复**：每工作台方块独立容器（`Game` 持 `std::map<BlockPos, CraftingGrid> m_tables`，位置序 → 存档写序确定；`setBlock` 收口创建/销毁 + 读档 parse 时不变量保证完整性【炉子同款，无 rescan】）+ 右键面板绑定所点方块（`InventoryUI::bind`，`FurnaceUI::bind` 范式——面板 UI 零视觉变化，只换数据源）+ 存档 format 19 `tables`（每工作台块一条目 {pos, slots[9]}、位置序、与方块差集**双向**交叉校验、19 必需空数组合法 / 19 携 `table_grid` 拒 / ≤18 携 `tables` 拒；**18→19 迁移 = sole 归属**：旧全局 `table_grid` 非空且世界恰 1 个工作台 → 归属之，0 或多个 → 丢弃【P44 先例】；`inv_grid`（E 面板）不动）。**记档偏离：破坏工作台丢弃容器内物品**（setBlock 收口删 state，只掉工作台自身 `drops` = 炉子破坏同款语义 = MC 规则）；**格式 19 槽不携 `damage`**（合成网格属 SURV-3 三处范围外的 grid 纪律，受损物品入格读档回满 = 同款 quirk）。`[CRAFT-1]`
- **工作台面板配方提示** — 表格面板 3×3 网格 + 输出格旁有 **shaped 配方提示列**（`[CRAFT-4]` 已解决）：33 个 shipped shaped 配方两列排布，每行 = mini 3×3 pattern（暗格 + 键位物品图标，按定义左上对齐 = 合法摆放）+ 输出图标 + "name xN"（背包可供应则白 / 否则暗 = `canSupply(patternBag)` 纯函数）；**提示行点击 = 消费性 no-op**（不合成不动背包，但吞点击 ≠ 槽外点击丢 carry）。E 面板位级不变（shapeless 列表保持）。`[Phase 17]`（提示 `[CRAFT-4]`）
- **E 面板 2×2 固定在共享 3×3 模型的左上** — E 面板合成格与表格面板共用同一个 `CraftingGrid` 3×3 模型，只暴露槽 {0,1,3,4}（左上 2×2）；宽度或高度为 3 的 pattern（如 pickaxe `PPP/.S./.S.`）无法摆入，E 里的 shaped 匹配只可能命中落在左上角的 pattern（2×2 工作台配方可在 E 摆出合成，与 MC 一致）。`[Phase 17]`
- **E 配方列表只显示 shapeless 配方** — shaped 配方不出现在 E 列表里（既有设计 = "shaped 在网格里摆"，2×2 网格摆不下的 pattern 占多数）；shaped 摆法的**游戏内指引 = 工作台面板的提示列**（`[CRAFT-4]`：mini pattern + 可供应着色）。`[Phase 17]`（指引 `[CRAFT-4]`）
- **E2E 合成输入的环境约束（本机 :0）** — xdotool 每次调用 ~150 ms（长序列追不上帧帽截图）；XTest 伪事件**每个动作后必须 `XFlush`**（libX11 会缓冲无应答请求成 ~1s 一批投递，同帧事件被 `wasPressed` 边沿逻辑折叠）；合成输入要求点击位置处游戏窗口在最上层（运行时临时 unmap UI 窗口），但 unmap 期间用户的真实鼠标会污染运行（指针锁定期间的真实 motion = 相机旋转），所以运行前须等光标静止 ~400 ms；地面命中格瞄准容差在 pitch −0.80 时仅 ~±10 px，驱动用 pitch −0.68（~±49 px）。Xephyr :99 仅作逻辑回退（:0 光标扫过 Xephyr 窗口会被转发进 :99；llvmpipe 渲染不能用于 base 字节基线）。`[Phase 17]`
- **指针锁定 grab delta（输入驱动 E2E 的环境约束，本机 :0）** — xdotool
  `mousemove`（XWarpPointer）只移虚拟光标、不移物理鼠标；游戏进 relative mode
  时 SDL 把光标 warp 到其自算窗口中心并上报一次性 motion delta ≈（warp 位置 −
  SDL 中心）+ 物理微漂移 → 锁定时首帧相机旋转一次（0.0025 rad/px，4 px =
  ±0.01 rad）。实测：warp 位置 1:1 传导（+4 px warp → +4 px 漂移）；同一 warp
  位 batch 首次 0 px、其后稳定 4 px（手静止可复现 = 物理微漂移）；WM 实际窗口
  原点 (4,64)（windowmove 0 60 不被尊重）→ SDL 中心 (636,416) 而非名义
  (644,424)。对策 = warp 光标到实测中心（origin+(632,352)）+ 背靠背同 warp +
  手静止；**存档 yaw/pitch 是漂移探测器**（干净 = 0.0000，漂移 = ±0.0100）。
  **输入驱动 E2E 的全帧字节一致受此环境物理限制：存档状态（含 yaw/pitch）是
  输入 E2E 的确定性权威证据，全帧字节一致用于无输入回归。** `[Phase 28]`

## 炉子 / 熔炼（Phase 40）

- ~~**炉子状态是单个全局容器**~~ — Phase 40 的 v1：`Game` 持有单个
  `FurnaceState`（input/fuel/output + 两个整数子步计数器），右键打开的**任何**
  炉子看到的是同一份熔炼状态（存档 format 6 `furnace`）。`[Phase 40]`，
  `[Phase 44]` 已修复：每炉子方块独立状态（`Game` 持
  `std::map<BlockPos, FurnaceState>`，位置序 → 逐帧 tick 确定；`setBlock`
  收口创建/销毁）+ 右键面板绑定所点方块（`FurnaceUI::bind`）+ 存档
  format 7 `furnaces`（每炉子块一条目、位置排序、与方块差集**双向**交叉
  校验）。format 6 加载**按规则迁移**：世界恰有 1 个炉子方块 → 全局状态
  归属它；0 个或多个 → 归属不可知，**丢弃**（所有炉子状态空载）——迁移在
  纯解析器（save.cpp）内完成，单测覆盖三分支。
- **炉子外观四个侧面共用同一纹理** — 带炉口/余火的前脸与其余三侧同纹理
  （BlockModel 只有 top/side/bottom 三面组）；顶/底面是石质纹理。
  `[Phase 40]`
- **炉子表现** — v1 的视觉提示只有面板里的进度条（`progressSubsteps /
  smeltSubsteps` 填充）；**熔炼完成音效 + 炉口火焰 `[CRAFT-5]` 已解决**
  （"smelt" ding 新 wav 空间化于炉块 + 帧锁定 info 日志；复燃 = 既有
  "ignite" 声；火焰 = 燃烧中炉子每 36 帧自炉块上方一格发射 P66 余烬批
  【专用盐、render.furnace_flames 门 off = 位级恒等；36 帧 = 余烬寿命
  2× + 裕量（30 帧撞 60 fps 寿命边界 = PITS 316）】）；**无熔炼经验球**
  = 项目无 XP 系统（矩阵外，保持记档偏离）。`[Phase 40]`
  （表现 `[CRAFT-5]`，经验 = 无 XP 系统）
- **可熔炼/燃料名单小且 wood 不可当燃料** — 数据字段 `smeltable_to` /
  `smelt_time`（(0,100] s，缺省 10）/ `fuel_time`（(0,1000] s）完全数据
  驱动、可扩展；初版仅 3 种肉可熔炼，燃料 = coal 80 s / planks 7.5 s /
  stick 5 s。wood 是隐式 block 物品（无数据文件）故无 `fuel_time`——
  **`[CRAFT-6]` 已解决：block 物品炉子字段 = block 属性继承**（显式同名
  物品文件被注册表拒绝 = "block items are defined by their block"，block
  属性是唯一数据驱动路线）：block 可选属性 `fuel_time`（(0,1000]，显式 0
  点名拒 = item 侧同款纪律）+ `smeltable_to`（显式空串点名拒 = ammo 纪律；
  产物存在性 = 既有 game init 全物品遍历自动覆盖 block 物品），
  `buildBlockItems` 继承进合成 item；内容 = **wood 20 s 燃料**（项目木族
  刻度：planks 7.5 / stick 5，coal 80 = MC 精确锚，20 s ≈ 2.7× 单 planks =
  MC log-vs-plank 比值方向）+ **sand → glass 熔炼**（MC 10 s 缺省）+
  **lava_bucket 1000 s**（MC 精确 = 区间上限）；炉子 tick 零改动（读 item
  字段从不读 block id = 硬规则 3；非燃料 burn 0 永不消耗 = 既有语义）。
  `[Phase 40]`（→ `[CRAFT-6]`）
- **熔炼计时是每帧固定 1/60 子步 + 整数倒计时（非墙钟）** — 与昼夜同类的
  世界状态：无输入运行逐字节可复现（存档 progress/fuel 是整数子步）；
  代价是非 60fps 下熔炼随帧钟变速（同昼夜条目）。点火 tick 授予整段燃烧
  时间且该 tick 同时推进一格进度（每根燃料实际多烧 1/60 s，刻意、确定性）。
  `[Phase 40]`

## 染色（Phase 41）

- ~~**染料只有部分颜色有 MC 同款来源**~~ — Phase 41 的 v1：白名单只有
  9 色（白/黑/棕/红/黄/灰/橙/粉/银），无蓝/绿/紫；补色需扩白名单 + 全
  dye 家族变体块 + 纹理。`[Phase 41]`，`[Phase 45]` 已修复：补全缺的
  6 色（蓝/绿/紫/品红/浅蓝/青，项目 silver 位 = MC light_gray → 15 染料
  全），**颜色词汇表纯数据**（dye item 的 `dye_color` 命名，加颜色零代码
  改动）；MC 同款来源全部进世界：`lapis_ore` 矿脉（蓝←lapis）、`cactus`
  实心块（绿←cactus）、`allium`/`tulip`/`cornflower` 十字植物（紫/品红/
  浅蓝）、青←lapis+绿（MC 1.14+ 配方）；来源块走 biome `decorations`
  表面装饰（沙漠仙人掌、草原/森林花）+ `ores.veins` 矿脉（lapis）。
- **同色重复染色不消耗染料** — 对已是目标颜色的方块再右键同色染料 = no-op
  （点击落到 placeBlock，染料非方块物品 = 无操作）；Minecraft 会消耗 1 个。
  v1 规则，文档化（MECHANICS「染色」）。`[Phase 41]`
- **sheep 只掉白 wool** — 无染色 sheep（MC 有 16 色羊）；彩色 wool 只能靠
  染色获得。`[Phase 41]`
- **wool 物品图标 = 3D 方块体** — `wool` 块与既有显式 material 物品同名，
  冲突解法是删显式物品、用隐式 block item（hotbar 渲染 3D 方块体而非 2D
  图标）；与 MC 的 2D 羊毛图标不同。`[Phase 41]`

## 牛奶桶 / 饮用（Phase 43）

- ~~**状态效果系统不存在：喝奶效果集 = 清红闪一项**~~ — `[Phase 46]` 已修复：
  数据驱动状态效果系统（poison/weakness/regeneration，见下「状态效果」
  章节）；喝奶完成 = 清红闪 + **清除全部负面效果**（MC 语义，正面再生
  保留）。坠落伤害起点（`fallStartY`）仍**刻意不清**——MC 坠落伤害不是
  状态效果，空中喝奶不减坠落伤害。`[Phase 43]`
- **喝奶不恢复饥饿** — MC 牛奶饥饿值 = 0：饮用通道只做「消耗 + 返还容器
  + 清效果」，不走 `restoreHunger` 路径；`drink_time` 与 `restore_hunger`
  互斥（同一右键按住通道，一个物品不能既是食物又是可饮用）。`[Phase 43]`
- **挤奶** — 持 `milk_with` 物品右键可挤 mob（cow）= 1 桶 → 1 奶桶
  （瞬时）：cow 不受伤害、不死、不掉落；**5 分钟挤奶冷却 `[SURV-10]`
  已解决**（`kMilkingCooldownSeconds 300` × 60 帧计数，per-mob 帧锁定
  【mob 步长 1/60 = 确定性】，冷却内再挤 = no-op 点击照吞【failed-milk
  同款】；mob ephemeral = 冷却从不入档【MC 同款：读档后 mob 重生】）；
  背包满时不产出但点击被吞（MC 一致）。挤奶音效与饮用开始共用 "milk"。
  `[Phase 43]`（冷却 `[SURV-10]`）
- **唯一可饮用物品 = 牛奶桶** — 水/熔岩桶（`[Phase 47/51]`）是**流体容器**
  （`collects_fluid`/`pours_fluid` 舀倒，见「流体桶」节），**不可饮用**（无 `drink_time`，与 MC 一致——原"无水瓶/熔岩桶（需流体物品设计，超范围）"注记已被消费）；
  `returns_item` 容器框架 + `drink_time` 数据字段已支持未来可饮容器
  （无容器 = 喝完直接消失，`returns_item` 可选）。`[Phase 43]`（水桶 `[Phase 47]`、熔岩桶 `[Phase 51]` 已就位 = 流体容器非饮品）
- **"milk" 音效在饮用开始时播放** — 与 MC 一致（咕嘟声在前，即使提前松开
  取消饮用也响过；同弓弦声在拉弓开始时播放的纪律）。`[Phase 43]`

## Mob（Phase 20）

- **mob 不入存档** — 生成物是瞬态的（与 Minecraft 一致：存档只持久化玩家/物品/方块编辑）；读档后 mob 重新按帧节奏生成。zombie 死亡掉落 `rotten_flesh` 是物品实体，随存档正常保存。`[Phase 20]`
- ~~**无 Peaceful 难度**~~ — `[ENT-8]` 已解决：`mobs.difficulty = "peaceful"` 四值词汇表（peaceful = Phase-25 保护环扩到全球 kSpawnProtectionPeaceful 1e6 → 敌对自然生成全拒，被动豁免照生，MC 和平核心语义）；"现有敌对消亡" 退化 = mobs ephemeral + difficulty 只 init 读取（无运行期切换）→ 读档后本无敌对（偏离记档，详见 phase-ent8.md）。`[Phase 25]`
- **保护环是水平圆、不跟随玩家** — 保护中心 = 新世界出生点（读档不改变它），半径为水平距离（无垂直分量）；v1 无床/村庄，保护不随玩家移动（Minecraft 的村庄保护环未实现）。`[Phase 25]`
- ~~**生成率缩放为全局节奏**~~ — 难度缩放的是尝试节奏（全部 mob 类型共享的 `check_every_frames`），不是 Minecraft 的按类型生成权重；easy/normal/hard = 30÷0.5/1.0/1.5 帧。`[Phase 25]` 发现，`[Phase 39]` 已修复：`mob_spawning.json` 的 `mobs` 条目改为 `{"id", "weight"?}` 对象（weight 1..10000，缺省 1；旧纯字符串条目显式拒绝 = 迁移防护；重复 id/越界/非整数 = 加载失败），类型抽取改为「每次 1 次流值映射进 [0, Σweight) 后按累计权重走名单序」——全 weight 1 与旧 `pick % 名单长度` 逐位相同（已发行数据零行为变化）。难度节奏缩放保留（与权重正交：节奏 = 多久尝试，权重 = 尝试时抽中哪种）。
- **无肢体动画/阴影/咆哮声** — 静态 box 模型（头+身两箱、单纹理六面同 tile、yaw 旋转朝向）；无行走摆臂、无 blob 阴影；受击复用 "hurt"、死亡复用 "death"，无 zombie 低吼。`[Phase 20]`
- ~~**mob 无 step-up**~~ — 1 格台阶会挡住追逐（贴墙滑动、可能永远到不了玩家）；玩家有 P16 step-up，mob 没有。`[Phase 20]`（追逐卡台阶）**`[ENT-3]` 已修复**：step-up 纯几何上提为 `Entity::stepUp`（player 逐位不变 = 既有 stepUp 单测零改动全过），mob 在水平移动受阻的同一位置、同一判定（P99 TRUE-1-block：solid 区顶高 ≤ 1.0 → slab/满块/玻璃板可踏、墙/栅栏 1.5 帽不可踏）调用同款抬升 + 本 tick 水平位移重放；AI/速度/子步节奏零改动、零 format（mob ephemeral）。**共享判定顺带钉死 P16 两个潜伏 quirk**（player 同款、ENT-3 探针实证，详见「生存」step-up 条）：+X/+Z 方向 front 格查 `snapped+1`（-X/-Z 查 `snapped-1` = 贴住的格），**恰 1 格宽的台阶在 +X/+Z 方向踏不过**（≥2 格宽或 -X/-Z 方向正常）；非整数脚高行走时 face 巧合对齐整数可触发凭空抬升 1.0 再落回。修复会改 player 行为，单独立项。
- ~~**mob 无游泳/水交互**~~ — `[ENT-4]` 已解决：走进水 = 共享流体模型（浮力 12 / 上浮帽 1.5 / 下沉帽 4 = 玩家 FLUID-2 同常量）——沉水被动上浮、水面 W' 几何漂浮（fps 无关终态）、池底站立、水平速度 ×0.6 门控、水下禁 step-up（**FLUID-3 修订**：`!m_inFluid` 门已删 = 涉水 mob 可踏 1 格岸壁出池；浮漂终态 = 游泳位 W'−h/2 半浸没；池深 ≥2 的 resting 体上浮；详见 phase-fluid3.md）；AI 状态机零改动（追逐/游走照常驱动，只是水下更慢）；熔岩同路（P58 燃烧照旧）。生成检查（地面格 + 头顶空余）不变。`[Phase 20]`
- ~~**mob 无坠落伤害**~~ — `[ENT-5]` 已解决：MobEntity 落地块镜像玩家 Phase-16 规则（净落差 > 3 且落地格 fresh 无流体 → damage，详见 phase-ent5.md）。`[Phase 20]`
- **无 mob 间仇恨** — 每只 mob 只以玩家为对象（无群袭/目标切换/视野遮挡检查）；玩家不可被 mob 推动（P19 玩家不可推动规则，mob 在玩家脸前停下）。`[Phase 20]`（`[ENT-6]` 起原"追墙会沿墙滑"已消费 = 卡住触发 BFS 绕障寻路，chase 态专属——wander/ranged 不寻路 + 楼梯/半块地板出图 + 单格 step quirk 几何下保持夹墙【节点超时逃逸】，详见 phase-ent6.md）
- **攻击是固定 0.25s 挥击节奏，无挥击动画/声音差异** — 按住左键对 mob 每 0.25s 一击（伤害 = 选中物品 `attack_damage` 数据）；无 swing 动画、命中音复用 "hurt"。`[Phase 20]`
- **mob 模拟走固定帧步长（`kMobStepPerFrame`）** — 运动实体的截图可复现约定（日循环同款）：mob 每帧固定推进 1/60s，非 60FPS 帧帽下 mob 时间随帧钟变慢（玩家/掉落物/流体仍是真实 dt）；掉帧时 mob 与玩家瞬时速度差 ≤1 个子步（无输入运行无感）。`[Phase 20]`
- **类型抽取先于光门，过不了光门的尝试作废** — 每次生成尝试先按名单确定性抽取类型（种子 ⊕ 帧号）再找格：白天抽中 zombie（需光 ≤0）则本次无格可用、整次作废，相当于该类型在不可生成时段的生成率减半；v1 接受（名单 2 种，影响有限）。`[Phase 22]`
- **被动 mob 无主动躲避** — cow 对玩家零反应（不逃跑 AI）：玩家靠近不触发任何行为；被打只有击退（0.3s 窗口后恢复游走），无持续远离玩家的逃跑速度。`[Phase 22]`
- ~~**掉落数量是固定列表，无随机区间**~~ — Phase 28 起掉落数据 = `drops` 列表
  （每条目 `{item, count}`，count 1..64 固定）；`[Phase 38]` 已加随机区间：条目可用
  `count_min`/`count_max`（0..64、min ≤ max、必须成对、不得与 `count` 混用），死亡时按列表顺序
  用该 mob 的种子流掷一次（0 = 该条目不掉）；skeleton 现掉 `bone` 0–2 + `arrow`，cow 仍固定 1
  `leather` + 1 `beef`（数据选择，非 schema 缺口）；无掉落幸运/附魔影响；`leather` 目前无
  合成用途（纯收集/交换物）。`[Phase 22/28]`（随机区间 `[Phase 38]`）
- ~~**skeleton 无视线检查**~~ — `[Phase 38]` 已修复：射击回调先过 `hasLineOfSight`（纯 raycast 函数：眼→玩家眼被非空气格挡住 = blocked），blocked 则不发（箭反正只会插墙消失），冷却照常消耗、下个间隔重试（Minecraft 行为）+ debug log。E2E：存档手工 96 格石墙（两 skeleton与玩家之间）→ blocked=6 / shot=0 / health 20 / 两 skeleton 照常生成。
- ~~**skeleton 掉落固定 1 个 `arrow`**~~ — `[Phase 38]` 已修复：新 material 物品 `bone`（图标 = 白色对角骨，generate_item_icons.py 新 salt 63，既有 17 图标零变化）+ skeleton `drops` = bone 0–2（随机区间）+ `arrow` 1。掷落用该 mob 自己的种子流（`rollDropCount`；固定 count 不推进流 → P38 前的精确流保留）；E2E 输入击杀 → 存档 entities = arrow×1 + bone×2。（Minecraft skeleton 还掉 0–2 箭；本项目箭保持固定 1。）
- ~~**skeleton 箭提前量只补偿重力、不预判位移**~~ — `[Phase 38]` 已修复：`computeAimDirection`（projectile.hpp 自由函数）——“目标位置 + 速度×飞行时间”迭代 2 次 + 0.5·g·t² 弧线提前量；静止目标（速度 0）与 Phase 27 瞄法逐位相同（回归锚：E2E 射击场景 P37/P38 存档逐字节一致），移动目标被预判（单测：侧向/逃离目标最近接近距离严格小于无预判）——细节见 `archive/phase-38.md`。

- **spider 无掉落** — MC 的 spider 掉 string/spider_eye 0–2，两物品未做
  （string 无用途、spider_eye 无用途）；`drops: []`。spider 是首个
  `on_hit_effect` mob（poison 15 s，近战命中落定施加，见「状态效果」）。
  `[Phase 46]`

## 弓 / 箭（Phase 21 + Phase 24 + Phase 42）

- **箭不入存档** — 瞬态实体（同 mob）：保存时跳过（存档守卫对已知瞬态种类跳过、其他未知种类仍报错），读档后飞行中/插墙上的箭消失。`[Phase 21]`
- **玩家自己的箭不自伤** — 玩家的弓箭不带 `hurtPlayer` 标志，扫掠测试不碰玩家（无自伤，单人生存 v1；Minecraft 同样不自伤）。**mob 射出的箭**（skeleton，Phase 27）带标志、扫掠命中玩家 AABB 即走 P16 受伤路径；箭的 `source` 指针使命中循环跳过射手自身（spawn 点在射手 AABB 内，不跳过则每支箭下一帧 t=0 自命中自灭）。`[Phase 21/27]`
- **箭无纹理 / 无箭头 / 无尾羽** — 单一棕色细盒（长 0.55 × 粗 0.05），无箭头/尾羽几何、无 atlas 箭纹理（纯色 `u_useTexture=0` 路径）。`[Phase 21]`
- ~~**插墙箭寿命 = 30s**~~ — 与飞行箭共用 `kMaxAge`（Minecraft 插墙箭 100s）；插住的方块被破坏/替换时箭留在原处（不重新碰撞、不掉落）。`[Phase 24]`，`[Phase 42]` 已修复：插墙箭独立寿命 `kStuckMaxAge = 100s`（飞行箭仍 30s；age 累计、按**当前**状态取上限——插墙 >30s 后释放的箭立即消亡，MC 分歧已文档化）+ `Game::setBlock` 收口（破坏/放置/染色换块都走它）释放卡住格内的插墙箭（碰撞盒**触面**判定：盒触及面且中心在面外，非重叠判定——resolver 精确 clamp 到面上，盒不进入块内）。
- ~~**拉弓 = 2D 手持图标位移**~~ — 拉弓时右下角手持图标按蓄力威力向屏幕中心拉（满蓄力 64px）；无 3D 手部/弓弦模型、无拉弓进度条（Minecraft 有手部动画 + 弓弦后拉）。`[Phase 24]`，`[Phase 42]` 已修复：3D 手持手部（`drawHeldHand`）——指针锁定且无面板时选中的物品渲染为视口锚定的 3D 模型（块物品 = 掉落物同款 3D 方块体 + atlas 光照；图标物品 = 2D 图标在 3D 平面，恒亮），拉弓时手部随蓄力后拉 + 弓体后仰（拉弓指示），进食时随进度抬向面部（进食指示）；2D 手持图标改固定（P24/28 的 2D 拉移删除）。仅锁定时渲染 → 无输入基线结构性免疫（无重锚）。无弓弦模型/拉弓进度条（仍无）。
- **非块 3D 手部 = 平面图标 billboard** — 图标物品的手部 = 2D 图标贴在 3D 平面上（无 per-item 3D 模型、无手部皮肤/弓弦几何）；无图标的物品没有 3D 手部（v1）。`[Phase 42]`
- **插墙箭拾取 = 返还发射器弹药** — 插墙箭距玩家**底心** ≤ 1.0 块即拾取，返还 1 个发射器的 `ammo` 物品（飞行箭不可拾取；mob 箭返还 mob 数据的 `ammo`，ranged mob 必填 + init 校验物品存在）；背包满 = 箭留墙内、每帧重试（v1，MC 语义接近）。箭不落地成掉落物（拾取直接进库存）。`[Phase 42]`
- **释放箭共享累计寿命** — 释放的箭（卡住格被破坏/替换）以零速度落回飞行物理（重力 + 碰撞 + 可再命中 mob），寿命上限切回 30s 飞行上限、age 不重置（插墙 >30s 的箭释放即消亡——与 MC 的总寿命语义有分歧，已文档化）。`[Phase 42]`
- **弹药消耗 = 槽序升序第一个持有槽** — 发射消耗 1 个 `ammo` 物品，取自槽序升序第一个持有它的槽位（hotbar 先于背包）；Minecraft 同样全库存取箭（语义接近）；无 `ammo` 字段的发射器 = 无限箭（Phase 21 前语义）。`[Phase 24]`
- **弓强化 = 独立物品而非附魔** — 项目无附魔系统：MC 的弓附魔 = 独立物品——`infinite_bow`（Phase 77，字段 `infinite`，shapeless bow + diamond → 1，仍须持有 ≥1 支箭 = MC 同款规则）+ **`power_bow`（Phase 79：`shoot_damage` 8 = 弓的 5 +3，shapeless bow + 2 diamond）** + **`speed_bow`（Phase 79：`shoot_speed` 20 = 弓的 15 +5，shapeless bow + gold_ingot）**——力量/速度复用发射器既有数据字段（无新字段）；无附魔等级/概率；**组合弓 = `master_bow`（Phase 82：力量+速度+无限三叠加，shapeless power_bow + speed_bow + infinite_bow → 1 = MC 附魔台的合成替身，给三把强化弓一个用途）**；穿透/火焰 = 无。`[Phase 77]` → `[Phase 79]` → `[Phase 82]`
- **发射器右键恒为拉弓** — 选中发射器时右键不交互工作台/不放置/不打开任何 GUI（Minecraft 弓语义）；无弹药时该次右键被吞（不能起拉、不放置）。`[Phase 24]`

## 状态效果（Phase 46）

- **weakness 无游戏内来源** — MC 来源是药水（未做）；v1 来源仅 mob
  `on_hit_effect`（当前 spider = poison 15 s）与存档注入（E2E 场景）。
  regeneration 已有游戏内来源（金苹果，Phase 49）。`[Phase 46]`（再生
  部分 `[Phase 49]` 已解决）
- **中毒无独立紫色 tint** — 毒 tick 复用玩家正常受伤路径（红纱 +
  "hurt" 音）；MC 的紫色屏幕 tint 未实现（需新的全屏 tint 通道）。
  `[Phase 46]`
- **tick 相位不存** — 存档只写 `{name, remaining}`；重载后下一 tick 从
  整 interval 起算（相位重置，秒级内无感）。`[Phase 46]`
- **效果只作用于玩家（v1）** — mob 不持效果（无凋灵/中毒 mob 等）；
  `on_hit_effect` 仅**近战命中**施加（远程箭不施加）；无 amplifier/
  时长档位（同效果重施加 = 重置 duration，tick 相位保留）。`[Phase 46]`
  **`[SURV-5]` 已演进**：双载体落地——mob 持 StatusEffects（`affects_mobs`
  门 + `undead_exempt`×`undead` 不死族免疫门 + `min_health_mob` 地板；
  Heal tick = mob no-op 记档偏离）+ 远程箭施加（item/mob 双源
  `arrow_effect`：skeleton 箭 wither 10s【玩家侧】/ poison_bow 箭
  poison 60s【mob 侧，可毒死】）。残余：**amplifier 档位 = `[SURV-6]`
  已解决**（效果 JSON `tier_max`/`tier_steps` + 全 grant 对可选 `tier`
  0 基 + format 20 存档 omit-when-0；5 加性修饰字段档位化 = MC
  I/II/III，tick/instant 形单档记档偏离——详见
  phase-surv6.md）；时长档位不变（grant 的 duration 本就是
  数据值，无档位语义）；mob 无回血（heal no-op）；箭击杀 mob 同帧
  不施加（记档）。
- **no-op 效果被注册表拒绝** — 无 tick 且 `melee_penalty` = 0 且
  `absorb_hp` = 0 的效果 JSON 加载失败（纯装饰效果 v1 无消费方）。
  `[Phase 46]`（`absorb_hp` 部分 `[Phase 56]` 更新）
- ~~**吸收层与正常心共用一个 health 值**~~ — `[Phase 72]` 已解决：**两分量模型**——
  `m_health`（0..base max）+ `m_absorption`（0..64）**独立池**；`damage()`
  **先扣池**（池单独不能致死，死亡只来自 health）；`heal()`/`setHealth()`
  只钳制到 base max（heal 永不填池）；池 = 效果 `absorb_hp` 总和的纯函数
  **只在同步点**（进食施加 / mob 命中 / 到期 / 牛奶清 / 死亡清——刷新恢复
  满层 = MC 属性重算），同步点之间伤害扣减的**持久**（扣空后不再自动回满）；
  读档存值权威（不重同步——池可低于效果总和）；HUD 吸收红心从**池**派生
  （池 0 = 零绘制，无输入结构性免疫保持）；**format 12** `player.absorption`
  （键纪律 + [0,64] + ≤ 效果总和不变量 + format ≤11 共享 health 迁移）。
  剩余边界见下条。`[Phase 56]` → `[Phase 72]`
- **吸收两分量模型的边界（文档化偏离）** — 池硬上限 **64**
  （`kAbsorptionMax`，`setAbsorption` 钳制；现役内容唯一来源 = 单个 4-HP
  吸收效果，总和不可能越界——未来若多个 `absorb_hp` 效果总和 > 64，池钳 64）；
  **同步点语义**：池被扣空后，任何同步点（含效果刷新——再吃金苹果）把池
  恢复满层（MC 属性重算同款）；**读档存值权威**（池 < 效果总和合法 =
  伤害在同步点间持续过，MC 语义）；**format ≤11 迁移**按 base max 拆共享
  health（超出部分 → 池，如 22 → 20+2——旧档无字段说明哪部分是层，迁移
  不损伤害）。`[Phase 72]`
- **效果图标行与吸收红心可视觉重叠** — 图标行左对齐 hotbar 左缘、吸收
  红心带在正常心带左侧（MC 布局）；同时激活 **≥3 个**效果时图标行会
  画到吸收红心区（现役内容可触发：spider 毒 + 金苹果 = 毒/再生/吸收
  3 个）。纯视觉（两者都可读性尚可），默认 20+4 几何下 2 个效果不
  重叠。`[Phase 56]`
- **燃烧无独立音效/火焰 tint** — fire 效果的 tick 走玩家正常受伤路径
  （P46 模式，与中毒同款）：每 0.2 s 一次 "hurt" 音 + 红纱（每秒 5
  次，无独立火焰噼啪音、无火焰 tint）。机制本身（熔岩点燃 + 离熔岩
  残留 8 s + 水灭火）`[Phase 57]` 已解决，表现层简化留此。`[Phase 57]`
- **火时长走 config 而非效果 JSON** — 点燃源是熔岩流体（代码词汇，无
  数据文件），时长 `player.fire_duration_seconds`（默认 8 = MC Normal）
  走 config 通道（P54 `lava_damage` 同款）；效果 JSON 只带
  `ignites_on_lava`/`extinguished_by_water` 两个数据标志。加第二个
  点燃源（如 MC 的火/岩浆怪）时需再定案。`[Phase 57]`

## 流体桶（Phase 47，Phase 51 双种类更新）

- **只有水/熔岩两种流体** — 流体种类词汇表在代码中（`FluidKind`：
  `water`/`lava`），`slime` 等其它流体名被解析拒绝；新增流体种类需改
  代码（编码区间/词汇表），桶对与反应表本身是数据。`[Phase 47]`
  （Phase 51 起词汇表 = `water` + `lava`，熔岩桶/黑曜石已就位）
- **流动流体不能舀** — 只有 level 8（源）格能舀（MC 一致）；且源格每
  tick 向四向补 level 7，**远处的玩家隔着源头自己的扩散永远舀不
  到**——要舀源必须站旁 1 格从上往下斜视源顶面（MC 同行为；E2E 场景
  的几何见 PITS「流体桶 E2E 视线」）。`[Phase 47]`
- **倒水只能倒向方块受击面** — 放置目标格约定（首个非 air 方块的受击
  面邻格）；对着水面/纯天空倒 = no-op（MC 对水面的倒法未实现）。
  倒出的源走正常 FluidSim（悬空下落、落地扩散），无"倒进容器"概念。
  `[Phase 47]`（**`[FLUID-4]` 已解决**：`bucketPourTarget` 纯函数——
  受击面邻格或（block 射线无命中时）fluid 射线命中的流动格（同种、
  level<8）都可成为倒水目标 = 该格变源；已源格 = no-op 桶保留（MC
  parity）；纯天空无流体命中仍 no-op。**`[FLUID-5]` 异种流体格解决**：
  水桶对熔岩格 = 反应倒（水源放受击法向一步的干空气格，既有 reaction
  pass 下 tick 转 obsidian）/ 熔岩桶对水格 = 替换倒（setFluid 既有替换
  语义，下 tick 按残留邻水转 obsidian/stone））。**`[FLUID-6]` 第三种类 honey 入桶词汇表**（bucket
  collects_fluid 三种类全舀 / honey_bucket 倒蜜 / 蜜桶对水格 = 同款
  替换倒；蜜无波纹、1/3 速、无 worldgen = 桶源））
  `[Phase 47]`
- **水中无游泳物理差异** — 玩家进水体只进入水中状态（重力减半，Phase
  9），无划水/呼吸机制；桶操作在水中同样可用（无水下交互限制）。
  `[Phase 47]`

## 熔岩 / 流体双种类（Phase 51）

- ~~**worldgen 不生成熔岩**~~ — `[Phase 86/88/90/93]` 已解决：worldgen 现生成熔岩湖——shipped `lava_lakes` 3 湖（(40,40) R4 深湖 / (55,45) R3 深湖 / (24,26) R1 表面湖）+ `lava_lakes_procedural` 5 个 seed 派生随机湖（region [60,90]²），详见「熔岩湖（Phase 86）」节。`[Phase 51]`
- ~~**熔岩无燃烧/减速**~~ — `[Phase 54]` 已修复：玩家 AABB 重叠熔岩格时
  进**同款游泳状态**（`isInLava`，水/熔岩并存时熔岩优先）+ 熔岩节奏
  （`lava_swim_speed` 1.9 / 上浮 1.5 / 重力 4 / 终端 2，config 可覆盖）+
  **灼烧伤害**（每 `lava_damage_interval_seconds` 1.0 s 真实 dt 受
  `lava_damage` 4 HP，Minecraft 4 HP/s，走正常伤害路径）+ 落入熔岩取消
  坠落伤害 + 熔岩内抑制疾跑/下蹲速度。Mob 在任一流体中不生成
  （P20 行为不变）。`[Phase 51]`（燃烧/减速 `[Phase 54]` 已解决）
- ~~**脱离熔岩无残留燃烧**~~ — `[Phase 57]` 已解决：`fire` 效果
  （1 HP / 0.2 s、可致死）——在熔岩内**每帧重施加**（duration 重置 =
  熔岩内持续燃烧、tick 相位保留；离熔岩后 countdown `fire_duration_seconds`
  8 s 继续烧至到期），水内**瞬时熄灭**（`extinguished_by_water`；水胜
  熔岩：两流体并存时无火），牛奶不灭火（`negative: false`，MC 语义）。
  表现层简化见"燃烧无独立音效/火焰 tint"条。`[Phase 54]`
  （`[Phase 57]` 已解决）
- ~~**熔岩不烧 Mob 与作物**~~ — `[Phase 58]` 已解决：**Mob 点燃**
  （生成时 `MobEntity::armFire` 从 ignites_on_lava 效果 + config
  `fire_duration_seconds` 取参武装；**整数帧燃烧格**：1 HP / 0.2 s、
  熔岩内每帧重置 duration【相位保留】、离熔岩烧 8 s、水瞬时熄灭且胜
  熔岩）+ **`flamable` 块属性**（wheat_1..7 全 7 阶）+ **FluidSim 燃烧
  pass**（熔岩格 6-邻 flammable 块烧成 air，无掉落，快照确定性）。
  Mob 无火焰视觉等 v1 限制见下三条新增条目。`[Phase 54]`
  （`[Phase 58]` 已解决）
- **Mob 燃烧无火焰视觉** — mob 的燃烧是纯 HP 通道：无火焰贴图/粒子、
  无红闪、无音效、无 HP 条——视觉上完全不可见（玩家燃烧有红闪 +
  "hurt" 音，Phase 57）。Minecraft 的 mob 身上有火焰粒子。
  `[Phase 58]`
- **hostile 光门与熔岩互斥** — hostile `spawn_max_light` 0 对熔岩方块
  光 15（level 无关，L1 膜也是 15）：任何熔岩格 ~15 块内 hostile 永不
  生成 → 无输入运行中 hostile 永远进不了熔岩（池边连生成都不行，且不
  寻路）；mob 点燃在自然场景只可能来自**被动 mob 游荡入**（E2E S2 =
  3×3 L1 熔岩膜 + 被动游荡；S2ctl 水膜对照 = 无光 → hostile 生成杀
  玩家、零点燃，隔离熔岩）。MC 同款几何（hostile 也要黑暗生成），但
  MC 玩家可引怪入熔岩——本项目无输入运行没有这一手。`[Phase 58]`
- ~~**v1 flammable = 小麦 7 阶**~~ — `[Phase 63]` 已扩展：`flamable`
  集 = **wheat_1..7 + wood/leaves/grass**（MC 天然可燃三块加入，纯
  数据 = 两消费者——点燃器 `[Phase 60]` 与熔岩燃烧 pass `[Phase 58]`
  ——立即生效；fresh 世界零熔岩 = 无输入下仍惰性）。仍不燃的 MC 可燃
  物：**羊毛**（文档化偏离：染色产物保留不可燃，见
  Phase63FlamableContent 的精确集断言）、高草/火把（未实现）。
  `[Phase 58]` → `[Phase 63]`
- **ignites 效果的 tick_interval 必须整帧** — init 交叉校验：
  ignites_on_lava 效果的 `tick_interval × 60` 必须是整帧数（容差
  1e-4 帧；float 0.2f×60 = 12.00000018 过、0.33 s = 19.8 帧点名
  拒绝）。mob 整数帧燃烧格依赖此性质；将来需要非整帧 interval 的
  点燃效果时须重新设计格。`[Phase 58]`
- **水+熔岩反应 = v1 最小规则** — 反应表数据驱动在熔岩块上
  （`water_reaction {source, flowing}` 严格配对）：熔岩格 6-邻域任有
  水 → 该熔岩格变方块（邻水含 level 8 源 → 黑曜石，否则石头）。
  **水不消耗**（MC 消耗水源）、**无蒸汽粒子**（MC 有）；每 tick 快照
  确定性。无 `water_reaction` 的惰性熔岩块不反应（水可与其相邻共存）。
  `[Phase 51]`
- **水/熔岩正面争夺同一空格 = apply 顺序决定** — 同一 tick 两种流体都
  提议同一空格时，apply 阶段水先熔岩后 = **熔岩胜出**（setFluid 替换
  语义）；瞬态"熔岩抢水"是确定性实现细节（apply 顺序），非数据规则。
  稳态由反应 pass 保证：无 6-邻接的水-熔岩对（否则反应必触发），
  边界收敛为石头/黑曜石墙。`[Phase 51]`
- ~~**湿润判定把熔岩也算"水"**~~ — `[Phase 54]` 已修复：`cropIsWet` 改
  `getFluidOfKind(..., Water)`——只算**水**（熔岩旁的耕地 = 干节奏；
  MC 熔岩不湿润，且会烧掉作物——烧 `[Phase 58]` 已解决，见"熔岩 /
  流体双种类"节）。
  `[Phase 51]`（`[Phase 54]` 已解决）
- **黑曜石只能靠反应获得** — 无自然生成（worldgen 现生成熔岩湖、反应场所天然存在——如向表面湖倒水，但生成不放黑曜石）、无合成配方（MC 获取方式 = 水浇熔岩，本项目同款反应）。
  `[Phase 51]`（"worldgen 无熔岩"前提 `[Phase 86/88/90/93]` 已失效）

## 信标（Phase 83）

- **信标效果 = 数据驱动效果列表（多效果同信标，"附魔台"替身）** —
  `beacon_effects`（非空数组，shipped = `["absorption", "speed"]`）指向**多个
  效果**，信标同时施加全部 = MC 信标"主 + 副效果"的落地（项目范式 = 叠加
  多效果于同一对象，即附魔台替身；"多效果同弓" = master_bow P82 已做，
  "多效果同信标" = 本条）。`beacon_effect`（单个）= 向后兼容 1 元素列表；两
  字段互斥（都写 = 拒绝）。效果词汇表 = 速度/力量/抗性/吸收（Phase 84 加
  speed/strength/resistance）。`[Phase 83]` → `[Phase 84]` → `[Phase 91]`
- **信标基座 = 仅 iron_block** — `beacon_base` 布尔属性（shipped 只 iron_block
  带）；MC 的 gold/diamond/netherite 块基座 = 无（项目无这些金属块）。金字塔
  等级 = 3×3/5×5/7×7/9×9 连续完整层（`world/beacon` 纯函数）。`[Phase 83]`
- **光束不入光照** — 信标光束是半透明渲染柱（`beacon.beam` 可关），**不提供
  方块光**（MC 光束照亮周围）；信标块本身 `light_emission 0`。`[Phase 83]`
- **信标范围 = Chebyshev 立方** — 玩家脚到信标格的切比雪夫距离 ≤
  `beacon.range_blocks`（默认 50）即生效（MC 是球状范围；立方是项目简化，
  文档化偏离）。`[Phase 83]`

## 台阶 slab（Phase 85）

- **slab = 仅下半块** — v1 只有**底部 slab**（y 0..0.5 实心，顶面站立）；MC
  的顶部 slab / 双 slab 叠合 / 斜向 = 无。`[Phase 85]`
- **slab 邻满块的同层侧面可能 Z-fight / 缺面** — 贪婪 mesh 不能做"半面剔
  除"：满块贴着 slab 时，满块的侧面要么整面绘制（slab 半区经深度测试遮挡，
  半重合面 Z-fight）要么整面剔除（顶部半区露洞）。shipped stone_slab 放在
  空邻居场景（E2E）无此问题；相邻满块是 v1 已知局限。`[Phase 85]`
- **slab 光照按满块不透明** — slab 顶半格是空气（光本应透过），但 v1 按
  `opaque: true` 处理（挡光如满块）；简化偏离。`[Phase 85]`

## 墙 wall（Phase 95）

- **墙 = 1.5 格高柱（第三个新块模型类型）= barrier** — `stone_wall`
  （shape wall，y 0..1.5 实心 = 满块 + 顶 0.5 帽）：玩家**不能穿过**（AABB
  与实心区重叠）也**不能跳过**（跳跃 ~1.25 < 1.5），须绕行或破坏。碰撞
  `solidTopHeight` = 1.5 + **y 扫描扩到下方格**（帽伸进上方格：AABB 在 y+1
  格时须查 y 格的墙帽——`hasRoomAt`/`moveAxis` 的 y0-1 扩展，非墙格 top≤1
  为 no-op）。MC 的墙连接 `[Phase 96]`：**合并墙几何无缝**（墙-墙共享侧面
  整面剔除、墙-满块侧面只绘暴露帽带 y 1..1.5）；MC 式延伸板对全柱墙不适用
  （柱已贴邻块，延伸板在柱体内不可见）= 偏离；栅栏顶/铁栏杆/玻璃板变体 =
  无（项目无 fence）。`[Phase 95]` `[Phase 96]`
- **墙侧面 vs 半块/透明邻居的保守整面** — 墙-墙共享面、墙-满块内部段
  `[Phase 96]` 已剔除；但对 **slab/楼梯** 邻居（只盖 y 0..0.5 / 斜面变化，
  下段有暴露区）与 **玻璃**（透明 pass 混合、无共面争）仍绘整面 y 0..1.5
  = 正确暴露（同 slab v1 保守取舍）。P95 曾记录"墙邻满块 Z-fight"系误判
  （满块面早被贪婪路径对 opaque 墙邻居剔除，无共面双绘）。`[Phase 95]`
  `[Phase 96]`
- **墙光照按满块不透明** — 帽（y 1..1.5）是空气（光本应透过），但 v1 按
  `opaque: true` 处理（挡光如满块）；同 slab 简化偏离。`[Phase 95]`

## 栅栏 fence（Phase 98）

- **栅栏 = 交叉细柱 + 连接横杆（第四个新块模型类型）= 1.5 barrier** —
  `fence`（shape fence、单 side 纹理、hardness 2.0 = MC、自掉、opaque 挡
  光）：mesh = 两根 1/8 厚交叉柱（y 0..1.5）+ 朝**栅栏/满不透明块**邻居
  的连接横杆（开两端：边界端与邻居横杆相接、柱端在柱面内部——端帽会共面
  双绘）；柱顶/底邻 opaque 剔除（栅栏叠栅栏）；**碰撞 = 墙同款 1.5
  barrier**（细柱只是 mesh，整格 1.5 实心，不能穿过也不能跳过）。贪婪
  剔面**不剔栅栏邻居的满块面**（细柱不能遮挡——P85 slab 同款问题的反向
  修正：slab opaque 剔满块面留上半"洞"，栅栏不剔保面）。`[Phase 98]`
- **栅栏连接偏离** — 横杆只朝栅栏/满块（slab/楼梯/墙邻居无横杆）；无 MC
  的"栅栏顶放置"规则（自由放置）；木质单纹理（无 MC 的黑曜石/链/苔石
  栅栏族）。`[Phase 98]`

## 玻璃板 glass_pane（Phase 99）

- **玻璃板 = 1/16 薄片（第五个新块模型类型）= 可跳实心** — `glass_pane`
  （shape glass_pane、单 side 纹理、hardness 0.3 = MC 玻璃、自掉、**非
  opaque + 强制 transparent**【光透过，cross 纪律同款强制属性】）：mesh =
  z 居中 1/16 厚 1×1 玻璃片（双面透明 pass）+ 四条边细 rim——**rim 朝
  玻璃板邻居剔除**（两片共面 rim 双绘 = 透明 pass 暗缝，Phase 96 连接
  剔除的透明版）、朝 opaque 邻居顶/底 rim 剔除；X 方向板排 = 连续单片
  （每片全宽共面）。**碰撞 = 整格实心 1.0（可跳：跳高 ~1.129 > 1.0，墙/
  栅栏 1.5 不可跳）**。`[Phase 99]`
- **玻璃板偏离** — 无 MC 的"玻璃板须放在固体上"放置规则（自由放置）；
  rim 简化（1/16 细边用全纹理压扁，无 MC 的 1px 框独立 UV）。
  `[Phase 99]`
- **跳跃/踏步物理加固（P99 修）** — ① stepUp 的"1 格台阶"判定改用
  `solidTopHeight ≤ 1.0`（原格 solid 属性判定会踏上墙/栅栏 1.5 实心区的
  帽 → 脚 46 起跳翻越 barrier）；② 跳跃冲量计入子步 maxSpeed 估计（原
  长帧 86ms 单子步整帧跳速位移 → 顶点膨胀 1.65 翻过 1.5 墙，帧率依赖
  物理；PITS 两条）。`[Phase 99]`

## 熔岩湖（Phase 86）

- **熔岩湖 = 固定位置列表 + 程序化分布** — `lava_lakes` 段是一个湖列表
  （shipped 3 湖：2 深湖 (40,40) R4 + (55,45) R3，1 表面湖 (24,26) R1）+
  `[Phase 93]` 的 **`lava_lakes_procedural`** 段（`count` 个湖在区域 [60,90]
  内**确定性随机**分布，radius/depth/fill 各取范围内随机值 = MC 熔岩湖散布
  世界；区域在出生点 -Z 视图外 + 视图距离内 → base18 免重锚）。每个湖
  `enabled: false` 可关。`[Phase 86]` → `[Phase 88]` → `[Phase 90]` →
  `[Phase 93]`
- **深湖是静态满熔岩** — 盆底上 `fill` 层满熔岩（level 8），被盆地围住
  （四周地形 ≥ 区域最低 − 0，盆底再低 depth）→ 不流动、不蔓延、**不燃烧**
  （熔岩埋于 rim 之下，与 rim 顶不 6-邻）。`[Phase 86]`
- **表面湖燃烧周围 + 溢出**（`[Phase 90]`）— `depth == fill` 的湖把熔岩顶
  放在区域最低高度（与草 rim 顶同高）→ P58 燃烧 pass 把 6-邻可燃 rim 点燃
  （草→火→燃尽=焦土环）；同时熔岩在地表会**沿下坡溢出盆地**（FluidSim 流
  动，流缘再点燃相邻可燃物）= MC 熔岩湖"流动 + 燃烧周围"的落地。深湖（depth
  > fill）无此行为（埋没、被围）。**偏离**：表面湖非静态（溢出 + 燃烧改世界，
  确定性但非静止）；溢出的最终形态取决于地形坡度（非固定形状）。

## 楼梯 stairs（Phase 87）

- **楼梯朝向 = per-instance 状态（真 per-block 旋转）** — `[Phase 94]` 突破
  "无 per-instance 状态"哲学：朝向是 **World 的每格 byte**（per-instance
  facing，0-3），碰撞（solidTopHeight）+ mesh（emitStairs）直接从 World 读，
  **存档 format 14**（`stairs` 数组 {pos, facing}）。**潜行 + 右键**一个楼梯
  = 原地旋转 per-instance 朝向 `(f+1)%4`（块 id **不变**，仍 stone_stairs）。
  P89 的 4 块变体（stair_facing 0-3）保留为**默认朝向**（放置时 per-instance
  朝向初始化为块的 stair_facing；未设置时碰撞/mesh 回落块 stair_facing = 单
  测无需改）；P92 的 `stair_rotates_to`（块 id 环）保留解析但**不再用于旋转**
  （旋转改设 per-instance 朝向）。**放置自动朝向**（Minecraft 规则）
  `[Phase 97]`：玩家放置楼梯时 per-instance 朝向 = `stairFacingFromYaw(玩家
  yaw)`（低端朝玩家视线方向；纯函数，45° 平局取 X 轴）——P89 变体的 baked
  stair_facing 对玩家放置失效（朝向以 yaw 为准，变体 id 仅存档/非游戏上下文
  保留）。`[Phase 87]` → `[Phase 89]` → `[Phase 92]` → `[Phase 94]` →
  `[Phase 97]`
- **楼梯碰撞 = 斜面（AABB 高端边高度）** — 玩家脚高度随其 AABB 在斜面高端
  的位置（取 AABB 覆盖的**最高**斜面点，Phase 89 修正 P87 的低端边 bug——
  原会切入楼梯高端）；沿 facing 轴（north/south 用 z、east/west 用 x）；MC
  亚像素斜面碰撞之外的更细粒度 = 无。`[Phase 87]` → `[Phase 89]`

## BlockState per-block 状态（ENT-2）

- **P94 楼梯朝向已泛化为通用 per-instance 状态机制** — `[ENT-2]`：`Chunk::m_blockState`
  每格 byte（哨兵 255）+ 双门控数据驱动（`Shape::Stairs` 隐式 count 4/default=
  stair_facing ∪ `model.state_count` [1,255] + `state_default` [0,count) 严格
  配对，楼梯显式 state_count = 矛盾拒）+ `World::get/setBlockState`（只验
  <255+y+存在+无变化；域 [0,stateCount) 由收口/旋转/parse 三处强制）+
  **format 15 `states` 段**（全量、双向交叉校验、14→15 值保真迁移、journal
  派生）+ **init 不变量**（worldgen 5 类放置源出现 stateful 块 = 点名拒——
  "stateful 格必 session 派生" 从 P94 内容巧合升级为结构不变量）。
  **旋转泛化**：潜行 + 右键 = 任意 stateful 块状态 `(s+1)%stateCount()`
  （楼梯行为零变化；未来 sapling = stage +1 手工催熟，GEN-2 接受为特性）。
  状态不改光属性（opaque/light_opacity 是 per-block 非 per-state，v1.0
  消费者全满足）。shipped 内容零字段改动（Q2 定案）。
- **两类 per-block 状态存储分界（YAGNI 定案，不统一）** — byte 状态
  （mesh/collision/snapshot 读、O(1B/格) chunk 数组）vs struct 状态
  （furnace/campfire 等 game-resident per-pos map、format 7/11 段）：
  消费者集合不相交（struct 负载 ~50-80B/格进 chunk array = 数十 GB 不可行；
  统一注册表 = 重写 format 7/11 + tick + 面板，无 v1.0 任务要求）。
  **CRAFT-1 工作台分块 = 照 campfire map 范式**（+1 map + 1 格式段 + 收口
  条目），从本相取纪律（收口不变量"stateful 块恒拥有状态/移除即清" +
  存档段双向交叉校验）而非 byte 机制。
- **init 校验边界** — 5 类枚举放置源（biome surface/subsurface/decorations、
  ores.veins、trees trunk/leaves）；`bottom_block`/`deep_block`（terrain
  fill）不在枚举内 = 边界以代码注释 + 经验测试 `RealWorldgenPlacesNoStatefulBlock`
  （扫真实资产 3×3 列全区域 hasState=0）钉死；未来 bottom/deep 用 stateful
  名时该测试先暴露。`[ENT-2]`

## 树生长（GEN-2）

- **野外森林单橡种（MC 混生）** — 世界生成零触碰（worldgen trees 段 /
  placeTrees / 生成器代码一行不改）：生成森林只有 wood+leaves 橡树，
  spruce/birch 只经**种植**落地（树种差异 = 内容维度经 sapling，非
  生成维度）。野外多树种 = 未来项（worldgen trees 段 species 数据 +
  选择 roll + 演示区 clear 列强制橡 + **重锚门**——生成森林视觉变化
  必然动基线）。`[GEN-2]`
- **旧存档无 spruce/birch 来源** — starter 库存（hotbar.json `inventory`
  段，槽 24/25/26 各 3 颗 sapling）只在**新世界**装载：读档整体替换
  inventory（applySave 契约），旧存档的 27 槽是玩家的真实持有——
  spruce/birch 在旧世界无来源（oak sapling 经既有 leaves 5% 掉落可达）。
  有意为之：不向旧存档注入物品。`[GEN-2]`
- **旋转副作用 = 潜行右键 sapling → stage+1 手工催熟** — ENT-2 旋转
  泛化（任意 stateful 块 `(s+1)%stateCount`）在 sapling 上的表现：
  潜行 + 右键 = 跳一阶段（stage 7 时回 0 = 重置）。**接受为特性**
  （确定性，E2E/调试加速器）；不加 `state_rotates` 逃逸字段（YAGNI）。`[GEN-2]`
- **斧对新木无效** — spruce/birch trunk 未加 3 镐/3 斧的
  `fast_against` 清单（P67 契约精确集不变）：斧/镐对新木 = 基础
  break_speed（hardness 2.0）。清单演进 = 独立未来项。`[GEN-2]`
- **落树 = 一次性 updateRegion 主线程 ~5-8 ms** — 冠影重光照在
  `placeGrownTree` 内 1 次过树 bbox（流体 tick 同款区域重光照，非逐格
  updateLocal）：**玩家触发**（roll 过 + 阶段满 + 空间够），非每帧
  成本；大视距下 bbox 跨 2-3 列 = 上限处可感知一帧填充，接受。
  落树 ensureColumn：树跨入未加载列时收口同步补列（~1.6 ms/列既有
  放置行为）。`[GEN-2]`
- **sapling 土壤被挖不自毁** — MC 掉种子；项目 = 生长暂停等待
  （土壤门失败 = 本帧无步，土壤恢复继续长）。`[GEN-2]`
- **spruce/birch leaves 不掉苹果** — apple = oak 专属（既有 1/128 不动）；
  新 leaves 的 chance_drops = 对应 sapling @ 0.05 + stick @ 0.02。`[GEN-2]`
- ~~**cz ≥ cx+1 半区整块不种树/不放装饰（wz 循环上界 bug）**~~ —
  placeTrees/placeDecorations 内层 `for (wz = wz0; wz < limit; ...)` 的
  `limit` 误用 X 基（`ox + kChunkSize [+ margin]`）：chunk 的 oz ≥
  ox+19（树）/ ox+16（装饰）时本 chunk 的 z 列不被迭代 → trunk 单列只由
  本列 chunk 写入 = **约世界一半（cz ≥ cx+1 对角半区）永久无树无花**；
  装饰在 cz = cx+1 层同样整块缺失。Phase 4（ae19893）起潜伏，101 道门
  未发现 = 基线视图朝 -Z（该侧 chunk 恒在种树侧）+ 生成器单测只断言
  局部区域。GEN-5 shipped 扫描（320×320 树生长侧盒）首次量化：修复侧
  346 树列正常。**`[GEN-6]` 已修复（v0.20.0）**：wz 上界改 Z 基
  `zlimit = oz + kChunkSize [+ margin]`（2 处一行级，wx/margin/格网/
  roll/GEN-5 守卫零改动）+ 2 判别性单测（pre-fix FAIL/post-fix PASS）；
  世界生成演进量化（shipped seed 123456789 真实资产探针）：门场景盒树列
  13→33（2.54×）/装饰 5→20、演示盒 24→30/+2、前瞻半区锥内 215 列
  （游戏内视野，非 60f fresh 视图）；**base18 免重锚**（第 103 次结构性
  免疫：区内 gained 5 特征全在视锥外 = 探针论证 + 像素终裁互证）+ 性能
  A/B v0.19.0 成本可忽略（细节见 `archive/phase-gen6.md`）。`[GEN-5]`
  发现，`[GEN-6]` 解决。

## 数据 / 内容

- **方块 per-instance 状态 = 仅楼梯朝向一种** — BlockState 概念仍未引入（附属、流体等级等需要时再设计）。`[Phase 2]`
  **`[Phase 94]` 起唯一例外 = 楼梯朝向**：World 每格 byte（chunk `m_stairFacing`，哨兵 255 = 未设置 →
  碰撞/mesh 回落块 `stair_facing` 默认），setBlock 收口维护（放置按块默认、潜行右键旋转、移除清哨兵），
  碰撞（solidTopHeight）+ mesh（emitStairs）直接读，存档 format 14 `stairs` 数组双向交叉校验——
  **范围仅限 stair 形状块**（详见「楼梯 stairs」节）。次级例外（既有）：信标（Phase 83）的**功率等级**不入状态
  ——`world/beacon` 纯函数每帧从基座重算；炉子/营火/火的 per-position 状态在 Game 的 map 里
  （format 7/11/10，非 World 每格字段，见各节）。
## 已修复（归档）→ 细节存档

各阶段的修复条目已移入 `docs/archive/phase-NN.md`（「阶段报告」= 原归档条目；
有会话存档节的阶段另含「会话存档」部分）：

| 阶段 | 存档文件 |
|---|---|
| Phase 3 | `archive/phase-3.md` |
| Phase 5 | `archive/phase-5.md` |
| Phase 6 | `archive/phase-6.md` |
| Phase 7 | `archive/phase-7.md` |
| Phase 8 | `archive/phase-8.md` |
| Phase 9 | `archive/phase-9.md` |
| Phase 10 | `archive/phase-10.md` |
| Phase 11 | `archive/phase-11.md` |
| Phase 12 | `archive/phase-12.md` |
| Phase 13 | `archive/phase-13.md` |
| Phase 14 | `archive/phase-14.md` |
| Phase 15 | `archive/phase-15.md` |
| Phase 16 | `archive/phase-16.md` |
| Phase 17 | `archive/phase-17.md` |
| Phase 18 | `archive/phase-18.md` |
| Phase 19 | `archive/phase-19.md` |
| Phase 20 | `archive/phase-20.md` |
| Phase 20.5 | `archive/phase-20.5.md` |
| Phase 21 | `archive/phase-21.md` |
| Phase 22 | `archive/phase-22.md` |
| Phase 23 | `archive/phase-23.md` |
| Phase 24 | `archive/phase-24.md` |
| Phase 25 | `archive/phase-25.md` |
| Phase 26 | `archive/phase-26.md` |
| Phase 27 | `archive/phase-27.md` |
| Phase 28 | `archive/phase-28.md` |
| Phase 29 | `archive/phase-29.md` |
| Phase 31 | `archive/phase-31.md` |
| Phase 32 | `archive/phase-32.md` |
| Phase 33 | `archive/phase-33.md` |
| Phase 34 | `archive/phase-34.md` |
| Phase 35 | `archive/phase-35.md` |
| Phase 36 | `archive/phase-36.md` |
| Phase 37 | `archive/phase-37.md` |
| Phase 38 | `archive/phase-38.md` |
| Phase 39 | `archive/phase-39.md` |
| Phase 40 | `archive/phase-40.md` |
| Phase 41 | `archive/phase-41.md` |
| Phase 42 | `archive/phase-42.md` |
| Phase 43 | `archive/phase-43.md` |
| Phase 44 | `archive/phase-44.md` |
| Phase 45 | `archive/phase-45.md` |
| Phase 46 | `archive/phase-46.md` |
| Phase 47 | `archive/phase-47.md` |
| Phase 48 | `archive/phase-48.md` |
| Phase 49 | `archive/phase-49.md` |
| Phase 50 | `archive/phase-50.md` |
| Phase 51 | `archive/phase-51.md` |
| Phase 52 | `archive/phase-52.md` |
| Phase 53 | `archive/phase-53.md` |
| Phase 54 | `archive/phase-54.md` |
| Phase 55 | `archive/phase-55.md` |
| Phase 56 | `archive/phase-56.md` |
| Phase 57 | `archive/phase-57.md` |
| Phase 58 | `archive/phase-58.md` |
| Phase 59 | `archive/phase-59.md` |
| Phase 60 | `archive/phase-60.md` |
| Phase 61 | `archive/phase-61.md` |
| Phase 62 | `archive/phase-62.md` |
| Phase 63 | `archive/phase-63.md` |
| Phase 64 | `archive/phase-64.md` |
| Phase 65 | `archive/phase-65.md` |
| Phase 66 | `archive/phase-66.md` |
| Phase 67 | `archive/phase-67.md` |
| Phase 68 | `archive/phase-68.md` |
| Phase 69 | `archive/phase-69.md` |
| Phase 70 | `archive/phase-70.md` |
| Phase 71 | `archive/phase-71.md` |
| Phase 72 | `archive/phase-72.md` |
| Phase 73 | `archive/phase-73.md` |
| Phase 74 | `archive/phase-74.md` |
| Phase 75 | `archive/phase-75.md` |
| Phase 76 | `archive/phase-76.md` |
| Phase 77 | `archive/phase-77.md` |
| Phase 78 | `archive/phase-78.md` |
| Phase 79 | `archive/phase-79.md` |
| Phase 80 | `archive/phase-80.md` |
| Phase 81 | `archive/phase-81.md` |
| Phase 82 | `archive/phase-82.md` |
| Phase 83 | `archive/phase-83.md` |
| Phase 84 | `archive/phase-84.md` |
| Phase 85 | `archive/phase-85.md` |
| Phase 86 | `archive/phase-86.md` |
| Phase 87 | `archive/phase-87.md` |
| Phase 88 | `archive/phase-88.md` |
| Phase 89 | `archive/phase-89.md` |
| Phase 90 | `archive/phase-90.md` |
| Phase 91 | `archive/phase-91.md` |
| Phase 92 | `archive/phase-92.md` |
| Phase 93 | `archive/phase-93.md` |
| Phase 94 | `archive/phase-94.md` |
| Phase 95 | `archive/phase-95.md` |
| Phase 96 | `archive/phase-96.md` |
| Phase 97 | `archive/phase-97.md` |
| Phase 98 | `archive/phase-98.md` |
| Phase 99 | `archive/phase-99.md` |
| Phase 99.x（性能系列） | `archive/phase-99.x.md` |
| CRAFT-1（v1.0 任务表：工作台分块） | `archive/phase-craft1.md` |
| CRAFT-2（v1.0 任务表：物品 3D 模型） | `archive/phase-craft2.md` |
| CRAFT-3（v1.0 任务表：材质音效） | `archive/phase-craft3.md` |
| ENT-1（v1.0 任务表：四肢摆动，含 1b 实机模型修复） | `archive/phase-ent1.md` |
| ENT-2（v1.0 任务表：BlockState per-block 状态泛化） | `archive/phase-ent2.md` |
| ENT-3（v1.0 任务表：mob 踏阶） | `archive/phase-ent3.md` |
| ENT-4（v1.0 任务表：水体交互） | `archive/phase-ent4.md` |
| FLUID-1（v1.0 任务表：水量守恒） | `archive/phase-fluid1.md` |
| FLUID-2（v1.0 任务表：游泳划水） | `archive/phase-fluid2.md` |
| FLUID-3（v1.0 任务表：mob 涉水修正） | `archive/phase-fluid3.md` |
| GEN-1（v1.0 任务表：面条洞穴） | `archive/phase-gen1.md` |
| GEN-2（v1.0 任务表：树生长） | `archive/phase-gen2.md` |
| GEN-3（v1.0 任务表：矿脉形态） | `archive/phase-gen3.md` |
| GEN-4（v1.0 任务表：洞穴破碎度） | `archive/phase-gen4.md` |
| GEN-5（v1.0 任务表：浮空树） | `archive/phase-gen5.md` |
| GEN-6（v1.0 任务表：树放置半区修复） | `archive/phase-gen6.md` |
| LGT-1（v1.0 任务表：union-region，砍除记档） | `archive/phase-lgt1-flip.md` |
| LGT-2（v1.0 任务表：叶子光衰减） | `archive/phase-lgt2.md` |
| LGT-3（v1.x 任务表：夜光重传播 + MC 月光地板） | `archive/phase-lgt3.md` |
| LGT-4（v1.x 任务表：光束入光） | `archive/phase-lgt4.md` |
| LGT-5（v1.x 任务表：半块透光） | `archive/phase-lgt5.md` |
| GEN-7（v1.x 任务表：基岩多层） | `archive/phase-gen7.md` |
| GEN-8（v1.x 任务表：地形互感/山峰互感） | `archive/phase-gen8.md` |
| PIPE-1（v1.0 任务表：worker 网格化） | `archive/phase-pipe-1.md` |
| PIPE-2（v1.0 任务表：存档增量） | `archive/phase-pipe-2.md` |
| PIPE-3（v1.x 任务表：流体 per-cell） | `archive/phase-pipe3.md` |
| PRE-1..6（v1.0 任务表：中文字体/边缘雾/水面波纹/云与环境音/透明 discard/掉落光照） | `archive/phase-pre1.md` |
| PRE-2（v1.0 任务表：边缘雾） | `archive/phase-pre2.md` |
| PRE-3（v1.0 任务表：水面波纹） | `archive/phase-pre3.md` |
| PRE-4（v1.0 任务表：云与环境音） | `archive/phase-pre4.md` |
| PRE-5（v1.0 任务表：透明 discard） | `archive/phase-pre5.md` |
| PRE-6（v1.0 任务表：掉落光照） | `archive/phase-pre6.md` |
| SAVE-1（v1.0 任务表：自动存档） | `archive/phase-save1.md` |
| STRM-1（v1.0 任务表：环形状） | `archive/phase-strm1.md` |
| STRM-2（v1.x 任务表：流体列保留 + 99.8 流送边界活性补洞） | `archive/phase-strm2.md` |
| SURV-1（v1.0 任务表：护甲） | `archive/phase-surv1.md` |
| SURV-2（v1.0 任务表：氧气窒息） | `archive/phase-surv2.md` |
| SURV-3（v1.0 任务表：耐久） | `archive/phase-surv3.md` |
| SURV-4（v1.0 任务表：药水饮用） | `archive/phase-surv4.md` |
| SURV-5（v1.0 任务表：mob 效果） | `archive/phase-surv5.md` |
| UXFIX-1（v1.0 任务表：实机渲染状态泄漏修复） | `archive/phase-uxfix1.md` |

P0–P2、P4 无细节存档（提交列表见 `SESSION_CONTEXT.md`）。
