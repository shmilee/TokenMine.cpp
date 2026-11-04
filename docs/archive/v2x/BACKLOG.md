# BACKLOG.md — v2.x 任务表（已归档，全两批完成）

> 2.x「上手与体验补全」期**收官（11-04）**归档：第一批（V2-1~V2-10，
> 11-03 收官）+ 第二批（V2-11~V2-18 = 原 3.x 八项并入，11-04 收官）。
> 现行执行权威 = `docs/BACKLOG.md`（**3.x「世界扩展」期任务表**——
> 原命名 v4.x，11-04 同日用户重编号定案改 3.x；由 `docs/
> BACKLOG_4x.md` 方向底稿 11-04 晋升，同款先例）。1.x 完整表 →
> `docs/archive/v1x/BACKLOG.md`。逐版细节 = 本目录 `phase-2.x.0.md`。

## v2.x 任务矩阵（10/10 全完成，11-03 收官）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ | ⑦ |
|---|---|---|---|---|---|---|---|
| STRM 流送与内存 | | | | | | | |
| PIPE 帧管线与并发 | | | | | | | |
| LGT 光照 | ~~V2-8 sky rescan clobber 修复~~ ✅ v2.8.0（门 179） | | | | | | |
| GEN 世界与生成 | | | | | | | |
| FLUID 流体 | | | | | | | |
| ENT 实体/Mob/移动 | ~~V2-5 新 mob 猪~~ ✅ v2.5.0（门 176） | ~~V2-9 step-up quirk 修复~~ ✅ v2.9.0（门 180） | | | | | |
| SURV 生存/战斗/效果 | | | | | | | |
| CRAFT 物品/合成/面板 | ~~V2-7 材料补齐批~~ ✅ v2.7.0（门 178） | | | | | | |
| SAVE 存档 | | | | | | | |
| PRE 表现层 | ~~V2-4 天气（雨/雷）~~ ✅ v2.4.0（门 175） | ~~V2-10 网格修复批~~ ✅ v2.10.0（门 181：叶子-叶子内部面剔除 cull_same_id + cutout discard 普查确认，base18 重锚 178148 B） | | | | | |
| UX 实机报障/体验细化 | ~~V2-1 第三人称前视角~~ ✅ v2.1.0（门 172） | ~~V2-2 玩家皮肤 Steve 化~~ ✅ v2.2.0（门 173） | ~~V2-3 教程系列化（农耕 + 厨房）~~ ✅ v2.3.0（门 174） | ~~V2-6 体验打磨批~~ ✅ v2.6.0（门 177） | | | |

> **2.x 定案说明（11-03 用户审议；V2-N = 执行序，立项时走设计定案）**：
> - **V2-3 教程系列化（农耕 + 厨房）**（中，门 174）：UX-7 机制系列化——
>   `tutorials/` 多文件按名序 = 系列序 + 存档 format 24→25（`player.tutorial`
>   加 `series` 字段；≤24 迁移默认 series 0；失效仍「重置上报」不拒档）+
>   系列完成自动推进 + T = 跳过当前系列；新系列②农耕（pickup.wheat_1 →
>   craft.farmland → place.farmland → place.wheat_1 → break.wheat_7 →
>   craft.bread → eat；生长 6 级×平均 360 帧 ≈ 36 s 节奏对教程友好；
>   **零新机制** = 全链数据已核实在位）+ 新系列③厨房（break.stone×3 补
>   锻石 → craft.furnace → place.furnace → interact.furnace →
>   smelt.cooked_chicken → eat）；事件词表 +2 前缀 `pickup.` / `smelt.`
>   （均帧锁定；smelt = CRAFT-5 既有帧锁定日志钩子）；新增字体字在 424
>   预算内零代码。既有 11 步基础链**零改动**。
> - **V2-4 天气（雨/雷）**（中大）：帧锁定粒子雨（P66 余烬先例）+ 雷声
>   （SoundBank 既有空间化）+ `render.weather` config；交互全走既有系统：
>   雨灭火/灭营火（水浇灭规则既有）、雨湿润耕地（`soilIsWet` 扩展雨水源）。
> - **V2-5 新 mob 猪**（小）：纯数据驱动（Mob JSON + 纹理，**V2-2
>   per-face 消费** = 猪脸）+ 新食物 pork 生/熟 2 件 + 掉落；验证「加 mob =
>   JSON + 纹理」数据驱动承诺。
> - **V2-6 体验打磨批**（小中，UX-8/9 同款多格批）：第一人称手臂
>   viewmodel（「无手部皮肤」记档消费）/ mob blob 阴影（记档消费）/ 物品
>   拾取磁力（「无磁力」记档消费）/ 月相（billboard「无月相」记档消费）。
> - **V2-7 材料补齐批**（小中）：planks 可放置化（现状 planks = item-only
>   不可铺地 = MC parity 缺口）+ gold_block / diamond_block（信标配「仅
>   iron_block」记档消费）+ spider 掉 string / spider_eye（「spider 无掉落」
>   记档消费）+ 块级 per-face 纹理（V2-2 MobBox `faces` 范式搬入
>   BlockModel → 炉子「四侧面共用纹理」记档消费）。
> - **V2-8 sky rescan clobber 修复**（小中）：LGT-1 记档潜伏 bug（天花板
>   骑跨光照 footprint 边界 → stale-dark 暗带）；修复形态两选一并已记档
>   （sky BFS 加 shell seed 推荐；union 单遍 = LGT-1 已砍除形态）。
> - **V2-9 step-up quirk 修复**（小）：记档「单独立项」两 quirk——+X/+Z
>   方向恰 1 格窄沿踏不过 / 非整数脚高凭空抬升 1.0 再落回；修复 = player
>   行为变更，既有单测钉死回归。
> - **V2-10 网格修复批**（小，门须重锚论证）：叶子-叶子内部面（剔除 v2 =
>   「邻居同种方块」记档）+ alpha cutout discard（shader 一行形记档）；
>   演示区叶冠/花可视范围 → base18 重锚或结构性免疫论证。

## 执行顺序

1. **V2-N = 执行序号（用户定案序）**：V2-3 → V2-4 → … → V2-10；格按系统
   归属落行（行内格序 = V2-N 序）。1.x「从上到下逐行」适用于整表预定；
   2.x 逐项由用户定案，执行按 V2-N。
2. 一任务一 phase，走标准阶段流程（设计定案 → 实现 → 构建+测试 → E2E →
   文档 → 提交 → 报告；详见 AGENTS.md）。
3. 版本号逐版 bump（自 v2.3.0 起：CMakeLists PROJECT_VERSION 单点 +
   test_debug_overlay F3 钉意识更新 + 构建后 banner 版本实证，PITS 355）。
4. **每版发布前必做性能 A/B**（用户硬要求），旧侧 = 上一 release tag
   （2.x 现仅 v2.0.0 有 tag）或指定 commit（v2.1.0 = f3dc85e / v2.2.0 =
   d9d62b0，暂不挂 tag 按 commit SHA 引用）。
5. 2.x tag/release 政策**未定**（v2.1/v2.2 暂不挂 tag）= 立项时问用户
   （1.x 政策参照 `docs/archive/v1x/BACKLOG.md` 执行顺序 #3）。
6. 表外 3.x/4.x 方向项（`docs/BACKLOG_3x.md` / `docs/BACKLOG_4x.md`）**不
   是本表执行项**；跨表只经 2.x 收官晋升流程，不跨表编号。

## 表外

- **3.x / 4.x 路线图（方向底稿，非本表执行项）**：3.x「系统扩展」期 8 项
  （XP / 附魔 / 床 / 蝙蝠 / 染色羊 / 雪原 biome / 顶部 slab / AO）=
  `docs/BACKLOG_3x.md`；4.x「世界扩展」期（下界维度锚 + 终局方向，条目
  3.x 收官时定）= `docs/BACKLOG_4x.md`。
- **无界项（矩阵外，立项需用户定案）**：新内容开放空间（方块/物品/配方/
  mob 可无限加）；netherite 材料链（依赖 4.x 下界）；末地 / 创造 / 多人
  等大方向。经验（XP）/附魔 = 3.x V3-1/V3-2 已立项化（方向底稿），移出
  本条。
- **已接受偏离（非任务）**：继承 1.x 清单（见
  `docs/archive/v1x/BACKLOG.md` 表外节：锻石不可熔回、箭不自伤、实体 age
  墙钟等）；2.x 各版自身偏离记档见 `docs/archive/v2x/phase-2.x.0.md`「偏离」
  节。
- **不立项**：PIPE-4 跨 chunk 合并（99.x 数据不支持，用户确认，见
  `docs/archive/v1x/BACKLOG.md` 表外节）。

## 变更记录（v2.x）

- 2026-11-03：**V2-10 网格修复批完成（v2.10.0，门 181 全 PASS）**：
  渲染节两记档修复（**2.x 收官前最后一项**）——① 叶子-叶子内部面
  剔除 = 块属性 `cull_same_id`（严格布尔，leaves 族 3 块首消费，
  shipped 恰三零影响钉）+ mesher greedy pass 同 id 剔面（纹理
  选择前 merge 不见；glass-glass/水规则/cross 族红线零触碰；
  单测几何钉 24v=6 quads vs 32v）② alpha cutout discard = **零
  代码确认**（PRE-5 桶无关 discard 全覆盖实证：普查 21 个 0 孔
  纹理 = 20 纯 cutout + glass blend 已覆盖；+ 普查单测钉防未来
  漏检，KNOWN_ISSUES 29 条划解）。2181/2174+7、零 format。门
  181：S1 **base18 重锚论证**（Phase 18 先例：旧侧 ≡ 现锚
  179116 B 环境干净 / 新侧 ×2 178148 B / diff 4185 px 全叶冠区
  天空·UI 零泄漏 / 2681 px 邻叶背面→天空 = 孔透视修复实证 /
  **新锚 178148 B 采用**）+ S2 + S3 + S6 点名 + 性能 A/B。详见
  docs/archive/v2x/phase-2.10.0.md。

- 2026-11-03：**V2-9 step-up quirk 修复完成（v2.9.0，门 180 全
  PASS）**：KNOWN_ISSUES 203/382 条「单独立项」两 quirk 定向消除
  （player/mob 共享 `Entity::stepUp`，ENT-3 探针实证）——① +X/+Z
  恰 1 格宽台阶踏不过 = front 格 `snapped+1` 不对称（+dir 查贴住格
  之后那格）→ 修 `front = snapped + (w > 0 ? 0 : -1)`；② 非整数
  脚高（slab 顶 1.5 / 楼梯斜面）face 巧合对齐整数 → 凭空抬升 1.0
  再落回（调用点无条件 + 位置判非因果判，mob 追逐 frame 606
  实证）→ 修 = **无状态真阻挡验**（front 列身体 y 跨内 ∃ solid 格
  顶 > feet 严格不等式；零新帧状态/零调用点改动）。整数脚高位级
  不变红线（既有 step-up 单测族零改动全过）。TDD：3 新测试 pre-fix
  FAIL（x/z 9.7 clamp / maxY 2.5）后 PASS + 既有 mob +X 单格钉
  反转；2174/2174（+3）。门 180：S1 第 180 次结构免疫（四方
  179116 B + savenorm EXACT）+ S2 台阶 E2E（自建平台 1 格台阶：
  旧树 x15.700 ×2 精确 clamp 实机命名复现 vs 新树踏过 x25.1）+
  S3 + S6 命名回归。详见 docs/archive/v2x/phase-2.9.0.md。

- 2026-11-03：**V2-8 sky rescan clobber 修复完成（v2.8.0，门 179 全
  PASS）**：LGT-1 记档潜伏 bug 定向消除（KNOWN_ISSUES「光照」条划解）
  ——天花板板骑跨光照 footprint 边界时，后一 region 的 sky rescan 把
  边界格 sky 重写为纯垂直值（opaque 下 0 / LGT-2 衰减冠下部分值 <
  真值），sky BFS 种子只取 footprint 内 lit 格（开敞源列在境外）→
  水平光流无法恢复 = stale-dark/stale-partial 暗带（1-14 格宽，流送
  drain 序触发）。修复 = 记档两选一中的 **sky BFS 加 shell seed**
  （union 单遍 = LGT-1 已砍除形态不复活）：updateRegion 天空种子盒
  从 footprint 扩为 +1 块 shell 环（rescan 仍 footprint-only、BFS 写
  界 inRegion 不变、no-op skip/propagateAll/块光零改动；shell 格持
  收敛值 = 恢复边界条件；无 clobber 时位级不变）。**独立性判据零改
  动**（算术实证：标准列盒 shell 读列 c±2 = 99.10 文档化 1-chunk
  边界读环，kLightParallelColumnGap 与 confluence 证明原样成立；
  light_job.hpp 零变更）。TDD：2 回归测试先在 v2.7.0 树证 FAIL
  （stale-dark 2464 diff 格 / stale-partial 544 diff 格，清点与设计
  预测精确吻合）后修复 PASS；2171/2171（+2）。门 179：S1 第 179 次
  结构免疫（**锚重启后重锚 179116 B = 679×744 WM tile 态**——/tmp
  丢失后从 v2.7.0 净树重导 ×2 恒等，字节数与原锚吻合 = 连续性；
  PITS 375 tile 几何协议 + dummy 暖机吞会话首窗 floating 例外）
  + S2 clobber 判别（CPU 探针旧库梯度 14..1→全 0 命名
  复现 / 新库完整保持 + E2E slab 无输入流送：板 x25..63 西区 x32..38
  旧树暗带 vs 新树恢复）+ S3 + S6 命名回归 + 性能 A/B 同世界同成本。
  环境事件：机器重启 /tmp 重建 + **XTEST 键盘恢复**（PITS 368 根治
  实证 = X 会话重启）。详见 docs/archive/v2x/phase-2.8.0.md。

- 2026-11-03：**V2-7 材料补齐批完成（v2.7.0，门 178 全 PASS）**：四子项
  均数据驱动（核心唯一新增 = 块级 per-face `faces` 通用能力，
  **无 faces = 位级不变红线**），**零 format（25 不动）/ 零 config
  新键**：① **planks 块化**（「material 不可铺地」MC parity 缺口
  消费）= 新 blocks/planks.json 复用图集既有 planks tile @ [2,4]
  零新纹理 + fuel_time 7.5 迁块属性（值不变）+ 删 items/planks.json
  【同名显式文件 = init 点名拒「duplicate item id」S6 实证】+ 8 配方
  全按名零改动（单测钉）+ 图标 = 3D 方块体记档偏离（wool 先例）；
  ② **gold_block/diamond_block**（「信标基座 = 仅 iron_block」消费）
  = iron_block 先例复制 + 9 锭 shaped 配方 ×2 + beacon_base: true
  入列（等级仍是 world/beacon 纯函数零核心代码；S2 = gold 3×3 基座
  光束 vs stone 对照 + 旧侧载 gold 场景 init 拒 = 可载性互斥 + CPU
  探针 level 1/0）；③ **spider 掉 string ×0-2 + spider_eye ×0-2**
  （「spider 无掉落」消费）= 既有 count_min/max 区间机制 + 2 新
  material 物品 + **纯收集记档偏离**（弓配方用 planks 不用 string；
  S2 = 夜间 scratch spider-only 场景击杀 E2E，单跑 kill 2 只两组
  掉落各自 0-2 区间 + 复跑逐位恒等）；④ **块级 per-face 纹理**
  （「炉子四侧面共用同一纹理」消费）= BlockModel.textures 可选
  faces 段（front/back/left/right 外法线定名，front=(0,0,−1)；
  严格解析未知键/非串/空串/cross·fence·glass_pane 形状全点名拒；
  mesher 10 调用点方向感知 = greedy merge 自动断并；首个内容
  furnace front 单 tile，**lit 态 = 既有炉口火焰粒子记档偏离**；
  S2 首跑踩面名约定坑 = 从 +Z 侧看 back 面三图恒等零 diff，
  改 −Z 侧 yaw 3.0 后 diff bbox 112×107 画面中心 = 前脸纹理区）。
  图集 102/128 纯 +12 行零移位、字体 423 零新字、既有 184 纹理
  PNG md5 零变化（5 新 1 删）；2169/2169（+12）；门 178：S1
  第 178 次结构免疫（四方 RAW + 锚 179116 B + savenorm EXACT）+
  S2 四子项判别全 PASS + S3 + S6 双点名（unknown face 'weirdface'
  / duplicate item id 'planks'）。门协议发现 PITS 373（敌对生成
  双门 = 生成保护 12 块 + 夜间被动 cap 挤占 → scratch 资产
  spider-only）/ 374（hold-swing 帧预算 + capture 时延锚 +
  块级 faces pose 面名约定 + symlink 进程名脱管）。详见
  docs/archive/v2x/phase-2.7.0.md。

- 2026-11-03：**V2-6 体验打磨批完成（v2.6.0，门 177）**：四子项各消费
  一处记档偏离，全为表现层/手感层，**零 format（25 不动）/ 零 config
  新键**：① **第一人称手臂**（「无手部皮肤」消费）= 玩家数据模型
  `arm_r` box re-center 走既有 box-model 管线（Steve pa_arm tile 六面
  全贴**零新纹理**，tile 下行 = 肤手；init 校验 `arm_r` 存在；门控 =
  锁定 ∧ 无面板 ∧ First 三态矩阵；姿态 = 手持 offset 平移继承 →
  弓拉/进食携臂微动；静态无行走摆动 = 记档偏离）；② **mob blob 阴影**
  （「无 blob 阴影」消费）= 实体脚下世界空间扁 quad（双面半透明黑
  alpha 0.3，边长 1.5×模型宽，中心 = 底心正下方底格顶面 + 0.01 防
  z-fight；blend pass 深度写关测开；玩家仅第三人称画 = MC 观感；纯
  渲染零帧状态）；③ **拾取磁力**（「无磁力」消费）= 延迟过后
  kMagnetRadius 2.5 内**帧锁定**吸向玩家（位移 = f(距离, 帧) 纯函数：
  径向 6×(1−d/2.5)² 加速趋近 + 24 帧周期 swirl 收紧螺旋；掉落物
  hasRoomAt 不穿墙，**插墙箭同款拉出消费**）；④ **月相**（「无月相」
  消费）= 8 相 = day cycle 累计天 index（帧锁定派生）% 8，既有月 quad
  叠暗色食相 quad（同 28px 圆盘滑移，偏移 = 相序纯函数
  `moonPhaseOffset`；**绘制序 = 食相先/月后**——r=800 平面 24-bit
  深度只分辨 ~160 块，亚块 nudge 无效【PITS 371】；零新纹理、图集
  99 零移位）。2157/2157（+22：月相 5 / 磁力 9 / 阴影 3 / 手臂 5）、
  -Werror 0、banner v2.6.0。基线自证：60f 无输入截图 = base18 锚
  179116 B **逐字节恒等**（四子项全惰性：locked-only / 无 mob 零绘制 /
  无掉落物零执行 / 正午月在地平线下整分支不执行）。详见
  docs/archive/v2x/phase-2.6.0.md。

- 2026-11-03：**V2-5 新 mob 猪完成（v2.5.0，门 176 全 PASS）**：
  「加 mob = JSON + 纹理」验证 = 零核心代码——pig.json（passive 10 HP /
  drops pork ×1-2 + leather ×0-1）+ 宽体 6-box 模型（4 腿对角 swing，
  head 嵌入 body cow 范式，64 相位 clip 验收）+ **head faces.front =
  pig_face = V2-2 per-face 纹理首个 mob 消费**（显式 8×8 猪脸零
  hash）+ pig_body 新 salt 纹理（既有 176 文件零变化 cmp 实证）+
  pork→cooked_pork 食物链（+3→+8 beef 熟同款刻度，无新配方）+ 生成
  权重 6→7 + 图集 97→99 零移位（99 条全 census 钉）+ 字体 422→423
  猪（顶 512 行不变免重锚，424 预算剩 1）。2135/2135（+7）。门 176：
  S1 第 176 次结构免疫（n1≡o1≡m1 跨版 + mobs-on ≡ off = 扫描
  render-inert + 锚 + savenorm EXACT）+ S2（生成序列 ×2 恒等 + 旧侧
  零 pig 互斥 / 杀猪链全存档权威【离线 f210 粉簇反投影解 KILL pose +
  pork ×1 掉落区间钉 + 两段接力拾取/熔炼/吃 hunger 14→20】）+ S6 点名
  2/2 + 性能 A/B 同世界同成本（9/3/1 逐位 + 18/18 55/55 恒等 + 猪自
  成本同档）。门内环境事故 PITS 368-370（XTEST 键盘服务器级失效——
  **后续输入门前须探测/修 X 会话** + drawin 窗盖中央点击 + mob 不
  持久化）。详见 docs/archive/v2x/phase-2.5.0.md。
- 2026-11-03：**V2-4 天气（雨/雷）完成（v2.4.0，门 175 全 PASS）**：core/weather 纯状态机（推进式 splitmix64 PITS 4，主流 = 世界
  seed + 雷独立 salt；CLEAR 9000-18000f / RAIN 3000-6000f / 雷 600-1800f；
  首段恒 CLEAR = 60f 基线免疫根）+ rain shader program（cloud 第四先例：
  静态 300 滴 VBO + 顶点逐滴纯函数位置 + per-frame 零上传）+ 天空
  dayFactor 单收口（RAIN ×0.85 + 雷 5 帧 +0.15 脉冲，clear/雾同源）+
  交互四件（雨灭火/灭营火 = extinguishedByWaterOrRain 谓词、灭燃烧效果
  玩家+mob inWater ∨ raining、润耕地 soilIsWet 扩参默认 false 位级不变
  雨中生长 2×）+ render.weather strict read 默认 true（off = 位级恒等）+
  --weather CLI（初始 state 覆盖，kRainMin 固定）+ F3 weather 行 + 雷
  新 wav（固定种子，既有 39 md5 零变化幂等实证；无雨声循环记档）+ **零
  format**（25 不动）。2128/2128（+19）、-Werror 0、banner v2.4.0；预门
  实证 60f 无输入 = base18 锚四方 md5 恒等。详见
  docs/archive/v2x/phase-2.4.0.md。

- 2026-11-03：**V2-3 教程系列化（农耕 + 厨房）完成（v2.3.0，门 174 全
  PASS）**：tutorials/ 多文件名序 = 系列序（beginner 11 步零改动 +
  farming ×2 修订 7 步 + kitchen 6 步，全链数据核实在位零新机制）+
  format 24→25（三向键纪律，≤24 值保真免迁）+ 事件词表 +pickup./
  smelt.（帧锁定）+ T = 跳当前系列 + 系列过渡无 toast；门内抓到并
  修复 UX-7 潜伏 bug = recipe 行合成漏发 craft 事件（PITS 360）。
  2109/2109（+25）、PITS 359→367。详见 docs/archive/v2x/phase-2.3.0.md。

- 2026-11-03：**2.x 任务定案（V2-3~V2-10 八项）+ 3.x / 4.x 路线图建表（用户
  11-03 定案）**：2.x = 「上手与体验补全」期（V2-3 教程系列化农耕+厨房 /
  V2-4 天气 / V2-5 猪 / V2-6 体验打磨批 / V2-7 材料补齐批 / V2-8 sky
  rescan clobber / V2-9 step-up quirk / V2-10 网格修复批，条目说明见矩阵
  表下）；3.x「系统扩展」期 8 项 + 4.x「世界扩展」期方向底稿建表 =
  `docs/BACKLOG_3x.md` / `docs/BACKLOG_4x.md`。纯文档无版本 bump。

- 2026-11-02：**2.x 任务矩阵搭建（用户定案）**：按 1.x 同款格式 = 10 核心
  系统行（STRM/PIPE/LGT/GEN/FLUID/ENT/SURV/CRAFT/SAVE/PRE）+ UX 行，①–⑦
  格；V2-1/V2-2 落 UX 行 ①/②，其余格留空待定案；执行顺序/表外节继承 1.x
  政策（详见 `docs/archive/v1x/BACKLOG.md`）。纯文档无版本 bump。

- 2026-11-02：**V2-2 玩家皮肤 Steve 化完成（v2.2.0，门 173 全 PASS）**：模型 per-face 纹理（既有 mob 位级不变）+ Steve 脸/衫/裤/靴 7 tile（图集零移位免重锚）。详见 docs/archive/v2x/phase-2.2.0.md。

- 2026-11-02：**V2-1 第三人称前视角完成（v2.1.0，门 172 全 PASS）**：2.x 首个功能项——前视 = 后视同款数学在翻转角下执行（纯函数零改动），C 键三循环，瞄准 aimReach D+kReach。详见 docs/archive/v2x/phase-2.1.0.md。

- 2026-11-02：1.x 文档归档 `docs/archive/v1x/`（181 份 phase 文件 git mv 保历史 + 全仓引用路径同步修正）+ 本表 v2.x 占位新增。

> **11-03 用户定案：原 3.x「系统扩展」期八项并入 2.x**（3.x 条目仅 8 个，
> 并入 2.x 续编号 V2-11~V2-18，版本 v2.11.0~v2.18.0）；**原 4.x「世界
> 扩展」保持大版本**（条目皆大任务，2.x 二批收官后本 4.x 表晋升执行
> 权威，版本 v4.x）。2.x 第一批（V2-1~V2-10，门 171-181 全 PASS，
> tag v2.10.0）完成表沉 `docs/archive/v2x/`。
>
> 本表八项 = 原 `BACKLOG_3x.md` 方向底稿晋升（条目内容零改动，仅
> V3-N → V2-N 重编号）；细节立项时设计定案。**用户已授权按 V2-11
> 起顺序推进**（11-03「继续推进原 3.x 转到 2.x 的 8 个任务」）。

## v2.x 第二批任务矩阵（8/8 完成，11-04 收官）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ | ⑦ |
|---|---|---|---|---|---|---|---|
| STRM 流送与内存 | | | | | | | |
| PIPE 帧管线与并发 | | | | | | | |
| LGT 光照 | | | | | | | |
| GEN 世界与生成 | ~~V2-16 新 biome：雪原/针叶林~~ ✅ v2.17.0（门 188：snowy_taiga 数据驱动【tree 6 键严格对象 + exclude_spawn_area 演示区结构保护 = 区内映射逐位不变】+ snow 满块 + planTree 针叶路径【无 tree biome 零触碰】；零 format 世界演进，结构免疫第 69 次免重锚） | ~~V2-17 顶部 slab/双 slab~~ ✅ v2.18.0（门 189：stone_slab state_count 3【底/顶/双满格】+ drops_count_by_state [1,1,2] 破掉 2 MC 材质守恒 + 放置瞄准规则 slabStateFromHit + 同格互补合并 + solidRange 物理区间 + partial-aware slab 射线【合并可达根】；零 format bump，结构免疫第 70 次） | | | | | |
| FLUID 流体 | | | | | | | |
| ENT 实体/Mob/移动 | ~~V2-14 新 mob 蝙蝠（飞行 AI）~~ ✅ v2.15.0（门 186：悬停巡航飞行 AI【正弦 bob 帧锁定 + 航向流重掷】+ 洞穴生成 in-air 分支【光 ≤3 跳过 floor 带任何 Y】+ 0.9×0.5 展翼扇翅模型 + squeak 氛围音 + 三数据旗互配纪律；零 format savenorm EXACT，结构免疫第 67 次） | ~~V2-15 16 色染色羊~~ ✅ v2.16.0（门 187：shears 238 耐久 + 剪毛 1-3 彩毛 + 出生毛色流首抽 temperate 表 + 染色改色 bare 拒染 + 死亡彩毛钩子 + per-entity 变体网格【15 色复用 wool 族 tile + sheep_skin】；零 format savenorm EXACT，结构免疫第 68 次） | | | | | |
| SURV 生存/战斗/效果 | ~~V2-11 XP 系统~~ ✅ v2.12.0（门 183：经验球 1..9 无合并分裂 + 全整数 MC 曲线【等级派生不存储】+ 三发放源数据驱动【hostile 5/passive 2/mine coal 1 diamond 5 iron 0 gold 0/smelt 1 统一】+ format 26 三向键 + save.cpp PITS 361 潜在 bug 修复 + XP 条 HUD；base18 第 4 次重锚 179027 B） | ~~V2-13 床（睡觉跳夜）~~ ✅ v2.14.0（门 185：15 色染色族 Slab 床 + 睡眠三门【距离 3.0/hostile 8×5 盒/夜窗】即时跳夜 + 天气重置 + format 28 respawn 恒写 + 验床重生【破坏回退世界出生点】+ sleep 音；结构免疫第 66 次免重锚） | | | | | |
| CRAFT 物品/合成/面板 | ~~V2-12 附魔~~ ✅ v2.13.0（门 184：7 附魔数据驱动【锋利/保护/效率/耐久/力量/速度/无限，MC 精确区间+权重】+ 附魔台/书架 2 块 2 配方【lapis 代 book 偏离】+ 确定性 offer 全整式【种子 = world^hash3(台位)^ability^xpSeed，仅面板开时求值】+ XP 整级扣减余量保留 + format 27【xp_seed hex + enchants 三处，四拒】+ 面板 furnace 范式 + 字体 +4 字【锋利限魔】；结构免疫第 65 次免重锚） | | | | | |
| SAVE 存档 | | | | | | | |
| PRE 表现层 | ~~V2-18 AO 环境光遮蔽~~ ✅ v2.19.0（门 190：经典 MC 顶点 AO 纯函数族 world/ao【3 邻格共享边查询 + opaque∧高度带闭区间 + 4 档 MC 精确系数 0.3 双邻特例】+ bake 入既有光 attribute【零 shader/零顶点格式/零 uniform】+ MaskCell 合并键扩 ao[4] 断 run + slab/stairs 发射 y 判带 + fluid/cross/wall/fence/pane 免 AO【MC 精确 + 偏离 4】+ render.ao 严格 bool 默认 true off=位级恒等；4 偏离 + fixer 3 定案；**二批收官**） | | | | | | |
| UX 实机报障/体验细化 | （开放：2.x 二批实机报障入此，V2-N 续编号 V2-20+） | ~~V2-19 视角/移动/时间显示批~~ ✅ v2.11.0（门 182：滚轮缩放【默认参数位级不变 + aimReach 联动】+ 双击 Space 飞行【1.6× 速 + 坠伤归零 + 水切出记档】+ 24h HUD 时钟【ramp 边精确对齐映射】；base18 第 3 次重锚 178676 B，全会话态零 format） | | | | | |

> **条目说明（11-03 用户审议原文；细节立项时设计定案）**：
> - **V2-11 XP 系统**（大）：经验球（实体 + 拾取）+ 玩家等级/XP 条
>   （HUD）+ 发放源（炉子熔炼【消费「无熔炼经验球」记档】/ mob 击杀 /
>   破坏）+ format；晚游戏循环第一步。
> - **V2-12 附魔**（大）：消费「附魔台替身」三处记档（master_bow /
>   信标 / 弓强化）；项目范式定案 = 真附魔台（确定性 roll，帧锁定
>   salt）vs 强化物品扩展，立项时定。
> - **V2-13 床**（中）：数据块 + 睡眠通道（夜间右键）+ 跳到黎明 +
>   重生点更新（R 重生流既有）；消费「v1 无床」两处记档（保护环 /
>   和平难度）。
> - **V2-14 蝙蝠**（中）：飞行 AI = 新 AI 型（洞穴生成）；模型 + 纹理
>   数据驱动（V2-2 per-face 消费）。
> - **V2-15 16 色染色羊**（中）：消费「sheep 只掉白 wool」记档；mob
>   持色（per-instance 状态 vs mob 数据，立项时定）+ 16 色 wool 掉落。
> - **V2-16 新 biome：雪原/针叶林**（中）：biome JSON + 雪面 + 装饰；
>   旧存档 = 世界生成演进先例（GEN-1/3/5）；新 biome 避开演示区 =
>   结构性免疫设计（熔岩湖先例）。
> - **V2-17 顶部 slab/双 slab**（中）：消费「slab = 仅下半块」记档；
>   ENT-2 `state_count` 机制现成（朝向 2 值）；剔除/mesh/光照（LGT-5
>   先例）三点适配。
> - **V2-18 AO 环境光遮蔽**（中大）：消费「按面采样光照，无 AO」记档；
>   mesher 顶点 AO + shader，经典 MC 块状 AO 观感；门须重锚（世界
>   光照视觉全变）。
> - **方向项（未编号）**：死亡统计屏 / Game Over 屏（1.x 记档范围外；
>   SURV-7 统计机制既有，纯呈现）——按 11-03 定案归 **4.x**。

## 执行顺序（第二批生效）

1. V2-N = 任务编号（2.x 续编号；实机报障项入 UX 行续编号
   V2-19+）。**版本号 = 执行序**（v2.11.0 起逐版 bump，CMakeLists
   单点）；任务号与版本号解耦（V2-1/V2-2 同款先例）。用户实机
   诉求可提前执行（11-03 V2-19 先于 V2-11 执行 = v2.11.0）。
2. 一任务一 phase，走标准阶段流程（设计定案 → 实现 → 构建+测试 →
   E2E → 文档 → 提交 → 报告；详见 AGENTS.md）。
3. 版本号逐版 bump（**v2.11.0 = 首个二批版本**；CMakeLists
   PROJECT_VERSION 单点 + test_debug_overlay F3 钉意识更新 + 构建后
   banner 版本实证，PITS 355）。
4. **每版发布前必做性能 A/B**（用户硬要求），旧侧 = 上一版本 commit。
5. tag/release 政策沿用 2.x 收官定案（v2.10.0 = 第一批系统收官 tag；
   二批收官 = v2.18.0 系统收官 tag + release，用户可推翻）。

## 变更记录（v2.x 第二批）

- 2026-11-04：**V2-18 AO 环境光遮蔽完成（v2.19.0，门 190 = 二批收官）**
  ——PRE 行，消费「按面采样光照，无逐顶点插值/环境光遮蔽」记档 [Phase 7]。
  MC 机制 11-04 wiki 核定（Light 页 smooth lighting 节 + Options 页默认
  ON）。**纯函数族 world/ao**（viewer 侧 C+n 基 3 邻格 = 与顶点共享边的
  前方三格【e 向 = 背离网格单元于顶点——设计稿「away from vertex」括注按
  MC 经典观感三场景实证定案 fixer 点 1】× opaque ∧ 高度带闭区间【剔面同
  opaque 旗项目自洽规则：满块/stairs/wall/fence [0,1]（偏离 2 斜面不精
  化）/slab 0,1,≥2 = solidRange 同款 kSlabHalf 常量；半块同款 MC「顶点
  只被同高度邻格遮」= 中平面恒不遮自动成立】× 4 档 MC 精确系数 1.0/
  0.75/0.5/0.3【side1∧side2 双邻特例 0.3 非计数 2】）；**bake 入既有
  skyLight/blockLight attribute**（零 shader/零顶点格式 15 float/零新
  uniform/per-draw 覆盖零触碰 = 偏离 3 覆盖面 AO 消失）；**生效面** =
  greedy 满块 + **MaskCell 键扩 ao[4]**（uint8 档位精确比较 = AO 异断
  run，merged 顶点取端 cell = seed.ao[i]【键强制全等】ao[4] 序 = 面表
  c0..c3 = 1×1 发射序单测钉）+ slab 三态 + stairs（斜面 = +Y 面规则
  fixer 点 3）；**fluid/cross 免 AO（MC 精确）+ wall/fence/pane 免 AO
  （偏离 4）= 豁免 6 顶点写点零触碰**；快照零新依赖（⊆ 3×3 列）；
  **render.ao 严格 bool 默认 true**（非 bool 点名拒 = S6；config.json
  不动）off = 查询不跑 + 系数恒 1.0 = pre-V2-19 位级恒等。2482/2482
  （+27）、banner v2.19.0、format 28 不变。fixer 自证三件（报告编排者
  前）：① off 60f ≡ 旧 v2.18.0 二进制同环境**逐字节**（本环境锚 179027
  旧二进制亦不复现 = 云/XP 条环境态漂移与 AO 无关 → 相对口径为位级
  根）② on ×2 恒等 224764 B ③ on-vs-off diff 36462 px 单调变暗审计
  【99.83% 逐通道 new≤old+1.5；61 px = run 断裂 z-fight 换面确定性类
  别 y 231..399；HUD/准星/天空 0 diff；bbox 全世界区】。坑位 4 条（e+1
  越界栈破坏 -O0 静默错值 / 面表 edgeA-B 轴不对齐 k 位取轴跨轴错配 /
  世界坐标手算 7 连错 / 锚环境漂移相对口径）。详见
  docs/archive/v2x/phase-2.19.0.md。**二批收官动作**（门 PASS 后编排者）：
  tag v2.19.0 + GitHub release（8 项总结）+ 4.x 晋升定案（BACKLOG_4x →
  执行权威）+ 二批文档沉 archive/v2x/ + AGENTS 收官状态。

- 2026-11-04：**V2-17 顶部 slab/双 slab 完成（v2.18.0，门 189 全
  PASS）**——GEN 行，消费「slab = 仅下半块」记档 [Phase 85]。
  stone_slab state 化（state_count 3：0 底部既有位级 / 1 顶部 y
  0.5..1 天花板贴放 / 2 双 slab 满格）+ drops_count_by_state [1,1,2]
  严格字段（双 slab 破坏掉 2 = MC 材质守恒；长度==state_count/零/非
  整/无 state 携带全点名拒）+ solidRange 物理区间扩展（{bottom,top}：
  0→{0,0.5} 位级 / 1→{0.5,1} / ≥2→{0,1}；床/stairs 现值位级红线，
  顶 slab 顶面恰 1.0 可踏升 + mob 生成 spawnFloorTop）+ emitSlab 三
  态（state 0 逐语句未动 / 顶镜像底面恒绘 / 双满格标准剔，零新剔除
  规则）+ 放置 slab 规则（:1173 扩展点：顶面→0 位级根 / 底面→1 / 侧
  面 local-y>0.5→1，slabStateFromHit 纯函数）+ 同格同 id 互补 state
  合并 → 双 slab + 蹲旋转 Slab 形排除（偏离 4）+ **partial-aware
  slab raycast**（门机缺口补口：cell DDA 全遮挡 → 共享 dda 子格判
  定，空气半穿透 + 内表面 ±Y 命中，5 块射线消费点 + 桶 + LOS + 第三
  人称共享一致 = 合并 E2E 可达根；记档行为变更 = 瞄准底 slab 空气半
  破身后块 MC 同款）。4 条偏离（双 slab 单格 state 表示 / 跨型合并不
  做 / 顶 slab 全挡无 sneak 缩高 / 蹲旋转排除）。2455/2455（+35）、
  banner v2.18.0、零 format bump（states format 15 机制）。门 189：
  S1 结构免疫第 70 次免重锚（≡ 锚 179027 零 diff + savenorm EXACT）/
  S2 三臂×双侧（顶 slab / 合并 state 2【旧侧 cell DDA 叠 2 格无
  state 互斥】/ 双 slab 破 2【旧侧 1】hold 3.3s 断点 + 2.83>磁力
  2.5 留地）/ S3 / S6 6/6 / 性能同档（9/1/3 逐位 + states 全集合恒
  等强口径 + 3 slab 自成本同档）。PITS 383。详见
  docs/archive/v2x/phase-2.18.0.md。

- 2026-11-04：**V2-16 雪原/针叶林 biome 完成（v2.17.0，门 188 全
  PASS）**——GEN 行。snowy_taiga.json（weight 2 / surface snow /
  tree_density 0.12 / height_scale 1.1 / **tree 严格 6 键对象**
  {spruce_trunk, spruce_leaves, 6, 3, 2, spruce} = sapling 同款面
  缺/多/词汇全点名 + create 交叉检 / **exclude_spawn_area = 演示区
  结构保护**：区内累计走查跳旗 → totalWeight 恒 10 = 现有映射逐位
  不变【GEN-1 spawn_clear 先例】）+ snow 满块（hardness 0.1 MC 精确
  / 新 snow 三件音族 / atlas 122 零移位；MC 1-8 层雪层 = 偏离 1）+
  placeTrees 针叶路径（同 kSaltTree h 复用 + planTree 锥冠逐 cell +
  root no-op 既有同款；**无 tree biome 硬编码路径零触碰**）。4 条偏
  离（满块代层 / 无 decorations / density 0.12 / weight 2）。**音效
  缺陷族 13 件随版修复**（generate_sounds LCG 逐采样重建 = 常值流
  → 4 静音 + 5 DC 退化 + 2 部分受损跨多版本潜伏 = 音频门盲区；修后
  峰值全非零 + 纹理度量落正常带，md5 变更恰 13 件幂等）。2415/2415
  （+23）、banner v2.17.0、零 format（biome 不入档，旧档 = 区外雪带
  世界演进 GEN-1/3/5 先例）。门 188：S1 结构免疫第 69 次免重锚（设
  计重锚预判被证据推翻：60f 流送半径内可见地形全落演示区 → ≡ 锚
  179027 零 diff + savenorm EXACT + 区内位级双钉）/ S2 雪块破/放单
  准星双动作 E2E（破 (2,54,2) → 同射线续至 (3,53,3) 地顶 = 放置
  (3,54,3)；hold 0.5s 断点时序；掉落 2.83>磁力 2.5 留地；旧侧词表
  互斥）/ S3 / S6 6/6 / 性能同档（9/1/3 逐位 + journal 0/0【世界演
  进 = 参考变化非 journal 口径发现】+ 雪带 1800f 稳态 111.9/112.5
  ms 同档）。PITS 382。详见 docs/archive/v2x/phase-2.17.0.md。

- 2026-11-04：**V2-15 16 色染色羊完成（v2.16.0，门 187 全 PASS）**
  ——消费「sheep 只掉白 wool」记档 [Phase 41]。shears 物品（238 耐
  久/break_speed 2/`shears` 严格旗/shapeless 2 iron_ingot/fast_
  against 整字段省略 = 不加速任何块）+ 剪毛磨损 1；sheep.json
  drops 1-3 wool MC 精确 + **wool_drops/wool_weights 严格对**（白
  850 黑 80 灰 30 棕 30 粉 10 temperate 整数表 = biome 表偏离；
  词表/零权重/空列表/孤字段点名拒，词表拒放 init checkWoolDrops
  交叉检）；实例态（per-mob 流首抽出生色【非 wool mob 零消费位级
  钉】/ bare / 再生 7200f 帧锁定 = 吃草门偏离）；渲染 = 15 色复用
  wool 族块 tile + sheep_skin（atlas 121 零移位）+ **per-entity 变
  体网格**（swingMeshes 范式，几何位级不变，非 sheep 零触碰）；RMB
  链 tryShear（1 ItemEntity 携 roll 活体掉落 + shear.wav 43 + 磨损）
  → tryDyeMob（bare 拒染 MC 同款）全数据旗无物种 switch；死亡彩毛
  纯函数 woolDeathDropName（bare 跳过/非 wool 位级不变）。2391/
  2391（+35）、banner v2.16.0、零 format。门 187：S1 结构免疫第 68
  次免重锚 + savenorm EXACT（≡ 锚 179027）/ S2 剪毛染色 E2E【两
  pass 瞄准 + 弹幕【围盒弃：羊 1.2>1×1；命中窗 = 出生→首次
  wander ~3s；key→RMB ≥0.15s】+ **断言 = DEBUG 日志行权威**：a
  剪毛+damage 1 / b 染红→剪 wool_red+染料耗尽 / c bare 拒染染料保
  留 / o 旧侧零行】/ S3 / S6 6/6 / 性能同档（9/1/3 逐位 + 16/16
  36/36 + 羊群 2 羊双树同位出生 ents 0.0606/0.0600）。PITS 381。
  详见 docs/archive/v2x/phase-2.16.0.md。

- 2026-11-04：**V2-14 蝙蝠完成（v2.15.0，门 186 全 PASS）**——项
  目首只飞行 mob。bat.json 全数据（HP 6/被动/spawn_max_light 3 洞
  穴语义/无 drops/xp 0/三新严格字段 flying+spawns_in_air+
  ambient_sound，flying⇒spawns_in_air 互配点名拒）+ model 0.9×0.5
  身头双翼 4 box 扇翅（swing 反相）+ 8 词表 + bat.png + squeak.wav
  确定性合成（既有资产 md5 全不变）；飞行 AI = runAi 纯新分支
  （def.flying 门控：航向 per-mob 流 2s 重掷 + 正弦悬停 0.2×sin/60f
  帧锁定 + 重力关 + 撞壁航向重掷 + knockback 回巡航 + ambient 音
  帧锁定 roll 空间化），既有 7 mob 族零改动全过；生成 in-air 分支
  （顶向扫描 + 全空气盒 + 光 ≤3 + 跳过 floor 带 = 任何 Y）；消散
  40/倒挂不做/takeoff 音不做 = 偏离 5-7。零 format（savenorm
  EXACT）。2356/2356（+26）、banner v2.15.0。门 186：S1 结构免疫
  第 67 次免重锚（≡ 锚 179027 + EXACT；编排者首跑误用 v2.13.0 树
  = PITS 379① 再证）/ S2 洞穴无输入 E2E（密封暗厅 seed 3355 四
  pick 全 bat：log 孪生逐位同 + 截图 ×2 恒等 + diff 43425 px 全褐
  色蝙蝠纹理泄漏审计零 + old 7 词表零 bat 行形状互斥）/ S3 / S6
  6/6 / 性能同档（9/1/3 逐位 + 16/16 36/36 + 洞穴 ents 0.093/0.089
  = 飞行不比走地贵）。PITS 380。详见 docs/archive/v2x/phase-2.15.0.md。

- 2026-11-04：**V2-13 床完成（v2.14.0，门 185 全 PASS）**——15 色
  染色族 Slab 床（wool 15 色词表同款无 lime；配方 3 同色 wool + 3
  planks MC 精确；纹理 15 tile 零移位；sleep.wav 确定性合成；
  hardness 0.2/cloth 音族）+ 睡眠三门定案序（b 距离 3.0 → c
  hostile 8×5 盒数据旗判据 → 设点 → a 夜窗：夜内即时跳夜
  【setTimeOfDay(kDawnStart) + 雨/雷天气纯重置 CLEAR】/ 夜外仅设点
  = MC 白天语义；b/c 拒全拒含设点 = 偏离 6）+ 重生点 format 28
  【player.respawn int 三元组恒写，三向键纪律 + 27 迁移哨兵 →
  世界出生点】+ R 键 respawn 验床【纯函数 bedRespawnValid：床位有
  任意床族块 + 空间 → 床位，否则世界出生点回退重新钉位 = MC"仅
  respawn 时查"】+ "sleep" 教程词入裸词表。6 条记档偏离（单格
  Slab 代 1×2 / 即时跳夜代 101-tick 动画 / 夜窗略宽 / 整格头位适配
  / mob 炸床不做 / 拒睡含设点）。2330/2330（+22）、banner v2.14.0。
  门 185：S1 结构免疫第 66 次免重锚（≡ 锚 179027 逐字节 + savenorm
  恰 {format 28, respawn}）/ S2 三臂（夜跳 time 0.0796 + respawn
  床格 / 日设点不跳 / mob 拒睡 + hp 22/60 交战实证；旧侧 format 27
  形状互斥）/ S3 / S6 5/5 点名 / 性能 A/B 同档（9/1/3 逐位 + 16/16
  36/36 + 预置床零成本）。PITS 379。详见
  docs/archive/v2x/phase-2.14.0.md。

- 2026-11-04：**V2-12 附魔系统完成（v2.13.0，门 184 全 PASS）**——
  立项定案 = 真附魔台范式（消费三处替身记档：master_bow P82 / 弓强
  化 P77-79 / 信标 P91）。7 附魔（assets/data/enchantments 数据驱
  动严格解析）：锋利 axe +(L+1) 点 / 保护 armor +4L 点共享 20 cap /
  效率 正确工具 +(1+L²) / 耐久 掷骰跳过（工具 L/(L+1)·护甲
  2L/5(L+1) = MC 1.21 精确）/ 力量 bow +L 伤 / 速度 bow +L 弹速 /
  无限 bow 不耗尽；物品 enchant_class+enchantability 严格互配（22
  件，仅基础 bow）+ lapis enchant_material 门；确定性 offer（core/
  enchanting 纯函数全整式：xpBase 三槽成本 + eCost randTri 整式 +
  权重选择循环 = MC 精确；bookshelfCount 15 环双层 cap 15 无视线偏
  离，仅面板开时求值 = 无输入零成本）；新块 附魔台（hardness 5 /
  light 7 / interact "enchant" / full_cube 偏离）+ 书架；配方 ×2
  （lapis 代 book 偏离）；tile 102→104；format 27（xp_seed 16-hex
  无损恒写 + enchants 三处 omit-when-empty + 四拒）；XP 整级扣减余
  量保留（T(30)=1395 → T(24)=828 实证）+ xpSeed 推进；面板
  （furnace 范式：RMB 开 + carry 模型 + 3 offer 行 + 关归还 +
  "craft" 音偏离）+ 字体 +4 字锋利限魔（atlas 2080→2112 顶 512 行
  不变，kCjkGlyphs 427 码点序 re-cell）。2308/2308（+59）、banner
  v2.13.0。门 184：S1 结构免疫第 65 次免重锚（≡ 锚 179027 B 逐字
  节——atlas v-UV 亚 LSB 零像素差；fixer 中间态"需重锚"= 环境伪影）
  + savenorm 恰 {format, xp_seed} / S2 面板 E2E（offer 种子确定 效率
  I 6/III 9/IIII 17 + 附魔应用全断言 + 旧侧 format 26 形状互斥 +
  面板开态 ×2 恒等）/ S3 / S6 12/12 点名 / 性能 A/B 同档（9/1/3 逐
  位 + 16/16 36/36 + 预置台场景零成本）。PITS 378（AABB 重叠顶高 /
  carry 模型 / xdotool --pid / savenorm 官方工具）。详见
  docs/archive/v2x/phase-2.13.0.md。

- 2026-11-04：**V2-11 XP 系统完成（v2.12.0，门 183 全 PASS）**——
  fixer 半成品（43 文件，11-03 中断取消入 stash）11-04 pop 零冲突
  恢复至 v2.11.0 之上 + 编排者修 3 处 WIP 编译破损（test_save.cpp）
  + 修 save.cpp 潜在 bug（PITS 361 违例第二实例：tutorial 白名单
  isV25 = fmt >= kSaveFormat → 字面量 >= 25）。全整数 MC 曲线
  （needed 2L+7/5L−38/9L−158，逆 = 整数二分，等级永不存储）+ 经验
  球 1..9（无合并分裂记档）+ 三发放源（mob 击杀 hostile 5/passive 2
  / 熔炼 smelt_xp 默认 1 统一记档 / 挖掘 mine_xp 现代 MC raw 值
  coal 1 diamond 5 iron 0 gold 0）严格 [0,9] 解析 + format 26
  （player.xp 恒写 + entities type∈{item,xp} 三向键）+ XP 条 HUD
  （热栏下恒可见，零新字形）。2249/2249（+42）、banner v2.12.0。
  门 183：S1 base18 第 4 次重锚论证（diff 2031 px 全落底部 XP 条带
  → 新锚 179027 B）+ S2 三发放源（熔炼 1/击杀 5【W 步行拾取：orb
  距静止玩家 2.54 > 磁力 2.5】/挖掘 1，旧侧 format 25 形状互斥）+
  S3 + S6 四项点名 + 性能 A/B 同世界同成本（9/1/3 逐位 + 16/16
  36/36 恒等 + 熔炼场景同档）。PITS 377（旧侧新格式场景 init 拒 /
  orb 磁力 3D 边界）。详见 docs/archive/v2x/phase-2.12.0.md。

- 2026-11-03：**V2-19 视角/移动/时间显示批完成（v2.11.0，门 182 全
  PASS）**——用户 11-03 实机诉求三项提前执行（先于 SURV/CRAFT 行，
  版本 = 执行序 v2.11.0，任务号与版本号解耦）：① 第三视角滚轮
  缩放（Input wheel 帧态 + session distance clamp 2-8 step 0.5，
  resolveThirdPersonCamera 默认参数 = 既有 16 单测位级不变；
  ThirdFront aimReach = distance + kReach 联动，瞄准射线自眼位
  UX-8g 零触碰）② 玩家飞行（双击 Space 0.3 s real-dt 窗 CRAFT-7
  先例，第二击吞跳跃；重力跳过 + 升降 4.0 b/s + 水平 1.6× 快于
  走路 + 碰撞照常 + 坠落累加器飞行归零【切出无爆发伤害】+
  水/熔岩自动切出记档 + 不存档记档）③ 24h HUD 时钟（纯函数
  clockString = fmod((t−0.05)×24+6, 24) 与 ramp 边精确对齐 晨
  06:00/午 12:00/昏 18:00/夜 00:00 + ulp 钉 04:47；renderUi 右上
  角恒可见；':' 字形在列零字体扩展）。2207/2207（+26）、零
  format（全会话态）。门 182：S1 **base18 第 3 次重锚论证**（diff
  609 px 全落右上角时钟区零泄漏 → 新锚 178676 B）+ S2 三判别
  （zoom new 视差/old None 互斥 + flight y62.13/54.00 互斥 +
  clock = S1 区）+ S3 + S6 命名单测 + 性能 A/B 同世界同成本
  （changesApplied 9/1/3 逐位 + 终档 16/16 36/36 恒等 + 全指标同
  档）。PITS 376（输入 E2E capture 重试/帧限收尾/pgrep 假
  PASS）。详见 docs/archive/v2x/phase-2.11.0.md。

## 表外

- 继承 1.x / 2.x 第一批表外：1.x 表外（见 `docs/archive/v1x/BACKLOG.md`
  表外节）+ 2.x 第一批表外（见本文件第一批「表外」节）。
- 二批无界项：新内容开放空间（方块/物品/配方/mob）；netherite 材料链
  （依赖 4.x 下界）= 立项需用户定案。
- 4.x「世界扩展」方向素材 = 已晋升执行权威 `docs/BACKLOG.md`
  （3.x 任务表——11-04 二批收官晋升定案为 v4.x，同日用户重编号定案
  改 3.x）。
