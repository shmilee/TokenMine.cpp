# Phase LGT-1 — union-region（**砍除记档**，v1.0 任务表 4/29）

> 2026-10-21（用户在场确认）。来源 = `docs/BACKLOG.md` LGT 行第 1 项：「同帧
> 2 个相邻 box 合并并集 region，省 ~25% 光照 job（99.x 残余 #2）」。

## 结论

**砍除**：step-0 插桩实测证伪设计前提——同帧选出的 2 列大多**远隔**而非
相邻，可合并帧占比 0.1%，预期节省 0.002 ms/帧 ≪ 1 ms 砍除线（PIPE-1 先例
翻转纪律，500× 过线）。设计案本身完成（含 1 个潜伏真 bug 的发现），留档备
未来「选列相邻化」方向复用。

## 设计案摘要（ora-2，已完成）

- **现状语义**：每帧 ≤1 光照 job，job 内对 m_lightPendingColumns 逐列建全列
  box（±16 块 footprint = 3×3 列），worker 顺序跑 `updateRegion`。稳态填充
  期每帧恰 2 box。
- **confluence 证明**：block 光并集 ≡ 顺序两次（逐位，shell 注入自愈）；sky
  光并集 ≥ 顺序逐点，唯一差异 = **修复潜伏 rescan clobber bug**（天花板骑跨
  footprint 边界时后一 region 的 rescan 擦除前一 region 的水平光流 = 1-2 格
  stale-dark 暗带；已入 KNOWN_ISSUES + PITS，未修）。
- **合并判据**：footprint 相交（双轴 gap <32 块 = |Δcol|≤2）传递闭包；
  Δ 任轴 ≥3 绝不合并（跨 gap 光贡献死亡，合并会擦旧列光）。
- **PIPE-1 排除规则零改动**（nextMeshColumns 不动，已证）。
- **实现范围**：~500-600 行（updateRegions 多 rect 单遍 API + updateRegion
  委托 + groupLightBoxes 纯函数 + ~11 单测含三轨 oracle）。

## step-0 实测（前提证伪的数据）

vd80 静止填充跑（场景列 (2,0)、scratch day_length 3600，16000f，插桩 =
postLightJobs 每帧 box 数/列坐标 + worker per-box/job 计时）：

| 指标 | 实测 |
|---|---|
| 稳态填充帧 n=2 box | **6418/6418（100%）** + n=1 一帧（启动） |
| 光照 job total | median **16.74 ms**、mean 17.37、max 39.36（per-box ~8.37） |
| 两列 Δ 分布（位置序） | 主导 (3..12, −15..−60) 类；**可合并（双轴 \|Δ\|≤2）= 6 帧 = 0.1%** |
| 预期并集节省 | **0.002 ms/帧**（≪ 1 ms 砍除线） |

**根因**：流送选列 = (distSq, x, z) 序。同 distSq 平局（同半径圆格点）时按
**位置序**取前两点 = 圆左弧 z=±z* 的**远隔对称对**（Δz = 2z* 典型 15-60
列）——设计案「最近 2 缺列 = 前沿相邻对（Δ 典型 (1,0)/(2,0)/(1,1)」的几何
直觉不成立（PITS 已收录）。600f 坐标探针实证：f4 (2,−2)(2,2)、f8 (0,2)(4,−2)
等。

## 处置与残余

- **砍除记档**（用户 2026-10-21 确认，推荐项）。BACKLOG 任务条 + 矩阵 +
  变更记录更新；99.x 残余 #2 划线。
- **光照 job 下限保留**：~16.7 ms/帧 = 流送 fps 主下限（PIPE-1 后 fps ~35-40
  的另一锁）。潜在方向（表外）：① 选列 tie-break 相邻化（同 distSq 按弧序
  配对）+ union —— STRM 相邻变更，需新设计评审 + S2 流送序列重锚；② sky
  shell seed 修 clobber（改动面大，留 100+）。
- **clobber 潜伏 bug**：未修（修复形态 = LGT-1 union 或 sky shell seed；
  KNOWN_ISSUES 有条目，暴露面窄 = 骑跨边界的天花板地形）。
- **插桩纪律**：提交前全删（grep LGT1 零命中、1212/1212、0 警告）；首跑
  SIGSEGV = moved unique_ptr 空引用（PITS 有条目）。
