# Phase V2-17 — 顶部 slab / 双 slab（v2.18.0，门 189 全 PASS）

> 二批 GEN 行（BACKLOG:「中」：消费「slab = 仅下半块」记档
> KNOWN_ISSUES [Phase 85]；ENT-2 state_count 机制现成；剔除/mesh/
> 光照 LGT-5 先例三点适配）。MC 机制 11-04 编排者 wiki 核定（Slab
> 页：放置 = 面上半/底面 → 顶 slab / 下半 → 底 slab；同型上下同格 =
> 双 slab；**双 slab 破坏 = 整块掉 2**；跨型堆叠合并 1.14+ = 偏离 2
> 范围外）。fixer 两次交付（ses_e5ad16551ffere1odBmDo8sRDY：V2-17
> 主体 + 门机发现的 partial-aware raycast 范围缺口补口）。**零 format
> bump**（states = format 15 机制，stone_slab 无 state → 有 state =
> 同数组新条目）。

## 实现

### 数据面
- **stone_slab.json**：+ `state_count 3` / `state_default 0`（严格成对
  既有 :797-835）+ **新严格字段 `drops_count_by_state [1,1,2]`**
  （int 1..64 逐元素点名 / 空数组拒 / 非数组拒 / **长度 == state_count
  点名** / 无显式 state_count 的块携带 = 死数据点名拒【含 stairs
  隐式状态】；game_init per-state 计数 ≤ drop 物品 maxStack 交叉检）
  ——双 slab 破坏掉 2 = MC 材质守恒精确
- 纹理/音/硬度(2.0)/light_opacity(0 LGT-5) 全不变（atlas 122 零
  变更）；无新配方（双 slab = 放置合并产物）
- 床 15 块 = shape Slab 无 state_count → hasState false → 恒 state
  default 0 = **位级不变**（门 = hasState() 非 shape）

### 核心面
1. **solidRange 物理扩展**（entity.cpp：solidTopHeight 单 top 锚格底
   → `SolidRange{bottom,top}` 区间）：Slab 有 state：0→{0,0.5}（= 现
   值位级）/ 1→{0.5,1} / ≥2→{0,1}（读 World state byte，unset →
   stateDefault 哨兵回落 emitStairs 同款）；**床/stairs/墙/栅栏/玻璃
   板/满块现值逐位不变**（既有 bed/stair/mob-step/pathfind 钉族零
   改动全过红线）；消费点全更新：hasRoomAt/moveAxis（重叠判
   [y+bottom,y+top]，Y 上行钳位面 = y+bottom）/ stepUp（顶 slab 顶面
   y+1 恰 1.0 可踏升 = MC 同款；顶 slab 格内站立 = AABB 重叠全挡
   【偏离：无 sneak 缩高】）/ isRestingOnSolid / **mob 生成 spawnFloorTop
   纯函数**（stateful slab 0→0.5 / 1,≥2→1.0 / 其余恒 1.0，出生脚高
   cell.y+floorTop）
2. **emitSlab state 化**（chunk_mesher）：slab pass 读 per-cell state
   （emitStairs 同款）；**state 0 既有循环逐语句未动**（仅 wx/wy/wz
   上提）；`emitSlabTop`（y 0.5+corner×0.5 镜像，**底面恒绘**对称
   state 0 顶面恒绘，顶/侧通用 opaque 剔，按面邻采样不变）+
   `emitSlabFull`（双 slab 满格标准邻剔除，slab pass 内绘制，greedy
   跳过 :177-185 不动）；拼合面无共面双绘（双 slab = 单格表示；上下
   两格叠放 = 不同平面 + 顶 slab 底面被下 opaque 自动剔 = 零新剔除
   规则——state-1 侧邻底 slab 同层混高半面残差 = P85 记档局限同族）
3. **放置 slab 规则**（placeBlock :1173-1177 自称扩展点消费）：手持
   slab 物品（shape Slab ∧ hasState 数据旗）算命中点 = eye +
   camera.forward()×hit->distance（targetBlock 契约量）→
   `slabStateFromHit` 纯函数（**顶面 → 0** = 现行为位级根 / **底面 →
   1** = 天花板贴放 / **侧面 local-y > 0.5 → 1 否则 0** = MC 精确，
   0.5 严格不等式 kSlabHalf 单常量）；**合并** `slabMergesToDouble`
   （同 id 互补 state → 目标格 setBlock + state 2，消耗 1 = MC 材质
   守恒）仅绕 canPlaceAt solid 拒、保 Y 范围 + 严格玩家重叠内联 +
   跳 canPlantAt（第一半放置已验土壤）；常规路径 state 后置覆写
   （楼梯朝向覆写同款 ENT-2 模式，state 0 = 写零脏 no-op）
4. **破坏**：breakBlock 在 setBlock(air) 前读 state，主掉落 count =
   dropsCountByState[state]（双 slab → 2 件，确定性散布既有）
5. **蹲旋转 Slab 形排除**（偏离 4：state = 垂直位非朝向，(s+1)%3 环
   转 2→0 销毁半块材质不守恒；台阶朝向环不受影响）
6. **partial-aware slab raycast**（门机范围缺口补口——**双 slab 合并
   E2E 可达性根**）：cell-based DDA 下 slab 格全遮挡（射线穿空气半也
   命中该格）→ 同格合并目标不可达 + 「瞄准底 slab 空气半破 slab」偏
   离 MC。修 = 共享 ddaFirstCell walk 对 Slab 形格子格判定
   （`slabSolidRangeAt` 纯 helper：shape ∧ solid 门 + per-cell state，
   unset→default 与 solidRange/emitSlab 同款回落；双 slab = nullopt
   整格）：射线自空气半入格 = **穿透**（先测内表面 y=0.5 交点【tHalf
   ≥ t 入口贴面含等命中 ∧ ≤ min(tMaxX,tMaxZ) 恰边界命中约定；toward-
   solid 方向门：底 slab 需 dir.y<0 严格/顶 slab dir.y>0 严格——水平
   射线永远到不了内表面】→ 命中 = hit cell slab 格 + 内表面法线
   ±Y + t 位值；否则继续 DDA）/ 自实心半入格 = 整格边界命中现行为 /
   双 slab = 整格不变。**消费方共享一致**：块射线 5 消费点（挖掘/
   放置/蹲旋转/交互/面板）+ 桶射线 ×2 + 骨架 LOS + 第三人称遮挡 ×2
   仅加 registry 实参零特判；targetMob 走 raycastAabb 构造性不受影响；
   非 slab 格恒 nullopt → walk 逐语句旧分支位级不变。**记档偏离**
   （MECHANICS 破坏节）：瞄准底 slab 空气半 = 破身后块（MC 同款，
   cell-DDA 旧行为变更）
7. **零 format bump**：states 数组 format 15 机制（双向交叉检——
   stateful 块须有 states 条目【门 189 场景 builder 首跑实证】）；
   28 不变

## 变更文件（30）
核心：entity.{hpp,cpp}（solidRange + 消费点）/ chunk_mesher.{hpp,cpp}
（emitSlab 三态 + emitSlabTop/Full）/ game_interaction（放置规则 +
合并 + 破坏 per-state + 蹲门）/ block.{hpp,cpp}（kSlabHalf +
slabStateFromHit + slabMergesToDouble + dropsCountByState 字段）/
block_registry（严格解析）/ game_init（maxStack 交叉检）/ mob_
spawning + game_entities（spawnFloorTop）/ raycast.{hpp,cpp}（partial-
aware dda + 3 签名 +BlockRegistry）/ third_person + game_frame +
game_entities（签名实参 4 行）/ CMakeLists（2.18.0）；数据：
stone_slab.json；测试：test_raycast +12 / test_chunk_mesher +7 /
test_entity +6 / test_mob +4 / test_lighting +1 / test_block +10 /
test_third_person 加参 / F3 钉；文档：MECHANICS（slab 节重写 + 射线
半格感知 + 破坏偏离）/ GAMEPLAY（台阶节）/ KNOWN_ISSUES（划解 + 4
偏离）

## 门 189（old = v2.17.0 tm_v2170_base/perf = fd0e09a）— 全 PASS

### S1 = 结构免疫第 70 次（免重锚）
- 新侧 60f ×2 恒等 ≡ 现锚 179027 B **零 diff**（worldgen 不放 slab
  = slab pass 空转 + 无 slab 格 raycast 探针恒 nullopt）+ savenorm
  EXACT（零 format bump）
- fixer 终版二进制（含 raycast 补口）自证 = 第 71 次同结论

### S2 = 三臂 × 双侧输入 E2E（PITS 376/379 协议）
- **臂 a 顶 slab**：场景石 (2,56,2) + slab×1，aim 石底面 (2.3,56,2.7)
  【eye 上方块底面可见性几何：石须抬至 eye y 带上方否则侧面先命中】
  → NEW 终档 states {(2,55,2), 1} + 物品耗尽；OLD = 同格底 slab 无
  state（legacy 位级）= 互斥
- **臂 b 合并双 slab**：场景锚石 (2,54,1) + 底 slab (2,54,2)【**states
  条目必备** = PITS 383①】+ 玩家 (0.5,54,2.5) aim 锚 +Z 面上半
  (2.5,54.7,2.0) → **NEW（partial-aware 射线穿 slab 空气半命中锚面 →
  target = slab 格 → 合并）终档 states {(2,54,2), 2} + 物品耗尽**；
  OLD（cell DDA 命中 slab 格 −X 面）= 邻格 (1,54,2) 叠第二块无 state
  = 互斥最强形（一格 state 2 vs 两格无 state）
- **臂 c 双 slab 破 2**：场景 slab state 2 + 石地板（hold 断点保护）
  LMB hold 3.3s【hardness 2.0 ~3.0s 破 + 0.3s 余量入石 1.5 ~2.25s
  未破重置 = PITS 7/379】→ NEW 掉落 **2 件** slab 物品留地【距玩家
  2.83 > 磁力 2.5 = PITS 377 不拾取，断言地+库存总数】；OLD 底 slab
  破掉 **1 件** = 互斥
- **门协议发现（PITS 383）**：① stateful 块场景 journal 无 states
  条目 = init 拒（双向交叉检）→ 游戏不启动 → 输入静默丢失（首跑 b
  臂失败根因，探针链 = init 日志 → 零 capture → 场景文件回读）② cell
  DDA 全遮挡 = 合并不可达（probe 隔离：仅 slab 场景 vs 石块场景 vs
  旧二进制三组对照定位）③ 临时插桩定界后**移除重建**（工作树不污染
  fixer 构建）

### S3 套件
- **2455/2455**（2391 基线 + 23 V2-17 + 12 raycast；编排者独立复跑）

### S6 = 6/6 点名
drops 长度不配（2≠3）/ 零元素 / 非整数 1.5 / 无 state 块携带（stone
死数据）/ stairs + state_count（严格成对，ENT-2 回归）/ state_count
256 越界

### 性能 A/B PASS 同世界同成本（用户硬要求）
- 主对 fall 900f 无输入：PERF-WIN 三窗同档（upd med 0.44/0.61/0.62
  vs 0.53/0.54/0.45 ms）+ **changesApplied 9/1/3 逐位** + 终档
  fluids 16/16 + blocks 36/36 + **states 0/0 全集合恒等**（V2-17 不
  触 worldgen = 比门 188 演示区子集口径更强的全集合断言）
- **slab 结构自成本**（3 slab【底/顶/双】新侧 vs 3 底 slab 旧侧，
  600f 无输入）：upd med 0.62 vs 0.50 ms 同档 = 三态 slab pass 无增量

## 偏离（记档 4 条）
1. 双 slab = 单格 state 2 合并表示（一 cell 一 id 红线下的唯一表示；
   MC「同格上下两块」= 状态合并；破掉 2 = 材质守恒）
2. 跨型堆叠合并不做（同 id 才合并；MC 1.14+ 跨 slab 型 = 范围外）
3. 顶 slab 格内全挡（项目无 sneak 缩高 = 既有 KNOWN_ISSUES；MC 1.5
   高可钻）
4. 蹲旋转 Slab 形排除（state = 垂直位非朝向，环转不守恒；台阶朝向环
   不受影响）
+ 行为变更记档（MECHANICS）：partial-aware 射线 = 瞄准底 slab 空气半
破身后块（MC 同款，cell-DDA 旧行为变更）
