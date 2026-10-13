# Phase 62 细节存档

> 范围（用户"先建议2,再建议1" = 建议 2，代理定案为最小完整版：**石头
> 工具线 v1 = 石镐**（镐路线中档，纯数据）；项目**无斧类工具** →
> "石斧" = 新工具型（用途语义：对树/肉的倍率）超出内容 phase 最小
> 范围【文档化偏离，见 KNOWN_ISSUES】）：
> ① `stone_pickaxe`（tool、max_stack 1、`break_speed 3.0` /
>    `attack_damage 2.5` = 项目压缩刻度上**恰居中**于木镐 2.0/2 与
>    铁镐 4.0/3；图标 = 共用 `pickaxe()` 函数的锻灰色调）；
> ② 配方 `stone_pickaxe`（shaped **CCC/.S./.S.**，C = cobblestone，
>    S = stick = MC 配方——**cobblestone 首个工具用途**：P61 材料链
>    闭环 = 挖石→锻石→石镐→更快挖铁）。
> **零核心代码**（tool 字段 / shaped 配方 / 图标全既有机制）。

## 阶段报告

- **`[2026-10-14, Phase 62]`** 石头工具线 v1：石镐
  — 1022/1022（+2），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 35 次
  结构性免疫【无重锚】+ S2 工具实战【stone 破坏掉 cobblestone×1，
  ×2 savenorm】+ S2ctl 无输入【×2 RAW】+ S6 init 失败 ×2）。

### 实现内容

**内容（零核心代码）**：
- `assets/data/items/stone_pickaxe.json`：`type: "tool"`、
  `tool: {break_speed: 3.0}`、`attack_damage: 2.5`、`max_stack: 1`、
  `icon: "stone_pickaxe"`。数值定案 = **项目压缩刻度恰居中**（木镐
  2.0/2 → 铁镐 4.0/3；MC 原始刻度木 2/石 4/铁 6、攻击木 3/石 4/铁 6
  与项目既有值不成比例 → 沿用项目刻度取中点，attack_damage 是 float
  允许 2.5）。
- `assets/data/recipes/stone_pickaxe.json`：shaped，
  `pattern: ["CCC"," S "," S "]`、`key: {C: cobblestone, S: stick}`
  （MC 配方；wooden/iron pickaxe 的 `PPP`/`III` 同款形状换 C）。
- `tools/generate_item_icons.py`：`stone_pickaxe()` =
  `pickaxe((140,140,146), (178,178,184), salt=55)`（锻灰 = 木棕与
  铁亮灰之间的色调）+ ITEMS 注册 → `stone_pickaxe.png`。
- 生成器重跑 = 既有图标全字节一致（确定性），只新增 1 个 PNG。

**测试（+2，1022/1022）**：
- `test_item`：`Phase62StonePickaxeItemContent`——真实 assets 断言
  Tool / maxStack 1 / **breakSpeed 3.0 恰居中**（注释钉住
  木 2.0 < 石 < 铁 4.0 关系）/ **attackDamage 2.5** / icon 名。
- `test_recipe`：`Phase62StonePickaxeRecipeContent`——真实 recipes：
  Shaped / output stone_pickaxe×1 / pattern 三行精确值 / key 恰 2 条
  且 C=cobblestone、S=stick（逐格钉死 = MC 配方防笔误）。

**无 format 变更**（format 9）。

### 关键决策

- **石镐 only（无石斧）**：调研 = 项目物品/配方目录无任何 axe；
  "斧"在本项目没有用途语义（对树/肉的破坏倍率不存在）→ 引入斧 =
  新工具型 + 新语义 = 超出内容 phase。石镐单独已完整交付"镐路线
  中档"（木→石→铁三档齐了）并消费 P61 的 cobblestone = 材料链闭环
  （挖石→锻石→石镐→更快挖铁→铁镐）。
- **数值 = 项目刻度取中点（非 MC 原始值）**：项目既有 木 2.0/铁 4.0
  与 MC 的 2/6 不成比例（P20 时代定的压缩刻度）→ 石 = 3.0（恰居中）
  而非 MC 的 4.0（= 会与铁镐的 4.0 打平，中档失去意义）。攻击 2→2.5
  →3 同款居中。
- **E2E = 工具实战场景（而非纯配方 UI 操作）**：S2 = stone 破坏场景
  换 slot0 为石镐——同时证明 ① 工具物品在游戏内注册可用（破坏耗时
  0.75 s < hold 1.0 s = 破坏发生本身即证明 break_speed 生效）② P61
  掉落链（掉 cobblestone）③ 确定性（×2 savenorm）。配方正确性 = 单测
  钉死（P55/P60 范式：UI 合成操作 E2E 成本高、机制在 P17/P40 已回归）。

### E2E 证据链（工具的证明方式）

- **S1 第 35 次结构性免疫**（**无重锚**）：工具物品**不在默认背包** +
  配方只在 UI + 图标无 UI 不采样 → fresh 60f ×2 + == base18 字节
  一致（mismatch = FAIL 而非重锚）。
- **S2 工具实战**（420f ×2 savenorm 字节一致 + save EXACT）：stone 放
  (22,46,−5)（P52 几何，P59/P61 复验）、slot0 = stone_pickaxe、瞄准
  锚定存档（零鼠标移动）、t_ml+2.9 s 左键 hold 1.0 s → 终态 diff =
  {}、**entities = [cobblestone×1]** @ (22.5,45,−4.5)、health 20 /
  pos / yaw-pitch 不变。
- **S2ctl 无输入**（240f ×2 RAW 字节一致）：stone 保留、零实体。
- **S6 init 失败 ×2**（scratch assets）：stone_pickaxe.json
  `break_speed: "fast"` → "'tool.break_speed' must be a number"（工具
  字段严格数值）/ 配方 key C→'cobblestonne' → "key entry 'C' uses
  unknown item 'cobblestonne'"（shaped key 交叉校验点名）。

### 会话存档

- **镐路线数值刻度**（供后续工具扩展）：wooden 2.0/2 → **stone
  3.0/2.5** → iron 4.0/3（项目压缩刻度；若加钻石镐 = 5.0/4 同款
  等差）。
- 脚本：`/tmp/opencode/p62_e2e.zsh`（全批：S1/S2/S2ctl/S6；
  助手 = p59_helpers.zsh 前缀改 p62_；由 p61 脚本 sed 定制——
  **定制点 = slot0 物品 + S6 两 case + 头注释/S1 计数**，逐节核过）；
  日志 `/tmp/opencode/p62e2e/p62_e2e.log`。
- **P63 调研（本 phase 会话内完成，未动工作树）**：flamable 扩展 =
  wood/leaves/grass 三块纯数据（MC 天然可燃三块 + P58 已做的
  wheat_1..7）；fresh 世界零熔岩 = 燃烧 pass 惰性 → 第 36 次免疫；
  P58 S4 熔岩池几何【池 (22,45,−5)/(22,45,−4) L8 + 3 石缘 +
  可燃块 (23,45,−5)】可复用于"熔岩烧木头"场景（wheat_3 换 wood）。
