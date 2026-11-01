# Phase 29 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 29 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-06, Phase 29]` 破坏粒子同一次爆发内所有粒子同偏移/同速度（P15 实现、P20 发现的潜在缺陷）** — 一行修复：`particle.cpp` 的 `uniform01(state)` 原来只调 `splitmix64(state)`（仓库 splitmix64 是**纯 finalizer**，不推进 state）→ 同一 burst 内 8 次调用取到同一个值，16 个粒子完全重叠成 1 个可见点（偏移/速度/寿命全同值；单粒子视觉在破坏反馈里不易察觉，故 P15 未发现，P20 给 mob AI 写推进式流时对比显出）。修复 = `state += 0x9E3779B97F4A7C15ULL`（gamma）后再 mix——与 `mob_entity.cpp` 的推进式流同款约定（state += gamma, result = mix(state)）。修复改变粒子外观（1 个团 → 16 个分散小立方体），但**无截图基线覆盖粒子**（粒子只在玩家破坏方块后存活 0.45–0.75s，所有无输入基线/回归场景零粒子）→ 基线结构性免疫。**测试** 669/669（+2）：`BurstParticlesAreDistinctWithinOneBurst`（回归：同 burst 16 个 position.x 与 16 个 life 全互异——旧 bug 下全同值）/ `BurstValuesMatchTheAdvancingStream`（钉精确流值：seed 0x1234、cell (1,2,3) 下 particle[0] pos (1.4108357429504395, 2.396921157836914, 3.5287749767303467) life 0.6028023362159729、particle[1] pos (1.187707781791687, 2.493948221206665, 3.238013982772827) life 0.5545276999473572——position/life 路径无 libm 调用，钉值跨平台有效；钉住"每次 uniform01 调用推进一次 state、每粒子 ox/oy/oz/u1..u4/life 调用序"）。**E2E**（P25 协议 + xdotool 脚本化输入）：**base18 门 ×2 字节不变**（**第 5 次结构性免疫验证**：粒子只在破坏时 spawn，无输入 = 零粒子）；**brk 对 ×2 存档确定性验证**（手工存档 p29brk：出生点 + **pitch −1.5 朝脚下方块**——准星距脚 0.115 块，4px grab 漂移只移动准星 0.0003 块 = 瞄准对 grab delta 免疫；脚本 D+1.5s 左键按住（**press 即锁指针** + 开始破坏）→ D+2.6s 松开（grass T=0.9s + 0.2s 余量 → 下方块 stage-1 裂纹））：**两拍存档除 yaw/pitch 外逐字段一致**且全部命中预测（blocks [[0,53,0,"air"]]、slot9 dirt×1【grass 掉 dirt；slot1 dirt 已满 64 → 入首个空格；物品从坑内被拾取（距脚 ~0.4 < kPickupDistance 1.0）】、pos (0.5,53,0.5)【掉 1 块无伤】、health 20、hunger 19【4s tick】、time 0.260417、entities 0）；yaw 0.0/−0.01、pitch −1.50/−1.51 = grab delta 漂移探测（同 P28 模式）；直下视角无天空 → 场景门改用唯一色计数（1746/1732），HUD 反检红心 680 px ✓；**粒子散布视觉验证**（vis 对：pitch −1.02 朝 (0,53,-1)，189f 有粒子 vs 300f 无粒子，两拍 yaw/pitch 均 0.0/−1.02 干净 = 同机位）：帧差 = 59×72 px / **353 种颜色**的 cluster（坑心处，= 分散粒子 + 旋转物品实体）——旧 bug（16 粒子重叠）应只有单立方体 ~40 px 团 + 少数字色。**新教训**：① 直下视角（pitch −1.5）的可视锥只覆盖脚周 0.11–1.28 块环且只含较低部分——破坏粒子（向上偏置速度）大多飞出锥外，**粒子视觉验证必须选斜视角（pitch −1.02 级别，准星在前方地面格面心）**；② 输入 E2E 的帧差分析要先排除已知 UI 变量（本对 29 px 差异 = 饥饿图标 20→19，非粒子）。**全帧字节一致仍受 grab delta 环境限制**（brk 对帧差 = 4px 相机旋转 + 饥饿图标；存档 = 确定性权威证据，同 P28 结论）。

## 会话存档（原 SESSION_CONTEXT）

- **提交** `32d8e47`（4 文件，+47/−2）。
- **目标**：P29 候选第 6 条（小项中唯一保留项，用户指示"只留最重要的必选项"）——
  粒子同 burst 同值缺陷一行修复（P20 发现的潜在缺陷）。
- **根因**：`particle.cpp` 的 `uniform01(state)` 只调 `splitmix64(state)`——仓库
  splitmix64 是**纯 finalizer**（x 按值传、不修改 state）→ 同一 burst 内 8 次
  uniform01 调用取到同一个 double → 16 个粒子的偏移（ox/oy/oz）/ 速度
  （u1..u4 → 方向/速率）/ 寿命全同值 → 完全重叠成 1 个可见点。P20 给 mob AI
  写推进式流（state += gamma 后再 mix）时对比显出，记入 KNOWN_ISSUES「UI /
  音效 / 粒子」节，修复留后续。
- **修复**：`uniform01` 在 mix 前加一行 `state += 0x9E3779B97F4A7C15ULL;`
  （gamma = splitmix64 标准增量，与 mob_entity.cpp / worldgen `kFeatureStep` /
  day_cycle 同一常量）——仓库推进式流同款约定（state += gamma, result =
  mix(state)）。
- **影响面**：粒子外观 1 个团 → 16 个分散小立方体；**无截图基线覆盖粒子**
  （粒子只在玩家破坏方块后存活 0.45–0.75s，所有无输入基线/回归场景零粒子）→
  基线结构性免疫。`uniform01` 是 particle.cpp 匿名命名空间私有函数，无其他
  系统受影响。
- **测试** 669/669（+2）：`BurstParticlesAreDistinctWithinOneBurst`（回归：
  同 burst 16 个 position.x 全互异 + 16 个 life 全互异——旧 bug 下全同值）；
  `BurstValuesMatchTheAdvancingStream`（钉精确流值：seed 0x1234、cell (1,2,3)
  下 particle[0] pos (1.4108357429504395, 2.396921157836914,
  3.5287749767303467) life 0.6028023362159729；particle[1] pos
  (1.187707781791687, 2.493948221206665, 3.238013982772827) life
  0.5545276999473572——position/life 路径无 libm 调用（velocity 路径有
  cos/sin/sqrt 故不钉），钉值跨平台有效；钉住"每次 uniform01 调用推进一次
  state、每粒子 ox/oy/oz/u1..u4/life 调用序"）。钉值由 Python 逐 IEEE 运算
  模拟（float32 量化用 struct round-trip）预计算，与 C++ 实跑逐位一致。
- **E2E**（P25/P27 协议 + xdotool 脚本化输入；驱动 /tmp/opencode/p29_e2e.zsh
  + p29_vis.zsh）：
  - **base18 门 ×2 字节不变**（**第 5 次结构性免疫验证**：粒子只在破坏时
    spawn，无输入 = 零粒子）。
  - **brk 对 ×2 存档确定性验证**：手工存档 p29brk（probe 法：新存档读出生点
    (0.5,54,0.5) → **pitch 改 −1.5（朝脚下方块）** + time 0.25；准星距脚
    0.115 块、距面边 0.385 块 → 4px grab 漂移只移动准星 0.0003 块 = **瞄准对
    grab delta 免疫**）；脚本 D+1.5s 左键按住（指针**press 即锁**——
    handleInteraction 的 `wasPressed(Left)` 分支；锁定次帧开始破坏）→
    D+2.6s 松开（grass T = 1.5×0.6 = 0.9s + 0.2s 余量 → 破坏后 0.2s 按住对
    下方块 (0,52,0) 的破坏进度 = stage-1 裂纹，确定性）。**两拍存档除
    yaw/pitch 外逐字段一致**且全部命中预测：blocks [[0,53,0,"air"]] /
    slot9 dirt×1（grass 掉 dirt；slot1 dirt 已满 64 → 入首个空格 slot9；物品
    在坑中心生成、玩家坠坑、玩家↔物品分离把物品推到身侧、距脚 ~0.4 <
    kPickupDistance 1.0 → 被拾取、entities 0）/ pos (0.5,53,0.5)（掉 1 块、
    fallDamage = max(0, 1−3) = 0）/ health 20 / hunger 19（4s tick 落帧 241）/
    time 0.260417（0.25 + 5/480）；yaw 0.0000/−0.0100、pitch
    −1.5000/−1.5100 = grab delta 漂移探测（同 P28 模式；pitch 距
    kMaxPitch 1.5533 钳制余量 0.0533 > 最大漂移 ~0.02 → 永不触钳）。直下
    视角无天空 → 场景门改唯一色计数（1746/1732 OK）；HUD 反检红心 680 px
    ✓。全帧 ×2 不一致 = 4px 相机旋转（环境，同 P28 结论：存档 = 确定性
    权威证据）。
  - **粒子散布视觉验证**（vis 对，pitch −1.02 朝 (0,53,-1) 面心）：189f
    （粒子 age = 0.75−D ∈ [0.25, 0.65] → 5–16 粒子在空中）vs 300f（粒子
    已死），两拍 yaw/pitch 均 0.0000/−1.0200（干净 grab）= 同机位 → 帧差 =
    59×72 px / **353 种颜色** cluster（坑心处 x[611,669] y[410,481] =
    分散粒子 + 旋转物品实体【物品坠坑未拾取（1.41 > 1.0）、entities 1】）——
    旧 bug（16 粒子重叠）应只有单立方体 ~40 px 团 + 少数字色（1–4 种）。
- **新踩坑（2 条）**：① 直下视角可视锥不含粒子（见坑位合集）② 帧差分析先
  排除已知 UI 变量（29 px = 饥饿图标 20→19，见坑位合集）。
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length
  600），与 P29 无关。
