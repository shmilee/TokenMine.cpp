# Phase v3.38.0 — V3-16g 扩展指导 demo 族：⑩ 完整黑夜历险（门 228）

> 3.x「世界扩展」V3-16 相位 G/7（survival 章节首支）。scene/panel_click/
> 索引内核全复用（16a 形）——本相位 = 内容（1 scene + 1 demo + 1 index
> 条目）+ 工具链增量（弓箭弹道 aim pin 探针族 arrow_sim.py /
> aim_from_log.py）+ **零内核**（弓/箭/火把/弓 knockback/mob 生成/磁吸/
> 自然回血全既有——16b/16c/16d/16e/16f 同款内容相位）。

## 实现

### scene guide_night（mk_scene 手术，幂等 28932 B）
40×26 平台（x0..39, z-25..-1，顶 y54，stone 垫 y50-52 + dirt 表层
y53）于 **time 0.55 = 19:12 入夜**（kDuskEnd 0.55，夜 = t∈[0.55,1.0)
= 18000f；day 600s = 36000f），**SURVIVAL**，player (19.5,54,-13)
中心，**seed 7**（seedhunt 8 粒实证：首批 4 spawn = f60 sheep /
f90 chicken / f120 chicken / **f150 zombie (20.5,-17.5) 唯一 hostile**；
无 skeleton【箭带 10s wither = 站桩 9s 死】/ 无 spider【1.0 b/s 打不
过 11.5-m 列 kite 账本】）。预置最小打造料（prepared-start 面）：
planks 9 (h0) / coal 1 (h1) / flint 1 (h2) / feather 1 (h3) / flint 1
+ feather 1（背包 slot 13/14 = b4/b5 词表——**inv_grid = 3×3 制作网格
非背包**，备料放背包非 grid，PITS 419⑧）。

### demo guide_night（18301f ≈ 05:05，454 actions，survival seq 1）
tour = 一次完整黑夜历险（造火把 + 弓箭 + 打败怪物 + 活到天亮）：
- **s1 P0 开场**（240f）：夜幕降临，zombie f150 自 4.6m 外 spawn。
- **s2 kite NE + 造工作台**（1148f）：N 10-m + E 19-m kite 拉开
  zombie，NE 角 (38.5,-23) E 面板 2×2 造 table（planks 9→5）。
- **s3 SE 停扎 + 最小 combat kit**（2607f）：S 23-m kite 回 SE
  (38.5,-2.5)，1-m 北退站位 place table (38,54,-2) + open + sticks
  ×1 + **bow**（pattern " S "/"S P "/" S "，P = **g5** 非 g4）+
  arrows ×1（4 箭）= 最小战斗 kit。zombie 19-m 外 wander 不追 = 长停
  扎安全。
- **s4 弓箭 combat**（3698f）：N 11.5-m 走到 (38.5,-14)（zombie
  7-m，进 detect 16 追来），**4 箭 full-draw 站射**击杀 zombie
  （**f3524** `mob 'zombie' died at (36.82,54,-15.56)`）。combat 力学
  详见下节。
- **s5 回 SE 造余 kit**（4762f）：S 11.5-m 走回 table（zombie 已死
  安全），**select sticks（非弓，PITS 419⑤）** + open + sticks ×1 +
  arrows ×1（b4/b5 备料）+ torch ×1（coal+stick→4）= 余 kit。
- **s7 火把环**（6530f）：走中心 (19.5,-13) + select torch，4 火把
  (26,-13)/(19,-13)/(13,-13)/(19,-19)——**先走到点再放**（每把目标
  格南 2-m 站位，PITS 419⑥）。
- **s8 黑夜巡视**（14394f）：C 第三人称 sweep ×3 + E 面板展示 1500f +
  收束 sweep（夜之主体，torch 环内 zombie 不再生成——f3540 新 spawn
  = cow 被动）。
- **s9 黎明**（18300f）：face E 观日出（t0.058 = 07:12 dawn ramp）。

**终态钉**（full run 实证）：player (19.57,54,-16.57) **hp20 存活** /
hunger 7 / xp 0；库存 **stick 2 / bow 1 (dur 380) / arrow 4**（planks/
coal/flint/feather/torch 全消耗）；**4 火把 + table 落位**；entities
0（zombie 掉落 rotten_flesh + xp orb 120s 消散 PITS 417⑭）；time
0.058（dawn）。

### combat 力学（16g 核心创新 = 短程弓箭弹道 aim pin）
弓 = **短程武器**：shoot_speed 15 + kGravity 20 → 平射极限 v²/g =
**11.25m**；14-19m 处任何 ≤12-m 上仰 lead 都不中（arrow_sim.py 实证
19-m/14-m 全 NO HIT）——v14「19-m 站射 4 箭全 miss」= 纯弹道 drop
（箭初速 vy=0，19-m 落 10·(19/15)² = 16m）。combat 必须 **≤10m**：
1. **箭在 RELEASE 发射非 press**（bow draw 于 RMB press 累积、release
   wasReleased 才发射，4 箭发射帧 = press+96f）——aim→release 的
   121-f 里追击 zombie 推进 1.6m → 须按 **release 帧**位置 aim
   （zombie 朝玩家来 = 无水平 lead，只垂直 drop lead）。
2. **hold=100 保满力**：满拉 = 1.6s = 96f，但注册 1-frame 滞后给
   1.583s → power 0.99 → 4.95/箭 → 4 箭 19.79 < 20 → zombie 0.2-hp
   存活续 melee（v19 实证）；hold=100（1.667s > cap）保满力。
3. **弹道 lead（mid-window 取 ±1-m 边际）**：7-m = +1.85 / 5.6-m =
   +1.0 / 4.3-m = +0.65 / 2.5-m = +0.25（arrow_sim.py 逐 release 帧
   解 hit window）。
4. **knockback 定点迭代**：箭中 = 4 b/s 水平 + 2.5-lift 击退
   （arrowKnockback Phase 42），zombie 轨迹依赖哪几箭中 → aim 按
   **实测**位置迭代 pin（v15→v19：2→2→3→3→4 命中，5 轮收敛；
   aim_from_log.py 自动提取 release 帧 zombie 位 + 解 mid-window）。

## 门结论（门 228 全 PASS）

**2998/2998**（banner re-pin 1：test_debug_overlay v3.37.0→v3.38.0；
内容相位零新增单测——init 交叉检查 8 条目全量 boot 复验）。

**结构免疫第 109 次**【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——banner
= F3 文本面零像素，v3.28.0/v3.32.0 同款】+ canary 4×2 新≡旧二进制
（tm_v337 v3.37.0）RAW+savenorm EXACT【example 266f / tutorial_dig
561f / tutorial_build 571f / tutorial_fly 541f——弓/箭/knockback/火把/
mob 生成/磁吸全既有，3.38.0 = 纯内容相位 + banner，solo 无输入路径
结构性零成本】+ **guide_night ×2 RAW（169498 B）+ savenorm EXACT**
【18301f 全帧确定性——combat aim pin + knockback 定点 + 火把环 + 夜
巡视全帧锁定实证】。

**combat 力学实证**：zombie f150 spawn (20.5,-17.5) 4.6-m 自 spawn 即
追（s2 kite 期玩家受 6 伤 20→14，自然回血 s4 满 20）；s4 站射 4 箭
full-draw（hold=100）→ **zombie died f3524 (36.82,54,-15.56)**；f3540
新 spawn = cow 被动（zombie 死开槽，cap 4 重满——s8 巡视无 hostile
威胁，玩家 hp20 存活至 dawn t0.058）；终态 entities 0（rotten_flesh +
xp orb 120s 消散 PITS 417⑭）。

**性能 A/B 同档**【旧 = tm_v337 v3.37.0 + g139 双树 27 markers：OW
900f 无输入 PERF-WIN 三窗 upd med 0.575/0.582/0.586 vs 0.562/0.583/
0.579 同档 + render 0.653/0.710/0.672 vs 0.599/0.699/0.674 同档 +
swap 15.25/15.21/15.24 vs 15.34/15.23/15.24 同档 + changesApplied
9/3/1 vs 9/2/1【f600 3v2 = real-dt streaming 抖动带，门 207 先例】
+ 终档跨树 savenorm EXACT = 结构性零成本最强形】。

**门内捕获 PITS 419 八坑**【①弓短程 11.25-m 19-m 全 miss ②release
发射 ③hold96 欠拉 0.2-hp 存活 ④knockback 定点 5 轮 ⑤持弓 RMB 拉弓
非开容器 ⑥火把先走到点 ⑦zombie 自 spawn 即追 ⑧inv_grid=制作网格非
背包 + panel tap ≥25-f + bow P=g5】。
