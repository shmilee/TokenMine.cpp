# BACKLOG.md — v2.x 任务表（执行权威）

> 11-02 文档深层整理：v1.x 任务矩阵（33 + UX 7 = 40，全完成）+ 执行顺序 +
> 表外 + v1.0 完成清单 + v1.x 时代变更记录 → `docs/archive/v1x/BACKLOG.md`；
> 本文件只留 v2.x。1.x 完整编年史 = `docs/archive/v1x/HISTORY.md`。
>
> **11-02 用户定案：2.x 任务矩阵按 1.x 同款格式搭建**（系统行 × ①–⑦ 格）：
> 10 个核心系统行（STRM…PRE）+ UX 行；**无任务的系统也先列行、格留空**；
> 任务只从本表取，不另列菜单；增删格子需用户确认。V2-1/V2-2（均为 1.x
> UX-4 第三人称相机 / UX-3 玩家身体模型的后续，用户实机诉求）落 UX 行
> ①/②；后续任务自 **V2-3** 起编号，按系统归属落行。
>
> **11-03 三表分工（用户定案）**：本表 = **2.x「上手与体验补全」期**（执行
> 权威，V2-3~V2-10 八项）；3.x = `docs/BACKLOG_3x.md`（「系统扩展」期，方向
> 底稿）；4.x = `docs/BACKLOG_4x.md`（「世界扩展」期，方向底稿）。2.x 收官
> 时：本表 → `docs/archive/v2x/BACKLOG.md`，3x 表晋升本文件（1.x→2.x 同款
> 先例）。

## v2.x 任务矩阵（5/10 完成）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ | ⑦ |
|---|---|---|---|---|---|---|---|
| STRM 流送与内存 | | | | | | | |
| PIPE 帧管线与并发 | | | | | | | |
| LGT 光照 | V2-8 sky rescan clobber 修复 | | | | | | |
| GEN 世界与生成 | | | | | | | |
| FLUID 流体 | | | | | | | |
| ENT 实体/Mob/移动 | ~~V2-5 新 mob 猪~~ ✅ v2.5.0（门 176：纯数据零核心代码 + V2-2 per-face 首个 mob 消费 + pork 食物链 + 图集零移位 + 猪字 +1） | V2-9 step-up quirk 修复 | | | | | |
| SURV 生存/战斗/效果 | | | | | | | |
| CRAFT 物品/合成/面板 | ~~V2-7 材料补齐批~~ ✅ v2.7.0（门 178：planks 块化 + gold/diamond_block【beacon_base 入列】+ spider 掉 string/spider_eye ×0-2 + 块级 per-face 通用能力【无 faces 位级不变红线，首个消费 furnace_front】，全数据驱动，零 format） | | | | | | |
| SAVE 存档 | | | | | | | |
| PRE 表现层 | ~~V2-4 天气（雨/雷）~~ ✅ v2.4.0（门 175 全 PASS：纯状态机首段恒 CLEAR + rain shader 静态 300 滴 + dayFactor 单收口 + 交互四件 ∨raining + 雷 wav，零 format，off ≡ v2.3.0 位级【门内三方恒等实证】） | V2-10 网格修复批 | | | | | |
| UX 实机报障/体验细化 | ~~V2-1 第三人称前视角~~ ✅ v2.1.0（门 172：C 键三循环 + 翻转角纯函数 + aimReach D+kReach） | ~~V2-2 玩家皮肤 Steve 化~~ ✅ v2.2.0（门 173：模型 per-face 纹理通用能力 + Steve 脸 8×8 像素 + 7 tile，既有 mob 位级不变） | ~~V2-3 教程系列化（农耕 + 厨房）~~ ✅ v2.3.0（门 174：系列化机制 + 2 新系列 + format 25 + recipe 行 emit 修复） | ~~V2-6 体验打磨批~~ ✅ v2.6.0（门 177：第一人称手臂 + mob blob 阴影 + 拾取磁力 + 月相，四记档消费，零 format） | | | |

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
  墙钟等）；2.x 各版自身偏离记档见 `docs/archive/phase-2.x.0.md`「偏离」
  节。
- **不立项**：PIPE-4 跨 chunk 合并（99.x 数据不支持，用户确认，见
  `docs/archive/v1x/BACKLOG.md` 表外节）。

## 变更记录（v2.x）

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
  docs/archive/phase-2.7.0.md。

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
  docs/archive/phase-2.6.0.md。

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
  持久化）。详见 docs/archive/phase-2.5.0.md。
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
  docs/archive/phase-2.4.0.md。

- 2026-11-03：**V2-3 教程系列化（农耕 + 厨房）完成（v2.3.0，门 174 全
  PASS）**：tutorials/ 多文件名序 = 系列序（beginner 11 步零改动 +
  farming ×2 修订 7 步 + kitchen 6 步，全链数据核实在位零新机制）+
  format 24→25（三向键纪律，≤24 值保真免迁）+ 事件词表 +pickup./
  smelt.（帧锁定）+ T = 跳当前系列 + 系列过渡无 toast；门内抓到并
  修复 UX-7 潜伏 bug = recipe 行合成漏发 craft 事件（PITS 360）。
  2109/2109（+25）、PITS 359→367。详见 docs/archive/phase-2.3.0.md。

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

- 2026-11-02：**V2-2 玩家皮肤 Steve 化完成（v2.2.0，门 173 全 PASS）**：模型 per-face 纹理（既有 mob 位级不变）+ Steve 脸/衫/裤/靴 7 tile（图集零移位免重锚）。详见 docs/archive/phase-2.2.0.md。

- 2026-11-02：**V2-1 第三人称前视角完成（v2.1.0，门 172 全 PASS）**：2.x 首个功能项——前视 = 后视同款数学在翻转角下执行（纯函数零改动），C 键三循环，瞄准 aimReach D+kReach。详见 docs/archive/phase-2.1.0.md。

- 2026-11-02：1.x 文档归档 `docs/archive/v1x/`（181 份 phase 文件 git mv 保历史 + 全仓引用路径同步修正）+ 本表 v2.x 占位新增。
