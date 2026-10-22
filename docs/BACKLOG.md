# BACKLOG.md — v1.0 任务表（执行权威）

> 来源：2026-10-20 对"文档中所有后续任务"的全量分类（依据 =
> KNOWN_ISSUES 残余/留待条 + `archive/phase-99.x.md`「残余」+ 全仓
> "留待/后续/未来"标记扫描）= 十大系统 63 项；按用户决定（10-20），
> **每类只保留靠前（最重要）一半 = 29 项——29 项全部完成 = v1.0 发布**。
> 此后任务只从本表取，不另列菜单；增删格子需用户确认。

## 砍半规则

- 每类只保留前一半（偶 n 取 n/2，奇 n 取 floor(n/2)）；重要性序 =
  用户实痛 > 机制缺口（影响可玩性）> 性能/架构债 > 视觉/音频打磨 >
  MC 细节对齐。
- 砍掉的项不删除：记入「砍掉清单」（保留原代号），可一句话恢复入表。

## v1.0 任务矩阵（29）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ |
|---|---|---|---|---|---|---|
| STRM 流送与内存 | ~~STRM-1 环形状~~ 完成 | | | | | |
| PIPE 帧管线与并发 | ~~PIPE-1 worker网格化~~ 完成 | ~~PIPE-2 存档增量~~ 完成 | | | | |
| LGT 光照 | ~~LGT-1 union-region~~ 砍除 | ~~LGT-2 叶子光衰减~~ 完成 | | | | |
| GEN 世界与生成 | ~~GEN-1 面条洞穴~~ 完成 | ~~GEN-2 树生长~~ 完成 | ~~GEN-3 矿脉形态~~ 完成 | | | |
| FLUID 流体 | FLUID-1 水量守恒 | FLUID-2 游泳划水 | | | | |
| ENT 实体/Mob/移动 | ENT-1 四肢摆动 | ~~ENT-2 BlockState★~~ 完成 | ENT-3 mob踏阶 | ENT-4 水体交互 | | |
| SURV 生存/战斗/效果 | SURV-1 护甲 | SURV-2 氧气窒息 | SURV-3 耐久 | SURV-4 药水饮用 | SURV-5 mob效果 | |
| CRAFT 物品/合成/面板 | CRAFT-1 工作台分块★ | CRAFT-2 物品3D模型 | CRAFT-3 材质音效 | | | |
| SAVE 存档 | SAVE-1 自动存档 | | | | | |
| PRE 表现层 | PRE-1 中文字体 | PRE-2 边缘雾 | PRE-3 水面波纹 | PRE-4 云与环境音 | PRE-5 透明discard | PRE-6 掉落光照 |

★ = 前置项：ENT-2（BlockState）是 CRAFT-1 / GEN-2 的前提。

## 执行顺序

1. 从上到下逐行、行内从左到右：STRM-1 → PIPE-1 → PIPE-2 → LGT-1 → … → PRE-6。
2. 前置约束：GEN-2、CRAFT-1 依赖 ENT-2（行序先到时先做 ENT-2）。
3. PIPE-1 的价值在 STRM-1 后重测（环形状缩小后 remesh 成本若已入帧预算，
   砍除提案需用户确认）。
4. 一任务一 phase，走标准阶段流程（实现 → 构建+测试 → E2E → 文档 →
   提交 → 报告）。

## 任务清单（与代号一一对应）

### STRM 流送与内存
- ~~**STRM-1 环形状**~~ — **完成（2026-10-21）**：方形 (2R+1)² → 圆盘 dx²+dz²≤R²（纯整数谓词）+ 远平面帽 R_eff=min(view_distance, 64)【用户定案选项 b：只装可绘制列，地平线视觉零下行】；vd80 实测 RSS 9.08→4.59 GB（−49.5%，原"12 GB"为估算、"~1/9"为半径问题非形状问题）；base18 第 81 次门（无输入结构性免疫）；原 ~1/9 目标 = R_eff=30（480 块地平线）一个常量可切换，用户暂不采用（99.x 残余 #1）

### PIPE 帧管线与并发
- ~~**PIPE-1 worker 网格化**~~ — **完成（2026-10-21）**：mesher 计算 → 4 worker 快照 job（每帧 ≤2 列：3×3 邻域 72 chunk memcpy 进 job 私有临时 World，ChunkMesher **零改动**）+ `nextMeshColumns` 纯函数选择规则（unlit 排除【99.4 lit pop-in 承接】/ 光足迹 Chebyshev ≥2 排除 / 在途排除 / `columnDistanceSq` 最近优先）+ 主线程只留列合并 GL 上传（~1-3 ms/列不可搬走）+ post 顺序不变量 gen→mesh→light（thread_pool.hpp 契约）+ integrate 先于 stream-integrate（防 clear-dirty 吞标脏）；价值重测（step-0 插桩实测）= 主线程 remesh 填充期 7-11.3 ms/帧 + complete 后 2.8 min 拖尾（脏集峰值 45,895）→ 搬 worker；base18 第 82 次门（A/B 四对字节一致，论证措辞 = 无输入 worker 活动有界于启动 16 mesh job 帧 ≤10 完成）；帧时序：pop-in N+1→N+2（帧锁定）+ 最坏 +2 帧收敛（PITS）；99.x 残余 #3
- ~~**PIPE-2 存档增量**~~ — **完成（2026-10-21 夜）**：退出 O(环大小) 全量 diff → **O(会话编辑数)**：World 写收口（setBlock/setFluid）位置集 journal（`SaveJournal` 双表 + 位移陷阱双上报 + 软上限 262,144 越线降级 legacy）+ 退出按 journal 触及 ∧ 现存 chunk 重算 fresh（makeChunkGenerator 同一回调，generateChunk 纯函数）、仅 live≠fresh 输出（**与 diffWorld 字节等价**——9 类变更 fixture 双阶段 oracle 单测 + E2E 跨 build/进程内对照）；楼梯 O(N) 世界扫描一并转 journal 派生（不变式 = worldgen 永不生成楼梯，真实 assets 钉死）；读档重放自动播种（observer 先于 tryLoadSave 安装）；卸载按列驱逐；`--save-full-diff` CLI 旗 = 进程内 oracle + 逃生锤；format 一字未动（P37 优化方向落地）

### LGT 光照
- ~~**LGT-1 union-region**~~ — **砍除（2026-10-21，step-0 翻转触发 + 用户确认）**：设计案（confluence 证明 + 合并判据 footprint 相交传递闭包 + 潜伏 sky rescan clobber bug 发现）已完成，但 step-0 插桩实测证伪前提——同帧选出的 2 列大多远隔（选列 (distSq,x,z) 序：同半径圆格点平局按位置序取前两点 = z=±z* 远隔对称对；vd80 静止填充 6418 帧中可合并 6 帧 = 0.1%，预期节省 **0.002 ms/帧** ≪ 1 ms 砍除线，500× 过线）；光照 job 下限 ~16.7 ms/帧（2 box × 8.37）保留 = 流送 fps 主下限，潜在方向 = 选列 tie-break 相邻化 + union（STRM 相邻变更，表外）；99.x 残余 #2
- ~~**LGT-2 叶子光衰减**~~ — **完成（2026-10-21）**：`light_opacity` 0-15 字段（缺省**由 `opaque` 派生** opaque→15/非 opaque→0 = 未设字段内容逐位复现旧行为）+ 统一进入格成本（流体 3 / 垂直天空光=O / 其余=max(1,O)，O=15 不穿透、emission 源格自身持值）+ rescan 逐层减 O；内容 = **leaves 2 / glass 1**（MC 值；glass 垂直 0→1 = 顺手修复现状玻璃顶垂直免费穿过）+ cross/glass_pane 薄模型 O=15 init 守卫；「渲染条」= mesh 烘焙侧零代码（新光值走既有 dirty→PIPE-1 快照 mesh→GL 管线）；base18 **第 83 门 = 重锚**（基线视图含叶冠，首个「光值类」重锚：A/B diff 212,174 px 限定世界表面【stone-gray 96,497/leaf-green 72,957/dirt-brown 24,108/sky-blue 2】、仅暗化【亮化 40 px = AA】、天空像素数一致）；封盒场景：S3a（仅 glowstone）全帧新旧字节一致 = 派生 O 的 E2E 级证明、S3b（熔岩湖）零差 = 流体固定 3 不变、S2（叶探针）亮度带互斥（row A 13/row B 9/冠下 9/control 15）；1226/1226（+14，存量 1212 零改动）；S4 输入放置留用户在场补跑（away 模式偷 lock）

### GEN 世界与生成
- ~~**GEN-1 面条洞穴**~~ — **完成（2026-10-21 夜）**：单噪声奶酪洞 → **两 3D value-noise 中位带相交管状隧道**（`|n1−0.5|<T ∧ |n2−0.5|<T`，MC spaghetti 同款；valueNoise3 既有原语零新噪声代码、n2 嵌套短路仅 ~12% 格求值）+ **surface_guard 0 = 洞口**（显式改判，原表达式 guard 0 仍结构性排除表面格）+ **spawn_clear_columns 2**（原点 ±2 列整列不挖，per-chunk 早退 = 出生/演示区任意 seed 结构安全）+ **每湖 lava 带排除盒**（create() 预计算 ResolvedLake，placeLavaLakes 改读预存 floorY 顺带消 per-chunk 重扫）；标定（纯生成器探针，零调参轮定稿 0.09/0.06/0/2）：挖除 5.38% / 洞口 94.7·64×64⁻¹ / run 宽 p90 6·5 / clear+5 湖盒 A/B 零差；gen 1.17 ms/列（低于现状 1.6）；零 format（旧存档新洞 = P86/88/93 先例）；base18 第 84 门 = **结构性免疫不重锚**（新 ×2 与 gate 83 锚字节一致，A/B 1 px 远洞剪影 = clear 区外预期差异）；1240/1240（+14 新 + 1 迁移）；E2E：S2 洞口静态 ×2 RAW + A/B 73k px / S3 跨 seed + seed9 ×2 savenorm EXACT / S4a 深湖中心掩码零差（排除盒）/ S4b 表面湖 rim+盆盒恒等、每世界确定（跨世界溢出形态差 = 世界演进预期）/ S6 点名拒绝 / **S2b 进洞走路推迟**（用户在场补 ~2 min）
- ~~**GEN-2 树生长**~~ — **完成（2026-10-22 夜）**：3 树种可种植 sapling（oak 复用 wood/leaves + spruce/birch 新块；块 id 路线，stateful cross 块 = ENT-2 state_count 8 即生长 stage）+ `tree` 对象 6 键严格解析（init 交叉校验含 tree⇒state_count≥2）+ **m_crops 平行生长引擎**（saplingGrowthSteps 纯函数：plant_on 土壤门 + growth_light 9 门 + roll%ticks 120 → Advance 状态+1 零光照成本 / 终阶段+fits → 落树）+ **`world/tree_shape` 纯几何**（planTree 3 形状表：oak 与 placeTrees 逐格同构【单测对照钉死】/ spruce 下宽上尖锥 / birch 细高小冠 + treeHeightRoll 新盐 PITS #4）+ **stage 视觉 = emitCross 绕格心缩放 (stage+1)/8**（无状态 cross 恒 scale 1 位级零变化）+ **落树 = placeGrownTree**（逐格 setBlock + 1 次 updateRegion bbox = LGT-2 冠影）+ 内容闭环（3 leaves chance_drops += sapling @0.05【oak = P55 演进】/ hotbar.json 新 `inventory` 段 3 sapling 各 3 颗入隐藏槽 24-26 = 不渲染）+ 9 新 block tile（salt 935 起 / 9 atlas 空位免重锚 / 既有 PNG 零变化）；**世界生成零触碰**（sapling stateful ⇒ ENT-2 init 不变量天然拒）；1285/1285（+25 新 + 2 内容演进【P55 掉落集 2→3 / P63 可燃 10→17】）；base18 **第 86 门 = 结构性免疫不重锚**（新 ×2 与门 84 锚字节一致）；E2E：gen2_scan 探针预计算选点（PITS #208 纪律）+ S2 落树场景 ×2 RAW + savenorm EXACT + 树 55 格与 planTree 逐格一致 + 截图树像素 / S3 推进对照 **stage 4 = 探针预计算逐帧一致** + A/B 324k px / S6 点名拒绝；**偏离**：野外森林单橡种（worldgen 多树种 = 未来项需重锚）/ 旧存档无 spruce-birch 来源（oak 经 leaves 可达）/ 潜旋 sapling = stage+1 手工催熟（接受为特性）/ 斧对新木无效；晨间输入三项推迟（放 sapling/潜旋催熟/破坏掉 self）。**GEN 行剩 GEN-3 = GEN-2 完成时 GEN 行 2/3**
- ~~**GEN-3 矿脉形态**~~ — **完成（2026-10-22 夜）**：随机游走脉 → **MC 3×2×1 旋转椭球脉**（`planOreVein` 纯函数：整数谓词 4u²+9v²+36w²≤36 恰 21 格【static_assert 钉死】+ 6 轴置换取向 + PITS #4 取数流；**gravel 同改**）+ placeOres 重接（**placeOre 零改动**、lattice/概率门/起点/盐流逐位保留、margin 17→11）+ **vein_size 语义演进 = 椭球簇数 K ∈ [1,8]**（shipped 全 K=1；K>1 机制实现 + 单测，未来调档零核心改动）+ **单轮密度标定**（探针 64 列×128 高：pass1 比值 2.6-5.6× → 概率降档定稿 0.154/0.0855/0.0376/0.0101/0.0208/0.0445 → pass2：coal 1.060×/iron 0.875×/gold 1.007×/gravel 1.073× 带内；**diamond 0.641×/lapis 0.732× 残留记档**【单轮纪律】）；1297/1297（+12 新 + 2 演进）；base18 **第 87 门 = 结构性免疫不重锚**（视图 ⊆ 演示区 + 演示区无洞 + 矿只替换 deep 层）；E2E：S2 洞壁矿 A/B（场景火把照明【深洞矿光 ≈0 不可见】+ 相机真眼高 pitch【PITS #209】）新旧 **6,703 px**（v0.3.0 资产 scratch TM_REPO 跑）/ S6 vein_size 9 点名拒绝；**偏离**：矿感 = 脉更大更稀（MC 1.18+ chunky sparse）/ 旧存档新脉形（worldgen 演进）/ K>1 未 shipped。

### FLUID 流体
- **FLUID-1 水量守恒** — 源/流动状态机，修"水量凭空增减"（世界条）
- **FLUID-2 游泳划水** — 水下划水物理差异（流体桶条）

### ENT 实体/Mob/移动
- **ENT-1 四肢摆动** — mob 摆腿/摆臂动画，per-part 模型矩阵（99.x 残余 #4）
- ~~**ENT-2 BlockState★**~~ — **完成（2026-10-22 夜）**：P94 楼梯朝向（World 每格 byte + format 14 `stairs`）泛化为**通用 per-instance 状态机制**：`Chunk::m_blockState` 换名 + 哨兵泛化 255 + 双门控数据驱动（Stairs 形状隐式 count 4 ∪ `model.state_count`[1,255]+`state_default` 严格配对，楼梯显式声明 = 矛盾拒）+ `World::get/setBlockState`（只验 <255，域由收口/旋转/parse 三处强制）+ Game 三写入点泛化（收口/旋转 `(s+1)%stateCount()`/P97 钩子）+ **format 15 `states` 段**（全量 + 逐条目域校验 + 双向交叉校验 + 键纪律三向 + 14→15 值保真迁移 + collectSave 两路 journal 派生）+ **init 不变量**（worldgen 5 类放置源 stateful 点名拒 = "stateful 格必 session 派生"结构化）；**四个 per-block map 不迁移**（byte/struct 两类状态消费者不相交 = YAGNI 定案，CRAFT-1 取 campfire 范式 + 纪律）；shipped 内容零字段改动；1260/1260（+20 新，存量换名适配语义不动）；base18 **第 85 门 = 结构性免疫不重锚**（新 ×2 与门 84 锚字节一致）；E2E：S2 预置状态场景 A/B **298,211 px = per-instance 朝向首次像素级验证**（P94 时代仅存证）+ round-trip 截图字节一致 + S3 迁移 shot 与 S2A 字节一致（值保真端到端背书）；晨间潜旋回归推迟（pent2_rot.zsh 就绪）；PITS +1（#208 场景几何探针纪律）
- **ENT-3 mob 踏阶** — mob 1 格 step-up，修追逐卡台阶（Mob 条）
- **ENT-4 水体交互** — mob/掉落物游泳、浮力（实体条）

### SURV 生存/战斗/状态效果
- **SURV-1 护甲** — 护甲系统（减伤/材质档/穿戴）（生存条）
- **SURV-2 氧气窒息** — 水下氧气条 + 窒息伤害（生存条）
- **SURV-3 耐久** — 工具/护甲耐久（物品/斧条）
- **SURV-4 药水饮用** — 药水系统，给 weakness 等效果游戏内来源（状态效果条）
- **SURV-5 mob 效果** — 效果作用于 mob（中毒/凋灵）、远程箭施加（状态效果条）

### CRAFT 物品/合成/面板
- **CRAFT-1 工作台分块★** — 每工作台独立容器，**依赖 ENT-2**（工作台条）
- **CRAFT-2 物品 3D 模型** — per-item 3D 模型（手持/世界/进食举到嘴）（UI 条）
- **CRAFT-3 材质音效** — per-block/per-item 材质音色 + 3D 空间化（UI 条）

### SAVE 存档
- **SAVE-1 自动存档** — 周期性 autosave + "保存并退出"流程（存档条）

### PRE 表现层
- **PRE-1 中文字体** — 字体中文字形（UI 条）
- **PRE-2 边缘雾** — 世界边缘雾，遮住流送悬空截面（渲染条）
- **PRE-3 水面波纹** — 水面波纹动画（渲染条）
- **PRE-4 云与环境音** — 云层 + 昼夜环境音（渲染条）
- **PRE-5 透明 discard** — alpha cutout discard，修透明孔深度写入（渲染条）
- **PRE-6 掉落光照** — 掉落物受本地光照并参与遮挡（实体条）

## 砍掉清单（34，v1.0 不含，保留原代号）

- STRM：STRM-2 流体列保留
- PIPE：PIPE-3 流体 per-cell / PIPE-4 跨 chunk 合并
- LGT：LGT-3 夜光重传播 / LGT-4 光束入光 / LGT-5 半块透光
- GEN：GEN-4 基岩多层 / GEN-5 地形互感 / GEN-6 耕地湿润
- FLUID：FLUID-3 倒向水面 / FLUID-4 水熔岩反应 / FLUID-5 流体种类扩展
- ENT：ENT-5 坠落伤害 / ENT-6 AI 补强 / ENT-7 移动惯性 / ENT-8 和平/保护环 / ENT-9 掉落可视化
- SURV：SURV-6 效果档位 / SURV-7 死亡统计 / SURV-8 工具 10× / SURV-9 饱和细化 / SURV-10 挤奶冷却
- CRAFT：CRAFT-4 配方提示 / CRAFT-5 炉子表现 / CRAFT-6 燃料熔炼 / CRAFT-7 背包交互
- SAVE：SAVE-2 速度入档 / SAVE-3 槽位管理
- PRE：PRE-7 中毒/燃烧表现 / PRE-8 mob 火焰 / PRE-9 箭/弓/手视觉 / **PRE-10 图标重叠修复（原表内唯一 bug 级项：效果图标行与吸收红心重叠，3 效果同激活可触发，可一句话恢复）** / PRE-11 HUD 细化 / PRE-12 tooltip/F3

## 表外

- ~~**文档债（建议最先做，非代码）**~~：**已解决**（2026-10-20，逐条代码复核后改写
  KNOWN_ISSUES 10 处 = 9 条 + 连带 obsidian 括注）：6 条完全过期 → 标记已解决
  （worldgen 熔岩湖 / 存档 format 14 迁移链 / 食物 10 种 / per-instance = 仅楼梯朝向 /
  shaped 配方 21 个 / 熔岩桶）；3 条部分过期 → 改写为活条目（仍无用途 = 仅 leather /
  钻 1 格缝隙仍未实现但楼梯斜面 1.6/1.8 有通行差异 + 潜行新增坐姿/旋转/低顶陷阱 /
  水熔岩桶 = 流体容器非饮品，唯一可饮用仍牛奶桶）；⑨ 复核结论 = 99.2 已解决
  （含作物列永不卸载：m_editedColumns insert-only + m_crops 等五类状态列双重豁免）
- **已接受偏离（非任务）**：锻石不可熔回、箭不自伤、水熔岩 apply 顺序、
  实体 age 墙钟、喝奶不恢复饥饿、同色染色不消耗等（文档明确接受的设计决策）
- **无界项（1.0 不做）**：新内容开放空间（方块/物品/配方/mob 可无限加）、
  经验（XP）/附魔系统

## 变更记录

- 2026-10-20：建立。63 项（文档全量后续任务分类）→ 用户决定每类砍半
  至 29 项 = v1.0 范围；执行顺序与前置约束见上。
- 2026-10-20：表外文档债已解决（KNOWN_ISSUES 9 条过期描述 + 连带 obsidian
  括注，逐条代码复核；6 条标已解决 / 3 条改写为活条目 / ⑨ 复核 = 99.2 已解决）。
- 2026-10-21：STRM-1 完成（用户定案选项 b：圆盘 + 远平面帽 R_eff=64；实测
  vd80 9.08→4.59 GB −49.5%；base18 第 81 次门）。
- 2026-10-21：PIPE-1 完成（worker 快照网格化；step-0 重测定案做；base18
  第 82 次门）。
- 2026-10-21：PIPE-2 完成（存档增量 journal；与 diffWorld 字节等价；
  vd80 全环退出 6.2 s → **17 ms**（journal 路径；3600f 场景 legacy 对照
  3.28 s ≈ 190×，退出成本 = O(会话编辑数)、与环大小脱钩））。PIPE 行 2/2
  全清，v1.0 任务表
  3/29 完成）。
- 2026-10-21：LGT-1 union-region **砍除记档**（step-0 实测：可合并帧
  6/6418 = 0.1%、预期节省 0.002 ms/帧 ≪ 1 ms 砍除线——同帧选列 (distSq,x,z)
  平局按位置序配对远隔列，设计前提被证伪；PIPE-1 先例翻转纪律触发，
  用户确认砍除；设计过程发现 1 个潜伏 sky rescan clobber bug 记 PITS；
  详见 `archive/phase-lgt1-flip.md`）。
- 2026-10-21：LGT-2 完成（叶子光衰减：light_opacity 0-15 字段缺省由 opaque
  派生 = 未设内容逐位复现旧行为 + 统一进入格成本 + leaves 2 / glass 1；
  base18 第 83 门 = 重锚【首个光值类重锚，A/B diff 限定冠/冠下表面、天空
  零变化】；封盒 S3a 全帧新旧字节一致 = 派生 O 的 E2E 级证明；1226/1212；
  S4 输入放置留用户在场补跑）。LGT 行 2/2 全清（1 砍 1 完成），v1.0 任务表
  推进至 GEN 行。
- 2026-10-21（夜）：GEN-1 完成（面条洞穴：两 3D 噪声中位带相交隧道 +
  洞口 surface_guard 0 + spawn_clear_columns 2 + 每湖 lava 带排除盒；标定
  零调参轮定稿；gen 1.17 ms/列；1240/1226；门 84 结构性免疫不重锚【新 ×2
  与 gate 83 锚字节一致】；E2E 批按用户要求压缩 = S2b 进洞走路推迟，
  其余全过）。
- 2026-10-22（夜，夜间自主推进）：ENT-2 完成（BlockState：P94 楼梯朝向
  泛化为通用 per-instance 状态机制；format 15 states 段 + 14→15 值保真
  迁移 + init 不变量【worldgen 放置源禁 stateful】+ 双门控数据驱动
  state_count/state_default；1260/1240；门 85 结构性免疫不重锚；E2E A/B
  298k px = 朝向首次像素级验证；晨间潜旋回归推迟）。发布 **v0.3.0**。
- 2026-10-22（夜，夜间自主推进）：GEN-2 完成（树生长：3 树种 sapling +
  tree 对象 + m_crops 平行生长引擎 + tree_shape 纯几何 3 形状 + emitCross
  stage 缩放 + 落树 placeGrownTree + 内容闭环 leaves 掉落/隐藏 starter；
  1285/1260；门 86 结构性免疫不重锚；E2E 探针预计算选点 + 落树 ×2 RAW +
  stage 4 逐帧一致 + A/B 324k px + S6 点名；世界生成零触碰）。发布
  **v0.4.0**。GEN 行 2/3（剩 GEN-3 矿脉形态）。
- 2026-10-22（夜，夜间自主推进）：GEN-3 完成（矿脉形态：随机游走 → MC
  3×2×1 旋转椭球 21 格脉 + vein_size = 簇数 K∈[1,8] shipped 全 1 + gravel
  同改 + 单轮密度标定【diamond/lapis 残留记档】；1297/1285；门 87 结构性
  免疫不重锚；E2E 洞壁矿 A/B 6,703 px【场景火把 + 真眼高 pitch，PITS
  #209 旧二进制 A/B 资产 scratch 纪律】）。发布 **v0.5.0**。**GEN 行
  3/3 全清**，v1.0 任务表推进至 FLUID 行（FLUID-1 设计已定稿）。
