# Phase 27 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 27 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-06, Phase 27]` 敌对 mob 仅 zombie（近战）——无远程 mob** — Phase 27 落地 skeleton（第二个敌对 mob、首个远程——**打破 P22/P26 纯数据流，新增代码**）。**数据** `data/mobs/skeleton.json`（HP 20、speed 0.8、`detect_radius` 16、`attack_range` 0 = 永不近战、`attack_damage` 2 = 箭伤、`attack_cooldown` 1.5、新可选字段 `shoot_range` 14 / `retreat_distance` 3 / `shoot_speed` 15（均默认 0 = 近战 mob）；`spawn_max_light` 0、`hostile` true = P25 自动进保护环/伤害缩放/生成节奏、掉 `arrow`（既有 material）；box 模型同 zombie 尺寸 0.6×1.8）+ 严格校验（负数拒绝、`shoot_speed` **显式**值须 (0,100]（缺席=0 合法、显式 0 是笔误）、`shoot_range>0` 必须有 `shoot_speed` 跨字段拒绝）/ 名单 [zombie, cow, sheep, chicken, skeleton]（抽取 = pick % 5）/ 纹理 2 tile（`skeleton_tile`：骨白 + 噪声，skull 深色眼窝 rows5-6 + 鼻腔 rows7-8；atlas 27→29 只占空位 [3,6]/[0,7]，既有坐标零移动）。**代码** `MobState::RangedAttack`（`shoot_range>0` 且玩家水平 ≤shoot_range 且高度差 ≤2：面向玩家 + 每 `attackCooldown` 经 shoot 回调放箭；≤retreat_distance 全速后撤**边退边射**（Minecraft kite）；超 shoot_range 按 Chase 逼近；`attack_range`=0 使近战分支对远程 mob 永不成立，纯数据表达）/ `Projectile` +`hurtPlayer`（默认 false）——游戏扫掠测试**先**对玩家 AABB（命中 = P16 受伤路径 + 箭 `kill()`；玩家自己的弓箭不带标志、仍穿过玩家）+ `source`（射手指针）——**箭不伤射手**：箭 spawn 点 = 头部 + 0.3 块瞄准偏移，仍在 0.6 宽身体的 AABB 内 → 下一帧扫掠 t=0（起点在盒内）自命中，每支箭自杀自灭 + 把射手击退向玩家（E2E 实证：6 箭全灭、skeleton 半速漂移、health 恒 20；命中循环跳过 `source` 修复）/ 放箭瞄点 = 玩家眼高 + **0.5·g·(d/speed)² 弧线提前量**（箭受重力 20 b/s²，无补偿时直线瞄只命中 ≤6.3 块，10 块外箭从玩家脚下飞过——E2E 实证 health 恒 20；`Projectile::kGravity` 转 public 供瞄点复用同一常量）/ `MobEntity::eyePosition()`（模型高 ×0.8 = 头部，放箭起点）/ 生成 debug 日志（cap 拒绝 / 抽中类型无格 / 生成位置 / skeleton 射击 / 箭命中玩家 / 箭插墙，`--log-level debug`，长期保留）。**测试** 650/650（+7：`ParsesSkeleton` 完整解析【含 ranged 三字段 + hostile + 掉 arrow】、`RejectsRangedFieldErrors`【负数 ×2 / 显式 0 / >100 / 跨字段缺 shoot_speed】、`RangedMobShootsAndHoldsDistance`【8 块外 300 帧：全程 RangedAttack、恰 4 发【冷却起点 0 + 1/60 浮点残差】、零近战、零水平漂移】、`RangedMobRetreatsWhenClose`【1 块贴近：首帧 velocity 背离玩家、300 帧后退至 >3 块、仍开火、零近战】、`ParsesFiveMobList`、`HurtPlayerFlagDefaultsFalseAndSettable` + `SourceDefaultsNullAndSettable`）。**E2E**（P25 协议：零并发 + 每拍前 unmap 覆盖窗 + 天门；**夜晚换 seed 9 手工存档 p27far**——seed 123456789 的 pick 流在 p25far 位置前 4 抽 = zombie/zombie/sheep/chicken、cap 于 f150 满、**skeleton 永不生成**（E2E 实证 health 恒 20 两轮；probe 工具用仓库 WorldGenerator 重放 spawn 扫描 + 表面高度，手工造 seed 9 存档：玩家 (22.5,45,0.5) = 世界出生点 +22 块、表面 44+1，保护环 R=12 外）：base18 门 ×2 字节不变（名单 4→5 结构性免疫第三次验证）、白天 360f mobs-on ×2 与 mobs-off ×2 逐字节一致（mobs-off == base = 正午平台）、vs 对照 **36607 px = 4 被动剪影**（2 sheep + chicken + cow，与 %5 pick 序列 f120/f150/f180/f240 逐一对应，f60/f90 zombie + f210/f300 skeleton 光门拒、f330 cap 拒，zombie 绿 0）、夜晚 360f ×2 逐字节一致 + **存档 health 8 确定性**（20 − 6 箭×2；debug 日志全程：f60 cow / f90 skeleton / f120 cow / f150 skeleton = cap 4，6 次 "skeleton shot" + 6 次 "arrow hit player for 2"，skeleton 站桩位置逐发不变）、天门 FRESH-FLASH（最后一击距截图 0.5s，伤害红纱 a≈0.25 全屏 tint——**去纱 diff**（对照图同参数加纱 + 容差 3）= 3 cluster = 2 skeleton（提亮后 skull 眼窝+鼻腔可辨）+ 2 cow，zombie 绿 0）、nomobs 对照 health 20 + FRESH。**踩坑（新教训，共 4 条）**：① **splitmix64 是纯 finalizer 不是状态机**——仓库 `splitmix64(x)` 无 `x += γ` 步（P27 预测脚本误加导致 %5 全表错、%4 恰好碰对 4 个值险些漏检）；**预测表必须逐字节对照仓库实现 + 用一次真实运行日志校验后再信**。② **箭自命中 t=0**——扫掠测试 "t=0 = 起点在盒内即命中" 的语义对 spawn 在自身盒内的箭是致命的（见上）；任何"实体 A 发射影响 A 自身的投射物"设计必须显式排除发射者。③ **直线瞄 + 重力 = 距离上限**——d²/(2·s²/g) ≤ 瞄点高度才命中；投射物射击 AI 必须做弧线提前量（或降低重力）。④ **E2E 外部输入污染**——用户在 :0 的真实鼠标点击/移动会落在 unmapped UI 后方的游戏窗口上（指针锁 + 视角转动 + 挖掘 → 截图含 HUD、物品、掉落，与"无输入"假设全破）；**×2 字节一致本身就是污染检测器**（本轮 dayc 对 393829 vs 364018 字节即暴露），另加 HUD 红心像素检查入驱动；无输入运行**永远不该有 HUD**（`locked && !panelOpen()` 门）——见到 HUD = 见到输入。

## 会话存档（原 SESSION_CONTEXT）

- **提交** `1b1c520`（17 文件，+574/−20）。
- **目标**：P27 候选第 1 条——skeleton，第二个敌对 mob、首个远程（站桩射击 +
  贴近 kite 后撤）。**打破 P22/P26 纯数据流**：被动 mob = 纯数据已验证两次，
  skeleton 需要 mob 端发射（AI 新状态）+ 箭伤玩家（新命中路径）= 新代码。
- **数据**：
  - `assets/data/mobs/skeleton.json`：HP 20、speed 0.8、detect 16、
    **attack_range 0 = 永不近战**（近战分支对远程 mob 永不成立，纯数据表达）、
    attack_damage 2（= 箭伤）、attack_cooldown 1.5、**新可选字段**
    `shoot_range` 14 / `retreat_distance` 3 / `shoot_speed` 15（均默认 0 =
    近战 mob）、spawn_max_light 0、**hostile true**（P25 自动进保护环 / 伤害
    缩放 0.5/1.0/4÷3 / 生成节奏 60/30/20）、掉 `arrow`×1（既有 material，
    图标已有）；box 模型同 zombie 尺寸（头 0.5³ pos [0,1.25,0] + 身 0.5×1.0×
    0.3 pos [0,0.5,0]，AABB 0.6×1.8）
  - `assets/data/mob_spawning.json`：名单 [zombie, cow, sheep, chicken,
    skeleton]（抽取 = pick % 5）
  - `assets/data/textures/block_atlas.json`：+2 tile（skeleton_body [3,6] /
    skeleton_head [0,7]，27→29，只占空位、既有坐标零移动）
  - 严格校验（mob_registry）：三新字段负数拒绝；`shoot_speed` **显式**值须
    (0,100]（缺席 = 0 合法 = 近战 mob，显式 0 是笔误拒绝）；**跨字段**
    `shoot_range > 0` 必须有 `shoot_speed > 0`
- **工具**：`tools/generate_mob_textures.py` + `skeleton_tile`（骨白
  (222,220,210) + noise 10；skull 深色眼窝 (25,22,20) rows5-6 x[3-5]∪[10-12]
  + 鼻腔 (35,32,28) rows7-8 x[7-8]；body (205,203,193) noise 10；salts
  0x5C11/0x5C12）
- **代码**：
  - `MobState::RangedAttack`（mob_entity）：seesPlayer 条件（水平 ≤
    detect_radius + |dy| ≤ 2）且 `shoot_range > 0` 且水平 ≤ shoot_range →
    面向玩家 + 每 attack_cooldown 经 `setShootCallback` 放箭（冷却计时与近战
    共享 attack_cooldown 字段）；**水平 ≤ retreat_distance → 全速后撤且继续
    射击**（Minecraft kite）；超 shoot_range 但 detect 内 → 按 Chase 逼近
  - `MobEntity::eyePosition()` = position + (0, model.height×0.8, 0)
    （kEyeHeightFraction，放箭起点）
  - `Projectile`：+`hurtPlayer`（ctor 默认 false）——游戏扫掠测试**先**对玩家
    AABB（命中 = player->damage(arrow->damage()) + arrow->kill()，红闪/音效/
    死亡由 updateSurvival 轮询驱动，命中点无额外音效）；玩家自己的弓箭不带
    标志、仍穿过玩家（无自伤）；+`source`（射手指针）——**命中循环跳过
    source**（见踩坑 2）
  - 放箭回调（game.cpp attemptMobSpawn，仅 shoot_range > 0 的 mob）：箭从
    mob 眼沿**弧线提前量**方向生成：`toPlayer.y += 0.5·kGravity·(d/speed)²`
    后重新归一化（见踩坑 3）；damage = attack_damage × hostileDamageScale
    （与近战回调同一缩放）；speed = shoot_speed；"shoot" 音效；`kGravity`
    转 public 供瞄点复用同一常量
  - 生成 debug 日志（长期保留，`--log-level debug`）：cap 拒绝 / 抽中类型无
    格 / 生成位置 / skeleton 射击（原点+距离+伤害）/ 箭命中玩家 / 箭插墙
    （仅 hurtPlayer 箭）
- **单测** 650/650（+7）：`ParsesSkeleton`（完整解析含 ranged 三字段 +
  hostile + 掉 arrow）/ `RejectsRangedFieldErrors`（负数 ×2 / 显式 0 / >100 /
  跨字段缺 shoot_speed）/ `RangedMobShootsAndHoldsDistance`（8 块外 300 帧：
  全程 RangedAttack、恰 4 发【冷却起点 0 + 1/60 浮点残差】、零近战、零水平
  漂移）/ `RangedMobRetreatsWhenClose`（1 块贴近：首帧 velocity 背离玩家、
  300 帧后退至 >3 块、仍开火、零近战）/ `ParsesFiveMobList` /
  `HurtPlayerFlagDefaultsFalseAndSettable` + `SourceDefaultsNullAndSettable`
- **E2E**（P25 协议；驱动 `/tmp/opencode/p27_e2e.zsh`；**本轮新增 HUD 红心
  像素污染检查**入 shot()）：
  - **夜晚必须换种子**（踩坑 4）：seed 123456789 的 pick 流在 p25far 位置
    前 4 抽 = zombie/zombie/sheep/chicken、cap 于 f150 满 → **skeleton 永不
    生成**（两轮实证 health 恒 20 + debug 日志确认）。存档只存 seed + 编辑、
    加载时校验 seed 相等 → 不能只改 config 种子。解决：probe 工具
    （`/tmp/opencode/probe_spawn.cpp`，链接仓库 mc_core 对象 + WorldGenerator
    重放 spawn 候选扫描 + surfaceHeight + 树干检查）→ 手工造 **seed 9 存档
    `p27far.json`**（0 编辑、玩家 (22.5, 45, 0.5) = 世界出生点 (0,0) +22 块
    = R=12 保护环外、表面 44 + 1）；seed 9 前 4 抽 = **cow/skeleton/cow/
    skeleton**（双 skeleton、无 zombie，最干净证据）
  - base 60f ×2：**双双 == base18 逐字节**（名单 4→5 结构性免疫，第三次
    验证）
  - day 360f（config_xep 正午平台，seed 123456789）：mobs-on ×2 逐字节、
    mobs-off ×2 逐字节 且 **mobs-off == base 逐字节**；vs 对照 **36607 px =
    4 被动剪影**（2 sheep + chicken + cow，提亮裁剪可辨；与 %5 pick 序列
    f120/f150/f180/f240 逐一对应——f60/f90 zombie + f210/f300 skeleton 光门
    拒、f330 cap 拒；zombie 绿 0 px）
  - night 360f（config_p27_night：seed 9 + day_length 1200 + start 0.536 +
    p27far）×2 **逐字节一致** + **存档 health 8 确定性**（20 − 6 箭×2）：
    debug 日志全程可验证——f60 cow (22.5,46,−5.5) / f90 skeleton
    (19.5,46,−4.5) 立即首射 / f120 cow / f150 skeleton (21.5,45,−4.5) /
    6 次 "skeleton shot"（skeleton 站桩位置逐发不变）+ 6 次 "arrow hit
    player for 2"；f180 起 cap 拒绝
  - 天门：nightm **FRESH-FLASH**（a≈0.25：最后一击距截图 0.5s，伤害红纱
    (1,0,0)×a 全屏 tint）；**去纱 diff**（对照图同参数加纱 + 容差 3）= 3
    cluster = 2 skeleton（提亮后 skull 眼窝+鼻腔可辨）+ 2 cow，zombie 绿 0
  - nomobs 对照（p27far + seed 9）×2 逐字节、health 20、FRESH
  - **dayc 对曾遭外部输入污染**（踩坑 5）：首轮 dayc_a/b 393829 vs 364018
    字节不匹配 + 截图含 HUD/挖掘/掉落 = 用户在 :0 的真实鼠标落在 unmapped
    UI 后方的游戏窗（指针锁 + 视角转动）。重跑轮全部 clean（HUD 检查 0 红
    像素）
- **踩坑**（4 条新教训，详见坑位合集）：
  1. **splitmix64 是纯 finalizer 不是状态机**——仓库 `world/noise.hpp` 的
     `splitmix64(x)` 无 `x += γ` 步；本会话手写 %5 预测 Python 时误加该步
     → 全表错（f60 预测 chicken、实际 zombie）；%4 下恰好碰对 4 个值（P26
     的 4 抽全对）险些漏检。**预测表 = 对照仓库实现抄公式 + 一次真实运行
     debug 日志校验前几抽后才可信**。
  2. **投射物自命中 t=0**——箭 spawn 点 = 头部 + 0.3 块偏移仍在 0.6 宽身体
     AABB 内；扫掠 "t=0 = 起点在盒内即命中" → 每箭下一帧命中自己的射手
     （自杀自灭 + 沿箭向击退 → 射手向玩家半速漂移；三症状同时出现：6 箭
     全灭 / health 恒 20 / skeleton 漂移）。修复 = `source` + 命中循环跳过。
     **凡"实体发射影响自身的投射物"必须显式排除发射者。**
  3. **直线瞄 + 重力 = 距离硬上限**——重力 20 b/s²、速度 15 b/s：直线瞄眼高
     只在 d ≤ ~6.3 块内命中（drop = d²/22.5）；10 块外箭从脚下飞过（实证
     health 恒 20）。修复 = 瞄点 + 0.5·g·(d/speed)²。
  4. **E2E pick 流与位置解耦**——类型抽取 = f(seed, frame)，与位置无关；
     位置只影响"哪个抽能找到格"。某位置 cap 早满 → 该位置永不生成某些
     类型（seed 123456789 @ p25far 的 skeleton）。要换 pick 流只能换 seed；
     换 seed 必须同步换存档 seed（加载校验）→ 手工造存档（probe 工具）。
  5. **E2E 外部输入污染**——见坑位合集（×2 字节一致 = 天然污染检测器；
     驱动加 HUD 红心检查；热栏 9 物品 = `data/hotbar.json` 初始配置，**不是**
     污染特征——污染特征是 HUD 可见 + 相机移动 + 地面掉落物）。
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length
  600），与 P27 无关。
