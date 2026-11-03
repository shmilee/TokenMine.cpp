# 版本历史（CHANGELOG）

> 本文件只留 **v2.x** 逐版明细（v2.0.0 = 1.x 收官里程碑 / V2-1 前视角 /
> V2-2 Steve 皮肤 / V2-3 教程系列化 / V2-4 天气 / V2-5 猪 / V2-6 体验
> 打磨批 / V2-7 材料补齐批）。
> v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x（v0.12.0→v0.1.0）→ `docs/archive/v0x/CHANGELOG.md`。逐版细节
> （机制/证据/偏离/性能 A/B）见 `docs/archive/v1x/phase-*.md` 阶段存档与
> GitHub Releases。README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本）。

## v2.9.0（2026-11-03）— V2-9 step-up quirk 修复（player/mob 共享 Entity::stepUp 两 quirk 定向消除）（门 180 全 PASS）

KNOWN_ISSUES 203/382 条「单独立项」两 quirk（ENT-3 探针实证，
player/mob 共享）：**① +X/+Z 恰 1 格宽台阶踏不过**（front 格
`snapped+1` 不对称 = +dir 查贴住格之后那格）→ 修 = `front =
snapped + (w > 0 ? 0 : -1)`（+dir 查贴住格本身，−dir 不变）；
**② 非整数脚高巧合对齐凭空抬升 1.0 再落回**（stepUp 调用点无条件，
「贴住整数块面」位置判非因果判——slab 顶 1.5 行走 face 巧合过整数
+ 前方低半板/同高 slab → 抬升再落回，mob 追逐 frame 606 实证）→
修 = **无状态真阻挡验**（front 列在身体 y 跨内 ∃ solid 格且顶 >
feet 严格不等式——moveAxis 同款行；不加 per-tick 旗标/不改调用
点/零新帧状态）。**整数脚高位级不变**（face 对齐 ⟹ 真 clamp ⟹
新校验恒过；既有 step-up 单测族零改动全过 = 红线）。TDD：3 新
测试（+X/+Z 单格踏过 + slab 顶行走 y 轨迹恒 1.5【相位 8.17 =
pre-fix 逐 0.01 扫相钉】）先在 v2.8.0 树证 FAIL（x/z 9.7 clamp /
maxY 2.5 抬升）后修复 PASS；既有 mob 单格 +X 钉**反转**（
DoesNotStep → StepsUp，ENT-6 封死走廊保留）。2174/2174（+3）、
零 format（25 不动）/ 零 config 新键。门 180：S1 **第 180 次结构
免疫**（四方 RAW 179116 B + 锚 + savenorm EXACT——无输入零位移
stepUp 永不触发）+ S2 **1 格台阶 E2E**（自建平台场景：旧树
**x = 15.700 ×2 精确 clamp**【quirk ① 实机命名复现】vs 新树
踏过 x 25.1/25.1）+ S3 2174 + S6 命名回归（pre-fix TDD 四证）+
**性能 A/B 同世界同成本**（changesApplied 9/1/3 三窗逐位 + 终档 16/16 36/36 集合恒等 + PERF-WIN 全指标同档 + 台阶场景自成本双侧同款 churn 同档）。**键盘协议回归**（PITS 368 已随 X 会话重起
根治——门 177 起鼠标-only 绕过退役）。详见
docs/archive/phase-2.9.0.md。

## v2.8.0（2026-11-03）— V2-8 sky rescan clobber 修复（LGT-1 记档潜伏 bug 定向消除）（门 179 全 PASS）

**光照核心修复**（KNOWN_ISSUES「光照」节划解）：天花板板骑跨光照
footprint 边界时，后一 region 的 sky rescan 把边界格 sky 重写为纯
垂直值（opaque 下 0 / LGT-2 衰减冠下部分值 < 真值），sky BFS 种子
只取 footprint 内 lit 格（开敞源列在境外）→ 水平光流无法恢复 =
**stale-dark/stale-partial 暗带**（1-14 格宽、天花板下，流送 drain
序触发，确定性"错得确定"）。修复 = 记档两选一中的 **sky BFS 加
shell seed**（union 单遍 = LGT-1 已砍除形态不复活）：
`updateRegion` 天空种子盒从 footprint 扩为 **+1 块 shell 环**
（rescan 仍 footprint-only、BFS 写界 inRegion 不变、no-op skip /
propagateAll / 块光段零改动；shell 格持收敛值 = 恢复边界条件，
严格增 BFS 向内推进；**无 clobber 时位级不变**——旧不动点上严格增
不触发）。**独立性判据零改动**（算术实证：标准列盒的 shell 读列
c±2 = 99.10 文档化 1-chunk 边界读环，kLightParallelColumnGap=4 与
confluence 证明原样成立；world/light_job.hpp 零变更）。零 format /
零 config 新键 / 零新数据。

**TDD**：2 回归测试先在 v2.7.0 树上证 FAIL（stale-dark 2464 diff
格 x8..21 全 0 / stale-partial 544 diff 格恰 = 冠下 x8..10 的
11<14/13/12——清点与设计预测精确吻合），修复后 PASS。2171/2171
（+2）、banner v2.8.0、base18 恒等（无输入启动窗 clobber 几何未
触发 = 结构免疫）。CPU 探针（真实资产半板几何）：旧库 = 梯度
14..1 被 updateRegion 毁成全 0（bug 命名复现）/ 新库 = 完整保持
（恢复实证）。

门 179 全 PASS：S1 第 179 次结构免疫（**锚重启后重锚 179116 B**
= 679×744 WM tile 态——/tmp 丢失后从 v2.7.0 净树重导 ×2 恒等，
字节数与原锚吻合 = 连续性；PITS 375 tile 几何协议 + dummy 暖机）
+ S2 clobber 判别（CPU 探针
旧/新互斥 + E2E slab 场景无输入流送：板 x25..63 西区 x32..38 旧树
stale-dark vs 新树梯度恢复，diff bbox @ 板下地台）+ S3 2171 + S6
命名回归 + **性能 A/B 同世界同成本**（changesApplied 9/1/3 三窗逐位 + 终档 16/16 36/36 集合恒等 + PERF-WIN 全指标同档 + slab 自成本同档）。**环境事件**：机器重启后 /tmp 重建 +
**XTEST 键盘恢复**（PITS 368 根治实证 = X 会话重启，W 键位移实证
z 0.5→−3.0，V2-9 起完整输入协议可用）。详见
docs/archive/phase-2.8.0.md。

## v2.7.0（2026-11-03）— V2-7 材料补齐批（planks 块化 / 金钻块 / spider 掉落 / 块级 per-face）

四子项均数据驱动（核心唯一新增 = 块级 `faces` 通用能力，
**无 faces = 位级不变红线**），**零 format（25 不动）/ 零 config
新键**：

- **planks 块化**：新块 `planks`（FullCube 复用图集既有 planks
  tile @ [2,4] **零新纹理**；hardness 2.0 / drops 自身 / sound wood
  / flamable / **fuel_time 7.5** 自物品迁入值不变 = wood.json 块
  属性先例）；`items/planks.json` 删（块物品按名自动注册；同名
  显式文件 = init 点名拒「duplicate item id 'planks'」S6 实证）；
  8 配方（bow/crafting_table/planks/stick/wooden_axe/pickaxe/
  shovel/fence）全按名解析 = 零改动（单测钉）。图标观感 = 3D
  方块体（记档偏离，wool 同名先例 Phase 41）。
- **gold_block / diamond_block**：iron_block 先例复制（hardness
  3.0/5.0 + 9 锭 shaped 配方 ×2 + 图集新 tile）；**`beacon_base:
  true`** 两块入信标配基座集合 {iron, gold, diamond}——等级仍是
  world/beacon 纯函数（3×3/5×5 连续完整层），零核心代码。
- **spider 掉落**：`string ×0-2` + `spider_eye ×0-2`（既有
  count_min/max 随机区间机制，MC 同款值；spider 其余字段零改动
  + 六 mob FNV 钉）+ 2 新 material 物品（螺旋绳 / 紫青眼球 icon
  新 salt）。**弓配方用 planks 不用 string → 两物品纯收集 = 记档
  偏离**（不加配方 = 最小面）。
- **块级 per-face 纹理**：`BlockModel.textures` 扩可选 `faces` 段
  （front/back/left/right 覆写 side，**面外法线定名**——
  front=(0,0,−1) 共享 yaw-0=−Z 约定；top/bottom 归既有键，faces
  内命名 = 点名拒；严格解析：未知键/非串/空串/cross·fence·
  glass_pane 形状全点名拒；`cubeFaceTexture` 方向感知重载 +
  mesher 10 调用点转向 = **per-face 覆写自动断 greedy merge**）。
  **无 faces = 位级不变**（空串回退 side；单测证明除 furnace 外
  全块方向查找 = 旧查找）。首个消费 = **furnace front 单 tile
  `furnace_front`**（宽炉口+余火）；**炉子无 lit 纹理态 = 记档
  偏离**（lit 表现 = 既有炉口火焰粒子）。

图集 102/128（3 新 tile row-7 [10..12,7]，纯 +12 行零移位）、
字体 423 零新字、既有 184 纹理 PNG md5 零变化（5 新 1 删普查
实证）；`-Werror` 0、banner v2.7.0。

门 178 全 PASS：S1 **第 178 次结构免疫**（new/old 60f 四方 RAW
全字节一致 + base18 锚 179116 B + 跨版 savenorm EXACT 零 format）
+ S2 四子项判别全 PASS（planks RMB 放置互斥终态 = 新侧块入 world
@ (0,54,-3) / 旧侧 material 无动作 / 首试命中零 CALIB；炉子前脸
new ×2 恒等 + 新旧 diff bbox 112×107 @ 画面中心 = 前脸纹理区【pose
站 −Z 侧朝 +Z yaw 3.0——首跑从 +Z 侧看 back 面三图恒等零 diff =
面名约定实证，PITS 374②】；beacon gold 3×3 基座光束 = 细竖列
x[316..355] + 基座金色 diff vs stone 对照 None + **旧侧载 gold
场景 init 点名拒 = 可载性互斥** + CPU 探针（真实资产+世界）
beaconLevel(gold)=1/(stone)=0；spider 夜间 scratch spider-only
场景击杀 E2E【PITS 373 敌对生成双门 = 生成保护 12 块 + 夜间被动
cap 挤占】：单跑 hold-swing kill 2 只，掉落两组
string×2+eye×2 / string×1+eye×2 **每组独立在 0-2 区间** +
**复跑两组逐位恒等**，旧侧 v2.6.0 spider.json 无 drops 字段 =
结构互斥）+ S3 2169/2169（+12）+ S6 双点名（`unknown face
'weirdface' in model.textures.faces` / `duplicate item id
'planks'`）+ **性能 A/B 同世界同成本**（900f 无输入：changesApplied
9/3/1 三窗双侧逐位 + 终档 fluids 18/18、blocks 55/55 集合恒等 +
PERF-WIN 全指标同档；FURN 炉子场景双侧 900f = per-face 覆写非空
分支零可测增量）。门协议
发现记 PITS 373/374（敌对生成双门 + hold-swing 帧预算/capture
时延锚/face 面名 pose/symlink 进程名脱管）。详见
docs/archive/phase-2.7.0.md。

## v2.6.0（2026-11-03）— V2-6 体验打磨批（第一人称手臂 / mob 阴影 / 拾取磁力 / 月相）（门 177 全 PASS）

UX-8/9 同款多格批：四子项各消费一处记档偏离，全为表现层/手感层，
**零 format（25 不动）/ 零 config 新键**：

1. **第一人称手臂**（「无手部皮肤——只渲染手持物品」消费）：
   `drawHeldHand` 加臂 viewmodel = 玩家数据模型 **`arm_r` box**
   （data/player_body.json，init 校验存在）re-center 后走既有
   box-model 管线（makeMobModelGeometry：chunk-mesher-verified UVs）
   ——Steve 皮肤 pa_arm tile 六面全贴（**零新纹理**，V2-2 tile；tile
   下 10 行 = 肤、上 6 行 = 袖，π 翻转姿态使肤手朝持物端）；
   **仅第一人称**（门控 = 指针锁定 ∧ 无面板 ∧ First = 既有手持门
   三态矩阵化，`firstPersonArmVisible` 纯函数单测钉）；**无手持也画
   空手（肤手）**；静态（无行走摆动 = 记档偏离）但继承手持 offset
   → 弓拉/进食通道携臂微动；懒构建（无输入运行永不建 mesh =
   结构性免疫）。
2. **mob blob 阴影**（「无 blob 阴影 = 仍活跃」消费）：每 mob 脚下
   （+ 玩家**仅第三人称**，第一人称无自身阴影 = MC 观感）世界空间扁
   quad：双面、半透明黑 **alpha 0.3** 命名常量、边长 = **1.5×模型
   宽**（blobShadowSide 纯函数钉）、中心 = 底心正下方**底格顶面**
   （blobShadowCenter = floor(footY) + 0.01 防 z-fight 抬升）——
   贴地面而非悬浮脚边（MC blob 观感）；UI shader 纯色路径（outline
   范式）+ blend pass（深度写关测开：山丘遮挡、地形承托）；纯渲染
   零帧状态（无新帧状态成员；mesh 懒构建，无阴影可画永不建）。
3. **物品拾取磁力**（「固定 1.0 格半径无磁力」消费）：0.5s 延迟过后
   水平距离 ≤ **kMagnetRadius 2.5** 的掉落物被逐渐吸向玩家——
   **帧锁定**（kMobStepPerFrame 同款固定步长纪律）：位移 =
   f(距离, 帧) 纯函数（entity/pickup_magnet.hpp）：径向速度 =
   kMagnetMaxSpeed 6 ×(1−d/2.5)²（远慢近快 = **加速趋近 + 距离
   衰减**，边缘 0）+ 侧向 **swirl**（24 帧周期、幅值 = 径向 25% =
   收紧螺旋 MC 手感，frame 有界 modulo 纪律）；掉落物步进过
   hasRoomAt（**磁力不穿墙** = MC 规则）；**箭头拾取（Phase 42 同款
   半径 1.0）同步消费** = 插墙箭被同款磁力拉出方块（无 room 检查：
   卡住箭 AABB 本就在 solid 内，stuck 箭不做物理步）；拾取半径 1.0 /
   延迟 0.5s / 入包规则零动（磁力只是把物品送进半径）；无 ready
   掉落物/插墙箭 = 零执行（60f 基线零实体 = 结构性免疫）。
4. **月相**（billboard「无月相」消费）：**8 相 = day cycle 累计天
   index（帧锁定派生，`DayCycle::dayIndex` = floor(start + total/len)）
   % 8**（纯函数 `moonPhaseIndex/moonPhaseOffset` 单测钉：新月同心 →
   满月相切，25/50/75% 象限刻度，盈凸 1..3 滑 +X / 亏凸 5..7 滑 −X）
   ；呈现 = 既有月 quad 上叠暗色「食相」quad（**零新纹理**：sky
   shader 纯色路径同款 28px 圆盘滑移，颜色 = 深夜天色 kNightSky，
   覆盖区读作天空 = 娥眉幻觉）；**绘制序 = 食相先/月后**——
   kSkyDistance 平面 24-bit 深度只分辨 ~160 块，同平面第二绘制只能
   靠严格 LESS 的绘制序取胜（亚块 nudge 被量化吞掉 = 实证废案，
   **PITS 371**）；正午基线月在地平线下 = 整分支不执行（结构性
   免疫）；**星星不动**（无星闪保持记档）。

2157/2157（+22：月相 5【8 相序列/偏移表/单调/天 index/基线钉】/
磁力 9【速度曲线/单调加速/到达零向量/swirl 峰/周期/越界】/ 阴影
3【贴地几何/边长/常量】/ 手臂 5【门控三态矩阵/姿态钉/静态确定/
offset 继承/相机随行】）、`-Werror` 0、banner v2.6.0。基线自证：
60f 无输入 `--screenshot` = base18 锚 179116 B **逐字节恒等**（四子项
全惰性）；功能实证：月相 1/3 两相像素判别（0.25r 娥眉 / 0.75r 凸月
宽度精确）、臂 viewmodel 锁屏态像素带（袖蓝 800px + 肤手 + 石方块
手持）、mob 阴影像素判别（猪/牛脚下草地精确 ×0.7 变暗 + 玩家脚区
100% 无阴影）。门 177 全 PASS：S1 **第 177 次结构性免疫**（60f 四方
RAW 全字节一致 + 锚 179116 B + 跨版 savenorm EXACT 零 format）+ S2
四子项判别全 PASS（手臂 new-locked ×2 恒等 + new vs old 臂区 diff
349×191 右下 / 阴影 mobs-off new≡old 零 quad 免疫 + mobs-on 地面
diff / 磁力 new ×2 恒等 + pork 吸入拾取 vs old 原地不动互斥终态 /
月相 new ×2 恒等 + **diff bbox 恰 28×28 px 月盘 @ 预测位**【存档
time 0.519 + 朝东 pose——PITS 372 载档 time 压过 --time】）+ S3
2157 + S6 点名（arm_r 缺失 init 拒）+ **性能 A/B 同世界同成本**
（changesApplied 9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + mobs-on
阴影 ents 同档 + 磁力场景同档）。门内环境事故 3 条记 PITS
368-370/372（XTEST 键盘服务器级失效 + drawin 窗盖中央点击 + mob 不
持久化 + 载档 time 优先）。详见 docs/archive/phase-2.6.0.md。

## v2.5.0（2026-11-03）— V2-5 新 mob 猪（门 176 全 PASS）

「加 mob = JSON + 纹理」数据驱动承诺的验证 = **零核心代码**：
**pig.json**（passive 10 HP / 0.6 speed / spawn_max_light 15 / drops
pork ×1-2 + leather ×0-1 随机区间既有机制）+ **宽体 6-box 模型**
（body + 4 粗短腿 + head 0.45³ 嵌入 body 前方 cow 范式；4 腿 0.4 rad
对角 swing；64 相位 clip 验收）+ **head `faces.front` = pig_face**
（**V2-2 per-face 纹理的首个 mob 消费**——此前仅玩家身体用过；显式
8×8 猪脸粉/深粉鼻口/2×2 眼 Steve 范式零 hash）+ **pig_body 纹理**
（MC 猪粉 (228,170,160)+noise，新 salt 0x5011；**既有 6 mob tile 176
文件全 cmp 零变化**）+ **pork/cooked_pork 食物链**（+3/1.8 sat →
smelt → +8/12.8 sat = beef 熟同款刻度；icon 生成器新 salt；无新配方
杀猪→熔炼 MC 同款）+ **生成权重** {"id":"pig","weight":1}（6→7 加权
抽取零代码）+ **图集 97→99**（pig_body [8,7]/pig_face [9,7] 进 row-7
空位，97 既有条目**零移位**，99 条全 census 逐格钉）+ **字体
422→423**（猪 U+732A 码点序，顶 512 行逐字节不变免重锚，424 预算剩
1）。2135/2135（+7 新 + 10 既有扩展）、-Werror 0、banner v2.5.0。
门 176 全 PASS：S1 **第 176 次结构性免疫**（60f 六跑 n1≡o1≡o2 跨版
RAW + n1≡m1【mobs-on ≡ off = first_spawn_frame 60 零 spawn 扫描
render-inert】+ m1≡m2 + 锚 179116 B + savenorm EXACT 零 format）+
S2 猪判别（a 生成序列 ×2 恒等 118 行【pig f60+f300 (0.5,-5.5) 外环
优先复现】+ 旧侧零 pig 互斥；b 杀猪链全存档权威：离线 f210 截图粉簇
反投影 + 走位自洽迭代解 KILL pose → 实跑零修正 3+ 命中 iron_axe →
**pork ×1 掉落区间钉** + leather ×0 → 两段接力【mobs-off save-posed
传送到掉落格】拾取/开炉/喂料/熔炼/取排/吃【selected 预置免数字键】
→ hunger 14→20）+ S3 2135 + S6 点名 2/2（unknown mob 'dragon' /
pig_face 图集缺条目）+ **性能 A/B 同世界同成本**（changesApplied
9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + mobs-on 猪自成本 ents
0.04-0.10 ms ≈ 既有 mob 同档无回归）。门内环境事故 3 条记 PITS
368-370（XTEST 键盘服务器级失效 + drawin 窗盖中央 + mob 不持久化）。
详见 docs/archive/phase-2.5.0.md。

## v2.4.0（2026-11-03）— V2-4 天气（雨/雷）（门 175 全 PASS）

帧锁定天气系统（设计定案 11-03 夜间自主推进）：**状态机 = core/weather 纯函数**（campfire 范式，splitmix64 推进式流 PITS 4——主流 seed = 世界 seed【同世界同序列】，雷 = 独立 salt 流 0x7E42B1D9；CLEAR [9000,18000)f / RAIN [3000,6000)f / 雷 [600,1800)f 命名常量；**首段恒 CLEAR** = 无输入 60f 基线永不进雨的结构免疫根）+ **呈现 = rain shader program**（cloud 第四先例：静态 VBO 300 滴出生偏移+相位【0xD93A5E17 专用 salt 流】init 一次上传 + 顶点 shader 逐滴纯函数位置【有界 fallOffset + 玩家 16³ 盒 boxOrigin，per-frame 零上传】+ 0.02×0.5 细竖条 alpha 0.35 白蓝 (200,210,235) 无纹理）+ **天空 dayFactor 消费点单收口**（RAIN ×0.85 + 雷 5 帧线性衰减 +0.15 脉冲——u_dayFactor/clear/雾同源，shader 表达式树零改动）+ **交互四件全帧锁定**（雨灭火块/灭营火 = 新纯谓词 extinguishedByWaterOrRain【6-邻水 ∨ raining，Phase 64/75 路径收口】/ 雨灭燃烧效果【玩家+mob inWater ∨ raining，re-ignite 先行保水胜火序，无屋檐检查 = 记档偏离】/ 雨润耕地【soilIsWet/cropIsWet/cropGrowthSteps 扩参默认 false = 旧语义位级不变，mesher 湿润视觉不传 = 视觉零变，雨中生长 2× = 111/200→200/200 钉】）+ **render.weather strict read 默认 true**（off = 状态机不跑+零雨滴+零交互 = v2.3.0 位级恒等）+ **--weather clear|rain CLI**（仅覆盖初始 state——rain = kRainMin 3000f 固定可复现，主流零消费其后走流；词校验点名）+ F3 `weather clear|rain` 行 + **雷新 wav**（generate_sounds 固定种子 0x70DE 60→40Hz 滚雷 3.5s，既有 39 wav md5 零变化+幂等实证；2D 中心不空间化；**无雨声循环** = 记档）+ **零 format**（25 不动，天气 = session 派生云漂移先例类）。2128/2128（+19：weather 17【设计 §6 八组 + 脉冲/wav 资产】+ F3 WeatherLine + mob ExtinguishedInRain）、-Werror 0、banner v2.4.0。门 175 全 PASS：S1 **三方恒等最强形**（60f 六跑 new-on ≡ old v2.3.0 ≡ new-off ≡ base18 锚逐字节 + savenorm EXACT 零 format）+ S2 雨判别全 PASS（rain ×2 恒等 / 雨可见 / 120f savenorm 精确 churn 判别【rain 侧恒无火 + 非世界键 EXACT】/ 营火+火块互斥终态 / 雷+停雨 4 事件 ×2 间隔 ±0.08s 帧锁定 + 雨窗 50.5 s ≈ kRainMin）+ S3 2128 + S6 点名 2/2 + **性能 A/B 同世界同成本**（changesApplied 9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + 全指标同档；rain 自成本 = 300 滴静态 VBO 在噪声带内）。详见 docs/archive/phase-2.4.0.md。

## v2.3.0（2026-11-03）— V2-3 教程系列化（农耕 + 厨房）（门 174 全 PASS）

用户 11-02 实机反馈「之前的新手指导比较单调，有没有多个任务系列可以添加」：**A 机制系列化**（`tutorials/` 多文件按名序 = 系列序 + `AssetManager::listDir` + 步 id 跨系列全局唯一 init 点名；`TutorialState` +seriesIndex 纯函数推广；**存档 format 24→25** `player.tutorial` 加 `series`——三向键纪律【25 缺 series 拒 / 23-24 携 = 版本谎报拒 / ≤24 载入缺省 0 = 值保真免迁】+ series 越界/id 不在该系列 = 重置上报不拒档；事件词表 +**pickup./smelt.** 两前缀均帧锁定【拾取收口 + 炉子 smelt-complete 消费点】；**T = 跳当前系列**；系列过渡无 toast【目标行换步即提示定案】；F3 行 `<s>/<total> <id> p/c`）+ **B 两个新系列**：② 农耕 7 步（2 种子→2 耕地→2 种→2 收 = 4 小麦→面包→吃，全链数据核实在位零新机制；**×2 修订** = wheat_7 掉 2 < bread 配方 3 的经济账死局修复）+ ③ 厨房 6 步（3 锻石→炉子→放→开→熔炼熟鸡→吃）；**beginner.json 字节零改动**；字体 401→422（+株 等，顶 512 行逐字节不变免重锚）。**门 174 内抓到并修复 UX-7 潜伏 bug**：E 面板 recipe 行左键合成成功漏发 tutorial craft 事件（玩家点配方行合成 = 永久卡教程；1.x 链从未经该路径 = 潜伏两版，PITS 360）。2109/2109（+25）、-Werror 0、banner v2.3.0。门 174：S1 **第 174 次结构性免疫免重锚**（四方 ×2 RAW 全字节一致 + 锚 179116 B 连续 + 跨版 savenorm 恰 2 项【format + series:0 值保真】）+ S2 输入链全 PASS（A 农耕 9 段【FAR 先放 = 累积态遮挡定案 + 生长轮询 + 走捡 + 逐 checkpoint 断言 ×8】/ B 厨房 2 段【开炉重捕获重试 + 燃料点火即耗物品判别】/ C T 三态 + 日志逐字 / D format 24 迁移值保真）+ S3 2109 + S6 精确点名 4/4 + 性能 A/B 同世界同成本【changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55 集合恒等 + 全指标同档】。PITS 359→367（8 条：360 recipe 行漏 emit / 361 format 边界字面量 / 362 教程经济账 / 363 锁定态禁预 move / 364 面板 uiSize 空间 / 365 漂移须重捕获 / 366 燃料点火即耗 / 367 饥饿满 no-op）——详见 docs/archive/phase-2.3.0.md。

## v2.2.0（2026-11-02）— V2-2 玩家皮肤 Steve 化（门 173 全 PASS）

用户实机反馈「看到正脸了，没有眼睛，没有头发，皮肤也丑，请仿照史蒂夫更新」：根因 = 每盒六面全贴同一 tile + head tile 是四侧脸 2×2 拼格 → 五官亚像素。两部分修复：**A 模型 per-face 纹理**（MobBox 可选 `faces` 六面覆写，严格解析 + init 存在性校验点名 box+面；**无 faces = 位级不变**——既有 6 mob 零改动 + 全族测试零语义改动 = 红线证明；角序/绕序零动，摆动 UV 随行零改动）+ **B Steve 风格皮肤**（生成器确定性显式像素：8×8 脸 = 棕发双色调 + 2×2 眼 ×2【鼻侧白 + 外侧蓝虹膜】+ 鼻/嘴阴影行；青蓝衫/蓝裤/棕靴；7 个 per-face 独立 16×16 派生 tile，头 8×8→16×16 2× 精确放大 = 五官像素级；图集 94→97 row-5 空位，**90 遗留条目零移位免重锚**，旧 4 拼格退役）。2084/2084（+5）、-Werror 0、banner v2.2.0。门 173：S1 **第 173 次结构性免疫免重锚**（四方 ×2 + 锚 179116 B + savenorm EXACT）+ S2 Steve 脸 A/B（三模式 ×2 + savenorm 两两 EXACT + 头带 0.6 阴影换算色分类：**旧前视零五官（2840 旧肤）/ 新前视眼白 306 + 虹膜蓝 324 + 发 1421 / 新后视全发 5280**）+ S6 ×2 精确点名 + 性能同世界同成本【首轮 render 偏置重跑证伪 + changesApplied 9/3/1 逐位 + 终档 18/18 55/55 diff 0】——详见 docs/archive/phase-2.2.0.md。

## v2.1.0（2026-11-02）— V2-1 第三人称前视角（2.x 首个功能项，门 172 全 PASS）

用户 11-02 实机诉求「第三视角下能看到自己的前面」= MC F5 前视角。**核心 = 前视是后视同款数学在翻转视角角下执行**（yaw+π/−pitch ⇒ −forward 翻转 = 玩家前方；up_cam 翻转不变 ⇒ v2.0.0 的 0.4 轨道抬升同款继承 = 前视头也在准星下方 29-96px；遮挡收敛/穿流体/embedded 自动对称）——`resolveThirdPersonCamera` 等纯函数零改动。C 键三循环 First→后视→前视→First + 新纯函数 `thirdPersonFrontAngles`；**瞄准 UX-8g 眼位射线 origin/方向零动**（前视方向 = 相机向 = 屏幕中心向自动翻转）+ 新 `aimReach()`（前视 = D+kReach = 玩家身后 5 块有效带 MC 前视惯例，First/后视位级不变）4 消费点 + drawHeldHand 前视隐藏（`!= First`，First/后视逐位等价）+ 身体前视自动渲染（既有门控）。2079/2079（+4：翻转角钉/up_cam 不变/空世界恒等 bit-exact+后视镜像/墙钳止镜像）、-Werror 0、banner v2.1.0。门 172：S1 **第 172 次结构性免疫免重锚**（四方 ×2 + 锚连续 179116 B + savenorm EXACT）+ S2 三模式矩阵（三模式 savenorm 两两 EXACT 不入档 + 皮肤判别 First 0 / 后视 10,151 / **前视 10,125 全 0.6 阴影调 = 脸朝向相机** + 世界朝向 B≠C 10,674 diff 互斥 + 六跑零漂移）+ S2b 前视挖矿互斥（前视破身后 4.5 块靶 vs 后视破面前墙，pitch 翻转场景坑首轮实证）+ 性能 A/B 同世界同成本【首轮 win2 churn 偏置重跑证伪 9/3/1 逐位 + 终档 18/18 55/55 diff 0】——详见 docs/archive/phase-2.1.0.md。

## v2.0.0（2026-11-02）— 1.x 收官里程碑发布（扫尾三件事，门 171 全 PASS）

1.x 任务表 40/40 + UX-8g/UX-9 实机报障 4 项全完成后的里程碑发布（无新玩法），用户 11-02 定案三件扫尾：

1. **1.x 阶段文档归档**：`docs/archive/` 181 份 phase 文档 `git mv` → `docs/archive/v1x/`（保历史）+ 全仓 556 处引用零残留 + BACKLOG v1.x 表标记归档、顶部开 v2.x 任务表占位（待用户定范围）+ AGENTS「现状」节瘦身（1.x 巨段编年史 149k + 逐门记录抽出 → `docs/archive/v1x/HISTORY.md` 只读全史）+ 活文档刷新到 2.0 状态（MECHANICS 12+ 处 / KNOWN_ISSUES 8+12 处 / GAMEPLAY 8 处，全数字源码实证）+ 门协议工具入仓（`tools/gate_compare.py` / `perf_gate_config.json` / `xenv.sh` = PITS 317 重建纪律）
2. **第三人称准星不压头**（用户报障，B 方案 = MC 同款抬相机）：轨道点沿相机 up 轴抬高 `kThirdPersonOrbitHeight` 0.4 块（`Camera::up()` = cross(right, forward) 单一三角真源，头 top 降 29px / 底缘降 96px 低于屏幕中心 = 准星零重叠，任意俯仰角恒定）；瞄准射线两模式仍自眼位（UX-8g 红线零动）= 固定 TP 视差 MC 同款记偏离；First 分支字节不变
3. **发布**：tag v2.0.0 + GitHub release（2.x 迭代起点）

2075/2075（扫尾② +3）、`-Werror` 0 警告、banner v2.0.0。门 171：S1 **第 171 次结构性免疫 + 默认 First 位级恒等 + base18 锚连续 179116 B 免重锚**（四方 ×2 全字节一致 + 跨版 savenorm EXACT）+ S2 TP 准星 A/B 判别 PASS（OLD 中央带 160 皮肤 px = 压头实证 / NEW 中央带 0 + 下移带 720 = 头降 29-96px 解析吻合 + 中心像素 OLD 面部 vs NEW 草逐像素 + 遮挡回归 2390 皮肤 = 相机出墙保持 + 跨侧 savenorm EXACT 同世界最强形）+ S3 2075 + 性能 A/B v1.50.0 vs v2.0.0 同世界同成本（changesApplied 9/3/1 逐位 + 终档 18/18 55/55 diff 0）——详见 docs/archive/phase-2.0.0.md。
