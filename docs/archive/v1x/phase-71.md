# Phase 71 细节存档

> 范围（用户"请继续未完成的 phase" = 延续定案建议 1，代理定案为最小
> 完整版：**campfire = 烤食物 + 座位**，消费火把 v1 条"无 campfire"
> 遗留）：`campfire` 块（cross、光 15、`interact: "campfire"` = 第三
> 交互词汇表值）+ MC 配方 + 单槽烤制（per-block 状态，炉子 P44 范式）
> + 烤制中物品渲染 + 座位（crouch 高度 = 坐姿）+ format 11 `campfires`。

## 阶段报告

- **`[2026-10-14, Phase 71]`** 营火 — 1092/1092（+20），`-Werror` 恒
  0 警告；E2E 批跑全绿（S1 第 44 次结构性免疫【无重锚】+ S2 烤制完成
  【×2 savenorm】+ S2b 熟取回 + S2c 生取回【各 ×2 savenorm】+ S2r 图标
  渲染【×2 RAW + 1192 px 像素 diff】+ S3 座位【×2 savenorm】+ S3ctl
  对照 + S6 init 失败 ×2）。

### 实现内容

**内容（3 件，全 JSON + 纹理）**：
- `assets/data/blocks/campfire.json`：cross 模型、`light_emission 15`
  （= MC 营火光）、hardness 0.5、**`drops: campfire`**（自掉可复用）、
  **非 flamable**（是火源外观不是可燃物）、`interact: "campfire"`。
- `assets/data/recipes/campfire.json`：shaped `SSS/LCL/LLL`（S = stick、
  L = wood、C = coal = MC 营火配方）。
- 纹理：`tools/generate_block_textures.py` 新 `campfire()`（log 行
  12..15 + 泪滴火焰行 1..11）+ atlas **空位 [12,0]**（68→69 tile，
  既有 tile 字节不变 = 无 UV 移位 = 无重锚）。

**核心**：
- `src/block/block.hpp` / `block_registry.cpp`：**`kInteractCampfire`**
  词汇表第三值（crafting_table / furnace / campfire；未知值错误消息
  点名全部三值）。
- `src/item/campfire.{hpp,cpp}`（新模块，纯函数 = 可单测）：
  `CampfireCookState {ItemStack slot; int progressSubsteps}` +
  `tickCampfireCook(state, item, substeps)`——**固定 1/60 子步 + 整数
  进度**（与炉子同类：无输入运行逐字节可复现）；速度 = **炉子节奏**
  （复用物品 `smelt_time` 数据、`smeltSubstepsFor`——无独立 config、
  无燃料 = MC 营火自燃）；**进度不变量**：progress > 0 ⇒ 槽恰 1 个
  可熔炼物（`smeltable_to` 非空 = 数据驱动门，项目 = 生牛肉/羊肉/鸡
  肉）；完成 = 槽换产物、progress 0（产物非可熔炼 → 熟品静置）。
- `src/core/game.{hpp,cpp}`（七处落点，炉子 P44 同款收口）：
  - `m_campfires`（`std::map<BlockPos, CampfireCookState>` 位置序 =
    确定性 tick 序）；setBlock 收口 emplace/erase。
  - 右键链：**营火分支在 `tryEat` 之前**（targetBlock 命中 `kInteract
    Campfire` → `tryCampfireCook` 消费点击）——MC 语义：手持食物右键
    营火 = 放上火上，不开吃。这是**必需**的前置：项目全部可熔炼物品
    都是食物，若进食通道先消费点击则烤槽永远填不上。炉子/工作台保留
    既有"手持食物先开吃"行为（既有偏离不变）。
  - 交互规则：手持可熔炼 + 空槽 = 放上 1 个（消耗 1、"place" 音）；
    **空手** + 非空槽 = 取回（生熟均可 = MC 空手规则、"pickup" 音）；
    其余组合 no-op；点击恒被消费（营火不放置）。
  - tick 循环（每状态每帧 1 子步）+ `drawCampfireCookItems`（槽非空：
    物品图标 0.5 交叉 quad 悬浮格上半部 y+0.75，cross 块惯例：视角
    无关、opaque pass、深度遮挡、+Y 假法线 = 顶面亮度档；空槽零绘制
    = 结构性免疫）+ render pass 挂载。
  - collectSave/applySave + **format ≤10 迁移**（无条目；手编旧文件
    带 campfire 块编辑 → 空状态）。
- `src/player/player.{hpp,cpp}`：**`m_campfireBlockId`**（构造时
  `idForName("campfire")` 解析一次，仿 `m_fireBlockId`；registry 无 =
  永不坐）+ **`m_sitting`**（每 tick 重算：crouch && onGround &&
  `overlapsBlock(m_campfireId)` 严格不等式 AABB 重叠；**不入存档**）
  → wish 清零（sprint 条件随之失效）+ `!m_sitting` 挡跳跃；crouch
  高度即坐姿（文档化偏离：无独立坐姿）。
- `src/core/save.{hpp,cpp}`：**format 11 `campfires`**——每 campfire
  块一条目（`pos` + `item`/`count` + 整数 `progress_substeps`），位置
  排序，与方块差集**双向**交叉校验（entry 无块 / 块无 entry 均拒）+
  进度不变量；**format 10 文件带 `campfires` 键 = 拒绝**（新键只属于
  新 format）。

### 变更文件

- 新：`src/item/campfire.{hpp,cpp}`、`tests/test_campfire.cpp`
- 新：`assets/data/blocks/campfire.json`、`assets/data/recipes/
  campfire.json`、`assets/textures/blocks/campfire.png`
- 改：`src/block/block.hpp`、`src/block/block_registry.cpp`（词汇表
  第三值）、`src/core/game.{hpp,cpp}`（七处落点）、`src/core/
  save.{hpp,cpp}`（format 11）、`src/player/player.{hpp,cpp}`（坐位）、
  `assets/data/textures/block_atlas.json`（空位 [12,0]）、`tools/
  generate_block_textures.py`（`campfire()`）、`CMakeLists.txt`
- 改：`tests/test_player.cpp`（fixture 加 campfire 块 + 座位 5 测试）、
  `tests/test_save.cpp`（15 处 format 10→11 适配：strip `campfires`
  ×14 + `asInt(),10→11` ×3）
- 文档：ROADMAP、KNOWN_ISSUES（火把 v1 条"无 campfire"已解决 + 新
  营火 v1 条）、MECHANICS（营火节）、GAMEPLAY（营火节）、PITS
  （+2）、AGENTS.md、本存档、SESSION_CONTEXT.md（本地）

### 测试

**1092/1092（+20）**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_campfire.cpp`（15）：
  - tick ×4：空槽 no-op / 进度推进（1/60 子步精确值）/ 完成换产物
    progress 0 / 产物非可熔炼静置。
  - interact 词汇 ×2：`"campfire"` 解析 / 未知值错误点名三值。
  - save ×7：format 11 往返 / 双向交叉校验（entry 无块、块无 entry）/
    进度不变量拒 / format 10 迁移空 / format 10 带 campfires 键拒。
  - 内容钉死 ×2：加载真实 `assets/data/blocks/campfire.json` +
    配方断言字段精确值（防概率/笔误，P55 同款）。
- `test_player.cpp`（+5，fixture 加 campfire 块）：
  SittingOnCampfireLocksMovement（crouch + 重叠 → 2 帧 pos 不动）/
  CrouchWalkWithoutCampfireStillMoves / UncrouchStandsUp /
  SittingBlocksJump / SittingNeedsGround（空中不坐）。
- `test_save.cpp`：15 处 format 10→11 适配（既有断言语义不变）。

### 构建与验证结果

- 构建 0 警告；测试 1092/1092。
- E2E（`/tmp/opencode/p71_e2e.zsh`；场景存档 = p27far 模板 + format 11
  全键 + seed 9；瞄准 pitch = p71scan 探针实证 **-0.220362**
  【HIT (22,45,-5) n=(0,0,1) dist 4.61 < reach 5】）：
  - **S1 第 44 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节
    一致（campfire 永不 worldgen + 默认背包无 + 配方只在 UI + atlas
    空位无 UV 移位 = 无输入零世界状态；无重锚）。
  - **S2 烤制完成**（900f 输入跑，+2.9s 右键放 beef，×2 savenorm +
    EXACT）：600 固定子步 = 10 s 炉速 → **12.9 s @60Hz / 28.9 s
    @23fps 完成，恒先于终态**（900f 最短墙钟 39.1 s，fps 无关
    margin）；终态 slot0 beef×2 + campfire 槽 cooked_beef×1
    progress 0 + blocks/pos/yaw/pitch/health/实体 全部 EXACT。
  - **S2b 熟取回**（场景预置 progress 590【帧 10 熟】+ 空手右键，
    200f ×2 savenorm）：终态 slot0 cooked_beef×1 + campfire 槽空。
  - **S2c 生取回**（预置 progress 0 + 空手右键，×2 savenorm）：终态
    slot0 beef×1 + campfire 槽空（取回重置进度）。
  - **S2r 图标渲染 + S2ctl 空槽对照**（均 no-input 600f，×2 **RAW**
    截图+存档字节一致）：600f = 10 s 让 chunk 流式落定（P64 60f 竞
    态条款）且 36 s 饥饿 tick 不触发（23fps 最坏 26 s < 36 s）= 零
    real-dt 字段移动；预置 beef@300 → **帧 300 烤成**（终态槽
    cooked_beef@0，渲染路径与进度无关）；像素 diff vs 对照 box
    [560..720 × 270..410] = **1192 px**（>100 阈值；火焰纹理两场景
    相同 = 抵消，隔离悬浮图标）。
  - **S3 座位**（输入跑 300f，**ctrl 先按 +0.1 s 再按 W** 2.0 s，
    ×2 savenorm + EXACT）：坐位在 W 生效前建立 → **pos 精确
    (22.5,45,0.5) 恒零漂移**（w-first 顺序会泄漏相位依赖的 1–2 帧
    漂移 ≈0.09 格 = 两次运行不一致，PITS 已录）；campfire 块仍在。
  - **S3ctl 对照**（同输入无 campfire，×1 关系断言）：crouch-walk
    2 s ≈ 2.58 格 → pos.z < 0.0（无营火则移动不被锁）。
  - **S6 init 失败 ×2**（scratch 全量 assets 拷贝）：interact
    `"campfirez"` → 词汇表错误点名 / 配方 key L `"woodz"` → "key
    entry 'L' uses unknown item" 点名。
  - **PITS +2**：xdotool 键名 `ctrl` ≠ `Left_Control`（非法名**静默
    忽略** = 输入静默失效）；键组合的 keydown 顺序 = 确定性变量
    （状态建立键先于移动键）。

### 已知问题（演进后）

- KNOWN_ISSUES「火把 v1 范围」：~~无 campfire~~ → **`[Phase 71]` 已解
  决**（campfire 块 = 单槽烤制 + 座位）。
- 新增「营火 v1 范围（文档化偏离）」：无燃料（= MC 自燃，非偏离）/
  速度 = 炉子节奏（MC 同款）/ **无水浇灭**（P64 fire 块有，营火
  没有）/ **单槽**（MC 营火 4 槽）/ 只烤可熔炼物品（MC 营火也只烤
  食物）/ 烤制中无噼啪音效 / **Mob 不可坐** / **坐姿 = crouch 高度**
  （无独立坐姿）/ **营火不是 fire 块**（无寿命条目、不蔓延、站里面
  不烧）/ 右键链营火分支先于进食（MC 语义；炉子/工作台保留既有进食
  优先 = 既有偏离不变）。

### 下一步候选（P72+ 定案输入）

- **吸收层两分量模型**（P56 遗留大改：`health` + `absorption` 独立
  池、伤害先扣吸收、heal 只填 health、format 12 迁移）= 定案建议 2。
- campfire v2（水浇灭 / 多槽 / Mob 座位）/ Mob 火接触（火 v3）/
  "无效率工具慢 10×"规则（P11 条唯一剩余）/ 熔岩湖 worldgen（用户
  明示"留到很久的未来"，勿选）。
