# Phase v3.15.0 — V3-13 死亡统计屏（门 205 全 PASS，2026-11-06）

> 用户 11-06 目标定案「完成 3.x 任务矩阵，从小到大推进，完成所有编号」
> 的第 1 项（最小项）。零 format / 零 config 键 / 无输入结构免疫。

## 范围与定案

- **死亡屏 2 行 → 统计屏**：标题「你死了！」+ 致死原因行 + 会话统计
  5 行（采挖/放置/击杀/行走/存活）+「按 R 重生」。
- **致死原因**：`Player::damage` 新增 `DamageInfo` 重载（2 参旧签名
  保留 = 委托 None 源，pre-V3-13 位级路径）；**仅致死一击**的来源记
  录（非致死打击不更新）。9 措辞全字体表内（逐字查 418 字表定措辞：
  摔/掘/外/从/坠 缺字 → 改「死于高处掉落」式）。
- **会话统计**：`Game::SessionStats`（采挖 = breakBlock 单收口 / 放置
  = placeBlock 双 setBlock 点含 slab 合并 / 击杀 = 玩家致死的近战 +
  箭）+ Player 水平位移累计（行走，传送阈值守卫）+ 存活 = 帧锁定
  m_frameCount/60。**不存档 = 记档偏离**（MC scoreboard 持久；会话级
  随进程重置）。击杀语义 ≠ SURV-7 持久 `kill.<name>`（全部死因）——
  记档区分。
- **Game Over 屏 = 死亡屏本身**（无 hardcore 模式，R 重生既有 = 记档
  偏离 MC 双屏）。

## 字体 +1 字形「于」（418 → 428 CJK 集）

`tools/generate_font.py`：CJK_CHARSET 代码点序插入 0x4E8E（+
CHARSET_COUNT）。重跑 = TTF pin 验证（NotoSansCJK sha256）+ legacy 33
位级自检 + top-512 红线恒等 + **扩展区重排格**（0x4E8E 之后全部 +1
格；atlas 尺寸 256×2112 不变，于 = cell 150）+ snippet 输出 → 手贴
`src/ui/font.hpp` kCjkGlyphs[428]（tools 永不碰 src/ 纪律）。
**零像素论证**：无输入渲染无 CJK 文本（教程 = locked pointer 才显）
→ S1 锚恒等（门内双跑复核）。test_font 4 锚重钉：PngBytes FNV
（da630ef8… → db4bbdf1…）/ TablePinned（428 尺寸 + 成员格 +1）/
ExtendedCellsPinned（全向量重钉；**vector 末元素无尾逗号 = 正则必漏**
单查修复）/ UvCellPositions（于新增 + 猪/镐/默/魔 +1 格）。

## 实现（代码 8 文件）

- `src/player/player.hpp`：DamageSource 枚举 9 值 + DamageInfo（类顶
  部声明——类内前向引用非法首跑捕获）+ damage 双签名（2 参保留 +
  3 参 info）+ deathInfo() + deathCauseLine() 纯函数 + statsDistance()
  + m_statsDistance/m_updateStartPos（private）。
- `src/player/player.cpp`：damage 2 参委托 3 参；致死分支记
  m_deathInfo（仅 source != None）；respawn 清原因；内部 4 调用点
  （fall/starve/lava/void）传源；update 尾 = 水平位移累计（dx+dz < 1.0
  阈值守卫 = 传送/重生跳不计数）。
- `src/core/game.hpp`：SessionStats（Game 类内，SURV-7 m_stats 旁——
  **首跑插错 Args 结构体**门内编译捕获）。
- `src/core/game_interaction.cpp`：breakBlock ++blocksMined /
  placeBlock 双点 ++blocksPlaced / 近战致死 ++mobsKilled。
- `src/core/game_entities.cpp`：箭→玩家 {Arrow} / 箭→mob 致死
  ++mobsKilled / mob hit callback {Mob, killerName}（**init-capture
  def.name**——lambda 捕获首跑漏捕获编译捕获）。
- `src/core/game_frame.cpp`：溺水 {Drown}（bypassArmor 保留）。
- `src/core/game_ui.cpp`：效果 tick {Effect, def.name} / 死亡统计屏
  8 行（固定物理偏移——**fScale 仅 font_scale 不含 ui.scale** 首版
  8 行叠一行门内捕获，PITS 397）。
- 版本三件套：CMakeLists 3.14.0 → 3.15.0 / test_debug_overlay F3 钉。

## 门 205（全 PASS）

- **S1 结构免疫第 85 次**：无输入 60f ×2 ≡ base19 逐字节 + savenorm
  EXACT（死亡屏不渲染 + 字体扩展区重排格零像素——**字体变更后双跑复
  核**）。
- **S2 坠落死亡场景臂**：场景手术（种子 123456789 存档挖 spawn 列
  y30..53 = 24 格竖井，24 − 3 偏移 > 20 HP 必死）→ 无输入 ×2：
  **RAW 恒等 + savenorm EXACT**（fall = 帧锁定物理 = 死亡帧确定；
  saturation = 已知 real-dt 字段 savenorm 域）+ 死亡屏裁剪目检（「死
  于高处掉落」+ 5 统计行 + 提示全渲染）。
- **S3 全套**：2792/2792（+5：DeathCauseKillingBlowWins /
  DeathCauseMobCarriesKillerName / DeathCauseLineFontSafe【9 措辞钉】
  / RespawnClearsDeathCause / StatsDistanceWalkAndTeleportGuard），
  `-Werror` 0 警告。
- **S6**：无（零新解析面：DamageInfo / SessionStats 均不序列化）。
- **性能 A/B（用户硬要求）**：旧 = d61a757 v3.14.0 + g139 vs 新 +
  g139（banner 实证）：900f 无输入——PERF-WIN 三窗 upd med
  0.588/0.634/0.629 vs 0.655/0.624/0.575 + render/swap 同档 +
  fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3/1 逐位
  恒等** + **终档 savenorm EXACT**（死亡屏/统计 = 死亡态渲染面，无输
  入零触碰）。

## 记档偏离

1. **会话统计不存档**（MC scoreboard 持久 = 偏离；format 零变更纪律）。
2. **Game Over 屏 = 死亡屏**（无 hardcore = MC 双屏收敛一屏）。
3. **击杀语义**：统计屏 = 玩家致死（近战 + 箭）会话级；F3 `kill.<name>`
   = 全部死因持久（SURV-7）——两个数可不同，各归其位。

## PITS

- **PITS 397**（门 205 双捕获）：① fScale(s) = s × font_scale（**不含
  ui.scale**）——文本行布局按 fScale 算行距 = 低估 ui.scale 倍（8 行叠
  一行），固定物理偏移 + 26px 行距纪律（demo 面板先例）。② 加 CJK 字
  工作流（charset + 生成器 + snippet 手贴 + 4 锚重钉）；ExtendedCells
  vector 末元素无尾逗号 = 正则重钉必漏。③ 扩展区重排格零像素论证 =
  无输入无 CJK 渲染（教程 locked 才显）→ S1 恒等。

## 变更文件（显式路径，20）

src/player/player.hpp / src/player/player.cpp / src/core/game.hpp /
src/core/game_interaction.cpp / src/core/game_entities.cpp /
src/core/game_frame.cpp / src/core/game_ui.cpp / tests/test_player.cpp
/ tests/test_font.cpp / tests/test_debug_overlay.cpp / CMakeLists.txt /
tools/generate_font.py / assets/textures/font.png / src/ui/font.hpp /
AGENTS.md / README.md / docs/CHANGELOG.md / docs/BACKLOG.md /
docs/GAMEPLAY.md / docs/MECHANICS.md / docs/MECHANICS_WORLD.md /
docs/PITS.md / docs/KNOWN_ISSUES.md / docs/archive/phase-3.15.0.md
（本文）。

Release 政策 = 延续：v3.15.0 只留 **tag + 提交**，无新 release。
