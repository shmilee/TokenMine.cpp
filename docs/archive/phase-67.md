# Phase 67 细节存档

> 范围（用户"下两个阶段是 建议1 和 建议3" = 建议 3，代理定案为最小
> 完整版：**斧类工具 = 数据驱动分块效率工具型**，全量消费 P62 遗留
> "无斧类工具" + 部分消费 P11"工具无分块类型效率"）：
> ① 物品可选字段 `fast_against`（非空块名数组）+ `fast_against_
>    multiplier`（>0 数）严格配对 + `effectiveBreakSpeed(item, block)`
>    纯函数 + init 交叉校验；
> ② 内容 wooden/stone/iron_axe（对 wood/leaves ×2 = MC、基础 1.0 =
>    手速、攻击 3/3.5/4）+ 3 个 MC L 形配方 + 3 图标（每物品独立
>    PNG，无 atlas 重锚）。
> **无 format 变更**（format 10，纯物品/配方内容）、**无核心 ItemType
> 变更**（斧 = 带 fast_against 的 tool，不加新枚举——P62 注记"新工具
> 型"的语义落点 = 分块倍率数据）。

## 阶段报告

- **`[2026-10-14, Phase 67]`** 斧类工具 — 1059/1059（+11），`-Werror`
  恒 0 警告；E2E 批跑全绿（S1 第 40 次结构性免疫【无重锚】+ S2 铁斧
  破木【1.5 s < 1.6 s hold，掉 wood×1，×2 savenorm】+ S2b 铁斧对石
  对照【2.25 s > hold 不破 = 倍率不越清单，×2 RAW 存档+截图】+
  S6 init 失败 ×2）。

### 实现内容

**核心（`src/item/item.{hpp,cpp}` / `src/item/item_registry.{hpp,cpp}`）**：
- `Item.fastAgainst`（`std::vector<std::string>`）+
  `Item.fastAgainstMultiplier`（float，0 = 无）：`fast_against` 非空
  数组 + `fast_against_multiplier` >0 数，**严格配对**（缺一拒绝）、
  **空数组拒绝**（no-op 纪律，P53 同款）、**仅 type "tool"**（通道
  所有权纪律——材料带此字段 = 破坏速度通道被非工具占用，解析拒绝）、
  元素非字符串/空串逐条目点名 `fast_against[i]`、multiplier 非数/≤0
  点名。
- `effectiveBreakSpeed(const Item& target...)` 纯函数（item.hpp，
  `breakDurationSeconds` 旁）：非 tool = 1.0；tool = `breakSpeed` ×
  （目标块名在 `fastAgainst` 内 ? `fastAgainstMultiplier` : 1）。
  破坏耗时公式本体（`kBreakBaseSeconds × hardness / speed`）未动——
  倍率并进 speed。
- `Game::selectedBreakSpeed(const Block&)`（原无参）：两调用点
  （updateBreaking 计时 + 裂纹阶段渲染）传目标块。
- **init 交叉校验**（game.cpp，grantsEffects 条目循环同款位置）：
  每条目 `fast_against[i]` 必须指向存在的方块，点名
  "references the unknown block '...'"（grows_on 纪律：typo 必须
  init 响亮失败，而不是静默按常速破坏）。

**内容**：
- `assets/data/items/{wooden,stone,iron}_axe.json`：type tool、
  `fast_against: ["wood","leaves"]` + `fast_against_multiplier: 2.0`
  （MC 斧对木 2×）、**无 `tool.break_speed`** = 缺省 1.0 = 手速
  （MC 斧对石 1× = 徒手节奏，项目同款）、`attack_damage` 3 / 3.5 /
  4（各比同档镐 +1 = 项目压缩刻度，MC 斧 > 镐）、max_stack 1。
- `assets/data/recipes/{wooden,stone,iron}_axe.json`：shaped L 形
  `PP/PX/ X`（= MC 斧形，空格 = 空）：木 = planks、**石 =
  cobblestone**（P61 材料链的第二个工具用途）、铁 = iron_ingot，各 +2
  stick，输出 ×1。
- `tools/generate_item_icons.py`：`axe(head, bevel, salt)`（左上块状
  斧头 + 左缘亮刃 + 4px 斜柄，pickaxe 柄同款）+ 3 条目；**图标 = 每
  物品独立 PNG**（`textures/items/`），全量再生 = 既有图标字节不变
  （确定性再生验证）。

### 变更文件

- 改：`src/item/item.{hpp,cpp}`（字段 + 纯函数）、
  `src/item/item_registry.{hpp,cpp}`（parseFastAgainst + 白名单 +
  调用）、`src/core/game.{hpp,cpp}`（selectedBreakSpeed 签名 + 两
  调用点 + init 交叉校验）
- 新：`assets/data/items/{wooden,stone,iron}_axe.json`、
  `assets/data/recipes/{wooden,stone,iron}_axe.json`、
  `assets/textures/items/{wooden,stone,iron}_axe.png`
- 改：`tools/generate_item_icons.py`（axe() + 3 条目）
- 改：`tests/test_item.cpp`（+9：解析 6 / 纯函数 3——内容钉死合入
  Phase67AxeItemContent 1）、`tests/test_recipe.cpp`（+1 三配方循环
  钉死）
- 文档：ROADMAP、KNOWN_ISSUES（无斧类工具 → 斧 v1 范围 / 工具无分块
  类型效率 → 演进）、PITS（+1）、MECHANICS（方块交互节）、GAMEPLAY
  （工作台条目）、AGENTS.md、本存档、SESSION_CONTEXT.md（本地）

### 测试

**1059/1059（+11）**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_item.cpp`（+10）：FastAgainstAcceptsToolPair /
  FastAgainstDefaultsAbsent / FastAgainstRejectsMissingPartner（两向）
  / FastAgainstRejectsEmptyList / FastAgainstRejectsBadEntryAnd
  Multiplier（`fast_against[1]` 点名 + multiplier 0）/
  FastAgainstRejectsMaterial / EffectiveBreakSpeedFastAgainstNamed
  BlocksOnly（木 2×、叶 2×、**石 1×**）/
  EffectiveBreakSpeedFlatToolStaysFlat（铁镐 4× 对木石皆 4）/
  EffectiveBreakSpeedNonToolsAreOne / Phase67AxeItemContent（3 斧
  全字段钉死 = 真实资产）。
- `test_recipe.cpp`（+1）：Phase67AxeRecipeContent（3 配方循环：
  L 形 pattern 结构 + 头材料映射 planks/cobblestone/iron_ingot +
  stick + 输出 ×1）。
- init 交叉校验走 E2E S6（Game init 级，无单测 = P64/P65/P66 同款）。

### 构建与验证结果

- 构建 0 警告；测试 1059/1059。
- E2E（批 2 全绿；批 1 仅 S2 的掉落落点 sanity 范围写错——机制全对，
  修正断言后整批重跑）：
  - **S1 第 40 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节
    一致（轴不在默认背包 + 配方只在 UI = 零世界状态，P62 镐同款论证；
    图标 = 每物品独立 PNG，block atlas 未动 = 无重锚）。
  - **S2 铁斧破木**（420f，输入：aim 锚定存档零鼠标 + 左键 hold
    **1.6 s**）：wood 放 P52 已验证格 (22,46,-5)；有效速度 1.0 × 2.0
    = 2.0 → 1.5 × 2.0 / 2.0 = **1.5 s < 1.6 s** 破（real-dt 计时，fps
    无关）；余量 0.1 s 内射线走已验证空气位 (22,46,-6) = 无 PITS-7
    续破；终态 blocks = {}（格回生成 air）+ **恰 1 个 wood×1 实体**
    停 **P59/P61 证明落点 (22.5, 45, -4.5)**（精确值断言）、
    fires = []，×2 savenorm 字节一致（age = 唯一 real-dt 字段；静止
    位是断言本体）。
  - **S2b 铁斧对石对照**（同格 stone、同 hold）：石 1.5 × 1.5 / 1.0
    = **2.25 s > 1.6 s** → 完好、零实体（**若倍率误越清单 = 1.125 s
    < hold 会破 = 可区分**）；终态零 real-dt 字段（hunger 未 tick、
    无实体/火、裂纹 transient 已清）→ **存档 + 截图 ×2 RAW 字节
    一致**（比 savenorm 更强的断言）。
  - **S6 init 失败 ×2**（scratch 资产）：iron_axe.json fast_against
    ["woodz"] → "item 'iron_axe' fast_against[0] references the unknown
    block 'woodz'"；coal.json（材料）加 fast_against → "'fast_against'
    is only valid for type \"tool\" (the break-speed channel; a
    material never breaks faster)"。

### 已知问题（演进后）

- KNOWN_ISSUES「无斧类工具」（P62 注记）→ **斧 v1 范围**：fast_against
  只 wood/leaves ×2、无"肉/战斗"倍率（对 mob = 平 attack_damage，MC
  斧对生物亦无特殊倍率）、无耐久（项目无耐久系统）。
- KNOWN_ISSUES「工具无分块类型效率」（P11）→ 演进：首个分块类别已落地
  （物品侧块名列表，方块侧无类别字段）；仍缺镐/铲分类（本项目无铲）+
  "无效率工具慢 10×"规则。

### 下一步候选（P68 定案输入）

- 剩余 KNOWN_ISSUES 大项：吸收层两分量模型（P56 遗留大改）/ campfire
  （火把交互扩展：烤食物 + 座位）/ 火接触伤害 / 铲类工具（土类倍率，
  fast_against 已铺路）/ 熔岩湖 worldgen（用户"留到很久的未来"）。
