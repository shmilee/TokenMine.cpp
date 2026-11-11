# Phase v3.39.0 — V3-16h① 扩展指导 demo 族：① 搭建下界传送门 + 去下界（门 229）

> 3.x「世界扩展」V3-16 相位 H/7 = dimension 章节首支（16h 拆两版：①
> 下界门 = v3.39.0 / ② 末影龙 = v3.40.0，16d 拆分先例）。scene/
> panel_click/索引内核全复用（16a 形）——**零内核**（下界传送门/
> 8:1 映射/下界 worldgen/flint_and_steel 点火全既有，V3-1 Phase B 门
> 194 基建）+ 内容（1 scene + 1 demo + 1 index 条目）。

## 实现

### scene guide_nether（mk_scene 手术，幂等 1969 B）
12×11 平台（x0..11, z-8..3, 顶 y54, stone y50-52 + dirt y53）于正午
（time 0.25），SURVIVAL，player (5.5,54,-2.0) 传送门前，**预置**：
flint_and_steel h0 + **obsidian 4×5 传送门框**（14 ring 格，平面
z=-4，x∈[4,7] y∈[54,58]，含角，未点燃）+ interior 6 格 air。obsidian
挖掘 = 15-s 裸手/格（37.5-s 错工具）= 14 格挖掘不现实 → **帧预置**
（prepared-start 面，16a/16f 同款；"搭建" = 预置帧 + 玩家点火）。

### demo guide_nether（10865f ≈ 03:01，98 actions，dimension seq 1）
tour = 搭建（预置）+ 点火 + 去下界 + 观下界：
- **s1 P0 开场**（240f）：未点燃的 obsidian 框。
- **s2 点火**（480f）：flint_and_steel 瞄准**底行顶面** (5.5,55,-3.5)
  → 命中面法向邻格 = interior (5,55,-4) → 同帧 `Portal activated at
  (5, 54, -4) (4x5)`。
- **s3 站入 + 传送**（693f）：walk 入 portal（z-3.5，AABB 重叠 portal
  格）→ 60-f 倒计时 `Portal entry: travel countdown armed` →
  `Traveled to nether at (1, 55, -1) from (5, 55, -4)`（8:1 映射，
  落点 = 生成帧底中心 (1,55,-0.5)，站在 obsidian 底行上）。
- **s4 下台阶**（772f）：walk +z 4-m 下 frame 到**下界地面 y53**
  （2-m 落，无坠伤）。
- **s5 观下界**（~10000f）：walk 回路 E/S/W/N（netherrack 高原）+
  C 第三人称 sweep ×3（玄武岩柱 + 熔岩海 + 无天穹红天 + 生成 portal）
  + 长静观。
- **s6 收束**：player 在下界地面 (0.97,53,2.87)。

**终态钉**：player 下界 (0.97,53,2.87) hp20（无坠伤/无熔岩）+
flint_and_steel h0（dur 63，点火耗 1）。**返程不做**（下界生成 portal
返程 = 900-f 冷却 + 2-m 上台帧难，② 末影龙 demo 的返程走 end exit
portal 更自然；本支聚焦"去下界"）。

### 门内捕获（PITS 420）
- **点火瞄准 = 底行顶面（朝内面）**：aim (5.5,55,-3.5) 命中 (5,54,-4)
  的 +y 面，法向邻格 (5,55,-4) = interior → 点火。aim 外侧面 = 机制
  正确无反应（PITS 386① 同款）。
- **传送后 author track 失效**：travel = 瞬移（8:1 映射换维度），Guide
  的 track (self.x/y/z) 须**手动 re-anchor** 到落点（无 set_pos 公开
  API，直设属性）。
- **返程 auto-re-travel 陷阱**：站在生成 portal 内（落点即在 portal
  内）→ 900-f 冷却后 auto 返程 → 回到 overworld portal 内 → 再 auto
  去下界（乒乓）。**规避 = 下 frame 到地面（出 portal AABB）**，本支
  终态在下界地面（不在 portal 内）= 无 re-travel。

## 门结论（门 229 全 PASS）

**2998/2998**（banner re-pin 1：test_debug_overlay v3.38.0→v3.39.0；
内容相位零新增单测）。

**结构免疫第 110 次**【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——banner
= F3 文本面零像素】+ canary 4×2 新≡旧二进制（tm_v338 v3.38.0）
RAW+savenorm EXACT【example 266f / tutorial_dig 561f / tutorial_build
571f / tutorial_fly 541f——下界传送门/8:1 映射/flint_and_steel 点火全
既有，3.39.0 = 纯内容相位 + banner，solo 无输入路径结构性零成本】+
**guide_nether ×2 RAW + savenorm EXACT**【10865f 全帧确定性——点火 +
传送 + 下 frame + 观下界全帧锁定实证】。

**传送实证**：点火同帧 `Portal activated at (5, 54, -4) (4x5)` + 60-f
倒计时 `Portal entry: travel countdown armed` + `Traveled to nether at
(1, 55, -1) from (5, 55, -4)`（8:1 映射落生成帧底中心 (1,55,-0.5)）+
下 frame 到 nether 地面 y53（2-m 落无坠伤，player (0.97,53,2.87) hp20）。

**性能 A/B 同档**【旧 = tm_v338 v3.38.0 + g139 双树 27 markers：OW
900f 无输入 PERF-WIN 三窗 upd med 0.451/0.725/0.542 vs 0.710/0.804/
0.793 同档 + swap 15.25/14.98/15.31 vs 15.06/14.85/14.87 同档 +
changesApplied 9/2/1 逐位 + 终档跨树 savenorm EXACT = 结构性零成本】。

**门内捕获 PITS 420 四坑**【①点火 aim 底行顶面朝内面 ②传送后 author
track 失效须手动 re-anchor ③返程 auto-re-travel 乒乓 = 下 frame 规避
④下界 frame 悬空 2-m】。
