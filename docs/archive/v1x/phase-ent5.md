# ENT-5 坠落伤害（v1.x 12/33，v1.12.0，门 127，ENT 行 1/6）

v1.x 任务矩阵第十二项。"坠落伤害"原始定义已佚 = 代理定案最小完整版：
**Mob 坠落伤害**（玩家侧 Phase 16 已完整：`fall > fall_damage_offset(3)
&& !isInWater() && !isInLava() → damage(fall − 3)`；MC 中 mob 同样受
坠落伤害 = 缺口在 mob 侧）。

## 定案

- **Player 规则镜像到 MobEntity**（`MobEntity::update` 落地转换处，
  stepUp 之后）：`m_onGround && !m_wasOnGround` 时净落差
  `m_fallStartY − position().y` **> kMobFallDamageOffset(3.0f)** 且
  **落地格无流体**（fresh `overlapsFluid(Water)||overlapsFluid(Lava)`
  扫描最终位置 = 玩家 P54 同款"任何流体取消坠落"；不用 tick 初的
  `m_inFluid`——落地帧该值滞后一 tick，会漏 1 深池，玩家注释同款
  教训）→ `damage(fall − 3)` 走既有伤害/死亡/掉落管线。
- **kMobFallDamageOffset = 3.0f 命名常量**（MC 值 = 玩家同值；不做
  per-mob 字段 = MC 无 mob 差异，YAGNI）。
- **钉点**：mobs 是 ephemeral（从不存档）→ 构造函数一次钉
  `m_fallStartY = pos.y`（无玩家 restorePosition 的读档重钉需求）；
  离地转换（`!m_onGround && m_wasOnGround`）重钉 = 走落悬崖的
  fall-start 正确（1 子步滞后 = 确定性，紧公差断言）。
- **帧锁定**（硬规则 8）：m_onGround = 本 tick 物理结果，无 real dt、
  无随机。零 format/config/内容/音效。
- **E2E 设计约束记档**：spawner floor band = 玩家脚高 ±kSpawnFloorDelta
  → 敌对 mob 只生成在玩家同层 ±2 = **生成 mob 的致死坠落（需 23+）
  无法用自然生成构造**（chase 受 detect_radius 16 限制到不了；
  knockback 方向 = 远离玩家 = 推不向悬崖）。→ S2 = **CPU harness
  探针**（FLUID-3 夜探针先例：窗口免、逐值确定性）：2×1 高台 y47 +
  清挖坑底 y22（25 落差）+ 无玩家（wander-only）+ 种子探测选出
  wander 走落的 seed（1/5/6/8 探针实证）；两侧物理同（同落 y22），
  new = 落地死（25−3=22 ≥ 20 hp）/ old = 存活 hp 20 = 互斥终态。
  S3 = 围栏竞技场零回归 A/B（mob 永不得坠落 → ENT-5 代码结构惰性 →
  new≡old 字节一致）。

## 实现

`src/entity/mob_entity.{hpp,cpp}`：kMobFallDamageOffset 常量 +
m_fallStartY/m_wasOnGround 成员 + 构造钉点 + update 落地块（~25 行）。

## 测试（1779/1779 = 1774 + 5，-Werror 0 警告）

MobFallDamageTest：9 块坠落 → 恰 6 伤（14.0 精确）/ 3 块内坠落 0 伤 /
1 深水池落地 0 伤（流体取消，fresh 扫描）/ 23 块坠落致死（既有死亡
管线）/ 走落 5 格平台 → ~2 伤（离地转换钉点 1 子步滞后 = EXPECT_NEAR
0.02 紧公差）。既有 mob 套件零改动通过 = 行为零回归。

## E2E 门 127

**S1 = 第 127 次结构性免疫免重锚**：fresh 60f 四方截图 ×2 387726 B
全字节一致（new v1.12.0 ≡ old v1.11.0 ≡ base18）+ savenorm new×2 /
old×2 EXACT（nomobs config：无 mob = 无坠落 = 结构惰性；honey 无关）。

**S2 = CPU harness 探针矩阵**（gen127：2×1 高台 y47 + 清挖坑底 y22 =
25 落差，无玩家 wander-only，种子 1/5/6/8 探针选出走落 seed；两侧
物理同 = 同落坑底 y22，**逐值 ×2 确定性**）：
- new v1.12.0：seed 1/5/6/8 全 **dead=true**（deadAt 499/370/201/229，
  25−3=22 ≥ 20 hp = 坠落致死走既有死亡管线），终位 y22.00；
- old v1.11.0：同 4 seed 全 **dead=false hp=20.00** 终位 y22.00
  （seed 6 两侧终位逐位同 (24.17,22.00,-6.55) = 物理恒等实证）；
- 互斥终态 × 4 seed × ×2 = 坠落伤害行为定案。
  （harness 坑 PITS 302：old 侧 binary 必须从 old 树 cwd 运行——相对
  assets 路径，repo cwd 的 honey.json 被旧注册表拒 → create() assert。）

**S3 = 围栏竞技场零回归 A/B**（12×12 石墙围栏 + 夜 0.75 + mobs on
g115_cfg，900f 无输入；围栏 + 平地板 = mob 永不得坠落 → ENT-5 代码
结构惰性）：敌对 mob 自然生成 + 追逐 + 击杀 idle 玩家（双侧同帧死，
hp 0 同位）——new ×2 RAW EXACT 235680 B + old ×2 RAW EXACT 235680 B +
**new_a ≡ old_a 字节一致**（mob 生成/追逐/攻击/死亡全链零回归）+
savenorm EXACT + 玩家态 EXACT。

## 性能 A/B（发布前必做）

**PASS = 成本可忽略**（tm_v111_perf v1.11.0 插桩 vs tm_v112_perf
v1.12.0-dev 插桩【v111 插桩 fluid.cpp 移植 + step0 game hunks + hunk#6
honey 区手工补，同 v111 先例】，同 seed（perf_ab17 剥 fluids →
gperf_ab22.json）fall 900f 三窗，PITS 15 每跑前重 cp）：fluidCells
win2/3 **29803=29803 / 29800=29800 逐位一致**（v112 首跑 29703/29700
= 一次性 churn 相位波动，重跑复现 29803/29800 = PITS 283 场景自分歧
类非回归；v111 亦三跑恒 29803/29800）+ win1 51925/51923（±0.004%
填充 real-dt 类）+ PIPE3 changesApplied 1404=1404 逐位 + tick med 5.1/
9.6 ms 同档 + upd/render/swap 全同档；终档流体差 = 1 格
（(83,36,76) lava = PITS 283 类 churn 格，同二进制自分歧先例）= 代码
成本零（mob 代码仅 update 落地块，fall 性能场景无坠落事件）。

## 坑位（PITS 追加）

- **PITS 301（tick 初的 m_inFluid 滞后一 tick = 漏 1 深池落地）**：
  ENT-5 首版用 m_inFluid 判落地流体——它在本 tick 移动前采样，落地帧
  的脚刚进入水格时 m_inFluid 还是 false → 池底落地仍吃坠落伤（玩家
  Phase-16 注释同款教训："a falling player enters a 1-deep pool one
  tick after the last airborne sample"）。修 = fresh
  overlapsFluid 扫描最终位置。单测 LandingInAPoolCancelsTheFall 钉死。
- **PITS 302（CPU harness 的 assets 相对路径 = binary 须从所属树 cwd
  运行）**：ENT-5 S2 首跑 old 侧 harness 从 repo cwd 运行 → 加载
  **新** assets（含 honey.json）→ v1.11.0 注册表拒未知 fluid 名 →
  WorldGenerator::create() assert（RC=134，输出全空 → 门脚本误判
  "OLD died?!"）。纪律：harness 对每个 binary 用 `( cd <tree> &&
  <harness> )`；门脚本断言前验 harness 有 seed= 行输出。
