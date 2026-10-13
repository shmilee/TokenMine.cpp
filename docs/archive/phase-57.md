# Phase 57 细节存档

> 范围（用户"按你的建议，继续下一phase" = 建议 1，代理定案为最小完整版：
> **脱离熔岩残留火 = 火系统 v1：玩家点燃 + 残留燃烧 + 水灭火**，消费
> P54 遗留"脱离熔岩无残留燃烧"；熔岩烧 Mob/作物 = 既有遗留，不动）：
> ① 效果两个数据标志 `ignites_on_lava`（至多一个注册效果可带，init
>    交叉校验）+ `extinguished_by_water`（`clearExtinguishable`）；
> ② `Game::updateEffects` 点燃/灭火（在 count==0 早退前每帧执行——
>    点火是**源**，无活跃效果也要跑）；
> ③ `fire` 效果（1 HP / 0.2 s = MC 燃烧节奏，可致死，`negative: false`
>    = 牛奶不灭火）+ config `player.fire_duration_seconds`（默认 8 =
>    MC Normal）+ effects.png 64→80（火焰 icon 4）；
> ④ 熔岩内 P54 直接灼烧（4 HP/s）与火 tick（5 HP/s）叠加 = ~9 HP/s
>    （MC 同款：lava damage 与 Fire 效果并存）。

## 阶段报告

- **`[2026-10-13, Phase 57]`** 脱离熔岩残留火：火系统 v1
  — 989/989（+7），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 31 次
  结构性免疫免重锚 + S2 熔岩内复合烧死 + S2b 纯点燃【mov config 熔岩
  直伤 300 s → 唯一伤害源 = 火，无 P57 则活着 → DEAD 终态 = 点燃
  证明】+ S3 水灭火零伤害 + S4 干地 14 tick → health 6 + S5 P54 游泳
  场景原样 + 纯火死于贴墙终态 + S6 init 失败 ×3）。

### 实现内容

**效果系统**（`core/status_effect.{hpp,cpp}`）：
- `Effect::ignitesOnLava` / `Effect::extinguishedByWater`（bool，缺省
  false；非 bool 点名拒绝；kFields +2）。
- `StatusEffects::clearExtinguishable(const EffectRegistry&)`：移除带
  `extinguished_by_water` 标志的活跃实例（clearNegative 同款
  erase-remove 纯函数）。

**玩家**（`player.{hpp,cpp}`）：
- `Player::Params::fireDurationSeconds`（默认 8.0 = MC Normal；
  (0,100]：0 走通用正值校验、>100 交叉校验点名）。
- 公开访问器 `inWater()` / `inLava()`（P54 tick 状态 `m_inWater`/
  `m_inLava` 暴露给游戏侧——`isInWater()`/`isInLava()` 是 private
  tick 内部件）。

**游戏**（`core/game.{hpp,cpp}`）：
- `m_fireEffectId`（init 扫描注册表：带 `ignites_on_lava` 的效果；
  **至多一个**，两个 = MC_FAIL 点名；零个合法 = 无火系统资产集）。
- `updateEffects` 重构：`dead` 检查后、`count() == 0` 早退**前**：
  `inLava()` → `apply(fire, fireDurationSeconds)`（**每帧**重施加 =
  duration 重置 + tick 相位保留 → 熔岩内持续燃烧，MC 语义）；
  `inWater()` → `clearExtinguishable`（**先于 tick** 执行 = 水内零
  燃烧伤害；两流体并存 = 先点燃后灭火 = 无火，水胜熔岩）。
- 顺序保证：`m_player->update`（物理，刷新 inWater/inLava）→
  `updateSurvival` → `updateEffects`（既有帧序，零改动）。

**内容**：
- `assets/data/effects/fire.json`：damage 1 / 0.2 s（MC：1 HP/4
  tick）、`min_health: 0`（可致死）、`negative: false`（牛奶不灭火 =
  MC 语义——灭火只有水）、两标志皆 true、icon 4。
- `tools/generate_effect_icons.py`：W 64→80，`tile_flame()`（8×12
  火焰剪影 + 黄芯，salt 800 段）。effects.png = 80×16。HUD UV 动态
  （`width/kIconPx`）= 零代码适配。

### 关键决策

- **火 = 效果，不是新系统**：完全复用 P46 效果系统（数据定义、真实
  dt 倒计时、tick 结算、HUD 图标、存档、死亡清空），新增仅两个数据
  标志 + 游戏侧每帧点燃/灭火接线。燃烧 tick 走正常受伤路径（红闪/
  音效/死亡全触发；**无独立火焰音效/tint** = 新增 KNOWN_ISSUES，P46
  中毒同款模式）。
- **时长走 config 不走效果 JSON**：点燃源是熔岩流体（代码词汇，无数据
  文件），效果 JSON 的 duration 惯例是**源**给的（mob 数据/进食数据）
  ——这里源没有数据，走 config 通道（P54 `lava_damage` 同款）。
  效果 JSON 只带两个行为标志。
- **`negative: false` = 牛奶不灭火**：`negative` 的机械语义是"被牛奶
  清"（不是道德语义）；MC 牛奶不灭火 → fire 取 false。文档化于
  GAMEPLAY/MECHANICS。
- **每帧重施加 vs 只在进入时施加**：每帧重施加 → 站在熔岩里 duration
  恒 8 s（持续燃烧）+ 离开时开始 countdown（残留 8 s）= MC 行为；
  只在进入时施加 → 熔岩里待 8 s 火就到期了（MC 不会）。apply 幂等
  （duration 重置、tick 相位保留）= 每帧重施加零副作用。
- **熔岩内 9 HP/s 叠加是 MC 同款**：MC 熔岩内 = 4 HP/s lava damage +
  Fire 效果 1 HP/4 tick 并存；项目 P54 直伤保留 + 火 tick 叠加。

### E2E 证据链（残留燃烧的证明方式）

"离熔岩后继续烧"无法用单一无输入终态直接观测（离开熔岩需要输入，
弹出水面后的落点时刻 = 真实 dt → 终态位置 fps 敏感）。证据链 =
组合：
- **S2b 纯点燃**（mov config 熔岩直伤拉 300 s = 禁用 → 池内无输入
  玩家的**唯一**伤害源 = 火效果：20 tick × 1 HP → DEAD；无 P57 则
  玩家活着停 20 HP → DEAD 终态 = 点燃证明，×2 字节）；
- **S5 游泳+纯火死于贴墙**（P54 S3 游泳场景**原样**：W 从 +1.0 s
  按住到结束 = 玩家持续顶着 rim 墙 = 几何钳制终态 fps 无关；mov
  config 唯一伤害源 = 火：掉池点燃（0.25 s）→ 沿池底漂到墙
  （~1.63 s）→ **死于贴墙处** (22.5,45,−3.3)；无 P57 则 P54 S3 终态
  = 活着贴墙 20 HP → DEAD 终态 = 火证明，×2 字节）；
- **S4 干地燃烧**（注入 fire 无输入：效果在**无熔岩**时照常烧 =
  效果一旦施加源无关；14 tick = 到期帧不 tick【P46 语义】→ health 6）；
- **S3 水灭火**（唯一停止路径 = 水：帧 1 先灭后 tick = 零伤害）；
- **接线结构**：离熔岩后无任何代码清除 fire（仅到期/水/死亡）——
  残留 = 结构性保证。
- S1 第 31 次结构性免疫（fresh 世界零熔岩【P54】零效果 = 点火源永不
  触发）；S6 init 失败 ×3（两 ignites_on_lava / fire_duration 0 /
  非 bool 标志）。

### 会话存档

- **S5 v1 失败复盘**（输入锚定 2.9 s 太晚）：玩家从 y47 落池后 9 HP/s
  满血 2.5 s 已死，W 按下前已死 → 终态 = 池底中央死亡位（非设计贴墙
  位）。修正 = W 锚定 1.0 s（入池即按）+ mov config（纯火 5 HP/s →
  死亡 4.25 s 晚于 W 锚定 + 顶墙 1.63 s）。教训入 PITS（输入锚定须
  早于最早死亡时刻）。
- **确定性终态设计**：P54 S3 的贴墙终态之所以 ×2 字节 = 玩家**持续
  顶着**几何边界（rest 位置 = 墙脸 − 0.3 唯一决定）。"弹出水面落
  某处停"不可用（落点 = 真实 dt ± 帧）。
- 场景几何 = P54 已验证池（seed 9 天然 2×1 空穴 (22,45,−5)/(22,45,−4)
  + 3 石 rim；流体模拟首 tick 起惰性）。
- 脚本：`/tmp/opencode/p57_e2e.zsh`（mov config = P54 mov 模式：
  `lava_damage_interval_seconds` 300 s）。
