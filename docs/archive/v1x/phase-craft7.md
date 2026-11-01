# CRAFT-7 背包交互（v1.x 矩阵 25/33，v1.25.0，门 140，CRAFT 行 4/4 = CRAFT 系统收官）

## 定案

原始定义已佚 = 定案 **背包面板交互的 MC 操作面（3/4 项 + 数字键）**（消费
KNOWN_ISSUES「背包 UI 右键语义极简」条）：

- **右键取半堆**：空手右键占位格取 `floor(N/2)` 最小 1（N=1 取自身、N=2/3
  取 1 = v1 行为边界、N=64 取 32；耐久随堆 = SURV-3 纪律）。共享
  `cellRightClick` 原语（E 面板/工作台/炉子面板同语义）。
- **shift 快捷移动**：E 屏语义 = hotbar(0..8)⇄storage(9..26) 互移（索引
  序、同物同耐久合并【SURV-3 门】、堆可跨格分裂）+ 容器格 → storage 优先
  （MC 主背包优先）+ **手持堆 no-op**（MC 保留 carry）+ 炉子面板 = 容器
  GUI 语义（炉槽 → 主背包；玩家背包侧 no-op；移 input 按 furnace.hpp 不变
  量重置进度）。左/右键 + shift 皆可（MC 同款；右 Shift 并入 LeftShift
  映射）。
- **双击收集分拣**：MC 分区语义——hotbar 0..8 与主背包 9..26 为**两个
  独立分区**（首轮单分区实现被测试揪出跨区 = 修正）；第一击拾堆（既有
  leftClick）+ 0.3 s 真实 dt 窗内第二击同格 = 收集该分区全部同物同耐久堆
  入点击格（满堆上限、溢出按索引序回填）；游戏层检测（`m_lastLeftSlot` +
  时间戳成员，kDoubleClickSeconds 0.3 命名常量——生存计时器类 = 输入态不
  入帧状态，无输入跑永不触碰 = 结构免疫根）；配方行/输出格/右键/shift
  均重置配对。
- **顶排数字键 1-9**：直接选热栏槽 1..9（MC；Tab 循环保留；面板开/关
  皆可用；numpad 不映射 = 记档）——消费 SURV-1 记档"热栏只有 Tab 循环无
  数字键"。
- **无拖拽**（记录偏离：三点击操作 + 数字键覆盖核心操作面；拖拽 = 指针
  捕获下鼠标轨迹跟踪 = 最高风险 E2E 类，矩阵外）。
- **关闭面板保留 carry 在 UI 实例**（v1 既有语义不变，MC 会退回背包 =
  记档偏离；E2E 判别设计消费此语义：OLD 侧 shift+点击 = 普通拾堆，关面板
  丢弃 = 与 NEW 侧终态互斥）。
- **零 format（21）/零 config/零新数据字段**（纯输入映射 + UI 状态机）。

## 实现

- `src/core/input.{hpp,cpp}`：GameKey +Digit1..Digit9（kKeyCount 同步——
  首轮忘更 = array-bounds 编译错实证）、mapKey +9 数字 + RSHIFT→
  LeftShift。
- `src/ui/inventory_ui.{hpp,cpp}`：cellRightClick 取半堆（共享原语）+
  匿名 namespace helper（moveStackIntoOrder / inventorySlotOrder /
  collectInventoryInto【分区】）+ InventoryUI::quickMove/doubleCollect +
  FurnaceUI::quickMove/doubleCollect。
- `src/core/game.{hpp,cpp}`：handlePanelClicks 三态派发（shift →
  quickMove / 双击窗 → doubleCollect / 普通）+ 配对状态 2 成员 +
  kDoubleClickSeconds + 数字键选择循环（Tab 旁）。
- 零渲染/零世界状态/零存档变更（carry 从不入档 = 既有）。

## 测试

1866/1866（+14：input 映射 10 例演进【9 数字 + RSHIFT】/ inventory_ui
9【取半堆演进 + shift-move 5（合并/反向/容器格/手持 no-op/耐久分离）+
双击 4（收集/溢出索引序/分区/需 carry/耐久分离——含 2 契约演进】/
furnace 4【output→storage + input 移走进度重置 + 玩家槽 no-op + 双击】）、
`-Werror` 0 警告。

## 门 140

- **S1 = 第 140 次结构性免疫免重锚**（fresh 60f 四方 ×2 **168140 B**
  全 EXACT + **base18 锚字节一致**（679×744 平铺态连续 = 门 139 刷新锚再
  验）+ savenorm 同侧/跨侧；无输入 = 面板永不打开 = handlePanelClicks
  的 panelOpen 门控惰性 + 数字键永不按下 + carry 成员恒空 = 双结构
  惰性）。
- **S2 = 输入跑 4 操作 × 2 侧 × 2 跑（判别 = 存档终态互斥）**（firefox
  显隐 PITS 314 + 捕获轮询 "Pointer captured" DEBUG 协议 + 绝对坐标探针
  probe140【纯 InventoryLayout 函数算槽位中心】+ 720f 无输入尾段）：
  - r1 shift-move：NEW slot0 stone64 → storage（9=64+30 分裂 10=30）vs
    OLD = 普通拾堆 + 关面板丢弃（9=30 原样，64 消失）
  - r2 右键取半：NEW slot5 dirt10 → 5 vs OLD → 9（取 1）
  - r3 双击分拣：NEW slot13 dirt3+17 dirt5 → 13=8/17 空 vs OLD 双击 =
    拾回（13=3/17=5 原样）
  - r4 数字键：NEW selected=4 vs OLD selected=0
  - 16/16 跑全过（r1-r3 主批 i=1 首试 ×12 + r4 补跑 i=1 首试 ×4）+ 同侧
    ×2 savenorm EXACT ×8 对 + 跨侧互斥断言全过 + **PITS 319 相机漂移只记
    录不门控**（首轮 r4 8/8 败 = 数字键误置 locked-world 段【面板开时
    !locked 早退】，移入 handleUiKeys 全局键段后 4/4 i=1 首试 = 根因实
    证）。
- **S3 = 套件 1866**。
- **无 S6**：零 parse 面（输入映射 + UI 状态机，无新数据字段/config）。
- **性能 A/B 必做**（真 v1.24 tm_v124_perf vs v1.25-dev tm_v125_perf，
  同款 g139 补丁器两侧同打，fresh --new --seed 123456789 900f 三窗 + 终
  档集合）：banner 实证 v1.24.0/v1.25.0 + PERF-WIN 全指标同档（upd med
  0.379/0.394/0.394 vs 0.366/0.365/0.439 win3 +11% 窗噪类 / render 0.40-
  0.48 / swap 15.6-15.8 vsync 锁定 / fluid 0.040-0.052 / fires 0.007-
  0.011 / spread 0.0004-0.0006 / ents 0.003 / stream 0.011-0.015）+
  PIPE3-S0 tickMs 同档 + **changesApplied max 9/3/1 三窗逐位一致** +
  **终档 fluids 18/18 + blocks 55/55 集合恒等（diff 0）= 同世界同成
  本**【无面板 = 全新代码惰性，结构惰性最强形】= 零回归。

## PITS

- **PITS 319**（捕获点击 grab delta 相机漂移在本 WM 平铺态系统性偏大：
  冒烟 8/8 次 yaw −0.255/pitch −1.09 但操作终态 8/8 正确 → 存档终态是
  唯一权威、相机只记录不门控；漂移时大时小 = 捕获瞬间 X 异步竞态）。
- kKeyCount 忘随枚举扩展 = -Werror array-bounds 编译错（首建实证，即时
  修复）。
