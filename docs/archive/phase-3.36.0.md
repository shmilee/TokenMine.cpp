# Phase v3.36.0 — V3-16e 扩展指导 demo 族：⑧ 材料（门 226）

> 3.x「世界扩展」V3-16 相位 E/7（build 章节第二支）。scene/panel_click/
> 索引内核全复用（16a 形）——本相位 = 内容（1 scene + 1 demo + 1 index
> 条目）+ 工具链增量（demo_author 向量行走模型）+ **内核 1 行×10
> 处**（demo 恒锁定）。22 轮根因迭代（PITS 417 二十二坑）= 本相位主体。

## 实现

### scene guide_materials（mk_scene 手术，幂等 21831 B）
30×26 平台（x0..29, z-25..0, 顶 y54，stone 垫高 y50-53）于**正午**
（time 0.25）+ player (15.5,54,-1.5) **SURVIVAL**（收集面——真破格
时长/磁力收集/E 面板展示）+ **预置 wooden_pickaxe 于 h2**（矿脉面，
pilot ④ 的 prepared-tool 孪生）+ 8580 unique cells / 8587 edits。

**7 biome 床**（表面 = y53 替换，标本 = y54 凸块/结构）：
- B1 森林（x2..11, z-23..-15）：grass 90 + 橡木 3（(4,-19)(7,-17)
  (10,-19) 三层树干 + 5×5 叶冠）
- B2 沙漠（x14..23, z-23..-15）：sand 90 + 仙人掌 2（(16,-21)(21,-17)，
  **v3.20 搬迁**——(16,-20) 原位在走线 0.2m 擦碰族）
- B6 羊毛（x25..28, z-23..-15）：grass 36 + 羊毛 3（z-18 排）+ 荧石 2
  （z-15 对）
- B3 针叶林（x2..11, z-10..-3）：snow 80 + 云杉 2（(5,-7)(9,-5) 六层
  树干）+ 雪块 3（**(3,-10)(9,-10)(3,-3)，v3.20 搬迁**——(9,-8) 原位
  与 z-8.5 走线 0.2m 擦碰 = 12 轮迭代的死点）
- B4 矿脉（x14..23, z-10..-3）：stone 出露 x16..19 z-8..-6 双层
  （**depth-1 定案**：z-7/-8 行 CUT，尾目标恒地板）+ 煤 2 / 铁 1（z-6
  南面）
- B5 花田（x25..28, z-10..-3）：grass 32 + 葱绒 2/矢车菊 2/郁金香 2
  （non-solid 可穿）
- 走道：z-14..-11 行 + z-2..0 行 + x0..1/x12..13/x24/x29 列

### demo guide_materials（25383f = 07:03，801 actions）
**tour**：P0（捕获 + 扫视）→ B5（6 花 1f tap）→ B4（**ore 先 stone
后**：铁 1 煤 2 裸手 1.25s 短拍 → 切 h1 石 3 ×2.3s——单拍链破 3 格
的修复形 PITS 417⑮）→ B6（羊毛 3 链 + 荧石 2）→ B2（仙人掌 2 + 砂
4）→ B1（木 3 + 草 2）→ B3（雪 3 + 云杉 2）→ E 面板展示（1020f 观众
停）→ C 第三人称四面巡视（4 线 9-22m + 4 扫视对）→ 收束 600f。

**22 轮根因迭代（v3.1-v3.22，全 PITS 417）** 的核心定案：
- **行走 = ENT-7 镜像模型**（指数逼近 4.3 + 松键 9f 滑行，k 8.0/12.0
  与 player.cpp 358-394 孪生）+ **v3.21 全向量化（根因 7+8）**：角进入
  速度沿旧轴（垂直新腿）——scalar 模型假设 entry 沿轴 = 每腿沿轴短走
  0.02-0.16m + 横向漂移 0.07-0.08m/角（25 腿累积 0.7m = B4 段脱轨
  根；旧稳态链 entry 假设对 rest-entry 短走 ~0.19m = B3 段脱轨根）；
  修 = _walk_dd(n, vx, vz, axis)（轴逼近/垂直 (1-FA) 衰减/coast 双
  分量/负轴符号）+ _coast 带位移（settle 间隙残余滑行 0.05m 实证）+
  诚实落点 + **walk_to v3.21**（2m 分块 + <2m 精确残差 + 0.66m 可
  表达下限【同向 entry 0.7m/s 时 8f 最小腿】+ 横向 ≤0.15m snap 自
  校正）；rest-entry 逐帧精确（17f +0.010 实测 vs +0.0096 模型）。
- **aim = 面点**（侧面带内点/顶面中心；4 格角点 aim = DDA 跳格坑
  ⑯；射线首 solid 33/33 审计）。
- **收集红线**：磁吸 = 速度 6(1-d/2.5)²（d>2.5 无）+ 前 0.5s 不吸不
  拾 + ready 后 d≤1.0 当帧即拾（⑱）→ 每 drop 必有轨迹帧 ≤1.05m
  （stand d≤1.0 或 detour 路过点；33/33 审计）。
- **间距规则**：走线/stand 与完好 y54/55 实体 ≥0.8m 首选；0.2m 平面
  部类保留（B4 出露 z-4.5 线等 12/12 实证 + entry-aware 腿 ±0.03m
  校准 = 0.17m 结构裕量）；**自家树干 y55 层拦体带**（⑲：破 y54 底
  格后层格仍挡，同柱线须绕）。

**终态钉**：33 标本全破（y54 凸块/花/矿格 None 或 'air'）+ 保留格
（出露 x18 双层 + 树干 y55/y56 层 + (9,53,-5) 雪地板 67% 部分破）+
地板 y53 零挖 + 零实体（33 drop 全收集）+ 库存精确（wool 3/glow 6
每块 3/cobble 3/coal 2/iron 1/cactus 2/sand 4/wood 3/dirt 2/snow 3/
spruce 2/花 6 + pickaxe 1）+ 终姿 (15.5,-1.5)±1.0 y54 hp 20。

### 内核 1 行 ×10（demo 恒锁定）
`m_demoPlayer ? true : m_window.relativeMouseMode()`：demo 回放 = 纯
注入，物理锁窗 flap（焦点丢失/OS 释放 relative mouse）不得吞注入事件
——**门内 5/5 复现**（用户移窗期间注入面事件静默丢失 = v3.1-v3.12
的"随机脱轨"红鲱鱼源头）。初 3 处（game_frame.cpp:423 inputLocked 镜像
+ game_interaction.cpp:99 mine 门 + game_ui.cpp:334 handleUiKeys）+
**canary RAW diff 捕获 7 处**（新 vs 旧二进制截图 byte 35/36 + savenorm
EXACT = 像素面分叉 sim 位级恒等 → 会话/渲染态成因；example 全帧空间
渐变 diff 909631 px = 视角非文字叠层）：C 相机 toggle（game_ui.cpp:260，
会话态——同一 C 事件【sdl_key 99：example f255-260 / tutorial_build
×6】一 run 处理一 run 丢弃 → First↔ThirdBack 分叉，相机不存档 =
savenorm 盲区）/ T 教程（game_ui.cpp:313，存档面）/ HUD locked
（game_ui.cpp:1458，截图面）/ 进食通道（game_interaction.cpp:2616，
存档面）/ ESC 退出（game_init.cpp:3069，环境暴露面）/ 手臂渲染
（game_render.cpp:534）/ 准星渲染（game_render.cpp:651，后两者 =
tutorial_dig/fly 无 C 亦 RAW diff 的成因）；12 处
relativeMouseMode() 读取审计全表，2 处有意不门控（脚步音 = 仅声音 /
net FrameState = 与 demo 互斥）。solo/net 无 demo 时位级不变
（m_demoPlayer null 门，S1 ≡ base19 实证）。

### 工具链增量
- demo_author 向量行走模型：_walk_dd(n, vx, vz, axis) 角进入速度轴
  归属（⑰㉑）+ _coast 带位移 + 诚实落点 + walk_to 0.66m 可表达带。
- **分段前缀验证（用户定案形，≤30 段 = 11 段，⑳）**：trim =
  at_frame≤F 保留 + F 处 wheel-0 延长按 + 跨 F hold 响亮拒；每段
  --frames F+100 + 段检查（pos ±0.5m 漂移金丝雀 / 累计 broken 集合 /
  累计库存精确 / 实体 0 / hp 20 / 地板零挖）；链式串行首 FAIL 即停；
  前缀 = 全跑逐帧恒等（确定性）→ 段 PASS 可传递。
- 审计脚本（/tmp 门资产）：碰撞（SOLID 含 y55 层）+ 33 首 solid +
  drop 收集轨迹帧 + 间距分级。

## 测试
2998/2998（+banner re-pin 1，内核零新增单测——内容相位先例 16b/16c/16d；
demo_author 模型 = Python 工具面）；-Wall -Wextra -Wpedantic -Werror 恒
0 警告（restore 后最终二进制复验）。

## 验证（门 226）
最终二进制（无探针无插桩，10 处锁定门控 + banner 3.36.0）全臂 PASS：
- **分段链 11/11 PASS**（05:02:10 收官，用户定案形 ≤30 段 = 11 段）：
  全段 pos 偏差 ≤0.017m / 33 破 / 0 实体 / hp 20；前缀 = 全跑逐帧恒等
  → 段 PASS 可传递。
- **arm A**（25883f，07:05:56）：MATERIALS CHECK PASS【33/33 破 +
  保留格 OK + 过挖 0 + 地板 y53 零挖 + 实体 0 + 库存精确 + pos 带
  (15.5,-1.5)±1.0 + hp 20】。
- **arm B**：MATERIALS CHECK PASS + gmA≡gmB **RAW EXACT + savenorm
  EXACT**（同 dmo ×2 = 确定性最强形）。
- **canary 4×2**：新新 RAW EXACT ×4 + **新 vs 旧二进制（tm_v335
  v3.35.0）RAW EXACT ×4 + savenorm EXACT ×4** = 10 处锁定门控的 solo
  冷路径位级证明（C 相机会话态修复实证：首轮 probe 跑的 byte 35/36
  全屏渐变 diff 消除）。
- **S1** f60×2 ≡ base19（225272 B）**byte-exact** + savenorm EXACT
  （结构免疫第 107 次：内容相位 + 锁定门控全冷）。
- **性能 A/B 同档**（g139 双树 27 markers，OW 900f 无输入，邻窗
  07:46 vs 07:47 复跑定档）：upd med f300 0.407 vs 0.386 / f600
  0.412 vs 0.387 / f900 0.381 vs 0.688（旧侧窗内环境噪声带，双跑
  先例形）+ render 0.467/0.465/0.451 vs 0.480/0.469/0.762 同档 +
  swap 15.5-15.7 vs 15.0-15.7 同档 + **changesApplied 9/3/1 逐位** +
  **终档跨树 savenorm EXACT ×2**【old≡new≡new2 三方 = 10 处锁定门控
  结构性零成本】。
- **2998/2998**（07:49:41 最终二进制，-Werror 0 警告）。
- **index.json 6 条**：`--list-demos` boot 交叉检 OK（文件 + strict
  load + SIM 门 + length 25383 匹配 + scene 存在）。

## 偏离
- 矿脉 depth-1（z-7/-8 CUT，MC 3 深读）= 单拍链破结构修复，视觉面
  不变（出露仍 4×2 顶面 + 南面矿）。
- 仙人掌/雪块搬迁 = 走线净距（MC 无固定标本位，生成域自由形）。
- 第三人称巡视 = 观众面（16c/16d 同款）。
