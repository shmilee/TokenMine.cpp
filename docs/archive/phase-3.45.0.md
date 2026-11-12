# Phase v3.45.0 — V3-17c 主菜单：登陆页 + 教学两级（门 235）

> V3-17 相位 C/4（17a 标题 v3.43.0 / 17b 设置 v3.44.0 / **17c 登陆 +
> 教学 = 本版** / 17d 多人 + MOTD）。BACKLOG 规格：③ 登陆页（本地名字
> ——net HELLO 名字载体消费，singleplayer 可选）④ 教学页（V3-16 demo
> 族宿主：分类 → demo 两级 + mode 上下文过滤 +「需联机」徽章）。
> **零字体**（词表核证：17c 所需字符全在 457 表——登/陆/请/符/暂/支/
> 演/示 8 缺字靠措辞规避：「名字（可选）」/「需联机」）= 跨树 RAW
> 恒 EXACT 门面延续。

## 实现

### 登陆页（单人启动前的可选名字面）
- 标题→单人 = **Login 页**（17a 的直接启动改经此页——12 BACKLOG
  规格「登陆页（本地名字——singleplayer 可选）」）：名字标题 +
  输入框（300×40 面板）+「（可选）」hint + 开始/返回钮。
- **文本输入生命周期** = m_textInputOn 页入开/页出关（SDL_StartText
  Input 仅登陆期开——创造搜索/聊天同款先例，结构性免疫根：无输入
  面不激活时零键差）。输入走 Input::frameText/frameBackspace（SDL
  文本编辑面，V3-11b 首消费方），**latinOnly 净化 + 12 上限**
  （net HELLO name 载体域——17d 消费）。
- m_menuName 默认 "player"（可选面——直接开始 = 用默认）；**会话
  态零 format**（名字不入存档——solo 显示面，17d net 面才消费）。
- 空名回落 "player"（开始前兜底）。

### 教学页两级（demo 族宿主）
- **tier 1** = kDemoCategories 5 章 +「· N 个」计数（init 缓存
  m_demoIndex 消费——零额外 IO）；**tier 2** = 章内 demo「name ·
  mm:ss」（.dmo 剥除 + **scale 1.0**——长文件名防溢出，门内截图冒
  烟捕获 1.5 溢出）+ coop 行「需联机」badge。
- mm:ss = main.cpp:601-605 **逐字**（total/3600, (total%3600)/60
  非 ceil——--list-demos 单源，test 钉 index 真实值 03:09/04:13/
  03:01）。
- **coop 拒收** = tier2 LaunchDemo 前 mode 检查：需联机消息 + 日志
  点名（net×demo 互斥 main.cpp:382 同款——solo boot 跑不了 coop
  demo）；当前 index coop 章空 = 拒收面由单测 StepActions 覆盖，
  门臂 = 空章列表截图。
- **菜单驱动 demo 启动 = startDemoFromMenu**：main.cpp:628-669
  的 --demo headless parse 复刻【loadDemoFile 严格（永不 force）+
  world spec→args（fresh seed/newWorld / save slot / scene）+
  m_args.demo/demoFile + **applyDemoArgs（init --demo 块提取共用）**
  】→ run() menuExit 2 → buildScene → runDemoReplay = CLI --demo
  的位级孪生（门臂 savenorm 实证）。

### 架构提取
- **applyDemoArgs()** = init 的 --demo 状态块**原文提取**（m_demo
  Mode/Replay + m_demoPlayer emplace + save/scene 存在性拒收）——
  CLI 与菜单双消费方共享单源（init 2810 区）。
- **m_demoIndex** = init 每次 boot 的 loadDemoIndexChecked 结果缓存
  （既有 fail-fast 不动，成员只是携同文档 = 零额外 IO）。
- 标题 item 0 语义变 = 单人→Login（TitleActions test re-pin）。
- render 三新面（login/tutorial）= hover 从 uiMouse() 逐帧派生
  （m_menuHovered 成员**移除**——17c 重构：渲染自读指针，循环与
  渲染零共享帧态 = 门截图确定性简化）。

### 测试（+5 = 3017）
test_menu.cpp：MenuLoginTest Layout（门几何全 rect + hit）/ Step（
start 钮/Enter/ESC/返回/field click no-op 5 面）+ MenuTutorialTest
Layout（rows/back/msg + hit）/ Step（tier1 OpenChapter / tier2
LaunchDemo / Back / air / out-of-range 7 面）/ LengthText（--list-
demos 逐字公式 + index 真实值 3 钉）+ TitleActions re-pin（item 0 →
Login）。

## 门内捕获（PITS 427 四坑）
① **背景构建竞态**（门 235 前置）：版本 bump 全量重编还在后台跑时
误信旧路径 tests 日志（上次运行 3017/3017 假完成信号）→ 中途 g139
插桩改源 → 混 .o 链接失败 + 同 build 目录双 ninja 竞态 + timeout
杀 zsh 留孤儿 ninja。纪律 = 背景构建以新二进制时间戳 + 专属日志为
证，改源前 `pgrep -x ninja` 必须为空。② **tier-2 长文件名溢出**：
初版「guide_materials.dmo · 07:03」@1.5 穿出行 hover 条（截图冒烟
捕获）——修 = .dmo 剥除 + tier-2 scale 1.0（最长 21 字符 @1.0 =
356 px 自 x70 止 426 < 453 页宽）。③ **mm:ss 逐字公式**：初稿
ceil 与 --list-demos 不符（11387f = 03:09 非 03:03）——菜单 =
main.cpp:601-605 单源镜像，test 钉 index 真实值。④ **清默认名计
数**：默认 "player" = 6 字符，初臂 4×BackSpace 剩 "pl" = "plgate"
假异常（两实跑 4 键全消费 = frameBackspace 无同帧吞并，纯计数
错）；日志行 = 双文本通路权威。

## 门结论（门 235 全 PASS）

**3017/3017**（+5 菜单族：MenuLoginTest Layout/Step + MenuTutorial
Test Layout/Step/LengthText + TitleActions re-pin item 0→Login；
banner re-pin 1）。

**S1 重锚 base20 = v3.45.0 标题**【f60 无输入 ×2 RAW 逐字节 + 与
旧锚 diff = 101 px 单行 rows 164-169 = 版本行 v3.44.0→v3.45.0 有界
断言 + defined mouse face (10,10) no-hover + 槽不创建断言 PASS →
base20 = v3.45.0 标题 21910 B】。

**登陆臂（SDL 文本输入首门实证）**：单人 → 6×BackSpace 清默认 →
type "gate" → 开始 → 600-f 无输入：日志 `singleplayer's name
'gate'`【frameText + frameBackspace 双通路权威——文本输入生命周期
（页入开/页出关）活体证明】+ **存档 ≡ 直接 --new 600-f savenorm
EXACT**【名字会话态零 format——不入档，菜单启动 = --new 位级孪生
延续】。

**教学臂（菜单驱动 demo = CLI 位级孪生最强形）**：教学 → day1 →
guide_mining：`Loaded demo scene 'pilot_mine.json.gz'` + `Demo
replay: 'guide_mining.dmo'` 日志面 + **菜单 ×2 savenorm EXACT +
RAW EXACT** + **菜单 ≡ CLI `--demo guide_mining.dmo` savenorm
EXACT**【applyDemoArgs 提取共享单源 = 双启动方零分叉实证】。

**coop 空章臂**：多人协作章（0 demo）tier-2 空列表渲染截图留证
（标题 + 返回钮，零行零崩溃）；coop 拒收面 = 单测 StepActions
覆盖（index 无 coop demo，17d 多人页才见 coop 数据）。

**Canary 4×2 新≡旧（tm_v344 v3.44.0）**【17c 零字体 = 跨树 RAW
恒 EXACT 门面延续】：树内 RAW r1==r2 EXACT ×4 + savenorm 跨树
EXACT ×4 + **跨树 RAW EXACT ×4**（PITS 425④ 不复发，同 2208 字体
FNV 6b4fde4b271b467e）。

**性能 A/B 同档**【旧 = tm_v344 + g139 双树：OW 900f 无输入 new
upd med 0.722/0.661/0.697 vs old 0.740/0.778/0.754（median 交错
入带：win1 old +0.018 / win2 new −0.117 / win3 new −0.057，机载
噪声域——old 轮 render max 14957ms 一次性 OS 停顿）+ render
0.807/0.845/0.853 vs 0.819/0.874/0.888 同档 + swap 14.91/14.94/
14.91 vs 14.90/14.82/14.77 同档 + **changesApplied new 9/3/1 vs
old 9/2/1**【win2 = streaming 边界 real-dt 域，门 207/210/217
先例】+ **终档跨树 savenorm EXACT** = 登陆/教学零世界面结构零成
本】。
