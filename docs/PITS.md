# 坑位合集（都踩过，勿再踩）

> Phase 30 文档整理：从 `SESSION_CONTEXT.md` **原样**移入（未改写）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/*.zsh`）指向当时状态；当前文档布局见
> `README.md`「文档地图」。
> pre-3.x 条目（PITS 1–384）→ `docs/archive/PITS_PRE3X.md`；本文件 = 3.x 时代 PITS 385–399，新条目继续前插于尾部块首。

- **gzip 容器可钉 / zlib 宏缺失 / 双跑 RAW 适用域（PITS 385，门 191
  V3-7 存档 gzip）**——① **zlib 自带 gzip 封装（windowBits 16+15）
  的 MTIME/XFL/OS 字节随 zlib 版本/发行版漂移**：确定性一级纪律要求
  同 payload 逐字节可复现 → 必须**手写容器**（固定 10B 头
  `1f 8b 08 00 00 00 00 00 00 03`【FLG=0/MTIME=0/XFL=0/OS=3】+ raw
  deflate【deflateInit2 windowBits=−15】+ 自算 CRC32/ISIZE 尾），
  且解码侧同 10B 头**逐字节全钉**（系统 gzip CLI 产物的真 MTIME =
  点名拒收，不静默容忍——机器生成文件纪律）。② **本机 zlib 1.3.2
  的 zconf.h 缺 `Z_DEFAULT_MEM_LEVEL` 宏**（上游 zlib 有，发行版
  裁剪；`#include <zlib.h>` 后引用 = 编译错实证）→ memLevel 用命名
  常量 `kSaveGzipMemLevel = 8`（= zlib 文档默认档）钉死，勿依赖该
  宏。③ **存档双跑 RAW 逐字节恒等的 E2E 适用域 = 无 real-dt 字段
  漂移的场景**：demo 区场景的 `fires[*].remaining`（硬规则 8 实火
  real-dt）必漂 = 既有 savenorm 剥离族（PITS 244/P56）——门 S1 口径
  = savenorm EXACT + 头 10B 钉 + 单测容器恒等（`SameInputYields-
  IdenticalOutput`），勿把「双跑 RAW 恒等」断言在有火场景当门标准
  （会假红）。
- **手柄实机验收三坑（PITS 395，V3-8a 11-06，用户山湾克隆
  045e:028e）**——① **SDL3 的 BUTTON/AXIS 事件须显式
  `SDL_OpenGamepad` 后才投递**（SDL 3.4.18 实机实证：PRE-OPEN 零事件 /
  POST-OPEN 全到，两段探针 = /tmp/opencode/gamepad_demo）：
  `GAMEPAD_ADDED` + 名字无 open 也有（"Gamepad connected" 日志正常）=
  **connected ≠ 可用**——门 193 合成事件单测永不暴露此 = "实机验收归
  用户"的结构性缺口。修 = V3-8a（d5a5ea6）：ADDED + open（失败 WARN
  点名、跟踪态保留）/ REMOVED + close + `gamepadOpen()` 诊断 getter +
  1 新测；诊断纪律 = 手柄"连接"日志正常 ≠ 输入通道正常，必验按键事件
  行。② **SDL 打开 evdev 节点失败只静默返 NULL**（首案 "Unable to open
  /dev/input/event15" = 用户不在 input 组）：旧代码不 open 不报错 =
  "连上但无反应"日志零线索——SDL 边界 NULL 返回必点名
  `SDL_GetError()`；排查先跑 `id -nG | grep input` +
  `ls -l /dev/input/by-id/`。③ **克隆件：未 open + 按键 = USB 自断**
  （~6 s 周期、每次全量重枚举 which 递增、dmesg = `USB disconnect` 在
  先 xpad `-19` 在后 = 设备先消失非驱动 reset）——**四象限实证**：未
  open+静置 = 稳（实验 A）/ 未 open+按键 = 周期断（游戏 + demo#1 open
  失败态）/ open+按键 = 5 分钟稳（demo#2）→ open 状态 = 判别变量；
  机制（xpad 输出报告流 × 克隆固件）未实证 = 记档。误诊记录：
  `wine control` 可用 ≠ 游戏可用（Wine 不走 SDL）；A/B 双机混淆（内核
  层检查全在 A 机、现象在 B 机）+ TLP/autosuspend 首嫌被实机数据推翻
  = **硬件问题排查先钉死"现象在哪台机器、哪一层"**。
- **portal 框几何三坑（PITS 386，门 192 V3-1 Phase A 门框）**——①
  **点火瞄准 = 面法向相邻格方向**：框内 air 与 obsidian 环共面（单格
  厚），射线穿过内 air 先命中环块的**面**——命中面法向的相邻格必须是
  框内 air 才点火（aim 环格朝框内的面）；瞄准环格外侧面（法向邻格 =
  框外）= 机制正确地无反应（门内首跑假"失败" = 瞄准点选在外侧面
  （−1,55.5,−2.5）而非内侧面（0,55.5,−1.5）；aim 点 = 环格朝内面的
  面上点，不是内 cell 的"格中心"）。② **站入位置 cell 索引 vs 格中心
  差 0.5**：框平面 z=−2（cell 索引，跨 −2..−1）——玩家脚 z 须 ∈
  (−2,−1)（格中心 −1.5），写 −2.5 = 前格（z=−3）= 与 portal 零重叠
  （无倒计时 + 坠地假"物理异常"；门内首跑实证）。③ **V3-7 起门脚本
  读存档必须过 gzip 魔数**：场景 builder / 终档断言直接 json.load
  落盘档 = UnicodeDecodeError 0x8b（魔数第二字节）——gate_compare.py
  load_save 同款探测（raw[:2] == 1f8b → gzip.decompress）；场景文件
  手写明文仍可（零迁移），但 30f base 档是 gzip。
- **g139 插桩器锚点随调用点形态演进（PITS 387，门 194 V3-1 Phase B）**
  ——Phase B 的 DimCtx 参数化改了 update() 的 drain 调用点（collectLit
  Columns(); → collectLitColumns(overworldCtx()); + nether 分支）→
  `tools/g139_instrument.py` 的 STREAMING 锚点 loudly-fail（PITS 317
  "锚点断裂 = 失败不静默" 纪律按预期触发，非 bug）。修法 = **插桩器
  加双形态锚**（新形态优先匹配，旧形态保留 = 旧侧树可重建），随版提交
  入仓；每个 perf 树用**自己 commit 的插桩器**（git archive 含
  tools/）重建 = PITS 317 纪律的自然推论。FLUID 锚（m_world 调用形）
  在 Phase B 后仍唯一（nether 调用 = *m_nether.world 异文）——改
  调用点形态前先 grep 全部 g139 锚点核一遍。
- **format bump 测试面 4 类降级点 + demo save-spec 结果捕获（PITS 399，
  门 207 v3.17.0 V3-11a 创造基础）**——① **存档 format 跳变 = 测试
  面全家**（首跑全套 61 失败全此族，非实现 bug）：4 类降级点须同
  commit 盘点——(a) **strip 助手本地复制 ×5 文件**（test_save/bed/
  campfire/drown/enchant_save 各持一份 `stripFormat30Keys` = 升
  `stripFormat31Keys` + 新键 strip）；(b) **手动 withoutKey 链**
  （SaveFormat8Test 的 player rebuild 链加 creative 一环）；(c)
  **inline rebuild**（test_durability player30 块 `k != "dimension"`
  加 `&& k != "creative"`）；(d) **gzip identity 测试**（29→30 rebase
  29→31：模拟旧档 strip creative + format 断言 + 往返断言）。盘点
  口诀 = grep tests/ 的 strip 助手 / `"format\": 30` 断言 /
  `asInt(), 30` 断言 / withoutKey player 链。② **demo save-spec 回放
  结果捕获 = demo 内 F5**：world = save slot spec 时 `--save` 须匹配
  槽（rc 2 点名）且退出**永不回写槽**（观察红线）→ 常规退出存档不
  存在；结果捕获 = 时间线显式 F5 键（F5 handler 无条件 writeSave 当
  活跃槽）。fresh world spec（4 canary）无此问题（常规退出写档）。
- **场景手术三坑 + 动画内容的门协议（PITS 398，门 206 v3.16.0
  V3-15 小批）**——① **新种子 123456789 的 fresh 档自带 3 团地下火**
  （(23,35,28)/(24,35,28)/(26,35,27) y35 视角外，worldgen 产物，全
  config 同现）：场景存档继承火 = fire 家族 real-dt（PITS 385）破 ×2
  RAW 域（fires 3↔4 蔓延漂移实证）——**场景手术先去火**：移除
  `blocks` 里的 fire 条目（单元格回落生成值）+ `fires: []`；**勿加
  air 覆盖**（同格双日志 = "duplicate block edits" 拒载）。② **存档
  手术格式**：流体日志 = `[x,y,z,level,name]`（**level 在 name 前**，
  熔岩满级 = 7）进 `fluids` 数组——块数组写流体名 = 静默错位；
  `player.armor` = **命名 dict**（head/chest/legs/feet）非数组
  （写索引键 = "not a known slot" 拒载）；池区地形表面 = 53（种子
  实况）——流体格落 solid = "cell occupied" 拒载（PITS 390 续），须
  先挖 air 再填流体 + 石环保险。③ **动画内容的门协议**：退出渲染帧
  数 off-by-one 抖动（PITS 391② 族）× 高频纹理（portal 漩涡）= 截图
  ×2 RAW 差 ~1300 px 全在动画 tile 区（相位差 1/120 tile）——**动画
  内容（UV 滚动/粒子）截图 ×2 恒等不是有效断言**；权威三件套 = 存档
  ×2 RAW（模拟确定性）+ **动画关控制臂 ×2 RAW**（管线确定性，
  `portal.anim/steam: false` 同场景复跑恒等实证）+ 单跑动画目检 /
  双帧 diff > 0（f400 vs f460 = 3560 px）。另：`MC_LOG_*` 宏 = 单参
  消息串（无 printf 格式）——插桩日志用 `+ std::to_string()` 拼接。
- **fScale 不含 ui.scale + 字体加字工作流（PITS 397，门 205 v3.15.0
  死亡统计屏）**——① **`fScale(s) = s × m_fontScale`（仅 ui.font_scale，
  不含 ui.scale）**：makeTextVertices 的字形物理尺寸 = 16px × ui.scale
  × fScale 参数，但**行距/坐标是调用方给的物理 px**——按「fScale(1.0)
  = 一行物理高度」算布局 = 低估 1 个 ui.scale 倍（死亡屏 8 行首版全叠
  在一行，门内 ×2 RAW 恒等 + 裁剪目检捕获）；文本行布局用**固定物理
  偏移**（demo 面板先例 h/2±N，行距 ≥ 26px 覆盖 ui.scale 2 的 32px
  字形 cap 高）。② **加 CJK 字工作流**：字形表查缺（418→428 字表在
  `tools/generate_font.py` CJK_CHARSET，代码点排序）→ charset +1 +
  CHARSET_COUNT +1 → 重跑生成器（TTF pin + legacy 33 位级自检 +
  top-512 红线 + 扩展区**重排格**：新字之后全部 +1 格）→ snippet 手贴
  `src/ui/font.hpp` kCjkGlyphs（tools 永不碰 src/）→ **test_font 4 锚
  重钉**（PngBytes FNV / TablePinned 尺寸+成员格 / ExtendedCellsPinned
  全向量 / UvCellPositions）——ExtendedCellsPinned 的 vector **末元素
  无尾逗号**（`{0x732A, 384}};`）= 正则重钉必漏，单独查。③ 扩展区重
  排格**零像素面**论证 = 无输入渲染无 CJK 文本（教程 = locked pointer
  才显）→ S1 锚恒等（门 205 实证）。
- **demo sim 镜像常量漏 bump + GameKey 尾钉 kKeyCount（PITS 396，
  门 204 v3.14.0 V3-8b 手柄映射）**——① **`tools/make_example_demo.py`
  的 `SIM_VERSION` = kDemoSimVersion 的 Python 镜像**：C++ bump
  （1→2）后忘记同步 Python 常量 → 重新 author 的 example.dmo 静默
  写成 sim 1（recorded_with 却是新版 = 自相矛盾的资产）——门 S2 首跑
  即被 sim 门点名拒收**捕获**（正是 sim 门的设计目的 = 金丝雀先于
  用户发现）；教训 = **bump kDemoSimVersion 时 grep 全仓镜像**（C++
  常量 + Python 工具 + 文档计数）。② **`kKeyCount = GameKey::T + 1`
  = 尾成员钉**：GameKey 加成员（FlyToggle）须同步 kKeyCount——漏更
  新 = 新键索引越出 `std::array<..., kKeyCount>` 边界（-Werror 零
  报警，静默 OOB）；enum 尾部「Keep in sync with kKeyCount」注释是
  唯一守卫，加成员先 grep kKeyCount。
- **S1 锚假失败 = 用户 config 调值 + 时钟映射误读（PITS 394，门 203
  v3.13.0 demo sim 版本）**——base19 锚 = **committed config** 捕获
  （ui.scale 1.5）；用户其后把工作树 config.json 调成 ui.scale 2.0
  （规则：永不提交/还原）→ 无输入截图时钟/XP 文本 4/3× 放大 + 位移 →
  S1 ≠ 锚**假失败**（差 3846 px 全在 UI 文本带：右上时钟 + 底部 XP
  「0」两带，世界/HUD 图标零 diff）。判别三件套：① 像素 diff 仅 UI
  文本带 ② glyph 尺寸比 = ui.scale 比精确（152/114 = 2.0/1.5）③
  `git show HEAD:config/config.json` vs 工作树 diff = 恰 ui.scale 单键
  → **S1 锚比较失败先 diff config**（committed vs 工作树），有 UI 几何
  调值 = 用 committed config 副本 + `--config` 重跑 S1（锚语义 =
  committed config 态）。伴生误读教训：`clockString` = dawn 对齐映射
  （hour24 = (t−kDawnStart 0.05)×24+6）——start_time 0.25 的表读
  **10:48 非直觉的 6:00**（day-plateau 天空 ≠ 正午表读；noon = t
  0.30），时间断言先套公式推（`day_cycle.hpp` 默认注释「// noon」已
  随本版更正）。
- **demo 编写（--demo-author）几何三坑（PITS 393，11-06 预设教程
  demo）**——① **初始俯仰 = −0.25 rad 非 0**（spawn 默认 pitch）：
  瞄准像素换算从 0 推 = 放置/挖掘全偏（实证：dy 158 以为 −0.395 实为
  −0.645，4 个放置点 3 个偏 1 格 + 1 个侧脸命中）；校准 = **单点微
  demo 实证**（锁指针 + 单发 mouse_move + RMB 放置 → 读存档实际落
  块，反推有效像素窗——环 1 格地面放置窗 dy ∈ [330,508]@sens 0.0025
  不含偏移 / 含偏移 [145,315]）。② **按住 Space = 第三次 tap**：
  双击窗 0.3 s（18 帧）内的任何 space key_down 都参与 flyToggle
  判定——双击起飞后 <18 帧内再按（升空 hold）= 立刻切回飞行关
  （实证：f130/f142 起飞 + f155 hold = 13 帧飞行后走路；修法 = hold
  起始 ≥ 第二 tap + 20 帧）。③ **demo 版本指纹 = TM_VERSION**：
  升版后旧 .dmo 拒播（「recorded with … incompatible」点名，设计
  纪律）——**提交仓的 .dmo 须随版重新 author**（--demo-author 逐字节
  可复现 = md5 恒等实证，时间线 JSON 即人类可读源）。另：探针算
  角度的 double-normalize bug（归一化后向量再除 len = 角度系统性
  偏浅）——**角度输出前核一遍归一化**，或直接用 raycast 命中格
  反推（probe4 范式：扫 dy 窗报命中，不手算）。
- **门日志 grep 假阴性（PITS 392，门 202 阴影缩尺报障）**——
  `mob spawn f...` / 多数子系统事件行 = **MC_LOG_DEBUG**：info 级
  门跑里 grep 事件计数恒 0 ≠ 事件未发生（首跑 "0 spawns" 误判，
  crop 目检实证 3 只羊在场）——事件计数断言的门跑加
  `--log-level debug`，或截图 crop 目检互补（日志计数与像素证据
  二选一须在场，勿以 info 级零计数当"未发生"证据）。
- **demo 门脚本六坑（PITS 391，门 201 V3-9 demo 回放）**——①
  **UI 批绘制序**：renderUi 的 addText/addQuad 经 batchFor 追加进
  `batches`，但主批冲刷循环在调用点之前已遍历完毕 = 顶点永不被
  drawUiBatch 消费（demo 角标/面板/暗帷全不可见；冒烟"面板像素"
  声称与门 E2E 矛盾 = claims 非 evidence 纪律实证）——修复 =
  自包含独立批（局部 vector 块内自冲刷）或移到冲刷循环之前。
  ② **截图 = swap 后读 back buffer = 落后一次渲染帧**（既有全部门
  语义，跑间恒等从未暴露）——headless 放完态退出（--frames 超 demo
  长度）的退出帧内容（帷/面板）需 still 帧**双渲染**才可被截图
  捕获（交互运行零影响；中段截图 = 第 N-1 帧内容）。③ **录制带
  --save = 闭环起点漂移**：槽会话 demo world spec = save 引用，而
  槽在录制退出时被终态覆写——录制→回放闭环须 **fresh 会话**（无
  --save，spec = fresh seed）。④ **author 输出名 = 输入基名 +
  .dmo**（timeline.json → timeline.dmo，非 --demo-record 的
  显式/时间戳名）。⑤ **g139 插桩标记锚 gameplay 循环**：V3-9 把
  run() 拆 runGameplayLoop + runDemoReplay，回放循环无标记 = demo
  自成本 PERF-WIN 零输出——perf 树手工镜像 3 测量点（scratch 不入
  库，PITS 387 族延伸；改循环结构前先 grep g139 锚）。⑥ **暂停态
  + --frames = 设计不退出**：headless 退出仅 `m_demoFrame ≥ total`
  触发——门脚本暂停探针（ESC 后不再恢复）会挂到 timeout；暂停帷
  像素证据 = 结束面板共路径覆盖（m_demoPaused 与 endPanel 共用
  demoQuad/demoText 块）。
- **场景手术几何三坑（PITS 390，门 199 V3-5 建筑内容）**——①
  **场景室坐标须查参考地形**：demo 区表面 y=53（seed 123456789
  发行值）——首跑水室放 y=50/51 = 参考石内，流体编辑 load 拒
  （save.cpp "cannot apply save fluid edit ... (cell occupied or
  level unchanged)"，窗口即毁 = 输入臂 BadWindow 假象）——场景室放
  y≥55 参考 air 区（块编辑覆盖任何参考均可；**流体编辑拒 solid
  参考**）。② **开放水源必漫溢**：源水（level 8）每 tick 扩同层邻
  air——无围堰"池"32 格 → 300f 后 402 格流体编辑（×2 恒等但断言
  计数全错 + 视觉淹没场景）——密封池 = 满底 + 顶周围墙环
  （24+24 格实证 32 恒定 60f+）。③ **发货世界自带确定性 fluid-
  settle 编辑**：程序湖边缘非源水（level 6-7）在 base 30f 跑中结算
  入档（seed 123456789 实证 x25-30, y34, z27-28 共 9 格，距 spawn
  ~30 块）——场景终档断言须**自格 scoped**（池格坐标精确匹配），非
  水格全量计数。
- **门脚本生成臂五坑（PITS 389，门 198 V3-3 下界 mob）**——① **门
  QUIET 配置 mobs.enabled = false 是静默纪律**：锚/无输入基线跑用
  g179_quiet.json（mob 生成全禁 = 基线干净），**生成臂须 mobs-on 变
  体配置**（g198_mobs.json = quiet + mobs.enabled true）——首跑零
  spawn 且无尝试日志 = 查配置先于查代码（插桩实证 enabled=0）。②
  **sed 派生门脚本的 OUT 目录要换**：g198_s1 首跑 OUT 仍指 s197/
  = PNG 落别处，`cmp -s 缺失文件` 静默假 DIFF（"S1 ANCHOR DIFF" =
  文件不存在）——cmp 前先 `ls` 验文件。③ **spawn 行坐标正则须含负
  号**：`at (x, y, z)` 的 z/x 可负（`[0-9.-]*`），`[0-9.]*` 漏全部
  负坐标行 = "NO SPAWNS" 假红（实际 4 只已生成）；另 "Registered
  mob" init 行 ≠ spawn 行（池隔离断言须锚 "mob spawn f"）。④
  **maxMobs cap 吞后续尝试**：4 只存活后每次尝试 "cap reached"（mob
  不跑 40 块不 despawn）——低权重 mob（strider 1/31）只在首批 ~4
  次抽选中有机会（8×600f 零命中 = 0.31 幂次运气内）；**决定性
  构造 = 环全清**（4704 air 编辑清掉生成环台地，留玩家立柱）→
  陆地 pick 全 "no valid cell" → cap 永不饱和 → 每次尝试新抽
  （1200f 场景 P≈71%，3 场景 99.9%）。⑤ **传送落点 = 生成压制
  位**：portal 光 11 场 + 下界原点 12 保护圈（hostile）= 落点周边
  环全拒（尝试全 "no valid cell"，机制正确非 bug）——下界生成臂用
  远离落点的高原场景（(13.5, 53, 0.5) 级）。air 编辑合法
  （落盘档 blocks 段 'air' 名 = 显式清空，实证 63-4704 格保留）。
- **输入门脚本四坑（PITS 388，门 197 Part 2 下界深化）**——①
  **timeout 包装使 `$!` = timeout 的 pid ≠ 游戏 pid**：`xdotool
  search --pid` 按 _NET_WM_PID 匹配 = 全败（FOCUS-FAIL / 窗搜空）——
  输入臂的游戏**直跑**（`--frames` 自限，无需 timeout 守护），
  `tp=$!` 才是游戏 pid。② **短 hold 的按键注册抖动**：1.0 s W-hold
  的灵魂沙步行比值实测 0.823（非 0.4）——排查终档坐标/平台几何全对
  后 = 输入时序伪影（XTEST keydown 相对窗口就绪的注册延迟 ±0.5 s
  量级）；3.0 s hold 稀释后比值 0.403 = 物理真值（单测 0.4±0.005
  同款）。**比值类输入断言 hold ≥3 s**（抖动/hold ≤ ~15%）。③ **水
  放置守卫拒玩家重叠格**：yaw 0 射线 x 恒等 = 命中格 x 恒 = 玩家格
  x → 顶面落位 = 玩家自身 feet 格 = 放置被拒（桶保留假"无反应"）——
  aim 几何须落**非玩家格**（双行平台 + pitch −1.0 让落位在前方第二
  行，验算 z = 0.5 − 1.62/tan(pitch) 落格 + 玩家 AABB 不重叠）。
  下界蒸发臂无此坑（T1 谓词在目标解析前 = aim 无关）。④ **性能 A/B
  不得与流送/输入负载臂并行**：同机并行 1800f 下界流送跑使 300f 窗
  upd med 0.444→0.806（+80%）——changesApplied/savenorm 不受影响但
  时序窗失真，独占复跑才入档。
