# Phase LGT-2 — 叶子光衰减（v1.0 任务表 第 5/29 项）

> 2026-10-21（用户在场授权"按顺序继续下两个任务"）。来源 =
> `docs/BACKLOG.md` LGT 行第 2 项：「BlockProperties 加 0-15 opacity 字段 +
> 传播算法改（渲染条）」。LGT 行 2/2 全清（LGT-1 砍除 + LGT-2 完成）。

## 设计定案（ora-2，全推荐项）

- **字段**：`light_opacity` 0-15（MC light_opacity 同款刻度：15=全挡不穿透、
  0=air 同款、中间=每格扣 N）。**缺省由 `opaque` 派生**（opaque→15 / 非
  opaque→0）= 未设字段的全部 shipped 内容**逐位复现旧行为**（backward-compat
  契约；存量 1212 测试零改动全过 = 实证）。越界 [0,15] 点名拒绝。
- **流体不并入字段**：保留代码常量 `kFluidLightAttenuation = 3`（P51 文档化
  简化不动；water/lava JSON 零改动）。
- **统一进入格成本**（私有助手 `cellOpacity` 一处定义）：
  `enterCost = 流体 3 / 垂直天空光 O / 其余 max(1,O)`；rescan 段内每格
  `light = max(0, light − O)`；O=15 = 不穿透（替换 3 处 `id != air && opaque`
  判定）；emission 源格自身持值不受影响（glowstone 范式）。
- **「渲染条」= mesh 烘焙侧零代码**：新光值经既有 dirty → PIPE-1 快照 mesh
  → 列合并 GL 管线自动落地（排除规则输入与 footprint 不变，mesh_job 零触碰）。
- **内容**：leaves = 2（任务名，MC 值）、glass = 1（MC 值；垂直 0→1 =
  顺手修复现状玻璃顶垂直免费穿过）；glass_pane/wheat/cross/torch = 派生 0
  不动；其余全部派生 = 现状。cross/glass_pane 薄模型 O=15 init 守卫
  （"must not be opaque" 矛盾同类扩展）。

## 变更文件

- `src/block/block.hpp`（+`lightOpacity` 字段 + 派生注释）
- `src/block/block_registry.cpp`（解析 + [0,15] 点名 + 派生 + 薄模型守卫扩展）
- `src/world/lighting.{hpp,cpp}`（`cellOpacity` + enterCost/rescan/不穿透 3
  代码点 + hpp 传播模型注释改写）
- `assets/data/blocks/leaves.json`（+2）、`assets/data/blocks/glass.json`（+1）
- `tests/test_block.cpp` / `test_growth.cpp` / `test_lighting.cpp`（+14 新）
- `docs/`（CONTENT_GUIDE light_opacity 节 / MECHANICS 光照节 / KNOWN_ISSUES
  clobber 交互注记 + 冠下生长行为条 / ROADMAP / BACKLOG / AGENTS / 本存档）

## 测试

1226/1226（**存量 1212 零改动全过** = 派生 backward-compat 实证 + 14 新，
`-Werror` 0 警告）：解析合法/越界点名/派生三例、薄模型 O=15 守卫、
SkyVerticalThroughLeaves（冠 1/2/8 层 → 13/11/3）、SkyHorizontalThroughLeafWall
（8 层外死光）、BlockLightThroughLeaves（glowstone + 叶墙 1/3/8 层 → 13/9/3 +
opaque 0 对照）、GlassVerticalCostOne（现状 0 的变更点钉死）、
AirAndWaterUnchanged（现状值回归）、UpdateRegionWithLeavesMatchesFullPropagation
（增量 vs propagateAll expectSameLight）、内容钉死（真实 assets：leaves=2 /
glass=1 / stone=15 派生 / wheat_1=0 / 流体 3）、生长门边界（冠下 9 临界过 /
7 停长）。

## 构建与验证结果（E2E）

- **S1 base18 第 83 门 = 重锚**（基线视图含 4+ 叶冠 = 首个「光值类」重锚，
  P41/P59 UV 类先例之后）：新二进制 fresh 60f ×2 RAW 字节一致（新锚）+
  基线二进制（tm_lgt1_baseline = 9556694）×2 + **A/B diff 212,174 px 限定
  世界表面**（旧图像素分类：stone-gray 96,497 / leaf-green 72,957 /
  dirt-brown 24,108 / other 18,610 / **sky-blue 2**；top-120 天空行 sky 差
  0）、**仅暗化**（亮化 40 px = AA 边）、天空像素数一致（skygate
  (120,181,242) 75,348 px 双严格）+ 红心 0。
- **S3a 封盒（仅 glowstone，无叶/玻璃）：全帧新旧二进制字节一致**（+ 新
  ×2 RAW）= 派生 O 的 **E2E 级**逐位证明（封盒排除背景叶冠变量）。
- **S2 封盒叶探针**（row A 1 叶 / row B 3 叶 / 3 层冠+顶 glowstone /
  control 顶 glowstone，全 block-light 探针免天空）：新 ×2 RAW；old vs new
  diff 40,394 px **darkened 40,388 / brightened 2（AA）**，簇 = 探针面
  亮度带（row A 面 13 / row B 面 9 / 冠下顶 9 / **control 顶 15 不变** =
  互斥带）+ 叶线相邻地板带（穿过叶的旧最优路径 +1~2 暗化，衰减公式推论）；
  四边缘 strip top/left/right 零差、TR 象限零差 = 无全局语义泄漏。
- **S3b 熔岩湖岸**（流体固定 3 回归）：old vs new **零差**（视角无叶冠）+
  新 ×2 RAW。
- **S4 输入放置（推迟）**：away 组件窗口隐藏复发（用户离桌）→ pointer lock
  6 连败 ×6 跑（窗口 IsUnviewable，点击未验证）→ 推迟至用户在场补跑
  （机制由单测覆盖：增量重光 fixture + 放置路径；瞄准几何已探针验证：
  射线命中 control 东面面心 → 放置格 (26,36,4)，光 12）。
- 几何探针（3 轮截图迭代）：S2 首版 control 与冠同深度 x 遮挡（视线角
  0.18 块）→ control 移 z=4；S4 首版射线打叶面 z 边缘（DDA tie-break
  歧义）→ 相机移 z=−3.5 对面面心（PITS 边缘瞄准先例）。

## 已知问题 / 偏离

- 冠下作物生长收紧（MC 一致，非缺陷）：wheat growth_light=9，3 层冠下
  临界仍可长、叠冠停长（KNOWN_ISSUES 有信息条；wheat 值不改 = MC）。
- clobber 潜伏 bug 交互：LGT-2 下后果从 stale-dark 扩到 stale-partial
  （暴露面量级不变，修复形态不变 = sky shell seed / union，留 100+；
  KNOWN_ISSUES clobber 条有交互注记）。
- 玻璃垂直 0→1 变更（现状非 MC 的顺手修复）：单测 GlassVerticalCostOne
  钉死 + S2 封盒无玻璃隔离（玻璃断言由单测覆盖）。
- 流送光照 job 下限 ~16.7 ms/帧不变（LGT-1 砍除残余；本 phase 性能中性：
  rescan 同阶 + BFS 每邻常数项）。

## 下一步建议

- 用户在场时补 S4 输入放置跑（away 模式休眠后，~5 min）。
- BACKLOG 顺序下一项 = **GEN-1 面条洞穴**（worldgen 变更 = 重锚候选；
  设计案待派）。
