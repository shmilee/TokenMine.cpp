# Phase 19 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 19 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 19]` 实体间互相穿过（P10，掉落物↔掉落物与通用部分）** — Phase 19 把 Phase 16 的玩家↔掉落物分离推广为通用实体间碰撞（分离 pass）：`Game::updateEntities` 在实体更新后对固定序 body 列表（玩家先、实体按索引）跑 `separateEntityList`——重叠对沿最小穿透轴（x→y→z 优先）推离：两个可推动体 50/50、对不可推动体走全量面到面跨度（玩家不可推动，P16 规则）；世界感知（`Entity::hasRoomAt` 严格格扫描，与碰撞解析同一约定）：体只移到其 AABB 可占据的位置，被 solid 挡住的份额由另一侧全量接管、两侧都挡则跳过该对；重复 sweep 直到某 sweep 最大位移 <`kSeparationConvergence`(1e-6 块，亚视觉残差) 或 `kSeparationSweeps`(32) 硬上限。参与/可推动是 `Entity` 数据位虚函数（非按类型硬编码）：`collidesWithEntities()`（Projectile=false，箭穿过实体）、`pushableByEntities()`（Player=false）。`separateFrom` 从 ItemEntity 上移到 Entity 基类（P16 三个单测原样保留为回归锚）。**行为修正**：中心精确重合（Q 丢在玩家脚底是最常见情形）时 P16 代码实际把物品推向 −axis（`<=` 分支），P19 首版按"列表靠前者走 −"会推到 +x——由 P18 参考二进制 E2E 对照运行（重叠物品出现在镜像位置 (482,590) 而非 (798,590)）捕获，改为"可推动体走 −axis、两可推动体列表靠后者走 −"，玩家↔掉落物方向与 P16 逐位一致（新增 `PairTiePlayerItemKeepsPhase16Direction` 锚定）。**行为变化（P16→P19）**：玩家↔掉落物分离从无条件推变为世界感知——唯一可推方向被 solid 挡死时物品保持原重叠位而不再被推进墙内（P16 会嵌入墙）。F3 overlay 背包行 +实体计数（F3 门控，基线零影响）。新增 10 个单测（SeparationTest：pair 50/50/玩家不可推/玩家↔物品 tie 的 P16 方向锚/双可推 tie/最小轴 Y+floor guard/接触不分离/双不可推/Q 丢簇收敛确定性/投射物跳过/数据位）+ 2 个 debug-overlay 行格式更新，共 539/539。E2E：base（无输入 60f）×2 与 `base18` 逐字节一致（无实体场景 pass 零操作）；items（84f：锁定→pitch −1.15→Q 丢 stone→Tab→Q 丢 dirt 同位，运行在丢弃后 ~0.25s 结束以避开 0.5s 拾取延迟）×2 逐字节一致 + 像素判据（stone 灰 (125,125,125) @ (482,590)、dirt 棕 (138,88,70) @ (390,590)，收敛位 (0.075,54,0.5)/(-0.175,54,0.5) 与单测簇同构：地形探针确认出生点 ±2 列面高全 53、两物品列均为 grass+air）；P18 参考运行同脚本：重叠物品 @ (482,590)、(390,590) 为草地 (99,152,46)——分离效果由前后像素差证明。新简化（非求解器/无次轴/无切向滑动）记入「实体（Phase 10）」节。

## 会话存档（原 SESSION_CONTEXT）

- **范围与决策**：P19 = 通用实体间碰撞（KNOWN_ISSUES 优先级首位候选：掉落物互相穿过；
  未来 mob 的前置）。基线门控不变（base18）：无实体场景分离 pass 零操作，无输入运行必须
  与 base18 逐字节一致。
- **设计（最终实现）**：
  - `Entity` 虚函数数据位：`collidesWithEntities()`（默认 true；Projectile=false——Minecraft
    箭穿过实体）、`pushableByEntities()`（默认 true；Player=false——P16 不可推动语义）。
    非按类型硬编码。
  - `separateFrom` 从 ItemEntity 上移到 Entity 基类（ItemEntity 继承；P16 三个既有单测
    原样未动 = 回归锚）。**世界感知**（P19 新增）：会把体嵌入 solid 的 push 被拒绝
    （`hasRoomAt`），体保持原位——P16 旧行为是嵌入墙内（已记录的行为变化）。
  - 自由函数（entity 层）：`computeEntitySeparation(aMin,aMax,bMin,bMax,aTieNegative)`
    （纯几何：严格重叠——接触面不算；最小穿透轴 x→y→z 优先；distance = **面到面跨度**
    （如 a 在右 = b.max−a.min）——只推 overlap 量在宽度不同时留残差）；
    `separateEntityPair(a,b,aFirst)`（两可推动 50/50、对不可推动 100/0；世界感知 fallback：
    被挡份额→另一侧全量；双挡→跳过）；`separateEntityList(bodies)`（列表序 i<j 成对 sweep，
    玩家先=tie 优先序；任一侧 `collidesWithEntities=false` 跳过；重复至无移动 OR 每 sweep
    最大位移 < `kSeparationConvergence`(1e-6) OR `kSeparationSweeps`(32)）。
  - **Tie 规则（本阶段最关键决策）**：中心精确 tie（最常见——Q 丢在玩家脚底）时，P16 代码
    实际把物品推向 **−axis**（`<=` 分支；旧注释写 +axis，注释是错的），P19 首版"列表靠前
    者走 −"会把玩家推 −、物品推 +x（行为变化）。由 **P18 参考二进制 E2E 对照**捕获（重叠
    物品出现在镜像位 (482,590) 而非 (798,590)），改为"可推动体走 −axis；两可推动体列表
    靠后者走 −"，玩家↔物品方向与 P16 逐位一致；`computeEntitySeparation` 参数 `aFirst`
    改名 `aTieNegative`（纯几何函数不感知实体，tie 策略由调用方解析）；新增
    `PairTiePlayerItemKeepsPhase16Direction` 锚定。
  - `Entity::hasRoomAt(Vec3)`：严格格扫描（与 moveAxis 同一约定），分离永不把体推进
    solid。
  - Game pass：`updateEntities` 实体更新后 bodies=[player]+全部实体 →
    `separateEntityList(bodies)`（替代旧玩家↔物品循环）；在 merge pass 之前跑（分离
    ≤0.25 < kMergeDistance 0.75，同类合并行为不变）。
  - F3 overlay 背包行：`"inventory %s  particles %zu  entities %zu"`（F3 门控→基线零影响）。
- **收敛（测试捕获）**：竞争约束（物品贴玩家且贴下一物品）每 sweep 残差减半——纯 8 次
  硬上限留 ~0.001 块重叠 → epsilon 停（1e-6）+ 上限 32 修复。残差亚视觉（~1e-5 px）、
  确定性（同输入两次运行逐位相同，单测 EXPECT_EQ 锚定）。
- **验证**：
  - 539/539（P19 新增 10 个 SeparationTest + 2 个 debug-overlay 行格式更新）。
  - base（无输入 60f）×2 与 base18 逐字节一致（门控；tie 规则修正后重跑确认）。
  - items（84f：锁定→pitch −1.15（R 0 360）→Q 丢 stone→Tab→Q 丢 dirt 同位）×2 逐字节
    一致 + 像素判据：stone 灰 (125,125,125) @ (482,590)、dirt 棕 (138,88,70) @ (390,590)
    （收敛位 (0.075,54,0.5)/(-0.175,54,0.5) 与单测簇同构；地形探针确认出生点 ±2 列面高
    全 53、两物品列 (0,53,0)/(−1,53,0) 均 grass、其上 air，y=54 稳定支撑）。
  - **P18 参考二进制对照**（worktree @ 2ea1ce1 独立构建）：同脚本 → 重叠物品在 stone 位
    (482,590) 灰（两 cube 同位、深度测试先画者胜=stone）、(390,590) 草地 (99,152,46)——
    分离效果由前后像素差证明。
  - **拾取时序**：运行须在丢弃后 0.5s（拾取延迟）内结束，否则物品被吸回。N=84，xact 启动
    前 0.584s 标定延迟（物品 age @ 截图 ≈0.25s；age = build + 84/60 − (δ + 0.371)；
    实测 build≈0.01s（128 chunk 生成很快）、δ≈0.80s（含标定延迟）；机器负载变化 build
    时间需重测——驱动里 sleep 0.584 处有注释）。
- **坑位（本阶段新踩）**：
  - **tie 方向是语义不是实现细节**：P16 注释与代码不符（注释 +axis、代码 −axis）且无测试
    锚定 tie 情形 → P19 首版"自然"改变行为。规则：推广既有行为时，先把旧代码的可观测
    行为（含 tie/边界情形）写成测试锚；E2E 保留参考二进制对照（旧 commit worktree 构建 +
    同输入脚本 + 像素对比）。
  - 收敛上限必须"epsilon 停 + 硬上限"双保险：纯次数上限在减半序列下留残差重叠。
  - 测试助手 `maxAabbExtent`（各轴取 max）概念错——3D 重叠要求**所有**轴重叠 →
    `minAabbExtent`（min）才正确。
  - 探针程序（Mat4/Camera 投影）位置参数化（argv）：硬编码位置每次换场景都要重编。
  - 资产根是 `assets`（数据在 `assets/data/`），不是仓库根的 `data/`（探针程序
    setRoot 用错会报 "asset path escapes asset root"）。
- **E2E 环境**：新驱动 `/tmp/opencode/p19_e2e.zsh`（基于 p17_e2e.zsh；`xact.c` 加 `q` 键
  = Q 丢弃；窗口**快速轮询**替代 sleep 0.6 使 xact 锚点贴近窗口创建；timing markers
  t_game/t_window/t_xact + xact 日志 + 游戏日志（Scene built/Frame limit）可算物品 age；
  PIL 内置验证 exit 3，VERIFY=0 可跳过做探针运行）。P18 参考二进制在
  /tmp/opencode/p18ref worktree（可删）。探针：probe_p19.cpp（投影，argv 参数化）、
  probe_p19_terrain.cpp（复刻游戏世界生成：seed 123456789、demo 区 4×4 列×8 垂直
  chunk=128、输出面高+列块）。
