# PRE-12 tooltip/F3（v1.x 矩阵 33/33 = v1.x 收官，v1.33.0，门 148，PRE 行 6/6）

## 定案

原始定义已佚（矩阵短标签"tooltip/F3"）= 定案 **悬停 tooltip 补齐 + F3
呈现框架**（消费三处记档边界：Phase-15"物品名 tooltip 只有名字——无
数量/描述/工具耐久/附魔文本"、SURV-6"档位机制/存档/行为 vs **F3/tooltip
显示**"、SURV-7"统计**显示** = PRE-12 呈现框架边界，本相零 UI"）：

1. **物品 tooltip 补齐**（纯函数 `ui/item_tooltip.itemTooltip(item,
   stack)`，campfire 范式单测拥有）：悬停格 = `name`（count 1）/
   `name xN`（count > 1，与配方行输出 " xN" 同款式）+ 工具第二行
   `damage/max`（F3 held 行同款 d/max 读法，满耐久显示 0/max——tooltip
   是文本，区别于 HUD 条满耐久零绘制）；**无描述/附魔文本**（项目无
   附魔系统、item JSON 无 description 字段 = 记档偏离，MC 同款无描述）
2. **F3 呈现框架两新面**：
   - 效果行带**档位罗马**（`effectTierRoman` 纯函数：tier 0 = "I" MC
     amplifier 惯例 / 1 = "II" / 2 = "III"，越界钳制 = 防御）
   - **stats 行**（SURV-7 计数呈现：`stats deathCount 1 | kill.chicken
     3 | ...` 键序，m_stats 只存非零 → 空 = "stats (none)" 基线行）
3. **tooltip 渲染层修复（门内发现的 Phase-15 潜伏缺陷）**：旧 tooltip
   与面板共享 kText batch（batch 按首用插入序 = 面板标题处，先于槽底
   kSlotBg/边框/图标绘制）+ 偏移 L(12) 随面板 fit scale 收缩
   （679×744 窗 s≈0.286 → 文本 4.6px 紧贴光标）= **悬停 tooltip 恒被
   槽底+图标覆盖 = Phase-15 起实际不可见**（E2E 首触发实证）。修 =
   tooltip 独立 batch（title kText / 耐久行 kTextDim），**绘制 pass 移
   到 2D batch 与 3D 块立方图标之后**（UI 最顶层，PRE-7 效果纱之前；
   无悬停 = 零 batch = 无输入基线不变）。
   **记档偏离**：效果图标 hover tooltip = 结构不可行（效果行仅
   locked 时绘制，locked 下物理光标固定在屏幕中心 = 悬停恒 false；
   档位/时长呈现 = F3 通道，MC 同款信息）。

零 format（22）/config/内容/音效/帧状态（F3 = 既有 m_debugOverlay
开关，tooltip = 既有悬停状态）。

## 实现

- `src/ui/item_tooltip.{hpp,cpp}`（新）：`ItemTooltip{title, durability}`
  + `itemTooltip(const Item&, const ItemStack&)` 纯函数
- `src/ui/debug_overlay.{hpp,cpp}`：`DebugInfo.stats`
  （map<string,uint64_t>）+ `effectTierRoman(int)` 纯函数 +
  `debugOverlayLines` +1 stats 行（键序 " | " 连接）
- `src/core/game.cpp`：三悬停点（炉子面板槽 / E 面板槽 / E 面板格）接
  `itemTooltip` + `appendTooltip` 独立 batch + cube 循环后 tooltip
  绘制 pass；F3 填充：效果行带 `effectTierRoman(a.tier)` +
  `info.stats = m_stats`
- `CMakeLists.txt`：item_tooltip.cpp + test 注册、1.32.0 → 1.33.0

## 测试

**1912/1912（+7：test_item_tooltip.cpp 新 5——单件无耐久/堆数 xN/满
耐久 0/max/磨损 d/max/数+耐久组合；test_debug_overlay 演进 2——stats
行多条目键序+单条目无分隔+effectTierRoman I/II/III/钳制）、banner
v1.33.0**、`-Werror` 0。

## 门 148（全 PASS 25/25：in-gate 21 + 离线 diff box 4）

矩阵态 = **1362×744 全平铺**（unmap firefox covers 后 game 独占全屏；
PITS 314 同款协议）。

- **S1 = 第 148 次结构性免疫 + base18 锚连续 168140 B 免重锚**（fresh
  60f 四方 ×2 全字节一致 new≡old + 跨版 savenorm EXACT；无 F3/无面板/
  无悬停 = PRE-12 全代码路径零执行）
- **S2a F3 stats 行 A/B PASS**：f3n/f3o（F3 键输入跑 400f，全零漂移
  cam=(0,−0.25)）——NEW F3 末行 `stats deathCount 1 | kill.chicken 3 |
  kill.sheep 1` vs OLD 无该行：diff **4762 px 全限 F3 盒（全宽 × 前
  15 行）带外 0** + 存档 stats 精确双侧 {deathCount:1, kill.chicken:3,
  kill.sheep:1}
- **S2b F3 效果罗马 A/B PASS**：romn/romo（M2 场景 5 效果 500s）——NEW
  效果行带 " I" 罗马 vs OLD 无：diff **4761 px 限 F3 盒带外 0**（效果
  remaining real-dt 数字差亦在盒内）+ 存档效果名 5/5 双侧
- **S2c tooltip 数量后缀 PASS**：tip1×2 侧（E 面板 hover 热栏 slot1
  dirt）——**new x2 RAW + old x2 RAW 字节同** + 跨侧 savenorm EXACT +
  NEW-vs-OLD diff **66 px 限 tooltip 盒（光标右下 280×55）带外 0**
  （NEW "dirt x64" 文本 vs OLD 无可见 tooltip【旧文本恒被槽底覆盖 =
  PITS 336 缺陷】）
- **S2c2 tooltip 耐久行 PASS**：tip2×2 侧（hover slot8
  wooden_pickaxe）——同款断言全 PASS，diff **78 px 限盒带外 0**（NEW
  "wooden_pickaxe" + "0.0/59" 双行 vs OLD 无）
- **S3 = 1912/1912 PASS**
- **性能 A/B（tm_v132_perf vs tm_v133_perf，同 g139 插桩，fresh --new
  --seed 123456789 900f nomobs 无 capture）：PASS**（banner 双侧实证
  + PERF-WIN 全指标同档 + changesApplied max 9/3/1 三窗逐位 + **终档
  fluids 18/18 diff 0 + blocks 55/55 diff 0 = 同世界同成本**【结构惰性
  最强形：无 F3/面板/悬停 = PRE-12 零执行】）

## PITS

- **PITS 337**（capture 点击的 `mousemove A B click 1` 单命令 X 竞态：
  XWarpPointer 异步 hint，press 可落旧光标位置 → 固定捕获 warp 漂移
  0.435/0.725 rad【174/290 px @ k 0.0025】，同窗同点击点 3 跑
  0/漂/漂 复现 → `mousemove --sync` + 独立 click 分命令）
- 门内发现（非编号）：**E 开面板须先指针锁定**（`relativeMouseMode()`
  门控）= 点击捕获 → E → hover 序列；未 unmap firefox covers 时点击被
  浏览器窗偷走（PITS 314 复现，calib 实证 captured=0）；**面板 fit
  scale 下旧 tooltip 恒不可见** = kText batch 首用于面板标题（先于槽
  底/图标绘制）+ L(12) 偏移随 scale 收缩（679 窗 s≈0.286 文本 4.6px
  紧贴光标全落槽内）双杀 —— Phase-15 起潜伏，E2E 首触发
