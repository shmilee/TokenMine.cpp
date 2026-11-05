# Phase 实机报障 — mob 阴影缩尺（v3.12.0，门 202 全 PASS）

> 用户 11-06 实机报障：「mob 下面的阴影方块比 mob 大；当阴影的一个
> 角还在高处方块上时，mob 落不到下一层——看起来是根据阴影判断而
> 不是根据 mob」。用户定案处置 = **缩小阴影视觉**（物理不动）。

## 核实结论（编排者定界，一次性探针后撤）

- **物理 = AABB 质心规则，阴影零参与**（代码核证：`blobShadow*`
  仅 `Game::drawMobShadows` 渲染路径消费；`moveAxis` Y-resolve 扫
  真实 aabbMin/aabbMax）。一次性探针单测（PITS 383 定界纪律，用后
  撤除）：zombie（宽 0.6）chase 走下悬崖，**失接地 = 身体中心恰好
  越过边缘 0.3000**（lastGroundedX=0.3000 / fallX=0.3133 = 身体半
  宽）——**Minecraft 同款**（MC 亦允许半身悬空，质心过边即落）。
- 用户所见 = 落体瞬间视觉状态：阴影 1.5×（半宽 0.45）> 身体半宽
  0.3 → 质心过边失接地时阴影远角还压在高处方块 0.15 = 看似阴影在
  托着它。
- 用户定案：物理保持 MC 语义；**阴影 scale 1.5× → 1.0×**（= 身体
  同宽足迹，消除压边误导观感；玩家第三人称阴影同共享常量一并收
  缩 = 一致）。

## 实现（1 常量 + 3 测试重钉 + 版本三件套）

- `src/entity/blob_shadow.hpp`：`kMobShadowSideScale 1.5f → 1.0f`
  （注释更新：压边观感报告 + 物理 = AABB 探针定案）
- `tests/test_blob_shadow.cpp`：SideScalesWithWidth 重钉
  （0.6/1.0/0.5 = 1.0×）+ ConstantsPinned（1.0f）
- `CMakeLists.txt` 3.11.0→3.12.0 + F3 钉 v3.12.0
- **2777/2777**（数量不变，重钉 3 例）

## 门 202（结构免疫第 82 次，对 base19；锚免重锚）

**结构免疫论证**：锚（无输入 60f，quiet 配置 mobs off + 第一人称
无玩家阴影）= 阴影 quad 从不渲染 → 位级不变。
- **S1** 无输入 60f ×2 ≡ base19（225272 B）逐字节 + 白名单 4 键 +
  banner v3.12.0。
- **S2 阴影视觉臂**（mobs-on OW 场景 g198_mobs.json，600f 无输入，
  config seed 123456789）：
  - (a) 新二进制 ×2 截图 RAW 恒等（mob 生成/游荡/新阴影确定性；
    场景实证 3 只可见 mob——crop 目检）
  - (b) **前后对比**（v3.11.0 参照树 tm_v600_base vs v3.12.0 仓库
    同场景）：diff = **2896 px / 3 簇地面带**（x[0,528] y[343,428]
    = 3 只 mob 的阴影区）+ **单调变亮审计 0/2896 违例**（阴影只缩
    小，无一处变暗）+ 天空带（上 1/4）零 diff + 紧致度 6.4%（quad
    带非散点）+ **crop 3× 目检**（同场景同羊同位，阴影由 1.5× 宽
    方缩为贴身体方——其余像素零变化）。
- **S6** 无（零新解析面）。
- **性能 A/B（用户硬要求）**：旧 = v3.11.0（tm_v600_perf = 4531c42 +
  g139 banner 实证），新 = v3.12.0（tm_v600_perf_new banner v3.12.0
  实证）：主对 900f 无输入同 seed——PERF-WIN 三窗同档（upd med
  0.442/0.527/0.615 vs 0.517/0.468/0.525 = 窗口间机器噪声带，均值
  0.528 vs 0.503；render 同档）+ swap vsync 同档 + fluid·fires·
  spread·ents·stream 同档 + **changesApplied 9/1/3 逐位恒等** +
  **终档 savenorm EXACT**（渲染常量零 format 完全恒等）。阴影 scale
  = 顶点构建同式更小系数，零每帧成本。

## PITS

- **PITS 392**（门日志 grep 假阴性）：`mob spawn f...` 等事件行 =
  MC_LOG_DEBUG——info 级门跑 grep 计数恒 0 ≠ 事件未发生；事件计数
  断言须 `--log-level debug` 跑，或截图 crop 目检互补（本门 (a)
  首跑 "0 spawns" 假阴性，crop 实证 3 只羊在场）。

## 变更文件（显式路径，4）

src/entity/blob_shadow.hpp（1 常量）/ tests/test_blob_shadow.cpp
（重钉 3 例）/ tests/test_debug_overlay.cpp（F3 v3.12.0）/
CMakeLists.txt（3.12.0）。

零改动红线核证：物理零触碰（moveAxis/onGround 族零 diff）/ 存档零
format / config 零改动 / 锚免重锚（S1 实证）。
