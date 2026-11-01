# Phase UX-9b — 教程火把步（v1.49.0，门 169 全 PASS）

> 消费用户 11-01 实机报障定案：「掉在洞里，黑乎乎看不到东西，我怎么
> 打怪物呢？」= 8 步教程到「挖石 + 石镐 + 杀鸡」结束，诱导下洞却从
> 不教点灯 = 认知链断环。**UX-9 体验细化（二轮实机报障）行第 ② 项**。

## 玩法背景（编排者核证）

游戏内点灯路线已存在且文档化（GAMEPLAY.md）：煤矿石（常见，矿脉
prob 0.154 到 y≤80）→ 煤；2 木板 = 4 木棍；**煤 + 木棍 = 火把 ×4**
（光 14 > 敌对生成门槛 7 = 照亮即封生成）。mob 恒白烘焙在暗处本可
见（夜 ~48% vs 环境 3%）——用户"看不到"的是**环境**（挖哪、出口、
煤在哪）= 教学缺口非渲染缺口。

## 实现（零核心代码）

- **beginner.json 8→11 步**（spick 后插 3 步，kill 收尾不变）：
  | # | id | goal | text | tip |
  |---|----|------|------|-----|
  | 7 | coal | break.coal_ore ×1 | 挖 1 个煤矿石 | 煤矿石多在地下洞中，用石镐挖 |
  | 8 | torch | craft.torch ×1 | 合成 1 个火把（1 煤 + 1 木棍） | 木棍 = 2 木板（E 面板配方） |
  | 9 | light | place.torch ×1 | 放 1 个火把 | 暗处放火把照亮，敌对不在亮处生成 |
- 事件词表既有 break./craft./place. 全覆盖 = 状态机/init 交叉校验
  （+coal_ore/torch/stick 点名）纯数据驱动
- **字集 395→401**（亮 U+4EAE / 处 U+5904 / 把 U+628A / 敌 U+654C /
  暗 U+6697 / 煤 U+7164 按 codepoint 序中段插入 = 既有格位移，UX-8b
  先例）：生成器 TTF SHA 钉 + legacy 33/33 位级自校验 + **顶 512 行
  逐字节不变**（131072 px identical 自检 + 独立解码对 HEAD 0 px diff
  + Top512 FNV 74b5d1418347dab6 未动）+ font.png 256×2080 尺寸不变
  （401 ≤ 427 cell 余量）md5 12a0fabf ×3 恒等
- 措辞微调 1 处（记档偏差）：coal tip brief 原文含 常/穴/快 3 字不在
  395 字集（预研漏核，fixer 逐字复核揪出）→ 微调免加字「煤矿石
  多在地下洞中，用石镐挖」（保 401 精确）
- 版本 1.49.0（CMakeLists 单点 + F3 banner 钉，PITS 355 纪律）

## 测试

2061/2061、`-Werror` 0 警告、banner v1.49.0：
- `ShippedBeginnerIsTheElevenStepChain`：11 步 id/goal/count exact +
  3 新步 text/tip 逐字钉 + 无 tip 集 {0,1,4,5,6,10}
- 字集测 401 + init 交叉校验 +coal_ore/torch/stick
- 字体测演进：CjkTablePinned（401/368/cell 128..495/nextCell 496）+
  GlyphUvCjkCellPositions（镐 cell 474 移位钉 + 默 495）+ 扩展格钉
  （24 旧钉移位 + 6 新字）+ FontPngBytesPinned FNV 重钉
  42976f1063d0610b + Top512 红线原钉未动

## 门 169（编排者执行，old 侧 = v1.48.0 = a520728 = tm_v149_base；
scratch/scratch_old 双资产目录 = PITS 358 纪律）

- **S1 = 第 169 次结构性免疫 + base18 锚连续 179116 B 免重锚**：
  fresh 60f 六跑（new-on ×2 / new-off（tutorial.enabled=false）×2 /
  old ×2）**全字节一致**（179116 B 含锚自身）+ 跨版 savenorm EXACT
  （无输入 = 无 locked = 零目标行绘制 = 零字体绘制；tutorial 存档态
  双侧同写 {walk,0}）
- **S2 教程链输入跑 PASS**（段 C 判别 + 段 D 杀鸡，各 4 跑 ×2/侧）：
  - **段 C（挖煤→合成→放置，mobs off，受控平台场景 s129 先例）**：
    分歧种子 NEW {coal,0} / OLD {kill,0} 同输入序列——
    **F5 中检查点互斥：NEW {torch,0} vs OLD {kill,0}**（挖掘事件
    仅 NEW 消费 = 步进判别）+ 终态**双侧收敛同形**（torch 配方
    两版皆有 → 双侧同合成同放置：torch@(0,55,−4) + slot1 torch3 +
    slot11 stick1 + grid 空 + 双侧 {kill,0}），跨侧 savenorm diff =
    **仅 player.pos[2] ±0.009**（real-dt 走位抖动类，PITS 320/347）
    = 同世界最强形
  - **段 D（杀鸡，chicken-only + max_mobs 1 脚手架 s136 先例，鸡速
    0.01 s162 先例）**：pose (0.5,54,−7) + W 0.6s 接近（spawner 环 6
    出射程 PITS 359）+ 石镐 0.35s hold = 2 swings×3 伤一击杀（击退
    漂移容差 PITS 359）——鸡位两侧确定性同 cell (0.5,52,−12.5)：
    **NEW 日志 "Tutorial: completed (step 11/11)" vs OLD "step 8/8"
    = 步数判别定案** + 双侧 kill.chicken 1 + feather ×1 + tutorial {}
    + 跨侧 savenorm diff = 仅 pos[2] ±0.001（同世界最强形）
  - 协议新坑 4 条入 PITS 356-359（E 面板释放指针锁须 re-lock /
    放置格玩家 AABB 重叠静默拒 / 跨版资产脱节第 6 次复现 / 近战
    击杀五要素）
- **S3** = 2061/2061。**无 S6**（零新 parse 面——schema 四键 + tip
  既有，11 步纯数据；字集 = 工具期数据，运行期 glyphUv 查表）
- **性能 A/B v1.48.0 vs v1.49.0（发布前硬门槛）PASS**：fall 900f 无
  输入——PERF-WIN 三窗全指标同档（render med 0.622/0.698/0.626 vs
  0.596/0.691/0.604 新版略优）+ PIPE3-S0 changesApplied **9/3/1 三窗
  逐位** + **终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 = 同
  世界同成本**（结构惰性最强形：无输入零教程 UI + 零字体绘制，
  font.png/11 步数据成本 = init 期解析一次性）

## 偏离/记档

- 措辞微调 1 处（coal tip，免加字，上节）
- 教程 11 步后 kill 步 = 步 11（NEW）vs 8（OLD）——跨版 A/B 的
  判别量 = 完成日志行步数（savenorm 终态双侧同形 {}）
- 场景受控平台（y53 石台 + y54/55 清除）= 自然地形凸起顶位/放置格
  重叠两类坑的根治（PITS 357 + v2 实证 y56 顶位）
- 本版不打 tag（实机报障修复惯例；UX-9 系统收官时 release 覆盖
  v1.48-v1.5x）
