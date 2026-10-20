# Phase STRM-1 — 流送环形状（v1.0 任务表 第 1/29 项）

> 2026-10-20/21。来源 = `docs/BACKLOG.md`（v1.0 任务表，执行权威）首项：
> "方形流送环 (2R+1)² → 圆形/球形；vd80 内存 12 GB → ~1/9（99.x 残余 #1）"。
> 用户定案（2026-10-20）：**选项 b = 圆盘 + 远平面帽 R_eff = min(view_distance, 64)**，
> 帽 = 代码常量静默映射（无 config 键，YAGNI）；~1/9 目标修订为 ~1/2
> （R_eff=30 档一个常量可切换，暂不采用）。

## 实现内容

- **谓词 Chebyshev 方形 → 平方欧氏圆盘**：`columnDistanceSq(a,b) = dx²+dz²`
  （int64，纯整数零浮点）；环 = {dx²+dz² ≤ R²}；`missingColumns` 仍走 [−R,R]²
  界框循环 + 平方过滤（每帧成本同现状），排序键 (distanceSq, x, z)；
  哨兵 = `missingSentinelDistanceSq(R) = (R+1)²`；触发 engage ⟺
  nearestMissingSq ≤ 1²（整数偏移下 = 玩家列 + 4 轴邻列，对角 (1,1) sq=2
  不触发）；旧 `columnDistance`（Chebyshev）失去全部消费者后删除。
- **远平面帽 `kStreamCapRadius = 64`**（game.cpp 命名常量 + 推导注释）：
  kFar = 1000 块 = 62.5 列；逐格点验证：可绘制列全部 d ≤ 63.91（连续界；
  格点最大 63.89，(41,49) 族），d ≥ 65 的列最近角 ≥ 1002.01 块（(40,50) 族，
  2 块裕量）= 原理上永不渲染，流送纯内存浪费。4 个轴列 (±64,0)/(0,±64)
  在盘内但永不可绘（无害包含，4 列）。
- **帽的施加**：`m_streamRadius` 保留 config 原值（报告语义：F3 "radius %d"
  + engage 日志）；两处几何消费者取 `min(m_streamRadius, kStreamCapRadius)`
  （streamChunks 环谓词/日志 M、unloadFarColumns keep 判定 →
  平方 `≤ (R_eff+1)²`，迟滞 +1 语义不变，保留集逻辑一字未动）。
- **日志 M 值**：2 处 `(2R+1)²` → `diskColumnCount(R_eff)`（精确格点计数
  助手；vd80 = 12853）。
- **连带修复**：`item_registry.cpp` 字段白名单漏列 `infinite`（P77 潜伏
  表面 bug：字段被解析但白名单未列 → 每次场景构建 WARN "unknown field
  'infinite' ignored"；行为一直正确【P77 E2E arrow×4 实证】，纯日志噪音）
  = 白名单补 1 字段。
- **确定性**：谓词路径零浮点；无 format 变更、无资产变更、无新 config 键；
  帧管线纪律全不动（2 列/帧 post、积压 8、光照 1 job/帧、remesh 预算 8）。

## 变更文件

- `src/world/chunk_streaming.{hpp,cpp}` — 圆盘谓词 + API（+`diskColumnCount`
  / `missingSentinelDistanceSq` / `nearestMissingDistanceSq`；−`columnDistance`）
- `src/core/game.cpp` — `kStreamCapRadius` + 推导注释；几何帽 2 处；卸载
  平方判定；日志 M 2 处；注释
- `src/core/game.hpp` — `m_streamRadius` 原值语义注释
- `src/item/item_registry.cpp` — 白名单补 `infinite`
- `tests/test_chunk_streaming.cpp` — 8 个方形期望改写 + 5 新测试
- `docs/` — KNOWN_ISSUES（99.2 残余② + vd80 残余 + 触发谓词）、BACKLOG
  （STRM-1 完成 + 目标修订）、ROADMAP（存档索引行）、PITS（+6，见下）、
  AGENTS（现状/测试数/基线门第 81 次/坑位数）

## 测试

1200/1200（1195 基线 + 5 新 − 0 删；8 个方形测试语义改写为圆盘）：
- `DiskColumnCountExact`：0→1、1→5、64→12853、80→20081（精确值）
- `DiskRadius80Boundary`：(80,0) in / (1,80) out / (50,62) in / (79,14) out
  + 全盘 20081 精确成员
- `MissingSentinelDistanceSq`：(R+1)² 各值
- `StreamingTriggerIsAxisOnly`：仅对角 (1,1) 缺失不 engage、仅轴邻 engage
  （与旧 Chebyshev 触发互斥钉死）
- `MissingColumnsEuclideanOrderDiffersFromChebyshev`：排序键语义钉死
- `-Wall -Wextra -Wpedantic -Werror` 0 警告

## 构建与验证结果（E2E）

- **S1 无输入基线门 = 第 81 次过门**：fresh 60f ×2【新代码】+ ×2【方形
  基线二进制】+ A/B 对比：**4 对截图全字节一致**（circ r1==r2、sq r1==r2、
  circ r1==sq r1、circ r2==sq r2）+ 红心 0 + skygate (120,181,242) 严格 0
  容差。注：原 `/tmp/opencode/base18.png` 被 /tmp 清理丢失（P36 先例
  重拍）——新旧二进制 A/B 直接对比 = 比对照丢失原件更强的免疫证据
  （无输入时流送永不 engage：spawn 列 (0,0) 最近缺失列距离 2 > 触发 1）。
- **S2 填充到 complete + RSS 实测 A/B**（同机同场景：玩家列 (2,0)）：
  - 圆：`Streaming complete: 12853/12853 columns (102824 chunks)`，
    RSS 平台 **4805760 kB ≈ 4.59 GB**
  - 方：`Streaming complete: 25921/25921 columns (207368 chunks)`，
    RSS 平台 **9522264 kB ≈ 9.08 GB**
  - **−49.5%**（比值 0.5047 vs 几何 12853/25921 = 0.4959，一致）；
    文档原"~12 GB"为估算，方形实测锚点 = 9.08 GB（477→384 KiB/列）
- **S5 圆边光照回归**（场景 = 玩家列 (64,0)，3600f ×2，P54 条款）：
  存档 ×2 字节一致 + notblack 0 + skygate PASS；截图差 = 流送边界
  pop-in（墙钟相关，P54 已知）。首轮 600f 失败两因：① 600f 窗口跨越
  表面湖焦土燃尽边界（火寿命 real-dt = P64 设计）→ 火集合两跑不同
  （对角火格互换）→ 改 3600f（越过 P90 静态终态窗）② 全黑帧（合成器
  遮挡下回读黑）→ 尺寸 + 全黑废拍守卫。
- **S4 卸载回归（输入驱动北走 12 s）**：夜间 away 组件（窗口隐藏
  ~10-30 s/周期；用户当夜只修了壁纸切换、窗口隐藏持续整夜）令 R=64 版
  3 次整跑失败（点击捕获被偷，PITS 有条目）。**采用小环变体**（scratch
  config view_distance=8 → R_eff=8：同一平方谓词 / keep band (R+1)² /
  月牙对账代码路径，半径参数化；NOLOCK——移动键不受指针锁定门控，输入
  有效性 = 自校准 k 证明）：填充 ~3 s + 走路 12 s 塞进窗口存活安全窗。
  **双跑全硬断言过**：k=3（北走 46.5 块自由距离后撞坡，与探针一致）、
  Σ"Unloaded N" = **26 = C(3)@R8 精确**（南月牙格点穷举）、末条 chunk
  计数 1368 = 1832−8×26 精确、玩家存活（health 20）、存档 ×2 仅玩家 z
  差 7 cm（real-dt 走路相位，A7 soft 预期内）；A6 红心 NOLOCK 模式降
  informational。**偏离**：R=64 全环卸载对账夜间未做（away 模式只在
  用户缺席时活跃——用户回桌后可补跑 ~15 min，可选）。

## 环境发现（PITS +6，STRM-1 E2E 夜跑）

1. away-mode 桌面偷点击（IsUnviewable/IsUnMapped → 指针落 root，键盘
   路由不同不受影响；三重防线：窗口守护 + 点击前指针目标检查 +
   "Pointer captured" 日志验证点击）
2. XTest 假输入不重置 X screensaver 计时器（夜跑 `xset s off; xset -dpms`，
   收工恢复）
3. unmap_covers 豁免自己窗口：xwininfo hex vs xdotool decimal id 格式
   不一致 → 豁免静默失败（_NET_WM_PID 双保险）
4. real-dt 火 + 活跃焦土场景存档：跑窗须越过静态终态（3600f）
5. 合成器全黑回读：notblack ≥ 800000 = 废拍重试
6. zsh 函数前缀赋值不持久（验证分支被静默跳过）

## 已知问题 / 偏离

- 原 BACKLOG 目标 "~1/9" 未达成（= 半径问题非形状问题；同 R 圆盘上限
  9.3 GB）；用户定案接受 ~1/2（选项 b），R_eff=30 档（~1.3 GB，480 块
  地平线）留作一个常量的后续开关——若采用需 base18 重锚论证 + PRE-2
  边缘雾紧迫性上升。
- 4 个轴列 (±64,0)/(0,±64) 在盘内但永不可绘（无害，4 列 ≈ 1.5 MB）。
- `updateChunkMeshes` 最近优先排序的内联 Chebyshev 启发**未改**（结果与
  序无关：mesh 是纯函数；改了只是 cosmetic）——行为不变。
- PIPE-1（worker 网格化）价值待本项后重测（BACKLOG 规则）：圆环不改变
  每帧流送成本结构（2 post / 1 light job / remesh 8 全部 per-column
  恒定），只减少总列数 −50% → 预期结论不变（remesh 仍在主线程），
  按用户 10-21 夜授权继续执行。

## 下一步建议

- PIPE-1 worker 网格化（BACKLOG 第 2 项；快照式设计：光照 region 与
  mesh 读竞争边界 = unlit 列 3×3 邻域）
- PIPE-2 存档增量（退出全量 diff → 增量编辑跟踪 O(编辑数)）
