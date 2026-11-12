# Phase v3.40.0 — V3-16h② 扩展指导 demo 族：② 打末影龙（门 230）

> 3.x「世界扩展」V3-16 相位 H/7 收官支（16h 拆两版：① 下界门 =
> v3.39.0 / ② 末影龙 = v3.40.0，16d 拆分先例）。scene/panel_click/
> 索引内核全复用（16a 形）——**零内核**（末地传送门嵌眼/travelEnd/
> 末影龙 boss/水晶/珍珠/爆炸/armor 全既有——V3-10a-d + V3-10b +
> V3-14a + SURV-1 基建）+ 内容（1 scene + 1 demo + 1 index 条目）+
> 工具链增量（pearl 轨迹探针）。

## 实现

### scene guide_dragon（mk_scene 手术，幂等 2006 B）
12×12 平台（x0..11, z0..11, 顶 y54, stone y50-52 + dirt y53）于正午
（time 0.25），**SURVIVAL + mobs ON**（dragon spawn 门
playerInEnd ∧ mobs），player (2.5,54,2.5)，**预置**：ender_eye 12
(h0) + ender_pearl 1 (h1) + **netherite 甲 4 件 (h2-h5)** +
end_portal_frame 5×5 环 12 格（center (6,6)，未嵌眼——frame survival
不可得 = 场景预置，nether obsidian 帧同款 prepared-start）。4 角 =
缺口（玩家入 center 通道）。

### demo guide_dragon（15214f ≈ 04:04，dimension seq 2）
tour = 装备 + 嵌眼 + 传送 + 珍珠 + 破晶 + 仪式 + 返程：
- **s1 P0 开场**（240f）：未嵌眼的 end 环。
- **s2a 装备**（446f）：netherite ×4 RMB 即穿（SURV-1 held-piece
  链，无面板——20 AP = 80% 减伤，enderman 6→1.2/击）。
- **s2 嵌 12 眼**（1328f）：选 h0 + 入 center（NW 角缺口）+ 11 格
  任意序 + **GOOD cell (6,4) 末位**（ring 扫描 8 候选中心，PITS 421）
  → 同帧 `End portal activated at (6, 54, 6)`。
- **s3 站入 60-f → 传送**（1533f）：`Traveled to end at (40, 53, 0)`
  （1:1，落 entry platform (40.5,53,0.5) 朝西）；dragon 60-f 后
  spawn (0.5,58,0.5) + **--dragon-damage 200 同帧杀**（f1442
  `mob 'ender_dragon' died`）+ 仪式（egg (0,54,0) + 23 orb 200XP +
  240-f ascension）。
- **s4 珍珠西掷**（1597f）：28° 仰角 30-b/s 弧 = 40-m 全程一掷
  （flat 仅 12-m）→ **`pearl teleport to (1.89, 54.69, -4.30)`**
  （2.5-m offset + 落 frame (1,53,-2) 顶 + 跌落复锚 y53）。
- **s5 入 center**（1905f）：step-up frame + step-down 入 3×3 中心
  → (0.61, 53, 1.37)（step 漂移 0.4-m，honest-landing pin）。
- **s6 破 2 出口水晶**（2233f）：(2,54,0) **f1960** + (-2,54,0)
  **f2019**（bare-hand hardness-0 瞬破，explode r3 爆坑 28 格
  endstone，frame destroyable-false 存活；塔晶 ±50 off-route CUT
  偏离记档）+ 观蛋（**蛋完好**——center 站位眼位离 egg box ≥1-m，
  PITS 422④）。
- **s7 站 center 等冷却**（3433f）：900-f 共享冷却 elapsed → 60-f
  倒计时自动 arm → **返程** `Traveled to overworld (end exit) at
  (0, 54, 0) from (0, 53, 1)`（落 overworld spawnPoint (0.5,54,0.5)
  平台 NW 角）。
- **s8/s9 主世界收束**：观点亮 end 门（9 格 portal + 12 格 eyed
  frame）+ sweep。

**终态钉**（gd19full 实证）：player overworld (0.5,54,0.5)
**hp20**（自然回血满）/ xp **92**（11/23 orb 磁吸收集；12 早落 orb
穿未载 chunk 沉基岩洋 y5 持久 end_xp_entities——streaming 时序产物
记档偏离）/ hunger 8 / **netherite 甲 4 件各 6.5 损** / inv 空
（12 眼 + 1 珍珠消耗）；end 段：egg (0,54,0) 完好 + 水晶 0 + 32 air
编辑（爆坑 28 + enderman carry 4）+ 12 orb y5；ow 段：lit portal
9/9 + eyed frame 12/12；time 0.6726。

**--dragon-damage 200 = 必带 CLI seam**（龙战不可手编 = 耦合移动
目标定案，10d1 同款；无 flag 回放 = 龙不杀 demo 不完整——记档）。

### 门内捕获（PITS 422 八坑）
① 传送后视图 re-anchor = yaw + 位置（pitch 已同步勿覆写——gd5 yaw
stale 珍珠飞东南 + gd6 pitch 覆写 -18° 珍珠 4-m 两死）② 珍珠射程 =
仰角主导（flat 12-m / 28° 40-m）③ 珍珠穿晶 = shatter_on_arrow
全 projectile 扫掠吞珍珠（gd10，z-0.6 aim 绕行）④ egg box = LMB
ray t0 陷阱（眼位入盒即挖，hardness-0 瞬破 + 3600-f 重生，gd14）
⑤ chunk 载竞 = 珍珠穿未载地形（gd17 244-f 虚空飞）+ orb 沉基岩洋
（12/23，throw timing 相对 streaming 钉）⑥ step 事件行走漂移 0.5-m
（gd15，honest-landing pin）⑦ rim 大 yaw 多 look 模型漂 = 破晶全
miss（gd16/18，center 小旋站位解）⑧ 出口门环 = 1-block 实心墙
（入 center = step-up + step-down；egg 悬 center 上空非实心）。

## 门结论（门 230 全 PASS）

**2998/2998**（banner re-pin 1：test_debug_overlay v3.39.0→v3.40.0；
内容相位零新增单测）。

**结构免疫第 111 次**【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——banner
= F3 文本面零像素】+ canary 4×2 新≡旧二进制（tm_v339 v3.39.0）
RAW+savenorm EXACT【example 266f / tutorial_dig 561f / tutorial_build
571f / tutorial_fly 541f——末地嵌眼/传送/龙/水晶/珍珠/armor 全既有，
3.40.0 = 纯内容相位 + banner，solo 无输入路径结构性零成本】+
**guide_dragon ×2 RAW + savenorm EXACT**【15214f 全帧确定性——嵌眼 +
传送 + 珍珠弧 + 破晶 + 仪式 + 返程全帧锁定实证，mobs-on + --dragon-
damage 200 臂】。

**End 段实证**：pearl teleport (1.89, 54.69, -4.30)（28° 弧 40-m）+
破晶 f1960 (2,54,0) / f2019 (-2,54,0)（explode r3 爆坑 28 格 +
enderman carry 4 = 32 air 编辑）+ egg 完好（center 站位眼位离 egg box
≥1-m）+ 返程 `Traveled to overworld (end exit) at (0, 54, 0) from
(0, 53, 1)` + 终态 overworld (0.5,54,0.5) **hp20** / xp 92（11/23
orb 收集；12 早落 orb 穿未载 chunk 沉基岩洋 y5 持久 end_xp_entities =
streaming 时序产物记档偏离）/ netherite 甲 4 件各 6.5 损 / inv 空 /
lit portal 9/9 + eyed frame 12/12 / time 0.6726。

**性能 A/B 同档**【旧 = tm_v339 v3.39.0 + g139 双树 27 markers：OW
900f 无输入 PERF-WIN 三窗 upd med 0.523/0.576/0.565 vs 0.527/0.583/
0.557 同档 + swap 15.38/15.25/15.27 vs 15.28/15.24/15.29 同档 +
changesApplied 逐位 + 终档跨树 savenorm EXACT = 结构性零成本】。

**门内捕获 PITS 422 八坑**【①传送后视图 re-anchor = yaw + 位置
（pitch 已同步勿覆写——gd5/gd6 两死）②珍珠射程仰角主导（flat 12-m /
28° 40-m）③珍珠穿晶 = shatter_on_arrow 全 projectile 吞珍珠（gd10）
④egg box = LMB ray t0 陷阱（gd14）⑤chunk 载竞 = 珍珠穿未载地形
（gd17）+ orb 沉基岩洋 12/23 ⑥step 事件行走漂移 0.5-m（gd15）
⑦rim 大 yaw 多 look 模型漂 = 破晶全 miss（gd16/18）⑧出口门环 =
1-block 实心墙】。
