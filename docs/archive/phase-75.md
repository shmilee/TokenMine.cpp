# Phase 75 细节存档

> 范围（用户"下一 phase 完成建议1" = P74 定案建议 1，代理定案为最小
> 完整版：**营火 v2 = 四槽 + 水浇灭**，全量消费 P71 遗留"单槽" +
> "无水浇灭"偏离；建议文本里的"燃料"经 P71 文档核实**不是偏离**——
> MC 营火自燃，本项目同款，故排除）：`CampfireCookState` 4 个独立
> 烤位 + 水浇灭成 `campfire_extinguished` 块（状态保留）+ 座位扩到未
> 点燃 + format 13 四槽形状。

## 阶段报告

- **`[2026-10-14, Phase 75]`** 营火 v2 — 1101/1101（+6：tick 独立 1
  + save 3 + 座位 1 + 内容钉死 1，另 4 个 tick 测试改写四槽），
  `-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 48 次结构性免疫【无重
  锚】+ S2 四槽烤【×2 savenorm + EXACT】+ S2b 取回【×2 savenorm +
  EXACT】+ S3 水浇灭【×2 RAW 存档+截图】+ S4 未点燃座位【×2 savenorm
  + EXACT】+ S6 存档加载失败 ×2）。

### 实现内容

**内容（1 新块 + 1 新纹理，全 JSON/工具链）**：
- `assets/data/blocks/campfire_extinguished.json`：cross 模型（柴堆无
  火苗纹理）、`light_emission 0`（火灭了）、hardness 0.5、自掉、非
  flamable、**`interact: "campfire"`**（同家族：烤制状态 + 座位都走
  既有交互路径）。
- 纹理：`tools/generate_block_textures.py` 新 `campfire_extinguished()`
  （= campfire 的柴堆行 12..15，**像素同源**——物理柴堆不变，只是没
  了火苗；确定性再生，既有 PNG 零改动）+ atlas **空位 [13,0]**（69→70
  tile，无 UV 移位 = **无重锚**）。

**核心**：
- `src/item/campfire.{hpp,cpp}`：`CampfireCookState` = **`slots[4]` +
  `progressSubsteps[4]`**（`kCampfireSlotCount = 4`，MC 营火 4 烤位）；
  `tickCampfireCook` 改为**每槽独立 tick**（私有 `tickSlot` 纯函数；
  各槽各自计时、互不影响——一格完成不碰别的格）。
- `src/core/game.{hpp,cpp}`（六处）：
  - init 解析家族 id：`m_campfireLitId` / `m_campfireExtId`（invalid =
    结构惰性，P70 模式）。
  - **水浇灭 pass**（每帧、烤 tick 之前）：点燃营火 6-邻任一流水体
    （任意等级）→ 快照收集后 `setBlock(extinguished)` + 局部重光照
    （光 15 → 0，P64 fire 块浇灭同款规则）；**状态保留**（setBlock
    收口对同 interact 的家族块不删状态）；无营火 fast-exit（结构性免
    疫）。
  - 烤 tick 门：**只有点燃的营火烤**（`getBlock != litId → skip`——
    未点燃 = 时钟暂停，物品保留）。
  - `tryCampfireCook`：放 = **第一个空槽**（仅点燃；四槽全满 no-op）；
    取 = **第一个非空槽**（点燃/未点燃均可，MC 空手规则）——索引序确
    定性，无面板 = 文档化简化。
  - `drawCampfireCookItems`：第一个非空槽的图标（四槽只画一个 = 文档
    化简化——一格放不下 4 个图标）。
  - `applySave` 迁移循环：家族名（`campfire` || `campfire_extinguished`
    无条目 → 空状态）。
- `src/player/player.{hpp,cpp}`：**座位扩到未点燃**（`m_campfireExtId`
  + `m_sitting = ... || overlapsBlock(ext)`——MC：柴堆没变，只是灭了；
  `overlapsBlock` 对 invalid 恒 false，旧夹具零影响）。
- `src/core/save.{hpp,cpp}`：**format 13**：
  - 写入：`campfires` 条目 = `{pos, slots: [恰 4 × {item, count,
    progress_substeps}]}`（空槽 = ""/0/0）。
  - parse：fmt ≥13 = 新形状（恰 4 校验、**每槽**严格三元组 + 每槽进度
    不变量、**家族双向交叉校验**）；fmt 11/12 = 旧单槽形状（字段白名
    单拒绝 `slots` 键 = 版本谎报）→ **单槽迁移**（旧 item/count/
    progress → 槽 0，其余空）；fmt ≤10 无键（旧规则）。
- **零新 config、零新物品/配方**。

**测试（1095 → 1101）**：
- tick：4 个 P71 测试改写四槽 + 新 `TickSlotsAreIndependent`（两槽各
  自计时、完成互不影响）。
- save：`FormatIsThirteenAndRoundTrips`（4 槽形状 + 未点燃块家族往返）
  + `FormatTwelveOldShapeMigratesToSlotZero`（单槽迁移）+
  `FormatTwelveEntryWithSlotsKeyRejected`（版本谎报）+
  `FormatThirteenEntryOldShapeRejected`（13 必需新形状）+ 3 个既有拒
  绝测试改写（家族交叉校验 / 每槽进度不变量 / 畸形条目——slots 长度 3
  等）；test_save.cpp 3 处 format 断言 12→13。
- 座位：`SittingOnExtinguishedCampfireLocksMovement`（未点燃也锁移动）。
- 内容钉死：`CampfireExtinguishedBlockContentPinned`（光 0 / interact
  家族 / 自掉 / 非 flamable / cross）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p75_e2e.zsh`）

- **S1 第 48 次结构性免疫**：未点燃块永不生成 + 默认背包无 + atlas
  空位无 UV 移位 + 浇灭 pass 无营火 fast-exit → fresh 60f ×2 + base18
  字节一致。
- **S2 四槽烤**（输入 1200f ×2 savenorm + EXACT）：P71 瞄准几何
  （campfire @(22,45,−5)、pitch −0.220362 = p71scan 真实射线证明），
  手持 beef ×4、**四次右键 0.5 s 间隔**（t_ml+2.9 起）→ 每槽 1 个、
  各自 600 子步烤熟 → 终态 **4 槽全 cooked_beef ×1** + 手空 + 时间帧
  确定（最后放置 4.4 s + 10 s = 14.4 s < 1200f 上界 20 s @60Hz，fps
  无关终态）。
- **S2b 取回**（输入 200f ×2 savenorm + EXACT）：预置槽 0 熟品 + 空手
  单次右键 → 手 = cooked_beef ×1、四槽全空（第一个非空槽规则）。
- **S3 水浇灭**（no-input 240f **×2 RAW 存档+截图字节一致** +
  EXACT）：feet 格点燃营火 + 6-邻**封闭 L8 水**（(23,45,0)：下方生成
  草支撑 + 3 石围边 + 第 4 邻 = 营火格自身——**任何块都停水横流**，
  水恒 L8 静态）。帧 1 浇灭 pass → `campfire_extinguished`（光 0）；
  终态：块 4（1 extinguished + 3 石）、水 1 条恒 L8、**烤制状态存活**
  （beef 槽 0 progress **0**——浇灭先于首烤 tick，它一次都没烤）、玩家
  20 血（AABB 不碰水格）。零 real-dt 活动 = RAW 最强形式。
- **S4 未点燃座位**（输入 240f ×2 savenorm + EXACT）：ctrl 先按再 W
  2.0 s（P71 确定性排序）→ pos 恒定 EXACT = 移动锁死（未点燃也坐得
  住）。
- **S6 存档加载失败 ×2**：恰 4 槽点名（3 槽 → "requires a 'slots'
  array of exactly 4"）/ 旧形状 unknown field 点名。
- **PITS +1**：Json 数组无元素变异 API（`arr[i] = x` 编译通过、运行
  期 "basic_string: construction from null" 误导异常 = 重建数组）。

### 变更文件

- `src/item/campfire.hpp`、`src/item/campfire.cpp`
- `src/core/game.hpp`、`src/core/game.cpp`
- `src/core/save.hpp`、`src/core/save.cpp`
- `src/player/player.hpp`、`src/player/player.cpp`
- `assets/data/blocks/campfire_extinguished.json`（新）、
  `assets/data/textures/block_atlas.json`（+1 空位）、
  `assets/textures/blocks/campfire_extinguished.png`（新，工具链产
  物）、`tools/generate_block_textures.py`
- `tests/test_campfire.cpp`、`tests/test_save.cpp`、
  `tests/test_player.cpp`
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+1）、`AGENTS.md`、
  `docs/archive/phase-75.md`

### 已知问题（本阶段）

- P71 遗留"单槽" + "无水浇灭"**已解决**（KNOWN_ISSUES 营火条 →
  "营火 v2 范围"，划删 + `[Phase 75]`）。
- 新文档化偏离（记入营火 v2 条）：**四槽无面板**（放/取 = 第一个空/
  非空槽，索引序）；**只渲染第一个非空槽图标**；**水浇灭后无再点燃**
  （MC 用燧石与钢点回——v2 不燃）；未点燃的不能放食物（没火烤不了 =
  MC 语义）。
- 建议 1 原文里的"燃料"经核实**不是偏离**（P71 文档：营火自燃 = 与
  MC 一致）——本阶段范围 = 四槽 + 水浇灭，燃料未实现（无需实现）。

### 下一步建议

1. **弓 v3 / 箭强化**（P42 遗留"无箭矢强化"——候选：无限弓或附魔箭，
   数据驱动物品字段，无 format 变更）。
2. **营火 v3**（低优先）：再点燃（燧石与钢对 `campfire_extinguished`
   的交互 = 新 interact 词汇表值或扩 `ignites_flamable` 语义）+ 四槽
   图标全渲染。
3. 信标（吸收第二来源，需方块实体系统 = 高成本，可拆 v1）。
4. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
