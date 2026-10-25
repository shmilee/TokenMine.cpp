# Phase CRAFT-1 — 工作台分块（v1.0 任务表 23/32，门 104）

> 每 crafting_table 块独立 3×3 容器（当前 = 全游戏共享一个 `m_tableGrid`）。
> 依赖 ENT-2（BlockState）已就绪——本任务**不消费** per-instance 状态
> （工作台无朝向），用 per-block map 范式 = 炉子 P44 / 营火 P71 同款
> （ENT-2 记档"四类 per-block map 不迁移，CRAFT-1 取 campfire 范式 + 纪律"
> 的兑现）。

## 实现内容

1. **per-block 容器**：`std::map<BlockPos, CraftingGrid> m_tables`（位置序）
   替换全局 `CraftingGrid m_tableGrid`
2. **面板绑定**：`openTablePanel(pos)`（右键分支 `openPanel(Table)` →
   绑定所点块）；`InventoryUI` 网格改可绑定（`bind(CraftingGrid*)`，
   `match()/grid()/click` 走绑定指针）——**UI 层零布局零视觉变化，仅换
   数据源**（FurnaceUI::bind 范式）
3. **E 面板终生绑定**：`Game()` 构造体 `m_inventoryUi.bind(m_inventoryGrid)`
   ——2×2 shapeless 行为零变化
4. **setBlock 收口**：格变 crafting_table → `emplace` 空格；移除/改他块 →
   `erase`（**破坏 = 容器内 9 物品丢弃、只掉工作台自身** = 炉子破坏同款
   语义，记档偏离 MC 掉落）
5. **卸载保护**：`protectStates(m_tables)`（含工作台列永不卸载 = 炉子同款）
6. **format 18 → 19 `tables` 段**：
   - 写：恒写（可空数组 = furnaces 键纪律），每块 {pos, slots[9]} 位置序；
     停写 `table_grid`
   - 读 19：`tables` 必需；`table_grid` 携带 = 拒（点名）；槽 format-3 纪律
     （未知物品 / count > max_stack / **槽带 damage 拒** = SURV-3 grid 范围
     纪律 / entry 未知字段）；双向交叉校验（entry ⇔ 块，分别点名）+ 重复
     pos 拒
   - 读 ≤18：`tables` 携带 = 拒（"format 19 or newer"）；`table_grid` 必需
     （3..18 原行为不变）
   - **迁移（parse 时）**：legacy `table_grid` 非空 ∧ 世界恰 1 工作台 →
     归属；0 / 多个 → 丢弃（P44 sole 归属先例）；解析期不变量 = 任意加载
     后每工作台块必有 entry（空 entry 自动建）→ applySave 直接填 map，
     **无 rescan**（炉子同款：parse 交叉校验 + 迁移即机制）
7. **save.cpp 重构**：furnace 的 `slotJson` lambda 提为自由函数
   `serializeSlotEntry`（tables 槽复用；行为字节同——提取原因 = 删
   table_grid 写后内联边界漂移触发 GCC16 -O3 variant maybe-uninitialized
   误报，提取后消除，`-Werror` 0 警告）

**零 config / 零内容 / 零新纹理 / 零音效**。`inv_grid` / E 面板全程不动。

## 变更文件

- `src/core/save.hpp`（kSaveFormat 19 + SavedTable + SaveGame 改 + schema 注释）
- `src/core/save.cpp`（serialize/parse + 迁移 + serializeSlotEntry 提取）
- `src/core/game.hpp`（m_tableGrid → m_tables + openTablePanel/tableAt）
- `src/core/game.cpp`（右键分支 / openTablePanel / setBlock 收口 / unload /
  collectSave / applySave）
- `src/ui/inventory_ui.{hpp,cpp}`（bind 化，零视觉变化）
- `tests/test_save.cpp`（12 新测 + 全部 ≤18 降级测试补「去 tables + 还
  table_grid」+ 既有 grid 测试重构）
- `tests/test_campfire.cpp` / `test_drown.cpp`（降级适配）/
  `test_durability.cpp`（format 断言 19）/ `test_inventory_ui.cpp`（2 新测
  + fixture bind）
- `docs/KNOWN_ISSUES.md`（工作台条目划除 + [CRAFT-1] 已修复注记）

## 测试（1535/1535，+14 新）

save 12 新：TablesRoundTrip（双表位置序逐格往返）/ EmptyTablesRoundTrip
（恒写空数组）/ Format19RequiresTablesKey / Format19RejectsTableGridKey /
RejectsTablesFieldInFormat18 / RejectsTableEntryBlockMismatch（双向两反例）/
RejectsDuplicateTableEntries / RejectsBadTableSlots（6 类坏值含槽带 damage）/
Format18To19MigrationSoleAttribution（cell0/cell8 精确归属）/
MigrationDiscardedWithSeveralTables / MigrationDiscardedWithNoTables /
MigrationEmptyLegacyGridLoadsEmpty。UI 2 新：RebindSeparatesTheGrids（rebind
后旧网格保内容、新网格空）/ CraftConsumesOnlyTheBoundGrid（绑空网格不消耗
且原网格完好；换绑回原网格 shaped 2×2 精确消费）。
`-Werror` 0 警告。headless 冒烟：fresh 19 存档（tables:[] + 无 table_grid）/
19 读档 / 18 遗留读档（退出回写 19）全过。

## 门 104

- **S1 PASS = 第 104 次结构性免疫**（10-26 00:0x，fresh 60f ×2 RAW 字节一致
  + vs base18 **字节一致** = 免重锚；论证 = crafting_table 在 assets/ 仅
  块数据/配方/atlas 3 处引用，worldgen.json 与全部 biomes 零引用 → 无输入
  fresh 零表块 → m_tables 恒空 → 收口零创建、无面板零渲染）
- **S2 双工作台互斥场景 = 补跑清单**（输入跑，xdotool 协议）：场景 =
  A=(21,46,−4) / B=(22,46,−4) 两表 + 玩家 (21.5,45,−2.5) pitch −0.30 +
  hotbar beef/coal；Run1 beef→A grid cell0（断言 B 全空）/ Run2 coal→B
  cell4（断言 A 不变）/ Run3 持久化重开 + 截图 A/B 面板互斥；**frozen
  v0.20.0 同输入 = 共享网格反例对照**（A/B 内容相同）。行为层 = 14 单测已证
  （rebind 隔离 + 只消费绑定网格 + 迁移/往返）
- **S3** = 套件 1535 PASS
- 性能 A/B v0.20.0（发布前必做）：fall 900f 三窗 **fluidCells
  123879/167781/236573 双侧逐位一致（同世界）+ upd/render/swap/ents/stream
  全同档** = 成本可忽略（m_tables 恒空 fast-exit = 结构惰性实证）；table
  场景对（2 表 + 玩家邻站 900f）= 补跑清单（黑相 stall 4+4 轮未过——两侧
  同款同世界空容器，证据同 fall 档）

## 偏离 / 记档

- 破坏工作台 = 容器内物品丢弃（炉子破坏先例；MC 掉物品 = 记档偏离）
- 工作台无朝向（不消费 ENT-2 per-instance 状态 = per-block map 定案）
- 面板零视觉变化（bind 化仅换数据源）
