# Phase 32 细节存档

> 用户指定方向："像 hungerDecaySeconds 这类参数，最好不要硬编码在代码中，
> 而是能在配置中修改。"——`Player::Params` 全部 24 个数值配置化。

## 阶段报告

- **`[2026-10-06, Phase 32]` 玩家参数配置化（用户指定）** — `Player::Params`
  全部 24 个数值（运动 walk/sprint/jump/gravity/terminal、游泳 4 项、
  几何 width/height/eye_height/crouch_height/crouch_speed_scale、
  mouse_sensitivity、生存 9 项 max_health/fall_damage_offset/regen 2 项/
  max_hunger/hunger_decay/starve_interval/regen_threshold/sprint_factor）
  可由 `config/config.json` 的 `player.*` 节覆盖。**机制**：
  `Player::Params::fromConfig(const Config&)` 静态方法（player 模块，纯
  函数可单测）——缺失键 = 结构体默认；**已存在键必须是 JSON 数字**
  （`Config::getDouble` 对类型不符会**静默回退默认**，故读 `at(key)` +
  `isNumber()` 严格检查——PITS 新坑位）；范围检查：非正拒绝（仅
  `fall_damage_offset`/`regen_hunger_threshold` 允许 0）、
  `eye_height < height`、`crouch_height < height`、
  `regen_hunger_threshold <= max_hunger`（跨字段）；未知 `player.*` 键
  容忍（config 前向兼容惯例）。`Player` 构造加 4 参重载（3 参委托 4 参——
  **坑**：嵌套结构 NSDMI 不能在同类的默认参数里用 `Params{}`，
  "default member initializer required before end of enclosing class"）。
  Game init `MC_TRY(fromConfig)` 后传给 `m_player.emplace`，出生点 room
  check 同步用配置尺寸（原用默认 `Player::Params{}`）。HUD 心带/饥饿带
  图标数由 `ceil(max/2)` 派生（原硬编码 10 + 每图标 2 点）——默认 20 =
  10 图标**逐位不变**，非默认 max 条带自动扩展（越界即裁，同既有窄窗口
  行为）。种子配置（`Config::defaultConfigJson`）带出 9 个生存节奏键
  （可发现性；mobs.* 同样可配置也不在种子中——先例）。**测试** 680/680
  （+11）：fromConfig 全默认 / 24 键全量覆盖精确值 / 逐键缺省混合 /
  非数值（字符串 "20"）拒绝 / 零与负拒绝 / offset 与 threshold 允许 0 /
  eye_height ≥ height 拒绝（==与>）/ crouch_height ≥ height 拒绝 /
  threshold > max_hunger 拒绝（== 允许）/ 未知键容忍 / **config→Player
  端到端**（`hunger_decay_seconds: 1` 的 Player 90 帧 = 1.5s 后 hunger 19，
  1/60 边界落帧 61）。`Params` 加 `= default` 的 `operator==`（24 字段
  成员比较，C++20）。**E2E**：base18 门 ×2 字节一致（**第 7 次结构性免疫**：
  config 解析仅启动期、config_xep 无 player 节 = 全默认零行为变化、HUD
  无输入不绘制且默认 20/20 派生 10 图标逐位一致）、白天 360f ×2 字节一致
  且 == p28_daym_a（场景零扰动）、**probe 运行游戏级验证**（config_p32probe
  = config_xep + `player.hunger_decay_seconds: 1`，360f `--save`：存档
  hunger 14 / sat 0 / health 20 / time 0.275 全命中——Python 镜像累加器
  预推：首个 1s 边界落帧 60（60×(1/60) 累加略大于 1.0，与 4s→帧 241 的
  "略小于"方向相反）、360f 内 6 tick → 20−6 = 14；证明配置值真的到达
  游戏循环里的活玩家）。

## 会话存档

- **提交**：P32 提交（本阶段唯一提交，hash 见 `SESSION_CONTEXT.md` 提交列表）。
- **目标**：用户反馈——`hungerDecaySeconds` 这类参数不应硬编码在代码中，
  应可在配置中修改（P31 刚把 4s→36s 写进代码，用户希望这类节奏参数可自调）。
- **范围决策**：整个 `Player::Params`（24 字段）全部暴露，不挑子集——
  部分暴露会留下任意的"为什么 X 不能配"边界；mouse_sensitivity 等更是
  典型的用户偏好项。HUD 与 `max_health`/`max_hunger` 的耦合（原硬编码
  10 图标 × 2 点）用 `ceil(max/2)` 派生解决，默认渲染逐位不变。
- **校验位置**：`fromConfig` 放 player 模块（静态方法）而非 game.cpp——
  纯函数、可单测、错误信息带键名；player→core/config 依赖无环
  （config.hpp 只依赖 json/error）。
- **存档格式**：不变——存档存的是 health/hunger/saturation 绝对值，
  读档时 `setHealth`/`setHunger` 按当时的 max 钳制（旧档在 max 变化的
  世界读入 = 钳制语义，无迁移）。
- `config/config.json`：种子配置（`Config::defaultConfigJson`）的新 `player`
  节影响新生成的 config 文件；**事后按用户要求**，9 个 `player` 默认键直接
  写入实际的 `config/config.json` 并经 `git commit --amend` 并入本提交——
  该文件自此为已提交文件（含用户调值 view_distance 80 / day_length 600），
  AGENTS.md「禁止事项」的"永不还原/提交 config.json"规则同步改写。
