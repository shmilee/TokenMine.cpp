# 版本历史（CHANGELOG）

> 本文件只留 **3.x 时代**（「世界扩展」期，v3.0.0 起）逐版明细。
> 2.x 全史（v2.19.0→v2.0.0，两批 20 版）→ `docs/archive/v2x/
> CHANGELOG.md`；v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x → `docs/archive/v0x/CHANGELOG.md`。逐版细节（机制/证据/偏离/
> 性能 A/B）见对应时代 `docs/archive/*/phase-*.md` 阶段存档。
> README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0.0 首个正式版 /
> v2.0.0 1.x 收官 / v3.0.0 3.x 开启）。
> 版本命名注：3.x「世界扩展」期 11-04 曾命名 4.x（2.x 第二批收官日
> 晋升定案），11-04 同日用户重编号定案改 3.x（2.x 已吸收原 3.x「系
> 统扩展」八项，回归自然序号）——任务号 V3-1~V3-7。

## v3.14.0（2026-11-06）— V3-8b 手柄按键映射定案（L3 蹲伏 / RT 疾跑 / B 飞行专用钮 + 首个 sim bump 1→2，门 204 全 PASS）

**用户 11-06 报障后续定案**（V3-8a 连接修复后）：「按键习惯问题需
进一步确认（可能有多套常见映射表?）」→ 调研 + 三项习惯定案。**调研
结论**：SDL3 GameController 按手柄数据库把 **Xbox / PlayStation /
Switch 三大布局归一化到同一逻辑位**（PS 的 ✕ = SOUTH = Xbox A 同角
色）= **现行单表已结构性覆盖三大布局，无需多表**（山湾克隆 = 数据库
已识别，V3-8a 实机 open 铁证）。**用户三项定案**（config 全自定义 =
定案**不做**）：① **L3 = 蹲伏**（补键盘 LeftControl 的手柄缺口，
MC Java 惯例；`SDL_GAMEPAD_BUTTON_LEFT_STICK` → 虚拟 Ctrl 既有源位
掩码模式）② **RT 扳机 = 疾跑**（`RIGHT_TRIGGER` 轴 0.2 死区数字化
按住 = Shift，左摇杆移动同款；B 位解放）③ **B = 飞行 toggle 专用
钮**（新 `GameKey::FlyToggle` **仅手柄键** = `mapKey` 永不返回【黑盒
256 键扫描单测钉】，单击直接 `setFlying(!)` 无双击窗；键盘双击 Space
不变，A 双击仍生效 = MC 同款冗余记偏离）。LT / R3 / GUIDE / Dpad
上下左保持未映射（记档）。

**首个实际 sim bump（1 → 2）**（v3.13.0 定案的 bump 纪律「输入路由」
条款首实例）：录制的 gamepad 事件按**当前解释**重放——sim-1 demo
携 L3/RT/B 事件将静默变行为 = 必须 bump。`tools/make_example_demo.py`
的 Python 镜像常量 `SIM_VERSION` 漏 bump = **门 204 S2 首跑捕获**
（example.dmo 静默写成 sim 1 + recorded_with 3.14.0 自相矛盾 → sim
门点名拒收，金丝雀先于用户发现，PITS 396①）。4 个 canary demo 重新
author（sim 2，字节可复现）。

**门 204**：结构免疫第 84 次【S1 无输入 60f ×2 ≡ base19（committed
config ui.scale 1.5 口径）逐字节 + savenorm EXACT——手柄映射 = 无手柄
冷路径 + GameKey 尾扩零像素零存档面】；S2 sim-2 canary 4 demo ×2 RAW
+ savenorm EXACT；**S2b 首个 bump 实证**【git 0705139 的 sim-1 canary
= rc 1 点名「demo sim 1 != this build sim 2 (recorded with 3.13.0)」
+ `--demo-force` rc 0 WARN + **force 回放终档 savenorm ≡ sim-2 canary
= 纯键鼠录制行为恒等最强形**】；S3 全套 **2787/2787**（+3：
RTTriggerSprintDigitize【0.2 死区边界 6553/6554 同款钉 + LT 恒零】/
L3CrouchHold / FlyToggleKeyIsGamepadOnly【256 键黑盒无孪生 + B 边
缘精确】+ TriggersUnmapped re-pin 为 LeftTriggerUnmapped）；性能 A/B
同档【旧 = d9c8c0a v3.13.0 + g139 vs 新 + g139，900f 无输入：PERF-WIN
upd 0.665/0.647/0.705 vs 0.671/0.628/0.693 + render/swap 三窗同档（旧
侧 win1 swap max 15139 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·
spread·ents·stream 同档 + changesApplied 9/2/1 vs 9/3/1【win2 差 =
real-dt 追赶抖动 PITS 385 域】+ **终档 savenorm EXACT**】。PITS 396
（Python 镜像常量漏 bump 门内捕获 + `kKeyCount` 尾钉 = GameKey 加成员
须同步【漏 = 新键索引越出 array 边界 -Werror 零报警静默 OOB】）。
**实机验收归用户**（门强度记档同款 V3-8：本环境全合成事件单测，山湾
克隆实机手感 = 你终审）。详见 `docs/archive/phase-3.14.0.md`。

## v3.13.0（2026-11-06）— demo 回放独立 sim 版本（旧 .dmo 零迁移直放，门 203 全 PASS）

**用户报障**：`--demo assets/demos/example.dmo` 被拒「demo recorded
with v3.11.0, this is v3.12.0 - incompatible」——旧设计把**构建版本串
当模拟语义的代理**，v3.12.0 这种纯渲染常量版也误杀全部已录 demo。**
用户定案设计**：demo 有独立版本号，游戏版本变化时按兼容性判断。

**实现**：① `kDemoSimVersion = 1`（`src/core/demo.hpp` 新常量）= 回放
准入**唯一判定轴**，独立于 TM_VERSION；**bump 纪律**：仅帧锁定模拟
语义变更（update 循环/物理/AI/流体/生成/输入路由/demo pacing）才 +1
——纯渲染/UI/音频/内容数据/文档版本**不 bump**（v3.12.0 阴影缩尺 =
不 bump 实例）。② `recorded_with`（录制时 TM_VERSION 串）**仅信息展
示**（回放日志行 + 结束面板新「recorded with」行，4 行 → 5 行布局）
永不拒播。③ **legacy 映射零迁移**：旧 schema（仅 `version` 键）→
sim := 1 + recorded_with := version（旧严格门已证 record-build ==
replay-build = sim 同期系，用户手头全部 .dmo 继续可放）；新 schema =
{sim int ≥1, recorded_with 非空串, world, dts, events}；`version` 与
sim/recorded_with 同现 = 混用 schema 点名拒。④ sim 不匹配 = rc 1 点
名「demo sim X != this build sim Y (recorded with Z) - the simulation
semantics differ」+ `--demo-force` 逃生门（仅 bypass sim 门，schema
严格性保留，WARN 点名；无 `--demo` 带 force = rc 2 点名）。⑤ **canary
纪律**（PROTOCOL.md E2E 新条）：4 个 shipped demo = 模拟语义金丝雀，
每版门内回放 savenorm EXACT——核心改动破坏 canary = 必须 bump + 重新
author（时间线 JSON 字节可复现）。

**门 203**：结构免疫第 83 次【S1 无输入 60f ×2 ≡ base19（committed
config ui.scale 1.5 口径）逐字节 + savenorm EXACT；用户工作树
ui.scale 2.0 口径 ×2 自洽——S1 首跑假失败 = 用户 config 调值（锚在
1.5 时代捕获，时钟/XP 文本 4/3× = 2.0/1.5 精确比），PITS 394】；S2
四 demo（含重新 author 的现行 schema 294 B example）回放 ×2 RAW 恒等
+ savenorm EXACT；**S2b 用户报障场景实证**【git 4531c42 旧 example.
dmo（legacy `version` 3.11.0）回放成功 + 终档 savenorm ≡ 现行 schema
回放 = 映射行为恒等】；S2c sim-2 拒 rc 1 点名 + force 旁路 rc 0 WARN；
S6 force 无 --demo rc 2 点名；性能 A/B 同档【旧 = 648b80d v3.12.0 +
g139 vs 新 = v3.13.0 + g139，900f 无输入：PERF-WIN upd/render/swap 三
窗同档（新侧 win1 swap max 14955 ms = 一次性 OS 停顿 med 正常）+
fluid·fires·spread·ents·stream 同档 + **终档 savenorm EXACT**（changes-
Applied 9/2/1 vs 9/3/1 窗口差 = real-dt 追赶抖动 PITS 385 域，savenorm
权威恒等）】。2784/2784（2777 基线 + V3-8a 手柄 open 修复 1【用户
11-06 并行提交 d5a5ea6，本提交前已入树】+ 本版 +6：SimPinnedToOne /
ParseLegacyMapping / ParseCurrentSchema / ParseRejectMixedSchemas /
ParseRejectCurrentSchemaFieldShapes / LoadLegacyFileAccepted +
LoadVersionMismatch 重钉为 LoadSimMismatch）。PITS 394（S1 锚假失败 = 用户 config 调值 + 时钟
dawn 对齐映射误读 start 0.25 = 10:48 非 6:00；`day_cycle.hpp`
startTimeOfDay 默认注释「// noon」随版更正）。4 个 demo 资产重新
author（现行 schema，字节可复现 md5 恒等）。详见
`docs/archive/phase-3.13.0.md`。

## v3.12.0（2026-11-06）— 实机报障：mob 阴影缩尺（1.5× → 1.0×，物理不动，门 202 全 PASS）

**用户实机报障**：mob 阴影方块比 mob 大，落体瞬间阴影角还压在高处
方块上 = 看似"根据阴影判定落地"。**核实定案**（一次性探针后撤，
PITS 383 纪律）：物理 = **AABB 质心规则**（探针实测失接地 = 身体
中心恰好越过边缘 0.3000 = 身体半宽，**Minecraft 同款**——MC 亦
允许半身悬空，质心过边即落）；`blobShadow*` 仅渲染消费（代码核
证），阴影零参与判定。所见 = 1.5× 阴影（半宽 0.45）> 身体半宽
0.3 的落体瞬间视觉状态。**用户定案处置 = 缩小阴影视觉**：
`kMobShadowSideScale 1.5f → 1.0f`（= 身体同宽足迹，消除压边误导；
玩家第三人称阴影同常量一并收拢）。1 常量 + 3 测试重钉 + 版本三件
套；2777/2777 不变。门 202 = **结构免疫第 82 次免重锚（对
base19）**【S1 无输入 ≡ base19 逐字节——锚无 mob 无玩家阴影 = 阴
影从不渲染】+ S2 阴影视觉臂【mobs-on 场景 ×2 RAW 恒等 + **前后
对比 2896 px = 3 簇地面阴影带，单调变亮审计 0 违例（阴影只缩
小）+ 天空带零 diff + crop 3× 目检**（同场景同羊同位，其余像素零
变化）】+ 性能 A/B 同档【upd 均值 0.528 vs 0.503 噪声带 +
changesApplied 9/1/3 逐位 + 终档 savenorm **EXACT**】。PITS 392
（DEBUG 级事件行 grep 假阴性——门跑 --log-level debug 或 crop 目检
互补）。详见 `docs/archive/phase-3.12.0.md`。

## v3.11.0（2026-11-05）— V3-9 demo 回放（注入内核 + 编写/录制/回放三入口 + 观众只观察路由，门 201 全 PASS，**3.x 矩阵收官**）

**用户 11-05 BACKLOG 定案条目执行**——① **demo 文件格式**（V3-7
确定性 gzip 容器 10B 头全钉 + 严格 schema：version 指纹 TM_VERSION
【不匹配拒播点名】/ world fresh{seed}|save{slot}【双键·缺键·槽字符
集拒】/ dts【负·>0.25 拒】/ events【乱序·悬空帧·未知 type 带 8 词
表·payload 面全严格】；**零 format** = 独立文件族）② **注入内核**
（core/demo 新：DemoPlayer 注入 = Input 公开 handle* 入口【与
xdotool 跑的游戏内路径同形，游戏逻辑零触碰】/ dtForFrame = **demo
dts 唯一时间源** / rendersPerFrame【0.5→2·1→1·2→1】/ nextSpeed
0.5→1→2 循环 纯函数）③ **双 Input 路由 = 架构边界**（回放：观众
Input 实例 = SDL 队列单消费者 + 玩家 m_input 零 pump 纯 demo 注入——
**改世界操作在观众通道无消费者 = 白名单非黑名单结构性零遗漏**；
录制：现状 pump + handle* 边界 tap 先记后注【录制零副作用单测钉】；
普通运行 tap 恒 null = 位级恒等）④ **三 CLI 旗互斥**（--demo 回放
/ --demo-author 头less 编写【timeline JSON 展开：hold 展开 + dts =
1/60 定步全帧锁定】/ --demo-record 游玩捕获 O(1)/帧【quit 帧未模拟
尾部排除】）⑤ **回放循环**（墙钟只 pacing int64µs 防漂移，慢机掉
渲染帧模拟不追；倍率切点 = demo 帧边界不切半帧；**0.5× = render-
only 保持**【第二渲染帧零 update 零 ++m_frameCount = 帧锁定系统不
双推】；**--frames 按模拟帧计** = 任意倍率同终态）⑥ **观众只观察**
（ESC = 暂停菜单**非退出**【Q 退出】/ C = 观察视角循环 / mouse +
摇杆 = 转镜头 **orbit render-only**【aimForward() = 回放模式基视角
无 orbit——观众转镜头不偷 demo 玩家瞄准，7 处世界向射线机械替换】/
手柄 Start=暂停 LB/RB=倍率 左摇杆=数字化步进【V3-8 已先行 = 定案
立项加】）⑦ **观察红线**（save-spec 回放引用槽永不回写【md5 前后
逐字节不变实证】/ --demo + --new/--seed 点名拒 = world spec 权威）
⑧ shipped 示例 demo（assets/demos/example.dmo 285 B 266 帧 14 事件：
W 走 + 挖掘 2 块 dirt + E 背包 + C 三后 + zoom；生成脚本重跑字节
恒等）+ data/demos/ gitignore（录制默认 rec_<ts>.dmo）。**版本三
件套** 3.10.0→3.11.0。2777/2777（+43：schema 拒收 14 / 加载级 4 /
gzip 容器恒等 3 / 注入内核 5 / 录制零副作用 3 / 编写展开 8 / 观众
路由 2 / 标签 3）。门 201 = **结构免疫第 81 次免重锚（对 base19）**
【S1 三旗 absent ≡ base19 逐字节 + savenorm EXACT vs v3.10.0 旧侧】
+ S2 六臂【(a) shipped 示例回放 ×2 RAW + savenorm EXACT = 定案"首
放固化" / (b) **录制→回放闭环**（rec.dmo 2810 B 10B 钉头 → ×2
RAW + EXACT）/ (c) **编写臂**（timeline → ×2 EXACT + 玩家位移
4.121 块 = xdotool 替代面实证）/ (d) **观众路由**（转镜头终档姿态
≡ 无转镜头逐位恒等 = orbit render-only）/ (e) **变速双向**（wheel
up/down 终档 savenorm EXACT vs 参照 + 像素 diff 76 px 全在 HUD
角标 = 任意倍率同模拟实证）/ (f) **结束面板**（放完态退出截图
patch lum 32.0 < 120 暗帷在场 + ×2 恒等）】+ S6 点名 7 面【版本
拒播 / 乱序 / 未知 type 带词表 / 负 dt / 超 0.25 / world 双键 /
gzip 损坏 全 rc=1 带路径】+ 性能 A/B 同档【主对旗 absent：upd 均值
0.501 vs 0.579 窗口噪声带 + changesApplied 逐位 + 终档 EXACT；回放
自成本 = sub-ms 零可测增量】。**门内缺陷修复**（fixer 二次交付）：
观众 UI 不可见 = demo UI 块位于主批冲刷循环之后【顶点永不被消费】
+ 项目截图 = swap 后读 back 落后一帧既有语义 → 自包含独立批 +
headless 放完双渲染 + badge 移时钟下（S1 锚修复后回归 PASS）。记档
偏离 8 条（英文 UI 串 + 独立批 / LB/RB 不分源 / quit 尾排除 / world
spec 权威 + 观察红线 / 简版示例 / 放完态退出 / 左摇杆数字化 / E2E
实证门内补齐）。**3.x「世界扩展」矩阵 11 项编号任务全 ✅（V3-1~
V3-9 + V3-8/V3-9 UX 行）**；方向项（死亡统计屏 / Game Over 屏 /
末地 / 创造 / 多人）矩阵外立项需用户定案。详见
`docs/archive/phase-3.11.0.md`。

## v3.10.0（2026-11-05）— V3-6 世界边缘雾（填充感知 fog end，base19 重锚，门 200 全 PASS）

**流送盘边缘不再露截面**——消费「世界边缘即虚空（无雾）」Phase
3/14 时代记档。编排者实证定案：无输入跑 worker = 启动 16 列（4×4
演示区 cheb 半径 2）后零扩展（PIPE-1 结构免疫纪律）→ 锚图"浮空
截面" = 演示区边界 32-45 块，远在雾 start 750 内完全可见（fog off ≡
on 逐字节实证：可见世界全 <750）；现有 PRE-2 雾按**目标盘**（min
1000/1024 = 1000）算 = 对未填充盘缘无效。修复 = **候选 B 标量化：
fog end = min(静态 end, 实际填充盘 chebyshev 半径 ×16)**：无输入/
填充中盘面雾吞截面（end 32 / start 24 = "浮空岛淡出"）；稳态行走
盘满 64 → min(1000, 1024) = 1000 **位级恒等**（单测含 65 滞回变体
钉）；**下界同款 clamp**（首传 3×3 catch-up 半径 1 = end 16 同款
隐藏）。纯代码 9 文件：`filledChebyshevRadius` 纯函数（流送已载列
max Chebyshev，填充 rim 语义 = 下闭保守性论证）+ `fillClampedEnd` /
`fogFilledRange` 纯函数（start = 0.75 × clamp 后 end——ramp 总止于
填充 rim）+ render 雾块（列 set→vector 快照共享零新树走查）；**零
配置新键 / 零 format / 零新 worker 活动**（无输入日志逐行恒等实证）。
2734/2734（+10：FogFilledTest 6【满盘位级恒等含 65 滞回 / 2→32 / 0
→ degenerate off / clamp 只降不升 / nether 16·32·96】+
FilledChebyshevRadius 4【演示区 4 可达列全 = 2 / 满盘 64·65 / 部分
填充】）。门 200 = **base19 重锚（首重锚，225272 B）**【diff 审计
20940 px 全在盘缘带：上天空 0 diff + 前景/HUD 0 diff + 逐通道单调
趋天空 0 违例 + 单雾因子 100% ≤2 LSB 舍入签名 + 留证五图；存档面
savenorm vs v3.9.0 旧侧 EXACT = 雾纯渲染】+ S1 ≡ base19 ×2 + S2
**fog-off 控制臂 ≡ base18 逐字节**（三方闭环 = 唯一像素面实证）+
性能 A/B 同档【无输入对 = 16 列扫描零成本 + changesApplied 逐位 +
终档 savenorm EXACT】。详见 `docs/archive/phase-3.10.0.md`。

## v3.9.0（2026-11-05）— V3-5 建筑内容族（黏土链/石砖/苔石/15 色陶瓦染色族/水底 sediment 生成，门 199 全 PASS）

**建筑的多样性**——① 黏土链（MC 完整形态）：水底 sediment 生成
（世界生成新 pass，worldgen.json 数据驱动 `sediment` 节：
clay_probability 0.06 / mossy_cobblestone 0.008，per-column 纯函数
新盐零噪声，dry 列零触碰，熔岩湖双谓词拒收，顶 1 层）→ 破 clay 掉
**clay_ball ×4** → 熔炼 brick → 2×2 合成 bricks → 熔炼 **terracotta**
② 石砖族：stone_bricks（2×2 cobblestone）/ mossy_cobblestone（水底
稀有生成，记档偏离 = MC 废墟替代）/ mossy_stone_bricks（shapeless
石砖×3 + 苔石×1）③ **陶瓦染色族**（Phase 41 机制首个新族）：
terracotta white 基块 + **14 染色变体**（项目染料词表 15 色含 white
无 lime——设计 16 色按项目词表收敛，偏离 1；init 交叉检缺一色点名）
④ 纹理 20 tile（14 染色 = 新确定性混合公式 (base×3+dye×2)//5，
零新盐）+ 2 icon + atlas 145→165 空格零移位（无幂二增长）。
**核心 = 仅 worldgen 4 文件**（Sediment 解析 + pass + 3 纯谓词 +
fail-fast；absent = disabled 位级 legacy；[0,1] 严格）。2724/2724
（+27：内容钉 13 / 生成 pass 8【含发行世界 census 1024²：eligible
3683 / clay 211 / mossy 26 沙漠沙洋底 only + 样本列 ground truth】/
config 6）。门 199 = **结构免疫第 80 次免重锚**（S1 ≡ 锚 224468 +
savenorm **EXACT** + 白名单 4 键——生成内容不入 diff + 锚图零水域 +
opaque 派生 15 同不透明度）+ S2 三臂【全手工密封水室（围堰 48 格，
PITS 390 场景几何教训）：(a) 展示行 ×2 RAW 恒等【6 件新纹理水下
可见实证 + 池格 32 精确】/ (b) **破 clay → 4× clay_ball 入库存**
【磁力拾取 + 编辑回退参考】/ (c) 熔炼臂【"Furnace smelt complete:
brick" + fuel_substeps 4201 验算】】+ S6 4/4 点名【概率越界 ×2 /
字符串 / **染色族缺一色交叉检**】+ 性能 A/B 同世界同成本（PERF-WIN 三窗同档：upd med 0.550/0.539/0.574 vs 0.607/0.574/0.612
  ms = 窗口间机器噪声带，均值 0.554 vs 0.598——sediment pass =
  生成链一次性列成本零每帧开销；render 均值 0.573 vs 0.579 同档；
  swap vsync 同档；fluid·fires·spread·ents·stream 同档）+
  changesApplied 9/1/3 逐位 + 终档 savenorm **EXACT**（生成内容不入
  diff 零 format 完全恒等最强形）。
记档偏离 6 条。详见 `docs/archive/phase-3.9.0.md`。

## v3.8.0（2026-11-05）— V3-3 下界 mob（5 种 + neutral/walks_on_lava 旗 + 下界生成池，门 198 全 PASS）

**下界有了居民**——① **5 mob 纯数据**：zombified_piglin（hostile，
zombie 数据孪生，**undead false**）/ **piglin·hoglin = neutral 新类**
（闲逛；受击追击——piglin 16HP/6 伤，hoglin 40HP/慢速 1.0/drops
hoglin_meat 1-2 + leather）/ magma_cube（hostile 16HP 慢速，**不分裂
不跳** blob 3 盒）/ **strider**（passive + **walks_on_lava**，熔岩面
行走）② **M1 neutral 旗**（与 hostile 互斥点名拒）：`damage()` 单
收口任意来源激怒（玩家/箭/火/摔落/毒，MC piglin 同款）+ 30 s real-dt
脱激 + 再击刷新 + 玩家死亡即时脱激；chase 谓词单式第二支
`(p.neutral ? m_angry : true)` = 非 neutral 位级恒等 ③ **M2
walks_on_lava 旗**：熔岩点火豁免（火块仍点燃 = MC 同款）+ 熔岩面
**feet-on-plane walk 档**（snap 不动点 fps 无关 + 深潜浮回 + 免溅
落）+ 生成面 = 熔岩源格顶面（findMobSpawnCell 尾参 flag 门控）④
**M3 下界生成池零新基建**（Phase B 预留槽兑现）：
mob_spawning_nether.json 权重 10/8/7/5/1 + fail-fast init 点名 +
单一共享类型表 + 策略继承主世界会话值；维度选池/只 tick 活跃维/非
活跃维冻结/mob 不入存档 = 既有语义零代码 ⑤ **strider
spawn_max_light 7→15**（门内实证机制缺口：熔岩自发光 15，7 门自毁
= 偏离 1）⑥ 食物链 hoglin_meat +4 / 熟 +8（pork 先例）+ 3 地面
ambient（hoglin grunt/piglin whisper/strider hiss——地面 roll 新
runAi 分支 = bat flyCruise 先例推广，数据门控零流抽取 = 偏离 3）+
纹理 9（atlas 136→145 空格零移位）。**版本三件套** 3.7.0→3.8.0。
2697/2697（+39：旗解析互斥 4 / 内容钉 6 / neutral 激怒态 7 / 熔岩
行走 7 / 生成谓词 6 / 池 5 / 音效 3 / 食物链 1 + census 重钉）。门
198 = **结构免疫第 79 次免重锚**（S1 ≡ 锚 224468 + EXACT + 白名单
4 键）+ S2 三臂【(a) 下界高原 ×2 生成流恒等（piglin f60/magma f90/
zpig f120/piglin f150 位置逐字节）+ 截图 RAW 恒等 + 终档 census /
(a2) **strider 决定性臂**（环全清 4704 air 编辑 = cap 永不饱和——
每次尝试新抽：**`strider f660 at (13.5, 32.0, −5.5)` ×2 恒等，y =
32.0 熔岩海顶面精确 + 540f 零 burn 点火豁免 E2E**）/ (b) 池隔离
（OW 生成 4 + 下界物种 0）】+ S6 6/6 点名【互斥 / 双旗非 bool /
池缺失 / 池未知 mob / ambient 单向严格对】+ 性能 A/B 同世界同成本
【PERF-WIN 三窗同档 + changesApplied 9/1/3 逐位 + **终档 savenorm
EXACT**【nether-pool only 零 format 最强形】】。记档偏离 7 条
（光门 15 / 异种流体 / 地面 ambient 分支 / 共享类型表 / 策略继承 /
下界原点保护圈 / ghast 延后）。PITS 389（门配置 mobs-off 纪律 / sed
派生 OUT 目录 / spawn 行正则负号 / cap 吞尝试 / 落点光场压制）。详见
`docs/archive/phase-3.8.0.md`。

## v3.7.0（2026-11-05）— V3-2 Part 2 下界深化（水蒸发/音效族 11 件/magma 光/ambience 第 4 上下文/soul sand 数据驱动减速，门 197 全 PASS）

**下界的行为补全**——收口 v3.4.0/v3.5.0 的 4 条记档偏离，全
nether-scoped（**overworld 位级不变**单测 + 锚双保险），零渲染/零
format（30 不变）/零 config。① **T1 水在下界蒸发**（v3.4.0 偏离 8
no-op → MC 蒸发语义）：下界泼水桶 = 桶变空桶 + `evaporate` 音 + info
日志，**零格触碰**（桶独占事件）+ 点击恒消耗；下界熔岩桶正常放置
（MC 同款）；主世界泼水 = 同一语句 false 分支位级恒等。② **T2 下界
音效族**（v3.5.0 偏离 3/4 stone·grass 复用收口）：新 `nether`/
`nylium`/`stem` 3 族 9 件（固定种子确定性合成）+ soul 族 → 既有
`sand` / ore·basalt·nether_brick → 既有 `stone` 重指派（sound_material
字段数据驱动零核心）——13 下界块音族全钉；SoundBank 49→60（11 新
wav 含 evaporate + ambience_nether，既有 49 件 md5 逐一不变幂等实证）。
③ **T3 magma_block light_emission 3**（v3.5.0 偏离 5 收口，MC 精确，
数据一行）。④ **T4 下界 ambience 第 4 上下文**（优先级 nether > cave
> day/night，MC 同款"下界洞窟也是下界"= 偏离 5）：
`ambienceContext` 加**默认参 inNether=false**（既有调用点位级不变，
overworld 30 组输入面恒等单测钉）+ `ambience_nether` 48 Hz 隆隆 2.0 s
新 wav；调度常量复用（300f 窗 × p 0.5 不动）。⑤ **T5 soul sand 减速**
（v3.5.0 偏离 D6 收口，数据驱动）：块属性 **`walk_speed_factor`** 严格
解析 number (0,1]（absent=1.0，越界/非数字 init 点名；**仅
soul_sand 0.4** MC 精确）+ player 步行消费 = **双 feet 格取最小因子**
（支撑行 × X 跨双端点严格重叠 × 中心 Z 列；飞行/流体/潜行豁免，疾跑
= 步行受减速 MC 同款偏离 3）；`speed × 1.0f ≡ speed` 位级恒等 = 主
世界无携带块时的结构免疫根。**版本三件套** 3.6.0→3.7.0。2658/2658
（+18：WalkSpeedFactorParses 6 形 / WalkSpeedFactorTest 7【纯函数
1.0 位级 / 2 s 位移比 0.4±0.005 / 边界双格 / 疾跑亦减速 / 潜行·飞行
·涉水豁免轨迹恒等】/ AmbienceNetherTest 5【优先级四形 / overworld
位级恒等钉 / 发行 seed roll 首 FALSE = 帧 2100 边界 + day 流去相关 /
wav 钉】/ SoundNetherFamilyTest 4【PITS 382 守卫】/ census 60 ×2 /
T1 谓词四形）。门 197 = **结构免疫第 78 次免重锚**（S1 无输入 ×2 ≡
锚 224468 + savenorm EXACT + 白名单 4 键）+ S2 下界场景三臂【(a)
shot ×2 RAW 恒等 + **ambience_nether 事件 a=2 b=2 逐一对**（帧锁定
确定性）+ nether 段 16 块编辑持久 / (b) **soul sand 步行比值
0.403**（全 soul 行 vs 全 neth 行 3.0 s W-hold：dx 4.321 vs 10.712，
T5 端到端）/ (c) **桶蒸发**：日志 + slot0 空桶 + nether 段零 water +
(c2) 主世界对照**水正常放置**零蒸发】+ S6 3/3 点名【1.5 / 0 /
"0.4" 字符串全带 "walk_speed_factor" 路径 rc=1】+ 性能 A/B 同世界同
成本【PERF-WIN 三窗同档（均值 0.568 vs 0.557 ms 新侧更低）+
changesApplied 9/1/3 逐位 + **终档 savenorm EXACT**】。记档偏离 6 条
（无蒸汽粒子 / portal UV 动画延后 = 渲染器基建 / 疾跑亦减速 / 双 feet
格取定形 / nether>cave 优先 / 11 wav = 设计计数笔误 T1 规格胜）。详见
`docs/archive/phase-3.7.0.md`。

## v3.6.0（2026-11-05）— V3-4 netherite 材料链（第四材料档，**零核心代码纯数据**，门 196 全 PASS）

**ancient_debris → scrap → ingot → netherite 工具/护甲**（完整 MC 链的
项目形态）——① 熔炼链：ancient_debris 块 `+smeltable_to: netherite_
scrap`（CRAFT-6 块物品继承，netherrack → nether_brick 先例）→ scrap
material 物品 ② **netherite_ingot** = shapeless 4× scrap + 4×
gold_ingot（记档偏离 1 = MC 用 gold nugget，项目无 nugget）③
**netherite 工具 3 件**（项目无钻石工具 = 直接 crafting 克隆铁形状，
记档偏离 2）：**durability 2031**（MC 精确）/ pick break_speed 4.0（
项目铁刻度）/ attack_damage 4·5·4.5（铁 +1）/ enchantability 14 /
**fast_against 2.0 × 21 块**（铁石族 10 + nether 台地 11 = 正确工具
语义）/ repair netherite_ingot ④ **netherite 护甲 4 件** = **shapeless
升级 ingot + diamond_<slot>**（项目无锻造台，记档偏离 3）：**durability
545/792/743/644**（= 钻石 ×1.5 MC 精确）/ armor_points 3/8/6/3 /
enchantability 10（钻石同款刻度）⑤ icon 9 件（暗红铜色板 = 下界世界观
一致，生成脚本确定性，既有 84 张 md5 不变）+ model 9 件（工具克隆铁系
/ 护甲克隆钻石系换纹理）。**零核心代码**（git status src/ = 空——
"加材料 = 数据"承诺的第四档验证）：smeltable_to/shapeless/shaped/
repair_with/fast_against/model 全既有机制。2640/2640（+23：内容钉族 /
熔炼数据 + tick 两段 / 合成行为 3 族（ingot/升级/工具网格）/ 护甲
×1.5 刻度验算 / icon·recipe census / **SmeltThenCraftEndToEnd** / S6
拒收 4 面）。门 196 = **结构免疫第 77 次免重锚**（S1 ≡ 锚 224468 +
白名单 4 键——新物品不在默认热栏 + 无世界生成消费 = 纯内容零像素）+
S6 点名 5/5【durability 0 / enchant_class 未知词带词表 / strict pair /
未知材料物品 / fast_against 未知块引用——全 init 带路径】+ 性能 A/B
同世界同成本【PERF-WIN 三窗全同档 + changesApplied 9/1/3 逐位 + 终档
savenorm EXACT 完全恒等】+ 熔炼臂实证（fixer 冒烟：600f 无输入
debris 入炉 → "Furnace smelt complete: netherite_scrap" + 炉内 output
×1 + fuel_substeps 4201 精确验算 + **xp orb ×1 = smelt_xp 发放面实证**
）。记档偏离 4 条（nugget/直接 crafting/无锻造台/暗红色板）。详见
`docs/archive/phase-3.6.0.md`。

## v3.5.0（2026-11-05）— 下界内容族 Part 1（V3-2 合并执行：群系 +
块族 + 矿脉 + 装饰 + **下界带修订为可航行空腔**，门 195 全 PASS）

**下界变成 MC 的下界**（第一步）——① **D9 带修订（编排者布局裁决，
实现后实证 v3.4.0 实心岩板下界不可航行）**：MC 下界废土形态 =
基岩底 0-4 / 熔岩海 5-31 全源（岩板下挖穿可见）/ **netherrack 台地
32-52**（群系表面 = 台地顶）/ **空气空间 53-122**（70 格可航行空腔）/
天花板 glowstone y=122（贴基岩顶底面 MC 精确）/ 基岩顶 123-127——
传送落点 Y=55 → 空腔内（首访观感实证：暗红天穹 + 远地平线雾吞 +
portal 平台）；世界生成演进先例（未编辑列按当前生成器重建，既有
portal 框持久）② **5 群系**（biomes_nether/ 独立池独立 registry，
MC 比例权重 36/22/17/16/9；netherBiomeAt = biomeIndexAt 克隆 + 独立
盐；装饰门控 = 数据派生旗 fill 零 name switch）③ **12 新块**
（nether_brick 2.0 / magma 1.5 / quartz_ore 3.0 / nether_gold_ore 3.0
drops gold_ingot / ancient_debris **10** = MC 50 的 obsidian /5 缩放
先例 / soul_sand·soil 0.5 / 双 nylium 0.4 grass 先例 / basalt 1.2 /
双 stem 0.4——全 stateless；纹理 12 件确定性生成，**atlas 16×8→16×16
幂二增长既有 124 格零移位 + 像素行恒等 = base 锚免重锚实证**）④ **3
矿脉**（lattice 8 per-cell 纯函数，台地 only：quartz p 0.0376 /
nether gold p 0.0208 / ancient debris p 0.0026）⑤ **5 装饰**（stem
柱 4..9 / basalt 柱 5..30 自 y53 可见 / soul_soil·magma per-cell 带
48-52 / 天花板 glowstone 复用既有块）⑥ quartz 物品 + nether brick 熔
炼/2×2 配方 2 件⑦ **init fail-fast**（门内 S6 实证发现：surface 存在
性/stateful 拒收从首传惰性前移 buildScene，主世界同款）。2617/2617
（+35）。门 195 = **结构免疫第 76 次免重锚**（S1 ≡ 锚逐字节 + 白名单
4 键）+ S2 内容双臂【(a) 传送空腔截图 ×2 RAW 恒等 + 观感实证 / (b)
**下界原生采矿**：quartz_ore 破 → quartz ×1 入库存【磁力 1.08<2.5】+
按住续破 soul_sand【PITS 7】+ 掉落 2.5 边缘留地【PITS 377】+ OW 段
26 块原样】+ S6 5/5 init 点名【weight 0 / 重复 id / 未知 surface /
空池 / stateful surface】+ 性能 A/B 同世界同成本【主对 900f 无输入：PERF-WIN 三窗全同档（upd med 0.578/0.615/0.634 vs 0.556/0.563/0.584 ms / render 0.597/0.651/0.596 vs 0.600/0.598/0.544）+ changesApplied 9/1/3 逐位 + 终档 savenorm **EXACT**【零 format 完全恒等】+ 下界含内容稳态 ≤ OW 基线（render med 0.365/0.361/0.362 vs 0.60 + fluid/fires 0.000 熔岩海零 churn）】。
详见 `docs/archive/phase-3.5.0.md`。

## v3.4.0（2026-11-05）— V3-1 Phase B 下界世界 + 真传送（门 194 全 PASS）

V3-1 三段拆分第二段（**下界维度本体**；Phase A 门框 v3.2.0 已发，
Phase C 内容族 = v3.5.x 系列）——**站进主世界传送门，真的去下界了**：
① **下界维度**（D1 增量并行成员：overworld 代码路径零 diff；D2 **惰性
构造**——首传前零 chunk/零 mesh/零流送/零内存；D3 **仅 Player 换绑**
（Entity setWorld 单咽喉点，m_world 引用→指针 17 点机械适配），
inactive 维 = 时间胶囊【world-pass 冻结，DayCycle/Weather 全局恒推进
= MC 时间连续】）② **下界 worldgen = 专属纯函数**（坐标的纯函数 =
零噪声构造性确定）：基岩底 0-4 / **熔岩海 5-31 全源静态**（流体零
churn）/ netherrack 32-122 / 基岩顶 123-127；派生 seed `worldSeed ^
kSaltNether` 不落盘（单源真值）；netherrack 块（hardness 0.4 MC 精确 /
drops 自身 / 纹理 atlas [13,7] 零移位 / 音族复用 stone 记档）③
**skyless 光照 2 参**（void/顶 sky 读 0 + rescan 顶源 0，shader 零
改动 = 无天穹下界）④ **真传送管线**（单帧帧锁定无 loading screen）：
60 帧倒计时 → 8:1 floorDiv 双向映射 → 目标维 ±16 搜活跃 portal（无 →
**生成 4×5 含角框 + 2×3 基岩平台**走咽喉点）→ 落点 hitbox 安全 +
step-up → 3×3 列盘同步 catch-up → 换绑 + restorePosition + 相机 →
流送自动 engage → portal_trigger/travel 2 件新音效（47→49）；**900
帧冷却**（15 s 防振荡，会话态）；**正交 portal yaw −π/2**（MC JE 顺
时针 90°）⑤ **渲染 5 点门控**（暗红 clear / 日月星云雨不画 / 雾
10→96 块短程 + F3 dimension 行 19→20）⑥ **format 30**：player.
dimension 严格词表恒写 + nether 段 {blocks,fluids,states} 恒写 +
nether_entities（item）恒写；29 携新键 = version lie 点名 / 30 缺键
点名 / 未知块名点名 / nether 内 stateful 块须 nether.states 条目
（PITS 383① 孪生）；两维 portal 载入 rescan；gzip 编码零改动；**下界
死亡 = 主世界重生**（MC 精确）⑦ 下界行为收口（7 per-block state 族
主世界 only / 床睡眠 no-op / 容器面板主世界 only / 下界火不烧尽 /
水桶 no-op = 8 条记档偏离）。2582/2582（+27：nether_gen 14【层带/熔
岩顶边/chunk 连续/负坐标/纯净性/派生 seed/框搜索族/hitbox 上探/正交
yaw】+ portal 映射往返 2 + format 30 族 7 + setWorld 钉 + F3 行 +
块内容钉）。门 194 = **结构免疫第 75 次免重锚最强形**（下界惰性未建：
S1 ≡ 锚 224468 逐字节 + 终档**恰增白名单 4 键**）+ **S2 双跳 arm
（全 headless）**：传送（"Traveled to nether at (1,55,−1) from
(0,55,−2)" 映射验算 + 下界截图 ×2 RAW 恒等【netherrack 岩洞 + portal
光 11 紫晕】+ 落点 [1,55,−0.5] 精确 + nether 段 26 块 = 生成框 +
平台）+ 返程（±16 搜中原 portal + 落回原 portal 内域底中心 + nether
段持久化 + time 恰 +120f 帧锁定验算）+ S6 点名 6/6 + 性能 A/B 同世界
同成本（PERF-WIN 三窗全同档 + changesApplied 9/1/3 逐位 + 终档白名单
4 键 + **下界稳态 ≤ OW 基线：render 0.349 vs 0.510 med、fluid/fires
= 0.000【全源静态熔岩海零 churn = recon 风险面 ③ 按设计消解实证】**）。
PITS 387（g139 插桩器锚点随 DimCtx 调用点形态演进 = 双形态锚 + 每树
用自己 commit 的插桩器）。详见 `docs/archive/phase-3.4.0.md`。

## v3.3.0（2026-11-05）— V3-8 手柄支持（门 193 全 PASS）

11-05 用户实机诉求定案（BACKLOG UX 行，v3.2.0 收口后紧接执行）：**SDL3
GameController 一阶 API，实现面 = 输入边界层**（世界/渲染/存档/物理
零触碰）——① **源位掩码 OR 语义**：`m_keyDown`/`m_buttonDown` bool →
每源一 bit（bit0 键鼠 / bit1 手柄），`isDown` = 任一源按下，每源独立
rising edge，`wasReleased`（弓）仅本源 falling 且掩码清 0 才置 =
**键鼠手柄天然双连共存**（同一动作的并集，任一侧按住不松）+ **无手柄
= bit1 永不置位 = 位级恒等**（legacy fixture 单测钉）；② **v1 映射**
（Xbox 布局，9 钮 + 2 摇杆）：左摇杆移动（死区 0.2 外数字化，四方向
全量重算幂等回中清键）/ 右摇杆视角（死区外重映射 × 100 px 当量合成
mouseDelta：杆右 = xrel+，杆上 = yrel−）/ A = Space【双击飞行自动
继承】/ B = LeftShift 疾跑 / X = E 背包 / Y = Q 丢弃 / LB = 挖掘 / RB =
使用 / Back = C 相机 / Start = ESC / D-pad 右 = Tab；LT/RT/L3/R3/Guide/
D-pad 上下左/paddles = v1 不映射记档；③ 单手柄模型（第二手柄忽略）+
连断日志（`SDL_GetGamepadNameForID`，无 = "unknown"）+ F3 末行连接态
（18→19 行）+ 断连清全部手柄位**不置释放边**（弓 stale-up 取消蓄力而
非悬挂）；④ `SDL_Init(VIDEO | SDL_INIT_GAMEPAD)`（无手柄环境惰性零成
本）；无 config 键（映射 = 命名常量，`kGamepadLookPixelsPerUnit` = 未来
config 化锚点）。2555/2555（+19：test_gamepad 18【映射纯函数钉 / 9 钮
按压释放含 RB 弓边 / 17 不映射钮零变化 / 死区边界 6225·6553·6554 /
四方向+对角+回中+切换幂等 / 右摇杆符号+增益精确 + 累积 + frameBegin
复位 / 重映射公式钉 / trigger 零变化 / 键·钮 OR 弓语义双序 / 断连清位
+ 再连 + 陈旧 no-op / 单手柄模型 / 未连接 no-op / **legacy 位级恒等
fixture**】+ DebugOverlay GamepadLine 四形）。门 193 = **结构免疫第 74
次免重锚**（本环境无物理手柄 = 零 SDL 手柄事件：S1 无输入 ×2 ≡ 锚
224468 逐字节 + gamepad 日志行 = 0）+ S3 全套 + S6 跳过记档（无
config/内容面）+ 性能 A/B 同世界同成本（PERF-WIN 三窗全同档 +
changesApplied 9/1/3 逐位 + **终档 savenorm EXACT 零 format 最强形**）；
**门强度记档**：全部 handler 逻辑单测覆盖（handle 入口 = handleKey 同
款模式）+ 实机验收 = 用户（release 提示）。详见
`docs/archive/phase-3.3.0.md`。

## v3.2.0（2026-11-05）— V3-1 Phase A 下界传送门框（门 192 全 PASS）

V3-1「下界维度·传送门与下界」三段拆分第一段（**门框**；Phase B 下界
世界 + 真传送 / Phase C 下界内容族另立项）——① **nether_portal 块**
（纯数据 + 紫罗兰漩涡纹理 atlas [13,6] 零移位 + portal_ambient 确定性
合成音效 46→47）：full-cell 半透盒（lava 先例）/ **destroyable false**
/ **light_emission 11**（MC 精确）/ 无 drops / **state_count 2 axis**
（ENT-2 机制）；② **core/portal 纯函数族**：MC 框语义（4×5..23×23
外廓、**角可选**、竖直平面、高度恒 ≥5）有界扫描（45×45×2 平面 +
前缀和 O(1) 候选判定，事件 <1 ms 级）+ 激活/去激活决策（obsidian 与
fire 双向事件检查 = "火须最后放" quirk 偏离记档；**非角破坏去激活 /
角破坏保留 = MC 精确**；**fire 熄灭 ≠ 去激活 = portal 自持**；逐框
确定性应用）+ 8:1 映射纯函数（floorDiv 负坐标安全）+ 站入 60 帧
（1 s）帧锁定 step + ambient roll；③ **setBlock 咽喉点钩子**（furnaces
簿记族克隆：obsidian/fire 事件 → 扫描 → 内 air+fire → portal
state=axis【fire 格被替换 = MC "one of them replacing the fire"】+
bbox 一次重光 + 日志）+ **点火分支 (c)**（命中环格**面法向相邻内 air
格**点火——框内 air 与环共面射线不可直达，slab 放置同款几何；磨损 +
音孪生）；④ **站入倒计时**（AABB 重叠 arms 60 帧 / 走出取消 / 无冷却
重计 / **GUI 互斥：portal 内开面板自动关**【MC 传送门规则，closePanel
既有语义 = click 重捕获】）；Phase A 传送 = 显式 no-op 桩
（`kPortalTravelEnabled = false` + static_assert 构建门，禁 TODO；
完成帧日志 "Nether ready at (mapped 8:1)" = Phase B 输入）；⑤ **载入
rescan**（孤立 portal 块 → air，PITS 383① 双向交叉检纪律 + 追踪集
重建 + 一次性重光）；⑥ **存档零 format**（29 不动；portal = 普通
stateful 块走 blockEdits + states 段）。2536/2536（+33：portal 纯
函数族 31【扫描域/角可选/3×4·4×4·24×23 拒/水平拒/共享框/transition
全形/entry step/映射/ambient roll/S6 内容拒收 3】+ 块内容钉 +
ambient wav 守卫）。门 192 = **结构免疫第 73 次免重锚**（≡ 锚 224468：
worldgen 零 obsidian + 打火石不在默认热栏）：S1 无输入 ×2 ≡ 锚 +
savenorm EXACT；**S2 输入 E2E 六臂**（激活【终档 6 portal state 0 +
fires 0 + flint damage 1 + 日志】/ 去激活【obsidian 硬度 10 17.5 s
hold】/ **角保留 MC 精确** / 站入【60 帧恰 "Nether ready at (0,55,−1)"
验算 + 零位移 + 每秒重计语义】/ **GUI 互斥决定性六跑**【d6 站入+E+
重捕获 ≡ d3 对照 RAW 逐字节 + d4/d5 框外面板留开 = 条件化】/ 旧侧
形状互斥）；S6 4/4 点名；性能 A/B 同世界同成本（PERF-WIN 三窗全同档
+ changesApplied 9/1/3 逐位 + **终档 savenorm EXACT**【零 format 最
强形】+ portal 自成本无可测 per-frame 增量）。PITS 386（门内三坑：
面法向点火方向 / cell 索引 vs 格中心差 0.5 / V3-7 起门脚本读档过 gzip
魔数）。详见 `docs/archive/phase-3.2.0.md`。

## v3.1.0（2026-11-04）— V3-7 存档 gzip 压缩（3.x 首功能版，门 191 全 PASS）

11-04 用户实机诉求定案（data/saves 体积大）先执项：**整档 gzip(JSON)
落盘，默认开**——① **确定性 gzip 容器**（手写 = 字节可钉：固定 10B
头 `1f 8b 08 00 00 00 00 00 00 03`【FLG=0/MTIME=0/XFL=0/OS=3】+ raw
deflate level 6 + CRC32/ISIZE 尾；zlib 自带 gzip 封装的 MTIME/XFL/OS
随版本漂移 = 排除，PITS 385①）：同世界存档逐字节可复现；② **加载
魔数 `1f 8b` 自动探测**（唯一编码判据，扩展名恒 .json）：旧明文档
**零迁移直读**（format 28 及更早明文档新构建原样加载）；解码严格
（头 10B 逐字节全钉 / 截断 / 半体 / CRC·ISIZE 不符全点名带路径，
系统 gzip CLI 产物点名拒收不静默容忍）；③ **config 键
`save.compression: "gzip"|"none"` 严格读**（`save.auto_save_seconds` +
`render.ao` 双先例）：absent = **gzip（代码默认 = 用户定案**；
committed config.json 无 save 节 = 零 config 改动）/ "none" = 明文
（旧行为位级恒等：同状态文件与 v3.0.0 差异 = 恰 root.format 单键）；
非 string / 空串 / 未知词 init 点名拒；④ **format 29**（schema 与 28
完全一致——编码与 format 号正交，29 = "gzip 时代"标记；per-format
规则全 `>= N` 下界检查 → 解析侧代码零改动）；⑤ **集成 = 写侧单
choke point**（`Game::writeSave` exit/F5/autosave 共享：payload
`dump(2)+'\n'` 一字不动 → 按 config gzip 或明文 → `writeAtomicFile`
原子性不变）**/ 读侧两个消费点**（`Game::readSaveFile` +
`--list-saves` 摘要，corrupt 槽 problem 命名不隐藏文件）；
save copy/rename/delete = 纯字节操作零改动；⑥ **`tools/
gate_compare.py` savenorm 加解压步**（魔数 → gzip.decompress）；
⑦ **save/load 日志加耗时 ms + 体积变换**（real-dt 性能证据，不入
字节门）。**zlib 零新依赖**（自研 PNG 已链接）。门 191 = **结构免疫
第 72 次免重锚**（存档 = 渲染无关路径，零像素）：S1 无输入 60f ×2
≡ 锚 224468 逐字节 + 头 10B 实证 + 存档 savenorm EXACT（RAW diff =
fires real-dt 三键 = PITS 385③ 既有口径）+ **S2 四臂**（gzip
roundtrip 6.25× + pos/time 帧锁定保持 / **遗留零迁移最强形式 =
savenorm 新旧 fresh-60f 恰 `.format: 28 vs 29` 单键** + faithful load
逐计数 / none 明文 + savenorm 恒等 / corrupt 512B 截断 exit=1 点名
不崩溃）+ S6 点名 3/3 + 性能 A/B 同世界同成本（IO 增量口径：save
写 gzip 1.2 ms / load inflate 0.3 ms @ 4 KB 档，门档 2.7 MB 级外推
≈ 0.5 MB 落盘）。实测压缩率：门档 4237 B → 677 B = **6.3×**（立项
前大档 5.4× = 4 万块 journal 1.74 MB → 0.32 MB）。2503/2503（+21：
test_save_gzip 19 + save_slot 2；既有 12 处 format 字面量钉 28→29
有意识更新，逻辑零改动）。PITS 385（① zlib 封装字节漂移 = 手写容器
② 本机 zlib 1.3.2 缺 Z_DEFAULT_MEM_LEVEL 宏 = 命名常量 8 ③ 双跑 RAW
恒等适用域 = 无火场景）。详见 `docs/archive/phase-3.1.0.md`。

## v3.0.0（2026-11-04）— 3.x「世界扩展」期开启（原 4.x 命名重编号；纯文档 + banner，零玩法变更）

2.x 全史收官（v2.19.0，两批 20 版）后的时代开启版：**① 重编号定案
（11-04 同日用户定案）**——「世界扩展」期任务表由原 4.x 命名改
**3.x**（2.x 已吸收原 3.x「系统扩展」八项 = V2-11~V2-18，「世界扩
展」回归自然序号）：任务号 V4-1~V4-7 → **V3-1~V3-7**（V3-1 下界
维度锚 / V3-2 下界内容族 / V3-3 下界 mob / V3-4 netherite 材料链 /
V3-5 建筑内容族 / V3-6 世界边缘雾 / V3-7 存档 gzip 压缩【11-04 用户
实机诉求，默认开 = format 29，先执 = **v3.1.0**】），执行权威 =
`docs/BACKLOG.md`（3.x 任务表）；**② README「版本与发布」精简为四里
程碑**（v0.1.0 / v1.0.0 / v2.0.0 / v3.0.0，逐版明细指向 CHANGELOG
时代分档）；**③ 活文档重编号刷新**（AGENTS/BACKLOG/CHANGELOG 时代
切换）。**零玩法/零 format/零像素变更**：banner v2.19.0 → v3.0.0
（PITS 355 三件套：CMakeLists PROJECT_VERSION 单点 + test_debug_
overlay F3 钉 + banner 实证）+ 全套测试复跑 + 无输入 base18 结构免
疫（banner 仅 F3 调试覆盖层 + 启动日志消费，截图零触碰）。下一版
**v3.1.0 = V3-7 存档 gzip 压缩**（用户开新会话后执行），其后 V3-1
下界维度拆分立项。
