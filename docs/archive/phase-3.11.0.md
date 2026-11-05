# Phase V3-9 — demo 回放（v3.11.0，门 201 全 PASS，3.x 矩阵收官）

> 设计定案 = `/tmp/opencode/v3110_design.md`（schema / 注入内核 / 双
> Input 路由 / 回放循环 / 编写 / 录制 / 观众 UI / 风险面全在其中）。
> 用户 11-05 BACKLOG 定案条目执行：**双入口共享一个注入内核（与
> E2E 输入跑同款输入路径，游戏逻辑零触碰）+ 只观察交互模型 + 输入
> 路由架构边界（白名单非黑名单）**。

## 实现

### demo 文件格式（V3-7 确定性 gzip 容器 + 严格 schema）
`{ version: TM_VERSION 指纹【不匹配拒播点名】/ world: fresh{seed} |
save{slot}【双键/缺键/槽字符集拒收】/ dts: [double]【负 / > kMaxDt
0.25 拒】/ events: [{frame, type, ...}]【乱序拒 / frame ≥ dts.size()
悬空拒 / 未知 type 带 8 词表点名 / payload 面全严格】}` → gzip 容器
（10B 头全钉 `1f 8b 08 00 00 00 00 00 00 03` + raw deflate + CRC32/
ISIZE；序列化字母键序 + double 最短往返 = 双跑字节恒等）。**零
format**（独立文件族，存档 schema 30 零触碰）。

### 注入内核（src/core/demo.{hpp,cpp} 新）
- **DemoPlayer**：injectForFrame（该帧全部 events → m_input.handle*
  同序 = 与 xdotool 跑的游戏内路径同形——Input 公开事件入口，零映射
  层）/ dtForFrame（demo dts = 唯一时间源）/ rendersPerFrame（0.5→2
  / 1→1 / 2→1）/ nextSpeed（0.5→1→2→0.5 循环 + 梯外前吸）纯函数
- **DemoRecorder**：Input::setEventTap 边界 tap（handle* 先记后注 =
  零副作用单测钉；录制 = 现状 pump + tap，游戏逻辑零触碰）+ 每帧 dt
  追加 O(1) + **quit 帧未模拟尾部排除**（discardPendingFrame——该帧
  pump 已记但 update 未跑，保留则回放注入未参与模拟的输入，偏离 3）
- **编写（author）**：timeline JSON `{world, actions: [{at_frame,
  action: hold|key_down|key_up|mouse_down|mouse_up|mouse_move|wheel,
  ...}]}` → 展开事件流（hold = down@at + up@until；**dts = 1/60 定
  步** = 无墙钟全帧锁定确定性）+ (frame, action, part) 全序手写钉 +
  13 面拒收点名；`--demo-author` = 头less 单跑（游戏不启动，save
  management 先例）

### 双 Input 路由（架构边界 = 白名单非黑名单）
- **回放**：观众 Input 实例 = SDL 队列单消费者（pump 排空 +
  quitRequested 供窗口关闭）；玩家 m_input **零 pump 纯 demo 注入**
  ——改世界操作在观众通道**无消费者** = 结构性零遗漏
- **录制**：现状 pump + handle* 边界 tap（先记后注）
- 普通运行：m_eventTap 恒 null（每事件一个判空分支）+ m_audience-
  Input 零消费 = 位级恒等（S1 锚实证）

### 回放循环（Game::run 拆 runGameplayLoop【旧体逐字】+ runDemo-
Replay【新】）
- dt = dts[f]（唯一时间源）；墙钟只 pacing（int64 µs 累计防漂移；
  慢机掉渲染帧模拟不追 = 记档偏离）；倍率切换 = 观众 wheel =
  demo 帧边界（不切半帧）；**0.5× = render-only 保持**（第二渲染帧
  零 update 零 ++m_frameCount——帧锁定系统不双推）；**--frames 按
  模拟帧计**（任意倍率同 --frames = 同模拟终态 = 门 (e) 臂实证基础）
- 观众白名单消费：ESC = 暂停菜单（**非退出**；暂停/结束共面板，Q
  退出）/ C = 视角循环（观察相机模式）/ mouse + 摇杆 = **转镜头
  orbit（render-only**：syncCamera 偏移角绕眼 re-resolve，零 orbit
  完全跳过；**aimForward()** 普通路径 = m_camera.forward() 原式 /
  回放 = 模式基视角无 orbit——观众转镜头不偷 demo 玩家瞄准，7 处
  世界向射线机械替换）/ 手柄 Start = 暂停 LB/RB = 倍率 左摇杆 =
  数字化步进转镜头（V3-8 已先行 = 立项加，定案）
- 放完 = 结束面板观察态（静止末帧 + 面板）；--frames 超出 demo 长度
  = 放完态渲染后干净退出（headless 不死循环，偏离 6）
- save-spec demo 回放 = **观察红线**：引用槽 init 后 reset 永不回写
  （槽 md5 回放前后逐字节不变实证）；--demo + --new/--seed = 点名拒
  （world spec 是起点权威，偏离 4）

### 观众 UI（回放模式零执行于普通运行 = 结构免疫）
- 角标 "DEMO x<速度>"（右上角时钟**下方** y28——原 y8 与 24h 时钟
  重叠，偏离 2）+ 暂停/结束面板（全幅 0.55 暗帷 + title/name/speed/
  hint 4 行，**自包含独立批**：batchFor 按 color 合并会把 demo 文字
  画进既有同色批 = 帷之下；独立批插入序即绘制序 帷→字→badge，最多
  6 个 UI draw call 可忽略，偏离 1）
- 字符串英文（字体 427 字形 CJK 表无 demo 词汇，偏离 1 设计稿记档）

### 资产
- `assets/demos/example.dmo`（285 B，266 帧 14 事件：W 走 50 帧 +
  LMB 捕获 + 俯角挖掘 ~3 s【2 块 dirt 被挖实证】+ E 背包开关 + C
  三后视角 + 滚轮 zoom）+ `example_timeline.json`（人类可读源）+
  `tools/make_example_demo.py`（确定性生成，重跑字节恒等 md5
  cf809697… 三次实证；手写 10B 钉头 PITS 385 纪律；版本 CMakeLists
  单源读）
- `.gitignore` + data/demos/（录制输出默认 rec_<ts>.dmo，data/saves/
  同款纪律）

## 门 201（结构免疫第 81 次，对 base19；锚 225272 B 免重锚）

- **S1** 三旗 absent 无输入 60f ×2 ≡ base19 逐字节 + savenorm EXACT
  （vs v3.10.0 参照树同跑）+ 白名单 4 键 + banner v3.11.0（**UI
  修复后复跑仍 PASS = 普通运行 demo 批零构造实证**）。
- **S2 六臂**：
  - (a) **shipped 示例回放 ×2**：末帧截图 RAW 恒等 + 终档 savenorm
    EXACT + "Demo replay: example.dmo (266 frames, 14 events, fresh
    seed 123456789)"（定案"首放 ×2 字节一致固化"）
  - (b) **录制→回放闭环**：无输入 300f 录制 → rec.dmo 2810 B（10B
    钉头）→ 回放 ×2 RAW 恒等 + savenorm EXACT（真实游玩捕获可复现；
    **录制须不带 --save**——槽会话 demo spec = save 引用且槽被退出
    写覆写终态 = 闭环起点漂移，PITS 391③）
  - (c) **编写臂**：timeline（W hold 5..70 + E 80/100）→
    timeline.dmo（**author 输出 = 输入基名 + .dmo**，PITS 391④）→
    回放 ×2 EXACT + **玩家位移 4.121 块**（事件流生效 = xdotool
    替代面实证）
  - (d) **观众路由臂**：回放中 xdotool 转镜头（两次 mousemove）→
    终档 player 姿态 ≡ 无转镜头对照**逐位恒等**（orbit = render-
    only 结构边界实证；捕获跳转的 orbit 抖动被姿态断言免疫）
  - (e) **变速臂**（双向）：wheel up/down 各一跑 → 终档 savenorm
    **EXACT vs 无 wheel 参照**（任意倍率同模拟 = --frames 按模拟帧
    计实证）+ 像素 diff **76 px 全在右上角 HUD crop**（badge x1→
    x2/x0.5；捕获前光标预置 = 零 orbit 抖动，PITS 388①族延伸）
  - (f) **结束面板**：--frames 300 > demo 266 → 放完态退出截图
    （headless 双渲染捕获，见 PITS 391②）：面板 patch（y470-500,
    x300-380）mean lum **32.0 < 120**（0.55 暗帷在场）+ ×2 恒等
- **S6 点名 7 面**（scratch .dmo 手术 = 手写 10B 头容器重编码）：
  version 指纹拒播（"recorded with … this is v3.11.0 - incompatible"）
  / 乱序（"is out of order"）/ **未知 type 带 8 词表**（"unknown
  type 'teleport' (key_down/key_up/mouse_down/mouse_up/mouse_move/
  wheel/gamepad_button/gamepad_axis)"）/ 负 dt / 超 kMaxDt / world
  双键（"a fresh world takes 'seed', not 'slot'"）/ gzip 损坏
  （"corrupt gzip body"）——全 rc=1 带路径
- **性能 A/B（用户硬要求）**：旧 = v3.10.0（tm_v500_perf = 68c2bbb +
  g139，banner v3.10.0 实证），新 = v3.11.0（tm_v500_perf_new +
  g139，banner v3.11.0 实证）：主对 900f 无输入（旗 absent）PERF-WIN
  三窗同档（upd med 0.524/0.548/0.430 vs 0.563/0.583/0.589——窗口
  间机器噪声带；render 0.545/0.575/0.463 vs 0.546/0.550/0.564 同
  档）+ **changesApplied 9/1/3 逐位恒等** + **终档 savenorm EXACT**
  （旗 absent 零 format 完全恒等）；**回放自成本**（rec.dmo 300f）：
  upd med 0.230 / render 0.331 / swap 0.261 / fluid 0.028 / stream
  0.007 ms——sub-ms 级零可测增量（注入 + pacing + 观众 pump）。
  **g139 标记镜像**：插桩器锚在 runGameplayLoop，runDemoReplay 新
  循环无标记 = 门侧手工镜像 3 测量点进 perf 树（scratch 不入库，
  PITS 387 族延伸，PITS 391⑤）。

## 门内缺陷与修复（fixer 二次交付）

**demo 观众 UI 不可见**（门 201 首跑捕获，fixer 冒烟声称的像素证据
有误——claims 非 evidence 纪律实证）：根因 = demo UI 块位于主 2D 批
**冲刷循环之后**（addText/addQuad 经 batchFor 追加的顶点永不被
drawUiBatch 消费）+ 定界发现项目截图 = swap 后读 **back buffer =
落后一次渲染帧**（既有全部门"截图 ≡ 第 N-1 帧"语义，跑间恒等从未
暴露）→ 修复 = 自包含独立批（冲刷序 主 2D + 立方体图标之后、tooltip
之前）+ headless 放完退出点 still 帧双渲染（退出帧内容可捕获；交互
运行零影响）+ badge 移时钟下（y8 与 24h 时钟重叠）。修复后 S1 锚
回归 PASS + (f) 面板 patch 32.0 + 角标 mid-demo crop 实证。

## 单测族（2734 → 2777，+43，tests/test_v3110_demo.cpp）

schema 拒收 14（roundtrip ×2 / 缺键逐一 / 意外键 / 未知 type / 乱序
/ 悬空帧 / 负 dt / 超 0.25 / 边界 0·恰 0.25 收 / world 双键 ×2 /
world 缺面 / 槽字符集 a·b / event 面 7）+ 加载级 4（版本拒播 / 缺
文件 / 非 gzip / 容器损坏 3 面）+ gzip 容器恒等 3（头 10B 逐字节 /
roundtrip 字节 / 双跑恒等）+ 注入内核 5（帧分派全序 / 同帧文件序 /
dtForFrame / rendersPerFrame / nextSpeed 循环 + 前吸）+ 录制零副作用
3（tapped/plain 双 Input 同流逐事件快照恒等 / 帧归属 / quit 尾排除）
+ 编写展开 8（keycode 23 名全钉 / button 表 / hold 展开 / hold-to-
end / 全序手写钉 / 拒收 13 面 / 空 actions / 端到端写盘回读 + 二次
生成恒等）+ 观众路由 2（双 Input 结构隔离 / 注入零漏计数）+ 标签纯
函数 3。F3 版本钉 v3.11.0。

## 偏离/记档（8 条）

1. demo UI 字符串英文（字体 CJK 表无 demo 词汇；'×' 缺字形 → 'x'）
   + 独立批不合并（同色批合并 = 帷下文字）
2. 观众 LB/RB 变速 = 手柄连接时真实鼠标点击亦触发（wasPressed 不分
   源；speed 只影响 pacing 不触模拟，注释记档）
3. 录制 quit 帧未模拟尾部事件排除（discardPendingFrame）
4. --demo + --new/--seed 点名拒（world spec 权威）；--demo(fresh) +
   --save 允许（= 退出写入）；save 引用槽永不回写（观察红线 md5
   实证）
5. 示例 demo = 简版（出生区转镜头 + 挖掘 + 背包；合同允许）
6. --frames 超 demo 长度 = 放完态渲染后退出（headless 不死循环）
7. 观众左摇杆转镜头 = 数字化步进 0.03 rad/帧（Input 模拟量私有）
8. 暂停/0.5× 保持/观众转镜头/倍率切换 = 单测钉纯函数面 + **门 201
   S2 (d)(e)(f) 臂 E2E 实证**（fixer 首报"无 E2E 物理输入实证"由
   编排者门内补齐）

## 变更文件（显式路径，16 + config 除外）

核心新 2（demo.{hpp,cpp}）+ 核心改 8（input.{hpp,cpp} tap +
InputEvent / main.cpp 三旗 / game.hpp 成员追加 + 声明 / game_init.
cpp 双循环拆分 + 回放循环 + 接线 + 双渲染 / game_frame.cpp
syncCamera 逐字提取 + orbit 分支 / game_interaction.cpp aimForward
+ 7 处射线 / game_ui.cpp 观众 UI 独立批）+ 资产 2（example.dmo +
example_timeline.json）+ 工具 1（make_example_demo.py）+ 测试 2
（新 43 例 + F3 钉）+ CMakeLists（3.11.0）+ .gitignore（data/
demos/）。

零改动红线核证：三旗 absent = 普通运行位级恒等（S1 ≡ base19 + 2777
全量回归 + input tap 判空分支论证）/ 存档零 format（savenorm EXACT
族）/ config 零改动 / 渲染器零新机制（独立 UI 批 = 既有 drawUiBatch
通道）。
