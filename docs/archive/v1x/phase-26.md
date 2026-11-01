# Phase 26 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 26 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 26]` 被动 mob 家族仅 cow（P22）——无 sheep/chicken** — Phase 26 按 P22 已验证的纯数据流落地 sheep/chicken（**零新增游戏代码**，纯数据 + 纹理 + 单测 + E2E）。**数据** `data/mobs/sheep.json`（HP 8、speed 0.5、全 0 攻击字段、`wander_interval` 3.0、`spawn_max_light` 15、掉 `wool`；box 头 0.4³ + 身 0.8×0.8×1.2，AABB 1.2×1.5 同 cow）/ `data/mobs/chicken.json`（HP 4、speed 0.6、`wander_interval` 2.0、掉 `feather`；box 头 0.25³ + 身 0.3×0.4×0.5，AABB 0.5×0.8 = **首个比 zombie 小的模型**——生成格/重叠/间距规则全用模型 AABB 数据，无改动）/ `wool` + `feather` material 物品（max_stack 64；图标 `tools/generate_item_icons.py`：wool = 白色绒毛板【复用 leather 的轮廓逻辑】、feather = 米白斜羽干 + 深色中轴）/ mob 纹理 4 tile（`tools/generate_mob_textures.py`：sheep 白绒毛 + 深色中性眼【噪声幅度 12 略高于 skin = 绒毛感】、chicken 米白羽 + 深色眼 + 红冠 rows1-2 + 橙喙 rows7-8；既有 6 个输出重跑逐字节零变化）/ atlas 23→27（`block_atlas.json` 新 tile 只占空位 [3,5]/[0,6]/[1,6]/[2,6]，既有坐标零移动）/ `mob_spawning.json` 名单 [zombie, cow, sheep, chicken]——类型抽取 = `pick % 名单长度`（种子 ⊕ 帧号），**扩名单改变各帧抽中类型**（f60：cow → chicken），但 **base 60 帧运行在首次尝试之前结构上零尝试**（`first_spawn_frame=60`：主循环 `update()` 执行时 `m_frameCount` 尚为 59、不满足 `>=60`，`++m_frameCount` 在其后 → 第 61 轮才首次尝试）→ **基线结构性不受名单影响**（P20 设计意图 "`first_spawn_frame=60` 保证 60 帧基线无 mob" 对任意名单成立）。**测试** 643/643（+4：`ParsesSheep`/`ParsesChicken` 完整被动解析【含小模型 AABB 0.5×0.8 尺寸断言】、`RejectsChickenBoxOutsideAabb`【头 box 顶 0.85 > AABB 0.8 拒绝——小模型下的 box-in-AABB 校验】、`ParsesFourMobList`【4 类型名单顺序断言——顺序是确定性抽取的锚】）。**E2E**（P25 协议：零并发 + 每拍前 unmap 覆盖窗 + ≥6s 窗口；白天场景 day_length 240 + start 0.25，360f → t=0.275 仍在正午平台、dayFactor 1.0 天空恒定）：base18 门 ×2 逐字节不变（结构论证 + 实证）；白天 360f mobs-on ×2 与 mobs-off ×2 各自逐字节一致（mobs-off == base 逐字节一致 = 正午平台静态场景），vs 对照 **15535 px 差 = 恰好 4 个 mob 剪影**（4 x-cluster：sheep x[216,310] y[164,238] 3715 px（树冠部分遮挡）/ 左 chicken x[346,412] y[326,405] 4029 / 中 chicken x[519,559] y[319,395] 2676（小模型 → 窄）/ cow x[660,734] y[243,320] 5115（最大）；与确定性预测逐一对应：f60/f90 chicken 生成、f120/f150/f210 zombie 光门拒绝、f180 cow 生成、f240 sheep 生成（`max_mobs` 4 满）、f270 chicken / f300 cow / f330 cow 全部 cap 拒绝）/ **zombie 绿 0 px**（白天敌对压制保持）/ 心带、饥饿带、热栏带全 0（无伤害、无饥饿衰减、无库存变化）。**踩坑（新教训）**：E2E 分析脚本自写 PNG 解码器按 RGB 3 通道假设，而游戏截图是 **RGBA 4 通道**（`takeScreenshot` 写 alpha=255）→ 通道错位产生假"周期性纹理错位 diff"（7 个跨屏 cluster + 大量 R/G/B 恰 255 饱和像素），极易误诊为 atlas UV 错位；改用 PIL 解码后正确（4 cluster、全在 mob 剪影内）。教训：**E2E 像素分析一律用标准库解码（PIL），先对已知颜色像素（天空/地面）做 sanity check 再统计**

## 会话存档（原 SESSION_CONTEXT）

- **提交** `3584137`（17 文件，+310/−14；**零新增/改动游戏代码**——纯数据 +
  纹理 + 单测 + 文档）。
- **目标**：P26 候选第 1 条「更多 mob」的被动部分——按 P22 已验证的纯数据流
  落地 sheep（掉 wool）/ chicken（掉 feather）。skeleton（第二种敌对 mob）需
  mob 端发射新代码，留 P27 候选 1。
- **数据**：
  - `assets/data/mobs/sheep.json`：HP 8、speed 0.5、detect/attack 全 0
    （attack_cooldown 1.0 = 被动约定）、wander 3.0、spawn_max_light 15、掉
    wool×1；box 头 0.4³（pos [0, 1.25, −0.4]）+ 身 0.8×0.8×1.2（pos
    [0, 0.9, 0]），AABB 1.2×1.5（同 cow）
  - `assets/data/mobs/chicken.json`：HP 4、speed 0.6、wander 2.0、掉
    feather×1；box 头 0.25³（pos [0, 0.6, −0.1]）+ 身 0.3×0.4×0.5（pos
    [0, 0.25, 0]），AABB 0.5×0.8 = **首个比 zombie 小的模型**（生成格/重叠/
    间距规则全用模型 AABB 数据，零改动）
  - `assets/data/items/wool.json` / `feather.json`：material、max_stack 64、
    同名 icon（目录自动加载，无 CMake 改动）
  - `assets/data/mob_spawning.json`：名单 [zombie, cow, sheep, chicken]
  - `assets/data/textures/block_atlas.json`：+4 tile（sheep_body [3,5] /
    sheep_head [0,6] / chicken_body [1,6] / chicken_head [2,6]，23→27，只占
    空位、既有坐标零移动）
- **工具**：`tools/generate_mob_textures.py` + `sheep_wool_tile`（白绒毛底
  (228,226,220) + noise 12【绒毛感】+ 深色中性眼 (40,38,34)）+ `chicken_tile`
  （米白羽 (238,236,226) + noise 8 + 深色眼 (35,30,25) + 红冠 rows1-2
  (178,44,38) + 橙喙 rows7-8 (228,148,44)）；`tools/generate_item_icons.py` +
  `wool`（白色绒毛板，复用 leather 轮廓逻辑 salt 61）+ `feather`（米白斜羽干
  + 深色中轴 salt 62-65）。既有 6 mob 纹理 + 12 图标重跑逐字节零变化。
- **关键结构论证（基线为何不受名单影响）**：类型抽取 =
  `splitmix64(seed ⊕ 0x9E3779B97F4A7C15×frame) % 名单长度`（game.cpp
  attemptMobSpawn）→ 名单 2→4 改变各帧抽中类型（f60：cow → chicken）。但主
  循环 `update() → render() → swapBuffers() → ++m_frameCount`，60 帧运行的
  `update()` 在 `m_frameCount = 0..59` 执行、`>= first_spawn_frame(60)` 永不
  成立 → **首次尝试发生在第 61 轮（m_frameCount=60）** → 60 帧运行结构上
  零尝试 → base18 门对任意名单免疫（P20 设计意图 "first_spawn_frame=60 保证
  60 帧基线无 mob" 成立，实证 ×2 字节一致）。
- **单测** 643/643（+4，tests/test_mob.cpp）：`ParsesSheep`（完整被动解析：
  HP 8 / spawn_max_light 15 / drops wool / AABB 1.2×1.5）/ `ParsesChicken`
  （小模型：AABB 0.5×0.8 / wander 2.0 / drops feather）/
  `RejectsChickenBoxOutsideAabb`（头 box 顶 0.7+0.15=0.85 > AABB 0.8 拒绝——
  小模型下的 box-in-AABB 校验）/ `ParsesFourMobList`（4 类型名单顺序断言——
  顺序是确定性抽取的锚）。
- **E2E**（P25 协议：零并发 + 每拍前 `unmap_covers`；驱动
  `/tmp/opencode/p26_e2e.zsh`；白天场景 = config_xep：day_length 240 + start
  0.25 + seed 123456789，无存档 = 出生点）：
  - base 60f ×2：**双双 == base18 逐字节**（结构论证 + 实证）
  - day_mobs 360f ×2（t = 0.25 + 6/240 = 0.275，仍正午平台，dayFactor 1.0
    天空恒定；~6.9s 墙钟 > 5s 冻结窗）：dm_a == dm_b 逐字节
  - day_nomobs 360f ×2：dn_a == dn_b，且 **dn_a == base_a 逐字节**（正午平台
    静态场景：无 mob、无 HUD、天空恒定 → 60f 与 360f 同图）
  - **dm vs dn：15535 px 差 = 恰好 4 个 mob 剪影**（PIL 解码；4 x-cluster）：
    sheep x[216,310] y[164,238] 3715 px（左树冠下，部分遮挡）/ 左 chicken
    x[346,412] y[326,405] 4029 px / 中 chicken x[519,559] y[319,395] 2676 px
    （小模型 → 窄剪影）/ cow x[660,734] y[243,320] 5115 px（最大剪影）
  - **与确定性生成预测逐一对应**（seed 123456789，normal 节奏 30，360 帧运行
    尝试帧 = m_frameCount 60..330）：f60 chicken 生成#1 / f90 chicken 生成#2 /
    f120 zombie 光门拒绝（正午）/ f150 zombie 拒 / f180 cow 生成#3 / f210
    zombie 拒 / f240 sheep 生成#4（max_mobs 4 满）/ f270 chicken、f300 cow、
    f330 cow 全部 cap 拒绝 → 终态 = 2 chicken + 1 sheep + 1 cow（截图可见
    红冠、白斑、绒毛、米白羽——四型全验证）
  - **zombie 绿 0 px**（白天敌对压制保持）
- **踩坑 1（RGBA）**：自写 PNG 解码器按 RGB 3 通道假设 → 游戏截图实为 RGBA 4
  通道 → 假"周期性纹理错位 diff"（7 个跨全宽 cluster + R/G/B 恰 255 饱和
  像素），险误诊为 atlas UV bug（本阶段刚改过 block_atlas.json）。改 PIL 后
  4 cluster、全在 mob 剪影内。教训详见坑位合集。
- **踩坑 2（HUD 门控澄清，影响 E2E 判据设计）**：整个 HUD（hotbar + 心 +
  饥饿）在 `renderUi` 里由 `if (locked && !panelOpen())` 门控（game.cpp
  ~2604）→ **无输入运行（指针从不锁定）根本不画 HUD** → "心带/饥饿带/热栏带
  零差"在无输入 E2E 中是**平凡满足**的（没有东西可差），真正的判据是 ×2 字节
  一致 + 世界区剪影 diff。饥饿衰减（4s 真实 dt，P23）在无输入运行中真实发生
  但截图不可见（只能经 --save 的 `playerHunger` 观察）。**P25 存档中
  "pe1 含 234px hunger 1-pt" 的标注据此修正：那是饥饿带区域内的世界像素
  （mob 移动），不是 HUD 图标**——P25 各判据本身（×2 一致、僵尸绿、存档
  health）不受影响。后续 E2E 若要验证 HUD 相关状态，须用锁定输入运行或
  --save 取存档值。
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length
  600），与 P26 无关。
