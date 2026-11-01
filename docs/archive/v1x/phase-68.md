# Phase 68 细节存档

> 范围（用户"好的,下一阶段完成建议1" = 建议 1，代理定案为最小
> 完整版：**铲类工具 = 土类分块效率**，消费 P67 铺好的 `fast_against`
> 机制 + 部分消费 P11"工具无分块类型效率"的铲分类遗留；**零核心代
> 码** = 纯内容，P62 石镐同款路径）：
> ① 内容 wooden/stone/iron_shovel（对 dirt/grass/sand/gravel ×2 = MC
>    铲对土 2×，项目土类全集——无 clay/sandstone/snow；基础 1.0 = 手
>    速 = MC 铲对石 1×；攻击 2.5/3/3.5 = MC 精确值）；
> ② 3 个 MC 竖列配方（M/S/S）+ 3 图标（每物品独立 PNG，无 atlas
>    重锚）。
> **无 format 变更**（format 10，纯物品/配方内容）、**无核心代码变
> 更**（机制 = P67 的 `fast_against` + `effectiveBreakSpeed`）。

## 阶段报告

- **`[2026-10-14, Phase 68]`** 铲类工具 — 1061/1061（+2），`-Werror`
  恒 0 警告；E2E 批跑全绿（S1 第 41 次结构性免疫【无重锚】+ S2 铁铲
  破 dirt【0.375 s << 1.6 s hold，掉 dirt×1，×2 savenorm】+ S2b 铁铲
  对石对照【2.25 s > hold 不破 = 倍率不越清单，×2 RAW 存档+截图】+
  S6 init 失败 ×2）。

### 实现内容

**内容（零核心代码——机制 = P67）**：
- `assets/data/items/{wooden,stone,iron}_shovel.json`：type tool、
  `fast_against: ["dirt","grass","sand","gravel"]` +
  `fast_against_multiplier: 2.0`（MC 铲对土 2×；**项目土类全集**——
  本项目无 clay/sandstone/snow，故清单 = 这 4 块）、**无
  `tool.break_speed`** = 缺省 1.0 = 手速（MC 铲对石 1×）、
  `attack_damage` 2.5 / 3 / 3.5（**MC 精确值**：同档 = 镐 +0.5 = MC
  增量同款 2.5−2 / 3−2.5 / 3.5−3，项目压缩镐刻度 2/2.5/3 保持该增
  量）、max_stack 1。
- `assets/data/recipes/{wooden,stone,iron}_shovel.json`：shaped 竖列
  `M/S/S`（= MC 铲形：顶 1 材料 + 下 2 木棍；单列 pattern = 解析器
  1..3 字符行合法，无新解析代码）：木 = planks、**石 =
  cobblestone**（P61 材料链的第三个工具用途）、铁 = iron_ingot，各 +2
  stick，输出 ×1。
- `tools/generate_item_icons.py`：`shovel(blade, bevel, salt)`（顶部宽
  铲斗 + 外缘亮 bevel + 底行圆角 + 4px 竖柄，MC 铲轮廓）+ 3 条目；**
  图标 = 每物品独立 PNG**（`textures/items/`），全量再生 = 既有图标
  字节不变（确定性再生验证）。

### 变更文件

- 新：`assets/data/items/{wooden,stone,iron}_shovel.json`、
  `assets/data/recipes/{wooden,stone,iron}_shovel.json`、
  `assets/textures/items/{wooden,stone,iron}_shovel.png`
- 改：`tools/generate_item_icons.py`（shovel() + 3 条目）
- 改：`tests/test_item.cpp`（+1：Phase68ShovelItemContent 三铲全字段
  钉死）、`tests/test_recipe.cpp`（+1：Phase68ShovelRecipeContent 三
  配方竖列钉死）
- 文档：ROADMAP、KNOWN_ISSUES（工具无完整分块类型效率 → 演进：铲类
  别落地）、MECHANICS（方块交互节：分块类别加铲）、GAMEPLAY（工作台
  条目）、AGENTS.md、本存档、SESSION_CONTEXT.md（本地）

### 测试

**1061/1061（+2）**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_item.cpp`（+1）：Phase68ShovelItemContent——3 铲循环：type
  Tool / max_stack 1 / breakSpeed 1.0（MC 铲对石 1×）/ fastAgainst
  恰 ["dirt","grass","sand","gravel"]（项目土类全集精确集断言，防概率
  笔误）/ multiplier 2.0（MC 2×）/ attack 2.5、3、3.5（MC 精确值）/
  icon 名。
- `test_recipe.cpp`（+1）：Phase68ShovelRecipeContent——3 配方循环：
  Shaped / 输出 ×1 / 单列 pattern 3 行各 1 字符（行 0 = 头材料、行
  1..2 = 双木棍）/ key 映射 planks/cobblestone/iron_ingot + stick。
- 解析拒绝与 `effectiveBreakSpeed` 纯函数 = P67 已覆盖（本阶段零核心
  代码），init 交叉校验走 E2E S6（Game init 级，P64/P65/P66/P67 同
  款）。

### 构建与验证结果

- 构建 0 警告；测试 1061/1061。
- E2E（批 2 全绿；批 1 失败 = E2E 脚本漏建 `p68_helpers.zsh`【`shot3`
  command not found——脚本工程问题，非项目缺陷】，修复后整批重跑）：
  - **S1 第 41 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节
    一致（铲不在默认背包 + 配方只在 UI = 零世界状态，P62/P67 同款论
    证；图标 = 每物品独立 PNG，block atlas 未动 = 无重锚）。
  - **S2 铁铲破 dirt**（420f，输入：aim 锚定存档零鼠标 + 左键 hold
    **1.6 s**）：dirt 放 P52 已验证格 (22,46,-5)；有效速度 1.0 × 2.0
    = 2.0 → 1.5 × 0.5 / 2.0 = **0.375 s << 1.6 s** 破（real-dt 计时，
    fps 无关）；破后余量 1.2 s 射线越过 reach 5 = 无 PITS-7 续破；
    终态 blocks = {}（格回生成 air）+ **恰 1 个 dirt×1 实体停
    P59/P61/P67 证明落点 (22.5, 45, -4.5)**（精确值断言）、fires =
    []，×2 savenorm 字节一致（age = 唯一 real-dt 字段；静止位是断言
    本体）。
  - **S2b 铁铲对石对照**（同格 stone、同 hold）：石 1.5 × 1.5 / 1.0
    = **2.25 s > 1.6 s** → 完好、零实体（**若倍率误越清单 = 1.125 s
    < hold 会破 = 可区分**）；终态零 real-dt 字段 → **存档 + 截图 ×2
    RAW 字节一致**（比 savenorm 更强的断言）。
  - **S6 init 失败 ×2**（scratch 资产）：iron_shovel.json
    fast_against ["dirtz"] → "item 'iron_shovel' fast_against[0]
    references the unknown block 'dirtz'"；coal.json（材料）加
    fast_against → "'fast_against' is only valid for type \"tool\"
    (the break-speed channel; a material never breaks faster)"。

### 已知问题（演进后）

- KNOWN_ISSUES「工具无完整分块类型效率」（P11 → P67 → P68）：铲类别
  落地（土类 ×2 = 项目土类全集）；仍缺镐分类（本项目镐 = flat 乘数，
  MC 镐对石类/矿类 2×）+ "无效率工具慢 10×"规则；方块侧无类别字段。

### 下一步候选（P69 定案输入）

- 剩余 KNOWN_ISSUES 大项：镐分类（`fast_against` 加石类/矿类——机制
  已铺好，纯内容，成本最低）/ 火接触伤害 / campfire（火把交互扩展：
  烤食物）/ 吸收层两分量模型（P56 遗留大改）/ 熔岩湖 worldgen（用户
  "留到很久的未来"）。
