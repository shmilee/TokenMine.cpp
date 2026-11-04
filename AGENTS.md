# AGENTS.md — 代理快速接收

> Phase 30 文档整理产物。本文件是代理接手本项目的**第一入口**：读完本页即可开工，
> 细节按「文档地图」按需加载。所有规则原文见 `docs/PROTOCOL.md`。

## 项目一句话

**TokenMine.cpp**——本地 LLM 编写的 C++ 版 Minecraft：可运行、可扩展的 3D 体素沙盒，
C++20 / SDL3 / OpenGL 3.3 core，
自研 `Result`/JSON/PNG，**内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，
不改核心代码），**确定性是一级要求**（无输入运行逐帧字节可复现）。

## 现状（3.x「世界扩展」期——v3.0.0 已发布 11-04，首功能版 v3.1.0 =
V3-7 存档 gzip 待执行）

- **v2.0.0 = 1.x 收官里程碑**（无新玩法）：1.x 任务表 40/40 + UX-8g/UX-9
  实机报障 4 项全完成（v1.50.0 = 最后 v1.x 版本）+ 2.0.0 扫尾三件事——
  ① 1.x 阶段文档归档 `docs/archive/v1x/`（181 份 phase + `HISTORY.md`
  全史）② **第三人称准星不压头**（MC 同款：轨道点沿相机 up 抬高 0.4 块，
  头降到屏幕中心下 29-96px；瞄准射线仍自眼位 = UX-8g 不变，TP 固定视差
  = MC 同款记偏离）③ 活文档刷新到 2.0 状态。门 171 全 PASS。
- **任务只从 `docs/BACKLOG.md` 的 v2.x 节取**（11-03 三期分工定案：2.x
  「上手与体验补全」V2-3~V2-10 八项**全部完成（11-03 收官，
  tag v2.10.0）**；**11-03 收官日用户定案：原 3.x 八项并入 2.x
  = 第二批 V2-11~V2-18（XP/附魔/床/蝙蝠/染色羊/雪原 biome/顶部
  slab/AO，`docs/BACKLOG.md` 执行权威）**；**用户实机诉求提前项
  V2-19 视角/移动/时钟批已完成 = v2.11.0 门 182**（滚轮缩放 +
  双击 Space 飞行 + 24h HUD 时钟，全会话态零 format，base18 第
  3 次重锚）+ **V2-11 XP 系统已完成 = v2.12.0 门 183**（stash
  半成品恢复 + 全整数 MC 曲线 + 经验球 + 三发放源 + format 26 +
  XP 条 HUD，base18 第 4 次重锚 179027 B）+ **V2-12 附魔已完成 =
  v2.13.0 门 184**（7 附魔数据驱动 + 附魔台/书架 + 确定性 offer
  全整式 + format 27 + 面板，结构免疫第 65 次免重锚）+ **V2-13 床
  已完成 = v2.14.0 门 185**（15 色染色族 Slab 床 + 睡眠三门即时跳
  夜 + format 28 respawn + 验床重生，结构免疫第 66 次免重锚）+
  **V2-14 蝙蝠已完成 = v2.15.0 门 186**（项目首只飞行 mob：悬停
  巡航 AI + 洞穴 in-air 生成 + 0.9×0.5 展翼扇翅 + squeak 氛围音，
  零 format savenorm EXACT，结构免疫第 67 次，详见
  `docs/archive/v2x/phase-2.15.0.md`）+ **V2-15 染色羊已完成 = v2.16.0
  门 187**（shears 238 耐久 + 剪毛 1-3 彩毛 + 出生毛色流首抽 + 染色
  bare 拒染 + 死亡彩毛钩子 + per-entity 变体网格，零 format savenorm
  EXACT，结构免疫第 68 次，详见 `docs/archive/v2x/phase-2.16.0.md`）+
  **V2-16 雪原已完成 = v2.17.0 门 188**（snowy_taiga 数据驱动 +
  exclude_spawn_area 演示区结构保护 + snow 满块 + planTree 针叶 +
  音效缺陷族 13 件修复，零 format 世界演进，结构免疫第 69 次，详见
  `docs/archive/v2x/phase-2.17.0.md`）+ **V2-17 顶部 slab/双 slab 已完
  成 = v2.18.0 门 189**（stone_slab state_count 3 + drops_count_by_
  state 破掉 2 + 放置瞄准规则 + 同格合并 + partial-aware slab 射线，
  零 format bump，结构免疫第 70 次，详见
  `docs/archive/v2x/phase-2.18.0.md`）+ **V2-18 AO 环境光遮蔽已完成
  = v2.19.0 门 190（二批收官：第二批 8/8 全完成）**（经典 MC 顶点 AO
  4 档 1.0/0.75/0.5/0.3 双邻特例 + opaque ∧ 高度带闭区间 + bake 入
  既有光 attribute = 零 shader/零顶点格式，greedy MaskCell+ao[4] 合并
  键，fluid/cross/wall/fence/pane 免 AO，render.ao 严格 bool 默认
  true，**off = pre-V2-19 位级恒等**【门内 off ≡ 旧锚实证】，零
  format，**门 190 重锚 base18 ← 224468 B**【单调变暗审计 + 1 px
  z-fight 白名单】，4 偏离记档，PITS 384，详见
  `docs/archive/v2x/phase-2.19.0.md`）；**2.x 全史
  （v2.0.0→v2.19.0，两批 20 版）11-04 收官** /
  3.x「世界扩展」= **新执行权威** `docs/BACKLOG.md`（11-04 晋升 +
  逐项定案 + **同日用户重编号定案：原 4.x 命名改 3.x**【2.x 已吸收
  原 3.x 八项，回归自然序号】）：6 项全保留 + 用户实机诉求 **V3-7
  存档 gzip 压缩（默认开 = format 29，旧明文档魔数探测零迁移，实测
  5.4×）先执 = v3.1.0**，其后 V3-1 下界维度锚【超大】拆分立项【门
  框→下界世界→内容三段】；**v3.0.0 = 3.x 开启版已发布**（11-04：
  重编号 + README 里程碑精简，纯文档 + banner，零玩法变更，tag +
  release）；2.x 总量 24 < pre-v2.0 的 78-84 = 用户
  采纳判据 ✓）；2.x 全史 =
  `docs/archive/v2x/HISTORY.md`（+ 同目录 BACKLOG/CHANGELOG/phase
  存档 20 份，全两批）；1.x 历史全在 `docs/archive/v1x/HISTORY.md`
  （只读，别把历史细节当现状）。
- 测试 **2482/2482**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告；
  存档 format 28（V2-13 床引入 player.respawn 恒写；27 = 附魔
  xp_seed+enchants；26 = XP；V2-14 蝙蝠 / V2-15 染色羊 / V2-16
  雪原 / V2-17 slab / V2-18 AO 均零 format【slab state = format 15
  states 机制；AO = 纯渲染 bake 入既有光 attribute】）；
  base18 锚 **224468 B**（679×744 WM tile 态——重锚谱系：179116 →
  178148【V2-10 门 181】→ 178676【V2-19 门 182】→ 179027【V2-11
  门 183】→ **224468【V2-18 门 190，AO 世界光照】；门 184-189 =
  结构免疫第 65-70 次免重锚**）；PITS
  375-379 + **PITS 380**（spawn 成功行无 'pick' / 门场景洞穴密封 /
  CAVE config seed 归属 / bob diff 带色相审计）+ **PITS 381**（热栏
  key→RMB ≥0.15s / 活体掉落物品不可作终档断言 / 羊静止窗）+
  **PITS 382**（确定性音效 LCG 逐采样重建 = 静音/退化 wav 缺陷族
  13 件【音频门盲区】）+ **PITS 383**（stateful 块场景 journal 须
  states 条目【init 拒 = 输入静默丢失】/ cell DDA 半块全遮挡 =
  合并不可达【partial-aware 射线修】/ 临时插桩定界后移除重建）+
  **PITS 384**（e+1 索引越界【sign 数组 (e+1)/2 纪律】/ e 向 = 背离
  网格单元于顶点 + 面表 edgeA/edgeB 轴错配双实现交叉钉 / 重锚
  z-fight 换面白名单【单调变暗审计豁免上限 + 逐 px 留证】/ 门窗口
  679×744「y>600 = UI 带」启发式无效【实测 HUD bbox】/ 世界坐标
  手算 7 连错）；
  门计数 190（**V2-1 前视角 v2.1.0** 门 172：
  C 键三循环，前视 = 后视同款数学翻转角执行 + aimReach D+kReach——详见
  `docs/archive/v2x/phase-2.1.0.md`；**V2-2 Steve 皮肤 v2.2.0** 门 173：
  模型 per-face 纹理通用能力【既有 mob 位级不变】+ Steve 脸 8×8 像素
  + 7 tile 图集零移位——详见 `docs/archive/v2x/phase-2.2.0.md`；
  **V2-3 教程系列化 v2.3.0** 门 174：tutorials/ 三系列（beginner 零
  改动 + 农耕 + 厨房）+ format 25 + pickup./smelt. 事件 + T 跳当前
  系列——详见 `docs/archive/v2x/phase-2.3.0.md`；
  **V2-4 天气 v2.4.0** 门 175 全 PASS：core/weather 纯状态机（推进式流
  首段恒 CLEAR【60f 基线免疫根】+ 雷独立 salt）+ rain shader 静态 300
  滴 + 天空 dayFactor 单收口（RAIN ×0.85 + 雷 5 帧脉冲）+ 交互四件
  （雨灭火/营火/燃烧效果/润耕地 2×）+ render.weather strict 默认 true
  【off = v2.3.0 位级恒等，门内三方恒等实证】+ --weather CLI + 雷 wav
  + 零 format；性能 A/B changesApplied 9/3/1 逐位 + 终档集合恒等——
  详见 `docs/archive/v2x/phase-2.4.0.md`；**V2-5 猪 v2.5.0** 门 176：
  纯数据零核心代码 + V2-2 per-face 首个 mob 消费（pig_face 显式 8×8
  脸）+ pork 食物链（+3→+8）+ 图集 99 零移位 + 字体 423——详见
  `docs/archive/v2x/phase-2.5.0.md`；**V2-6 体验打磨批 v2.6.0** 门 177：
  四子项各消费一处记档偏离——① 第一人称手臂（arm_r box 数据驱动
  pa_arm 零新纹理 + 三态门控 + 静态无摆动记档）② mob blob 阴影
  （1.5×宽扁 quad alpha 0.3 贴底格顶面 + 玩家仅第三人称）③ 拾取磁力
  （帧锁定 f(距离,帧)：6×(1−d/2.5)² 加速趋近 + 24f swirl + hasRoomAt
  不穿墙 + 插墙箭同款拉出）④ 月相（8 相 = 累计天 index%8，食相 quad
  滑移零新纹理，**绘制序 = 食相先/月后**——r=800 平面 24-bit 深度
  ~160 块分辨，PITS 371）+ 零 format；基线 60f = base18 逐字节恒等
  ——详见 `docs/archive/v2x/phase-2.6.0.md`；**V2-7 材料补齐批 v2.7.0**
  门 178：四子项全数据驱动（核心唯一新增 = 块级 per-face faces 段，
  无 faces 位级不变红线）——① planks 块化（既有 tile 零新纹理 +
  fuel_time 7.5 迁块属性 + 8 配方按名零改动）② gold/diamond_block
  （iron_block 先例 + beacon_base 布尔入列）③ spider 掉
  string/spider_eye ×0-2（纯收集记档）④ furnace_front 单 tile
  （lit = 既有火焰粒子记档）+ 图集 102 零移位 + 零 format——详见
  `docs/archive/v2x/phase-2.7.0.md`；**V2-8 sky rescan clobber 修复 v2.8.0**
  门 179：光照核心定向修复（LGT-1 记档潜伏 bug：天花板骑跨 footprint
  边界 → 后一 region rescan 覆写边界格 sky 为纯垂直值 + sky BFS 种子
  无界外源 = stale-dark/stale-partial 暗带）——修复 = sky BFS 种子盒
  +1 块 shell 环（rescan/BFS 写界/no-op skip/propagateAll 零改动；
  shell 读 = 既有 1-chunk 边界读环 → 独立性判据/confluence 零改动）+
  TDD 双回归（pre-fix 2464/544 diff 格复现）+ CPU 探针 + E2E slab
  无输入流送判别 + 零 format——详见 `docs/archive/v2x/phase-2.8.0.md`；
  **V2-9 step-up quirk 修复 v2.9.0** 门 180：player/mob 共享
  Entity::stepUp 两 quirk 定向消除——① +X/+Z 恰 1 格宽台阶踏不过
  （front 格 snapped+1 不对称 → 查贴住格）② 非整数脚高巧合对齐
  凭空抬升 1.0（flush 位置判 → 无状态真阻挡验，严格不等式）；
  整数脚高位级不变红线 + TDD 3 新 + 1 反转 + 零 format——详见
  `docs/archive/v2x/phase-2.9.0.md`；**V2-10 网格修复批 v2.10.0** 门
  181：渲染节两记档修复——① 叶子-叶子内部面剔除（块属性
  cull_same_id 严格布尔，leaves 族 3 块首消费，mesher greedy pass
  同 id 剔面纹理选择前，glass/水/cross 族红线零触碰）② alpha
  cutout discard 零代码确认（PRE-5 桶无关 discard 普查全覆盖 +
  单测钉）；**base18 第 2 次重锚 178148 B**（diff 4185 px 全叶冠
  区零泄漏论证，Phase 18 先例）+ 零 format——详见
  `docs/archive/v2x/phase-2.10.0.md`）。
- 活跃已知问题：`docs/KNOWN_ISSUES.md`；**3.x「世界扩展」= 当前执行
  权威：`docs/BACKLOG.md`**（11-04 晋升 + 逐项定案 + 同日重编号
  【原 4.x 命名改 3.x】：6 项全保留 + 用户诉求 **V3-7 存档 gzip
  （默认开 = format 29）先执 = v3.1.0**（用户开新会话后执行），其后
  V3-1 下界维度锚【超大】拆分立项；v3.0.0 = 开启版已发布【纯文档 +
  banner】；逐版 tag + GitHub release 政策沿用 2.x 定案）；2.x 全
  任务表 = `docs/archive/v2x/BACKLOG.md`；1.x = `docs/archive/v1x/`；
  会话交接：`opencode-sessions/SESSION_CONTEXT.md`。
## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告）
./build/bin/tokenmine_tests                                                 # 测试（2075）
./build/bin/tokenmine                                                       # 运行（ESC 退出）
./build/bin/tokenmine --frames N [--screenshot /abs.png] [--save slot] [--new] [--config path]
./build/bin/tokenmine --seed 42 --time 0.75 --log-level info                # 其它参数（--help 看全部）
```

游戏必须从仓库 cwd 启动（资源相对路径）；独立测试程序链接
`build/lib/libmc_core.a -lSDL3 -lGLEW -lGL -lz -lpthread`。

门协议工具（PITS 317 纪律：门资产可从仓库重建）：`tools/gate_compare.py`
（raw = PNG 字节比对 / norm = savenorm 比对，剥 real-dt 字段）、
`tools/perf_gate_config.json`（性能 A/B 用 config）、`tools/g139_instrument.py`
（性能插桩补丁器）、`tools/xenv.sh`（门机 X 环境）。

## 硬性规则 Top 10（违反即 bug；全文 docs/PROTOCOL.md）

1. **坐标**：world/chunk/local 严格分离，换算只在 `coordinates.hpp`；负坐标 `floorDiv`。
2. **渲染隔离**：游戏逻辑不碰 GL/SDL（renderer/ 与 window/input 边界除外）。
3. **数据驱动**：核心不 switch 方块 id（读 properties）；air 是代码哨兵（无 air.json）。
4. **错误与代码质量**：`Result<T>` + `MC_TRY` 显式传播；**无 TODO/伪代码/魔法数字/
   硬编码方块检查/删功能过构建/禁用测试**。
5. **相机**：yaw 0 = 朝 -Z、正 = 左转；pitch 正 = 上、clamp ±~89°；**鼠标转向仅在
   指针锁定时生效**（无输入运行字节复现的保证）。
6. **玩家物理**：position = AABB 底面中心；重叠判定**严格不等式**（无 epsilon）；
   防穿透 = 子步化（每子步 ≤0.4 块、≤64 子步）+ 终端速度 38。
7. **chunk streaming**：worker job 只在 **post→flush 窗口**运行（窗口内主线程只读
   World）；**光照 job：流送与流体追赶两类各至多 2 个/帧，同窗内两两位级独立**
   （99.10：判据
   `lightRegionsIndependent` = 写 reach ±1 列 + block 光 BFS 读环 +1 列双向不相交
   （gap≥4 任轴），`world/light_job.hpp` 纯函数；依赖对回退单 job 顺序 =
   pre-99.10 逐位行为；confluence 证明 =
   `docs/archive/v1x/phase-lgt1-flip.md`；99.11：流体 tick 的 changed 格 → 列粒度
   待集 `m_fluidLightPending`（`changedCellColumns` 合并同列），
   `planFluidLightJobs` 按 set 序取至多 `kFluidLightColumnsPerFrame = 2` 列/帧
   发 worker job（newTerrain=true 强制天空传播 = 99.9 跳过的流体洞修正），
   跨类同窗仅当对每个流送 job 列都独立（失败候选跳过留待下帧）；净变 chunk 走
   job-local `dirtyOut`，flush 后主线程统一 markChunkDirty → 流体列重烘
   （99.4 纪律）；卸载驱逐待集死列防无界增长）。
8. **确定性**：昼夜/Mob/箭/粒子 = 固定 1/60 帧步长；生存计时器（饥饿/进食/破坏/
   坠落/实体 age）= 真实 dt；截图只在帧边界字节一致。
9. **面表绕序**有回归测试守护（`FaceWindingIsCounterClockwiseFromOutside`），勿改
   顶点序；光照 = `max(sky − (8/15)×(1−df), block)/15 × faceShade`（顶 1.0/±X 0.8/±Z
   0.6/底 0.5；LGT-3 仿射撤退 + kNightSkyLevel 7 月光地板；per-draw 光覆盖 =
   PRE-6/LGT-4 掉落/光束，覆盖后强制复位）。
10. **出生点**：4×4 chunk 演示区列，按 `kSpawnCandidates` 自动找干燥陆地
    （h > seaLevel 且出生 AABB 内无 solid）。

## E2E 协议（摘要；全文 docs/PROTOCOL.md「E2E 协议」）

- **零并发 :0 活动**（用户手离开鼠标）+ 每拍截图前 `unmap_covers`。
- **无输入运行**：×2 全帧字节一致 + skygate（FRESH 模式色）+ base 门
  `/tmp/opencode/base18.png` 字节一致（新系统需论证"无输入下零活动"= 结构性免疫）。
- **输入驱动运行（xdotool 脚本）**：**存档状态（含 yaw/pitch）是确定性权威证据**——
  全帧字节一致受 SDL 指针锁定 grab delta 环境限制（一次性相机旋转 ±0.01 rad = 4 px；
  缓解 = warp 实际原点 + (632,352)、同批同 warp、手不动；存档 yaw/pitch = 漂移探测器）。
- 输入锚定窗口发现时刻，D ∈ [0.1, 0.5] s；tick 类断言需 ≥1.6 s 裕量（1/60 双累加
  边界：36 s 饥饿 tick = 帧 2161，20 s 饥饿伤害 = 帧 1200，1.6 s 进食通道完成 = 帧 97）。
- **HUD 红心检查**（心带 y[646,661] x[551,729]）：无输入运行 = 0（>50 = 污染废拍）；
  输入运行 >50（输入生效）。

## 阶段流程（每个 phase 按此顺序）

1. 用户确认方向 → 2. 实现（数据驱动、无 TODO）→ 3. 构建 + 全量测试（`-Werror`）→
4. E2E（base 门 + 回归对 + 场景专属验证，按上面协议）→ 5. 文档（CHANGELOG v2.x 条 / BACKLOG v2.x 表 + 变更记录、
KNOWN_ISSUES、opencode-sessions/SESSION_CONTEXT）→ 6. **显式路径列表** git commit（永不 `git add -A`；
永不提交 `opencode-sessions/` 下任何文件（本地））→
7. 中文阶段报告（标准结构：实现内容/变更文件/测试/构建与验证结果/已知问题/下一步建议）。

## 坑位 Top 8（全文 docs/PITS.md，384 条）

1. **SDL grab delta**（P28）：输入 E2E 全帧字节一致不保证——存档 yaw/pitch 为漂移
   探测器；驱动 warp 用实际窗口原点（实测 (4,64)）+ (632,352)。
2. **物理光标**（P5）：指针未锁定时 SDL 仍投递 mouse motion → 无输入 E2E 必须零
   :0 活动，红心检查 = 污染哨兵。
3. **粒子视觉验证视角**（P29）：直下视角可视锥不含破坏粒子——必须斜视角（pitch
   −1.02 级）；帧差分析先排除已知 UI 变量（饥饿/HP 图标也在 diff 里）。
4. **splitmix64 是纯 finalizer**（P29）：不推进 state——推进式流必须
   `state += 0x9E3779B97F4A7C15ULL` 后再 mix（同 burst 同值缺陷根因）。
5. **`std::string::replace(pos,len,const char*,n)` 截断**（P28）：只取替换串前 n 字符。
6. **`--save slot` 第二次运行会加载第一次写的存档**（P15）：E2E 对跑每拍从源副本
   `cp` 新鲜存档。
7. **按住破坏会自动续破下一块**（P15）：输入脚本的释放时机 = 破坏完成 + 余量，
   余量内的按住对下一块产生确定性裂纹进度。
8. **像素验证**：`glReadPixels` 必须在 `swapBuffers()` 后（前缓冲）；
   `magick ... txt:` 输出 `(r,g,b,a)` 开头 `(` 破坏数值转换，awk 取 `$2`。

## 禁止事项

- `config/config.json` 是**已提交文件**（P32 起：用户调值 view_distance 80 /
  day_length 600 + `player` 默认节）——**不还原用户的调值**；内容改动仅在用户
  要求时做，提交用显式路径。
- **永不 `git add -A`**——提交用显式路径列表。
- `opencode-sessions/` 目录（SESSION_CONTEXT/kickoff_prompt 等）本地专用（`.git/info/exclude`），永不提交。
- 无 TODO/伪代码/硬编码检查/魔法数字；新内容一律走数据文件 + 注册表严格校验。

## 文档地图

| 文档 | 内容 | 何时读 |
|---|---|---|
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（现状 v2.0.0，16 子系统分节；2026-11-02 刷新，细节证据在 v1x 阶段存档） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ 存档使用 + worldgen 参数 | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（384 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃；pre-2.0 已修复索引 → `docs/archive/PRE2.0-FIXED-INDEX.md`） | 阶段验收前后 |
| [docs/archive/v0x/ROADMAP.md](docs/archive/v0x/ROADMAP.md) | 0.x 时代 Phase 0–29 规划（已归档） | 了解历史 |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | 版本历史（3.x 时代逐版明细，v3.0.0 起；2.x 全两批 → `docs/archive/v2x/CHANGELOG.md`、v1.x → `docs/archive/v1x/CHANGELOG.md`、v0.x → `docs/archive/v0x/CHANGELOG.md`） | 查某版改了什么 |
| [docs/BACKLOG.md](docs/BACKLOG.md) | **3.x「世界扩展」任务表（执行权威）**（11-04 二批收官晋升 + 逐项定案 + 同日重编号【原 4.x 命名改 3.x】：6 项全保留 + 用户诉求 **V3-7 存档 gzip（默认开 = format 29）先执 = v3.1.0**，其后 V3-1 下界维度锚拆分立项；2.x 全任务表 → `docs/archive/v2x/BACKLOG.md`、v1.x 表 → `docs/archive/v1x/BACKLOG.md`） | 取下一个任务 |
| [docs/archive/v1x/](docs/archive/v1x/) | 1.x 阶段细节存档（181 份 phase，只读历史）+ HISTORY.md 全史 + v1x/CHANGELOG.md + v1x/BACKLOG.md | 深挖 1.x 某阶段 |
| [docs/archive/v0x/](docs/archive/v0x/) | 0.x 时代文档（ROADMAP 规划 / DISCUSSION 记录 / v0x/CHANGELOG.md） | 深挖 0.x |
| [docs/archive/v2x/](docs/archive/v2x/) | 2.x 阶段细节存档（20 份 phase，全两批，只读历史）+ v2x/HISTORY.md 全史 + v2x/CHANGELOG.md + v2x/BACKLOG.md | 深挖 2.x 某阶段 |
| `docs/archive/phase-3.x.0.md`（新阶段放 docs/archive/） | 3.x 阶段细节存档（收官后沉 `docs/archive/v3x/`） | 深挖某阶段 |
| `opencode-sessions/SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步（+ kickoff_prompt 启动 prompt；SESSION_CONTEXT_PRE2.0 / LEGACY_STATUS_P30 / SESSION_HISTORY / 门资产 → 同目录 `archive/`） | 任何"继续"会话 |
