# Phase v3.34.0 — V3-16c 扩展指导 demo 族：③ 完整房子+床（门 224）

> 3.x「世界扩展」V3-16 相位 C/7（day1 章节 ④⑤⑥③ 的 ③ 一支，day1
> 收官）。内核零变更（16a 的 scene/panel_click/索引全复用）——本相位 =
> 内容（1 scene + 1 demo + 1 index 条目）+ 工具链增量（demo_author
> walk_leg y/hold + mk_scene 容器状态透传）。

## 实现

### scene guide_house（mk_scene 手术，幂等）
12×12 平台（x0..11, z-11..1, 顶 y54）+ **工作台 (1,54,-1)**（床
shaped 配方宿主，prepared start；**门内捕获⑥后自 (0,54,-3) 移此**——
原位与出生点 (0.5,54,-2.5) 的 xz 重叠 = 捕获①根因）+ player
(0.5,54,-2.5) yaw -1.0 于**夜**（time 0.75 = 18:00，sleep 夜窗
f<10800）+ h0 planks 36 / h1 wool 3 + tables 容器条目（与 block
同格同步，mk_scene 新增透传）。

### demo guide_house（18875f = 05:14，511 actions）
**定案结构（v21；v4-v20 共 17 版迭代，门内捕获全 PITS 415）**：
- **P0 夜开**（0-600）：扫视 + 工作台视认。
- **D 床预制先做**（600-1135）：出生点开表（(1.0,55.5,-1.0) 面
  aim）→ 面板 WWW/PPP → 床入 h2 → close。床先于墙 = 夜窗时序面
  （sleep f≈5794 ≪ 10800，余量 5000f+）。
- **A 一段墙 9 / B 二段墙 9**（1135-3492）：地面顶面 aim（1m 外，
  顶面暴露 = 目标格上方空）——v10-v21 全版复验稳定（t=|v| 精确）。
- **C1 脚手架 S1 (1,54,-6) + S2 (1,55,-5)**（3492-3818）：NW 外
  双块，地面 aim（S1 石顶 / S2 二段西 -X 面），终局挖回 = 净 0 块。
- **C2 爬升**（3818-3938）：S1 顶 step-up → S2 顶 step-up（**全 demo
  唯一 step-up 链**；落点 (1.09,-4.33,y56) 实测）。
- **C3 middle x2 + north x2**（3938-4078）：S2 顶 elevated 眼
  (y57.62) 顶面 aim。
- **C3b/C3d 北行 x3-5**（4078-4184）：middle x2 顶 step-up（lift
  损耗过补偿 hold=28）→ 屋顶 walk z≈-5.5 → 二段北墙顶面 aim（t=|v|
  逐位）。
- **C3e 南行 x5/x4 + middle x5**（4184-4500）：屋顶 walk 原位二段
  南墙顶面 aim（y57 floor-crossing 过 middle 空格 = 未放置时唯一
  可行面）；middle x5 = middle x4 +X 链面（**aim (5.0,56.9,-4.25)
  陡化**——捕获⑤：y57 crossing 偏移 1.19m，z_eye ∈ [-6.0,-5.3] 全
  落 middle 空格，19-25cm 边距）。
- **C4 下行 4-hops**（4500-4650）：屋顶西缘 2 块坠（W 8f 提前释放
  = 无坠漂，捕获③）→ S1 顶走 2 hop → 地面 1 块坠；落点
  (0.3-0.6,-5.5..-5.8,y54) 紧带。
- **C9 脚手架双挖**（4650-5130）：**落点原位**（无走位——捕获③
  后定案）：S2 -X 面 / S1 -X 面 aim（±0.2 落点漂移鲁棒），magnet
  拾回 +2 planks。
- **C7 室内 middle x3/x4**（5130-5450）：x1.5 列→z-1.8 行（**长 leg
  目标过补偿——捕获④**）→门停 (3.4,-4.7)：middle x3/x4 = 已放 roof
  链 +X 面（y56 crossing 过目标空格 = 空气，v15 定案面）。
- **C5b 南行 x2/x3**（5450-5650）：门缺口列 (3.5,-3.5) 地面仰面
  （x3 列两层全空 = 唯一可行站位；middle +Z 面，y56 crossing 过
  south 空格）。
- **C8 床 + sleep**（5650-6150）：门停 face E place_ahead → 床
  (4,54,-5)；bed -X 面 aim → RMB = **Slept at the bed (4,54,-5)**
  （f5794，夜窗内）。
- **E 黎明巡视**（6150-18875）：床面 → 出门 → 第三人称正门 →
  四面 walk-around（东/东北/西北/西/正门）→ 日出（东 +X look +
  3 sweep pair）。sleep 不移动玩家（代码核证 = E 从门停起）。

**终态钉**：house 30 planks = y54 9 墙 / y55 9 墙 / y56 12 屋顶
（北 4+中 4+南 4，全验证）/ 零 stray（y57/58 空）+ 床 (4,54,-5) +
respawn (4.5,54.5,-4.5) + h0 planks 3（36-30 房-3 床+2 脚手架回）+
h1 空 + 零实体零火 + hp 20 + time 0.3522（夜跳 0.05 + E 10881f/
36000）。

### 工具链增量
- `demo_author.py` `walk_leg(f, tx, tz, y=None, hold=None)`：y =
  调用方对 leg 端 feet 高度的知识（step-up +1 / drop -N，track 平面
  假设的抬离）；hold = 显式 hold 帧（短跳：W 提前释放 = 坠时无 W
  无漂）。既有 demo 零影响（尾参缺省）。
- `mk_scene.py` 容器状态透传：spec `tables`/`furnaces` → doc（场景
  预置工作台的容器网格 twin；无此面 = init "save 'tables' has an
  entry at (x,y,z) which is not a crafting_table" 点名）。

## 门内捕获（PITS 415 七坑，v4-v20 迭代全录）
1. **出生点与表格格重叠**：scene player (0.5,54,-2.5) 与 table
   (0,54,-3) xz 重叠 → 载入静默顶到表格顶 y55（无点名！）→ track
   y54 全偏 + 开表 RMB 打空放 stray + close_panel 的 E 开起 inventory
   （玩家物理冻结 + 指针释放）+ 其 capture 无效（panel 开时 LMB =
   面板点击非捕获）→ 下游 11 panel problem + 全 demo 死。纪律：场景
   spec 的 player pos 不重叠任何 solid 格（含 prepared 容器）；顶出
   无日志 = 取证盲点（probe 位姿第一手）。
2. **地面眼 y55.62 < y56 = 二层侧/底面截断**：低于二层顶的一切
   屋顶顶面 aim，射线 y56 crossing 若落在二层墙 xz 内 = 命中侧/底
   面 → middle 槽 stray / 拒收。屋顶 12 的可行面谱 = ① elevated 眼
   （y57.62：S2 顶/屋顶 walk）顶面 ② 已放 roof 链 ±X 面 ③ 室内门
   列 middle +Z 仰面（y56 crossing 落 south 空格）④ middle x2 顶
   step-up 链。**南 x3 特殊**：二段南 x3 = 门缺口（air）无顶面 →
   只能 ③。
3. **C4 坠的 W-hold 漂 0.5-0.8m**：16f hold 覆盖整 2m 坠（~20f）=
   坠中 5-7f W = 0.35-0.5m 漂 + 屋顶 walk z stop 0.3 过冲 → C9 挖
   位全偏（S1/S2 未挖 or aim 上树 leaves）→ 门链死。纪律：hop 化
   （8f W + 释放 + 自由坠）×4 段；落点带收紧后 C9 = 落点原位双挖。
4. **长 walk leg 损耗 0.4-0.5m（accel + stop margin）**：3.1m leg
   实停短 0.46m → 下游 +X leg 在 z∈(-3.3,-2.7) 被一段南墙 z 带
   夹（0.1-0.3m z 重叠 × 0.01m x 重叠即 clamp）→ 整链卡 x1.7。
   纪律：长 leg 目标过补偿（z-2.4→z-1.8：实落 z∈[-2.9,-2.7] 全
   离墙带；且门停 -Z leg 被二段北墙头位 clamp = 终点鲁棒）。
5. **屋顶 walk 的 z stop margin 方向不定（0-0.5m）**：-Z 1.5m leg
   实停 ∈ [-5.5,-6.0]：middle x5 的 y57 floor-crossing z = z_eye +
   1.62×dz/dy —— 1cm 边距 = 1cm 生死（v16/v18 两次踩北行 floor
   stray）。纪律：aim 陡化（dz 1.25→1.7 / y 56.3→56.9）= crossing
   偏移 0.84→1.19m，全 z 带 19-25cm 边距。
6. **场景 journal 容器态与 blocks 三处同步**：移表格 = blocks 条目
   + tables 条目 + 该格 air 条目移除（同格重复 = init "duplicate
   block" 拒收；tables 缺格 = init "not a crafting_table" 拒收）。
7. **gzip 场景资产 Python 读**：`json.load` 直读 =
   UnicodeDecodeError 0x8b（PITS 386 先兆）——gate 脚本/probe 一律
   走 gate_compare.load_save 的魔数探测。

## 门结论（门 224 全 PASS）
- **2998/2998**（内容相位零新增单测——16a init 交叉检查 4 条目全量
  boot 复验 + --list-demos 4 条目执法）；`-Wall -Wextra -Wpedantic
  -Werror` 0 警告（探针移除后重建）。
- **S1 结构免疫第 105 次**：f60 ×2 ≡ base19 逐字节（225272 B，
  3.33.0/3.34.0 双 banner 复验——banner = F3 文本面零像素）+
  savenorm EXACT。
- **canary 4 EXACT**（example/tutorial_dig/tutorial_build/
  tutorial_fly ×2 RAW + savenorm——sim 2 不 bump 执法，3.34.0
  banner 二进制复跑）。
- **guide_house ×2 RAW + savenorm EXACT** + 终态内容断言（上节钉值）
  + 零 panel problem + sleep f5794（夜窗 f<10800 内）。
- **性能 A/B 同档**（旧 = 477082c v3.33.0 worktree + g139 双树 27
  markers：OW 900f 无输入 PERF-WIN 三窗 ×2 跑 upd med
  0.560/0.567/0.573 & 0.580/0.591/0.592 vs 0.584/0.579/0.572 &
  0.579/0.591/0.568 同档 + render 0.607/0.662/0.656 & 0.642/0.709/
  0.646 vs 0.652/0.683/0.648 & 0.654/0.715/0.621 同档 + swap
  15.21-15.30 同档 + changesApplied 9/3/1 逐位 + 终档跨树 savenorm
  EXACT ×2 = 内容相位结构性零成本）。
- **零 format 33 不变 / sim 2 不 bump**（walk_leg 尾参缺省 = 既有
  demo 时间线字节位级；mk_scene 透传 = 工具面）。

## 已知 / 后移
- E walk-around 的终点视位随漂移略偏（巡视面，无断言；终态 pos
  不入钉）。
- 16c = day1 章节 ④⑤⑥③ 全 4 支完成；V3-16 残余 16d ⑦皇宫⑧材料 /
  16e ⑨河流 / 16f ①下界门②末影龙 / 16g ⑩黑夜历险+coop 占位。
- 场景 gzip 资产的扩展名混淆面（Python 直读 0x8b）= V3-18 存档
  扩展名（11-08 用户诉求，BACKLOG 已调研定案 .json.gz 方向）的同族
  收益，随 V3-18 一并收口。
