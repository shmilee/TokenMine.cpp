# Phase 22 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 22 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

**提交**：`4a3cfc2`（16 文件 +248/−26）。**测试**：603/603（+3）。
**基线门**：`/tmp/opencode/base18.png` ×2 逐字节通过（atlas +2 tile 对既有渲染零影响——
新 tile 只占空位，既有 tile 坐标零移动）。

### 设计
- **passive mob = 纯数据**：`detect_radius`/`attack_range`/`attack_damage` 三个字段
  校验从 `> 0` 放宽为 `>= 0`（`mob_registry.cpp` 新增 `readNonNegativeNumber`，
  错误文案 "must be >= 0"）。0 值语义：`distH <= 0` 永假（除非玩家与 mob 底心
  精确重合——measure zero，且此时 0 伤害 hit 回调被 `Player::damage` 的
  `amount <= 0` 守卫吞掉，无红闪无音效）→ AI 状态机对 passive mob 只走 Wander，
  **零 AI 代码改动、零特判**。health/speed/attack_cooldown/wander_interval 仍须 >0
  （passive 数据的 cooldown 是"从未使用但仍须合法"的完整性字段）。
- **cow**（`assets/data/mobs/cow.json`）：HP 10、speed 0.5（游走 = 0.5×0.5 = 0.25 b/s）、
  detect/range/damage 全 0、attack_cooldown 1.0、wander_interval 3.0、
  `spawn_max_light` 15、drops `leather`×1。模型 AABB 1.2×1.5：身箱 0.8×0.7×1.1
  中心 [0,0.9,0]（y 0.55..1.25）+ 头箱 0.45³ 中心 [0,1.2,-0.35]（y 0.975..1.425，
  前探 0.025 出身体前脸——头在前 −Z 脸）。**首个比 zombie（0.6 宽）宽的模型**
  （1.2 宽）——生成格扫描/玩家重叠/间距规则全部用模型 AABB 数据，无需改动。
  注意 v1 cow 无腿箱（zombie 同款头+身两箱约定）：身箱底距脚 0.55，AABB 下部
  是空碰撞区——视觉"悬浮"感由身箱高度覆盖（E2E 截图确认自然）。
- **leather**（`assets/data/items/leather.json`）：material、max_stack 64、
  图标 `leather.png`（tan 皮革片：不规则板 + 深 rim + 2 折痕线，
  `tools/generate_item_icons.py` 新增 `leather()`，salt 41/43/45）。
  当前无合成用途（纯掉落收集物，KNOWN_ISSUES 已记）。
- **cow 纹理**：`tools/generate_mob_textures.py` 新增 `cow_fur_tile(salt, base,
  eyes)`——棕底 (122,88,62) + 白斑（`h2(x,y,salt+7) % 100 < 26` → (228,222,210)）
  + 噪声 ±10；头加深色眼 (40,28,20) 行 5-6（zombie 同款眼位）。salt
  0xC0F1(身)/0xC0F2(头)。重跑后既有 zombie/物品输出**逐字节不变**。
- **atlas**（`assets/data/textures/block_atlas.json`）：`cow_body` [1,5]、
  `cow_head` [2,5]（row 5 空位；4×8=32 格，21→23 用，9 空）。
- **spawning 名单**：`mob_spawning.json` `mobs: ["zombie", "cow"]`。
  类型抽取 = `splitmix64(世界种子 ⊕ 0x9E3779B97F4A7C15 × frame) % 名单长度`，
  **发生在光门之前**——抽中 zombie 而白天无合法格 = 该次尝试作废
  （帧 60/90/120/150/180/210 六次尝试中，本 seed 仅帧 90 抽中 cow 并成功
  spawn 于 (0,53,-6)；其余 5 次抽中 zombie 全部作废）。v1 接受：
  节奏固定、类型 50/50，某类型不可生成时段其生成率减半（KNOWN_ISSUES 已记）。
  `attemptMobSpawn` 注释已更新（原 "v1 data lists a single mob" 过时）。

### 注册顺序事实（排查中确认，非 bug）
- `loadDirectory` 按**字母序**加载 → **cow 先注册 = MobID 0**，zombie = 1。
  `m_mobs` 是 vector、`at(id)` 下标直取、`mobMesh(typeId())` 按 id 建缓存——
  一切自洽。排查时曾见 "mob cow tid 0" 误以为是 ID 冲突，实为字母序。
- 写测试/日志时不要假设名单顺序 = 注册顺序 = ID 顺序。

### 测试（+3，tests/test_mob.cpp）
- `MobRegistryTest.ParsesPassiveMob`：cow 式完整合成定义（detect/range/damage 0、
  spawn_max_light 15、drops leather、双箱 1.2×1.5 模型）解析 + 字段断言。
- `MobRegistryTest.RejectsNegativePassiveFields`：7 用例表——detect −1 / range −0.5 /
  damage −1 / health 0 / speed 0 / cooldown 0 / wander 0 全部拒绝（0 对前 3 个
  合法、对后 4 个非法）。
- `MobAiTest.PassiveMobNeverChasesOrAttacks`：kZombieDef 改 3 个 0 值注册；
  玩家距 0.5（hostile 1.6 射程内）；300 帧（5s，跨 ~2 次 wander 决策）
  每帧断言 `state() == Wander` + hit 回调 0 次 + 总伤害 0。

### E2E（驱动 /tmp/opencode/p22_e2e.zsh，p205 驱动复制改日志名）
- **base 60f ×2 vs base18：逐字节通过**（60f 内无 spawn 尝试 + 新 atlas tile
  不移动既有坐标 → 零影响）。
- **白天 240f（config_xep.json，start 0.25 正午）mobs-on ×2 逐字节一致**；
  **mobs-off（config_xep_nomobs.json）×2 逐字节一致**。
- **diff on vs off = 7578 px**，bbox x[631,743] y[244,326]（屏幕中央偏左单一
  紧凑轮廓 = 1 头 cow，帧 90 spawn 于 (0,53,-6) 正前方 ~6 格后缓慢游走）；
  分类：cow 棕 5806（77%）+ 白斑 75（1%）+ other 1697（22% = 深色毛/边缘
  抗锯齿，判据见下）+ **zombie 绿 0**（白天敌对压制保持）；UI 带（y≥660）0、
  心带 0。bbox 高 83px < 理论全高 ~200px = 前景草脊遮挡身箱下缘 + 身箱本就
  距脚 0.55 起——正常，截图目视确认 cow（棕白斑+眼）站在草地上。
- 像素判据：cow 棕 = R>G>B ∧ R−B≥12 ∧ R<175；cow 白 = R>195 ∧ G>185 ∧ B>170
  ∧ 色差<30；zombie 绿 = G≥R+25（沿用）。

### 本会话踩坑（E2E 排查，重要）
1. **驱动相对路径陷阱**：`p22_e2e.zsh` 内部 `cd /home/shmilee/project/TokenMine.cpp`，
   调用方传**相对路径** out 时截图写进**仓库根目录**（/tmp/opencode 里剩旧文件
   → 新旧图比对得出错误结论，且仓库目录被污染）。修法：调用一律传绝对路径；
   跑完 `git status` 核对无 stray 文件。
2. **对照运行忘传配置**：`p205/p22_e2e.zsh` 第 3 参默认 `config_xep.json`
   （mobs-on）——跑 day_nomobs 必须显式传 `config_xep_nomobs.json`，否则
   "对照"与实验同配置、diff 恒 0（本会话第一轮 diff=0 的真因）。
3. **PNG 字节大小 ≠ 像素差异**：文件大小不同但解码后相同（旧文件混淆）——
   diff 一律走 `magick txt:`/raw RGB 比对，不靠 `cmp` 截图文件本身。
4. **排查手段有效**：临时 `fprintf(stderr, ...)` DBG（类型抽取结果/cell 结果/
   实体每 10 帧 pos·hp·dead/drawMobs 每 30 帧 tid·verts）+ throwaway probe
   二进制（probe_cow.cpp：同种子生成 chunk 直接 dump 方块列地图）——
   两轮定位到"cow 活着且在走但被误判不可见"，最终发现是 E2E 调用问题。
   排查后 **DBG 代码已全部移除**（git diff 核对 game.cpp 只剩注释更新）。

### 变更文件
- `src/entity/mob_registry.cpp`（`readNonNegativeNumber` + 3 字段改用）
- `src/entity/mob_registry.hpp`（示例 JSON 注释：3 字段 >= 0 语义）
- `src/entity/mob_entity.hpp`（AI 头注释：passive mob 说明）
- `src/core/game.cpp`（仅 attemptMobSpawn 注释更新：名单多类型 + 光门作废语义）
- `assets/data/mobs/cow.json`（新）、`assets/data/items/leather.json`（新）
- `assets/data/mob_spawning.json`（mobs +cow）
- `assets/data/textures/block_atlas.json`（+cow_body/cow_head 2 格）
- `assets/textures/mobs/cow_body.png`/`cow_head.png`（新）、
  `assets/textures/items/leather.png`（新）
- `tools/generate_mob_textures.py`（`cow_fur_tile` + 2 输出）
- `tools/generate_item_icons.py`（`leather()` + 映射）
- `tests/test_mob.cpp`（+3）
- README（mob 章节：passive 数据/光门 cow 行/AI passive 段/死亡掉落 cow/
  渲染 cow 模型/物品 24 种/"新增一个 Mob" 校验行 + 结尾 v1 数据清单 + roadmap
  P22 行）、KNOWN_ISSUES（Mob 节：光门条目补 cow + 新增 3 条——光门作废尝试/
  passive 无躲避/cow 掉落固定 1）、SESSION_CONTEXT（本文件）。
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length 600），
  与 P22 无关。
