# Phase UX-8a/8b — 教程木板步 + 目标行辅助文字（v1.42.0，门 162）

> 消费用户 v1.41.0 实机反馈④「按照指引做工作台，但假如我是新手，只有
> 简单提示还是不知道如何去做」——认知链缺"先木板后工作台"一环。
> **v1.42.0 = UX-8 体验细化系统首版（8a+8b 合并）**；系统收官 v1.45.0
> 打 tag + release（10-31 政策：tag 1:1 对应 release，系统收官才挂）。

## 定案（用户 10-31 四问 → UX-8 行 5 项，本相 = ①②）

1. **UX-8a 教程木板步（纯内容）**：7 步链在 `chop` 后插入 `planks` 步
   → 8 步：`walk → chop → planks → table → wpick → stone → spick → kill`。
   - planks 步 text「合成 4 个木板（1 木头变 4）」/ tip「按 E 打开面板，
     选木头点输出」/ `craft.planks` / **count 1**（craft 事件在 clickOutput
     配方成功点发 n=1——一次输出点击 = 4 块木板但事件计数 1，不是 4）。
   - table 步扩写「做 1 个工作台（4 木板排 2x2）」（**ASCII 小写 x**——
     × 不在字集）+ tip「E 面板 2x2 格摆满木板，点输出」。
   - 其余 6 步 text/goal/count 一字未动（walk 步 text 不变 = 无输入基线
     目标行位级不变前提）。
2. **UX-8b 目标行辅助文字**：`TutorialStep` 尾加可选 `tip`（严格解析：
   出现必须非空串——非字符串/显式 "" 点名拒 = ammo 纪律）+ 纯函数
   `tutorialTipLine(state, steps)` + 主行**上方**第二行 kTextDim 0.55
   灰（tip 行 = HUD 堆栈最顶行，结构上不可能撞心/效果/护甲行；行距 =
   既有命名常量 22px；门控 = 主行同门 ∧ !done ∧ tip 非空 → 无 tip 步
   对 v1.41.0 位级不变）。

## 实现

- `assets/data/tutorials/beginner.json`：8 步（2 步带 tip，6 步 tip 缺省）。
- `src/core/tutorial.{hpp,cpp}`：tip 字段 + 解析 + `tutorialTipLine`
  纯函数；format 23 零改动（id 集只扩不删 = 既有存档全有效——walk/chop/
  table/… 旧 id 全保留，无迁移无重置）。
- `src/core/game_ui.cpp`：目标线段 tip 行（`tutorialTipLine` 非空才画）。
- **字集 392→395**：排 U+6392=cell 292 / 摆 U+6446=294 / 输 U+8F93=445。
  **偏差（门内定案）**：3 字全为**中段 codepoint** = 生成器 dense 布局
  （cell = 128+i）→ **195 个既有扩展字形格位移 +1..+3**（font.png 行
  1160..1975 变更）——brief 预期的"纯尾部 append"不成立；**红线保持**：
  顶 512 行逐字节不变 + legacy 33 位级自检 + 无输入帧零采样变更区 =
  **免重锚**（kCjkGlyphs 表 + test_font 7 个移位 pin 同步演进）。
- **顺带修 `tools/generate_font.py` 自毁潜伏 bug**（fixer 首轮实跑即
  触发）：文件缺 `# >>> UX-6 AUTO TABLE BEGIN` 标记 → 首次扩表时
  `write_table_section` 的 `src.index` 自匹配代码行、把 .py 自身改坏
  （PNG 已写出但脚本报废；git checkout 恢复）→ 补标记 + 重写时复发射
  标记 + `py_compile` 回归 = 后续任何加字一条命令安全（PITS 352）。
- font.png 256×2080 尺寸不变、29574 B、md5 `e0d92bf6f715d3b2bd19832d81493fe6`、
  项目 FNV 钉 `5ea51ddcf9f65071`；重跑幂等 ×3（PNG + .py 双自稳）。

## 测试

2022/2022（+4）、`-Werror` 0 警告、banner v1.42.0：

- `ShippedBeginnerIsTheEightStepChain`（8 步 id/goal/count 精确 + planks
  count==1 事件语义 + table text 含 "2x2" + 两步 tip 精确 + 其余 6 步
  tip 空）
- `ShippedTextsStayInsideTheFontSet` 演进 = text + **tip** 逐字 ⊆ 字集
- 新 ×4：tip 有/缺省解析、非字符串/显式空串/未知键点名、tip 行纯函数
  有/无/done 空
- test_font：FNV 重锚 + 395 表结构钉（max cell 489 / nextCell 490）+
  7 个移位 pin 更新 + 3 新字 pin；顶 512 红线测试零改动

**编排者独立核证**（claims 不是 evidence）：git 面 11 文件 ✓ /
beginner.json 8 步逐字 ✓ / font.png 顶 512 raw 扫描线字节一致（直接解码
比对）+ md5/FNV 双钉复算 ✓ / kCjkGlyphs[395] 表形态 + 排摆输 cell
292/294/445 ✓ / 8 步 text+tip 逐字 ⊆ 395 ✓ / 重建 2022 自跑 ✓ /
生成器幂等 ×3（PNG + .py 双 md5 恒）✓ / tip 行门控代码 ✓。

## 门 162（编排者执行，old 侧 = v1.41.0 = 7723d46 = tm_v142_base）

- **S1 = 第 162 次结构性免疫 + base18 锚连续 168140 B 免重锚**：fresh
  60f 六跑（on×2 + off×2 + old×2）全字节一致 + 跨版 savenorm **EXACT**
  （比门 161 更强——old 侧 v1.41.0 本有 format 23 + tutorial，双侧同写
  {id:walk,count:0}；论证 = 无输入未锁定 = 零教程 UI 零事件 + tip 行
  仅锁定态 + 字集合变更区零采样）。
- **S2 tip 像素 A/B PASS**：存档 pose {id:table,0} 捕获 240f——new×2
  RAW 219866 B / old×2 RAW 216884 B 字节一致 + **跨侧 diff 4399 px 全限
  目标带 y[567..623]**（裁图实证：NEW = 灰 tip 行 + 白新主行 2 行 vs
  OLD = 旧主行 1 行；世界逐像素一致、红心行同位）。
- **S2 全链输入跑 PASS**（独占 1362×744 covers unmap 协议；鸡脚手架
  chicken-only max_mobs 1 speed 0.01；四段 A→B1→B2a→B2b F5 存读续接
  = 8 步逐步推进实证 `{walk}→{chop}→{planks}→{table}→{wpick}→{spick}
  →{kill}→{}`）：
  - **A 段**（walk→砍木×3→**合成木板**→**做台**）：四跑全 PASS——×2 RAW
    双侧字节一致（131660 B）+ **跨侧 RAW 字节一致**（代码级免疫：终帧
    双侧同为 wpick 目标行，text 无 tip 位级不变）+ 跨侧 savenorm **EXACT**
    （format 23 双侧 + tutorial 同值 {wpick,0}）；终态 slot5 台 / slot9
    planks 8 / slot10 leaves / 网格空。
  - **B1 段**（放台→木镐→挖石×5→捡圆石）：四跑全 ckpt——{spick,0} +
    镐 damage 5 + 圆石 5；跨侧 savenorm 仅 pos 走位 real-dt 抖动（±5mm）。
  - **B2a 段**（存档 pose 转正→石镐 12 点击）：**同侧 savenorm 双侧
    EXACT + 跨侧 savenorm EXACT**（无走位 = 全确定）。
  - **B2b 段**（存档 pose 预瞄 + W 0.45s 逼近 + 按住挥击×3→杀鸡）：
    NEW×2 **tutorial {} + stats kill.chicken 1 + 日志
    `Tutorial: completed (step 8/8)`** + 掉落 feather/生鸡；OLD×2
    `step 7/7`（**日志步数 = 8 步链存在性的直接判别**）；跨侧 savenorm
    = real-dt 抖动类（掉落 28µm / pos 30mm）。
  - **跳过 T**：NEW + OLD 双侧 parity（{} + "Tutorial: skipped"——T 键
    v1.41.0 已含，门 162 old 侧 = parity 检查非 no-op 对照）。
- **S3** = 2022/2022。**S6 = ×2 tip 严格解析点名 PASS**（"steps'[2]
  'tip' must be a string, got a non-string" / "…must be a non-empty
  string (an explicit \"\" is rejected)"）。
- **性能 A/B v1.41.0 vs v1.42.0（发布前硬门槛）PASS**：fall 900f 串行
  零并行——PERF-WIN 三窗 upd med 0.377/0.382/0.401 vs
  0.357/0.384/0.397、render med 0.423/0.408/0.448 vs
  0.413/0.437/0.430、swap med 15.70/15.80/15.67 vs
  15.76/15.74/15.74 = 全指标同档；PIPE3-S0 changesApplied **9/3/1
  三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 =
  同世界同成本**（tutorial 双侧同值 {walk,0} = 结构惰性最强形）。

## 门内发现（PITS 351-352）

- **PITS 351（matchesShapeless 全格原料规则）**：shapeless 配方匹配要求
  **每个占位格都是该配方原料**（crafting_grid.cpp:108）——"木板×3 +
  木头×1"混置网格**同时杀死** planks（木板格非原料）与 table（2x2 缺
  木板）与 stick（木头格非原料）= 门 162 首轮 trace 实证（输出点击
  no-op，后续点击级联错位）。推论：**教程合成步的网格预置必须单配方
  纯净**；两个分时合成（planks→table）= 单木预置 + 整堆放置
  （shaped 每格 ≥1 合法，4 木板整堆入一格也匹配）+ 3 次 re-verify RMB
  拆 1/1/1/1（精确无残留）——v1 13 点击（5 RMB 无验证）发散 new1≠new2
  弃用，v3 13 点击（3 RMB 落位 re-verify + F5 检查点 ck1 归因）定案。
- **PITS 352（generate_font.py 自毁）**：写表函数 `write_table_section`
  用 `src.index(BEGIN_MARKER)` 定位表段，committed 文件缺标记 → 自匹配
  到**自身代码行** → 首次扩表即把 .py 改坏（PNG 已写出 = 假成功）。
  修 = 补标记 + 重写时复发射标记。教训：任何"脚本改自己"的段落定位
  须有存在性断言（找不到标记 = fail loud，不许 index 自匹配）。

## 偏离/记档

- 字集中段插入 = 195 格位移（非尾部 append）——红线（顶 512 + legacy
  33 + 无输入零采样）位级保持免重锚；**余量行 62..64 剩 30 格 = 421
  字内加字零代码**（再中段插入会继续位移 = 单测 pin 演进成本，记档）。
- tip 行 = 主行上方（非下方）：下方贴心行无间距，上方 = 堆栈顶零碰撞
  风险。
- planks 步 count 1 语义 = "合成 1 次"（事件 n=1），text 说"4 个木板"
  = 产出量——事件计数与产出量分离（craft 收口点固定 n=1，改 n=产出量
  涟漪全部 craft 步 = 不做）。
- 门 162 old 侧 = v1.41.0（format 23）：跨版 savenorm/RAW 可比性 =
  **更强**（门 161 old = v1.40.0 format 22 有键差异）。
