# Phase 58 细节存档

> 范围（用户"按你的建议继续下两个phase" = 建议 1，代理定案为最小完整版：
> **火系统 v2 = 熔岩烧 Mob 与作物，全量消费 P54 遗留"熔岩不烧 Mob 与
> 作物"条目**）：
> ① 块属性 `flamable`（可选 bool 缺省 false；内容 = wheat_1..7 全 7 阶）；
> ② FluidSim 燃烧 pass（熔岩格 6-邻 flammable 块 → air，无掉落，快照
>    确定性）；
> ③ Mob 点燃（生成时 `armFire` 从 ignites_on_lava 效果 + config
>    `fire_duration_seconds` 取参；**整数帧燃烧格**；熔岩内每帧重置
>    duration【相位保留】、离熔岩烧 8 s、水瞬时熄灭且胜熔岩）；
> ④ init 交叉校验（ignites 效果 tick_interval×60 必须整帧数，容差
>    1e-4 帧；`flamable` 非 bool 点名拒绝）；
> ⑤ 顺手修复 P56 `grants_effects` unknown-field 白名单一行漏列。
> 多掉落内容再扩展（gravel + flint）= 建议 2，留 Phase 59。

## 阶段报告

- **`[2026-10-13, Phase 58]`** 火系统 v2：熔岩烧 Mob 与作物
  — 1007/1007（+18），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 32 次
  结构性免疫免重锚 + S2 熔岩膜 10 只被动 mob 烧死【4 cow / 3 chicken
  / 3 sheep】玩家 20 HP 恒定 + S2ctl 水膜对照【hostile 杀玩家 health 0、
  零点燃】+ S4 作物燃烧【wheat_3 → air、池 +1 L7】+ S6 init 失败
  ×2）。

### 实现内容

**实体基类**（`entity/entity.{hpp,cpp}`）：
- `Entity::overlapsFluid(const World&, FluidKind)`（AABB 扫描该种类流体
  格，严格不等式重叠）——自 `player.cpp` 文件本地函数**上移**到基类
  （`Player::isInWater`/`isInLava` 改调基类，行为不变，989 旧测试全
  过）。

**块**（`block/block.{hpp,cpp}`、`block/block_registry.cpp`）：
- `BlockProperties::flamable`（bool，缺省 false）；`checkField` 严格
  解析（非 bool = "block property 'flamable' must be a boolean"）；
  kFields +1。
- 内容：`wheat_1..7.json` 全 7 阶加 `flamable: true`（MC 小麦会烧）。

**流体**（`world/fluid.{hpp,cpp}`）：
- `burnFlamableNeighbors`（匿名空间纯函数，快照语义同反应 pass）：扫
  所有 LAVA 格 6-邻 **BLOCK** 格，`flamable` → 收集、排序后
  `setBlock(air)`（**无掉落**；一格被两格收集只烧一次——第二次
  setBlock 报无变化）。注册表无 flamable 块 = fast-exit（fresh 世界零
  成本）。
- 并入 `FluidSim::tick`：反应 pass 后、per-kind 模拟前（烧只销毁
  BLOCK 不碰流体 = 位置相对 per-kind 扫描行为中性；反应先走让转化格
  可见）。changed 合并。

**Mob**（`entity/mob_entity.{hpp,cpp}`）：
- `armFire(double tickInterval, double tickAmount, double duration)`：
  生成时由 `Game::attemptMobSpawn` 调用（点火效果存在才调）。燃烧 =
  **整数帧格**：`m_fireIntervalFrames = llround(tickInterval × 60)`
  （init 已保证整帧）、`m_fireFrames` int 帧计数、`m_fireRemaining`
  double 时长——tick k **精确**落在第 12k 个燃烧步（float double 累加
  首版实测相位 off-by-one，弃用，见 PITS）。
- `update()` 在 `moveInSubsteps` 后：`inLava()` →
  `m_fireRemaining = duration`（**相位不清零** = 熔岩内持续燃烧）；
  `inWater()` → `m_fireRemaining = 0` + 相位重置（**先于 tick** 执行 =
  水内零伤害、水胜熔岩，P57 玩家同款）；`m_fireRemaining -= dt` 后
  `> 0` 才 `++m_fireFrames`，每跨一个 interval 帧结算 1 次 damage
  （走 `MobEntity::damage` 正常路径，**可致死**；**到期帧不 tick** =
  P46 语义）。
- 无 fire 效果的资产集 = `m_fireEffectId == -1` = 全部 mob 结构不燃
  （结构性免疫，同 P57 玩家侧）。

**游戏**（`core/game.cpp`）：
- init 交叉校验：点火效果（`m_fireEffectId != -1` 时）的
  `tickInterval × 60` 必须是**整帧数**——容差 **1e-4 帧**（Effect::
  tickInterval 是 float，0.2f×60 = 12.00000018 过；0.33 s = 19.8 帧
  点名拒绝 "…is not a whole number of 1/60 s frames (the mob burn
  grid)"）。
- `attemptMobSpawn`：`make_unique` 后、push 前，`m_fireEffectId != -1`
  时从 `m_effects.at(...)` + `m_player->params().fireDurationSeconds`
  取参 `armFire`（与玩家同源时长）。

**顺手修复**（`item/item_registry.cpp`）：
- P56 引入的 `grants_effects` 漏列 unknown-field 白名单（解析正常，仅
  每次 init 一条 WARN 噪声）——补一行，E2E 日志干净。

**测试**（+18，1007/1007）：
- `test_entity`：`overlapsFluid` ×2（水/熔岩 AABB 重叠）。
- `test_mob`：`MobFireTest` ×7（armFire 参数 / 熔岩内重置保留相位 /
  离熔岩 12 帧格 tick / 到期帧不 tick / 水瞬时熄灭先于 tick / 致死 /
  未武装不燃）。
- `test_block`：`flamable` 解析 ×3（缺省 false / 非 bool 拒绝 / 显式
  true）+ wheat_1..7 钉死（加载真实 assets 断言全 7 阶 true，防概率
  笔误同款纪律）。
- `test_fluid`：`FluidBurnTest` ×5（烧 6-邻 flammable 无掉落 / 非
  flammable 不动 / 双收集只烧一次 / 无 flamable 块 fast-exit / 烧后
  邻格可被流体填充）。

### 关键决策

- **Mob 燃烧 = 整数帧格，不是 P46 玩家效果同款 double 累加**：玩家
  效果 tick 走真实 dt（硬规则 8，生存计时器类）；但 mob 燃烧需要
  "tick 时刻 = 整帧整数倍"（init 校验依赖它、确定性断言依赖它）。
  double 累加 0.2f 间隔 + 1/60 步进实测 off-by-one → int 帧计数。
  时长（remaining）仍 double（真实 dt 语义 = 离熔岩 8 s 残留），tick
  由帧计数推进——两个量分开存。
- **熔岩内重置不清相位（P57 玩家同款）**：inLava 每帧
  `remaining = duration`，不清帧计数 → 熔岩内持续燃烧节奏连续；离
  熔岩从当前相位继续 countdown 8 s = 残留。水 = 瞬时 `remaining = 0`
  + 相位重置（水内零伤害 + 出水重烧从零节奏，MC 语义）。
- **烧作物无掉落**：FluidSim 燃烧 pass 只 `setBlock(air)`（对比玩家
  破坏有掉落）——MC 烧掉的小麦也不掉物品；且燃烧 pass 在 world 层，
  掉落需要物品实体通道（游戏层），v1 不引。
- **flamable = 6-邻接触即烧，不是"烧进熔岩"或延迟**：快照 pass 每帧
  跑 = 接触即烧（帧 1 生效）；水不烧（只有 LAVA 格参与）；烧成 air
  后熔岩可正常扩散进该格（S4 观测到 +1 L7）。
- **init 整帧校验容差 1e-4 帧**：float 误差量级（0.2f×60 偏
  0.00000018 帧）；真离格值（0.33 s 偏 0.2 帧）照样拒。1e-9 会拒掉
  自带 fire（PITS）。
- **armFire 在生成时，不在每帧**：点燃是**生成时武装**（参数固定）+
  熔岩内持续（每帧重置 remaining）——不是"每帧 apply 效果实例"（mob
  不走 StatusEffects，无效果实例概念；armFire 只存三个参数）。

### E2E 证据链（熔岩烧 Mob 与作物的证明方式）

"mob 走进熔岩"在无输入运行无法由玩家引怪（hostile 被熔岩光门挡，
见 PITS）——场景设计 = **地面 level-1 熔岩膜**（被动 mob 生成在膜边、
游走踩过即燃 = 几何必然，不赌随机游走）：
- **S2 熔岩膜**（无输入，2400f，×2 savenorm【剥 {hunger,
  saturation, remaining, age}】字节一致）：3×3 L1 膜铺地面 y46（纯
  fluid 编辑零 block 编辑：L1 = 终止膜零扩散、上方 4 个天然坑格首
  tick 灌入坑内）→ **10 只被动 mob 烧死**（4 cow / 3 chicken / 3
  sheep = 20 掉落 = 18 item 实体【2 对同位堆叠】）+ 膜光 15 挡全部
  hostile 生成 → 玩家 20 HP / 位置恒定 + fluids 终态 9 格精确 + 掉落
  位置全部落在膜/坑区域 box 内。**无 P58 则零死亡零掉落**（mob 不
  燃）= 烧死证明。
- **S2ctl 水膜对照**（同 9 格几何改水，×2 savenorm 字节一致）：水光
  0 → hostile 于暗 ring 生成杀死无输入玩家（health 0）+ **entities
  [] = 零 mob 烧死** = 隔离出"熔岩（而非场景几何）是唯一点燃源"，
  同时证明光门差异（S2 玩家活着 / S2ctl 玩家死）。
- **S4 作物燃烧**（无输入，240f，×2 RAW 字节一致）：P54 2×1 L8 池 +
  3 石 rim + `wheat_3` 块编辑贴池 6-邻 → 帧 1 燃烧 pass 销毁小麦（格
  = "air"，**无掉落** entities []）+ 池向该 air 格扩散 +1 L7【终端：
  其余邻格 solid/rim】+ rim 完好 + 玩家 20 HP / hunger 20。
- **S1 第 32 次结构性免疫**：fresh 世界零熔岩（P54）、零 flammable
  块（wheat 只玩家种）、60f 内零 mob = 两条燃烧路径皆惰性 → base18
  字节一致，无重锚。
- **S6 init 失败 ×2**：`flamable: "yes"` 非 bool / fire
  `tick_interval: 0.33`（19.8 帧 > 1e-4）——scratch assets，错误点名
  P58 数据失败。

### 会话存档

- **场景迭代史（三次死路 → 膜）**：
  1. 3×3 挖掘池：存档**禁止同格 block+fluid 编辑**（挖坑 + 注水单
     存档不可表达，两段式也撞同一校验）→ 死路。
  2. 五格天然腔池：被动 mob 池边出生但随机游走离开（2400f 探针零
     横穿）；hostile 被熔岩光门 15 全挡；cap 4 卡死 → 死路。
  3. 不可落袋几何（探针实证）：0.9 宽 cow AABB 跨 (21,−5) 池格必然
     同时压北/南侧实块 → 永久受支持不坠落；仅 x22 列可落但 cow 需要
     x-center 0.1 窗不可能 → **地面 level-1 膜** = 无需下坠、走过即
     燃、3×3 目标 → 2400f 10 只烧死。教训入 PITS（E2E "mob 进入
     危险区" = 几何必然）。
- **S2 首跑发现**：膜上方 4 格（不是 3）是天然坑——(23,45,−4) 也是
  top 44 天然坑（首版断言按 3 坑写假失败）→ 断言必须按**终态流体**
  写（膜残留 5 + 坑内 4）。
- **物品 rest 位置确定性论证**：掉落零初速（死亡位生成，破坏块同款
  约定）→ x/z 无物理位移；分离 = 位置投影（不注速度）→ 终位 =
  固定步轨迹的纯函数 → ×2 字节稳（savenorm 剥 {hunger, saturation,
  remaining, age}，物品位置**保留** = 正是断言）。
- **S4 脚本 bug**：scene() helper 只处理膜模式，S4 池 fluids 写成
  []（"0 fluid edits"）→ 首跑 fluids [] 小麦未烧（无熔岩）；修正
  后全绿。
- **grants_effects WARN**：P56 遗留白名单漏列（解析正常仅 WARN），
  P58 顺手一行修复并复跑全批（最终二进制 E2E 全绿）。
- **裸截图假失败**：不经窗口稳定化直接 `--screenshot` 截到 WM 平铺
  窗（1272×1552）→ base 门 cmp 假失败（PITS 新增）。
- 脚本：`/tmp/opencode/p58_e2e.zsh`（全批：S1/S2/S2ctl/S4/S6；
  savenorm = python 剥字段后 sort_keys dump）；探针：
  `/tmp/opencode/p58_probe.zsh`（膜场景）+ `p58scene.cpp` /
  `p58rng.cpp` / `p58scan2.cpp`（场景复现 / wander RNG 校准 / 扫描
  序）。
