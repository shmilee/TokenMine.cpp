# Phase 2.0.0 — 1.x 收官里程碑发布（扫尾三件事，门 171 全 PASS）

> 用户 11-02 定案：「接下来准备发布 2.0.0，在此之前完成扫尾」=
> ① 第三人称准星不压头（B 方案 = MC 同款抬相机，用户二选一后定案）
> ② 文档整理（已完成任务 + 1.x 阶段文档归档）
> ③ 为 2.x 做准备（避免受过时文档影响）+ ④ 其他扫尾。
> **2.0.0 = 1.x 全完成的里程碑发布（无新玩法）**：tag v2.0.0 +
> GitHub release；2.x 任务表（docs/BACKLOG.md v2.x 节）待用户定范围。

## 扫尾②：第三人称准星不压头（B 方案，4bbacaf）

用户报障「第三人称瞄准的十字不要放我头上」：UX-4 相机 = 眼位后 4.0 块，
屏幕中心射线穿过玩家头部 → 准星（屏幕中心 2px 十字）压在头上（门 S2
OLD 侧中央带 160 皮肤 px 实证）。

- **轨道点沿相机 up 轴抬高 `kThirdPersonOrbitHeight = 0.4f`**
  （`thirdPersonOrbitCenter(eye, yaw, pitch)` 新纯函数）——pitch 0 时
  头 top 投影 (0.4−0.18)/4 ≈ **29px**、头底缘 (0.4+0.32)/4 ≈ **96px**
  低于屏幕中心（shipped 70° vfov / 744px 窗，~531 px/单位@1 块）=
  准星零重叠；**沿 up_cam 构造保证任意 yaw/pitch 偏移恒定**（仰视/
  俯视不回归头上）
- `Camera::up()` = `cross(right(), forward())`（lookAt 推导：
  cross(forward, unitY) = cos(pitch)·right() 正标量 → view 矩阵实际
  up 即此式；零新三角，单一真源与视图矩阵锁步；单测钉
  viewMatrix() up 行 == up() 6 朝向 1e-6）
- `resolveThirdPersonCamera` 逻辑零改动（签名本就吃 orbitCenter：
  遮挡段长度恒 4.0、embedded/raycast/穿流体三分支不变）；调用点唯一 =
  game_frame ThirdBack 分支，**First 分支字节不变 = 结构免疫根**
- 瞄准射线两模式仍从 `eyePosition()` 出发（UX-8g 红线零动）→ 屏幕
  中心射线与眼位射线平行偏移 0.4 = **固定第三人称视差**（目标 2m 处
  ~36px、5m 处 ~24px；MC TP 同款观感，记偏离，代码零补偿）
- crouch：eyePosition() 含蹲缩 → 轨道点随之降，零额外代码

## 扫尾①：1.x 阶段文档归档（eec7206）

- `docs/archive/` 181 份 phase 文档（v0.x P 时代 + v1.x 任务表时代，
  全完成）`git mv` → **`docs/archive/v1x/`**（保历史；2.x 的 phase
  文档今后直接放 archive 根）
- 全仓引用单一规则修正 **556 处**（AGENTS 86 / ROADMAP 133 /
  KNOWN_ISSUES 147 / CHANGELOG 27 / BACKLOG 27 / README 2 / 其余 4 +
  v1x 内互引 32 + opencode-sessions 92 本地），`archive/phase-` 残留 = 0
- BACKLOG：v1.x 任务表（40/40 ✅）标记归档 + 顶部开 **v2.x 任务表占位**
  （待用户定范围）

## 扫尾③：2.x 准备（避免过时文档影响）

- **AGENTS.md「现状」节重写** = 2.0.0 时代（v1.x 巨段编年史 149k +
  逐门记录 19k 抽出 → `docs/archive/v1x/HISTORY.md` 只读全史；
  AGENTS 只留当前状态 + 指针）；命令节测试数 1802→2075、坑位计数
  359、文档地图 v1x/ 双行（1.x 只读 / 2.x 新档）
- **MECHANICS.md 刷新 v1.33.0→v2.0.0**（16 子系统逐节事实更新：
  format 24 / 36 槽 / 教程 11 步 / TP 相机 + 视差 / cave_ambient /
  云 shader / 79 件物品模型等 v1.34 起变更）
- **KNOWN_ISSUES.md 分诊**（v1.34→v2.0 已修条目移入已修复索引 +
  新增 TP 视差设计记档）
- **GAMEPLAY.md**（教程 11 步 / 第三人称观感 + 两键亮度 / 36 槽背包 /
  数字键 / 工作台修复）
- **门协议工具入仓**（PITS 317 纪律：门资产须可从仓库重建）：
  `tools/gate_compare.py`（raw/norm 比对器，/tmp 副本 g117.py 归档化）
  + `tools/perf_gate_config.json` + `tools/xenv.sh`

## 门 171（编排者执行；old 侧 = v1.50.0 = 8e46310 = tm_v150_base）

- **S1 = 第 171 次结构性免疫 + 默认 First 位级恒等 + base18 锚连续
  179116 B 免重锚**：fresh 60f 四方 ×2（new v2.0.0 ×2 / old ×2）
  全字节一致 + 跨版 savenorm EXACT（ThirdBack 分支无输入永不执行）
- **S2 = TP 准星 A/B 判别 PASS**（gate-158 捕获协议，1200f，
  同 fresh 场景跨侧）：
  - a（OLD v1.50.0 TP）×2 RAW 269318 B 字节一致 / b（NEW TP）×2 RAW
    281133 B 字节一致 / w（NEW 墙场景）×2 RAW 277057 B 字节一致
  - **皮肤判别量 (224,178,132)±15**：OLD 中央带（中心 ±20px）= **160
    px = 准星压头实证（用户报障本体）** / NEW 中央带 = **0 px** / NEW
    下移带（中心 +23..+108px）= **720 px = 头降到准星下方（29-96px
    解析值吻合）**
  - 中心像素抽样：OLD (330,372)/(348,372) = 面部 (137,109,81) vs NEW
    = 草 (83,154,57)/(93,164,60)；(339,400) OLD 面部 vs NEW 恰皮肤
    (224,178,132) = 头带位移逐像素实证
  - **w 遮挡回归（gate-158 墙场景重跑）= 2390 皮肤 px = 相机仍出墙
    （embedded 退出语义随抬升轨道保持，gate-158 修复不回退）**
  - **跨侧 savenorm a≡b EXACT ×2 = 相机模式/轨道抬升不入档、同世界
    最强形** + 玩家 pos/yaw/pitch/hp 全精确零漂移
- **S3** = 2075/2075。**无 S6**（零 parse 面——纯几何常量 + 纯函数）
- **性能 A/B v1.50.0 vs v2.0.0（发布前硬门槛）PASS**：fall 900f 无
  输入 First（默认 = 抬升代码结构惰性）——PERF-WIN 三窗全指标同档
  （render med 0.488/0.489/0.466 vs 0.444/0.438/0.472）+ PIPE3-S0
  changesApplied **9/3/1 三窗逐位** + 终档 fluids 18/18 + blocks 55/55
  集合恒等 diff 0 = 同世界同成本

## 发布

**tag v2.0.0 + GitHub release**（里程碑 notes：1.x 矩阵 40/40 +
UX-8g/UX-9 实机报障全清 + 扫尾三件事 + 工程数据）；README 里程碑
节 +v2.0.0。2.x 任务表 = 空，待用户定范围。
