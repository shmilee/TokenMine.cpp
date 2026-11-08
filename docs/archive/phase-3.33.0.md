# Phase v3.33.0 — V3-16b 扩展指导 demo 族：⑤补充体力 + ⑥修复回血（门 223）

> 3.x「世界扩展」V3-16 相位 B/7（day1 章节 ④⑤⑥③ 的 ⑤⑥ 两支）。
> 内核零变更（16a 的 scene/panel_click/索引全复用）——本相位 = 内容 +
> 工具链增量（2 scene + 2 demo + 2 index 条目 + 2 处 authoring 修正）。

## 实现

### scene ②（mk_scene 手术，幂等）
- **guide_food**：12×12 平台 + 6×4 麦田（farmland 24 格 y53【平台挖
  洞——同格 journal 重复点名拒收的门内捕获②，y53 挖洞后铺 farmland】+
  wheat y54：wheat_7 ×8【可收】+ 年轻梯度 ×16【wheat_6×5 / wheat_4×5
  / wheat_2×4 / wheat_1×2——帧锁定生长（360 拍/级 roll）在 demo 期间
  视觉长熟 = 教学细节】）。player (0.5,54,0.5)，**起姿 yaw 2.5 朝空
  旷侧**（门内捕获①：起姿朝麦田 = f0 捕获点击的 hold 尾部瞬破 0 硬
  度作物 3 株）；hunger 10 / sat 0 / 空库存；time 0.10。
- **guide_regen**：平台 + 5 格石柱 (10,54..58,-8)【5 落差台——fall
  5 − offset 3 = 2 伤害】。player 柱顶 (10.5,59,-7.5) yaw 0；hunger
  14；h0 bread ×2（场景预置 = guide 的 prepared start 面）；time 0.10。

### demo ⑤ guide_food（18262f = 05:04，249 actions）
巡视麦田 → 逐格收割 8 株 wheat_7（**1f tap = 1 破**——门内捕获③：
2f tap = 2 破帧，射线后方的作物【f3000 时全熟 = 硬度 0】被顺破；
行墙视线同 16a 逐格贴线）→ 周长收集 → E 面板面包 ×5（16 麦耗 15 剩
1）→ 进食 ×2（1.6s 通道 96f，RMB hold 100f；hunger 10→20 满）→ 满
条巡视收尾。
**终态钉**：h0 麦 1 / h1 面包 3 / sel h1 / hp 20 / hunger 17 / sat 8
（衰减 8 tick 的确定性值）/ 田 16 株全熟 wheat_7 / farmland 24 完好 /
z-1 行 + z-4 x2,x3 空 / 零实体。

### demo ⑥ guide_regen（18344f = 05:05，82 actions）
柱顶巡视 → 走下台（5 落差，hp 20→18）→ 落地观察 → 进食（hunger
14→19，sat 6）→ **回血等待**（10s 门 + 2×4s tick = hp 18→20，每半
心耗 1 饥饿——自然回血的完整教学面）→ 满条第三人称收尾。
**终态钉**：hp 20 / hunger 13 / sat 2 / h0 面包 1 / 落点
(10.5,54,-10.04) / 柱 5 格完好 / 零实体。

### 工具链增量（demo_author.py）
- `capture()` 2f 化（down f / up f+1 = **零 locked-mining 帧**——16a
  的 5f 在 16b 场景起姿下 = 首帧瞬破 3 株的门内捕获①；16a pilot 的
  已 author dmo 不受影响）。
- 旧分段版 `walk_polyline` 死代码清除（引用已改名常量 = 雷）。

## 门内捕获（PITS 414 四坑）
1. **f0 捕获点击的 hold 尾部 = 挖掘**：5f LMB（down f0/up f5）= f0
   捕获（!locked 分支）+ f1-f4 locked 挖掘——起姿射线穿硬度 0 作物
   瞬破（3 株 wheat 无声消失；f60 无 demo 对照跑完好 vs demo f300 缺
   株 = 二分定位）。修 = capture 2f。
2. **scene journal 同格重复**：平台石 y53 + farmland y53 同格 = init
   点名 "duplicate block edits"——挖洞再铺。
3. **1f vs 2f tap 的硬度 0 连锁**：crop 全硬度 0 = 每挖掘帧瞬破射线
   首 solid；4f tap（16a 默认）= 顺破 2 株（且 f3000 后年轻麦全熟
   变硬度 0 = 后方也瞬破）；1f tap = 恰 1 破。
4. **perf A/B 的 cwd 陷阱**：`cd A && build; cd B && run` 的 `;` 后
   run 用的是 B 的 cwd——旧树臂误用新二进制（banner 同版不可辨）；
   显式全路径 + 独立 cwd 重跑才入档。

## 测试 / 门结论
- **2998/2998**（零核心变更——内容相位无新增单测，16a 内核族执法 +
  init 交叉检查 3 条目全量 boot 复验）；`-Wall -Wextra -Wpedantic
  -Werror` 0 警告。
- **S1 结构免疫第 104 次**：f60 ×2 ≡ base19 逐字节 + savenorm EXACT
  （banner 3.33.0 零像素复验）。
- **canary 4 EXACT**（×2 RAW + savenorm——sim 2 不变执法）。
- **⑤⑥ ×2 RAW + savenorm EXACT** + 终态内容断言（上节钉值）+ 零
  panel problem。
- **性能 A/B 同档**（旧 = 75266af v3.32.0 worktree + g139 双树 27
  markers：OW 900f 无输入 PERF-WIN 三窗 upd med 0.559/0.588/0.585 vs
  0.537/0.570/0.564 同档 + render 0.584/0.683/0.651 vs
  0.585/0.666/0.624 同档 + swap 15.33/15.22/15.24 vs
  15.34/15.24/15.31 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm
  EXACT = 内容相位结构性零成本）。

## 已知 / 后移
- ⑤ 的田在 demo 期间全熟（16 株 → wheat_7）= 生长教学的可视化，
  非 bug（帧锁定 roll 确定性）。
- 16c ③房子+床 = day1 收官（床睡三闸 + 夜跳 0.05 的时序面）。
