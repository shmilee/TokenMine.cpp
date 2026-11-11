# 坑位合集（都踩过，勿再踩）

> Phase 30 文档整理：从 `SESSION_CONTEXT.md` **原样**移入（未改写）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/*.zsh`）指向当时状态；当前文档布局见
> `README.md`「文档地图」。
> pre-3.x 条目（PITS 1–384）→ `docs/archive/PITS_PRE3X.md`；本文件 = 3.x 时代 PITS 385–419，新条目继续前插于尾部块首。

- **黑夜 guide authoring 弓箭 combat 八坑（PITS 419，门 228 v3.38.0
  V3-16g ⑩ 黑夜历险）**——① **弓 = 短程武器（flat range v²/g =
  11.25m）**：shoot_speed 15 + g 20 → 平射极限 11.25m；14-19m 处任何
  ≤12m 上仰 lead 都不中（arrow_sim.py 实证 19m/14m 全 NO HIT）——
  v14「19m 站射 4 箭全 miss」= 纯弹道 drop（箭初速 vy=0，19m 落
  10·(19/15)² = 16m）。combat 必须 ≤10m，aim 带垂直 drop lead（7m
  = +1.8m / 5m = +0.9m / 3m = +0.4m / 2m = +0.15m，mid-window 取值
  留 ±1m 边际）。② **箭在 RELEASE 发射非 press**：bow draw 于 RMB
  press 累积、release（wasReleased）才发射——4 箭实际发射帧 =
  press+96f。aim→release 的 121f 里追击 zombie 朝玩家推进 1.6m →
  按 press 帧位置 aim 必欠；须按 **release 帧**位置 aim（zombie 朝
  玩家来 = 无水平 lead，只垂直 drop lead）。③ **hold=96 有 1-frame
  欠拉边界 = 99% power**：满拉 = 1.6s = 96f，但注册 1-frame 滞后给
  1.583s → power 0.99 → 4.95/箭 → 4 箭 19.79 < 20 → zombie 0.2hp
  存活续 melee（v19 实证）；**hold=100**（1.667s > cap）保满力。
  ④ **箭 knockback = 定点迭代**：箭中 = 4 b/s 水平 + 2.5 lift 击退
  （arrowKnockback，Phase 42），zombie 轨迹依赖哪几箭命中 → aim 须
  按**实测**位置迭代 pin（v15→v19：2→2→3→3→4 命中，5 轮收敛）；
  「无 knockback = 路径与 aim 独立」只对 wander/瞬移 mob 成立，追击
  mob 被击退。⑤ **持 launcher 时 RMB = 拉弓非开容器**：combat 后弓
  仍在手，s5 的 open_ahead（RMB）被 bow draw 吞 → table 不开 = 35
  panel error；开容器前须 g.select 切**非弓非块**物（sticks，digit
  6）。⑥ **火把放置 = 先走到点再放**：place-after-walk = 6-7m aim >
  kReach 4.09 拒收（v22 1-of-4）；每把先 walk_to 目标格南 2m，再
  face + look_point + click。⑦ **zombie 自 spawn 即追**：seed 7 的
  zombie f150 距玩家 4.6m < detect 16 → 立即追，s2 kite 期玩家受 6
  伤（20→14）自然回血 s4 满；无 skeleton seed = seedhunt 8 粒实证
  （skeleton 箭带 10s wither = 站桩 9s 死，spider 1.0 b/s 打不过
  11.5m 列 kite 账本）。⑧ **inv_grid = 3×3 制作网格非背包**（E 2×2
  = 格 0/1/3/4 三列式 r*3+c）：备料放背包 slot 13/14（b4/b5 词表）
  非 inv_grid（v11 捕获：grid cell 4 撞 E 面板 g3 = table pattern
  破）；panel tap 间隔 ≥25f（20f 双击收集窗边界吞点击，16a pilot
  形）；bow pattern " S "/"S P "/" S " 的 P = **g5**（MR idx 5）非
  g4；select(n) = digit n = h(n-1)（table h4=5 / bow h6=7 / torch
  h8=9）；carry 换货语义 = 取 B 前先 merge 回（out 前 carry 须空）。
- **河流 guide authoring 十六坑（PITS 418，门 227 v3.37.0 V3-16f ⑨
  河流）**——① **journal 重复块响亮拒**：mk_scene campfires 面只写
  state entry（块须 spec blocks 自携 "campfire"，自 append 同格 =
  load 侧 "duplicate block edits" 拒）。② **2-deep 池顶层源不水平
  传播**：流体模型 resting on same-kind fluid = B 规则（无 spread）
  → 舀痕永久空洞；1-deep 池（石底）舀痕 = 邻源 1 tick 补 level-7
  稳（不升 8——底下是块）【probe B 实证，定案 1-deep + 2 痕入终态
  钉】。③ **第二舀必换源格**：level-7 痕不可舀（isSource =
  level==8 严格），同 aim 二次 RMB 静默 no-op；aim 换池远角源
  (19)/(20,53,-13) DDA 逐射线核。④ **沟端列长穿线 = 挖后卡死**：
  列 x17.5 穿沟端格 (17,53,-7) 挖前实心可走/挖后 1 落洞 + step-up
  失败（W hold 撞墙停沟底）；干路 = bank 行 z-5.5/z-9.0 + x18.5
  列（沟东 bank）+ x17.5 限池缘段。⑤ **mk_scene fire remaining =
  real-dt**：base 90f 跑 worldgen 火随墙钟漂 → 场景资产 = 构建时刻
  快照（幂等 = 同次双跑字节恒等，重生成 = 有意动作）。⑥ **--demo-
  author 输出 <timeline>.dmo**（手动 mv committed 名）。⑦ **demo
  scene 只解析 assets/demos/scenes/**（probe 场景临时拷入后删不
  入库）。⑧ **timeline 键词表小写**（space/w/c/e/1..9 响亮拒）。
  ⑨ **zsh 函数 $GC 展开空**（调用无 $，否则 "command not found:
  norm" 静默吞 savenorm 对比）。⑩ **游泳腿 = 显式 hold + 相对
  坐标**：WALK 模型（4.3 + 指数 ramp）不适用 swim（3.1 恒定 +
  被动浮）——hold 15/252/space25/30 probe v3 pin + 腿全相对 track
  （sx = g.x − N）；walk 模型 hold 过冲 3.4-m 落自然地形假 PASS =
  pos 断言须带 y + 平台域。⑪ **walk_leg 显式 hold = 帧非米**：
  显式 hold 仅 probe-pinned 异 pace 腿（swim/fall 短 hold）；普通
  行走腿 = 模型解（hold=None）——step-in 30f 按 ramp 建模 = 2-m 走
  行跳落点 → 每 dig aim 偏一格（x9 回瞄 x8 石裸手 45f 静默空挥，
  track-实际 >1-m s4 belt 捕获）。⑫ **campfire 格内侧面挖邻格 =
  必中 campfire**（块射线首 non-air 格 cross 可打）：陡 pitch 的
  y54 穿越点 = 站 x+0.3 < 目标格带 → 首命中 campfire 顶面 45f
  hold 静默挖掉营火（dirt 14/15 捕获）；修 = 邻格顶面挖（step-up
  爬泥 + 垂直下打 + fall 回 + g.y=53 诚实 track）。⑬ **mid-coast
  1-m 横向 jog = 漂移放大器**（s7 三轮 + 游泳入口两实例）：2-m 腿
  释放滑行中换向 jog 几乎不执行 + 后位腿继承 0.2-0.5-m 偏移 +
  4-m 东腿 coast 的 at(20) settle 不足 = 入口 x +1.77-m 东冲（20.27
  实测 vs 18.5 track）→ 252-f 泳 pin 短停 x6.25 河心 y53 → 后续
  20-m 巡视东腿冲过平台缘 y38 hp13；**纪律 = 关键 aim/entry 站位
  只由 2-m 分块列/行末端到达且末腿与 aim 轴正交（入口列走 z 末
  腿 = coast 仅 z 向，x 列锁定）**。⑭ **倒水/顶面 aim 站位 = 先算
  y(eye−1.62) 穿越点落格**：陡 pitch 射线 y54 穿越 = 站前 0.6-m——
  穿越点所在行决定首 solid（bank 泥顶 vs 沟底石顶）：站位须在
  目标格行内（z-7 沟行 = 0.3-m z 穿越容差），z-5.5 站位（bank
  行）几何不可倒水。⑮ **池格非实心 = 干路选列须验整列身下实心**
  （x18.5 列穿池 z-10/-11 → 落水游泳 3.1 b/s step-in 15f 不足停
  池心 s6 链 FAIL；池西缘走 x17.5 列）。⑯ **demo finished = 模拟
  冻结（still-frame 语义）**：回放达末帧后世界停 tick（退出存档
  记冻结态——s8 段 F+100 存档东倒 6-tick cascade 仅 1 tick；
  dwell 120→240-f 后完整 V 形 arm A ×2 实证）；**纪律 = 最后世界
  作用动作后 dwell ≥240-f**。
- **材料 guide authoring 二十二坑（PITS 417，门 226 v3.36.0 V3-16e ⑧ 材料）**——① **ENT-7 步行惯性 = 恒定 m/s 不存在（十一版 6 种失败形的统一根因）**：ground 行走 = 指数逼近 walkSpeed 4.3（kGroundAccelRate 8.0）+ 松键指数滑动（kGroundDecelRate 12.0，kVelocityStopEps 0.001，player.cpp 358-394）——每腿门到门距离是 hold 长度的函数（33f hold = 2.2173 m 稳态，非 33×4.3/60=2.365 也非 33×3.55/60=1.953）。v3.1-v3.12 的 WALK 常数两代皆错：4.3 名义（v3.5：2m 腿 30f=2.012 近准但 1m 腿 16f=1.073 偏 +7%，叠加旧二进制锁 flap → 实测 14.18m 误归因"3.55 实测速度"）/ 3.55（v3.10 margin0：每腿过冲+11% 乱挖 8/34；v3.11 margin+16：过冲 +27% 飞出平台 0/34 死亡）。**定案（v3.13）= 作者侧 ENT-7 镜像模型**（demo_author.py _walk_dd/_walk_hold_for 稳态表 argmin ±0.04 断言 + Guide._walkV 速度跟踪：全部 cursor 推进处 _coast/_to 滑行记账——非 walk beat 的间隙也消耗残余速度，漏记 = 链式腿 entry 漂移）；模型对探针链 15.4917 vs 实测 15.4922 = 0.0005 m 吻合；混合腿探针（2/1/1.1/3/2 m = 12.1 m）x err +0.0175 m、z err 0.0000 = PASS。② **探针协议**：单长 leg 测的是平均速度（ramp 摊平）≠ 短腿真实行为——探针必须复刻 dmo 的 leg 形状（链式 2m = 33f 主体）+ 混合 leg 类终验；z 漂移读数 = 方向 bias 判据（walk_leg auto-face 精确，15 m 0.0000 m 实证）；**手写 walk_to/walk_leg 必须 `f = g.walk_leg(...)`**（丢返回值 = 全腿同帧折叠，probe1/probe2 曾整批数据作废）；存档 yaw = 连续弧度（mod 2π 判位，-56.5487 rad ≡ 0）。③ **锁 flap 门内捕获（= 真 bug，3 行 C++ 修保留为防御纵深）**：demo 玩家 sim 锁门 = 物理指针锁 relativeMouseMode()（game_frame 的 inputLocked 镜像 →Player::update 丢 mouse delta；game_interaction mine 门【未锁 LMB = 捕获非挖掘】；game_ui UI 组）——用户在机切走 → 失焦 → OS 释放 relative-mouse →未聚焦重抓失败 → 整窗注入的旋转/挖掘被丢弃（W 键不 gate = 行走继续但方向冻结相机 = B1 trench 形签名）。修 = demo 模式（m_demoPlayer 存在）三处强制 inputLocked = true（demo 玩家 = 纯注入，V3-9 设计兑现）；**f0 capture tap 语义变**（f0 down 走 mine 分支 = 1f 挖掘——f0 线 hardness-0 目标会瞬破：场景 f0 线须 air 或 dmo 重锚；probe 实证本场景 ents 0）；聚焦窗位级不变（canary 4 EXACT 执法）。④ **AABB pin 组合几何 = 死路（6 版 pin 方案全灭的教训）**：停块 + 树干 x-pin + 上层 tier 挡射线，在 0.5-1.5 m 步行抖动下逐版以不同形失败（停块不可达/z 带重叠边际/tier 行 DDA 翻转）——**漂移源不除，几何 pin 无解**；除源 = ① 的模型校准（每腿 ≤0.035 m）后 pin 全撤复原。⑤ **花 tap 的 1m 格 DDA 边界**：hardness 0 = 1f tap 瞬破，tap 的射线对 1m 面边界极敏感——leg 过冲（+0.01~0.21 m）把击点推过格边界 = 顺破射线后方（v3.10 的 B5 花 1/6 形）；模型校准后每腿 ±0.035 m 内 = 结构免疫。⑥ **g139_instrument.py 需 `<tree-dir>` 参数**（缺参 = IndexError）+ `python3 script 2>&1 | tail -2 && build && echo READY` 的管道吞退出码 =假 READY（PITS 387 族续）：插桩器验证 = 看 "instrumented ... 27 markers"行 + rc 独立断言，READY 标记后接 `&& echo` 之前必须无管道。⑦ **性能 A/B 旧树重建纪律**：perf 树 = 其自身 commit 的插桩器（PITS 387）——tm_v335（00cb62a）跑自己的 tools/g139_instrument.py 绝对路径参数；当前树 g139 源已插（27 标记，锁修复未触锚点 rc 0）但构建在 arm A 后（ETXTBSY），perf 完成后**须还原**（game_init.cpp = git checkout HEAD；game_frame.cpp = HEAD 版 + 重贴 3 行锁修复；删 g139win.hpp）再重建 + 2998。⑧ **STAND-ON 踩层 tap = 掉层 + step-up = 步行模型外的漂移源（v3.13 诊断实证）**：破掉脚下层（标本在 y53 层 = 平台表层）→ 玩家掉 1m（feet y54→y53）→后续 walk leg 跨越完整层时 stepUp 1m（攀爬帧水平位移 < 平帧）→ 每处 fall+stepUp 损耗 0.3-0.6m 且方向随路几何 = 探针（平直无碰撞链）不可见的校准外漂移。诊断实证（v3.13 dmo）：f7200（B2 前）real vs track (+0.02,+0.10) 贴轨；f11070（B1 wood(7) beat）real (8.323,-16.3) vs track (7.5,-15.5) = (+0.82,-0.80) 45° 1.16m；f12100 real (10.544,-15.7) vs track (9.4,-18.5) = (+1.14,+2.8)——B2 区两次 sand STAND-ON 掉层 + 两次 step-up 的累积。定案 = **一切标本 tap 站相邻完整层 side-above**（aim 标本顶面中心：射线过 y54 平面时入标本 cell 的 DDA 核证），场景几何不得让玩家站在将破的格上——drop/step-up 是 ENT-7 门到门模型（平直链标定）的适用域外，结构回避而非参数吸收。⑨ **aim 点必须逐 beat 核在面/顶面 cell 带内**（v3.13 实证：wood(4)/(10) 的 aim z-18.5 在 +X/-X 面 z 带 [-20,-19) 外 0.5m = 射线水平掠过面南侧打空；雪 (9,53,-5) 的 aim (9.5,54.0,-4.5) 从 (9.5,-5.5) 发射 = 过 y54 后入 (9,53,-5) cell 若已破则穿透打石 partial = 冗余且错）。DDA 手算核证纪律：aim 点 ∈ 目标 cell 的面带/顶面带 + 射线首个 solid cell = 目标（逐 beat，含 over-dig 尾的后续 cell）。⑩ **tour 走线 × 场景几何 AABB 碰撞 = 固定腿长偏移的批量源（probe7 插桩定案）**：走线穿过 y54/55 实体 cell（树干/仙人掌/矿丘）→ 玩家 AABB 接触停墙（wall-slide）→ 后续固定长度腿保持偏移（track 不知碰撞）→ 一切 beat 从偏移位 aim 打偏（arm A v3.14 的 B1 出口 z-18.0 撞 wood(4) trunk →卡 x4.3 → 8.2m 偏移 → B3 0/6 + 雪 patch (4,53,-6/-5) 破 = 签名）。插桩签名 = W=1 ∧ vx=0.0000 ∧ x 冻结 ∧ z 沿墙微漂（probe7 f96 实证）。**审计纪律**：tour 全走线 × 场景 y54/55 实体做 AABB 带（±0.3）重叠核（cell 范围 [c,c+1) 严格不等式）：花族 solid:false 排除 / 先破后走的标本排除（B6 萤石假阳）/ **破树干留上层 y55 完好 = 仍挡身体 y54-55.62**（出口线须走 trunk z 带外 ≥0.2m）/ 0.2m 边际在 ±0.035 校准抖动下可用。⑪ **探针的 Guide pos ≠ 世界 pos**：dmo 的 world spec = 场景存档（玩家起始 = 场景 player pos），Guide(pos=...) 只进作者 track 不进世界——探针 pos 须 = 场景 player pos，否则 track/世界错位探针作废（probe4）。探针走线也须过碰撞审计（probe6/7 撞矿丘作废）。⑫ **cell 范围读反 = aim "修"反**：cell (x,y,z) 覆盖 [x,x+1)×[y,y+1)×[z,z+1)——trunk (4,54,-19) 的 z 带 = [-19,-18)（非 [-20,-19)）：v3.14 把 wood(4/10) 的 aim 从 z-18.5（= 面带中心）"修"到 z-19.5（= 面带外 1m）= 误修（面 z 带读反 1 cell）。aim 点的 DDA 核证必须用 [c,c+1) 语义。⑬ **demo 确定性 = 物理锁读取全门控（③ 的 3 处之外，canary RAW diff 捕获的 7 处）**：canary 4 demo 新 vs 旧二进制 RAW 截图 byte 35/36 差异 + savenorm EXACT = 像素面分叉但 sim 位级恒等 → 渲染/会话态成因。example 全帧空间渐变 diff（909631 px，四象限 meanΔ 系统性偏移）= 视角不同非文字叠层→ **C 相机会话态**：game_ui C toggle 直读 `m_window.relativeMouseMode()`，Mesa 无头物理锁 flap（focus/grab 环境非确定）→ 同一 C 事件（example f255-260 / tutorial_build ×6，sdl_key 99 实证）一 run 处理一 run 丢弃→ cameraMode First↔ThirdBack 分叉（不存档 = savenorm 盲区）。审计全部 12 处 relativeMouseMode() 读取，7 处须 demo 门控（sim 面或截图确定面）：C 相机（会话态）/ T 教程（存档面）/ HUD locked（截图面）/ 进食通道（存档面）/ ESC 退出（环境暴露面）/ 手臂渲染 / 准星渲染（后两者 = tutorial_dig/fly 无 C 亦 RAW diff 的成因）；2 处有意不门控：脚步音（仅声音）/ net FrameState（与 demo 互斥）。修 = 全 10 处 `(m_demoPlayer ? true : ...)`。**纪律 = demo 玩家路径上的一切物理状态读取必须经 m_demoPlayer 门控——指针锁定 = 环境属性，不得进入 demo 的 sim/渲染面；新读点引入时按 12 处 grep 清单审计。**⑭ **掉落物 120s 消散 + 磁力纯水平 = 坑内掉落必失（guide authoring 的收集红线）**：item 寿命 kDespawnSeconds 120s（7200f，MC 同款）+ 拾取 =物品 bottom-center ↔ 玩家 bottom-center 3D ≤1.0（bob 0.05 纯渲染）+磁力位移 y 分量恒 0（"ledge-gap pull is the player's to walk down"）→1m 坑（y53 地板格破碎）内物品：垂直恰 1.0 + scatter → 恒 >1.0 =站坑正上方也拾不到 → 120s 消散 = 库存缺失 + entities 0 的签名（门内实证 19 件：dirt 2/sand 4/snow 8/cobble 4/spruce 1）。**定案 = 一切标本破格须使掉落物落玩家层面**（y54 凸块范式：花/羊毛/仙人掌/树干节拍实证收集）——地板格标本改凸块化而非"破后下坑拾取"（坑内拾取= 步行模型外的 fall/step-up 段 = 校准外漂移源 + 1m 坠伤边界）。⑮ **B4 链破 = 工具切换时序的隐藏变量**：g.select(f, 2) 只在第一列 stone 节拍后切一次 → 后续 stone 节拍全带镐（2.5s = 150f）：镐 stone 0.5625s + ore 1.125s + 后排 stone 0.5625s = 2.25s < 2.5s →单节拍链破 3 格（过挖 4 格的根因）；x16 列 = select 前裸手 stone 节拍 = 干净（同形 12 版未察觉 = 列间工具态差异）。**审计纪律**：逐节拍 tool 状态必须从 dmo 的 Digit 键事件推（SDL3 键码 Digit1=49/Digit2=50/.../Digit0=48——48 非 Digit1），hold/时长仿真须含 wrong-tool ×0.1 罚（SURV-8）：镐对 wood/grass/sand/snow/cactus = 0.2 速度（15s/0.9s/0.75→3.75s/0.15→0.75s/0.6→3.0s）。⑯ **aim 点落 4 格角点 = DDA 跳格**：aim (10.0,54.5,-18.0) 恰在 x+z 双平面交点（ray 同帧跨两轴 → DDA 双轴同步进 → 目标 cell (10,54,-19) 被整体跳过，首 solid = 后方地板）——aim 点必须至少一轴严格在目标 cell 带内（面中心/面内点：(10.0,54.5,-18.5) 的 z 严格内部 ✓）。look_point 的 atan2 无此保护（纯几何），审计的 DDA 仿真须复刻游戏的同帧双跨行为（tMax 相等时多轴同步进）。⑰ **作者行走模型稳态链假设 = rest-entry 腿系统性短走 ~0.19m**（gate-226 根因 7，12 轮根因迭代的最后一坑）：_walk_hold_for(d) 用 steady-chain 表（80 次迭代 warmup 的 entry ≈ 0.71 m/s = 前腿滑行残留）解 hold 帧数，但 break_at 的 90f+ 静置后 _walkV 已被 _coast 衰减到 0——rest-entry 腿用 chain 表的 16f 实走仅 0.918m（v=0 起步的 16f 指数逼近 + 9f 滑行），系统性短 ~0.19m；v3.19 的 B1 出口 1m 腿实测 +0.197m 偏移（track 假设 1.0m 精确到达）= 该坑的实机形（方向/符号随前后腿组合）。chain 行为本身是校准过的（7×33f 链 15.4922 vs 模型 15.4917）——**修 = hold 求解必须按实际 entry 速度**（_walk_dd(n, entry) 逐 n，entry = 跟踪的 _walkV）：rest-entry 1m=17f / 2m=31f，chain 16f/30f 不变。教训：镜像模型的状态变量（entry 速度）必须在每个调用点传真值，"稳态表"只对稳态调用合法；段验证的 pos ±0.5m 金丝雀是此类坑的最终兜底（S1 实证 0.05/0.07m = 修后精度）。⑱ **磁吸 = 速度模型 + ready 后当帧即拾（无停留要求）**（旧理解两处皆错）：pickupMagnetRadialSpeed = 6×(1-d/2.5)² 是**速度**（非加速度，d>2.5 恒 0）；前 0.5s（30f，MC 10-tick 同款）pickupReady 假 = 不吸不拾；ready 后 d≤1.0m = **当帧进库存**（路过一帧即可，无 dwell）；item 底中心 ↔ 玩家底中心 3D 距。推论：stand 距 drop 2.0m 的磁吸需~2.9-4.7s 站定才够（v3.19 wood(10) 3.3s 站定窗 + 0.5s ready = 丢失实证）；2.24m 恒不可行（19s）。**guide authoring 的收集红线 = 每个 drop 在 T0+30f 后必有轨迹帧 ≤1.05m**（stand d≤1.0 或 detour leg 到 d=1.0 点；3.0m 的"路过顺收"不可依赖——v3.19 的 wool(27) 3.16m 靠后路恰好 1.0m 点 = 侥幸形，v3.20 全部显式化）。⑲ **自家树干 y55 层拦体带通过线**（B3 z-4.5 线 2 处碰撞 = 审计捕获）：破 y54 底格后 y55+ 层仍实——玩家体带 y[54,55.8) 与 (55,56) 层 y 重叠 0.8m → 同柱任何 z 线在层格 z 带内 0.2m+ 通过 = 碰撞（非角接触级，是 0.6m 正重叠）。审计的 SOLID 集必须含 y55（碰撞带 y∈{54,55}，不只是 y54 凸块）；"破后通过"只免除被破格本身，层格须绕（z-3.5/z-5.5 类线或 0.8m+ 净距列）。⑳ **分段前缀验证的 trim 纪律**（gate-226 用户定案形，≤30 段）：前缀 dmo = timeline 过滤 at_frame≤F + F 处 wheel-0 事件延长按（长度= max(at_frame+1, hold.until_frame+1) 的 C++ author 口径）；**跨 F 的 hold（at_frame≤F<until_frame）= 边界非静默点 = 响亮拒**（F 取 break settle 尾 / walk settle 尾 / observe 尾等返回帧）；边界帧的"finished" 行 = 段完成权威。前缀回放 = 全跑逐帧恒等（确定性）→段 PASS 的结论可传递（前段不重验）。首 FAIL 即停 = 上游修复后仅重跑失败段及以后（省 2.5-3h 全链）。㉑ **角进入速度的轴归属 = 向量模型的最后一块**（gate-226 根因 8，v3.21）：行走速度是**向量**——拐角处进入速度沿旧轴（垂直于新腿）：新腿轴分量从 ~0 起步（scalar 模型假设 entry 沿轴 = 每腿沿轴短走 0.02-0.16m + 旧分量在 W 期间 (1-FA) 衰减 = 横向漂移 0.07-0.08m/角，25 腿累积 0.7m = B4 段脱轨：coal 未破 + 地板过挖 + 位置偏 0.71m）；rest-entry 腿的 per-frame 计算本身精确（17f +0.010 实测 vs +0.0096 模型）——偏差不在帧循环在 entry 归属。修 = 全向量化（轴逼近 T /垂直 (1-FA) 衰减 / coast 双分量 / 负轴符号翻转【axis>=1 条件把 z 腿当 x 腿 = "11m-at-8f" 响亮断言捕获】）+ 诚实落点（不再强制航点）+ at()/settle 间隙滑行带位移（S1 +0.052m 实证）。**可表达带**：同向 coast-entry ~0.7 m/s 时 8f 最小腿 = 0.66m——sub-0.66m 残差不可 walk（0.16m 微腿 = 响亮断言），walk_to = 主导轴 2m 分块 + <2m 精确残差 + 横向 ≤0.15m snap（下一主轴自校正）。教训：镜像模型的**状态必须是游戏状态的同构**（向量 vs 标量 = 角几何的隐假设），"校准到探针"只覆盖探针的形状（同轴链），换形状（角/残差/间隙）就露馅。㉒ **新增 leg 必过审计："线在格 x 带内" = 该轴线禁穿该格 z 带**（gate-226 v3.21/v3.22 两犯）：B2 的 sand(16,-16) 格 x 带 = [16,17)——任何 x∈(15.7,16.3) 的竖向线穿过其 z 带 [-16,-15) = 碰撞（玩家 AABB x 带与格 x 带重叠 0.4m+）：v3.21 修 x15.5 线时新增的 4m 北腿又放回了 x16.5（= v3.19 的合法形——v3.19 从 x15.5 横向入 stand 故 x16.5 线只走 z-14.5 一段不穿 z 带；v3.21 的新 detour 把 x16.5 线延到了 z-18.5→-14.5 = 穿带）→ 审计捕获。**纪律 = 每条新增/修改 leg 逐条过碰撞审计（含时序 intact 判定），"抄旧版合法形"须核对其前后文（approach/exit 不同 = 同一线段合法性不同）。**
- **皇宫 guide authoring 十三坑（PITS 416，门 225 v3.35.0 V3-16d ⑦ 皇宫）**——① **creative scene = 入场飞行（applySave setFlying(true)，V3-11a）+ 飞行 toggle 的三段诊断链**：**根因**：scene creative=true → 载档即 fly ON（"a creative's load's flies" = G toggle 的孪生）——**任何 creative scene 的 demo 开场飞行态是隐含前提**（v6 实证：墙段「地面」walk 实际 airwalk 7.12 b/s = +0.89m/1m leg 累积 15m 漂移 → 绝对 aim 偏 → stray 网格 + 期望格 2/138，f~7200 autosave 快照 player (23.09,-2.3) vs track (7.5,0.5) = 超台缘 x20 飞出平台）。**v4 链**：入口 Space 双击（4f = 276ms，solo 墙钟窗 0.3s——120s autosave【kAutoSaveDefaultSeconds 默认开，v12 config 无 save 节】的帧 stall 跨 tap 对即 flake）侥幸入窗 = toggle fly OFF → 墙段地面 92/92【"v16c 地面域"实证 = v4 侥幸面】→ 塔段 fly_up 双击 @785.6s（780s autosave 后 5.6s）flake = fly 保持 OFF → 17f「飞行」hold 按 4.3 = 1.22m/2m 段 + 地面仰视 t3/t4 几何不可能（eye 55.62 < t2 顶 y56）= 塔 0/8 + 地板 0/38。**定案（v7）= 飞行 ON/OFF 全走 gamepad B 单按**（kDemoSyntheticGamepadWhich 1 + SDL_GAMEPAD_BUTTON_EAST 1 → mapGamepadButton → FlyToggle：无窗、无 jump grant、sim 帧确定——demo pacing 与墙钟解耦【sim 帧恒 1/60 dt，stall 只延帧不改 sim 时长】，唯一墙钟敏感面 = 双击窗，B = 结构性免疫）：入口 B @f20（fly OFF）+ 4 处飞行 toggle；timeline 词表拒 gamepad（author v1 = 键鼠）→ dmo 后贴 side-car（⑥）；帧停 f1075 地面域验证 x -0.014m。② **飞行校准 = 7.12 b/s 实测（名义 6.88 = walkSpeed×kFlySpeedMultiplier）**：17f hold = 2.0112m/2m 段零累积——飞行 v 由 isDown 直设（无注册欠程，区别于走路 +2 margin）→ hold = round(d/FLY×60) 自锚定，误差 ≤0.03m/段；垂直 kFlyVerticalSpeed 4.0 b/s 精确（65f = 4.33m，无 jump 0.33 = 脚 58.33 眼 59.95）。③ **脚手架 step-up 链 = 死路（飞行悬停放定案）**：S2 楼梯格从任何可达位都放不了（站 S1 顶时玩家 AABB 恒与 S2 格 z 带重叠 → canPlaceAt 静默拒收）+ 角落 tie aim（x-cross 与 y-cross 同 t，±0.1 偏移翻转 DDA）+ creative 30f 即挖连锁（duration 0 每挖掘帧瞬破 = hold 连挖 2 深洞穿平台卡巡视）——全砍；t3/t4 = 飞行悬停（脚 58.33 > t4 顶 y58）aim 目标 t2/t3 **顶面**（aim 点在面平面上 → 命中与眼高/横移无关，8 aim 逐射线核证，眼 59.95 vs 60.3 鲁棒）。④ **帧停诊断法**：--frames 钉 beat 后 1-20f + --save 读 journal diff vs 场景 = 该帧地面真值（终态 diff 区分不了「没放过」和「放过又被破坏」）；**autosave 陷阱** = 中途覆写 save 文件（~120s 节拍）——只认收尾 "World saved" 行的档。⑤ **scene inventory/inv_grid 严格对象形**：slot = {"count": N, "item": "name"}（空槽 {"count":0,"item":""}）——存档 strict parse 面，字符串/数组形点名拒收；mk_scene.py 的 slots 三元组 [slot,item,count] 展开须守此形。⑥ **dmo 后贴的字节 parity 纪律**：demo_fly_patch.py 复刻 game gzipEncode（pinned 10B 头 1f8b0800000000000003 + raw deflate level 6/mem 8 + CRC32/ISIZE 小端）+ Json dump（键排序 + compact 分隔 + 整数 double 整写 + 短回环其余——本 dmo 值域内与 Python repr 恒同）；**no-op 自校每次跑断言字节恒等**（zlib/JSON 漂移 = loud fail 非 silent）；插入保文件序（base = (frame,action,part) 稳定排序——down@f 与 up@f+1 各插**自己帧**的首位，共享 index 会让 up 落在同帧 no-op wheel 前 = 乱序破 frameBegin 前缀和）。⑦ **demo 玩家通道 = 纯注入（用户活动在机免疫）**：m_input 不 pump SDL（仅 audience 通道 m_audienceInput = orbit/wheel 变速/C 视角/ESC 暂停 = 渲染面）→ 回放对物理鼠标/键盘零敏感；**唯一例外 = 帧节奏**（vsync 抖动影响墙钟双击窗 = ① 根 + fps ≥10 下限）。⑧ **checker stray 判定 = 名称比较非存在性**：「不在场景即 stray」漏 air→block 替换（场景有 air 条目、存档变 block = 同名格不同物）——stray = 存档格名 ≠ 场景格名（y≥50 带）+ 平台完整性检查（y50-53 平台石被挖 = creative 连锁坑的终态签名）。
  ⑨ **飞行 aim 的双坑（v7/v8b 各 0/8 实证）**：(a) **kReach 面**——aim 点在目标面上 = 几何正确 ≠ 射程正确：B tap 无 jump 0.33 = 眼 59.95 时 8 aim 全 4.12-4.15m > kReach 4.09 = 静默 0 放置（v4 侥幸版含 jump = 眼 60.28 = 4.05m 内，差 0.33m）；**hover 高按射程反解**：脚 57.0（45f 升）= 眼 58.62 = 8 aim 2.2-3.0m 余量 ~1m。（b) **track 的飞行高度 = 隐形坑**（v8b：射程修了仍 0/8）：builder 的 Guide 追踪（self.y）不随飞行变（fly_up 只发 space hold 不更新 y = 54）→ 飞行中的 look_point/look_at 的 pitch 从**地面眼 55.62** 算（应 58.62）→ SW t3 的 aim 变成**仰视 +0.29 rad**（应俯 -1.12）→ 射线朝天上飞 = 射程内无 solid = 静默 0 放置（dmo 事件实证：dy -76.8px 仰 vs 修复后 +354.3px 俯）。**纪律：飞行段的 track 须同步 y（fly_up 尾 g.y=57.0 / fly_down 尾 g.y=54.0）+ 飞行 aim 的 dmo dy 符号 = 俯为正**（pitch 修正 dy=+ = 视线下降）。
  ⑪ **行走漂移 = fps 函数（reg loss 1.6-2.4f/16f leg 变）+ 地板线 0.1-m AABB 裕量死局**：WALK_STOP_MARGIN=2 按 reg 2.5f 标定（16f leg 实走 0.97m）——实测 reg 随帧节奏变（14.5-fps 单跑 ≈2.3-2.4f → +0.01-0.02m/leg；6.3-fps 并行跑 ≈1.6f → +0.036m/leg）：门 fps 下 38-leg 地板累积 +0.2m 即破东列 0.1m 裕量（line x13.5 AABB 边 13.9 vs 目标带 x14：v7 +0.21 漂移 = 0/7 canPlaceAt 静默拒）；低 fps 下 +0.5m 漂穿越格宽 = 北 x3 的 aim 落 x4（x4 占位后 x4 beat 拒 = x3 永缺——帧停实证 (4,54,-10) 在 x3 RMB 后已存在）。**纪律：放置线对目标格带的 AABB 裕量 ≥1m（2-m aim 内圈线：东列 x12.5 定案）+ 漂移按门 fps（单跑 14.5）估算非并行 fps**（并行帧停 = 漂移放大 2× 的诊断工具，非门节奏）。
  ⑩ **游戏窗焦点 = 用户在机的演示臂杀手（v7 冻结 + 截断实证）**：用户在机时游戏窗可成 _NET_ACTIVE_WINDOW → 击键直达 audience Input：**ESC = demo 暂停**（still 分支：sim 冻结 m_demoFrame 停走 = player 冻结 + yaw 冻结 + W/RMB 全不应用的精确签名，非 panel/pointer bug）+ 窗口关闭 = 截断（**rc 0 + "World saved" + "shut down cleanly" 的假健康**——无 "Demo replay finished (N frames)" 行 = 截断，臂作废）；"Pointer captured" 行出现在 timeline 无 LMB 的帧 = 用户点击窗口的面（audience 面，无 sim 影响）。**防护 = isolate_win.sh**（游戏窗 _NET_WM_DESKTOP → 用户桌面之外的桌 4，映射后 4s 移窗；S1 的 679×744 base19 几何留常桌 ~6s 暴露可接受）+ **每臂 grep "Demo replay finished"**。
  ⑫ **走线在墙体内 = 墙 1-m 厚双向带（v9 + v10 双门内捕获：两条"绕行"线都是墙隧道）**：v9 NE 走线 z-10.5：t1 北墙 (x,54,-11) 占 z[-11,-10) × y[54,55)——走线 AABB z[-10.9,-10.1] **在墙 z 带内**（0.8-m 重叠）+ 脚 y[54,54.8] 在墙 y 带内 → x 方向"0.1-m 裕量"无意义（墙 x 带连续 x2..16，AABB x 宽 0.8 恒在内）= 整条 16-m 走线 = 墙隧道（卡死 → 后续 NE/SE 塔 aim 偏 14-m 全失效 → 级联出平台地形）。v10 "修正" z-9.0 绕行 = **同族孪生**：AABB z[-9.4,-8.6] ∩ 东墙 (15,54,-9) z 带 [-9,-8) 0.4-m 重叠 + 脚 y 带重叠 → 16-m leg 卡 x14.6 → NE/SE 0/4 + 出口/tour 从卡死位级联出平台（终位 (-10.82,53,2.41)）。**根律：内圈任何 z 线 +X 走必撞东墙 x15（x15 连续带 z-11..-1 全封）——环内只有 0.1-m 墙线（x0.5/x16.5/z0.5，92/92 实证）与四角点安全；跨角走线必须出环**。绕行线三轴核（v11 定案 z-12.0：北墙 z 带外 0.6-m / 平台北缘 z-13 外 0.9-m 支撑 / 端头 x0.5+x16.5 = 实证墙线）。对比安全例：x0.5/x16.5 线 vs 墙 x1/x15 带 = 线在墙 x 带外 0.1-m（AABB 0.4 < 0.5 半厚 = 出带）✓ 92/92 实证——**"线在墙带内 vs 外" = 0.1-m 裕量与 0.4-0.8-m 重叠的天壤之别，逐线三轴核，一条 z 线要核北墙 + 东墙两面**。
  ⑬ **终态 checker 的集合成员核对齐元数（v8c 起 6 版假阳性）**：stray 判定拿 save 的 3-tuple (x,y,z) 去比 4-tuple (x,y,z,name) 的期望集 = 恒 not-in → 皇宫 100 格全列 "stray"（v11 100/100 实为 PASS 却打印 FAIL，靠肉眼滤"负 x 族真 stray"才没误判）；修 = 期望集先投影 {e[:3]} 再比。**纪律：同一文件里两种形状的键（坐标 vs 坐标+名称）做成员核对齐元数，或统一用投影后的坐标集**。 （门内补记：pkill -f 自杀——kill 命令行含 pattern 时 pkill -f 匹配**自身 shell 包装** = "Killed by SIGTERM" 假象；pgrep -f 同 pattern 自匹配假 "still alive"；kill 后验证用 `pgrep -x tokenmine` 或独立 pid + 二次确认。）

- **guide authoring 三批七坑（PITS 415，门 224 v3.34.0 V3-16c ③
  房子+床）**——① **场景出生点与 prepared 容器格 xz 重叠 = 静默
  顶出**：scene player (0.5,54,-2.5) 与 table (0,54,-3) 同 xz 带
  → 载入把玩家**顶到容器顶 y55（无任何点名日志！）**→ track 的
  y54 眼高全偏 + 开表 RMB 打空在石顶放 stray + `close_panel` 的 E
  边沿**开起 inventory**（玩家物理冻结 + 指针释放）+ 其后的
  `capture()` 无效（panel 开时 LMB = 面板点击非捕获）→ 11 panel
  problem + 下游全死。纪律：场景 spec 的 player pos 不重叠任何
  solid 格（含预置容器）；**顶出无日志 = 取证盲点**——probe 位姿
  第一手（feet y 非整数带 = 顶出/坠落信号）。② **地面眼 y55.62 <
  y56 = 二层侧/底面截断**：低于二层顶（y56）的一切屋顶顶面 aim，
  射线 y56 crossing 落在二层墙 xz 内 = 命中侧/底面 → middle 槽
  stray（(x,55,-5) 填充）/ 已占拒收。屋顶可行面谱 = elevated 眼
  （y57.62）顶面 + 已放 roof 链 ±X 面 + 门缺口列 middle +Z 仰面
  （y56 crossing 须落 south 空格）；**门缺口列（x3 两层全 air）=
  唯一可同时站立 + 仰面放南行的站位**。③ **坠落腿的 W-hold 覆盖
  坠程 = 漂 0.5-0.8m**：16f hold 覆盖整 2m 坠（~20f）= 坠中 5-7f
  的 W 漂 0.35-0.5m + 屋顶 walk z stop 0.3 过冲 → 落点带 0.7m →
  下游挖位/aim 全偏（S1/S2 未挖 = C7 门链卡死；aim 上树
  leaves）。纪律：**hop 化**（8f W 提前释放 + 自由坠无 W 无漂）×4
  段收紧落点带 ±0.3，挖 = 落点原位。④ **长 walk leg 损耗
  0.4-0.5m（accel + stop margin）**：3.1m leg 实停短 0.46m → 下
  游 +X leg 的行 z 落进一段墙 z 带（0.1-0.3m 重叠 × 0.01m x 重叠
  即 clamp）→ 整链卡 x1.7。纪律：长 leg 目标过补偿（z-2.4→
  z-1.8）+ 终点 leg 用头位 clamp 收口（门停 = 二段北墙头位
  z-4.7 鲁棒）。⑤ **屋顶 walk 的 z stop margin 方向不定
  （0-0.5m）**：链面 aim 的 y57 floor-crossing z = z_eye +
  1.62×dz/dy —— 边距 1cm = 1cm 生死（同形 aim 两次踩 floor
  stray (4,58,-6)）。纪律：aim 陡化（dz 加大 + aim y 抬高）=
  crossing 偏移 0.84→1.19m，全 z 带 19-25cm 边距。⑥ **场景
  journal 容器态三处同步**：移/增容器 = blocks 条目 + tables 条
  目（per-block 网格）+ 该格 air 条目移除——同格重复 = init
  "duplicate block" 拒收；tables 缺块 = "save 'tables' has an entry
  at (x, y, z) which is not a crafting_table block" 拒收。
  mk_scene.py 需 tables/furnaces 透传（16c 新增）。⑦ **gzip 场景
  资产 Python 直读 = UnicodeDecodeError 0x8b**（PITS 386 先兆的
  16c 复踩）——gate 脚本/probe 一律走 gate_compare.load_save 的魔
  数探测。
- **guide authoring 二批四坑（PITS 414，门 223 v3.33.0 V3-16b ⑤⑥）**
  ——① **f0 捕获点击的 hold 尾部 = 挖掘**：5f LMB（down f0/up f5）
  = f0 捕获（!locked 分支 return）+ f1-f4 **locked 挖掘**——起姿射
  线穿硬度 0 作物（wheat 1..7 全硬度 0）= 无声瞬破 3 株（f60 无
  demo 对照跑完好 vs demo f300 缺株 = 二分定位；16a pilot 起姿朝
  树 = 硬度 2.0 无瞬破故未暴露）——**capture() 定案 2f**（down f /
  up f+1 = 零 locked-mining 帧，场景起姿任意安全）。② **scene
  journal 同格重复**：平台石 y53 + farmland y53 同格双条目 = init
  点名 "duplicate block edits"（存档 strict parse 面）——挖洞再铺。
  ③ **1f vs 2f tap 的硬度 0 连锁**：每挖掘帧 = 射线首 solid 瞬破
  （duration 0 当帧 break）：4f tap 顺破 2 株 + **f3000 后年轻麦
  全熟（帧锁定 360 拍/级 roll）变硬度 0 = 射线后方也瞬破**（wheat_1
  混入库存污染合成节奏 = h1 wheat_1×3 实证）——作物 tap 定案
  **hold=1**（1 破帧 = 恰 1 破）；硬度 > 0 的块 4f tap 无连锁
  （续帧 < 破碎时间）。④ **perf A/B 的 cwd 陷阱**：`cd A && cmake
  --build A; cd B && ...` 后 `;` 接的 run 若不带显式路径 = B 的
  二进制——旧树臂误用新二进制且 banner 同版不可辨（两次 v3.32.0
  输出同值 = 假 A/B）——**perf 跑显式全路径 + 独立 cwd**（本门旧树
  臂重跑才入档）。
- **authored guide demo 编写十九坑（PITS 413，门 222 v3.32.0
  V3-16a 扩展指导 demo 族 pilot ④）**——① **demo 须初始 LMB 捕获**：
  首事件 = 帧 0 的 LMB tap（指针锁定 = look/面板 E 的门；未捕获的
  LMB = 纯捕获不挖掘——f0 缺捕获 = 全 demo 面板打不开）。②
  **closePanelFor 不重锁指针**（开面板释放 = 设计）：关面板后的
  世界动作（RMB 开容器/放置/下一 E）须先补捕获点击——Guide DSL
  `close_panel` = E tap + 25f 后 capture 合成。③ **look 符号**：游戏
  `yaw -= dx*sens`——+dyaw 的转向要发**负** dx（`dx = -dyaw/SENS`）；
  sweep/look_rel 首版符号反 = 全 demo 视角反打（f500 探针 yaw +0.3
  vs 追踪 -0.3 对称暴露）。④ **硬编码帧 × 动态行走重叠**：P2 的
  860 硬编码帧 vs P1 折线延长 = look 在 walk hold 期间发射 → 玩家
  沿旧 yaw 走 5m 乱闯（截图树挡路实证）——beat 一律 `f = g.cursor`
  动态链。⑤ **斜行子轴分量被物理丢弃**：45° 走 = 4× 帧损耗（子轴
  碰撞消解逐轴量化）——全部行走轴对齐（leg 断言 |dx|<0.05 ∨
  |dz|<0.05）。⑥ **walk_polyline 须同步 self.cursor**：局部 f 走完
  不回写 = 下一 beat 从旧 cursor 起步（与 walk 重叠的根）。⑦ **hold
  注册损耗标定**：23f→20.6 / 15f→12.8 / 103f→100.4 有效帧 = 每次
  hold 起始 ~2.5f 不动——stop margin +2f → 实际短 0.03-0.11m（格内）；
  追踪模型 = 精确角点到达（相对瞄准吸收短差），**非**按 emit 帧满推。
  ⑧ **放置瞄地面非块中心**：瞄块中心射线飞 0.5 过 → 落邻格（表放到
  (4,-7) 非 (5,-7) 实证）——`place_at/look_ground` 瞄顶面点。⑨
  **绝对格瞄准对漂移零容错 → 相对瞄准**：`place_ahead/open_ahead` =
  面朝方向 1m 前（目标随玩家移动：±0.3m 漂移同格；r26 时代 3m
  tracked 漂移 = 放置全败根）。⑩ **行墙挡横向视线**：从 (7.5,-9.5)
  瞄 x4 的射线穿 (5,54,-9) 格 y54.7 = 打错块（x4/x9 漏挖实证）——
  逐格 1m 正北贴线挖（射线只穿空气 y55 格 + 目标格）。⑪ **磁吸衰减
  (1-d/2.5)²**：2.2m 掉落拉力 0.15 m/s² 收集不了（6 掉落留地实证）
  ——收集线 = 1.0-1.5m（z-9.5 贴线 vs z-10.5 隔线）。⑫ **工具态决定
  破碎时长**：砂裸手 0.75s（45f）≠ 镐 0.375s——P2.5 手持是木头
  （h0）不是镐（P8 才造），0.7s hold 差 3f 不碎（TEMP-LMB 瞄准
  全对纯时长不足实证）。⑬ **续挖穿平台（PITS 7 延伸）**：hold 余量
  须 < 下方平台石破碎时间（石 0.56s/34f：余量 ≤0.4s；砂裸手余量
  15f < 135f ✓）——stone 1.0s / ore 1.4s / wood 3.3s / sand 1.0s
  定案带。⑭ **mobs-on config 下镐误击**：生成动物入准星 = +2 melee
  耐久（15 vs 11 实证；targetMobFor 只匹 MobEntity 非掉落物）——
  demo E2E 用 mobs-off config（与 scene 基线一致）。⑮ **furnace
  面板的共享 36 格**：resolver panel id 0（inventory）的开态检查须
  含 Furnace + 其格几何 = **FurnaceLayout.slots**（≠ InventoryLayout
  位置——三面板同 36 格不同 rect）。⑯ **base19 锚 = f60 非 f900**：
  时钟 10:50 反解 t=0.2517（f≈43-60 首稳帧）；900f 跑 = 15937px 假
  DIFF（太阳/树冠/时钟位移，旧树同形 = 环境面排除）——S1 锚协议
  帧数从阶段文档取，勿从性能臂借（900f 是 PERF 窗数）。⑰
  **`--frames` 须 = demo 长度 + ~500**：finished-panel 跑到 cap
  （28000 = 白等 4.5 分钟实证）。⑱ **场景 journal 同态移除**：破块
  后与 worldgen 同态的格从 journal 消失（行石破 = 终档查不到 =
  正常）——断言走「终档 ∪ 场景」差集非单档枚举。⑲ **scene 资产
  完整性约束**：空槽存档形 `{"item":"","count":0}`（36 条）/
  inv_grid 恰 9 条 / 火块↔fires 条目配对（clear_entities 只清
  entities 不清 fires）——mk_scene 手术按存档 strict parse 回灌
  验，幂等（双跑字节恒等）。
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
- **REJOIN 快照面 + 幽灵/renumber/accept 节拍 + journal live-diff +
  respawn 0.5-y 八坑（PITS 412，门 221 v3.31.0 V3-12c2 断线恢复 +
  format 33）**——① **Json(bool) 指针陷阱**：`Json x = obj->find("k")`
  （const Json* 未解引用）= 隐式 bool 构造 → 后续 .at()/.find() 变体崩
  "std::get: wrong index for variant"；一律 `*obj->find("k")`（本会话
  test_durability 首踩，re-pin 族系统性排查）。② **legacyShape 改形不
  改 format 键**：33 format + legacy root 形 = 假 lie（"format 33 must
  not carry a root 'player'"）——当前 format 的负向测试必须走
  withPrimaryEntry（entry 级，format 保 33），legacyShape 仅用于降级
  声明 ≤32 的测试。③ **respawn int-cell 丢 0.5-y**：format 28 的
  int cell triple + bedSpawnPosition 还原对**非 cell-center 值**（net
  spawn 位整数 y = 地面顶）丢 0.5（host c1 spawnPos (4.5,52,0.5) →
  档 (4,52,0) → client 还原 (4.5,52.5,0.5) = hash player1 分歧根）；
  format 33 改精确 float triple，≤32 保 int cell（per-format 解析分
  支 + 测试 legacyShape 重编码 floor）。④ **removal 的 slot 重编号破
  旧客户端**：compact erase 使 slot 移位 → 已有客户端 ring 越界写
  （3 玩家帧 states 写入 2 行 ring = 堆破坏 → 后续 segfault）+ N 增长
  不知情 → 必须 **Join/Renumber CTRL 广播**（行随玩家移、输入历史跟
  走、无冲洗）。⑤ **中会话 accept 节拍**：barrier 停滞的 host 不再跑
  per-frame accept → backlog 连接等 15s（门内实证）——**listener fd
  入 barrier 的 select**（停滞期也 accept）。⑥ **client renumber 必须
  跑 100% drop**（调 netRemovePlayer 非裸 erase）——裸 erase 只 host
  drop、client 无 → ents 分歧（门内实证 c1@420）。⑦ **net 主机载多玩
  家档 = stale 幽灵 → slot/m_nets 错乱 → segfault**：重启后存档的其
  他玩家连接已死、slot 1.. 无连接但占位 → 新客 netAddPlayer(1) 覆写
  幽灵 + m_nets index=slot-1 映射破 → **net 主机（m_args.listenPort
  != 0）抑制幽灵**（玩家集 = {自身}+{新连接}，与 solo 档 host 等价；
  重连者 = 新体 D25，其旧物在 removal 的 100% drop 已在 entities 可捡
  回）。⑧ **feedJournal 须哈希 live-diff**（journal ∪ retained 对生成
  参考的 diff，journalDiff 单线程）非 journal 原始 cell——rejoin client
  回填 journal = 文档 cell、host live journal 含回滚编辑的幽灵 cell →
  原始 cell 集合不等（门内实证 journals 节分歧）；retained 本身即 diff
  逐字入。环境面：本机 Mesa GL init ~10s 恰超 host 10s 首客窗（竞态，
  host+client 同起或 client 迟 2s 入窗）；多进程负载下 client 首帧
  worker 饥饿 15s（PITS 388④ 独占纪律，门臂串行）。

- **accept 窗单次 break + ms/s 混用 + 客户端全槽体 + E isLocal 门
  + typed 'e' 截断 + Mesa 读回竞态 + WM 几何噪声 + 尾帧 broken
  pipe + 屏障 min 水印九坑（PITS 411，门 220 v3.30.0 V3-12c1 大厅/
  聊天/N 泛化/心跳）**——① **accept 窗 250ms 静默尝试单次即 break**：
  首客后窗内无新客的第一次静默 accept 就关窗 → c2 ~1s 后连接 =
  refused 实证；修 = 静默尝试**截止前重试**（`now - windowStart <
  3s → continue`），窗纪律 = 首客 10s + 3s 窗 + WELCOME 窗后统一
  flush（同 N = lockstep 前提）。② **首客 accept 超时 ms/s 混
  用**：`kNetStallTimeoutSec`（秒）直接传给 startHost 的毫秒参
  数 = "no client in 10 ms" 实证；纪律 = 超时常量单位入名（Sec/
  Ms 后缀）+ 传参处显式换算。③ **客户端 netAddPlayer(1) 硬编
  码**（12b 双人形）：N>2 下 slot 2 客户端缺自己的体（localPlayer()
  越界 SIGSEGV）+ 客户端只模拟 2 玩家 vs 主机 3（f0 哈希 desync
  孪生）——修 = 客户端**全槽** netAddPlayer(1..n-1)（lockstep = 每
  机模拟每玩家，player 0 = 主机体 = 各机本机构造）；纪律 = 人数
  泛化时逐处 grep 硬编码 slot。④ **E 面板 isLocal 门 = 主机玩家
  面板 desync**：12b 的 `isLocal &&` 门让客户端忽略主机玩家的面
  板边沿（主机按 E 暂停 player 0 物理，客户端不暂停）——f570 哈希
  点名实证；修 = **E/ESC 边沿对全部玩家处理**（边沿 = 属主机物理
  按键经捕获中继全机对称）+ 窗口/声音动作 **localOwner 自门**
  （`&p == localPlayer()`——远程玩家的面板状态是 sim，其窗口动作不
  碰本机指针/文本输入/声音）。⑤ **typed 'e' 开面板截断消
  息**：聊天打字含映射键（'e'=E 面板）→ 面板 open 动作关 SDL 文
  本输入 → 后续字符全丢（"hello gate"→"he" 实证）+ 键边沿进 sim
  （desync 孪生）；修 = **Input 打字抑制**（可打印域 0x20..0x7e
  在 owner 机 FrameState 捕获前本地吞，ESC/Enter/Backspace/功能键
  存活；抑制旗 on 时清 held 键防卡键）——纪律 = **文本输入期一切实
  键边沿必须在捕获前裁决**（表现/模拟二分在 Input 边界做）。⑥
  **Mesa 无头前缓冲读回竞态**：3 进程并行 load 下偶发截图 = 纯彩
  块垃圾帧（进程轮换波及，单批必有一中）——**权威 = net hash 行 +
  savenorm**（垃圾帧批次哈希/存档全绿），截图 RAW ×2 取各进程干净
  批次；环境面非代码缺陷，不修（前缓冲语义 PITS 388⑧ 不变）。
  ⑦ **WM 平铺几何噪声**：3 窗并发 tiled 尺寸各异（679×245 vs
  679×744）打截图 RAW 面——门臂 config 用 `resizable=false` 钉
  1280×720（D19 requested parity 的截图延伸；base19 锚 = solo 窗
  口不受扰）。⑧ **尾帧 send vs 对端干净退出竞态**：双方同
  --frames N 终帧，主机 frame-limit break 先关 socket → 客户端最
  后 INPUT send = broken pipe ERROR 噪声（模拟已完成 = 无害）；修
  = finalFrame 容错（F+1 == frames 的 send 失败 = 干净退出）。
  ⑨ **屏障 anyProgress 死锁形**：stall 检测用「任一远端达标」基准
  → 健康同伴持续进展遮死者的静默 = 最滞后永不移除永久阻塞；修 =
  **min 水印进展基准**（最滞后者的水印 10s 无进展 = 移除，移除后
  重基准）。- **client 环槽偏 1 + 主机快跑屏障 + RST≠FIN + WM 平铺 + uiScale 时
  序 + seed 豁免 + 无火种子 + --new 净主机 + 起始库存 per-session
  九坑（PITS 410，门 219 v3.29.0 V3-12b 2 人多人 PoC）**——① **lock-
  step 环槽 store 偏 1**：主机侧客户端 INPUT{f} 存 f%3 正确（消费帧
  f+D 的读槽恰合），但**客户端侧收到的主机帧态（属帧 f−D）必须存
  ringSlot(f) 而非 f%3**（读槽 = ringSlot(N) = (N+1)%3，差 1 = 主机
  玩家输入晚 1 帧）——f150 哈希点名抓获（f120 前 = W 按前位差未积
  累，首哈希违例在动作后第 3 个哈希窗）；纪律 = **环槽写读对称性
  用消费帧验证**（写 slot 的帧 X 必须被读 slot 的帧 X+D 消费）。②
  **无头主机快跑吃未发输入**：主机 55fps 跑赢客户端 0.3s 启动偏斜
  → 消费客户端 ring 冷零 = 静默 desync 根；修 = **lockstep 屏障**
  （主机 pre(N) 等 INPUT[N−D] 水印达标，waitPacket 10s 点名超时；
  因果链 N−D < N 无死锁）。③ **客户端干净退出 = TCP RST 非 FIN**
  （进程退出时缓冲区有未读数据 = 内核 RST）：主机 send ECONNRESET
  原当致命 → 会话死；与 poll EOF 同形 = 移除 player + solo 续跑。
  ④ **WM 平铺竞态打 window parity**：无头双窗被 WM 各自平铺（679×
  744 vs 1280×720）= 环境噪声（PITS 388 族）→ parity 改 **config
  请求几何**（requestedWidth/Height，D19 记档）。⑤ **HELLO 的
  uiScale 时序**：client 握手先于 buildScene（种子权威要求）→
  m_uiScale 未设 = 0.0 误拒 window parity；改 config 直读
  （uiScaleFromConfig）。⑥ **world.seed 入 simConfigHash = 误拒**
  （client 本地 config 种子 ≠ 主机 --seed；WELCOME 种子 = 权威 →
  豁免出 parity 键表）。⑦ **无输入臂跨 run savenorm EXACT 需无火
  种子**（fire 熄灭 real-dt 域，PITS 385/390 族——同种子同 run 双
  进程同序 ✓，跨 run 位差 ✗）：seed 11/22 无火 / 33/44/55 有火
  扫描定案。⑧ **--new 须 --save 卡净主机**（净会话世界恒 fresh，
  --new 无 slot 合法 = WELCOME fresh 形的 CLI 门）。⑨ **起始库存
  per-session**：netAddPlayer 空 Inventory（默认构造）= 断线移除
  零掉落静默（门臂 entities=0 抓获）；fillStarterInventory 提取
  buildScene 块共享（hotbar.json items + inventory 段）。
- **负 cell 中心 + 无蛋 ≠ 首访 + demo 死点击 + 旋转 dy 累计 +
  /tmp 路径重写五坑（PITS 409，门 217 v3.27.0 V3-10d2 召唤与复
  杀）**——① **负坐标 cell 中心 = -N + 0.5 非 -N - 0.5**（硬规
  则 1 的 floorDiv 纪律的中心侧）：10d1 的 kEndCrystalSlots/
  kEndTowerSlots 负侧中心全写成 -2.5/-50.5（= cell -3/-51 的中
  心）——晶普查走整数 cell 词表（kCrystalCells）故玩法正确，但
  箭碎扫掠 floor(中心) 在 -X/-Z 侧**静默测错格**（-X 晶箭永不
  碎）；10d2 面晶词表的单测 pin（floor == 意图 cell）首跑抓获；
  纪律 = **Vec3 中心词表入库前逐格 floor 回验**。② **无蛋 ≠ 首
  访**：10d1 生成门「无蛋 → 生成」在玩家破蛋后 60f 把龙无召唤
  复生（MC：破蛋 ≠ 复生，只有召唤复生）——门臂首跑 f381 实证；
  修 = **败亡会话旗**封闭首访门 + 召唤显式延迟（重载复位旗 =
  D6 重载读保持）。③ **demo 同帧 mouse_down+mouse_up = 死点
  击**（边沿同帧抵消——press 不落地）：点击须 ≥2 帧 hold；**hold
  续帧按新瞄准重新取目标**（创造即挖沿射线续破 reach 内下一块
  ——破晶 → 续帧自动破蛋 = 一 hold 两用，门臂据此设计）。④
  **demo 旋转 dy 逐目标累计**（mouse_move dy 改 pitch 是绝对姿
  势增量——两个同 pitch 目标之间只发 dx；门内对第二目标重发
  dy = pitch 过旋 0.16 rad 瞄准落空 1 次返工）。⑤ **门脚本的
  /tmp 路径被重写**（`mv x /tmp/x.bak` 的 bak 落源目录——/tmp
  前缀被吃 = 文件"失踪"假象 + 后续 case 串染）：备份用
  cp+rm 到显式目录（/tmp/opencode 子目录）+ 每 case 后 diff -rq
  资产完整性回验。
- **init 交叉检顺序 + 数值累加器泄漏 + demo 存档红线 + 流送依赖
  锚点 + 哨兵首帧 delta + 同帧 spawn+kill + 血条 margin + CJK
  边界八坑（PITS 408，门 216 v3.26.0 V3-10d1 末影龙）**——①
  **init 交叉检必须后置于所检子系统的 load**：dragon_breath 效果
  存在性检放在 `m_effects.loadDirectory` 之前 = 假 fail 首跑实锤
  （「breath effect missing」，效果根本未载）；wav 检走
  `m_assets.exists`（文件面）无此问题——**检对象未载 ≠ 对象不
  存在**。② **多字段共享累加器 = 缺值泄漏**：readDragonNumber
  缺键不动 out，共享 `double v` 吃上字段值（fireball 缺省吃
  breath_radius 的 2 过了 >0 配对门）——负例单测首跑抓获；纪律
  = 每读前 reset 或每字段独立局部。③ **demo 存档红线实证面**：
  replay 的 save 参考 demo 的 `--save` 被 init reset（PITS 407①
  同款机制的存档侧）= 跑完 slot 文件恒为输入副本——**demo 臂
  无终档断言**（权威 = 截图 + DEBUG 行）；canary（fresh 参考）
  不触发 reset 有退出写。④ **流送依赖的世界锚点须卸载无关**：
  raw `getBlock` 未载 chunk 读 air = 晶普查 0→6 随流送漂（假
  crystalDelta → 假 strafe + 驻坐概率漂移）+ 蛋门未载读 air →
  假复生白嫖洞；正解 = **未载 = worldgen 新鲜 = 完好**（晶不可
  在未载 chunk 破——破路径全读 chunk 态）；applyWorldEdits 走
  getOrCreateChunk 强制载编辑列 = 重载臂蛋门天然正确。⑤ **-1
  哨兵首帧 delta**：census 初值 -1 的首帧「-1→6」≠ 碎晶 = 假
  变化（strafe 触发器）——首帧禁报（`last >= 0` 门）。⑥ **同帧
  spawn+kill 漏账**：局部指针在 spawn 段前捕获 = 同帧死的龙
  不跑仪式账（蛋永不落 → f119 复生死循环首跑实证）；正解 =
  spawn/seam 后重捕获 + despawn 对**任何** dead-boss 保持（含
  ascension -1）+ 记账单收口 game pass（despawn 永不计 boss
  死）。⑦ **血条 CJK 名行 margin**：by=8 的名行 y=8−16=−8 屏外
  裁切（截图首跑实证）——margin 24。⑧ **CJK charset 解析边
  界**：`CJK_CHARSET = [...]` 尾部有意重复项 + `];` 越界匹配
  （s.find('];') 过界到别处）= raw 扫描 433 伪影（真实 430
  唯一）——插入/校验用引号正则 + 真实列表边界（`'\n]'`），
  勿按 raw >127 扫描计数。
- **demo 槽不回写 + 火脚级重叠 + 拾格带含脚下 + 遮挡首格语义
  + 下探穿固体虚空 + 截图尺寸/按住 LMB 七坑（PITS 407，门 215
  v3.25.0 V3-10c 末影人）**——① **demo 引用槽永不回写（假测量
  根）**：demo replay world=save 时 `m_args.saveSlot` 被
  reset（game_init.cpp 的引用槽红线）= 跑完读 slot 文件读到的
  **永远是输入场景副本**（yaw/pitch 恒不动的假象 → 误判
  mouse_move 无效，绕了两小时插桩）；判别 = g213 旧 demo 复跑
  （12 格不同嵌入实证 mouse_move 有效）；**demo 臂终态权威 =
  DEBUG 事件行 + 截图**，勿读 slot。② **火须放脚级（air 层）
  才有严格 AABB 重叠**：mob 脚 y53.0 站 y52 块顶——火格 y52
  与身体盒仅面接触（y=53 边界）= 严格不等式拒（inFire 永不真，
  点燃零实证）；火格 y53（脚所占地块）= 真重叠点燃。③ **拾格
  带须含脚下（dy −1）**：feet 53 的站立体下方地块 = y52；摸取
  扫描带 dy 0..2 = 纯空气层 → 恒不触发（首 probe 零 carried
  实证）；dy −1..1 含脚下才活。④ **遮挡检 = ray 首非 air 须
  == 目标格**：`hasLineOfSight` 的语义是「段内无非 air」——
  目标格自身即首非 air = 恒拒（箭撞击语义）；「mob 看得见
  块」须 `raycastBlock(eye→格心)` 首命中 == 目标格（更近命中
  = 遮挡）。⑤ **瞬移「向下首个 roomy」探针穿固体掉虚空**：
  从候选位下探到首个站得下的层 = 台地实体层之下（y45 虚空）
  → 落点 >40 距静默 despawn（门内实证：#1 f436 后消失，
  无任何日志）；正解 = 候选位本身须空旷（hasRoomAt ∧ 无
  流体）+ **16 格内 footprint 有地面**（任一 solid 支撑 =
  站立约定），过隙候选拒收；全败吞没 = 永久 DEBUG 行
  （swallowed）= 吞没可观测。⑥ **无头截图 = 679×744 渲染目
  标（非窗口 1280×720）**：aim 投影 crop 数学须读**实际图像
  尺寸**（identify）——按窗口尺寸 crop 全偏（门内 4 连踩）。
  ⑦ **按住 LMB 会挖准星所指（创造即挖）**：截图转身前须先
  松 LMB——f908 按住到 f1501 的 LMB 在 f1200 转身瞬间开始挖
  掉落物下方地板（f1203 地板格被挖、item 下坠 1 块 = 视觉
  谜团根）；timeline 纪律 = 观察段无按下键。
- **lattice 位切 int 截断 + JS 三元误写 + stale assets 再踩三坑
  （PITS 406，门 214 v3.24.0 V3-10b 末地内容族）**——① **hash 位切
  直接 int 截断 = 负值模运算**：`static_cast<int>(h >> 24) % span`
  的 `h >> 24` 最高 2^40——int 截断取低 32 位可负 → `%` 负余数 →
  参数出带（半径 -2..12 实测，单测边界断言首跑全炸）；纪律 =
  **模运算先于 int 转换**（`(h >> 24) % span` 再 cast），nether
  盐族位切同纪律普查。② **JS 三元误写进 Python 生成器**（`? :`
  语法错）——纹理生成器是 Python，函数体从 C++ 思维带过来的语法
  杂音须先 `py_compile` 再跑（md5 幂等纪律只在跑成后才起作用）。
  ③ **run 目录 stale assets 三连踩**（PITS 403② 再验）：10b 新
  biomes_end/ 未同步 g213 run 目录 → init rc 1「requires at least
  one biome」= fail-fast 按预期工作（非 bug）；每版资产面变更后
  run 目录 rm -rf + cp -r 重建 + 新目录存在性 grep 回验。
- **场景手术先探地形 + shader 无光分支双 floor + GCC-16 假阳性结构
  隔离 + 存档伪造只剥对应后键 + 场景派生同步 fluids 五坑（PITS 405，
  门 213 v3.23.0 V3-10a 末地维度框架）**——① **场景手术选位 = 先探
  地形**：fresh 世界出生点邻域是丘陵（feet 54 起爬升）——body 层环 +
  地板手术被高处地形埋没（黑屏 + RMB 全落空，假象极似「没瞄准」）；
  纪律 = **7×7 挖深坑**（目标层..70 air + 目标层−1 石地板）平顶化，
  与绝对高度无关（门内首探 → 二探实证）。② **shader 无光分支只读
  u_caveAmbient**：basic.frag `light <= 0.001 → amb = u_caveAmbient`
  ——无光源维度（10a 末地全岛 light 0）的 ambient floor 须 **u_ambient
  + u_caveAmbient 双 uniform 同设**，单设 u_ambient = 岛面仍走
  config 0.03 cave floor = 全黑截图（门内黑屏首因之二）。③ **GCC-16
  -O3 maybe-uninitialized 假阳性族**：新代码增量改变 -O3 内联决策，
  **既有** serializeSlotEntry 的 Json variant 假阳性复发（`-Wall
  -Wextra -Wpedantic -Werror` 下报错但行为正确）；`noinline` 属性
  无效，**结构隔离有效**（大段序列化抽独立静态函数 + 测试 TU 的 cell
  构造抽助手）；判别纪律 = 先验 HEAD 树同编译器干净（真回归 vs 假阳
  性），再动结构。④ **存档伪造真形 = 只剥对应后键**：format-30 伪造
  若用 pre-30 strip 助手（剥 nether/dimension = 30 必携键）= 缺键假
  失败；正确 = 只剥 31/32 键（creative/end/end_entities）保留 30 键；
  pre-32 strip 助手（5 文件本地副本）须随 format bump 扩新键。⑤ **场
  景派生须同步 fluids**：从带 fluids 编辑的 base 派生新场景清 blocks
  不清 fluids → init rc 1「cell occupied」（fluid 落生成固体格）——
  派生时 blocks/fluids/fires 三节同清。
- **git checkout 还原回退未提交功能 + 骑乘场景视线三件 + demo 瞄准
  换算 + Space 双击窗 + 分离 opt-out 五坑（PITS 404，门 212
  v3.22.0 V3-14d strider 骑乘）**——① **`git checkout --` 还原
  temp 改的数据文件 = 未提交的功能加法一并回退**：S6 点名臂
  （temp 改坏 → 跑 → `git checkout --` 还原）把**同文件里未提交的
  功能键**也回退了（v3.21 piglin.json 的 barter、v3.22 strider.json
  的 rideable 两连踩）——后续探跑静默用旧数据（谓词假 = 瞄准全对
  也不触发，假象极似「没打中」）。纪律 = temp 改动用 python 原地
  改 + **手工字符串还原**（非 git）+ 还原后 **grep 回验功能字段
  仍在**。② **骑乘场景视线三件 = 贯通沟 + 桥 + 灯**：1×1/浅沟的
  rim 遮挡（射线降过台面 y 在沟口外 0.09-1.1m）→ 贯通沟（目标列 +
  进深列全挖到流体顶，射线沿线每列空气）+ **桥格**（流体顶 +1 的
  固体顶面 = 玩家起点/骑行通道，免 burn 且同层可行走）+ **灯排**
  （glowstone 高位照亮——18 格上 = 光 0 不可见，6 格上 = 光 9；
  **灯位须对 spawn 光门验算**：strider 15 门，脚格光 ≤15 不变）。
  ③ **demo 瞄准换算**：pitch/yaw 按**体心**（feet + height/2）非
  feet（feet 距 5.54 > kReach 5.0 但体心 5.0 恰界 = MISS 假象的
  反面）；delta px = Δrad / mouseSensitivity **0.0025**（180° =
  1257px 单帧——mouseDelta 无钳制）；LMB 捕获点击**先打空**（下方
  桥面/流体，勿打坐骑 = 1 HP + 击退污染）。④ **Space 落乘须绕过
  双击飞行窗记录**：落乘 tap 若记 m_lastSpacePressTime = 落乘后速
  按误开飞行——落乘分支在记录之前 return。⑤ **分离 pass 顶坐骑**：
  骑手盒（背位起 1.8）与坐骑盒（1.5）设计性重叠 → separateEntity-
  List 逐帧把坐骑从骑手脚下推出；`collidesWithEntities()` 虚门
  （弹道 opt-out 先例）首消费于 mob = `!m_ridden`。
- **demo 观察红线 + run 目录 stale 资产 + kReach 瞄准 + 生成环
  视线 + ambient 耗流五坑（PITS 403，门 211 v3.21.0 V3-14c piglin
  交易）**——① **demo 引用存档槽 = 永不回写**（game_init
  saveSlot.reset 观察红线）：CLI「drop --save or match it」的
  match = 红线不写而非写档——交易臂终态权威 = 终帧截图 ×2 RAW +
  DEBUG 动作行（`bartered with 'piglin' (...)` 逐字恒等 = 帧锁定
  roll 实证），勿等「World saved」。② **run 目录 stale assets**：
  池变体/副本树每次仓内资产改动后必须**整体重拷**（rm -rf + cp -r
  + 重打池补丁）——两连探跑静默用旧 piglin.json（targetMob HIT +
  无 barter + gold 未动 = 数据缺而非瞄准错，假象极似「没打中」）；
  判别 = 拷后 grep 目标数据字段存在性。③ **kReach 边界瞄准**：
  眼-mob feet 距 5.54 > kReach 5.0 但眼-体心 5.0 恰界内——瞄准
  验算按**体心**（feet + height/2）非 feet；校准工具 = 临时插桩
  HIT/MISS + 近 mob 位打印（PITS 383③ 定界后移除重建纪律，门内
  实证 3 轮：MISS(aim 偏 0.47x) → HIT 无 barter(stale 资产) →
  HIT + barter(数据齐)）。④ **生成环视线几何**：scan 序 = dz 降
  dx 升（ring ceil(maxRadius) 圈）——钉目标 cell 须拒**全部
  scan-序先行格**（本门 12 拒生沟：dz=-6 行 1 + dz=-5 行 7 +
  dz=-4 行 4 = 先行有效格全灭，目标 (dx=0,dz=-4) 首中）；1×1
  洞**不可见**（2 深挖洞的 rim 遮挡：射线降过台地顶 y53 的 z 点
  = 洞口远端外 0.09-1.1m）→ **1×3 视线沟**（超环 cell【dist 3 <
  min 4】自由挖，不扰生成）。⑤ **ambient 每帧耗流**：ground
  ambient 每帧 roll uniform01（流态 = 点击帧的函数）——xdotool
  ±帧抖动破 roll 恒等（两次 run 掷出不同物 = savenorm 必裂）；
  **断言 roll 结果的输入臂必用 authored demo**（帧锁定，roll 逐字
  恒等），xdotool 只用于结果不变量型断言（「某物入包」级）。
- **下界决定性场景 + 字体码点笔误 + canary --frames 五坑（PITS 402，
  门 210 v3.20.0 V3-14b ghast）**——① **下界生成决定性构造三件**：
  **环清三带**（kSpawnFloorDelta 2 → 地面 pick 的 floor top 须 ∈
  [player±2]；只清一层沟底 top 仍在带内 = 地面 mob 饱和 cap 吞 ghast
  抽选【scene v1 实证：4 只地面 mob 后 cap reached】；清 y50-52 三
  带 = 沟底 top 50 出带 = 地面 pick 全败 cap 永不饱和）+ **obsidian
  顶盖**（in-air 生成 = 柱内顶向下扫描首个 airCells 全空气格；无
  顶盖 ghast 生 y121 天花板下【scene v2】，顶盖 y58 起 feet 56 =
  kDetectVertical 2.0 眼差 3.0 不可见不发射【scene v3】，y57 起 feet
  55 = 恰线 dy 2.0 可见 = 定案）+ **场景手术必需字段**（player.
  effects format 8+ / absorption format 12+，pop 掉 = init 点名拒载
  ——两次踩坑）。② **mob 不入档 = 「同位重生」假说陷阱**：format 30
  nether_entities 只存 items（设计项，deviation 6）——两次「同 cell
  ghast spawn」= 新抽选落同确定性 cell（前一只漂移出 kSpawnClearance
  1.0），非重生；**死亡反证法** = 死 mob 必留 XP orb（xp>0 恒发，
  120s 寿命）+ 掉落（0-1 roll 可 0）——终档 orb 缺席 = 未死。③
  **爆点 = 火球盒底位**（arrow->position()，非发射眼非盒中心）：正/
  负对照格的判定距离按盒底算（玩家接触爆点在玩家盒边 → 头顶 1.2-3.0
  的 netherrack 架被破、br 3 obsidian 平台/顶盖恰界存活；顶盖卡壳
  爆点在盒底 → 距发射 ghast 中心 1.91 = 5 自伤【MC 免疫自身火球 =
  记档偏离】）。④ **字体一致性 pin 抓不到码点语义错**：V3-14a 加的
  「第二爆炸字」= 爽 U+723D（本意 炸 U+70B8）——charset/表/PNG/
  单测四处一致地错（430 排序连续 + FNV 全自洽），首渲染（死因屏
  crop）才显「爆?杀死」。**加 CJK 字形必目检新字 glyph + 首渲染
  E2E crop**（生成器 emit snippet 后 grep 目标字 U+XXXX 回验码点）。
  修复面 = charset 换入 + 九 glyph 重格 +1（0x70B9..0x7206）+ top-
  512 红线 131072 px 恒等 + **S1 锚字节恒等**（锚场景无消费方文本 =
  免重锚）。⑤ **canary headless 必 --frames > demo total**：--demo
  无 --frames = 播完进 finished panel 观察循环等观众 ESC/Q，
  headless 永等（10 分钟无输出假 hang）；timeout 杀进程留的 mid-run
  autosave 双跑「EXACT」= 假阳性证据陷阱（同模拟时刻 auto-save 恒
  等 ≠ 终态）——demo total = Σdts×60 帧（example 266 / dig 561 /
  build 571 / fly 541），--frames 700 通吃。
- **爆炸门场景三纪律（PITS 401，门 209 v3.19.0 V3-14a 爆炸机制）**
  ——① **森林生物群系 = 到处可燃**：blast 必点火 → real-dt 火寿命域
  （PITS 385：RAW 不适用，savenorm 权威）——**密封石室场景**（房间
  全 stone 编辑填充 blast 全域 + 玩家空气口袋 + 场景手术去火
  【PITS 398①：移除 fire 日志条目，漏移 = init 点名"fire block has
  no fires entry"】）= 无火确定性域。② **blast 破回生成态 = 日志条
  目消失**（blocks journal = 与生成世界的 DIFF，非事件日志）：断言
  破块用 `blocks.get(cell) == None` + **界外对照格**（恰好 radius 处
  local 强度 0 = 存活 pin，(3,56,0) 先例）+ 房内石墙残留对照，勿断
  "air 编辑存在"。③ **地下生成火簇回落**：场景去火后载档世界生成
  重放 3 团火（(23,35,28) 簇 + 1 蔓延）= 存档 fires[] 恒 4 条
  real-dt——断言域 = savenorm（剥 fires[*].remaining）+ 截图 RAW
  （火在 y35 地下不可见）。
- **demo 输入四坑 + X grab 屏捕 + 门机窗口口径（PITS 400，门 208
  v3.18.0 V3-11b 创造物品栏）**——① **demo 无初始指针锁定**：游戏
  启动指针未锁（首次 LMB = 捕获帧，game_interaction 的 capture 路径），
  一切 locked-pointer 门控键（G/E/C/挖掘）在 demo 里必须先有一击
  捕获 LMB 才生效——门 207 的 S2 "G 切换"因此从未在 demo 里生效
  （其"即挖"实为 f21 捕获次帧，"结果存档"实为场景副本，F5 被
  save-spec 红线禁用 = 见②，记录已修正，补证 = 门 208 S2c）。②
  **demo save-spec = 观察红线禁 F5**：replay 的 world = save slot 时
  `m_args.saveSlot.reset()`（该槽永不回写）→ F5 handler 同判空报
  "no save slot"（WARN 不显 = 默认 info 级漏看，PITS 392 族）；
  **结果捕获只存在于 fresh spec + --save**（退出写 = 回放结果的唯一
  落盘点）。③ **xdotool 数字键双投递**：`xdotool key 2` = KEYDOWN
  （热栏选择）+ TEXT_INPUT（'2' 字符）——搜索聚焦态下数字入搜索串
  （"app2" 无匹配 = 授予全灭假象）；定案 = 物品栏 tab 开时数字 =
  文本不切热栏（MC 搜索焦点语义），槽位须在开面板前选。④ **X
  指针抓握阻塞外来 X 客户端**：游戏捕获指针期间 `import -window`
  等屏捕恒挂 EAGAIN（X grab 纪律）——面板态视觉证据 = **游戏自身
  退出截图**（退出时面板仍开 = 帧定格捕获）；屏捕只许在指针自由
  窗口。⑤ **门机实际窗口 = 679×744**（config 请求 1280×720 被 WM
  约束不生效）——布局探针/点击坐标必须按**截图实测尺寸**算 UI
  空间（÷ui.scale），且臂必须 `--config` committed 口径（用户
  ui.scale 2.0 = PITS 394 族，漏 --config 坐标全错）。
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
  `asInt(), 30` 断言 / withoutKey player 链。② **demo save-spec 回放的结果捕获**：world = save slot spec 时
  `--save` 须匹配槽（rc 2 点名）且退出**永不回写槽**（观察红线）——
  门 208 实证红线同时禁用了 F5（saveSlot.reset = F5 判空 WARN，
  门 207 的"F5 结果捕获"记录无效 = PITS 400①②）；**结果捕获 =
  fresh world spec + `--save` 的退出写**（唯一落盘点）。fresh world
  spec（4 canary）无此问题（常规退出写档）。
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
