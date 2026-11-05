# Phase V3-6 — 世界边缘雾（v3.10.0，门 200 全 PASS，base19 重锚）

> 矩阵项 V3-6「世界边缘雾/遮挡」= 消费「世界边缘即虚空（无雾）」
> 记档（KNOWN_ISSUES Phase 3/14 时代）。**编排者实证定案**
> （/tmp/opencode/v310_recon.md 备忘 + 门内 5 组实证跑）：
> - 无输入跑 worker = 启动 16 列（4×4 演示区，cheb 半径 2）后零
>   扩展（PIPE-1 结构免疫纪律："无输入 worker 活动有界于启动 16
>   mesh job"——60f/600f/2000f "Meshed column" 日志均 16 列实证）
> - → base18 锚图的"浮空截面" = **演示区边界 32-45 块**（远在雾
>   start 750 内 = 完全可见：悬空 chunk 截面 + 孤柱树）
> - fog off ≡ fog on 逐字节（可见世界全 <750 块）→ 现有 PRE-2 雾
>   按**目标盘**（min(kFar 1000, 64×16 = 1024) = 1000）计算，对
>   **未填充盘缘无效**
> - 稳态行走盘填满 64 chunks（拉式流送 2 列/帧 >> 步行速度）→ 那时
>   现有雾正确
> - 修复 = **候选 B 标量化：fog end = min(静态 end, 实际填充盘
>   chebyshev 半径 ×16)**

## 实现（纯代码 2 文件对 + render 雾块，~90 行）

- `src/world/chunk_streaming.{hpp,cpp}`：**`filledChebyshevRadius
  (loaded, player)` 纯函数** = 已加载列相对玩家列的 max Chebyshev
  距离（空集 0；单次线性扫描零分配）。**语义 = 填充 rim**（最远已
  载列，非缺失 rim）：流送 nearest-first 投递 + FIFO 集成使填充
  下闭 → 填充中读值 = 实际地形 rim（保守偏小 = 雾更早吞）；+1 卸载
  滞回带大位移后短暂保留 64-65 旧 rim 列 → 读值只可能膨胀过无操作
  阈值（≥63），clamp 被延迟而非过触发；无输入演示区 = 恒 2（单测
  4 个可达玩家列全钉）。
- `src/renderer/fog.{hpp,cpp}`：**`fillClampedEnd(staticEnd,
  filledBlocks)`**（min，std::min 首参位级恒等）+ **`fogFilledRange
  (streamBlocks, filledBlocks, farPlane)`**（end = min(far, stream,
  filled)；start = 0.75 × clamp 后 end——ramp 总止于填充 rim）。
  **满盘（filledBlocks ≥ 静态 end）= 改前逐位恒等**（稳态红线，
  test_fog V36 族钉含 65 滞回变体）。
- `src/core/game_render.cpp` 雾块：OW = fogFilledRange(rEff,
  filled, kFar)；**下界同款 clamp**（{kNetherFogStart 10,
  fillClampedEnd(96, filled)}——首传 3×3 catch-up 半径 1 = end 16
  同款隐藏；start 常量 10 不变）；列 set→vector 快照从列 pass 提升
  到雾块（单次 set 走查共享，雾扫描 = 连续 vector 线性读，零新树
  走查——~0.1-0.5 ms/帧稳态成本归零）；玩家列 = 流送同款 floorDiv；
  degenerate guard（end ≤ start → off，含空填充）+ render.fog 门控
  不变。
- **零配置新键 / 零 format（30 不变）/ 零新 worker 活动**（无输入
  日志 pre/post 事件序列逐行恒等实证）。

## 门 200（**base19 重锚** = 首重锚；旧 base18 224468 B 留档）

- **重锚论证三件套**（fixer 产 + 编排者独立复验）：
  - 无输入 60f ×2 RAW 恒等 → **base19.png = 225272 B**
  - **base18 → base19 diff 审计**：20940 px，bbox x[12,678]
    y[206,386]（盘缘带横穿视线的中远树线 + 左断崖边 + 右碎片 +
    24-32 块雾带内中距地形）——y<200 上天空 **0 diff** / y>400 前景
    地面/HUD/热栏 **0 diff**（编排者 g200_s1 独立断言前景零 diff）/
    20940/20940 在 base18 均为非天空色 = 可见几何 / **逐通道单调趋
    天空 0 违例（62820 通道）** / 单雾因子 f∈[0,1] 逐 px：91.7%
    精确 1 LSB、100% 在 2 LSB 内（残差 = GPU 8-bit 舍入签名 max
    spread 0.077）/ 全雾 f=1 1056 px = 远景树碎片（base18 处绿叶
    色）——留证 g200_diffmask/diffmag/fog_factor_map/diff_overlay/
    strip 五图（三联目检 = "浮空岛淡出"：中远树线雾化、近景锐利）
  - **存档面零变化**：v3.10.0 无输入 60f 终档 savenorm vs v3.9.0
    参照树（tm_v400_base，独立确认 ≡ base18 先验）同跑终档 =
    **EXACT**（雾 = 纯渲染）
- **S1** 无输入 60f ×2 ≡ base19 逐字节 + savenorm EXACT（对旧侧）+
  白名单 4 键 + banner v3.10.0。
- **S2 fog-off 控制臂**（render.fog=false）：60f ×2 RAW 恒等 + **≡
  base18 逐字节**（224468 B）——**三方闭环**：v3.10.0+fog on =
  base19 / +fog off = base18（改前位级恒等 = 唯一像素面 = 雾范围
  计算实证）/ 存档 EXACT。
- **S3** 全套 2734/2734（独立复跑）。
- **S6** 无（零新解析面——纯代码 + 既有 render.fog 门控）。
- **性能 A/B（用户硬要求）**：旧 = v3.9.0（tm_v400_perf = 7aa8f84 +
  g139，banner v3.9.0 实证），新 = v3.10.0（tm_v400_perf_new + g139，
  banner v3.10.0 实证）：主对 900f 无输入同 seed——PERF-WIN 三窗
  同档（upd med 0.590/0.426/0.585 vs 0.600/0.613/0.580——600f 窗
  v390 侧异常低值 = 窗口间机器噪声带，p95 0.906 vs 0.951 同档；
  无输入对 = 16 列填充盘 → 线性扫描零成本实证）+ render 同档
  （0.530 vs 0.565 均值）+ swap vsync 同档 + fluid·fires·spread·
  ents·stream 同档 + **changesApplied 9/1/3 逐位恒等** + **终档
  savenorm EXACT**（纯渲染零 format 完全恒等）。

## 单测族（2724 → 2734，+10）

FogFilledTest 6（满盘 64 → 1000 位级恒等含 **65 滞回变体** / 2 → 32 /
0 → degenerate off / clamp 只降不升 / 0.75 联动 / nether 16·32·96 钉）
+ FilledChebyshevRadius 4（演示区 4 可达玩家列全 = 2 + 边列 = 3 /
空 = 0 独列 = 0 3×3 = 1 / 满盘 64 = 64 带 = 65 小盘 8 = 8 / 部分
填充 5·4·去轴点 = 4）+ F3 版本钉 v3.10.0。

## 偏离/记档

1. savenorm 旧锚 = v3.9.0 参照树自产存档（任务点名 s199 文件门脚本
   自删不存在——等强形式：直接 pre-change 对照 + 先验复现 base18）。
2. 语义取"max 已加载 Chebyshev"（填充 rim）非"最小缺失环"：后者无
   输入读 3 ≠ 设计钉 2；前者与两钉（2/64）唯一自洽（保守性论证见上）。
3. 列 set→vector 快照提升到雾块前（零新树走查，稳态成本归零；列
   pass 零行为变化 = 同一份拷贝）。
4. **base19 = 新锚**（225272 B）：无输入视图"地形雾化到盘缘"（观感
   = 浮空岛淡出，近景锐利不变）；**稳态行走视图 = 改前位级恒等**
   （满盘 clamp 无操作——单测钉 + 论证）；后续门 S1 口径 = base19。
5. 下界首传 3×3 catch-up（半径 1）同款隐藏（end 16——96 静态 end 的
   2/3 收缩，下界场景截图按门重基线非锚面）。

## 变更文件（显式路径，9）

src/world/chunk_streaming.{hpp,cpp}（filledChebyshevRadius 纯函数）/
src/renderer/fog.{hpp,cpp}（fillClampedEnd + fogFilledRange 纯函数）/
src/core/game_render.cpp（雾块填充感知 + 列快照共享 + 下界同款
clamp）/ CMakeLists.txt（3.10.0）/ tests/{test_fog +6,
test_chunk_streaming +4, test_debug_overlay F3 钉}。

零改动红线核证：存档零 format（savenorm EXACT 实证）/ config 零改动
（零新键）/ 流送零新活动（无输入日志逐行恒等实证）/ 稳态渲染位级
恒等（满盘 clamp 无操作单测 + fog-off ≡ base18 三方闭环）。
