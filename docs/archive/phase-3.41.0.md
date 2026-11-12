# Phase v3.41.0 — V3-19 死亡屏计时（门 231）

> 3.x「世界扩展」UX 实机报障项（11-08 用户报障——「死亡界面存活时间
> 不停增加」）。**表现层 3 点修**（零 format / 零 config / 零新资产）：
> 根因 = 死亡屏「存活」行活读 `m_frameCount`（game_ui.cpp:2042）——
> 死亡后世界模拟照跑帧继续推进，数字一直涨；其余四行（采挖/放置/
> 击杀 = 会话计数器、行走 = 物理冻结）天然静止，唯此行脱缰。

## 实现（3 点 + 1 测试面）

1. **`PlayerSession + int deathFrame = -1`**（game.hpp）——存活→死亡
   转移的帧快照（会话态，不存档——死亡屏 = 会话面，V3-13 会话统计
   同款偏离记档）。
2. **转移块快照**（game_ui.cpp alive→dead，SURV-7 `deathCount++` 旁）：
   `s.deathFrame = m_frameCount`。
3. **死亡屏读改快照**（game_ui.cpp 存活行）：活读 →
   `Player::survivalStatsLine(lsession.deathFrame, m_frameCount)`——
   **-1 回落 = 旧读法（位级红线）**：会话内未死者（不可达——屏只在
   死时画）走原活读，行为逐位不变。
4. **测试面 = 纯函数**（`Player::survivalStatsLine`，deathCauseLine 的
   孪生——cause 行读死亡的 DAMAGE INFO，此行读死亡的 FRAME）：
   快照帧独立（deathFrame=1234 @ 任意 frameCount 同文）+ -1 回落
   活读位级（1234f ≠ 1294f）+ 整式边界（3600f = 1 分 0 秒）——
   `PlayerTest.SurvivalLineIsDeathFrameSnapshot`（2999 = 2998 + 1）。

## 门结论（门 231 全 PASS）

**2999/2999**（+1 V3-19 纯函数族）。

**结构免疫第 112 次**【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——死亡
屏只在死时画，锚场景无死亡 = 零像素】+ canary 4×2 新≡旧二进制
（tm_v340 v3.40.0）RAW+savenorm EXACT【无死亡 = 存活行冷路径】。

**死亡屏两帧臂（新断言，BACKLOG 规格）**：8×8 平台 + 玩家 y84 悬空
（fall 30 → dmg 27 > 20，无输入无 mob 确定性坠死 @ f≈97，base 30-f
场景），day_length 1e7 冻日：
- **新树**：--frames 254 vs 434 截图，存活行 crop（y468-498 实测
  bbox——PITS 423①：h/2+74 名义位 ≠ 渲染 bbox）→ **AE = 0 逐像素
  冻结**（两帧同文「存活 0 分 1 秒」）。
- **旧树对照（TDD 前置复现）**：同臂 tm_v340 → 254 vs 434 **AE = 43
  px**、254 vs 600 **AE = 42 px**（活读爬升「4 秒」→「7 秒」→「10
  秒」——秒位字形像素差 = 修复前行为实证，既有 ×2 RAW 同帧协议抓
  不到跨帧漂移，本臂专治）。

**性能 A/B 同档**【旧 = tm_v340 v3.40.0 + g139 双树 27 markers：OW
900f 无输入 PERF-WIN 三窗同档 + changesApplied 逐位 + 终档跨树
savenorm EXACT = 表现层零自成本】。

**门内捕获 PITS 423 两坑**【①死亡屏行 y 名义位（h/2+offset）≠ 渲染
bbox——存活行实际 top ≈ h/2+108（标题 scale-2.0 的 32-px 行步偏移），
crop 带须截图实测（PITS 384⑤ 的死亡屏续篇）②两帧臂的日冻结 =
day_length 1e7 配置（太阳 <0.001° 位移 = 背景 0-diff，存活行 crop
可严格 EXACT）】。
