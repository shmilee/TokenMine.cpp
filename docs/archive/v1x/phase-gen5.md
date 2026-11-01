# Phase GEN-5 — 浮空树（v1.0 任务表 20/31，门 101）

> 用户报障（10-25）："洞穴生成之后，有一些树长在半空中。"代理定案 =
> placeTrees 表面格守卫（与 placeDecorations P45 同款规则，树是漏网案例）。

## 根因

生成顺序 `carveCaves` → `placeTrees`：`surface_guard 0`（shipped）使洞口可达
表面格本身；placeTrees 用 `heightAt`（地形高度，与洞无关）选树位 + 无条件
placeIfAir → 表面格被洞吃掉后整棵树悬空。placeDecorations（P45）有同款
守卫（"a flower would float over the hole"），树漏了。

## 实现内容

1. **谓词提取**（`src/world/world_generator.{hpp,cpp}`）：carveCaves 的逐格
   挖除判定提为 `bool caveCarvesCell(int wx, int wy, int wz, int height) const`
   ——cavesEnabled / spawn-clear 区（cell 所在 chunk）/ band
   [caveMinY, topExclusive) / 湖排除盒 / 双 valueNoise3 中位带（n2 仅 n1 在带
   内才求值 = 短路保留），**逐位搬移**；carveCaves 改逐格调它（两个 per-chunk
   早退原样保留）= **行为恒等**
2. **placeTrees 守卫**：seaLevel 检查后
   `if (caveCarvesCell(wx, hgt, wz, hgt)) continue;`（表面格被洞吃 = 跳过
   整棵树；支撑下探不做——1 格厚顶非浮空，设计边界）
3. **跨 chunk 约束**：守卫是 (wx,wy,wz,height) + 生成器不可变状态
   (m_seed, m_params, m_lakes) 的纯函数，不读任何 chunk 数据——margin 机制下
   同一 trunk 列被多个邻 chunk 独立考虑，各 chunk 以相同实参调同一函数 →
   判定必然一致 → 整树同生同灭，无半截冠，worker 并行安全

**零 format / 零 config / 零 content**（纯核心修复）。世界生成演进 = 旧存档
新地形先例（浮空树消失 = 预期行为变化）。

## 变更文件

- `src/world/world_generator.hpp`（caveCarvesCell 公开 API + 生成顺序注释）
- `src/world/world_generator.cpp`（谓词提取 + carveCaves 改调 + placeTrees 守卫）
- `tests/test_world_generator.cpp`（+4 测 + 扫描/生成盒辅助）

## 测试（1494/1494，+4 新）

1. `CaveCarvesCellMatchesOldInlinePredicate` — 提取恒等 A/B：chunk 全部 4096
   格新谓词 == 旧内联公式（n2 短路同款）+ spawn 区整带恒 false + cavesEnabled
   分支恒 false
2. `CarveCavesWritesExactlyCaveCarvesCell` — carveCaves 输出 == 谓词（带内
   IFF）；与既有 `CaveMatchesPurePredicate`（chunk == 旧公式）传递合成 =
   新 carveCaves 逐位等于旧 carveCaves
3. `TreesOnIntactSurfaceStillGrow` — 守卫反例：表面完好树列照种（trunk 底 =
   hgt+1 连续、长 ∈ [4,7]）+ 跨 chunk 完好树 seam 格 = leaves（邻 chunk 补齐）
4. `TreesSkipCaveCarvedSurfaceColumns` — 守卫正例：表面被挖树列同 seed
   caves-off/on A/B【off 世界 trunk 长 4..7 在 / on 世界该列 wood=0 + 树带
   leaves=0】+ 跨 chunk 正例：seam 格 off=leaves / on=air、on 整盒 wood=0
   （8 邻树列均未被守卫 = 盒内消失的只有这棵树）

## shipped 世界实证（throwaway 扫描，真实资产 + seed 123456789 + cs 0.05）

树生长侧 320×320 盒 = 346 树列中 **14 列（4.05%）表面格被挖 = 修复前的浮空
树**；首列 (340,16) h=48 A/B：caves-off trunk 长 5 / caves-on trunk 长 0 =
守卫生效。守卫判定纯函数 = 跨 chunk/跨 run 字节可复现。

## 门 101 E2E

- **S1 基线 PASS（10-25 18:24，gate 内）**：fresh 60f ×2 RAW 字节一致 +
  **vs base18 锚字节一致 = 第 101 次结构性免疫，无需重锚**（spawn clear 区
  洞穴永不挖 + 守卫只跳"表面被挖"列 = 演示区构造免疫兑现；fixer 交付时
  预跑同款 PASS 与门内复跑双证）
- **S2 浮空树判别 PASS（补跑收 10-25 18:38，gate101_retry.zsh att 8）**：
  门内首轮黑相 8 次未得窗（PITS 253）→ 高频环（8 s/探，frozen binary
  tm_v0180_bin 免与后续 fixer 构建互撞）att 8 成。场景 = 玩家
  (334.5,45,16.5) yaw −1.274 pitch 0.69 框住 (340,12)+(340,16) 两棵浮空
  树【g101_heights 探针预验：两列 h=48 guard=1 lat=1，站位 h=44 guard=0
  非格；数学预验两树均在视场内】；old v0.17.0 ×2 RAW + new ×2 RAW + PITS
  254 场景加载 pos 读回断言。**A/B diff = 182,469 world px**（bbox
  x[0,1329] y[78,690]，质心 (801,369) = 两树投影区）；颜色分析定案 =
  **OLD 侧 diff px 以叶绿+干褐为主**（leaf-green 49,527 + 深绿/褐变体
  ~100k = 树体），**NEW 侧 = sky-blue 129,903（71%）+ 被树遮挡后露出的
  背景树像素**（冠层后移景）；HUD 心带 0 px、边缘 209 px（AA）=
  浮空树移除的像素级判别
- **S3 机制回归 PASS（gate 内 18:1x）**：1494/1494（4 新测 =
  caves-off/on 同 seed A/B + 跨 chunk seam 守卫正反例 + 谓词恒等×2）
- S6：无（零 parse 面——纯核心，无新字段/内容）
- PITS 254（场景 cp 到不存在的 save 目录 = 旧侧静默 fresh 世界 A/B 空对，
  首轮踩坑已修：旧侧运行目录预建 + 读回断言）

## 性能（插桩 A/B v0.17.0，发布前必做，10-25 18:2x）

- **fall_armor 900f 三 300f 窗**（new = GEN-5 / old = v0.17.0，同场景
  同 config；timing-clean 接受：NEW 侧 try 1 截图黑但 swap med 干净）：
  **fluidCells 123879/167781/236573 三窗双侧逐位一致 = 同一世界**（GEN-5
  不改世界生成参数，最强有效性信号）：
  - upd med 50.3/49.3/48.7（old 46.2/50.0/46.8 同档）
  - render med 0.323/0.480/0.625（old 0.316/0.474/0.628 同档）
  - swap med 0.371/0.473/0.473（old 0.446/0.470/0.478 同档，无 stall）
  - flush med 38.7/34.7/32.6（old 34.3/35.6/30.0 同档）
  - fluid 1320/2543/3792 ms（old 1237/2453/3783 同档；每格
    10.6/15.2/16.0 ns vs old 10.0/14.6/16.0 = 同档）
  - player 2.60/2.56/2.64（old 2.41/2.46/2.65）/ ents 0.35/0.34/0.31
    （old 0.30/0.31/0.30）/ stream 2082/2162/2432 ms
    （old 1799/2137/2460，同档）
- **结论：GEN-5 代码成本 ≈ 0**（守卫 = 每过 roll 树候选列 1 次纯谓词，
  世界逐位不变 = 同负载同档）

## 交付中上报（超出本 phase 范围，未修，待定夺）

**placeTrees/placeDecorations wz 循环上界 bug（Phase 4 ae19893 起潜伏）**：
内层 `for (wz = wz0; wz < limit; ...)` 的 `limit` 是 X 基（`ox + kChunkSize
[+ margin]`）——chunk 的 oz ≥ ox+19（树）/ ox+16（装饰）时 wz 循环恒空或不
及覆盖本 chunk 的 z 列 → **cz ≥ cx+1 半区（约世界一半）整块不种树/不放
装饰**（trunk 单列只由本列 chunk 写入 = 永久缺失）。100 道门未发现 = 基线
视图朝 -Z（该侧 chunk 恒在"种树侧"）。修复 = 一行（wz 上界改 `oz +
kChunkSize [+ margin]`），但使大量 chunk 新增树/装饰 = 世界生成演进，需
独立 phase + 门 + 重锚评估。**10-25 用户定案：立项 GEN-6（矩阵第
32 项），执行序 = SURV-5 之后 = v0.20.0（release ③ 计划不变）**。
