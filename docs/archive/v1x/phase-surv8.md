# SURV-8 工具 10×（v1.x 19/33，v1.19.0，门 134，SURV 行 3/5）

v1.x 任务矩阵第十九项。原始定义已佚 = 代理定案最小完整版：**MC 无效率
工具 10× 惩罚规则**（消费 KNOWN_ISSUES Phase-11 条"仍缺：MC 的'无效率
工具慢 10×'规则（本项目'无效率' = flat 档，不慢 10×）"）——Minecraft
的采矿规则：对目标方块**没有效率加成**的工具，采矿速度 = 自身速度
÷ 10（经典例：镐挖泥土慢、剑挖石头 = 1/10 手速）。

## 定案

- **规则 = 纯函数 `effectiveBreakSpeed` 的一个分支**（item.hpp）：
  工具且目标 ∈ `fast_against` → `breakSpeed × multiplier`（P67 既有，
  位级不变）；工具且目标 ∉ 列表 → **`breakSpeed × kWrongToolFactor`**
  （新命名常量 **0.1f**，MC 精确 1/10）；**无 fast_against 列表的工具
  对一切方块吃惩罚**（= MC 剑的情形）；手/非工具物品恒 1.0（位级不变）。
- **零数据模型变更**：无新字段/无新内容/无 config（9 工具 JSON 一字不
  动 = 快对清单保持 P67-P69 值，惩罚是核心规则不是内容调参）。
- **P69 obsidian 排除条款随规则演进**：铁镐挖 obsidian 从"flat 3.75 s"
  变为"慢档 37.5 s"（0.4 有效速）= MC 方向一致（铁镐对黑曜石本就是
  慢的情形）。
- **项目刻度下的实际值**（exact，单测钉死）：
  - 铁镐（4.0）× 木（硬度 2.0，off-list）= 0.4 → **7.5 s**（SURV-8 前
    0.75 s）——E2E 判别窗口设计依据。
  - 铁斧（1.0 flat）× 石（硬度 1.5，off-list）= 0.1 → **22.5 s**。
  - on-list 路径位级不变：铁镐×石 = 8.0 → 0.28125 s / 铁斧×木 = 2.0
    → 1.5 s（E2B 对照 + 单测双向防泄漏）。
- break 通道（`selectedBreakSpeed`，game.cpp 单调用点）零改动——规则
  全在纯函数内。

## 实现

`src/item/item.hpp`（kWrongToolFactor 常量 + effectiveBreakSpeed 分支
重写 + 注释更新）、`tests/test_item.cpp`（4 处 off-list 钉演进 + 1 测
改名 + 1 新测）、`CMakeLists.txt` + `tests/test_debug_overlay.cpp`（版
本 bump）。

## 测试（1828/1828，+1 净，-Werror 0 警告）

- `EffectiveBreakSpeedFastAgainstNamedBlocksOnly`：斧 off-list 石
  1.0 → **0.1**（惩罚）。
- `EffectiveBreakSpeedFlatToolStaysFlat` → 改名
  `EffectiveBreakSpeedListlessToolTakesWrongToolPenalty`：无列表镐
  4.0 → **0.4**（一切方块吃惩罚 = 剑情形）+ kWrongToolFactor = 0.1
  exact 钉。
- `Phase69PickaxeEffectiveBreakSpeed`：铁镐 off-list 木/obsidian
  4.0 → **0.4**（on-list 8.0 族 + 木镐×石 4.0 钉保持）。
- 新 `Surv8WrongToolDurations`（真资产 exact-time）：铁镐×木 **7.5 s**
  / 铁斧×石 **22.5 s**（EXPECT_NEAR 1e-6 = 0.1f 非二进制精确的浮点残
  差说明）/ on-list 铁镐×石 0.28125 s + 铁斧×木 1.5 s 位级保持。
- 手/非工具 = 1.0 族零改动。

## E2E 门 134

- **S1 = 第 134 次结构性免疫免重锚**：fresh 60f 四方 ×2（new v1.19.0
  / old v1.18.0 = 71dfc06，g118_nomobs）+ base18 + savenorm（无输入 =
  无按住破坏 = 规则永不执行 + 内容/纹理零变更）。
- **S2a 判别（铁镐 × 木，off-list）**：几何 = 玩家 (22.5,45,-2.5)
  yaw0/pitch0（视线 y46.62 恒切 y46 格行）+ 目标 (22,46,-5)（1.5 块
  距离）+ 石地板 y44 + 头部清空 y45-47。**按住 0.85 s**（窗口 ∈
  (0.75, 7.5)）：OLD 侧 0.75 s 破 + 续按 0.1 s < 后方空气无后续 +
  wood×1 掉落静止 (22.5,45,-4.5)（距玩家 3.0 > kPickupDistance 1.0 不
  拾取）；NEW 侧 7.5 s ≫ 0.85 s 存活 + 无掉落。**终态互斥**（块在/
  不在 + 掉落在/不在），×2/侧 RAW + 存档断言 + old-vs-new 截图 diff
  计数记录。
- **S2b 对照（铁镐 × 石，on-list = 零回归）**：同几何，石目标（掉
  cobblestone = P61 内容）。按住 0.42 s（> 0.28125 s 双侧同破 + 续按
  0.14 s < 0.28125 s 无后续）：**四跑（old×2 + new×2）RAW 全字节一致
  + 跨侧 savenorm EXACT** = on-list 路径位级不变实证（双向无泄漏）。
- **S3 = 套件 1828**。
- **无 S6**：零 parse 面（规则 = 核心命名常量，数据模型零变更）。
- **输入协议坑（门内实证）**：捕获点击的 grab delta = 相机终漂移
  （yaw −0.0175 / pitch −0.1675，PITS 28 族）——**批内完全可复现**
  （同窗几何 → 同 delta；冒烟 3 跑终相机逐值相同 + 2 跑截图字节一致）
  → 脚本加**相机基线守卫**（首个成功输入跑设基线，终相机不一致的跑
  重试）= RAW/savenorm 比较的前置条件。

## 性能 A/B（发布前必做）

tm_v118_perf（banner 实证 v1.18.0）vs tm_v119_perf（**全新 rsync 树
（不继承 build/ = PITS 306）+ 插桩 fluid.cpp 移植 + step0 patch +
hunk#6 手工 + 全新 cmake 配置**；banner 实证 v1.19.0），同 seed fall
900f：规则 = effectiveBreakSpeed 一个分支（无输入永不执行 = 结构惰性）
⇒ **PASS = 成本可忽略（结构性惰性实证）**：fluidCells win1
51926/51928（±2 fill 窗 real-dt 类）+ **win2 29803 = 29803 逐位 +
win3 29800 = 29800 逐位** + PIPE3 changesApplied 1404 = 1404 逐位
（fluidCellsSeen 43994/43048 = −2.2% 窗相位类，fluid 代码两树位级
相同 = 噪声证）+ visitedChunks 268/266 同档 + upd/render/swap 全
同档（swap win3 max 11.3/28.0 = 单发环境尖峰，med 同档）。无输入
fall 场景永不执行 break 通道 = 规则结构惰性。

## 坑（门内）

- **输入跑捕获点击的 grab delta = 终相机漂移，批内可复现（PITS 309）**：
  左键捕获点击使 SDL 相对模式初始化时记入一次性 delta（冒烟实测
  yaw −0.0175 / pitch −0.1675 = 9.6° 俯），终截图/存档的相机因此偏离
  场景值——同窗几何下**完全可复现**（3 冒烟跑终相机逐值相同 + 2 跑
  截图字节一致），故门脚本加**相机基线守卫**（首个成功输入跑设基线，
  终相机不一致的跑重试）= 批内 RAW/savenorm 可比的前置；跨侧
  （old vs new）同窗几何同 warp → 同 delta → S2b 四跑 RAW 全字节
  一致达成。
- **自然区 real-dt churn 裁定（PITS 244/283 类，非回归）**：S2a
  old×2 存档 blocks 差 3 格（(87,35,80) 族，**距场景 107 块**，同
  二进制自分歧）+ S2b new×2 fluids 差 1 格（x73-81 自然湖区）+
  S2b 跨侧 fluids 1 格同签名 = 环境性 churn（场景区 target/drop/
  player/slot 全 EXACT + S2b 四跑 RAW 全字节一致 = 零回归判据不
  受影响）。
- **0.1f 非二进制精确**：breakDurationSeconds 的除法带 ~1e-7 浮点残
  差，EXPECT_DOUBLE_EQ 4-ULP 容差必炸 → 惩罚路径的 exact-time 断言用
  EXPECT_NEAR 1e-6（注释说明），on-list 二进制精确值保持
  EXPECT_DOUBLE_EQ。
- **v119 性能树后台构建无输出早死**（`set -e` + heredoc 步，与上轮
  v118 同款）→ 手工补 hunk#6 + 构建（markers 15 + banner 实证）。
- 冒烟跑间 1 帧环境性 WM 差异（8px 黑条类）= 既有重试覆盖。

## 发布

v1.19.0 = 新组 1.16-1.20 第 4 版（组内不打 tag；组末 v1.20.0 → tag +
GitHub release）。正确性 A/B 旧侧 = v1.18.0 = 71dfc06（tm_v118_src）。
