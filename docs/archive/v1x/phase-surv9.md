# SURV-9 饱和细化（v1.x 20/33，v1.20.0，门 135，SURV 行 4/5，组末版）

v1.x 任务矩阵第二十项。原始定义已佚 = 代理定案最小完整版：**饱和度模
型细化的两个 MC 要素**（消费 KNOWN_ISSUES Phase-28 饱和度条"未建模"
清单的 ② 生肉低饱和度 + ③ 回复不消耗饱食度；① exhaustion 子点 =
Phase 31 节奏制设计选择，保持记档偏离——本相不动节奏体系）。

## 定案

- **② 逐食物 `saturation` 数据字段**（item JSON 可选，MC 精确值）：
  严格 number [0, 20]（显式 0 合法 = 不加饱食度；>20 = 越 buffer 上限
  点名）+ **配对纪律**（须有 restore_hunger = 食物，非食物带字段 =
  死字段点名，fast_against 同款）+ **缺省 = 旧 2× 规则位级复现**
  （`foodSatiety` 纯函数 = field ? field : restore_hunger × kSaturation
  Factor）。内容 = **10 食物全写 MC 值**（rotten 0.8 / 生 beef 1.8 /
  mutton 1.2 / chicken 1.2 = 生肉低饱和度即玩法点 / 熟 beef 12.8 /
  mutton 7.2 / chicken 7.2 / bread 6.0 / apple 2.4 / golden 9.0）。
- **③ 自然回复半心消耗 1 点饥饿**（MC 规则）：`Player::drainOneHunger
  Point()` 私有原语（buffer 先扣、耗尽掉条 = Phase 28 衰减 tick 的同
  一路径上提）——**decay tick 与回复成本共用**（decay 位级恒等重构）+
  **满血时区间免费流逝**（无回复 = 无成本，既有 clamp 语义保持）。
  回复门（hunger ≥ 6，bar 计）不变 = Phase 23 记档保持（MC 18 = 既有
  偏离，非本相面）。
- 零 format（5 的 saturation 字段既有，值域不变）/零 config/零纹理。

## 实现

`src/item/item.hpp`（Item::saturation optional 字段）、
`src/item/item_registry.cpp`（parseFood 加 saturation 严格块）、
`src/core/food.hpp`（foodSatiety 纯函数 + item.hpp include）、
`src/core/game.cpp`（进食点改用 foodSatiety）、`src/player/player.
{hpp,cpp}`（drainOneHungerPoint + 回复成本 + decay 重构）、
`assets/data/items/` 10 食物 JSON（+saturation 行）。

## 测试（1835/1835，+7 净，-Werror 0 警告）

- test_item +4：ParsesSaturationField（0.8/显式 0/20 上限）/
  RejectsBadSaturation（非食物配对/负/越界/非数字 4 形）/
  FoodSatietyFieldElseLegacy（字段优先 + 缺省 2× 位级）/
  Surv9FoodSaturationContent（真资产 10 食物 MC 值逐一点名）。
- test_player +3 新 + 1 扩展：RegenCostsSaturationBeforeTheBar（buffer
  先扣 bar 不动）/ RegenAtFullHealthCostsNothing（满血区间免费）/
  RegenGateClosesWhenTheCostDrainsTheBar（成本把 bar 打到门 6 以下 =
  门关，MC 语义）/ RegenAfterDelay 扩展（成本断言：19→20 一次成本
  bar 20→19，第二次满血免费 = 1 个 half heart 一个成本的精确性）。
- 1/60 双累加边界：14 s 恰好 = 840 帧累加略短于 4.0 → 新测用 15 s
  带 1 s 裕量（协议 tick 类断言裕量纪律）。

## E2E 门 135

- **S1 = 第 135 次结构性免疫免重锚**：fresh 60f 四方 ×2（new v1.20.0 /
  old v1.19.0 = 5725bce，g118_nomobs）+ base18 + savenorm（无进食无
  伤害无回复 + 60f < 36 s 无衰减 = 饱和度零触碰；内容 = JSON 数字，
  零纹理）。
- **S2a 回复成本终态判别**（s135_regen 密封房：hp 10 / bar 20 /
  buffer 6，**g135_nodecay config** = hunger_decay_seconds 9999【g95
  先例】消 D 变量，3300f = 53-83 s 跨 fps 全 ≥ 50 s = 恰 10 次回复
  到满）：NEW 终态 **hp 20 / sat 0 / bar 16 exact**（10 成本 = buffer
  6 + bar 4）vs OLD 终态 **hp 20 / sat 6 / bar 20 exact**（零成本）=
  **精确终态互斥，fps 无关**；×2/侧 RAW。
- **S2b 进食饱食度判别**（s135_eat 密封房：hunger 14 / buffer 0 /
  slot0 rotten_flesh×1，900f + 捕获 + 右键按住 2.0 s【1.6 s 通道完
  成】，g135_nodecay）：两侧 slot 空 + hunger 18 exact；NEW **sat
  0.8 exact**（MC rotten flesh 值）vs OLD **sat 8.0 exact**（2× 规
  则）= 精确互斥；×2/侧 RAW（相机基线守卫 PITS 309）。
- **S2c 空闲对照**（s135_idle fresh 值，300f 无输入 nodecay）：四跑
  RAW 全字节一致 + 跨侧 savenorm EXACT = 零回归。
- **S6 ×3 点名**（临时资产改 + git checkout 还原）：stick + saturation
  （"requires 'restore_hunger'" 配对）/ beef saturation −0.5（"must be
  in [0, 20]"）/ bread saturation "high"（"must be a number"）。
- **S3 = 套件 1835**。

## 性能 A/B（发布前必做，组末版）

tm_v119_perf（banner 实证 v1.19.0）vs tm_v120_perf（**全新 rsync 树 +
插桩 fluid.cpp 移植 + step0 patch + hunk#6 手工 + 全新 cmake 配置** =
PITS 306；banner 实证 v1.20.0），同 seed fall 900f：规则 = 回复成本
一个 float 减 + 进食通道一个函数调用（fall 场景零进食零回复成本 =
结构惰性）⇒ **PASS = 成本可忽略（结构性惰性实证）**：fluidCells win1
51927/51928（±1 fill 窗 real-dt 类）+ **win2 29803 = 29803 逐位 +
win3 29800 = 29800 逐位** + PIPE3 changesApplied 1404 = 1404 逐位
（fluidCellsSeen 43996/43048 = −2.2% 窗相位类，与门 134 对同值 =
流体代码两树位级相同噪声证）+ visitedChunks 268/266 同档 +
upd/render/swap 全同档（swap win3 max 13.4/27.8 = 单发环境尖峰，med
同档）。fall 场景零进食 + 回复成本 = 一个 float 减（零流体/渲染成
本）。

## 坑（门内）

- **zsh 脚本无 `!` 管道前缀（PITS 310）**：`! grep ...` = bash 习
  惯，zsh 脚本里 = 语法错且 **`zsh -n` 零诊断输出**（静默 RC=1）→
  head -n 二分定位；解法 = `grep -qv` 或 `if !`。
- **fps 随场景变化（PITS 311）**：密封房场景 50-60 fps vs fresh
  spawn 13-31 fps（渲染负载不同）——real-dt 终态设计不能假设全局
  fps 带；首版 1600f band 设计冒烟即失效（hp 14 非 20 = 4 次回复
  非 10 次），改 nodecay config + 3300f 精确终态。
- **后台复合命令的 heredoc/python 步静默死**（PITS 312，v118/v119/
  v120 性能树三连）：`set -e` + 后台 job + heredoc python = 失败无
  输出早死；解法 = 关键步前台跑或用脚本文件（-c 多行串也易坏）。

## 发布

v1.20.0 = **新组 1.16-1.20 组末 → tag v1.20.0 + GitHub release**
（notes 覆盖 v1.16-1.20：ENT-9 / SURV-6 / SURV-7 / SURV-8 / SURV-9
五版）。正确性 A/B 旧侧 = v1.19.0 = 5725bce（tm_v119_src）。
