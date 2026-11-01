# Phase 25 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 25 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 25]` 敌对 mob 生成无保护区 / 无难度系统（Mob 节遗留）** — Phase 25 实现生成保护半径/难度系统。**纯模型** `core/difficulty.hpp`（新，头文件，游戏与单测共享）：`parseDifficulty`（easy/normal/hard 严格大小写解析，未知值拒绝）/ `spawnProtectionRadius`（8/12/16 = Minecraft 值）/ `hostileDamageScale`（0.5/1.0/4÷3——zombie 3 点 → 1.5/3/4，镜像 MC 2.5/3/4 的相对形状）/ `hostileSpawnRateScale`（0.5/1.0/1.5）/ `checkEveryFramesFor(base, d) = max(1, round(base/scale))`（30 → 60/30/20）；**normal = 三个缩放全 1.0 = 基线行为零变化**（默认值）。**数据** mob JSON 新字段 `hostile`（可选 bool，默认 false，非 bool 严格拒绝——数据驱动，不靠 `attack_damage>0` 推断；zombie.json = true）。**代码** `findMobSpawnCell` 新默认参 (protectionRadius, protectionX, protectionZ)：保护环 = **世界出生点**（世界属性，读档不改它）水平圆，**仅 hostile 受检、passive 豁免**（Minecraft 约定），扫描**首查**（最廉价拒绝；radius 0 = 无保护）/ Game：config `mobs.difficulty` 严格解析（默认 "normal" = 零行为变化）、生成节奏 = `checkEveryFramesFor(m_spawnConfig.checkEveryFrames, m_difficulty)`、伤害缩放 per-mob 在 hit 回调捕获（`hostile ? scale : 1.0`）、F3 新 `difficulty` 行、版本行 Phase 25。**测试** 639/639（+13：`test_difficulty.cpp` 新 6[解析 3/取值表 2/checkEveryFramesFor round+min1 1]、hostile 解析 3[缺省 false/true/非 bool 拒绝]、保护几何 4[环内拒/环外过/边界语义/passive 豁免]）。**E2E**（黄昏场景：day_length 1200 + start 0.536，dayFactor≈0.04——场景可见且敌对可生成；存档 p25prot = 玩家距世界出生 5 块、p25far = 22 块【所有保护圈外】；**协议**：批内零并发 :0 活动 + 每运行前 unmap 覆盖游戏区的已映射窗口 + 每截图过天空色新鲜度门【严格 0 容差、flash-aware——本阶段发现前缓冲呈现冻结与红闪伪装两个 E2E 机制，详见 SESSION_CONTEXT 坑位合集】）：base18 门 ×2 逐字节一致（normal 缺省 = 零变化）；**prot（360f）**：pe/pn/ph ×2 各自逐字节一致，对新鲜 nomobs 对照的**僵尸绿像素**判据 = easy **6589 px**（敌对在 R=8 环外生成并走向玩家）/ normal **0**（R=12 环内全保护）/ hard **0**（R=16），cow（豁免）棕像素 7563/7368/14665 照常生成；心带全 0（本确定运行僵尸生成格偏远侧、首击 >f360——早批"心带 5228"数已作废，系陈旧帧产物）；**rate（360f）**：每难度 3 次运行两两逐字节一致，对 p25far 新鲜 nomobs 对照：僵尸绿 6754/6770/7336、总差 50272/53550/54078、难度两两差 3968/5365/3926（生成时序不同 → 组成/位置不同）、心带 0（health 20）；节奏 60/30/20 本身由单测锚定（4-mob 上限使可见数量收敛 = v1 已知简化）；**dmg（600f）**：de/dh ×2 各自逐字节一致（天空 (123,60,37) 精确 = 无红闪），dmg-normal 6 次运行逐字节一致（天空 (143,51,31) = flash(α≈0.15) 罩在新鲜天空上——最后一击落截图帧前 ~17 模拟帧，红闪真实 dt 衰减在 60fps 下剩 ~217ms；30fps 运行同一击的闪已过期 = 旧批"两簇"是帧率产物而非非确定）；**存档 health 判据**（确定性模拟，全部运行一致）：easy **12.5**（5 击 ×1.5）/ normal **2.0**（6 击 ×3.0）/ hard **4.0**（4 击 ×4.0），心带两两差 1497/2864 px；跨难度世界区 ±1~4 小差 = 黄昏暗 mob 剪影位置不同（HUD 带差严格限于 mob x 范围 ≤810 = 半透明 HUD 透底，非缺陷）。**新 v1 条**：无 Peaceful / 保护 = 静态圆（无村庄保护）/ 节奏 = 全局共享缩放（非按类型权重），见 Mob 节。

## 会话存档（原 SESSION_CONTEXT）

提交 `36e2b1d`（2026-10-05，16 文件 +430/−14）。

### 设计（生成保护半径/难度系统）
- **`core/difficulty.hpp`（新，头文件纯模型，游戏与单测共享）**：`enum class Difficulty
  {Easy, Normal, Hard}` + `parseDifficulty`（严格大小写敏感，未知值拒绝）/
  `spawnProtectionRadius`（easy 8 / normal 12 / hard 16 = Minecraft 值）/
  `hostileDamageScale`（0.5 / 1.0 / 4÷3——zombie 3 点 → 1.5/3/4，镜像 MC 2.5/3/4 的
  相对形状，normal = 数据基线）/ `hostileSpawnRateScale`（0.5/1.0/1.5）/
  `checkEveryFramesFor(base, d) = max(1, round(base/scale))`（30 → 60/30/20）。
- **mob 数据字段 `hostile`**（可选 bool，默认 false，非 bool 严格拒绝）：数据驱动而非
  行为推导（不靠 `attack_damage>0` 推断——避免"有攻击数据 = 敌对"的语义耦合）；
  zombie.json = true。
- **保护环** = **世界出生点**（新世界的出生点，世界属性；读档不改它——存的是玩家位置）
  的水平圆；**仅 hostile 受检、passive 豁免**（Minecraft 约定，radius 0 = 无保护）；
  `findMobSpawnCell` 新默认参 (protectionRadius, protectionX, protectionZ)，扫描
  **首查**（最廉价拒绝：先过环再做光/重叠/间距检查）。
- **Game 集成**：config `mobs.difficulty` 严格解析（默认 "normal" = 零行为变化）/ 生成
  节奏 = `checkEveryFramesFor(m_spawnConfig.checkEveryFrames, m_difficulty)` / 伤害缩放
  per-mob 在 hit 回调捕获（`hostile ? scale : 1.0`，非 hostile 恒 1.0）/ F3 新
  `difficulty` 行 / 版本行 "Phase 25"。
- v1 简化（KNOWN_ISSUES Mob 节新条）：无 Peaceful；保护 = 静态圆（无村庄保护）；
  节奏 = 全局共享缩放（非按类型权重）；4-mob 上限使可见数量收敛。

### 测试（+13，639/639）
- `tests/test_difficulty.cpp`（新）6 个：解析（3 合法 + 未知/大小写/空拒绝路径）、
  半径+伤害+节奏取值表、checkEveryFramesFor（round + min 1）。
- `tests/test_mob.cpp` +3（hostile：缺省 false / true / 非 bool 拒绝）+4（保护几何：
  环内拒 / 环外过 / 边界语义 / passive 豁免 radius 0）。
- 既有 626 全绿（normal 缺省路径 = 零行为变化回归）。

### E2E（黄昏场景；新协议 = 批内零并发 :0 活动 + 每运行前 unmap 覆盖游戏区的已映射
窗口 + 每截图过天空色新鲜度门【严格 0 容差、flash-aware】）
- **配置/存档**：config_p25_{easy,normal,hard}(_nomobs).json（day_length 1200、
  start 0.536 → 黄昏 dayFactor≈0.04：场景可见且敌对生成光 floor(15×df)=0）；
  p25prot.json（玩家距世界出生 5 块）/ p25far.json（22 块，环 16..28 = 所有保护圈外）。
- **base 门**：p25_base_1/2 与 base18.png 逐字节一致（normal 缺省 = 零变化）。
- **prot（360f，t=0.541）**：pe1=pe2 / pn1=pn2 / ph1=ph2 各自逐字节一致；对照
  ctrl360_1=ctrl360_2 逐字节一致（FRESH (130,63,38)）；**僵尸绿像素判据**（run 中
  强绿差像素：g>1.15r 且 g>1.15b 且 g>4，且对照像素非绿）：easy **6589 px**（敌对在
  R=8 环外生成并走向玩家）/ normal **0**（R=12 环内全保护）/ hard **0**（R=16）；
  cow（豁免）棕像素 7563/7368/14665 照常生成；总差 68543/7840/15379（pe1 含 hunger
  条 1 点真实 dt 差 234 px——P23 约定）；心带全 0（本确定运行僵尸生成格偏远侧、首击
  >f360——早批"心带 5228"数已作废，系陈旧帧产物）。
- **rate（360f，far 位置）**：每难度 3 次运行两两逐字节一致（9/9）；对照
  ctrlfar_1=ctrlfar_2 逐字节一致（FRESH，hunger 18 与运行一致）；vs 对照：总差
  50272/53550/54078、大差(>9) 10450/11238/11195、僵尸绿 6754/6770/7336、cow
  0/3200/3073、心带 0（health 20 全满）；难度两两差 3968/5365/3926（生成时序不同 →
  组成/位置不同）；节奏 60/30/20 本身由单测锚定（checkEveryFramesFor 表），E2E 验证
  "黄昏敌对/被动按难度确定性生成"（4-mob 上限使 normal/hard 可见数量收敛 = v1 已知
  简化；180f 窗口本可区分 easy=3 mob 但落在启动冻结窗内不可信——见下踩坑 1）。
- **dmg（600f，far 位置）**：de1=de2=r2de、dh1=dh2=r2dh 逐字节一致（天空
  (123,60,37) 精确 = 无红闪）；dmg-normal 6 次运行逐字节一致（dgn_1 = 旧 dn1 = dn3，
  跨天跨驱动复现）（天空 (143,51,31) = flash(α≈0.15) 罩在新鲜 (123,60,37) 上——
  normal 最后一击落截图帧前 ~17 模拟帧，红闪真实 dt 衰减在 60fps 下剩 ~217ms；
  30fps 运行同一击的闪已过期 → 旧批"两簇"是帧率产物而非非确定）；**存档 health
  判据**（确定性模拟，全部运行一致）：easy **12.5**（5 击 ×1.5）/ normal **2.0**
  （6 击 ×3.0）/ hard **4.0**（4 击 ×4.0）——伤害缩放 × 生成时序 → 击数不同；心带
  两两差 de–dh **1497 px**、de–dgn **2864 px**（含罩）、dgn–dh 2864 px；跨难度世界区
  ±1~4 小差 = 黄昏暗 mob 剪影位置不同（已验证：HUD 带差严格限于 mob x 范围 ≤810、
  范围外为零 = 半透明 HUD 透底，非缺陷）。

### 本会话重大踩坑（E2E 截图；详见「坑位合集」新增条）
1. **前缓冲呈现冻结（真陈旧）**：窗口创建后 ~3-5 s 内前缓冲停在早期帧（全屏遮挡——
   OpenCode UI 2552×1552 + 其 awesome frame——延长它）→ 3 s 运行（180f）11/11 陈旧
   （天空反解出 8~106 帧偏移）、6/10 s 运行（360f/600f）23/23 新鲜；正午平台场景不可见
   （P24"箭带 158~440px 抖动"疑似同源，待后续运动实体用例复核）。对策：E2E 窗口 ≥6 s
   + 预 unmap 遮挡窗（UI client + frame + 子窗，xwininfo 几何判定）+ 天空色门。
2. **红闪伪装成陈旧（假陈旧）**：flash(α) 罩住天空使纯天空色门误判——(123,60,37) →
   (143,51,31) 精确等于 flash(0.15)（8 次运行 6 次逐字节一致 = 确定性模拟帧 + 稳定
   60fps）。对策：skygate.py 改 flash-aware（接受 α∈[0.10,0.25] 罩变体，±1/通道）。
3. **早批 180f 数字全部作废**（天空 R=136..140 均陈旧）→ rate 判据全部 360f 重测；
   prot"心带 5228"旧数作废 → 改僵尸绿像素判据（6589/0/0）——360f 内首击未到，
   心带恒 0 不是判据。教训：判据像素区域必须与"该帧时刻游戏状态"核对，不能沿用
   陈旧帧上测出的数。
4. **ctrl 必须同存档**：rate 的 nomobs 对照必须 p25far（同机位）；误用 p25prot
   对照得 68% 全图差（不同视角），白跑一轮。

### 变更文件
- `src/core/difficulty.hpp`（新）/ `src/core/game.{hpp,cpp}`（config 解析、节奏、伤害
  缩放、F3、版本行）/ `src/entity/mob_registry.{hpp,cpp}`（hostile 字段）/
  `src/entity/mob_spawning.{hpp,cpp}`（findMobSpawnCell 保护参）/
  `src/ui/debug_overlay.{hpp,cpp}`（difficulty 行）/ `assets/data/mobs/zombie.json`
  （+hostile）/ `tests/test_difficulty.cpp`（新）/ `tests/test_mob.cpp` /
  `tests/test_debug_overlay.cpp` / `CMakeLists.txt` / README.md（Mob 节 +难度 bullet、
  roadmap +P25 行）/ KNOWN_ISSUES.md（Mob 节 3 条新 v1 条 + P25 归档条）
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length 600），与 P25 无关。
