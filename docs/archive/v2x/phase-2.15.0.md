# Phase V2-14 — 蝙蝠（项目首只飞行 mob）（v2.15.0，门 186 全 PASS）

> 二批 ENT 行（BACKLOG:「中」：飞行 AI = 新 AI 型【洞穴生成】；模型
> + 纹理数据驱动）。MC 机制 11-04 编排者 wiki 核定（Bat 页：HP 6 /
> 被动 ambient / 光 ≤3 洞穴 / 无掉落 xp 0 / 悬停巡航 + 倒挂 / 碰撞盒
> 不与玩家交互 / squeak 音）。**零 format**（mob 不持久化 PITS 375③
> = 无存档面，savenorm EXACT 实证）。

## 实现

### 数据（全数据驱动，三新字段 + 互配纪律）
- **bat.json**：HP 6 / speed 1.0（巡航速消费）/ 被动全 0 /
  wander_interval 2.0（巡航转向间隔复用）/ spawn_max_light **3**
  （洞穴语义 = MC 精确：夜底光 7 > 3 → 露天不可生）/ 无 drops /
  xp 0 / **`flying` + `spawns_in_air` + `ambient_sound:"squeak"` +
  `ambient_interval:3.0`**（严格解析：类型/空串/正数点名拒；唯一硬
  互配 flying⇒spawns_in_air；孤 ambient_interval 无音名 = 死数据
  点名拒）
- **model 0.9×0.5**（记档偏离 1：MC 盒 0.5 宽 × 0.9 高竖薄容不下
  展翼 AABB）：身 0.5×0.3×0.5 + 头 0.25×0.2×0.2 前 + **双翼
  0.35×0.05×0.5**（±0.275 = 0.9 极限）+ swings 双翼 0.9 rad group
  0/1 反相（飞行扇翅 = chicken 翅盒不摆先例的反例，巡航恒水平位移
  → 相位恒动）
- **mob_spawning.json** + {bat, weight 1}（8 词表；记档偏离 2：MC
  ambient 独立 cap，项目入被动池 max_mobs 4）
- **纹理**：bat.png（深褐黑 (40,34,30) + 翼膜暖斑 30%，salt 0xB441，
  generate_mob_textures.py 新函数；既有 24 mob tile 重跑 md5 全不变）
  + atlas 119→120（(10,6) 首空格，零既有移位 census 钉）
- **音**：squeak.wav（generate_sounds：900→1400Hz 滑升 + 30Hz 微颤
  + LCG 噪响 0.15，0.12 s，env ×0.1 = MC ambient vol 0.1 类，seed
  0xB441，5292 采样；既有 41 wav 重跑 md5 全不变；SoundBank 42）

### 飞行 AI（core 增量，数据旗门控零物种 switch）
- `runAi` 顶部 `def.flying → flyCruise()`（纯新分支；既有 7 mob
  测试族零改动全过 = 位级红线）：
  - 航向 = per-mob 流（m_rng nextMix PITS 30 推进纪律）每
    wander_interval 2s 重掷（水平 2D 角 uniform）+ 数据 speed 巡航
  - **垂直正弦悬停**：vy = kBatBobAmplitude 0.2 × sin(2π·
    frame_local/kBatBobPeriodFrames 60)（per-mob 局部帧计数 1/60 定
    步帧锁定；bob 初相 = 构造时流首抽【flying 才消费，非 flying 零
    消费 = 位级红线】）
  - **integrate flying 分支**：vy 每帧设正弦基线（重力关/终端豁免/
    流体旁路——悬停穿水，实现级偏离）+ stepUp flying 门
  - **撞壁 = afterSubstep 水平钳零 → 下帧航向重掷**（记档偏离 3：
    无寻路，自然绕行近似）
  - 玩家碰撞保留（记档偏离 4：MC JE 蝙蝠不交互，项目走既有 mob
    push）+ knockback 0.3s 滑移后回巡航（帧钉）
  - **ambient squeak**：每帧 p = 1/(interval×60) per-mob 流 roll →
    空间化 play("squeak", pos)（帧锁定确定性；无 ambient 的 mob 流
    零消费）
- **生成 in-air 分支**（findMobSpawnCell `spawnsInAir` 尾参默认
  false）：**顶向扫描**（kWorldHeight 向下）+ box 全空气无流体 +
  currentSpawnLight ≤ spawn_max_light（≤3 = 洞穴）+ **跳过 floor 带
  参数（任何 Y = MC）** + 玩家重叠/间距既有；出生位 = 盒底格中心
- 消散 40（记档偏离 5：池统一，MC 32）/ **倒挂不做**（偏离 6）/
  **takeoff 音不做**（偏离 7）
- 教程 "kill.bat" 入闭合格言（kill.<mob> 既有模式）

## 变更文件（21：18 M + 3 新）
新：assets/data/mobs/bat.json / textures/mobs/bat.png /
sounds/squeak.wav；改：mob_registry（4 字段 + 互配）/ mob_entity
（flyCruise/integrate/stepUp 门/ambient 回调）/ mob_spawning
（in-air 分支）/ game_entities（传参 + 出生 y 分支 + 回调装配）/
game_init（音名 file-based 交叉检）/ mob_spawning.json（8 词）/
block_atlas.json（120）/ generate_mob_textures.py + generate_sounds.py
（新函数）/ CMakeLists（2.15.0）/ test_mob（25+1）/ test_atlas /
test_sound（2）/ test_tutorial（1）/ test_debug_overlay（F3 钉）

## 门 186（old = v2.14.0 tm_v2140_base/perf = cc05d53）— 全 PASS

### S1 = 结构免疫第 67 次（**免重锚 + 零 format 最强形**）
- 新侧 60f ×2 恒等 且 ≡ 现锚 179027 B 逐字节（first_spawn_frame 60
  免疫根 + 蝙蝠不入 worldgen + 新 tile/音惰性）
- **savenorm = EXACT**（零 format：无新存档键，与旧侧完全同形）
- **门协议再证 PITS 379①**：编排者首跑又误用 v2.13.0 树（72e0028）
  当旧侧 → savenorm 3 键假象；cc05d53 = v2.14.0 重建后 EXACT
  （banner 实证纪律 = 建树后 grep 版本行）

### S2 = 洞穴无输入 E2E（氛围 mob 范式：证据 = log 层 + 像素孪生）
- 场景 = **密封暗厅**（石顶光 0；亮洞口会触发流送重光照跨 chunk
  收敛 = 帧 600 像素时序依赖 → 密封 = 门场景纪律 PITS 380②）+
  seed 3355（f60/90/120/150 四次 pick 全 bat → 4 只占满 cap，他 mob
  零生）+ 玩家在厅内
- **new ×2**：log 4 蝙蝠 spawn 行 f60/90/120/150 位置**逐位相同**
  【(0.5,45,0.5)/(8.5,45,0.5)/(9.5,45,0.5)/(0.5,45,0.5)】+ 截图
  逐字节恒等（生成帧锁定 + 飞行帧锁定 = 像素确定）
- **对照（mobs off）×2** 恒等 + 零 spawn；**new vs 对照 diff =
  43425 px 全为褐色蝙蝠纹理像素**（4 只 bob 不同屏高：y 9-220 带，
  近景每只 ~10k px）——**泄漏审计**：HUD 区（y>600）0 像素 / 时钟
  bbox 0 像素 / 非褐色 0 像素 = 零泄漏（PITS 380④：蝙蝠 diff 带
  随 bob 变化，审计 = 色相 + HUD 区，非固定带）
- **old（v2.14.0，同场景 format 28 双向可载）**：7 词表无 bat =
  零 'bat' spawn 行（形状互斥最强形）

### S3 套件
- **2356/2356**（2330 + 26；编排者独立复跑）

### S6 = 6/6 点名
flying 无 spawns_in_air（互配）/ ambient_sound 缺 wav 文件（init
交叉检）/ ambient_sound 非 string / ambient_interval 0 / 词表
'batt' 未知 / model box 越 0.9×0.5 AABB——全点名

### 性能 A/B PASS 同世界同成本（用户硬要求）
- 主对 fall 900f 无输入 seed 123456789：PERF-WIN 三窗全指标同档
  （upd med 0.59/0.60/0.56 vs 0.64/0.51/0.38 ms——8 词表 pick 流
  差异 = mob 组成可异但**终档 fluids 16/16 + blocks 36/36 集合
  恒等**【mob 不入档】）+ **changesApplied 9/1/3 逐位**
- **洞穴场景自成本**（600f 无输入，CAVE_CFG = 场景专属 seed 3355
  config——perf config 的 seed 与场景世界不匹配 = PITS 380③）：
  v2.14.0（7 词表生 4 洞穴 mob）vs v2.15.0（4 蝙蝠飞行 AI）：
  ents 0.0933 vs 0.0888 ms / upd render 全同档 = **飞行 AI（无寻
  路无重力）不比走地 mob 贵**

## 偏离（记档 7 条 + 实现级 3 处）
1. model 0.9×0.5（MC 0.5×0.9 竖薄）2. 被动池 max_mobs 4（MC
ambient 独立 cap）3. 撞壁 = 下帧航向重掷（无寻路）4. 玩家碰撞保
留（MC JE 不交互）5. 消散 40（MC 32）6. 倒挂不做 7. takeoff 音
不做。实现级：flying 不 step-up / flying 旁路流体（悬停穿水）/
ambient_interval 默认 3.0（无音名时）

## 门协议发现（PITS 380）
① mob spawn 成功行 = `mob spawn fN: 'name' at (x,y,z)`（**无
'pick'**——'pick' 是拒绝行措辞，正则带 pick = 零命中假无生）② 门
场景洞穴须**密封**（亮洞口 = 流送重光照跨 chunk 收敛，帧 600 像素
时序依赖负载）③ 场景专属 config 的 seed 归属场景（perf CAVE 自成本
须 CAVE_CFG，勿用主对 config）④ 飞行 mob diff 带随 bob 变化 = 泄漏
审计用色相 + HUD 区判，非固定 y 带
