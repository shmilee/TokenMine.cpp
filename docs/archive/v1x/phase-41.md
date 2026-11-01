# Phase 41 细节存档

> 用户确认方向（经 question 工具选"4. wool 后续（推荐）"）。
> 范围 = P12/P20 遗留"wool 无用途/无染色"：可放置的 wool 方块 + 数据驱动
> 染色系统（染料物品 + 可染方块家族 + 右键换色交互）。

## 阶段报告

- **`[2026-10-11, Phase 41]` wool 后续：wool 方块 + 染色系统** — 9 个
  wool 方块（白 base + 8 染色变体）+ 9 种染料物品 + 9 条 shapeless 配方 +
  方块属性 `dye_base`/`dye_color` + 物品字段 `dye_color` + 右键染色交互
  （换块 + 消耗 + cloth 音效）+ init 交叉校验（含"每个 (base, 颜色) 变体
  必须存在"）。无存档格式变更（format 6，diff 按块名存）。

  **数据驱动字段**
  - 物品 `dye_color`（`item_registry` 新 `parseDye`，白名单 9 色：
    white/black/brown/red/yellow/orange/pink/gray/silver；显式空串拒绝 =
    ammo 纪律；whitelist +1 键）。
  - 方块 `dye_base` + `dye_color`（`block_registry` 解析；两者必须同时
    给；`dye_base` 与 `interact` 互斥；显式空串拒绝）。
  - **命名约定 `dyeVariantName(base, color)`**（`block.hpp` 单一定义，
    交互与 init 交叉校验共用）= `"<base>_<color>"`，**white 特例 = base
    名本身**（`wool_white` 不存在，白色就是 `wool`）。
  - 数据：9 方块（`wool` + `wool_<color>`；hardness 0.8 = MC、掉自身、6 面
    同 tile）；9 物品 `dye_<color>`（material、max_stack 64、16×16 图标）；
    9 配方：白←骨×1→×2、黑←煤×1→×2、棕←腐肉×1→×2、黄←金锭×1→×2、
    红←牛肉×1、灰=白+黑、橙=红+黄、粉=红+白、银=灰+白（MC 数值/形状）。
  - **wool 物品冲突解法**：`wool` 块与既有显式 material 物品 `wool.json`
    同名 → 删显式物品（`items/wool.json` + `textures/items/wool.png`），
    隐式 block item 继承名字（hotbar 图标变 3D 方块体，KNOWN_ISSUES 记录）。
  - **atlas 4×8 → 8×8**（32→64 格，用 40）：既有 tile 的 [col,row] 不变，
    新 tile = wool [3,7]（旧空位）+ 8 染色 [4..7,0..1]。但所有既有 UV 的
    x 缩放从 /32 变 /64 → **无输入基线门必须重锚**（P18/P36 先例；本阶段
    不是"结构性免疫"计数阶段）。

  **init 交叉校验（ammo/smelt 同款模式，game.cpp）**
  - dyeable 块必须有 `dye_color`（registry 已挡，此处挡"颜色无染料物品"）；
  - 块的 `dye_color` 必须是某个染料物品的颜色；
  - **每个 (dye_base, 白名单颜色) 组合的变体块必须存在**（缺 = 死染料，
    报错点名 base+颜色）——加新颜色时全 dye 家族都要补变体块。

  **染色交互（game.cpp handleInteraction，右键顺序：食物 → launcher →
  interact → 染色 → 放置）**：`targetBlock()` 命中格 `dyeBase` 非空 +
  选中格物品 `dyeColor` 非空 + 目标变体 `!= 现名` → `setBlock(变体)` +
  `Lighting::updateLocal`（染色前后都 opaque，光不变，重算是保险）+
  `take(slot, 1)` + `play("cloth")` + **return 吞点击**（染料非方块物品，
  不放置）。**同色重复 = no-op 不消耗不拦截**（fall through 到 placeBlock
  无操作——文档化 v1 规则，MC 会消耗，KNOWN_ISSUES 记录）。

  **纹理/音效（全程序化确定性生成，既有资产重跑字节零漂移已验证）**
  - `generate_block_textures.py`：`WOOL_PALETTE`（wool (232,230,224) / red
    (185,54,64) / yellow (232,199,60) / orange (240,118,19) / pink
    (234,138,195) / brown (119,81,51) / gray (62,68,71) / black
    (25,23,25) / silver (142,142,147)）+ `make_wool_tile`（base 色 + 单
    noise(±12) 全通道——色相恒定）；盐值 = 100 + 排序 index。
  - `generate_item_icons.py`：删 `wool()`，+`DYE_PALETTE` +
    `make_dye_icon`（盐值 70 + 排序 index）。
  - `generate_sounds.py`：+`cloth`（Lcg(0xC107)）；SoundBank 目录自动
    加载全部 .wav，14 样本。
  - 世界生成不含 wool（新块不进 biome 数据 → 新世界零 wool = base 门
    休眠论证）。

  **测试**：762/762（+11）——test_block 8（DyeFieldsDefaultToEmpty /
  ParsesDyeFamily / ParsesDyedVariant / RejectsDyeColorWithoutBase /
  RejectsEmptyDyeFields / RejectsDyeableWithInteract /
  DyeVariantNameWhiteIsTheBase / DyeVariantNameAppendsTheColor）+
  test_item 3（ParsesDyeColor / DyeColorAbsentByDefault /
  RejectsBadDyeColor）。`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
  启动冒烟：atlas 128×128（40 tile）、26 块注册、14 音效、交叉校验通过。

  **E2E（批跑 + 判别补跑，全绿）**
  - **S1 base 门 + 重锚**：P41 ×2 + P40 代码参考构建（worktree @a47f8c5
    独立 build）×1 新世界 60f **三方字节一致** → 重锚 base18.png（名字
    沿用 P36 先例）+ skygate（FRESH 模式色）。休眠论证：新世界无 wool
    块（非 worldgen 产出）、无 dye 在手、染色需 dye 物品。
  - **S2 回归 A/B**（p27far 夜 360f，seed 9，skeleton 红纱场景）：P41 ×2
    存档字节一致（确定性权威）+ 截图 veil 容差 ×2（real-dt 红纱边界，
    100% 像素 ≤1/通道，P27 同签名）+ **P40 ref 同场景存档跨构建字节
    一致**（无 format 升格 → 跨构建 A/B 有效，与 P40 相反）+ 出生序列 +
    health 8。
  - **S3 染色渲染场景**（正午、mobs off、p27far 派生存档放 wool
    (22,45,−2) + wool_red (23,45,−2)，300f 无输入完全静态）：存档 ×2
    字节一致 + 截图 ×2 字节一致 + **像素断言（颜色分类法）**：194,561
    diff = 194,559 wool 色（顶面 ×1.0 精确 + 正面 **×0.612**【shader
    环境光下限，见下】±1）+ 2 光照边界 texel 翻转（≤20/通道）+ 0 其他。
  - **S4 染色输入运行**（S3 场景 + slot0 `dye_red`×3 + pitch
    atan(−1.12/1.5) = −0.6413643599084533 瞄准白面中心 (22.5,45.5,−1)）：
    锁指针（左键，留微量裂纹）→ 右键染色：存档**精确**（两块皆
    wool_red、dye 3→2、**pos** [22.5,45,0.5] / yaw / pitch 零漂移、
    health/hunger 20/20、无 fluids）+ 截图 vs 无输入对照：world diff
    87,418 **全红色类/暗裂纹（0 bad）** + 准星 40（锁定时 UI，文档化
    例外）+ HUD 21,059（hotbar 计数 3→2）。
  - **判别补跑**：首批 S4 的一次输入运行玩家凭空走 0.575 块（z 0.5→
    −0.075 = 4.3 块/s × ~8 帧 ≈ 134ms 的 W 键）——判别实验（仅锁指针+
    左键 / 完整输入 各一次）|dz| = 0 两次 → 物理按键污染（非游戏 bug），
    复跑后 S4 存档/像素全过。

  **像素断言的相机/着色器模型（本轮实测校准，PITS 详）**
  - 投影：70° = **垂直** fov；`sx` 用 `tan(35°)×(1280/720)`、`sy` 用
    `tan(35°)`（初版两轴同用 tan(35°) → x 压缩 9/16 → bbox 全错）。
  - 光：`basic.frag` = `base × (0.03 + 0.97 × max(sky×day, block) ×
    faceShade)` → 顶面恰 ×1.0、±Z 面 **×0.612**（非 ×0.6）、±X 0.806、
    底 0.515。
  - 断言法：颜色分类（确定性纹理色集 × 光公式）而非投影 bbox——两个
    不同纹理相邻块的渲染共享边实测偏离分析投影最远 ~20px（确定性、
    跨运行稳定，成因未明）；bbox+颜色必然假阳性。

## 会话存档

- **命名撞车**：wool 块 vs 既有 `wool.json` 显式物品——选"删显式物品、
  用隐式 block item"而非"改块名"（块名 `wool` 是 dye 家族 base，改名会
  传染 9 个变体名 + 配方 + 掉落数据）；代价 = hotbar 图标从 2D 变 3D
  方块体（KNOWN_ISSUES 记录）。
- **同色重复 no-op**：不拦截不消耗（点击落到 placeBlock，染料非方块物品
  = 无操作）。MC 会消耗 1 个。v1 规则文档化——拦截需要"消费式"交互
  分支（先消耗再检查），收益低。
- **base18 重锚链**：atlas 扩容全 UV 移位 → 不能直接沿用 P40 参考图；
  重锚 = P41 新图，但先证明 P41 == P40 代码参考构建逐字节（新世界无
  wool = C++ 变更休眠）→ 重锚链可信（P18/P36 同款）。
- **S4 一次运行被物理 W 键污染**（134ms ≈ 8 帧，走 0.575 块）：先被带
  偏去怀疑相机模型（截图整体错位 → 投影"全错"），实际判别实验 10 分钟
  定位。教训已进 PITS：**pos 是移动漂移探测器**（同 yaw/pitch），输入
  E2E 存档断言必须含 pos 精确值。
- **物品名 `dye_<color>`**：E2E 脚本手滑写成 `red_dye` → init 报
  `save slot #0 holds unknown item`（注册日志明明 id=40）——手工构造
  存档的物品/方块名一律抄注册日志/数据文件。
- **准星 diff**：输入 vs 无输入对照截图必差 ~40px 白色加号（准星只在
  指针锁定时渲染，MECHANICS 已文档化）——断言给中心 20×20 框近白像素
  开例外。
- **下一步候选**（SESSION_CONTEXT）：5. 弓后续 2（3D 手部/插墙箭寿命/
  箭处置/击退随蓄力）/ 6. 新食物剩余（牛奶桶最小；面包/金苹果需种植/
  苹果来源）/ 8. per-block 炉子状态 / 染色 7 色补全（需新材质来源 +
  白名单扩容）。
