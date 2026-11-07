# Phase v3.27.0 — V3-10d2 召唤与复杀（门 217 全 PASS，2026-11-07）

## 范围与定案

**V3-10d 末影龙相位 D2/2（收官）：召唤序列 + 复杀闭环**（≈10c×1，
侦察 §7/§9/§13 全采纳）。龙核心/首杀 = v3.26.0；本版补上 MC 的
「破蛋 → 4 晶召唤 → 复杀 8XP → 蛋 60s 重生」完整循环。**format 32
不变**（召唤/蛋计时/败亡旗全会话态；蛋 = 世界态既有面）/ **sim 2
不 bump**（canary 4 全 OW EXACT 实证）/ 零 config / 块 135 不变（
crystal_base = 既有 4 块新旗，非新块）/ 声音·atlas·效果·配方不变 /
**字体零新增**（F3 龙态行全英文——528/528 满区不碰，R7 规避）。

- **`crystal_base` 块旗**（strict bool 缺省冷）= obsidian / bedrock /
  end_portal_frame / end_portal_frame_eyed 4 块置 true（MC 基岩/黑曜
  石 + 项目出口帧等价）；**`requires_crystal_base`**（strict bool）=
  end_crystal 独占。放置门（placeBlock 数据门，零物种 switch）：
  携 requires 的块只能贴 crystal_base 块的面放置——**end_crystal =
  生存可放置**（10d1 配方 glass10+eye+membrane 的生存闭环）。
- **蛋 `blast_resistance` 3**（obsidian 孪生——MC 蛋近不可被晶爆
  毁的读：出口晶爆半径 3 在蛋位局域强度 1.0 < 3 存活；10d2 门内
  发现——无 br 3 破晶会把蛋炸掉）。
- **召唤序列**（game pass 1b，单收口）：**4 出口面晶全在 ∧ 无蛋 ∧
  无龙** = 60f 倒计时武装；面晶破/蛋回/龙活任一 = 取消（MC 规则，
  会话门）。面晶词表 = kEndSummonFaceSlots 4 格（±X 对 = 竞技场出口
  晶【arena 晶算数】，±Z 对 worldgen 缺席 = 只能玩家放置）。**完成
  时**：4 面晶引爆（shatterCrystalAt 单收口——相邻晶被彼此的爆
  波及 = MC 的 4 晶同时爆读【门内实证：2 条 shatter 行 + 2 条爆
  波及，终态 4 格全 air】）+ 待重生蛋计时清除（召唤移除 ≠ 玩家
  破蛋，MC 无召唤后无蛋读）+ **缺位塔晶回写**（worldgen 态幂等重
  写，已载 chunk only——未载塔 = worldgen 新鲜 = 完好，10d1 普
  查读）+ **显式 60f 生成延迟**（首访门被败亡旗封闭后的唯一复
  生路）。
- **败亡旗 `m_dragonDefeated`**（会话态，首杀入账置位不清除）：
  10d1 生成门「无蛋 → 生成」= **首访读**——败亡后蛋的缺席是**破
  蛋**（召唤前置），不是生成信号【门内捕获：破蛋 60f 后龙无召唤
  复生 = MC 语义破坏】；重载 = 旗复位（D6「无蛋 → 龙回」重载读保
  持）。
- **蛋会话重生**：玩家破蛋 → 3600f（60s，MC ~1min 同读）死线回写
  (0,54,0)（air-only 护栏）；**重载复位计时器 = D7 偏离**（蛋存在
  性落盘不受影响）；召唤序列清除待重生计时（见上）。
- **复杀 XP = 8**（单颗 8 值 orb——MC 500/12000 比例 1/24 × 项目
  200 的 D5 读；首杀 = 10d1 的 200 burst 不变）。仪式入账按
  m_dragonDeaths 分派（#1 = 200 burst，#2+ = 单 8 orb）。
- **--dragon-damage 每生成施加**（10d1 的一次性读改 per-spawn——单
  次生成行为不变，同跑可走全 杀→破蛋→召唤→复杀 循环 = 门臂的决
  定论）。
- **F3 第 21 行 = dragon**（英文开发者工具纪律）：none / spawn Nf /
  orbit / strafe / perch / ascend N/240 / summon Nf / egg / egg
  respawn Nf——boss 会话态全可见。

## 变更文件

数据 6：blocks/{obsidian,bedrock,end_portal_frame,
end_portal_frame_eyed}.json（+crystal_base）/ end_crystal.json（+
requires_crystal_base）/ dragon_egg.json（+blast_resistance 3）。
代码 ~10：block.hpp + block_registry（2 strict bool 旗）/
game_interaction（放置门 + 破蛋计时）/ game_frame（pass 1b 召唤 +
蛋重生 + 复杀 XP + per-spawn seam + 10d1 负中心词表修正）/
end_portal.hpp（kEndSummonFaceSlots + 中心修正）/ game_local.hpp（4
常量）/ game.hpp（EndDim +4 会话字段）/ debug_overlay.{hpp,cpp}（
+dragon 行）/ game_ui（F3 填充）。
测试：test_ender_dragon +4（DragonSummon 族：strict bool 负例 /
shipped 基座集恰 4+1 / 面晶词表钉 / 常量钉）/ test_debug_overlay
+1（DragonLine）+ 3 re-pin（21 行）/ 10d1 词表修正无 re-pin（塔晶
/ 面晶单测全过 = 修正前的单测用的是整数 cell 词表）。

## 门 217 结果

- **S1 结构免疫第 98 次**：≡ base19 逐字节 ×2 + savenorm EXACT
  （召唤/放置/蛋门全冷：无晶在手持 + 无蛋 + F3 未开 = 位级根）。
- **S2 召唤+复杀臂 = authored demo ×2 RAW + 逐字**（场景 = fresh
  末地，玩家 (4.5,53,0.5) yaw π/2 创造手持 end_crystal×64；
  --dragon-damage 200 每生成）：f59 spawn1 + 杀1 + 蛋 + **200 XP
  burst（23 orb f124..f234 逐位）** / f321 破出口晶（hold，蛋
  br3 存活）/ f322 破蛋（hold 续，**respawn f3922 调度**）/
  f340/f355/f373 三面晶放置（crystal_base 门，RMB 帧位钉）/ **f373
  summon armed（4 面晶）** / **f433 sequence complete**（2 shatter
  行 + 2 爆波及 = 4 晶同爆读）/ **f493 spawn2 + 杀2 + 蛋2 + 8 XP
  burst（单 orb value 8 f558）**；600f 终态 = 升天中。
- **S2a 无输入末地臂 600f 回归**（×2 RAW + EXACT，summon 行 = 0
  = 门惰性实证）。
- **10d1 臂回归**：kill 臂（f59 死 + 200XP 逐字 ×2 RAW + EXACT——
  per-spawn seam 单次生成行为不变）+ 破晶 demo 臂（f151 shatter →
  f152 strafe + 火球 f152/f303 逐位 ×2 RAW）+ 重载臂（蛋在无复生
  ×2 RAW）全同 10d1。
- **S6 十六面点名**（10d1 十四面复验 + crystal_base 字符串 /
  requires_crystal_base 数值 2 新面，全 rc 1 点名）。
- **canary 4 ×2 RAW + savenorm EXACT**（sim 2 不 bump 实证）。
- **性能 A/B 同档**（旧 = 94dc3d9 v3.26.0 + g139 双树）：banner 实证 v3.26.0/v3.27.0 + 双场景——OW 900f 无输入：PERF-WIN 三窗 upd med 0.561/0.562/0.571 vs 0.556/0.571/0.573 同档 + render 0.619/0.656/0.661 vs 0.630/0.697/0.638 + swap 15.3 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形；末地战斗 600f（mobs-on 变体，双侧龙在场）：upd med 0.870/1.017 vs 0.841/1.023 同档 + render 0.502/0.584 vs 0.503/0.569 + swap 15.2 同档 + ents 0.39/0.46 vs 0.36/0.47 同档 = 10d2 增量（放置门/蛋/召唤/F3 全冷）无可测自成本】
- **2964/2964**（+5：DragonSummon 4 + DragonLine 1）；
  `-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- **门内捕获 5 坑全 PITS 409**：① **负 cell 中心 = -N+0.5 非
  -N-0.5**（10d1 kEndCrystalSlots/kEndTowerSlots 负侧中心全偏 1
  格——普查走整数 cell 词表故玩法正确，箭碎扫掠 floor(中心) 在
  -X 侧静默测错格；10d2 面晶词表单测抓获，词表修正后 10d1 全臂
  回归同值）② **无蛋 ≠ 首访**（破蛋 60f 后龙无召唤复生 = 败亡旗
  缺失，门臂首跑实证）③ **demo 同帧 down+up = 死点击**（边沿同帧
  抵消——点击须 ≥2 帧 hold；hold 续帧按新瞄准重新取目标 = 创造即
  挖沿射线续破 reach 内下一块，破晶→破蛋一 hold 完成）④ **demo 旋
  转 dy 逐目标累计**（同 pitch 的第二目标重发 dy = 过旋，门内 1
  次返工）⑤ **门脚本 /tmp 路径被重写**（mv 目标 /tmp/x.bak 落源目
  录 = bak 失踪假象——cp+rm 显式目录）。

## MC 偏离记档（10d2）

1. **蛋重生 = 60s 会话计时**（重载复位 = D7 偏离续；蛋存在性落盘）。
2. **复杀亦有蛋**（10d1 D7 续——MC JE 仅首杀有蛋，项目简化记档）。
3. **召唤 = 4 面晶（arena ±X 晶算数）+ 无蛋 + 无龙**——MC 的召唤
   还需龙已死（本版的「无龙」含升天完成——尸身在 = 不召唤，MC 同
   读）。
4. **召唤序列 60f 倒计时**（MC 无显式延迟——晶放完即爆即召；项目
   的 1s 读 = 可取消窗，偏离记档）。
5. **4 面晶引爆消耗**（含 arena ±X 晶——召唤后需重补 2 颗才能再
   召唤；MC 同款消耗）。
6. **塔晶回写 = 召唤时补缺**（MC 召唤不补塔晶——塔的晶在 MC 里
   重生龙时不恢复；项目补 = 治疗源恢复，偏离记档）。
7. **crystal_base = 4 块**（obsidian/bedrock/出口双帧——MC 的
   crystal base 是专属块；项目无专属块 = 既有块旗化，偏离记档）。
8. **蛋 blast_resistance 3**（MC 蛋 br 360000——项目 obsidian 刻度
   的孪生读，晶爆 r3 存活 = 语义等效）。

## 后移（矩阵余项）

- **V3-12 多人**（最大项，立项侦察先行）。
- 10d 尾巴（非矩阵，KNOWN_ISSUES 挂账）：末地 orb 持久化
  （format 33 复议）/ endermite / 定身 / 跨维池复议 / F3 之外的
  召唤 UI（如 crystal 数量提示）。
