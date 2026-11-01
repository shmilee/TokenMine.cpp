# PRE-10 图标重叠修复（v1.x 矩阵 31/33，v1.31.0，门 146，PRE 行 4/6）

## 定案

原始定义 = 矩阵标注"图标重叠修复★"（v1.0 砍掉清单回填 = **唯一 bug 级项**）。
消费 KNOWN_ISSUES「效果图标行与吸收红心可视觉重叠」Phase-56 条：效果图标
行与心行**同行**（左对齐 hotbar 左缘）、吸收红心带在正常心带左侧——
图标行够长时画到吸收红心区（现状几何 w=1362 下 **5 个**效果触发：第 5 个
图标 547..563 ∩ 吸收带 552..586；KI 原文"≥3"= Phase 56 旧 slot 几何，
现状已演化为 5，但 shipped 内容可触发：金苹果 regen+absorption + spider
毒 + 信标 speed + 弱点/抗性/力量药水 = 5-6 活跃效果）。

定案 = **MC 三行 HUD 堆栈**（每行独占，行距 = kIconPx + kStripGap = 22，
任意图标数零碰撞）：
- **心行**（hy）：吸收带 + 正常心 + 饥饿/氧气（零改动）
- **效果行**（hy − 22，新）：效果图标从左锚 = **心行最左缘**（池在时 =
  吸收带左缘 `hx0 − kStripGap − absW`，否则 = `hx0`）——MC 布局
- **护甲行**（hy − 44，上移一行）：SURV-1 护甲槽从 hy − 22 上移——MC
  护甲行在效果行之上

纯布局常量改动（game.cpp drawUI 两段），零 format/config/内容/音效/世界
状态/帧状态；HUD 仅指针锁定时绘制 = 无输入基线结构免疫根。

## 实现

- `game.cpp`：效果图标段 `int ex = x0`（hotbar 左缘同行）→ `effX0`
  （心行最左缘）+ `ey = hy − kIconPx − kStripGap`（专属行）；护甲段
  `ay = hy − kIconPx − kStripGap` → `hy − 2×(kIconPx + kStripGap)`；
  注释更新（三行堆栈语义）
- 版本 1.30.0 → 1.31.0（CMakeLists + banner 测试）

## 测试

**1897/1897**（零新测——布局在 GL 路径内不可单测，门 146 像素断言钉死；
PRE-7 HUD 同款先例）、`-Werror` 0、banner v1.31.0。

## 门 146

- **S1 第 146 次结构性免疫 + base18 锚连续 387726 B 免重锚**（fresh 60f
  四方 ×2 全 EXACT + new≡old + 跨版 savenorm EXACT；无 capture = 无
  HUD 效果/护甲绘制 = 结构免疫）
- **S2 HUD capture 矩阵 PASS**（N/A/M × 新旧 ×2 = 12 跑，runin_lib
  原子中心点击协议 + "Pointer captured" 轮询 + **尺寸门 1362×744**
  【PITS 331】+ **漂移门 <0.001 rad**【PITS 332】；黑帧/WM 重排/漂移
  全重试兜底，12 跑全零漂移落收）：存档证据 6/6（效果/护甲/池注入
  原样）+ x2 RAW 6 对全字节同 + **像素断言 7/7**（DPI 1.5× 缩放自探测
  【PITS 330】：a bare HUD old_N≡new_N 字节同【代码级免疫】/
  c **OLD 吸收带 bug footprint 45 px**【第 5 效果图标叠吸收带】/
  d **NEW 吸收带 diff = 0**【修复实证】/ e NEW 效果行渲染 288 px
  【5 图标专属行】/ f **HUD 带外 diff = 0**【世界零回归】/ g 护甲行
  移位双向各 624 px【new @ hy−44 / old @ hy−22】）
- **S3 1897/1897**
- 无 S6（零 parse 面：布局常量，无数据字段）
- **性能 A/B（tm_v130_perf vs tm_v131_perf，同 g139 插桩，fresh --new
  --seed 123456789 900f nomobs 无 capture）：PASS**（banner 双侧实证
  + PERF-WIN 全指标同档 + changesApplied max 9/3/1 三窗逐位 + **终档
  fluids 18/18 + blocks 55/55 集合恒等 = 同世界同成本**【结构惰性最强
  形：无 capture = 零 HUD 绘制】）

## PITS

- **330**（HUD 像素断言须按 DPI 缩放换算：布局虚拟坐标 908×496 vs
  截图物理帧缓冲 1362×744 = 1.5×；黄选中框自探测 s + 虚拟空间算带）
- **331**（门内 WM 重排破 A/B 矩阵尺寸一致性：capture 跑须尺寸门
  只收 base18 锚态 + 重试）
- **332**（capture 跑 x2 RAW 相机漂移门须 <0.001 rad：0.005 漂移 =
  世界 1-2 px 位移破字节同）
