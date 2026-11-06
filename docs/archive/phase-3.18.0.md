# Phase v3.18.0 — V3-11b 创造物品栏（门 208 全 PASS，2026-11-06）

矩阵 ⬜ 项小→大推进 4/6。V3-11 创造模式 = 2 相位；本版 =
**Phase B**（创造物品栏：全物品搜索/给予面板），V3-11 创造模式
**全项完成**。

## 范围与定案

- **入口 = 创造下 E 面板双 tab**（MC 同款整合，非独立面板）：标题行
  「物品栏/背包」两按钮（激活 = 橙框）；**物品栏 tab = 搜索框 + 6×4
  授予网格 + 双行提示**；**背包 tab = 既有 E 面板内容位级**（+标题行
  tab）。生存 E 面板零变化（门 208 旧≡新二进制字节恒等实证）。
- **搜索 = 前缀匹配**（name 升序 224 物品；空 = 全量；词表
  [a-z0-9_] 小写归一；24 上限）+ 滚轮翻页（1 tick = 1 项，双向夹紧；
  搜索缩表重夹 = filtered() 读路径重夹）。
- **文本输入 = 项目首个打字面**（新基建）：
  - `SDL_StartTextInput` 仅「创造 + Inventory 面板 + 物品栏 tab」期
    开启（openPanel 开 / closePanel 关）——**SDL3 文本输入默认关**
    = 无输入运行零 TEXT_INPUT 事件 = 结构性免疫根。
  - Input 帧态 `frameText` + `frameBackspace`（wheelDelta 孪生，
    frameBegin 重置；kFrameTextMax 256 raw 界）。
  - **不入 tap 词表**（InputEvent 无字符串载体）= demo 录制 schema
    8 型不变、canary 零重 author、录制的 demo 永不打字（记档偏离）。
  - TEXT_EDITING（IME 组合）不处理 = 直打字符走 TEXT_INPUT（记档
    偏离）。
- **授予 = 选中热栏位**（左键满组 / 右键单个）：同物品有隙 = 合并并
  清 damage/enchants（创造授予 = 新栈规则）；异物品 = 替换；满同物
  = no-op；授予播放 pickup 音。MC 光标拖放 = 未做（记档偏离）。
- **搜索焦点数字语义**：物品栏 tab 开时数字键 = 文本入搜索，**不切
  热栏**（MC 搜索焦点同款）——槽位须在开面板前选（门内 xdotool
  实证捕获：`xdotool key 2` 双投递 KEYDOWN+TEXT_INPUT = "app2" 无
  匹配假象，PITS 400③）。
- **demo author 面扩展**：timeline `mouse_move` 可选 `x`/`y` 绝对位
  （**文件 schema 既有** x/y——author 面补齐 = 面板点击坐标；缺省 0
  = 4 canary 位级恒等）+ 键词表 `+g`（V3-11a 的 G 键；Python 镜像
  KEYCODES 同步 = PITS 396① 纪律）。
- **零 format（31 不变）/ 零 config 键 / sim 不 bump**：面板 = 会话
  态（搜索串/滚动/激活 tab 不落盘）；授予经既有 Inventory 公开
  操作（canary 4 demo 回放 EXACT = 实证）。

## 实现（代码 11 文件 + 测试 3）

- `src/core/window.hpp/cpp`：`setTextInputEnabled`（SDL_Start/
  StopTextInput 边界；shutdown 停输入）。
- `src/core/input.hpp/cpp`：frameText/frameBackspace 帧态 +
  handleTextInput（pump 的 TEXT_INPUT 分支）+ handleKey BACKSPACE 旗。
- `src/ui/inventory_ui.hpp/cpp`：`CreativeLayout`（E kind 共享外框 +
  标题行 tab + 搜索框 + 6×4 网格，纯几何）+ `CreativeTabUI`（搜索
  词表/上限/回删 + 过滤排序 + 滚动夹紧 + 授予合并/替换，纯状态机）。
- `src/core/game.hpp`：PanelState + creativeUi/creativeTab（会话态）。
- `src/core/game_ui.cpp`：openPanel/closePanel 文本输入边界 + creative
  默认物品栏 tab + 点击路由（tab 切换/授予/背包直通）+ 渲染（tab
  按钮/搜索框/光标闪烁/网格/提示/tooltip）+ 搜索焦点数字门。
- `src/core/demo.cpp/hpp`：timeline mouse_move 可选 x/y + 键词表 g。
- `tools/make_example_demo.py`：KEYCODES +g（镜像同步）。
- 版本三件套：CMakeLists 3.18.0 + test_debug_overlay F3 钉 3.18.0。
- 测试：`tests/test_creative_ui.cpp`（新文件 ×8：布局共享外框/命中
  路由 + 过滤排序/词表上限/滚动夹紧缩表重夹/授予满组单个合并替换/
  越界 no-op/清耐久附魔）+ `tests/test_input.cpp` ×2（frameText
  累计重置 + frameBackspace 帧态）。

## 门 208（全 PASS）

- **S1 ≡ base19 逐字节 ×2 + savenorm EXACT**（**结构免疫第 88
  次**——文本输入默认关 + 面板未开 = 零像素零键差）。
- **S2 面板臂**（全 headless demo，捕获点击先行 = PITS 400①）：
  - a0（无面板对照）vs a1（物品栏 tab）= 55 360 px（面板在）；
    a1 vs a3（背包 tab）= 941 px + **tab 行 107 px**（激活态）；
    a2（授予后背包 tab）vs a3 = 1 790 px = **slot0 图标 67 px +
    配方列随库存重排**（确定性 ×2 RAW 已证）。
  - **a2 授予臂（fresh + 退出写）**：cell0 点击 → **存档 slot0 =
    allium×64**（name 序第一 = 前缀空全量）+ 背包视图 slot0 渲染 +
    截图/存档 ×2 RAW。
  - **a7 生存 E 像素免疫**：生存场景（format 30）E 面板——**旧
    （v3.17.0）≡ 新（v3.18.0）二进制截图字节恒等**（生存 E 面零
    变化最强形）。
  - 全部臂 截图 ×2 RAW + 存档 ×2 RAW。
- **S2c v3.17.0 补证臂**（门 207 空转断言的真 E2E，PITS 400①②）：
  - **a4b（fresh + 捕获 + G + 低头 + 单帧 LMB）**：退出写档断言
    **creative=True / (0,53,0)=air【即挖】/ entities=0【无掉落】/
    pos (0.5,54,0.5)【飞行不坠】/ 起点 stone 未动**——G 切换/即挖/
    无掉落/飞行四语义首次真 E2E。
  - **a5（生存对照）**：同窗口单帧 LMB → creative=False / (0,53,0)
    未破（1 帧破不动 stone）——即挖 = 创造独有实证。
- **S2d 搜索 xdotool 臂**（项目首个文本输入 E2E ×2）：fresh + 捕获 +
  G + **2（开面板前选槽）** + E + 打字 "app" + cell0 点击 → 退出
  写档 **slot1 = apple×64**（"app" 前缀唯一匹配 = 搜索/文本输入/
  授予全链）+ 退出截图物品栏 tab 活跃（橙框）+ cell0 = 苹果图标
  （194,56,50）；**调试日志实证 frameText 逐字符送达（a/p/p/2）**；
  X grab 屏捕挂起 + xdotool 数字双投递 + windowmove 破 XIM 焦点 =
  门内三捕获（PITS 400③④⑤）。
- **S2b canary 4 demo** ×2 RAW + savenorm EXACT（**sim 不 bump 实
  证**；author 面 x/y/g 扩展 = canary 零重 author 位级恒等实证）。
- **S3 全套 2811/2811**（+10：CreativeLayout ×2 / CreativeTabUI ×6 /
  Input frame ×2），`-Werror` 0 警告。
- **S6**：零新解析面（无 config 键/无 format；demo author 面扩展 =
  既有 schema 词的可选化，4 canary 位级恒等实证）。
- **性能 A/B（用户硬要求）**：旧 = 57c417e v3.17.0（git archive）+
  g139 vs 新 = 工作树 + g139（banner 实证 v3.17.0 / v3.18.0），900f
  无输入独占——PERF-WIN 三窗 upd med 0.635/0.624/0.753 vs 0.712/0.586/0.605 + render 0.665/0.712/0.841 vs 0.714/0.584/0.649 + swap med 15.173/15.049/14.820 vs 14.927/15.240/15.177（新侧 win2 swap max 208 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3/1 逐位恒等** + **终档 savenorm EXACT**（零 format = 最强形——面板/文本输入无输入全冷路径结构性零成本）

## 门内捕获（PITS 400）

1. **demo 无初始指针锁定**（首次 LMB = 捕获帧；locked 门控键须捕获
   先行——门 207 的 G/即挖 demo 断言空转复盘）。
2. **demo save-spec 红线禁 F5**（saveSlot.reset = F5 判空 WARN；结果
   捕获 = fresh spec + --save 退出写）。
3. **xdotool 数字键双投递**（KEYDOWN + TEXT_INPUT——搜索焦点数字语
   义定案的实证来源）。
4. **X 指针抓握阻塞外来 X 客户端**（import 恒挂 EAGAIN——面板视觉 =
   游戏自身退出截图）。
5. **门机实际窗口 679×744 ≠ config 请求 1280×720**（布局坐标按截图
   实测算；臂必须 --config committed 口径 + windowmove 破 XIM 焦点
   = 不移动窗）。

## 记档偏离

- 授予目标 = 选中热栏位（MC 光标拖放未做）。
- 搜索 = 前缀（非子串）；IME 组合不处理。
- 搜索焦点数字语义（tab 开时数字 = 文本）。
- 文本输入不入 demo 词表（录制 schema 不变）。
- 创造物品栏 = 224 全量单列（无 MC 分类 tab）。

## 变更文件（显式路径，25）

src/core/window.hpp src/core/window.cpp src/core/input.hpp
src/core/input.cpp src/core/game.hpp src/core/game_ui.cpp
src/core/demo.hpp src/core/demo.cpp src/ui/inventory_ui.hpp
src/ui/inventory_ui.cpp tools/make_example_demo.py CMakeLists.txt
tests/test_creative_ui.cpp tests/test_input.cpp
tests/test_debug_overlay.cpp AGENTS.md README.md docs/CHANGELOG.md
docs/BACKLOG.md docs/GAMEPLAY.md docs/MECHANICS.md docs/PITS.md
docs/KNOWN_ISSUES.md docs/archive/phase-3.17.0.md
docs/archive/phase-3.18.0.md
