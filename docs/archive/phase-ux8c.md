# Phase UX-8c — E 面板配方两列 + 可合成排序（v1.43.0，门 163）

> 消费用户 v1.41.0 实机反馈②「按 E 打开面板后，配方列的很长，能不能
> 整理编排一下」——E 面板 32 条 shapeless 配方单列满高（405×742），
> 用户 ui.scale 2.0 调大字体后 fit scale 再收缩 = 行文字 ~11 物理 px
> 不可读。**v1.43.0 = UX-8 体验细化系统第 2 版**（系统收官 v1.45.0
> 挂 tag/release，10-31 政策）。

## 定案（两点）

1. **两列排布**：CRAFT-4 工作台面板（shaped 34 条两列）的机制泛化到
   E 面板（shapeless 32 条）——`InventoryLayout::compute` 的
   listW/leftRows/行分列三处**去 kind 分支**，两种面板同款公式（左列
   前 (n+1)/2 行、右列余下）。行高 ≈2×（16 行 vs 32 行分同一高度）。
2. **可合成排序置顶**：纯函数 `orderedRecipeHints` 两遍同序扫描稳定
   分区（canCraft 真在前、假在后，组内保注册表序）——新手打开面板
   先看到"现在能做的"（farmland/glass_pane/planks 白行顶格），发现性
   保留（不可做的暗行仍在，不隐藏）。

**CraftingTable kind 位级不变 = 红线**（CRAFT-4 已门；统一公式对
table 取值逐式同原值 = 回归测试原值 1278×217/3-2 分列钉死）。
工作台 shaped 列表**不排序**（display-only，CRAFT-4 原序，记档偏离）。
行内两行文本 **fitLabel** 约束列宽（name 行保 " xN" 后缀 / 需要行保
前缀只裁原料尾 = CRAFT-4 同款确定性纪律）。

## 实现

- `src/ui/inventory_ui.{hpp,cpp}`：compute 三处去 kind 分支（table 变量
  仅余 gridCols 2/3 用途）+ `orderedRecipeHints` 声明/实现（放
  inventory_ui = 面板呈现序消费方全在 panel 代码，头已含三所需类型，
  game_ui 零新增 include）。
- `src/core/game_ui.cpp`：handlePanelClicks + renderUi **两处 hints
  构造同一 seam**（点击与渲染行号语义一致）；E 面板行渲染两行文本
  fitLabel（`avail = r.w − L(32) − L(4)`）。
- CMakeLists 1.42.0→1.43.0 + F3 banner 钉。

**零 format/config/内容/音效**；面板开 = 输入驱动 = 无输入零 layout
执行（结构免疫根）。

## 测试

2026/2026（+4）、`-Werror` 0 警告、banner v1.43.0：

- **EPanelTwoColumnGeometry**：16/16 分列 + 面板宽 1288/高 896 公式 +
  行距 52 + 列间隙命中空
- **EPanelOddRowSplit**：31→16-15 / 1→单行左列
- **EPanelRowsFitTheColumn**：fitLabel 两行不溢列（含 master_bow 46 字
  最长 needs + "需要：" 前缀存活）
- **OrderedRecipeHintsCraftableFirst**：全可原序 / 全不可原序 / 假真
  假混合稳定分组 / 空列表
- 契约演进 ×2（WideWindowKeepsTheNativeSize：E 面板 native 宽
  1000→1288 使 1280 窗失 scale 1.0 → 1362 窗钉原值；
  NarrowWindowScalesThePanelDown：分母 1000→1288、槽宽 33→26）
- **CraftingTable 位级回归钉原值通过 = 红线守住**

## 门 163（编排者执行，old 侧 = v1.42.0 = 78c7380 = tm_v143_base）

- **S1 = 第 163 次结构性免疫 + base18 锚连续 168140 B 免重锚**：fresh
  60f 四方 ×2（new×2 + old×2）全字节一致 + **跨侧 RAW 字节一致**
  （代码级免疫：无面板 = 零 layout 执行，两列/排序代码全惰性）+ 跨版
  savenorm EXACT。
- **S2 面板 A/B PASS**（E 键开面板 600f 截图，quiet config；首轮 300f 窗 < 脚本固定 sleep+捕获序列 = 游戏先于 E 键帧限退出【门协议：输入跑帧窗 ≥ 序列时长 ×1.5 裕量】，600f 修）：×2 RAW
  双侧（new 211915 B / old 221352 B 字节一致）+ 跨侧 savenorm **EXACT**
  （纯渲染 = 世界零触碰）+ **肉眼终裁**：NEW = 两列 16/16 行、可合成
  白行顶格（farmland/glass_pane/planks——starter wood/glass/dirt 供料）、
  fitLabel 尾截实证（"需要: flint x1, s…"）vs OLD = 单列 32 行注册表
  序（planks 白行沉列 ~60% 处 = 用户痛点本体）；**diff 262,529 px
  全在两侧面板并集内**（分区采样实证：面板外世界像素零 diff，面板内 =
  半透明混合区随布局移动）。
- **S3** = 2026/2026。**无 S6**（零 parse 面——纯布局/呈现序，零新
  数据字段）。
- **性能 A/B v1.42.0 vs v1.43.0（发布前硬门槛）PASS**：fall 900f
  串行零并行——PERF-WIN 三窗 upd med 0.414/0.376/0.432 vs
  0.371/0.399/0.401、render med 0.468/0.451/0.471 vs
  0.461/0.432/0.420、swap med 15.61/15.67/15.65 vs
  15.60/15.69/15.75 = 全指标同档；PIPE3-S0 changesApplied **9/3/1
  三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 =
  同世界同成本**（tutorial 双侧同值 {walk,0}；无面板 = 两列/排序
  代码零执行 = 结构惰性最强形）。

## 偏离/记档

- E 面板 native 宽 1000→1288 = 679 半平铺窗 fit scale 0.453→0.352
  （行文字物理 px 因行高 2× 实际可读性提升；ui.scale 2.0 用户态同
  向受益）——布局面积换行数，CRAFT-4 同款权衡。
- 工作台面板不排序（display-only 已门）；将来若要 = 同一纯函数泛化
  （一行 `if (!table)` 去掉）。
- 排序随库存实时变（canCraft 每帧查询）= 开面板时放材料行序会动
  （确定性纯函数，×2 RAW 同库存同序 = 门已证；MC 无此列表 = 无对标
  约束）。
