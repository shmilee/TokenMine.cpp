# Phase 54 细节存档

> 范围（用户"好的.按建议继续。" = 建议 1，代理定案为最小完整版：**熔岩
> 后续 = 燃烧 + 下沉**，消费 P51 遗留"熔岩无燃烧/减速" + "湿润判定把
> 熔岩也算水"；worldgen 熔岩湖/脱离残留火/熔岩烧 Mob 与作物留后续）：
> ① `Player::isInLava()`（AABB 流体扫描，与 `isInWater` 共享实现，
>    水/熔岩并存时熔岩优先）；
> ② 熔岩节奏（`lava_swim_speed` 1.9 / 上浮 1.5 / 重力 4 / 终端 2，
>    config 可覆盖，比值对齐 MC 运动系数比 0.014/0.0231）；
> ③ 灼烧伤害（真实 dt 生存计时器，每 1.0 s 受 4 HP = MC 4 HP/s，
>    走正常伤害路径；离熔岩重置）；
> ④ 落入熔岩取消坠落伤害 + 熔岩内抑制疾跑/下蹲；
> ⑤ `cropIsWet` 只算水（熔岩旁耕地 = 干节奏）；
> ⑥ 6 个新 `player.*` config 键（含 `lava_damage` ≤ `max_health` 交叉
>    校验）。

## 阶段报告

- **`[2026-10-13, Phase 54]`** 熔岩后续（燃烧 + 下沉）
  — 964/964（+10），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 28 次
  结构性免疫免重锚 + S2 灼烧致死【熔岩池里 max_health 4，×2 存档字节
  一致】+ S3 熔岩游泳【终端状态贴 +Z 墙 (22.5,45,−3.3)，×2 字节一致】
  + S4 水回归【×2 存档字节一致】+ S5 init 失败 ×2 干净报错）。

### 实现内容

**熔岩游泳状态 + 节奏**（`src/player/player.{hpp,cpp}`）：
- `aabbOverlapsFluid(world, mn, mx, kind)` 匿名命名空间自由函数：
  `isInWater`/`isInLava` 共享的 AABB 逐格严格重叠扫描（每轴
  `min < c+1 && max > c`，与碰撞同款约定）。
- tick 状态 `m_inLava`（与 `m_inWater` 同点计算，一次/帧）。速度选择
  熔岩优先：`m_inLava ? lavaSwimSpeed : (m_inWater ? swimSpeed :
  (Shift ? sprint : walk))`；疾跑判定与下蹲速度缩放加 `!m_inLava`；
  `integrate()` 熔岩分支（lavaGravity / lavaTerminalVelocity /
  Space→lavaSwimUpSpeed，同水分支结构）；子步尺寸估算 `vyAfter` 同步
  三分支。
- **落入熔岩取消坠落伤害**：着陆判定 `!isInWater() && !isInLava()`
  （任一流体取消，与水同款）。

**灼烧伤害**（真实 dt 生存计时器，`Player::update` 饿死块之后）：
- `m_lavaAccum += dt`（仅 `m_inLava && !m_dead`）；
  `while (m_lavaAccum >= lavaDamageIntervalSeconds)`：减间隔 +
  `damage(lavaDamage)`（正常路径 = 红闪/音效/死亡全触发）+ 死亡即
  break。
- **离熔岩重置**（`else m_lavaAccum = 0.0`）：v1 无残留火（MC 离熔岩
  燃烧数秒 = 火系统，遗留 KNOWN_ISSUES）。
- `respawn()` 重置 `m_lavaAccum`。

**`cropIsWet` 只算水**（`src/world/growth.cpp`）：
- `world.getFluid(...)` → `world.getFluidOfKind(..., FluidKind::Water)`：
  切比雪夫 4 内的熔岩不再湿润耕地（MC 熔岩不湿润；烧作物是遗留）。

**config（P32 范式）**：`player.lava_swim_speed`（1.9）、
`player.lava_swim_up_speed`（1.5）、`player.lava_gravity`（4）、
`player.lava_terminal_velocity`（2）、`player.lava_damage_interval_seconds`
（1.0）、`player.lava_damage`（4）——缺键 = 默认、已存在必须 JSON 数字
且 >0（`lava_damage` 另加 ≤ `max_health` 交叉校验，拒启点名）。种子配置
不带出（P32"其余键写出即生效"同款）。

### 变更文件

- `src/player/player.hpp` — Params 6 键（熔岩节奏 4 + 灼烧 2）+
  `isInLava()` 声明 + `m_inLava`/`m_lavaAccum` 成员 + fromConfig 拒绝
  清单注释。
- `src/player/player.cpp` — fromConfig 6 读 + `lava_damage` 交叉校验 +
  `aabbOverlapsFluid` 共享扫描 + 速度/疾跑/下蹲/`vyAfter`/坠落取消 +
  `integrate()` 熔岩分支 + 灼烧计时器 + respawn 重置。
- `src/world/growth.cpp` — `cropIsWet` 只算水（+ 注释：MC 熔岩烧作物
  是遗留）。
- `tests/test_player.cpp` — +9（灼烧速率 90 帧恰 1 tick / 320 帧致死 /
  离熔岩重置 / 熔岩比水慢 + 单帧精确速度 / 上浮节奏 / 疾跑抑制 /
  坠落取消【浅池 1 层：2 层池的 1 s 穿池浸没会混入 1 个灼烧 tick】/
  config interval 0 与负速度拒绝 / config `lava_damage > max_health`
  拒绝）+ `buildPool` 加 `FluidKind` 参数。
- `tests/test_growth.cpp` — +1（熔岩旁耕地步数 == 无流体干场景
  【同模数严格相等】，同位水 > 干【湿路径仍活】）。
- 文档：`AGENTS.md`（现状 P54 / 964 / 第 28 次免疫）、`ROADMAP.md`、
  `KNOWN_ISSUES.md`（关闭 2 + 新增 1）、`PITS.md`（+2）、
  `CONTENT_GUIDE.md`（6 键入表）、`GAMEPLAY.md`（熔岩节改"会烧"）、
  `archive/phase-54.md`（本文件）。

### 构建与验证结果

- 构建：`-Wall -Wextra -Wpedantic -Werror` 0 警告。
- 测试：**964/964**（+10）。
- E2E（`/tmp/opencode/p54_e2e.zsh`，批跑全绿）：
  - **S1 base18 门**：P54 ×2 60f 无输入字节一致 + == base18 →
    **第 28 次结构性免疫（免重锚）**【熔岩只存在于场景（桶/放置），
    fresh 世界零熔岩零作物 = 无输入零活动】+ 红心 0。
  - **S2 灼烧致死**（无输入 600f）：玩家站在熔岩池里（池 = seed-9 天然
    2×1 空穴 (22,45,−5/−4) + 3 石块编辑围墙；die config
    `max_health: 4` → 首 tick 即致死 = 布尔断言，fps 无关）：存档 ×2
    字节一致（**场景跑确定性权威**）+ 存档精确（health 0 + 位置
    (22.5,45,−4.5) 不变 + hunger 20 + 池/墙完好 + 零实体）+ 截图
    notblack/skygate 健全性（截图 ×2 不做——见"场景截图 ×2 流送边界"
    决策）。
  - **S3 熔岩游泳**（输入 900f）：mov config 灼烧间隔 300 s 隔离伤害；
    玩家从池上方 (22.5,47,−4.5) 面朝 +Z（yaw π）载入（**载入即自由落
    体**——落体从 t=0 开始，不是 W 按下时），W 按住 @ 2.9 s 锚定至运行
    结束：存档 ×2 字节一致（输入跑确定性权威）+ 存档精确（**终端状态
    = 贴 +Z 墙 (22.5, 45, −3.3)**——到达 ~3.7 s ≪ 900f 最短实跑 5.1 s
    = fps 无关【P53 纪律】+ health 20 = 无灼烧 + 零实体）。
  - **S4 水回归**（无输入 240f，同池灌水）：存档 ×2 字节一致 +
    health 20（水不烧）+ 位置不变 + 池完好 + 截图健全性。
  - **S5 init 失败 ×2**（config）：`lava_damage_interval_seconds: 0` →
    rc=1 + "config player.lava_damage_interval_seconds must be positive"；
    `lava_damage: 100`（> max_health 20）→ rc=1 + "config
    player.lava_damage must be <= player.max_health"。

### 关键决策

- **场景池 = 天然空穴 + 方块编辑围墙（首版"挖坑灌岩"被拒）**：存档
  禁止**同格**同时有 block 编辑与 fluid 编辑（parseSave 硬拒 "a block
  displaces fluid"）——"挖 4 格 air + 同格灌 level-8 熔岩"不可一步完成。
  对策：p54terr 地形探针（逐列 top y + 格内 fluid）找到 seed-9 的天然
  2×1 空穴 (22,45,−5/−4)（列顶 44 → y=45 天然 air、无天然水、底 = y=44
  顶面），同 y 四邻中 3 个天然 air 的格用**石头编辑**封墙（天然 air 格
  放石头 = 合法 block 编辑，与池格 fluid 编辑不同格），其余 4 邻天然
  solid → level-8 池首 tick 起稳定（流体模拟惰性）。PITS 已收录。
- **灼烧 E2E = 致死布尔断言（fps 无关）**：场景跑 fps 漂移大（本批
  26..62），精确 tick 数断言会赛跑；`max_health: 4` 让首 tick（1.0 s）
  即致死 → 断言"dead"只要运行 >1.0 s（600f 最短实跑 9.7 s，余量巨大）。
  精确速率（4 HP/1 s）由固定 dt 单测覆盖。
- **游泳 E2E = 终端状态断言（fps 无关）**：W 按住不释放，断言终态
  （贴墙位置）而非中途位置；到达时刻 ~3.7 s ≪ 最短运行 5.1 s。
- **隔离控制参数必须盖住整次运行**：S3 首版 `lava_damage_interval: 30`
  被 26 fps 的 34.3 s 实跑反超（浸没 34 s → 打 1 tick，health 16 ≠ 20，
  游泳断言全对）——隔离用时长参数取最长可能运行的数倍（300 s）。
  判断实跑时长用日志首末时间戳，不用别的场景 fps 外推。PITS 已收录。
- **场景截图 ×2 字节一致不是有效断言（流送边界与墙钟相关）**：S4 水塘
  场景（玩家 pitch 0 水平视野）240f 两跑（7.50 s vs 7.51 s，差 10 ms）
  截图 ×2 不一致：diff = 屏幕 (750,302)-(769,311) 的 20×10 px 树冠碎片
  （反投影 ≈ 世界 (25,48,−16) = 第 2 环流送列 (1,0,−2) 与已完列
  (1,0,−1) 跨界的树冠部分）——**存档 ×2 字节一致**（世界全同）但终帧
  渲染差在 pop-in 完成时刻：P14 已知"1 帧暗 pop-in"是帧锁定的，但
  **哪个列在第 N 帧前集成完**取决于 worker 墙钟吞吐（光照 job 每帧至多
  1 个、流送 job 在 post→flush 窗口按墙钟跑）。base 门（fresh 60f）与
  俯角场景（P47–P53 放置场景都瞄下方，视野无地平线）不受影响。对策 =
  场景跑确定性权威 = 存档 ×2，截图只做 notblack + skygate（PROTOCOL.md
  加场景例外条款；PITS 已收录）。4 连跑实证：严格两分（天空版/树冠版
  交替），同位置同形状。
- **坠落取消测试用浅池**：2 层池里 11 格坠落 = 穿池 1 s + 池底停留，
  运行尾段累计浸没 ≥1.0 s → 混入 1 个灼烧 tick（health 16 ≠ 20）；
  改 1 层池 + 短跑（落地后总浸没 <1.0 s）隔离坠落取消本身。

### 已知问题（新增/更新，详见 KNOWN_ISSUES.md）

- ~~熔岩无燃烧/减速~~（P51）→ 已修复（本阶段）。
- ~~湿润判定把熔岩也算"水"~~（P51）→ 已修复（本阶段）。
- **新增**：脱离熔岩无残留燃烧（v1 无火系统）+ 熔岩不烧 Mob 与作物。

### 下一步建议

1. **worldgen 熔岩湖**（消费 P51 遗留"worldgen 不生成熔岩"；需 base18
   重锚——世界生成变更，P41 范式）：深谷/海平面以下的低地按参数概率
   填熔岩源（worldgen 参数数据驱动）。
2. **多掉落内容扩展**（更多方块用 `chance_drops`，小）。
3. **金苹果后续**（吸收层）。
