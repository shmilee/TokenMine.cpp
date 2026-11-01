# Phase 35 细节存档

> 用户确认方向（原话）："scale=2 足够了。请你继续 1. 高分 UI 素材 和
> 2. 面板自适应 UI 空间"——即 P34 报告候选 ①（高分 UI 素材：16×16 字体 +
> 32×32 物品图标）与 ②（面板自适应 UI 空间）。背景：P34 的 `ui.scale` 只
> 做 GPU 放大——8×8 源字形放大后仍是 8×8 的细节上限，面板 1000 UI px 宽在
> `ui.scale=2`（UI 640 宽）下两侧裁切。

## 阶段报告

- **`[2026-10-07, Phase 35]` 高分 UI 素材 + 面板自适应（用户确认）** —
  **1. 字体 8×8 → 16×16**（`src/ui/font.{hpp,cpp}` +
  `tools/generate_font.py` 重写 + `assets/textures/font.png` 重新生成）：
  - atlas 128×256（16px cell）→ **256×512（32px cell、8px 偏移）**；ASCII
    32..126 沿用 `(c % 8, c // 8)` 索引（行索引 4..15，故 16 行）。
  - **字形表来源**：一次性从系统 DejaVu Sans Bold 提取（PIL，em 16px、
    基线 row 12【descender 占 13..15】、alpha > 128 阈值 → 2px 笔画），
    提取结果作为**静态表嵌入生成器**（生成器保持纯 stdlib、确定性输出，
    构建/再跑不需要 TTF；文件头注明提取参数）。手写 95 个 16×16 字形
    首稿 195 行宽度/行数错（生成器断言抓到）、修完长度后大量形状畸形
    （'a'/'b'/'&' 等）——弃用，改提取路线 + 预览 sheet（全字形 + 样例句）
    目检迭代。
  - **`textWidth`/`makeTextVertices` 的 scale 参数 int → double**（≤0 钳
    1.0；分数缩放供面板自适应；字距保持 1×s）——面板等比缩放需要分数
    glyph 尺寸（如 0.638）。
  - 布局常量随 16px 字形：**`kF3LineHeight` 14→18**；死亡屏 died（scale
    2.0 = 32px）y = h/2−40、hint（scale 1.0 = 16px）y = h/2+12（P34 的
    −24/+8 是 8px 字体的值）。
- **2. 物品图标 16×16 → 32×32**（`tools/generate_item_icons.py` 重写 +
  17 个 PNG 重新生成）：`SIZE` 16→32，**形状谓词按 32 坐标系重评估**
  （弧线/椭圆/对角线在 2× 分辨率采样——获得真 2× 边缘细节，非 2×2
  upscale；splitmix64 噪声在 32 坐标下仍确定性、4× 密）；**离散单像素
  特征逐一 ×2**（新增 `setpx2` 助手画 2×2 块：glint/斑点/纤维/骨端
  knob/箭镞/羽冠；线条坐标与宽度 ×2）；羽毛翅膀初稿忘 ×2（±1 太细，
  放大目检后修 ±2）。block 物品图标仍用方块纹理（32×32 方块纹理不在
  本阶段范围）。
- **3. 面板自适应 UI 空间**（`src/ui/inventory_ui.{hpp,cpp}` +
  game.cpp 面板渲染段）：
  - native 设计字体感知化：`kTitleHeight` 30→40（标题 scale 1.5 = 24px，
    偏移 +4）、`kCraftRowHeight` 46→52（name 行 y+8 / needs 行 y+28，
    各 16px）→ native **1000×228**（P34 的 1000×218）；其余常量不变
    （slot 52 / gap 4 / padding 12 / craftWidth 280 / craftGap 12）；
    删除未使用遗留常量 `kSectionGap`。
  - `compute()` 内推导贴合系数
    **`s = min(1, (w−2)/1000, (h−2)/228)`**（double），新增
    `InventoryLayout::scale()` 成员暴露给渲染侧。**长度全程 double
    运算、只在 rect 边界 lround**（`scaledLength` ≥1px 保底；逐段独立
    lround 会让段宽之和超窗——s=0.638 时 2×8+321+8+110+8+179=642>640，
    PITS 坑位）；子 rect 从同一组 double 原点推导，相邻 rect 互相一致，
    hit test（slotAt/gridCellAt/inOutput/inRecipe）用同一 rect 集合自动
    跟随。s=1 时与 P34 布局逐位一致（新 native 常量除外）。
  - 渲染侧跟随 `s`：标题/配方 header 文字 scale `1.5s`（`InventoryLayout::
    kTitleFontScale`，24px native）、配方 name/needs 与 tooltip/关闭提示
    scale `s`（y 偏移 +8s/+28s/+12s/−20s）、槽图标 `L(34)`、配方行图标
    `L(28)`（中心 r.x+14s、文字 x=r.x+32s）、槽计数 scale `s`、carry 图标
    `L(34)`；**hotbar 等 HUD 不随面板缩放**（UI 空间缩放已覆盖高分屏
    需求）。
  - **标题 2s→1.5s（用户反馈微调，提交前完成）**：E2E 截图目检 + 用户
    反馈"ui.scale=2 时面板上的字体很大，物品大小合适"——scale 2 下 2s
    标题 = 40.8 物理 px，达 66px 槽位高的 62%（正文 20.4 px = 31%、图标
    44 px 均合适）；标题缩到 1.5s = 30.6 物理 px（槽位 46%）。只改绘制
    scale：`kTitleHeight` 40 不变 → 面板 1000×228、s=0.638、槽位坐标、
    正文/图标全部不变（E2E 点击坐标无需重锚）。
  - **E2E 坐标**（`InventoryLayout::compute` 直接 dump 核对）：scale 1
    （1280×720，s=1）：panel (140,246,1000,228)、slot 5 中心 (458,436)、
    slot 8 (626,436)；scale 2（UI 640×360，s=0.638）：panel
    (1,107,638,145)（**恰好放下，P34 裁切消除**）、slot 5 UI (203,228)
    → 物理 (406,456)、slot 8 UI (310,228) → 物理 (620,456)。
  **构建/测试**：696/696（+4：`MakeTextVerticesFractionalScale`（scale
  0.5：glyph 8px、advance 8.5）、`TextWidth` 补 0.5/钳位断言、
  `WideWindowKeepsTheNativeSize`（1280×720 → s=1、1000×228 钉值）、
  `NarrowWindowScalesThePanelDown`（640×360 → s=0.638、槽 33、贴合每边
  ≥1px、相邻不重叠、hit 跟随）、`ShortWindowScalesByHeight`（高度约束）；
  test_font 全部锚点重钉 256×512：'Q' cell (1,10) → (40,328)、' ' →
  (8,136)、'~' → (200,488)、`textWidth("AB",2)=34`）；`-Werror` 0 警告。
  实现中 bug：槽位循环 `int slot` 遮蔽 `double slot`（布局全错，
  `slot.w=1`）——钉值单测当场抓住（PITS 坑位）。

## E2E（P25 协议 + xdotool 脚本；`/tmp/opencode/p35_e2e.zsh`）

- **环境事故 1（冒烟期）：缺失 probe config 被游戏自动补成种子
  （`ui.scale: 1`）**——冒烟脚本引用了批处理才生成的
  `config_p35ui2.json`，游戏首启把缺失文件写成默认种子配置（含
  `"ui": {"scale": 1}`），5 次"scale 2"面板运行全部实际按 scale 1 跑
  （点击坐标按 scale 2 布局算，槽 5 偏一格落槽 4）。发现路径：存档结果
  对不上坐标推算 → 布局 dump 程序核对（C++ 布局与 Python 镜像一致）→
  临时 PANELCLICK 日志暴露 `scale=1.00` → 文件内容检查。**对策**（批
  脚本开头固化）：`rm` 旧 probe 文件 → 从 config_xep 重新生成 → 断言
  `ui == {'scale': 2}` 等关键键值。P34 probe 坑（残留错值）的具体化
  （这次是文件不存在被自动补默认）→ PITS。
- **环境事故 2（冒烟期）：指针解锁后 WM 堆叠翻转**——E 开面板释放指针后，
  `xdotool getmouselocation` 的 window 字段间歇切到用户浏览器窗口
  （停在上面的物理光标 + focus-follows-mouse）：观察到 `key E` 被吞
  （面板从未打开，0 次 handlePanelClicks）与点击落错窗。对策：每个输入
  步前 `xdotool windowraise`（纯 WM 层，零 SDL 事件）+ 每次运行后**按
  存档状态验证重试**（槽位结果 + `blocks`/`fluids` 为空，≤3 次）。正式
  批 0 次重试触发。→ PITS。
- **基线门（第 10 次结构性免疫）**：base18 ×2 字节一致、白天 360f ×2
  字节一致且 == p28_daym_a、四拍 skygate 全 FRESH。论证：无输入 →
  指针从不锁定 → hotbar/死亡屏/F3/面板全不绘制，改动的字体/图标资产
  只加载不采样，面板布局代码不在无输入路径上。
- **死亡屏 A/B（void 存档 y=−33、全程无输入，新 16×16 字体）**：dieA1/A2
  = ui.scale 1、dieB1/B2 = 2，各 60f + 截图。断言：存档 health 0 ×4、
  存档对逐字段一致（pos[1] 豁免 ~2e-5，P34 既有约定）、截图 A 对/B 对
  各自逐字节一致；**白字 bbox 与字形表预测逐像素一致**——A (488,320,
  299,67)（died scale 2.0 32px + hint scale 1.0 16px，含 20px 行隙）、
  B (336,280,598,134)，B 宽高恰好 2×A、双中心 640 内（637/635——ink 在
  advance 内不对称 + ×2 拉伸奇偶舍入 2px，确定性由截图对证明）。预测
  方法：从嵌入字形表逐 glyph 算 ink bbox（advance = 17×scale）。
- **F3（输入驱动：中心点击锁定 + F3 切换，无相机输入，180f ×2）**：
  白字墨 10993px（两拍相同，>200 阈值）；**存档字节一致**（确定性
  权威）；首行常数文本 "MineCraft v0.0.1 (Phase 25)" crop (8,8,460,24)
  字节一致（16×16 字体渲染确定性——FPS 行随墙钟变化，不入字节断言）。
- **面板点击（输入驱动、存档为确定性权威，300f ×2/尺度，0 重试）**：
  源存档 slot 5 = stone×1、slot 8 = 空、slot 6 = stick×1（stick 是 flat
  物品 → 验证 32×32 图标）。scale 1 物理 (458,436)/(626,436)、scale 2
  物理 (406,456)/(620,456)。断言 ×4 运行：槽 5 空（取走）、槽 8 =
  stone×1（放入）、槽 6 = stick×1（未动）、selected 0、pos/yaw/pitch
  与源一致（无相机输入）、无块/流体编辑（解锁态误点击会破坏世界——
  哨兵）；A 对/B 对存档与截图各自字节一致；**scale 1 与 scale 2 存档
  跨尺度逐字节相同**（同一逻辑结果，面板几何不同不影响存档）。
- **scale 2 面板贴合 + 32×32 图标（panB1 截图）**：面板物理 x[2,1278)
  全宽在位（(4,300) 与 (1276,300) 暗底像素 (26,25,29)/(32,40,54)——
  kPanel 0.88 半透明叠场景）；slot 6 木棍图标区 stick 色像素
  （140,100,58 ±10）：scale 1 n=180、scale 2 n=289（44 物理 px = 22 UI
  px × 2）。

## 提交

（见 `SESSION_CONTEXT.md` 提交列表；显式路径：font.hpp/cpp、
inventory_ui.hpp/cpp、game.cpp、test_font.cpp、test_inventory_ui.cpp、
tools/generate_font.py、tools/generate_item_icons.py、font.png、
items/*.png ×17、docs/*、AGENTS.md、config/config.json【用户自加
`ui.scale: 2`，已暂存，按规则不还原、显式路径带上】）
