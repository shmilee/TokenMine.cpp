# ENT-8 和平/保护环（v1.x 15/33，v1.15.0，门 130，ENT 行 4/6）

v1.x 任务矩阵第十五项。"和平/保护环"原始定义已佚 = 代理定案最小完整
版：**peaceful 难度**（消费 KNOWN_ISSUES Phase-25 条"无 Peaceful 难度
——难度仅 easy/normal/hard"）。保护环本体（敌对生成保护圈 8/12/16
锚世界出生点、被动豁免）= Phase 25 既有；peaceful = 该保护环扩到覆盖
全球（敌对自然生成全拒）。

## 定案

- **difficulty 词汇表 3→4**：`mobs.difficulty` = "easy" | "normal" |
  "hard" | **"peaceful"**（严格解析同款纪律；misspelling 点名）。
- **peaceful 语义（复用 Phase-25 机制，零新代码路径）**：
  `spawnProtectionRadius(Peaceful) = kSpawnProtectionPeaceful(1e6)` =
  保护圈覆盖全球 → 每个敌对生成格必在圈内 → 自然敌对生成全拒；被动
  mob 豁免（protection 0）不受影响 = MC 和平"无敌对 + 被动照生"。
  1e6 保持平方比较在 float32 范围内（named constant，非魔法数）。
- **API 一致性值**：`hostileDamageScale(Peaceful) = 0.0`（无宿主消费，
  读作"敌对零伤"）/ `hostileSpawnRateScale(Peaceful) = 1.0`（cadence
  算术不变）——peaceful 的 cadence = 30（moot：保护先于尝试拒掉）。
- **记档偏离**：① 无"现有敌对消亡"——mobs ephemeral（从不存档）+
  difficulty 只在 init 读取（无运行期切换）→ 读档后世界本无敌对，
  "消亡"退化为"不存在"（MC 同款净效果）② 无 MC peaceful 的自动回血/
  饥饿恢复（生存计时器语义 = 另行立项；本项目 peaceful 的核心 = 无敌
  对）③ 保护锚 = 世界出生点（Phase-25 既有，非 MC 的玩家重生锚）。
- 零 format（19）/零内容/零音效。

## 实现

`src/core/difficulty.hpp`（header-only：enum +Peaceful + name/parse +
3 常量 + 3 switch case + 注释，~30 行）。game.cpp 零改动（保护圈收口
点 findMobSpawnCell 既有）。

## 测试（1802/1802，-Werror 0 警告）

test_difficulty 6 测演进（+peaceful 断言）：ParsesTheFourSpellings
（4 值解析 + name）/ RejectsOtherSpelling（"Peaceful"/"peacful" 拒，
"peaceful" 移出拒列）/ ProtectionRadiusIsMinecrafts（peaceful = 1e6
> hard 16）/ DamageScale（peaceful 0.0）/ SpawnRateScales（peaceful
1.0）/ CadenceFromBase30（peaceful = 30 不变）。

## E2E 门 130

场景 = 复用 s129 已验证平台（x22-33 z-35..0 地板 y44 + air carve，
玩家 (27.5,45,-17) = 平台中点，生成环 8-16 的南北弧在平台上；2400f
跑，无输入）。

**S1 = 第 130 次结构性免疫免重锚**：fresh 60f 四方截图 ×2 387726 B
全字节一致（new v1.15.0 ≡ old v1.14.0 ≡ base18）+ savenorm new×2 /
old×2 EXACT（peaceful = config 值，默认 normal 路径零触碰）。

**S2a peaceful 夜 ×2（判别主体）**：零敌对生成 → **玩家 hp 20 恒安全**
+ ×2 RAW EXACT 342632 B + savenorm EXACT。
**S2b normal 夜 ×2（新二进制）**：敌对生成 + 击杀 idle 玩家 → **hp 0**
+ ×2 RAW EXACT 337109 B。
**S2c normal 夜 ×2（旧 v1.14.0）**：hp 0 + ×2 RAW EXACT + **S2b_new
≡ S2c_old 字节一致**（normal 生成/追逐/击杀全链零回归 = ENT-8 代码
惰性实证）。
**S2d peaceful 昼 ×2（被动豁免实证）**：hp 20 + ×2 RAW EXACT 346925
B + savenorm EXACT + **vs S2e nomobs 昼对照截图 diff 45,148 px**
（被动 mob 成群出现 = 豁免生效，45k px ≫ 2000 阈）。
（冒烟探针先行实证：peaceful 夜 hp 20 vs normal 夜 hp 0 互斥。）

**S3 = 套件 1802/1802**（1.15.0 binary 终跑）。

## 性能 A/B（发布前必做）

tm_v114_perf（v1.14.0 插桩）vs tm_v115_perf（v1.15.0-dev 插桩【同先
例：v114 插桩 fluid.cpp 移植 + step0 game hunks + hunk#6 手工补】），
**PASS = 成本可忽略**（tm_v114_perf v1.14.0 插桩 vs tm_v115_perf
v1.15.0-dev 插桩【同先例：v114 插桩 fluid.cpp 移植 + step0 game hunks
+ hunk#6 手工补】，同 seed（perf_ab17 剥 fluids → gperf_ab22.json）
fall 900f 三窗，PITS 15 每跑前重 cp；peaceful = config 值，fall 场景
config 默认 normal = ENT-8 代码路径惰性）：fluidCells win1 51969 逐位
+ win2/3 29803/29800 vs 29703/29700（**±100 = 场景 rim churn 相位一
周期，非回归**——裁定链：① 终档流体差 3 格 + 火差 2 格全在程序化熔岩
湖 rim 区 (72..84,36,70..77) ② **同二进制 v114 自分歧实证**（r1 火
50 格 / r2 火 51 格 + 流体 churn 簇 3 格位移，PITS 283 类先例门 117）
③ ENT-8 代码 diff = difficulty.hpp（normal 下惰性）+ 版本串，流体代码
位级同）+ PIPE3 changesApplied 1404/1405（±1 churn 类）+ 全指标同档
= 代码成本零。

## 发布

**v1.15.0 = 组 1.11-1.15 组末 → tag v1.15.0 + GitHub release**（notes
覆盖 v1.11.0 FLUID-6 / v1.12.0 ENT-5 / v1.13.0 ENT-6 / v1.14.0 ENT-7
/ v1.15.0 ENT-8 = 组内 5 版；发布规则 = BACKLOG「执行顺序」第 3 条）。
