# CRAFT-4 配方提示（v1.x 矩阵 22/33，v1.22.0，门 137，CRAFT 行 1/4）

## 定案

原始定义已佚 = 定案 **工作台面板 shaped 配方提示列**（消费 KNOWN_ISSUES
Phase-17 条「工作台面板无配方列表——表格面板只有 3×3 网格 + 输出格，无
任何配方提示；配方提示只存在于 E 面板列表（且该列表仅 shapeless），
shaped 配方的摆法没有游戏内指引」）：

- **工作台面板（3×3）加 shaped 配方提示列**：33 个 shipped shaped 配方
  （注册表文件序）排成 **两列**（每列 `kCraftWidth` 280，列间
  `kRecipeColumnGap` 8；33 行单列会把面板拟合因子压到 ~0.34、mini
  pattern 缩到 5 px 不可用——两列 = 行高 ~27 UI px / 格 ~7 UI px）。
- **每行 = mini 3×3 pattern + 输出图标 + "name xN"**：pattern 按定义
  左上对齐（本身是网格匹配接受的一个合法摆放），暗格底 + 每 pattern 格
  一个键位物品小图标（空格 = 空暗格）；name 颜色 = **可供应则白 / 否则
  暗**（`canSupply(inventory, patternBag(recipe))` 纯函数：pattern 每覆
  盖格 1 单位、空格除外、同物品跨字符合并、ItemID 升序）。
- **提示行点击 = 消费性 no-op**：不合成、不动网格/背包，但**吞掉点击**
  ——区别于"所有槽外点击丢弃 carry"规则（left-click 落到行上 ≠ 落到槽
  外）。右键落行 = 既有"槽外右键 no-op"位级路径。
- **E 面板位级不变**（shapeless 列表 + 布局 + 渲染分支零改动；
  `compute()` 对 Inventory kind 的几何逐式未动 = 既有测试全绿钉死）。
- **长 name 行内溢出 = 确定性 `fitLabel` 尾截**（" xN" 后缀恒保，先截
  name——行宽 280 UI 下 18+ 字符 item id 会越列，如 leather_chestplate）。

**顺带修复（PRE-1 潜伏漏字，P65 先例 = 触及面内小修并入）**：
字体 CJK 静态表 32 字形漏 **作（U+4F5C）**——PRE-1 的"18 UI 字"实为
19 字误计（你死了按重生**熔炉工作台**背包合成需要关闭）→ 工作台面板
标题自 v0.25.0 起一直渲染 "工?台"（门 137 S2 截图首曝）。修复 =
`kCjkGlyphs` 改显式 (cp, cell) 表 33 条：既有 32 条 cell 0..31 逐位
不动，作 取 **死格 cell 127（ASCII DEL，col 7 row 15，永不渲染）**——
font.png 256×512 尺寸零变化、**全图仅 cell 127 一个格变化**（56 ink
px，逐像素 diff 实证）、既有 32 CJK + 全 ASCII UV 位级零变化（PRE-1
双红线保持）。字形提取 = NotoSansCJK-Regular.ttc SC 面 em 16 行盒
rows 5..20 阈值 ≥128（PRE-1 同款规格）；**提取管线自校验 = 32/32 既
有字形逐位复现**后才信任新字形。FNV 钉重锚 3e4d748347f5f658 →
**74b5d1418347dab6**。

**记档偏离**：① 一键自动摆放（点击提示行把 pattern 填进网格 = MC
配方书行为）范围外——提示是纯展示层，交互增强 = CRAFT-7 背包交互候选；
② E 面板不加 shaped 提示（2×2 网格摆不下的 pattern 占多数，且 KNOWN
ISSUES 记档的 E 面板 shapeless-only 是既有设计 = "shaped 在网格里摆"）；
③ 提示不随背包内容过滤（全量 33 行 + 可供应着色 = E 面板同款静态列表
范式）；④ 行内 name 溢出截断（见上）= 行宽约束下的最小呈现。

## 实现

- `src/item/recipe.{hpp,cpp}`：`patternBag(const Recipe&)` +
  `canSupply(const Inventory&, const bag&)` 纯函数（campfire 范式）。
- `src/ui/font.{hpp,cpp}`：`fitLabel(name, suffix, scale, availPx)` 纯
  函数（逐码点尾截，后缀恒保）+ CJK 表改显式 (cp, cell) 33 条 + 作。
- `src/ui/inventory_ui.{hpp,cpp}`：`InventoryLayout::compute` 表 kind
  分支 = 两列提示区（`listW` / `leftRows` / 行矩形两列分配）+
  `craftHeader` 两种 kind 都算（E 侧矩形逐式不变）；新常量
  `kRecipeColumnGap`。
- `src/core/game.{hpp,cpp}`：`Game::shapedRecipes()`（注册表序，
  `shapelessRecipes()` 孪生）；`handlePanelClicks` 行检查两种 kind 都
  做（表 kind = 吞点击 no-op / E kind = 原合成分支逐式搬移）；渲染段
  表 kind 分支画 mini pattern + 输出图标 + name（可供应着色 + fitLabel
  截断），E 分支原样；"合成"标题两面板都画。
- `tools/generate_font.py` + `assets/textures/font.png`：作 字形 +
  CJK_CELL 显式映射（既有 32 条 cell 不变 + 作 = 127）。
- 零 format（21）/config/内容/音效。

## 测试

1844/1844（+7 净：`TableHintRowHitTest`【两列命中 + 列间空隙 + 与
slot/grid/output 互斥】/ `PatternBagTest` ×3【每格 1 单位 + ItemID 升
序 + 跨字符合并 + 空格/shapeless 空袋】/ `CanSupplyTest` ×2【跨槽求和
+ 空袋 + 多条目】/ `FitLabelKeepsSuffixAndTrimsName`；`TableKindIs3x3
WithShapedHintList`【两列分配 + 原生 1344 宽 + 行矩形】契约演进重写
`TableKindIs3x3WithoutRecipeList`；字体 3 测契约演进：`CjkTablePinned`
32→33【作 cell 127 钉死 + 既有 32 条 cell <32 唯一】/ `GlyphUvCjkCell
Positions` 加 作 → (232,488) 钉 / `FontPngBytesPinned` FNV 重锚），
`-Werror` 0 警告。E kind 布局测试（`WideWindowKeepsTheNativeSize` 等）
零改动全绿 = E 面板位级免疫。

## 门 137

- **S1 = 第 137 次结构性免疫 + base18 锚刷新（PITS 315）**：fresh 60f
  四方 ×2 **168140 B 全 EXACT**（窗口 = 679×744 半平铺态——用户
  Firefox 窗在场时 WM 的当前平铺；旧锚 1362×744 387726 B 尺寸失配 =
  环境态变更非回归，锚刷新 + 记档；终审二进制复跑再 EXACT）+ savenorm
  同侧 ×2 + 跨侧全 EXACT。结构惰性 = 无面板打开 → 提示列零渲染、
  `shapedRecipes()` 零调用、font 零 CJK 采样。
- **S2 = 工作台场景判别输入跑 PASS**：scene（g104 实证开台几何：表
  (21,39,-4) + 玩家 (21.5,39,-2.5) yaw 0 pitch −0.7 = 准星打表块 +Z
  面【eye 1.62 解析命中 y≈39.78】；slot5 dirt ×1 + 供料物品若干
  【planks 9/stick 8/wood 8/coal 2/cobble 8/iron 3 = 行着色白/暗可辨】；
  nomobs + nodecay config）输入 = 捕获 → 右键开台 → 左点 slot5（携
  dirt）→ 左点提示行 0【NEW = no-op 保 carry / OLD = 同窗位在旧窄面板
  外 = 丢 carry】→ 左点 slot5【NEW = dirt 回槽 / OLD = no-op】→
  **闭合鼠标路径回中心**（两侧 locked delta 望远镜求和 = 0 → 终相机 =
  场景值 = 跨侧世界渲染一致，A/B 像素 diff 才能限定在面板带）。终态
  互斥：**NEW slot5 = dirt ×1 vs OLD slot5 = 空** + 两侧 tables 条目在
  （面板开过、网格 9 槽空）+ 终相机 (0,−0.7) 双侧逐值 = ×2/侧 RAW
  **335034 B（new）/ 280285 B（old）全 EXACT** + savenorm 同侧 =
  **仅 PITS 244/283 类 churn**（NEW 5 格 lava 流前 + 3 air 反应缘 /
  OLD 5 格 lava 流前，@ (81-88,35-36,73-80)，距玩家 ~100 块视图外——
  同侧 RAW 截图 EXACT 实证视图零差）+ **A/B 像素断言 PASS**（diff
  640,430 px **100.00%** 限两面板矩形并集带 + 新面板右半【提示列区】
  diff 255,883 px = 提示内容可见）。**着色目检 16/16 行采样正确**
  （白 = 可供应：bow/bucket/campfire/crafting/fence/iron_axe/iron_
  helmet【shipped 3 铁 pattern】；暗 = 不可供应：diamond ×4/golden_
  apple/iron_block/iron_boots/iron_chestplate/iron_leggings）。S2 首
  轮批（无 firefox 显隐）6/6 捕获失败 = **PITS 314**；终审批黑窗相位
  长（new_a i=5 / new_b i=1 / old_a i=2 / old_b i=3 收 PASS）= 证据门
  + 重试设计自证。
- **S3 = 套件 1844**。
- **无 S6**：零 parse 面（提示 = 既有配方注册表派生，无新数据字段；
  字体表 = 代码常量非数据）。
- **性能 A/B 必做 = PASS**（真 v1.21.0 tm_v121_perf vs v1.22.0-dev
  tm_v122_perf，banner 实证，fall 900f 三窗）：fluidCells
  win2 **29803 = 29803 逐位** + win3 **29800 = 29800 逐位** + win1 ±6
  （fill 窗 real-dt 类）+ PIPE3 **changesApplied 1404 = 1404 逐位** +
  全指标同档 = 成本可忽略（结构惰性：无输入无面板 = 提示零渲染）。终
  窗 fluidCellsSeen 三轮对 43996/43048、43996/43048、43994/43996 =
  **树间 Δ ≤ 树内同二进制跑间 Δ（946）** = PITS 283 类熔岩湖活动相
  摆动（v121 两跑 43996/43048、v122 两跑 43048/43994 = 同树翻转实
  证），非树差。

## PITS

- **PITS 314（unmap_covers 谓词缺口 = 浏览器吞点击）**：`unmap_covers`
  的覆盖谓词要求 `y+h ≥ 770`，用户 Firefox 窗 1362×744+0+20 的
  y+h = 764 不满足 → 预存全屏浏览器窗不被隐藏 → 捕获/输入点击全部落
  入浏览器（游戏日志零 Pointer 行 = 静默失败 6/6，游戏跑满 900f 干净
  退出）→ 输入跑前**显式隐藏预存 covers**（本门 = xdotool search
  --class/--name firefox → windowunmap，跑毕 windowmap 恢复用户窗口）。
- **PITS 315（WM 平铺态环境依赖 = base18 锚尺寸失效）**：用户开/关
  Firefox 后 awesomeWM 平铺变化——S1（firefox 在场）游戏窗 = 679×744
  半平铺（GEN-8 同款），S2（firefox 显隐）= 1362×744 全宽 → 旧锚
  1362×744 尺寸失配。S1 锚字节门 = 先验四方尺寸一致 + 尺寸 = 锚尺寸；
  失配 = 环境态变更 → 锚刷新 + 记档（非回归）。门 137 锚 = 679×744
  168140 B（S1 firefox 在场态）。另收 zsh 假象：`for` 循环内 `local
  ev` + `ev=$(...)` 二次迭代把上一轮旧值以 `ev='...'` 引号形打到
  stdout（证据门 i=5 假象"BAD 却 OK"；证据门逻辑本身正确）→ 循环内
  局部变量一次性声明。
