# Phase v3.43.0 — V3-17a 主菜单系统：标题页 + 教学入口 + 字体 +21（门 233）

> 3.x「世界扩展」PRE 表现层项（11-07 用户实机诉求——「demo 要分组展示；
> 游戏打开后有设置/登陆/教学页面」）。**V3-17 相位 A/4**（17a 标题 +
> 教学入口 → 17b 设置 → 17c 登陆 + 教学页两级 → 17d 多人页 + MOTD，
> BACKLOG 定案拆分）。本相 = 菜单架构 + 标题页 + 3 个 stub 页 + 字体
> +21 字形。**表现层零模拟**：菜单期无输入 = 字节可复现（确定性纪律
> 延伸到菜单）。

## 实现

### 字体 +21（人设教学返主距气声缩窗联创继第大维远征历协）
菜单五页 + 5 章标题的字形需求（explorer 侦察逐字核证 vs 433 表全缺）：
- **机制 = 代码点序插入 + 后缀重格**（v3.15.0 于 / v3.14b 九重格先例，
  font.hpp「next addition must grow kFontAtlasHeight, the re-cell shifts
  every later glyph, the top-512 red line holds」的预定形）——**侦察
  「尾部追加」建议否决**：glyphUv = lower_bound 有序表（尾部追加破排序
  不变量须改热路径，重格后缀则全不变量保真且渲染像素恒等）。
- 扩展区 V3-10d 后 528/528 满 → atlas **2112 → 2208（66 → 69 行）**，
  新字形 cell 147..498（散布），其后 400 扩展字形 +1..+21 重格（龙
  527 → 548 = 新末格）；**top-512 红线 131072 px 逐字节恒等**（生成器
  自检 + test_font 区哈希 74b5d1418347dab6 不变）。
- generate_font.py 454 表 + 自更新 CJK_FONT/CJK_CELL 段；font.png
  FNV re-pin 9d0c9e32976679ff；test_font 32/32（锚 10 + 向量 56 +
  计数 + 尺寸 + 新增 3 成员钉 主147/学256/远498）。
- **assetFNV 变**（HELLO 四指纹之一）= 混版 net 互拒（预期，记档——
  v3.14b 字体变先例同款）。

### 菜单架构（第 4 循环）
- **`menuEnabled()`**（game_menu.cpp）= 交互 boot 判：
  `m_demoMode == Off && listenPort == 0 && connectHost 空 && !script &&
  !m_args.newWorld`——demo/net/script 旗运行 + **--new**（显式新档
  启动指令——门的 world 跑与用户的「重新开始」从不坐在菜单点击后）
  **结构性绕开菜单**（canary 4 demo + net 臂 + script 臂 + perf
  --new 臂 = pre-V3-17 位级形，V3-9 三旗 absent 先例的扩展；管理旗
  在 main 即退出，不达 Game）。其余世界塑造旗（--seed/--time/
  --explode/--weather/--dragon-damage）= **与菜单组合**：种子覆写
  在菜单分支前已入 m_config（单人启动的世界消费它）；frame-0 动作
  在游戏相首帧触发。
- **`buildUiAssets()`**（game_init.cpp）= buildScene 头提取的幂等最小
  GL 面：ui shader + font 纹理 + m_uiScale/m_fontScale（原 buildScene
  两处块 + 1851 区 scale 块移入；菜单 init 建一次，单人启动后
  buildScene 重入跳过——GL 资源单次加载）。
- **`runMenuLoop()`**（game_menu.cpp）= 第 4 循环：pump（m_input 独消
  费者）→ 纯状态机 `menuStepTitle/menuStepStub`（menu_layout.hpp，
  测试孪生）→ `renderMenu()` → swap → fps_cap 孪生 pacing。**
  m_frameCount 不推进**（模拟帧数自世界首帧起——单人启动 = 直接
  --new 的位级孪生，click 臂 savenorm 实证）。`--frames` 在菜单 =
  菜单帧计数（S1 锚 f60 = 标题）。
- **`renderMenu()`** = clear（kMenuClear 0.04/0.05/0.08 具名常量——
  昼夜未建）+ UiBatch only（**禁 render/renderUi**——读世界态未建）；
  调色板 + batchFor/addQuad/addText 原语 = renderUi 文件局部 lambda
  的**拷贝**（renderUi 原文零触碰 = 游戏面位级根）。
- **标题页**：logo「TokenMine.cpp」scale 2.0（453 UI px 宽 440——
  2.0 为 453 空间的最大整 scale）+ 版本行「v3.43.0」dim + 5 项
  单人/多人/设置/教学/退出（240×32 按钮 44-pitch，0.40h 起；hover =
  白条 + kHover 文）；**单人 = 直接 fresh world-start**（--new 形：
  config 种子、存档不触碰——存档列表 = 17c 单人页面）；多人/设置 =
  stub「the later face」（17b/17d）；**教学 stub = kDemoCategories
  5 章标题渲染**（两级 demo 列表 + mode 过滤 + 需联机 badge = 17c）。
- 输入面 = 鼠标 click + hover + Enter 激活 + ESC 返回/退出（**零新
  GameKey**——input/net FrameState 面不碰）；窗口 X = quit。
- run() 分支：menu 退出（无世界）= 跳过 save 写（槽不创建——S1 断言）
  + 跳过 loop；单人启动 = m_args.newWorld = true + buildScene +
  runGameplayLoop。

### 测试（+5 = 3010）
test_menu.cpp：TitleGeometryAtGateSpace（453×496 门几何全按钮钉 +
  单人中心 (227,214) = gate xdotool 点击同源公式）/ StubGeometry（返回
  钮 + 章行）/ TitleActions（状态机 11 面）/ StubBack（4 面）/
  TitleLabelsAndChapters（词表钉 + kDemoCategories 单源 5 章）。

## 门内捕获（PITS 425 三坑）
① **beginFrame viewport = DRAWABLE（render() 双坐标系）**：renderMenu
初版用 UI 空间（uiSize 453×496）调 beginFrame（= glViewport）→ 菜单
内容压缩到左上角——纪律 = beginFrame(m_window.width(), height())
【drawable】+ uiProjection(uiSize)【布局空间】（代码评审捕获，截图冒
烟前）。② **菜单 hover = 鼠标位置依赖面**：无输入 f60 锚画面 = 标题
菜单，hover 条随物理鼠标位置画（停在按钮上 = ×2 RAW 锚破）——门
纪律 = S1 两跑各在窗口出现后 xdotool mousemove --window 到 (10,10)
【全按钮外 = no-hover】= base20 锚的 defined mouse face。③ **字体
增字 = 代码点序插入 + 后缀重格（尾部追加否决）**：glyphUv = 有序表
lower_bound——尾部追加破排序不变量（热路径改线性扫）；项目机制
（v3.15.0 于 / v3.14b 九重格先例）= 代码点序插入 + 后缀 +1..+21
重格 + atlas 2112→2208（region V3-10d 起 528/528 满）——top-512 红线
平凡保 + 屏上像素恒等（同 bitmap 新 cell）。**衍生门面**：atlas 长
度变 = 全部字形归一化 v-UV（y/高度）重缩放 → 跨二进制文本抗锯齿边
缘亚 texel 偏移 = canary 跨树 RAW 首次带 92-118 px（0.01%）文本边缘
差（17b 起两树同字体 = 恢复 EXACT）——审计 = 逐 px 文本边缘分类
（diff 邻域含 text-like 灰白像素）100% + 零世界像素 + diffmap 留证。

## 门结论（门 233 全 PASS）

**3010/3010**（+5 菜单族：TitleGeometryAtGateSpace【453×496 门几何全
按钮钉 + 单人中心 (227,214) = gate xdotool 同源公式】/ StubGeometry /
TitleActions 11 面 / StubBack 4 面 / TitleLabelsAndChapters 词表 +
kDemoCategories 单源 5 章；banner re-pin 1）。

**S1 重锚 base20**【f60 无输入 = 标题菜单——21900 B，679×744 锚几何
（config 钉死 PITS 424④ 继承）+ defined mouse face (10,10) no-hover
（×2 各 warp）】：×2 RAW 逐字节恒等【菜单无输入渲染 = 纯函数
（无墙钟入像素：17a 标题全静态——无 blink 无 pulse；fps_cap 只 pacing）
= 论证】+ **槽不创建断言 PASS**【菜单退出 = 世界未建 = save 写跳过，
--save 槽不触碰——新 S1 面】。

**Canary 4×2 新≡旧（tm_v342 v3.42.0）**【demo 绕菜单 = 结构免疫实证】：
树内 RAW r1==r2 EXACT ×4 + savenorm 跨树 EXACT ×4【零世界差】+ 跨树
RAW = **字体 v-UV 重缩放文本边缘白名单**（92-118 px = 0.010-0.013%
@1280×720；diff 行全 UI 文本带【clock/badge/finished 面板/toast】；
逐 px 审计 100% 文本边缘【邻域 text-like 灰白】+ 零世界像素 + 心行/
图标 0 差；diffmap + crop 留证 /tmp/opencode/g233/；17b 起两树同
2208 字体 = 跨树 RAW 恢复 EXACT）。

**Click 臂（单人启动 ≡ 直接 --new = 菜单零模拟贡献最强形）**：xdotool
点击单人中心（UI (227,214) = 窗口 (340,321)，layout 纯函数同源——
test 钉 + gate 同公式）→ 600-f 无输入 → 退出存档 vs 直接 `--new
--frames 600`（--new 绕菜单的门形）→ **savenorm EXACT**（日志
"menu: singleplayer — the fresh world's start"——m_frameCount 自世界
首帧 0 起 = 直接 --new 的位级孪生；菜单相的真实 dt pacing 不入存档）。

**性能 A/B 同档**【旧 = tm_v342 v3.42.0 + g139 双树：OW 900f 无输入
两轮——round1 new upd med 0.361/0.513/0.573 vs old 0.389/0.396/0.349
（new 轮内升 = 机载漂移 loadavg 2.1）+ control 轮 new 0.579/0.611/
0.586 vs old 0.427/0.614/0.605（median 交错入带：win2 恒等 ±0.003，
win1/win3 树间差 < 树内跨轮差）+ render/swap 同档 + **changesApplied
9/3/1 逐位 ×2** + **终档跨树 savenorm EXACT（control 轮）= 结构零成
本**——菜单 = 第 4 循环，world pass 位级不变（buildUiAssets 提取 =
同资源同序零行为差）。
