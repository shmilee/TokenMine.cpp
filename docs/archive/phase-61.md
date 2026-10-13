# Phase 61 细节存档

> 范围（用户"下两个 phase，请继续建议1 和建议2" = 建议 2，代理定案为
> 最小完整版：**stone→cobblestone 材料链 = cobblestone 块 + stone 自掉
> 变更【内容契约变更】+ 4 锻石配方**，MC 材料循环；cobblestone 熔炼
> 回 stone = 文档化偏离【block item 无 item 数据，见 KNOWN_ISSUES】）：
> ① `cobblestone` 块（hardness 2.0 = MC、自掉、纹理 = stone 底 + 深色
>    裂纹缝【交错 5px 缝格、块心提亮，salt 91..92】）；
> ② `stone.json` `drops: "stone" → "cobblestone"`（MC：挖天然石得锻石；
>    **内容契约变更**——stone 只能经合成回来）；
> ③ 配方 `stone`（shapeless 4 cobblestone → 1 stone = MC 1.14+）；
> ④ atlas 空位 [9,0]（65→66/128、grid 16 不变 = 无 UV 移位 = **无重锚**）。
> **零核心代码**（drops/配方/atlas 全既有机制；契约变更安全性 = 调研
> 审计：无配方以 "stone" 为原料、只有 stone.json 掉 "stone"）。

## 阶段报告

- **`[2026-10-14, Phase 61]`** stone→cobblestone 材料链
  — 1020/1020（+2），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 34 次
  结构性免疫【无重锚】+ S2 掉落变更【stone 破坏掉 cobblestone×1 而非
  stone×1，×2 savenorm】+ S2ctl 无输入【×2 RAW】+ S6 init 失败 ×2）。

### 实现内容

**内容（零核心代码）**：
- `assets/data/blocks/cobblestone.json`：`hardness 2.0`（MC 值）、
  `drops: "cobblestone"`（自掉，gravel/stone 先例）、full_cube 三面向
  `cobblestone` tile。
- `assets/data/blocks/stone.json`：`drops: "stone" → "cobblestone"`
  （**本 phase 唯一的既有文件行为变更** = 内容契约变更，文档化）。
- `assets/data/recipes/stone.json`：shapeless，output {stone, 1}，
  ingredients [cobblestone×4]（MC 1.14+ 的 2×2 合成 = 本引擎的
  shapeless 4 原料袋）。
- `tools/generate_block_textures.py`：`cobblestone(x, y)`（108 灰底 +
  交错 5px 缝格【cy==4 或 (x+row·2)%5==0】= 深色裂纹缝 72 级、缝外
  块心提亮 ±12、salt 91..92 空位）+ TEXTURES 注册。
- **atlas**：`block_atlas.json` + `cobblestone: [9, 0]`（第 66 tile，
  **空位**——grid 16 不变 = 无 UV 移位 = 无重锚；P59 重锚后的扩容
  先例未再触发）。
- 生成器重跑 = 既有 PNG 全字节一致（确定性），只新增
  `assets/textures/blocks/cobblestone.png`。

**测试（+2，1020/1020）**：
- `test_block`：`Phase61CobblestoneContent`——真实 assets 断言
  cobblestone hardness 2.0 / drops "cobblestone" / dropsCount 1 / 非
  flamable + **stone drops = "cobblestone"**（钉死契约变更本身——
  drops 名 typo 解析全过，只有内容级断言能抓）。
- `test_recipe`：`Phase61StoneRecipeContent`——真实 recipes：shapeless
  / output stone×1 / 恰好 1 原料 cobblestone×4。

**无 format 变更**（format 9）。

### 关键决策

- **熔炼偏离 = 文档化，不动核心**：MC 的 cobble→stone 主路线是熔炼，
  但 `smeltable_to` 是**物品字段**而 block item 无独立 JSON（隐式
  生成、无 item 数据）——给 block item 加数据覆盖 = 核心改动（物品
  注册表 + 存档语义），超出"内容 phase"最小范围。4 锻石合成（MC
  1.14+ 同样存在）已完整覆盖转换 → KNOWN_ISSUES 文档化偏离。
- **4 cobble → 1 stone（MC 1.14+ 值）**：代理建议原文写"2 cobble→1
  stone"，定案从 MC（4）——项目纪律 = MC 值优先；shapeless 4 原料
  袋 = 既有 schema 直接表达（2×2 网格放 4 个 = 匹配）。
- **契约变更安全性 = 调研审计先行**（本会话内，P60 E2E 等待期）：
  `grep '"stone"' recipes/` = 无配方以 stone 为原料；`grep drops` =
  只有 stone.json 掉 "stone"（obsidian/熔岩反应的 "stone" = 方块放置
  不是 drops，不受影响）→ 变更不破坏任何既有内容链。
- **S2 场景 = P52 几何 + 破坏范式复用**：stone 放 (22,46,−5)（P59
  已复验 HIT + 掉落落位 (22.5,45,−4.5)），铁镐 hold 1.0 s（破坏
  1.5×1.5/4 = 0.5625 s，P55 margin 纪律；P52 走廊 reach 内无后续
  块 = 按住余量安全）。**断言 = drop item 名**（cobblestone ≠ stone
  = 契约变更的直接证明；无 P61 则掉 stone×1 → 断言失败）。

### E2E 证据链（掉落变更的证明方式）

- **S1 第 34 次结构性免疫**（**无重锚**）：cobblestone tile = **空位**
  【grid 16 不变 = 无 UV 移位】+ drops 变更**只在破坏时生效**（无
  输入 = 零破坏）+ cobblestone 不出现在 fresh 世界（只作为掉落/
  放置）→ fresh 60f ×2 + == base18（P59 基线）字节一致。脚本对
  mismatch **FAIL 而非重锚**。
- **S2 掉落变更**（420f ×2 savenorm 字节一致 + save EXACT）：stone 放
  (22,46,−5)、slot0 = iron_pickaxe、瞄准锚定存档（零鼠标移动）、
  t_ml+2.9 s 左键 hold 1.0 s → 终态 diff = {}（目标格恢复生成态 air）、
  **entities = [cobblestone×1]** @ (22.5,45,−4.5)（**item 名 =
  cobblestone 而非 stone** = MC 材料循环生效）、health 20 / pos /
  yaw-pitch 不变。
- **S2ctl 无输入**（S2 场景零输入，240f ×2 RAW 字节一致）：stone 保留
  （diff = {stone}）、零实体 = 掉落来自破坏输入而非场景装载。
- **S6 init 失败 ×2**（scratch assets）：cobblestone.json **删除** →
  "block 'stone' drops unknown item 'cobblestone'"（P49 物品存在性
  校验点名——契约变更的**依赖方向**被钉死：stone 现在依赖
  cobblestone 存在）/ 配方 stone.json cobblestone→cobblestonne →
  "recipe 'stone' uses unknown ingredient item 'cobblestonne'"。

### 会话存档

- **零核心代码 phase 的完整形态**：块 + drops 变更 + 配方 + atlas 空位
  + 纹理，全部既有机制；调研审计（配方依赖/drops 依赖/atlas 空位/
  smeltable 词汇表）先于实现 = 契约变更的安全网。
- **P61 调研在 P60 E2E 等待期完成**（不改工作树 = 两 phase 的 git 边界
  干净）；`smeltable_to` 持有者全为 material（beef/chicken/mutton）=
  block item 熔炼不可行的直接证据。
- 脚本：`/tmp/opencode/p61_e2e.zsh`（全批：S1/S2/S2ctl/S6；
  助手 = p59_helpers.zsh 前缀改 p61_；run_break = P59 原样复用）；
  日志 `/tmp/opencode/p61e2e/p61_e2e.log`。P60 的 zsh 续行教训已应用
  （本批多行 echo 全部带反斜杠，零脚本 bug）。
