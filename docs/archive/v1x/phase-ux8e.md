# Phase UX-8e — 字体独立缩放（v1.45.0，门 165）

> 消费用户 v1.41.0 实机反馈③「字体大小现阶段可以调大吗？」——
> `ui.scale` 已有但整 UI 等比缩放（字体+图标+面板一起，面板 fit scale
> 反向收缩行文字）。**v1.45.0 = UX-8 体验细化系统第 4 版**（系统收官
> 顺延 v1.46.0，10-31 政策 tag 1:1 对应 release）。

## 定案

`ui.font_scale` = **只缩文本、不缩图标/面板几何**的独立旋钮：

1. **严格读**：`uiFontScaleFromConfig` 纯函数（`ui/ui_scale.{hpp,cpp}`，
   与 `uiScaleFromConfig` 同族同文件）double **(0, `kFontScaleMax` 4.0]**，
   **代码默认 1.0 = 对 v1.44.0 位级恒等**（config.json 零改动，
   render.fog / save.auto_save_seconds 代码默认先例）。纪律 = SAVE-1/UX-1
   同款：absent = 1.0 / 非 number / 非有限 / ≤0 / >4 全点名拒；检查序 =
   非有限 → 范围（double 域）。
2. **应用缝**：全部文本渲染收敛在 `Game::renderUi`（game_ui.cpp）单
   函数——顶部 `const double fs = m_fontScale; auto fScale = [fs](s){return
   s*fs;}`，**每一处文本 scale 参数经 fScale**：addText 19 / textWidth 12
   / fitLabel 3 / appendTooltip 2 处直接 makeTextVertices（经 `ts =
   fScale(s)`，3 调用点零改动）。**一致性红线：每个 textWidth 必须与配对
   addText 用同一有效 scale**——居中公式 `(w − textWidth)/2` 依赖此，
   fs≠1 时文本仍精确居中；fs=1.0 时 `s×1.0` IEEE 精确 = 结构免疫根。
3. **行距随字形**（三处文本堆栈行距 ×fs，lround 取整，fs=1.0 恒等）：
   F3 行步进 `f3Step = lround(kF3LineHeight × fs)` / 教程 tip 行偏移
   `lround((kUiIconPx+kUiStripGap) × fs)` / tooltip lineGap
   `lround(lineGap × fs)`。**图标/槽位/面板几何/心行/护甲行/效果行/热栏
   间距全部零动** = 独立性本体；面板内文本溢出 = 极端 fs 接受（记档
   偏离，MC 同款）。
4. `Game::m_fontScale` init 常量（零 format/零帧状态）。

## 测试

2049/2049（+16）、`-Werror` 0 警告、banner v1.45.0：

- 严格读 13：absent=1.0 / ui 节无键=1.0 / 显式 1 / 2.0 / 0.7 小数 /
  4.0 边界 / 0 拒（点名含 "(0, "）/ −1 拒 / 4.0001 拒 / "fast" 拒 /
  null 拒 / true 拒 / 非有限拒（点名 "finite"）
- textWidth 线性 1 测 3 断言（ASCII + CJK 教程行同款，整数关系位级）
- fScale 位级恒等 1 测（s∈{1.0, 2.0, 0.352, 12.0} EXPECT_DOUBLE_EQ）
- 行距恒等 1 测（lround(18×1.0)=18 / 22 / 16，真实 ui_layout 常量）
- 偏差 ×1：非有限用例用 `Json(infinity)` 非 C++ 字面量（-Woverflow 风险）

## 门 165（编排者执行，old 侧 = v1.44.0 = dbc5160 前 = tm_v145_base）

- **S1 = 第 165 次结构性免疫 + base18 锚连续 168140 B 免重锚**：fresh
  60f 四方 ×2（new×2 + old×2，quiet config）全字节一致 + **跨侧 RAW
  字节一致**（无输入无 F3/面板/锁定 = 零文本绘制，fScale 零执行）+
  跨版 savenorm EXACT。
- **S2a 教程行 A/B PASS**（门 162 tip 场景范式：存档 pose + tutorial
  {id:table,0} = 有 tip 的两行文本；捕获 600f；**config 显式 ui.scale
  1.0** 控变量）：
  - 六跑（new-fs10×2 / new-fs15×2 / old-fs10config×2）×2 RAW 全字节
    一致（205947 B ×4 / 208112 B ×2）
  - **代码级免疫 + forward-compat**：new-fs10 ≡ old-fs10config **RAW
    字节一致**（v1.44.0 忽略未知 font_scale 键 + 默认 1.0 逐像素同）
  - **像素 diff 5210 px bbox [98,615,564,671] = 恰两文本行并集**：tip
    行上移 11px（行距 22→33 = ×1.5 解析值）+ 两行字形 16→24px；居中
    锚 x0 = (679 − textWidth(fs))/2 解析值逐像素吻合（main 行 x0 = 98
    exact）；**世界/天空/热栏/计数/心行零 diff**
  - savenorm：fs10 vs fs15 唯一 diff = 远端熔岩湖 (30-31,35,26) 2 格
    stone churn + fires（**同二进制 fs10×2 自分歧 16 项同区实证 =
    PITS 283 类 real-dt**）；player/tutorial/inventory 段全 IDENTICAL
  - 首轮 6 跑废 = **PITS 353**（quiet 基线 config 自带用户 ui.scale 1.5
    → fs15 跑成 ui1.5×fs1.5 = 2.25× 文本越界，一度误诊居中 bug）
- **S2b F3 盒 A/B PASS**（最大文本面 16 行；单键 F3 无捕获）：×2 RAW
  双侧 EXACT（fps 行本相位同整 = 强于协议要求）+ savenorm EXACT（F3
  零存档态 = 纯渲染实证）+ **diff 43038 px bbox [8,8,678,432] 全限 F3
  文本盒**（fs15 盒 = 16 行 × lround(18×1.5) 27px 步进 vs fs10 18px =
  盒本身放大；盒下世界/HUD 零 diff）
- **S6 = ×3 精确点名 PASS**（"fast" → "must be a number" / 0 → "must be
  in (0, 4.000000], got 0.000000" / 4.5 → "…got 4.500000"）
- **S3** = 2049/2049。
- **性能 A/B v1.44.0 vs v1.45.0（发布前硬门槛）PASS**：fall 900f 串行
  零并行——PERF-WIN 三窗 upd med 0.390/0.386/0.435 vs
  0.388/0.368/0.427、render med 0.424/0.453/0.444 vs
  0.454/0.424/0.442、swap med 15.72/15.71/15.64 vs
  15.69/15.78/15.65 = 全指标同档；PIPE3-S0 changesApplied **9/3/1
  三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 =
  同世界同成本**（无 F3/面板/锁定 = fScale 零执行 = 结构惰性最强形）。

## 门内发现（PITS 353-354）

- **PITS 353（门 config 继承用户 ui 调值 = 复合缩放误诊）**：g150_quiet
  基线 config 自带 `ui.scale 1.5`（用户调值），字体 A/B 的 fs15 跑实际
  = ui 1.5 × fs 1.5 = **2.25× 文本**——tip 行 459→688 逻辑 px 越过
  679 窗 → 双侧越界，首轮 6 跑矩阵全废 + 一度误诊"居中公式 bug"（实际
  缝完全正确）。修 = 文本缩放门 config **显式钉 ui.scale 1.0**（控
  变量纪律：A/B 只允许一个变量）。教训：quiet 基线 config 不是纯净
  对照——凡涉及 ui.scale 交互的门必须重建显式 ui 节。
- **PITS 354（输入跑帧窗 < 脚本序列时长，门 163 同款第 2 次复现）**：
  240f ≈ 3.5s（本相位环境 ~68fps）< 固定 sleep 4+2s + 捕获轮询序列 →
  游戏先于点击帧限退出，CAPTURE-FAIL。协议 = **帧窗 ≥ 序列时长 ×1.5**
  （600f 修）；环境 fps 随时段变（门 162 同场景 240f 够用 = 当时 ~13fps
  流送期），帧窗按当前环境 fps 实测重估。

## 偏离/记档

- 面板内文本溢出 = 极端 fs 接受（fs 4.0 上限 + ui.scale 叠加时行宽可
  越面板；MC 字体缩放同款，无换行/自适应 = 矩阵外）。
- F3 开发者屏同样缩放（文本通道统一，无豁免——开发者工具纪律的文本
  面例外，记档）。
- 文本行距仅三处文本堆栈随 fs（F3/教程/tooltip）；HUD 图标行距不动 =
  文本与图标行重叠只可能发生在极端 fs（记档，MC 同款）。
