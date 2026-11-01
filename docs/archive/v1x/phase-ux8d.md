# Phase UX-8d — 掉落确定性伪散落（v1.44.0，门 164）

> 消费用户 v1.41.0 实机反馈①「打木头时，同一棵树底部掉落一个木头，
> 继续打上面还是掉落一个木头——两个木头不能掉在同一个位置？」
> **v1.44.0 = UX-8 体验细化系统第 3 版**（系统收官顺延 v1.46.0，
> 10-31 政策 tag 1:1 对应 release）。

## 根因与定案

现状：`Game::breakBlock` 的**所有**掉落（主 `drops` / `chance_drop` /
`chance_drops` 逐条目）生成在**被破方块中心**零初速——同块双掉落（树叶
→ 木棍+苹果）**完全重叠**、一列被破方块排成整齐中心线（视觉呆板，
用户问①本体）。

定案 = **确定性伪散落**（MC 散落观感，零 real-dt，硬规则 8）：

- 新纯函数 `block/drop_scatter`：
  `dropSpawnOffset(seed, x, y, z, dropIndex) → Vec2 (dx, dz)`
  - 专用 base salt `0x4E7A2C9D5B1F8E03`（与 kChanceDropSalt /
    kChanceDropsSalt / kCropRollSalt / kSaplingRollSalt 全不同 =
    互不追踪，grep 实证）
  - per-index salt = `splitmix64(base + dropIndex·golden)`（先推进后
    mix = PITS #4；同格多掉落去相关）
  - 链 = `splitmix64(seed + hash3(x,y,z,indexSalt))`；上/下 32 位独立
    半经 **double 精确映射 [0,1)**（`1/2^32` 对 32 位整数精确——float
    24 位尾数会 256 桶化，弃用 = chance_drop 53 位同款纪律）→
    `[-kDropScatterRange, +kDropScatterRange]`，`kDropScatterRange
    = 0.2f`
  - **界论证**：`ItemEntity::kSize` 0.25（半 0.125）+ 偏移 0.2 =
    0.325 < 0.5 = **物品恒留在被破格内**（不侵入邻格 solid，无碰撞/
    浮空新问题；单测 `StaysInsideTheBrokenCell` 钉死）
  - **y 不偏移**：垂直散落把物品顶进上方格/埋进下方格 = 物理歧义；
    MC 散落亦主要水平
  - **与 chance roll 构造性去相关**：偏移与 roll 各为 (seed, 位置)
    的纯函数且共享零 salt——偏移值不随 roll 命中/未命中变（无
    re-scatter）
- **breakBlock 接线**：`dropIndex` 0 = 主 drops / 1 = chance_drop /
  `2+i` = chance_drops 条目 i（与 `rollChanceDropEntry` entryIndex
  同序约定）；`dropSeed` 单点捕获（两处 roll 顺带复用同值 = 行为逐位
  同）；破坏音效保持格中心（声源 = 块非散落物）
- **范围**：只改破坏掉落。Q 丢弃（玩家脚位）/ mob 击杀掉落（mob 底心
  天然分散）/ 拾取返还 / 存档读档 / 粒子 / PRE-6 掉落光（散落物同格 =
  光格恒等）**零触碰**（ItemEntity 全部 6 构造点逐一核过）
- **合并安全**：同格双散落最大间距 √(0.4²+0.4²) ≈ 0.566 <
  `kMergeDistance` 0.75 = 同物双掉落仍必合并（改前 0.0 平凡合并）
- **零 format**（entities 段已存 pos = 散落位置自然入档；读档 =
  散落后的位置，无迁移）/ 零 config / 零新 JSON / 零 real-dt

## 测试

2033/2033（+7）、`-Werror` 0 警告、banner v1.44.0：

- `Bounds`：4 seed × 7×3×7 格 × 4 dropIndex 穷边界（|dx|,|dz| ≤ 0.2）
- `Deterministic`：同参 ×10 恒等
- `StaysInsideTheBrokenCell`：格内界论证钉（kDropScatterRange +
  kSize·0.5 < 0.5，含 item_entity.hpp 交叉）
- `OneBreakDecorrelatesAllDrops`：单破 4 选 2 全对不同偏移
- `SeedDependent`：不同 seed 不同偏移
- `NegativeCoordinates`：负坐标两补哈希（世界生成同款）
- `BandSpread`：32×32 带展布（range > 半带 = 非常数哈希）
- 既有掉落位断言测试演进 = **0**（无单测断言 breakBlock 中心位——
  test_entity 全为显式坐标直接构造）

## 门 164（编排者执行，old 侧 = v1.43.0 = d2b3218 = tm_v144_base）

- **S1 = 第 164 次结构性免疫 + base18 锚连续 168140 B 免重锚**：fresh
  60f 四方 ×2（new×2 + old×2）全字节一致 + **跨侧 RAW 字节一致**
  （无输入无破坏 = dropSpawnOffset 零执行）+ 跨版 savenorm EXACT。
- **S2 散落 A/B PASS**（门 162 实证瞄准几何：wood row (0,55,−4..−6)，
  玩家 W 0.8s → (0.5,54,−2.7) yaw 0 pitch −0.12，9.5s L-hold 恰破 3
  木 = 3.0s/木）：
  - **双侧确定性**：×2 RAW 字节一致（new 201619 B / old 201843 B）+
    同侧 savenorm EXACT ×2
  - **OLD = 格中心**：掉落 (0.5,54,−4.5) / (0.5,54,−5.5) exact + 第 3
    破（cell −4）掉落 (0.5,54,−3.5) **恰中心 = 距玩家 0.8 < kPickup
    Distance 1.0 → 自动拾入 slot9**（OLD = 2 实体 + 库存 wood 1）
  - **NEW = 散落 ±0.2 内且非中心**：三掉落 (0.357,54,−3.698) /
    (0.495,54,−4.492) / (0.577,54,−5.305)——逐格界检
    dx/dz = −0.143/−0.198、−0.005/+0.008、+0.077/+0.195 全 ≤ 0.2 且
    无一恰中心；**第 1 破散落位距玩家 1.008 > 1.0 → 不拾取**（NEW =
    3 实体 + slot9 空）= **拾取反转 = 终态互斥**（散落把物品推过
    拾取半径 = 用户可观测行为差异）
  - **跨侧 savenorm 恰 7 项 diff 全在散落/拾取语义**（entities len
    3/2 + 6 pos 分量 + slot9 count/item）；世界 blocks/fluids、玩家、
    tutorial {planks,0} 双侧同值零 diff = 同世界最强形
  - 像素 diff 7046 px 限掉落纵深列带（3 木沿视线纵深 = 竖直列，
    裁图肉眼终裁：NEW 近位散落掉落 tan 立方在位 vs OLD 同位空缺）
- **S3** = 2033/2033。**无 S6**（零 parse 面——纯函数，零新数据字段）。
- **性能 A/B v1.43.0 vs v1.44.0（发布前硬门槛）PASS**：fall 900f
  串行零并行——PERF-WIN 三窗 upd med 0.408/0.380/0.376 vs
  0.337/0.356/0.344、render med 0.430/0.436/0.439 vs
  0.388/0.406/0.400、swap med 15.71/15.72/15.74 vs
  15.80/15.84/15.86 = 全指标同档；PIPE3-S0 changesApplied **9/3/1
  三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 =
  同世界同成本**（无输入无破坏 = dropSpawnOffset 零执行 = 结构
  惰性最强形）。

## 偏离/记档

- 散落只作用于**破坏掉落**（Q 丢弃/mob 击杀 = 位置已天然分散，记档
  不扩大）；mc 的"弹射+滚动"物理 = 矩阵外（零初速静止观感 + 伪散落
  偏移 = 本项目确定性约束下的 MC 同款落点分布）。
- dropIndex 0 的 per-index salt = splitmix64(base)（0·golden 项平凡），
  与 chance_drops entry 0 约定一致（实现内注释）。
- 同格多掉落最大间距 0.566 < 合并半径 0.75 = 同物仍合并（树叶双掉落
  木棍+苹果为**异物**不合并但不再完全重叠 = 用户问①的树叶场景
  直接修复）。
