# 版本历史（CHANGELOG）

> 逐版细节（机制/证据/偏离/性能 A/B）见 `docs/archive/phase-*.md` 阶段存档与
> GitHub Releases。README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本）。

## v1.38.0（2026-10-30）— **tag v1.38.0 = UX-3+4 合并 tag**

### UX-4 第三人称/拉远相机（UX 行 4/7，门 158 全 PASS）

C 键切换第一/第三人称（MC 现代相机键；F5 已被手动存档占用 = 记档
偏离）——第三人称 = 相机**眼位后上方球面偏移 4.0 块**（offset =
−Camera::forward×D，与视图矩阵同一三角表达式位级同源）+ **遮挡收敛**
（嵌墙 = 解析穿出再留 0.1 间隙玩家侧【门 S2b 实证修正：初版 t−margin
滞留墙内】/ 远墙 = raycastBlock 面前 0.1 停 / 穿流体 = MC 同款）。
**零状态**（相机每帧派生，无新 Game 成员 = 零 format（22）/不入档，
读档回 First = MC 同款）+ 第一人称（默认）位级恒等 = 结构免疫根 +
drawHeldHand 第三人称隐藏（2D 手持角标 = HUD 层照画；交互/瞄准/箭/
声音零改动，第三人称瞄准自相机 = 屏幕中心与准星一致）。1946/1946
（+8：offset 轴钉/嵌入穿出 3.9 S2b 回归/穿流体/forward 同源守护）、
-Werror 0、banner v1.38.0。门 158：S1 **第 158 次结构性免疫 + base18
锚连续 168140 B** + S2 输入跑 12/12【B×2/C×2 RAW 恒等 + 皮肤判别
A=0/B=1697/C=1394 视差单调 + 遮挡出墙 + save A≡B（模式不入档）/C 差
恰 8 墙格 + 三侧 player 零漂移】+ 性能 A/B v1.37 vs v1.38 fall 900f
全指标同档 + changesApplied 9/3/1 逐位 + 终档 fluids 18/18 + blocks
55/55 集合恒等。PITS 344（xdotool search --pid 须真实进程 PID）。
**tag v1.38.0 = UX-3+4 合并 tag**（UX 行政策第 2 个兑现）。UX-3 明细
见 v1.37.0 条目。详见 docs/archive/phase-ux4.md。

## v1.37.0（2026-10-30）— UX-3 玩家身体模型（UX 行 3/7，门 157 全 PASS）

玩家世界空间身体 = **MC 标准 5 部件 box 模型**（头 0.5³ / 躯干
0.5×0.55×0.25 / 四肢 0.25×0.75×0.25，总高 1.8）+ **行走摆臂摆腿**
（四部件对角步态 arm_l+leg_r / arm_r+leg_l，amplitude leg 0.35 / arm
0.3 = 64 相位 checkSwingClip 全过）。**全管线复用 mob box 模型系统**
（`MobRegistry::parseModel` 加 label 默认参 / `makeMobModelGeometry` /
ENT-1 `advanceSwingPhase` 相位机镜像进 `Player::update` /
`checkSwingClip`），零新渲染机制。纹理 = **生成式占位皮肤**（用户
定案：64×64 经典布局官方 wiki 4px-arm 模板逐区解码验证 + 4 派生
16×16 部件 tile 尾部追加图集 [10..13,4] 免重锚；
`tools/generate_player_skin.py` 确定性幂等，真实皮肤同布局一键替换）。
数据 = 独立 `assets/data/player_body.json`（只含 model 段，无假 AI
字段；模型 width 1.0 = box 验证界非碰撞，玩家 AABB 0.6×1.8 零触碰）。
渲染 = `drawPlayerBody`（opaque 段 drawMobs 后，lazy build 门关时永不
build，零 per-draw uniform，烘焙恒亮 = mob 同款记档偏离）+
`CameraMode { First, ThirdBack }` 门 **UX-3 恒 First = 身体不可见**
（第三人称 = UX-4 消费；手持 viewmodel 零改动）。相位 render-only
不入档 = **零 format（22）/ 零 config / 零音效**。1938/1938（+9：
PlayerBodyTest 6【模型钉/tile 布局/64 相位 NoClip/非真空/解析点名/
皮肤 FNV 5 件 + 15 旧 tile 零变化】+ PlayerSwingPhaseTest 3【行走
推进/滑行续摆+衰减/无输入惰性】）、`-Werror` 0、banner v1.37.0。
门 157：S1 **第 157 次结构性免疫 + base18 锚连续 168140 B 免重锚**
（fresh 60f 四方字节一致 + 跨版 savenorm EXACT；三重惰性 = 门恒关 +
相位恒 0 + 图集追加零移位）+ S6 init 点名 ×3（box 出界 / 未知 part /
amplitude 0 全 "player body model" 精确）+ S3 1938 + 性能 A/B v1.36
vs v1.37 fall 900f 全指标同档 + changesApplied 9/3/1 三窗逐位 + 终档
fluids 18/18 + blocks 55/55 集合恒等 = 同世界同成本【结构惰性最强
形】。偏离：占位皮肤纯色分区 / 头部 cap 与躯干侧面 tile 不表征 /
模型烘焙恒亮（mob 同款）/ crouch 不缩放（记档）。本版不打 tag（**tag
v1.38.0 = UX-3+4 合并**）。详见 docs/archive/phase-ux3.md。

## v1.36.0（2026-10-30）— **tag v1.36.0 = UX-1+2 合并 tag（UX 政策首个兑现）**

### UX-2 Game 拆分/装配层收敛（UX 行 2/7，门 151-156 全 PASS）

**A 段（纯文件拆分，零结构变更）**：8776 行 game.cpp 单 TU → **8 个
game_*.cpp**（A1：game_init 13 法 / game_streaming 10 法 / game_save 5
法 + game_local.hpp 私有共享头；A2：game_frame 7 / game_interaction 26
【右键链 12 级顺序 = 所有权随迁】/ game_entities 12 / game_render 9 /
game_ui 23；game.cpp 退役删除；game.hpp 零触碰）。方法体 verbatim
（编排者独立抽验 5 最长法整法 diff 零差异 + 全签名在位 + 纯删确认）。

**B 段（域状态 struct 收拢 = 装配点显式化，路线 A+B 用户定案）**：
B1 per-block 23 成员 → PerBlockStates m_states（setBlock 9 项簿记 + 五
消费者站点全显式化，146 处改写）/ B2 流送 14 成员 → StreamingState
m_streaming（零初始化重排，115 处）/ B3 mob 11 成员 → MobSystem m_mobs
（零重排，51 处）/ B4 交互 11 + 面板 8 → InteractionState m_interaction
+ PanelState m_panel（安全重排，序映射表论证：全默认构造零跨读 + 红线
items<inventory<recipes<UI 三构造序保持）。每 struct 族内相对序逐字
保持 = 构造序 = 初始化序 = 行为面红线；4 跨域簿记留平铺记档。
**痛点直接缓解**：新 per-block 系统 = 域内 1 成员 + 5 消费点显式
m_states.xxx（前 = 9 项簿记散落 + 隐式耦合）。

1929/1929（全子相零测试改动 = 拆分零测试面影响实证，192 套件全纯函数
级）、-Werror 0、banner v1.36.0。门 151-156 全 PASS（子相协议 = 无像素
门）：每子相 new ×2 + **对 v1.35.0 冻结基线（tm_v136_base）字节一致 +
base18 锚连续 168140 B + 跨版 savenorm EXACT**；B 段每子相加读档往返
全状态恒等（applySave/rescan* 经新 struct 行为零差证明）；A 段传递链
闭合（a2n1 ≡ a1n1 ≡ a1o1 ≡ 锚 全 EXACT = 终态二进制对拆分前位级恒等）。
性能 A/B v1.35.0 vs v1.36.0 fall 900f 三窗全指标同档 + changesApplied 9/3/1 逐位 + 终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 = 同世界同成本【纯重构最强形】；补丁器 g139_instrument v2 双模式适配 8-TU 布局（legacy 输出字节一致实证，PITS 343 机械改写协议）。C 方案（子系统
对象化）记档矩阵外。详见 docs/archive/phase-ux2.md。

**tag v1.36.0 = UX-1+2 合并 tag**（10-30 用户定案 UX 行政策兑现：两两
合并 tag v1.36/1.38/1.40/1.41 收官 + UX 系统 release 于 UX-7 收官，
notes 覆盖 v1.35-1.41）。UX-1 夜变暗程度明细见 v1.35.0 条目。

## v1.35.0（2026-10-30）— UX-1 夜变暗程度（UX 行 1/7，门 150 全 PASS）

**UX-1**（v1.x 任务表 34/40，10-30 用户定案方向）：消费 LGT-3 定案「10-27
深夜须可夜视」（初版深夜全黑被实机反馈推翻 = `kNightSkyLevel 7` 的由来）
→ **深夜天空地板可配置，可设全黑夜**：config `render.night_sky_level`
（int [0,15]，**代码默认 7 = 对 v1.34.0 位级恒等**；严格读：absent = 7 /
非 number / 非有限 / 越界 / 非整数全点名拒，检查序 = 非有限 → 范围
（double 域）→ 整数 → int 投影【1e18 int cast = UB 路径，编排者核证
封堵】= SAVE-1 纪律，config.json 零改动）——shader 两编译期常量
（`kNightRetreat 8.0/15.0` / `kAmbient 0.03`）→ **两 uniform**
（`u_nightRetreat = (15−L)/15` / `u_ambient`，表达式树一字未动，init
bind→set→unbind 一次性注入 = 每 run 常量无 per-draw 变化 = UXFIX-1
泄漏类结构不适用）+ **全黑夜 = L 0 且环境底随地板归零**（`kAmbientFloor
0.03` 单一真源，L=0 → 0.0 = 真纯黑无残光）+ 主机纯函数尾参参数化
（`effectiveSkyLight(F)` / `currentSpawnLight` / `findMobSpawnCell`，
默认参 = 旧常量 = 既有调用点位级不变）+ **spawn 成对语义任意 L 成立**
（夜露天等级 = L 本身：L≤7 敌对照生 / 火把圈 14 恒免疫 / L=15 夜不生成
= 用户配置选择，文档/测试同步）；仿射撤退其它消费者（信标光束 LGT-4 /
掉落 PRE-6 / 云 / 手持块立方）经 shader 公式自动继承零代码。零 format
（22）/内容/音效/帧状态。1929/1929（+16：config 严格读 11【含 1e18 大
有限值防御】+ retreat scale 位级钉 + ambient 规则 + effectiveSkyLight
配置地板【含 16×16 正午恒等网格】+ F 版位级钉 + spawn 镜像 4 值）、
`-Werror` 0、banner v1.35.0。门 150 全 PASS：S1 = **第 150 次结构性
免疫 + base18 锚连续 168140 B 免重锚**（fresh 60f 四方 RAW 全字节一致
+ 跨版 savenorm EXACT）；S2 深夜 0.75 无输入 600f 多向 9 跑：
**old v1.34.0 ≡ new-L7 字节一致 160959 B = 代码级免疫（深夜帧亦位级
恒等）** + old 忽略未知键 = forward-compat + **L0 vs L7 diff 378,350 px
逐像素分量全仅暗化（违反 0）/ L15 vs L7 同数 378,350 px 全仅亮化（违反
0）= 时间不变集**【LGT-3 门 118 同型特征证明】+ 六存档 savenorm 全对
EXACT（光照不入档）+ 全黑夜帧 22453 B ≈ 纯黑实证；S6 init 点名 ×3；S3
1929；性能 A/B v1.34.0 vs v1.35.0 fall 900f 三窗全指标同档 +
changesApplied 9/3/1 逐位 + **终档 fluids 18/18 + blocks 55/55 集合恒等
= 同世界同成本**【结构惰性最强形】。PITS 341（门脚本截图相对路径静默
落游戏 CWD = 全门假 FAIL）+ 342（门批前查活跃游戏进程 + 日志时钟 NTP
回拨非单调）+ 270 第 3 次复现（rsync 无锚 --exclude data 吞
assets/data）。**UX 行 tag/release 政策（10-30 用户定案，同款 10-28）**：
两两合并 tag（v1.36.0 = UX-1+2 / v1.38.0 = UX-3+4 / v1.40.0 = UX-5+6 /
v1.41.0 = UX-7 收官）+ UX 系统 release 收官时发（notes 覆盖
v1.35-1.41）——本版不打 tag。详见 `docs/archive/phase-ux1.md`。

## v1.34.0（2026-10-30）— 插队修复 ×3（矩阵 UX 行开工前清积压）

**④ 物品解析白名单补 `saturation`**：门 141 内发现的 10 行 init WARN
噪音（10 个 SURV-9 食物 JSON 的 `saturation` 字段在 item_registry
顶层 unknown-field 白名单缺名）→ 白名单加一行，解析路径零改动。门
149 S2b 实证：v1.33.0 init 恰 10 行 / v1.34.0 = 0 行。

**③ 性能插桩器入 `tools/`**：`tools/g139_instrument.py`（g139 族门
脚本的 perf 树 27 标记补丁器；锚点结构性断言 = game.cpp 结构变化时
loud fail，PITS 317 纪律——perf 树可从 repo+commit+本工具重建，/tmp
重启不丢）。门 149 性能 A/B 双侧同打 = 入 tools/ 即实战首验。

**炉子配方（内容缺口修复）**：Phase 40 起炉子系统完整可用但 shipped
配方零 furnace + 初始库存无 = **正常游玩拿不到炉子**（10-30
GAMEPLAY.md 刷新时发现，KNOWN_ISSUES 落档）→ shaped 配方
`8 cobblestone 围圈 → 1 furnace`（MC 1.14+ 摆法，纯内容零核心代码；
锻石 = Phase 61 挖石头掉落 = 石头→锻石→炉子闭环；隐藏 starter 槽不
给 = 配方即获取途径）。内容钉死单测含真实匹配器正/负例。配方
65→66（shaped 33→34）。

1913/1913（+1 炉子钉死）、`-Werror` 0、banner v1.34.0。门 149 全
PASS：S1 第 149 次结构性免疫 + base18 锚连续 168140 B 免重锚 + 跨版
savenorm EXACT；S2b WARN 10→0；S2 炉子合成 A/B 6/6（NEW 侧 full
19 点击 E2E：×2 RAW + ×2 savenorm + 终态 slot0 furnace ×1 = shaped
围圈命中实证 / OLD 侧 minimal 7 点击竞态无关断言：无 furnace/无
stone/8 锻石守恒 + ×2 RAW + ×2 savenorm；**双二进制 CPU harness
逻辑层终态互斥**：同 19 点击 NEW = furnace ×1 vs OLD = stone ×2
【v1.33.0 无 furnace 配方，满围圈也只 shapeless 石头双合成】）；S3
1913；性能 A/B v1.33.0 vs v1.34.0 PASS（banner 双侧 + 全指标同档 +
changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55
集合恒等 = 同世界同成本）。PITS 338（面板点击落位验证协议）+ 339
（场景容器块须携状态段）+ 340（长输入序列浮动窗 + 强制几何）。详见
`docs/archive/phase-fixes-v134.md`。

## v1.33.0（2026-10-30）— **tag v1.33.0 = PRE-11+12 合并 tag + v1.x 任务矩阵 33/33 全完成 + PRE 系统 release**

**PRE-12 tooltip/F3**（v1.x 任务表 33/33 = v1.x 收官，门 148，PRE 行 6/6）：
原始定义已佚 = 定案 **悬停 tooltip 补齐 + F3 呈现框架**（消费三处记档
边界：Phase-15"物品 tooltip 只有名字"、SURV-6 档位"vs F3/tooltip
显示"、SURV-7 统计"显示 = PRE-12 呈现框架"）：① 物品 tooltip =
`name`/`name xN`（count>1）+ 工具第二行 `damage/max`（纯函数
`ui/item_tooltip`，campfire 范式 5 单测；**tooltip 渲染层修复** = 门内
发现 Phase-15 潜伏缺陷：旧文本与面板共享 kText batch【绘制先于槽底/
图标】+ 偏移随面板 fit scale 收缩【679×744 窗 s≈0.286 文本 4.6px】=
悬停 tooltip 恒被覆盖不可见 → 独立 batch 移至 3D 块立方图标后绘制，
无悬停零 batch 无输入基线不变；记档偏离 = 效果图标 hover tooltip 结构
不可行【locked 光标固定中心】档位/时长呈现 = F3 通道）；② F3 两新面：
效果行带档位罗马（`effectTierRoman`：tier 0 = "I" MC amplifier 惯例，
越界钳制）+ **stats 行**（`stats deathCount N | kill.<name> N` 键序，
空 = "(none)" 基线）；无描述/附魔文本（项目无附魔系统 = 记档偏离，MC
同款）。零 format（22）/config/内容/音效/帧状态。1912/1912（+7：
tooltip 5 + stats 行 2 + effectTierRoman 钳制）、`-Werror` 0、banner
v1.33.0；门 148（全 PASS 25/25）：S1 第 148 次结构性免疫 + base18 锚
连续 168140 B 免重锚 + 跨版 savenorm EXACT；S2 四输入矩阵 1362×744
全零漂移【S2a F3 stats 行 diff 4762 px 限 F3 盒带外 0 + 存档 stats
精确双侧 / S2b F3 效果罗马 diff 4761 px 限盒 + 效果名 5/5 / S2c
tooltip "dirt x64" x2 RAW ×2 侧 + 跨侧 savenorm EXACT + diff 66 px 限
tooltip 盒带外 0 / S2c2 tooltip "wooden_pickaxe"+"0.0/59" 同断言
diff 78 px 限盒】——OLD 侧无可见 tooltip = Phase-15 潜伏覆盖缺陷的
判别实证；S3 1912；
性能 A/B 真 v1.32 vs v1.33-dev PASS（banner 双侧 + 全指标同档 +
changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55
集合恒等 = 同世界同成本【结构惰性最强形：无 F3/面板/悬停 = PRE-12 零
执行】）。详见 `docs/archive/phase-pre12.md`。

**tag v1.33.0 = PRE-11+12 合并 tag**（PRE 政策兑现：v1.32.0 组内未打，
本 tag 覆盖 PRE-11 心行窄窗回退 + PRE-12 tooltip/F3）= **v1.x 任务矩阵
33/33 全完成 + PRE 系统收官 release**。

## v1.32.0（2026-10-30）

**PRE-11 HUD 细化**（v1.x 任务表 32/33，门 147，PRE 行 5/6）：原始定义
已佚 = 定案 **心行窄窗回退布局**（消费 KNOWN_ISSUES UI 节 Phase-23 条
"饥饿 HUD 在极窄窗口会被裁剪——越界即裁，无回退布局"）：纯函数
`core/ui_layout.heartRowLayout`（新文件，campfire 范式 8 单测）——整行
（[吸收带][心][6px][饥饿|氧气]）超出右缘时左移直至**右排右缘贴屏**
（吸收带最先被裁 = 记档 last resort，低于最小行宽心也裁 = 记档极限）；
**宽窗返回 legacy 坐标位级精确**（无输入基线 + E2E 窗零触碰 = 结构免疫
根）；右排宽度按本帧实渲染排预留（氧气替换饥饿 = SURV-2 原位语义）；
顺带 UI 度量常量（16/2/6）收口 ui_layout.hpp 单一真源（drawUI 别名）。
零 format（22）/config/内容/音效/帧状态。1905/1905（+8：宽窗 legacy
位级/池锚/贴合边界/1px 过界移 1/窄窗右排贴屏/带池吸收先裁/sub-row 心
也裁/零宽守卫）、`-Werror` 0；门 147：S1 第 147 次结构性免疫 +
**base18 锚刷新 387726→168140 B**（WM 平铺态 1362×744→679×744，
PITS 265/315 类环境态，四方 clean 后采用）+ 跨版 savenorm EXACT；S2
宽窗 capture A/B 10/10 PASS（N/M × 新旧 ×2 = 8 跑全零漂移：存档证据
4/4 + x2 RAW 4 对 + **new_N≡old_N / new_M≡old_M 字节同 = 代码级免疫**
【三行 HUD 全覆盖】；半态判别 = 8 布局单测位级钉死，本门矩阵落宽态）
+ PITS 333-336【336 = 高负载取景间歇纯黑读回 → 场景相机移地表】；S3 1905；性能 A/B 真 v1.31 vs v1.32-dev
PASS（banner 双侧 + 全指标同档 + changesApplied 9/3/1 三窗逐位 + 终档
fluids 18/18 + blocks 55/55 集合恒等 = 同世界同成本【结构惰性最强形：
无 capture = drawUI 心行段零执行】）。v1.32.0 不打 tag（PRE 政策：
tag v1.33.0 = PRE-11+12 合并，PRE-12 收官时打 + PRE 系统 release）。
详见 `docs/archive/phase-pre11.md`。

## v1.31.0（2026-10-30）— **tag v1.31.0 = PRE-9+10 合并 tag**

**PRE-10 图标重叠修复**（v1.x 任务表 31/33，门 146，PRE 行 4/6，
**矩阵唯一 bug 级项**）：原始定义 = 砍掉清单回填项"图标重叠修复★"
（消费 KNOWN_ISSUES「效果图标行与吸收红心可视觉重叠」Phase-56 条：
效果图标行与心行同行，图标多时画到吸收红心区——现状几何 w=1362 下
5 个效果触发，shipped 内容可达 5-6）= 定案 **MC 三行 HUD 堆栈**（行距
kIconPx+kStripGap = 22，每行独占 = 任意图标数零碰撞）：心行（hy，零
改动）/ **效果行（hy−22，新专属行，左锚 = 心行最左缘 = 池在时吸收带
左缘，MC 布局）**/ **护甲行（hy−44，SURV-1 护甲槽上移一行 = MC 护甲
在效果行之上）**。纯布局常量（game.cpp drawUI 两段），零 format
（22）/config/内容/音效/帧状态；HUD 仅指针锁定时绘制 = 无输入结构
免疫根。1897/1897（零新测 = 布局 GL 路径内不可单测，门像素断言钉死，
PRE-7 先例）、`-Werror` 0；门 146：S1 第 146 次结构性免疫 + base18
锚连续 387726 B 免重锚 + 跨版 savenorm EXACT；S2 HUD capture 矩阵
PASS（N/A/M × 新旧 ×2 = 12 跑，原子中心点击协议 + 尺寸门【PITS 331】
+ 漂移门 <0.001 rad【PITS 332】，12 跑全零漂移 + 存档证据 6/6 +
x2 RAW 6 对字节同 + **像素断言 7/7**【DPI 1.5× 缩放自探测 PITS 330：
bare HUD old_N≡new_N 字节同 = 代码级免疫 / **OLD 吸收带 bug footprint
45 px** / **NEW 吸收带 diff = 0** = 修复实证 / NEW 效果行 288 px /
HUD 带外 0 diff = 世界零回归 / 护甲行移位双向各 624 px】）；S3 1897；
性能 A/B 真 v1.30 vs v1.31-dev PASS（全指标同档 + changesApplied
9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55 集合恒等 = 同世界
同成本【结构惰性最强形：无 capture = 零 HUD 绘制】）。**PITS 330**
（HUD 像素断言须按 DPI 缩放换算：布局虚拟 908×496 vs 截图物理
1362×744 = 1.5×，黄选中框自探测 s）/ **331**（门内 WM 重排破 A/B
矩阵尺寸一致性 → capture 跑尺寸门）/ **332**（capture 跑 x2 RAW 漂移
门须 <0.001 rad）。**tag v1.31.0 = PRE-9+10 合并 tag**（PRE 政策；
release 于 PRE-12 收官，notes 将覆盖 v1.30-1.31）。详见
`docs/archive/phase-pre9.md` + `docs/archive/phase-pre10.md`。

## v1.30.0（2026-10-30）— 不打 tag（PRE 政策：tag v1.31.0 = PRE-9+10 合并）

**PRE-9 箭/弓/手视觉**（v1.x 任务表 30/33，门 145，PRE 行 3/6）——原始
定义已佚 = 定案 **箭视觉（MC 式三段箭形：棕色杆 + 灰色锥形箭头 + 白色
尾羽）**（消费 KNOWN_ISSUES「弓 / 箭」节 Phase-21 条"箭无纹理 / 无箭头 /
无尾羽——单一棕色细盒"）。弓（3D model + 拉弓指示，CRAFT-2/Phase-42）
与手（3D 手持 model，CRAFT-2/ENT-9）已大体解决，本项聚焦最明确缺口 =
箭几何：`makeArrowGeometry` 纯函数返回三段（棕杆盒 0.31 + 灰四棱锥箭
头 0.12【尖 @ 箭前端 -Z】+ 白宽尾羽盒 0.09 > 杆 0.05，总长 0.55 = 原
轮廓尺寸不变）+ `drawArrows` 每箭三色三次纯色 draw（零 shader/纹理/
atlas 改动）+ 箭朝向/物理零改动（帧锁定 = 确定性）。零 format
（22）/config/内容/音效/世界状态。1897/1897（+3 净：4 旧几何测 → 7
新三段测）、`-Werror` 0；门 145：S1 第 145 次结构性免疫 + base18 锚
连续 387726 B 免重锚 + 跨版 savenorm EXACT；S2 箭 A/B f170 PASS
（skeleton-only 脚手架 0 伤无 wither → 箭飞行零红闪 hp 20 恒：NEW×2
RAW 497478 B 字节同 + OLD×2 497489 B 字节同 + **NEW-vs-OLD diff 2512
px 限 bbox x[661,700] y[53,134]** 箭区三段形 vs 棕盒 + savenorm
NEW==OLD 箭瞬态不入档 + 六跑全一次成功）；S3 首轮 1896/1897 =
资产脚手架未还原（内容钉死单测读真实资产 → **PITS 329**，还原后
1897）；性能 A/B 真 v1.29 vs v1.30-dev PASS（全指标同档 +
changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55
集合恒等 = 同世界同成本【结构惰性最强形】）。**PITS 328**（护甲减伤
floor 在乘积上非最终值：满钻甲 20 点不免疫箭伤 2 = 0.4 伤 + 红闪破
×2 RAW，零红闪场景须伤害源头为 0）/ **329**（门内 S3 与资产脚手架
互斥：S2 脚手架须 S3 前还原）。详见 `docs/archive/phase-pre9.md`。

## v1.29.0（2026-10-30）— **tag v1.29.0 = PRE-7+8 合并 tag**

**PRE-8 mob 火焰**（v1.x 任务表 29/33，PRE 行 2/6，门 144）——原始定义已佚
= 定案 **燃烧 mob 的余烬火焰**（消费 KNOWN_ISSUES「Mob 燃烧无火焰视觉」
Phase-58 条）：`Game::updateMobFlames` 每 36 帧（`kMobFlameEveryNFrames`，
CRAFT-5 周期）每燃烧 mob（`MobEntity::burning` = P57/P73 火系统状态）一批
上升余烬（P66 `spawnFire` 复用，专用盐 `kMobFlameRollSalt`）+ **自发光**
（block 光 1.0 深夜也亮 + 中心格天光环境项，与 CRAFT-5 炉口环境光余烬的
记档差异 = 炉口火探出 / mob 火裹身）+ 中心格 = `entityCenterCell` 纯函数
（AABB 中心 position+height/2 逐轴 floor 负坐标安全，从 PRE-6 `itemDropLight`
提取共享）+ `render.mob_flames` getBool 代码默认 true（config.json 零改动，
off = 对 v1.28.0 位级恒等 = 结构免疫根）。零 format（22）/config/内容/音效/
世界状态。1894/1894（+6：entityCenterCell 4 + config 2）、`-Werror` 0；门
144：S1 第 144 次结构性免疫 + base18 锚连续 387726 B 免重锚 + 跨版 savenorm
EXACT；S2 燃烧 mob 活粒子协议 PASS（场景 = 膜区抬平 y39 + 8 格火膜环 + 2 格
石柱 + 僵尸追踪 dy=2 零伤：OFF×2/OLD×2/**OFF≡OLD 字节同 = 代码级免疫** +
**ON×2 N=540 diff 371 px 限簇** = 活余烬 + savenorm 仅 1 格视图外熔岩湖
churn【PITS 244/283 类】+ 玩家 hp20）；性能 A/B 真 v1.28 vs v1.29-dev PASS
（全指标同档 + changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks
55/55 集合恒等 = 同世界同成本）。**PITS 324**（被动 mob 夜间不生成：需光
≥9 月光仅 7）/ **325**（敌对生成保护圈 12 = 场景须离出生点 >12）/ **326**
（抬高玩家 dy 双重约束：周边高地形破安全 → 膜区抬平）/ **327**（余烬截图
帧数须为 36 倍数，否则活余烬缺席 diff=0 假阴性）。详见
`docs/archive/phase-pre8.md`。

## v1.28.0（2026-10-29）

**PRE-7 中毒/燃烧表现**（v1.x 任务表 28/33，PRE 行 1/6，门 143）——原始
定义已佚 = 定案 **全屏效果 tint 通道（数据驱动）+ poison 紫色纱**（消费
KNOWN_ISSUES「中毒无独立紫色 tint」条 = P46 原文"需新的全屏 tint 通道"）：
效果 JSON 可选 `tint` [r,g,b,a]（严格解析；缺省全零 = 无纱零绘制 = 结构
免疫根；正 alpha = no-op 规则的"表现层动作"演进）+ poison [0.63,0.24,
0.86,0.35] = MC 中毒紫 + HUD pass 红纱前每活跃 tinted 效果一个全屏 quad
（`activeEffectTints` 纯函数；恒定 alpha = 脉动记档偏离）；燃烧半边界
= fire 无 tint（玩家燃烧 = 既有 0.2 s tick 红纱不变，mob 火焰 = PRE-8
独立项）。零 format（22）/config/音效。1888/1888（+6）、`-Werror` 0；
门 143：S1 第 143 次结构性免疫免重锚 + 跨版 savenorm EXACT；S2 三方
poison 纱 A/B（混合数学 A ≈ 0.65·B + 0.35·tint 红闪相位重试）；性能 A/B
同世界同成本。详见 `docs/archive/phase-pre7.md`。

## v1.27.0（2026-10-29）— **tag v1.27.0 = SAVE-2+3 合并 tag + SAVE 系统 release（notes 覆盖 v1.26-1.27）**

**SAVE-3 槽位管理**（v1.x 任务表 27/33，SAVE 行 2/2 = **SAVE 系统收官**，门
142）——原始定义已佚 = 定案 **CLI 槽位管理（headless 四操作）**（消费
KNOWN_ISSUES「多个槽只是多个文件，手动切换」+ SAVE-1 残余「无多槽轮转」
条）：`--list-saves`（名序确定性列表，摘要 = 纯文件内容【format/seed/昼时/
位置/血量】无 mtime = 重复运行逐字节同输出；损坏文件浮现 UNREADABLE +
点名）+ `--copy-save S D`（字节保真分支存档，D 已存在拒，原子写 = SAVE-1
同款失败零半写）+ `--rename-save S D`（POSIX 原子改名）+ `--delete-save S`；
槽名 = `--save` 字符集（天然杀路径穿越）；管理旗标独占（每跑一个、与游戏
旗标互斥 = rc 2 点名，管理跑不启动游戏）。零 format（22）/config/渲染/运行
期路径 = 普通跑结构惰性。1882/1882（+10）、`-Werror` 0；门 142：S1 第 142
次结构性免疫 + **跨版 save 字节一致**（零 format 变更）；S2 headless 管理
矩阵（list ×2 确定性 + 损坏浮现 + 全操作 rc/点名/字节保真 + 旧二进制拒旗
标）；性能 A/B 同世界同成本。详见 `docs/archive/phase-save3.md`。

## v1.26.0（2026-10-29）

**SAVE-2 速度入档**（v1.x 任务表 26/33，SAVE 行 1/2，门 141，按 10-28 政策
不打 tag——SAVE-3 → tag v1.27.0 = SAVE-2+3 合并 + SAVE 系统 release）——原始
定义已佚 = 定案 **玩家速度入档**（消费 KNOWN_ISSUES「玩家速度/onGround 不存」
速度半）：format 21→22 `player.velocity` 3 数组恒写（oxygen/stats 键纪律：22
必键 / ≤21 携 = 版本谎报点名；区间 ±100 `kSavedVelocityMax` 损坏档点名）+
读档 `restorePosition` 后 `setVelocity`（≤21 载入 = 零速 legacy 值保真免迁）+
onGround 不存（位置派生 = 记档偏离）。1872/1872（+6）、`-Werror` 0；门 141：
S1 第 141 次结构性免疫 + base18 锚刷新（WM 平铺态 PITS 265/315）+ **跨版 save
diff 恰 2 项**{format, +velocity}；S2 −Z 漂移终态判别（z 带 8.4-8.7 块互斥 +
zero/twin 整数平面全 exact + twin PNG ≡ zero PNG + 跨侧 player 段位级 = legacy
回归双证；初版中间帧设计被 PITS 320 推翻）；S2d 旧二进制拒 format 22 + S6 ×2
点名；性能 A/B 同世界同成本（changesApplied 9/3/1 逐位 + 终档集合 diff 0）。
门内发现：物品严格解析白名单漏 `saturation` = 10 行 init WARN 噪音（两侧恒同
结构无害，收录 KNOWN_ISSUES 待基础设施轮修）。详见
`docs/archive/phase-save2.md`。

## v1.25.0（2026-10-29）— **tag v1.25.0 = CRAFT-6+7 合并 tag + CRAFT 系统 release（notes 覆盖 v1.22-1.25）**

**CRAFT-7 背包交互**（v1.x 任务表 25/33，CRAFT 行 4/4 = **CRAFT 系统收官**，门 140）——原始
定义已佚 = 代理定案 **背包面板 MC 操作面（3/4 项 + 数字键）**（消费 KNOWN_ISSUES「背包 UI
右键语义极简」）：
- **右键取半堆**：空手右键占位格取 `floor(N/2)` 最小 1（N=1 取自身、N=2/3 取 1 = v1 行为
  边界、N=64 取 32；耐久随堆 SURV-3）。共享 `cellRightClick` 原语 = E 屏/工作台/炉子面板
  同语义。
- **shift 快捷移动**：E 屏 = hotbar(0..8)⇄storage(9..26) 互移（索引序、同物同耐久合并、
  堆可跨格分裂）+ 容器格 → storage 优先（MC 主背包优先）+ 手持堆 no-op；炉子面板 = 容器
  GUI 语义（炉槽 → 主背包、玩家背包侧 no-op、移 input 按 furnace.hpp 不变量重置进度）；
  左右 Shift 皆可（RSHIFT → LeftShift 映射）。
- **双击收集分拣**：MC 分区语义（hotbar 与主背包为独立分区——首轮单分区实现被单测揪出
  跨区 = 修正）；第一击拾堆 + 0.3 s 真实 dt 窗内第二击同格 = 收集分区内同物同耐久堆入
  点击格（满堆上限、溢出索引序回填）；游戏层检测（kDoubleClickSeconds 0.3 命名常量，
  生存计时器类 = 输入态不入帧状态，无输入跑永不触碰）。
- **顶排数字键 1-9**：直接选热栏槽（MC；Tab 循环保留；面板开/关皆可 = 全局键段
  handleUiKeys——首轮误置 locked-world 段被 E2E 8/8 实证【面板开时 !locked 早退】，移入后
  4/4 i=1 首试）。
- **无拖拽**（记录偏离：三点击 + 数字键覆盖核心操作面；拖拽 = 指针捕获下轨迹跟踪 =
  最高风险 E2E 类，矩阵外）。

零 format（21）/config/新数据字段。1866/1866（+14：input 映射演进 10 / inventory_ui 9 /
furnace 4）、`-Werror` 0 警告。门 140：S1 = **第 140 次结构性免疫免重锚**（fresh 60f 四方
×2 168140 B 全 EXACT + base18 锚字节一致 + savenorm 同侧/跨侧；panelOpen 门控惰性 + 数字
键未按下 = 双结构惰性）+ S2 输入跑 **16/16 PASS**（firefox 显隐 PITS 314 + 捕获轮询
"Pointer captured" 协议 + probe140 纯布局函数算绝对槽位坐标 + 720f：r1 shift-move NEW
slot9=64+slot10=30 分裂 vs OLD 拾堆 + 关面板丢弃（9=30 原样）/ r2 取半 NEW dirt5 vs OLD
dirt9 / r3 双击 NEW slot13=8+slot17 空 vs OLD 拾回原样（13=3+17=5）/ r4 数字键 NEW
selected=4 vs OLD selected=0 = **四操作存档终态全互斥** + 同侧 ×2 savenorm EXACT ×8 对）+
S3 1866 + 无 S6（零 parse 面）+ **PITS 319**（捕获点击 grab delta 相机漂移在本 WM 平铺态
系统性偏大——冒烟 8/8 次 yaw −0.255/pitch −1.09 但操作终态 8/8 正确 → 存档终态 = 唯一权威、
相机只记录不门控；漂移时大时小 = 捕获瞬间 X 异步竞态）+ 性能 A/B 真 v1.24 vs v1.25 PASS
（banner 实证 + 同款 g139 插桩补丁器两侧同打；PERF-WIN 全指标同档【upd med 0.379/0.394/
0.394 vs 0.366/0.365/0.439】+ PIPE3-S0 tickMs 同档 + **changesApplied max 9/3/1 三窗逐位
一致** + **终档 fluids 18/18 + blocks 55/55 集合恒等 = 同世界同成本**【无面板结构惰性最强
形】）。

**CRAFT 系统收官（release 政策：用户 10-28 定案）**：v1.22 CRAFT-4 配方提示 / v1.23 CRAFT-5
炉子表现（tag v1.23.0 合并 CRAFT-4+5）/ v1.24 CRAFT-6 燃料熔炼 / v1.25 CRAFT-7 背包交互
（tag v1.25.0 合并 CRAFT-6+7）→ GitHub release "TokenMine.cpp v1.25.0" notes 覆盖 v1.22-
1.25。

## v1.24.0（2026-10-29）

**CRAFT-6 燃料熔炼**（v1.x 任务表 24/33，CRAFT 行 3/4，门 139）——原始定义已佚 =
代理定案 **block 物品的炉子字段 = block 属性继承**（消费 KNOWN_ISSUES Phase-40「可
熔炼/燃料名单小且 wood 不可当燃料」）：
- **技术路线**：`buildBlockItems` 合成 block 物品且显式同名物品文件被注册表拒绝
（"block items are defined by their block"）= 物品侧无文件可挂字段 → 新增 block
可选属性 `fuel_time`（(0,1000]，显式 0 点名拒 = item 侧同款纪律）+ `smeltable_to`
（显式空串点名拒 = ammo 纪律；产物存在性 = 既有 game init 全物品遍历自动覆盖
block 物品，**零新校验代码**）→ 继承进合成 item。炉子 tick 零改动（读 item 字段
从不读 block id = 硬规则 3；非燃料 burn 0 永不消耗 = 既有语义）。
- **内容（3 项，MC 语义，零新资产）**：`wood` 块 + `fuel_time: 20`（项目木族刻度：
planks 7.5 / stick 5，coal 80 = MC 精确锚；20 s ≈ 2.7× 单 planks = MC log-vs-plank
比值方向）+ `sand` 块 + `smeltable_to: "glass"`（MC 10 s 缺省）+ `lava_bucket`
物品 + `fuel_time: 1000`（MC 精确 = 区间上限）。

零 format（21）/config/新资产/音效。1852/1852（+5：解析 2 / 内容钉 1 / 继承 + 真
tick 2）、`-Werror` 0 警告。门 139：S1 = **第 139 次结构性免疫**（fresh 60f 四方
×2 168140 B 全 EXACT + savenorm 同侧/跨侧；furnace 永不 worldgen = m_furnaces 空
双结构惰性 + 属性纯数据零渲染变更；**base18 锚随服务器重启 /tmp 清空丢失 = 四方
字节一致后刷新采用**【与丢失前同尺寸同 WM 态，PITS 265/315/317 先例】）+ S2 炉
场景无输入跑 PASS（input sand×1 + fuel wood×1 预置，900f：NEW frame 1 点火【wood
消耗 →1200】/ frame 600 熔炼完成【glass×1】/ 900f fuel_substeps 301【点火帧授予不
消耗 = 1200−899，冒烟精确实证】仍燃 = P66 活粒子类【new×2 RAW 差 1453 px 100% 含于
火焰带 + 炉子态 ×2 逐位一致】vs OLD wood 非燃料恒保留 = 恒无火无熔炼【old×2 RAW
235855 B EXACT + 日志 0 行】= **新旧终态互斥（玻璃在产 vs 原料恒存）** + new vs old
4337 px bbox 130×150 紧凑火焰簇 + savenorm 仅熔岩湖 churn【blocks 集合恒等 +
fluids 1-5 格 @ ~100 块视图外 = PITS 244/283 类】）+ S3 1852 + S6 ×3 点名 PASS
【fuel_time 显式 0 / smeltable_to 空串 / "glassz" 未知产物 = game init 交叉校验
覆盖 block 物品实证】+ 性能 A/B 真 v1.23 vs v1.24 PASS（banner 实证 + 同款 g139
插桩补丁器两侧同打；PERF-WIN 全指标同档 + changesApplied max 9/3/1 三窗逐位 +
**终档 fluids 18/18 + blocks 55/55 集合恒等 = 同世界同成本**【结构惰性最强形】）。
**PITS 317**（服务器重启清空 /tmp = 全部门资产丢失 → 重建清单：xenv / g117 对比器
/ configs / 参照树【git archive + 同款补丁器】/ 锚刷新采用 / 冻结 seed 可换
`--new --seed`【世界生成跨二进制确定性 = 核心不变量】；教训 = 门协议资产必须可从
仓库 + commit 重建）。发布政策（用户 10-28 定案）：**v1.24.0 不打 tag**；
**v1.25.0 = CRAFT-6+7 合并 tag + CRAFT 系统 release**（notes 覆盖 v1.22-1.25）。

## v1.23.0（2026-10-29）— **tag v1.23.0 = CRAFT-4+5 合并 tag（新政策首个）**

**CRAFT-5 炉子表现**（v1.x 任务表 23/33，CRAFT 行 2/4，门 138）——原始定义已佚 =
代理定案 **炉子的两个 MC 表现提示**（消费 KNOWN_ISSUES Phase-40「无熔炼完成音效、
无火焰动画、无经验」）：
- **炉口火焰粒子**：燃烧中炉子（`fuelSubsteps > 0`）每 `kFurnaceParticleEveryNFrames
  = 36` 帧（0.6 s）自炉块**上方一格**发射 P66 `spawnFire` 余烬批（2 粒、负重力、
  0.3-0.5 s 寿命、火纹理），专用盐 `kFurnaceParticleRollSalt`（与火余烬流永不互追）；
  周期 36 = 寿命 2× + 裕量（初版 30 帧撞 60 fps 寿命边界 = 同侧 RAW 被 125 px 残留
  余烬破实证 → PITS 316）；发射门 `(m_frameCount + 1) % 36`（帧计数器 update/render
  后自增：900 帧跑末帧见 899、900%36=0 → 截图恒携 age-0 批）；**粒子光读上方空气格**
  （炉格 opaque 且无 light_emission = 读炉格烘出黑粒子 diff 0 实证）；
  `render.furnace_flames` getBool 默认 true，**off = 对 v1.22.0 位级恒等**。
- **熔炼完成音效 "smelt"**：新 wav（generate_sounds.py 1320/660 Hz 双泛音钟 + LCG
  攻击 tick，专用种子 0x51DE；**既有 38 wav 字节零变化 md5 实证 + 重跑幂等**）+
  完成帧空间化于炉块中心（CRAFT-3 `play(name, origin)` 范式）+ 帧锁定 info 日志
  `Furnace smelt complete: <product> at (x, y, z)`（E2E 证据；headless 音频降级
  play 静默但日志发 = PRE-4 先例）。
- **复燃"咔"声**：燃料 0→燃烧转移帧复用既有 "ignite" wav（P60）+ 日志。
- **事件检测 = 纯函数** `furnaceTickEvents(before, after)`（furnace.{hpp,cpp}，
  campfire 范式）：tick 保持无副作用（音效/日志归 game 循环，音频不入帧状态 = 硬
  规则 8）；`lit` = fuelSubsteps 0→>0（燃尽后复燃 = 同款转移）；`smeltCompleted`
  = 输出恰 +1 且同产物（快照同帧 tick 前取 = UI 变更不可能伪装）。
- **无经验**：项目无 XP 系统（矩阵外，记档偏离）。

零 format（21）/config.json/内容。1847/1847（+3：`FurnaceEventsTest` lit 三态 /
完成四例 / 真 tick 600 子步端到端）、`-Werror` 0 警告。门 138：S1 = **第 138 次
结构性免疫免重锚**（fresh 60f 四方 ×2 168140 B 679×744 锚连续 + savenorm 同侧/跨侧；
furnace 永不 worldgen = m_furnaces 空双结构惰性）+ S2 炉子场景无输入跑 PASS（表
(21,39,-4) + beef 预置 progress 580/600 + fuel 3000：frame 20 熔炼完成、900 帧仍
燃）：on×2 **savenorm EXACT** + RAW 差 4232 px **100% 含于火焰簇**（age-0 批的当帧
update 积一帧真实 dt 位移 = fps 依赖，P66 活粒子类【P54 条款】；簇外世界逐像素恒等）
+ off×2 RAW 235855 B EXACT + **off ≡ old（v1.22.0）RAW 字节一致 = 代码级免疫** +
on≡off savenorm EXACT（火焰不入档纯渲染证）+ off=old savenorm 仅熔岩湖 churn
（blocks 集合恒等 + fluids 5 格流前 @ ~100 块视图外 = PITS 244/283 类）+ **on vs
off 像素 4417 px bbox 130×150 紧凑簇**（上中画面、不触热栏带）+ 日志 new 侧恰 1 行
smelt（frame 20 帧锁定）/ old 侧 0 + 终态六跑全部 cooked_beef ×1 / fuel_substeps
2100 / progress 0 exact + S3 1847 + **无 S6**（零 parse 面）+ 性能 A/B 真 v1.22 vs
v1.23 PASS（banner 实证；fluidCells **win1 51927 逐位** + win2/3 29703/29803 ·
29700/29800（±100 = rim 簇 churn 类 gate 131 先例）+ PIPE3 **changesApplied 1404
逐位** + 全指标同档 = 成本可忽略，结构惰性）。**PITS 316**（性能树 step0 hunk #7
锚定 update() 区 = 新插入移上下文 → 手工补块 + marker 计数验；黑窗 PNG 字节随窗口
位置变 = sz 大小门；帧计数器后自增 + 粒子光读空气格 + 周期 36 边界 = 火焰 age-0 截图
设计三坑）。发布政策（用户 10-28 定案）：**tag v1.23.0 覆盖 CRAFT-4+5 = 首个合并
tag**；CRAFT release = 系统收官（CRAFT-7）时发（notes 覆盖 v1.22-1.25）。

## v1.22.0（2026-10-29）— CRAFT 系统首版

**CRAFT-4 配方提示**（v1.x 任务表 22/33，CRAFT 行 1/4，门 137）——原始定义已佚 = 代理
定案 **工作台面板 shaped 配方提示列**（消费 KNOWN_ISSUES Phase-17「工作台面板无配方
列表……shaped 配方的摆法没有游戏内指引」）：
- **33 个 shipped shaped 配方两列提示**：每行 = mini 3×3 pattern（暗格 + 键位物品
  图标，按定义左上对齐 = 网格匹配接受的合法摆放）+ 输出图标 + "name xN"；行着色 =
  **背包可供应则白 / 否则暗**（新纯函数 `canSupply(inventory, patternBag(recipe))`：
  pattern 每覆盖格 1 单位、空格除外、同物品跨字符合并、ItemID 升序）。
- **提示行点击 = 消费性 no-op**：不合成、不动网格/背包，但吞掉点击（区别于"所有槽
  外点击丢弃 carry"规则）。右键落行 = 既有槽外 no-op 位级路径。
- **E 面板位级不变**（shapeless 列表 + 布局几何 + 渲染分支零改动 = 既有布局测试
  零改动全绿钉死）。长 name 行内溢出 = 确定性 `fitLabel` 尾截（" xN" 后缀恒保）。
- **顺带修 PRE-1 潜伏漏字**（P65 触及面内小修先例）：CJK 静态表 32 字形漏 **作
  （U+4F5C）**——PRE-1「18 UI 字」实为 19 字误计，工作台面板标题自 v0.25.0 起渲染
  「工?台」。修复 = `kCjkGlyphs` 改显式 (cp, cell) 表 33 条：既有 32 条 cell 0..31
  逐位不动，作取**死格 cell 127（ASCII DEL，永不渲染）**——font.png 256×512 尺寸零
  变化、全图仅 cell 127 一格变化（56 ink px 逐像素 diff 实证）、既有 32 CJK + 全
  ASCII UV 位级零变化（PRE-1 双红线保持）。字形提取 = NotoSansCJK SC Regular em 16
  行盒 rows 5..20 阈值 ≥128（PRE-1 同款规格），**提取管线 32/32 既有字形逐位复现
  自校验**后才信任。

零 format（21）/config/内容/音效。1844/1844（+7 净：两列布局 + 行命中 + patternBag
×3 + canSupply ×2 + fitLabel；`TableKindIs3x3WithoutRecipeList` 契约演进重写 + 字体
3 测演进含 FNV 重锚 74b5d1418347dab6）、`-Werror` 0 警告。门 137：S1 = **第 137 次
结构性免疫 + base18 锚刷新**（679×744 半平铺态 168140 B——PITS 315：用户 Firefox 窗
在场 WM 平铺变化，旧锚 1362×744 尺寸失配 = 环境态非回归；四方 ×2 全 EXACT +
savenorm 同侧/跨侧）+ S2 判别 PASS（g104 实证开台几何 + 供料物品场景：捕获 → 右键
开台 → 左点 slot5 携 dirt → 左点提示行 0【NEW no-op 保 carry / OLD 同窗位在旧窄面板
外 = 丢 carry】→ 左点 slot5【NEW dirt 回槽】→ 闭合鼠标路径回中心【两侧终相机 =
场景值】：**NEW slot5 = dirt ×1 vs OLD = 空 = 互斥** + 两侧 tables 条目 + 终相机
(0,−0.7) 逐值 + ×2/侧 RAW 335034/280285 B EXACT + savenorm 同侧仅 PITS 244/283 类熔岩湖
churn（~100 块视图外，同侧 RAW EXACT 实证视图零差）+ A/B 像素 640430 px 100.0% 限两
面板并集带 + 新面板右半提示列 255883 px 可见）+ S3 1844 + **无 S6**（零 parse 面）+
性能 A/B 真 v1.21 vs v1.22 PASS（banner 实证；fluidCells win2/3 **29803/29800 逐位**
×2 轮 + win1 ±3 + PIPE3 **changesApplied 1404 逐位** + 全指标同档 = 成本可忽略，
结构惰性；终窗 fluidCellsSeen 43048/43996 树间翻转 = 树内同幅 = PITS 283 类湖相）。
**PITS 314**（unmap_covers 覆盖谓词 y+h≥770 不盖 1362×744+0+20 浏览器窗 → 点击全落
浏览器静默失败；输入跑前显式隐藏预存 covers）+ **315**（WM 平铺态环境依赖 + 锚刷新
协议；zsh 循环内 local 重声明打印旧值假象）。发布政策（用户 10-28 定案）下 v1.22.0
不打 tag（tag 两项合并：v1.23.0 = CRAFT-4+5 候选）；CRAFT release = 系统收官（CRAFT-
7）时发。

## v1.21.0（2026-10-28）— **SURV 系统收官：tag v1.21.0 + GitHub
release**

**SURV-10 挤奶冷却**（v1.x 任务表 21/33，SURV 行 5/5 收官，门 136）
——原始定义已佚 = 代理定案 **MC 5 分钟挤奶冷却**（消费 KNOWN_ISSUES
Phase-43"无 5 分钟挤奶冷却、可无限连续挤"）：
- **per-mob 帧锁定计数器**：`MobEntity::m_milkCooldownFrames`（int；
  `kMilkingCooldownSeconds = 300` × 60 = **18000 帧**命名常量，Phase
  58 整数帧计数纪律 = 确定性）；`update` 每帧 −1（固定 1/60 mob 步
  长，硬规则 8）；`milkReady()` / `startMilkCooldown()`。
- **tryMilk 单点门控**：milkWith 匹配后——冷却中 = **no-op**（不耗
  bucket、不产奶、不发声）但**点击照吞**（failed-milk 既有规则）；
  成功挤奶 → 启动冷却。
- mob ephemeral（不入存档）→ 冷却从不持久化（MC 同款：读档后 mob 重
  生无冷却）。零 format/config/内容（cow JSON 零改动）。

1837/1837（+2 净：CooldownBlocksAndExpiresOnTheFrameBoundary【17999
帧仍冷 / **第 18000 帧精确过期** / 再挤再冷】+ CooldownIsPerMob
Instance）、`-Werror` 0 警告。门 136：S1 = **第 136 次结构性免疫免
重锚**（fresh 60f 四方 ×2 387726 B + base18 + savenorm；nomobs = 无
牛无挤奶双结构惰性）+ **S2 挤奶判别 PASS**（s136_cage 场景 + 临时数
据脚手架【cow-only + max_mobs 1 + cow speed 0.01 缓牛——readPositive
Number 拒 0】：捕获 + 双右键 2 s 间隔——**NEW 终态 bucket 1 + milk 1
【第二击冷却拒绝】vs OLD（v1.20.0）终态 bucket 0 + milk 2【双挤】
= 互斥**，×2/侧 RAW 452077/452070 B EXACT；首跑 old_b 静默失奶 = 低
帧率相位 + 未禁衰减【hunger 18/19 图标差破同侧 RAW】→ nodecay config
【hunger_decay 9999 先例】+ 奶数证据门重跑全过）+ S3 1837 + **无
S6**【零 parse 面】+ 性能 A/B 真 v1.20 vs v1.21 PASS（banner 版本实
证 = PITS 306 纪律；fluidCells win2/3 29803/29800 **逐位** + win1
±3 + **PIPE3 三项全逐位**【visited 266 / seen 43048 / applied 1404】
+ 全指标同档 = 结构性惰性，成本可忽略）。**PITS 313**（挤奶 E2E 瞄
准链四坑：① 宽 AABB 穿墙——1.2 宽 cow 经 leading-cell 检查穿过 1 格
石墙【封死仍穿 = 盲区】→ 弃密封笼改"spawn 重叠 → 确定性挤出驻车"+
瞄准测得驻车点 ② 射线被自家墙挡 → 1 格 sightline 窗【牛穿不过窗】
③ **点击时相机 = 场景值**——修正 PITS 309 归因：终相机漂移 = 脚本跑
后 mousemove 时指针仍锁定（非捕获点击 grab delta）；按终相机预补偿
= 脱靶 7cm 实证 ④ 低帧率相位 + 未禁衰减破同侧 RAW → nodecay + 终态
证据门）。

## v1.20.0（2026-10-28）— **组 1.16-1.20 组末：tag v1.20.0 + GitHub
release**（notes 覆盖 v1.16-1.20 五版）

**SURV-9 饱和细化**（v1.x 任务表 20/33，SURV 行 4/5，门 135）——原始
定义已佚 = 代理定案**饱和度模型细化的两个 MC 要素**（消费 KNOWN_
ISSUES Phase-28 条未建模清单 ② 生肉低饱和度 + ③ 回复不消耗饱食度；
① exhaustion 子点 = Phase 31 节奏制设计选择，保持记档偏离）：
- **① 逐食物 `saturation` 数据字段**（item JSON 可选）：严格 number
  [0, 20]（显式 0 合法 = 不加饱食度）+ **配对纪律**（须有
  restore_hunger，非食物带字段 = 死字段点名）+ **缺省 = 旧 2× 规则位
  级复现**（`foodSatiety` 纯函数 = field ? field : restore_hunger ×
  kSaturationFactor）。内容 = **10 食物全写 MC 精确值**（rotten 0.8 /
  生 beef 1.8 / mutton 1.2 / chicken 1.2 = 生肉低饱和度即玩法点 / 熟
  beef 12.8 / mutton 7.2 / chicken 7.2 / bread 6.0 / apple 2.4 /
  golden 9.0）。
- **② 自然回复半心消耗 1 点饥饿**（MC 规则）：`Player::
  drainOneHungerPoint()` 私有原语（buffer 先扣、耗尽掉条 = Phase 28
  衰减 tick 同款路径上提）——**decay tick 与回复成本共用**（decay 位
  级恒等重构）+ **满血时区间免费流逝**（无回复 = 无成本）；回复门
  （bar ≥ 6）不变。
- 零 format（5）/零 config/零纹理（内容 = JSON 数字）。

1835/1835（+7 净：ParsesSaturationField / RejectsBadSaturation 4 形 /
FoodSatietyFieldElseLegacy / Surv9FoodSaturationContent 真资产 10 值
/ RegenCostsSaturationBeforeTheBar / RegenAtFullHealthCostsNothing /
RegenGateClosesWhenTheCostDrainsTheBar + RegenAfterDelay 扩展成本断
言；1/60 双累加边界 = 14 s 测改 15 s 带 1 s 裕量）、`-Werror` 0 警
告。门 135：S1 = **第 135 次结构性免疫免重锚**（fresh 60f 四方 ×2
387726 B + base18 + savenorm；无进食无回复 + 60f < 36 s 无衰减 = 饱
和零触碰）+ **S2a 回复成本终态判别 PASS**（密封房 hp10/bar20/
buffer6 + nodecay config【hunger_decay_seconds 9999 = g95 先例，消
real-dt D 变量】+ 3300f 跨 fps 全 ≥ 50 s 恰 10 次回复到满：NEW 终态
**hp 20 / sat 0 / bar 16 exact**【10 成本 = buffer 6 + bar 4】vs OLD
**hp 20 / sat 6 / bar 20 exact** = 精确终态互斥 fps 无关，×2/侧 RAW）
+ **S2b 进食饱食度判别 PASS**（hunger 14 / buffer 0 / rotten_flesh
×1，右键按住 2.0 s 过 1.6 s 通道：两侧 slot 空 + hunger 18 exact；
NEW **sat 0.8 exact**【MC 值】vs OLD **sat 8.0 exact**【2× 规则】，
×2/侧 RAW + 相机基线守卫 PITS 309）+ **S2c 空闲对照**（四跑 RAW 全
字节一致 261601 B + 跨侧 savenorm EXACT = 零回归）+ S6 ×3 点名【非
食物配对 / 负值 / 非数字，临时资产改 + 还原】+ S3 1835 + 性能 A/B 真
v1.19 vs v1.20 PASS（banner 版本实证 = PITS 306 纪律；fluidCells
win2/3 29803/29800 **逐位** + win1 ±1 + PIPE3 changesApplied 1404
逐位 + 全指标同档 = 结构性惰性，成本可忽略）。**PITS 310**（zsh 脚本
无 `!` 管道前缀 = bash 习惯，`zsh -n` 零诊断输出静默 RC=1 → head -n
二分 + grep -qv 解法）+ **311**（fps 随场景变化：密封房 50-60 vs
fresh spawn 13-31 = real-dt 终态设计先冒烟实测或消变量）+ **312**
（后台复合命令 heredoc/python 步静默死 = 性能树三连，关键编辑步前台
跑）。

## v1.19.0（2026-10-28）

**SURV-8 工具 10×**（v1.x 任务表 19/33，SURV 行 3/5，门 134）——
原始定义已佚 = 代理定案 **MC 无效率工具 10× 惩罚规则**（消费
KNOWN_ISSUES Phase-11"仍缺：MC 的'无效率工具慢 10×'规则（本项目
'无效率' = flat 档，不慢 10×）"）：
- **规则 = 纯函数 `effectiveBreakSpeed` 一个分支**（item.hpp）：工具
  且目标 ∈ `fast_against` → `breakSpeed × multiplier`（P67 既有，位级
  不变）；工具且目标 ∉ 列表 → **`breakSpeed × kWrongToolFactor`**
  （新命名常量 **0.1f** = MC 精确 1/10：镐挖泥土慢、剑挖石头 1/10
  手速）；**无 fast_against 列表的工具对一切方块吃惩罚**（= MC 剑的
  情形）；手/非工具物品恒 1.0（位级不变）。
- **零数据模型/零内容/零 config**：9 工具 JSON 一字不动（快对清单保
  持 P67-P69 值，惩罚是核心规则不是内容调参）；break 通道
  （selectedBreakSpeed 单调用点）零改动。
- **P69 obsidian 排除条款随规则演进**：铁镐挖 obsidian 从"flat 3.75 s"
  变"慢档 37.5 s"（有效速 0.4）= MC 方向一致（铁镐对黑曜石本就是慢
  的情形）。
- **exact 值**（真资产单测钉死）：铁镐（4.0）×木（硬度 2.0，off-list）
  = 0.4 → **7.5 s**（SURV-8 前 0.75 s）；铁斧（1.0 flat）×石（硬度
  1.5，off-list）= 0.1 → **22.5 s**；on-list 位级保持（铁镐×石 =
  8.0 → 0.28125 s / 铁斧×木 = 2.0 → 1.5 s = 双向防泄漏）。

1828/1828（+1 净：Surv8WrongToolDurations 真资产 exact-time【
EXPECT_NEAR 1e-6 = 0.1f 非二进制精确的浮点残差说明；二进制精确的
on-list 值保持 EXPECT_DOUBLE_EQ】+ 4 处 off-list 钉演进 + 1 测改名
EffectiveBreakSpeedFlatToolStaysFlat → ListlessToolTakesWrongTool
Penalty）、`-Werror` 0 警告。门 134：S1 = **第 134 次结构性免疫免重
锚**（fresh 60f 四方 ×2 387726 B + base18 + savenorm；无输入无按住
破坏 = 规则永不执行 + 内容/纹理零变更）+ **S2a 判别 PASS**（铁镐×木
off-list，几何 = 玩家 (22.5,45,-2.5) yaw0/pitch0 视线恒切 y46 格行 +
目标 (22,46,-5) 1.5 块：按住 0.85 s ∈ (0.75, 7.5)——OLD 侧 0.75 s 破 +
wood×1 掉落静止 (22.5,45,-4.5)【距玩家 3.0 > kPickupDistance 1.0 不
拾取】/ NEW 侧存活无掉落 = 终态互斥，×2/侧 RAW + 存档断言 4/4）+
**S2b 对照 PASS**（铁镐×石 on-list：按住 0.42 s 双侧同破 0.28125 s +
cobblestone 掉落【P61 内容】：**四跑 old×2+new×2 RAW 全字节一致
609396 B + 跨侧 savenorm 场景区 EXACT** = on-list 路径位级不变零回归
最强形）+ S3 1828 + **无 S6**【零 parse 面：规则 = 核心命名常量】+
性能 A/B 真 v1.18 vs v1.19 PASS（banner 版本实证 = PITS 306 纪律；
fluidCells win2/3 29803/29800 **逐位** + win1 ±2 + PIPE3
changesApplied 1404 逐位 + visited 268/266 同档 + 全指标同档 = 结构
性惰性，成本可忽略）。**PITS 309**（输入跑捕获点击的 grab delta = 终
相机漂移 yaw −0.0175/pitch −0.1675 = 9.6° 俯，远超 PITS 28 ±0.01 rad
估计；同窗几何下完全可复现 → 门脚本相机基线守卫 = 批内 RAW/savenorm
可比前提；跨侧同窗几何同 warp → S2b 四跑 RAW 全字节一致）+ 自然区
real-dt churn 裁定（S2a old×2 blocks 差 3 格距场景 107 块 + S2b fluids
差 1 格自然湖区 = PITS 244/283 类同二进制自分歧，非回归）。

## v1.18.0（2026-10-28）

**SURV-7 死亡统计**（v1.x 任务表 18/33，SURV 行 2/5，门 133）——
原始定义已佚 = 代理定案**统计系统 v1 = 死亡相关计数器**（消费
KNOWN_ISSUES Phase-16"无死亡分数/死亡统计"的死亡次数半；与 PRE-12
边界：SURV-7 = 计数/存档/校验本体（零 UI），PRE-12 = 统计屏/F3
呈现框架）：
- **封闭词表**：`deathCount`（玩家 alive→dead 过渡计数，任意死因，
  一次死亡 = 一次计数）+ 每注册 mob 一个 `kill.<name>`（**mob 注册
  表合成** = block item 合成先例：加一个 mob JSON 自动获得 kill 统
  计，零核心改动；无新数据文件）。计数事件帧锁定：死亡过渡块 +
  despawn pass 的每个 `dead()` mob（含无掉落 mob——死亡是事件不
  是掉落；距离 despawn 不算死）。
- **format 20 → 21 `player.stats`**：恒写对象（空对象合法 =
  tables/furnaces 键纪律）；entries = 名 + 整 ≥ 0（非整数/负点名）；
  pre-21 带键 = 版本谎报（格式依赖白名单）；pre-21 读入 = 空
  stats（系统不存在 = 免迁）；**词表校验 = game 加载点
  `validateStats` 单收口点名**（parseSave 不加 MobRegistry 参 =
  184 调用点涟漪——rescan 范式孪生）。容器 `m_stats` map 只存非零
  （omit-when-default）。
- **顺带修复 SURV-6 漏网 bug**：game 存盘填充路径
  `SavedEffect{name, remaining}` 只传 2 值 = 活体 tier-1 效果存盘
  时丢档降 tier 0（门 132 同侧 savenorm 未暴露）→ 补 `a.tier`；
  门 133 S2c 终档 tier 1 往返断言 = 回归证。

1827/1827（+7 净：SaveFormat21Test ×5【往返/空对象/缺键点名/版本
谎报/形状三形拒】+ ValidateStatsTest ×2【封闭词表接受/未知三形点
名】；38 处旧 format 迁移/降级测试契约演进 = 降级须剥 stats 键，含
2 个 downgrade helper 内建剥离）、`-Werror` 0 警告。门 133：S1 =
**第 133 次结构性免疫免重锚**（fresh 60f 四方 ×2 387726 B +
base18 + savenorm；零死零 mob = 零计数 + 零 UI 触碰）+ **S2a 昼步
道 kills PASS**（真生成环 5-6 的 band 石地板 + 5×5 玩家台 + 台间/
带外 22 深洞——ENT-8 文档"环 8-16"误记两连零生成后 SPAWNPROBE
实证重做 = PITS 307：被动 mob 生成 band 游走掉洞 → stats =
kill.chicken×2 + kill.sheep×2 双侧逐值 + deathCount 缺（玩家安全）
×2 RAW + savenorm）+ **S2b 夜 + resistance tier 1**（hp 0 +
**deathCount == 1 exact** + effects [] 死亡清 ×2 RAW + savenorm）+
**S2c 密封房 tier 往返**（终档 {resistance, ~110, **tier 1**} =
SURV-6 填充 bug 回归证 ×2 RAW + savenorm 剥 remaining 留 tier）+
**S2d 旧 v1.17 侧**（format 20 + 无 stats 键 = 系统不存在）+ S6 ×3
点名（kill.dragon / deathCount −1 / f21 缺 stats 键）+ S3 1827 +
性能 A/B 真 v1.17 vs v1.18 PASS（banner 版本实证 = PITS 306 纪律；
fluidCells win2/3 29803/29800 **逐位** + win1 ±4 + PIPE3
fluidCellsSeen 43994/changesApplied 1404/visited 268 三项逐位 +
全指标同档 = 结构性惰性实证，成本可忽略）。**PITS 307**（spawner
生成环 = 数据 min/max_radius 5/6，非文档误记的 8-16——场景设计前
须实证 spawner 入口参数）+ **PITS 308**（`grep -c` 零匹配 RC=1 静
默断 `&&` 链 = 场景脚本整段未执行，冒烟用旧文件两连误诊）。

## v1.17.0（2026-10-28）

**SURV-6 效果档位**（v1.x 任务表 17/33，SURV 行 1/5，门 132）——
原始定义已佚 = 代理定案**状态效果档位化（Minecraft amplifier
levels I/II/III），全数据驱动**（消费 KNOWN_ISSUES Phase-46 条"无
amplifier/时长档位"残余的 amplifier 半；与 PRE-12 边界：SURV-6 =
档位机制/存档/行为，PRE-12 = F3/tooltip 的档位显示）：
- **效果数据 2 新键**：`tier_max`（int [1,3]，缺省 1 = 单档位级复现
  旧行为；>3/非整数点名）+ `tier_steps`（对象：仅 5 个加性修饰字段
  可档位——`speed_multiplier`/`absorb_hp`/`melee_bonus`/
  `melee_penalty`/`damage_reduction`，每值 number ≥ 0；tick/instant
  形 = 单档记档偏离：档位改 tick 节奏会动帧网格，instant 无消费
  面）。三门纪律：**配对**（非零 step 须 base 在场）/ **死字段**
  （tier_max 1 带 step 拒）/ **no-op**（tier_max > 1 全零 step 拒）。
- **全 grant 对可选 `tier`**（0 基，缺省 0 = 位级旧行为；**范围
  init 交叉校验点名**——registry 解析期不可得，on_hit_effect 同款
  纪律）：物品 `grants_effect`/`grants_effects`（食物/饮品）/
  `arrow_effect`（弓）+ mob `on_hit_effect`（近战）/ `arrow_effect`
  （skeleton 箭）。`StatusEffects::apply(effect, duration, tier)`：
  RESET 语义扩到档位（再施放换档，不叠加 amplifier）。
- **聚合 tier 化**（`effectTieredValue(tier, base, step)` 纯函数 =
  base + tier×step）：speed 乘积 / absorption 和 / bonus 和 /
  penalty 和 / resistance 取最高——每实例先按档取值再按既有组合律
  合成；**tier 0 = 全部聚合位级复现旧值**（结构免疫根）。
- **format 19 → 20**：`player.effects` 条目可带 `tier`（**omit-
  when-0** = damage 纪律：tier-0 条目与 format-19 字节同形 ⇒ f19
  文件原样读入 = 迁移免迁；f19 及更早带 tier 键 = 版本谎报，格式
  依赖白名单自动点名；非整数/≥ tier_max 点名，registry parse 期可
  得 = 吸收池不变量同款）。
- **内容 = MC 档位值 5 效果**：speed 1.2+0.2（I/II/III = 1.2/1.4/
  1.6）/ absorption 4+2（4/6/8）/ resistance 0.2+0.2（0.2/0.4/
  0.6）/ strength 2+2（2/4/6 项目压缩刻度）/ weakness 4+4（4/8/
  12 = MC 精确值）；其余 6 效果单档不变。
- **无档位 in-game 消费面（记档偏离）**：信标 `beacon_effects` =
  纯名称列表（无 grant 对象，P91 形态）；药水无酿造系统（starter
  隐藏槽 27 槽全满，PITS 300）→ 档位实例现仅经存档注入（E2E 范
  式，P84 先例）+ 未来内容扩展（信标 grant 对象化 / 酿造 = 矩阵
  外）。零音效/零纹理（HUD 图标 = 既有效果 tile，档位显示 =
  PRE-12）。

1820/1820（+17 净：parse 6 / save 6 / item 3 / mob 2；7 处 format
钉 19→20 契约演进）、`-Werror` 0 警告。门 132：S1 = **第 132 次
结构性免疫免重锚**（fresh 60f 四方 ×2 387726 B + base18 +
savenorm；fresh 零效果 = count 0 fast-exit 聚合零执行）+ **S2 密封
坠落室三值互斥 PASS**（s131 房扩展坠落室：feet y53 → 8 格坠落 = 5
伤，config 禁再生 = P84 范式，600f 无输入：s2a 无效果 **hp 15** /
s2b resistance tier 0（5×0.8 = 4 伤）**hp 16** / s2c tier 1
（5×0.6 = 3 伤）**hp 17**——各 ×2 RAW 327492 + savenorm EXACT，档
位经 存档→apply→减伤管线 端到端实证）+ **S2d 迁移证明**（旧
v1.16.0 二进制 + s2b 的 f19 孪生文件 → hp 16 = pre-SURV-6 代码原
样读取）+ **S6 ×3 点名**（旧二进制拒 format 20 / tier_max 4 /
grants tier 3 越界）+ S3 1820 + 性能 A/B 真 v1.16 vs v1.17 PASS
（零效果场景结构惰性：双侧终档 effects 全空 + win1 fluidCells
51927/51924 ±3 + win2/3 ±100 = 4 格 rim 簇 PITS 283 类非回归 +
PIPE3 changesApplied 1404 逐位 = 成本可忽略）。**PITS 306**（cp -a
性能树带 build/ = 陈旧 CMakeCache 目录失配 + ninja mtime 假"无活"
→ 静默用旧二进制——门 131 性能 v116 侧实为旧 v1.15 二进制【ENT-9
零 src 差 ⇒ 该结论仍成立且更稳，记档】；修 = rm build 重配重编 +
perf 日志 banner 版本实证 + 构建 grep 大小写不敏感）。

## v1.16.0（2026-10-28）

**ENT-9 掉落可视化**（v1.x 任务表 16/33，**ENT 行 5/5 收尾**，门
131，新组 1.16-1.20 首版）——"掉落可视化"原始定义已佚 = 代理定案
**全量掉落可见（纯内容，零核心代码）**（消费 KNOWN_ISSUES Phase-11
"非 block 物品的掉落物在 world 中不可见" + Phase-15 "物品无 3D
模型"两条）：CRAFT-2 后 63 件非 block 无 model 物品的掉落不可见
（含 mob 掉落 rotten_flesh/bone/arrow/feather/beef/mutton/leather +
所有可 Q 丢弃物：食材/锭/矿/染料/盔甲/桶/药水/瓶），解法 = 全部补
`model` 段（texture = 既有 icon PNG，零新纹理免重锚；CRAFT-2 渲染
路径自动覆盖手持/世界掉落/进食三上下文）：
- **model 集合 16 → 全 79 件非 block 物品**：assets/data/items 目录
  即全非 block 集（block 物品由块数据合成、无 item JSON，P61
  先例）→ 契约演进"每个 item 文件都有 model"。
- **家族几何**（单位空间 1-3 box 剪影）：ingot/gem/chunk/dye/sheet/
  bread/meat/wheat/stick/feather/bucket/armor 各 1 box；bone 3（杆 +
  骨节）/ arrow 3（杆 + 头 + 羽）/ bottle·potion·flint_and_steel·
  rotten 2 box；master_bow = bow.json 3 元素结构复用（texture 换
  自身 icon）。
- **共面 Z-fight 修正**（CRAFT-2
  ShippedModelsHaveNoFaceCoincidentBoxes 守护首轮揪出）：bottle/
  potion 颈底、arrow 箭头、rotten 二团 = 相交埋入替代共面（apple
  柄先例：相交合法，共面非法）。
- **零 src 变更/零 format（19）/config/音效**（ENT-8 后代码位级同 =
  性能 A/B 最强形）；与 PRE-12 边界：ENT-9 = 掉落物本体可见（内容），
  PRE-12 = 通用 tooltip/F3 框架（UI，后续行）。

1803/1803（+1 净：`ModelItemSetIsExactly16` 契约演进 →
`AllShippedItemsHaveModels`（全 79 件逐名 + texture==icon 钉死）+
`Ent9FamilyGeometrySpotChecks`（12 家族元素数抽查）；mesh 守护
modelCount 16→79）、`-Werror` 0 警告。门 131：S1 = **第 131 次结
构性免疫免重锚**（fresh 60f 四方 ×2 387726 B + base18 + savenorm；
手持 slot0 = stone 立方路径不变 + 增益件全在隐藏 starter 槽 17-26
不渲染 + fresh 零掉落 + 模型网格惰性 build-once）+ **S2 密封房 A/B
PASS**（s129 平台围合 + 432 glowstone 房顶【air carve 占位改写，
PITS 305】+ 10 件预置 item 实体 + slot0 iron_ingot，600f 无输入：
×2 RAW/侧 + savenorm ×2/侧 + **跨侧 savenorm EXACT** = 实体物理位级
一致纯渲染实证 + **A/B diff 684 px 全限世界带 y[308..380]** + 心带/
热栏带零 = HUD 2D 图标零变化）+ S3 1803 + 性能 A/B 最强形 PASS（两
性能树 src 位级一致 ⇒ win1 fluidCells 51926 逐位 + win2/3 ±100 =
**同二进制 v115perf 自分歧**【门 130 两跑 29703/29700、门 131 跑
29803/29800】+ 终档 3 格 rim 簇 (81..84,36,73..77) 实证 = PITS 283
类非回归 + PIPE3 changesApplied 1404 逐位 = 成本可忽略）。
**新组 1.16-1.20 开始**（组内不打 tag；组末 v1.20.0 → tag + release；
正确性 A/B 旧侧 = v1.15.0 = c1f9d31）。

## v1.15.0（2026-10-28）

**ENT-8 和平/保护环**（v1.x 任务表 15/33，ENT 行 4/6，门 130）——"和平
/保护环"原始定义已佚 = 代理定案 **peaceful 难度**（消费 KNOWN_ISSUES
Phase-25 记档"无 Peaceful 难度：难度仅 easy/normal/hard"；保护环本体
= Phase 25 既有的敌对生成保护圈 8/12/16 锚世界出生点 + 被动豁免——
peaceful = 该保护环扩到覆盖全球）：
- **difficulty 词汇表 3→4**：`mobs.difficulty` = "easy" | "normal" |
  "hard" | **"peaceful"**（严格解析同款纪律，misspelling 点名）。
- **peaceful 语义（零新代码路径 = Phase-25 机制复用）**：
  `spawnProtectionRadius(Peaceful) = kSpawnProtectionPeaceful(1e6)` →
  每个敌对生成格必在保护圈内 → **自然敌对生成全拒**；被动 mob 豁免
  （protection 0）不受影响 = MC 和平"无敌对 + 被动照生"。1e6 保持平方
  比较在 float32 范围内（named constant）。
- **API 一致性值**：`hostileDamageScale(Peaceful) = 0.0`（读作"敌对
  零伤"）/ `hostileSpawnRateScale(Peaceful) = 1.0`（cadence 算术不变，
  moot = 保护先于尝试拒掉）。
- **记档偏离**：① 无"现有敌对消亡"——mobs ephemeral（从不存档）+
  difficulty 只 init 读取（无运行期切换）→ 读档后世界本无敌对，"消亡"
  退化为"不存在"（MC 同款净效果）② 无 MC peaceful 的自动回血/饥饿
  恢复（生存计时器语义 = 另行立项）③ 保护锚 = 世界出生点（Phase-25
  既有，非玩家重生锚）。
- **零 format（19）/config 新键/内容/音效**；game.cpp 零改动（保护圈
  收口点 findMobSpawnCell 既有）。

1802/1802（test_difficulty 6 测演进 +peaceful 断言：4 值解析 + name /
"Peaceful"/"peacful" 拒 / protection 1e6 > hard 16 / damage 0.0 / rate
1.0 / cadence 30 不变）、`-Werror` 0 警告。门 130：S1 = **第 130 次结
构性免疫免重锚**（fresh 60f 四方 ×2 387726 B + base18 + savenorm；
peaceful = config 值，默认 normal 路径零触碰）+ **S2 五跑判别 PASS**
（平台场景复用 s129 几何，玩家 (27.5,45,-17)，2400f 无输入）：S2a
peaceful 夜 ×2 = **hp 20 恒安全**（零敌对，RAW 342632 + savenorm）/
S2b normal 夜新 ×2 = **hp 0**（敌对击杀，RAW 337109）/ S2c normal 夜
旧 v1.14.0 ×2 = hp 0 + **S2b ≡ S2c 字节一致**（normal 生成/追逐/击杀
全链零回归）/ S2d peaceful 昼 ×2 = hp 20 + **vs nomobs 昼对照 diff
45,148 px**（被动成群 = 豁免生效）/ S2e 对照在档 + 性能 A/B 同 seed
900f（fluidCells win1 51969 逐位 + win2/3 29803 vs 29703 = **±100 rim
churn 相位一周期，裁定非回归**：终档差全在程序化熔岩湖 rim 区 + 同二
进制 v114 自分歧 fires 50↔51 + 流体簇 3 格位移实证 = PITS 283 类 +
ENT-8 代码 diff = difficulty.hpp（normal 下惰性）+ 版本串）= 成本可
忽略。**组 1.11-1.15 收官 = v1.15.0 tag + GitHub release**（notes 覆盖
v1.11.0 FLUID-6 honey / v1.12.0 ENT-5 坠落 / v1.13.0 ENT-6 寻路 /
v1.14.0 ENT-7 惯性 / v1.15.0 ENT-8 和平 = 组内 5 版；发布规则 =
BACKLOG「执行顺序」第 3 条：tag 1:1 release，组 release 5 版一组）。

## v1.14.0（2026-10-28）

**ENT-7 移动惯性**（v1.x 任务表 14/33，ENT 行 3/6，门 129）——消费
KNOWN_ISSUES Phase-5 记档"水平速度无惯性：期望速度每 tick 直接设定（无
加速/摩擦），松键即停"。玩家地面/空中移动惯性（MC 式加速/摩擦；"移动
惯性"原始定义已佚 = 代理定案；mob 侧 AI 每帧设精确速度 = 既有设计，
范围外记档）：
- **指数趋近模型（fps 无关，真实 dt）**：有 wish 时 `v += (dir×S − v)×
  (1 − e^{−k dt})`（地面 kGroundAccelRate 8.0 = 敏捷起步 ~0.4 s 达 95%
  / 空中 kAirSteerRate 2.0 = 弱转向）；无 wish 时 `v ×= e^{−k dt}`
  （地面 kGroundDecelRate 12.0 = MC 0.5448/tick 摩擦档【walk 4.3 松键
  滑行 ~0.36 块】/ 空中 kAirDecelRate 1.9 = MC 0.91/tick 拖曳档【松键
  滑翔 ~2.3 块】）；速度 < kVelocityStopEps(0.001) → **恰好 0**（纯衰减
  永不达 0 = 静止态确定性红线，存档字节约定）。knockback 衰减
  std::exp 先例（entity.cpp）。
- **FLUID-2 档保持瞬置**（水/水面/熔岩的 fluid 集成 = 独立调参模型，
  单测 FluidTierKeepsTheInstantSet 钉死 = 记档偏离）；sitting/crouch
  edge guard/子步物理/stepUp 零改动；beacon speed 乘子（Phase 84）作用
  于目标速度 = 趋近目标随之缩放（免费继承）；跳跃冲量零改动。
- **零 format（19）/config/内容/音效**。

1802/1802（+4 PlayerInertiaTest：GroundReleaseSlidesThenRests【滑行 ≈
4.3/12 解析带 + velocity 恰好 0 + 非瞬停】/ AirReleaseKeepsGliding【空中
松键 0.267 s 后 vz > 2.0 vs 地面档同窗 < 0.4 = 档位互斥】/
FluidTierKeepsTheInstantSet / StopEpsilonRestsExactly【静止位不漂移】）、
`-Werror` 0 警告、**3 契约演进重钉**（固定 dt 确定性新值：
SlidesAlongWall 斜向滑行 2 s 减起步 ramp = z 4.3115 /
EdgeBlockNoFloat 0.5 s 地面程减 ramp = 1.6049 / SprintIsFasterThanWalk
1 s walk 3.7977 / sprint 4.9458 = 冲刺优势全秒保持）。门 129：S1 =
**第 129 次结构性免疫免重锚**（fresh 60f 四方 ×2 387726 B + base18 +
savenorm；无输入无 wish = 速度恒 0 = 衰减分支 no-op 位级恒等）+ **S2
空中滑翔判别输入跑 PASS**（s129_glide：平台 x22-33 顶 y45 + 深坑
x34-39 底 y37 = 8 格坠落 + air carve 清自然地形；玩家 x31.5 朝 +X，
hold W 0.8 s → ~0.77 s 出檐、~0.22 s 空中松键：old v1.13.0 无 wish
瞬清零 → 空中 vz = 0 直落（landing x ≈ 34.6）/ new v1.14.0 弱空中衰减
→ 滑翔 4.3(1−e^{−1.9Δ})/1.9 ≈ 1.3-1.6 块（landing x ≈ 36.0）——per-pair
cross **1.4254/1.3332 ∈ (0.8,1.8)** ×2 + 四跑 **y37.0 exact**（8 格坠落
双侧共享实证）+ z 0.5 恒（无漂移）+ 落坑 x∈(34,39.5) + within-pair
|Δx| < 0.6；**首版 ground-slide 判别废弃**：松键时序 real-dt 抖动 ±0.22
块与地面滑行 0.358 同量级 → 首轮 cross 负值 −0.18 实证不可判别 =
**PITS 304**（判别量须 ≥3× 抖动量，否则换几何主导面）+ **PITS 303**
（1500f ≈ 46 s 墙钟 > 自然再生窗 → 坠落 5 伤终态被回血 hp 20，弃 hp
断言改 y37 落面证坠落）+ **S3 放置零回归输入跑 PASS**（s129_place：
无移动键 = 惯性结构惰性——new ×2 RAW EXACT 381031 B + old ×2 RAW EXACT
+ **new_a ≡ old_a 字节一致** + savenorm EXACT + 玩家位恒 + 放置块
(27,45,−7) 在档）+ 性能 A/B 同 seed 900f（fluidCells win2/3
**29803=29803 / 29800=29800 逐位一致** + win1 ±0.006% 填充 real-dt 类
+ PIPE3 visitedChunks 268 / fluidCellsSeen 43994 / changesApplied 1404
逐位 + 全指标同档 = **成本可忽略**，无输入结构惰性实证）。组
1.11-1.15 进行中（第 4 版，组内不打 tag = 发布规则见 BACKLOG「执行
顺序」）。

## v1.13.0（2026-10-28）

**ENT-6 AI 补强**（v1.x 任务表 13/33，ENT 行 2/6，门 128）——"AI
补强"原始定义已佚 = 代理定案 **mob 追逐寻路**（消费 KNOWN_ISSUES
记档缺口"每只 mob 只以玩家为对象……追墙会沿墙滑"；直线追逐被
墙/台阶/深坑挡住时永久卡死 = 最显著的 AI 缺口）：
- **新纯函数模块 `entity/pathfind.{hpp,cpp}`**（campfire 范式）：
  `cellStandable`（feet 格 + bodyCells 头空 = air + 下方 = solid
  FullCube = 整数地板；楼梯/半块 = 非整数 feet 出图 = 记档偏离）+
  `pathfindBFS`（统一代价 BFS，固定邻居序 = 确定性：flat 四邻 /
  step-up +1【物理 stepUp 可踏 1.0 同款模型】/ fall 1..kPathMaxFall(8)
  深走廊格全 air；target zone = Chebyshev ≤1【末段直线 chase 收】；
  展开帽 kPathMaxNodes(512)；无随机无 float = 帧锁定安全（硬规则 8）。
  空路径 = from 已在 zone / 错误 = 无路）。
- **MobEntity 卡住触发集成**：chase 窗 kStuckWindowSeconds(0.5 s)
  水平 progress < kStuckProgressFactor(0.25)×期望 → `tryPathfind` →
  `MobState::Pathfind`（新枚举值）跟随节点（数据 speed 指向下一节点
  中心）+ 每 kRepathSeconds(0.5 s) 重规划（目标移动跟进）+ 失败
  kLostCooldownSeconds(1.0 s) 节流 + **kNodeTimeoutSeconds(2.0 s) 节点
  超时逃逸**（A* 模型可能错——共享 step-up 的 +X/+Z 单格 quirk 使
  规划的 step 物理不可执行 → 弃路径回直线 chase = quirk 几何下行为
  与 ENT-6 前逐位一致，单测钉死）。**结构惰性**：未卡住 = 逐位恒等
  直线追逐；wander/attack/ranged 零触碰。
- **零 format（19）/config/内容/音效**。
- **范围外记档偏离**：寻路只覆盖 chase（wander 每 2.5 s 重掷自然
  逃逸 / ranged kite / 被动 mob 无逃跑 AI = 既有条）；无跳跃边（MC
  mob 亦不跳）；楼梯/半块地板出图（整数晶格纪律）；fall 走廊深帽 8
  （更深坠落直线 chase 亦坠）。

1798/1798（+19：PathfindStandableTest 5 + PathfindBFSTest 10【直线 /
绕墙 / 密封无路 / 1 深坑 fall+cross / 2 深坑绕行 / step-up 平台 /
确定性 / zone 空路径 / from 非 standable 拒 / 节点帽】+ MobPathfindTest
4【卡住找路到玩家攻击 / 平地永不寻路 = 结构惰性 / 密封 lost 恒夹零
命中 / 玩家离场弃路径】）、`-Werror` 0 警告。**4 契约演进重钉**
（MobStepUpTest，ENT-6 意图行为变更 = "恒夹墙" → "绕行不翻越"）：
DoesNotStepUpWall/Fence/TwoBlockStep → sawPathfind + **maxY 恒 1.0**
（原"不可踏"判定存活：1.5 barrier/2 步无 FullCube 顶节点 = A* 不规划
翻越）+ DoesNotStepSingleCellStepPlusX → 改名
SingleCellStepPlusXStaysClampedAtTheQuirk（走廊双侧封墙 = 唯一通路是
quirk 步 → A* 规划 → 物理拒绝 → 节点超时弃路径 → **x 9.7 恒夹 = quirk
pin 在 ENT-6 后存活**；quirk 本体修复仍 = player 行为变更单独立项）。
门 128：S1 = **第 128 次结构性免疫免重锚**（fresh 60f 四方 ×2 387726 B
+ base18 + savenorm new×2/old×2 EXACT；nomobs = BFS 结构惰性）+ **S2
harness A/B 矩阵 PASS**（gen128 单源双库【state 整数面：旧枚举无
Pathfind 值 = 旧侧 sawPathfind 构造性 false】2400f ×2/侧）：wall
NEW hits=19 sawPathfind=true maxY=1.0 pos=(12.188,1,7.409) Attack【绕
墙末端到达攻击】vs OLD hits=0 pos=(7.700,1,6.5) Chase 恒夹【8.0−0.3】
互斥终态 / flat **NEW≡OLD 逐值全同**（hits=29 同终位 = 未阻塞时结构
惰性的二进制级证明）/ sealed **NEW≡OLD 逐值全同**（hits=0 恒夹 = 无路
时 lost 回落旧行为）+ **S3 围栏竞技场零回归 A/B PASS**（s127_arena
900f 夜 mob on：new×2/old×2 RAW EXACT 235680 + **new≡old 字节一致**
【围栏内部无障 = 无 chase 卡住】+ savenorm EXACT）+ 性能 A/B
fluidCells win2/3 29803/29800 双侧逐位【重跑复现】+ PIPE3 逐位 + 全同
档【win3 upd med 首轮 3.44 vs 5.50 = 重跑 6.27 vs 4.96 双向摆动 =
环境噪声证伪】+ 终档流体 4 格差（PITS 283 类 churn 簇）= 成本可忽略。
组 1.11-1.15 进行中（第 3 版，组内不打 tag = 发布规则见 BACKLOG
「执行顺序」）。

## v1.12.0（2026-10-28）

**ENT-5 坠落伤害**（v1.x 任务表 12/33，ENT 行 1/6，门 127）——玩家侧
Phase 16 已完整（`fall > fall_damage_offset(3) && !isInWater() &&
!isInLava() → damage(fall − 3)`，P36 读档重钉、P54 任何流体取消），
"坠落伤害"定案 = **Mob 侧坠落伤害**（Minecraft 中 mob 同样受坠落
伤害；缺口在 MobEntity 无落差跟踪）：
- **Player 规则镜像到 `MobEntity::update`**（stepUp 之后的落地转换
  处）：`m_onGround && !m_wasOnGround` 时净落差 `m_fallStartY −
  position().y` **> kMobFallDamageOffset**（3.0f 命名常量 = MC 同值，
  玩家同值；不做 per-mob 字段——MC 无 mob 差异，YAGNI）且**落地格
  无流体**（fresh `overlapsFluid(Water) || overlapsFluid(Lava)` 扫描
  最终位置 = 玩家 P54 同款"任何流体取消坠落"；tick 初的 `m_inFluid`
  滞后一 tick 会漏 1 深池 = PITS 301）→ `damage(fall − 3)` 走既有
  伤害/死亡/掉落管线（致死掉落零新代码）。
- **钉点**：mobs 是 ephemeral（从不存档）→ 构造函数一次钉
  `m_fallStartY = pos.y`（无玩家 restorePosition 的读档重钉需求）；
  离地转换（`!m_onGround && m_wasOnGround`）重钉 = 走落悬崖的
  fall-start 正确（转换钉点晚 1 物理子步 = 确定性，单测紧公差）。
- **帧锁定**（硬规则 8）：m_onGround = 本 tick 物理结果；无 real dt、
  无随机。**零 format（19）/config/内容/音效**。
- **E2E 设计约束记档**：spawner floor band = 玩家脚高 ±kSpawnFloorDelta
  → 敌对 mob 只生成在玩家同层 ±2，**生成 mob 的致死坠落（需落差
  23+）无法自然生成构造**（chase 受 detect_radius 16 限制到不了；
  knockback 方向 = 远离玩家 = 推不向悬崖）→ S2 = CPU harness 探针
  （窗口免、逐值确定性，FLUID-3 夜探针先例）。

1779/1779（+5 MobFallDamageTest：9 块坠落恰 6 伤 / 3 块内 0 伤 / 1 深
水池落地 0 伤【流体取消，fresh 扫描】/ 23 块坠落致死【既有死亡管线】/
走落 5 格平台 ~2 伤【离地转换钉点 EXPECT_NEAR 0.02 紧公差】）、
-Werror 0 警告。门 127：S1 = **第 127 次结构性免疫免重锚**（fresh
60f 四方截图 ×2 387726 B 全字节一致 + base18 + savenorm new×2/old×2
EXACT；nomobs config 无 mob = 无坠落 = 结构惰性）+ **S2 harness 探针
矩阵 PASS**（2×1 高台 y47 + 清挖坑底 y22 = 25 落差，无玩家 wander-
only，种子 1/5/6/8 探针选出走落 seed）：new ×4 seed 全 **dead**
（deadAt 499/370/201/229，25−3=22 ≥ 20 hp 坠落致死）终位 y22.00 /
old ×4 seed 全 **hp 20.00 存活** 终位 y22.00（seed 6 两侧终位逐位同
(24.17,22.00,-6.55) = 物理恒等实证）——互斥终态 ×4 ×2 确定性（PITS
302：harness 须从所属树 cwd 运行，assets 相对路径）+ **S3 围栏竞技场
零回归 A/B PASS**（12×12 石墙 + 夜 0.75 + mobs on，900f 无输入：敌对
mob 自然生成+追逐+击杀 idle 玩家，围栏 = mob 永不坠落 → ENT-5 代码
结构惰性：new ×2 RAW EXACT 235680 B + old ×2 RAW EXACT + **new_a ≡
old_a 字节一致**（mob 生成/追逐/攻击/死亡全链零回归）+ savenorm EXACT
+ 玩家同帧死同位 hp 0）。+ 性能 A/B 同 seed 900f（fluidCells win2/3
**29803=29803 / 29800=29800 双侧逐位**【v112 首跑 29703 = 一次性
churn 相位波动，重跑复现 29803 = PITS 283 场景自分歧类非回归】+ win1
±0.004% 填充 real-dt 类 + PIPE3 changesApplied 1404=1404 逐位 + 全
指标同档 = **成本可忽略**，mob 代码仅 update 落地块 = 无坠落事件时
零执行）。PITS 301-302（m_inFluid tick 初滞后一 tick / harness 树
cwd）。组 1.11-1.15 进行中（v1.11.0 首版 → v1.12.0，组内不打 tag =
10-28 定案 tag 策略）。

## v1.11.0（2026-10-28）

**FLUID-6 流体种类扩展**（v1.x 任务表 11/33，FLUID 行 3/3 收尾，门
126）——第三流体种类 = **honey（Minecraft 1.14+ 蜂蜜）**，最小完整
版定案：1/3 速高粘度 + 不参与水-熔岩反应 + 可桶舀/倒 + 无波纹 + 无
worldgen（桶源 = 项目无蜂箱的记档偏离）：
- **编码三层化**（chunk.hpp）：`FluidKind{Water=1, Lava=2, Honey=3}`；
  编码带 0 / 水 1..8 / 熔岩 9..16 / **蜜 17..24**（水/熔岩带位级不
  变）；fluidCode/LevelOf/KindOf 纯算术三层泛化；词汇表 +honey（存档
  fluids 段种类名直接生效 = **零 format（19）**）。
- **粘度 = 帧锁定 tick 门控**（单门控记档简化：下落与扩散同 1/3
  速）：`FluidSim::tick(..., honeyActive)` 第 4 参；Game 门 =
  `(m_frameCount / kFluidTickEveryNFrames) % kHoneyTickPeriodFluidTicks
  (3) == 0`；水/熔岩模拟路径零改动（门只控第三段
  simulateType(Honey)）。
- **无反应 = 数据驱动惰性**：honey 块无 water_reaction 字段 →
  reaction pass（熔岩侧驱动）对蜜天然不触发；蜜/水/熔岩 6-邻共存
  互不替换（"他种类 = 阻挡"语义既有泛化，CellReader 4 元组 +
  kindLevel/otherLevel lambda 泛化）。
- **per-kind 容器 3→4**（PITS 299：kind 1 基下标 → 尺寸 = 种类数+1；
  3 尺寸时 Honey 静默 OOB 段错在下游测试）+ chunk_mesher 两种类三元
  派发 → 三种类（honey 顶面无波纹 = emitFluid 既有 kind==Water 门控
  天然排除）。
- **内容**：honey.json 块（非 solid/opaque、transparent、destroyable
  false、无 light_emission = 不发光、atlas 空位 [15,1] 免重锚）+ honey
  纹理（generate_block_textures.py honey() 琥珀光泽，既有 PNG 字节零
  变化）+ honey_bucket 物品（pours_fluid honey / pours_becomes
  bucket）+ bucket.json collects_fluid +honey（三种类一桶全舀）+
  **隐藏 starter 槽 16**（GEN-2 范式；PITS 300 = storage 范围
  [9,26]，槽 27 首版被点名拒绝）。
- **桶交互零改动继承**（FLUID-4/5）：蜜桶对空气 = 放置 / 对蜜流动 =
  变源 / 蜜桶对水格 = 替换倒（(c) 泛化 else 分支）/ 水桶对熔岩 = 反应
  倒（(c1) 特例不变）。

1774/1774（+8：编码带位级 / 门控冻结 / 每门控 tick 落一格 / 每门控
tick 扩一环 / 蜜水 6-邻共存互不替换 / 他种支撑不坠 / 门控不影响水 /
蜜顶面无波纹）、-Werror 0 警告。门 126：S1 = **第 126 次结构性免疫免
重锚**（fresh 60f 四方截图 ×2 387726 B 全字节一致 + savenorm new×2 /
old×2 EXACT；new vs old 存档差 = 恰好 2 项 honey_bucket 隐藏 starter
【存储格不渲染 = 截图免疫保持，GEN-2 先例】）+ **S2 六场景 PASS**
（CPU harness gen126 实证瞄准/落位 oracle）：A 蜜倒【new ×2 RAW
EXACT 407951 B：蜜源 (24,45,-8) L8 + 邻格 L7 封源池稳态 + slot0 =
bucket】/ W 水回归 A/B【s124 水场景 4 跑：new ×2 与 old ×2 各 RAW
EXACT 409647 B + **new_a ≡ old_a 字节一致 = 蜜代码结构惰性零回归**】/
B 落蜜【密封竖井 40f 无输入 ×2 RAW 708974 B：蜜 (24,39,-8) L8 井中
= 5 门控 tick oracle 逐位】/ C 落水对照【同井 40f：水 (24,37,-8) L8
已落底（21 帧落定）×2 RAW 708974 B —— **B vs C = 1/3 速证明**（同帧
蜜 y39 仍在落 vs 水 y37 已落底，互斥终态）】/ D 无反应行【蜜 24 / 水
25 / 封石 26 / 熔岩 27 全封 600f：三源恒 8/8/8 原位 + 行内零
obsidian/stone 转化 ×2 RAW 699617 B】/ S6【old v1.10.0 二进制 + 蜜
场景 = init 失败点名 unknown fluid 'honey'（新种类存档向前不兼容 =
记档）】。+ 性能 A/B 同 seed 900f（fluidCells win2/3 **逐位一致**
29803/29800 + win1 −0.02% 填充 real-dt 类 + PIPE3 changesApplied
1404=1404 / fluidCellsSeen 43994=43994 逐位 + 全指标同档 = **成本可
忽略**，无输入无蜜 = 门控与第三模拟段零执行结构惰性实证）。
PITS 299-300（per-kind 容器尺寸 = 种类数+1 哨兵槽纪律 / 隐藏 starter
槽范围 [9,26]）。**组 1.6-1.10 已于 v1.10.0 收官（tag + GitHub
release）；v1.11.0 = 新组 1.11-1.15 首版**（组内不打 tag = 10-28
定案 tag 策略）。

## v1.10.0（2026-10-28）

**FLUID-5 水熔岩反应**（v1.x 任务表 10/33，FLUID 行 2/3，门 125；消费
FLUID-4 范围外记档"水桶对熔岩面 = no-op（水-熔岩反应交互 = FLUID-5
行）"）——模拟侧水-熔岩反应 P51 已完整（FluidSim reaction pass：熔岩
6-邻水 → lava 块 water_reaction 数据驱动产物【邻水含 L8 源 = obsidian /
仅流动水 = stone】，水不消耗），**本项零新反应规则** = 桶×熔岩交互两
规则（MC parity，纯函数扩展 `bucketPourTarget` 候选接受，返回型不变，
game 写路径零改动）：
- **(c1) 水桶对熔岩格 = 反应倒**：水源放 candidate + 受击法向一步
  （顶视池格 = 正上方；侧向受击 = 开阔侧；无 block 命中 = +Y 约定），
  该格须 air 无流体 + canPlaceAt；熔岩由既有 reaction pass 下 tick 转
  产物（倒出的水是 L8 源 → 项目规则恒 obsidian = 单一规则源）；无空气
  位 = no-op 桶保留（MC：水放不下即什么都不发生）。
- **(c2) 熔岩桶对水格 = 替换倒**：setFluid 既有"替换他种流体"语义把水
  格换成熔岩源（桶消耗），reaction pass 下 tick 按残留邻水转
  obsidian/stone（无邻水 = 熔岩留存）——MC 同款（熔岩替换水随即反应）。
- **范围外记档**：蒸汽（P51 既有偏离保留）/ 流动熔岩舀取维持 P47 现状。

1766/1766（+10 改 1 RaycastPourTargetTest：反应倒目标 = 法向一步干空气
格【流动/源熔岩同目标 = 产物由 pass 决定与熔岩 level 无关】/ 无空气位
no-op / placement Y 越界 / placement 玩家重叠 / placement 持流体（防御
分支钉死）/ 水平法向开阔侧 / 无 block 命中 +Y 约定 ×2 / 替换倒源水与
流动水 = 水格本身；改 1 = P47 异种 no-op → FLUID-5 反应倒）、-Werror 0
警告。门 125：S1 = **第 125 次结构性免疫免重锚**（fresh 60f 四方 ×2
387726 B 全字节一致 new≡old v1.9.0≡base18 + savenorm EXACT——无输入 =
无右键 = (c) 分支零执行）+ **S2 三场景单次右键输入跑 PASS**（CPU
harness gen125_ray 实证瞄准几何；存档状态 = 权威断言；左点捕获协议
PITS 295）：A 反应倒【单封熔岩源 + 观察柱陡顶视】new ×2 熔岩 →
**obsidian** + 熔岩流体消失 + slot0 = bucket（平台浸水稳态）/ old ×2
熔岩恒 L8 + water_bucket【×2 RAW 对 EXACT 207329/205364 B】；B 替换倒
【FLUID-4 水 pool + 熔岩桶】new ×2 流动水格 → **obsidian** + 源水恒
L8 + slot0 = bucket / old ×2 水恒【L7+L8】+ lava_bucket【×2 RAW 对
EXACT 404382/408971 B】；C 守卫【2 格熔岩池，placement = 源熔岩格持
流体 → nullopt】new 熔岩恒【L7+L8】+ water_bucket 保留。**场景 A 双层
周界墙稳态化**（首版无墙 = 倒出的水漫出平台无限扩散 → ×2 RAW 帧差
发散 MISMATCH 实证；CPU 收敛探针 FluidSim::tick 循环至零 changed =
场景区 5 tick 稳态 → RAW 恢复 = **PITS 297**）。**PITS 298**（World::
getFluid 返回任一种类 level 0..8 非 chunk 编码 0/1..8/9..16——
fluidKindOf(getFluid()) 对熔岩格恒 Water，(c) 分支首轮 8 新测全败，
种类判定走 getFluidKind）。+ 性能 A/B 同 seed 900f（fluidCells win2/3
**逐位一致** 29803/29800 + win1 +0.006% 填充 real-dt 类 + PIPE3
changesApplied 1404=1404 + 全指标同档 = **成本可忽略**，无输入桶代码
结构惰性实证）。

**组边界 release**：v1.10.0 = 组 1.6–1.10 末版 → tag v1.10.0 + GitHub
release（覆盖 GEN-7 基岩多层 v1.6.0 / GEN-8 地形互感 v1.7.0 / GEN-9 耕地
湿润 v1.8.0 / FLUID-4 倒向水面 v1.9.0 / FLUID-5 水熔岩反应 v1.10.0；
10-28 定案 tag 策略 = 组内不打 tag、组边界 release 1:1）。

## v1.9.0（2026-10-28）

**FLUID-4 倒向水面**（v1.x 任务表 9/33，FLUID 行 1/3，门 124；消费
P47 流体桶记档偏离"倒水只能倒向方块受击面——对着水面/纯天空倒 =
no-op（MC 对水面的倒法未实现）"）——**纯函数 `bucketPourTarget`**
（world/raycast，canPlaceAt 同款落点）：倒水候选 = ① block 射线受击
面邻格（P47 放置约定——block 射线穿过流体，1 深水池的地面受击使池顶
水格成为候选 = "对着池子倒"成立）或 ② block 射线无命中（超 block
射程的开阔水面）时 fluid 射线自身命中的格；无 cross-fallback（block
命中恒优先 = MC 桶对块面瞄准）。候选过 canPlaceAt（Y 范围/实心/玩家
重叠，与旧路径共享）且满足：**(a) 空气无流体 = 旧放置（位级不变）**
或 **(b) 桶自身种类的流动格（level < 8）= 倒向水面，该格变 level-8
源**。已源格（level 8）/异种流体格 = no-op（桶保留，点击 fall
through，Minecraft 语义：源上倒源无变化）。tryFluidBucket pour 分支
重接，写入路径零改动（setFluid L8 + 列保护 + Lighting::updateLocal +
"splash" + 物品交换）。**零 format（19）/零 config/零内容/零音效**
（水/熔岩桶对既有 P47/P51；流体 = 世界编辑既有存档路径）。
**范围外记档**：① 流动水舀取维持 P47 现状（舀侧扩展另立项）② 水桶
对熔岩面 = no-op（kind 不匹配；水-熔岩反应交互 = FLUID-5 行）③ 倒出
的源走正常 FluidSim（既有）。1756/1756（+11 RaycastPourTargetTest：
旧放置位级 / 倒向流动 / 已源 no-op / 异种 no-op（水拒熔岩 + 熔岩桶收
熔岩 = 种类正确性）/ 实心 / 玩家重叠 / 开阔水（无 block 命中）/ 开阔
已源 no-op / 双无命中 / Y 越界 / block 命中优先无 cross-fallback）、
-Werror 0 警告。门 124：S1 = **第 124 次结构性免疫免重锚**（fresh
60f 四方 ×2 387726 B 全字节一致 new≡old v1.8.0≡base18 + savenorm
EXACT——无输入 = 无右键 = tryFluidBucket 零执行）+ **S2 单次右键输入
跑 PASS**（受控石台 + 封源池【源 L8 (25,45,-8) + 流动 L7 (24,45,-8)
+ 6 石封 = PITS 291 纪律】+ slot0 water_bucket；瞄准几何 = CPU
harness 实证：东玩家 (27.5,45,-7.5) 射线穿源格+流动格打西封石 +X 面
→ 邻格 = 流动格）：new ×2 流动格 → **L8 + slot0 = bucket（互斥终态）
+ "poured" 日志行** / old v1.8.0 ×2 流动恒 L7 + slot0 = water_bucket
（P47 旧 no-op 行为不变）/ S2b 西玩家经东封石瞄准已源格 → no-op 桶
保留（level-8 守卫）；×2 RAW/侧 + A/B 截图 diff 19,819 px = 手持图标
+ 热栏槽 0 + 池区水面（全为倒水直接后果）；输入协议 = 左点捕获（轮询
"Pointer captured" DEBUG 日志）+ 单次右键——**右键交互链只在指针锁定
时执行，裸右键静默丢弃（PITS 295）**；存档状态 = 权威断言（输入跑
协议定位）。+ 性能 A/B 同 seed 900f（fluidCells win2/3 **逐位一致**
= 零 worldgen 改动同世界 oracle + PIPE3 changesApplied 逐位 + 全指标
同档 = **成本可忽略**，无输入桶代码结构惰性实证）。PITS 295-296
（捕获协议 / 同槽双进程终档竞态 = 断言前验 clean exit）。只提交
（组内不打 tag = 10-28 定案 tag 策略）。

## v1.8.0（2026-10-28）

**GEN-9 耕地湿润**（v1.x 任务表 8/33，GEN 行 3/3 收尾，门 123；"耕地
湿润"原始定义已佚，代理定案 = farmland 湿/干视觉状态 = MC farmland
wet/dry）——块模型可选 `moisture_top`（FullCube-only 形状门控【其余 6
形状点名拒 = 死字段纪律】+ 严格非空串 + `requiredTextures` init 交叉
校验）；纯谓词 `soilIsWet`（Chebyshev 半径 4 同层水 level ≥1，熔岩不
湿 = P50 生长节奏同款规则——`cropIsWet` 改委托 = **生长节奏与视觉状态
一函数两消费者，永不打架**）；mesher FullCube 顶面 **mesh 时从活流体
场派生**（湿 tile/干 tile，湿干邻格永不贪婪并 quad）；Game 侧
`m_moistureBlocks` 追踪集（setBlock 收口数据驱动 + 读档 rescan）+
**每 10 帧周期 sweep 收敛**（worker 快照 mesh 可能在水流送前烘 dry，
且 flip 门对"tracked == live 但 mesh stale"死锁 → seed 点强制重烘 +
周期 sweep 双保险；无耕地世界 = 空 map fast-exit 结构零成本）。shipped
= farmland `moisture_top: "farmland_wet"` + atlas 空位 [9,4]（无 UV
移位免重锚）+ `farmland_wet.png`（同 salt 90/91/92 暗化 0.72×，均值
71.8/52.0/35.6 vs 干 101.5/72.4/48.6 = "同一片田、湿了"；生成器幂等
74 既有 PNG 字节零变化）。**零 format（19）/零 config/零新音效**——湿态
是世界派生量（block + fluid 编辑的函数），存档两侧 A/B 同形 = 强
oracle。1745/1745（+12：block 解析/内容钉 6 + growth 谓词 4 + mesher
3）、-Werror 0 警告。门 123：S1 = **第 123 次结构性免疫免重锚**
（fresh 60f 四方 ×2 387726 B 全字节一致 + savenorm EXACT）+ **S2 受控
平台场景双视角 PASS**（石平台 13×13 + 湿耕地 + 干耕地 + 4 石封源 L8
水：俯视 diff 108,542 px 全限耕地顶面带、100% new 更暗、old
(102,72,49)→new (72,52,36) = 纹理均值精确吻合 / 侧视 888 px 精确隔离 +
干耕地带零 diff + 场景区存档 EXACT【远端程序化熔岩湖 real-dt churn
5 格 = 同二进制 ×2 同位分歧，PITS 244/283 非缺陷类】）+ 性能 A/B 同
seed 900f（**fluidCells win2/3 逐位一致** + PIPE3 visitedChunks/
fluidCellsSeen/changesApplied 逐位 + 全指标同档 = 成本可忽略，零耕地
sweep 结构惰性实证）。**偏离**：二元湿/干（MC 是 0-3 四级 moisture
按水距衰减）/ 掉落物与手持立方保持干纹理（无世界上下文）/ 视觉收敛
延迟 ≤ 10 帧（0.17 s）/ 无 MC 水道机制。**记档**：大场景远景
（自然地形 8.5 块侧视 demo 列）湿耕地顶面新旧二进制同现不渲染 = 该
场景配置 GL 列 mesh 集成时机 quirk（CPU mesh 证湿 quad 已发射 + 受控
场景全正常 + old 侧同现 = 非 GEN-9 回归）→ 门改受控场景，KNOWN_ISSUES
候选。PITS 291-294（L8 源必须 4 石封【本项目源非 MC 静态源，240f
平台扩散 71 格实证】/ 大场景远景 GL quirk + 渲染不可见标准取证链
【红纹理判别帧 + CPU mesh 临时日志 + 世界视线盒 dump】/ zsh 无引号
`${var:-word}` 不分词 = gate 脚本 bash 化 / 场景 python 构建器静默未
写入 = 落盘后重读断言关键编辑）。只 tag（release 组 1.6-1.10 未满）。

## v1.7.0（2026-10-27）

**GEN-8 地形互感**（v1.x 任务表 7/33，门 122；"地形互感"原始定义已佚，
代理定案 = 山峰互感 mountain passes）——worldgen `terrain.passes`
新段（enabled/noise_scale (0,0.05]/threshold (0,1)/amplitude [0,64]
逐条点名，缺省段 = disabled 位级 legacy）；**coarse ridge mask**：
独立低频 fbm2（0.008 尺度 = 特征周期 ~125 块、3 octaves、kSaltPasses
专用 salt）> threshold 时 `bonus = floor(amplitude×(mask−threshold)/(1−threshold))`
只升不降，`heightAt = min(base+bonus, kWorldHeight−16)`，纯函数零
chunk 数据 = 跨 chunk 一致构造性成立；shipped {0.008, 0.55, 6} 探针
标定单轮定案（mask 实测 [0.07,0.88]：0.72 首选全世零 bonus 死参数，
0.55 = 全世 ~10% 列 bonus≥2 / max 4 / demo 区 242 列 +1）；**首版
局部峰设计废弃记档**（lround 平顶杀严格局部最大 = PITS 290）。1733/1733
（+11：config 7 + 生成 4）、-Werror 0 警告；4 契约演进重钉（GEN-3 矿
计数 3 值 + GEN-4 洞穴探针 3 测，方向 pin 逐条全保持）。门 122：S1 =
**第 122 次结构性免疫免重锚**（fresh 60f 三方 ×2 六跑 387726 B 全字节
一致：new≡base18≡old≡off；demo 区 242 bonus 列全在 z≥0 = 基线视锥
后方探针定量论证）+ **S2 山脊盆地排水判别 PASS**（玩家 (-304.5,35,560.5)
西望全世最强脊簇：off≡old 字节一致 = 代码级免疫 + ×2 RAW/侧 + diff
525,506 px 顶天带零 + old 侧 diff 区 30.5% 水域 → new 侧 0% 水域
100% 地形 = 抬升排水定案 + player 态 EXACT）+ 性能 A/B 同 seed 900f
（冻结起跑态流体格被抬升吞没 → 剥 fluids 重生 seed；render/swap/stream
同档或更优 + 每流体格成本 14.2→13.9µs + PIPE3 同档 = 代码成本可
忽略）；坑 = Firefox 同名窗平铺分半 + 黑窗双因（尺寸门 (1362,744)
定案）+ raw string 缺 `)"` 吞并下一个 TEST 宏（--gtest_list_tests
发现）。只 tag（release 组 1.6-1.10 未满）。

## v1.6.0（2026-10-27）

**GEN-7 基岩多层**（v1.x 任务表 6/33，门 121；消费 KNOWN_ISSUES Phase 4
"基岩只有单层"）——worldgen `terrain.bedrock_layers` 新字段（int [1,8]，
缺省 1 = 旧行为位级复现，越界/非整数点名拒）；shipped = **3 = MC 经典
基岩底**（y0/y2 恒床岩 + y1 每格确定性 50% roll `bedrockLayerRoll`
纯谓词，kSaltBedrock 专用 salt）；`fillTerrain` 单分支，layers=1 恒假
= 向后兼容；零 format（世界生成演进 = 旧存档新底先例）。1722/1722
（+8：config 5 + 生成 3）、-Werror 0 警告；GEN-3 矿计数契约演进重钉
4 值（y1/y2 变床岩 = 可放置深石集缩小，GEN-4 先例）。门 121：S1 第
121 次结构性免疫免重锚（基岩底 y0-2 在出生点视图外）+ S2 竖井壁带
判别（3×3 全深井 + 悬台内瞰）：diff 13172 px 93.2% 集中横带 + 99.0%
new 更暗（old 石亮 95.9 / new 床岩暗 42.1 = 解析值吻合）+ savenorm
EXACT + ×2 RAW/侧 + 性能 A/B 同起跑态 fall 900f fluidCells 三窗
229457/218800/221654 逐位一致 + 全指标同档 = 成本可忽略。PITS 289
（init 失败后旧产物残留 = 假 PASS：探针验证四件套 ERROR/clean exit/
真终档/截图 mtime）。只 tag。

## v1.5.0（2026-10-27）

**LGT-5 半块透光**（v1.x 任务表 5/33，门 120；**release 组 1.1-1.5 组末
= 本版本发 GitHub release**）——slab/stairs/wall/fence（7 块）加
`light_opacity: 0` = **MC 半块透光规则**（光如空气穿过，竖直天空免费 +
每格成本 1 = enterCost max(1,0) 与空气逐值同），**零核心代码**（LGT-2
统一进入格成本机制消费面 +1）；`opaque: true` 保持 = 剔面/网格渲染零
变更（P85 半块剔面局限保留 = 记档偏离）。1714/1714（+3：内容钉死 1 +
竖直天空透光 1 + 密封盒块光层透 1）、-Werror 0 警告；门 120：S1 第 120
次结构性免疫免重锚（半块永不生成 = 世界光场零变化）+ S2 密封石室判别
（**唯一开口 = slab 格 + 外壁 glowstone 15**）：old 室恒暗 (10.6) vs
new 光穿半块灌亮全室 (76.8)，diff 945782 px（93.3% 室内面）0 px 变暗 =
纯光增益 + savenorm EXACT（世界全同）+ ×2 RAW/侧 + 性能 A/B 同起跑态
fall 900f：fluidCells 三窗 229457/218800/221654 双侧逐位一致 + 全指标
同档 = 成本可忽略。PITS 288（场景构建覆盖 base 块编辑 = 孤儿 fires 交叉
校验 init 死）。**组 release 1.1-1.5**：notes 覆盖 v1.1.0-1.5.0（STRM-2
流体列保留 / PIPE-3 流体 per-cell / LGT-3 夜光重传播+月光地板 / LGT-4
光束入光 / LGT-5 半块透光）。

## v1.4.0（2026-10-27）

**LGT-4 光束入光**（v1.x 任务表 4/33，门 119/119b；信标光束 = 全项目最后
一个恒白 unlit 消费者）——复用 PRE-6 per-draw 光照覆盖范式：纯函数
`beamLightSample`（**信标块上方空气格 (x,y+1,z)** = beam 最低格的
sky/block ÷15；信标块自身格不透明恒 (0,0) = 门 119 E2E 揪出并修复的采样
缺陷，OwnCellOpaqueAlwaysIgnored 回归单测钉死）+ `u_drop*` 三 uniform
改名中性（u_useLightOverride/u_lightOverrideSky/u_lightOverrideBlock）+
每 beam draw 后 bind+复位 0（UXFIX-1 泄漏纪律）；beam 深夜 = LGT-3 月光
7/15（非恒白）、近萤石/火把亮化、白天露天 = 1.0 恒白（df=1 恒等根）；
零 format/config/内容。1711/1711（+7）、-Werror 0 警告；门 119：S1 第
119 次结构性免疫（fresh 60f 四方 + base18 连续，最终二进制复验）+ S2a
首版场景 beam 失明废弃（on/off 双二进制 diff=0 实证，PITS 287）+ S2b
门 119b（pitch 1.25 取景）：sanity day on/off 1646 px + day df=1 恒等
RAW（128226 B 字节一致，beam 在框内非平凡）+ night ×2 RAW/侧 + **night
beam 列 1646 px 全限竖直窄带仅暗化（带均 (66,105,116)→(33,53,63)，
比值 0.483 解析吻合，零亮化，带外 0 diff）** + savenorm EXACT + 性能
A/B 同起跑态 fall 900f：fluidCells 三窗 214627/194500/230306 双侧逐位
一致 + 全指标同档 = 成本可忽略（无信标结构惰性）。只 tag（release 组
1.1-1.5 末发）。

## v1.3.0（2026-10-27）

**LGT-3 夜光重传播 + MC 月光地板**（v1.x 任务表 3/33，门 118；深夜可夜视
= 10-27 夜用户实机定案）——呈现层昼夜缩放升级为**天空光场对源等级仿射
（无损）重演**：所有天空光源共享源等级 S 且逐格成本与 S 无关 ⇒
`有效天空光 = max(0, stored − (15−S))` 逐格精确（confluence 单测 = 源等级
S 直接重传播 vs 仿射逐格相等钉死），**零重传播**（shader 天空项一行 +
生成光门整数镜像）。**MC 月光地板**：S = 7 + 8×dayFactor（`kNightSkyLevel
= 7` = 午夜露天月光 7/15，深夜不再全黑 = 夜视能力恢复）+ 敌对
`spawn_max_light` 0→7（MC 配对：夜露天 7≤7 可生成保夜险 / 火把圈 14>7
免疫 / 白天 15>7 不生成 = 玩法闭环）。正午 df=1 恒等 = 免重锚。1704/1704
（+9）、-Werror 0 警告；门 118：S1 第 118 次结构性免疫免重锚 + S2 深夜
0.75 A/B 828,043 px 全仅亮化 + 黄昏 0.50 同数 diff + open-sky 锚盒零
diff + savenorm EXACT + 性能 A/B fall 900f 全指标同档 + fluidCells 三窗
逐位一致 = 零成本变更。只 tag（release 组 1.1-1.5 末发）。

## v1.2.0（2026-10-27）

**PIPE-3 流体 per-cell**（v1.x 任务表 2/33，门 117）——99.8 chunk 粒度
活动门控的 per-cell 化：tick 成本从 O(visited chunk × 32K 格 × 2 遍 +
全流体 chunk 集合遍历) 降到 **O(活动 chunk + 活流体格)**：per-chunk
per-kind 流体格列表（`setFluidAt` 成员变化 push / drain lazy stale /
walk 过滤+列序+去重+压缩，不变式 = 活格 ⇒ ≥1 条目）+ World per-kind
活动 chunk 集合（6 插入点穷尽 flag 置位点，walk 消费，flag 双检 belt）
+ falling 标志 per-column 化（colFlag[256] + 跨 chunk topFlags[256B]，
引用语义位级同旧）+ reaction/burn 活格扫 + propose map max 合并（序
无关）。流体状态**逐位不变**（门 117 S2 位级一致 oracle：fall/fill 跨
侧 savenorm EXACT + 截屏 RAW 字节同；cascade nospread 位稳 oracle 存档
RAW 854590 B 逐字节同；fire 区 1 格差 = 同二进制自分歧实证 = 既有
real-dt 类，非回归）。性能 A/B：fall 流体段 −50%（1261→631 ms/300f
窗）、活跃 tick 34.8→12.6 ms（2.8×）、fill churn 窗 −80%、沉降下界
−50%，fluidCells 逐位一致 + 帧指标全同档 = 纯收益。顺带修
`setFluidAt` 潜伏 debug assert（lava 编码 9..16；NDEBUG 零变化）+
test_mesh_job static_assert 契约演进（vector 成员）。只 tag（release
组 1.1-1.5 末发）。

## v1.1.0（2026-10-27）

**STRM-2 流体列保留**（v1.x 任务表 1/33，门 116）——仅流体模拟改动的列
（远端熔岩流/海岸膜，无 fire/状态条目 = 无卸载保护资格）卸载后重置为
生成态的修复：驱逐时 journal diff 快照按列保留（`retainColumnDiff` =
`journalDiffChunk` 字节同语义，保留 diff 条目非 chunk = 内存有界）+ 列
再流送时 integrate 回放（blocks 先 fluids 后、不插 m_editedColumns =
fluid-only 列保持可卸载）+ 退出存档双路径 `mergeRetained`（含
--save-full-diff oracle 不失真）。零 format（19）/config/内容。**随门
116 场景首触发修复 99.8 活动门流送边界活性缺口**：settled（冷）流体
前缘对 void 的 spread 被拒后转冷，目标列流送到达不唤醒 = 级联永久卡
在流送边界（插桩实证 395 tick 零推进）；修 = `insertChunk`/
`getOrCreateChunk` 的 `wakeFluidChunks`（自身 + 6 面邻持该 kind 的
chunk 各走一次，镜像 setBlock UNSTICK；唤醒仅在插入事件 = 性能面零
回退）。性能 A/B fall 900f：全指标同档或更优（flush −3..−10%、stream
−3..−6%）；fluidCells 123879 逐位一致 + 167781→167782 /
236573→236575（+1/+2 = 活性补洞行为语义：静态梯度提前一格越流送
边界，非 churn 放大）。只 tag（release 组 1.1-1.5 末发）。

## v1.0（2026-10-27）

**首个正式版**：v1.0 任务矩阵 34/34 全完成（十大系统 + 2 项实机报障）。覆盖
v0.31.0 / v0.32.0 两版 + 版本号收口（二进制内版本字符串自 v0.16.0 起滞后，
本次恢复诚实：启动日志/F3 首行 = `TokenMine.cpp v1.0`）。正式版 release
见 GitHub Releases "TokenMine.cpp v1.0.0"（原 tag v1.0，10-27 改名；
Latest = v1.5.0 组 release）。

## v0.32.0（2026-10-26）

**FLUID-3 mob 涉水修正**（任务表 34/34，实机报障修复）——牛羊鸡走入水中/
悬崖边的"悬空/站在水上"：浮漂终态从干身踩水皮改半浸没游泳位（身体中轴 =
水面，MC 游泳观感）+ 深水（≥2 格）池底 mob 可上浮出底 + 1 格浅池涉水不变
+ 涉水 mob 可踏 1 格岸壁出池 + 悬崖坠水不再"降落伞"减速（全速过水面带）。
玩家游泳行为位级不变。门 115。

## v0.31.0（2026-10-26）

**UXFIX-1 实机渲染修复**（实机报障修复）——两条：① 热栏/面板的方块物品
图标显示为半透明白盒 ② 挖掘时方块显示全黑（裂纹白线在、块面纯黑）。根因 =
PRE-4 云引入的 GL per-draw 状态泄漏（u_useTexture/u_color/u_alpha + blend
状态跨 draw 残留），锁定态专属、无输入门全盲；修复 = 云/光束 blend 所有权
上移 + 挖掘覆盖层自持混合 + 方块图标立方显式纹理配方。无输入位级恒等。
门 114。

## v0.30.0（2026-10-26）

**PRE-6 掉落光照**（任务表 32/32 全完成，PRE 行 6/6）——v1.0 候选。
掉落物受本地光照：洞穴里的掉落不再亮如白昼、火把旁的掉落被照亮（每帧采样
所在格光照，昼夜自动跟随）；`render.drop_light` 可关（代码默认开，
config.json 零改动），关 = 与 v0.29.0 位级恒等。

## v0.29.0（2026-10-26）

**PRE-5 透明 discard**（任务表 31/32，PRE 行 5/6）——修复"从树叶/玻璃孔
看不见后面"：alpha-0 像素不再写入深度（cutout 段 discard + 深度写，
半透明段水/玻璃/云改为不写深度）；物品掉落从叶孔可见。20 个 cutout 块
数据驱动路由（leaves/花草/麦/苗/火焰/火把/营火），`render.alpha_test`
可关（代码默认开，config.json 零改动），关 = 与 v0.28.0 位级恒等。

## v0.28.0（2026-10-26）

**PRE-4 云与环境音**（任务表 30/32，PRE 行 4/6）——天空云层：y=108
半透明平板随帧锁定漂移（MC 节奏 100 s/格），相机移动/漂移步进才重建
（零每帧开销）；昼夜环境音：洞穴/白天/夜晚三上下文各一环境底噪（新增 3
个确定性音效，音量低于全部 SFX），5 s 一掷的帧锁定调度。`render.clouds` /
`audio.ambience` 均可关（代码默认开，config.json 零改动），关 = 与
v0.27.0 位级恒等。

## v0.27.0（2026-10-26）

**PRE-3 水面波纹**（任务表 29/32，PRE 行 3/6）——静态水面帧锁定 shimmer：
水顶面 UV 采样窗逐帧平移（白噪声纹理 + NEAREST = 清晰 texel 台阶），走
既有 tiled 采样路径（fract 自动 tile 内 wrap，零 tile 识别）；
`render.water_ripples` 可关（代码默认开，config.json 零改动），关 = 与
v0.26.0 位级恒等。

## v0.26.0（2026-10-26）

**PRE-2 边缘雾**（任务表 28/32，PRE 行 2/6）——世界边缘距离雾：流送圆盘
边缘的原始 chunk 切面（"渲染条"）向同帧天空色线性溶解（雾程 =
min(远平面, 16×min(view_distance,64)) 块的 75%→100%），昼夜跟随；
`render.fog` 可关（代码默认开，config.json 零改动），关 = 与 v0.25.0
位级恒等。

## v0.25.0（2026-10-26）

**PRE-1 中文字体**（任务表 27/32，PRE 行 1/6）——UI 文本中文支持：UTF-8
解码渲染（坏字节回退 `?`）+ 32 个 CJK glyph（图集死格复用，尺寸零变化）
+ 8 处用户可见 UI 字符串中文化（死亡屏/面板标题；F3 调试屏保持英文）。

## v0.24.0（2026-10-26）

**SAVE-1 自动存档**（任务表 26/32，SAVE 行 1/1 收尾）——周期性自动存档
（`save.auto_save_seconds` 默认 120 s，真实 dt 节奏，无存档槽不启用）+
崩溃韧性原子写（temp + rename，手动/退出/自动三路共用——kill -9 不再
产出半写档）。

## v0.23.0（2026-10-26）

**CRAFT-3 材质音效**（任务表 25/32，CRAFT 行 3/3 收尾）——per-block
材质音色（`sound_material` 数据字段，6 材质 × 3 事件 = 18 个确定性新音效，
32 块赋值，无字段块位级不变）+ 3D 空间化（SoundBank stereo 化，距离衰减
cutoff 16 块 + 恒定增益声像，dig/破坏/放置/旋转/丢弃/拾取世界空间化，
UI/手持音效保持 2D）。

## v0.22.0（2026-10-26）

**CRAFT-2 物品 3D 模型**（任务表 24/32）——per-item 数据驱动 box 模型
（item JSON 可选 `model` 段 = 1..16 个 axis-aligned box + 单一纹理，严格
解析），三上下文渲染：手持（替代 icon 面片、继承弓拉/进食举到嘴变换）/
世界掉落（替代不可见掉落）/ 进食举到嘴（零额外代码）；16 内容模型（工具
9 + 弓 5 + 苹果 2，纹理 = 既有图标 PNG，零新纹理）；无 model 物品三上下文
位级不变。

## v0.21.0（2026-10-26）

**CRAFT-1 工作台分块**（任务表 23/32）——每个工作台独立 3×3 容器
（format 19 `tables` 段 + 面板绑定所点块，炉子/营火 per-block 范式；
破坏丢弃容器物品 = 炉子同款记档）。

## v0.20.0（2026-10-25）

**GEN-6 树放置半区修复**（任务表 32/32 执行序 22 项）——修复 Phase 4 起
潜伏的 wz 循环上界 bug：约世界一半（cz ≥ cx+1 对角半区）恢复种树/放装饰
（wz 上界 2 处一行级修复，零 format/config/content，跨 chunk 无缝契约
保持）。

## v0.19.0（2026-10-25）

**SURV-5 mob 效果**（任务表 21/32，SURV 行 5/5 全清）——状态效果双载体：
skeleton 箭带 wither（1HP/4s 无地板可杀）、毒弓 poison_bow（箭中 mob 中毒
60s 可毒死）、不死族免疫 poison（MC 语义）。

## v0.18.0（2026-10-25）

**GEN-5 浮空树**（任务表 20/31，用户报障修复）——洞穴挖掉的地面上不再长树
（caveCarvesCell 纯谓词守卫，carveCaves 行为恒等，零 format/config/
content）。

## v0.17.0（2026-10-25）

**GEN-4 洞穴破碎度**（任务表 19/31，用户报障修复）——洞穴重标定：洞体更
大更少，小洞窟 −91%，中位洞体积 9-130×（noise_scale 0.09→0.05 单参数，
零核心改动）。

## v0.16.0（2026-10-25）

**SURV-4 药水饮用**（任务表 18/29）——6 种单效果药水（虚弱 1:30 / 迅捷
3:00 / 再生 0:45 / 中毒 0:45 / 治疗 +4 瞬时 / 伤害 −6 瞬时护甲豁免）+
玻璃瓶舀水-倒水循环 + `instant_hp` 瞬时效果原语；喝 = 按住右键 1.6 s
返还玻璃瓶，再喝同效果重置不叠加。无酿造台（starter 发放，记档偏离）。
format 18 不变。

## v0.15.0（2026-10-25）

**SURV-3 耐久**（任务表 17/29）——27 件工具/护甲/弓 MC 精确耐久值 + 五类
磨损（破块/打 mob/射出/点燃/护甲减伤事件）+ format 18 + HUD 2px 耐久条；
无修复系统（记档偏离）。

## v0.14.0（2026-10-25）

**SURV-2 氧气窒息**（16/29）——水下氧气条 + 溺水伤害（帧锁定 15 s 空气 /
每 2.5 s 2 HP，溺水豁免护甲减伤 = MC 伤害表实证），format 17。

## v0.13.0（2026-10-25）

**SURV-1 护甲**（15/29）——12 件护甲 MC 精确值 + 整数 floor 减伤（全伤害
源含坠落）+ 右键穿戴/交换/脱下，format 16。

## v0.12.0（2026-10-25）

**ENT-4 水体交互**（14/29，ENT 行收尾）——mob/掉落物浮力（被动上浮/潜行
潜水/水面漂浮），陆上行为逐位不变。（FLUID-3 起共享模型演进：游泳位/
深度门/踏阶解禁，见 v0.32.0。）

## v0.11.0（2026-10-25）

**ENT-3 mob 踏阶**（13/29）+ **ENT-1b 模型修复**（实机反馈）——mob 可上
1 格台阶（与 player 同判定）；四足比例/关节/穿模修正。

## v0.10.0（2026-10-25）

**ENT-1 四肢摆动**（12/29）——mob 行走摆腿动画：纯渲染层，数据驱动
`swings` 段 + 编译期定点正弦表（零运行期 libm）。

## v0.9.0（2026-10-24）

**99.11 流体光照异步化**——"转摄像头卡"深层根因修复：列粒度 worker
追赶，海洋世界静置 update p95 秒级 → ~90 ms，vd16 冷填充 5.5×。

## v0.8.0（2026-10-24）

**99.10 光照 job 并行**——流送期光照下限 16.7 → ~10 ms；新旧二进制 300
帧字节一致（confluence 证明）。

## v0.7.0（2026-10-24）

**游泳划水**（FLUID-2）——被动上浮/潜行潜水/水面跳出，对齐 MC。

## v0.6.0（2026-10-23）

**水量守恒**（FLUID-1）——每格流体变化归入具名事件（会计恒等式单测钉
死），熔岩同款。

## v0.5.0（2026-10-22）

**矿脉形态 MC 化**（GEN-3，GEN 行收尾）——3×2×1 旋转椭球脉；旧存档可读。

## v0.4.0（2026-10-22）

**树生长**（GEN-2）——3 树种可种植树苗 + 数据驱动树形生长（format 15）。

## v0.3.0（2026-10-22）

**BlockState 通用 per-block 状态**（ENT-2）——楼梯朝向泛化为状态机制
（format 15，旧存档直接可读）。

## v0.2.0（2026-10-22）

**任务表前五项**（STRM-1/PIPE-1/PIPE-2/LGT-2/GEN-1）——流送环圆盘化
（vd80 内存 −49.5%）、网格化搬 worker（主线程 remesh −96%）、存档增量
（退出 6.2 s → 17 ms）、叶子光衰减、面条洞穴。

## v0.1.0（2026-10-17，首个公开版）

功能阶段 P0–P99 + 性能系列 99.x 全部完成：可玩的世界生成/生存/Mob/弓/
合成/流体/昼夜；1195/1195 测试、`-Werror` 0 警告。
