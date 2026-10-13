# Phase 65 细节存档

> 范围（用户"按建议顺序，继续完成下两个" = 建议 2，代理定案为最小
> 完整版：**火把 torch**；campfire/火蔓延超出最小内容范围【P62 无斧
> 类工具同款文档化偏离，见 KNOWN_ISSUES 火把 v1 范围条目】）：
> ① `torch` 块（cross + 火把纹理 atlas 空位 [11,0] 免重锚、光 14 =
>    MC、hardness 0 瞬破、drops torch = 可复用、非 flamable = 光源不
>    是火源）；
> ② 配方 shapeless coal+stick → 4 火把（MC；coal 首个合成用途——P40
>    起只是炉子燃料）；
> ③ bug 修复：破坏 cross 块崩溃（P45 潜在，见下）。
> **无 format 变更**（format 10，torch = 普通块编辑）。

## 阶段报告

- **`[2026-10-14, Phase 65]`** 火把内容：torch 方块 + cross 块破坏
  崩溃修复 — 1036/1036（+2），`-Werror` 恒 0 警告；E2E 批跑全绿
  （S1 第 38 次结构性免疫【无重锚】+ S2 放置【石块锚点顶面，×2
  RAW】+ S3 夜间渲染+光【600f，A≠B + 火焰像素 + ×2 各】+ S4 破坏
  → 掉 torch 物品【左键按住，×2 savenorm】+ S2ctl 无输入【×2 RAW】
  + S6 init 失败 ×2）。

### 实现内容

**内容**：
- `assets/data/blocks/torch.json`：hardness 0.0（瞬破）、solid/opaque
  false、transparent true、`light_emission 14`（MC 火把值）、
  `drops: torch`（掉自己 = 可复用，与 fire 的无 drops 相反）、非
  flamable（光源不是火源：不引燃相邻可燃块）、cross 模型 + `torch`
  纹理。block item 自动（可放置 = 永久营火）。
- `tools/generate_block_textures.py`：`torch()` tile（16×16 RGBA 透明
  底；2px 棕色木棍 row 6..15 + 顶部泪滴火焰 row 1..5 = 黄芯/橙/深红
  三层 + 逐行 hash 倾斜，与全项目纹理同款确定性）；TEXTURES 注册。
- `block_atlas.json`：`"torch": [11, 0]` = **空位**（67→68/128）→
  无 UV 移位 → **无重锚**。
- `assets/data/recipes/torch.json`：shapeless `coal` ×1 + `stick` ×1 →
  `torch` ×4（MC 配方；coal 从"只有炉子燃料"升级为真材料）。

**核心（bug 修复：破坏 cross 块崩溃）**：
- 症状：左键破坏 torch（hardness 0 瞬破）后 ~0.5 s 游戏崩溃
  `std::out_of_range: unordered_map::at`；gdb 回溯 =
  `Game::drawParticles → AtlasLayout::uvMin`。
- 根因：`drawParticles` 对所有块的破坏粒子用 `cubeFaceTexture`
  （top/side/bottom 槽）查纹理；cross 模型（P45 引入）没有这三项
  纹理（只有 `cross`）→ 空名 → atlas `unordered_map::at` 抛异常。
  P45 起的**潜在 bug**——此前无任何 E2E 破坏过 cross 块（作物/花/
  火把都能触发）；item_mesh（物品 3D 立方体）与 chunk_mesher（世界
  cross 面片）两个既有路径各有 cross 分支，唯独粒子路径漏了。
- 修复：`drawParticles` 加 cross 分支——粒子小立方体六面全用
  `cross` 纹理（与上述两分支同款单一查找），~6 行。
- KNOWN_ISSUES 已修复索引 +1 条（strikethrough 条目）。

**无**：campfire（新交互语义）、火蔓延（火 v1 范围）、worldgen 变更、
format 变更、config 变更。

### 变更文件

- 内容：`assets/data/blocks/torch.json`（新）、
  `assets/textures/blocks/torch.png`（新）、
  `assets/data/textures/block_atlas.json`、
  `assets/data/recipes/torch.json`（新）、
  `tools/generate_block_textures.py`（torch tile）。
- 核心：`src/core/game.cpp`（drawParticles cross 分支）。
- 测试：`tests/test_block.cpp`（+1：torch 内容钉死）、
  `tests/test_recipe.cpp`（+1：配方钉死，RealAssets 范式）。
- 文档：`docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`（火把 v1 条 +
  coal 用途演进 + cross 崩溃已修复条）、`docs/PITS.md`（+3）、
  `docs/GAMEPLAY.md`（配方列表 + 火把条目）、
  `docs/archive/phase-65.md`（本文件）、`AGENTS.md`。

### 测试

**1036/1036**（+2）：
- `Phase65TorchBlockContent`：真实资产钉死（光 14、cross + torch
  纹理、hardness 0、drops torch、非 flamable、solid/opaque false /
  transparent true）。
- `Phase65TorchRecipeContent`：真实资产钉死（shapeless、output
  torch×4、ingredients coal×1 + stick×1）。

### 构建与验证

- `cmake --build build`：0 警告（`-Wall -Wextra -Wpedantic -Werror`）。
- E2E 全绿（`/tmp/opencode/p65_e2e.zsh`，批 5 rc=0）：
  - **S1 第 38 次结构性免疫**：fresh 60f ×2 + == base18 字节一致
    （**无重锚**；torch 永不生成 + 只经合成、默认背包无 + atlas 空
    位无 UV 移位）。
  - **S2 放置**（输入，420f）：石块锚点 (22,45,-4)【P54 验证生成
    空格】+ slot0 = torch，**顶面瞄准** pitch -atan2(0.62, 4.0)
    （射线 (22.5,46.0,-3.5) 中顶面、距离 4.05 < reach 5、路径全程
    y46 已验证空位 = 避开表面装饰格）→ 单次右键 = torch 立于
    (22,46,-4) + 物品消耗（slot0 空）+ **fires = []**（火把非 fire
    块：无寿命条目，永久燃烧）+ 零实体；终态无真实 dt 字段 → ×2
    RAW 字节一致 + save EXACT。
  - **S3 夜间渲染+光**（无输入，午夜 time 0.75，600f ×2 各）：
    场景 A = torch 于 P52 验证瞄准格 (22,46,-5) vs 场景 B 无 torch；
    A×2 == A'、B×2 == B' 字节、A ≠ B（torch 被绘制）、火焰色像素
    （r>180,g>60,b<140；月/星蓝白过不了蓝滤）A>20 / B<5；存档 A =
    torch 块 + fires []。
  - **S4 破坏 → 掉落**（输入，420f）：场景 = 锚点 + torch
    (22,46,-4)，pitch 0，**左键按住 1.0 s**（hardness 0 = 首帧即破）
    → 终态 diff = 仅锚点（torch 格回生成 air，无条目）+ entities
    恰 1 条 torch×1 停在锚点顶面（零初速 + 位置分离 = P58 范式确定
    性 resting 位）；×2 savenorm 字节一致（entity age 剥除）。
    **本场景 = cross 块破坏崩溃修复的回归证据**（粒子帧渲染无崩
    溃）。
  - S2ctl 无输入对照（240f ×2 RAW）：锚点保留、无 torch 出现、
    fires []。
  - S6 init 失败 ×2（scratch assets）：torch.json light_emission 16
    → `'light_emission' must be in [0, 15]`；配方原料 typo "coall"
    → `uses unknown ingredient item 'coall'`。

### 已知问题 / 文档化偏离

- **火把 v1 范围**（KNOWN_ISSUES 新条目）：无点燃/蔓延（光源不是火
  源）；无支撑要求（通用 placeBlock：任意实心面可贴含底面——MC 火
  把需支撑，本项目无支撑语义）；光恒定 14 无闪烁；无 campfire。
- **cross 块破坏崩溃**（P45 潜在 → P65 修复）：见上文。
- coal 用途条目演进：`[Phase 40]` 炉子燃料 → `[Phase 65]` 首个合成
  用途（火把）。
- PITS +3：① 瞄准 pitch 正 = 朝上（P65 首轮 S2 用 +atan2 = 朝上，
  射线飞过目标——"输入生效但世界零变化"先怀疑瞄准几何；顶面瞄准全
  程 y46 = 避开表面装饰格）；② 破坏 = 左键按住（右键按住 = 放置/
  交互链，空格 no-op——"按错键"签名与瞄准错误同形态）；③ awesomeWM
  窗口稳定化：float toggle 是 TOGGLE（每轮发会翻转）+ 窗口创建瞬态
  （1280×720@640,440 几百 ms 内被平铺）+ **60f 跑进程寿命仅 ~1.85 s
  = 稳定化全路径预算 <~1.2 s**（S1 短帧截图给外层重试层；WM 桌面/
  布局随用户活动变化 → 稳定化失败先查 `_NET_CURRENT_DESKTOP`）。

### 会话存档要点

- 调研范式（内容 phase 复用）：coal/stick 物品存在确认 + 配方
  shapeless 范式现成 + atlas 空位 [11,0] 确认（免重锚）+ block item
  自动（无 item JSON 需求——block item 渲染 3D 块立方体，item_mesh
  已有 cross 分支）+ 右键链放置分支审计（interact→dye→bucket→
  ignite→placeBlock，torch 全部 fall through 到 placeBlock）+
  破坏输入 = 左键（`updateBreaking`）。
- 瞄准几何（顶面范式，可复用）：目标 = 锚点顶面 (x, 46, z)；
  pitch = -atan2(0.62, 4.0)；眼高 46.62 → 顶面 y46 下降 0.62 跨水平
  4.0 → 3D 距离 4.05；整条路径 (22,46,0..-3) 全 y46（P62 射线已验证
  空位）= 不进表面 y45 装饰风险带。
- gdb 崩溃定位范式：`gdb -batch -ex run -ex "bt 25" -ex "thread
  apply all bt 8"` 后台跑 + 并行 xdotool 输入 = 复现 + 全线程回溯，
  一步定位 `drawParticles → uvMin`。
