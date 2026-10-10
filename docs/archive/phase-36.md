# Phase 36 细节存档

> 用户确认方向（原话）："先2+3打包。"（= P36 候选 ② 死亡屏白字修复 +
> ③ 存档空中加载绕过坠落伤害，均在 P33 E2E 顺带发现、列 KNOWN_ISSUES）

## 阶段报告

- **`[2026-10-10, Phase 36]` UI 着色器文字着色修复 + 读档坠落追踪重钉
  （用户确认"先2+3打包"）** — 两个独立小修复打包。

  **① 死亡屏白字（P33 发现）**：`assets/shaders/ui.frag` textured 分支
  原为 `vec4(texel.rgb, u_color.a * texel.a)`——u_color 只调制 alpha
  （P15 起着色器注释有记载的既有行为），死亡屏 "You died!" 传入的红色
  `Vec4{1, 0.35, 0.35}` 被静默忽略、按字体纹理白渲染。修复 =
  `vec4(texel.rgb * u_color.rgb, u_color.a * texel.a)`。**影响面审计**：
  全部 textured 绘制（字体文字 / 物品图标 / 心·饥饿带 / 裂纹叠加）的
  u_color.rgb 均为白（1,1,1）→ 逐位不变；唯一非白 = 死亡屏 died
  （1,0.35,0.35,1）→ 只有该行文字变色（0.35×255 = 89.25 → 墨像素精确
  (255,89,89)：字体 alpha 二值 + u_color.a=1 → 无混合）。hint 行（kText
  白基）保持白；kTextDim = 白 0.55 alpha（暗化走 alpha，原本即生效，
  不变）。

  **② 存档空中加载绕过坠落伤害（P33 发现，探针实证 46–59 格自由落体
  满血落地）**：`fallStartY` 只在"离地"转换帧记录（player.cpp 生存节），
  读档走 `Entity::teleportTo`（只设位置+清速度，不碰 Player 坠落追踪）
  → 空中读档没有离地转换，`fallStartY` 保持初始 0 → 落地结算
  `fall = 0 − 落地Y < 0` → 0 伤害。修复 = `Player::restorePosition(pos)`
  （player.hpp 内联：`teleportTo(pos)` + `m_fallStartY = pos.y` 重钉），
  `Game::applySave` 的 `m_player->teleportTo(save.playerPos)` 改走
  `restorePosition`。地面读档的"just landed"帧见净下落 0 → 不造伤害
  （单测守护）。为何不动构造/respawn：出生/重生点均为干燥陆地（硬性
  规则 10），其后的离地转换会重钉——缺转换的唯一路径是读档。

  **测试 698/698（+2）**：`RestorePositionMidAirTakesFallDamage`
  （空中恢复 10 格 → 净下落精确 10.0【恢复/落地都在整数平面】→
  10−3 = 7 HP 精确）/ `RestorePositionAtRestTakesNoDamage`（地面读档
  零伤害）。`-Wall -Wextra -Wpedantic -Werror` 0 警告。

  **E2E**（`/tmp/opencode/p36_e2e.zsh` + F3 重跑
  `/tmp/opencode/p36_f3_rerun.zsh`；19 张截图全部 1280×720）：
  - **无输入基线（第 11 次结构性免疫 + A/B 重锚）**：base_a/base_b
    （60f）、daym_a/daym_b（360f）配对字节一致，且 **== P35 代码参考构建**
    （refbase1/refbase2/refdaym1）——P36 对无输入渲染逐位 no-op（shader
    改动只被 UI textured 绘制采样【无输入不绘 UI】、fallStartY 改动只在
    applySave 路径【无 --save 不触发】）；skygate ×7 全 FRESH
    （mode (120,181,242)）；`base18.png` / `p36_daym_ref.png` 重锚。
  - **死亡屏 A/B（红字生效）**：红字 bbox A (488,320,282,24) n=1704、
    B (336,280,564,48) n=6816——**与字形表预测逐像素一致，B 宽高恰好
    2×A、像素恰好 4×A**；白字 bbox 只剩 hint 行 A (489,372,298,15)
    n=833、B (338,384,596,30) n=3332（与 P35 白字 hint 预测一致——
    died 行不再是白）；存档 health 0 ×4 + 存档对（pos[1] 豁免 ~3e-5，
    P34 约定）+ 截图 A 对/B 对各字节一致。
  - **fall A/B（空中读档坠落伤害生效）**：存档 pos [0.5,62,0.5]（出生
    地面 54 上方 8 格）加载 → 落地后 **health = 15.0 精确**（20 − (8−3)；
    修复前 = 20 的绕过）+ pos = [0.5,54,0.5] 精确 + yaw/pitch 不变 +
    无块/流体编辑；存档对、截图对各自字节一致。
  - **面板 A/B（白 UI 逐位不变）**：槽 5→8 搬运 + 槽 6 木棍不动 +
    selected 0 + pos/yaw/pitch 不变 + 无编辑（pcheck ×4）；A 对/B 对
    存档与截图各自字节一致、**跨尺度存档逐字节相同**；P35 像素断言照搬
    且**数值与 P35 完全相同**：面板暗底 (4,300) (26,25,29) / (1276,300)
    (32,40,54)、stick 图标 180/289 px。
  - **F3 A/B（白字渲染不变）**：白墨 **10993px ×2 = P35 精确值复现**
    （白字在新着色器下逐位不变的直接证据）+ 存档对字节一致（确定性
    权威）+ 首行常数 crop (8,8,460,24) 字节一致。
  - 重试统计：主批 12 次（全部 WM 竞态或 F3 墨水误判——见环境事故 ④，
    0 次真实失败）、F3 重跑 0 次；0 FATAL（最终工件）。

  **环境事故（4 项 → PITS 新坑位）**：
  1. **/tmp 清理抹掉全部 E2E 参考资产与批脚本**（P33→P36 间隔 3 天）：
     `base18.png` / `p28_daym_a.png` / `config_xep.json` / `skygate.py` /
     `p35_e2e.zsh` 全失。重建：probe 配置值 = 代码默认配置（批脚本内重新
     生成 + 断言内容——"游戏对缺失 config 自动补种子"的 P34/P35 坑位
     照旧适用）；**跨阶段基线门改用 P35 代码参考构建 A/B**（`git
     worktree add /tmp/opencode/p36ref bebab7c` + 独立 build，同 SDL
     0.4.18——比旧参考图更严格，还顺带隔离系统库升级影响）；`base18.png`
     从验证过的运行重新生成、链条经 A/B 重锚（第 11 次结构性免疫）。
  2. **SDL3 系统升级 0.4.16 → 0.4.18**：CMake 缓存指向已消失的
     `/usr/lib/libSDL3.so.0.4.16` → 链接期报错 → 重跑 `cmake -B build`
     重新解析即愈（0 警告保持）。
  3. **zsh 的 `set -- $var` 不做词分割**（与 bash 不同）：`pair="a b"`
     下 `set -- $pair` 只设 `$1="a b"`、`$2` 未设，`set -u` 下致命错误
     （首跑批死在第一个 pair 循环）→ 要分割用 `set -- ${=pair}`。
  4. **awesome WM 平铺新窗口是异步的、还会顺手改尺寸**：launch 后一次性
     量宽（1280 ✓）可能赶上平铺前的旧值，随后 WM resize 到平铺尺寸
     （实测 1272×1552）——游戏按窗口尺寸渲染，截图 = 1272×1552，整拍
     作废（P36 首拍 refbase1 即如此废掉；P35 的一次性宽度检查防不住）。
     对策：launch 后**轮询**直到 1280×720（≤12 次，miss 时重新
     float/move/raise）+ **运行后校验游戏日志
     "Screenshot saved: … (WxH)" = 1280x720**（中途被平铺过 = 作废）+
     shot3 重试环（≤3）+ 批 preflight `pgrep -f "[b]uild/bin/minecraft"`
     （方括号技巧防 pgrep 包装 shell 自匹配）。

  **文档同步**：KNOWN_ISSUES（白字条目 + 空中读档条目 → 已修复划线 +
  索引行 36 + "玩家速度/onGround 不存"条目加 P36 交叉注记）/ MECHANICS
  （生存节坠落伤害句 + UI 节文字着色句）/ PITS +4 / ROADMAP（P36 行 +
  索引）/ AGENTS 现状（P0–P36、698/698、第 11 次 + 参考重锚注记）。

## 会话存档

### 实现

- `assets/shaders/ui.frag`：textured 分支 `texel.rgb × u_color.rgb` +
  注释更新（记载修复原因与"白 u_color.rgb = 逐位不变"论证）。
- `src/player/player.hpp`：`restorePosition(Vec3)` 内联方法
  （`teleportTo` + `m_fallStartY = pos.y`），注释说明重钉理由。
- `src/core/game.cpp`：applySave 玩家节改 `restorePosition` + 注释。
- `tests/test_player.cpp`：+2（生存节内新小节 "save restore (Phase 36)"）。
- 变更文件共 4（+ 文档 7：KNOWN_ISSUES / MECHANICS / PITS / ROADMAP /
  AGENTS / 本文件）。

### E2E 设计（批脚本 `/tmp/opencode/p36_e2e.zsh`）

- 参考构建（P35 代码 `bebab7c`，worktree `/tmp/opencode/p36ref`）：
  refbase1/refbase2（60f 无输入）、refdaym1（360f）。
- P36 无输入基线：base_a/base_b（60f）、daym_a/daym_b（360f）——
  配对字节一致 + A/B vs 参考构建字节一致（P36 对无输入渲染 = no-op：
  shader 改动只被 UI textured 绘制采样【无输入不绘 UI】、fallStartY
  改动只在 applySave 路径【无输入运行无 --save】）= 第 11 次结构性免疫；
  base18.png / p36_daym_ref.png 重锚。skygate（mode = (120,181,242)
  FRESH 天空色）全部无输入拍。
- 死亡屏 A/B（void 存档 pos y=−33、无输入、60f、scale 1/2 各 ×2）：
  **红字** bbox A (488,320,282,24) / B (336,280,564,48)（B 恰好 2×A，
  字形表预测同 P35 白字几何）+ 红色墨像素计数（A > 500、B > 1000，
  范围 r≥250/g∈[80,100]/b∈[80,100] 容驱动舍入）+ **白字 bbox 只剩
  hint 行**（A (489,372,298,15) / B (338,384,596,30)——证明 died 行
  不再是白）+ 存档 health 0 + 存档对（pos[1] 豁免，P34 约定）+ 截图对。
- fall A/B（空中读档存档 pos = [0.5, 62, 0.5] = 出生地面 54 上方 8 格、
  无输入、180f）：存档为权威——**health == 15.0**（20 − (8−3)；修复前
  = 20 的绕过）+ pos == [0.5, 54, 0.5] 精确 + yaw/pitch 不变 + 无块
  编辑 + 存档对/截图对字节一致。
- 面板 A/B（输入驱动、E 开面板、slot 5 → slot 8 搬运，坐标同 P35：
  scale 1 (458,436)/(626,436)、scale 2 (406,456)/(620,456)、300f、
  pcheck + 重试环）：白 UI 在新着色器下逐位不变 → P35 像素断言照搬
  （面板暗底 (4,300)/(1276,300)、stick 图标 180/289 阈值）+ 存档对/
  跨尺度存档/截图对。
- F3 A/B（中心点击锁定 + F3、无相机输入、180f）：白墨 > 10000（P35 同
  场景实测 10993——白字逐位不变则计数应精确复现，作加分确认）+ 存档对
  字节一致（确定性权威）+ 首行常数 crop (8,8,460,24) 字节一致。

### 已知问题/限制

- 旧 base18.png（P18 锚定的参考图）已随 /tmp 清理永久丢失——参考链经
  P35 代码 A/B 重锚，跨阶段（P18→P35）的字节连续性无法再回溯验证
  （P33/P34/P35 各批的 == base18 门当时都通过过，链条当时连续）。
- SDL 0.4.16 → 0.4.18 升级本身对渲染的影响无法验证（旧库已从系统消失）
  ——P36 的 A/B 只证明"同 SDL 下 P35 代码 ≡ P36 代码"。
