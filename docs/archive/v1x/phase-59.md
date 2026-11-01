# Phase 59 细节存档

> 范围（用户"按你的建议继续下两个phase" = 建议 2，代理定案为最小完整版：
> **多掉落内容再扩展 = gravel worldgen 矿脉 + flint 物品 + gravel 10%
> 概率掉落**，消费 P55 遗留"多掉落内容扩展"的第二轮；排除 stone→
> cobblestone 材料链【无 cobblestone 块，引材料链超出最小范围】、flint
> v1 无用途【文档化偏离，见 KNOWN_ISSUES】）：
> ① `gravel` 块（hardness 0.6 = MC、自掉 + `chance_drop: {flint, 1,
>    0.1}` = MC 值——P55 后单条目 chance_drop 的首个现役内容使用者）；
> ② `flint` 物品（material、64 叠、图标 = 3 片燧石碎块，生成器同款
>    确定性噪声）；
> ③ worldgen `ores.veins` + gravel（probability 0.25 / vein 4 / max_y
>    80 = 常见石替换脉，唯一的非矿石"矿脉"）；
> ④ **atlas 8×8 → 16×8**（64/64 已满，gravel = 第 65 tile → 所有既有
>    UV 移位 u=col/8→col/16 → base18 重锚，P41 先例）；
> ⑤ 3 个内容钉死单测（P55 范式）。**零核心代码**（vein block 的 init
>    交叉校验已在 WorldGenerator::create；`air` 哨兵已注册 = 场景 diff
>    能清走廊——后者是 E2E 的关键发现）。

## 阶段报告

- **`[2026-10-13, Phase 59]`** 多掉落内容再扩展：gravel + flint
  — 1010/1010（+3），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 重锚
  【atlas 16×8 移位所有 UV】+ S2 flint 掉落【roll-PASS 格，gravel +
  flint，×2 savenorm】+ S2b 无 flint 对照【roll-FAIL 格，gravel only】
  + S2ctl 无输入【×2 RAW】+ S6 init 失败 ×2）。

### 实现内容

**内容（零核心代码）**：
- `assets/data/blocks/gravel.json`：`hardness 0.6`（MC 值）、
  `drops: "gravel"`（block 物品自掉，stone 先例）、
  `chance_drop: {item: flint, count: 1, chance: 0.1}`（MC 值）、
  纹理三面向 `gravel` tile。
- `assets/data/items/flint.json`：`material` + `icon: flint`（64 叠
  默认）。
- `assets/data/worldgen.json`：`ores.veins` 加
  `{block: gravel, probability: 0.25, vein_size: 4, max_y: 80}`
  （常见、近全高；MC 的 gravel 是 stone 替换 patch，本引擎矿脉机制
  即"只替换 stone"，语义对齐）。
- `tools/generate_block_textures.py`：`gravel(x, y)`（stone 同款灰底
  + 2×2 暗卵石簇 + 浅色 fleck，salt 93..97 空位）+ TEXTURES 注册。
- `tools/generate_item_icons.py`：`flint()`（3 片尖角碎块三角形 +
  打光高光，16×16 设计空间 ×2）+ ITEMS 注册。
- **atlas**：`block_atlas.json` columns 8→16、`gravel: [8, 0]`
  （第 65 tile）——既有 64 tile 坐标不变，UV 分母变 8→16 = 全体移位。

**测试（+3，1010/1010，P55 内容钉死范式）**：
- `test_block`：`Phase59GravelDropContent`——真实 assets 断言
  hardness 0.6 / drops "gravel" / chance_drop {flint,1,0.1} / 无
  chance_drops 列表 / 非 flamable（概率笔误 0.1 vs 0.01 解析与 init
  全过，只有内容级断言能抓）。
- `test_worldgen_config`：`Phase59GravelVeinContent`——真实
  worldgen.json 断言 gravel 脉唯一 + 三参数精确值。
- `test_item`：`Phase59FlintItemContent`——真实 assets 断言 flint =
  Material / 64 叠 + gravel block 物品注册。

**无 format 变更**（format 9）。

### 关键决策

- **atlas 扩容而非复用纹理**：8×8 = 64/64 已满（P45 的"空位无 UV
  移位"先例失效）。gravel 有自己的纹理（项目纪律：每个块有自己的
  16×16 确定性纹理），复用 stone tile 是视觉质量妥协 → 扩 16×8
  （+64 空位，够后续数 phase）+ base18 重锚（P41 先例：门文件原地
  覆盖、名字与计数延续，第 33 次过门 = Phase 60）。
- **gravel 走矿脉机制**（不是 biome 装饰/表层混合）：世界空间格网
  随机游走脉 = 既有的 stone 替换通道（零代码、跨 chunk 无缝、init
  校验现成）；MC 的 gravel patch 语义最接近。
- **flint v1 无用途（文档化偏离）**：MC 的 flint 进箭头/燧石与钢
  合成链；本项目箭是直接物品、无箭头链 → 引一条只服务 flint 的
  合成链超出"内容扩展"最小范围。加用途 = 数据零核心代码，留后续。
- **排除 stone→cobblestone**：无 cobblestone 块；"破坏 stone 掉
  cobblestone"是材料链重构（改变 stone 现有自掉行为 = 破坏既有内容
  契约），不是内容扩展。
- **E2E 场景 = 真实 roll 预计算 + 真实 raycast 复验**（P55 范式）：
  破坏掉落是 per-cell 确定性 → 探针（rollChanceDrop + raycastBlock
  真实函数）先定"哪格掉 flint、哪格不掉"，场景放格后 E2E 只是复验。
  S2 与 S2b 同块同跑法不同格不同掉落 = roll 隔离证明。

### E2E 证据链（flint 掉落的证明方式）

- **S1 重锚**：fresh 60f ×2 字节一致（确定性先于重锚成立）→ 与旧
  base18 必不一致（UV 移位）→ 覆盖门文件；二跑与已重锚门文件字节
  一致 = 重锚稳定。
- **S2 flint 掉落**（420f ×2 savenorm【剥 {hunger, saturation,
  remaining, age}，物品位置保留】字节一致 + save EXACT）：gravel 放
  roll-PASS 格 (21,46,−1)（真实 rollChanceDrop(9,·,0.1) = true），
  瞄准 yaw=π/4 / pitch=atan2(−0.12,√2) **锚定存档**（零鼠标移动，
  只有左键按住 1.0 s）→ 破坏掉 **gravel×1 + flint×1**（x 向 0.25
  分离【(21.625,46,−0.5)/(21.375,46,−0.5)】、y46 落地 = 列顶 45 草
  方块顶面），终态 diff = 3 格走廊 air（目标格恢复生成态 air = 条目
  消失）。**无 P59 则该格只掉 gravel**（chance_drop 不存在）= flint
  证明。
- **S2b 无 flint 对照**（同块、P52 验证几何 (22,46,−5) yaw 0）：该
  格 roll = false → 只掉 **gravel×1**（无 flint）= per-cell
  确定性 roll 隔离（同跑法不同格不同果，非每次运行的随机）。
- **S2ctl 无输入**（S2 场景零输入，240f ×2 RAW 字节一致）：目标完好
  （diff 含 gravel + 3 走廊 air）、零实体 = 掉落来自破坏输入而非
  场景装载。
- **S6 init 失败 ×2**（scratch assets）：vein block 名 typo →
  "worldgen ore: unknown block 'grvael'"（WorldGenerator::create 交叉
  校验，gravel 脉依赖它）/ flint.json 删除 → "block 'gravel'
  chance_drop names unknown item 'flint'"（P49 物品存在性校验点名）。

### 会话存档

- **45° 角点射线（本 phase 最大的坑）**：S2 首版选 (21,46,−1) 后，
  首跑 ×2 不一致——掉落多 2..3 块**对角** dirt。诊断：yaw 恰 π/4 →
  射线 |dx|=|dz| → **每格角点同 t 跨越 x/z 边界**，DDA 角点
  tie-break 选中对角单元 (19,46,−2)（而非手算的 (19,46,−3) 路径）
  → 连破对角 y46 固块，数量 = hold 时长 × fps 的函数（PITS #7 的
  fps 敏感形态）。修复 = 场景 diff 显式清 3 个角点格（**真实
  raycast 模拟**定序列：放目标→raycast→setBlock air→再 raycast 循环
  = 目标唯一、reach 内无后续）；`air` 是注册哨兵（block diff 可写
  "air"）。备选（未用）= 换 yaw-0 走廊格——但 seed 9 走廊 z −1..−27
  全 roll FAIL（0.9^27 ≈ 4%），走廊格不可行。
- **P52 走廊 roll 全 FAIL**：(22,46,−5)（P55 验证格）roll = false，
  且 z −6..−27 全 false——S2b 顺势用 (22,46,−5) 做**无 flint 对照**
  （FAIL 格 = 天然对照场景，一石二鸟）。
- **zsh 脚本两坑**（各废一批）：`$(cat helpers.zsh)` 独立成行 = 执行
  输出（command not found）→ `eval "$(cat ...)"`；外层 `python3 -
  << 'EOF'` 补丁内容含恰好一行的 EOF（内嵌 zsh heredoc 结束标记）→
  外层 heredoc 提前终止 → 换外层标记。
- **run_break 时序**（P55 模式复述）：launch() 窗口稳定化后**立即
  返回**，游戏后台跑 420f ≈ 7..18 s；输入锚（读日志 t_ml + 2.9 s）
  与 mousedown/hold/up 必须全在 `wait $game` 之前；每跑终态存档 wait
  后立即 cp（下一跑源副本会覆盖 data/saves/<tag>.json）。
- **gravel 纹理/图标**：生成器重跑 = 既有 PNG 全字节一致（确定性），
  只新增 2 个 PNG；flint 图标 260 opaque px（3 碎块）合理。
- **矿脉生效验证**：探针计数 seed 9 = 301 块 / base seed = 225 块
  （64×64×64 区域）——脉不是死的。
- 脚本：`/tmp/opencode/p59_e2e.zsh`（全批：S1/S2/S2b/S2ctl/S6；
  助手块 = P58 的 unmap_covers/launch/shot/shot3/cmpf/savenorm/
  save_exact + P55 的 save_report/skygate/heartcheck/notblack/
  run_break 模式）；探针：`p59roll.cpp`（roll 预计算 + 走廊检查）/
  `p59aim.cpp`（候选格真实 raycast + 下方地形）/ `p59seq.cpp`（顺序
  破坏序列模拟——角点问题诊断工具）/ `p59scan.cpp`（走廊 roll 扫描）/
  `p59vein.cpp`（矿脉计数）。
