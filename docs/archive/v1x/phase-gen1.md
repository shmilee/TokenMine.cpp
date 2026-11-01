# Phase GEN-1 — 面条洞穴（v1.0 任务表 第 6/29 项）

> 2026-10-21（夜；用户在场确认全推荐参数：湖排除盒 / spawn_clear_columns=2 /
> surface_guard=0 洞口 / scale 0.09 + T 0.06 / 悬树接受）。来源 =
> `docs/BACKLOG.md` GEN 行第 1 项：「单噪声奶酪洞 → 两 3D 噪声相交隧道
> （世界条）」。

## 设计定案（ora-2）

- **算法**：两独立 3D value-noise 中位带相交 `|n1−0.5|<T ∧ |n2−0.5|<T`
  （MC spaghetti 同款管状隧道；`valueNoise3` 既有原语零新噪声代码；n2 仅
  n1 入带时求值 = 嵌套短路）。否决：单场双阈值（blob 非管）/ 2D 挤出
  （层状）/ curl 噪声（超最小版）。
- **湖交互 = 每湖 lava 带排除盒**（区域 x/z ±1 × [floorY−1, floorY+fill]，
  create() 预计算 ResolvedLake；placeLavaLakes 改读预存 floorY = 顺带消除
  per-chunk 区域重扫）——保 P88/P90 熔岩湖静态被围字节契约（"接受 MC 式
  泄漏"否决：泄漏毁契约 + 燃烧混沌）。
- **出生点 = spawn_clear_columns 2 结构排除**（原点 Chebyshev ≤2 列整列
  不挖，per-chunk 早退）→ 演示区恒 intact = 任意 seed 出生点安全 +
  **base18 门 84 结构性免疫（不重锚）**（否决"调参让 shipped seed 恰好
  无洞" = 脆弱）。
- **洞口 = surface_guard 0 语义扩展**（现 263-264 行表达式 guard 0 仍
  结构性排除表面格 → 显式改判 `topExclusive = guard>0 ? h−guard : h+1`）。
- **其余交互零代码**：矿 deep-only 替换（洞壁暴露自然成立）/ 树 placeIfAir
  （洞口悬树 = MC 同款，文档化）/ 装饰让位检查（"caves may have eaten it"
  直接生效）/ 海床洞一次性渗水（文档化）/ 洞内 24/7 敌对生成（光门 0，
  MC 一致）/ 旧存档新洞（P86/88/93 世界生成演进先例，零 format）。

## 标定（step-0 纯生成器探针，免游戏跑）

起点参数即四指标全达标 = **零调参轮定稿 (scale 0.09 / T 0.06 / guard 0 /
clear 2)**：挖除占比 5.38%（目标 2-8%）/ 洞口密度 94.7 个·64×64⁻¹
（目标 ≥3）/ run 宽 p90 = 6(水平)·5(垂直)（目标 2-8）/ clear 区 A/B diff 0
+ 5 湖盒内 A/B diff 0（盒外 4563 = 洞穴正常工作）。0.05 疏 / 0.07 密两档
对照数据在 fixer 报告。

## 性能

gen job **1.17 ms/列（中位，探针 3 pass）**、worst 1.48 < 2.4 预算线 →
不降级半分辨率；且低于现状参照 1.6（n2 仅 ~12% 入带格求值 + clear 早退 +
湖重扫消除）。流送 fps 预期零损失（light 16.7 ms 仍主导 flush）。

## 变更文件

- `src/world/worldgen_config.{hpp,cpp}`（+tubeThreshold/spawnClearColumns
  解析校验点名；删 caveThreshold；surface_guard 注释扩 0 语义）
- `src/world/world_generator.{hpp,cpp}`（carveCaves 重写：嵌套谓词 + clear
  区 per-chunk 早退 + 湖盒 per-cell 判 + guard 0 改判；ResolvedLake 预计算；
  kSaltCave2）
- `assets/data/worldgen.json`（caves 段定稿参数）
- `tests/test_world_generator.cpp`（14 新 + 1 迁移：CavesCarveBelowSurface
  采样区移 col 4+ = clear 区外）、`tests/test_worldgen_config.cpp`（解析）
- `docs/`（CONTENT_GUIDE caves 段 / MECHANICS 洞穴节 / KNOWN_ISSUES 四信息
  条 / ROADMAP / BACKLOG / AGENTS / 本存档）

## 测试

1240/1240（存量 1226 除 CavesCarveBelowSurface 迁移外零改动 + 14 新 + 1
迁移），`-Werror` 0 警告。关键：CaveMatchesPurePredicate（chunk 内容 vs
谓词逐位）/ LavaLakeExclusion（盒内零洞 + floorY 双算对照）/
SpaghettiTubeWidth（形态钉）/ SpawnClearZoneUnscathed /
CaveOpeningsReachSurface / CrossSeed 不同 + SameSeed 稳定 / OresUnaffected
回归 / CavesCanBeDisabled 零改动过。

## 构建与验证结果（E2E）

（批跑经用户要求压缩：S2b 进洞走路推迟，其余全过。截图经白帧重试守卫，
存档 = 权威。）

- **S1 base18 门 84 = 结构性免疫（不重锚）**：新二进制 fresh 60f ×2 与
  **gate 83 锚字节一致**（clear 区保护演示区 = 论证兑现）；A/B vs 前
  GEN-1 基线（46d0d72）= **1 px**（远地形剪影处旧奶酪洞/新面条洞的
  洞穴内壁面差 = clear 区外的预期世界生成差异，非缺陷）。红心 0 +
  skygate (120,181,242) 75,348 px 严格。
- **S2 洞口场景**（seed 123456789，3 连洞口 (142-144,36,6) depth=8，
  600f 无输入）：新 ×2 **RAW 字节一致**（静态）；old vs new diff
  **73,401 px**（洞穴布局不同 = 功能可见性实证，限定世界区）。
- **S3 跨 seed**（同相机位，seed 9 vs 123456789）：全帧 diff 921,600
  （两世界完全不同 ✓）；seed 9 ×2 存档 savenorm **EXACT**（各世界内
  确定性 ✓）。
- **S4a 深湖回归**（(40,40) 湖岸，P88 同款几何）：新 ×2 RAW 字节一致
  （湖静态被围 ✓）；old vs new **中心湖面掩码 diff = 0**（排除盒生效 =
  湖盆/rim 零变化），总 diff 19,066 px = 远处地形洞穴形态（预期）。
- **S4b 表面湖回归**（P90 场景，fresh 600f spread0）：新 ×2 RAW 字节
  一致；**rim 焦土 blocks 跨世界 IDENTICAL** + 湖盆盒流体恒等 + 新世界
  流体集 ⊆ 旧世界（旧多 88 格 = 旧奶酪洞世界的 −z 溢出臂）——表面湖
  「确定性但非静止」（P90 契约）两世界各自成立，跨世界溢出形态差 =
  世界生成演进预期行为（非排除盒泄漏：盆盒/rim 完好）。
- **S6 config init 失败**：scratch 资产 `spawn_clear_columns=33` →
  **rc=1 + 点名** `'caves.spawn_clear_columns' must be in [0, 32]`；
  `tube_threshold=0` = 合法 no-op（闭区间 [0,0.5] 端点语义，单测钉死——
  实现范围比设计表 [0,8]/(0,0.5) 宽一档，fixer 定案 + 测试背书）。
- **S2b 进洞走路（推迟）**：批跑按用户要求提前收尾时中断；机制由单测
  （增量重光/放置路径）+ S2 静态视图覆盖，用户在场时补跑（~2 min，
  场景/几何已就绪）。

## 已知问题 / 偏离

- 洞口悬树（MC 同款视觉，placeIfAir 不查脚下；follow-up 可选 ~5 行）
- 海床洞一次性渗水（生成平衡后静态）
- 洞内 24/7 敌对生成（MC 洞穴语义；出生保护圈不变）
- 旧存档加载得新洞穴（世界生成演进先例）
- 远处洞穴剪影 A/B 1 px（S1 记录；门禁 = 锚字节一致，权威通过）

## 下一步建议

- 用户在场补 S2b（若批跑 lock 失败）。
- BACKLOG 顺序下一项 = **GEN-2 树生长**（依赖 ENT-2 BlockState → 按执行
  顺序规则"行序先到时先做 ENT-2"——GEN-2 派单前须先做 ENT-2；或用户
  重排）。
