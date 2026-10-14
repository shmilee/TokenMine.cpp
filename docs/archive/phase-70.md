# Phase 70 细节存档

> 范围（用户"按建议，继续下两个 Phase" = 建议 2，代理定案为最小
> 完整版：**玩家火接触伤害**——玩家 AABB 重叠 fire 块格时受伤，
> 消费火 v2 条"无接触伤害"遗留；**mob 火接触 = 文档化偏离**
> 【v3 范围：P58 熔岩点燃 mob 已有，直接站火 = 后续】）：
> config 2 键 `player.fire_damage`（默认 1，0 = 关）+
> `player.fire_damage_interval_seconds`（默认 0.2）+
> `Entity::overlapsBlock` + Player `isInFire`/`m_fireAccum`。
> **无 format 变更**（format 10）。

## 阶段报告

- **`[2026-10-14, Phase 70]`** 火接触伤害 — 1072/1072（+9），
  `-Werror` 恒 0 警告；E2E 批跑全绿（全 no-input：S1 第 43 次结构性
  免疫【无重锚】+ S2 玩家烧死【4.0 s，×2 savenorm】+ S2b off-switch
  存活【×2 RAW 存档+截图】+ S6 config 失败 ×2）。

### 实现内容

**核心（P54 熔岩灼烧的块版）**：
- `src/entity/entity.{hpp,cpp}`：**`Entity::overlapsBlock(BlockID)`**
  ——`overlapsFluid` 的块格孪生：同款 AABB、同款逐轴严格不等式
  （面恰好贴格边界 = 不重叠）、caller 传 id（方法从不点名方块 =
  数据驱动）；invalid id = 永不。
- `src/player/player.{hpp,cpp}`：
  - `Params`：`fireDamage = 1.0f`（**0 = 关**，`≤ maxHealth` 交叉
    校验，mirror `lavaDamage`）+ `fireDamageIntervalSeconds = 0.2`
    （>0 硬校验，mirror `lavaDamageIntervalSeconds`）。
  - 构造时 `m_fireBlockId(blocks.idForName("fire"))` 解析一次
    （registry 无 fire 块 = invalid = `isInFire` 恒 false——
    fluid.cpp:529 "回落 air"同款容错，单测 fixture 语义）。
  - `Player::update`：`m_inFire = isInFire()` 每帧算 + **`m_fireAccum`
    real-dt 计时器** mirror 熔岩块：在火里累积、跨 interval 走正常
    `damage()` 路径（红闪/音效/死亡处理全触发）、离开重置未到期
    tick、`fireDamage 0` fast-exit。
- `fromConfig`：两键读取（`fire_damage` 允许 0 = 唯一新增的"0 合法"
  伤害键）+ `fire_damage ≤ max_health` 交叉校验。

### 变更文件

- 改：`src/entity/entity.{hpp,cpp}`（`overlapsBlock` 声明+实现）
- 改：`src/player/player.{hpp,cpp}`（Params 2 字段 + `m_inFire`/
  `m_fireAccum`/`m_fireBlockId` 成员 + `isInFire`/`inFire` + update
  计时器块 + fromConfig 两键+校验+文档注释）
- 改：`tests/test_player.cpp`（fixture 加 fire 块 + 9 测试：行为
  5 + config 4）
- 文档：ROADMAP、KNOWN_ISSUES（火 v2 条 + 伤害来源条演进）、
  MECHANICS（火节加接触伤害条）、GAMEPLAY（火条目）、AGENTS.md、
  本存档、SESSION_CONTEXT.md（本地）

### 测试

**1072/1072（+9）**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告：
- `test_player.cpp`（fixture 加非 solid `fire` 块）：
  - FireBurnsAtTheConfiguredRate——站火 13 帧 = 1 tick = 19 HP
    （**1/60 double 累加跨 0.2 在第 13 帧** = P58 off-by-one 教训，
    断言用 13 不用标称 12）。
  - FireBurnKills——250 帧（第 20 tick ≈ 帧 241）→ 死亡 health 0。
  - FireBurnResetsOnExit——tick 后离开 1 s 无伤、回火**重新**计 0.2 s
    （11 帧不 tick、第 13 帧 tick = 离开真的重置了相位）。
  - FireDamageZeroDisables——off-switch：250 帧站火 health 20 不死
    （inFire 状态仍计算 = 关的是伤害不是检测）。
  - FireFaceTouchIsNotOverlap——AABB max.x 恰贴格面（7.7+0.3=8.0）
    = 不重叠、内移 0.1 = 重叠（严格不等式约定）。
  - config ×4：默认 1.0/0.2 / interval 0 点名拒 / fire_damage 0 合法
    （+ max 合法）/ fire_damage 21 点名拒（mirror P54 熔岩两测）。

### 构建与验证结果

- 构建 0 警告；测试 1072/1072。
- E2E（一次全绿；**全 no-input**——场景存档把火放在玩家脚下）：
  - **S1 第 43 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节
    一致（P70 = 2 个 config 键默认值 + 只在 AABB 重叠 fire 块时运行
    的接触路径；fresh 世界零 fire 块【worldgen 永不生成，P64】= 路
    径惰性 = P58 同款论证；无 block/atlas 变更 = 无重锚）。
  - **S2 玩家烧死**（600f no input，×2 savenorm）：场景 = fire 放
    (22,45,0)【玩家脚边生成 AIR 格，玩家 AABB x 22.2..22.8 / y
    45..46.8 / z 0.2..0.8 严格重叠】+ fires 条目 remaining 8.0 +
    `fire.spread_chance 0` config【周边草 flamable，0 chance 保场景
    静态 = P66 A/C 对照组范式】；1 HP/0.2 s = 5 HP/s → **4.0 s 死**
    （real-dt，fps 无关）；fire 8.0 s 燃尽 < 10.0 s 最短跑（margin
    2.0 s）；终态 health 0 + fires [] + blocks {}【fire 格回生成
    air = 无 diff 条目】+ pos/yaw/pitch 未动 + **零实体**（无玩家
    死亡掉落）+ 零效果（接触伤害是计时器不是效果），×2 savenorm 字
    节一致。
  - **S2b off-switch**（同场景 + `player.fire_damage 0`，×2 RAW）：
    存活 health 20、hunger 20（<36 s 无衰减 = 静态字段）、fires []、
    blocks {}——终态**零 real-dt 字段**（燃尽死亡爆发粒子 8.5 s 前
    全灭）→ **存档 + 截图 ×2 RAW 字节一致**（比 savenorm 强）。
  - **S6 config 失败 ×2**：`fire_damage_interval_seconds 0` → 通用
    正数校验点名；`fire_damage 21` → "must be <= player.max_health"
    点名（P54 熔岩交叉校验同款）。

### 已知问题（演进后）

- KNOWN_ISSUES「火 v2 范围」：无接触伤害 → **玩家接触伤害已落地**
  （P70）；**Mob 站火里不受伤害** = 火 v3 范围（新偏离，已写入该
  条 + 「伤害来源」条）。

### 下一步候选（P71+ 定案输入）

- campfire（火把交互扩展：烤食物 + 座位）/ 吸收层两分量模型（P56
  遗留大改）/ Mob 火接触（火 v3，本 phase 新偏离）/ "无效率工具慢
  10×"规则（P69 后 P11 条唯一剩余）/ 熔岩湖 worldgen（用户明示
  "留到很久的未来"，勿选）。
