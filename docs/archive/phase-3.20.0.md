# Phase v3.20.0 — V3-14b ghast（门 210 全 PASS，2026-11-06）

## 范围与定案

**ghast = 飞行远程攻击者**（bat 飞行数据旗 + skeleton 射击数据融合；
项目首个爆炸射手，V3-14a 爆炸机制的首个游戏内消费方）：

- 数据：health 10 / speed 0.7 / detect 16 / shoot_range 14 /
  shoot_speed 10 / cooldown 4.0 / **explode_radius 3** /
  shoot_max_light 15 / hostile / flying / spawns_in_air / ambient
  "ghast" 8.0 / drops ghast_membrane 0-1 / xp 5；model 1.5×1.75
  五盒（body + 4 tentacles）。
- **explode_radius 数据属性**（strict 非负数；>0 须 shoot_range>0——
  爆炸近战是笔误，弹是爆轰唯一载体）；**ammo 规则豁免爆炸射手**
  （火球撞击即消耗永不钉住 = pickup 不可达，skeleton 的 ammo 规则
  对 explode_radius 0 的射手不变）。
- **shoot_sound 数据字段**（默认 "shoot" = 位级 pre-v3.20.0；
  ghast = "ghast_roar" 发射前咆哮；init 交叉检 wav 存在 =
  ambient_sound 孪生，文件口径 headless 亦严格）。
- **flyShoot 分支**（runAi 飞行分支内，`p.flying && p.shootRange>0`
  门——bat shootRange 0 走 flyCruise 原文位级红线）：seesPlayer
  （alive + detect + kDetectVertical 2.0 + shootRange）→ RangedAttack
  态 + 面向玩家 + 水平速度归零（hover 保持 = integrate 的 bob 基线）
  + 冷却射击；否则 bat cruise 原文（wander + ambient roll 都在那里）。
- **火球 = Projectile 复用**（damage 0 / hurtPlayer true / 无 ammo
  无 effect / explodeRadius + explodeSource="ghast" 尾参缺省 0 =
  旧调用位级）；爆炸接线三处：stuck → explodeAt+kill（钉住即爆，
  永不钉住）/ 玩家接触 → explodeAt / **mob 扫描跳过爆炸弹**（火球
  穿 mob = MC 孪生）。
- 内容族：ghast.png 16×16 自绘（atlas [9,10] 空格零移位，165→166）
  + ghast_membrane 物品 + 32×32 RGBA 透明底图标【**物品图标 = 独立
  PNG 非 atlas tile**——atlas 只加 ghast 块面】+ 声音 ghast（环境
  嘶鸣 0x6600 定种 1.4s 0.5 ambient class，61739 帧 peak 16734）/
  ghast_roar（发射咆哮 0x6E0B 0.5s SFX class，22050 帧 peak
  25351；49→66 幂等，既有 64 件位级不变）+ 下界池 +ghast weight 2
  （总权重 33，尾追加既有 5 条目位级）。
- 渲染：火球 = 单色发光立方（kFireballSize 0.4，色 1.0/0.55/0.15，
  无旋转——blob 任意朝向同读；drawArrows 分支 + buildFireballMesh
  惰性 = 箭三段孪生）。
- 零 format 31 不变 / **sim 不 bump**（下界惰性 + canary EXACT）/
  无击退（项目无击退面 V3-15 先例）/ 破碎无掉落 / 点火 100%
  确定（V3-14a 爆炸语义继承）。

**门内捕获（字体修复并入本版）**：V3-14a 给字体加的「第二爆炸字」
= **爽 U+723D**（笔误，本意 炸 U+70B8）——charset/表/PNG/单测四处
一致地错，`被 ghast 的爆炸杀死` 首渲染（门 210 死因屏 crop）显
「爆?杀死」。修复 = charset 0x723D→0x70B8 换入（码点序 0x70B9..
0x7206 九 glyph 重格 +1，0x7248 起净零）+ 生成器重跑（TTF pin /
legacy 33 位级 / **top-512 红线 131072 px 恒等**）+ snippet 回贴
font.hpp + 测试重钉（font.png FNV 94cbd43cfb750568 →
0fce6a0766e78125）。**修复后 S1 ≡ base19 字节恒等**（锚场景无
消费方文本 = 第 91 次结构免疫）。教训入 PITS 402④。

## 实现（代码 12 + 数据 9 + 工具 2 + 测试 10 + CMakeLists 1）

- `src/entity/mob_registry.{hpp,cpp}`：explodeRadius/shootSound
  字段 + strict 解析 + ammo 豁免 + shoot_range 交叉检。
- `src/entity/mob_entity.{hpp,cpp}`：flyShoot 分支（飞行 dispatch
  内数据门）+ 声明。
- `src/entity/projectile.{hpp,cpp}`：explodeRadius/explodeSource
  尾参（缺省 0/{} = 位级）+ explodes()/explodeRadius()/
  explodeSource() 访问器。
- `src/entity/arrow_mesh.{hpp,cpp}`：kFireballSize/Color +
  makeFireballGeometry（共享 makeBox 单色路）。
- `src/core/game_entities.cpp`：shoot 回调火球参数（exploding 门）+
  shoot_sound 数据驱动 + arrowSteps 三爆炸接线 + drawArrows 火球
  分支 + buildFireballMesh。
- `src/core/game_init.cpp`：shoot_sound wav init 交叉检
  （ambient_sound 孪生）。
- `src/core/game.hpp`：m_fireballMesh + buildFireballMesh 声明。
- `src/ui/font.hpp`：kCjkGlyphs 重格（0x70B8 @371，九 glyph +1）。
- assets：ghast.json / ghast_membrane.json / 池 / atlas / ghast.png
  / 图标 / 双 wav / font.png。
- `tools/generate_sounds.py`：ghast + ghast_roar（定种，幂等）。
- `tools/generate_font.py`：charset 0x723D→0x70B8 + 自动表同步。
- tests：+12（MobRegistryTest 爆炸面 ×5 / MobFlyShootTest ×4 /
  SoundGhastTest ×2 / GhastDataPinned）+ 全套重钉（atlas 166 /
  池 6·33 / 声音 66 / item 98 ×4 / 字体 FNV + cell）。

## 门 210（全 PASS）

- **S1 结构免疫第 91 次**：60f 无输入 ×2 ≡ **base19 225272 B
  字节恒等** + savenorm EXACT（字体重格后复验——锚场景无 9 个
  移位 glyph 的消费方文本 = 位级不变）。
- **S2 下界决定性场景 v5**（player nether 高原 (40.5,53,40.5)
  pitch 0 + obsidian 平台 4 格【br 3 负对照】+ 环清 y50-52 三带
  【kSpawnFloorDelta 2 → 沟底 top 50 出带 = 地面 pick 全败】+
  obsidian 顶盖 x34-47 z34-47 y57-121【in-air 顶向下扫描压 feet
  55 = kDetectVertical 2.0 恰线】+ netherrack 架 4 格 y56【br 0
  正对照】；13332 nether 编辑 + effects/absorption 必需字段）：
  1800f ×2 截图 **RAW 恒等** + **savenorm EXACT** + 终档 9/9
  【架 4/4 破【**玩家接触爆炸所破**——架在玩家头顶 1.2-3.0，
  飞行火球爆点在顶盖/沟底】/ 平台 obsidian 4/4 存 + 顶盖采样
  4/4 存【br 3 = r 3 恰界】/ player health 0 + deathCount 1
  【4 接触 ×~6-10 衰减伤害】/ fires [] / nether_entities []
  【**mob 不入档**——format 30 设计项 = items only】/ obsidian
  census 12744 = 顶盖 12740 + 平台 4】。
- **S2 决定性时间线**（debug 探跑 1800f，×2 逐位复现）：地面
  pick 全败（环清）→ f240 ghast#1 生 (40.5,55,34.5)【权重流
  [31,33) 首抽 + in-air 首格】→ 发射 4s 冷却节奏【f240/f483/
  f724】→ shot1 顶盖卡壳自爆【弹道补偿 0.5·g·t² = 3.9 块 @6.26m
  速度 10 = 朝天花板上扬，ghast 鼻尖 1.91 内自伤 5（血 10→5）】
  → shot2 弹道恒等过预视点 = 玩家眼接触【爆点在玩家盒：玩家
  ~10（20→14）+ 头顶 netherrack 架破】→ shot3 沟底 netherrack
  卡壳（3 深沟底 top 50）自爆（破沟底 + release 日志）→
  f870 ghast#2 生【同确定性 cell——#1 已漂移 1.6 出
  kSpawnClearance 1.0；非重生】→ f1110 ghast#3 生 (37.5,55,35.5)
  + 双 ghast 同帧发射 → f~1100-1150 玩家死【deathCount 1，
  nether 未重生 = 死亡屏等 R，headless 无输入】→ 余程 3 ghast
  flyCruise（playerAlive 门 = 零发射，日志实证）。
- **S2 视觉**：492f 单跑截图 = **火球发光立方 + ghast 五盒模型**
  同框（ghast 眼高带 ±kDetectVertical 内、距相机 4-6m）；1800f
  终帧死因屏 crop = **`被 ghast 的爆炸杀死` 全行渲染正确**
  【字体修复 E2E 实证——修复前 crop 显「爆?杀死」】。
- **S6 点名**：temp 资产移走 ghast_roar.wav → init rc 1
  `mob 'ghast' shoot_sound 'ghast_roar' is missing
  'sounds/ghast_roar.wav'`（恢复后 init 正常）。
- **S2b canary 4 demo** ×2 截图 RAW + savenorm EXACT（266/561/
  571/541 帧，**sim 不 bump 实证**）。canary 须 `--frames > total`
  ——headless --demo 无 --frames = 播完进 finished panel 观察
  循环永不退出（PITS 402⑤）。
- **S3 全套 2833/2833**（+12），`-Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = e1320b4 v3.19.0（git archive）
  + g139 vs 新 = 工作树 + g139（单次插桩 + 300f 冒烟验 PERF-WIN
  出数），900f 无输入 tools/perf_gate_config.json 独占——banner
  实证 v3.19.0/v3.20.0 + PERF-WIN 三窗 upd med 0.578/0.604/0.640
  vs 0.549/0.530/0.557 + render med 0.645/0.701/0.698 vs
  0.633/0.556/0.637 + swap med 15.197/15.176/15.107 vs
  15.325/15.362/15.289（同档，新侧 upd 略快 = 噪声带）+
  fluid·fires·spread·ents·stream 同档 + **changesApplied 9/2/1 vs
  9/3/1**【win2 ±1 = streaming 边界 real-dt 域，v3.14.0/门 209
  先例】+ **终档 savenorm EXACT**（零 format = 最强形——ghast
  全冷路径【下界惰性 + 无输入无 mob 射击】结构性零成本）。
  门内小捕获：perf 脚本 GC 变量 zsh 不分词致 savenorm 行失败
  （手动补跑 EXACT）+ 冒烟行漏 --save（--new requires --save
  CLI 纪律）+ **双 ninja 同 build 目录竞写 .o = 构建事故**
  （pkill pattern 未匹配 cwd 致旧全量构建残留，插桩器/构建
  严禁并发纪律的构建侧实例）。

## 门内捕获（PITS 402）

1. **下界决定性场景构造三件**：环清三带（kSpawnFloorDelta 2 →
   沟底 floor top 出 [player±2] 带 = 地面 pick 全败 cap 永不
   饱和）+ obsidian 顶盖（in-air 顶向下扫描首命中 = feet 55，
   kDetectVertical 2.0 对玩家眼恰线 dy = |53−55| = 2.0 可见）
   + 场景手术必需字段（player.effects format 8+ / absorption
   format 12+，pop 掉 = init 点名拒载）。
2. **in-air 顶向下扫描的 cell 口径**：air box = model height 占
   格数（ghast 1.75 → 2 格）；顶盖从 y58 起 = feet 56（眼差
   3.0 不可见不发射 = scene v3 失败因），y57 起 = feet 55（恰线
   可见 = 定案）。
3. **爆点 = 火球盒底位**（arrow->position()，非眼非盒中心）：
   接触爆点在玩家盒边、卡壳爆点在盒底——正/负对照格的判定距离
   必须按盒底算（架在玩家头顶 1.2-3.0 = 接触爆破，飞行火球
   爆点距架 4.3+ = 不破）。
4. **字体码点笔误四一致**（charset/表/PNG/单测全一致地错 =
   一致性 pin 抓不到语义错）：单测 pin 的是「430 码点排序/
   cell 连续/PNG FNV」，不 pin「这个码点是不是这个字」——
   E2E 死因屏 crop 目检 = 语义门。修复后 top-512 红线 + S1
   锚字节恒等 = 重格面最小化实证。
5. **canary headless 必须 --frames > demo total**：--demo 无
   --frames = 播完进 finished panel 观察循环（等观众 ESC/Q，
   headless 永等）；timeout 杀进程留下的 mid-run autosave 双跑
   「EXACT」= 假阳性证据陷阱（两次在同一模拟时刻 auto-save，
   恒等但不代表终态）。
6. **bob 恰线闪烁**：spawn 定案 dy = 恰 2.0 = kDetectVertical
   边界——bob 把 feet 抬过 55.0 即 |dy| 2.0+ 不可见 → 瞬回
   flyCruise 漂移（探跑实证 ghast 4s 内漂 0.3-1.6）→ 再可见
   回 hover。确定性（frame-locked bob）但发射节奏非严格 4.0s
   整数倍。

## 记档偏离

- **火球 = 箭弹道**（g 20 + 弧线补偿 0.5·g·t²）：MC 火球近
  直线，近程（<8m）补偿项 2.4+ 块 = 发射朝上（6.26m → 3.9 块
  上扬）；开阔下界弹道恒等保证过预视点（打得到），有顶盖场景
  打天花板上自爆 = 场景语义非 bug（记档）。
- **射手自伤**：explodeAt 无 source 豁免（MC ghast 免疫自身
  火球）——顶盖卡壳爆点在鼻尖 r3 内 = 5 自伤（探跑实证，
  确定性）。
- 无击退 / 破碎无掉落 / 流体不触碰 / 点火 100%（V3-14a 继承
  记档）。
- ghast 死亡掉落 0-1 membrane + xp 5 orb（mob 不入档 = 重载
  重生，format 30 设计项延续）。

## 变更文件（显式路径，42）

见 git commit。
