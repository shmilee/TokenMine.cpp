# Phase 33 细节存档

> 用户确认方向："根据你的建议1来"——死亡时暂停流式加载。由用户日志发现：
> 玩家死亡（身体冻结、不再行走）后，`Streamed column ... integrated` 日志
> 仍持续滚动、chunk 计数持续上涨——流式在死亡画面背后把 `view_distance`
> 80 的半径方继续填满（~20.7 万 chunk / 数 GB 内存 / 数小时 CPU）。

## 阶段报告

- **`[2026-10-06, Phase 33]` 死亡时暂停流式加载（用户确认）** —
  `Game::streamChunks()`（game.cpp）加一行门控：`m_player->isDead()` 时在
  **迟滞逻辑之前**提前返回。**设计要点**：(a) 门控放在 `nearestMissing`/
  `m_streamingEngaged` 判断之前 → 激活状态在死亡期间**保留**，`R` 重生后
  从原状态继续填充（不重置、不抖动）；(b) `integrateStreamedColumns()`
  与 `postLightJobs()` **不门控**——死亡瞬间在飞的 ≤1 列
  （`kStreamColumnsPerFrame=1`）经 post→flush 窗口自然排空并照常照亮，
  若一并门控该列会停在光 0 且 mesh 永不再更新；(c) 昼夜/流体/mob/实体
  不受影响（世界照常 tick，与背包打开同规则，game.cpp 注释有先例）。
  用户日志中"退出 13.6s"是另一问题（存档 `diffWorld` O(已加载 chunk)
  单线程，~0.14 ms/chunk），本阶段不处理（P34 候选：并行 diff）。
  **E2E（全程无输入、零 xdotool 输入 → 无 SDL grab-delta 干扰，存档+
  截图字节一致即权威）**：
  - 基线门：base18 ×2 字节一致（**第 8 次结构性免疫**：无输入运行不会死
    → 门控惰性）；白天 360f ×2 字节一致且 == p28_daym_a（场景零扰动）。
  - **饿死死亡场景**：probe config（config_xep + `player.hunger_decay
    _seconds: 1` + `starve_damage_interval_seconds: 1` + `view_distance:
    25`）+ 存档把玩家置于演示区**边缘列** (1,0)（世界坐标 16.5, 44.0,
    0.5——距缺失列 ≤1 才激活流式，见 PITS）。死亡时刻确定性 = t=40.0s
    整（20 饥饿点 × 1s 衰减 = 20s 空 + 20 × 1 HP/1s = 40s；生存计时器
    用真实 dt，与 fps 无关；昼夜仍为固定 1/60 每帧步长）。`view_distance
    25`（51×51=2601 列）保证死亡时方未满（60fps 下填满需 ~43.1s > 40s；
    实测该机 20fps，死亡时仅 946/2601 列）。断言：流式已激活（frame 1）
    + ≥1 列集成 + **41.5s 后零集成**（无门控则会继续到 ~43s，任何 fps
    下均可区分）+ 最终 7696 chunks < 20808 + 存档 health 0 /
    pos (16.5, 44, 0.5) / time = 0.25 + 3605/14400 精确 + 存档 ×2 与
    截图 ×2 逐字节一致 + 死亡屏幕像素验证（白字 612 px，见下）。
  - **补充验证（虚空死亡 + F3）**：存档把玩家置于 y=−33（< −32 击杀面）
    → 加载即死 → F3 覆盖层显示 `health 0.0/20.0` + `streaming idle
    pending 0`（门控生效：死亡从 frame 0 起，流式从未激活）；同截图确认
    "You died! / Press R to respawn" 正常渲染。
  **E2E 过程发现（均非本阶段引入，记入 KNOWN_ISSUES / PITS）**：
  1. **"You died!" 实际是白字**：`ui.frag` textured 批次输出 `texel.rgb`
     （字体纹理白字），`u_color` 只调制 alpha（着色器注释记载的既有
     设计）→ 死亡屏幕传的红色 `Vec4{1,0.35,0.35}` 被静默忽略（合成
     "needs:" 暗色、tooltip 颜色同样无效）。P16 起死亡屏幕从未被像素级
     E2E 验证。P34 候选：着色器改 `texel.rgb × u_color.rgb` + 受影响
     屏幕重新验证。
  2. **存档加载到空中绕过坠落伤害**：`fallStartY` 只在"离地"转换帧记录；
     加载到空中的玩家无此转换，`fallStartY` 保持 0 → `fall = 0 − 落地Y
     < 0` → 0 伤害（探针实证：46–59 格自由落体全部满血落地）。正常游玩
     不受影响（出生/重生都在地面）。P34 候选：`applySave` 置空中
     `fallStartY = pos.y`（一行 + 单测）。
  3. **流式激活方向**（PITS 新坑位）：激活需玩家距缺失列 ≤1（站在已加载
     区**边缘**）；演示区内部玩家永不激活即使半径方未满——P33 首版 E2E
     把玩家放中心，720 帧 0 条流式日志，场景白做。
  4. **日志 ANSI 色码**（PITS）：logger 重定向到文件也写 `\x1b[32mINFO`
     色码，解析正则须先剥离。
  5. **存档 time 含源存档偏移**（PITS）：5f 源存档的 time 已含 5 步，
     断言起点不能假设 = config start_time_of_day。
  680/680（无新增单测——Game 级行为按既有惯例走 E2E 验证）。

## 会话存档

- **提交**：P33 提交（本阶段唯一提交，hash 见 `SESSION_CONTEXT.md` 提交
  列表）。
- **目标**：用户看日志怀疑"内存泄漏"——死亡后（玩家冻结、未行走）流式
  仍在加载。诊断结论：非泄漏（有界：半径方填满即停；粒子/mob/箭/掉落物
  均有上限），根因 = 流式"填满玩家周围半径方"的设计 + 死亡不暂停 +
  `view_distance` 80 太大（静止上限 207,368 chunk）。用户确认做建议 1
  （死亡暂停流式）；建议 2（调小 view_distance）为用户自决（config 已可
  自调），建议 3（并行 diff 存档，13.6s 退出）留作 P34 候选。
- **范围决策**：只门控 `streamChunks()` 一处——最小改动、保留迟滞状态、
  在飞 job 自然排空。不加"死亡时暂停昼夜/mob"选项（超出用户确认范围，
  且改变更多确定性行为需额外 E2E 论证）。
- **E2E 方案演进**：首版用"高 y 存档摔死"（100→46 格坠落）→ 发现坠落
  伤害被绕过（上述发现 2）→ 改"虚空落体"→ 发现死亡发生在 frame 1、
  早于流式 post（门控不可观测）→ 终版"饿死"（probe config 1s 节奏 +
  边缘列）：死亡时刻解析确定（40.0s 整）、全程无输入、门控前后流式
  活动可区分（半径 25 方在 60fps 下 43.1s 才满 > 40s 死亡）。
- **E2E 环境实测**：view_distance 25（~2 万 chunk / ~5000 draw call）在
  本机（Mesa Intel Arc）≈ 20 fps——大半径渲染是 draw-call 受限（用户
  日志中 view_distance 80 流式期 ~0.6s/列 同因）。
- **废弃脚本**：`/tmp/opencode/p33_e2e.zsh`（批次驱动）、
  `p33_probe.zsh`（边缘列地形探针）、`config_p33die.json`、
  `p33die_edited.json`、`p33void.json` 等（/tmp 一次性）。
