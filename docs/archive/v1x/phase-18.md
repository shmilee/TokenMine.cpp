# Phase 18 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 18 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

- **范围与决策**：用户指定 P18=Greedy Meshing（KNOWN_ISSUES 优先级首位）。实现前发现与
  base3 字节门控**结构性冲突**：合并 quad 的 UV 插值与逐面片在浮点层不同（数学同一
  映射、不同舍入）+ 合并 quad 剪影边缘线亚像素偏移 → 无输入基线必然有少数像素变化
  （非 bug）。向用户说明后**批准重拍基线**（base18），回归检测目的不变、只是参考图
  换掉。
- **设计（最终实现）**：
  - 逐方向×逐切片：kCubeFaces 6 方向 × 17 平面（p=0..16，+normal 面在 p=ca+1、
    −normal 面在 p=ca）× 16×16 mask。mask cell=（tile、sub-mesh、面光），面光=面
    邻格光照（与 P17 逐面片同约定）；**光保守合并**：面光不同不合并 → 每个合并 quad
    = 同纹理同光面片的并集，光照零近似。
  - 每平面**双 slice**（+normal 来自平面下格、−normal 来自平面上格，独立合并）：
    相邻非 opaque 方块（叶片-叶片/玻璃-玻璃）之间的双面按设计保留（正确、略浪费）。
  - 发射：种子格 c0 角为锚，沿 kCubeFaces 的 edgeW（c1−c0）/edgeC（c3−c0）延伸
    w×h；mask 轴与 face 边轴的方向关系用点积（signA/signB）推导，α/β 区间
    （signA=+1：[0,w]；−1：[1−w,1]；β 同理）→ 角点顺序与 legacy (c0,c1,c2,c3) 同
    型，winding 恒沿法线。
  - **tiled UV 约定**：合并 quad（w·h>1）= `uv` 存 tile 索引（整数）、`uvLocal` 存
    tile 单位位置（c0 侧 v=1 端、c3 侧 v=1−h 可负）、`tiled=1`；`basic.frag`：
    `sampleUv = (v_uv + fract(v_uvLocal)) * u_tileSize`（`u_tileSize`=1/(cols,rows)，
    game 每帧设）。单面片 1×1 保持 legacy atlas UV + `tiled=0`（shader 直采，逐位
    精确）——**Option B**：回归面最小化。GLSL `fract` 对负数= x−floor(x) ∈[0,1)
    正确；插值的 tile 单位位置在整数处折叠=旧逐面片的纹素接缝，逐格纹理正确。
  - `MeshVertex` 13 float/52B（+`uvLocal` vec2、+`tiled` float）；VAO +location 5/6；
    sky/ui 着色器不声明 location 5/6=合法忽略。fluid 路径（emitFluid）零改动。
- **验证（关键，方法论可复用）**：
  - 529/529（新增 6：MergedQuadGeometryAndTiledUvs/SingleFaceKeepsLegacyUv/
    FlatSlabTopMergesIntoOneQuad/DifferentFaceLightSplitsRun/DifferentTilesSplitRun/
    TransparentPairKeepsBothBetweenFaces；AdjacentBlocksCullSharedFace 40→24 顶点）。
  - **几何覆盖集对比**：临时程序把 git HEAD 旧 mesher 以别名类编入，逐 chunk 把两侧
    mesh 的每个 quad 展开为单位面片 cell 集合（17×17 列区域 2312 chunk）→ **1,514,610
    面片 cell 完全相等、0 mismatch**（几何 100% 无差）。
  - **隔离实验**（排查"天空变绿"差异时用的，有效）：(a) 旧 mesher 装进新二进制渲染 →
    与 base3 **逐字节一致**（证明新顶点格式/VAO/shader legacy 路径像素精确）；
    (b) 新 mesher 禁用合并扩展 → 3285 px 差（UV 约定漂移本身）；(c) Option A（全
    tiled）vs Option B（单面 legacy）→ **0 px 差**（本视图单面片漂移不翻转纹素）→
    选 B（永久安全，代价≈0）。
  - 最终差异解剖（vs base3，3593px=0.39%）：1214 big（远世界边界山脊剪影边缘的
    天空↔绿色翻转，合并 quad 边缘线亚像素偏移）、1842 mid（1 纹素重采样的几 px 同色
    补丁）、537 small（亚纹素）。无内部大面积变色。
  - **基线门控变更**：`/tmp/opencode/base18.png` 为新基线（×2 字节一致）；base3.png
    为历史件（P14–P17 参考）。E2E 全绿：base/inv/table ×2 字节一致 + PIL 像素判据
    （table 第一次 exit 3 输入丢失、重试后 OK——P17 既有行为）。
  - 性能：200 chunk（5×5 列+真实光照，seed 123456789）：总三角形 109,648→33,558
    （3.3×，不透明 3.5×/透明 2.3×）、顶点 219,296→67,116（3.3×）。全视野（半径 8）
    外推 ~87 万→~27 万三角形。
- **坑位（本阶段新踩）**：
  - 合并 quad 与逐面片**不像素恒等**（UV 浮点重插值 + 边缘线光栅化）——重拍基线吸收；
    想"字节精确的合并"只能做顶点去重（三角形不减），已放弃。
  - `fract(插值 tile 单位位置)` 看似会在整数处"折叠"，实为逐格平铺的正确机制
    （折叠处=旧面片接缝）；推导时用 α/β 参数化+逐角验证（含 signA=−1 的 −Z/−Y/+X
    方向），别只验 + 方向。
  - `emitMergedQuad` 里声明 `u0/v0` 与形参（mask 坐标 `int u0,v0`）重名 →
    `-Werror=shadow`，改 `legU0/legU1/legV0/legV1`。
  - gtest `EXPECT_EQ(v.uv, Vec2{0,1})` 花括号逗号被宏解析成多参数 → 加一层括号
    `(Vec2 {0.0f, 1.0f})`。
  - 临时对比程序：`git show HEAD:src/world/chunk_mesher.cpp` 抽出旧实现，sed 改类名
    + 手写别名头（成员镜像）；`#define ChunkMesher ChunkMesherOld` 再 include 头文件
    不行（`#pragma once` 防不住）；链接用 g++（gcc 缺 C++ 运行时）+ `-lm`。
  - 用户在本阶段期间手改了 `config/config.json`（view_distance 8→80、
    day_length 240→600，00:22）——**不要还原、不要提交**（E2E 门控用独立的
    /tmp/opencode/config_xep.json，不受影响）。
- **E2E 环境**：与 P17 相同（p17_e2e.zsh 驱动未改、xact、鼠标静置等待、UI unmap）；
  基线 gate 图换 base18.png。
