# Phase v3.32.0 — V3-16a 扩展指导 demo 族：数据层 + scene + panel_click + pilot ④（门 222）

> 3.x「世界扩展」V3-16 相位 A/7。V3-16 = 10 主题 3-10 分钟级教学 demo
> 族（11-07 用户实机诉求）；本相位落**内核 + 工具链 + pilot ④ 挖矿打造
> 工具**，后续相位 16b~16g 按章节扩（BACKLOG 定案）。

## 定案与实现

**阶段拆分（编排者定案，11-08 入 BACKLOG）**：v3.32.0 = 16a（本相位）
→ 16b ⑤体力⑥回血 → 16c ③房子+床 → 16d ⑦皇宫⑧材料 → 16e ⑨河流 →
16f ①下界门②末影龙 → 16g ⑩完整黑夜历险 + coop 占位。

### 1. demo 世界 spec 第三形：scene（committed 确定性存档资产）

`{type:"scene", name}` → `assets/demos/scenes/<name>.json`（committed
gzip 存档文档，只读——资产永不是退出写路径 = 观察红线结构根）。
`--save` 可与 scene demo 组合（退出写回放结果）。装载走
`tryLoadSave` 的 scene 分支（`m_demoSceneName` 非空即载 scene，优先于
slot）；replay setup 对 scene 名做存在性点名拒收（save spec 孪生）。

### 2. panel_click 事件（几何无关格地址——guide demo 的 uiScale/窗口鲁棒面）

- `InputEvent::Kind::PanelClick`（词表尾，**record 的 tap 永不产生**——
  author 面独占）；`u32 = panel id`（0 inventory / 1 table / 2 furnace /
  3 creative），`code = slot id`（0..35 玩家背包三面板共享；100..103 =
  E 面板 2×2；110 = out；120/121/122 = furnace in/fuel/out；200..208 =
  table 3×3；300..323 = creative 6×4），`value = button`（0 left / 1
  right / 2 shift）。
- **resolver**（`Game::demoPanelClick`）= 开面板检查（panel id 0 接受
  Inventory|Table|**Furnace** 三开态——门内捕获②③：共享 36 格在
  furnace 打开时须可用 + 其几何是 **FurnaceLayout** 的 slots，与
  InventoryLayout 不同形）+ layout 纯函数 rect 中心（render 孪生，
  当前窗口几何含 uiScale/fit factor）+ 真实 Input closure 合成点击
  （面板逻辑零新路径：位置 + shift + 同帧 press 边沿）。
- author 词表面（`authorPanel/authorPanelSlot/authorPanelButton` +
  `demoPanelName/demoPanelSlotName` 显示逆）严格拒收点名；net
  `--script` 拒收 panel_click（40B FrameState 无 panel 字段的文档边界，
  coop 后移）。
- **sim 版本保持 2**：panel_click = 增量事件类型（既有 8 型的字节位级
  不变，canary 4 EXACT = 裁决证据，门内执法）。

### 3. 索引数据层（11-07 定案四轴 schema）

`assets/demos/index.json` = `{demos:[{category, seq, mode, tags,
file, length_frames}]}` 封闭键集。category 封闭词表 = `kDemoCategories`
5 章【day1 第一天基础 / build 建造与大地 / dimension 维度远征 /
survival 生存历险 / coop 多人协作——4+1 章节定案】；seq 类内学习序
1..n 连续；mode solo|coop 硬约束；tags [a-z0-9_]+ 软标签（v1 只存不
渲）。**init 每次 boot 全量交叉检查**（`loadDemoIndexChecked`，nether
池 fail-fast 传统）：文件存在 + 完整 strict load（容器 + payload + SIM
门——stale sim 的 guide init 点名，canary 孪生）+ length 匹配 + scene
存在。**`--list-demos`** 独立无头旗（save management 孪生）= 索引表 +
交叉检查 + 时长（mm:ss @1/60）。

### 4. pilot demo ④ 挖矿打造工具（day1 #1，11387f = 03:09）

scene `pilot_mine`（`tools/mk_scene.py` 手术，幂等验证）：12×12 平台
（顶 y54，**y50-53 实心填充**——续挖穿洞 + 下探虚空复合教训）+ 树
（干 6 格 + 叶冠）+ 矿排（z=-9：煤×2/铁×1/石×8，**x0 空 = 路线穿越
口**）+ 砂柱×2（熔炉的 smeltable——砂→玻璃 10s×2）。时间线
（`tools/demo_author.py` Guide DSL + `tools/scenes/build_guide_mining.py`
beat 脚本）：开场巡视 → 砍树 5（留顶格视觉）→ 挖砂 2 → E 面板木板
×20 → 工作台 → **相对放置**（面 -Z 1m 前）→ 木棍×8 + 木镐 → 逐格
贴线挖矿（11 格）→ 熔炉环合成 → 相对放置 + 熔砂×2 → 第三人称收尾。
**终态（门内钉）**：h0 煤 2 / h2 铁 1 / h4 木镐 dmg 11 / h5 木板 9 /
h8 玻璃 2；表 (5,54,-7) + 熔炉 (4,54,-7)（终态空——燃料 6 木棍 30s
> 熔 20s，火余燃至 f10846 = 确定性）；行全空；零实体；hunger 17；
time 0.4163。

**鲁棒性定案（r1-r29 门内捕获的教训，全 PITS 413）**：
- 全部行走**轴对齐**（斜行子轴分量被物理丢弃 = 4× 损耗）+ track 模型
  = 精确角点到达（stop margin +2f → 实际短 0.03-0.11m，格内）；
- 放置/开容器 = **相对瞄准**（`place_ahead/open_ahead`：面朝方向 1m
  前——目标随玩家移动，±0.3m 漂移结构容忍；绝对格瞄准的 1 格漂移
  死根）；
- 挖矿 = 逐格 1m 正北贴线（**行墙挡横向视线**：从 x7.5 瞄 x4 的射线
  穿 x5 格 y54.7 = 打错块）+ 磁吸 1.0-1.5m 收集带（2.2m 的拉力
  0.15 m/s² 不够）；
- hold 时长 = 破碎时间 + margin < 下方平台石破碎时间（续挖红线 PITS 7
  延伸）；**工具态决定破碎时间**（砂裸手 0.75s ≠ 镐 0.375s——P2.5 时
  手持是木头不是镐）。

## 变更文件

`src/core/demo.{hpp,cpp}`（Scene 型 + 索引 schema/交叉检查 +
panel_click 词表/编解码/author 展开/script 拒收 + injectForFrame 带
resolver 返回 string）/ `src/core/game_ui.cpp`（`demoPanelClick`
resolver，含三开态 + FurnaceLayout 共享格）/ `src/core/game_init.cpp`
（replay scene 分支 + init `loadDemoIndexChecked` + 回放循环 resolver
接线 + ERROR 日志）/ `src/core/game_save.cpp`（tryLoadSave scene
分支）/ `src/core/main.cpp`（--list-demos + Scene 桥接不碰 saveSlot）/
`src/core/game.hpp`（声明 + m_demoSceneName）/ `src/core/input.hpp`
（Kind::PanelClick 尾）/ `tests/test_v3110_demo.cpp`（V316aDemo ×11）/
`assets/demos/{index.json, guide_mining.dmo, guide_mining_timeline.json,
scenes/pilot_mine.json}` / `tools/{mk_scene.py, demo_author.py,
scenes/build_guide_mining.py, scenes/pilot_mine.json}` / CMakeLists
3.32.0 + `tests/test_debug_overlay.cpp` banner re-pin。

## 测试 / 门结论

- **2998/2998**（+11 V316aDemo：词表/展开/编解码/script 拒收/
  resolver 面/scene spec/索引 parse 拒收族/交叉检查五面）；
  `-Wall -Wextra -Wpedantic -Werror` 0 警告。
- **S1 结构免疫第 103 次**：f60 无输入 ×2 ≡ base19 逐字节 + savenorm
  EXACT + **旧树 v3.31.0 f60 亦 ≡ base19**（三方闭环；banner 零像素）。
  门内捕获①：**base19 锚 = f60 非 f900**（时钟 10:50 反解 t=0.2517；
  900f 跑 = 15937px 假 DIFF【太阳/树冠/时钟】，旧树同形 = 环境面
  排除）。
- **canary 4 EXACT**（bump 后复验 ×2 RAW + savenorm——sim 2 不 bump
  执法）。
- **pilot ×2 RAW + savenorm EXACT** + 终态内容断言（上节钉）。
- **S6 五面 rc 1 点名**（缺 dmo / length 错 / 未知 category / scene
  缺 / sim 错——sim 面单测执法）。
- **性能 A/B 同档**（旧 = 1f36bf2 v3.31.0 worktree + g139 双树 27
  markers：banner 实证 v3.31.0/v3.32.0；OW 900f 无输入 PERF-WIN 三窗
  upd med 0.564/0.576/0.599 vs 0.576/0.581/0.590 同档 + render
  0.579/0.628/0.658 vs 0.628/0.693/0.739 在旧树 2× spread 带内
  【0.698/0.721/0.736——环境漂移实证】+ swap 15.3/15.2/15.2 vs
  15.2/15.2/15.1 同档 + changesApplied 9/3/1 vs 9/2/1【win2 =
  real-dt 抖动带，门 207 先例 9/3-2/1】+ **终档 savenorm EXACT**
  = demo 内核 solo 无输入路径结构性零成本）。
- 门内捕获②③（resolver 三开态 + FurnaceLayout 共享格）+ ④（mobs-on
  config 下镐误击生成动物 = 耐久 15 vs 11——demo E2E 用 mobs-off
  config，与 scene 基线一致）= 全 PITS 413。

## 已知 / 后移

- tags 只存不渲（教学页 V3-17 消费）；coop 章节占位（空章 =
  "(empty — the later face)"）。
- 熔炉燃料余燃（关炉后火继续烧空）= MC 同款语义，终态确定性内。
- 16b~16g 各主题 = 本相位工具链（mk_scene/Guide DSL/相对放置/逐格贴
  线）的直接复用。
