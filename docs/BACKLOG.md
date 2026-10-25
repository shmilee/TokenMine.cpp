# BACKLOG.md — v1.0 任务表（执行权威）

> 来源：2026-10-20 对"文档中所有后续任务"的全量分类（依据 =
> KNOWN_ISSUES 残余/留待条 + `archive/phase-99.x.md`「残余」+ 全仓
> "留待/后续/未来"标记扫描）= 十大系统 63 项；按用户决定（10-20），
> **每类只保留靠前（最重要）一半 = 29 项；10-25 用户新增 2 项**
>（GEN 行 ④⑤：洞穴破碎度 / 浮空树，用户实机体验报障）**+ 10-25
> 交付上报 1 项（GEN 行 ⑥：树放置半区缺失，wz 循环上界 bug，用户定案
> 立项）= 32 项——32 项全部完成 = v1.0 发布**。此后任务只从本表取，
> 不另列菜单；增删格子需用户确认。

## 砍半规则

- 每类只保留前一半（偶 n 取 n/2，奇 n 取 floor(n/2)）；重要性序 =
  用户实痛 > 机制缺口（影响可玩性）> 性能/架构债 > 视觉/音频打磨 >
  MC 细节对齐。
- 砍掉的项不删除：记入「砍掉清单」（保留原代号），可一句话恢复入表。

## v1.0 任务矩阵（32）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ |
|---|---|---|---|---|---|---|
| STRM 流送与内存 | ~~STRM-1 环形状~~ 完成 | | | | | |
| PIPE 帧管线与并发 | ~~PIPE-1 worker网格化~~ 完成 | ~~PIPE-2 存档增量~~ 完成 | | | | |
| LGT 光照 | ~~LGT-1 union-region~~ 砍除 | ~~LGT-2 叶子光衰减~~ 完成 | | | | |
| GEN 世界与生成 | ~~GEN-1 面条洞穴~~ 完成 | ~~GEN-2 树生长~~ 完成 | ~~GEN-3 矿脉形态~~ 完成 | ~~GEN-4 洞穴破碎度~~ 完成 | ~~GEN-5 浮空树~~ 完成 | ~~GEN-6 树放置半区缺失~~ 完成（v0.20.0） |
| FLUID 流体 | ~~FLUID-1 水量守恒~~ 完成 | ~~FLUID-2 游泳划水~~ 完成 | | | | |
| ENT 实体/Mob/移动 | ~~ENT-1 四肢摆动~~ 完成 | ~~ENT-2 BlockState★~~ 完成 | ~~ENT-3 mob踏阶~~ 完成 | ~~ENT-4 水体交互~~ 完成 | | |
| SURV 生存/战斗/效果 | ~~SURV-1 护甲~~ 完成 | ~~SURV-2 氧气窒息~~ 完成 | ~~SURV-3 耐久~~ 完成 | ~~SURV-4 药水饮用~~ 完成 | ~~SURV-5 mob效果~~ 完成 | |
| CRAFT 物品/合成/面板 | ~~CRAFT-1 工作台分块★~~ 完成（v0.21.0） | ~~CRAFT-2 物品3D模型~~ 完成（v0.22.0） | ~~CRAFT-3 材质音效~~ 完成（v0.23.0） | | | |
| SAVE 存档 | SAVE-1 自动存档 | | | | | |
| PRE 表现层 | ~~PRE-1 中文字体~~ 完成（v0.25.0） | PRE-2 边缘雾 | PRE-3 水面波纹 | PRE-4 云与环境音 | PRE-5 透明discard | PRE-6 掉落光照 |

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

- ~~**GEN-4 洞穴破碎度**~~ — **完成（2026-10-25，发布 v0.17.0）**（用户
  报障：洞穴太多太碎、小洞频繁）：GEN-1 面条洞穴参数重标定 =
  **`noise_scale` 0.09 → 0.05**（tube_threshold 0.06 保持；worldgen.json
  唯一内容变更，**src/ 零变更**）——噪声特征周期 ~1.8× = 管体更粗更长、
  小口袋并入主管；t 不动 = n2 短路率持平不加密度（0.05/0.07 聚合 6.55%
  出带证否）。**标定**：探针 `tests/cave_probe.{hpp}` + 独立驱动
  `build/bin/cave_probe`（真实资产 + `WorldGenerator::create` 工厂，
  32×32 设计样本 + **96×96 交叉验证**，seed {123456789,9,42} 全网格 +
  7 抽查，26-邻洪泛连通域统计）；方法论修正 = 单 seed 摆动 ±1.5pp
  噪声主导 → **3-seed 聚合选择规则**（4-6% 带内 + pocket 最少 + 域数
  最少）。**效果（96×96 四 seed 聚合）**：挖除 5.82%→5.12%（−12%）/
  域数 35→8 / **pocket(<8格) 110→10 格（−91%）** / 中位洞体积
  12-42→392-1554 格（9-130×）= "更少更大"直击报障三点。1490/1490
  （+5 新 + 2 契约演进重钉【GEN-3 矿计数派生漂移全在原软带内 / P90
  溢流路由 churn 1→3 湖契约不变，两跑字节一致】）。**门 100**：S2 探针
  统计 gate 内独立复跑 PASS（3 seed 逐 seed 全指标更优）+ 演示区恒等
  结构性（spawn_clear 门控 + cs 只被洞穴谓词消费）+ S1 PASS = 60f 视图结构性免疫（新 ×2 RAW +
  vs v0.16.0 A/B **0 px**——洞穴几何差异在 60f 流送视图外；base18 锚
  刷新；洞口分布全量重排 −23% = 探针实证的世界演进事实）；性能 A/B v0.16.0 fall 900f 三窗 upd/render/flush/player/ents/stream 全同档 = **代码成本零**（fluid 段 +74% 格 = 洞穴重排世界演进，每格成本同档）。零 format（旧存档新洞形 = P86/88/93/GEN-1 先例）。
  GEN 行 4/5（GEN-5 完成 + 上报新条目 GEN-6 待用户定案 → 6 项）。
- ~~**GEN-5 浮空树**~~ — **完成（2026-10-25，发布 v0.18.0）**（用户
  报障：洞穴生成后部分树长在半空中）：根因 = placeTrees 用 heightAt（与洞
  无关）选位 + surface_guard 0 洞口可达表面格 = 悬空树（placeDecorations
  P45 有同款守卫，树漏网）。修复 = carveCaves 逐格判定提纯函数
  `caveCarvesCell(wx,wy,wz,height)`（**逐位搬移**，carveCaves 改调 = 行为
  恒等）+ placeTrees 守卫 `if (caveCarvesCell(wx, hgt, wz, hgt)) continue;`
  （表面格被洞吃 = 跳过整棵树；**跨 chunk 纯函数重算**不读 chunk 数据 =
  整树同生同灭无半截冠，worker 并行安全）；零 format/config/content。
  1494/1494（+4：恒等 A/B×2 + 守卫正反例含跨 chunk seam）。门 101：
  S2 判别场景（玩家 (334,16) 框 (340,12)+(340,16) 两浮空树，old 有树/
  new 无树 A/B）+ S1 基线（spawn clear 区洞穴永不挖 = 构造免疫）+ S3
  套件复跑 PASS（1494 gate 内）。门 101 实得：**S1 PASS = 第 101 次结构
  性免疫**（fresh 60f ×2 RAW + vs base18 字节一致，无需重锚）；S2 截图对 **PASS（补跑收 18:38，gate101_retry att 8）** =
  A/B diff 182,469 world px 质心 (801,369) 两树投影区，颜色定案 OLD 侧
  叶绿+干褐（树体）/ NEW 侧 sky 71% + 背景树（遮挡移除），HUD 0 px
  （PITS 254 场景 cp 坑已修；高频环 8 s/探 + frozen binary 免构建互撞）。shipped 实证：
  346 树列 14 列（4.05%）被跳。性能（插桩 A/B v0.17.0，发布前）：fall_armor 900f 三窗
  **fluidCells 逐位一致**（同世界）+ upd/render/swap/flush/fluid/player/ents/stream
  全指标同档 = **代码成本 ≈ 0**（timing-clean 接受）。GEN 行 5/6。
  **交付中上报潜伏 bug → 新条目 GEN-6（待用户定案）**。GEN 行 5/6。
- ~~**GEN-6 树放置半区缺失**~~ — **完成（2026-10-25，v0.20.0；10-25 交付上报，
  用户定案立项 = 第 32 项；原 GEN-6 耕地湿润在砍掉清单，槽位沿用 GEN-4/5
  改派先例）**——placeTrees/placeDecorations 内层 wz 循环上界误用 X 基
  `limit`（`ox + kChunkSize [+ margin]`，Phase 4 ae19893 起潜伏，102 道门
  未发现 = 基线视图朝 -Z 恒在种树侧）→ **cz ≥ cx+1 半区（约世界一半）整块
  不种树/不放装饰**；修复 = wz 上界改 Z 基 `zlimit`（2 处一行级，
  wx/margin/格网/roll/GEN-5 守卫零改动）+ 2 判别性单测（pre-fix FAIL 实证
  git stash 验证）；1521/1521（+2）、0 警告；零 format（18）/config/content
  = 世界生成演进（旧存档新树先例 P86/88/93 + GEN-4/5 同款）。探针量化
  （shipped seed 123456789 真实资产）：门场景盒树列 13→33（2.54×）/装饰
  5→20、演示盒 24→30/+2、前瞻半区锥内 215 列（游戏内视野）。门 103：
  S1 PASS = 第 103 次结构性免疫（fresh 60f ×2 RAW + vs base18 字节一致，
  **免重锚** = 区内 gained 5 特征全在视锥外【探针视锥论证 + 像素终裁
  互证】）+ S2 树群判别场景（(10.5,42,30.5) 斜视 (8,32) h40 t7 新增树，
  old=frozen v0.19.0/new 各 ×2 RAW 双侧一致 + **A/B diff 476,835 world px**【NEW 侧叶绿 44%+干褐 55% = 树干+树冠 vs OLD 侧草/天空 = 树添加像素定案，补跑收 23:2x retry att 28；提交时首轮窗几何混合 PITS 249 → retry v3 尺寸门】）+ S3 套件 1521（跨 chunk
  seam 一致性 = 新测 #1）；性能 A/B v0.19.0（发布前，timing-clean）：fall + tree 双场景对三窗
  fluidCells 双侧逐位一致（同世界）+ upd/render/swap/ents 全同档 =
  **成本可忽略无回归**（2 行纯 worldgen 零每帧代码，树密度 2.54×
  渲染成本环境带内不可见）。
  **执行序（10-25 用户定案）= SURV-5 之后 = v0.20.0**（只 tag，release ④
  覆盖 v0.20-22 = CRAFT 行全清）。

### FLUID 流体
- ~~**FLUID-1 水量守恒**~~ — **完成（2026-10-22 夜→23 晨）**：根因 = 三条无记账体积变化路径（落池抬升【唯一"凭空增多"】/ 浅膜吸收 / 世界底漏排）；**step-0 混沌探针触发翻转**（统一吸收把悬崖薄膜级联变永久 period-2 翻转：环 24→196 churn 格、总编辑 6.4×、超出 Q3 授权 = 回炉）→ **路线 A 定案**：只删 `L>B → propose(下方,L)` 抬升一分支（`L≥B → 该格保留`静态沉积稳压器保留）= 守恒目标完整达成 + churn 回旧水位，**净核心 diff = fluid.cpp 一分支**；level 即 MC flow time 反相编码 = **零新状态零 format**；守恒不变量 = 逐 tick 会计恒等式 ΔF=σ−α−κ−δ（封闭盒 200 tick 逐 tick 断言）+ C1-C4 派生断言；熔岩同改（Q1）；游戏侧零代码（桶/反应锚定 L8=源恒等）。1312/1312（15 新 + 2 适配）、0 警告；base18 **第 88 门 = 结构性免疫不重锚**（门 85-88 连锁）；E2E：S2 封闭盒 600f ×2 RAW + **64 格流体与 probe 精确终态表逐位一致** + S4a 深湖静态 + S4b P90 **新旧二进制 core 逐段 identical**（v0.3.0 scratch A/B）；KNOWN_ISSUES 守恒条移入已修复 + 新增"站流水瀑布/崖膜永久轮换 24 格类（接受现状）"。
- ~~**FLUID-2 游泳划水**~~ — **完成（2026-10-23 夜）**：被动上浮
  （buoyancy 12 / cap 1.5，vy≥0 门 = 设计案唯一自洽读法）+ 水面漂浮
  （W' = 中心列最高 L8 +1 几何钳制，kMaxSubstepMove 0.4 带，fps 无关
  终态）+ 潜行下沉 1.5 b/s + Space 水面正常跳 8.5（Q3）+ 水面速度三门 +
  config 三新键 / water_gravity 退役（P74 容忍）；熔岩/空气分支逐位未
  动；1328/1328（+16）、0 警告；门 89 = **结构性免疫不重锚**（与门 88
  状态锚字节一致，锚重拍 P36 先例）；E2E S2/S2b 浮漂 y==47 精确 ×2
  EXACT【深度无关】+ S2c 落水 y==42 池底互斥 ×2 EXACT【场景预置 bug
  feet 49 头嵌盒顶 → 48 修正】；S3 水下输入三项留新机器（2fps 墙钟
  hold 抖动，PITS 有录）；5 处设计案偏差全部机制级合理（commit 有
  录）。发布 **v0.7.0**。FLUID 行 2/2 全清。

### ENT 实体/Mob/移动
- ~~**ENT-1 四肢摆动**~~ — **完成（2026-10-24 夜）**：mob 摆腿动画（99.x 残余 #4）：**swing 相位 = per-mob 整数**（`MobEntity::m_swingPhase` 0..63，移动帧 +2【整周期 32 帧 = 0.533 s】/ 静止帧 −4 衰减到 0【16 帧收步】，「移动」= 帧边界水平位移严格不等式【不读 AI 意图】；纯渲染层 = 不入存档【mob 本 ephemeral】+ 无 swing 数据类型恒 0 结构性惰性）+ **constexpr 16.16 定点正弦表**（64 条、编译期 14 项 Taylor 生成、零运行期 libm【std::sin 跨平台位级不稳定】；余数界 x²⁹/29! ≤ 9.2e-9 ≪ 半量子）+ **数据驱动 `swings` 段**（box 可选 `name` + 模型 `swings: [{part, amplitude (0,1] 弧度, group 0/1}]` 严格解析、part 存在性解析期交叉校验【grows_to 纪律、文件内引用】、未知键/重名/重复认领点名拒）+ **CPU 逐帧顶点变换**（pivot = box 顶面中心纯函数、绕 mob 局部 X 轴【yaw 前】、定点 sin/cos 整数 Taylor 每部件预计算、法线同旋【face shade 桶跟随】、phase 0 = 位级恒等 = 静态 fast path 结构性免疫）+ **per-mob 动态 VBO**（`Mesh::uploadDynamic`/`updateVertices` GL_DYNAMIC_DRAW、首摆帧分配、updateEntities 拆除 pass 释放）+ **config `mobs.limb_swing`**（默认 true、false = v0.9.0 静态基线 E2E 对照组）+ **内容**（chicken 2 腿 0.5 rad 半周相位 / cow·sheep 4 腿 0.7 rad 对角步态 / spider 8 腿 0.9 rad 偶奇分组；**zombie/skeleton 无独立 arm part = 不造几何记档**）；**AI/物理/存档零改动**；1367/1367（+17）、0 警告；base18 门 = 结构性免疫（基线帧无 mob = 99.7 先例）；偏离：翅不摆（MC 只飞行时扇翅）/ zombie·skeleton 无臂 / 相位不入档 / 幅度上限 1 rad
- ~~**ENT-2 BlockState★**~~ — **完成（2026-10-22 夜）**：P94 楼梯朝向（World 每格 byte + format 14 `stairs`）泛化为**通用 per-instance 状态机制**：`Chunk::m_blockState` 换名 + 哨兵泛化 255 + 双门控数据驱动（Stairs 形状隐式 count 4 ∪ `model.state_count`[1,255]+`state_default` 严格配对，楼梯显式声明 = 矛盾拒）+ `World::get/setBlockState`（只验 <255，域由收口/旋转/parse 三处强制）+ Game 三写入点泛化（收口/旋转 `(s+1)%stateCount()`/P97 钩子）+ **format 15 `states` 段**（全量 + 逐条目域校验 + 双向交叉校验 + 键纪律三向 + 14→15 值保真迁移 + collectSave 两路 journal 派生）+ **init 不变量**（worldgen 5 类放置源 stateful 点名拒 = "stateful 格必 session 派生"结构化）；**四个 per-block map 不迁移**（byte/struct 两类状态消费者不相交 = YAGNI 定案，CRAFT-1 取 campfire 范式 + 纪律）；shipped 内容零字段改动；1260/1260（+20 新，存量换名适配语义不动）；base18 **第 85 门 = 结构性免疫不重锚**（新 ×2 与门 84 锚字节一致）；E2E：S2 预置状态场景 A/B **298,211 px = per-instance 朝向首次像素级验证**（P94 时代仅存证）+ round-trip 截图字节一致 + S3 迁移 shot 与 S2A 字节一致（值保真端到端背书）；晨间潜旋回归推迟（pent2_rot.zsh 就绪）；PITS +1（#208 场景几何探针纪律）
- ~~**ENT-3 mob 踏阶**~~ — **完成（2026-10-25）**：step-up 纯几何从 Player 上提为 `Entity::stepUp`（受保护，player 行为**逐位不变** = `test_player.cpp` 零改动全过钉死）+ MobEntity 在 `moveInSubsteps` 前后镜像调用（pre-substep 水平速度捕获【被 clamp 轴子步内清零】+ `onGround` 门 + `normalized(preMove)` + 本 tick 距离重放 ≤ 终端速度 38/60 ≈ 0.633 < 1.0 无穿透 = 硬规则 6 共存）——判定与 player **同一个函数**：P99 TRUE-1-block（solid 区顶高 ≤ 1.0+1e-6：slab 0.5/满块 1.0/玻璃板 1.0 可踏，墙/栅栏 1.5 帽不可踏，楼梯 = 脚位点采样）+ front 格上方 2 格空 + face 贴整数 ±1e-4 + X 先 Z 后每 tick 至多一次；AI/速度/子步节奏零改动、零 format/config/内容（mob ephemeral）；帧锁定纯几何（无 real dt/无随机 = 硬规则 8）。1376/1367（+9：踏满块/踏 slab 贴顶 1.5 exact/不踏墙/不踏栅栏/不踏 2 格高/帧锁定双体位级一致/击退入阶 hasRoomAt 全程/单格 −X 可踏/+X 钉死 quirk）、`-Werror` 0 警告；门 93：竞技场（12×12 平坦 + 3 高墙封闭 + 满宽 2 格厚 barrier——**2 格厚 = 绕开 +Z 1 格宽 quirk**，PITS）三态互斥 PASS【new+台阶 zombie 追逐 7.8 块对角路径**踏阶跨越**至攻击位连击击杀 idle 玩家（hp<20）/ old v0.10.0 二进制+同场景 hp=20 卡台阶 / new+全墙 barrier hp=20 不可踏；×2 RAW 字节一致 + savenorm EXACT】+ 摆动可见性（on/off 157,158 world px 隔离，config 三验协议）+ base 门对 gate88 锚（结构性免疫：基线帧无 mob）+ 插桩性能（render med 0.47-0.60 ms / ents 11-26 ms/300f = 成本可忽略）。**顺带记档 2 个 P16 潜伏 quirk**（探针实证 + player 同款：① +X/+Z 方向 front 格查 snapped+1 → 恰 1 格宽台阶踏不过（−X/−Z 正常）② 非整数脚高行走 face 巧合对齐整数 → 凭空抬升 1.0 再落回——均为共享几何的既有行为，修复 = **player 行为变更**单独立项，本 phase 只记档不修）。
- ~~**ENT-4 水体交互**~~ — **完成（2026-10-25，发布 v0.12.0）**：mob/掉落物游泳、浮力（实体条）——玩家 FLUID-2 的**无输入孪生**上提 `Entity` 共享（`refreshFluidState`：inFluid = AABB 严格重叠水或熔岩【player isInWater/isInLava 同款】+ W' = 中心列最高 L8 +1【扫深 32，同款循环】+ atSurface 0.4 带 + **kind 记忆**【漂浮终态脚面离水由 atSurface 带维持——空中沿用最后浸没 kind，否则熔岩漂浮终态塌，首轮单测实证】；`integrateFluid`：浮漂 snap【交叉重查 + 上浮速门，终态 y=W'/vy=0/onGround=false = fps 无关不动点】> 被动上浮【buoyancy 12 帽 1.5，vy≥0 门 = 带动量入水沉底 MC dive 语义 + 池底站立 fresh resting 门 + 流膜无面门】> 下沉【同值下漂帽 −4】，流体中零重力；`isRestingOnSolid` 纯上提 = player 逐位不变 FLUID-2 单测零改动）。mob：AI 状态机零改动（追逐/游走照常）+ 涉水/水下水平 ×`kUnderwaterSpeedFactor` 0.6（AI 与击退同门，无 per-mob 数据字段 = YAGNI 记档偏离）+ **水下禁 step-up**（ENT-3 门 +`!m_inFluid`）+ 子步峰值速度流体中 cap 峰集（陆上公式逐位未动）+ **站立 mob 不上浮**（同 MC——mob 上浮只经击退上抛）；item：同款浮力（预置静水浮面静止无漂移【摩擦保留 = MC 简化记档】+ 带动量坠水沉底站立互斥）+ **熔岩对称同浮力**（player 熔岩弱节奏 = 记档偏离）；箭穿过水不动（投射物边界记档）；P58 燃烧零改动（MobFireTest 熔岩池坐底同速率钉死）；零 format/config/内容、帧锁定（1/60 无 real dt 无随机）。1400/1400（+11：mob 7【判定/沉水上浮 W'=3 exact 不动点/坠水沉底互斥/涉水 ×0.6 ratio/水下不踏阶 vs 陆上跨越互斥/双体 300 帧逐位/陆上 inert】+ item 4【浮面 exact + 滑行静止/坠水沉底互斥/熔岩对称/陆上 inert】）、干净重建 `-Werror` 0 警告、陆上逐位不变（存量 1389 零回归 + 双 inert 锚）。**门 94**：S1 base 对 gate88 锚（结构性免疫）+ S2 item 浮面 **39.0 exact**/沉底 **36.0 exact** 互斥 ×2 RAW + savenorm EXACT（fire=9999s 对照 blocks 924/924 EXACT）+ S3 涉水三态互斥（arena#2：5×5 岛 level 39 + 满宽水膜环 + **高架干道**【level 39 实体桥——膜水沿同层干道漫灌 L8→L7 探针实证 f38=7 → 通道变水道零生成的根治】+ 1 行水隙；st1 new+膜 生成干道→涉水隙→卡岛南面 Δ3.80>1.6 y=38 静止 **hp=20** / st2 old v0.11.0+膜 同场景同生成点 踏阶上岛 Δ0.99 **hp=0** / st3 new 无膜 陆上跨越 **hp=0**；×2 RAW ×3 + savenorm2 EXACT ×3）。**场景设计三轮失效根因链**（PITS 243-245）：① 生成环审计须用 spawner 精确语义【整数格偏移环 dx²+dz²∈[25,36] + floor band ±kSpawnFloorDelta 2 + headroom 2 格含流体 + 未编辑自然地形=潜在生成平台 + 环边界 ±0.5 jitter】——墙顶/岛缘/自然地形生成平台曾致三态全 hp20 两轮（zombie 生成在 y41/y43 墙顶被墙封死）；② 膜水漫灌 L8→L7 流；③ 玩家预置格半格行界（z-18.5 属 z-19 行 = 预置进 seal 墙 → 玩家被推出 y43）。工具定案：`tm_scene_check.py` v2 跑前秒级预校验（负例验证拦全部 4 类失效）+ `tm_run_lib.zsh` 黑相 1 帧探针协议。**存档 lake 火 real-dt 漂移**（非缺陷，PITS 244）：shipped 表面湖 (24,26) 帧 0 点燃（backtrace 实证 FluidSim::tick→setBlock），火寿命 4s = real dt → 跨墙钟跑存档 blocks 段 ±13 格漂移；fire=9999s 对照 EXACT。性能（插桩 A/B v0.11.0，发布前测试）：render med 0.41-0.72 ms（old 0.42-0.84 同档）/ ents 2.7-3.4 ms/300f（含 1 涉水 zombie；old 2.3-3.6 同档）/ vd16 填充全指标一致 = **ENT-4 成本可忽略**。ENT 行 4/4 全清。

### SURV 生存/战斗/状态效果
- ~~**SURV-1 护甲**~~ — **完成（2026-10-25，发布 v0.13.0）**：12 件护甲 = **MC 精确值**（leather 1/3/2/1 = 7 / iron 2/6/5/1 = 14 / diamond 3/8/6/3 = 20，max_stack 1、材质档三链；chain 无材料 = 记档偏离）+ **MC 整数 floor 减伤**（`Player::damage` 单收口 = **全伤害源生效含坠落**【摔落缓冲已覆盖】，次序 armor → resistance → 吸收池，`floor(amount×(100−4×pts))/100`，**pts 0 位级恒等** = 存量伤害测试零改动）+ **右键穿戴/交换/脱下**（点火器之后、潜行旋转之前；空手按 head→chest→legs→feet 序脱下到选中槽——**震荡语义**：下点 = 重穿，脱整套须每点之间 Tab 换空槽【热栏只有 Tab 循环无数字键 = 既有交互面】；10-25 门内修复脱下 **dangling reference**【引用跨清空 = 护甲件消失】→ 逻辑上提纯函数 `item/armor.{hpp,cpp}` `armorRightClickUse` = campfire 范式 + 9 单测含回归，PITS 246）+ **format 16 `player.armor`**（恒写只含占位槽、双向交叉校验 + 槽名-物品数据点名、15→16 迁移空）+ **HUD 心行上方护甲排**（仅占位槽且指针捕获时绘制 = 无甲零绘制结构性免疫）+ cloth 音效复用 + F3 armor 点行。1430/1430（+21 首轮 / +9 修复）、`-Werror` 0 警告。**门 95**：S1 base 对 **gate88 锚字节一致（真免疫）** + S2 坠落 13 格 = **10.0 / 15.6 / 18.0 三值互斥 ×2 RAW**【旧 v0.12.0 无甲 = 10.0 回归恒等；floor(10×44)/100 = 4.4 / floor(10×60)/100 = 6.0】+ S3 穿戴 armor={head:iron_helmet} ×2 RAW + 整套脱下（L R Tab R Tab R Tab R → slots0-3 序 + sel=3）×2 RAW + savenorm2 EXACT ×2【fires remaining real-dt 剥离 = PITS 244 非缺陷】+ S4 护甲排 **81 px 包围盒 = 恰 4 件皮甲图标行、带外零差异**（HUD 仅指针捕获时绘制 = 捕获+重捕协议，"Pointer captured" ×1 实证 ×4）+ S6 init 失败点名 ×2【`armor_points must be in [0,20], got 21` / `armor_slot 'hood' is not a known slot`】。**PITS 247 = T450 输入送达协议**（PID 定位窗口 + viewable+top 窗口期 + 每输入前 windowfocus --sync + 1200f 长跑 + 黑相 1 帧探针门控）。**偏离**：float HP 无半心量化（1.0 伤×3 点 = 0.88 非 0）、全源减伤（窒息 = SURV-2 届时 MC 豁免规则）、mob 无护甲、无耐久（= SURV-3）、无附魔。性能（插桩 A/B v0.12.0，发布前测试）：fall 900f 三窗 render med 0.37-0.47 / upd med 45.8-48.7 / stream 1.8-2.4 s 全指标同档（old render 0.37-0.45 / upd 46.7-49.9 / stream 1.8-2.4 s）+ vd16 填充 render 0.71-0.72 vs 0.69-0.75 / stream 44.7/30.5 vs 45.6/30.8 ms = **护甲成本可忽略**。SURV 行 1/5。
- ~~**SURV-2 氧气窒息**~~ — **完成（2026-10-25，发布 v0.14.0）**：水下氧气条 + 窒息（溺水）伤害，**MC 语义查证落地**（minecraft.wiki 伤害调整表：Drowning/Suffocation = **Armor: No effect / Resistance: Applies**——SURV-1"届时按 MC 豁免规则"的立项兑现）：帧锁定氧气系统（150 空气 = `kOxygenMax`，头没入每 6 帧 −1 = 900 帧 15.0 s 耗尽，耗尽后每 150 帧 = 2.5 s 2.0 伤，出水立即满；时间线钉死：满空气帧 894 耗尽 / 帧 1044 首伤 / 预置 0 → 149,299,…,1499 十次 = 死）+ **头没入 = 眼位单格流体判定**（`eyePosition()` 含 crouch 缩减与相机同款；水/熔岩同款 = MC 眼在水里规则——站 1 格浅池眼上水面 = 换气不扣）+ 纯函数 `player/drown.{hpp,cpp}`（campfire 范式，Game 帧 pass 薄适配）+ **`Player::damage(float, bypassArmor = false)`**（溺水跳过 SURV-1 floor 减伤步，resistance 与吸收池照常；默认参 = 既有调用点位级不变）+ **format 17 `player.oxygen`**（恒写 [0,150]、键纪律全套点名、15/16→17 迁移满空气、drownTicks 不存 = MC 只存 airSupply 同款记档）+ **HUD 气泡排**（水下时替换饥饿排原位 = MC 空气条语义，10 泡 ceil(氧/15) 单 tile 无半态，新纹理 oxygen.png salt 105 确定性生成）+ F3 oxygen 行；零 config 键（MC 值命名常量）、无气泡声、mob 不溺水（MC 同款）、无呼吸附魔/水肺（项目无对应系统）。1448/1448（+18）、`-Werror` 0 警告、存量 1430 零回归。**门 96**：S1 base = **新旧 A/B 同窗态字节一致**（历史 gate88 锚因 away 重平铺尺寸失配弃用 = PITS 249，NEW vs v0.13.0 fresh 60f 全帧一致 = 更强免疫证明）+ **死亡场景 RAW ×2 字节一致**（1500f 全时间线 s2e/s2b）+ 存档级确定性 savenorm3 EXACT ×5【剥 fires remaining/hunger/age real-dt】+ **S2 溺水死 hp 0**（10 次 ×2.0）/ **S2b 满空气 hp 12.0 exact + o2 0**（双节奏帧精确）/ **S2c 满钻甲 hp 0 死 = 豁免实证**（误走全源减伤会 hp 16 活，死/活互斥）/ **S3 浅池换气 o2 150 + hp 20 恒定**（池底不上浮 pos 钉死）/ S5 600f hp 12.0（4 次 149-599）+ o2 0 在档 fmt 17 + S6 init 失败 ×3【版本谎报/越界 151/非整数点名】；S4 气泡排像素隔离：首轮 deep/shallow 场景对弃用（几何不同 = 覆盖层全帧差）→ 修正协议（同 deep 几何只改 o2：s4a 150 / s4b 135 → 600f 终态 50/35 = 4/3 泡互斥、hp 20/20），**状态级隔离已证**（savenorm 剥离后唯一差异 = o2 50/35），像素对 = 补跑清单（10-25 午后黑相波 6 轮未得双干净帧）。**环境坑 PITS 248-250**：zsh 空变量算术恒 0 = 黑帧静默判 GREEN（全批假 GREEN 教训）/ 重平铺改窗尺寸 = 跨时段 RAW 对失效 / 1 帧探针 ≠ 整跑干净。性能（插桩 A/B v0.13.0，发布前测试）：underwater 900f + vd16 填充 render/upd/player/ents/stream 全指标同档 = **成本可忽略**。SURV 行 2/5。
- ~~**SURV-3 耐久**~~ — **完成（2026-10-25，发布 v0.15.0）**：工具/护甲/弓/点火器耐久（MC 值实证，wiki Durability 页查证）：27 件内容全 MC 精确值（wood 59 / stone 131 / iron 250 镐斧锹；弓族 5 件 384；燧石与钢 64；护甲 leather 55/80/75/65、iron 165/240/225/195、diamond 363/528/495/429 = 11:16:15:13）+ `durability` 严格字段（允许集 = Tool ∪ armor ∪ 发射器 ∪ 点火器，其它点名拒）+ **`ItemStack.damage` float**（堆移动随堆；**合并门 = 同物品且同 damage** 记档简化）+ 纯函数 `item/durability.{hpp,cpp}`（applyDurability / armorWearPerPiece = max(1, incoming/4) float 精确；破碎 = 动作完成点清空，现代 MC 行为）+ **五类帧锁定磨损**：破非瞬断块 +1（hardness 0 = 0）/ 镐斧锹打 mob +2 / 射出 +1（含 infinite）/ 点燃 +1 / **护甲每次"实际减伤"事件每件 −max(1,incoming/4)**（溺水 bypass = 不磨损，SURV-2 对齐；本项目饥饿/火 tick/虚空/poison 也磨损 = 记档偏离 MC）+ **format 18**（damage >0 才写三处；armor 满耐久保字符串形 = 与 fmt17 逐字节同形免疫最强、受损翻严格对象；≤17 带键/对象形 = 谎报点名；damage ≥ durability = 破碎态非法点名）+ **HUD 2px 三色耐久条**（绿≥1/2/黄≥1/4/红<1/4 = MC 渐变简化；满耐久零绘制结构性免疫；drawIcon 共享点全覆盖）+ F3 手持 d/max + 破碎音复用 break wav（护甲无声记档）；**无修复系统**（铁砧/磨刀石/Mending = 矩阵外记档）。1466/1466（+18）、`-Werror` 0 警告、存量 1448 零回归。**门 97**：S1 base = **NEW vs v0.14.0 fresh 60f RAW identical**（免疫）+ **S3 坠落护甲磨损**：hp 10.8（= 20 − floor(10×92)/100 减伤实证，对照无甲 10.0）+ **铁盔 damage 2.5 exact**（max(1,10/4)）×2 savenorm3 EXACT + **S3b 溺水豁免**：满铁甲 1500f **hp 0 死**（bypass 实证，误走减伤会 hp 11.2 活）+ **4 件 damage 恒 0** ×2 savenorm3 EXACT + S6 init 失败 ×3【apple 带 damage 点名 / damage 250 ≥ 250 破碎态点名 / fmt17 带 damage 谎报点名】；**S2 挖矿破碎（10-25 午后补收 PASS）**：600f 输入对 s2m_a600 碎（249→槽空）/ s2m_b600 存活（248→249）+ cobble 掉落 (22.5,38,−2.5) 在档（1500f 长跑 entities=[] 而 600f 有 = 未解 quirk，600f 证据有效）；**S5 耐久条像素（10-25 午后补收 PASS）**：912 px 完整分解 = 条 288 px 只在 damaged（hotbar 槽 0 + viewmodel 两 2px 带，100/250 = 黄段）+ 护甲行 624 px 只在 full（场景皮甲差异，双区互斥）；**S4 弓射出 = 补跑清单**（10-25 14:3x-14:5x 相位 8 次未送达，存档证据协议已备）。**场景坑 PITS 251**：format 升级场景漏 player.oxygen 必需键 = 6/7 场景 init 静默失败（误诊 20 min）。性能（插桩 A/B v0.14.0，发布前）：fall_armor 900f 三窗 render/upd/player/ents/stream 全指标同档 = **成本可忽略**。SURV 行 3/5。
- ~~**SURV-4 药水饮用**~~ — **完成（2026-10-25，发布 v0.16.0）**：药水系统 =
  饮用通道给效果（weakness 等效果的游戏内来源，消费 status_effect.hpp
  "weakness 无游戏内来源 = potions not built yet" 记档）：
  **饮用通道复用**（P43 drink_time real-dt 1.6 s = MC 32 tick 同款、
  returns_item 返还既有、milk 音效开始播放既有、中断不消费既有）+
  **`grants_effect(s)` 放开到 drinkable**（原 food-only；一个物品一个
  通道纪律不变；duration 放宽 >= 0）+ **效果新字段 `instant_hp`**
  （整数非 0、[-32,32]、与 tick_type 互斥、显式 0 拒；施加时结算一次：
  正 = heal 钳 base max / 负 = damage(bypassArmor=true) = 护甲豁免
  Resistance 照常——MC 药水伤害语义，SURV-2 溺水同款管线）+
  **`Game::applyPlayerEffect` 收口**（替换吃单值/吃列表/mob 命中 3 既有
  施加点 + 新饮用侧 = 玩家效果施加单入口；RESET 语义既有零改动 =
  再喝同效果重置不叠加；无 grants 的 drinkable = clearStatusEffects
  原行为位级不变）+ **init 交叉校验**（grants 条目 duration==0 ⇔
  效果 instantHp != 0，双向点名）；**内容**：2 新效果（healing +4 /
  harming −6，icon 8/9）+ 8 新物品（glass_bottle collects_fluid water
  桶对范式 / water_bottle pours 回瓶纯容器不可饮 / 6 药水 drink_time
  1.6 + returns glass_bottle + max_stack 1：weakness 90 / speed 180 /
  regeneration 45 / poison 45 / healing 0 / harming 0 = MC 精确时长）+
  hotbar.json 隐藏 starter 槽 17-23（GEN-2 范式，无输入零渲染）+
  effects.png 128→160 尾部 append（既有 8 tile 逐像素零变化验证）+
  8 物品 icon 独立 PNG（salt 128-135 接续）；**零 format（18 不变）/
  零 config / 零新音效**。1485/1485（+19）、`-Werror` 0 警告、存量
  1466 零回归。**门 98**：S1 base = **fresh 60f 新版 ×2 RAW + vs
  v0.15.0 二进制 A/B 字节一致**（第 98 次过门，结构性免疫不重锚）+
  S6 init 失败 ×5 全点名【grants 非 drinkable 拒 / 未知效果名 /
  duration 0 给非瞬时 / instant_hp 显式 0 / 桶对 (water,glass_bottle)
  双 full 唯一性破（点名被破坏侧）】；【S2 时长互斥 / S2b 重置 /
  S3 瞬时互斥（harming+满钻甲 = bypass 实证 10.0 vs 14.8）/ S4 瓶-水
  循环 = 输入跑，采集器 v3 运行中——结果待回填】。**偏离**（KNOWN_
  ISSUES [SURV-4] 条）：无酿造台（starter 发放）/ water_bottle 不可饮
  （无中性饮用）/ 瓶舀水移除水源（桶对范式）/ 瞬时伤害护甲豁免（MC
  语义）/ 再喝 = 重置（项目 apply 语义）/ 单效果瓶。性能（插桩 A/B
  v0.15.0，发布前）：fall_armor 900f 三窗 fluidCells 逐位一致 +
  upd/render/flush/player/ents/stream 全指标同档（swap med ~0.5 ms
  无 stall = 深黑相 timing-clean 接受协议首用）= **成本可忽略**。SURV
  行 4/5。
- ~~**SURV-5 mob 效果**~~ — **完成（2026-10-25，发布 v0.19.0）**：状态
  效果双载体（玩家 + mob）全数据驱动——效果侧 3 新字段
  （`affects_mobs` / `min_health_mob` 整数 0/1 缺省跟随 min_health /
  `undead_exempt`×mob `undead` 组合门）+ **wither**（1.0/4.0s 无地板
  双目标可杀，MC wither I）+ poison `min_health_mob 0`（mob 可毒死，
  玩家地板 1.0 不动）+ MobEntity 持 StatusEffects（count==0 fast-exit，
  Heal no-op 记档）+ Projectile 尾部 `effectId/effectDuration`（默认 −1
  位级恒等，9 参 legacy 形钉死）+ 双源 `arrow_effect`（skeleton
  {wither,10} / poison_bow {poison,60} = MC arrow of poison 1:00；item
  侧要求 shoot_speed + **affects_mobs**，mob 侧要求 shoot_range，init
  双向交叉校验点名）+ skeleton/zombie `undead: true`（不死族免疫
  poison，wither 唯一伤不死族）+ poison_bow（durability 384 弓族，配方
  bow+potion_poison 不还瓶记档）+ effects.png 160→176 尾部 append
  （既有 10 tile 逐像素零变化 + 77 icon 逐字节零变化 + 生成器幂等）。
  **零 format（18）/零 config/零新音效**。1519/1519（+25：解析/坏值/
  载体行为 ×6/内容钉死/投射物缺省位级）。门 102：S6 3/3 点名 PASS +
  **S1 PASS = 第 102 次结构性免疫**（fresh 60f ×2 RAW + vs base18 字节
  一致）+ S2 探针 PASS（skeleton-only 场景 420f：effects 含 wither
  remaining 4.2 + hp 8 存活 = 接线全链实证；PITS 255-258 场景四坑）+
  S2 截图对**补跑收 PASS**（×2 RAW 双侧一致 + 存档断言 wither/hp 9 vs
  []/16 互斥；0 px A/B = 世界逐位一致，心行 locked 门控无输入不绘制）；
  S3 毒杀 cow/S4 zombie 免疫 = 输入跑补跑清单（行为层 test_mob 已证）。性能 A/B v0.18.0（发布前）：fall 900f 三窗 fluidCells 逐位一致（同世界）
  + 全指标同档 + skel 场景 1200f ents 段双侧同档（new 更低 = 玩家被
  wither 更早死战斗活动减少，非代码差）= **成本可忽略无回归**（fast-exit
  实证）。**SURV 行 5/5 全清，release ③ = v0.19.0**。

### CRAFT 物品/合成/面板
- ~~**CRAFT-1 工作台分块★**~~ — **完成（2026-10-26，v0.21.0）**：全游戏共享
  `m_tableGrid` → 每 crafting_table 块独立 `CraftingGrid`（`m_tables` map，
  炉子 P44/营火 P71 per-block 范式，不消费 ENT-2 状态【工作台无朝向】）+
  `openTablePanel(pos)` 绑定所点块（`InventoryUI::bind` 零视觉变化，E 面板
  终生绑定 2×2 零改动）+ setBlock 收口（**破坏 = 容器物品丢弃** = 炉子同款
  记档偏离）+ **format 19 `tables` 段**（位置序 + 双向交叉校验 + 键纪律
  【19 必 tables 拒 table_grid / ≤18 反之】+ 18→19 parse 时 sole 归属迁移
  【legacy 非空 ∧ 恰 1 表 → 归属，0/多 → 丢弃】）+ `serializeSlotEntry`
  提取（furnace lambda 复用；GCC16 -O3 误报消除）。1535/1535（+14：save 12
  + UI rebind 隔离 2）、0 警告、headless 冒烟（19 写读 + 18 遗留读全过）。
  门 104：S1 PASS = 第 104 次结构性免疫（fresh 60f ×2 RAW + vs base18 字节
  一致，免重锚 = crafting_table 零 worldgen/biomes 引用 → m_tables 恒空）+
  S2 双工作台互斥输入跑 **PASS**（14/14 断言：NEW 链 A=beef@0∧B=coal@4 互斥+持久；OLD frozen v0.20.0 反例 = 共享 table_grid 单容器 {0:beef,4:coal}；×2 savenorm；协议坑 PITS 260【持食物右键 = tryEat 吞点击 / 窗中心 = 隐藏 starter 槽 24 / ui.scale 1.5 布局空间换算】）+ S3
  套件 1535；性能 A/B v0.20.0 fall 900f 三窗 fluidCells 逐位一致 + 全指标
  同档 = 成本可忽略（结构惰性实证；table 场景对弃用 = 首轮场景注入 bug PITS 251 复现，修后两侧同构无代码路径差）。
- ~~**CRAFT-2 物品 3D 模型**~~ — **完成（2026-10-26，v0.22.0）**：物品 JSON 可选
  `model` 段（1..16 axis-aligned box + 单纹理 + uv 可选，严格解析：未知键三层点名 /
  from<to / uv 退化矩形 / 空 / >16 全拒）+ 纯函数 `makeItemModelGeometry`（新文件
  item_model_mesh，kCubeFaces 角序/法线泛化，faceShade 纪律，居中单位空间，双面关裁剪）
  + init per-item 模型纹理（icon 文件存在性交叉校验，texture==icon 双上传 ≈64 KB
  VRAM 一次性）+ `itemModelMesh` 惰性缓存（幂等纯几何）+ 手持接入（替代 icon 面片，
  弓拉/进食举到嘴变换零额外代码继承，block 立方手不动）+ 世界掉落接入（替代不可见
  掉落，blending pass only，帧锁定 bob+spin）；16 内容模型（工具 9/弓 5【master_bow
  无 model】/苹果 2，纹理 = 既有图标 PNG，零新纹理）。无 `model` 物品三上下文位级
  不变（`it.model.has_value()` 静态门控）；零 format（19）/config/音效。1549/1549
  （+14：解析 8 + 网格几何 6 含绕序守护/Z-fight 守卫）、`-Werror` 0 警告。门 105：
  S1 PASS = 第 105 次结构性免疫（fresh 60f ×2 RAW + vs base18 字节一致，免重锚 =
  手持仅 locked 渲染 + fresh 零掉落 + HUD 2D 不动 + 纹理只 init 加载）+ S2 掉落场景
  A/B PASS（3 model 物品 600f 无输入，×2 RAW/侧 + A/B diff 181 采样 px ≈724 实际 px
  限世界区 HUD 零 + 双侧 entities 位级一致 = 纯渲染实证；坑 = scratch 资产漏同步
  PITS 255-258 第 4 次）+ S3 套件 1549；性能 A/B v0.21.0（发布前必做）fall + drop
  双场景对 900f 三窗 fluidCells 双侧逐位一致 + 全指标同档（drop 对 render med
  +0.02-0.03 ms = 3 物品 box 成本同档内）= 成本可忽略。H1/H2/H3 手持输入跑 = 补跑
  清单（PITS 260 协议）——详见 `docs/archive/phase-craft2.md`
- ~~**CRAFT-3 材质音效**~~ — **完成（2026-10-26，v0.23.0，CRAFT 行 3/3 收尾）**：
  块 JSON 可选 `properties.sound_material`（严格字符串 + 显式空串拒）+ init
  交叉校验（引用材质 ⇒ `<m>_dig/_place/_break.wav` 3 件文件级存在，点名缺失
  件 = registry 序首败块 + 事件序）+ **SoundBank mono→stereo** + 纯函数空间化
  （`distanceGain` 线性衰减 cutoff `kSoundCutoffBlocks=16` 命名常量零 config /
  `panGains` 恒定增益线性声像 gL+gR=1 / `toStereo` 16-bit 钳制）+
  `play(name, optional<Vec3> origin)` 新 API + `setListener(相机)` 帧始同步
  + 6 调用点位空间化（dig tick/破坏完成/放置/潜行旋转/丢弃/物品拾取；手持
  破碎/step/shoot/eat/ui_* 保持 2D 位级不变）+ 内容 = generate_sounds.py
  确定性扩展 **6 材质 × 3 事件 = 18 新 wav**（stone/wood/gravel/sand/cloth/
  glass，固定种子 LCG/sine；既有 17 字节零变化 md5 实证）+ **32 块赋值**
  （stone 8 含矿脉族 MC 语义/wood 3/gravel 1/sand 3【sandstone 系】/cloth 15
  【wool 全染变】/glass 2）；无 `sound_material` 块三事件名位级不变；零
  format（19）/config；音频不入帧状态。1567/1567（+18：空间化纯函数 8 +
  事件名/校验 8 + 解析/内容钉死 2）、`-Werror` 0 警告。门 106：S1 PASS =
  第 106 次结构性免疫（免重锚 = 渲染零触碰 + 无输入零音效事件）+ S3 套件
  1567 + S6 init 失败 ×2 点名 PASS + 性能 A/B v0.22.0 fall 900f 三窗
  fluidCells 逐位一致 + 全指标同档 = 成本可忽略（结构惰性）；S2 音色听感 =
  用户实机（无头不可听记档）——详见 `docs/archive/phase-craft3.md`

### SAVE 存档
- ~~**SAVE-1 自动存档**~~ — **完成（2026-10-26，v0.24.0，SAVE 行 1/1 收尾）**：
  `save.auto_save_seconds` 代码默认 120.0（**config.json 零改动**，fire 节同款模式；
  显式 0 = 关闭，负数/非数字点名拒）+ **真实 dt 自动存档计时器**（`--save` 槽门控
  = 无槽结构惰性；`autoSaveStep` 纯函数 campfire-tick 范式；成功归零/失败不清零
  下帧重试无退避 = 记档简化）+ **原子写**（`writeAtomicFile` 同目录 `.tmp` +
  `std::filesystem::rename` POSIX 原子，F5/退出/自动三路共用增强；任一步失败删
  tmp + 原 MC_FAIL 语义，失败写永不毁旧档）+ 日志行可区分（`World auto-saved to`
  vs 既有 `World saved to`）。纯函数提取 3 件（config 严格读/计时器步进/原子写）。
  1590/1590（+23：config 11 + 步进 5 + 原子写 7）、`-Werror` 0 警告、零 format
  （19）；门 107：S1 PASS = 第 107 次结构性免疫（fresh 60f ×2 RAW + vs base18
  字节一致，免重锚 = 槽门控 + 60f 实时间 ≪ 120 s 窗内零事件）+ S2 自动存档实证
  （mov-config interval=3：600f 日志 3 行 auto-saved 实间隔 3.017/3.016/3.016 s
  + 退出行可区分 + 终档 time 0.268333 精确匹配 + blockEdits 9→10→13→14 增量
  journal 经自动路径工作实证 + ×2 终档 savenorm EXACT）+ S2b 崩溃韧性（kill -9
  中途：slot 可解析 format 19 中途快照 time 0.2567 + 零 .tmp 残留 = 半写档
  不可达）+ S6 config 点名 ×2（-5 / "fast"）+ S3 套件 1590；性能 A/B v0.23.0
  fall 900f 三窗 fluidCells 123879/167781/236573 双侧逐位一致 + 全指标同档 +
  双侧 auto-saved 事件 = 0（跑期 ≪120 s 结构惰性实证）= 成本可忽略。
  记档偏离：真实 dt 节奏（MC 按游戏 tick；本项目生存计时器纪律一致）/ 无多槽
  轮转与"保存并退出"流程（矩阵外残余）/ 失败重试下帧级无退避——详见
  `docs/archive/phase-save1.md`

### PRE 表现层
- ~~**PRE-1 中文字体**~~ — **完成（2026-10-26，v0.25.0，PRE 行 1/6）**：
  UTF-8 解码渲染（纯函数 utf8NextCodepoint 严格 1..4 字节，坏字节逐字节
  `?` 回退 = 旧语义保持）+ CJK glyph 静态表 32（18 UI 字 + 10 中文标点
  + 4 余量，离线 Noto Sans CJK SC em 16 提取 = ASCII 同款 .py 静态表
  范式，TTF 仅工具期）+ **图集 rows 0..3 死格复用**（ASCII 控制字符 cell
  恒透明从未被采样 → 图集 256×512 尺寸零变化 + glyphUv(32..126) UV
  位级零变化 = 双红线字面满足；ASCII 区逐像素 diff 0 实证 + .py 重跑
  md5 钉死）+ textWidth/顶点按 codepoint 迭代（CJK 16px 等宽 advance）
  + 8 处 UI 字符串中文化（死亡屏 ×2/熔炉/工作台/背包/合成/需要：/按 E
  关闭 ×2；F3 调试屏保持英文 + 物品名数据驱动 id 不译 = 记档偏离）。
  1610/1610（+20）、`-Werror` 0 警告。门 108：S1 PASS = 第 108 次结构性
  免疫（免重锚 = addText 19 调用点全枚举门控零绘制）+ S2 死亡屏文本 A/B
  PASS（900f 坠落死场景，×2 RAW/侧 + diff 1911 采样 px 限中部文本带 +
  双侧存档一致）+ S3 套件 1610；性能 A/B v0.24.0 fall 900f 三窗 fluidCells 逐位一致 + 全指标同档
  （重跑 ±1% 级，首轮 stream 偏 = 页缓存环境噪声重跑证伪）= 成本可忽略
  ——详见 `docs/archive/phase-pre1.md`
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
  至 29 项 + 10-25 用户新增 2 项（GEN-4/5）+ 10-25 交付上报 1 项（GEN-6，执行序 SURV-5 后）= **32 项 = v1.0 范围**；执行顺序与前置约束见上。
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
- 2026-10-22（夜→23 晨）：FLUID-1 完成（水量守恒：根因三路径无记账；
  step-0 探针触发翻转【统一吸收 = 崖膜永久 churn 6.4×】→ 路线 A = 只删
  抬升一分支 + 静态沉积稳压器保留；零 format【level 即 flow time 反相】；
  会计恒等式 ΔF=σ−α−κ−δ 逐 tick 单测；1312/1297；门 88 结构性免疫不重
  锚；E2E 封闭盒 64 格精确终态 + P90 新旧二进制 core identical）。发布
  **v0.6.0**。FLUID 行 1/2（FLUID-2 设计 ora 运行中）。输入 E2E 积压
  （重启后桌面态：root-top 点击 + xdotool 键名 Left_Control 失效 → 已修
  Control_L + xprop 回退，等窗口重跑）。
- 2026-10-23：输入 E2E 积压清理（本机 T450，原 10-23 定案"留新机器"
  的积压在本机提前清完）：7 项全部验证输入链正常（lock/crouch/
  click/hold/walk），**零产品代码缺陷**——失败全部 = 环境（i915 GLX
  间歇黑相，PITS 有录：黑帧跑存档完全有效 → 输入跑按协议以存档为权威，
  take 门放宽为"帧满+存档"）或场景设计缺口。完整协议 ×2 savenorm
  EXACT：**ENT-2 潜旋**（states 1→2 + 相机钉死）/ **GEN-2 潜旋催熟**
  （states 0→1 精确 + 树苗保留 + ×2 EXACT）；功能性通过：**GEN-2 放置**
  （放置 ✓ 槽消耗 ✓ ×2 savenorm OK；stage 漂移 = 生长引擎活跃，"stage 0"
  断言需暗室场景重建——留新机器）/ **GEN-2 破坏**（块消失 ✓ 状态清 ✓
  掉落→拾取 ✓ slot 1→2；×2 受流送环自然世界 fps 耦合 churn 阻塞——留
  新机器）/ **GEN-3 挖矿**（矿消失 ✓ 石保留 ✓ coal 掉落实体 ✓；×2 同
  churn + 断言 kind 字段 bug 记档——留新机器）；部分：**GEN-1 进洞**
  （走位输入 ✓ ×2 逐位一致 z=4.7 撞墙停；阈值 5.5 不可达 = 场景走廊
  几何死路，需重建——留新机器）/ **LGT-2 放叶**（点击送达 ✓，放置被
  plant_on 正确拒绝 = 瞄准打石地，场景瞄准重建——留新机器）。harness
  修复 8 处 + 场景 format 15 修正 3 个（均入资产 tarball，PITS +5）。
- 2026-10-23（夜）：FLUID-2 游泳划水完成（被动上浮 + 水面漂浮几何
  钳制 + 潜行下沉；5 处设计案偏差机制级合理；1328/1312；门 89 结构性
  免疫不重锚【与门 88 状态锚字节一致】；S2/S2b y==47 深度无关 ×2
  EXACT、S2c y==42 互斥 ×2 EXACT【场景预置修正】；S3 输入三项留新
  机器）。发布 **v0.7.0**。FLUID 行 2/2 全清，v1.0 任务表 11/29。
- 2026-10-24（夜）：ENT-1 四肢摆动完成（swing 相位 per-mob 整数 +
  constexpr 16.16 定点正弦表零运行期 libm + `swings` 数据段严格解析 +
  CPU 逐帧顶点变换 per-mob 动态 VBO + config mobs.limb_swing；chicken
  2/cow 4/sheep 4/spider 8 腿，zombie·skeleton 无臂记档；AI/物理/存档
  零改动；1367/1350 +17、0 警告）。ENT 行 1/4，v1.0 任务表 12/29。
- 2026-10-24（夜，补记）：99.10 光照 job 并行 + 99.11 流体光照异步化
  发布（性能系列，非任务表项；99.11 = 用户卡顿深层根因修复——流体 tick
  巨盒主线程同步 re-light → 列粒度待集 + 每帧 2 列预算 worker 追赶；
  vd16 填充 244.7→44.6 s 5.5×、静置 26× 实时、update p95 7.6 s→80 ms）。
  发布 **v0.8.0 / v0.9.0**。
- 2026-10-25：ENT-1b 模型修复（用户实机反馈：四足 body 长>高对齐 MC
  比例带 + 关节出体 + 摆幅下调 + 蜘蛛腿加长外移 + 穿模谓词
  checkSwingClip 全相位 896 验收；zombie·skeleton 人形未动）。
- 2026-10-25：ENT-3 mob 踏阶完成（step-up 纯几何上提 Entity::stepUp，
  player 行为逐位不变 = test_player 零改动钉死；mob 镜像调用
  pre-substep 捕获 + onGround 门 + 本 tick 距离重放 ≤0.633 无穿透；
  判定与 player 完全一致 = P99 TRUE-1-block：slab 0.5/满块 1.0/玻璃板
  可踏、墙/栅栏 1.5 不可踏；帧锁定纯几何无 real dt；零 format/config/
  内容；1376/1367 +9、0 警告；门 93 竞技场三态互斥【new+台阶 zombie
  跨越击杀玩家 hp<20 / old v0.10.0+台阶 hp=20 卡住 / new+墙 hp=20
  不可踏；×2 字节一致 + savenorm EXACT】+ 摆动可见性 157,158 world px
  on/off 隔离 + base 门对锚 + 插桩性能 render med 0.47-0.60 ms）。
  记档 2 个 P16 潜伏 quirk（+X/+Z 恰 1 格宽台阶踏不过 / slab face
  巧合抬升——player/mob 共享，修复 = player 行为变更单独立项）。
  ENT 行 2/4，v1.0 任务表 13/29。发布 **v0.11.0**（含 ENT-1b）。
- 2026-10-25：ENT-4 水体交互完成（玩家 FLUID-2 无输入孪生上提 Entity 共享；
  mob 涉水/水下 ×0.6 + 禁 step-up + 漂浮不动点；item 浮面静止 + 熔岩对称；
  箭不动；陆上逐位不变；1400/1400（+11）、门 94 全绿【base 对锚 + item
  39/36 exact 互斥 + 涉水三态互斥】、插桩 A/B 成本可忽略；场景设计三轮
  失效根因链 + 工具定案 tm_scene_check/tm_run_lib PITS 243-245）。
  ENT 行 4/4 全清，v1.0 任务表 14/29。发布 **v0.12.0**。
- 2026-10-25：SURV-1 护甲完成（12 件 MC 精确值 + floor 减伤全源含坠落 +
  右键穿戴/交换/脱下 + format 16 + HUD 护甲排；1430/1430（+21/+9）、
  门 95 全绿【base 对 gate88 锚 + 坠落 10.0/15.6/18.0 三值互斥 ×2 RAW +
  穿戴/整套脱下 RAW+savenorm2 EXACT + 护甲排 81 px 包围盒隔离 + S6 ×2】；
  门内修复脱下 dangling reference → 纯函数 item/armor + 9 单测 PITS 246；
  T450 输入送达协议 PITS 247；插桩 A/B 成本可忽略）。
  SURV 行 1/5，v1.0 任务表 15/29。发布 **v0.13.0**。
- 2026-10-25：SURV-2 氧气窒息完成（帧锁定氧气 150/6 帧/150 帧 2.0 伤 +
  眼位单格水/熔岩判定 + 溺水豁免护甲 = MC 伤害调整表实证 + format 17 +
  HUD 气泡排替换饥饿排；1448/1448（+18）、门 96：新旧 A/B 基线字节一致
  + 死亡 RAW 对 + savenorm3 ×5 + 豁免死/活互斥 + S6 ×3；S4 像素隔离 =
  晨间补跑【黑相波 PITS 250】；插桩 A/B 成本可忽略）。
  SURV 行 2/5，v1.0 任务表 16/29。发布 **v0.14.0**。
- 2026-10-25：SURV-3 耐久完成（27 件 MC 精确值 + ItemStack.damage +
  五类帧锁定磨损【护甲 −max(1,incoming/4) float 精确、溺水 bypass 不
  磨损】+ format 18 + HUD 2px 三色耐久条；1466/1466（+18）、门 97：
  S1 新旧 A/B 免疫 + S3 盔 2.5 exact + S3b 豁免死/恒 0 互斥 + S6 ×3；
  输入跑 S2/S4/S5 = 窗口相位关闭补跑清单；场景坑 PITS 251；插桩 A/B
  成本可忽略）。
  SURV 行 3/5，v1.0 任务表 17/29。发布 **v0.15.0**。