# Phase 43 细节存档

> 用户确认方向（v1 语义 = 「容器框架 + 清红闪/受伤计时」——不先做状态效果
> 系统，`clearStatusEffects()` 留扩展点）。
> 范围 = P23/P28 遗留「牛奶桶（无实体容器物品）」：`bucket`/`milk_bucket`
> 物品 + 3 铁锭配方 + 可饮用容器框架（`drink_time`/`returns_item`）+ cow
> 挤奶（`milk_with`/`milk_gives`）+ "milk" 音效 + 2 图标。
> 无存档格式变更（format 6，物品按名存）。

## 阶段报告

- **`[2026-10-12, Phase 43]` 新食物剩余：牛奶桶最小项**
  — 781/781（+12），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 17 次结构性
  免疫免重锚 + S2 跨构建存档 STRICT 一致 + S3/S4 输入运行存档精确 +
  像素 0 bad）。

### 实现内容

**物品字段（`item/item.hpp` + `item/item_registry.{hpp,cpp}`）**
- `drinkTime`（JSON `drink_time`，秒 (0,10]）：present = 可饮用容器，
  饮用通道时长**数据驱动**（进食状态机泛化的 duration）。显式 0 拒绝 =
  `restore_hunger` 纪律（缺省字段 = 不可饮用，显式 0 = 手滑）。
- `returnsItem`（JSON `returns_item`，非空物品名）：喝完返还的容器；
  **要求 `drink_time` 存在**（无饮用时长的 returns = 死字段，成对纪律）；
  显式空串/非字符串拒绝（ammo 纪律）；init 对 ItemRegistry 交叉校验
  存在（ammo/smelt 模式，容器可能定义在后面的文件）。
- **互斥**：`restore_hunger > 0` ∧ `drink_time > 0` 拒绝——同一右键按住
  通道，一个物品不能既是食物又是可饮用。
- `parseDrink` 在 `registerItem` 中于 `parseFood` 之后调用（互斥检查需要
  已解析的 restoreHunger）；未知键列表加两键。

**进食通道泛化（`core/food.hpp`）**
- `EatChannel::start(double duration = kEatDurationSeconds)` + 新成员
  `total`；`progress()` 除以**通道自身** `total`（数据驱动时长按自身归一）。
  `reset()` 不动 total（start 必设 > 0，progress 有 active 守卫，无除零）。

**mob 字段（`entity/mob_registry.{hpp,cpp}`）**
- `MobProperties::milkWith`/`milkGives`（JSON `milk_with`/`milk_gives`）：
  必须**成对**（只给一边 = 报错，dye_base/dye_color 纪律）、非空字符串
  （ammo 纪律）；init 交叉校验两物品存在。
- `assets/data/mobs/cow.json` 加 `"milk_with": "bucket"`,
  `"milk_gives": "milk_bucket"`。

**数据（`assets/data/`）**
- `items/bucket.json`：material，max_stack 16（MC 值），icon bucket。
- `items/milk_bucket.json`：material，max_stack 1，icon milk_bucket，
  `drink_time 1.6`（MC 牛奶时长），`returns_item "bucket"`。
- `recipes/bucket.json`：shaped `I I`/` I `（I = iron_ingot）→ bucket ×1
  （MC 配方；2 行 pattern 已有先例 arrow.json）。
- 新物品**不进**默认 hotbar / 世界生成（结构性免疫的前提）。

**声音（`tools/generate_sounds.py` → `assets/sounds/milk.wav`）**
- `milk()`：~0.28s 液体咕嘟——双脉冲门控（sin²³ 3.5Hz gate）× 下扫气泡
  sine（150→100Hz chirp）+ lowpass 噪声水花，`Lcg(0x0CC0)` 确定性；
  饮用开始与挤奶共用。既有 WAV 重跑字节零漂移（确定性回归 ✓）。

**图标（`tools/generate_item_icons.py` → 2 PNG）**
- `bucket(milk=False/True, salt=55/57)`：铁灰梯形桶（顶宽 20 底宽 12，
  rim 亮带）+ 牛奶 = 开口内白色椭圆面；splitmix64 确定性。既有图标重跑
  字节零漂移 ✓。

**Game 逻辑（`core/game.{cpp,hpp}`）**
- `tryDrink()`：右键序 tryEat 之后。选中 `drinkTime > 0` 物品 → 点击被吞；
  `returns_item` 非空时先 `isFull(returns)` 探针（count-1 可加语义）——
  满背包 = no-op 但仍吞点击（MC 一致：空桶没处放时喝不了）；
  通道激活中再按 = 忽略（饮用继续）；`start(drinkTime)` 武装共享通道 +
  **开始时**播 "milk"（MC 一致：咕嘟声在前，取消也响过——同弓弦声）。
- `updateEatChannel` 完成分支：`restoreHunger > 0` = 原进食路径（回复
  饥饿 + 2× 饱和度 + "eat"）；否则 = 饮用路径：`take` 1 + `add(returns, 1)`
  （开始已探明可加，add 不会失败）+ `clearStatusEffects()` +
  `MC_LOG_DEBUG("drank ...")`。
- `tryMilk()`：右键序 launcher 之后、方块 interact 之前。`targetMob()`
  （复用近战目标：kReach 5.0 + 方块遮挡）+ `props.milkWith == 选中物品名`
  → 满背包（`isFull(milkGives)`）= 不产出但吞点击；否则 `take` 1 手持 +
  `add` 1 gives + "milk" 音效 + debug 日志；**mob 零影响**（无伤害/死亡/
  掉落/冷却——v1 规则，MC 5 分钟挤奶冷却分歧已文档化）。
- `clearStatusEffects()`：v1 效果集 = `m_hurtFlash = 0.0`（清红色受击
  纱幕）；**`fallStartY` 刻意不清**（MC：坠落伤害不是状态效果——空中喝奶
  不减坠落伤害）。状态效果系统不存在，此为扩展点。

### 变更文件

- `src/item/item.hpp`（drinkTime/returnsItem 字段）
- `src/item/item_registry.hpp` / `src/item/item_registry.cpp`（parseDrink +
  互斥 + 未知键列表 + registerItem 调用序）
- `src/core/food.hpp`（`start(duration)` + `total` + progress 归一）
- `src/entity/mob_registry.hpp` / `src/entity/mob_registry.cpp`
  （milkWith/milkGives 成对校验）
- `src/core/game.hpp` / `src/core/game.cpp`（tryDrink/tryMilk/
  clearStatusEffects + 右键序 + init 交叉校验 ×2 + 完成分支 + 2 日志行）
- `assets/data/items/bucket.json`（新）/ `assets/data/items/milk_bucket.json`
  （新）/ `assets/data/recipes/bucket.json`（新）
- `assets/data/mobs/cow.json`（+2 字段）
- `tools/generate_sounds.py`（milk()）/ `assets/sounds/milk.wav`（新）
- `tools/generate_item_icons.py`（bucket 两图标）/
  `assets/textures/items/bucket.png`（新）/ `milk_bucket.png`（新）
- `tests/test_item.cpp`（+6）/ `tests/test_mob.cpp`（+2，cow 定义加 milk
  字段）/ `tests/test_food.cpp`（+4）
- 文档：KNOWN_ISSUES（+「牛奶桶 / 饮用」节 5 条 + 食物节划掉牛奶桶）/
  MECHANICS（+「饮用 / 挤奶」节 + 右键序 + 音效 13→15）/ GAMEPLAY
  （+「牛奶」节）/ CONTENT_GUIDE（物品模板 +2 字段 + mob 挤奶字段）/
  PITS（+5）/ ROADMAP（P43 行 + 索引）/ AGENTS（现状/781）

### 测试

781/781（+12），`-Wall -Wextra -Wpedantic -Werror` 0 警告。
- **test_item +6**：ParsesDrinkableFields（1.6 + returns bucket）/
  DrinkableDefaultsToAbsent / DrinkTimeWithoutReturnsIsLegal（returns 可选）
  / RejectsBadDrinkTime（0/−1/11/非数值）/ RejectsBadReturnsItem（空串/
  非字符串/无 drink_time 死字段）/ FoodAndDrinkableAreMutuallyExclusive。
- **test_mob +2**：MilkingPairAbsentByDefault（zombie）/
  RejectsBadMilkingPair（单字段 ×2/显式空串/非字符串，锚点插入法）。
- **test_food +4**：CustomDurationRunsAtItsOwnClock（0.5s 通道第 **31** 帧
  完成——IEEE 30×(1/60) 累加略短于 double 0.5，97 帧现象同款，钉帧 +
  注释）/ CustomDurationProgressNormalizesByOwnTotal（0.25s/0.5s ≈ 0.5）/
  CustomDurationEarlyReleaseStillCancels / DefaultStartIsStillTheEatDuration。

### 构建与验证（E2E）

批脚本 `/tmp/opencode/p43_e2e.zsh`（一次性），零并发 :0 活动。

- **S1 base18 门**：P43 ×2 + P42 代码参考构建（worktree @9628583，
  **直接加载 P43 资产**——旧解析器未知 item 键警告忽略、未知 mob property
  键静默忽略、配方输出 "bucket" 因 bucket.json 被旧解析器当普通 material
  注册而通过交叉校验 → own-cwd 规则**不适用**，PITS 新坑）×1，新世界
  60f 无输入 → **三方字节一致 + base18 字节一致 = 第 17 次结构性免疫
  （无重锚）** + skygate (120,181,242)。
- **S2 回归 A/B**（p27far 夜 360f seed 9）：P43 ×2 存档字节一致（确定性
  权威）+ **P42 ref 存档跨构建 STRICT 字节一致**（无 format 升格 + P43
  不改箭/mob/生存行为 → 无容忍分支）+ 截图 ×2（P43 ×2 字节一致 / vs ref
  veil 容差 100% 像素 ≤1 通道 = real-dt 红纱边界同 P27 签名）+ health 8
  + 出生序列。
- **S3 输入运行「挤奶」**（正午 mobs on、seed 9、**600f**）：源存档 =
  p27far 派生（玩家 **[22.5,45,−1.0]**【f32 精确值，探查验证 EXACT】，
  列 (22,−1) 与规范出生同锚点、地面同高 → 站桩零行走；slot0 bucket×1；
  无块）。**探查定几何**（必须用场景真实玩家 pos——规范出生点探查的牛位
  差一整格 = PITS 新坑）：f60 牛 (22.5,46,−6.5) 近面 t=4.9 < kReach 5.0
  （玩家 z −0.25 时 t=5.65 **够不着**，列内移到 −1.0 拉回）；f120 牛
  (19.5,46,−5.5) x 18.9..20.1 / f210 牛 (21.5,46,−5.5) x 20.9..22.1
  永不交叉 x=22.5 射线；f240 牛 (22.5,46,−5.5) t=3.9 为后备目标
  （targetMob 最近命中自动跳过超距牛）→ **有效挤奶窗口 f60–f419**
  （牛出生→首 wander 完全静止 = wander_interval 3.0s = 180 帧）。输入：
  锁指针 → **0.35s 间隔 ×24 右键序列**（本机 ~40fps = wall ≈ 1.5× 游戏
  秒；单次点击落帧随加载时长漂移 ±0.3s+ 会错过窗口且**无报错**——窗口前
  点击 = 安全空操作（无 mob 射程内 + 射线无块 + 材料不可放置），挤后点击
  = 饮用通道 1 帧 start+cancel 不消耗）：存档**精确**（slot0
  bucket→milk_bucket、cow 零掉落【其他槽全空】、pos **精确**、yaw/pitch
  零漂移、20/20）+ 日志 `milked mob 'cow' (bucket -> milk_bucket)` ×1 +
  像素 vs 无输入对照（同 4 牛同位置 = AI 纯帧函数，diff 取消）：38,164
  diff = 手部 8,375（milk_bucket 图标 quad）+ HUD 29,749 + 准星 40 +
  **0 bad**。
- **S4 输入运行「饮用」**（正午 mobs off、seed 9、300f）：源存档 slot0
  milk_bucket×1、无块（水平射线 y 46.62 越平坦原 → 无目标块 outline）。
  输入：锁 → 右键按住 1.8s（1.6s 数据驱动时长 + 0.2s 余量）→ 释放：
  存档**精确**（slot0 milk_bucket→bucket【take 腾空位 + add 落首空位 =
  同槽】、pos 精确、yaw/pitch 零、20/20）+ 日志
  `drank 'milk_bucket' (returned 'bucket')` ×1 + 像素：38,087 diff =
  手部 8,334（bucket 图标 quad）+ HUD 29,713 + 准星 40 + **0 bad**。
  （红闪清除 = v1 效果集钩子：m_hurtFlash 不入存档，像素时序脆弱不做，
  由代码路径 + 通道单测承担。）

### 已知问题（→ KNOWN_ISSUES「牛奶桶 / 饮用」）

- 状态效果系统不存在：喝奶效果集 = 清红闪一项（扩展点已留；坠落起点刻意
  不清 = MC 一致）。
- 喝奶不恢复饥饿（MC 牛奶值 = 0）。
- 挤奶无冷却、mob 零影响（MC 5 分钟冷却分歧）。
- 唯一可饮用 = 牛奶桶（无水瓶/熔岩桶；`returns_item` 框架支持未来容器）。
- "milk" 音效在饮用开始时播放（MC 一致，取消也响过）。

### PITS 新增（5 条）

1. 存档 pos 是 float32——EXACT 断言只许用二进制精确值（−0.3 →
   −0.30000001192092896 静默失败；用 −1.0/−0.25 类 k/2^m 值，定 pos 前
   先探查读回验证）。
2. mob 生成环锚定玩家列——探查必须用场景真实玩家 pos（列 (22,0) 探查的
   牛位 ≠ 列 (22,−1) 场景的牛位，差一整格把 t 从 4.65 推到 5.65 出
   kReach）。
3. ~40fps 下固定 sleep 单次输入会错过确定性窗口且无报错——0.35s 间隔
   点击序列（25–60fps 全覆盖）+ 窗口前点击安全空操作 + 动作后点击无害
   的两个设计前提。
4. `isFull` = "count-1 可加"探针（无空格 ∧ 无未满同物堆）= add(1) 必然
   成功的精确判据；勿写"有空格"版（漏并入堆，max_stack > 1 容器会错）。
5. 跨构建 A/B 的 ref 不必然要自己 cwd（P39 规则边界）：旧解析器未知键
   容忍 + 新物品文件被旧代码当普通 material 注册时，新数据目录可被旧
   二进制加载——跑批前先想一遍新数据在旧解析器下的命运。

### 下一步建议

- **状态效果系统**（自然延伸：`clearStatusEffects()` 扩展点已留——中毒/
  虚弱/再生 + 数据驱动效果字段 + 效果图标 HUD；喝奶的 MC 语义即「清除
  负面效果」才完整）。
- 其他新食物剩余：面包（需小麦/种植系统）、金苹果（需苹果来源）——均为
  独立系统，非最小项。
- 流体桶（水/熔岩桶：`bucket` 框架已在，需流体物品设计）。
