# Phase 64 细节存档

> 范围（用户"按建议顺序，继续完成下两个" = 建议 1，代理定案为最小
> 完整版：**火视觉 v1 = fire 方块**；粒子/蔓延超出最小范围【见
> KNOWN_ISSUES 火 v1 范围条目】）：
> ① `fire` 块（cross + 火焰纹理 atlas 空位 [10,0] 免重锚、光 15、
>    瞬破、无掉落、非 flamable = 燃烧终态）；
> ② 燃烧目标改 fire：点燃器（P60）+ 熔岩燃烧 pass（P58）的
>    `setBlock(air)` → `setBlock(fire)`（registry 无 fire 块时回落 air
>    = 单测 fixture 语义）；
> ③ `Game::m_fires` 剩余寿命 map（setBlock 收口创建/删除 = 炉子范式；
>    real-dt 倒计时）+ config `fire.lifetime_seconds`（默认 4.0，>0 硬
>    校验）+ 燃尽回 air；
> ④ 水浇灭（fire 6-邻任一流水格 → 瞬灭；水胜火 = P57；熔岩不浇灭）；
> ⑤ init 交叉校验（有 flamable 块必须有 fire 块，点名拒绝）；
> ⑥ **format 10** `fires`（{pos, remaining}，双向交叉校验 = 炉子键
>    纪律，format 9 迁移 = 空 + 手编旧文件补满寿命条目）。
> **无 worldgen 变更**。

## 阶段报告

- **`[2026-10-14, Phase 64]`** 火视觉 v1：fire 方块 — 1034/1034
  （+11），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 37 次结构性免疫
  【无重锚】+ S2 点燃器 → fire 活体终态【MOV 300，×2 savenorm】+
  S3 燃尽【600f ×2 RAW】+ S4 水浇灭【高架水，×2 RAW】+ S5 夜间
  渲染+光【600f，A≠B + 火焰像素 + ×2 各】+ S2ctl 无输入【×2 RAW】+
  S6 init 失败 ×2）。

### 实现内容

**内容**：
- `assets/data/blocks/fire.json`：hardness 0.0（瞬破）、solid/opaque
  false、transparent true、`light_emission 15`（MC 值）、**无 drops**
  字段（破坏/燃尽无炭）、cross 模型 + `fire` 纹理。
- `tools/generate_block_textures.py`：`fire()` 火焰 tile（16×16 RGBA
  透明底；泪滴形 = 白芯/黄/橙/红四层 + 逐行 hash 倾斜 = 静态"闪烁"
  风格，与全项目纹理同款确定性）；TEXTURES 注册。
- `block_atlas.json`：`"fire": [10, 0]` = **首个空位**（66→67/128）→
  无 UV 移位 → **无重锚**。

**核心**：
- `src/core/save.{hpp,cpp}`：`SavedFire {pos, remainingSeconds}` +
  `SaveGame::fires` + `kSaveFormat = 10`；序列化（位置序、恒写含空）；
  解析（format ≥10 要求 `fires` 键 = 炉子键纪律；format <10 携带
  `fires` 拒绝；remaining > 0 硬校验；与块编辑**双向**交叉校验；
  topLevel 键集加 "fires"）。
- `src/core/game.{hpp,cpp}`：
  - init：`m_fireId = idForName("fire")` + **交叉校验**（anyFlamable
    && !m_fireId.valid() → MC_FAIL 点名）+ `fire.lifetime_seconds`
    （默认 4.0，≤0 → MC_FAIL）。
  - `setBlock` 收口：fire 块 → `m_fires.emplace(pos, 满寿命)`（emplace
    保既有倒计时，同块重设不重置）；离 fire → erase。
  - 点燃分支：`setBlock(air)` → `setBlock(m_fireId)`（flamable 目标
    存在 ⇒ init 保证 m_fireId 有效，注释钉死）。
  - 流体段：`FluidSim::tick` 的 changed 列表中"现在是 fire 的格"补
    注册满寿命（熔岩 pass 走 world.setBlock 绕过收口）。
  - `updateFires(dt)`（流体之后、meshing 之前 = 主线程写窗口）：
    map 位置序迭代 → 倒计时（real-dt，生存计时器类）≤0 收集 + 6-邻
    水检查（`getFluidOfKind`，越界读 0）→ 迭代后统一 setBlock(air) +
    `Lighting::updateLocal`（迭代中不结构性改 map）。
  - 存档两侧：序列化 `m_fires → save.fires`；applySave `save.fires →
    m_fires` + 迁移（块编辑里有 fire 块但无条目 = 手编旧文件 → 补满
    寿命条目，空炉子状态同款模式）。
- `src/world/fluid.cpp`：`burnFlamableNeighbors(world, blocks, burnTo)`
  加燃烧目标参数；`FluidSim::tick` 解析 `idForName("fire")`（valid ?
  fire : air，注释：init 交叉校验保证游戏内 registry 必有 fire）。

**测试（+11，1034/1034）**：
- `test_block`：`Phase64FireBlockContent`——真实 assets 钉死 fire 块
  全字段（cross/光 15/瞬破/无 drops/非 flamable/纹理名）。
- `test_fluid`：`BurnsToFireWhenRegistryHasFire`——带 fire 块的
  registry 烧成 fire（非 air）+ 二次 tick 不复烧（fire 非 flamable）。
  既有 air 语义测试不动（无 fire 块的 fixture = 回落 air）。
- `test_save`（+9）：fires 往返（含 format 10 断言）/ 空 fires 往返 /
  format 9 无 fires 加载 / format 9 手编 fire 块迁移空 / format 9 带
  fires 键拒绝 / 双向不匹配拒绝 ×2 / 重复条目拒绝 / remaining ≤0
  拒绝 / 缺 remaining 拒绝。
- **既有 save 测试契约演进**：12 处 format 降级测试补
  `withoutKey(root, "fires")`（新序列化恒写 fires 键）+ 2 处 format
  断言 9→10 + Fixture `makeBlocks()` 注册 fire 块（块编辑引用需
  注册）。

### 关键决策

- **范围 = fire 块（光源 + 燃尽 + 浇灭 + 存档续烧）**：建议原文
  "火视觉（火焰块/粒子）"——块是 MC 火语义的核心（发光、暂存、水灭）；
  粒子（燃烧时刻特效）与蔓延（火引燃相邻可燃）是独立机制，文档化
  偏离（KNOWN_ISSUES 火 v1 条）。
- **燃烧目标 = 具名引用 "fire"（核心词汇表条目）**：per-block
  `burns_to` 字段 = 10 个文件各加一行 + 新可燃块易忘 → 静默 air 烧；
  单点具名引用 + init 交叉校验（flamable 存在 ⇒ fire 必须存在）更
  严。先例：P47 流体名词汇表（"water" only）。
- **寿命 = real-dt（生存计时器类）而非固定帧**：燃尽时刻墙钟稳定
  （MC 语义）；代价 = E2E 终态需 fps 无关设计（MOV config + 足够长
  帧数，见下）。
- **熔岩 pass 的 fire 注册走 changed 列表**（不改 FluidSim 签名）：
  该 pass 走 world.setBlock（世界模块，不经 Game 收口）；changed 格
  通常极少（稳态世界 = 空），逐格 `getBlock == m_fireId` 检查廉价且
  无歧义。
- **结构性免疫论证（第 37 次，无重锚）**：fire 块**永不生成**
  （worldgen 未动）+ fresh 世界零熔岩（P54）= 燃烧 pass 惰性 +
  默认背包无点燃器（P60）+ atlas 空位 [10,0] 无 UV 移位 → 无输入零
  活动。

### E2E 证据链

- **S1 第 37 次结构性免疫**（**无重锚**）：fresh 60f ×2 + == base18
  字节一致 + skygate + 红心哨兵 0。
- **S2 点燃器 → fire 活体终态**（MOV lifetime 300 = P57 mov-config
  模式）：wood @ (22,46,-5)（P52 验证几何，瞄准锚定存档零鼠标），
  单次右键 → 终态 blocks = {(22,46,-5): fire}、fires 恰 1 条且
  remaining ∈ (250, 300]（420f = 7..18.3 s，点燃 2.9 s）、entities
  []（无掉落）、玩家 20 HP/位置/瞄准不变；×2 savenorm（savenorm 新增
  剥 fires remaining = real-dt 字段）字节一致。
- **S3 燃尽**（无输入，默认 lifetime 4.0，600f）：场景 fire @
  (22,46,-5) remaining 4.0 → 600f = 10..26.1 s > 4.0 = **fps 无关**
  终态 air：diff = {}、fires = []、entities []；×2 RAW 字节一致。
- **S4 水浇灭**（无输入，MOV 300，240f）：**高架水**几何——石台
  (22,45,-5) 上 L8 水 (22,46,-5) + 四侧 y46 石封边 + 火在正上方
  (22,47,-5)；300 s 寿命排除燃尽（240f ≤ 10.4 s）= 水是唯一灭因；
  终态 diff = 恰 5 石、fluids = {(22,46,-5): L8 water}（水未漏）、
  fires = []；×2 RAW 字节一致。**首轮失败两坑**（PITS 收录）：①
  裸 L8 水源在高原上**横向漫灌 29 格**（水 flow 7 vs 熔岩 1——"L8 源
  静态"是熔岩专属规则）；② (23,45,-5) 是**生成装饰格**（非 air），
  烧掉后 diff 出 "air" 条目（断言按"生成 air 格无条目"写 = 误）。
- **S5 夜间渲染 + 光**（无输入，午夜场景，**600f** ×2 各）：场景 A =
  fire @ (22,46,-5)（P52 几何，MOV 300 + 场景 remaining 300），B =
  同场景无火；A×2 / B×2 各自字节一致、A ≠ B（火被渲染）、A 有火焰色
  像素（r>180, g>60, b<140 >20——月亮 (0.85,0.88,1.0) 与星星蓝白
  过不了 b<140 滤）、B <5；存档侧 A = fire 活体 / B = 空。**首轮
  失败坑**（PITS 收录）：60f `--save` 跑可视 chunk 流送未竟（worker
  竞态）= 一跑下半屏整片天空（52 万 px diff）——升 600f（S3 已证终
  态确定）。另见 8px WM 黑条（环境性，pair 级重试吸收）。
- **S2ctl 无输入对照**（240f ×2 RAW）：wood 保留、fires = []、
  零实体。
- **S6 init 失败 ×2**：① 删 `fire.json`（wheat 仍 flamable）→
  "the flamable blocks require the 'fire' block"（交叉校验点名）；
  ② config `fire.lifetime_seconds: 0` → "config
  fire.lifetime_seconds must be > 0"。

### 已知问题（转 KNOWN_ISSUES）

- **火 v1 范围**（新条目）：无粒子 / 无蔓延（火不引燃相邻可燃块）/
  fire 可放置（临时营火）/ 光恒定 15 无闪烁 / 无接触伤害。
- 燧石与钢条目更新："无火视觉"部分由本 phase 解决（条目尾部标注
  [Phase 64]）。
- 伤害来源条目修正（自 P54/P57 起陈旧）：补熔岩灼烧/燃烧效果，注明
  fire 块无接触伤害。

### PITS（+2，现 182 条）

1. **"L8 源静态"是熔岩专属规则，水 L8 会横向漫灌（P64）**——水场景
   静止一摊水 = 高架水（石台 L8 + 四侧石封边）；流体蔓延规则 = 只
   水平 + 下落、绝不向上、任何块（含非 solid）挡流。
2. **`--save` 短帧截图跑 = 可视 chunk 流送竞态（P64）**——场景截图
   跑用 600f；新鲜世界 60f 基线不受影响（出生演示区同步生成）。

### 变更文件

内容：`assets/data/blocks/fire.json`（新）、`assets/textures/blocks/
fire.png`（新）、`assets/data/textures/block_atlas.json`（+fire [10,0]）、
`tools/generate_block_textures.py`（fire tile）。
核心：`src/core/save.{hpp,cpp}`（SavedFire + format 10）、
`src/core/game.{hpp,cpp}`（m_fires/m_fireId/m_fireLifetime/updateFires
+ init 校验 + 收口 + 点燃分支 + 存档两侧）、`src/world/fluid.cpp`
（燃烧目标参数）。
测试：`tests/test_block.cpp`（+1）、`tests/test_fluid.cpp`（+1）、
`tests/test_save.cpp`（+9，含 12 处降级测试演进）。
文档：本文件 + ROADMAP/KNOWN_ISSUES/PITS/GAMEPLAY/MECHANICS/
CONTENT_GUIDE/AGENTS。
