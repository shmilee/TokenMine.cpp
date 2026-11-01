# Phase 44 细节存档

> 用户确认方向（"先完成 2和 3 即两个遗留内容"之 1 = P40 遗留「单个全局
> FurnaceState」）。
> 范围 = per-block 炉子状态：`Game` 每炉子方块独立 `FurnaceState`
> （`std::map<BlockPos, FurnaceState>`，位置序）+ `Game::setBlock` 收口
> 维护「每炉子块必有状态」不变量 + 右键面板绑定所点方块（`FurnaceUI::bind`）
> + 存档 format 7（`furnaces` 数组，与方块差集双向交叉校验）+ format 6
> 迁移规则。
> 纯代码变更——无新数据文件（跨构建参考构建直接同资产，无 own-cwd 问题）。

## 阶段报告

- **`[2026-10-12, Phase 44]` per-block 炉子状态**
  — 788/788（+13−6，save 测试节重写），`-Werror` 恒 0 警告；E2E 批跑全绿
  （S1 第 18 次结构性免疫免重锚 + S2 截图 vs P43 构建字节一致 + S3 双炉
  独立精确 + S4 面板绑定精确 + S5 format 6 迁移精确）。

### 实现内容

**Game 状态（`core/game.{hpp,cpp}`）**
- `FurnaceState m_furnace` → `std::map<BlockPos, FurnaceState> m_furnaces`
  + `BlockPos m_openFurnacePos`（面板打开时有效）。`std::map` 位置序 →
  逐帧 tick 循环（每态每帧 1/60 子步，规则不变）迭代顺序确定。
- `Game::setBlock` 收口维护不变量：新块 `properties.interact ==
  kInteractFurnace`（数据驱动判定"是炉子"，无块名硬编码）→ emplace 空态；
  否则 erase。放置/破坏/染色换块全走 setBlock（P42 既有收口）。
  worldgen 不产炉子，流式 chunk 不走 Game::setBlock → 新世界 map 恒空
  （结构性免疫）。
- 右键炉子 → `openFurnacePanel(pos)`：记 `m_openFurnacePos` +
  `m_furnaceUi.bind(furnaceAt(pos))` + `openPanel(Furnace)`。面板打开期间
  块交互暂停、无其他路径变更该块 → 绑定态稳定（无需重绑）。
- `collectSave`：map 序（位置排序）写 `save.furnaces`。`applySave`：
  `m_furnaces` 重建自 `save.furnaces`（解析器已 deriveProgressFor，拷贝保真）。

**FurnaceUI（`ui/inventory_ui.{hpp,cpp}`）**
- `FurnaceState& m_furnace` 成员引用 → `FurnaceState* m_furnace = nullptr`
  + `bind(FurnaceState&)`。构造签名 `(Inventory&, const ItemRegistry&)`
  （Game 构造初始化列表同步；test_furnace 8 处 UI 测试改 bind）。

**存档 format 7（`core/save.{hpp,cpp}`）**
- `SaveGame::furnace`（FurnaceState）→ `std::vector<SavedFurnace> furnaces`
  （`SavedFurnace {BlockPos pos; FurnaceState state;}`，位置排序）。
- 写：`"furnaces"` 数组恒写（可空），每条目 = `pos` [x,y,z] + 三槽 +
  `progress_substeps`/`fuel_substeps`。
- 读（严格）：顶层 `furnace`（fmt 6）与 `furnaces`（fmt 7）双键皆已知、
  按格式互斥——fmt 7 带 `furnace` 拒、fmt 6 带 `furnaces` 拒、fmt 1-5 带
  任一拒。条目校验：对象、未知字段拒、`pos` 3 整数 + y 界、重复 pos 拒。
  状态字段校验提取为共享 helper `parseFurnaceState`（fmt 6 全局对象与
  fmt 7 条目共用：三槽 + 计数器界 + **progress 不变量** + deriveProgressFor）。
- **双向交叉校验**（fmt 7）：条目位置集 == 世界炉子块集（炉子块 = 方块
  差集中 `interact == kInteractFurnace` 的块，数据驱动；worldgen 不产炉子
  → 差集即全集）。entry 无块 / 块无 entry 各出指名错误。
- **format 6 迁移**（纯解析器内）：读全局 `furnace` 对象；世界**恰 1 个**
  炉子块 → 状态归属它；0 个或多个 → 丢弃（全部空载）。
- **加载后不变量补全**：任何格式加载后，缺 entry 的炉子块补空 entry
  （fmt 1-5 场景；fmt 6/7 已由各自分支保证）。

**测试（`tests/test_save.cpp` +13−6，`tests/test_furnace.cpp` 8 处 bind）**
- fixture 加 `furnace` 块（`interact: "furnace"`）。
- 新：FurnacesRoundTrip（双块、一态满一空，format 7 断言）/
  EmptyFurnacesRoundTrip（空数组恒写）/ FormatFiveLoadsWithEmptyFurnaces
  （炉子块空载）/ FormatSixMigratesStateToSoleFurnace /
  FormatSixMigrationDiscardsWithoutSoleFurnace（multi + zero 双子例）/
  RejectsFurnaceFieldInFormatSeven / RejectsFurnacesFieldInFormatSix /
  RejectsFurnaceFieldInOldFormat（改）/ RejectsFurnaceEntryBlockMismatch
  （双向×2）/ RejectsDuplicateFurnaceEntries / RejectsBadFurnacePos（y 越界/
  非整数/arity 3 种）/ RejectsProgressWithoutSmeltableInput（改）/
  RejectsBadFurnaceFields（改，坏字段循环化）。

### E2E（批跑，全绿；`/tmp/opencode/p44_e2e.zsh` 一次性）

- **S1 base18 门**：P44 ×2 + **P43 代码参考构建（worktree @6da7e39）**
  ×1 新世界 60f **四方字节一致 = 第 18 次结构性免疫（免重锚）**——新世界
  无炉子块（map 空 → tick no-op）、setBlock 维护对 worldgen 不触发、
  无输入无面板、P44 无新数据文件（ref 同资产）。skygate (120,181,242) ×168830。
- **S2 回归 A/B**（p27far 夜 360f seed 9）：P44 ×2 存档（format 7）字节一致
  （确定性权威）+ **截图 vs P43 构建同场景截图字节一致**（format 升格 →
  跨构建存档 A/B 不可能——P40 先例；场景无炉子 = map 空，行为 == P43 空
  全局态；本次强于纱容差）+ health 8 + 出生序列（f60 cow/f90 sk/f120 cow/
  f150 sk 后 cap 4）。
- **S3 双炉独立性**（正午 still、mobs off、300f 无输入；fmt7 源存档：
  A (22,45,−2) beef×2+coal×1、B (23,45,−2) output cooked_beef×3）：存档
  **精确**——A 点火 tick 1 后 progress 300 / fuelSubsteps 4501（=4800−299）/
  beef×2 未熔完（600>300）/ fuel 已耗；**B 恒静**（无燃料不点火；满 output
  若为全局态会暂停 A——per-block 核心断言）+ 存档 ×2 字节一致 + 20/20。
- **S4 面板绑定输入运行**（双块空态 + 瞄准 A 面中心 (22.5,45.5,−1)
  pitch atan(−1.12/1.5) = −0.64136【P40 probe 路径复用】+ slot0 coal【非食物】
  / slot1 beef×3，300f）：锁 → 右键开 A 面板 → 左键取 slot1 → 左键放
  input 槽：存档**精确**——**A.input beef×3、B 全空**（点击落 A 态非 B 态）、
  slot1 空、coal 未动、计数 0/0、yaw/pitch 零漂移、20/20 无进食；
  截图 vs 无输入对照：**面板区内 100.0% 像素差、区外 0.00%**。
- **S5 format 6 迁移**（fmt6 源存档：全局态 beef×3+coal×1+progress 300 +
  恰 1 炉子块 (22,45,−2)，60f 无输入）：输出 **format 7 精确**——唯一 entry
  在 (22,45,−2)，progress 360（=300+60）、fuel 已耗、fuelSubsteps 4741
  （=4800−59）、20/20。迁移 multi/zero 分支由单测承担。

### 已知问题（P44 新增/变更）

- format 6 存档的全局炉子状态按规则迁移（sole 归属 / 其余丢弃）——
  KNOWN_ISSUES 炉子节（原「单个全局容器」条目标记已修复）。
- 其余炉子 v1 限制不变（外观四面同纹理 / 无音效动画 / 燃料名单小）。

### 提交

- 显式路径提交（9 源文件 + 文档），提交信息见 git log（P44 行）。
