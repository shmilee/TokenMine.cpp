# Phase UX-5 — 耐久修复系统（v1.x 任务表 38/40，门 159，UX 行 5/7）

> 消费 SURV-3「无修复系统（矩阵外记档）」条 = **工作台材料修复（MC 铁砧
> 语义的 crafting-grid 形式）**：受损工具/护甲 + 材料放入合成网格 → 输出
> 槽 = 修复后的物品（1 材料 = +25% max 耐久），点击输出取走。数据驱动
> （`repair_with` 字段），全管线复用 CRAFT-1 工作台 + SURV-3 耐久。
> 本版不打 tag（**tag v1.40.0 = UX-5+6 合并**）。

## 定案

- **物品 JSON 可选严格字段 `repair_with`**（材料物品名；显式空串/非串
  点名拒 = ammo 纪律；**配对纪律：无 `durability` 的物品携带 = init
  点名拒**；init 交叉校验材料存在 = grows_to 纪律；解析白名单 +1 项）。
- **修复量 = MC 铁砧**：1 材料恢复 `kRepairFraction 0.25f` × max 耐久
  （damage −= max×0.25 钳 0；float 固定运算序 = 确定性；命名常量零
  config——修复规则 = 核心行为，材料 = 内容数据）。
- **匹配 = 配方回退**（GridMatch 尾加 `optional<RepairMatch>`）：配方
  恒优先，无配方匹配时 `matchRepair`（格索引序首个受损可修物品 × 首个
  其材料格）。shipped 实证碰撞 = 受损弓 + 钻石 → infinite_bow 配方胜 /
  受损弓 + 棍 → 无配方 → 修复（Priority 专测钉死）。
- **交互 = 既有输出槽**：点击输出 = 修复执行（材料 −1、工具格变修复后
  堆、工具随 carry 出格 = 取结果）；零新布局/零新面板/零新键。
- **28 件 shipped 全配 MC 精确材料**（brief 27 件 = SURV-3 期计数；
  SURV-5 poison_bow 亦带 durability 384 = 弓族第 6 件，fixer 勘察实证）：
  wood 系 3 → planks / stone 系 3 → cobblestone / iron 系 3 → iron_ingot
  / 弓族 6 → stick / flint_and_steel → flint / 皮甲 4 → leather / 铁甲
  4 → iron_ingot / 钻甲 4 → diamond。
- **E 面板 2×2 同款生效**（match() 共享；MC 铁砧仅限工作台 = 记档偏离）。
- **flint_and_steel maxStack 64**（注册表默认，既有内容）→ applyRepair
  **保格内 count**（修复永不改变件数；max-stack-1 工具行为逐位同 brief；
  PreservesTheToolCount 单测钉死）。

## 实现

1. `src/item/repair.{hpp,cpp}`（新，campfire 纯函数范式）：
   `matchRepair`（格索引序配对，nullopt = 无）/ `applyRepair`（原位
   damage 减 + 材料 take(1)）/ `checkRepairMaterials`（init 交叉校验，
   CRAFT-3 block_sound 范式：registry 序首败点名）。
2. `item_registry`：`parseRepair` 严格解析 + 白名单 +`repair_with` +
   `Item::repairWith` 字段。
3. `crafting_grid`：GridMatch +repair（matchGridRecipe 返回值位级不变
   = 既有 140+ 调用/测试零改动）。
4. `inventory_ui`：`match()` 无配方回退调 matchRepair；`clickOutput`
   前置 repair 分支（守卫 = 异 item / 同 item 异 damage / maxStack 溢出
   三重拒【SURV-3 同 damage 合并纪律——修复输出携 damage，光标混合
   damage 未定义】+ m_carry.damage 赋值【新 carry 恒 0，harness 首轮
   揪出】）。
5. `game_ui.cpp` 输出段双分支构造 out（repair 携 outputDamage →
   drawIcon 耐久条自动显示，零布局变化）。
6. `game_init.cpp` buildScene：`MC_TRY(checkRepairMaterials(m_items))`。
7. 28 件 JSON 各 +1 行 + 版本 1.38.0→1.39.0。

**零 format（22：damage 已入档，修复结果 = damage 值）/ 零 config / 零
新资产 / 零新音效（"craft" 音 clickOutput 成功路径自动）。**

## 测试

1975/1975（+29）、`-Werror` 0 警告、banner v1.39.0 实证：

- **MatchRepairTest 11**：空格 / 满耐久 / 无材料 / 错材料 / exact
  62.5（铁镐 125→62.5）/ 多受损工具取最低格（皮盔+铁镐双受损 → 格序
  胜）/ 多材料取最低格 / 无 repair_with 惰性 / 钳制 0 / 3 回合收敛
  187.5→125→62.5→0
- **ApplyRepairTest 2**：原位 + 耗 1 / 保 count（flint 堆）
- **RepairPriorityTest 2**：配方胜（受损镐 + 铁锭成 reinforced 配方格
  → match 返回配方、repair nullopt）/ 无配方 → 修复
- **RepairParseTest 3** + **RepairInitTest 4**（空串 / 非串 / 无
  durability / 未知材料 "ingotz" 全点名原文）
- **RepairContentTest 2**：28 件精确映射表 + 无 durable 外携带
- **RepairUiTest 5**（InventoryUI 组合件 harness）：四回合协议
  240→177.5→115→52.5→0 + ingot 8→4 + 第 5 击 no-op / 右键同左 / 异
  carry 拒 / 异 damage 拒 / 同 damage 合并

## 门 159（编排者执行，old 侧 = v1.38.0 = eb7badc = tm_v139_base）

- **S1 = 第 159 次结构性免疫 + base18 锚连续 168140 B 免重锚**（fresh
  60f 四方 ×2 全字节一致 + 跨版 savenorm EXACT；无输入无工作台无面板 =
  match() 零执行 + repair 纯数据惰性）。
- **S2 = 修复输入跑 PASS**（协议 = 捕获 → 右键开台【crosshair 对
  (0,54,−3) 工作台，dist 3.54 < kReach 5.0，射线 y=54.73 入格解析
  预验】→ 9 点击：slot0 取受损铁镐(187.5) → grid4 放 → slot1 取铁锭×8
  → grid1 放 → **输出点击【修复 187.5→125.0，铁锭 8→7】** → grid4 取回
  → slot0 放回；窗口 679×744 整屏内验证【PITS 340 类：WM 平铺原点
  漂移 (328,452) 半出屏 → 点击钳位，改"整窗在屏内"验证协议】+
  getmouselocation 落位验证无变量污染 + WID 逐点击复查 + 1200f 预算）：
  - **NEW ×2 RAW 字节一致 203181 B + 终态 slot0 = iron_pickaxe damage
    125.0 exact【187.5 − 250×0.25 解析值】+ ingot 7 + cell4 空**
  - **OLD ×2 RAW 字节一致 202634 B + 终态 slot0 空 + ingot 8 + cell4 =
    iron_pickaxe（输出 no-op：无配方无修复）= 互斥**
  - **跨侧 savenorm diff 恰 6 字段全在修复语义**（slot0 count/damage/
    item + ingot 7/8 + cell4 占位）——**世界零 diff = 同世界最强形**
  - 像素 diff 445 px 限 bbox [12,175,314,249] = 面板区（slot0 图标 +
    铁锭计数 + cell4），世界区零
  - **PITS 345**：表格/网格槽存档丢弃 damage（serializeSlotEntry =
    CRAFT-1 "槽带 damage 拒"纪律）→ 修复终态断言必须落 **inventory
    槽**（首跑把工具留在 grid = save 无 damage 键误判"修复未生效"，
    浪费 1 跑）
- **S3** = 1975/1975。**S6 = ×3 精确点名 PASS**（"item 'leather' has
  'repair_with' but no 'durability' (repair is only for durable items)" /
  "item 'iron_pickaxe' references unknown repair material 'ingotz'" /
  "'repair_with' must be a non-empty item name"）。
- **性能 A/B v1.38.0 vs v1.39.0（发布前硬门槛）PASS**：fall 900f 三窗
  upd med 0.374/0.392/0.356 vs 0.378/0.352/0.401、render med
  0.428/0.458/0.424 vs 0.408/0.429/0.450、swap med 15.77/15.68/15.80 vs
  15.78/15.82/15.67 = 全指标同档；PIPE3-S0 changesApplied **9/3/1 三窗
  逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 = 同世界
  同成本【结构惰性最强形：无工作台无面板 = match 零执行】**。

## 偏离/记档

- **任意 crafting grid 可修**（E 面板 2×2 同款；MC 铁砧 = 独立工作台
  方块矩阵外）
- **无 XP 成本 / 无成本递增**（项目无 XP 系统；MC 反复修复成本上升不
  建模 = 记档）
- **配方恒优先**（可修物品作配方原料时修复让位——shipped 弓族实证；
  数据驱动无核心 switch）
- **表格槽不入 damage**（CRAFT-1 既有纪律；修复中间态在格内运行时有效，
  落 inventory 才持久——E2E 断言纪律 PITS 345）
- flint_and_steel ×64 堆修复保 count（MC 同款：铁砧修 1 件不拆堆——此处
  整堆同 damage 语义，堆 = 1 逻辑件）
