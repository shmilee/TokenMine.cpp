# Phase V2-3 — 教程系列化（农耕 + 厨房）（v2.3.0，门 174 全 PASS）

> 2.x 第三项（V2-3 执行序首项）。用户 11-02 实机反馈方向：「之前的新手
> 指导比较单调，有没有多个任务系列可以添加」。定案 = UX-7 教程机制
> 系列化（tutorials/ 多文件名序 = 系列序）+ 两个新系列（②农耕 ③厨房）
> + 事件词表 +2 前缀（pickup./smelt.）+ 存档 format 24→25。

## 根因（单调点）

UX-7 起教程 = 单链 11 步（第一天生存基础），`done` = 终态永久结束；
游戏后半程循环（农耕/面包、炉子/熔炼、弓/防御、建造）零覆盖——玩家杀
掉第 11 步的鸡后进入「然后呢？」真空。架构无碍（tutorials/ 目录、
{id,count} 存档、帧锁定纯状态机、单点事件收口皆为多系列就绪），缺的
是机制系列化 + 内容。

## 实现

### A 机制系列化（core/tutorial 纯函数 + 存档 + 适配器）

- `AssetManager::listDir(relative)`（新）：regular files 基名、**字节序
  排序**（注册表文件序范式）、防穿越同款 resolve、目录缺失/非目录点名
- init：`data/tutorials/*.json` 枚举（非 .json 点名拒、空目录点名拒）
  → 逐文件 parseTutorial（零改动）+ validateTutorialGoals（零改动）→
  `checkSeriesGlobalIds`（新纯函数：跨系列步 id 重名点名 series i/j）
  → `m_tutorialSeries`（替换 m_tutorialSteps）
- `TutorialState` + `seriesIndex`；`tutorialApply` 多系列推广（越本系列
  末步 → 下一系列首步；越末系列 → done 钳位同款纪律）；
  `tutorialRestore` +series 参（**series 越界 = RESET**（报日志不拒档 =
  id 失效语义推广：数据是内容）/ id 不在该系列 = RESET / progress ≥
  count = 损坏拒档不变）
- **存档 format 24→25**：InProgress 形状 `{series, id, count}`；三向键
  纪律：format 25 缺 series = 版本谎报拒 / format 23-24 携 series =
  版本谎报拒 / format 23-24 载入 = series 默认 0（beginner.json 字节
  零改动 = **值保真免迁**，UX-8a 同款）。**编排者核证补钉（PITS 361）**：
  slot 数边界 `fmt >= kSaveFormat` 在 bump 到 25 后会误判 format-24 档
  为 27 槽 → 改字面量 `fmt >= 24`
- 事件词表 +2 前缀 `pickup.` / `smelt.`（语法错误文案从词表派生零第二
  名单）；validateTutorialGoals：pickup/smelt → item 存在性
- 发射点 2 处（单点收口纪律）：`pickup.<item>` = 玩家拾取 ItemEntity
  处（"pickup" 音旁，game_entities）；`smelt.<item>` = 炉子 smelt-
  complete 消费点（"smelt complete:" 帧锁定日志旁，game_frame）
- **门 174 A1 实证补修（PITS 360）**：E 面板 recipe 行左键分支
  （canCraft → craft）成功合成**漏发** tutorial craft 事件（UX-7 的
  emit 只在 grid/output 分支；1.x 链从未经 recipe 行合成 = 潜伏）→
  玩家点配方行合成会永久卡教程。修 = recipe 行分支补同款 emit（每
  次成功合成恰好一发的契约两路径统一）
- 日志定案（E2E 证据逐字）：非末系列完成 `Tutorial: series complete
  (<s+1>/<total>, step <n>/<n>)` / 末系列完成 `Tutorial: completed
  (series <total>/<total>, step <n>/<n>)` / T 跳非末 `Tutorial: series
  skipped (<s+1>/<total>)` / T 跳末 `Tutorial: skipped`（不变）；
  **toast 只在 done 翻转**（系列过渡无 toast = 目标行换步即提示，零
  新文本）
- T 键 = 跳**当前系列**（非末 → 下一系列首步无 toast；末 → done +
  toast 旧行为）
- F3 行：`<series+1>/<total> <id> <progress>/<count>`（例 `2/3 seed
  0/1`；off/done 不变；F3 钉同步）
- HUD 目标行/tip 行：签名换 series vector，布局零变化（仅 locked 绘制
  = 无输入结构免疫根保持）

### B 内容（两个新系列，全链数据核实在位 = 零新机制）

- `farming.json` 7 步（×2 修订版）：pickup.wheat_1 ×2 → craft.farmland
  ×2 → place.farmland ×2 → place.wheat_1 ×2 → break.wheat_7 ×2 →
  craft.bread → eat。**修订根因（PITS 362）**：wheat_7 drops_count = 2
  < bread 配方 3 wheat = 单株链死局 → 全链 ×2（4 wheat = bread 3 +
  余 1）；harvest text「约半分钟」→「约 1 分钟」（干耕地 360×
  drySlowdown 2 = 720f/级均值 ×6 ≈ 72 s，演示区无水 = 探针实证；
  实测两株并行 ~40 s 熟 = 随机 roll 快侧，轮询上限 210 s 覆盖 p99）
- `kitchen.json` 6 步：break.stone ×3（基础链 5 + 3 = 8 锻石）→
  craft.furnace → place.furnace → interact.furnace →
  smelt.cooked_chicken → eat
- `beginner.json` **字节零改动**（红线，git diff 实证）
- 字体：14 行新文本核 401 字集 → 缺字 2（株 U+682A + 分钟既有）→
  字集 401→**422**（generate_font.py CJK_CHARSET 码点序插入，扩展区
  389 字形整体 +1 重格，**顶 512 行 131072 px 逐字节不变**（BMP 解码
  cmp 实证）= 免重锚红线；atlas 余 3 cell）

## 门 174（编排者执行；old 侧 = v2.2.0 = d9d62b0 = tm_v230_base）

### S1 = 第 174 次结构性免疫 + base18 锚连续 179116 B 免重锚
- fresh 60f 四方 ×2（new n1/n2 / old o1/o2，最终二进制重跑实证）：
  **n1≡n2、n1≡o1、n1≡o2、o1≡o2 全 RAW 字节一致 + n1 ≡ base18.png 锚
  逐字节**
- 跨版 savenorm（gate_compare.py）= **恰 2 项**：format 25 vs 24 +
  新侧 `player.tutorial.series:0`（值保真，无其它 diff）
- 无输入零 tutorial 日志（冒烟 grep 0）

### S2 = 输入链（存档权威；probe174_aim v2 累积态真实 raycast +
probe174_panel ui 空间布局标定；g161 纪律：世界动作只在 fresh
capture 后、面板关闭后不做世界动作、锁定态点击零预 move（PITS 363））
- **A 农耕链（9 段全 PASS）**：A1 合成耕地×2（recipe 行点击，emit
  修复实证）→ A2 **FAR 先放**（−0.50；NEAR 后放会挡 FAR 落点 =
  probe v2 累积态发现，PITS 364 族）→ A3 NEAR（−0.70）→ A4 种 FAR
  （−0.21）→ A5 种 NEAR（−0.245）→ A6 生长轮询（15 s×14 上限；实测
  poll 1 熟）+ 破 NEAR（−0.245 继承）+ W 0.8 s 走捡（掉落 2.5 块外
  无自动拾取，走位 real-dt 位置只记录不门控 PITS 319）→ A7 重 pose
  破 FAR（−0.045）+ 走捡 → A8 面包（recipe 行）→ A9 吃（hunger 14
  注入 PITS 367，14→19 = +5 实证）。终档判别 `{series:2, id:"cobble"}`
  + 日志 `Tutorial: series complete (2/3, step 7/7)` 逐字命中 +
  wheat ×1 余 + bread 0（逐 checkpoint 存档断言 ×8）
- **B 厨房链（2 段全 PASS）**：B1 RMB 开炉（**重试 = Escape + 重新
  capture 换漂移抽值**，PITS 365；首试 miss 二试命中）→ 面板
  coal→fuel / chicken→input（ui 空间坐标 PITS 364；**fuel 判别 =
  fuel_substeps > 0**（点火即耗物品 PITS 366），substeps 4584 实证）→
  12 s 熔炼（面板不暂停世界 P15）→ output 取熟鸡→slot2 → B2 key 3 +
  长按 1.8 s 吃（14+6 = 20 满）。终档 `{}`（Done）+ 日志 `Tutorial:
  completed (series 3/3, step 6/6)` 逐字命中 + 食物全消
- **C T 跳过三态 PASS**：{series:0,chop} →T→ {series:1,seed} +
  `series skipped (1/3)` →T→ {series:2,cobble} + `(2/3)` →T→ {} +
  `skipped` + toast
- **D format 24 迁移 PASS**：S1 旧档 o1（{id:walk,count:0} 无 series）
  喂新二进制 30f → 重写 format 25 {series:0,id:walk,count:0} + 零
  reset 日志（值保真免迁实证）

### S3 套件
- 2109/2109（2084 + 25：多系列机器 / restore 迁移 / format 25 键纪律 /
  全局 id / 词表 / 内容钉 / F3 钉）

### S6 精确点名（真实二进制实证 4/4）
- format 24 携 series = 「unknown field 'series'」版本谎报拒 ✓
- format 25 缺 series = 「requires 'series'」拒 ✓
- series 9 越界 = **重置上报**（`Save tutorial step 'walk' (series 9)
  is not in the current tutorial data — resetting to the first step`）
  不拒档 ✓
- tutorials 目录全局步 id 重名 = 「series 0 step id 'chop' collides
  with series 3」init 拒 ✓
- （词表语法 / smelt 目标缺物品 / 非 .json 条目 = 单测钉）

### 性能 A/B（用户硬要求，每版发布前）— PASS 同世界同成本
- v2.2.0（tm_v230_perf，d9d62b0 + g139）vs v2.3.0（tm_v230_perf_new，
  最终工作树 + g139，双 27 标记 split 布局）——fall 900f 无输入第一人
  称 seed 123456789，banner 双侧 v2.2.0/v2.3.0 实证：
  - PERF-WIN 三窗全指标同档：upd med 0.64/0.68/0.64 vs 0.65/0.63/
    0.69 ms；p95 25.6/20.2/21.2 vs 23.0/19.8/24.2；render med
    0.64/0.69/0.62 vs 0.60/0.67/0.67；swap med 15.2/15.1/15.2 vs
    15.3/15.3/15.1（vsync 主导同值）；fluid 0.051/0.049/0.049 vs
    0.051/0.045/0.050；fires/spread/ents/stream 全同档
  - **PIPE3-S0 changesApplied 三窗 max 9/3/1 逐位**（双侧）
  - **终档 fluids 18/18 + blocks 55/55 集合恒等**（同世界最强形）
  - = 结构惰性实证（无输入零 tutorial 事件、HUD locked-only、字体顶
    512 行不变、双新发射点零触发）
- 门内插曲：旧侧树首建 CMakeCache 跨目录污染 → ninja "no work to do"
  静默未重编（插桩未进二进制 = PERF-WIN 全空）→ rm build 重配重编
  修复（参照树纪律 = PITS 317 族：构建后须核插桩输出非空）

## 测试与构建

- 2109/2109、`-Wall -Wextra -Wpedantic -Werror` 0 警告、banner v2.3.0
  （PROJECT_VERSION 单点 + F3 钉 + 构建后实证）

## 偏离（记档）

- 系列过渡无 toast（目标行换步即提示；零新文本 = 字体零压力）
- T = 跳当前系列（非一键全跳；末系列 T = 旧 done 行为）
- 干耕地生长文案「约 1 分钟」= 72 s 均值（波动 ±；两株独立 roll 并行
  = 先到先破，教程不要求同时熟）
- 种子 1/8 概率文案（tip「草方块有 1/8 概率掉种子」= 12.5% 钉死值）

## PITS（359 → 367，8 条）

- **360** recipe 行合成漏发 tutorial craft 事件（门 174 A1 实证：玩家
  点配方行会永久卡教程；1.x 链从未经该路径 = 潜伏）
- **361** kSaveFormat bump 后 per-format 形状边界须用引入 format
  字面量（`>= kSaveFormat` 移动目标误判 format-24 档 27 槽）
- **362** 教程链内容须核经济账（掉落数 × 株数 ≥ 下游配方需求；
  wheat_7 ×2 < bread 3 = 死局）
- **363** 锁定态（pointer captured）世界点击前**禁止** mousemove
  预 move：XWarpPointer 合成 MotionNotify → SDL 相对 delta = 相机漂移
  0.1-0.2 rad（PITS 28/337 族；capture 点击后光标已在物理中心）
- **364** 面板布局 = **uiSize（窗口 ÷ uiScale）空间**，鼠标同缩放；
  探针须 ui 空间跑、物理点击 = ui×uiScale+origin；且窗口尺寸随 WM
  平铺态变（独占 1362×744 / 半平铺 679×744）= PITS 314/334 族首案
  （E 行区宽 154px 侥幸命中、炉子槽 52px 全 miss 实证）
- **365** capture 的 grab delta 漂移在同一次 capture 内不变 = 世界 RMB
  重试无效；**Escape 释放 + 重新 capture 换一次漂移抽值**（PITS 28/
  319 族；炉子开面板 4 连 miss → 重捕获二试命中实证）
- **366** 炉子燃料**点火即消耗物品**转 fuel_substeps（CRAFT-5 ignition
  语义）：「fuel 槽有煤」断言 = 假阴性陷阱，判别 = fuel_substeps > 0
- **367** 饥饿满时吃 = 静默 no-op（P23 规则）：quiet config 冻结
  hunger 20 满 → 教程 eat 步永不完成；E2E 场景须注入 hunger < max
