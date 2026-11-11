# Phase v3.37.0 — V3-16f 扩展指导 demo 族：⑨ 河流（门 227）

> 3.x「世界扩展」V3-16 相位 F/7（build 章节第三支）。scene/panel_click/
> 索引内核全复用（16a 形）——本相位 = 内容（1 scene + 1 demo + 1 index
> 条目）+ 工具链增量（mk_scene fluids/campfires 两 strict 面）+
> **零内核**（水桶/流体/营火/火把/游泳全既有——16b/16c/16d/16e 同款
> 内容相位）。

## 实现

### scene guide_river（mk_scene 手术，幂等 10232 B）
24×15 平台（x0..23, z-15..-1, 顶 y54，stone 垫 y50-52 + **dirt 表层
y53**【裸手 0.75s/格挖掘面】）于正午（time 0.25）+ player
(1.5,54,-4.5) **SURVIVAL** + 预置 **bucket h0 + torch h1**（河流两物
品 prepared-start 面）：
- **密封源水池**（x18..21, z-13..-10，**1-deep**：y53 water level 8
  ×16 源，y52 stone 底）——2-deep 捕获：顶层源"resting on a fluid"
  = 模型无水平传播 → 舀痕永久空洞；1-deep 舀痕 = 邻源 1 tick 补
  level-7 稳（probe B 实证）。
- **campfire (10,53,-7) LIT + 4 空槽 state**（沟中央——挖掘 15 格
  的 x10 skip 格；6-邻接水即灭 = 教学对比件）。

### demo guide_river（19841f ≈ 05:31，443 actions）
tour = P0 开场 → 池巡视 → **挖河 15**【x2 顶面自 bank + x3..x9 沟内
侧面带 + x10 过 campfire + x11..x17；drop 沟底 d=0 磁吸随进随收】
→ **舀水 1**（池远角源 (19,53,-13)——DDA 核：第二舀必换格 (20,53,
-13)，level-7 痕不可舀）→ **倒水 W**（沟底面带 (2,53,-7)——站位 re-anchor pin (1.58,-7.0)
的 y53 穿越落格定 x2）→ 观西梯度【8..1 铺 8 格，营火邻格 x9 湿 =
T0+21 tick，灭 = T0+22 帧】→ 舀水 2 → 倒水 E（(17,53,
-7)）→ 观东梯度【2..8】→ **火把置沟北岸 (14,54,-5)**【水火对比教
学：营火灭 vs 火把亮——MC 偏离记档】→ **游泳横渡**【step-in 15f +
14m 252f + space 25f 升 + step-out 30f——probe B v3 pin，实测落点
(0.65,54,-6.16)，游后诚实 re-anchor】→ E 面板展示【15 泥土】→ C
第三人称双线巡视【终点 21.5 = 东缘 2.3-m margin】→ 收束。

**行走路线 = 列/行 2-m 分块 + 末腿同轴纪律**（PITS 418⑬ 两实例）：
关键 aim/entry 站位（倒水/舀水/游泳入口）只由 2-m 分块的列/行末端
到达，且**末腿须与 entry 的 aim 轴正交**——游泳入口实例：4-m 东腿
coast 后 at(20) settle 不足 + 0.8-m jog = 入口 x +1.77-m 东冲
（track 18.5 → 实际 20.27）→ 252-f 泳 pin 1.77-m 短停 x6.25 河心
y53（后续 20-m 巡逻东腿从 x6 起 → 冲过平台东缘 y38 hp13 门内
捕获）；修复 = 入口列 x18.5 走 z 轴末腿（2.0+0.8-m 同轴，coast
仅 z 向）→ 入口 x 列锁定，实测落点 (0.65,54,-6.16) ≡ probe pin。
倒水站位须在目标格行内（z-7 沟行，y54 穿越点落格盘算 PITS 418⑭——
x17.5 列穿沟端格 (17,53,-7) = 挖后卡死，x18.5 列穿池落水 PITS
418⑮）；浅水格（level≤7 水面 <54）feet 54 无严格重叠 = 干走路
合法（strict-inequality 协议面）；长开放路径（20-m 巡视线）漂移
0.5-1.1-m 累计（16e 律）→ 近平台缘巡视终点留 ≥2.3-m margin（22.5
→ 21.5 门内捕获）。

### 稳态钉（probe A/B 实证）
- 沟 (x2..x17, y53) = [8,7,6,5,4,3,2,1, **campfire_extinguished**,
  2,3,4,5,6,7,8]（倒水 x2/x17——站位 y53 穿越点落格定案（re-anchor
  pin (1.58,-7.0) 后 x2 确定）；营火邻格 x9 level-1 湿 = 灭帧
  T0+19；1 源 = 最远 8 格 reach【L-1 递减 + level-1 终膜】）。
- 池 = 14 源 + 2× level-7 舀痕（(19,53,-13)/(20,53,-13)）。
- campfire 灭帧 = T0 + 22【T0 = 西倒帧对齐 3f；x9 = 第 7 次铺展
  tick = T0+21 湿，灭 = 次帧 pass——demo x2 倒水形；probe A 预置
  双源形二分 pin f16 为另一几何】。
- 流体 tick 每 3f 帧锁定（kFluidTickEveryNFrames）→ 全铺展 ≤30f，
  观流窗 = demo pacing（非物理时长）。

### 工具链增量
- **mk_scene fluids 面**【[x,y,z,level,"kind"] strict 五元，level 0 =
  抽干，base worldgen journal 碰撞响亮拒】+ **campfires 面**【[x,y,z]
  → 4 空槽 state entry；块须 spec blocks 自携 "campfire"——journal 重
  复块响亮拒 = probe A 门内捕获①】。
- 分段前缀验证 14 段（≤30 ✓，16e 同款 trim 纪律）。

## 测试
2998/2998（0 警告 -Werror；banner re-pin 1 = test_debug_overlay
3.36.0→3.37.0；内核零新增单测——内容相位先例 16b/16c/16d/16e）。

## 验证（门 227 全 PASS）
- **分段前缀 14/14**（开发期链验证：s1..s14 全 PASS；s7 三轮 + s6
  落水 + s8 cascade + s10 torch + s11 游泳 + s13 巡视东缘 = 门内
  捕获族，PITS 418 十六坑）。
- **arm A/B**：guide_river ×2（gmA/gmB，19841f+500）RAW EXACT
  （433812 B）+ savenorm EXACT + s14_full checker 全钉【对称 V 沟形
  + 池 14 源 2 痕 + 营火灭 + torch (14,54,-5) + 零实体 + 终姿
  (1.36,54,-4.76) hp 20】。
- **canary 4×2**（example/tutorial_dig/tutorial_build/tutorial_fly）：
  新新 RAW EXACT + 新 vs 旧二进制（tm_v336 v3.36.0）RAW+savenorm
  EXACT ×4【banner 差 = F3 文本面零像素，16e 先例】= 锁定修冷路径
  位级证明（16f 零内核，src/ 逐文件恒等仅 banner 差）。
- **S1 第 108 次**：f60 ×2 ≡ base19 逐字节（225272 B）+ savenorm
  EXACT + banner 3.37.0 零像素复验。
- **性能 A/B 同档**：旧 = tm_v336（4139b4e v3.36.0 + g139 27
  markers）+ 新树 g139 双树：banner 实证 v3.36.0/v3.37.0 + OW 900-f
  无输入三窗 upd med 0.555/0.609/0.583 vs 0.599/0.626/0.625 同档 +
  render 0.563/0.708/0.642 vs 0.620/0.706/0.683 同档 + swap
  15.32/15.23/15.27 vs 15.23/15.18/15.18 同档 + changesApplied 9/3/1
  逐位 + **终档跨树 savenorm EXACT = 结构性零成本最强形**。
- **index boot 交叉检**：--list-demos 7 条目全量（文件 + strict load
  + SIM 门 + length 19841 匹配 + scene 存在）OK。

## 偏离
- 2-deep 池弃用（顶层源无水平传播 = 模型性质，舀痕永久）→ 1-deep +
  level-7 痕（视觉 = 水面 1/8 浅痕，记档）。
- 火把不入沟（块占格 = 隔断水流 + 挖格冲突）→ 沟北岸放置
  (14,54,-5)【站位 (14.5,-3.5) 盘算：y54 穿越 z-4.61 落格 (14,54,-5)
  air → 首 solid = (14,53,-5) bank 泥顶；站位 AABB 与放置格零重叠
  （-4.0 站位 0.3-m 重叠被 canPlaceAt 拒 = 门内捕获）；x14 水面
  level-5 在火把南 2 格，视觉对比面保留】。
- 营火灭帧由西水触发（非东水——东梯度 x11 先湿但 campfire 已灭；
  教学读 = "水漫到营火即灭" 单一因果）。
- MC 对比：torch 入水不灭（项目数据面，16f 教育利用）；无鱼/无落沙
  成石（内核缺口，不教）。
