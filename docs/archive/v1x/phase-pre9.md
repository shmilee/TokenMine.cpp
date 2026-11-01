# PRE-9 箭/弓/手视觉（v1.x 矩阵 30/33，v1.30.0，门 145，PRE 行 3/6）

## 定案

原始定义已佚 = 定案 **箭视觉（MC 式三段箭形：棕色杆 + 灰色锥形箭头 + 白色
尾羽）**（消费 KNOWN_ISSUES「弓 / 箭」节 Phase-21 条"箭无纹理 / 无箭头 /
无尾羽——单一棕色细盒"）。弓（3D model + 拉弓指示，CRAFT-2/Phase-42）与手
（3D 手持 model，CRAFT-2/ENT-9）已大体解决，本项聚焦最明确的缺口 = 箭几何：

- **三段几何**（`makeArrowGeometry` 返回 `ArrowGeometry{shaft, tip,
  fletching}`，纯函数）：
  - **棕色杆 shaft**：细盒（kArrowThickness×kArrowThickness×kArrowShaftLength
    0.31，居中）
  - **灰色箭头 tip**：四棱锥（kArrowThickness 方基 @ 杆前端，顶点 @ 箭前端
    -Z，尖头 = MC 箭头）
  - **白色尾羽 fletching**：宽盒（kArrowFletchWidth 0.09 > 杆 0.05，@ 箭后端
    +Z，羽毛外扩）
  - 三段总长 = kArrowLength 0.55（与 PRE-9 前盒长一致 = 整体轮廓尺寸不变，
    A/B diff 是形状+颜色而非尺寸）
- **三色三次纯色 draw**（零 shader/纹理/atlas 改动）：`drawArrows` 每箭按
  shaft(棕)/tip(灰)/fletch(白) 三次 `u_color` draw，同 model（经典 MC 箭色）
- **箭朝向不变**（`arrowOrientation` 把 local -Z 转到速度方向，帧锁定物理 =
  确定性）
- **零 format/config/内容/音效/世界状态**（纯渲染几何）

## 实现

- `arrow_mesh.{hpp,cpp}`：`ArrowGeometry` 三段结构 + `makeArrowGeometry`（box
  helper 复用 kCubeFaces + pyramid helper 四面外绕序）+ 三段尺寸/颜色常量
- `game.{hpp,cpp}`：`m_arrowMesh` → 三段 mesh（shaft/tip/fletch）+
  `buildArrowMeshes()`（幂等懒构建）+ `drawArrows` 每箭三段 draw
- 无资产变更（箭几何是代码，非数据）

## 测试

**1897/1897（+3 净：4 旧几何测 → 7 新三段测）**：PartVertexCounts（杆/羽
24v/36i、头 12v/12i）+ PartsTileTheTotalLength（三段和 = 0.55）+
WholeArrowSpansLengthCenteredOnOrigin（全箭 z ∈ [-0.275,+0.275] 两端达）+
TipIsPointedPyramidAtFront（尖头 4 顶点 @ -L/2 轴上 + 8 基顶）+
FletchingWiderThanShaft（羽宽 > 杆宽）+ WindingIsOutwardOnEveryPart（杆/羽
盒外绕序【对盒自身中心，fletch 偏移原点】+ 头锥四面外法线）+
FullSkyLightNoBlockLight（三段全 sky 1.0）。`-Werror` 0，banner v1.30.0。

## 门 145

- **S1 第 145 次结构性免疫 + base18 锚连续 387726 B 免重锚**（fresh 60f
  四方 ×2 全 EXACT + new≡old + 跨版 savenorm EXACT【零 format 变更】；
  无输入无箭 = 零 arrow draw 结构惰性）
- **S2 箭 A/B f170 PASS**（skeleton-only 脚手架 attack_damage 0 + 去
  arrow_effect → 箭飞行但 0 伤 0 红闪，满钻甲保险，玩家 hp 20 恒）：
  NEW×2 RAW 497478 B 字节同 + OLD×2 RAW 497489 B 字节同 +
  **NEW-vs-OLD diff 2512 px 限 bbox x[661,700] y[53,134]**（箭区，
  锥形灰头+白尾羽 vs 棕盒；crop 4× 放大实证三段箭形）+ savenorm
  NEW==OLD（箭瞬态不入档）+ 六跑全一次成功（无黑帧重试）
- **S3 首轮 1896/1897**（`MobContent.SkeletonUndeadAndWitherArrowPinned`
  失败 = **资产脚手架未还原**，内容钉死单测读真实资产 → **PITS 329**；
  还原后重建 **1897/1897**）
- 无 S6（零 parse 面：箭几何是代码，无数据字段）
- 性能 A/B（tm_v129_perf vs tm_v130_perf，同 g139 插桩，fresh --new
  --seed 123456789 900f nomobs）：PASS（banner 双侧实证 + PERF-WIN 全
  指标同档【render med +0.02 ms win1/2 = 环境噪声级】+ changesApplied
  max 9/3/1 三窗逐位 + **终档 fluids 18/18 + blocks 55/55 集合恒等 =
  同世界同成本**【结构惰性最强形：nomobs 无箭 = 零 arrow draw】）

## PITS

- **328**（护甲减伤 floor 在乘积上非最终值：满钻甲 20 点不免疫箭伤 2
  → 0.4 伤 + 红闪 real-dt 破 ×2 RAW；零红闪场景须伤害源头为 0）
- **329**（门内 S3 套件与资产脚手架互斥：内容钉死单测消费工作区
  assets/，S2 脚手架须在 S3 前还原）
