# BACKLOG.md — v2.x 任务表·第二批（执行权威）

> **11-03 用户定案：原 3.x「系统扩展」期八项并入 2.x**（3.x 条目仅 8 个，
> 并入 2.x 续编号 V2-11~V2-18，版本 v2.11.0~v2.18.0）；**原 4.x「世界
> 扩展」保持大版本**（条目皆大任务，2.x 二批收官后本 4.x 表晋升执行
> 权威，版本 v4.x）。2.x 第一批（V2-1~V2-10，门 171-181 全 PASS，
> tag v2.10.0）完成表沉 `docs/archive/v2x/`。
>
> 本表八项 = 原 `BACKLOG_3x.md` 方向底稿晋升（条目内容零改动，仅
> V3-N → V2-N 重编号）；细节立项时设计定案。**用户已授权按 V2-11
> 起顺序推进**（11-03「继续推进原 3.x 转到 2.x 的 8 个任务」）。

## v2.x 第二批任务矩阵（6/8 完成）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ | ⑦ |
|---|---|---|---|---|---|---|---|
| STRM 流送与内存 | | | | | | | |
| PIPE 帧管线与并发 | | | | | | | |
| LGT 光照 | | | | | | | |
| GEN 世界与生成 | ~~V2-16 新 biome：雪原/针叶林~~ ✅ v2.17.0（门 188：snowy_taiga 数据驱动【tree 6 键严格对象 + exclude_spawn_area 演示区结构保护 = 区内映射逐位不变】+ snow 满块 + planTree 针叶路径【无 tree biome 零触碰】；零 format 世界演进，结构免疫第 69 次免重锚） | V2-17 顶部 slab/双 slab | | | | | |
| FLUID 流体 | | | | | | | |
| ENT 实体/Mob/移动 | ~~V2-14 新 mob 蝙蝠（飞行 AI）~~ ✅ v2.15.0（门 186：悬停巡航飞行 AI【正弦 bob 帧锁定 + 航向流重掷】+ 洞穴生成 in-air 分支【光 ≤3 跳过 floor 带任何 Y】+ 0.9×0.5 展翼扇翅模型 + squeak 氛围音 + 三数据旗互配纪律；零 format savenorm EXACT，结构免疫第 67 次） | ~~V2-15 16 色染色羊~~ ✅ v2.16.0（门 187：shears 238 耐久 + 剪毛 1-3 彩毛 + 出生毛色流首抽 temperate 表 + 染色改色 bare 拒染 + 死亡彩毛钩子 + per-entity 变体网格【15 色复用 wool 族 tile + sheep_skin】；零 format savenorm EXACT，结构免疫第 68 次） | | | | | |
| SURV 生存/战斗/效果 | ~~V2-11 XP 系统~~ ✅ v2.12.0（门 183：经验球 1..9 无合并分裂 + 全整数 MC 曲线【等级派生不存储】+ 三发放源数据驱动【hostile 5/passive 2/mine coal 1 diamond 5 iron 0 gold 0/smelt 1 统一】+ format 26 三向键 + save.cpp PITS 361 潜在 bug 修复 + XP 条 HUD；base18 第 4 次重锚 179027 B） | ~~V2-13 床（睡觉跳夜）~~ ✅ v2.14.0（门 185：15 色染色族 Slab 床 + 睡眠三门【距离 3.0/hostile 8×5 盒/夜窗】即时跳夜 + 天气重置 + format 28 respawn 恒写 + 验床重生【破坏回退世界出生点】+ sleep 音；结构免疫第 66 次免重锚） | | | | | |
| CRAFT 物品/合成/面板 | ~~V2-12 附魔~~ ✅ v2.13.0（门 184：7 附魔数据驱动【锋利/保护/效率/耐久/力量/速度/无限，MC 精确区间+权重】+ 附魔台/书架 2 块 2 配方【lapis 代 book 偏离】+ 确定性 offer 全整式【种子 = world^hash3(台位)^ability^xpSeed，仅面板开时求值】+ XP 整级扣减余量保留 + format 27【xp_seed hex + enchants 三处，四拒】+ 面板 furnace 范式 + 字体 +4 字【锋利限魔】；结构免疫第 65 次免重锚） | | | | | |
| SAVE 存档 | | | | | | | |
| PRE 表现层 | V2-18 AO 环境光遮蔽 | | | | | | |
| UX 实机报障/体验细化 | （开放：2.x 二批实机报障入此，V2-N 续编号 V2-20+） | ~~V2-19 视角/移动/时间显示批~~ ✅ v2.11.0（门 182：滚轮缩放【默认参数位级不变 + aimReach 联动】+ 双击 Space 飞行【1.6× 速 + 坠伤归零 + 水切出记档】+ 24h HUD 时钟【ramp 边精确对齐映射】；base18 第 3 次重锚 178676 B，全会话态零 format） | | | | | |

> **条目说明（11-03 用户审议原文；细节立项时设计定案）**：
> - **V2-11 XP 系统**（大）：经验球（实体 + 拾取）+ 玩家等级/XP 条
>   （HUD）+ 发放源（炉子熔炼【消费「无熔炼经验球」记档】/ mob 击杀 /
>   破坏）+ format；晚游戏循环第一步。
> - **V2-12 附魔**（大）：消费「附魔台替身」三处记档（master_bow /
>   信标 / 弓强化）；项目范式定案 = 真附魔台（确定性 roll，帧锁定
>   salt）vs 强化物品扩展，立项时定。
> - **V2-13 床**（中）：数据块 + 睡眠通道（夜间右键）+ 跳到黎明 +
>   重生点更新（R 重生流既有）；消费「v1 无床」两处记档（保护环 /
>   和平难度）。
> - **V2-14 蝙蝠**（中）：飞行 AI = 新 AI 型（洞穴生成）；模型 + 纹理
>   数据驱动（V2-2 per-face 消费）。
> - **V2-15 16 色染色羊**（中）：消费「sheep 只掉白 wool」记档；mob
>   持色（per-instance 状态 vs mob 数据，立项时定）+ 16 色 wool 掉落。
> - **V2-16 新 biome：雪原/针叶林**（中）：biome JSON + 雪面 + 装饰；
>   旧存档 = 世界生成演进先例（GEN-1/3/5）；新 biome 避开演示区 =
>   结构性免疫设计（熔岩湖先例）。
> - **V2-17 顶部 slab/双 slab**（中）：消费「slab = 仅下半块」记档；
>   ENT-2 `state_count` 机制现成（朝向 2 值）；剔除/mesh/光照（LGT-5
>   先例）三点适配。
> - **V2-18 AO 环境光遮蔽**（中大）：消费「按面采样光照，无 AO」记档；
>   mesher 顶点 AO + shader，经典 MC 块状 AO 观感；门须重锚（世界
>   光照视觉全变）。
> - **方向项（未编号）**：死亡统计屏 / Game Over 屏（1.x 记档范围外；
>   SURV-7 统计机制既有，纯呈现）——按 11-03 定案归 **4.x**。

## 执行顺序（第二批生效）

1. V2-N = 任务编号（2.x 续编号；实机报障项入 UX 行续编号
   V2-19+）。**版本号 = 执行序**（v2.11.0 起逐版 bump，CMakeLists
   单点）；任务号与版本号解耦（V2-1/V2-2 同款先例）。用户实机
   诉求可提前执行（11-03 V2-19 先于 V2-11 执行 = v2.11.0）。
2. 一任务一 phase，走标准阶段流程（设计定案 → 实现 → 构建+测试 →
   E2E → 文档 → 提交 → 报告；详见 AGENTS.md）。
3. 版本号逐版 bump（**v2.11.0 = 首个二批版本**；CMakeLists
   PROJECT_VERSION 单点 + test_debug_overlay F3 钉意识更新 + 构建后
   banner 版本实证，PITS 355）。
4. **每版发布前必做性能 A/B**（用户硬要求），旧侧 = 上一版本 commit。
5. tag/release 政策沿用 2.x 收官定案（v2.10.0 = 第一批系统收官 tag；
   二批收官 = v2.18.0 系统收官 tag + release，用户可推翻）。

## 变更记录（v2.x 第二批）

- 2026-11-04：**V2-16 雪原/针叶林 biome 完成（v2.17.0，门 188 全
  PASS）**——GEN 行。snowy_taiga.json（weight 2 / surface snow /
  tree_density 0.12 / height_scale 1.1 / **tree 严格 6 键对象**
  {spruce_trunk, spruce_leaves, 6, 3, 2, spruce} = sapling 同款面
  缺/多/词汇全点名 + create 交叉检 / **exclude_spawn_area = 演示区
  结构保护**：区内累计走查跳旗 → totalWeight 恒 10 = 现有映射逐位
  不变【GEN-1 spawn_clear 先例】）+ snow 满块（hardness 0.1 MC 精确
  / 新 snow 三件音族 / atlas 122 零移位；MC 1-8 层雪层 = 偏离 1）+
  placeTrees 针叶路径（同 kSaltTree h 复用 + planTree 锥冠逐 cell +
  root no-op 既有同款；**无 tree biome 硬编码路径零触碰**）。4 条偏
  离（满块代层 / 无 decorations / density 0.12 / weight 2）。**音效
  缺陷族 13 件随版修复**（generate_sounds LCG 逐采样重建 = 常值流
  → 4 静音 + 5 DC 退化 + 2 部分受损跨多版本潜伏 = 音频门盲区；修后
  峰值全非零 + 纹理度量落正常带，md5 变更恰 13 件幂等）。2415/2415
  （+23）、banner v2.17.0、零 format（biome 不入档，旧档 = 区外雪带
  世界演进 GEN-1/3/5 先例）。门 188：S1 结构免疫第 69 次免重锚（设
  计重锚预判被证据推翻：60f 流送半径内可见地形全落演示区 → ≡ 锚
  179027 零 diff + savenorm EXACT + 区内位级双钉）/ S2 雪块破/放单
  准星双动作 E2E（破 (2,54,2) → 同射线续至 (3,53,3) 地顶 = 放置
  (3,54,3)；hold 0.5s 断点时序；掉落 2.83>磁力 2.5 留地；旧侧词表
  互斥）/ S3 / S6 6/6 / 性能同档（9/1/3 逐位 + journal 0/0【世界演
  进 = 参考变化非 journal 口径发现】+ 雪带 1800f 稳态 111.9/112.5
  ms 同档）。PITS 382。详见 docs/archive/phase-2.17.0.md。

- 2026-11-04：**V2-15 16 色染色羊完成（v2.16.0，门 187 全 PASS）**
  ——消费「sheep 只掉白 wool」记档 [Phase 41]。shears 物品（238 耐
  久/break_speed 2/`shears` 严格旗/shapeless 2 iron_ingot/fast_
  against 整字段省略 = 不加速任何块）+ 剪毛磨损 1；sheep.json
  drops 1-3 wool MC 精确 + **wool_drops/wool_weights 严格对**（白
  850 黑 80 灰 30 棕 30 粉 10 temperate 整数表 = biome 表偏离；
  词表/零权重/空列表/孤字段点名拒，词表拒放 init checkWoolDrops
  交叉检）；实例态（per-mob 流首抽出生色【非 wool mob 零消费位级
  钉】/ bare / 再生 7200f 帧锁定 = 吃草门偏离）；渲染 = 15 色复用
  wool 族块 tile + sheep_skin（atlas 121 零移位）+ **per-entity 变
  体网格**（swingMeshes 范式，几何位级不变，非 sheep 零触碰）；RMB
  链 tryShear（1 ItemEntity 携 roll 活体掉落 + shear.wav 43 + 磨损）
  → tryDyeMob（bare 拒染 MC 同款）全数据旗无物种 switch；死亡彩毛
  纯函数 woolDeathDropName（bare 跳过/非 wool 位级不变）。2391/
  2391（+35）、banner v2.16.0、零 format。门 187：S1 结构免疫第 68
  次免重锚 + savenorm EXACT（≡ 锚 179027）/ S2 剪毛染色 E2E【两
  pass 瞄准 + 弹幕【围盒弃：羊 1.2>1×1；命中窗 = 出生→首次
  wander ~3s；key→RMB ≥0.15s】+ **断言 = DEBUG 日志行权威**：a
  剪毛+damage 1 / b 染红→剪 wool_red+染料耗尽 / c bare 拒染染料保
  留 / o 旧侧零行】/ S3 / S6 6/6 / 性能同档（9/1/3 逐位 + 16/16
  36/36 + 羊群 2 羊双树同位出生 ents 0.0606/0.0600）。PITS 381。
  详见 docs/archive/phase-2.16.0.md。

- 2026-11-04：**V2-14 蝙蝠完成（v2.15.0，门 186 全 PASS）**——项
  目首只飞行 mob。bat.json 全数据（HP 6/被动/spawn_max_light 3 洞
  穴语义/无 drops/xp 0/三新严格字段 flying+spawns_in_air+
  ambient_sound，flying⇒spawns_in_air 互配点名拒）+ model 0.9×0.5
  身头双翼 4 box 扇翅（swing 反相）+ 8 词表 + bat.png + squeak.wav
  确定性合成（既有资产 md5 全不变）；飞行 AI = runAi 纯新分支
  （def.flying 门控：航向 per-mob 流 2s 重掷 + 正弦悬停 0.2×sin/60f
  帧锁定 + 重力关 + 撞壁航向重掷 + knockback 回巡航 + ambient 音
  帧锁定 roll 空间化），既有 7 mob 族零改动全过；生成 in-air 分支
  （顶向扫描 + 全空气盒 + 光 ≤3 + 跳过 floor 带 = 任何 Y）；消散
  40/倒挂不做/takeoff 音不做 = 偏离 5-7。零 format（savenorm
  EXACT）。2356/2356（+26）、banner v2.15.0。门 186：S1 结构免疫
  第 67 次免重锚（≡ 锚 179027 + EXACT；编排者首跑误用 v2.13.0 树
  = PITS 379① 再证）/ S2 洞穴无输入 E2E（密封暗厅 seed 3355 四
  pick 全 bat：log 孪生逐位同 + 截图 ×2 恒等 + diff 43425 px 全褐
  色蝙蝠纹理泄漏审计零 + old 7 词表零 bat 行形状互斥）/ S3 / S6
  6/6 / 性能同档（9/1/3 逐位 + 16/16 36/36 + 洞穴 ents 0.093/0.089
  = 飞行不比走地贵）。PITS 380。详见 docs/archive/phase-2.15.0.md。

- 2026-11-04：**V2-13 床完成（v2.14.0，门 185 全 PASS）**——15 色
  染色族 Slab 床（wool 15 色词表同款无 lime；配方 3 同色 wool + 3
  planks MC 精确；纹理 15 tile 零移位；sleep.wav 确定性合成；
  hardness 0.2/cloth 音族）+ 睡眠三门定案序（b 距离 3.0 → c
  hostile 8×5 盒数据旗判据 → 设点 → a 夜窗：夜内即时跳夜
  【setTimeOfDay(kDawnStart) + 雨/雷天气纯重置 CLEAR】/ 夜外仅设点
  = MC 白天语义；b/c 拒全拒含设点 = 偏离 6）+ 重生点 format 28
  【player.respawn int 三元组恒写，三向键纪律 + 27 迁移哨兵 →
  世界出生点】+ R 键 respawn 验床【纯函数 bedRespawnValid：床位有
  任意床族块 + 空间 → 床位，否则世界出生点回退重新钉位 = MC"仅
  respawn 时查"】+ "sleep" 教程词入裸词表。6 条记档偏离（单格
  Slab 代 1×2 / 即时跳夜代 101-tick 动画 / 夜窗略宽 / 整格头位适配
  / mob 炸床不做 / 拒睡含设点）。2330/2330（+22）、banner v2.14.0。
  门 185：S1 结构免疫第 66 次免重锚（≡ 锚 179027 逐字节 + savenorm
  恰 {format 28, respawn}）/ S2 三臂（夜跳 time 0.0796 + respawn
  床格 / 日设点不跳 / mob 拒睡 + hp 22/60 交战实证；旧侧 format 27
  形状互斥）/ S3 / S6 5/5 点名 / 性能 A/B 同档（9/1/3 逐位 + 16/16
  36/36 + 预置床零成本）。PITS 379。详见
  docs/archive/phase-2.14.0.md。

- 2026-11-04：**V2-12 附魔系统完成（v2.13.0，门 184 全 PASS）**——
  立项定案 = 真附魔台范式（消费三处替身记档：master_bow P82 / 弓强
  化 P77-79 / 信标 P91）。7 附魔（assets/data/enchantments 数据驱
  动严格解析）：锋利 axe +(L+1) 点 / 保护 armor +4L 点共享 20 cap /
  效率 正确工具 +(1+L²) / 耐久 掷骰跳过（工具 L/(L+1)·护甲
  2L/5(L+1) = MC 1.21 精确）/ 力量 bow +L 伤 / 速度 bow +L 弹速 /
  无限 bow 不耗尽；物品 enchant_class+enchantability 严格互配（22
  件，仅基础 bow）+ lapis enchant_material 门；确定性 offer（core/
  enchanting 纯函数全整式：xpBase 三槽成本 + eCost randTri 整式 +
  权重选择循环 = MC 精确；bookshelfCount 15 环双层 cap 15 无视线偏
  离，仅面板开时求值 = 无输入零成本）；新块 附魔台（hardness 5 /
  light 7 / interact "enchant" / full_cube 偏离）+ 书架；配方 ×2
  （lapis 代 book 偏离）；tile 102→104；format 27（xp_seed 16-hex
  无损恒写 + enchants 三处 omit-when-empty + 四拒）；XP 整级扣减余
  量保留（T(30)=1395 → T(24)=828 实证）+ xpSeed 推进；面板
  （furnace 范式：RMB 开 + carry 模型 + 3 offer 行 + 关归还 +
  "craft" 音偏离）+ 字体 +4 字锋利限魔（atlas 2080→2112 顶 512 行
  不变，kCjkGlyphs 427 码点序 re-cell）。2308/2308（+59）、banner
  v2.13.0。门 184：S1 结构免疫第 65 次免重锚（≡ 锚 179027 B 逐字
  节——atlas v-UV 亚 LSB 零像素差；fixer 中间态"需重锚"= 环境伪影）
  + savenorm 恰 {format, xp_seed} / S2 面板 E2E（offer 种子确定 效率
  I 6/III 9/IIII 17 + 附魔应用全断言 + 旧侧 format 26 形状互斥 +
  面板开态 ×2 恒等）/ S3 / S6 12/12 点名 / 性能 A/B 同档（9/1/3 逐
  位 + 16/16 36/36 + 预置台场景零成本）。PITS 378（AABB 重叠顶高 /
  carry 模型 / xdotool --pid / savenorm 官方工具）。详见
  docs/archive/phase-2.13.0.md。

- 2026-11-04：**V2-11 XP 系统完成（v2.12.0，门 183 全 PASS）**——
  fixer 半成品（43 文件，11-03 中断取消入 stash）11-04 pop 零冲突
  恢复至 v2.11.0 之上 + 编排者修 3 处 WIP 编译破损（test_save.cpp）
  + 修 save.cpp 潜在 bug（PITS 361 违例第二实例：tutorial 白名单
  isV25 = fmt >= kSaveFormat → 字面量 >= 25）。全整数 MC 曲线
  （needed 2L+7/5L−38/9L−158，逆 = 整数二分，等级永不存储）+ 经验
  球 1..9（无合并分裂记档）+ 三发放源（mob 击杀 hostile 5/passive 2
  / 熔炼 smelt_xp 默认 1 统一记档 / 挖掘 mine_xp 现代 MC raw 值
  coal 1 diamond 5 iron 0 gold 0）严格 [0,9] 解析 + format 26
  （player.xp 恒写 + entities type∈{item,xp} 三向键）+ XP 条 HUD
  （热栏下恒可见，零新字形）。2249/2249（+42）、banner v2.12.0。
  门 183：S1 base18 第 4 次重锚论证（diff 2031 px 全落底部 XP 条带
  → 新锚 179027 B）+ S2 三发放源（熔炼 1/击杀 5【W 步行拾取：orb
  距静止玩家 2.54 > 磁力 2.5】/挖掘 1，旧侧 format 25 形状互斥）+
  S3 + S6 四项点名 + 性能 A/B 同世界同成本（9/1/3 逐位 + 16/16
  36/36 恒等 + 熔炼场景同档）。PITS 377（旧侧新格式场景 init 拒 /
  orb 磁力 3D 边界）。详见 docs/archive/phase-2.12.0.md。

- 2026-11-03：**V2-19 视角/移动/时间显示批完成（v2.11.0，门 182 全
  PASS）**——用户 11-03 实机诉求三项提前执行（先于 SURV/CRAFT 行，
  版本 = 执行序 v2.11.0，任务号与版本号解耦）：① 第三视角滚轮
  缩放（Input wheel 帧态 + session distance clamp 2-8 step 0.5，
  resolveThirdPersonCamera 默认参数 = 既有 16 单测位级不变；
  ThirdFront aimReach = distance + kReach 联动，瞄准射线自眼位
  UX-8g 零触碰）② 玩家飞行（双击 Space 0.3 s real-dt 窗 CRAFT-7
  先例，第二击吞跳跃；重力跳过 + 升降 4.0 b/s + 水平 1.6× 快于
  走路 + 碰撞照常 + 坠落累加器飞行归零【切出无爆发伤害】+
  水/熔岩自动切出记档 + 不存档记档）③ 24h HUD 时钟（纯函数
  clockString = fmod((t−0.05)×24+6, 24) 与 ramp 边精确对齐 晨
  06:00/午 12:00/昏 18:00/夜 00:00 + ulp 钉 04:47；renderUi 右上
  角恒可见；':' 字形在列零字体扩展）。2207/2207（+26）、零
  format（全会话态）。门 182：S1 **base18 第 3 次重锚论证**（diff
  609 px 全落右上角时钟区零泄漏 → 新锚 178676 B）+ S2 三判别
  （zoom new 视差/old None 互斥 + flight y62.13/54.00 互斥 +
  clock = S1 区）+ S3 + S6 命名单测 + 性能 A/B 同世界同成本
  （changesApplied 9/1/3 逐位 + 终档 16/16 36/36 恒等 + 全指标同
  档）。PITS 376（输入 E2E capture 重试/帧限收尾/pgrep 假
  PASS）。详见 docs/archive/phase-2.11.0.md。

## 表外

- 继承 1.x / 2.x 第一批表外：1.x 表外（见 `docs/archive/v1x/BACKLOG.md`
  表外节）+ 2.x 第一批表外（见 `docs/archive/v2x/BACKLOG.md` 表外节）。
- 二批无界项：新内容开放空间（方块/物品/配方/mob）；netherite 材料链
  （依赖 4.x 下界）= 立项需用户定案。
- 4.x「世界扩展」方向素材 = `docs/BACKLOG_4x.md`（下界维度锚 + 终局
  方向；二批收官时定案晋升执行权威）。
