# Phase UX-7 — 新手指导（v1.x 任务表 40/40，门 161，UX 行 7/7 收官）

> 消费「信息通道缺失」可玩性瓶颈（玩家没有任何途径知道配方存在 /
> 下一步干什么）= **MC objective 式数据驱动逐步教程**（10-30 kickoff
> 五部件定案）。**tag v1.41.0 = UX-7 收官 + UX 系统 GitHub release
> （notes 覆盖 v1.35-1.41）= UX 行 7/7 全清，v1.x 任务矩阵 40/40 全完成。**

## 定案（五部件）

1. **内容 = JSON 数据文件** `assets/data/tutorials/beginner.json`
   （注册表范式严格解析点名）：步骤列表，每步 {id, text（中文）,
   goal（事件）, count}。shipped 7 步链条：
   `walk → break.wood×3 → craft.crafting_table → craft.wooden_pickaxe
   → break.stone×5 → craft.stone_pickaxe → kill.chicken`。
2. **事件词表封闭**（SURV-7 stats 先例）：`craft.<物品> / break.<方块>
   / place.<方块> / kill.<mob> / interact.<方块>` + 裸词 `walk / jump
   / eat`——每类 = Game **一个收口点**调 `emitTutorialEvent(name, n)`
   （单点，done/disabled 零执行 = UX-2 痛点不重蹈）。
3. **步骤状态机纯函数** `core/tutorial.{hpp,cpp}`（campfire 范式）：
   `tutorialApply(state, name, n, steps)`——done → 原样；goal 匹配 →
   progress += n；≥ count → 步进（越尾 done）。**帧锁定**（事件在帧
   边界，零 real-dt）= E2E 字节可复现根。
4. **存档 format 22→23 `player.tutorial`**（tables 键纪律先例）：
   恒写（进行中 {id, count} / 完成 {}）；23 必键 / ≤23 携键版本谎报
   点名 / ≤22 免迁 / **id 不在当前数据 = 重置首步**（info 日志，非
   init 失败 = kickoff 定案）/ count ≥ 步 count = 损坏点名 /
   交叉校验在 game 加载点（validateStats 范式，parseSave 不加参）。
5. **UI**：下中部目标行（热栏上方一行，PRE-10 堆叠纪律：效果行/护甲
   行激活时上移一档；locked ∧ !panelOpen ∧ !dead ∧ enabled ∧
   (进行中 ∨ toast 激活)）+ **120 帧完成 toast**（kTutorialToastFrames
   帧锁定计数）+ **T 跳过键**（GameKey 尾追加，C 键先例；locked ∧
   enabled ∧ !done）+ config `tutorial.enabled` **严格读**代码默认
   true（SAVE-1 范式——getBool 类型失配静默回落是项目已知坑；
   config.json 零改动）+ F3 `tutorial` 行（英文纪律）。

**字集衔接（UX-6 兑现）**：brief 原措辞 8 字缺（左/住/锻/鸡/拳/×/教/
程 ∉ 392 集）→ 措辞微调（"打木头（持续击打）"/"（掉出石料）"/"打 1
只白色小动物（打 4 下）"/toast "全部完成！"）+ 单测
`ShippedTextsStayInsideTheFontSet` 全字逐字钉死在字集内（缺字 = 生成
器一条命令，无需再定案）。

## 实现要点

- 收口点（每类事件一个）：walk/jump = game_frame player update 前后
  纯比较（ENT-1 帧边界位移判移动同款）；break/place/eat =
  game_interaction 完成点（drink 不发 = 记档）；craft = game_ui
  clickOutput **配方分支**（点击前 GridMatch 判别——UX-5 repair 完成
  同样播 "craft" 音但**不发事件** = 记档）；kill = game_entities 死亡
  移除点（任意死因，与 SURV-7 计数同位）；interact = 开炉/开台/营火
  三站点。
- `tutorialRestore`（game 加载点）：id 失效重置 + info 日志；toast
  不跨读档（只置 0）。
- **零新资产文件 / 零新音效 / 零帧状态新增**（toast 计数器 = 帧锁定，
  无输入恒 0）。

## 测试

2018/2018（+40）、`-Werror` 0 警告、banner v1.41.0：

- **test_tutorial.cpp 34**：解析（合法 7 步 / 未知键 / 空 steps / id
  重复 / count 0 / goal 词表外 "craftz.x" / 交叉校验 break.stonex /
  kill.dragon 点名）+ 纯函数（首事件推进 / 不匹配忽略 / count 恰好
  步进 / n>1 / 尾步 done / done 后免疫 / 连续多事件）+ UI 纯函数
  （目标行 count 1 无进度 / count 3 带 " 1/3" / done 空串）+ toast
  帧边界 120/119/0 + config 严格读（缺省 true / false / 非 bool 点名）
  + **内容钉死**（shipped 7 步 id/goal/count 精确 + text 全字 ⊆ 392
  字集 + 真实注册表交叉校验存活）
- **SaveFormat23Test 6**：写读往返（进行中 / done 空对象）/ 23 缺键
  拒 / ≤22 携键版本谎拒 / count ≥ 步 count 拒 / id 失效重置 / ≤22
  免迁
- test_debug_overlay（F3 钉 v1.41.0 + tutorial 基线行）/ test_input
  （+T）/ test_save + 3 处旧 format 降级测试剥键演进

## 门 161（编排者执行，old 侧 = v1.40.0 = e20885f = tm_v141_base）

- **S1 = 第 161 次结构性免疫 + base18 锚连续 168140 B 免重锚**
  （fresh 60f 六跑：on×2 + off×2 + old×2 全字节一致 + 跨版 savenorm
  恰 2 项 {format 22→23, +tutorial {id:walk,count:0}}；论证 = 无输入
  未锁定 = 零教程 UI（目标行 locked 门控）+ 零事件（首步 walk 不
  推进）= 状态字节稳定。**kickoff 预期的"一次重锚"未发生 = 更强
  结果**（目标行只在锁定态绘制，无输入基线零触碰））。
- **S2 = 全链输入跑 PASS**（独占 1362×744 态 = covers unmap 协议
  PITS 314；鸡脚手架 chicken-only max_mobs 1 speed 0.01；四段
  A→B1→B2a→B2b **F5 存读续接** = 教程跨读档实证）：
  - **A 段**（walk→砍木×3→合台）：new×2 + old×2 全 PASS——×2 RAW
    双侧字节一致（184199 B）+ **跨侧 RAW 字节一致**（代码级免疫：
    终帧面板态无教程 UI）+ 跨侧 savenorm 恰 2 项（世界零 diff）；
    终态 tutorial {id:wpick,0} / slot5 台 / 木 0（3 破）。
  - **B1 段**（选镐→挖石×5→捡圆石）：4 跑全 ckpt——终态 tutorial
    {id:spick,0} + 圆石 5 + 镐 damage 5（SURV-3 磨损随跑）；同侧/
    跨侧 savenorm = **仅 pos 走位 real-dt 抖动**（±7mm，PITS 244/320
    类）+ 新侧 tutorial 项 = 全字段跨侧恒等（最强形）。
  - **B2a 段**（存档 pose 转正对桌→石镐 12 点击）：同侧 savenorm
    **双侧 EXACT**（无走位 = 全确定）+ 跨侧恰 2 项；终态 {id:kill,0}
    + slot14 石镐。
  - **B2b 段**（存档 pose 预瞄 + W 0.45s 逼近 + 按住挥击×3→杀鸡）：
    NEW×2 **tutorial {} + stats kill.chicken 1 + 日志
    `Tutorial: completed (step 7/7)`** + 掉落 feather/生鸡 ×2 RAW；
    OLD×2 同行为无 tutorial 键（世界零 diff）；跨侧 savenorm =
    format + tutorial + real-dt 抖动（pos 0.0015 / 掉落 0.0001-0.012）。
  - **跳过 T**：NEW tutorial {} + 日志 `Tutorial: skipped` / OLD
    T 未映射 no-op 对照（无 tutorial 键）。
- **S3** = 2018/2018。**S6 = ×2 精确点名 PASS**（"tutorial step
  'chop' goal 'break.stonex' names unknown block 'stonex'" / "…
  'kill.dragon' names unknown mob 'dragon'"）。
- **性能 A/B v1.40.0 vs v1.41.0（发布前硬门槛）PASS**：fall 900f
  串行零并行——PERF-WIN 三窗 upd med 0.391/0.392/0.406 vs
  0.392/0.407/0.477、render med 0.426/0.451/0.423 vs
  0.441/0.445/0.466、swap med 15.61/15.70/15.68 vs
  15.68/15.70/15.56 = 全指标同档（win 内 ±~15% 摆动 = 既有门重跑
  噪声量级）；PIPE3-S0 changesApplied **9/3/1 三窗逐位**；**终档
  fluids 18/18 + blocks 55/55 集合恒等 diff 0 = 同世界同成本【结构
  惰性最强形：无输入零事件零 UI，教程段零执行】**（终档唯一差异 =
  v140 无 tutorial 键 vs v141 {id:walk,count:0} = format 23 恒写初态，
  非运行期状态）。

## 门内发现（PITS 347-350）

- **PITS 347（SDL3/dppx1.5 指针 warp 相机模型）**：捕获态
  `xdotool mousemove --sync (Xa,Ya)` 产生的 grab delta = **−(Xa,Ya)
  （绝对屏幕坐标取反，×k 0.0025）**，非 −(目标−中心)：warp (1311,
  394) → yaw −3.28 / pitch −1.10 双点实证 + pitch 钳 ±1.55 复现
  吻合。推论：**绝对 warp 只能左转（Δyaw≤0）+ 俯（Δpitch≤0）**，
  180°/右转/仰视不可达 = 输入跑相机控制弃 warp，改**存档 pose
  预置**（save 写 yaw/pitch/pos，B2a/B2b 段全 pose 化）+ pitch 下压
  用 (0, Y) 目标（Δpitch = −Y·k 精确，二次数据点验证）。
- **PITS 348（攻击 = isDown 按住，1ms tap 不可见）**：近战攻击走
  `isDown(Left)` + kSwingInterval 0.25 摆动节奏；XTEST `click 1`
  （press+release <1 帧）在 wasPressed 边缘类（面板点击）有效，在
  isDown 轮询类**恒不可见**（6 tap 0 伤害实证，pose/鸡位/射线全对）
  → 改 `mousedown; sleep 0.35; mouseup`（一次按住 = 2 摆 = 4 伤 =
  鸡全血）。挖掘（3.0s 按住）天然免疫此坑。
- **PITS 349（空槽整堆放置语义）**：携带满堆左点**空**格 = **整堆
  放入**（非 1）；配方 shaped 匹配 = 每 pattern 格 ≥1（多余 count
  合法，CPU harness 实证 4/2/2+4/4 命中）→ 拆分协议 = 整堆放 + CRAFT-7
  右键取半（floor(N/2)）循环到 pattern 格 + 清理回合归位（8 木板 →
  4/2/2 三格 + 5 回 slot 全确定）。
- **PITS 350（spawner 环锚玩家，非世界出生点）**：鸡生成环 4-6 以
  **玩家所在列**为锚（三组 pose 实证：玩家 z −4.5 → 鸡 z −10.5；z
  −8 → z −13.5；环距恒 5.5-6.0）——AGENTS 旧记"锚世界出生点"在
  玩家远离出生点后失效；鸡恒在玩家前方 ~5.5 + 下坡 1-2 = **kReach 5
  之外**，杀鸡须先走近（B2b 的 W 0.45s 逼近步）；且鸡生成只在
  frame 60（first_spawn）按**当时 pose** 定，之后 wandering 0.01
  近静止 = 存档 pose 可完全预言落点（无亚格 jitter，代码实证）。

## 偏离/记档

- 目标行 = 纯文本无箭头/热点高亮（kickoff v1 范围外条款）。
- craft 事件 = 合成网格配方分支（E 面板 2×2 + 工作台 3×3 同款）；
  炉子熔炼/UX-5 修复**不发** craft（数据驱动无核心 switch，扩展点
  记档在代码注释）。
- eat = 食物通道完成（drink 不发 = 记档）；interact = 开炉/开台/营火
  （shipped 链条未消费，词表完整性）。
- 跳过键 T 完成后不可恢复（done 恒存；重玩 = 改教程数据 = 重置语义
  覆盖，矩阵外）。
- 完成 toast 120 帧固定窗（无 MC 式可配置）；F3 行英文（开发者工具
  纪律）。
- 教程进度 = 行为计数（事件），非状态查询（如"背包里有镐"）= MC
  objective 同款（记档）。
