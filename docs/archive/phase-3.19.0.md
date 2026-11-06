# Phase v3.19.0 — V3-14a 爆炸机制（门 209 全 PASS，2026-11-06）

矩阵 ⬜ 项小→大推进（V3-14 下界 mob 深化相位 A/4）。爆炸机制族
核心 = **伤害衰减 / 破块 / 点火**（BACKLOG 定案范围），ghast（V3-14b）
前置——机制先行，首个真触发 = ghast 火球（调同一
`Game::explodeAt`，sourceName 命名死因）。

## 范围与定案

- **纯核心 `world/explosion`**（fire_spread 纯函数模型）：
  - `scanExplosion(world, blocks, center, radius)`：包围盒 (2r+1)³
    确定 x/y/z 序扫描（格中心 +0.5 距离约定），格 in-range 当
    dist ≤ radius。`broken` = solid ∧ `destroyable` ∧
    **`blast_resistance` < 局域强度**（radius − dist：中心满强度、
    边界 0——边界格 local 0 = 任何 br ≥ 0 的块存活，门的
    (3,56,0) 界上存活 pin）；`ignited` = 未破 ∧ `flamable`（break
    分支优先——破了的格无物可燃）。流体不触碰（块层 only，记档）。
  - `explosionDamage(dist, radius, base)`：`ceil((1 − dist/radius) ×
    base)` 整读；dist ≥ radius 或参数非正 = 0（blast 硬界）。
    kExplosionDefaultRadius 3.0（TNT 级）/ kExplosionBaseDamage
    12.0（20 HP 的 60%，中心读）。
- **块属性 `blast_resistance`**（hard rule 3 = 数据非代码）：strict
  非负数，absent = 0.0（任意 blast 可破，向后兼容默认）；**obsidian
  = 3**（radius-3 blast 的局域强度全域 ≤ 3 → `3 < local` 恒假 =
  存活——传送门黑曜石数据免疫，bedrock 的 destroyable=false 自动
  免疫）。负值/非数 = init 点名（S6 实机 rc 1）。
- **接线 `Game::explodeAt`**（game_frame，--explode 与 ghast 的共享
  seam）：破碎 → `setBlock(air)` 收口（journal/relight/bookkeeping）
  + 碎片 `spawnBreak`（共享 m_particleBreakCounter 黄金比种子契约，
  扫描序 = 确定性序）；点燃 → `setBlock(fireId)` + updateLocal（收口
  登记火寿命）；中心 `spawnFire` 余烬闪；`boom` 音（生成器 0xB007
  定种双低通 LCG + 55→28 Hz 滑频，0.9 s，peak 32574 pin）；衰减
  伤害 = 玩家 `damage(dmg, false, {Explosion, sourceName})`（创造门
  自动免疫）+ mobs `MobEntity::damage(dmg)`（无死因行 = 玩家独享）。
  **无击退**（项目无玩家/mob 击退面——V3-15 击退抗性弃做同款，记档）。
- **死因行**（V3-13 族 +1）：name 空 = 「死于爆炸」；非空 = 「被
  <name> 的爆炸杀死」（ghast 预留）。**字体 +2 字形 爆 U+7206 /
  炸 U+723D**（428→430：扩展区 re-cell +2——0x71E7 后所有扩展字形
  移位；top-512 红线位级 + legacy 33 位级 + atlas 256×2112 尺寸不
  变 + headroom cells 525..527 剩 3；test_font 全锚重钉含 PNG FNV
  重钉）。
- **触发 = `--explode x y z [r]` CLI seam**（--weather 孪生）：帧 0
  应用（runGameplayLoop 首帧前——整场回放可见，同 run 恒等）；严格
  有限数解析（短参/非数/负 radius 点名）；r 缺省 3；**互斥** = 管理
  旗 / demo-author / demo-replay（replay 的输入 = demo 的，seam 无
  demo 形——点名拒）。**游戏内暂无消费方 = V3-14a 相位设计**（ghast
  前置机制族，记档）。
- **零 format 31 不变**（爆炸 = 世界编辑，journal 既有）/ **sim 不
  bump**（无输入路径全冷 + seam 无 demo 形——canary 4 demo EXACT 实
  证）/ 零 config 键。

## 实现（代码 12 文件 + 数据 2 + 测试 3）

- `src/world/explosion.hpp/cpp`（新）：kExplosionDefaultRadius /
  kExplosionBaseDamage + explosionDamage + ExplosionScan +
  scanExplosion + ExplosionSpec。
- `src/block/block.hpp/cpp`（registry 解析 + 校验）+
  `assets/data/blocks/obsidian.json`（blast_resistance 3）。
- `src/player/player.hpp/cpp`：DamageSource::Explosion + 死因 2 行。
- `src/ui/font.hpp` + `tools/generate_font.py`（CJK_CHARSET +2 +
  自嵌 bitmap）+ `assets/textures/font.png`（re-cell，md5 重钉）。
- `tools/generate_sounds.py` + `assets/sounds/boom.wav`（0xB007 定种）。
- `src/core/game.hpp`（GameArgs.explode + explodeAt 声明）、
  `game_frame.cpp`（explodeAt 实现）、`game_init.cpp`（帧 0 应用）、
  `main.cpp`（--explode 解析 + 三重互斥 + help）。
- 测试：`tests/test_explosion.cpp`（新 ×5）+ `tests/test_block.cpp`
  ×4 + `tests/test_sound.cpp`（boom pin + 目录计数 63→64 ×2 处）+
  `tests/test_player.cpp`（死因 2 断言）+ `tests/test_font.cpp`（全
  锚重钉）。
- 版本三件套 3.19.0。

## 门 209（全 PASS）

- **S1 ≡ base19 逐字节 ×2 + savenorm EXACT**（**结构免疫第 89
  次**——无 seam 无 blast：scan 从不跑、boom 不入帧态、obsidian 数
  据仅 init 解析、字体扩展区 = 无 CJK 渲染位级）。
- **S2 密封石室双臂**（fresh 60f 去火 + 14×14×7 石室编辑填充 blast
  全域 + 玩家 2×2×3 空气口袋——**森林群系到处可燃，石室封死火域**
  = PITS 401①）：
  - **a1**（blast (0.5,56.5,0.5) r3，帧 0）：**health 18**（dist
    2.5 → ceil(12×1/6) = 2 衰减整读实证）+ (1,56,0)/(2,56,0) 破
    【**journal 回生成态 = 条目消失**——破回 air（生成态）的编辑从
    diff journal 蒸发，断言 = blocks.get() None + 界外对照，PITS
    401②】+ **(3,56,0) 恰半径界上存活**（local 强度 0 = 硬界
    pin）+ (5,56,5) 房外石墙残留对照 + **entities = 0**（无掉落）+
    pos 稳定（无击退语义 = 玩家原位）。
  - **a2**（石室 + obsidian 柱 (2,56,0)/(2,57,0)）：**obsidian 双
    存活** vs 邻格 (1,56,0) 破 = **blast_resistance 数据属性对
    照实证**（br 3 打败 local 1.0/2.0）；余同 a1。
  - 双臂：截图 ×2 RAW + **savenorm EXACT ×2**（RAW = fires[] real-dt
    域：地下生成火簇 (23,35,28) 系 4 条回落 PITS 401③——savenorm 剥
    remaining 后世界恒等；火在 y35 地下 = 截图 RAW 不受扰）。
- **S2b canary 4 demo** ×2 RAW + savenorm EXACT（**sim 不 bump 实
  证**；--explode 无 demo 形 = 回放面零接触）。
- **S6 点名**：init 负 blast_resistance → **rc 1**「block file
  'data/blocks/_g209bad.json': 'blast_resistance' must be >= 0, got
  -1.000000」（temp 资产，门后删除）+ 单元 BlastResistance ×4
  （absent 0 / roundtrip / 负值 / 非数）。
- **S3 全套 2821/2821**（+10：ExplosionTest
  DamageFalloffCurve/ScanBreaksBlastableKeepsResistant/
  ScanIgnitesFlamableAndBreakWins/ScanIsDeterministicAndOrderStable/
  ScanNoOpOnEmptyBlast + BlastResistance ×4 + SoundExplosionTest.
  BoomWavShipsAndParses；死因 2 断言并入 DeathCauseLineFontSafe），
  `-Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = b71cb5e v3.18.0（git archive）+
  g139 vs 新 = 工作树 + g139（banner 实证 v3.18.0 / v3.19.0），900f
  无输入独占——PERF-WIN 三窗 upd med 0.393/0.550/0.593 vs 0.571/0.549/0.588 + render 0.494/0.661/0.675 vs 0.651/0.678/0.675 + swap med 15.571/15.269/15.215 vs 15.212/15.282/15.249（旧侧 win1-3 swap max ~32 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3-2/1**【win2 ±1 = streaming 边界 real-dt 域，v3.14.0 先例】+ **终档 savenorm EXACT**（零 format = 最强形——爆炸全冷路径结构性零成本）

## 门内捕获（PITS 401）

1. **森林群系 = 到处可燃**——blast 必点火 → real-dt 火寿命域（RAW
   失效）；密封石室（全 stone 填充 blast 全域 + 玩家口袋）= 无火
   确定性域；场景手术漏移 fire 日志条目 = init 点名「fire block has
   no fires entry」（PITS 398① 的爆炸侧复现）。
2. **blast 破回生成态 = journal 条目消失**（blocks journal = 与生成
   世界的 DIFF，非事件日志）——断言破块 = `blocks.get(cell) ==
   None` + 界外/房外对照格 + (3,56,0) 界上存活 pin，勿断 air 编辑
   存在。
3. **地下生成火簇回落**——场景去火后载档世界生成重放 3 团火 + 1
   蔓延 = fires[] 恒 4 条 real-dt；断言域 = savenorm + 截图 RAW
   （火 y35 地下不可见）。

## 记档偏离

- 无击退（项目无玩家/mob 击退面）。
- 点火 100% 确定（MC creeper 25% roll 未做）。
- 破碎无掉落（MC 孪生）/ 流体不触碰（块层 only）。
- 游戏内无触发方（V3-14b ghast = 首个真触发）——--explode seam 门
  面。

## 变更文件（显式路径，32）

src/world/explosion.hpp src/world/explosion.cpp
src/block/block.hpp src/block/block_registry.cpp
src/player/player.hpp src/player/player.cpp src/ui/font.hpp
src/core/game.hpp src/core/game_frame.cpp src/core/game_init.cpp
src/core/main.cpp assets/data/blocks/obsidian.json
assets/textures/font.png assets/sounds/boom.wav
tools/generate_font.py tools/generate_sounds.py CMakeLists.txt
tests/test_explosion.cpp tests/test_block.cpp tests/test_sound.cpp
tests/test_player.cpp tests/test_font.cpp tests/test_debug_overlay.cpp
AGENTS.md README.md docs/CHANGELOG.md docs/BACKLOG.md docs/GAMEPLAY.md
docs/MECHANICS.md docs/PITS.md docs/KNOWN_ISSUES.md
docs/archive/phase-3.19.0.md
