# PRE-7 中毒/燃烧表现（v1.x 矩阵 28/33，v1.28.0，门 143，PRE 行 1/6）

## 定案

原始定义已佚 = 定案 **全屏效果 tint 通道（数据驱动）+ poison 紫色纱**
（消费 KNOWN_ISSUES 状态效果节「中毒无独立紫色 tint」条——原文即点名
"需新的全屏 tint 通道"）：

- **效果 JSON 可选 `tint`**：4 数组 `[r,g,b,a]` 全 [0,1]（严格解析：
  非数组/长度≠4/非数字/越界逐条点名 = ammo 纪律；缺省 = 全零 = 无纱
  零绘制 = 结构免疫根）
- **no-op 规则演进**：正 alpha tint = 表现层动作，满足"效果必须做点
  什么"（纯装饰纱效果合法；alpha 0 = 仍 no-op 拒）
- **内容**：`poison.json` + `tint [0.63, 0.24, 0.86, 0.35]`（MC 中毒紫
  ≈ RGB 160/60/220，alpha 0.35）；其余 10 效果无 tint（零纱）
- **渲染**：HUD pass 末（hurt flash 之前）每**活跃玩家** tinted 效果
  一个全屏 quad（`activeEffectTints` 纯函数收集，StatusEffects 确定性
  应用序）；恒定 alpha（MC 中毒纱脉动 = 记档偏离）
- **燃烧半 = 边界记档**：玩家燃烧表现 = 既有红纱/0.2 s tick（P57/
  hurtFlash）不变；fire 效果无 tint；**mob 火焰视觉 = PRE-8 独立项**
  （矩阵行名"中毒/燃烧"的燃烧半在 PRE-8 兑现）
- 零 format（22）/零 config/零音效/零世界状态

## 实现

- `status_effect.hpp`：`Effect::tintR/G/B/A` + `EffectTint` + 纯函数
  `activeEffectTints(StatusEffects, EffectRegistry)`
- `status_effect.cpp`：`tint` 白名单 + 严格解析 + no-op 规则演进 +
  纯函数实现（alpha ≤ 0 跳过）
- `game.cpp`：HUD pass 红纱前 veil 循环（零活跃 tint = 零 quad）
- `assets/data/effects/poison.json`：+tint（唯一内容变更）

## 测试

**1888/1888（+6 `EffectParse.TintValid/TintAbsentIsTheZeroVeil/
TintRejections/TintAlphaAloneIsNotNoOp` + `StatusEffects.
ActiveEffectTintsOnlyForTintedEffects` + `EffectContent.PoisonTintPinned`
【11 效果逐名钉：poison 紫 / 其余全零】**）**，`-Werror` 0 警告，
banner v1.28.0。

## 门 143

**S1 PASS = 第 143 次结构性免疫 + base18 锚连续**（387726 B 1362×744
免重锚）：quiet config fresh 60f 无输入四方 ×2（new v1.28.0-dev ×2 / old
v1.27.0 tm_v127_src ×2）RAW 全 EXACT + **new≡old**（无效果 = 零 tint =
零新 quad；poison.json 内容 init-only）+ 同侧 savenorm EXACT ×2 +
**跨版 savenorm EXACT**（零 format 变更，两侧恒写 22）。

**S2 PASS = 三方 poison 纱 A/B（混合数学，f300 场景 = fresh 60f 基降
format 21【旧可读】+ effects 注入；config nomobs+spread0+regen/nodecay
9999）**：
- B twin（new + effects []）×2 **RAW 387726 B 字节一致**（静态对照）
- A（new + poison remaining 60）：**blend residual 0.22/255**（断言
  A ≈ 0.65·B + 0.35·[160,60,220]，世界区 y[0,640) 2×2 采样 = **紫色纱
  按设计精确渲染**；首帧即过 = 终帧不撞红闪相位）+ 存档 effects
  {poison, 53.19}（<60 = 毒在跑）
- C（old v1.27.0 + poison）：**blend residual 33.9** = 无纱（旧侧无
  tint 通道；A vs C 150× 分离 = 判别成立）
- 燃烧半 = 边界记档（fire 无 tint，玩家燃烧 = 既有 0.2 s tick 红纱
  不变；mob 火焰 = PRE-8）

**S6 PASS**：scratch 资产 poison tint 破 3 元素 → init rc1 点名
`effect 'poison': 'tint' must be an array of 4 numbers [r, g, b, a] in
[0, 1]`。

**S3 PASS**：套件 1888/1888。

**性能 A/B PASS**（真 v1.27.0 tm_v127_perf vs v1.28.0-dev tm_v128_perf，
banner 实证，同款 g139 插桩两侧同打 27 标记，fresh --new --seed 123456789
900f 三窗 + 终档集合）：PERF-WIN 全指标同档（upd med 0.391/0.362/0.369
vs 0.388/0.385/0.408 ms【win3 +11% = 窗口噪声级】/ render med 0.52-0.58
vs 0.61 / swap med 15.51-15.62 vs 15.51-15.55 vsync 锁定 / fluid
0.043-0.051 vs 0.048-0.055 / fires 0.007-0.009 vs 0.008-0.011 / spread
0.0005-0.0006 / ents 0.0032-0.0034 vs 0.0035-0.0039 / stream
0.013-0.015 vs 0.015）+ PIPE3-S0 tickMs med 0.043-0.052 vs 0.049-0.055
同档 + **changesApplied max 9/3/1 三窗逐位一致** + **终档 fluids 18/18 +
blocks 55/55 集合恒等（diff 0）= 同世界同成本**【结构惰性最强形：无输入
fresh 跑零活跃效果 = 零 tint quad，poison.json 内容 init-only】= 成本可
忽略，零回归。

## PITS

- **PITS 322（门脚本场景 cp 双层 .json 后缀第 6 次 + scene-verify 须
  hard-fail）**——g143 run_scene 的 `$scene` 参已含 .json，函数内又拼
  `$scene.json` → cp 静默失败 → 场景校验 python FileNotFoundError 后
  **脚本继续**（无 set -e）→ `--save` 空槽 = fresh 世界 → 全场景跑出生
  点：B twin ×2 RAW 假 PASS（两侧同 fresh）、A blend 恒 33.9 不收敛空耗
  8×2 次重试。修 = `cp $S/$scene` + scene-verify 失败 `exit 1` + cp
  `|| exit 1`（PITS 294/305 同款；教训 = 场景装载的任何一步失败必须
  杀死整跑，静默继续 = 假绿温床）。
- **PITS 323（判别阈值须对实测基线校准，勿猜）**——S2 C 侧"无纱"阈值
  初设 ≥40（凭 A 侧 0.22 外推），实测无纱残差 33.9 → 假 FAIL 一次。
  修 = 阈值取**两实测值之间**（A 0.22 有纱 / C 33.9 无纱 → ≤8 判有纱、
  ≥10 判无纱，双裕量）；凡像素混合数学判别，先跑两个对照把基线测出来
  再定阈值（本门 A/C 即基线，C 的 33.9 同时是"无纱残差"的标定值）。
