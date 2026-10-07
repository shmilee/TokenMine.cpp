# Phase 34 细节存档

> 用户指定方向（原话）："在我的高分屏上，字体都太小了，而且物品栏、物品的
> 像素都不高，对眼睛不友好"。用户显示 = 2560×1600 eDP，312 mm（192 DPI），
> **dppx = 1（无 HiDPI 缩放）**——问题不是模糊（纹理已 GL_NEAREST，放大锐利），
> 是纯物理像素的 UI（字体 8px、面板格 44px、物品图标 16px）在 QHD 面板上太小。
> 范围（推荐 A，用户未答范围问题直接说"继续"）：UI 坐标空间 + `ui.scale`
> 配置 + HiDPI 修正；高分 UI 素材（16×16 字体/32×32 图标）与世界方块纹理
> 重制留作后续候选。

## 阶段报告

- **`[2026-10-07, Phase 34]` UI 坐标空间 + `ui.scale`（用户指定）** —
  全部屏幕空间 UI 从"物理像素"改为 **UI 像素空间** = 窗口**逻辑**尺寸 ÷
  `ui.scale`，正交投影拉伸到可绘制区（GPU 放大，NEAREST 锐利）。
  **设计要点**：
  1. **`ui.scale`**（新 config 键，`src/ui/ui_scale.{hpp,cpp}` 新模块）：
     缺键 = 1.0（逐位基线）；已存在必须是 JSON 数字 ∈ [1, 4]（
     `kUiScaleMin/kUiScaleMax`）；非数值/越界 = 启动报错且键名入消息
     （P32 `Player::Params::fromConfig` 同款模式——Config getter 对类型
     错静默回退，故走 `at()` + `isNumber()` 读原始值）。`< 1` 会让 UI
     缩到设计尺寸以下；`> 4` 时 UI 空间（窗口 ÷ scale）低于 320 px 最小
     窗且面板裁切（既有窄窗行为），故上界 4。
  2. **`Window::logicalWidth/Height`**（`SDL_GetWindowSize`）：与
     `width()/height()`（`SDL_GetWindowSizeInPixels`，可绘制像素）并列。
     dppx = 1 时两者相等；SDL **鼠标事件坐标是逻辑坐标**——UI 命中必须从
     逻辑尺寸派生（顺带修复潜在 HiDPI bug：旧代码 UI 几何用物理像素、
     鼠标用逻辑坐标，dppx = 2 时面板点击偏移 2×；本环境 dppx = 1 未触发，
     改动在此环境可证明为 no-op）。
  3. **`Game::uiSize()/uiMouse()`**（game.cpp）：`uiSize` = 逻辑尺寸 ÷
     `m_uiScale`（`lround`，下限 1）；`uiMouse` = SDL 逻辑坐标 ÷ scale。
     两处命中点（`handlePanelClicks` 布局+点击、`renderUi` 悬停）改走
     UI 空间；`render()`（viewport/aspect）与 `takeScreenshot` 保持物理
     像素（世界渲染与截图尺寸不受 `ui.scale` 影响）。`InventoryLayout::
     compute` 是纯几何，传入 UI 空间尺寸即整体缩放（含单测 (1280,720,4)
     锚不变）。
  4. **默认种子配置**（`Config::defaultConfigJson`）带出 `"ui": {"scale":
     1}`（P32 player 节先例，可发现性）。
  **范围边界**：面板宽 1000 UI px，窗口 1280×720 + `ui.scale = 2` 时 UI
  空间 640 宽 → 面板两侧裁切（居中，`panel.x = −180` UI）——既有窄窗行为，
  中间槽位可用；不引入自适应面板（后续候选：面板随 UI 空间缩放/滚动）。
  **构建/测试**：692/692（+12：ui.scale 缺省 1.0、显式 1.0、分数 1.5、
  边界 1.0/4.0、0.999/4.001/0/−1/字符串/null/bool 拒绝且键名入消息）；
  `-Werror` 0 警告。

## E2E（P25 协议 + xdotool 脚本；`/tmp/opencode/p34_e2e.zsh`）

- **环境事故与恢复**：用户会话重启后 awesome WM 把新窗口**平铺**成
  1272×1552 tile（`windowmove/windowsize` 被忽略）——P0–P33 的会话窗口是
  浮动 1280×720。恢复 = 用户自己的 awesome 浮动切换绑定
  `Super+ctrl+space`（`windowfocus` + 按键；首按可能因焦点竞态被吞 →
  循环"量宽 ≠1280 重发"上限 3 次，仍不对 FATAL）。纯 WM 层输入：游戏无
  焦点事件处理，零 SDL 事件 → 无输入基线结构性不受影响。
- **基线门**：base18 ×2 字节一致（**第 9 次结构性免疫**：`ui.scale` 缺省
  1.0 + dppx = 1 → UI 空间 = 可绘制像素，逐位不变）；白天 360f ×2 字节
  一致且 == p28_daym_a（场景零扰动）；四拍 skygate 全 FRESH。
- **死亡屏 A/B（void 存档、全程无输入）**：5f 源存档置 pos y=−33
  （< −32 击杀面）→ 加载即死。dieA1/A2 = `ui.scale` 1（缺省）、dieB1/B2 =
  2，各 60f + 截图。断言：存档 health 0 ×4；截图 A 对/B 对各自逐字节
  一致；**白字 bbox 恰好 2×**——died 行（font scale 2）A = (560, 150) →
  B = (480, 300)，hint 行（font scale 1）A = (559, 160) → B = (478, 320)
  （`textWidth = n×8×scale + (n−1)×scale` 含 1px 字距：原点 (1280−160)/2
  = 560 / (640−160)/2 UI×2 = 480；NEAREST 640→1280 恰好 2.0×，无亚像素）；
  中心对齐（A 639 / B 638，±1）。存档对比：四份仅 `player.pos[1]` 差
  ~2.3e-5（加载即死首帧物理用真实 dt 落一点才触发击杀面——"玩家物理 =
  真实 dt"既有约定的自然推论；截图与其余字段全逐字节一致，见 PITS）。
- **面板点击（输入驱动、存档 = 确定性权威）**：源存档 slot 5 = stone×1、
  slot 8 清空；每拍：窗心点击（锁指针+聚焦）→ `E`（开面板，释放指针）→
  点 slot 5（取）→ 点 slot 8（放）→ 面板**保持打开**（末帧截图显示
  结果）。坐标：scale 1（1280×720 布局，panel 1000×218 居中、hotbar 行
  y = 405）slot 5 (458,431) / slot 8 (626,431)；scale 2（UI 640×360 ×2，
  hotbar 行 UI y = 225；面板左侧裁切 → 选两尺度均可见的槽 5/8）
  slot 5 (276,502) / slot 8 (612,502)。断言：四份存档 slot 5 空 / slot 8
  = stone×1 / selected 0 / pos·yaw·pitch 与源完全一致（无相机输入：面板
  打开期间指针释放，`handlePanelClicks` 与 `handleInteraction` 互斥）；
  A 对/B 对存档+截图逐字节一致；**scale 1 与 scale 2 存档跨尺度逐字节
  相同**（同一逻辑结果——两个尺度命中测试语义一致的直接证据）。
- **过程坑**（→ PITS）：probe 配置 `config_p34ui2.json` 残留上一会话的
  `ui.scale: 1`（键对值错）→ 两整轮 panel 运行按 scale 1 渲染、scale 2
  坐标点击落空；批处理必须每批重新生成 probe 配置并核对内容。`textWidth`
  含字距（ink bbox ≠ n×8×scale）。

## 变更文件

- 新：`src/ui/ui_scale.{hpp,cpp}`（`uiScaleFromConfig` + 边界常量）、
  `tests/test_ui_scale.cpp`（12 测）
- 改：`src/core/window.{hpp,cpp}`（`logicalWidth/Height`）、
  `src/core/game.{hpp,cpp}`（`m_uiScale`、`UiSize uiSize()`、
  `uiMouse()`、`renderUi`/`handlePanelClicks` 改 UI 空间、init 加载、
  `<utility>` include）、`src/core/config.cpp`（种子 + `"ui"` 节）、
  `CMakeLists.txt`（+2 源文件）
- 文档：ROADMAP（P34 行+索引）、MECHANICS（UI 坐标空间节）、
  CONTENT_GUIDE（`ui.scale` 表）、KNOWN_ISSUES（字体条目：`ui.scale`
  放大 + 源 8×8 细节上限）、PITS（+4：awesome 平铺、probe 配置核对、
  `textWidth` 字距、加载即死 pos.y 抖动）

## 已知问题 / 后续候选

- 高分 UI 素材：16×16 字形字体 + 32×32 物品图标（纯 UI 资产，基线不受
  影响——无输入运行不绘 UI；需两个生成器 + 全 UI 像素重验证，独立阶段）
- 世界方块纹理 32×32 重制（改世界观感，需重拍基线，不推荐混入 UI 阶段）
- 面板自适应 UI 空间（当前 640 UI 宽下 1000 UI px 面板两侧裁切 = 既有
  窄窗行为）
- 继承自 P33 的候选：死亡屏白字（`ui.frag` `texel.rgb × u_color.rgb`）、
  存档 `diffWorld` 并行化（退出 13.6s）、`fallStartY` 空中加载一行修复
- 本环境 dppx = 1，`SDL_VIDEO_DPPX=2` 探测无效（截图仍 1280×720）→
  dppx ≠ 1 路径无法在本机 E2E 验证，只有"dppx = 1 时 no-op"论证 +
  代码审查（`SDL_GetWindowSize` vs `SDL_GetWindowSizeInPixels` 语义）
