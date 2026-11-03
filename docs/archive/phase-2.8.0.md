# Phase V2-8 — sky rescan clobber 修复（v2.8.0，门 179 全 PASS）

> 2.x 第八项（LGT 行）：修复 LGT-1 记档的潜伏 bug（KNOWN_ISSUES「光照」
> 节，`[Phase 83]` 信标期无关——99.10 光照 job 双类化时记档）。
> 设计定案 = /tmp/opencode/v28_design.md。修复形态 = 记档两选一中的
> **sky BFS 加 shell seed**（union 单遍 = LGT-1 已砍除形态，不复活）。
> **零 format（25 不动）/ 零 config 新键 / 零新数据 / 零纹理**。

## 修复内容

### Bug（LGT-1 记档，LGT-2 交互扩展）
天花板板骑跨光照 footprint 边界（一侧开敞列、另一侧 opaque 天花板
延伸）时：后一 region 的 sky rescan 把 footprint 内边界格 sky 重写为
纯垂直值（opaque 下 = 0；LGT-2 衰减冠下 = 部分值 < 真值），而 sky BFS
种子只取 **footprint 内** lit 格——开敞源列在 footprint 外 → 无种子
→ 水平光流无法恢复 = **stale-dark / stale-partial 暗带**（天花板下，
1-14 格宽）。block 光不受影响（壳注入自愈）；确定性不受影响（错得
确定）。触发几何 = 流送 drain（列 job 升序、2/帧，每列 newTerrain
updateRegion）：最后触碰 clobber 区且 footprint 排除源的 job 的覆写
无恢复。单一编辑区不可能触发（R=16 footprint > 15 光程 → 源恒在
footprint 内）。

### 实现（src/world/lighting.cpp `updateRegion` 天空种子段，唯一改动）
- 种子盒从 footprint（sx0..sx1 × sz0..sz1）扩为 **+1 块 shell 环**
  （hx0=sx0−1, hx1=sx1+1, hz0=sz0−1, hz1=sz1+1）：chunk 循环界
  floorDiv(h*, kChunkSize) + 单元格检查换 hx/hz。**rescan 段零改动**
  （仍只写 footprint——shell 列是被保护的正确值）；**BFS 零改动**
  （inRegion 写界 = footprint 不变，shell 种子格出队只读、邻写被
  inRegion 拒）；**no-op skip / propagateAll / 块光段零改动**。
- **充分性**：被 clobber 格的真值流路径要么在 footprint 内（rescan
  精确垂直起源 = 全 lit footprint 种子已含），要么穿越 footprint
  边界——路径上第一个界外格 = shell 格（持收敛值 ≥1，本 region 从未
  写它）→ shell 种子恢复边界条件，严格增 BFS 向内推进。
- **无 clobber 时位级不变**：旧不动点上每格 ≥ 壳邻格−1 → 严格增不
  触发 → 结果与 footprint-only 种子位级相同。
- **独立性判据零改动（算术实证）**：标准列盒 [16c,16c+15] 的
  footprint = [16c−16, 16c+31]（列 c±1），shell = [16c−17, 16c+32]，
  其中 16c−17 = 16(c−2)+15、16c+32 = 16(c+2)+0 → shell 读列 = c±2
  **恰为** 99.10 文档化的「1-chunk 边界读环」（块光同款读环）→
  lightRegionsIndependent（gap≥4：写 A=cA±1 ∩ 读 B=cB±2 不相交）与
  confluence 证明（phase-lgt1-flip.md）零改动。`world/light_job.hpp`
  零变更（git diff 实证）。
- **历史教训保留**：种子 = 全 lit footprint（pre-99.9 减种子回归根因
  注释保留）——本修复只**加** shell 环，从不缩 footprint 种子集。

## 变更文件
- `src/world/lighting.cpp`：updateRegion 天空种子盒 +1 块 shell 环
  （chunk 界 + 单元格检查）+ 机制/充分性/读环论证注释
- `tests/test_lighting.cpp`：+2 回归（TDD：修复前树上先证 FAIL）
  - `SkyRescanShellSeedClobberRecovery`（stale-dark：stone 半板
    x∈[8..31] y=12，box {24,0,8}-{24,12,8} footprint x8..40 排除源
    x<8 → pre-fix 2464 diff 格 x8..21 全 0）
  - `SkyRescanShellSeedLgt2PartialRecovery`（stale-partial：leaves
    两层冠 x∈[8..20] + stone 沿 x∈[21..31] → pre-fix 544 diff 格
    恰 = x8..10 冠下 11<14/13/12 + 叶行 x8,y12（石沿 x21..30 由
    footprint 内冠种子恢复 = 设计预测吻合））
- `CMakeLists.txt`：2.7.0→2.8.0（单点）
- `tests/test_debug_overlay.cpp`：F3 版本钉 v2.8.0（PITS 355）

## 实现侧自证（编排者独立核证）
- 构建 `-Wall -Wextra -Wpedantic -Werror` 0 警告
- **2171/2171**（2169 既有 + 2 回归；编排者独立复跑）
- TDD 证据链（fixer 交付）：pre-fix 双测试 FAIL（diff 格清点与设计
  预测精确吻合）→ 修复后 PASS
- 判据零改动算术实证（上文 shell 读 ⊆ 既有读环）
- 冒烟 banner v2.8.0 + format 25
- **base18 恒等**：60f 无输入 = 新锚 372516 B（1280×720 无平铺态）
  逐字节 IDENTICAL（结构免疫预核——启动窗内 clobber 几何未触发）

## 门 179（编排者执行；old 侧 = v2.7.0 = 9869617 树）— 全 PASS

### S1 = 第 179 次结构性免疫 + 锚重导连续
- fresh 60f 四方（new v2.8.0 n1/n2 vs old v2.7.0 o1/o2）全 179116 B
  **RAW 全字节一致** + n1 ≡ base18 锚 + 跨版 savenorm **EXACT**
  （零 format）。锚 = /tmp 丢失后从**旧版净树（v2.7.0）**重导
  （×2 恒等实证）：**179116 B / 679×744 WM tile 态**——与原锚
  字节数吻合 = 连续性证据（新 X 会话 1366×768 屏，Awesome tile
  布局强制窗几何，PITS 375：会话首窗 floating 例外须 dummy 暖机
  吞掉；xdotool windowsize 对 tile 态无效）

### S2 = clobber 判别（CPU 探针权威 + E2E 像素呈现）
- CPU 探针（probe179_clobber，真实资产，半板几何）：
  - 旧库（v2.7.0）：converged y11 = 15×4, 14, 13, …, 1, 0 →
    updateRegion 后 **全 0（x8..21 梯度被毁不恢复 = bug 复现）**
  - 新库（v2.8.0）：updateRegion 后 **14, 13, …, 2, 1 完整保持
    = shell 种子恢复**
- E2E slab 场景（无输入流送）：板 x25..63 z0..4 y55（开阔谷地上空，
  地面 y36/37，probe179_terrain 实证无树）；demo 4×4 启动即载全部
  板列；light job set 序升序 drain——最后触碰西区 clobber 区
  x32..38 的 job = 列 (3,1)（footprint x32..79，西源 x≤31 在境外）：
  - 旧树：西区暗带 x32..38（stale-dark）
  - 新树：梯度恢复（shell x31 种子）
  - 判别 = 新旧截图 diff bbox @ 板下地台区 + 各树 ×2 恒等
  - 实测：旧 ×2 恒等（316069 B）/ 新 ×2 恒等（316417 B）；
    **新旧 diff bbox (239,541)-(339,699)**（核心 y559-599 ×
    x240-296，板下地台投影 @ 画面下中 = 预测位），diff 区外
    **零**杂散像素；极性 = 旧暗 → 新亮（地板亮度恢复，如
    (275,560) old (47,87,123) → new (53,100,142)；天空
    (120,181,242) 恒等）

### S3 套件
- 2171/2171（编排者独立复跑）

### S6 命名回归
- CPU 探针旧库（v2.7.0 libmc_core.a）= bug 命名复现：converged
  y11 = 15×4, 14, 13, …, 1, 0 → updateRegion 后 **15×4, 0×15**
  （梯度全毁）；新库（v2.8.0）同几何 = **逐值完整保持**（互斥）
- 2 个回归测试名称点名（SkyRescanShellSeedClobberRecovery /
  *Lgt2PartialRecovery），TDD pre-fix FAIL 在 v2.7.0 树实证

### 性能 A/B PASS 同世界同成本（用户硬要求，发布前）
- v2.7.0（tm_v280_perf = 9869617 + g139 27 标记）vs v2.8.0
  （tm_v280_perf_new = 终版工作树 + g139 双 27 标记，banner 双侧
  实证）：fall 900f 无输入 seed 123456789：
  - PERF-WIN 三窗全指标同档无系统偏移（upd med 0.63/0.60/0.58 vs
    0.62/0.38/0.38 ms；p95 窗间摆动同款；render med 0.55/0.53/0.45
    vs 0.54/0.38/0.41；swap 15.3/15.4/15.5 vs 15.3/15.9/15.8 =
    vsync 主导同值；fluid 0.045/0.041/0.048 vs 0.042/0.038/0.041、
    fires/ents/stream 全同档——shell 种子环成本不可测）
  - **PIPE3-S0 changesApplied 三窗 max 9/1/3 双侧逐位**
  - **终档 fluids 16/16 + blocks 36/36 集合恒等**（同世界最强形）
  - **slab 场景自成本**（clobber 场景 900f 双侧 = 修复的恢复 BFS
    实测点）：upd med 1.55/1.55/1.59 vs 1.93/1.62/1.71 ms；fluid
    0.41/0.44/0.40 vs 0.46/0.49/0.48 = **同档（新侧持平或更低）**

### 环境事件（重启后重锚）
- 机器重启（用户离机前）→ /tmp 全失：base18 锚 / 参照树 / s179 资产
  重建；新锚 372516 B（1280×720 无平铺，新屏 1366×768 窗不平铺——
  旧 679×744 半平铺态几何不可复现）按先例刷新采用，×2 确定性实证
- **XTEST 键盘恢复**（PITS 368 根治实证 = X 会话重启）：W 键 0.8s
  位移 z 0.5→−3.0（V2-9 起完整输入协议可用）

## 偏离（记档）
（无——修复 = 记档 bug 的定向消除；两 quirk 记录本身划解）
