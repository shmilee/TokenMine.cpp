# Phase 37 细节存档

> 用户确认方向（原话）："首先按建议7清理，然后按顺序：P37是建议1；
> P38是建议2。"（= P36 报告候选 1：并行化存档 diff；建议 7 = 清理参考
> worktree，已于本阶段开工前完成：`git worktree remove p36ref` +
> `git worktree prune` 抹掉 p18ref 残留条目）

## 阶段报告

- **`[2026-10-10, Phase 37]` 并行化存档 diff（P33 诊断的退出慢）** —
  用户日志中"退出 13.6 s"（P33 诊断）= 写存档时 `diffWorld` 对每个已加载
  chunk 用 scratch chunk 重跑纯函数 `generateChunk` 逐格比对，单线程
  ~0.14 ms/chunk；view_distance 80 的世界 ~100k+ chunk → 退出尖峰 ~14 s。

  **设计**：
  - `diffWorld` 签名 +`int workers = 1`（默认串行，既有调用/测试零变化）。
    `collectSave`（退出写档 + F5 同一路径）恒传 `kSaveDiffWorkers`。
  - 阈值判定在函数内：`workers > 1 && chunk 数 > kSaveDiffParallelMinChunks`
    （256）才走并行——小世界（典型演示区 ~2.3k chunk 以下、5 帧源存档
    几十 chunk）保持单线程，线程生成零开销。
  - 并行路径 = **临时 4-worker `ThreadPool`**（函数内局部，析构即 join，
    不碰流式池）：每 chunk 一个 job 调抽出的 `diffChunk` 写自己的
    slice（`blockParts[i]`/`fluidParts[i]`），flush 后按 chunk 序合并。
  - **正确性核心**：输出末尾按位置排序是**全序**（一格至多一个编辑，
    位置不重复由校验保证）→ 并行结果与串行逐字节相同，与调度顺序无关。
  - **worker 安全边界**：job 只做 `findChunk`（只读）+ `generateChunk`
    （纯 const 函数，流式池本就在 4 worker 上并发调用，P14 既有模式）；
    主线程阻塞在 `pool.flush()` → 窗口内无 World 写者。F5 路径下流式
    job 可能在途——它们零 World 接触（scratch chunk 模式），并发读安全。
  - 存档格式零变化（只是换了执行方式）；`kSaveDiffWorkers = 4` /
    `kSaveDiffParallelMinChunks = 256` 定义在 save.hpp（测试可引用）。

  **单测 +2（700/700）**：
  - `ParallelDiffMatchesSerialDiff`：16×17 = 272 chunk（>256 走并行）
    世界，3 个编辑散布在不同 chunk（挖/放/流体变薄）→ workers=4 输出
    与 workers=1 逐字段相同（位置 + 名称 + 等级）。
  - `SmallWorldIgnoresTheWorkerCount`：1-chunk 世界 workers=4 → 回落
    串行，结果正确（单编辑、无流体）。

  **E2E（两批全绿、0 真实失败；12 截图全 1280×720；批脚本
  `/tmp/opencode/p37_e2e.zsh` 复用 P36 全部 helper 与 WM 稳定化设计）**：
  - **S1 base18 门（第 12 次结构性免疫）**：无输入 60f ×2 字节一致，
    且 == `/tmp/opencode/base18.png`（P36 重锚的参考图，本次直接复用）
    且 == P36 代码参考构建（`git worktree p37ref @8e8c6de` 独立 build，
    同 SDL 0.4.18）逐字节。免疫论证：无 `--save` 时 `collectSave` /
    `diffWorld` 永不执行；F5 需要输入。
  - **S2 存档 A/B（核心证据）**：5f 源存档 + 手工 3 编辑（挖 (0,53,0)
    →air、放石 (0,54,0)、流体 (2,80,2)=3——均在出生列、保证与生成态
    不同）→ 两个构建各加载同一源存档、300f（vd 8 → 2312 chunk 加载
    > 256，P37 走并行、P36 串行）退出重存：**P36 vs P37 存档逐字节
    一致**、P37 ×2 确定性逐字节一致、截图 A/B 与 P37 对逐字节一致
    （渲染零变化的直接证明）。
  - **发现（PITS 新坑位）**：手工流体编辑在空气里**下落 + 水平扩散**
    （流体模拟动态、逐帧确定）——300f 后水落到地面扩散成 16 格，
    (2,80,2) 为空。"恰好 3 个编辑"断言首跑全 MISMATCH（2 方块编辑
    精确、fluids=16）。修正 = 编辑断言改**沉降终态精确钉值**（2 方块
    + 16 流体格逐格列出；三个构建输出完全一致 = 流体模拟确定性跨
    构建复现）。
  - **S3 timing 对（vd 25、1200f、~9600 chunk 加载）**：P36 墙钟
    20.18 s vs P37 19.20 s → **Δ0.98 s**（退出 diff 串行 ~1.3 s →
    并行 ~0.35 s，符合 0.14 ms/chunk 诊断 ÷4 worker）+ 无编辑存档对
    字节一致。
  - **S4 F5（帧中并行 diff、流式 job 在途）**：纯方块源存档（避免
    流体演化使 mid/exit 不可比）→ 锁定 + F5 → F5 存档与退出存档
    blocks/fluids/pos/yaw/pitch 全同（time 字段推进豁免，日志记录两端
    值）。**初跑教训（PITS 新坑位）**：F5 落在末帧附近（launch()
    稳定化在进程启动后耗时 0.3–1.2 s 吃掉输入预算）→ mid/exit time
    逐位相同 = 弱证据；加预算重跑后 F5 落中段帧（time 0.2554 vs 退出
    0.2628），证据成立。

  **变更文件**：`src/core/save.hpp`（+`kSaveDiffWorkers` /
  `kSaveDiffParallelMinChunks`、`diffWorld` +`int workers = 1` + 文档）、
  `src/core/save.cpp`（+include thread_pool.hpp、抽出 `diffChunk`、
  `diffWorld` 双路径）、`src/core/game.cpp`（`collectSave` 传
  `kSaveDiffWorkers` + 注释）、`tests/test_save.cpp`（+2 测试）、
  文档（ROADMAP P37 行 + 索引、KNOWN_ISSUES diff 条目 → 已修复划线 +
  索引行 37、MECHANICS 存档节并行句、PITS +2、AGENTS 现状、本文件）。

  **E2E 证据文件**（/tmp/opencode/）：`p37_e2e.log`（两批）、
  `p37_base_a/b.png` + `p37_refbase.png`、`p37_ab_p36/p37a/p37b.png`、
  `p37ab_ab_p36/p37a/p37b.json`（A/B 存档）、`p37_src_edited.json` /
  `p37_src_blocks.json`（手工源存档）、`t36s_tm_p36.json` /
  `t37s_tm_p37.json`（timing 存档）、`p37_f5_mid/exit.json`（F5 存档）、
  参考构建 worktree `p37ref`（@8e8c6de，独立 build）。

## 会话存档

- 开工前先执行了 P36 报告的候选 7（清理）：`git worktree remove
  /tmp/opencode/p36ref` + `git worktree prune`（p18ref 目录早已被
  /tmp 清理抹除，只剩 prunable 条目）。
- 参考构建惯例延续 P36：A/B 门 = 独立 worktree build（同 SDL 版本）
  替代旧参考资产；P37 的 A/B 对象 = P36 代码（8e8c6de）。
- 批脚本首跑 S2 假失败（check_edits 预期写错——流体动态沉降），非代码
  bug；修正断言后整批重跑全绿。E2E 批内 0 重试（WM 稳定化设计复用
  P36 后本批零竞态）。
- P38（用户已排定 = 候选 2：skeleton 后续三项）在本阶段报告后开工。
