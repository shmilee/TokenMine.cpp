# Phase v3.26.0 — V3-10d1 末影龙核心 + 首杀闭环（门 216 全 PASS，2026-11-07）

## 范围与定案

**矩阵唯一真大项的首相位：末影龙 boss 核心 + 首杀闭环**（≈ 10c×2-3
规模，侦察全采纳 `/tmp/opencode/v310d_recon.md` 446 行 13 节 + 门内
微调 ×2）。龙 = **第 16 mob 纯数据**（核心零物种 switch——数据旗 +
纯函数，硬规则 3 的第五档验证）；**format 32 不变（零 bump 最强形：
龙会话态 + 击败态 = 蛋块世界态）** / **sim 2 不 bump**（canary 4 全
OW，EXACT 实证）/ 零 config / 块 133→135 / mob 15→16 / atlas
174→179（5 瓦片零移位）/ 声音 71→75 / 效果 11→12 / 配方 113→114 /
**字体 430→433（扩展区 528/528 恰好满——后续 CJK 须扩
kFontAtlasHeight）**。

### 龙的数据面（8 文件）

- **ender_dragon.json**：200 HP（MC 精确，zombie/piglin 先例——ghast
  10 的 /2 刻度是 flying 例外，龙不采）/ speed 0.7 / hostile / flying +
  spawns_in_air + **dragon_flight** / **boss** / contact_damage 5 /
  fire_exempt + effect_exempt + knockback_exempt 三旗 / breath 3 参
  （8s 冷却 · 3s 云 · r2）+ fireball 2 参（2.5s · 12 b/s）+
  crystal_break_damage 10 + detect_vertical 8 / ambient
  dragon_ambient / **xp 0**（爆发 = 仪式 200 orb，非掉落）/ model
  **8×4 AABB 八盒双翼 swing**（头 + 颈 + 躯干 + 尾 4 段 + 双翼，脸
  8×8 独立 tile 紫发光眼缝）。
- **块旗 2 新**（block 侧，strict，缺省冷）：`explode_radius`
  （number ≥ 0 缺省 0 = 惰性——破块即自爆，块本身是引信）+
  `shatter_on_arrow`（strict bool 缺省 false——箭扫掠碎块消费箭）。
- **end_crystal.json**：non-solid cutout / 光 14 / 硬度 0 / 爆半径 3 /
  箭碎——**无连锁 = 结构性质**（explodeAt 直收 air 不走 breakBlock，
  MC JE 读）。
- **dragon_egg.json**：non-solid cutout / 光 1 / 掉自身——**D17：
  solid false 否则挡返程头格**。
- **dragon_breath.json**：damage 1.5/0.5s（=3HP/s MC 精确）/
  min_health 0 可致死 / affects_mobs false（云只打玩家）/ icon 11
  （effects.png 176→192 新紫漩涡 tile，盐 1500+ 幂等）/ tint 紫。
- **end_crystal 配方**：glass10 + ender_eye + ghast_membrane shapeless
  （MC 泪→膜项目等价）。
- **纹理 5 张**（块 2 + mob 3，新盐 1090+/0x10D1-3 幂等零移位）+
  **声音 4 件**（ambient 0xD0A1 / growl 0xD0A2 / death 0xD0A3 峰值
  25271 驯过 / shoot 0xD0A4）。

### 数据旗配对检（mob_registry，milk 纪律）

dragon_flight **须** flying ∧ spawns_in_air ∧ breath 3 参全 >0 ∧
fireball 2 参全 >0；**breath/fireball 参存在须** dragon_flight（无
消费者 = 死字段点名）。contact_damage (0,100] 数值门（自门控无配对）/
三免疫旗 strict bool / detect_vertical (0,64]（只放宽 kDetectVertical
2.0 地板，max() 读）。**门内捕获：readDragonNumber 共享 `double v`
累加器——缺键 read 不动 v，缺字段的值泄漏上字段（fireball 缺省吃
breath_radius 的 2 过了 >0 门）** = PITS 408②，负例单测抓获。

### dragonFlight AI（runAi flying 分派最顶纯新分支）

三态复用 MobState 零新增：**Wander = 盘旋**（kDragonOrbitRadius **30**
【门内裁决：40 + player-pull 恒过入场点头顶 → 30 定心 + 移除 pull，
交战 = strafe/perch 职责】/ 角速 = speed×10 弧速÷半径）/
**RangedAttack = 侧移**（朝玩家 + 垂直漂移半弧速 3.5s 定长 + 火球
2.5s 冷却经 shoot 回调）/ **Attack = 驻坐**（直线 2 倍速赴出口锚
(0.5,56.5,0.5) + 咆哮 1.25s 预告 → 玩家脚位吐息云 + 三起飞条件：连
吐 4 / 玩家离 150 / 驻坐累计受击 >50）。

**流契约钉死**：每帧 ambient 1 抽（数据间隔概率）+ **绕圈跨圈帧恰
2 抽**（u1 侧移 0.25 / u2 驻坐 1/(3+活晶)——**strafe 命中亦消费
u2**【门内重构：inline else-if 的 1 抽形是 bug】，纯函数
`rollDragonLapDecision(state, crystals)` = 10c roll 孪生，单测钉
「每结局恰 2 步 + 普查方向/量级」）。晶碎 = `crystalDelta ∧
seesPlayer` 立即入侧移（MC 保规则；非轨道帧单消费丢弃）。integrate
龙高度分支：vy = clamp((target−y)×2.5, ±8) + bob（纯帧函数）。

### 末影水晶（新块非实体，6 固定槽）

kEndCrystalSlots 单源词表（±50,59,±50 塔顶 4 + ±2,54,0 出口 2；
**D3：MC 10 座→6 座**——塔 5×5 obsidian y53..58 worldgen pass 4 写入
+ 空气-only 护栏）。**自产火 = 晶上 1 格**（末地永久火 D9 闭集零
churn，setBlock 一次性）。**治疗 1HP/30f 最近 32 内晶**（数据序
tie-break；满血无 beam）。**破晶反伤 10**（治疗源晶 =
m_dragonHealSlot 本窗读，帧序：damage 先于爆）。**箭碎**：arrowSteps
扫掠 6 槽 AABB raycast（shatter 消费箭）；玩家破块走 breakBlock 爆钩
（hardness 0 = 生存即挖 R3 实证）。**普查卸载无关**【门内捕获：raw
getBlock 随流送 0→6 漂 = 假 crystalDelta + 假驻坐概率 → 未载 chunk =
worldgen 新鲜 = 完好计 live；蛋门同理】= PITS 408④。

### 龙息云 + 火球（首个非箭/珍珠投掷物变体）

BreathCloudEntity 新实体：**零物理零流**（r(t) = r0+0.15·age 纯帧龄，
3s/r2 吐息于玩家脚位；火球 stuck 尾参 **leavesCloud**【缺省 false =
箭/ghast/珍珠位级红线】→ 5s/r2.5 云于撞击点——**撞击即云无直接伤
D11**）。伤害 = dragon_breath 效果 30 帧 re-grant（potion 同款
applyPlayerEffect 路径；**死因 fallback「死于药水」D14 零字形**）。
存档：end 段 **orb + 云静默跳过**（偏离记档：末地 orb 不持久，120s
燃尽限界损失，format 33 end_xp_entities 后移 10d2 复议）。

### 死亡仪式（首杀全链，R14 0 帧差）

m_dead 帧（game pass 收口——**门内捕获：局部 dragon 指针在 spawn 段
前捕获 = 同帧 spawn+kill 漏账 → 蛋永不落 = 重载白嫖洞 + f119 复生
死循环【首次实证】；修 = spawn/seam 后重捕获 + despawn 对任何
dead-boss 保持 + 记账单收口 game pass** = PITS 408①⑧）：蛋
setBlock (0,54,0) + kill stat + tutorial + death 行 + **INFO「egg
placed」** → 升天 60f 直线赴 perch + 180f 悬停（晶纹理粒子每 4f，
种子契约）+ **f≥124 每 5f 放 1 orb ×23 = 22×9+1×2 = 200 整**（D5
项目 XP 刻度）+ f299 wail 音 + 延迟门 240f 移除 + m_dragon 置空。

### boss 血条（renderUi 内联段）

!panelOpen ∧ playerInEnd ∧ 龙在场 门；186×8 十段 + 「末影龙」CJK
（**字体 +3 = 433：影 278 / 末 330 / 龙 527【扩展区最后一格
528/528 满】，FNV f693835a061f0016**，top-512 红线 + legacy 33 位级
恒等）；kBossBarTopMargin **24**【门内捕获 8 裁名】；2s 升天淡出
（ascension/120 线性 alpha）。

### 生成与 --dragon-damage seam

生成门：playerInEnd ∧ mobs.enabled ∧ 无龙 ∧ (0,54,0) 非蛋 → 60f 延迟
→ kDragonSpawnPos (0.5,58,0.5)（出口顶，飞入轨道），seed = worldSeed
^ kSaltDragon（0x10D0E11D5E4D73A6，singleton 不走池 spawnCounter）。
**龙战斗 demo 不可手编**（耦合移动目标：龙轨迹依赖玩家命中，玩家
瞄准依赖龙轨迹）→ **`--dragon-damage <n>` CLI seam**（--explode 孪生：
首帧龙存在时施加 n 伤害，严格有限数 >0，管理旗互斥，nullopt = 结构
免疫根）——击杀仪式臂的确定性触发。

## 变更文件

数据 8：mobs/ender_dragon.json / blocks/{end_crystal,dragon_egg}.json
/ effects/dragon_breath.json / recipes/end_crystal.json /
block_atlas.json（179）/ ender_dragon 纹理 5 png +
sounds 4 wav + font.png（433）+ effects.png（192）。
代码 ~20：block.hpp / block_registry / game_interaction（爆钩 + 点名
行）/ mob_registry（8 组旗 + 配对检 + 泄漏修）/ mob_entity
（dragonFlight + 常量族 + 纯函数 + 三门 + heal + 接触冷却 +
orbitAngleFromPosition 升 public）/ projectile（+leavesCloud 尾参）/
**breath_cloud.{hpp,cpp} 新** / end_portal.hpp（6 常量族）/ end_gen
（8 名 + pass 4 塔晶）/ game_init（id 解析 + wav×3 + 效果交叉检
【后置 loadDirectory——PITS 408③】）/ game_frame
（updateDragonGameplay 五段 + spawnDragon + shatterCrystalAt）/
game_entities（箭碎扫掠 + 火球云 + boss 三豁免 + 死体延迟门 + m_dragon
置空）/ game_render（beam + 云 + 惰性 mesh）/ game_ui（血条段）/
game_save（end 段 orb/云跳过）/ main.cpp（--dragon-damage + 互斥 +
help）/ game_local.hpp（仪式常量）/ CMakeLists（3.26.0 + breath_cloud）。
测试：test_ender_dragon.cpp 新 26 项（解析负例 10 / 纯函数 3 / AI 6 /
免疫 4 / 云 3）+ test_end_gen 塔晶 +1 / test_explosion 无连锁 +1 /
test_font ×4 re-pin / census re-pin 21 项。

## 门 216 结果

- **S1 结构免疫第 97 次**：≡ base19 逐字节 ×2 + savenorm EXACT（末地
  全链 playerInEnd 门 + 数据旗缺省冷 = 位级根；init 8 名 fail-fast 在
  OW 跑亦过 = 块已注册）。
- **S2a 无输入末地臂 600f**：龙 f59 spawn + 轨道 + 血条 + enderman
  共存，×2 RAW + savenorm EXACT（end 段 = 晶火 + 携块 air，逐字）。
- **S2b 破晶臂 = authored demo**（晶静止目标可脚本；玩家 (6.5,53,0.5)
  yaw π/2 距晶 3.5 在 kReach 5 内）：f151 `shattered explode_radius 3
  at (2,54,0)` + **f152 strafe（crystalDelta 触发）** + 火球 f152/f303
  （2.5s 冷却逐位）+ f363 回轨道，×2 RAW + 战斗行逐字（**demo 存档
  红线：save 参考 demo 的 --save 被 reset = 观察只读【PITS 407① 实证
  面】，终态权威 = 截图 + DEBUG 行**）。
- **S2c 击杀仪式臂**（--dragon-damage 200）：f59 spawn+death+蛋+
  INFO 逐字 / 23 orb f124..f234 每 5f 逐位（末 2 值 D5）/ f299 wail +
  移除 / ×2 RAW + savenorm EXACT + 终档 end 段 = 恰 6 键【蛋 + 晶火 +
  4 enderman air——orb 不持久按偏离】。
- **重载臂**：杀后档 → 新进程：蛋在 = **无复生**（spawn 门蛋检）+
  血条缺席，×2 RAW + 终档蛋逐字。
- **S6 十四面点名**（全 rc 1 点名）：dragon_flight 缺 flying / 缺
  fireball 对 / breath 对无 dragon_flight / end_crystal stateful
  （model 配对点名）/ explode_radius −1 / shatter_on_arrow 字符串 /
  wav×3 缺 / dragon_breath 效果缺 / end_crystal 缺（recipe 点名先至）/
  dragon_egg 缺（EndGen 点名）/ atlas 瓦片缺 / contact_damage 0。
- **canary 4 ×2 RAW + savenorm EXACT**（sim 2 不 bump 实证——全 OW 无
  末地无龙，无 --demo-force）。
- **性能 A/B 同档**（旧 = 23c4e7c v3.25.0 + g139 双树）：banner 实证 v3.25.0/v3.26.0 + 双场景——OW 900f 无输入：PERF-WIN 三窗 upd med 0.594/0.548/0.586 vs 0.576/0.574/0.583 同档 + render 0.637/0.544/0.651 vs 0.617/0.640/0.654 + swap 15.3 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形；末地战斗 600f（mobs-on 变体）：旧（无龙仅 enderman）upd med 0.525/0.687 vs 新（龙 + enderman）0.905/1.012 = boss 自成本 ~+0.4 ms upd + ~0.1 ms render（beam/云/血条），swap 15.2 同档 + changesApplied 0/0 逐位】
- **2959/2959**（+29：dragon 族 27（解析 10 / 纯函数 3 / AI 6 /
  免疫 4 / 云 3 / shipped 1）+ 塔晶 1 + 无连锁 1）；
  `-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- **门内捕获 8 坑全 PITS 408**：① init 顺序——交叉检必须后置于所检
  子系统的 loadDirectory（效果检前置 = 假 fail 首跑实锤）② 数值累加
  器泄漏——缺键 read 不动共享 v，缺字段吃上字段值（负例单测首跑抓）
  ③ demo 红线——save 参考 demo 的 --save 被 reset，终态 = 截图 +
  DEBUG 行 ④ 流送依赖锚点——raw getBlock 未载 chunk 读 air = 普查
  0→6 漂 + 假 crystalDelta + 蛋门假复生（未载 = worldgen 新鲜 = 完好）
  ⑤ -1 哨兵首帧 delta（普查首帧 ≠ 碎晶）⑥ 同帧 spawn+kill 漏账（局部
  指针捕获序 + despawn 保持门）⑦ 血条 margin 8 裁 CJK 名行（24 定案）
  ⑧ CJK charset 解析边界（`];` 与尾部有意重复项——引号正则 + 真实
  列表边界，raw 扫描 433 是伪影）。

## MC 偏离记档（10d1，D 号续侦察表）

1. **无俯冲**（Java 现行无独立 dive 态——BE only，弃做有 wiki 依据）。
2. **6 晶非 10**（塔×1 顶 + 出口×2；无铁栏杆笼 = 中层晶不可见）。
3. **升天 3s 非 10s**（60f 飞 + 180f 悬停，D10 压缩）。
4. **XP = 23 orb（22×9+2 = 200 项目刻度）非 MC 155 tick 直发**；
   **末地 orb 不持久**（end 段静默跳过，120s 燃尽限界损失，format 33
   后移 10d2 复议）。
5. **云无 Slowness**（项目无减速效果）；**火球撞击无直接伤**（云即伤
   D11）；**无自伤**（D14 死因 fallback 措辞）。
6. **接触 = 1s 冷却无击退**（MC 0.5s 命中窗简化，D13；knockback_exempt
   旗）。
7. **治疗源晶反伤 10 = 破晶帧**（m_dragonHealSlot 本窗读，帧序钉）。
8. **蛋 solid false**（D17：挡返程头格）；**蛋 worldgen 无**（仅仪式
   写——重载门读）。
9. **轨道定心 30 无 player-pull**（门内裁决：MC 巡航不贴玩家，交战 =
   strafe/perch 职责）。
10. **晶火 = 永久块**（D9 闭集，破晶爆半径收 fire 格；不随晶灭单独
    熄灭 = 末地火语义）。
11. **beam 垂直化**（晶→龙直线简化为垂直光柱 + 扁盒云——渲染面小化）。
12. **龙战斗 demo 不可手编** → --dragon-damage seam（耦合系统定案）。

## 后移 10d2（门 217）

crystal_base 放置规则 + 4 晶召唤序列 + 复杀 8XP + 蛋 60s 会话重生 +
F3 龙态行 + 末地 orb 持久化复议（format 33）+ 主世界/下界池复议 +
scream/stare/ambient 音族扩展。
