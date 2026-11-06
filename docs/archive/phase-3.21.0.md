# Phase v3.21.0 — V3-14c piglin 交易（门 211 全 PASS，2026-11-06）

## 范围与定案

**piglin barter = 物物交换**（neutral 激怒联动的既有类上叠加交易面）：
玩家右键 piglin 且手持 `gold_ingot` → 消耗 1 金锭，piglin 掷出
一个加权表物品（地面 ItemEntity，磁力吸回），**不激怒**。

- **数据**（piglin.json）：`barter_input: "gold_ingot"` +
  `barter_outputs` 12 项加权表（MC 比例项目近似：gravel 80×8-16 /
  nether_brick 40 / obsidian 40 / string 20×3-9 / leather 20×2-4 /
  soul_sand 10×2-8 / quartz 10×5-12 / ghast_membrane 10 /
  hoglin_meat 10 / water_bottle 6 / golden_apple 2 / netherite_scrap
  1 头奖；Σ=249）。**单输入**（现行 MC 仅 gold ingot，无三档表 =
  任务假设证伪弃做）；数量区间 [1,64]（drops 的 0 概率式拒收——
  付金无货是笔误不是机制）。
- **机制**：解析 = milk 配对纪律（孤字段拒）+ wool_weights 字段
  纪律（未知字段/重名/weight [1,10000] 拒）+ drops 数量两形式
  （count 与 range 互斥）；init 交叉检 = 纯函数 `checkBarter`
  （checkWoolDrops 范式：input 存在 + 每 output 存在 + countMax ≤
  maxStack，首个失败 mob 点名）；roll = `barterItem()` 无状态
  （rollBarterOutput 单 uniform01 累计权重 + rollDropCount 单
  nextMix，固定数量零流消耗）；交互 = `tryBarter`（tryMilk 范式
  挂点 tryDyeMob 后：targetMob + 数据谓词 barterInput == 手持名；
  错手持 = 吞点击【failed-milk 规则】；非 barter mob = 穿透位级）。
- **不激怒 = 结构性质**：激怒单收口在 damage()，barter 路径零
  damage 调用（挤奶先例的同款证明，无需豁免字段）。
- **音效**：barter.wav（0x6A11 定种 0.16s 双音 chime 880→1318.5 Hz
  + 2.01× 微光 + 噪边 tick，SFX class；66→67 幂等，既有 66 件
  字节恒等 = git status 仅新增 1 文件实证）。
- 零 format 31 不变 / **sim 不 bump**（canary 4 EXACT）/ 零
  config / 零新纹理（物品图标全既有，atlas 166 不动）。

## 实现（代码 6 + 数据 2 + 工具 1 + 测试 3 + CMakeLists 1）

- `src/entity/mob_registry.{hpp,cpp}`：BarterOutput + 字段 +
  rollBarterOutput 纯函数 + 解析纪律族。
- `src/entity/mob_entity.{hpp,cpp}`：barterItem()（无状态 roll，
  非 barter mob 零流消耗）。
- `src/entity/barter_check.{hpp,cpp}`：checkBarter 纯函数
  （checkWoolDrops 孪生模块）。
- `src/core/game_interaction.cpp` + `game.hpp`：tryBarter +
  dispatch 挂点。
- `src/core/game_init.cpp`：include + MC_TRY(checkBarter)。
- assets：piglin.json（+barter 面）/ barter.wav。
- tools/generate_sounds.py：barter()。
- tests：test_barter.cpp（checkBarter ×6）+ test_mob.cpp（解析
  ×4 / roll 纯函数 ×1 / MobBarterTest ×2 / PiglinBarterDataPinned）
  + test_sound.cpp（census 67 ×2 + BarterWav pin）+ FNV 重钉。

## 门 211（全 PASS）

- **S1 结构免疫第 92 次**：60f 无输入 ×2 ≡ **base19 字节恒等** +
  savenorm EXACT（barter 活在 wasPressed(Right) 下，无输入全冷）。
- **S2 交易臂 = authored demo**（`--demo-author` timeline：world
  = save 槽 + f30 LMB 捕获 + f120 RMB + f600 零 delta 延展）：
  猪灵独占池变体 run 目录（pool piglin 100 / max_mobs 1）+ 场景
  v3（nether 高原，player (40.5,53,38.0) yaw 0.1271 pitch −0.6358
  【临时插桩 HIT/MISS 校准准星——猪灵 f120 游走位 (40.029,51,
  34.331)，眼-体心距 5.0 = kReach 边界内】+ **1×3 视线沟**
  （z34-36 挖 2 深：射线在 z36.09 才降到台地顶面以下——1×1 洞
  被 rim 遮挡不可见 PITS 403④）+ **12 拒生沟**（scan 序先于
  目标 cell (40,34) 的全部环内格 3 深挖 = 沟底 top 50 出
  kSpawnFloorDelta 带）：f60 猪灵精准生入沟 (40.5,51,34.5) →
  f120 RMB = **`bartered with 'piglin' (gold_ingot -> obsidian x1)`**
  ×2 逐字恒等（帧锁定 roll）+ 终帧截图 **RAW 恒等** + 热栏 crop
  目检（gold 3→2 已消耗 + 心满 HP 20 不激怒 + 猪灵沟内平静）。
  **口径注**：demo 引用存档槽 = 观察红线不回写终档（game_init
  saveSlot.reset PITS 403①）——终态权威 = 截图 RAW + DEBUG 行。
- **S2 无输入臂**（同场景 1800f ×2）：截图 RAW + savenorm EXACT +
  6/6（hp 20 平静 / deathCount 0 / 视线沟 6 air / 拒生沟 36 air /
  gold 3 未动 / 玩家 nether 原位）。
- **S6 点名**：temp 假 output → init rc 1 `barter_check.cpp: mob
  'piglin' barter output is an unknown item 'bogus_item'`（直证
  checkBarter）；ghast_membrane 移走 → drop 检先点名（两物品缺失
  面均 rc 1 点名）。
- **S2b canary 4 demo** ×2 RAW + savenorm EXACT（sim 不 bump 实证；
  headless 必 --frames 700 > total = PITS 402⑤ 纪律）。
- **S3 全套 2847/2847**（+14：BarterCheck ×6 / 解析 ×4 / roll ×1 /
  MobBarter ×2 / SoundBarter ×1），`-Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = 113ed11 v3.20.0 + g139 vs 新 =
  工作树 + g139（单次插桩 + 300f 冒烟验 PERF-WIN），900f 无输入
  tools/perf_gate_config.json 独占——banner 实证 v3.20.0/v3.21.0 +
  PERF-WIN 三窗 upd med 0.561/0.584/0.571 vs 0.552/0.567/0.567 +
  render med 0.638/0.692/0.660 vs 0.610/0.670/0.660 + swap med
  15.30/15.25/15.26 vs 15.33/15.24/15.24（同档）+ fluid·fires·
  spread·ents·stream 同档 + **changesApplied 9/3/1 vs 9/2/1**
  【win2 ±1 = streaming 边界 real-dt 域，v3.14.0/门 209 先例】+
  **终档 savenorm EXACT**（零 format = 最强形——barter 全冷路径
  结构性零成本）。

## 门内捕获（PITS 403）

1. **demo 观察红线 = 无退出写**：引用存档槽的 demo 永不回写该槽
   （saveSlot.reset）——CLI「drop --save or match it」的 match =
   红线不写，非写档；终态权威 = 截图 ×2 RAW + DEBUG 动作行。
2. **run 目录 stale assets**：池变体/副本树每次仓内资产改动后
   必须重拷——两连探跑静默用旧 piglin.json（HIT + 无 barter +
   gold 未动 = 数据缺而非瞄准错）；判别 = 拷后 grep 数据字段。
3. **kReach 边界瞄准**：眼-猪灵 feet 距 5.54 > kReach 5.0，瞄准
   必须按体心（5.0 恰界内）；临时插桩 HIT/MISS + 近 mob 位打印 =
   校准工具（PITS 383③ 定界后移除重建纪律）。
4. **生成环视线几何**：scan 序（dz 降 dx 升）钉目标 cell 须拒
   全部 scan-序先行格（12 拒生沟）；1×1 洞不可见（rim 遮挡——
   射线 z36.09 才降过台地顶）→ 1×3 视线沟（超环 cell 自由挖）。
5. **ambient 每帧耗流**：ground ambient 每帧 roll uniform01 = 流
   态是点击帧的函数——xdotool ±帧抖动破 roll 恒等；断言 roll 的
   输入臂必用 authored demo（帧锁定），xdotool 只用于结果不变量
   型断言。

## 记档偏离

- **即时交易**（MC 6s 检视周期 → 即时，挤奶先例）；**单输入**
  gold_ingot（现行 MC 无三档表 = 任务假设证伪）；表 = MC 比例
  **项目近似**（golden_apple/hoglin_meat/netherite_scrap 项目特色
  项，blackstone/spectral arrow 项目无对应弃）；**产出走地面
  ItemEntity + 磁力**（非直接入包，MC 抛出孪生）；数量 [1,64]
  （drops 0 概率式弃 = 付金无货非机制）。
- barter 不激怒 = 结构性（零 damage 路径），无新字段。

## 变更文件（显式路径，14）

见 git commit。
